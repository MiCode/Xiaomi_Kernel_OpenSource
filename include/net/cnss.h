/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/device.h>

/* max 20mhz channel count */
#define CNSS_MAX_CH_NUM       45

struct dev_info {
	struct device	*dev;
	char	*dump_buffer;
	unsigned long dump_size;
	int (*dev_shutdown)(void);
	int (*dev_powerup)(void);
	void (*dev_crashshutdown)(void);
};

extern int cnss_config(struct dev_info *device_info);
extern void cnss_deinit(void);
extern void cnss_device_crashed(void);
extern int cnss_set_wlan_unsafe_channel(u16 *unsafe_ch_list, u16 ch_count);
extern int cnss_get_wlan_unsafe_channel(u16 *unsafe_ch_list,
						u16 *ch_count, u16 buf_len);
