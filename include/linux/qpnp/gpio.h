/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <mach/qpnp.h>

#define QPNP_GPIO_DIR_IN			0
#define QPNP_GPIO_DIR_OUT			1
#define QPNP_GPIO_DIR_BOTH			2

#define QPNP_GPIO_INVERT_DISABLE		0
#define QPNP_GPIO_INVERT_ENABLE			1

#define QPNP_GPIO_OUT_BUF_CMOS			0
#define QPNP_GPIO_OUT_BUF_OPEN_DRAIN_NMOS	1
#define QPNP_GPIO_OUT_BUF_OPEN_DRAIN_PMOS	2

#define QPNP_GPIO_VIN0				0
#define QPNP_GPIO_VIN1				1
#define QPNP_GPIO_VIN2				2
#define QPNP_GPIO_VIN3				3
#define QPNP_GPIO_VIN4				4
#define QPNP_GPIO_VIN5				5
#define QPNP_GPIO_VIN6				6
#define QPNP_GPIO_VIN7				7

#define QPNP_GPIO_PULL_UP_30			0
#define QPNP_GPIO_PULL_UP_1P5			1
#define QPNP_GPIO_PULL_UP_31P5			2
#define QPNP_GPIO_PULL_UP_1P5_30		3
#define QPNP_GPIO_PULL_DN			4
#define QPNP_GPIO_PULL_NO			5

#define QPNP_GPIO_OUT_STRENGTH_LOW		1
#define QPNP_GPIO_OUT_STRENGTH_MED		2
#define QPNP_GPIO_OUT_STRENGTH_HIGH		3

#define QPNP_GPIO_SRC_FUNC_NORMAL		0
#define QPNP_GPIO_SRC_FUNC_PAIRED		1
#define QPNP_GPIO_SRC_FUNC_1			2
#define QPNP_GPIO_SRC_FUNC_2			3
#define QPNP_GPIO_SRC_DTEST1			4
#define QPNP_GPIO_SRC_DTEST2			5
#define QPNP_GPIO_SRC_DTEST3			6
#define QPNP_GPIO_SRC_DTEST4			7

#define QPNP_GPIO_MASTER_DISABLE		0
#define QPNP_GPIO_MASTER_ENABLE			1

/**
 * struct qpnp_gpio_cfg - structure to specify gpio configurtion values
 * @direction:		indicates whether the gpio should be input, output, or
 *			both. Should be of the type QPNP_GPIO_DIR_*
 * @output_type:	indicates gpio should be configured as CMOS or open
 *			drain. Should be of the type QPNP_GPIO_OUT_BUF_*
 * @invert:		Invert the signal of the gpio line -
 *			QPNP_GPIO_INVERT_DISABLE or QPNP_GPIO_INVERT_ENABLE
 * @pull:		Indicates whether a pull up or pull down should be
 *			applied. If a pullup is required the current strength
 *			needs to be specified. Current values of 30uA, 1.5uA,
 *			31.5uA, 1.5uA with 30uA boost are supported. This value
 *			should be one of the QPNP_GPIO_PULL_*
 * @vin_sel:		specifies the voltage level when the output is set to 1.
 *			For an input gpio specifies the voltage level at which
 *			the input is interpreted as a logical 1.
 * @out_strength:	the amount of current supplied for an output gpio,
 *			should be of the type QPNP_GPIO_STRENGTH_*
 * @source_sel:		choose alternate function for the gpio. Certain gpios
 *			can be paired (shorted) with each other. Some gpio pin
 *			can act as alternate functions. This parameter should
 *			be of type QPNP_GPIO_SRC_*.
 * @master_en:		QPNP_GPIO_MASTER_ENABLE = Enable features within the
 *			GPIO block based on configurations.
 *			QPNP_GPIO_MASTER_DISABLE = Completely disable the GPIO
 *			block and let the pin float with high impedance
 *			regardless of other settings.
 */
struct qpnp_gpio_cfg {
	unsigned int direction;
	unsigned int output_type;
	unsigned int invert;
	unsigned int pull;
	unsigned int vin_sel;
	unsigned int out_strength;
	unsigned int src_select;
	unsigned int master_en;
};

/**
 * qpnp_gpio_config - Apply gpio configuration for Linux gpio
 * @gpio: Linux gpio number to configure.
 * @param: parameters to configure.
 *
 * This routine takes a Linux gpio number that corresponds with a
 * PMIC gpio and applies the configuration specified in 'param'.
 * This gpio number can be ascertained by of_get_gpio_flags() or
 * the qpnp_gpio_map_gpio() API.
 */
int qpnp_gpio_config(int gpio, struct qpnp_gpio_cfg *param);

/**
 * qpnp_gpio_map_gpio - Obtain Linux GPIO number from device spec
 * @slave_id: slave_id of the spmi_device for the gpio in question.
 * @pmic_gpio: PMIC gpio number to lookup.
 *
 * This routine is used in legacy configurations that do not support
 * Device Tree. If you are using Device Tree, you should not use this.
 * For such cases, use of_get_gpio() instead.
 */
int qpnp_gpio_map_gpio(uint16_t slave_id, uint32_t pmic_gpio);
