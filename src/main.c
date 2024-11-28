/*
 * Copyright (c) 2024 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>

#include <ble_observer.h>
#include <usb_net.h>
#include <stream_client.h>

LOG_MODULE_REGISTER(ble, LOG_LEVEL_INF);

int main(void)
{
	int ret;

	printk("Hello, World!\n");

	/* Initialize NET interface management */
	usb_net_iface_init();

	/* Initialize the USB Subsystem */
	ret = usb_enable(NULL);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return ret;
	}

	LOG_INF("USB initialized %d", 0);

	/* Initialize the Bluetooth Subsystem */
	ret = bt_enable(NULL);
	if (ret != 0) {
		LOG_ERR("Bluetooth init failed (ret %d)", ret);
		return ret;
	}

	LOG_INF("Bluetooth initialized %d", 0);

	/* Start the BLE observer thread */
	ble_observer_start();

	/* Start the streamc client */
	stream_client_init();
	
	return 0;
}