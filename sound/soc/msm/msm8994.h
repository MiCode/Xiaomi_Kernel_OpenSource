/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#ifndef MSM_MSM8994_H
#define MSM_MSM8994_H

enum pinctrl_pin_state {
	STATE_DISABLE = 0,
	STATE_AUXPCM_ACTIVE,
	STATE_MI2S_ACTIVE,
	STATE_ACTIVE
};

struct msm_pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *disable;
	struct pinctrl_state *mi2s_active;
	struct pinctrl_state *auxpcm_active;
	struct pinctrl_state *active;
	enum pinctrl_pin_state curr_state;
	void __iomem *mux;
};

struct msm8994_asoc_mach_data {
	int mclk_gpio;
	u32 mclk_freq;
	int us_euro_gpio;
	uint32_t curr_hs_impedance;
	struct msm_pinctrl_info pinctrl_info;
	void __iomem *pri_mux;
	void __iomem *sec_mux;
	struct msm_pinctrl_info pri_mi2s_pinctrl_info;
	struct msm_pinctrl_info sec_mi2s_pinctrl_info;
	struct msm_pinctrl_info tert_mi2s_pinctrl_info;
	struct msm_pinctrl_info quat_mi2s_pinctrl_info;
};

#endif /* MSM_MSM8994_H */
