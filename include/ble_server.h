/*
 * Copyright (c) 2026 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BLE_SERVER_H
#define _BLE_SERVER_H

#include <stdint.h>

#include <zephyr/bluetooth/addr.h>

int ble_server_start(void);

#endif /* _BLE_SERVER_H */