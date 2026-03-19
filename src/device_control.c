/*
 * Copyright (c) 2024 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <device_control.h>
#include <stream_client.h>

LOG_MODULE_REGISTER(device_control, LOG_LEVEL_INF);

/* TX queue: outgoing commands (device → server) */
K_MSGQ_DEFINE(device_control_tx_msgq,
			  sizeof(device_ctrl_command_msg_t),
			  CONFIG_COPRO_DEVICE_CONTROL_CMD_QUEUE_SIZE,
			  4);

/* RX queue: incoming state updates (server → device) */
K_MSGQ_DEFINE(device_control_rx_msgq,
			  sizeof(device_ctrl_state_msg_t),
			  CONFIG_COPRO_DEVICE_CONTROL_STATE_QUEUE_SIZE,
			  4);

/**
 * @brief Serialize a device control command into a byte buffer.
 *
 * Wire layout:
 *   - byte 0: command type (@ref device_ctrl_cmd_t)
 *
 * @param cmd  Command to serialize.
 * @param buf  Destination buffer.
 * @param len  Buffer size; must be >= 1.
 * @return Number of bytes written (1), or -EINVAL if the buffer is too small.
 */
static int device_ctrl_command_serialize(device_ctrl_cmd_t cmd, uint8_t *buf, size_t len)
{
	if (len < 1u) {
		return -EINVAL;
	}

	buf[0] = (uint8_t)cmd;

	return 1;
}

int device_control_send_cmd(device_ctrl_cmd_t cmd)
{
	LOG_DBG("Sending command: 0x%02x", (unsigned int)cmd);
	uint8_t buf[sizeof(device_ctrl_command_msg_t)];

	int ret = device_ctrl_command_serialize(cmd, buf, sizeof(buf));

	if (ret < 0) {
		LOG_ERR("Failed to serialize command 0x%02x: %d", (unsigned int)cmd, ret);
		return ret;
	}

	ret = k_msgq_put(&device_control_tx_msgq, buf, K_NO_WAIT);
	if (ret < 0) {
		LOG_WRN("TX queue full, dropping command 0x%02x", (unsigned int)cmd);
	}

	return ret;
}

static device_ctrl_garage_doors_state_t garage_state = {
	.left_door  = DEVICE_CTRL_DOOR_UNKNOWN,
	.gate       = DEVICE_CTRL_DOOR_UNKNOWN,
	.right_door = DEVICE_CTRL_DOOR_UNKNOWN,
};

static device_ctrl_state_cb_t state_cb;

static void device_control_rx_thread(void *a, void *b, void *c)
{
	device_ctrl_state_msg_t msg;

	for (;;) {
		k_msgq_get(&device_control_rx_msgq, &msg, K_FOREVER);

		switch (msg.type) {
		case DEVICE_CTRL_STATE_TYPE_GARAGE_DOORS: {
			garage_state = msg.payload.garage_doors;

			LOG_DBG("Garage state update: left=%d gate=%d right=%d",
					garage_state.left_door, garage_state.gate,
					garage_state.right_door);

			if (state_cb != NULL) {
				state_cb(&msg.payload.garage_doors);
			}
			break;
		}
		default:
			LOG_WRN("RX: unknown state type 0x%02x", (unsigned int)msg.type);
			break;
		}
	}
}

K_THREAD_DEFINE(device_ctrl_rx_tid, 1024u,
				device_control_rx_thread, NULL, NULL, NULL,
				K_PRIO_COOP(7), 0, SYS_FOREVER_MS);

device_ctrl_garage_doors_state_t device_control_get_garage_doors_state(void)
{
	return garage_state;
}

void device_control_set_state_cb(device_ctrl_state_cb_t cb)
{
	state_cb = cb;
}

int device_control_start(void)
{
	k_thread_start(device_ctrl_rx_tid);
	return 0;
}
