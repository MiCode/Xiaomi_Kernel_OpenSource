/*
 * Copyright (c) 2012-2013, 2017, The Linux Foundation. All rights reserved.
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

#ifndef __REGULATOR_QPNP_REGULATOR_H__
#define __REGULATOR_QPNP_REGULATOR_H__

#include <linux/regulator/machine.h>

#define QPNP_REGULATOR_DRIVER_NAME "qcom,qpnp-regulator"

/* Pin control enable input pins. */
#define QPNP_REGULATOR_PIN_CTRL_ENABLE_NONE		0x00
#define QPNP_REGULATOR_PIN_CTRL_ENABLE_EN0		0x01
#define QPNP_REGULATOR_PIN_CTRL_ENABLE_EN1		0x02
#define QPNP_REGULATOR_PIN_CTRL_ENABLE_EN2		0x04
#define QPNP_REGULATOR_PIN_CTRL_ENABLE_EN3		0x08
#define QPNP_REGULATOR_PIN_CTRL_ENABLE_HW_DEFAULT	0x10

/* Pin control high power mode input pins. */
#define QPNP_REGULATOR_PIN_CTRL_HPM_NONE		0x00
#define QPNP_REGULATOR_PIN_CTRL_HPM_EN0			0x01
#define QPNP_REGULATOR_PIN_CTRL_HPM_EN1			0x02
#define QPNP_REGULATOR_PIN_CTRL_HPM_EN2			0x04
#define QPNP_REGULATOR_PIN_CTRL_HPM_EN3			0x08
#define QPNP_REGULATOR_PIN_CTRL_HPM_SLEEP_B		0x10
#define QPNP_REGULATOR_PIN_CTRL_HPM_HW_DEFAULT		0x20

/*
 * Used with enable parameters to specify that hardware default register values
 * should be left unaltered.
 */
#define QPNP_REGULATOR_DISABLE				0
#define QPNP_REGULATOR_ENABLE				1
#define QPNP_REGULATOR_USE_HW_DEFAULT			2

/* Soft start strength of a voltage switch type regulator */
enum qpnp_vs_soft_start_str {
	QPNP_VS_SOFT_START_STR_0P05_UA,
	QPNP_VS_SOFT_START_STR_0P25_UA,
	QPNP_VS_SOFT_START_STR_0P55_UA,
	QPNP_VS_SOFT_START_STR_0P75_UA,
	QPNP_VS_SOFT_START_STR_HW_DEFAULT,
};

/* Current limit of a boost type regulator */
enum qpnp_boost_current_limit {
	QPNP_BOOST_CURRENT_LIMIT_300_MA,
	QPNP_BOOST_CURRENT_LIMIT_600_MA,
	QPNP_BOOST_CURRENT_LIMIT_900_MA,
	QPNP_BOOST_CURRENT_LIMIT_1200_MA,
	QPNP_BOOST_CURRENT_LIMIT_1500_MA,
	QPNP_BOOST_CURRENT_LIMIT_1800_MA,
	QPNP_BOOST_CURRENT_LIMIT_2100_MA,
	QPNP_BOOST_CURRENT_LIMIT_2400_MA,
	QPNP_BOOST_CURRENT_LIMIT_HW_DEFAULT,
};

/**
 * struct qpnp_regulator_platform_data - qpnp-regulator initialization data
 * @init_data:		regulator constraints
 * @pull_down_enable:       1 = Enable output pull down resistor when the
 *			        regulator is disabled
 *			    0 = Disable pull down resistor
 *			    QPNP_REGULATOR_USE_HW_DEFAULT = do not modify
 *			        pull down state
 * @pin_ctrl_enable:        Bit mask specifying which hardware pins should be
 *				used to enable the regulator, if any
 *			    Value should be an ORing of
 *				QPNP_REGULATOR_PIN_CTRL_ENABLE_* constants.  If
 *				the bit specified by
 *				QPNP_REGULATOR_PIN_CTRL_ENABLE_HW_DEFAULT is
 *				set, then pin control enable hardware registers
 *				will not be modified.
 * @pin_ctrl_hpm:           Bit mask specifying which hardware pins should be
 *				used to force the regulator into high power
 *				mode, if any
 *			    Value should be an ORing of
 *				QPNP_REGULATOR_PIN_CTRL_HPM_* constants.  If
 *				the bit specified by
 *				QPNP_REGULATOR_PIN_CTRL_HPM_HW_DEFAULT is
 *				set, then pin control mode hardware registers
 *				will not be modified.
 * @system_load:            Load in uA present on regulator that is not captured
 *				by any consumer request
 * @enable_time:            Time in us to delay after enabling the regulator
 * @ocp_enable:             1 = Allow over current protection (OCP) to be
 *				enabled for voltage switch type regulators so
 *				that they latch off automatically when over
 *				current is detected.  OCP is enabled when in HPM
 *				or auto mode.
 *			    0 = Disable OCP
 *			    QPNP_REGULATOR_USE_HW_DEFAULT = do not modify
 *			        OCP state
 * @ocp_irq:                IRQ number of the voltage switch OCP IRQ.  If
 *				specified the voltage switch will be toggled off
 *				and back on when OCP triggers in order to handle
 *				high in-rush current.
 * @ocp_max_retries:        Maximum number of times to try toggling a voltage
 *				switch off and back on as a result of
 *				consecutive over current events.
 * @ocp_retry_delay_ms:     Time to delay in milliseconds between each
 *				voltage switch toggle after an over current
 *				event takes place.
 * @boost_current_limit:    This parameter sets the current limit of boost type
 *				regulators.  Its value should be one of
 *				QPNP_BOOST_CURRENT_LIMIT_*.  If its value is
 *				QPNP_BOOST_CURRENT_LIMIT_HW_DEFAULT, then the
 *				boost current limit will be left at its default
 *				hardware value.
 * @soft_start_enable:      1 = Enable soft start for LDO and voltage switch
 *				type regulators so that output voltage slowly
 *				ramps up when the regulator is enabled
 *			    0 = Disable soft start
 *			    QPNP_REGULATOR_USE_HW_DEFAULT = do not modify
 *			        soft start state
 * @vs_soft_start_strength: This parameter sets the soft start strength for
 *				voltage switch type regulators.  Its value
 *				should be one of QPNP_VS_SOFT_START_STR_*.  If
 *				its value is QPNP_VS_SOFT_START_STR_HW_DEFAULT,
 *				then the soft start strength will be left at its
 *				default hardware value.
 * @auto_mode_enable:       1 = Enable automatic hardware selection of regulator
 *				mode (HPM vs LPM).  Auto mode is not available
 *				on boost type regulators
 *			    0 = Disable auto mode selection
 *			    QPNP_REGULATOR_USE_HW_DEFAULT = do not modify
 *			        auto mode state
 * @bypass_mode_enable:     1 = Enable bypass mode for an LDO type regulator so
 *				that it acts like a switch and simply outputs
 *				its input voltage
 *			    0 = Do not enable bypass mode
 *			    QPNP_REGULATOR_USE_HW_DEFAULT = do not modify
 *			        bypass mode state
 * @hpm_enable:             1 = Enable high power mode (HPM), also referred to
 *				as NPM.  HPM consumes more ground current than
 *				LPM, but it can source significantly higher load
 *				current.  HPM is not available on boost type
 *				regulators.  For voltage switch type regulators,
 *				HPM implies that over current protection and
 *				soft start are active all the time.  This
 *				configuration can be overwritten by changing the
 *				regulator's mode dynamically.
 *			    0 = Do not enable HPM
 *			    QPNP_REGULATOR_USE_HW_DEFAULT = do not modify
 *			        HPM state
 * @base_addr:              SMPI base address for the regulator peripheral
 */
struct qpnp_regulator_platform_data {
	struct regulator_init_data		init_data;
	int					pull_down_enable;
	unsigned int				pin_ctrl_enable;
	unsigned int				pin_ctrl_hpm;
	int					system_load;
	int					enable_time;
	int					ocp_enable;
	int					ocp_irq;
	int					ocp_max_retries;
	int					ocp_retry_delay_ms;
	enum qpnp_boost_current_limit		boost_current_limit;
	int					soft_start_enable;
	enum qpnp_vs_soft_start_str		vs_soft_start_strength;
	int					auto_mode_enable;
	int					bypass_mode_enable;
	int					hpm_enable;
	u16					base_addr;
};

#ifdef CONFIG_REGULATOR_QPNP

/**
 * qpnp_regulator_init() - register spmi driver for qpnp-regulator
 *
 * This initialization function should be called in systems in which driver
 * registration ordering must be controlled precisely.
 */
int __init qpnp_regulator_init(void);

#else

static inline int __init qpnp_regulator_init(void)
{
	return -ENODEV;
}

#endif /* CONFIG_REGULATOR_QPNP */

#endif
