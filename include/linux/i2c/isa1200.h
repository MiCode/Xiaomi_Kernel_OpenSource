/*
 *  isa1200.h - ISA1200 Haptic Motor driver
 *
 *  Copyright (C) 2009 Samsung Electronics
 *  Kyungmin Park <kyungmin.park@samsung.com>
 *  Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_ISA1200_H
#define __LINUX_ISA1200_H

enum mode_control {
	POWER_DOWN_MODE = 0,
	PWM_INPUT_MODE,
	PWM_GEN_MODE,
	WAVE_GEN_MODE
};

union pwm_div_freq {
	unsigned int pwm_div; /* PWM gen mode */
	unsigned int pwm_freq; /* PWM input mode */
};

struct isa1200_platform_data {
	const char *name;
	unsigned int pwm_ch_id; /* pwm channel id */
	unsigned int max_timeout;
	unsigned int hap_en_gpio;
	bool overdrive_high; /* high/low overdrive */
	bool overdrive_en; /* enable/disable overdrive */
	enum mode_control mode_ctrl; /* input/generation/wave */
	union pwm_div_freq pwm_fd;
	bool smart_en; /* smart mode enable/disable */
	bool is_erm;
	bool ext_clk_en;
	unsigned int chip_en;
	unsigned int duty;
	int (*power_on)(int on);
	int (*dev_setup)(bool on);
};

#endif /* __LINUX_ISA1200_H */
