/* Copyright (c) 2012, 2015, The Linux Foundation. All rights reserved.
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

/* Mode select */
#define QPNP_PIN_MODE_DIG_IN			0
#define QPNP_PIN_MODE_DIG_OUT			1
#define QPNP_PIN_MODE_DIG_IN_OUT		2
#define QPNP_PIN_MODE_ANA_PASS_THRU		3
#define QPNP_PIN_MODE_BIDIR			3
#define QPNP_PIN_MODE_AIN			4
#define QPNP_PIN_MODE_AOUT			5
#define QPNP_PIN_MODE_SINK			6

/* Invert source select (GPIO, MPP) */
#define QPNP_PIN_INVERT_DISABLE			0
#define QPNP_PIN_INVERT_ENABLE			1

/* Output type (GPIO) */
#define QPNP_PIN_OUT_BUF_CMOS			0
#define QPNP_PIN_OUT_BUF_OPEN_DRAIN_NMOS	1
#define QPNP_PIN_OUT_BUF_OPEN_DRAIN_PMOS	2
#define QPNP_PIN_OUT_BUF_NO_DRIVE		3

/* Voltage select (GPIO, MPP) */
#define QPNP_PIN_VIN0				0
#define QPNP_PIN_VIN1				1
#define QPNP_PIN_VIN2				2
#define QPNP_PIN_VIN3				3
#define QPNP_PIN_VIN4				4
#define QPNP_PIN_VIN5				5
#define QPNP_PIN_VIN6				6
#define QPNP_PIN_VIN7				7

/* Pull Up Values (GPIO) */
#define QPNP_PIN_GPIO_PULL_UP_30		0
#define QPNP_PIN_GPIO_PULL_UP_1P5		1
#define QPNP_PIN_GPIO_PULL_UP_31P5		2
#define QPNP_PIN_GPIO_PULL_UP_1P5_30		3
#define QPNP_PIN_GPIO_PULL_DN			4
#define QPNP_PIN_GPIO_PULL_NO			5

/* Pull Up Values (MPP) */
#define QPNP_PIN_MPP_PULL_UP_0P6KOHM		0
#define QPNP_PIN_MPP_PULL_UP_OPEN		1
#define QPNP_PIN_MPP_PULL_UP_10KOHM		2
#define QPNP_PIN_MPP_PULL_UP_30KOHM		3

/* Out Strength (GPIO) */
#define QPNP_PIN_OUT_STRENGTH_LOW		1
#define QPNP_PIN_OUT_STRENGTH_MED		2
#define QPNP_PIN_OUT_STRENGTH_HIGH		3

/* Digital-in CTL (GPIO/MPP) */
#define QPNP_PIN_DIG_IN_CTL_DTEST1		1
#define QPNP_PIN_DIG_IN_CTL_DTEST2		2
#define QPNP_PIN_DIG_IN_CTL_DTEST3		3
#define QPNP_PIN_DIG_IN_CTL_DTEST4		4

/* Source Select (GPIO) / Enable Select (MPP) */
#define QPNP_PIN_SEL_FUNC_CONSTANT		0
#define QPNP_PIN_SEL_FUNC_PAIRED		1
#define QPNP_PIN_SEL_FUNC_1			2
#define QPNP_PIN_SEL_FUNC_2			3
#define QPNP_PIN_SEL_DTEST1			4
#define QPNP_PIN_SEL_DTEST2			5
#define QPNP_PIN_SEL_DTEST3			6
#define QPNP_PIN_SEL_DTEST4			7

/* Source Select for GPIO_LV/GPIO_MV only */
#define QPNP_PIN_LV_MV_SEL_FUNC_CONSTANT	0
#define QPNP_PIN_LV_MV_SEL_FUNC_PAIRED		1
#define QPNP_PIN_LV_MV_SEL_FUNC_1		2
#define QPNP_PIN_LV_MV_SEL_FUNC_2		3
#define QPNP_PIN_LV_MV_SEL_FUNC_3		4
#define QPNP_PIN_LV_MV_SEL_FUNC_4		5
#define QPNP_PIN_LV_MV_SEL_DTEST1		6
#define QPNP_PIN_LV_MV_SEL_DTEST2		7
#define QPNP_PIN_LV_MV_SEL_DTEST3		8
#define QPNP_PIN_LV_MV_SEL_DTEST4		9

/* Master enable (GPIO, MPP) */
#define QPNP_PIN_MASTER_DISABLE			0
#define QPNP_PIN_MASTER_ENABLE			1

/* Analog Output (MPP) */
#define QPNP_PIN_AOUT_1V25			0
#define QPNP_PIN_AOUT_0V625			1
#define QPNP_PIN_AOUT_0V3125			2
#define QPNP_PIN_AOUT_MPP			3
#define QPNP_PIN_AOUT_ABUS1			4
#define QPNP_PIN_AOUT_ABUS2			5
#define QPNP_PIN_AOUT_ABUS3			6
#define QPNP_PIN_AOUT_ABUS4			7

/* Analog Input (MPP) */
#define QPNP_PIN_AIN_AMUX_CH5			0
#define QPNP_PIN_AIN_AMUX_CH6			1
#define QPNP_PIN_AIN_AMUX_CH7			2
#define QPNP_PIN_AIN_AMUX_CH8			3
#define QPNP_PIN_AIN_AMUX_ABUS1			4
#define QPNP_PIN_AIN_AMUX_ABUS2			5
#define QPNP_PIN_AIN_AMUX_ABUS3			6
#define QPNP_PIN_AIN_AMUX_ABUS4			7

/* Current Sink (MPP) */
#define QPNP_PIN_CS_OUT_5MA			0
#define QPNP_PIN_CS_OUT_10MA			1
#define QPNP_PIN_CS_OUT_15MA			2
#define QPNP_PIN_CS_OUT_20MA			3
#define QPNP_PIN_CS_OUT_25MA			4
#define QPNP_PIN_CS_OUT_30MA			5
#define QPNP_PIN_CS_OUT_35MA			6
#define QPNP_PIN_CS_OUT_40MA			7

/* ANALOG PASS SEL (GPIO LV/MV) */
#define QPNP_PIN_APASS_SEL_ATEST1		0
#define QPNP_PIN_APASS_SEL_ATEST2		1
#define QPNP_PIN_APASS_SEL_ATEST3		2
#define QPNP_PIN_APASS_SEL_ATEST4		3

/**
 * struct qpnp_pin_cfg - structure to specify pin configurtion values
 * @mode:		indicates whether the pin should be input, output, or
 *			both for gpios. mpp pins also support bidirectional,
 *			analog in, analog out and current sink. This value
 *			should be of type QPNP_PIN_MODE_*.
 * @output_type:	indicates pin should be configured as CMOS or open
 *			drain. Should be of the type QPNP_PIN_OUT_BUF_*. This
 *			setting applies for gpios only.
 * @invert:		Invert the signal of the line -
 *			QPNP_PIN_INVERT_DISABLE or QPNP_PIN_INVERT_ENABLE.
 * @pull:		This parameter should be programmed to different values
 *			depending on whether it's GPIO or MPP.
 *			For GPIO, it indicates whether a pull up or pull down
 *			should be applied. If a pullup is required the
 *			current strength needs to be specified.
 *			Current values of 30uA, 1.5uA, 31.5uA, 1.5uA with 30uA
 *			boost are supported. This value should be one of
 *			the QPNP_PIN_GPIO_PULL_*. Note that the hardware ignores
 *			this configuration if the GPIO is not set to input or
 *			output open-drain mode.
 *			For MPP, it indicates whether a pullup should be
 *			applied for bidirectitional mode only. The hardware
 *			ignores the configuration when operating in other modes.
 *			This value should be one of the QPNP_PIN_MPP_PULL_*.
 * @vin_sel:		specifies the voltage level when the output is set to 1.
 *			For an input gpio specifies the voltage level at which
 *			the input is interpreted as a logical 1.
 * @out_strength:	the amount of current supplied for an output gpio,
 *			should be of the type QPNP_PIN_STRENGTH_*.
 * @src_sel:		select alternate function for the pin. Certain pins
 *			can be paired (shorted) with each other. Some pins
 *			can act as alternate functions. In the context of
 *			gpio, this acts as a source select. For mpps,
 *			this is an enable select.
 *			This parameter should be of type QPNP_PIN_SEL_*.
 * @master_en:		QPNP_PIN_MASTER_ENABLE = Enable features within the
 *			pin block based on configurations.
 *			QPNP_PIN_MASTER_DISABLE = Completely disable the pin
 *			block and let the pin float with high impedance
 *			regardless of other settings.
 * @aout_ref:		Set the analog output reference. This parameter should
 *			be of type QPNP_PIN_AOUT_*. This parameter only applies
 *			to mpp pins.
 * @ain_route:		Set the source for analog input. This parameter
 *			should be of type QPNP_PIN_AIN_*. This parameter only
 *			applies to mpp pins.
 * @cs_out:		Set the the amount of current to sync in mA. This
 *			parameter should be of type QPNP_PIN_CS_OUT_*. This
 *			parameter only applies to mpp pins.
 * @apass_sel:		Set the ATEST line to which the signal is to be
 *			routed to. The parameter should be of type
 *			QPNP_PIN_APASS_SEL_*. This
 *			parameter only applies to GPIO LV/MV pins.
 * @dtest_sel:		Select the DTEST line to which the signal needs
 *			is routed to. The parameter should be of type
 *			QPNP_PIN_DIG_IN_CTL_*. The parameter applies
 *			to both gpio and mpp pins.
 */
struct qpnp_pin_cfg {
	int mode;
	int output_type;
	int invert;
	int pull;
	int vin_sel;
	int out_strength;
	int src_sel;
	int master_en;
	int aout_ref;
	int ain_route;
	int cs_out;
	int apass_sel;
	int dtest_sel;
};

/**
 * qpnp_pin_config - Apply pin configuration for Linux gpio
 * @gpio: Linux gpio number to configure.
 * @param: parameters to configure.
 *
 * This routine takes a Linux gpio number that corresponds with a
 * PMIC pin and applies the configuration specified in 'param'.
 * This gpio number can be ascertained by of_get_gpio_flags() or
 * the qpnp_pin_map_gpio() API.
 */
int qpnp_pin_config(int gpio, struct qpnp_pin_cfg *param);

/**
 * qpnp_pin_map - Obtain Linux GPIO number from device spec
 * @name: Name assigned by the 'label' binding for the primary node.
 * @pmic_pin: PMIC pin number to lookup.
 *
 * This routine is used in legacy configurations that do not support
 * Device Tree. If you are using Device Tree, you should not use this.
 * For such cases, use of_get_gpio() or friends instead.
 */
int qpnp_pin_map(const char *name, uint32_t pmic_pin);
