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

#ifndef __MFD_PM8XXX_MISC_H__
#define __MFD_PM8XXX_MISC_H__

#include <linux/err.h>

#define PM8XXX_MISC_DEV_NAME	"pm8xxx-misc"

/**
 * struct pm8xxx_misc_platform_data - PM8xxx misc driver platform data
 * @priority:	PMIC prority level in a multi-PMIC system. Lower value means
 *		greater priority. Actions are performed from highest to lowest
 *		priority PMIC.
 */
struct pm8xxx_misc_platform_data {
	int	priority;
};

enum pm8xxx_uart_path_sel {
	UART_NONE,
	UART_TX1_RX1,
	UART_TX2_RX2,
	UART_TX3_RX3,
};

enum pm8xxx_coincell_chg_voltage {
	PM8XXX_COINCELL_VOLTAGE_3p2V = 1,
	PM8XXX_COINCELL_VOLTAGE_3p1V,
	PM8XXX_COINCELL_VOLTAGE_3p0V,
	PM8XXX_COINCELL_VOLTAGE_2p5V = 16
};

enum pm8xxx_coincell_chg_resistor {
	PM8XXX_COINCELL_RESISTOR_2100_OHMS,
	PM8XXX_COINCELL_RESISTOR_1700_OHMS,
	PM8XXX_COINCELL_RESISTOR_1200_OHMS,
	PM8XXX_COINCELL_RESISTOR_800_OHMS
};

enum pm8xxx_coincell_chg_state {
	PM8XXX_COINCELL_CHG_DISABLE,
	PM8XXX_COINCELL_CHG_ENABLE
};

struct pm8xxx_coincell_chg {
	enum pm8xxx_coincell_chg_state		state;
	enum pm8xxx_coincell_chg_voltage	voltage;
	enum pm8xxx_coincell_chg_resistor	resistor;
};

enum pm8xxx_smpl_delay {
	PM8XXX_SMPL_DELAY_0p5,
	PM8XXX_SMPL_DELAY_1p0,
	PM8XXX_SMPL_DELAY_1p5,
	PM8XXX_SMPL_DELAY_2p0,
};

enum pm8xxx_pon_config {
	PM8XXX_DISABLE_HARD_RESET = 0,
	PM8XXX_SHUTDOWN_ON_HARD_RESET,
	PM8XXX_RESTART_ON_HARD_RESET,
};

#if defined(CONFIG_MFD_PM8XXX_MISC) || defined(CONFIG_MFD_PM8XXX_MISC_MODULE)

/**
 * pm8xxx_reset_pwr_off - switch all PM8XXX PMIC chips attached to the system to
 *			  either reset or shutdown when they are turned off
 * @reset: 0 = shudown the PMICs, 1 = shutdown and then restart the PMICs
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_reset_pwr_off(int reset);

int pm8xxx_uart_gpio_mux_ctrl(enum pm8xxx_uart_path_sel uart_path_sel);

/**
 * pm8xxx_coincell_chg_config - Disables or enables the coincell charger, and
 *				configures its voltage and resistor settings.
 * @chg_config:			Holds both voltage and resistor values, and a
 *				switch to change the state of charger.
 *				If state is to disable the charger then
 *				both voltage and resistor are disregarded.
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_coincell_chg_config(struct pm8xxx_coincell_chg *chg_config);

/**
 * pm8xxx_smpl_control - enables/disables SMPL detection
 * @enable: 0 = shutdown PMIC on power loss, 1 = reset PMIC on power loss
 *
 * This function enables or disables the Sudden Momentary Power Loss detection
 * module.  If SMPL detection is enabled, then when a sufficiently long power
 * loss event occurs, the PMIC will automatically reset itself.  If SMPL
 * detection is disabled, then the PMIC will shutdown when power loss occurs.
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_smpl_control(int enable);

/**
 * pm8xxx_smpl_set_delay - sets the SMPL detection time delay
 * @delay: enum value corresponding to delay time
 *
 * This function sets the time delay of the SMPL detection module.  If power
 * is reapplied within this interval, then the PMIC reset automatically.  The
 * SMPL detection module must be enabled for this delay time to take effect.
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_smpl_set_delay(enum pm8xxx_smpl_delay delay);

/**
 * pm8xxx_watchdog_reset_control - enables/disables watchdog reset detection
 * @enable: 0 = shutdown when PS_HOLD goes low, 1 = reset when PS_HOLD goes low
 *
 * This function enables or disables the PMIC watchdog reset detection feature.
 * If watchdog reset detection is enabled, then the PMIC will reset itself
 * when PS_HOLD goes low.  If it is not enabled, then the PMIC will shutdown
 * when PS_HOLD goes low.
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_watchdog_reset_control(int enable);

/**
 * pm8xxx_hard_reset_config - Allows different reset configurations
 *
 * config = DISABLE_HARD_RESET to disable hard reset
 *	  = SHUTDOWN_ON_HARD_RESET to turn off the system on hard reset
 *	  = RESTART_ON_HARD_RESET to restart the system on hard reset
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_hard_reset_config(enum pm8xxx_pon_config config);

/**
 * pm8xxx_stay_on - enables stay_on feature
 *
 * PMIC stay-on feature allows PMIC to ignore MSM PS_HOLD=low
 * signal so that some special functions like debugging could be
 * performed.
 *
 * This feature should not be used in any product release.
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_stay_on(void);

#else

static inline int pm8xxx_reset_pwr_off(int reset)
{
	return -ENODEV;
}
static inline int
pm8xxx_uart_gpio_mux_ctrl(enum pm8xxx_uart_path_sel uart_path_sel)
{
	return -ENODEV;
}
static inline int
pm8xxx_coincell_chg_config(struct pm8xxx_coincell_chg *chg_config)
{
	return -ENODEV;
}
static inline int pm8xxx_smpl_set_delay(enum pm8xxx_smpl_delay delay)
{
	return -ENODEV;
}
static inline int pm8xxx_smpl_control(int enable)
{
	return -ENODEV;
}
static inline int pm8xxx_watchdog_reset_control(int enable)
{
	return -ENODEV;
}
static inline int pm8xxx_hard_reset_config(enum pm8xxx_pon_config config)
{
	return -ENODEV;
}
static inline int pm8xxx_stay_on(void)
{
	return -ENODEV;
}
#endif

#endif
