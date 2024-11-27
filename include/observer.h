/*
 * Copyright (c) 2024 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BLE_OBSERVER_H
#define _BLE_OBSERVER_H

#include <stdint.h>

#include <zephyr/bluetooth/addr.h>

typedef struct {
	int8_t rssi;		   // RSSI
	int16_t temperature;   // Device measured temperature, base unit : 1e-2 °C
	uint16_t humidity;	   /* Device measured humidity, base unit : 1e-2 % */
	uint16_t battery_mv;   // Device measured battery voltage, base unit: 1 mV
	uint8_t battery_level; // Device measured battery level, base unit:  %, valid if > 0
} xiaomi_measurements_t;

typedef struct {
	bt_addr_le_t addr;					// Record device address
	uint32_t time;						// Time of record
	xiaomi_measurements_t measurements; // Record measurements
	uint32_t valid;						// Record validity
} xiaomi_record_t;

int ble_observer_start(void);

#endif /* _BLE_OBSERVER_H */