/* arch/arm/mach-msm/include/mach/msm_touch.h
 *
 * Platform data for MSM touchscreen driver.
 *
 * Copyright (c) 2008-2009, The Linux Foundation. All rights reserved.
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

#ifndef _MACH_MSM_TOUCH_H_
#define _MACH_MSM_TOUCH_H_

struct msm_ts_platform_data {
	unsigned int x_max;
	unsigned int y_max;
	unsigned int pressure_max;
};

#endif
