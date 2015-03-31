/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 * Copyright (C) 2015 XiaoMi, Inc.
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

#ifndef MSM_MSM8974_H
#define MSM_MSM8974_H

struct msm_auxpcm_gpio {
	unsigned gpio_no;
	const char *gpio_name;
};

struct msm_auxpcm_ctrl {
	struct msm_auxpcm_gpio *pin_data;
	u32 cnt;
	void __iomem *mux;
};

struct msm8974_asoc_mach_data {
	int mclk_gpio;
	u32 mclk_freq;
	int us_euro_gpio;
	uint32_t curr_hs_impedance;
	struct msm_auxpcm_ctrl *pri_auxpcm_ctrl;
	struct msm_auxpcm_ctrl *sec_auxpcm_ctrl;
};

#endif /* MSM_MSM8974_H */
