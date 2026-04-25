/*
 * Copyright (c) 2024 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/dfu/mcuboot.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/retention/bootmode.h>
#include <zephyr/sys/reboot.h>

#include <dfu.h>
#include <led.h>

LOG_MODULE_REGISTER(dfu, LOG_LEVEL_INF);

/* LED blink period while signalling DFU mode entry */
#define DFU_LED_BLINK_PERIOD_MS 100U
/* How long to blink before actually rebooting */
#define DFU_LED_BLINK_DURATION_MS 500U

int dfu_confirm_image(void)
{
	int ret = boot_write_img_confirmed();

	if (ret < 0) {
		LOG_ERR("Failed to confirm image: %d", ret);
	} else {
		LOG_INF("Firmware image confirmed");
	}
	return ret;
}

int dfu_enter_bootloader(void)
{
	int ret;

	LOG_INF("Entering MCUboot serial recovery mode ...");

	/* Rapid blink to make the transition visible on the LED */
	board_led_blink_start(DFU_LED_BLINK_PERIOD_MS);
	k_sleep(K_MSEC(DFU_LED_BLINK_DURATION_MS));

	ret = bootmode_set(BOOT_MODE_TYPE_BOOTLOADER);
	if (ret < 0) {
		LOG_ERR("Failed to set boot mode: %d", ret);
		board_led_blink_stop();
		return ret;
	}

	sys_reboot(SYS_REBOOT_COLD);

	/* Never reached */
	return 0;
}

/* Confirm the image automatically once the application initialises */
static int dfu_auto_confirm(void)
{
	return dfu_confirm_image();
}
SYS_INIT(dfu_auto_confirm, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
