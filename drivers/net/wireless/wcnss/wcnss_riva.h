/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#ifndef _WCNSS_RIVA_H_
#define _WCNSS_RIVA_H_

#include <linux/device.h>

enum wcnss_opcode {
	WCNSS_WLAN_SWITCH_OFF = 0,
	WCNSS_WLAN_SWITCH_ON,
};

struct wcnss_wlan_config {
	int		use_48mhz_xo;
};

int wcnss_wlan_power(struct device *dev,
		struct wcnss_wlan_config *cfg,
		enum wcnss_opcode opcode);

#endif /* _WCNSS_RIVA_H_ */
