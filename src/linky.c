#include "zephyr/bluetooth/gap.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include <linky.h>

LOG_MODULE_REGISTER(linky, LOG_LEVEL_INF);

K_MSGQ_DEFINE(linky_msgq, LINKY_RECORD_BUF_SIZE, CONFIG_COPRO_LINKY_QUEUE_SIZE, 4);

bool linky_adv_data_recognize_cb(struct bt_data *data, void *user_data)
{
	switch (data->type) {
	case BT_DATA_NAME_COMPLETE: {
		if (strncmp((const char *)data->data,
					CONFIG_COPRO_LINKY_TIC_COMPLETE_NAME,
					data->data_len) == 0) {

			if (user_data != NULL) {
				bool *recognized = (bool *)user_data;
				*recognized		 = true;
			}

			/* Fully parsed */
			return false;
		}
	} break;
	default:
		break;
	}

	return true;
}

bool linky_adv_data_parse_measurements_cb(struct bt_data *data, void *user_data)
{
	linky_tic_record_t *record = (linky_tic_record_t *)user_data;

	switch (data->type) {
	case BT_DATA_MANUFACTURER_DATA: {
		LOG_HEXDUMP_DBG(data->data, data->data_len, "Manufacturer Data");
		memcpy(
			record->raw, &data->data[2u], MIN(data->data_len - 2u, sizeof(record->raw)));
		record->flags |= LINKY_RECORD_FLAG_VALID;
		return false;
	} break;
	default:
		break;
	}

	return true;
}

int linky_record_serialize(const linky_tic_record_t *xc, uint8_t *buf, size_t len)
{
	if (len < LINKY_RECORD_BUF_SIZE) {
		return -ENOMEM;
	}

	/* Buffer layout is as follows:
	 *  - 6 bytes: BLE address
	 *  - 1 byte: BLE address type
	 *  - 1 byte: RSSI
	 *  - 1 byte: header version
	 *	- 4 bytes: flags
	 *  - 8 bytes: timestamp
	 *  - 64 bytes: raw TIC data
	 */

	buf[0] = xc->addr.a.val[5];
	buf[1] = xc->addr.a.val[4];
	buf[2] = xc->addr.a.val[3];
	buf[3] = xc->addr.a.val[2];
	buf[4] = xc->addr.a.val[1];
	buf[5] = xc->addr.a.val[0];
	buf[6] = xc->addr.type;
	buf[7] = xc->rssi;
	buf[8] = 0x01; // Header version
	sys_put_le32(xc->flags, &buf[9]);
	sys_put_le64(xc->timestamp, &buf[13]);
	memcpy(&buf[21], xc->raw, sizeof(xc->raw));

	return LINKY_RECORD_BUF_SIZE;
}