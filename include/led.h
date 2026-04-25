/*
 * Copyright (c) 2024 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _USB_LED_H
#define _USB_LED_H

#include <stdbool.h>
#include <stdint.h>

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

/**
 * @brief Start periodic LED blinking.
 *
 * @param period_ms Half-period in milliseconds (LED toggles every period_ms).
 * @return 0 on success, negative errno on failure.
 */
int board_led_blink_start(uint32_t period_ms);

/**
 * @brief Stop LED blinking (LED stays in its current state).
 */
void board_led_blink_stop(void);

#endif /* _USB_LED_H */