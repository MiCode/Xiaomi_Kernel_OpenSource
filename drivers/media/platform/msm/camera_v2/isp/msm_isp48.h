/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_ISP48_H__
#define __MSM_ISP48_H__

extern struct msm_vfe_hardware_info vfe48_hw_info;

enum msm_vfe_clk_rates {
	MSM_VFE_CLK_RATE_SVS = 0,
	MSM_VFE_CLK_RATE_NOMINAL = 1,
	MSM_VFE_CLK_RATE_TURBO = 2,
	MSM_VFE_MAX_CLK_RATES = 3,
};

#endif /* __MSM_ISP48_H__ */
