#include "ble_server.h"
#include "zephyr/bluetooth/bluetooth.h"
#include "zephyr/logging/log.h"
#include "zephyr/logging/log_core.h"
#include "zephyr/sys/util.h"

#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(ble_server, LOG_LEVEL_DBG);

#define DEVICE_NAME		CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),

};

static unsigned char url_data[] = {0x17, '/', '/', 'a', 'c', 'a', 'd', 'e', 'm',
								   'y',	 '.', 'n', 'o', 'r', 'd', 'i', 'c', 's',
								   'e',	 'm', 'i', '.', 'c', 'o', 'm'};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_URI, url_data, sizeof(url_data)),
};

static const struct bt_le_adv_param *adv_param =
	BT_LE_ADV_PARAM(BT_LE_ADV_OPT_NONE, /* No options specified */
					800,				/* Min Advertising Interval 500ms (800*0.625ms) */
					801,   /* Max Advertising Interval 500.625ms (801*0.625ms) */
					NULL); /* Set to NULL for undirected advertising */

int ble_server_start(void)
{
	int ret;

	ret = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (ret) {
		LOG_ERR("Advertising failed to start (err %d)\n", ret);
		return -1;
	}

	return 0;
}