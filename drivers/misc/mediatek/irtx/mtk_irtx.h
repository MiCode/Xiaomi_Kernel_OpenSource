/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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

#define IRTX_IOC_SET_CARRIER_FREQ   _IOW('R', 0, unsigned int)
#define IRTX_IOC_GET_SOLUTTION_TYPE _IOR('R', 1, unsigned int)
#define IRTX_IOC_SET_DUTY_CYCLE     _IOW('R', 2, unsigned int)
#define IRTX_IOC_SET_IRTX_LED_EN    _IOW('R', 10, unsigned int)
#ifdef CONFIG_COMPAT
#define COMPAT_IRTX_IOC_SET_CARRIER_FREQ   _IOW('R', 0, compat_uint_t)
#define COMPAT_IRTX_IOC_GET_SOLUTTION_TYPE _IOR('R', 1, compat_uint_t)
#define COMPAT_IRTX_IOC_SET_DUTY_CYCLE     _IOW('R', 2, compat_uint_t)
#define COMPAT_IRTX_IOC_SET_IRTX_LED_EN    _IOW('R', 10, compat_uint_t)
#endif

