/*
 * Copyright (c) 2024 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _DFU_H
#define _DFU_H

/**
 * @brief Confirm that the currently running firmware is good.
 *
 * Must be called once the application is considered operational.  If this is
 * not called before the next reset, MCUboot will treat the update as failed
 * and revert to the previous image.
 *
 * @return 0 on success, negative errno on failure.
 */
int dfu_confirm_image(void);

/**
 * @brief Enter MCUboot serial-recovery (DFU) mode.
 *
 * Sets the retention boot-mode flag to BOOT_MODE_TYPE_BOOTLOADER, blinks the
 * LED rapidly to signal the transition, then performs a cold reboot.
 * The MCUboot bootloader will detect the flag and enter USB CDC-ACM serial
 * recovery so the host can upload a new firmware image via mcumgr.
 *
 * This function does not return on success.
 *
 * @return Negative errno if setting the boot mode failed (function returns).
 */
int dfu_enter_bootloader(void);

#endif /* _DFU_H */
