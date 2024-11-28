/*
 * Copyright (c) 2024 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BLE_OBSERVER_H
#define _BLE_OBSERVER_H

#include <stdint.h>

#include <zephyr/bluetooth/addr.h>

int ble_observer_start(void);

#endif /* _BLE_OBSERVER_H */