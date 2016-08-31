/*
 * as3722.h definitions
 *
 * Copyright (C) 2013 ams
 * Copyright (c) 2013, NVIDIA Corporation. All rights reserved.
 *
 * Author: Florian Lobmaier <florian.lobmaier@ams.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef __LINUX_MFD_AS3722_PLAT_H
#define __LINUX_MFD_AS3722_PLAT_H

#include <linux/types.h>

#define AS3722_EXT_CONTROL_ENABLE1		0x1
#define AS3722_EXT_CONTROL_ENABLE2		0x2
#define AS3722_EXT_CONTROL_ENABLE3		0x3

/* ADC */
enum as3722_adc_source {
	AS3722_ADC_SD0 = 0,
	AS3722_ADC_SD1 = 1,
	AS3722_ADC_SD6 = 2,
	AS3722_ADC_TEMP_SENSOR = 3,
	AS3722_ADC_VSUP = 4,
	AS3722_ADC_GPIO1 = 5,
	AS3722_ADC_GPIO2 = 6,
	AS3722_ADC_GPIO3 = 7,
	AS3722_ADC_GPIO4 = 8,
	AS3722_ADC_GPIO6 = 9,
	AS3722_ADC_GPIO7 = 10,
	AS3722_ADC_VBAT = 11,
	AS3722_ADC_PWM_CLK2 = 12,
	AS3722_ADC_PWM_DAT2 = 13,
	AS3722_ADC_TEMP1_SD0 = 16,
	AS3722_ADC_TEMP2_SD0 = 17,
	AS3722_ADC_TEMP3_SD0 = 18,
	AS3722_ADC_TEMP4_SD0 = 19,
	AS3722_ADC_TEMP_SD1 = 20,
	AS3722_ADC_TEMP1_SD6 = 21,
	AS3722_ADC_TEMP2_SD6 = 22,
};

enum as3722_adc_channel {
	AS3722_ADC0 = 0,
	AS3722_ADC1 = 1,
};

/* regulator IDs */
enum as3722_regulators_id_ {
	AS3722_SD0,
	AS3722_SD1,
	AS3722_SD2,
	AS3722_SD3,
	AS3722_SD4,
	AS3722_SD5,
	AS3722_SD6,
	AS3722_LDO0,
	AS3722_LDO1,
	AS3722_LDO2,
	AS3722_LDO3,
	AS3722_LDO4,
	AS3722_LDO5,
	AS3722_LDO6,
	AS3722_LDO7,
	AS3722_LDO9,
	AS3722_LDO10,
	AS3722_LDO11,
	AS3722_NUM_REGULATORS,
};

/* GPIO IDs */
enum as3722_gpio_id {
	 AS3722_GPIO0,
	 AS3722_GPIO1,
	 AS3722_GPIO2,
	 AS3722_GPIO3,
	 AS3722_GPIO4,
	 AS3722_GPIO5,
	 AS3722_GPIO6,
	 AS3722_GPIO7,
	 AS3722_NUM_GPIO,
};

/*
 * struct as3722_pinctrl_platform_data: Pincontrol platform data.
 * @pin: name of pin.
 * @function: Function option of pin. NULL for default.
 * @prop_bias_pull: Pull up, pull down and normal option. NULL for default.
 * @prop_open_drain: Open drain enable/disable. NULL for default.
 * @prop_high_impedance: High impedance enable/disable. NULL for default.
 * @prop_gpio_mode: GPIO mode, if pin function is in gpio, gpio mode
 *			like input, output-high and output-low.
 */
struct as3722_pinctrl_platform_data {
	const char *pin;
	const char *function;
	const char *prop_bias_pull;
	const char *prop_open_drain;
	const char *prop_high_impedance;
	const char *prop_gpio_mode;
};

/*
 * as3722_regulator_platform_data: Regulator platform data.
 * @ext_control: External control.
 */
struct as3722_regulator_platform_data {
	struct regulator_init_data *reg_init;
	int ext_control;
	bool enable_tracking;
	bool disable_tracking_suspend;
	bool volatile_vsel;
};

/*
 * as3722_adc_extcon_platform_data: ADC platform data.
 * @connection_name: Extcon connection name.
 */
struct as3722_adc_extcon_platform_data {
	const char *connection_name;
	bool enable_adc1_continuous_mode;
	bool enable_low_voltage_range;
	int adc_channel;
	int hi_threshold;
	int low_threshold;
};

struct as3722_platform_data {
	struct as3722_regulator_platform_data *reg_pdata[AS3722_NUM_REGULATORS];
	int gpio_base;
	int irq_base;
	int irq_type;
	int use_internal_int_pullup;
	int use_internal_i2c_pullup;
	int num_gpio_cfgs;
	bool use_power_off;
	bool use_power_reset;
	struct as3722_gpio_config *gpio_cfgs;
	struct as3722_pinctrl_platform_data *pinctrl_pdata;
	int num_pinctrl;
	struct as3722_adc_extcon_platform_data *extcon_pdata;
	int watchdog_timer_initial_period;
	int watchdog_timer_mode;
	u32 major_rev;
	u32 minor_rev;
	bool enable_clk32k_out;
};

#endif
