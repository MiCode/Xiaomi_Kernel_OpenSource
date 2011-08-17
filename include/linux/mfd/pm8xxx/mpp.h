/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#ifndef __PM8XXX_MPP_H
#define __PM8XXX_MPP_H

#include <linux/errno.h>

#define PM8XXX_MPP_DEV_NAME	"pm8xxx-mpp"

struct pm8xxx_mpp_core_data {
	int	base_addr;
	int	nmpps;
};

struct pm8xxx_mpp_platform_data {
	struct pm8xxx_mpp_core_data	core_data;
	int				mpp_base;
};

/**
 * struct pm8xxx_mpp_config_data - structure to specify mpp configuration values
 * @type:	MPP type which determines the overall MPP function (i.e. digital
 *		in/out/bi, analog in/out, current sink, or test).  It should be
 *		set to the value of one of PM8XXX_MPP_TYPE_D_*.
 * @level:	meaning depends upon MPP type specified
 * @control:	meaning depends upon MPP type specified
 *
 * Usage of level argument:
 * 1. type = PM8XXX_MPP_TYPE_D_INPUT, PM8XXX_MPP_TYPE_D_OUTPUT,
 *	     PM8XXX_MPP_TYPE_D_BI_DIR, or PM8XXX_MPP_TYPE_DTEST_OUTPUT -
 *
 *	level specifies that digital logic level to use for the MPP.  It should
 *	be set to the value of one of PM8XXX_MPP_DIG_LEVEL_*.  Actual regulator
 *	connections for these level choices are PMIC chip specific.
 *
 * 2. type = PM8XXX_MPP_TYPE_A_INPUT -
 *
 *	level specifies where in the PMIC chip the analog input value should
 *	be routed to.  It should be set to the value of one of
 *	PM8XXX_MPP_AIN_AMUX_*.
 *
 * 3. type = PM8XXX_MPP_TYPE_A_OUTPUT -
 *
 *	level specifies the output analog voltage reference level.  It should
 *	be set to the value of one of PM8XXX_MPP_AOUT_LVL_*.
 *
 * 4. type = PM8XXX_MPP_TYPE_SINK or PM8XXX_MPP_TYPE_DTEST_SINK -
 *
 *	level specifies the output current level.  It should be set to the value
 *	of one of PM8XXX_MPP_CS_OUT_*.
 *
 * Usage of control argument:
 * 1. type = PM8XXX_MPP_TYPE_D_INPUT -
 *
 *	control specifies how the digital input should be routed in the chip.
 *	It should be set to the value of one of PM8XXX_MPP_DIN_TO_*.
 *
 * 2. type = PM8XXX_MPP_TYPE_D_OUTPUT -
 *
 *	control specifies the digital output value.  It should be set to the
 *	value of one of PM8XXX_MPP_DOUT_CTRL_*.
 *
 * 3. type = PM8XXX_MPP_TYPE_D_BI_DIR -
 *
 *	control specifies the pullup resistor value.  It should be set to the
 *	value of one of PM8XXX_MPP_BI_PULLUP_*.
 *
 * 4. type = PM8XXX_MPP_TYPE_A_INPUT -
 *
 *	control is unused; a value of 0 is sufficient.
 *
 * 5. type = PM8XXX_MPP_TYPE_A_OUTPUT -
 *
 *	control specifies if analog output is enabled.  It should be set to the
 *	value of one of PM8XXX_MPP_AOUT_CTRL_*.
 *
 * 6. type = PM8XXX_MPP_TYPE_SINK -
 *
 *	control specifies if current sinking is enabled.  It should be set to
 *	the value of one of PM8XXX_MPP_CS_CTRL_*.
 *
 * 7. type = PM8XXX_MPP_TYPE_DTEST_SINK -
 *
 *	control specifies if current sinking is enabled.  It should be set to
 *	the value of one of PM8XXX_MPP_DTEST_CS_CTRL_*.
 *
 * 8. type = PM8XXX_MPP_TYPE_DTEST_OUTPUT -
 *
 *	control specifies which DTEST bus value to output.  It should be set to
 *	the value of one of PM8XXX_MPP_DTEST_*.
 */
struct pm8xxx_mpp_config_data {
	unsigned	type;
	unsigned	level;
	unsigned	control;
};

/* API */
#if defined(CONFIG_GPIO_PM8XXX_MPP) || defined(CONFIG_GPIO_PM8XXX_MPP_MODULE)

/**
 * pm8xxx_mpp_config() - configure control options of a multi-purpose pin (MPP)
 * @mpp:	global GPIO number corresponding to the MPP
 * @config:	configuration to set for this MPP
 * Context: can sleep
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_mpp_config(unsigned mpp, struct pm8xxx_mpp_config_data *config);

#else

static inline int pm8xxx_mpp_config(unsigned mpp,
				    struct pm8xxx_mpp_config_data *config)
{
	return -ENXIO;
}

#endif

/* MPP Type: type */
#define	PM8XXX_MPP_TYPE_D_INPUT		0
#define	PM8XXX_MPP_TYPE_D_OUTPUT	1
#define	PM8XXX_MPP_TYPE_D_BI_DIR	2
#define	PM8XXX_MPP_TYPE_A_INPUT		3
#define	PM8XXX_MPP_TYPE_A_OUTPUT	4
#define	PM8XXX_MPP_TYPE_SINK		5
#define	PM8XXX_MPP_TYPE_DTEST_SINK	6
#define	PM8XXX_MPP_TYPE_DTEST_OUTPUT	7

/* Digital Input/Output: level */
#define	PM8XXX_MPP_DIG_LEVEL_VIO_0	0
#define	PM8XXX_MPP_DIG_LEVEL_VIO_1	1
#define	PM8XXX_MPP_DIG_LEVEL_VIO_2	2
#define	PM8XXX_MPP_DIG_LEVEL_VIO_3	3
#define	PM8XXX_MPP_DIG_LEVEL_VIO_4	4
#define	PM8XXX_MPP_DIG_LEVEL_VIO_5	5
#define	PM8XXX_MPP_DIG_LEVEL_VIO_6	6
#define	PM8XXX_MPP_DIG_LEVEL_VIO_7	7

/* Digital Input/Output: level [PM8058] */
#define	PM8058_MPP_DIG_LEVEL_VPH	0
#define	PM8058_MPP_DIG_LEVEL_S3		1
#define	PM8058_MPP_DIG_LEVEL_L2		2
#define	PM8058_MPP_DIG_LEVEL_L3		3

/* Digital Input/Output: level [PM8901] */
#define	PM8901_MPP_DIG_LEVEL_MSMIO	0
#define	PM8901_MPP_DIG_LEVEL_DIG	1
#define	PM8901_MPP_DIG_LEVEL_L5		2
#define	PM8901_MPP_DIG_LEVEL_S4		3
#define	PM8901_MPP_DIG_LEVEL_VPH	4

/* Digital Input/Output: level [PM8921] */
#define	PM8921_MPP_DIG_LEVEL_S4		1
#define	PM8921_MPP_DIG_LEVEL_L15	3
#define	PM8921_MPP_DIG_LEVEL_L17	4
#define	PM8921_MPP_DIG_LEVEL_VPH	7

/* Digital Input: control */
#define	PM8XXX_MPP_DIN_TO_INT		0
#define	PM8XXX_MPP_DIN_TO_DBUS1		1
#define	PM8XXX_MPP_DIN_TO_DBUS2		2
#define	PM8XXX_MPP_DIN_TO_DBUS3		3

/* Digital Output: control */
#define	PM8XXX_MPP_DOUT_CTRL_LOW	0
#define	PM8XXX_MPP_DOUT_CTRL_HIGH	1
#define	PM8XXX_MPP_DOUT_CTRL_MPP	2
#define	PM8XXX_MPP_DOUT_CTRL_INV_MPP	3

/* Bidirectional: control */
#define	PM8XXX_MPP_BI_PULLUP_1KOHM	0
#define	PM8XXX_MPP_BI_PULLUP_OPEN	1
#define	PM8XXX_MPP_BI_PULLUP_10KOHM	2
#define	PM8XXX_MPP_BI_PULLUP_30KOHM	3

/* Analog Input: level */
#define	PM8XXX_MPP_AIN_AMUX_CH5		0
#define	PM8XXX_MPP_AIN_AMUX_CH6		1
#define	PM8XXX_MPP_AIN_AMUX_CH7		2
#define	PM8XXX_MPP_AIN_AMUX_CH8		3
#define	PM8XXX_MPP_AIN_AMUX_CH9		4
#define	PM8XXX_MPP_AIN_AMUX_ABUS1	5
#define	PM8XXX_MPP_AIN_AMUX_ABUS2	6
#define	PM8XXX_MPP_AIN_AMUX_ABUS3	7

/* Analog Output: level */
#define	PM8XXX_MPP_AOUT_LVL_1V25	0
#define	PM8XXX_MPP_AOUT_LVL_1V25_2	1
#define	PM8XXX_MPP_AOUT_LVL_0V625	2
#define	PM8XXX_MPP_AOUT_LVL_0V3125	3
#define	PM8XXX_MPP_AOUT_LVL_MPP		4
#define	PM8XXX_MPP_AOUT_LVL_ABUS1	5
#define	PM8XXX_MPP_AOUT_LVL_ABUS2	6
#define	PM8XXX_MPP_AOUT_LVL_ABUS3	7

/* Analog Output: control */
#define	PM8XXX_MPP_AOUT_CTRL_DISABLE		0
#define	PM8XXX_MPP_AOUT_CTRL_ENABLE		1
#define	PM8XXX_MPP_AOUT_CTRL_MPP_HIGH_EN	2
#define	PM8XXX_MPP_AOUT_CTRL_MPP_LOW_EN		3

/* Current Sink: level */
#define	PM8XXX_MPP_CS_OUT_5MA		0
#define	PM8XXX_MPP_CS_OUT_10MA		1
#define	PM8XXX_MPP_CS_OUT_15MA		2
#define	PM8XXX_MPP_CS_OUT_20MA		3
#define	PM8XXX_MPP_CS_OUT_25MA		4
#define	PM8XXX_MPP_CS_OUT_30MA		5
#define	PM8XXX_MPP_CS_OUT_35MA		6
#define	PM8XXX_MPP_CS_OUT_40MA		7

/* Current Sink: control */
#define	PM8XXX_MPP_CS_CTRL_DISABLE	0
#define	PM8XXX_MPP_CS_CTRL_ENABLE	1
#define	PM8XXX_MPP_CS_CTRL_MPP_HIGH_EN	2
#define	PM8XXX_MPP_CS_CTRL_MPP_LOW_EN	3

/* DTEST Current Sink: control */
#define	PM8XXX_MPP_DTEST_CS_CTRL_EN1	0
#define	PM8XXX_MPP_DTEST_CS_CTRL_EN2	1
#define	PM8XXX_MPP_DTEST_CS_CTRL_EN3	2
#define	PM8XXX_MPP_DTEST_CS_CTRL_EN4	3

/* DTEST Digital Output: control */
#define	PM8XXX_MPP_DTEST_DBUS1		0
#define	PM8XXX_MPP_DTEST_DBUS2		1
#define	PM8XXX_MPP_DTEST_DBUS3		2
#define	PM8XXX_MPP_DTEST_DBUS4		3

#endif
