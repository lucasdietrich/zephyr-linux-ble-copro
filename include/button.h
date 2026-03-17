/*
 * Copyright (c) 2024 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _USB_BUTTON_H
#define _USB_BUTTON_H

#include <stdint.h>
#if CONFIG_COPRO_BUTTON
#define BUTTON_IS_PRESSED()  board_button_is_pressed()
#else
#define BUTTON_IS_PRESSED() 0
#endif

extern bool button_pressed_flag;

int board_button_init(void);

int board_button_is_pressed(void);

#endif /* _USB_LED_H */