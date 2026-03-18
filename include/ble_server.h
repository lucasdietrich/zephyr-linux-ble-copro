/*
 * Copyright (c) 2026 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BLE_SERVER_H
#define _BLE_SERVER_H

#include <stdint.h>

#include <zephyr/bluetooth/addr.h>

int ble_server_start(void);

int my_lbs_send_sensor_notify(uint32_t sensor_value);
int my_lbs_send_button_state_indicate(bool button_state);

typedef enum {
	BLE_CTRL_CMD_CONNECTED		   = 0x01,
	BLE_CTRL_CMD_DISCONNECTED	   = 0x02,
	BLE_CTRL_CMD_PAIRING_CODE	   = 0x03,
	BLE_CTRL_CMD_PAIRING_RESULT	   = 0x04,
} ble_ctrl_cmd_t;


struct ble_ctrl_pairing_code {
	uint32_t passkey; /* 6-digit numeric code */
};

struct ble_ctrl_pairing_result {
	uint32_t success; // 0 for success, non-zero for failure (e.g. cancelled by user)
};

struct ble_ctrl_tx_msg {
	uint32_t cmd;
	bt_addr_le_t addr;
	union {
		struct ble_ctrl_pairing_code pairing_code;
		struct ble_ctrl_pairing_result pairing_result;
	} param;
};

struct ble_ctrl_rx_msg {
	uint32_t cmd;
};

#define SC_NAME_BLE_CONTROL "ble-control"
#define SC_ID_BLE_CONTROL	0x4f154ca0

#define SC_TX_PAYLOAD_SIZE_BLE_CONTROL	 sizeof(struct ble_ctrl_tx_msg)
#define SC_TX_MSG_QUEUE_SIZE_BLE_CONTROL 4u

#define SC_RX_PAYLOAD_SIZE_BLE_CONTROL	 sizeof(struct ble_ctrl_rx_msg)
#define SC_RX_MSG_QUEUE_SIZE_BLE_CONTROL 4u

int bt_ctrl_msg_send_pairing_code(const bt_addr_le_t *addr, uint32_t passkey);
int bt_ctrl_msg_send_pairing_result(const bt_addr_le_t *addr, bool success);
int bt_ctrl_msg_send_connected(const bt_addr_le_t *addr);
int bt_ctrl_msg_send_disconnected(const bt_addr_le_t *addr);

#endif /* _BLE_SERVER_H */