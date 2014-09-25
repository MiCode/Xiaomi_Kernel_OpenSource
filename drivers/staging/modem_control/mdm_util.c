/*
 * linux/drivers/modem_control/mdm_util.c
 *
 * Version 1.0
 *
 * Utilities for modem control driver.
 *
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * Contact: Faouaz Tenoutit <faouazx.tenoutit@intel.com>
 *          Frederic Berat <fredericx.berat@intel.com>
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
#include "mcd_cpu.h"
#include "mcd_pmic.h"
#include "mcd_acpi.h"

/* Modem control driver instance */
struct mdm_ctrl *mdm_drv;

/**
 *  mdm_ctrl_set_opened - Set the open device state
 *  @drv: Reference to the driver structure
 *  @value: Value to set
 *
 */
inline void mdm_ctrl_set_opened(struct mdm_info *mdm, int value)
{
	/* Set the open flag */
	mdm->opened = value;
}

/**
 *  mdm_ctrl_get_opened - Return the device open state
 *  @drv: Reference to the driver structure
 *
 */
inline int mdm_ctrl_get_opened(struct mdm_info *mdm)
{
	/* Get the open flag */
	return mdm->opened;
}

/**
 *  mdm_ctrl_enable_flashing - Set the modem state to FW_DOWNLOAD_READY
 *
 */
void mdm_ctrl_enable_flashing(unsigned long int param)
{
	struct mdm_info *mdm = (struct mdm_info *)param;

	del_timer(&mdm->flashing_timer);
	if (mdm_ctrl_get_state(mdm) != MDM_CTRL_STATE_IPC_READY)
		mdm_ctrl_set_state(mdm, MDM_CTRL_STATE_FW_DOWNLOAD_READY);
}

/**
 *  mdm_ctrl_launch_timer - Timer launcher helper
 *  @mdm: modem info
 *  @delay: Timer duration
 *  @timer_type: Timer type
 *
 *  Type can be MDM_TIMER_FLASH_ENABLE.
 *  Note: Type MDM_TIMER_FLASH_DISABLE is not used anymore.
 */
void mdm_ctrl_launch_timer(struct mdm_info *mdm, int delay,
			   unsigned int timer_type)
{
	struct timer_list *timer = &mdm->flashing_timer;
	timer->data = (unsigned long int)mdm;

	switch (timer_type) {
	case MDM_TIMER_FLASH_ENABLE:
		timer->function = mdm_ctrl_enable_flashing;
		break;
	case MDM_TIMER_FLASH_DISABLE:
	default:
		pr_err(DRVNAME ": Unrecognized timer type %d", timer_type);
		del_timer(timer);
		return;
	}
	mod_timer(timer, jiffies + msecs_to_jiffies(delay));
}

/**
 *  mdm_ctrl_set_mdm_cpu - Set modem sequences functions to use
 *  @drv: Reference to the driver structure
 *
 */
void mdm_ctrl_set_mdm_cpu(struct mdm_info *mdm)
{
	int board_type = mdm->pdata->board_type;
	struct mcd_base_info *pdata = mdm->pdata;

	pr_info(DRVNAME ": board type: %d", board_type);

	switch (board_type) {
	case BOARD_AOB:
		pdata->mdm.init = mcd_mdm_init;
		if (mdm->pdata->mdm_ver == MODEM_2230)
			pdata->mdm.power_on = mcd_mdm_cold_boot_2230;
		else
			pdata->mdm.power_on = mcd_mdm_cold_boot;
		pdata->mdm.warm_reset = mcd_mdm_warm_reset;
		pdata->mdm.power_off = mcd_mdm_power_off;
		pdata->mdm.cleanup = mcd_mdm_cleanup;
		pdata->mdm.get_wflash_delay = mcd_mdm_get_wflash_delay;
		pdata->mdm.get_cflash_delay = mcd_mdm_get_cflash_delay;
		pdata->cpu.init = cpu_init_gpio;
		pdata->cpu.cleanup = cpu_cleanup_gpio;
		pdata->cpu.get_mdm_state = get_gpio_mdm_state;
		pdata->cpu.get_irq_cdump = get_gpio_irq_cdump;
		pdata->cpu.get_irq_rst = get_gpio_irq_rst;
		pdata->cpu.get_gpio_rst = get_gpio_rst;
		pdata->cpu.get_gpio_pwr = get_gpio_pwr;
		break;
	case BOARD_NGFF:
		pdata->mdm.init = mcd_mdm_init;
		pdata->mdm.power_on = mcd_mdm_cold_boot_ngff;
		pdata->mdm.warm_reset = mcd_mdm_warm_reset;
		pdata->mdm.power_off = mcd_mdm_power_off;
		pdata->mdm.cleanup = mcd_mdm_cleanup;
		pdata->mdm.get_wflash_delay = mcd_mdm_get_wflash_delay;
		pdata->mdm.get_cflash_delay = mcd_mdm_get_cflash_delay;
		pdata->cpu.init = cpu_init_gpio_ngff;
		pdata->cpu.cleanup = cpu_cleanup_gpio_ngff;
		pdata->cpu.get_mdm_state = get_gpio_mdm_state_ngff;
		pdata->cpu.get_irq_cdump = get_gpio_irq_cdump_ngff;
		pdata->cpu.get_irq_rst = get_gpio_irq_rst_ngff;
		pdata->cpu.get_gpio_rst = get_gpio_rst;
		pdata->cpu.get_gpio_pwr = get_gpio_pwr;
		break;
	default:
		pr_info(DRVNAME ": Can't retrieve conf specific functions");
		mdm->is_mdm_ctrl_disabled = true;
		break;
	}
}

/**
 * configures PMIC
 * @drv: Reference to the driver structure
 */
static void mdm_ctrl_set_pmic(struct mdm_info *mdm)
{
	int pmic_type = mdm->pdata->pmic_ver;
	struct mcd_base_info *pdata = mdm->pdata;

	switch (pmic_type) {
	case PMIC_MFLD:
	case PMIC_MRFL:
	case PMIC_BYT:
	case PMIC_MOOR:
	case PMIC_CHT:
		pdata->pmic.init = pmic_io_init;
		pdata->pmic.power_on_mdm = pmic_io_power_on_mdm;
		pdata->pmic.power_off_mdm = pmic_io_power_off_mdm;
		pdata->pmic.cleanup = pmic_io_cleanup;
		pdata->pmic.get_early_pwr_on = pmic_io_get_early_pwr_on;
		pdata->pmic.get_early_pwr_off = pmic_io_get_early_pwr_off;
		break;
	case PMIC_CLVT:
		pdata->pmic.init = pmic_io_init;
		pdata->pmic.power_on_mdm = pmic_io_power_on_ctp_mdm;
		pdata->pmic.power_off_mdm = pmic_io_power_off_mdm;
		pdata->pmic.cleanup = pmic_io_cleanup;
		pdata->pmic.get_early_pwr_on = pmic_io_get_early_pwr_on;
		pdata->pmic.get_early_pwr_off = pmic_io_get_early_pwr_off;
		break;
	default:
		pr_info(DRVNAME ": Can't retrieve pmic specific functions");
		mdm->is_mdm_ctrl_disabled = true;
		break;
	}
}

/**
 *  mdm_ctrl_set_state -  Effectively sets the modem state on work execution
 *  @state : New state to set
 *
 */
inline void mdm_ctrl_set_state(struct mdm_info *mdm, int state)
{
	/* Set the current modem state */
	atomic_set(&mdm->modem_state, state);
	if (likely(state != MDM_CTRL_STATE_UNKNOWN) &&
	    (state & mdm->polled_states)) {

		mdm->polled_state_reached = true;
		/* Waking up the poll work queue */
		wake_up(&mdm->wait_wq);
		pr_info(DRVNAME ": Waking up polling 0x%x\r\n", state);
#ifdef CONFIG_HAS_WAKELOCK
		/* Grab the wakelock for 10 ms to avoid
		   the system going to sleep */
		if (mdm->opened)
			wake_lock_timeout(&mdm->stay_awake,
					msecs_to_jiffies(10));
#endif

	}
}

/**
 *  mdm_ctrl_get_state - Return the local current modem state
 *  @drv: Reference to the driver structure
 *
 *  Note: Real current state may be different in case of self-reset
 *	  or if user has manually changed the state.
 */
inline int mdm_ctrl_get_state(struct mdm_info *mdm)
{
	return atomic_read(&mdm->modem_state);
}

/**
 *  modem_ctrl_create_pdata - Create platform data
 *
 *  pdata is created base on information given by platform.
 *  Data used is the modem type, the cpu type and the pmic type.
 */
struct mcd_base_info *modem_ctrl_get_dev_data(struct platform_device *pdev)
{
	struct mcd_base_info *info = NULL;

		/* FOR ACPI HANDLING */
		if (get_modem_acpi_data(pdev)) {
			pr_err(DRVNAME
			       " %s: No registered info found. Disabling driver.",
			       __func__);
			return NULL;
		}

	info = pdev->dev.platform_data;

	pr_err(DRVNAME ": cpu: %d pmic: %d.", info->cpu_ver, info->pmic_ver);
	if ((info->cpu_ver == CPU_UNSUP) || (info->pmic_ver == PMIC_UNSUP)) {
		/* mdm_ctrl is disabled as some components */
		/* of the platform are not supported */
		kfree(info);
		return NULL;
	}

	return info;
}

/**
 *  mdm_ctrl_get_device_info - Create platform and modem data.
 *  @drv: Reference to the driver structure
 *  @pdev: Reference to platform device data
 *
 *  Platform are build from SFI table data.
 */
int mdm_ctrl_get_device_info(struct mdm_ctrl *drv,
			      struct platform_device *pdev)
{
	int ret = 0;

	drv->all_pdata = modem_ctrl_get_dev_data(pdev);
	if (!drv->all_pdata)
		ret = -1;

	return ret;
}

/**
 * mdm_ctrl_get_modem_data - get platform data for one modem
 * @drv: Reference to the driver structure
 * @minor: modem id
 *
 * @return 0 if successful
 */
int mdm_ctrl_get_modem_data(struct mdm_ctrl *drv, int minor)
{
	int ret = -1;

	if (!drv->all_pdata) {
		pr_err(DRVNAME ": %s platform data is NULL\n", __func__);
		goto out;
	}

	drv->mdm[minor].pdata = &drv->all_pdata[minor];

	if (!drv->mdm[minor].pdata->cpu_data) {
		drv->mdm[minor].is_mdm_ctrl_disabled = true;
		pr_info(DRVNAME " %s: Disabling modem %d. No known device\n",
				__func__, minor);
	} else {
		drv->mdm[minor].pdata->board_type = BOARD_UNSUP;
		mdm_ctrl_set_pmic(&drv->mdm[minor]);
		ret = 0;
	}

 out:
	return ret;
}
