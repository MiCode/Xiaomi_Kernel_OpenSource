/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

/*
 * Qualcomm PMIC8XXX gpio driver header file
 *
 */

#ifndef __PM8XXX_GPIO_H
#define __PM8XXX_GPIO_H

#include <linux/errno.h>

#define PM8XXX_GPIO_DEV_NAME	"pm8xxx-gpio"

struct pm8xxx_gpio_core_data {
	int	ngpios;
};

struct pm8xxx_gpio_platform_data {
	struct pm8xxx_gpio_core_data	gpio_cdata;
	int				gpio_base;
};

/* GPIO parameters */
/* direction */
#define	PM_GPIO_DIR_OUT			0x01
#define	PM_GPIO_DIR_IN			0x02
#define	PM_GPIO_DIR_BOTH		(PM_GPIO_DIR_OUT | PM_GPIO_DIR_IN)

/* output_buffer */
#define	PM_GPIO_OUT_BUF_OPEN_DRAIN	1
#define	PM_GPIO_OUT_BUF_CMOS		0

/* pull */
#define	PM_GPIO_PULL_UP_30		0
#define	PM_GPIO_PULL_UP_1P5		1
#define	PM_GPIO_PULL_UP_31P5		2
#define	PM_GPIO_PULL_UP_1P5_30		3
#define	PM_GPIO_PULL_DN			4
#define	PM_GPIO_PULL_NO			5

/* vin_sel: Voltage Input Select */
#define	PM_GPIO_VIN_VPH			0 /* 3v ~ 4.4v */
#define	PM_GPIO_VIN_BB			1 /* ~3.3v */
#define	PM_GPIO_VIN_S4			2 /* 1.8v */
#define	PM_GPIO_VIN_L15			3
#define	PM_GPIO_VIN_L4			4
#define	PM_GPIO_VIN_L3			5
#define	PM_GPIO_VIN_L17			6

/* out_strength */
#define	PM_GPIO_STRENGTH_NO		0
#define	PM_GPIO_STRENGTH_HIGH		1
#define	PM_GPIO_STRENGTH_MED		2
#define	PM_GPIO_STRENGTH_LOW		3

/* function */
#define	PM_GPIO_FUNC_NORMAL		0
#define	PM_GPIO_FUNC_PAIRED		1
#define	PM_GPIO_FUNC_1			2
#define	PM_GPIO_FUNC_2			3
#define	PM_GPIO_DTEST1			4
#define	PM_GPIO_DTEST2			5
#define	PM_GPIO_DTEST3			6
#define	PM_GPIO_DTEST4			7

/**
 * struct pm_gpio - structure to specify gpio configurtion values
 * @direction:		indicates whether the gpio should be input, output, or
 *			both. Should be of the type PM_GPIO_DIR_*
 * @output_buffer:	indicates gpio should be configured as CMOS or open
 *			drain. Should be of the type PM_GPIO_OUT_BUF_*
 * @output_value:	The gpio output value of the gpio line - 0 or 1
 * @pull:		Indicates whether a pull up or pull down should be
 *			applied. If a pullup is required the current strength
 *			needs to be specified. Current values of 30uA, 1.5uA,
 *			31.5uA, 1.5uA with 30uA boost are supported. This value
 *			should be one of the PM_GPIO_PULL_*
 * @vin_sel:		specifies the voltage level when the output is set to 1.
 *			For an input gpio specifies the voltage level at which
 *			the input is interpreted as a logical 1.
 * @out_strength:	the amount of current supplied for an output gpio,
 *			should be of the type PM_GPIO_STRENGTH_*
 * @function:		choose alternate function for the gpio. Certain gpios
 *			can be paired (shorted) with each other. Some gpio pin
 *			can act as alternate functions. This parameter should
 *			be of type PM_GPIO_FUNC_*
 * @inv_int_pol:	Invert polarity before feeding the line to the interrupt
 *			module in pmic. This feature will almost be never used
 *			since the pm8xxx interrupt block can detect both edges
 *			and both levels.
 * @disable_pin:	Disable the gpio by configuring it as high impedance.
 */
struct pm_gpio {
	int		direction;
	int		output_buffer;
	int		output_value;
	int		pull;
	int		vin_sel;
	int		out_strength;
	int		function;
	int		inv_int_pol;
	int		disable_pin;
};

#if defined(CONFIG_GPIO_PM8XXX) || defined(CONFIG_GPIO_PM8XXX_MODULE)
/**
 * pm8xxx_gpio_config - configure a gpio controlled by a pm8xxx chip
 * @gpio: gpio number to configure
 * @param: configuration values
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_gpio_config(int gpio, struct pm_gpio *param);
#else
static inline int pm8xxx_gpio_config(int gpio, struct pm_gpio *param)
{
	return -ENXIO;
}
#endif

#endif
