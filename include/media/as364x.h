/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.

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

#ifndef __AS364X_H__
#define __AS364X_H__

#include <media/nvc_torch.h>

enum {
	AS3643,
	AS3647,
	AS3648,
	AS364X_NUM,
};

struct as364x_led_config {
	u16 color_setting;
	u16 flash_torch_ratio;	/* max flash to max torch ratio, in 1/1000 */
	u16 granularity;	/* 1, 10, 100, ... to carry float settings */
	u16 flash_levels;	/* calibrated flash levels < 32 */
	/* this table contains the calibrated flash level - luminance pair */
	struct nvc_torch_lumi_level_v1 *lumi_levels;
};

struct as364x_config {
	u32 led_mask; /* which led(s) enabled, 1/2/3 - left/right/both */
	bool synchronized_led;  /* if both leds enabled, consider as one. */
	bool use_tx_mask; /* enable TXMASK */

	u16 I_limit_mA; /* AS3647/AS3648: 2000, 2500, 3000, 3500 mA
			 * AS3643: 1000, 1500, 2000, 2500 mA for the coil*/
	u16 txmasked_current_mA; /* 57,113,...847, roughly 56.47 mA steps */
	u16 vin_low_v_run_mV; /* 0xffff=off, 3000, 3070, 3140, 3220, 3300,
				3338, 3470 mV battery limit for dynamic flash
				 reduction */
	u16 vin_low_v_mV; /* 0xffff=off, 3000, 3070, 3140, 3220, 3300, 3338,
				3470mV battery limit for flash denial */
	u8 strobe_type; /* 1=edge, 2=level, 3=i2c */
	u8 inct_pwm; /* pwm duty cycle for indicator or low current mode */
	bool freq_switch_on;
	/* balance the current sinks for unmatched LED forward valtages */
	bool load_balance_on;
	bool led_off_when_vin_low; /* if 0 txmask current is used */
	bool boost_mode; /* all LED current are increased by 11% */
	/* LED configuration, two identical leds must be connected. */
	u16 max_total_current_mA; /* Both leds' maximum peak current in mA */
	u16 max_peak_current_mA; /* This led's maximum peak current in mA */
	u16 max_torch_current_mA; /* This leds maximum torch current in mA */
	u16 max_peak_duration_ms; /* the maximum duration max_peak_current_mA
				     can be applied */
	u16 max_sustained_current_mA; /* This leds maximum sustained current
					 in mA */
	u16 min_current_mA; /* This leds minimum current in mA, desired
			       values smaller than this will be realised
			       using PWM. */
	u8 def_ftimer; /* default flash timer setting 0 ~ 255, 2ms ~ 1280mS */
	struct as364x_led_config led_config[2];
};

struct as364x_power_rail {
	struct regulator *v_in;
	struct regulator *v_i2c;
};

struct as364x_platform_data {
	struct as364x_config config;
	u32 type; /* flash device type, refer to as364x_type */
	unsigned cfg; /* use the NVC_CFG_ defines */
	unsigned num; /* see implementation notes in driver */
	const char *dev_name; /* see implementation notes in driver */
	struct nvc_torch_pin_state pinstate; /* see notes in driver */
	unsigned gpio_strobe; /* GPIO connected to the ACT signal */
	bool strobe_low_act; /* strobe state active low */

	int (*power_on_callback)(struct as364x_power_rail *pw);
	int (*power_off_callback)(struct as364x_power_rail *pw);
};

#endif
/* __AS364X_H__ */
