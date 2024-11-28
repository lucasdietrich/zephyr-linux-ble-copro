/*
 * Copyright (c) 2024 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <stdio.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

#include <ble_observer.h>
#include <stream_client.h>
#include <xiaomi.h>

LOG_MODULE_REGISTER(obv, LOG_LEVEL_INF);

static void device_found(const bt_addr_le_t *addr,
						 int8_t rssi,
						 uint8_t type,
						 struct net_buf_simple *ad)
{
	int ret;

	if (xiomi_bt_addr_manufacturer_match(&addr->a) == true) {
		xiaomi_record_t xc = {0};
		if (xiaomi_bt_data_parse(addr, rssi, ad, &xc) == true) {
			// do something with the xiaomi record
			// stream_send_data(STREAM_CHANNEL_ID_XIAOMI, &xc, sizeof(xc));

			ret = k_msgq_put(&xiaomi_msgq, &xc, K_NO_WAIT);
			if (ret) {
				LOG_ERR("Failed to put data in xiaomi_msgq: %d", ret);
			}
		}
	}
}

int ble_observer_start(void)
{
	struct bt_le_scan_param scan_param = {
		.type	  = BT_LE_SCAN_TYPE_PASSIVE,
		.options  = BT_LE_SCAN_OPT_NONE, /* don't filter duplicates */
		.interval = BT_GAP_SCAN_FAST_INTERVAL,
		.window	  = BT_GAP_SCAN_FAST_WINDOW,
	};

	int ret = bt_le_scan_start(&scan_param, device_found);
	if (ret) {
		LOG_ERR("Starting scanning failed (ret %d)", ret);
		return ret;
	}

	return ret;
}