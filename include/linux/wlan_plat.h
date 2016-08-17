/* include/linux/wlan_plat.h
 *
 * Copyright (c) 2010 Google, Inc.
 * Copyright (c) 2013 NVIDIA Corporation. All rights reserved.
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
#ifndef _LINUX_WLAN_PLAT_H_
#define _LINUX_WLAN_PLAT_H_

#include <linux/edp.h>

#if defined(CONFIG_BCMDHD_EDP_SUPPORT)
typedef enum e_edp_state {
	EDP_STATE_ON = 0,
	EDP_STATE_OFF
}wifi_edp_state;
#endif

struct wifi_platform_data {
	int (*set_power)(int val);
	int (*set_reset)(int val);
	int (*set_carddetect)(int val);
	void *(*mem_prealloc)(int section, unsigned long size);
	int (*get_mac_addr)(unsigned char *buf);
	void *(*get_country_code)(char *ccode);
#if defined(CONFIG_BCMDHD_EDP_SUPPORT)
	struct edp_client client_info;
#endif
};

#endif
