/*
 * Copyright (c) 2026 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BLE_SERVER_H
#define _BLE_SERVER_H

#include <stdint.h>

#include <zephyr/bluetooth/addr.h>

/* ---------------------------------------------------------------------------
 * Garage Control Service
 *
 * Attribute layout:
 *  0  – primary service declaration
 *  1  – left door characteristic declaration
 *  2  – left door characteristic value       ← indicate target
 *  3  – left door CCC descriptor
 *  4  – right door characteristic declaration
 *  5  – right door characteristic value      ← indicate target
 *  6  – right door CCC descriptor
 *  7  – gate characteristic declaration
 *  8  – gate characteristic value            ← indicate target
 *  9  – gate CCC descriptor
 *  10 – firmware flags characteristic declaration
 *  11 – firmware flags characteristic value  ← notify target
 *  12 – firmware flags CCC descriptor
 * ---------------------------------------------------------------------------*/

#define BT_UUID_GARAGE_SERVICE_VAL                                                       \
	BT_UUID_128_ENCODE(0x539f0000, 0x43b5, 0x4c29, 0x9ea2, 0x99a56589ca60)
#define BT_UUID_GARAGE_LEFT_DOOR_VAL                                                     \
	BT_UUID_128_ENCODE(0x539f0001, 0x43b5, 0x4c29, 0x9ea2, 0x99a56589ca60)
#define BT_UUID_GARAGE_RIGHT_DOOR_VAL                                                    \
	BT_UUID_128_ENCODE(0x539f0002, 0x43b5, 0x4c29, 0x9ea2, 0x99a56589ca60)
#define BT_UUID_GARAGE_GATE_VAL                                                          \
	BT_UUID_128_ENCODE(0x539f0003, 0x43b5, 0x4c29, 0x9ea2, 0x99a56589ca60)
#define BT_UUID_GARAGE_FLAGS_VAL                                                         \
	BT_UUID_128_ENCODE(0x539f0004, 0x43b5, 0x4c29, 0x9ea2, 0x99a56589ca60)

#define BT_UUID_GARAGE_SERVICE	  BT_UUID_DECLARE_128(BT_UUID_GARAGE_SERVICE_VAL)
#define BT_UUID_GARAGE_LEFT_DOOR  BT_UUID_DECLARE_128(BT_UUID_GARAGE_LEFT_DOOR_VAL)
#define BT_UUID_GARAGE_RIGHT_DOOR BT_UUID_DECLARE_128(BT_UUID_GARAGE_RIGHT_DOOR_VAL)
#define BT_UUID_GARAGE_GATE		  BT_UUID_DECLARE_128(BT_UUID_GARAGE_GATE_VAL)
#define BT_UUID_GARAGE_FLAGS	  BT_UUID_DECLARE_128(BT_UUID_GARAGE_FLAGS_VAL)

/* Firmware status flags bit definitions (firmware flags characteristic) */
#define BLE_FLAG_SERVER_CONNECTED BIT(0) /**< bit 0: TCP stream server is connected */

int ble_server_start(void);

typedef enum {
	BLE_CTRL_EVENT_CONNECTED		   = 0x01,
	BLE_CTRL_EVENT_DISCONNECTED	   = 0x02,
	BLE_CTRL_EVENT_PAIRING_CODE	   = 0x03,
	BLE_CTRL_EVENT_PAIRING_RESULT	   = 0x04,
	BLE_CTRL_EVENT_ALL_BONDS_REMOVED = 0x05, 
} ble_ctrl_event_t;

/* RX actions (server → device) */
#define BLE_CTRL_ACTION_REMOVE_ALL_BONDS 0xFFFFFFFFu
#define BLE_CTRL_ACTION_REMOVE_BOND		 0xFFFFFFFEu

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
	uint32_t action;
	bt_addr_le_t addr; /* optional address parameter, usage depends on action */
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
int bt_ctrl_msg_send_all_bonds_removed(void);

#endif /* _BLE_SERVER_H */