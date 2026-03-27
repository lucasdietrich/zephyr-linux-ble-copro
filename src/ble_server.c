#include "ble_server.h"
#include "led.h"
#include "stream_client.h"
#include "zephyr/sys/byteorder.h"

#include <device_control.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_core.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/gatt.h>

K_MSGQ_DEFINE(ble_ctrl_tx_msgq,
			  SC_TX_PAYLOAD_SIZE_BLE_CONTROL,
			  SC_TX_MSG_QUEUE_SIZE_BLE_CONTROL,
			  4);

K_MSGQ_DEFINE(ble_ctrl_rx_msgq,
			  SC_RX_PAYLOAD_SIZE_BLE_CONTROL,
			  SC_RX_MSG_QUEUE_SIZE_BLE_CONTROL,
			  4);

LOG_MODULE_REGISTER(ble_server, LOG_LEVEL_DBG);

#define DEVICE_NAME		CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

struct bt_conn *my_conn = NULL;
static struct k_work adv_work;

#define COMPANY_ID_CODE 0xFFFF
typedef struct adv_mfg_data {
	uint16_t company_code; /* Company Identifier Code. */
	uint16_t number_press;
} adv_mfg_data_type;

static adv_mfg_data_type adv_mfg_data = {COMPANY_ID_CODE, 0x77};

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
	BT_DATA(
		BT_DATA_MANUFACTURER_DATA, (unsigned char *)&adv_mfg_data, sizeof(adv_mfg_data)),
};

// TODO
// static unsigned char url_data[] = {0x17, '/', '/', 'a', 'c', 'a', 'd', 'e', 'm',
// 								   'y',	 '.', 'n', 'o', 'r', 'd', 'i', 'c', 's',
// 								   'e',	 'm', 'i', '.', 'c', 'o', 'm'};

static const struct bt_data sd[] = {
	// BT_DATA(BT_DATA_URI, url_data, sizeof(url_data)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL,
				  BT_UUID_128_ENCODE(0x539f0000, 0x43b5, 0x4c29, 0x9ea2, 0x99a56589ca60)),
};

static const struct bt_le_adv_param *adv_param = BT_LE_ADV_PARAM(
	BT_LE_ADV_OPT_CONN | BT_LE_ADV_OPT_USE_IDENTITY,
	BT_GAP_ADV_FAST_INT_MIN_1, /* Min Advertising Interval 500ms (800*0.625ms) */
	BT_GAP_ADV_FAST_INT_MAX_1, /* Max Advertising Interval 500.625ms (801*0.625ms) */
	NULL);					   /* Set to NULL for undirected advertising */

static void adv_work_handler(struct k_work *work)
{
	int err = bt_le_adv_start(adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));

	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");
}

static void advertising_start(void)
{
	k_work_submit(&adv_work);
}

#if CONFIG_COPRO_BLE_IDLE_TIMEOUT_S > 0
static void idle_timeout_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(idle_timeout_work, idle_timeout_handler);

static void idle_timeout_handler(struct k_work *work)
{
	if (my_conn) {
		LOG_WRN("BLE idle timeout (%ds), disconnecting",
				CONFIG_COPRO_BLE_IDLE_TIMEOUT_S);
		bt_conn_disconnect(my_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	}
}

static void idle_timer_reset(void)
{
	k_work_reschedule(&idle_timeout_work,
					  K_SECONDS(CONFIG_COPRO_BLE_IDLE_TIMEOUT_S));
}

static void idle_timer_cancel(void)
{
	k_work_cancel_delayable(&idle_timeout_work);
}
#else
static inline void idle_timer_reset(void)  {}
static inline void idle_timer_cancel(void) {}
#endif

static uint8_t firmware_flags;
static void firmware_flags_notify(void);

#if defined(CONFIG_COPRO_DEVICE_CONTROL)
static void stream_conn_cb(bool connected)
{
	if (connected) {
		firmware_flags |= BLE_FLAG_SERVER_CONNECTED;
	} else {
		firmware_flags &= ~BLE_FLAG_SERVER_CONNECTED;
	}
	firmware_flags_notify();
}
#endif


void on_connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("Connection error %d", err);
        return;
    }
    LOG_INF("Connected");
    my_conn = bt_conn_ref(conn);

    // TODO
    board_led_off();
	bt_ctrl_msg_send_connected(bt_conn_get_dst(conn));
	idle_timer_reset();
}

void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Disconnected. Reason %d", reason);
    bt_conn_unref(my_conn);
	idle_timer_cancel();

    // TODO
    board_led_on();
	bt_ctrl_msg_send_disconnected(bt_conn_get_dst(conn));
}

static void on_recycled(void)
{
	printk("Connection object available from previous conn. Disconnect is complete!\n");
	advertising_start();
}

void on_security_changed(struct bt_conn *conn,
						 bt_security_t level,
						 enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		LOG_INF("Security changed: %s level %u\n", addr, level);

		if (level == BT_SECURITY_L4) {
			bt_ctrl_msg_send_pairing_result(bt_conn_get_dst(conn), true);
		}
		idle_timer_reset();
	} else {
		LOG_INF("Security failed: %s level %u err %d\n", addr, level,
			err);
		bt_ctrl_msg_send_pairing_result(bt_conn_get_dst(conn), false);
	}
}

void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Passkey for %s: %06u\n", addr, passkey);
	bt_ctrl_msg_send_pairing_code(bt_conn_get_dst(conn), passkey);
}

void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing cancelled: %s\n", addr);
	bt_ctrl_msg_send_pairing_result(bt_conn_get_dst(conn), false);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected              = on_connected,
    .disconnected           = on_disconnected,
    .recycled               = on_recycled,
	.security_changed 		= on_security_changed,
};

static struct bt_conn_auth_cb conn_auth_callbacks = {
	.passkey_display = auth_passkey_display,
	.cancel = auth_cancel,
};

#if defined(CONFIG_COPRO_DEVICE_CONTROL)

static bool indicate_left_door_enabled;
static bool indicate_right_door_enabled;
static bool indicate_gate_enabled;
static bool notify_flags_enabled;

static void garage_left_door_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	indicate_left_door_enabled = (value == BT_GATT_CCC_INDICATE);
}

static void garage_right_door_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	indicate_right_door_enabled = (value == BT_GATT_CCC_INDICATE);
}

static void garage_gate_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	indicate_gate_enabled = (value == BT_GATT_CCC_INDICATE);
}

static void garage_flags_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	notify_flags_enabled = (value == BT_GATT_CCC_NOTIFY);
}

static ssize_t read_left_door(struct bt_conn *conn, const struct bt_gatt_attr *attr,
							  void *buf, uint16_t len, uint16_t offset)
{
	LOG_DBG("Read left door state, handle: %u, conn: %p", attr->handle, (void *)conn);

	device_ctrl_garage_doors_state_t state = device_control_get_garage_doors_state();
	uint8_t val = (uint8_t)state.left_door;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &val, sizeof(val));
}

static ssize_t write_left_door(struct bt_conn *conn, const struct bt_gatt_attr *attr,
							   const void *buf, uint16_t len,
							   uint16_t offset, uint8_t flags)
{
	LOG_DBG("Write left door state, handle: %u, conn: %p", attr->handle, (void *)conn);

	if (len != 1U || offset != 0) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	if (*((const uint8_t *)buf) != 0x01) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	struct bt_conn_info info;
	bt_conn_get_info(conn, &info);
	device_control_send_cmd(DEVICE_CTRL_CMD_OPEN_LEFT_GARAGE_DOOR, info.le.dst);
	idle_timer_reset();
	return len;
}

static ssize_t read_right_door(struct bt_conn *conn, const struct bt_gatt_attr *attr,
							   void *buf, uint16_t len, uint16_t offset)
{
	LOG_DBG("Attribute read, handle: %u, conn: %p", attr->handle, (void *)conn);

	device_ctrl_garage_doors_state_t state = device_control_get_garage_doors_state();
	uint8_t val = (uint8_t)state.right_door;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &val, sizeof(val));
}

static ssize_t write_right_door(struct bt_conn *conn, const struct bt_gatt_attr *attr,
								const void *buf, uint16_t len,
								uint16_t offset, uint8_t flags)
{
	LOG_DBG("Attribute write, handle: %u, conn: %p", attr->handle, (void *)conn);

	if (len != 1U || offset != 0) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}
	if (*((const uint8_t *)buf) != 0x01) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}
	struct bt_conn_info info;
	bt_conn_get_info(conn, &info);
	device_control_send_cmd(DEVICE_CTRL_CMD_OPEN_RIGHT_GARAGE_DOOR, info.le.dst);
	idle_timer_reset();
	return len;
}

static ssize_t read_gate(struct bt_conn *conn, const struct bt_gatt_attr *attr,
						 void *buf, uint16_t len, uint16_t offset)
{
	LOG_DBG("Attribute read, handle: %u, conn: %p", attr->handle, (void *)conn);

	device_ctrl_garage_doors_state_t state = device_control_get_garage_doors_state();
	uint8_t val = (uint8_t)state.gate;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &val, sizeof(val));
}

static ssize_t read_flags(struct bt_conn *conn, const struct bt_gatt_attr *attr,
					  void *buf, uint16_t len, uint16_t offset)
{
	LOG_DBG("Read firmware flags, handle: %u, conn: %p", attr->handle, (void *)conn);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &firmware_flags,
							 sizeof(firmware_flags));
}

BT_GATT_SERVICE_DEFINE(
	garage_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_GARAGE_SERVICE),

	BT_GATT_CHARACTERISTIC(BT_UUID_GARAGE_LEFT_DOOR,
						   BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_INDICATE,
						   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE_AUTHEN,
						   read_left_door, write_left_door, NULL),
	BT_GATT_CCC(garage_left_door_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	BT_GATT_CHARACTERISTIC(BT_UUID_GARAGE_RIGHT_DOOR,
						   BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_INDICATE,
						   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE_AUTHEN,
						   read_right_door, write_right_door, NULL),
	BT_GATT_CCC(garage_right_door_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	BT_GATT_CHARACTERISTIC(BT_UUID_GARAGE_GATE,
						   BT_GATT_CHRC_READ | BT_GATT_CHRC_INDICATE,
						   BT_GATT_PERM_READ,
						   read_gate, NULL, NULL),
	BT_GATT_CCC(garage_gate_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	BT_GATT_CHARACTERISTIC(BT_UUID_GARAGE_FLAGS,
						   BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
						   BT_GATT_PERM_READ,
						   read_flags, NULL, NULL),
	BT_GATT_CCC(garage_flags_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

static void garage_indicate_cb(struct bt_conn *conn,
							   struct bt_gatt_indicate_params *params,
							   uint8_t err)
{
	LOG_DBG("Garage indication %s", err != 0U ? "fail" : "success");
}

/* Static params + value buffers — must outlive the ATT confirmation. */
static struct bt_gatt_indicate_params garage_left_ind_params;
static struct bt_gatt_indicate_params garage_right_ind_params;
static struct bt_gatt_indicate_params garage_gate_ind_params;
static uint8_t garage_left_ind_val;
static uint8_t garage_right_ind_val;
static uint8_t garage_gate_ind_val;

/* Sends BLE indications for whichever channels are flagged in @changed. */
static void garage_doors_notify_state(const device_ctrl_garage_doors_state_t *state,
									  uint8_t changed)
{
	if ((changed & DEVICE_CTRL_GARAGE_CHANGED_LEFT_DOOR) && indicate_left_door_enabled) {
		garage_left_ind_val                = (uint8_t)state->left_door;
		garage_left_ind_params.attr        = &garage_svc.attrs[2];
		garage_left_ind_params.func        = garage_indicate_cb;
		garage_left_ind_params.destroy     = NULL;
		garage_left_ind_params.data        = &garage_left_ind_val;
		garage_left_ind_params.len         = sizeof(garage_left_ind_val);
		bt_gatt_indicate(NULL, &garage_left_ind_params);
	}
	if ((changed & DEVICE_CTRL_GARAGE_CHANGED_RIGHT_DOOR) && indicate_right_door_enabled) {
		garage_right_ind_val               = (uint8_t)state->right_door;
		garage_right_ind_params.attr       = &garage_svc.attrs[5];
		garage_right_ind_params.func       = garage_indicate_cb;
		garage_right_ind_params.destroy    = NULL;
		garage_right_ind_params.data       = &garage_right_ind_val;
		garage_right_ind_params.len        = sizeof(garage_right_ind_val);
		bt_gatt_indicate(NULL, &garage_right_ind_params);
	}
	if ((changed & DEVICE_CTRL_GARAGE_CHANGED_GATE) && indicate_gate_enabled) {
		garage_gate_ind_val                = (uint8_t)state->gate;
		garage_gate_ind_params.attr        = &garage_svc.attrs[8];
		garage_gate_ind_params.func        = garage_indicate_cb;
		garage_gate_ind_params.destroy     = NULL;
		garage_gate_ind_params.data        = &garage_gate_ind_val;
		garage_gate_ind_params.len         = sizeof(garage_gate_ind_val);
		bt_gatt_indicate(NULL, &garage_gate_ind_params);
	}
}

/* Registered with device_control_set_state_cb(). Forwards the pre-computed
 * changed bitmask directly to the BLE indication function. */
static void garage_state_cb(const device_ctrl_garage_doors_state_t *state, uint8_t changed)
{
	garage_doors_notify_state(state, changed);
}

static void firmware_flags_notify(void)
{
	if (!notify_flags_enabled) {
		return;
	}
	bt_gatt_notify(NULL, &garage_svc.attrs[11], &firmware_flags, sizeof(firmware_flags));
}

#endif /* CONFIG_COPRO_DEVICE_CONTROL */

void iter_bond_cb(const struct bt_bond_info *info, void *user_data)
{
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(&info->addr, addr, sizeof(addr));
	printk("Bonded device: %s\n", addr);
}

static void ble_ctrl_rx_thread(void *a, void *b, void *c)
{
	int ret;
	struct ble_ctrl_rx_msg msg;

	for (;;) {
		k_msgq_get(&ble_ctrl_rx_msgq, &msg, K_FOREVER);

		switch (msg.action) {
		case BLE_CTRL_ACTION_REMOVE_ALL_BONDS:
			LOG_INF("Removing all bonds");
			ret = bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);
			if (ret == 0) {
				bt_ctrl_msg_send_all_bonds_removed();
			} else {
				LOG_ERR("Failed to remove all bonds: %d", ret);
			}
			break;
		default:
			LOG_WRN("Unknown RX ctrl action: 0x%08X", msg.action);
			break;
		}
	}
}

K_THREAD_DEFINE(ble_ctrl_rx_tid, 1024u,
				ble_ctrl_rx_thread, NULL, NULL, NULL,
				K_PRIO_PREEMPT(10), 0, SYS_FOREVER_MS);

int ble_server_start(void)
{
	int ret;

	/* Configure the stream client */
	ret = stream_client_channel_add(SC_ID_BLE_CONTROL,
									SC_NAME_BLE_CONTROL,
								&ble_ctrl_tx_msgq, &ble_ctrl_rx_msgq);
	if (ret < 0) {
		LOG_ERR("Failed to add linky channel to stream client: %d", ret);
		return ret;
	}

	bt_addr_le_t addr;
	ret = bt_addr_le_from_str("FF:EE:DD:CC:BB:AA", "random", &addr);
	if (ret) {
		printk("Invalid BT address (err %d)\n", ret);
	}

	ret = bt_id_create(&addr, NULL);
	if (ret < 0) {
		printk("Creating new ID failed (err %d)\n", ret);
	}

	ret = bt_conn_auth_cb_register(&conn_auth_callbacks);
	if (ret) {
		LOG_INF("Failed to register authorization callbacks.\n");
		return -1;
	}

	bt_foreach_bond(BT_ID_DEFAULT, iter_bond_cb, NULL);

	k_thread_start(ble_ctrl_rx_tid);

	k_work_init(&adv_work, adv_work_handler);
	advertising_start();

#if defined(CONFIG_COPRO_DEVICE_CONTROL)
	device_control_set_state_cb(garage_state_cb);
	stream_client_set_conn_cb(stream_conn_cb);
#endif

	return 0;
}

static void bt_ctrl_msg_serialize(struct ble_ctrl_tx_msg *msg, uint8_t *buf, size_t buf_size)
{
	if (buf_size < sizeof(struct ble_ctrl_tx_msg)) {
		LOG_ERR("Buffer too small for serialization");
		return;
	}

	memset(buf, 0, buf_size);

	sys_put_le32(msg->cmd, &buf[0]);
	buf[4] = msg->addr.type;
	memcpy(&buf[5], &msg->addr.a.val, sizeof(msg->addr.a.val));
	switch (msg->cmd) {
		case BLE_CTRL_EVENT_PAIRING_CODE:
			sys_put_le32(msg->param.pairing_code.passkey, &buf[4 + sizeof(bt_addr_le_t)]);
			break;
		case BLE_CTRL_EVENT_PAIRING_RESULT:
			buf[4 + sizeof(bt_addr_le_t)] = msg->param.pairing_result.success ? 0x00 : 0x01;
			break;
		case BLE_CTRL_EVENT_CONNECTED:
		case BLE_CTRL_EVENT_DISCONNECTED:
		case BLE_CTRL_EVENT_ALL_BONDS_REMOVED:
			/* No additional parameters */
			break;
		default:
			LOG_ERR("Unknown command: %u", msg->cmd);
			break;
	}
}

int bt_ctrl_msg_send_pairing_code(const bt_addr_le_t *addr, uint32_t passkey)
{
	struct ble_ctrl_tx_msg msg = {
		.cmd = BLE_CTRL_EVENT_PAIRING_CODE,
		.addr = *addr,
		.param = {
			.pairing_code = {
				.passkey = passkey,
			},
		},
	};

	uint8_t buf[SC_TX_PAYLOAD_SIZE_BLE_CONTROL] = {0};
	bt_ctrl_msg_serialize(&msg, buf, sizeof(buf));

	return k_msgq_put(&ble_ctrl_tx_msgq, (const void *)buf, K_NO_WAIT);
}

int bt_ctrl_msg_send_pairing_result(const bt_addr_le_t *addr, bool success)
{
	struct ble_ctrl_tx_msg msg = {
		.cmd = BLE_CTRL_EVENT_PAIRING_RESULT,
		.addr = *addr,
		.param = {
			.pairing_result = {
				.success = success,
			},
		},
	};

	uint8_t buf[SC_TX_PAYLOAD_SIZE_BLE_CONTROL] = {0};
	bt_ctrl_msg_serialize(&msg, buf, sizeof(buf));

	return k_msgq_put(&ble_ctrl_tx_msgq, (const void *)buf, K_NO_WAIT);
}

int bt_ctrl_msg_send_connected(const bt_addr_le_t *addr)
{
	struct ble_ctrl_tx_msg msg = {
		.cmd = BLE_CTRL_EVENT_CONNECTED,
		.addr = *addr,
	};

	uint8_t buf[SC_TX_PAYLOAD_SIZE_BLE_CONTROL] = {0};
	bt_ctrl_msg_serialize(&msg, buf, sizeof(buf));

	return k_msgq_put(&ble_ctrl_tx_msgq, (const void *)buf, K_NO_WAIT);
}

int bt_ctrl_msg_send_disconnected(const bt_addr_le_t *addr)
{
	struct ble_ctrl_tx_msg msg = {
		.cmd = BLE_CTRL_EVENT_DISCONNECTED,
		.addr = *addr,
		.param = {
			.pairing_result = {
				.success = false,
			},
		},
	};

	uint8_t buf[SC_TX_PAYLOAD_SIZE_BLE_CONTROL] = {0};
	bt_ctrl_msg_serialize(&msg, buf, sizeof(buf));

	return k_msgq_put(&ble_ctrl_tx_msgq, (const void *)buf, K_NO_WAIT);
}

int bt_ctrl_msg_send_all_bonds_removed(void)
{
	struct ble_ctrl_tx_msg msg = {
		.cmd = BLE_CTRL_EVENT_ALL_BONDS_REMOVED,
	};

	uint8_t buf[SC_TX_PAYLOAD_SIZE_BLE_CONTROL] = {0};
	bt_ctrl_msg_serialize(&msg, buf, sizeof(buf));

	return k_msgq_put(&ble_ctrl_tx_msgq, (const void *)buf, K_NO_WAIT);
}