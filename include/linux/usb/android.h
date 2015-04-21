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

#define MAX_STREAMING_FUNCS 3
#define FUNC_NAME_LEN 10

enum android_pm_qos_state {
	WFI,
	IDLE_PC,
	IDLE_PC_RPM,
	NO_USB_VOTE,
	MAX_VOTES = NO_USB_VOTE,
};

struct android_usb_platform_data {
	int (*update_pid_and_serial_num)(uint32_t, const char *);
	u32 pm_qos_latency[MAX_VOTES];
	u8 usb_core_id;
	char streaming_func[MAX_STREAMING_FUNCS][FUNC_NAME_LEN];
	int  streaming_func_count;
};

#endif	/* __LINUX_USB_ANDROID_H */
