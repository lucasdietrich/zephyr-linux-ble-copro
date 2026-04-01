/*
 * Copyright (c) 2024 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _USB_LED_H
#define _USB_LED_H

#include <stdbool.h>
#if CONFIG_COPRO_LED
#define LED_ON()  board_led_on()
#define LED_OFF() board_led_off()
#else
#define LED_ON()
#define LED_OFF()
#endif

int board_led_init(void);

int board_led_on(void);

int board_led_off(void);

int board_led_set(bool on);

#endif /* _USB_LED_H */