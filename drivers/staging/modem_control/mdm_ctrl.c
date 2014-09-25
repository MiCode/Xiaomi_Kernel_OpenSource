/**
 * linux/modules/drivers/modem_control/mdm_ctrl.c
 *
 * Version 1.0
 *
 * This code allows to power and reset IMC modems.
 * There is a list of commands available in include/linux/mdm_ctrl.h
 * Current version supports the following modems :
 * - IMC6260
 * - IMC6360
 * - IMC7160
 * - IMC7260
 * There is no guarantee for other modems
 *
 *
 * Intel Mobile driver for modem powering.
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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

#define MDM_BOOT_DEVNAME	CONFIG_MDM_CTRL_DEV_NAME

/* Modem readiness wait duration (sec) */
#define MDM_MODEM_READY_DELAY	60

/**
 *  mdm_ctrl_handle_hangup - This function handle the modem reset/coredump
 *  @work: a reference to work queue element
 *
 */
static void mdm_ctrl_handle_hangup(struct work_struct *work)
{
	struct mdm_info *mdm = container_of(work, struct mdm_info, hangup_work);
	int modem_rst;

	/* Check the hangup reason */
	modem_rst = mdm->hangup_causes;

	if (modem_rst & MDM_CTRL_HU_COREDUMP)
		mdm_ctrl_set_state(mdm, MDM_CTRL_STATE_COREDUMP);

	if (modem_rst & MDM_CTRL_HU_RESET)
		mdm_ctrl_set_state(mdm, MDM_CTRL_STATE_WARM_BOOT);

	pr_info(DRVNAME ": %s (reasons: 0x%X)\n", __func__,
			mdm->hangup_causes);
}

/*****************************************************************************
 *
 * Local driver functions
 *
 ****************************************************************************/

static int mdm_ctrl_cold_boot(struct mdm_info *mdm)
{
	int ret = 0;

	struct mdm_ops *mdm_ops = &mdm->pdata->mdm;
	struct cpu_ops *cpu = &mdm->pdata->cpu;
	struct pmic_ops *pmic = &mdm->pdata->pmic;

	void *mdm_data = mdm->pdata->modem_data;
	void *cpu_data = mdm->pdata->cpu_data;
	void *pmic_data = mdm->pdata->pmic_data;

	int rst, pwr_on, cflash_delay;

	pr_warn(DRVNAME ": Cold boot requested");

	/* Set the current modem state */
	mdm_ctrl_set_state(mdm, MDM_CTRL_STATE_COLD_BOOT);

	/* AP request => just ignore the modem reset */
	atomic_set(&mdm->rst_ongoing, 1);

	rst = cpu->get_gpio_rst(cpu_data);
	pwr_on = cpu->get_gpio_pwr(cpu_data);
	cflash_delay = mdm_ops->get_cflash_delay(mdm_data);

	/* @TODO: remove this */
	if (mdm->pdata->mdm_ver != MODEM_2230) {
		if (pmic->power_on_mdm(pmic_data)) {
			pr_err(DRVNAME ": Error PMIC power-ON.");
			ret = -1;
			goto end;
		}
	}

	if (mdm_ops->power_on(mdm_data, rst, pwr_on)) {
		pr_err(DRVNAME ": Error MDM power-ON.");
		ret = -1;
		goto end;
	}

	mdm_ctrl_launch_timer(mdm, cflash_delay, MDM_TIMER_FLASH_ENABLE);

	/* If no IPC ready signal between modem and AP */
	if (!mdm->pdata->cpu.get_irq_rst(mdm->pdata->cpu_data)) {
		atomic_set(&mdm->rst_ongoing, 0);
		mdm_ctrl_set_state(mdm, MDM_CTRL_STATE_IPC_READY);
	}
end:
	return ret;
}

static int mdm_ctrl_normal_warm_reset(struct mdm_info *mdm)
{
	struct mdm_ops *mdm_ops = &mdm->pdata->mdm;
	struct cpu_ops *cpu = &mdm->pdata->cpu;

	void *mdm_data = mdm->pdata->modem_data;
	void *cpu_data = mdm->pdata->cpu_data;

	int rst, wflash_delay;

	pr_info(DRVNAME ": Normal warm reset requested\n");

	/* AP requested reset => just ignore */
	atomic_set(&mdm->rst_ongoing, 1);

	/* Set the current modem state */
	mdm_ctrl_set_state(mdm, MDM_CTRL_STATE_WARM_BOOT);

	rst = cpu->get_gpio_rst(cpu_data);
	wflash_delay = mdm_ops->get_wflash_delay(mdm_data);
	mdm_ops->warm_reset(mdm_data, rst);

	mdm_ctrl_launch_timer(mdm, wflash_delay, MDM_TIMER_FLASH_ENABLE);

	return 0;
}

static int mdm_ctrl_flashing_warm_reset(struct mdm_info *mdm)
{
	struct mdm_ops *mdm_ops = &mdm->pdata->mdm;
	struct cpu_ops *cpu = &mdm->pdata->cpu;

	void *mdm_data = mdm->pdata->modem_data;
	void *cpu_data = mdm->pdata->cpu_data;

	int rst, wflash_delay;

	pr_info(DRVNAME ": Flashing warm reset requested");

	/* AP requested reset => just ignore */
	atomic_set(&mdm->rst_ongoing, 1);

	/* Set the current modem state */
	mdm_ctrl_set_state(mdm, MDM_CTRL_STATE_WARM_BOOT);

	rst = cpu->get_gpio_rst(cpu_data);
	wflash_delay = mdm_ops->get_wflash_delay(mdm_data);
	mdm_ops->warm_reset(mdm_data, rst);

	msleep(wflash_delay);

	return 0;
}

static int mdm_ctrl_power_off(struct mdm_info *mdm)
{
	int ret = 0;

	struct mdm_ops *mdm_ops = &mdm->pdata->mdm;
	struct cpu_ops *cpu = &mdm->pdata->cpu;
	struct pmic_ops *pmic = &mdm->pdata->pmic;

	void *mdm_data = mdm->pdata->modem_data;
	void *cpu_data = mdm->pdata->cpu_data;
	void *pmic_data = mdm->pdata->pmic_data;

	int rst;

	pr_info(DRVNAME ": Power OFF requested");

	/* AP requested reset => just ignore */
	atomic_set(&mdm->rst_ongoing, 1);

	/* Set the modem state to OFF */
	mdm_ctrl_set_state(mdm, MDM_CTRL_STATE_OFF);

	rst = cpu->get_gpio_rst(cpu_data);
	if (mdm_ops->power_off(mdm_data, rst)) {
		pr_err(DRVNAME ": Error MDM power-OFF.");
		ret = -1;
		goto end;
	}
	if (mdm->pdata->mdm_ver != MODEM_2230) {
		if (pmic->power_off_mdm(pmic_data)) {
			pr_err(DRVNAME ": Error PMIC power-OFF.");
			ret = -1;
			goto end;
		}
	}
end:
	return ret;
}

static int mdm_ctrl_cold_reset(struct mdm_info *mdm)
{
	pr_warn(DRVNAME ": Cold reset requested");

	mdm_ctrl_power_off(mdm);
	mdm_ctrl_cold_boot(mdm);

	return 0;
}

/**
 *  mdm_ctrl_coredump_it - Modem has signaled a core dump
 *  @irq: IRQ number
 *  @data: mdm_ctrl driver reference
 *
 *  Schedule a work to handle CORE_DUMP depending on current modem state.
 */
static irqreturn_t mdm_ctrl_coredump_it(int irq, void *data)
{
	struct mdm_info *mdm = data;

	pr_err(DRVNAME ": CORE_DUMP it");

	/* Ignoring event if we are in OFF state. */
	if (mdm_ctrl_get_state(mdm) == MDM_CTRL_STATE_OFF) {
		pr_err(DRVNAME ": CORE_DUMP while OFF\n");
		goto out;
	}

	/* Ignoring if Modem reset is ongoing. */
	if (atomic_read(&mdm->rst_ongoing) == 1) {
		pr_err(DRVNAME ": CORE_DUMP while Modem Reset is ongoing\n");
		goto out;
	}

	/* Set the reason & launch the work to handle the hangup */
	mdm->hangup_causes |= MDM_CTRL_HU_COREDUMP;
	queue_work(mdm->hu_wq, &mdm->hangup_work);

 out:
	return IRQ_HANDLED;
}

/**
 *  mdm_ctrl_reset_it - Modem has changed reset state
 *  @irq: IRQ number
 *  @data: mdm_ctrl driver reference
 *
 *  Change current state and schedule work to handle unexpected resets.
 */
static irqreturn_t mdm_ctrl_reset_it(int irq, void *data)
{
	int value, reset_ongoing;
	struct mdm_info *mdm = data;

	value = mdm->pdata->cpu.get_mdm_state(mdm->pdata->cpu_data);

	/* Ignoring event if we are in OFF state. */
	if (mdm_ctrl_get_state(mdm) == MDM_CTRL_STATE_OFF) {
		/* Logging event in order to minimise risk of hidding bug */
		pr_err(DRVNAME ": RESET_OUT 0x%x while OFF\n", value);
		goto out;
	}

	/* If reset is ongoing we expect falling if applicable and rising
	 * edge.
	 */
	reset_ongoing = atomic_read(&mdm->rst_ongoing);
	if (reset_ongoing) {
		pr_err(DRVNAME ": RESET_OUT 0x%x\n", value);

		/* Rising EDGE (IPC ready) */
		if (value) {
			/* Reset the reset ongoing flag */
			atomic_set(&mdm->rst_ongoing, 0);

			pr_err(DRVNAME ": IPC READY !\n");
			mdm_ctrl_set_state(mdm, MDM_CTRL_STATE_IPC_READY);
		}

		goto out;
	}

	pr_err(DRVNAME ": Unexpected RESET_OUT 0x%x\n", value);

	/* Unexpected reset received */
	atomic_set(&mdm->rst_ongoing, 1);

	/* Set the reason & launch the work to handle the hangup */
	mdm->hangup_causes |= MDM_CTRL_HU_RESET;
	queue_work(mdm->hu_wq, &mdm->hangup_work);

 out:
	return IRQ_HANDLED;
}

/**
 *  clear_hangup_reasons - Clear the hangup reasons flag
 */
static void clear_hangup_reasons(struct mdm_info *mdm)
{
	mdm->hangup_causes = MDM_CTRL_NO_HU;
}

/**
 *  get_hangup_reasons - Hangup reason flag accessor
 */
static int get_hangup_reasons(struct mdm_info *mdm)
{
	return mdm->hangup_causes;
}

/*****************************************************************************
 *
 * Char device functions
 *
 ****************************************************************************/

/**
 *  mdm_ctrl_dev_open - Manage device access
 *  @inode: The node
 *  @filep: Reference to file
 *
 *  Called when a process tries to open the device file
 */
static int mdm_ctrl_dev_open(struct inode *inode, struct file *filep)
{
	unsigned int minor = iminor(inode);
	struct mdm_info *mdm = &mdm_drv->mdm[minor];

	mutex_lock(&mdm->lock);
	/* Only ONE instance of this device can be opened */
	if (mdm_ctrl_get_opened(mdm)) {
		mutex_unlock(&mdm->lock);
		return -EBUSY;
	}

	/* Set the open flag */
	mdm_ctrl_set_opened(mdm, 1);
	mutex_unlock(&mdm->lock);
	return 0;
}

/**
 *  mdm_ctrl_dev_close - Reset open state
 *  @inode: The node
 *  @filep: Reference to file
 *
 *  Called when a process closes the device file.
 */
static int mdm_ctrl_dev_close(struct inode *inode, struct file *filep)
{
	unsigned int minor = iminor(inode);
	struct mdm_info *mdm = &mdm_drv->mdm[minor];

	/* Set the open flag */
	mutex_lock(&mdm->lock);
	mdm_ctrl_set_opened(mdm, 0);
	mutex_unlock(&mdm->lock);
	return 0;
}

inline bool mcd_is_initialized(struct mdm_info *mdm)
{
	return (BOARD_UNSUP != mdm->pdata->board_type) &&
		(MODEM_UNSUP != mdm->pdata->mdm_ver);
}

static int mcd_init(struct mdm_info *mdm)
{
	int ret = 0;

	if (mdm->pdata->mdm.init(mdm->pdata->modem_data)) {
		pr_err(DRVNAME ": MDM init failed...returning -ENODEV.");
		ret = -ENODEV;
		goto out;
	}

	if (mdm->pdata->cpu.init(mdm->pdata->cpu_data)) {
		pr_err(DRVNAME ": CPU init failed...returning -ENODEV.");
		ret = -ENODEV;
		goto del_mdm;
	}

	if (mdm->pdata->cpu.get_irq_rst(mdm->pdata->cpu_data) > 0) {
		ret = request_irq(mdm->pdata->cpu.
			get_irq_rst(mdm->pdata->cpu_data),
			mdm_ctrl_reset_it,
			IRQF_TRIGGER_RISING |
			IRQF_TRIGGER_FALLING |
			IRQF_NO_SUSPEND,
			DRVNAME, mdm);
		if (ret) {
			pr_err(DRVNAME
				": IRQ request failed (err:%d) for GPIO (RST_OUT)\n",
				ret);
			ret = -ENODEV;
			goto del_cpu;
		}
	}

	if (mdm->pdata->cpu.get_irq_cdump(mdm->pdata->cpu_data) > 0) {
		ret = request_irq(mdm->pdata->cpu.
			get_irq_cdump(mdm->pdata->cpu_data),
			mdm_ctrl_coredump_it,
			IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND, DRVNAME,
			mdm);
		if (ret) {
			pr_err(DRVNAME
				": IRQ request failed (err:%d) for GPIO (CORE DUMP)\n",
				ret);
			ret = -ENODEV;
			goto free_irq;
		}
	}

	/* Modem power off sequence */
	if (mdm->pdata->pmic.get_early_pwr_off(mdm->pdata->pmic_data)) {
		if (mdm_ctrl_power_off(mdm)) {
			ret = -EPROBE_DEFER;
			goto free_all;
		}
	}

	/* Modem cold boot sequence */
	if (mdm->pdata->pmic.get_early_pwr_on(mdm->pdata->pmic_data)) {
		if (mdm_ctrl_cold_boot(mdm)) {
			ret = -EPROBE_DEFER;
			goto free_all;
		}
	}

	pr_info(DRVNAME ": %s initialization has succeed\n", __func__);
	return 0;

 free_all:
	free_irq(mdm->pdata->cpu.get_irq_cdump(mdm->pdata->cpu_data), mdm);

 free_irq:
	free_irq(mdm->pdata->cpu.get_irq_rst(mdm->pdata->cpu_data), mdm);

 del_cpu:
	mdm->pdata->cpu.cleanup(mdm->pdata->cpu_data);

 del_mdm:
	mdm->pdata->mdm.cleanup(mdm->pdata->modem_data);

out:
	return ret;
}

/**
 *  mdm_ctrl_dev_ioctl - Process ioctl requests
 *  @filep: Reference to file that stores private data.
 *  @cmd: Command that should be executed.
 *  @arg: Command's arguments.
 *
 */
long mdm_ctrl_dev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	unsigned int minor = iminor(file_inode(filep));
	struct mdm_info *mdm = &mdm_drv->mdm[minor];
	long ret = 0;
	unsigned int mdm_state;
	unsigned int param;
	struct mdm_ctrl_cfg cfg;

	pr_info(DRVNAME ": ioctl request 0x%x received on %d\n", cmd, minor);

	if (!mcd_is_initialized(mdm)) {
		if (cmd == MDM_CTRL_SET_CFG) {
			ret = copy_from_user(&cfg, (void *)arg, sizeof(cfg));
			if (ret < 0) {
				pr_info(DRVNAME ": copy from user failed ret = %ld\n",
					ret);
				goto out;
			}

			/* The modem family must be set first */
			mcd_set_mdm(mdm->pdata, cfg.type);
			mdm->pdata->board_type = cfg.board;

			mdm_ctrl_set_mdm_cpu(mdm);
			mcd_finalize_cpu_data(mdm->pdata);

			ret = mcd_init(mdm);
			if (!ret)
				pr_info(DRVNAME ": modem (board: %d, family: %d)",
						cfg.board, cfg.type);
			else
				mdm->is_mdm_ctrl_disabled = true;
		} else {
			pr_err(DRVNAME ": Provide modem and board type first");
			ret = -EINVAL;
		}
		goto out;
	}

	mdm_state = mdm_ctrl_get_state(mdm);

	switch (cmd) {
	case MDM_CTRL_POWER_OFF:
		/* Unconditional power off */
		mdm_ctrl_power_off(mdm);
		break;

	case MDM_CTRL_POWER_ON:
		/* Only allowed when modem is OFF or in unknown state */
		if ((mdm_state == MDM_CTRL_STATE_OFF) ||
				(mdm_state == MDM_CTRL_STATE_UNKNOWN))
			mdm_ctrl_cold_boot(mdm);
		else
			/* Specific log in COREDUMP state */
			if (mdm_state == MDM_CTRL_STATE_COREDUMP)
				pr_err(DRVNAME ": Power ON not allowed (coredump)");
			else
				pr_info(DRVNAME ": Powering on while already on");
		break;

	case MDM_CTRL_WARM_RESET:
		/* Allowed in any state unless OFF */
		if (mdm_state != MDM_CTRL_STATE_OFF)
			mdm_ctrl_normal_warm_reset(mdm);
		else
			pr_err(DRVNAME ": Warm reset not allowed (Modem OFF)");
		break;

	case MDM_CTRL_FLASHING_WARM_RESET:
		/* Allowed in any state unless OFF */
		if (mdm_state != MDM_CTRL_STATE_OFF)
			mdm_ctrl_flashing_warm_reset(mdm);
		else
			pr_err(DRVNAME ": Warm reset not allowed (Modem OFF)");
		break;

	case MDM_CTRL_COLD_RESET:
		/* Allowed in any state unless OFF */
		if (mdm_state != MDM_CTRL_STATE_OFF)
			mdm_ctrl_cold_reset(mdm);
		else
			pr_err(DRVNAME ": Cold reset not allowed (Modem OFF)");
		break;

	case MDM_CTRL_SET_STATE:
		/* Read the user command params */
		ret = copy_from_user(&param, (void *)arg, sizeof(param));
		if (ret < 0) {
			pr_info(DRVNAME ": copy from user failed ret = %ld\n",
				ret);
			goto out;
		}

		/* Filtering states. Allow any state ? */
		param &=
		    (MDM_CTRL_STATE_OFF |
		     MDM_CTRL_STATE_COLD_BOOT |
		     MDM_CTRL_STATE_WARM_BOOT |
		     MDM_CTRL_STATE_COREDUMP |
		     MDM_CTRL_STATE_IPC_READY |
		     MDM_CTRL_STATE_FW_DOWNLOAD_READY);

		mdm_ctrl_set_state(mdm, param);
		break;

	case MDM_CTRL_GET_STATE:
		/* Return supposed current state.
		 * Real state can be different.
		 */
		param = mdm_state;

		ret = copy_to_user((void __user *)arg, &param, sizeof(param));
		if (ret < 0) {
			pr_info(DRVNAME ": copy to user failed ret = %ld\n",
				ret);
			return ret;
		}
		break;

	case MDM_CTRL_GET_HANGUP_REASONS:
		/* Return last hangup reason. Can be cumulative
		 * if they were not cleared since last hangup.
		 */
		param = get_hangup_reasons(mdm);

		ret = copy_to_user((void __user *)arg, &param, sizeof(param));
		if (ret < 0) {
			pr_info(DRVNAME ": copy to user failed ret = %ld\n",
				ret);
			return ret;
		}
		break;

	case MDM_CTRL_CLEAR_HANGUP_REASONS:
		clear_hangup_reasons(mdm);
		break;

	case MDM_CTRL_SET_POLLED_STATES:
		/* Set state to poll on. */
		/* Read the user command params */
		ret = copy_from_user(&param, (void *)arg, sizeof(param));
		if (ret < 0) {
			pr_info(DRVNAME ": copy from user failed ret = %ld\n",
				ret);
			return ret;
		}
		mdm->polled_states = param;
		/* Poll is active ? */
		if (waitqueue_active(&mdm->wait_wq)) {
			mdm_state = mdm_ctrl_get_state(mdm);
			/* Check if current state is awaited */
			if (mdm_state)
				mdm->polled_state_reached = ((mdm_state & param)
							     == mdm_state);

			/* Waking up the wait work queue to handle any
			 * polled state reached.
			 */
			wake_up(&mdm->wait_wq);
		} else {
			/* Assume that mono threaded client are probably
			 * not polling yet and that they are not interested
			 * in the current state. This state may change until
			 * they start the poll. May be an issue for some cases.
			 */
			mdm->polled_state_reached = false;
		}

		pr_info(DRVNAME ": states polled = 0x%x\n",
			mdm->polled_states);
		break;

	case MDM_CTRL_SET_CFG:
		pr_info(DRVNAME ": already configured\n");
		ret = -EBUSY;
		break;

	default:
		pr_err(DRVNAME ": ioctl command %x unknown\n", cmd);
		ret = -ENOIOCTLCMD;
	}

 out:
	return ret;
}

/**
 *  mdm_ctrl_dev_read - Device read function
 *  @filep: Reference to file
 *  @data: User data
 *  @count: Bytes read.
 *  @ppos: Reference to position in file.
 *
 *  Called when a process, which already opened the dev file, attempts to
 *  read from it. Not allowed.
 */
static ssize_t mdm_ctrl_dev_read(struct file *filep,
				 char __user *data,
				 size_t count, loff_t *ppos)
{
	pr_err(DRVNAME ": Nothing to read\n");
	return -EINVAL;
}

/**
 *  mdm_ctrl_dev_write - Device write function
 *  @filep: Reference to file
 *  @data: User data
 *  @count: Bytes read.
 *  @ppos: Reference to position in file.
 *
 *  Called when a process writes to dev file.
 *  Not allowed.
 */
static ssize_t mdm_ctrl_dev_write(struct file *filep,
				  const char __user *data,
				  size_t count, loff_t *ppos)
{
	pr_err(DRVNAME ": Nothing to write to\n");
	return -EINVAL;
}

/**
 *  mdm_ctrl_dev_poll - Poll function
 *  @filep: Reference to file storing private data
 *  @pt: Reference to poll table structure
 *
 *  Flush the change state workqueue to ensure there is no new state pending.
 *  Relaunch the poll wait workqueue.
 *  Return POLLHUP|POLLRDNORM if any of the polled states was reached.
 */
static unsigned int mdm_ctrl_dev_poll(struct file *filep,
				      struct poll_table_struct *pt)
{
	unsigned int minor = iminor(file_inode(filep));
	struct mdm_info *mdm = &mdm_drv->mdm[minor];
	unsigned int ret = 0;

	/* Wait event change */
	poll_wait(filep, &mdm->wait_wq, pt);

	/* State notify */
	if (mdm->polled_state_reached ||
	    (mdm_ctrl_get_state(mdm) & mdm->polled_states)) {

		mdm->polled_state_reached = false;
		ret |= POLLHUP | POLLRDNORM;
		pr_info(DRVNAME ": POLLHUP occured. Current state = 0x%x\n",
			mdm_ctrl_get_state(mdm));
#ifdef CONFIG_HAS_WAKELOCK
		wake_unlock(&mdm->stay_awake);
#endif
	}

	return ret;
}

/**
 * Device driver file operations
 */
static const struct file_operations mdm_ctrl_ops = {
	.open = mdm_ctrl_dev_open,
	.read = mdm_ctrl_dev_read,
	.write = mdm_ctrl_dev_write,
	.poll = mdm_ctrl_dev_poll,
	.release = mdm_ctrl_dev_close,
	.unlocked_ioctl = mdm_ctrl_dev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mdm_ctrl_dev_ioctl
#endif
};

/**
 * Destroy all modem specific resources like timer, workqueue, etc
 */
static void mdm_cleanup(struct mdm_info *mdm)
{
	if (mdm->hu_wq) {
		/* if wq is initialized, it means that timer, wake-lock and
		 * mutex are also initialized */
		destroy_workqueue(mdm->hu_wq);
		del_timer(&mdm->flashing_timer);
		mutex_destroy(&mdm->lock);
#ifdef CONFIG_HAS_WAKELOCK
		wake_lock_destroy(&mdm->stay_awake);
#endif
	}
}

/**
 *  mdm_ctrl_module_init - initialises the Modem Control driver
 *
 */
static int mdm_ctrl_module_probe(struct platform_device *pdev)
{
	int ret;
	struct mdm_ctrl *new_drv;
	int i;
	char name[25];

	pr_info(DRVNAME ": probing mdm_ctrl\n");
	/* Allocate modem struct data */
	new_drv = kzalloc(sizeof(struct mdm_ctrl), GFP_KERNEL);
	if (!new_drv) {
		pr_err(DRVNAME ": Out of memory(new_drv)\n");
		ret = -ENOMEM;
		goto out;
	}

	pr_info(DRVNAME ": Getting device infos\n");

	/* Pre-initialisation: Retrieve platform device data.
	 * For ACPI platforms, this function shall be called before
	 * get_nb_mdms. Otherwise, the number of modem is not known.
	 *
	 * new_drv will contain all modem specifics data such as cpu name,
	 * pmic and enabling of early power mode */
	if (mdm_ctrl_get_device_info(new_drv, pdev)) {
		pr_err(DRVNAME ": failed to retrieve platform data\n");
		ret = -ENODEV;
		goto free_drv;
	}

	new_drv->nb_mdms = get_nb_mdms();
	pr_info(DRVNAME ": number of modems: %d\n", new_drv->nb_mdms);

	/* Register the device */
	ret = alloc_chrdev_region(&new_drv->tdev, 0, new_drv->nb_mdms,
			MDM_BOOT_DEVNAME);
	if (ret) {
		pr_err(DRVNAME ": alloc_chrdev_region failed (err: %d)\n", ret);
		goto free_drv;
	}

	new_drv->major = MAJOR(new_drv->tdev);
	cdev_init(&new_drv->cdev, &mdm_ctrl_ops);
	new_drv->cdev.owner = THIS_MODULE;

	ret = cdev_add(&new_drv->cdev, new_drv->tdev, new_drv->nb_mdms);
	if (ret) {
		pr_err(DRVNAME ": cdev_add failed (err: %d)\n", ret);
		goto unreg_reg;
	}

	new_drv->class = class_create(THIS_MODULE, DRVNAME);
	if (IS_ERR(new_drv->class)) {
		pr_err(DRVNAME ": class_create failed (err: %d)\n", ret);
		ret = -EIO;
		goto del_cdev;
	}

	new_drv->mdm = kzalloc(sizeof(struct mdm_info) * new_drv->nb_mdms,
				GFP_KERNEL);
	if (!new_drv->mdm) {
		pr_err(DRVNAME ": Out of memory (new_drv->mdm)\n");
		ret = -ENOMEM;
		goto del_class;
	}

	for (i = 0; i < new_drv->nb_mdms; i++) {
		struct mdm_info *mdm = &new_drv->mdm[i];
		if (!mdm) {
			pr_err(DRVNAME " %s: mdm is NULL\n", __func__);
			break;
		}

		mdm_ctrl_get_modem_data(new_drv, i);
		if (mdm->is_mdm_ctrl_disabled) {
			pr_info(DRVNAME ": modem %d is disabled\n", i);
			continue;
		}

		mdm->polled_state_reached = false;
		mdm->dev = device_create(new_drv->class, NULL,
				MKDEV(new_drv->major, i),
				NULL, MDM_BOOT_DEVNAME"%d", i);

		if (IS_ERR(mdm->dev)) {
			pr_err(DRVNAME ": device_create failed (err: %ld)\n",
					PTR_ERR(mdm->dev));
			ret = -EIO;
			goto del_mdms;
		}

		/* Create a high priority ordered
		 * workqueue to change modem state */
		INIT_WORK(&mdm->hangup_work, mdm_ctrl_handle_hangup);

		/* Create a workqueue to manage hangup */
		snprintf(name, sizeof(name), "%s-hu_wq%d", DRVNAME, i);
		mdm->hu_wq = create_singlethread_workqueue(name);
		if (!mdm->hu_wq) {
			pr_err(DRVNAME
				": Unable to create control workqueue: %s\n",
				name);
			ret = -EIO;
			goto del_mdms;
		}

		/* Initialization */
		mutex_init(&mdm->lock);
		init_waitqueue_head(&mdm->wait_wq);
		init_timer(&mdm->flashing_timer);

#ifdef CONFIG_HAS_WAKELOCK
		snprintf(name, sizeof(name), "%s-wakelock%d", DRVNAME, i);
		wake_lock_init(&mdm->stay_awake, WAKE_LOCK_SUSPEND, name);
#endif

		if (mdm->pdata->mdm_ver != MODEM_2230) {
			if (mdm->pdata->pmic.init(mdm->pdata->pmic_data)) {
				pr_err(DRVNAME ": PMIC init failed...returning -ENODEV\n");
				ret = -ENODEV;
				goto del_mdms;
			}
		}

		mdm_ctrl_set_state(mdm, MDM_CTRL_STATE_OFF);
		pr_info(DRVNAME " %s: Modem %d initialized\n", __func__, i);
	}

	/* Everything is OK */
	mdm_drv = new_drv;

	pr_info(DRVNAME ": MCD initialization successful\n");
	return 0;

 del_mdms:
	for (i = 0; i < new_drv->nb_mdms; i++)
		mdm_cleanup(&new_drv->mdm[i]);

	kfree(new_drv->mdm);

	device_destroy(new_drv->class, new_drv->tdev);

 del_class:
	class_destroy(new_drv->class);

 del_cdev:
	cdev_del(&new_drv->cdev);

 unreg_reg:
	unregister_chrdev_region(new_drv->tdev, 1);

 free_drv:
	kfree(new_drv->all_pdata);
	kfree(new_drv);

 out:
	pr_err(DRVNAME ": ERROR initializing MCD (err:%d)\n", ret);
	return ret;
}

/**
 *  mdm_ctrl_module_exit - Frees the resources taken by the control driver
 */
static int mdm_ctrl_module_remove(struct platform_device *pdev)
{
	int i = 0;

	if (!mdm_drv)
		return 0;

	for (i = 0; i < mdm_drv->nb_mdms; i++) {
		struct mdm_info *mdm = &mdm_drv->mdm[i];

		if (mdm->is_mdm_ctrl_disabled)
			continue;

		mdm_cleanup(mdm);

		if (mdm->pdata->cpu.get_irq_cdump(mdm->pdata->cpu_data) > 0)
			free_irq(mdm->pdata->cpu.
				 get_irq_cdump(mdm->pdata->cpu_data), NULL);
		if (mdm->pdata->cpu.get_irq_rst(mdm->pdata->cpu_data) > 0)
			free_irq(mdm->pdata->cpu.
				 get_irq_rst(mdm->pdata->cpu_data), NULL);

		mdm->pdata->mdm.cleanup(mdm->pdata->modem_data);
		mdm->pdata->cpu.cleanup(mdm->pdata->cpu_data);
		mdm->pdata->pmic.cleanup(mdm->pdata->pmic_data);
	}

	/* Unregister the device */
	device_destroy(mdm_drv->class, mdm_drv->tdev);
	class_destroy(mdm_drv->class);
	cdev_del(&mdm_drv->cdev);
	unregister_chrdev_region(mdm_drv->tdev, 1);

	/* Free the driver context */
	kfree(mdm_drv->all_pdata);
	kfree(mdm_drv->mdm);
	kfree(mdm_drv);

	mdm_drv = NULL;

	return 0;
}

/* FOR ACPI HANDLING */
static struct acpi_device_id mdm_ctrl_acpi_ids[] = {
	/* ACPI IDs here */
	{"MCD0001", 0},
	{}
};

MODULE_DEVICE_TABLE(acpi, mdm_ctrl_acpi_ids);

static const struct platform_device_id mdm_ctrl_id_table[] = {
	{DEVICE_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(platform, mdm_ctrl_id_table);

static struct platform_driver mcd_driver = {
	.probe = mdm_ctrl_module_probe,
	.remove = mdm_ctrl_module_remove,
	.driver = {
		   .name = DRVNAME,
		   .owner = THIS_MODULE,
		   /* FOR ACPI HANDLING */
		   .acpi_match_table = ACPI_PTR(mdm_ctrl_acpi_ids),
		   },
	.id_table = mdm_ctrl_id_table,
};

static int __init mdm_ctrl_module_init(void)
{
	return platform_driver_register(&mcd_driver);
}

static void __exit mdm_ctrl_module_exit(void)
{
	platform_driver_unregister(&mcd_driver);
}

module_init(mdm_ctrl_module_init);
module_exit(mdm_ctrl_module_exit);

MODULE_AUTHOR("Faouaz Tenoutit <faouazx.tenoutit@intel.com>");
MODULE_AUTHOR("Frederic Berat <fredericx.berat@intel.com>");
MODULE_DESCRIPTION("Intel Modem control driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DEVICE_NAME);
