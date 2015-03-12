/*
 * linux/drivers/modem_control/mcd_pmic.c
 *
 * Version 1.0
 * This code allows to access the pmic specifics
 * of each supported platforms
 * among other things, it permits to communicate with the SCU/PMIC
 * to cold boot/reset the modem
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 * Contact: Ranquet Guillaume <guillaumex.ranquet@intel.com>
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/mfd/intel_soc_pmic.h>
#include <linux/mdm_ctrl_board.h>
#include "mcd_pmic.h"

int pmic_io_init(void *data)
{
	return 0;
}

int pmic_io_power_on_ctp_mdm(void *data)
{
	/* on ctp, there's no power on, only a reset operation exist */
	/* just do nothing! */
	return 0;
}

int pmic_io_power_on_mdm(void *data)
{
	struct mdm_ctrl_pmic_data *pmic_data = data;
	int ret = 0;
	u16 addr = pmic_data->chipctrl;
	u8 def_value = 0x00;
	u8 iodata;

	if (pmic_data->chipctrl_mask) {
		/* Get the current register value in order to not
		 * override it
		 */
		def_value = intel_soc_pmic_readb(addr);
		if (def_value < 0) {
			pr_err(DRVNAME ": intel_soc_pmic_readb(ON)  failed (err: %d)\n",
					def_value);
			return -1;
		}
	}
	/* Write the new register value (CHIPCNTRL_ON) */
	iodata =
		(def_value & pmic_data->chipctrl_mask) |
		(pmic_data->chipctrlon << PMIC_MODEMCTRL_REG_MODEMOFF_SHIFT);
	ret = intel_soc_pmic_writeb(addr, iodata);
	if (ret) {
		pr_err(DRVNAME ": intel_soc_pmic_writeb(ON) failed (err: %d)\n",
				ret);
		return -1;
	}
	/* Wait before RESET_PWRDN_N to be 1 */
	usleep_range(pmic_data->pwr_down_duration,
		     pmic_data->pwr_down_duration);

	return 0;
}

int pmic_io_power_off_mdm(void *data)
{
	struct mdm_ctrl_pmic_data *pmic_data = data;
	int ret = 0;
	u16 addr = pmic_data->chipctrl;
	u8 iodata;
	u8 def_value = 0x00;

	if (pmic_data->chipctrl_mask) {
		/* Get the current register value*/
		def_value = intel_soc_pmic_readb(addr);
		if (def_value < 0) {
			pr_err(DRVNAME ": intel_soc_pmic_readb(OFF)  failed (err: %d)\n",
					def_value);
			return -1;
		}
	}

	/* Write the new register value (CHIPCNTRL_OFF) */
	iodata =
		(def_value & pmic_data->chipctrl_mask) |
	    (pmic_data->chipctrloff << PMIC_MODEMCTRL_REG_MODEMOFF_SHIFT);
	ret = intel_soc_pmic_writeb(addr, iodata);
	if (ret) {
		pr_err(DRVNAME ": intel_soc_pmic_writeb(OFF) failed (err: %d)\n",
				ret);
		return -1;
	}
	/* Safety sleep. Avoid to directly call power on. */
	usleep_range(pmic_data->pwr_down_duration,
		     pmic_data->pwr_down_duration);

	return 0;
}

int pmic_io_cleanup(void *data)
{
	return 0;
}

int pmic_io_get_early_pwr_on(void *data)
{
	struct mdm_ctrl_pmic_data *pmic_data = data;
	return pmic_data->early_pwr_on;
}

int pmic_io_get_early_pwr_off(void *data)
{
	struct mdm_ctrl_pmic_data *pmic_data = data;
	return pmic_data->early_pwr_off;
}

int pmic_io_power_on_mdm2(void *data)
{
	struct mdm_ctrl_pmic_data *pmic_data = data;
	int ret = 0;
	u16 addr = pmic_data->chipctrl;
	u8 def_value = 0x00;
	u8 iodata;

	if (pmic_data->chipctrl_mask) {
		/* Get the current register value in order to not
		 * override it
		 */
		def_value = intel_soc_pmic_readb(addr);
		if (def_value < 0) {
			pr_err(DRVNAME ": intel_soc_pmic_readb(MDM_CTRL)  failed (err: %d)\n",
				   def_value);
			return -1;
		}
	}
	/* Write the new register value (SDWN_ON) */
	iodata =
		(def_value & pmic_data->chipctrl_mask) |
		(pmic_data->chipctrlon << PMIC_MODEMCTRL_REG_SDWN_SHIFT);
	ret = intel_soc_pmic_writeb(addr, iodata);
	if (ret) {
		pr_err(DRVNAME ": intel_soc_pmic_writeb(SDWN) failed (err: %d)\n",
			   ret);
		return -1;
	}

	usleep_range(TSDWN2ON, TSDWN2ON+1);

	/* Write the new register value (POWER_ON) */
	iodata =
		(iodata & pmic_data->chipctrl_mask) |
		(pmic_data->chipctrlon << PMIC_MODEMCTRL_REG_MODEMOFF_SHIFT);
	ret = intel_soc_pmic_writeb(addr, iodata);
	if (ret) {
		pr_err(DRVNAME ": intel_soc_pmic_writeb(ON) failed (err: %d)\n",
			   ret);
		return -1;
	}

	return 0;
}

int pmic_io_power_off_mdm2(void *data)
{
	struct mdm_ctrl_pmic_data *pmic_data = data;
	int ret = 0;
	u16 addr = pmic_data->chipctrl;
	u8 iodata;
	u8 def_value = 0x00;

	if (pmic_data->chipctrl_mask) {
		/* Get the current register value in order to not override it */
		def_value = intel_soc_pmic_readb(addr);
		if (def_value < 0) {
			pr_err(DRVNAME ": intel_soc_pmic_readb(MDM_CTRL)  failed (err: %d)\n",
				def_value);
			return -1;
		}
	}

	/* Write the new register value (SDWN_OFF) */
	iodata =
		(def_value & pmic_data->chipctrl_mask) |
		(pmic_data->chipctrloff << PMIC_MODEMCTRL_REG_SDWN_SHIFT);
	ret = intel_soc_pmic_writeb(addr, iodata);
	if (ret) {
		pr_err(DRVNAME ": intel_soc_pmic_writeb(SDWN) failed (err: %d)\n",
			   ret);
		return -1;
	}

	usleep_range(TSDWN2OFF, TSDWN2OFF+1);

	/* Write the new register value (POWER_OFF) */
	iodata =
		(iodata & pmic_data->chipctrl_mask) |
		(pmic_data->chipctrloff << PMIC_MODEMCTRL_REG_MODEMOFF_SHIFT);
	ret = intel_soc_pmic_writeb(addr, iodata);
	if (ret) {
		pr_err(DRVNAME ": intel_soc_pmic_writeb(ON) failed (err: %d)\n",
		   ret);
		return -1;
	}

	/* Safety sleep. Avoid to directly call power on. */
	usleep_range(pmic_data->pwr_down_duration,
				 pmic_data->pwr_down_duration);

	return 0;
}
