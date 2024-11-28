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
	uint32_t flags;						// Flags
} xiaomi_record_t;

#define STREAM_CHANNEL_NAME_XIAOMI	 "xiaomi_measurements"
#define STREAM_CHANNEL_ID_XIAOMI		 0xFA30FA42lu
#define STREAM_CHANNEL_PL_SIZE_XIAOMI sizeof(xiaomi_measurements_t)
#define STREAM_CHANNEL_FLAGS_XIAOMI	 0x0lu

bool xiomi_bt_addr_manufacturer_match(const bt_addr_t *addr);

bool xiaomi_bt_data_parse(const bt_addr_le_t *addr,
						  int8_t rssi,
						  struct net_buf_simple *ad,
						  xiaomi_record_t *xc);

extern struct k_msgq xiaomi_msgq;

#endif /* _XIAOMI_LYWSD03MMC_H */