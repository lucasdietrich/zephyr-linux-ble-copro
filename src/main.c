/*
 * Copyright (c) 2024 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ble_server.h"
#include <complex.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/settings/settings.h>

#include <ble_observer.h>
#include <led.h>
#include <button.h>
#include <device_control.h>
#include <linky.h>
#include <stream_client.h>
#include <usb_net.h>
#include <xiaomi.h>

LOG_MODULE_REGISTER(ble, LOG_LEVEL_INF);

int main(void)
{
	int ret;

#if CONFIG_COPRO_LED
	/* Led initialization */
	ret = board_led_init();
	if (ret != 0) {
		LOG_ERR("Failed to initialize LED (ret %d)", ret);
		return ret;
	}
#endif

	ret = board_button_init();
	if (ret != 0) {
		LOG_ERR("Failed to initialize button (ret %d)", ret);
		return ret;
	}

#if CONFIG_COPRO_USB_NETWORK
	/* Initialize NET interface management */
	usb_net_iface_init();
#endif

#if !CONFIG_USB_DEVICE_INITIALIZE_AT_BOOT
	/* Initialize the USB Subsystem */
	ret = usb_enable(NULL);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB (red %d)", ret);
		return ret;
	}

	LOG_INF("USB initialized %d", 0);
#endif

	/* Initialize the Bluetooth Subsystem */
	ret = bt_enable(NULL);
	if (ret != 0) {
		LOG_ERR("Bluetooth init failed (ret %d)", ret);
		return ret;
	}

	ret = settings_load();
	if (ret != 0) {
		LOG_ERR("Failed to load settings (ret %d)", ret);
		return ret;
	}

	LOG_INF("Bluetooth initialized %d", 0);

	/* Start the BLE observer thread */
	ble_observer_start();

#if CONFIG_COPRO_BLE_SERVER
	ret = ble_server_start();
	if (ret != 0) {
		LOG_ERR("Failed to start BLE server (ret %d)", ret);
		return ret;
	}
#endif

#if CONFIG_COPRO_XIAOMI_LYWSD03MMC
	/* Configure the stream client */
	ret = stream_client_channel_add(
		SC_ID_XIAOMI, SC_NAME_XIAOMI, &xiaomi_msgq, NULL);
	if (ret < 0) {
		LOG_ERR("Failed to add xiaomi channel to stream client: %d", ret);
		return ret;
	}
#endif /* CONFIG_COPRO_XIAOMI_LYWSD03MMC */

#if CONFIG_COPRO_LINKY_TIC
	/* Configure the stream client */
	ret = stream_client_channel_add(
		SC_ID_LINKY_TIC, SC_NAME_LINKY_TIC, &linky_msgq, NULL);
	if (ret < 0) {
		LOG_ERR("Failed to add linky channel to stream client: %d", ret);
		return ret;
	}
#endif /* CONFIG_COPRO_LINKY_TIC */

#if CONFIG_COPRO_DEVICE_CONTROL
	ret = stream_client_channel_add(SC_ID_DEVICE_CONTROL,
									SC_NAME_DEVICE_CONTROL,
									&device_control_tx_msgq,
									&device_control_rx_msgq);
	if (ret < 0) {
		LOG_ERR("Failed to register device-control channel: %d", ret);
		return ret;
	}

	device_control_start();
#endif /* CONFIG_COPRO_DEVICE_CONTROL */

	/* Start the stream client after all channels have been added */
	stream_client_start();

	return 0;
}