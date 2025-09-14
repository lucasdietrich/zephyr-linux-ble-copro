/*
 * Copyright (c) 2024 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _XIAOMI_LYWSD03MMC_H
#define _XIAOMI_LYWSD03MMC_H

#include <stddef.h>
#include <stdint.h>

#include <zephyr/bluetooth/bluetooth.h>

#define XIAOMI_MANUFACTURER_ADDR_STR "A4:C1:38:00:00:00"

typedef struct {
	int8_t rssi;		   // RSSI
	int16_t temperature;   // Device measured temperature, base unit : 1e-2 °C
	uint16_t humidity;	   /* Device measured humidity, base unit : 1e-2 % */
	uint16_t battery_mv;   // Device measured battery voltage, base unit: 1 mV
	uint8_t battery_level; // Device measured battery level, base unit:  %, valid if > 0
} xiaomi_measurements_t;

#define XIAOMI_RECORD_FLAG_VALID 0x01

typedef struct {
	bt_addr_le_t addr;					// Record device address
	xiaomi_measurements_t measurements; // Record measurements
	int64_t timestamp;					// Time of record (uptime since boot)
	uint32_t flags;						// flags
} xiaomi_record_t;

#define STREAM_CHANNEL_NAME_XIAOMI	  "xiaomi-lywsd03mmc-measurements"
#define STREAM_CHANNEL_ID_XIAOMI	  0xFA30FA42lu
#define STREAM_CHANNEL_PL_SIZE_XIAOMI sizeof(xiaomi_measurements_t)

bool xiaomi_bt_data_parse(const bt_addr_le_t *addr,
						  int8_t rssi,
						  struct net_buf_simple *ad,
						  xiaomi_record_t *xc);

#define XIAOMI_RECORD_BUF_SIZE		 24
#define XIAOMI_RECORD_HEADER_VERSION 0x01

/* Buffer layout is as follows:
 *  - 6 bytes: BLE address
 *   - 1 byte: BLE address type
 *  - 1 byte: RSSI
 *  - 1 byte: header version
 *  - 8 bytes: timestamp
 *  - 2 bytes: temperature
 *  - 2 bytes: humidity
 *  - 2 bytes: battery voltage (mV)
 *  - 1 byte: battery level (%)
 */
int xiaomi_record_serialize(const xiaomi_record_t *xc, uint8_t *buf, size_t len);

extern struct k_msgq xiaomi_msgq;

#endif /* _XIAOMI_LYWSD03MMC_H */