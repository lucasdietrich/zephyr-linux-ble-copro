/*
 * Copyright (c) 2024 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _DEVICE_CONTROL_H
#define _DEVICE_CONTROL_H

#include <stdint.h>

#include <zephyr/kernel.h>

/* Stream channel registration */
#define SC_NAME_DEVICE_CONTROL "device-control"
#define SC_ID_DEVICE_CONTROL   0xeb5d8977

/*---------------------------------------------------------------------------
 * TX: Commands sent from device to server
 *---------------------------------------------------------------------------*/

/**
 * @brief Device control command types.
 */
typedef enum __packed {
	DEVICE_CTRL_CMD_OPEN_LEFT_GARAGE_DOOR  = 0x01,
	DEVICE_CTRL_CMD_OPEN_RIGHT_GARAGE_DOOR = 0x02,
} device_ctrl_cmd_t;

/**
 * @brief Command message transmitted over the stream channel (TX wire format).
 *
 * Wire layout:
 *   - 1 byte: command type (@ref device_ctrl_cmd_t)
 */
typedef struct __packed {
	device_ctrl_cmd_t cmd;
} device_ctrl_command_msg_t;

/*---------------------------------------------------------------------------
 * RX: States received from server
 *---------------------------------------------------------------------------*/

/**
 * @brief Individual door / gate position state.
 */
typedef enum __packed {
	DEVICE_CTRL_DOOR_CLOSED  = 0x00,
	DEVICE_CTRL_DOOR_OPEN    = 0x01,
	DEVICE_CTRL_DOOR_OPENING = 0x02,
	DEVICE_CTRL_DOOR_CLOSING = 0x03,
	DEVICE_CTRL_DOOR_UNKNOWN = 0xFF,
} device_ctrl_door_state_t;

/**
 * @brief Garage doors state payload.
 *
 * Carries the live state of the left door, central gate and right door.
 */
typedef struct __packed {
	device_ctrl_door_state_t left_door;  /**< Left garage door */
	device_ctrl_door_state_t gate;       /**< Central gate */
	device_ctrl_door_state_t right_door; /**< Right garage door */
} device_ctrl_garage_doors_state_t;

/**
 * @brief State type tag (first byte of every RX state frame).
 */
typedef enum __packed {
	DEVICE_CTRL_STATE_TYPE_GARAGE_DOORS = 0x01,
	/* Add future state types here */
} device_ctrl_state_type_t;

/**
 * @brief Union of all possible state payloads.
 *
 * Extend this union when new state types are introduced.
 */
typedef union __packed {
	device_ctrl_garage_doors_state_t garage_doors;
	/* Future state payloads go here */
} device_ctrl_state_payload_t;

/**
 * @brief Full state message received over the stream channel (RX wire format).
 *
 * Wire layout:
 *   - 1 byte:  state type  (@ref device_ctrl_state_type_t)
 *   - N bytes: state payload (@ref device_ctrl_state_payload_t, union member
 *              selected by @p type)
 */
typedef struct __packed {
	device_ctrl_state_type_t    type;
	device_ctrl_state_payload_t payload;
} device_ctrl_state_msg_t;

/*---------------------------------------------------------------------------
 * Message queues (defined in device_control.c)
 *---------------------------------------------------------------------------*/

/** TX queue – holds outgoing @ref device_ctrl_command_msg_t items. */
extern struct k_msgq device_control_tx_msgq;

/** RX queue – holds incoming @ref device_ctrl_state_msg_t items. */
extern struct k_msgq device_control_rx_msgq;

/*---------------------------------------------------------------------------
 * State update callback
 *---------------------------------------------------------------------------*/

/**
 * @brief Callback invoked (from the RX thread) whenever the garage-doors
 *        state is updated.
 *
 * The pointer is valid only for the duration of the call; copy the
 * contents if you need to keep them.
 */
typedef void (*device_ctrl_state_cb_t)(const device_ctrl_garage_doors_state_t *state);

/*---------------------------------------------------------------------------
 * API
 *---------------------------------------------------------------------------*/

/**
 * @brief Enqueue a command to be sent to the server.
 *
 * Thread-safe, non-blocking.  Returns -ENOMSG if the TX queue is full.
 *
 * @param cmd Command to send.
 * @return 0 on success, negative errno on failure.
 */
int device_control_send_cmd(device_ctrl_cmd_t cmd);

/**
 * @brief Return the most recently received garage-doors state.
 *
 * Thread-safe (protected by a spinlock).
 */
device_ctrl_garage_doors_state_t device_control_get_garage_doors_state(void);

/**
 * @brief Register a callback invoked by the RX thread on every state update.
 *
 * Only one callback is supported at a time.  Pass NULL to unregister.
 */
void device_control_set_state_cb(device_ctrl_state_cb_t cb);

/**
 * @brief Start the background RX processing thread.
 *
 * Must be called after stream_client_start().
 */
int device_control_start(void);

#endif /* _DEVICE_CONTROL_H */
