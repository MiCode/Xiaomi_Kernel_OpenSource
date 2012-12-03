/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/debugfs.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <linux/mfd/pm8xxx/misc.h>
#include <mach/mdm.h>
#include <mach/restart.h>
#include <mach/subsystem_notif.h>
#include <mach/subsystem_restart.h>
#include <linux/msm_charm.h>
#include "msm_watchdog.h"
#include "devices.h"

#define CHARM_MODEM_TIMEOUT	6000
#define CHARM_HOLD_TIME		4000
#define CHARM_MODEM_DELTA	100

static void (*power_on_charm)(void);
static void (*power_down_charm)(void);

static int charm_debug_on;
static int charm_status_irq;
static int charm_errfatal_irq;
static int charm_ready;
static enum charm_boot_type boot_type = CHARM_NORMAL_BOOT;
static int charm_boot_status;
static int charm_ram_dump_status;
static struct workqueue_struct *charm_queue;

#define CHARM_DBG(...)	do { if (charm_debug_on) \
					pr_info(__VA_ARGS__); \
			} while (0);


DECLARE_COMPLETION(charm_needs_reload);
DECLARE_COMPLETION(charm_boot);
DECLARE_COMPLETION(charm_ram_dumps);

static void charm_disable_irqs(void)
{
	disable_irq_nosync(charm_errfatal_irq);
	disable_irq_nosync(charm_status_irq);

}

static int charm_subsys_shutdown(const struct subsys_desc *crashed_subsys)
{
	charm_ready = 0;
	power_down_charm();
	return 0;
}

static int charm_subsys_powerup(const struct subsys_desc *crashed_subsys)
{
	power_on_charm();
	boot_type = CHARM_NORMAL_BOOT;
	complete(&charm_needs_reload);
	wait_for_completion(&charm_boot);
	pr_info("%s: charm modem has been restarted\n", __func__);
	INIT_COMPLETION(charm_boot);
	return charm_boot_status;
}

static int charm_subsys_ramdumps(int want_dumps,
				const struct subsys_desc *crashed_subsys)
{
	charm_ram_dump_status = 0;
	if (want_dumps) {
		boot_type = CHARM_RAM_DUMPS;
		complete(&charm_needs_reload);
		wait_for_completion(&charm_ram_dumps);
		INIT_COMPLETION(charm_ram_dumps);
		power_down_charm();
	}
	return charm_ram_dump_status;
}

static struct subsys_device *charm_subsys;

static struct subsys_desc charm_subsystem = {
	.shutdown = charm_subsys_shutdown,
	.ramdump = charm_subsys_ramdumps,
	.powerup = charm_subsys_powerup,
	.name = "external_modem",
};

static int charm_panic_prep(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	int i;

	CHARM_DBG("%s: setting AP2MDM_ERRFATAL high for a non graceful reset\n",
			 __func__);
	if (subsys_get_restart_level(charm_subsys) == RESET_SOC)
		pm8xxx_stay_on();

	charm_disable_irqs();
	gpio_set_value(AP2MDM_ERRFATAL, 1);
	gpio_set_value(AP2MDM_WAKEUP, 1);
	for (i = CHARM_MODEM_TIMEOUT; i > 0; i -= CHARM_MODEM_DELTA) {
		pet_watchdog();
		mdelay(CHARM_MODEM_DELTA);
		if (gpio_get_value(MDM2AP_STATUS) == 0)
			break;
	}
	if (i <= 0)
		pr_err("%s: MDM2AP_STATUS never went low\n", __func__);
	return NOTIFY_DONE;
}

static struct notifier_block charm_panic_blk = {
	.notifier_call  = charm_panic_prep,
};

static int first_boot = 1;

static long charm_modem_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{

	int status, ret = 0;

	if (_IOC_TYPE(cmd) != CHARM_CODE) {
		pr_err("%s: invalid ioctl code\n", __func__);
		return -EINVAL;
	}

	CHARM_DBG("%s: Entering ioctl cmd = %d\n", __func__, _IOC_NR(cmd));
	switch (cmd) {
	case WAKE_CHARM:
		CHARM_DBG("%s: Powering on\n", __func__);
		power_on_charm();
		break;
	case CHECK_FOR_BOOT:
		if (gpio_get_value(MDM2AP_STATUS) == 0)
			put_user(1, (unsigned long __user *) arg);
		else
			put_user(0, (unsigned long __user *) arg);
		break;
	case NORMAL_BOOT_DONE:
		CHARM_DBG("%s: check if charm is booted up\n", __func__);
		get_user(status, (unsigned long __user *) arg);
		if (status)
			charm_boot_status = -EIO;
		else
			charm_boot_status = 0;
		charm_ready = 1;

		gpio_set_value(AP2MDM_KPDPWR_N, 0);
		if (!first_boot)
			complete(&charm_boot);
		else
			first_boot = 0;
		break;
	case RAM_DUMP_DONE:
		CHARM_DBG("%s: charm done collecting RAM dumps\n", __func__);
		get_user(status, (unsigned long __user *) arg);
		if (status)
			charm_ram_dump_status = -EIO;
		else
			charm_ram_dump_status = 0;
		complete(&charm_ram_dumps);
		break;
	case WAIT_FOR_RESTART:
		CHARM_DBG("%s: wait for charm to need images reloaded\n",
				__func__);
		ret = wait_for_completion_interruptible(&charm_needs_reload);
		if (!ret)
			put_user(boot_type, (unsigned long __user *) arg);
		INIT_COMPLETION(charm_needs_reload);
		break;
	default:
		pr_err("%s: invalid ioctl cmd = %d\n", __func__, _IOC_NR(cmd));
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int charm_modem_open(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations charm_modem_fops = {
	.owner		= THIS_MODULE,
	.open		= charm_modem_open,
	.unlocked_ioctl	= charm_modem_ioctl,
};


struct miscdevice charm_modem_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "mdm",
	.fops	= &charm_modem_fops
};



static void charm_status_fn(struct work_struct *work)
{
	pr_info("Reseting the charm because status changed\n");
	subsystem_restart_dev(charm_subsys);
}

static DECLARE_WORK(charm_status_work, charm_status_fn);

static void charm_fatal_fn(struct work_struct *work)
{
	pr_info("Reseting the charm due to an errfatal\n");
	if (subsys_get_restart_level(charm_subsys) == RESET_SOC)
		pm8xxx_stay_on();
	subsystem_restart_dev(charm_subsys);
}

static DECLARE_WORK(charm_fatal_work, charm_fatal_fn);

static irqreturn_t charm_errfatal(int irq, void *dev_id)
{
	CHARM_DBG("%s: charm got errfatal interrupt\n", __func__);
	if (charm_ready && (gpio_get_value(MDM2AP_STATUS) == 1)) {
		CHARM_DBG("%s: scheduling work now\n", __func__);
		queue_work(charm_queue, &charm_fatal_work);
	}
	return IRQ_HANDLED;
}

static irqreturn_t charm_status_change(int irq, void *dev_id)
{
	CHARM_DBG("%s: charm sent status change interrupt\n", __func__);
	if ((gpio_get_value(MDM2AP_STATUS) == 0) && charm_ready) {
		CHARM_DBG("%s: scheduling work now\n", __func__);
		queue_work(charm_queue, &charm_status_work);
	} else if (gpio_get_value(MDM2AP_STATUS) == 1) {
		CHARM_DBG("%s: charm is now ready\n", __func__);
	}
	return IRQ_HANDLED;
}

static int charm_debug_on_set(void *data, u64 val)
{
	charm_debug_on = val;
	return 0;
}

static int charm_debug_on_get(void *data, u64 *val)
{
	*val = charm_debug_on;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(charm_debug_on_fops,
			charm_debug_on_get,
			charm_debug_on_set, "%llu\n");

static int charm_debugfs_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("charm_dbg", 0);
	if (IS_ERR(dent))
		return PTR_ERR(dent);

	debugfs_create_file("debug_on", 0644, dent, NULL,
			&charm_debug_on_fops);
	return 0;
}

static int gsbi9_uart_notifier_cb(struct notifier_block *this,
					unsigned long code, void *_cmd)
{
	switch (code) {
	case SUBSYS_AFTER_SHUTDOWN:
		platform_device_unregister(msm_device_uart_gsbi9);
		msm_device_uart_gsbi9 = msm_add_gsbi9_uart();
		if (IS_ERR(msm_device_uart_gsbi9))
			pr_err("%s(): Failed to create uart gsbi9 device\n",
								__func__);
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block gsbi9_nb = {
	.notifier_call = gsbi9_uart_notifier_cb,
};

static int __init charm_modem_probe(struct platform_device *pdev)
{
	int ret, irq;
	struct charm_platform_data *d = pdev->dev.platform_data;

	gpio_request(AP2MDM_STATUS, "AP2MDM_STATUS");
	gpio_request(AP2MDM_ERRFATAL, "AP2MDM_ERRFATAL");
	gpio_request(AP2MDM_KPDPWR_N, "AP2MDM_KPDPWR_N");
	gpio_request(AP2MDM_PMIC_RESET_N, "AP2MDM_PMIC_RESET_N");
	gpio_request(MDM2AP_STATUS, "MDM2AP_STATUS");
	gpio_request(MDM2AP_ERRFATAL, "MDM2AP_ERRFATAL");
	gpio_request(AP2MDM_WAKEUP, "AP2MDM_WAKEUP");

	gpio_direction_output(AP2MDM_STATUS, 1);
	gpio_direction_output(AP2MDM_ERRFATAL, 0);
	gpio_direction_output(AP2MDM_WAKEUP, 0);
	gpio_direction_input(MDM2AP_STATUS);
	gpio_direction_input(MDM2AP_ERRFATAL);

	power_on_charm = d->charm_modem_on;
	power_down_charm = d->charm_modem_off;

	charm_queue = create_singlethread_workqueue("charm_queue");
	if (!charm_queue) {
		pr_err("%s: could not create workqueue. All charm \
				functionality will be disabled\n",
			__func__);
		ret = -ENOMEM;
		goto fatal_err;
	}

	atomic_notifier_chain_register(&panic_notifier_list, &charm_panic_blk);
	charm_debugfs_init();

	charm_subsys = subsys_register(&charm_subsystem);
	if (IS_ERR(charm_subsys)) {
		ret = PTR_ERR(charm_subsys);
		goto fatal_err;
	}
	subsys_default_online(charm_subsys);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		pr_err("%s: could not get MDM2AP_ERRFATAL IRQ resource. \
			error=%d No IRQ will be generated on errfatal.",
			__func__, irq);
		goto errfatal_err;
	}

	ret = request_irq(irq, charm_errfatal,
		IRQF_TRIGGER_RISING , "charm errfatal", NULL);

	if (ret < 0) {
		pr_err("%s: MDM2AP_ERRFATAL IRQ#%d request failed with error=%d\
			. No IRQ will be generated on errfatal.",
			__func__, irq, ret);
		goto errfatal_err;
	}
	charm_errfatal_irq = irq;

errfatal_err:

	irq = platform_get_irq(pdev, 1);
	if (irq < 0) {
		pr_err("%s: could not get MDM2AP_STATUS IRQ resource. \
			error=%d No IRQ will be generated on status change.",
			__func__, irq);
		goto status_err;
	}

	ret = request_threaded_irq(irq, NULL, charm_status_change,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
		"charm status", NULL);

	if (ret < 0) {
		pr_err("%s: MDM2AP_STATUS IRQ#%d request failed with error=%d\
			. No IRQ will be generated on status change.",
			__func__, irq, ret);
		goto status_err;
	}
	charm_status_irq = irq;

status_err:
	subsys_notif_register_notifier("external_modem", &gsbi9_nb);

	pr_info("%s: Registering charm modem\n", __func__);

	return misc_register(&charm_modem_misc);

fatal_err:
	gpio_free(AP2MDM_STATUS);
	gpio_free(AP2MDM_ERRFATAL);
	gpio_free(AP2MDM_KPDPWR_N);
	gpio_free(AP2MDM_PMIC_RESET_N);
	gpio_free(MDM2AP_STATUS);
	gpio_free(MDM2AP_ERRFATAL);
	return ret;

}


static int __devexit charm_modem_remove(struct platform_device *pdev)
{
	gpio_free(AP2MDM_STATUS);
	gpio_free(AP2MDM_ERRFATAL);
	gpio_free(AP2MDM_KPDPWR_N);
	gpio_free(AP2MDM_PMIC_RESET_N);
	gpio_free(MDM2AP_STATUS);
	gpio_free(MDM2AP_ERRFATAL);

	return misc_deregister(&charm_modem_misc);
}

static void charm_modem_shutdown(struct platform_device *pdev)
{
	int i;

	CHARM_DBG("%s: setting AP2MDM_STATUS low for a graceful restart\n",
		__func__);

	charm_disable_irqs();

	gpio_set_value(AP2MDM_STATUS, 0);
	gpio_set_value(AP2MDM_WAKEUP, 1);

	for (i = CHARM_MODEM_TIMEOUT; i > 0; i -= CHARM_MODEM_DELTA) {
		pet_watchdog();
		msleep(CHARM_MODEM_DELTA);
		if (gpio_get_value(MDM2AP_STATUS) == 0)
			break;
	}

	if (i <= 0) {
		pr_err("%s: MDM2AP_STATUS never went low.\n",
			 __func__);
		gpio_direction_output(AP2MDM_PMIC_RESET_N, 1);
		for (i = CHARM_HOLD_TIME; i > 0; i -= CHARM_MODEM_DELTA) {
			pet_watchdog();
			msleep(CHARM_MODEM_DELTA);
		}
		gpio_direction_output(AP2MDM_PMIC_RESET_N, 0);
	}
	gpio_set_value(AP2MDM_WAKEUP, 0);
}

static struct platform_driver charm_modem_driver = {
	.remove         = charm_modem_remove,
	.shutdown	= charm_modem_shutdown,
	.driver         = {
		.name = "charm_modem",
		.owner = THIS_MODULE
	},
};

static int __init charm_modem_init(void)
{
	return platform_driver_probe(&charm_modem_driver, charm_modem_probe);
}

static void __exit charm_modem_exit(void)
{
	platform_driver_unregister(&charm_modem_driver);
}

module_init(charm_modem_init);
module_exit(charm_modem_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("msm8660 charm modem driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("charm_modem");
