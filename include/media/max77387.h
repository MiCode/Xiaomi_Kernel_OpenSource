/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.

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

#ifndef __MAX77387_H__
#define __MAX77387_H__

#include <media/nvc_torch.h>

struct max77387_power_rail {
	/* to enable the module power */
	struct regulator *vin;
	/* to enable the host interface power */
	struct regulator *vdd;
};

struct max77387_led_config {
	u16 color_setting;
	u16 flash_torch_ratio;	/* max flash to max torch ratio, in 1/1000 */
	u16 granularity;	/* 1, 10, 100, ... to carry float settings */
	u16 flash_levels;	/* calibrated flash levels < 32 */
	/* this table contains the calibrated flash level - luminance pair */
	struct nvc_torch_lumi_level_v1 *lumi_levels;
};

struct max77387_config {
	u32 led_mask;		/* led(s) enabled, 1/2/3 - left/right/both */
	bool synchronized_led;  /* if both leds enabled, consider as one. */
	u16 flash_trigger_mode;	/* 0, 3=flash is triggered via i2c interface.
				   1=high level on the flash_en pin will turn
				   the flash on.
				   2=high level on the torch_en pin will turn
				   the flash on.
				   4=high level on both torch_en and flash_en
				   pin will turn the flash on.
				   5=high level on the torch_en or flash_en pin
				   will turn the flash on. */
	u16 flash_mode;		/* 1=one_shot_mode, flash is triggerred on the
				   rising edge of FLASHEN/TORCHEN/I2C_bit, and
				   terminated based on the flash safety timer
				   value.
				   0, 2=run for MAX timer, flash is triggerred
				   on the rising edge of FLASHEN/TORCHEN/I2C,
				   and terminated based on the falling edge of
				   FLASHEN/TORCHEN/I2C_bit and flash safety
				   timer value, whichever comes first.*/
	u16 torch_trigger_mode;	/* 0, 3=torch is triggered via i2c interface.
				   1=high level on the flash_en pin will turn
				   the torch on.
				   2=high level on the torch_en pin will turn
				   the torch on.
				   4=high level on both torch_en and flash_en
				   pin will turn the torch on.
				   5=high level on the torch_en or flash_en pin
				   will turn the torch on. */
	u16 torch_mode;		/* 1=torch safety timer disabled, torch is
				   controlled solely by the FLASHEN/TORCHEN/I2C.
				   2=one_shot_mode, torch is triggerred on the
				   rising edge of FLASHEN/TORCHEN/I2C_bit, and
				   terminated based on the torch safety timer
				   setting.
				   0, 3=run MAX timer, torch is triggerred on
				   the rising edge of FLASHEN/TORCHEN/I2C_bit,
				   and terminated based on the falling edge of
				   FLASHEN/TORCHEN/ I2C_bit and torch safety
				   timer setting, whichever comes first.*/
	u16 adaptive_mode;	/* 1=fix mode, 2=adaptive mode */
	/* TX MASK settings */
	u16 tx1_mask_mA;
	u16 tx2_mask_mA;
	/* flash/torch ramp settings */
	u16 flash_rampup_uS;
	u16 flash_rampdn_uS;
	u16 torch_rampup_uS;
	u16 torch_rampdn_uS;
	/* default flash timer */
	u16 def_ftimer;
	/* LED configuration, two identical leds must be connected. */
	u16 max_total_current_mA; /* Both leds' maximum peak current in mA */
	u16 max_peak_current_mA; /* This led's maximum peak current in mA */
	u16 max_torch_current_mA; /* This leds maximum torch current in mA */
	u16 max_peak_duration_ms; /* the maximum duration max_peak_current_mA
				     can be applied */
	u16 max_flash_threshold_mV;  /* low battery detection threshold.
					2400mV ~ 3400mV. */
	u16 max_flash_hysteresis_mV; /* low battery detection hysteresis.
					100mV ~ 300mV */
	u16 max_flash_lbdly_f_uS; /* Low battery delay timer for falling edge
					detection. Adjustable from 256uS to
					2048uS in 256uS steps. */
	u16 max_flash_lbdly_r_uS; /* Low battery delay timer for raising edge
					detection. Adjustable from 256uS to
					2048uS in 256uS steps. */
	struct max77387_led_config led_config[2];
};

struct max77387_platform_data {
	struct max77387_config config;
	unsigned cfg; /* use the NVC_CFG_ defines */
	unsigned num; /* see implementation notes in driver */
	unsigned sync; /* see implementation notes in driver */
	const char *dev_name; /* see implementation notes in driver */
	struct nvc_torch_pin_state pinstate; /* see notes in driver */
	unsigned gpio_strobe; /* GPIO connected to the ACT signal */

	int (*poweron_callback)(struct max77387_power_rail *pw);
	int (*poweroff_callback)(struct max77387_power_rail *pw);
};

#endif
/* __MAX77387_H__ */
