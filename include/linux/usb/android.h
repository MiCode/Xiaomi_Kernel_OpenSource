/*
 * Platform data for Android USB
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef	__LINUX_USB_ANDROID_H
#define	__LINUX_USB_ANDROID_H

#include <linux/usb/composite.h>

struct android_usb_platform_data {
	int (*update_pid_and_serial_num)(uint32_t, const char *);
	u32 swfi_latency;
	u8 usb_core_id;
	bool cdrom;
};

#ifndef CONFIG_TARGET_CORE
static inline int f_tcm_init(int (*connect_cb)(bool connect))
{
	/*
	 * Fail bind() not init(). If a function init() returns error
	 * android composite registration would fail.
	 */
	return 0;
}
static inline void f_tcm_exit(void)
{
}
static inline int tcm_bind_config(struct usb_configuration *c)
{
	return -ENODEV;
}
#endif

#endif	/* __LINUX_USB_ANDROID_H */
