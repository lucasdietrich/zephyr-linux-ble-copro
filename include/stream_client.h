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

int stream_client_channel_add(uint32_t channel_id, const char *name, struct k_msgq *msgq);

int stream_try_connect(void);

#endif /* _STREAM_CLIENT_H */