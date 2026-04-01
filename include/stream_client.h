/*
 * Copyright (c) 2024 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _STREAM_CLIENT_H
#define _STREAM_CLIENT_H

#include <stddef.h>
#include <stdint.h>

#include <zephyr/kernel.h>

int stream_client_start(void);

bool stream_client_is_connected(void);

/**
 * @brief Callback invoked when the TCP stream server connection state changes.
 *
 * @param connected true when a connection is established, false on disconnect.
 */
typedef void (*stream_client_conn_cb_t)(bool connected);

void stream_client_set_conn_cb(stream_client_conn_cb_t cb);

int stream_client_channel_add(uint32_t channel_id,
							  const char *name,
							  struct k_msgq *tx_msgq,
							  struct k_msgq *rx_msgq);

int stream_try_connect(void);

#endif /* _STREAM_CLIENT_H */