/*
* Copyright (C) 2012 Texas Instruments
 * Copyright (C) 2018 XiaoMi, Inc.
*
* License Terms: GNU General Public License v2
*
* Simple driver for Texas Instruments LM3644 LED driver chip
*
* Author: Tao, Jun <taojun@xiaomi.com>
*/

#ifndef __LINUX_LM3644_H
#define __LINUX_LM3644_H

#define LM3644_NAME "leds-lm3644"

struct lm3644_platform_data {
	int tx_gpio;
	int torch_gpio;
	int hwen_gpio;
	int ir_prot_time;

	/* Simulative PWM settings */
	bool use_simulative_pwm;
	unsigned int pwm_period_us;
	unsigned int pwm_duty_us;
};

#endif /* __LINUX_LM3644_H */
