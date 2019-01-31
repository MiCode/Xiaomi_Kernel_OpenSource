/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/regulator/consumer.h>

struct mt_irtx {
	unsigned int pwm_ch;
	unsigned int pwm_data_invert;
	void __iomem *reg_base;
	unsigned int irq;
	struct platform_device *plat_dev;
	unsigned int carrier_freq;
	atomic_t usage_cnt;
	struct clk *clk_irtx_main;
	/* GPIO pin control */
	struct pinctrl *ppinctrl_irtx;
	/* for MT6370 sub-pmic control */
	struct regulator *buck;
};

struct irtx_config {
	unsigned int start : 1;
	unsigned int mode : 2;
	unsigned int sw_o : 1;
	unsigned int b_ord : 1;
	unsigned int r_ord : 1;
	unsigned int ir_os : 1;
	unsigned int ir_inv : 1;
	unsigned int bit_num : 7;
	unsigned int data_inv : 1;
};

#define IRTX_IOC_SET_CARRIER_FREQ   _IOW('R', 0, unsigned int)
#define IRTX_IOC_GET_SOLUTTION_TYPE _IOR('R', 1, unsigned int)
#define IRTX_IOC_SET_IRTX_LED_EN    _IOW('R', 10, unsigned int)

