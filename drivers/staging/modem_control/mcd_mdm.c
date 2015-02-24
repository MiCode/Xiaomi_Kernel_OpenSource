/**
 * linux/drivers/modem_control/mcd_mdm.c
 *
 * Version 1.0
 *
 * This code includes power sequences for IMC 7060 modems and its derivative.
 * That includes :
 *	- XMM6360
 *	- XMM7160
 *	- XMM7260
 * There is no guarantee for other modems.
 *
 * Intel Mobile driver for modem powering.
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 * Contact: Ranquet Guillaune <guillaumex.ranquet@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */

#include "mdm_util.h"
#include "mcd_mdm.h"

#include <linux/mdm_ctrl.h>

/*****************************************************************************
 *
 * Modem Power/Reset functions
 *
 ****************************************************************************/

int mcd_mdm_init(void *data)
{
	return 0;
}

/**
 *  mcd_mdm_cold_boot - Perform a modem cold boot sequence
 *  @drv: Reference to the driver structure
 *
 *  - Set to HIGH the PWRDWN_N to switch ON the modem
 *  - Set to HIGH the RESET_BB_N
 *  - Do a pulse on ON1
 */
int mcd_mdm_cold_boot(void *data, int rst, int pwr_on)
{
	struct mdm_ctrl_mdm_data *mdm_data = data;

	if ((rst == INVALID_GPIO)
		|| (pwr_on == INVALID_GPIO))
		return -EINVAL;

	/* Toggle the RESET_BB_N */
	gpio_set_value(rst, 1);

	/* Wait before doing the pulse on ON1 */
	usleep_range(mdm_data->pre_on_delay, mdm_data->pre_on_delay + 1);

	/* Do a pulse on ON1 */
	gpio_set_value(pwr_on, 1);
	usleep_range(mdm_data->on_duration, mdm_data->on_duration + 1);
	gpio_set_value(pwr_on, 0);

	return 0;
}

/**
 *  mdm_ctrl_silent_warm_reset_7x6x - Perform a silent modem warm reset
 *				      sequence
 *  @drv: Reference to the driver structure
 *
 *  - Do a pulse on the RESET_BB_N
 *  - No struct modification
 *  - debug purpose only
 */
int mcd_mdm_warm_reset(void *data, int rst)
{
	struct mdm_ctrl_mdm_data *mdm_data = data;

	if (rst == INVALID_GPIO)
		return -EINVAL;

	gpio_set_value(rst, 0);
	usleep_range(mdm_data->warm_rst_duration,
			mdm_data->warm_rst_duration + 1);
	gpio_set_value(rst, 1);

	return 0;
}

/**
 *  mcd_mdm_power_off - Perform the modem switch OFF sequence
 *  @drv: Reference to the driver structure
 *
 *  - Set to low the ON1
 *  - Write the PMIC reg
 */
int mcd_mdm_power_off(void *data, int rst)
{
	struct mdm_ctrl_mdm_data *mdm_data = data;

	if (rst == INVALID_GPIO)
		return -EINVAL;

	/* Set the RESET_BB_N to 0 */
	gpio_set_value(rst, 0);

	/* Wait before doing the pulse on ON1 */
	usleep_range(mdm_data->pre_pwr_down_delay,
		mdm_data->pre_pwr_down_delay + 1);

	return 0;
}

int mcd_mdm_get_cflash_delay(void *data)
{
	struct mdm_ctrl_mdm_data *mdm_data = data;
	return mdm_data->pre_cflash_delay;
}

int mcd_mdm_get_wflash_delay(void *data)
{
	struct mdm_ctrl_mdm_data *mdm_data = data;
	return mdm_data->pre_wflash_delay;
}

int mcd_mdm_cleanup(void *data)
{
	return 0;
}

/**
 *  mcd_mdm_cold_boot_ngff - Perform a NGFF modem cold boot sequence
 *  @drv: Reference to the driver structure
 *
 *  - Set to HIGH the RESET_BB_N
 *  - reset USB hub
 */
int mcd_mdm_cold_boot_ngff(void *data, int rst, int pwr_on)
{

	struct mdm_ctrl_cpu_data *cpu_data;
	struct gpio_desc  *gpio_rst_desc = gpio_to_desc(rst);

	cpu_data = container_of(&gpio_rst_desc, struct mdm_ctrl_cpu_data,
				gpio_rst_bbn);

	if ((rst == INVALID_GPIO) ||
		(cpu_data->gpio_rst_usbhub == NULL))
		return -EINVAL;

	/* Toggle the RESET_BB_N */
	gpio_set_value(rst, 1);

	/* reset the USB hub here*/
	usleep_range(1000, 1001);
	gpiod_set_value(cpu_data->gpio_rst_usbhub, 0);
	usleep_range(1000, 1001);
	gpiod_set_value(cpu_data->gpio_rst_usbhub, 1);

	return 0;
}

int mcd_mdm_cold_boot_2230(void *data, int rst, int pwr_on)
{
	struct mdm_ctrl_mdm_data *mdm_data = data;

	if ((rst == INVALID_GPIO)
		&& (pwr_on == INVALID_GPIO))
		return -EINVAL;

	/* Toggle the RESET_BB_N */
	gpio_set_value(rst, 0);

	/* Toggle the POWER_ON */
	usleep_range(mdm_data->pre_on_delay, mdm_data->pre_on_delay + 1);
	gpio_set_value(pwr_on, 0);

	/* Toggle RESET_BB_N */
	usleep_range(mdm_data->on_duration, mdm_data->on_duration + 1);
	gpio_set_value(rst, 1);

	/* Toggle POWER_ON */
	usleep_range(100000, 100001);
	gpio_set_value(pwr_on, 1);

	return 0;
}
