/*
 *  isa1200.h - ISA1200 Haptic Motor driver
 *
 *  Copyright (C) 2009 Samsung Electronics
 *  Kyungmin Park <kyungmin.park@samsung.com>
 *  Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_ISA1200_H
#define __LINUX_ISA1200_H

#define ISA_I2C_VTG_MAX_UV		1800000
#define ISA_I2C_VTG_MIN_UV		1800000
#define ISA_I2C_CURR_UA			9630

struct isa1200_regulator {
	const char *name;
	u32	min_uV;
	u32	max_uV;
	u32	load_uA;
};

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
	unsigned int hap_len_gpio;
	bool overdrive_high; /* high/low overdrive */
	bool overdrive_en; /* enable/disable overdrive */
	enum mode_control mode_ctrl; /* input/generation/wave */
	union pwm_div_freq pwm_fd;
	bool smart_en; /* smart mode enable/disable */
	bool is_erm;
	bool ext_clk_en;
	bool need_pwm_clk;
	unsigned int chip_en;
	unsigned int duty;
	struct isa1200_regulator *regulator_info;
	u8 num_regulators;
	int (*power_on)(int on);
	int (*dev_setup)(bool on);
	int (*clk_enable)(bool on);
};

#endif /* __LINUX_ISA1200_H */
