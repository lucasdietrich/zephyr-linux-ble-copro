

/*
 * Copyright (c) 2025 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _LINKY_H
#define _LINKY_H

#include <stddef.h>
#include <stdint.h>

#include <zephyr/bluetooth/bluetooth.h>

#define LINKY_TIC_RAW_BUFFER_SIZE 64u

#define STREAM_CHANNEL_NAME_LINKY_TIC "linky-tic-measurements"
#define STREAM_CHANNEL_ID_LINKY_TIC	  0xCD1F14BDlu

#define LINKY_RECORD_FLAG_VALID BIT(0)

typedef struct {
	bt_addr_le_t addr; // Record device address
	int8_t rssi;	   // RSSI
	char raw[LINKY_TIC_RAW_BUFFER_SIZE];
	int64_t timestamp; // Time of record (uptime since boot)
	uint32_t flags;	   // Temporary flags
} linky_tic_record_t;

extern struct k_msgq linky_msgq;

bool linky_adv_data_recognize_cb(struct bt_data *data, void *user_data);

bool linky_adv_data_parse_measurements_cb(struct bt_data *data, void *user_data);

#define LINKY_RECORD_BUF_SIZE		(21u + LINKY_TIC_RAW_BUFFER_SIZE)
#define LINKY_RECORD_HEADER_VERSION 0x01

int linky_record_serialize(const linky_tic_record_t *lc, uint8_t *buf, size_t len);

#endif /* _LINKY_H */