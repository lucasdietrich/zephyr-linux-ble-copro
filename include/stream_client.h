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

int stream_client_init(void);

int stream_try_connect(void);

typedef struct stream_channel {
	char name[32];		 // channel name
	uint32_t channel_id; // channel id
	size_t pl_size;		 // Size of the payload sent on this channel in bytes
	uint32_t flags;		 // channel flags
} stream_channel_t;

int stream_channel_setup(uint32_t channel_id, struct k_msgq *msgq, size_t pl_size);

int stream_send_data(uint32_t channel_id, void *data, size_t len);

#endif /* _STREAM_CLIENT_H */