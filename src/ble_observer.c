/*
 * Copyright (c) 2024 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "linky.h"
#include "zephyr/bluetooth/addr.h"

#include <stddef.h>
#include <stdio.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

#include <ble_observer.h>
#include <stream_client.h>
#include <xiaomi.h>

LOG_MODULE_REGISTER(obv, LOG_LEVEL_INF);

static bool bt_addr_manufacturer_match(const char *mf_addr_str, const bt_addr_t *addr)
{
	bt_addr_t mf;
	bt_addr_from_str(mf_addr_str, &mf);

	return memcmp(&addr->val[3], &mf.val[3], 3U) == 0;
}

static void device_found(const bt_addr_le_t *addr,
						 int8_t rssi,
						 uint8_t type,
						 struct net_buf_simple *ad)
{
	int ret;

#if CONFIG_COPRO_XIAOMI_LYWSD03MMC
	if (bt_addr_manufacturer_match(XIAOMI_MANUFACTURER_ADDR_STR, &addr->a) == true) {
		xiaomi_record_t xc = {0};
		if (xiaomi_bt_data_parse(addr, rssi, ad, &xc) == true) {

			char buf_record[XIAOMI_RECORD_BUF_SIZE];
			ret = xiaomi_record_serialize(&xc, buf_record, XIAOMI_RECORD_BUF_SIZE);
			if (ret < 0) {
				LOG_ERR("Failed to serialize xiaomi record: %d", ret);
				return;
			}

			ret = k_msgq_put(&xiaomi_msgq, buf_record, K_NO_WAIT);
			if (ret < 0) {
				LOG_ERR("Failed to put xiaomi record in msgq: %d", ret);
			}
		}
		return;
	}
#endif

#if CONFIG_COPRO_LINKY_TIC
	bool is_linky = false;
	struct net_buf_simple_state state;
	net_buf_simple_save(ad, &state);
	bt_data_parse(ad, linky_adv_data_recognize_cb, &is_linky);
	net_buf_simple_restore(ad, &state);

	if (is_linky == true) {
		linky_tic_record_t record = {0};
		bt_addr_le_copy(&record.addr, addr);
		record.rssi		 = rssi;
		record.timestamp = k_uptime_get();

		char addr_str[BT_ADDR_STR_LEN];
		bt_addr_to_str(&addr->a, addr_str, sizeof(addr_str));
		LOG_INF("Linky found: %s (RSSI %d)", addr_str, (int)rssi);
		bt_data_parse(ad, linky_adv_data_parse_measurements_cb, (void *)&record);

		char buf_record[LINKY_RECORD_BUF_SIZE];
		ret = linky_record_serialize(&record, buf_record, sizeof(buf_record));
		if (ret < 0) {
			LOG_ERR("Failed to serialize linky record: %d", ret);
			return;
		}

		if ((record.flags & LINKY_RECORD_FLAG_VALID) != 0) {
			ret = k_msgq_put(&linky_msgq, buf_record, K_NO_WAIT);
			if (ret < 0) {
				LOG_ERR("Failed to put linky record in msgq: %d", ret);
			}
		}

		return;
	}
#endif
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