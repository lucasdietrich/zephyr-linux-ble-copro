#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include <xiaomi.h>

LOG_MODULE_REGISTER(xiaomi, LOG_LEVEL_INF);

#define XIAOMI_CUSTOM_ATC_NAME_STARTS_WITH "ATC_"
#define XIAOMI_CUSTOM_ATC_NAME_STARTS_WITH_SIZE                                          \
	(sizeof(XIAOMI_CUSTOM_ATC_NAME_STARTS_WITH) - 1)

#define XIAOMI_CUSTOM_ATC_ADV_PAYLOAD_SIZE sizeof(struct xiaomi_atc_custom_adv_payload)

K_MSGQ_DEFINE(xiaomi_msgq, XIAOMI_RECORD_BUF_SIZE, CONFIG_COPRO_XIAOMI_QUEUE_SIZE, 4);

/* https://github.com/pvvx/ATC_MiThermometer#custom-format-all-data-little-endian
 */
struct xiaomi_atc_custom_adv_payload {
	uint16_t UUID;		   // = 0x181A, GATT Service 0x181A Environmental Sensing
	uint8_t MAC[6];		   // [0] - lo, .. [6] - hi digits
	int16_t temperature;   // x 0.01 degree
	uint16_t humidity;	   // x 0.01 %
	uint16_t battery_mv;   // mV
	uint8_t battery_level; // 0..100 %
	uint8_t counter;	   // measurement count
	uint8_t flags;		   // GPIO_TRG pin (marking "reset" on circuit board) flags:
						   // bit0: Reed Switch, input
						   // bit1: GPIO_TRG pin output value (pull Up/Down)
						   // bit2: Output GPIO_TRG pin is controlled according to
						   // the set parameters bit3: Temperature trigger event
						   // bit4: Humidity trigger event
} __attribute__((packed));

struct xiaomi_atc_custom_adv {
	uint8_t size; // = 19
	uint8_t uid;  // = 0x16, 16-bit UUID

	struct xiaomi_atc_custom_adv_payload payload;
};

static bool adv_data_cb(struct bt_data *data, void *user_data)
{
	switch (data->type) {
	case BT_DATA_NAME_COMPLETE: {
		if ((data->data_len >= XIAOMI_CUSTOM_ATC_NAME_STARTS_WITH_SIZE) &&
			(memcmp(data->data,
					XIAOMI_CUSTOM_ATC_NAME_STARTS_WITH,
					XIAOMI_CUSTOM_ATC_NAME_STARTS_WITH_SIZE) == 0)) {

			/* copy device name */
			char name[128u];
			size_t copy_len = MIN(data->data_len, sizeof(name) - 1);
			memcpy(name, data->data, copy_len);
			name[copy_len] = '\0';

			LOG_INF("[XIAOMI] name: %s", name);
		}
	} break;
	case BT_DATA_SVC_DATA16: {
		if (data->data_len == XIAOMI_CUSTOM_ATC_ADV_PAYLOAD_SIZE) {
			struct xiaomi_atc_custom_adv_payload *const payload =
				(struct xiaomi_atc_custom_adv_payload *)data->data;

			xiaomi_record_t *const xc = (xiaomi_record_t *)user_data;

			if (payload->UUID == BT_UUID_ESS_VAL) {
				xc->flags					   = XIAOMI_RECORD_FLAG_VALID;
				xc->timestamp				   = k_uptime_get();
				xc->measurements.battery_level = payload->battery_level;
				xc->measurements.battery_mv	   = payload->battery_mv;
				xc->measurements.humidity	   = payload->humidity;
				xc->measurements.temperature   = payload->temperature;

				/* Fully parsed */
				return false;
			}
		}
	} break;
	default:
		break;
	}

	return true;
}

bool xiaomi_bt_data_parse(const bt_addr_le_t *addr,
						  int8_t rssi,
						  struct net_buf_simple *ad,
						  xiaomi_record_t *xc)
{
	int success = false;

	memset(xc, 0, sizeof(xiaomi_record_t));

	bt_data_parse(ad, adv_data_cb, xc);

	if ((xc->flags & XIAOMI_RECORD_FLAG_VALID) != 0) {
		bt_addr_le_copy(&xc->addr, addr);
		xc->measurements.rssi = rssi;

		char mac_str[BT_ADDR_STR_LEN];
		bt_addr_to_str(&addr->a, mac_str, sizeof(mac_str));
		LOG_INF("[XIAOMI] mac: %s rssi: %d bat: %u mV temp: %u "
				"°C hum: %u %%",
				mac_str,
				(int)rssi,
				xc->measurements.battery_mv,
				xc->measurements.temperature / 100,
				xc->measurements.humidity / 100);

		success = true;
	}

	return success;
}

int xiaomi_record_serialize(const xiaomi_record_t *xc, uint8_t *buf, size_t len)
{
	if (len < XIAOMI_RECORD_BUF_SIZE) {
		return -ENOMEM;
	}

	buf[0] = xc->addr.a.val[5];
	buf[1] = xc->addr.a.val[4];
	buf[2] = xc->addr.a.val[3];
	buf[3] = xc->addr.a.val[2];
	buf[4] = xc->addr.a.val[1];
	buf[5] = xc->addr.a.val[0];
	buf[6] = xc->addr.type;
	buf[7] = xc->measurements.rssi;
	buf[8] = XIAOMI_RECORD_HEADER_VERSION;
	sys_put_le64(xc->timestamp, &buf[9]);
	sys_put_le16(xc->measurements.temperature, &buf[17]);
	sys_put_le16(xc->measurements.humidity, &buf[19]);
	sys_put_le16(xc->measurements.battery_mv, &buf[21]);
	buf[23] = xc->measurements.battery_level;

	return XIAOMI_RECORD_BUF_SIZE;
}