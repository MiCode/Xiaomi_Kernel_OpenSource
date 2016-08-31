/*
 * Copyright (c) 2012 - 2013, NVIDIA CORPORATION.  All rights reserved.

 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.

 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __LM3565_H__
#define __LM3565_H__

#include <media/nvc_torch.h>

struct lm3565_led_config {
	u16 color_setting;
	u16 granularity;	/* 1, 10, 100, ... to carry float settings */
	u16 flash_levels;	/* calibrated flash levels < 32 */
	/* this table contains the calibrated flash level - luminance pair */
	struct nvc_torch_lumi_level_v1 *lumi_levels;
};

struct lm3565_config {
	u16 txmask_current_mA; /* 30, 60, ... 480, in 30 mA steps */
	u16 txmask_inductor_mA; /* 2300, 2600, 2900, 3300 */
	u16 vin_low_v_mV; /* 0=off, 3000, 3100, 3200, 3300, 3400, 3500,
				3600mV, 3700mV battery limit for flash denial */
	u16 vin_low_c_mA; /* 0=off, 150, 180, 210, 240 */
	u8 strobe_type; /* 1=edge, 2=level, 3=i2c */
	u16 max_peak_current_mA; /* This led's maximum peak current in mA */
	u16 max_peak_duration_ms; /* the maximum duration max_peak_current_mA
				     can be applied */
	u16 max_torch_current_mA; /* This leds maximum torch current in mA */
	u16 max_sustained_current_mA; /* This leds maximum sustained current
					 in mA */
	u16 min_current_mA; /* This leds minimum current in mA, desired
			       values smaller than this will be realised
			       using PWM. */
	/* default flash timer setting in mS, zero will be ignored. */
	u16 def_flash_time_mS;
	struct lm3565_led_config led_config;
};

struct lm3565_power_rail {
	struct regulator *v_in;
	struct regulator *v_i2c;
	struct nvc_gpio enable_gpio;
};

struct lm3565_platform_data {
	struct lm3565_config config;
	u32 type; /* flash device type, refer to lm3565_type */
	unsigned cfg; /* use the NVC_CFG_ defines */
	unsigned num; /* see implementation notes in driver */
	const char *dev_name; /* see implementation notes in driver */
	struct nvc_torch_pin_state pinstate; /* see notes in driver */
	/* GPIO configuration connected to the ACT signal */
	struct nvc_gpio_pdata strobe_gpio;
	/* GPIO configuration connected to the enable pin */
	struct nvc_gpio_pdata enable_gpio;

	int (*power_on_callback)(struct lm3565_power_rail *pw);
	int (*power_off_callback)(struct lm3565_power_rail *pw);
};

#endif
/* __LM3565_H__ */
