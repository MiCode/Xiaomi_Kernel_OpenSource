/*
 * Internal platform definitions for msm/qsd touchscreen devices
 *
 * Copyright (C) 2008 Google Incorporated
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ASM_ARCH_MSM_TS_H
#define __ASM_ARCH_MSM_TS_H

#include <linux/input.h>

/* The dimensions for the virtual key are for the other axis, i.e. if
 * virtual keys are in the Y dimension then min/max is the range in the X
 * dimension where that key would be activated */
struct ts_virt_key {
	int key;
	int min;
	int max;
};

struct msm_ts_virtual_keys {
	struct ts_virt_key	*keys;
	int			num_keys;
};

struct msm_ts_platform_data {
	uint32_t			min_x;
	uint32_t			max_x;
	uint32_t			min_y;
	uint32_t			max_y;
	uint32_t			min_press;
	uint32_t			max_press;
	struct msm_ts_virtual_keys	*vkeys_x;
	uint32_t			virt_x_start;
	struct msm_ts_virtual_keys	*vkeys_y;
	uint32_t			virt_y_start;
	uint32_t			inv_x;
	uint32_t			inv_y;
	bool				can_wakeup;
};

#endif /* __ASM_ARCH_MSM_TS_H */
