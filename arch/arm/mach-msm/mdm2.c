/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/clk.h>
#include <linux/mfd/pmic8058.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <mach/mdm2.h>
#include <mach/restart.h>
#include <mach/subsystem_notif.h>
#include <mach/subsystem_restart.h>
#include <linux/msm_charm.h>
#include "msm_watchdog.h"
#include "devices.h"
#include "clock.h"
#include "mdm_private.h"

#define MDM_MODEM_TIMEOUT	6000
#define MDM_HOLD_TIME		4000
#define MDM_MODEM_DELTA		100

static int mdm_debug_on;
static int first_power_on = 1;
static int hsic_peripheral_status = 1;
static DEFINE_MUTEX(hsic_status_lock);

static void mdm_peripheral_connect(struct mdm_modem_drv *mdm_drv)
{
	mutex_lock(&hsic_status_lock);
	if (hsic_peripheral_status)
		goto out;
	if (mdm_drv->pdata->peripheral_platform_device)
		platform_device_add(mdm_drv->pdata->peripheral_platform_device);
	hsic_peripheral_status = 1;
out:
	mutex_unlock(&hsic_status_lock);
}

static void mdm_peripheral_disconnect(struct mdm_modem_drv *mdm_drv)
{
	mutex_lock(&hsic_status_lock);
	if (!hsic_peripheral_status)
		goto out;
	if (mdm_drv->pdata->peripheral_platform_device)
		platform_device_del(mdm_drv->pdata->peripheral_platform_device);
	hsic_peripheral_status = 0;
out:
	mutex_unlock(&hsic_status_lock);
}

static void power_on_mdm(struct mdm_modem_drv *mdm_drv)
{
	mdm_peripheral_disconnect(mdm_drv);

	/* Pull RESET gpio low and wait for it to settle. */
	pr_debug("Pulling RESET gpio low\n");
	gpio_direction_output(mdm_drv->ap2mdm_pmic_reset_n_gpio, 0);
	usleep_range(5000, 10000);

	/* Deassert RESET first and wait for ir to settle. */
	pr_debug("%s: Pulling RESET gpio high\n", __func__);
	gpio_direction_output(mdm_drv->ap2mdm_pmic_reset_n_gpio, 1);
	usleep(1000);

	/* Pull PWR gpio high and wait for it to settle, but only
	 * the first time the mdm is powered up.
	 * Some targets do not use ap2mdm_kpdpwr_n_gpio.
	 */
	if (first_power_on) {
		if (mdm_drv->ap2mdm_kpdpwr_n_gpio > 0) {
			pr_debug("%s: Powering on mdm modem\n", __func__);
			gpio_direction_output(mdm_drv->ap2mdm_kpdpwr_n_gpio, 1);
			usleep(1000);
		}
		first_power_on = 0;
	}
	mdm_peripheral_connect(mdm_drv);

	msleep(200);
}

static void power_down_mdm(struct mdm_modem_drv *mdm_drv)
{
	int i;

	for (i = MDM_MODEM_TIMEOUT; i > 0; i -= MDM_MODEM_DELTA) {
		pet_watchdog();
		msleep(MDM_MODEM_DELTA);
		if (gpio_get_value(mdm_drv->mdm2ap_status_gpio) == 0)
			break;
	}
	if (i <= 0) {
		pr_err("%s: MDM2AP_STATUS never went low.\n",
			 __func__);
		gpio_direction_output(mdm_drv->ap2mdm_pmic_reset_n_gpio, 0);

		for (i = MDM_HOLD_TIME; i > 0; i -= MDM_MODEM_DELTA) {
			pet_watchdog();
			msleep(MDM_MODEM_DELTA);
		}
	}
	if (mdm_drv->ap2mdm_kpdpwr_n_gpio > 0)
		gpio_direction_output(mdm_drv->ap2mdm_kpdpwr_n_gpio, 0);
	mdm_peripheral_disconnect(mdm_drv);
}

static void debug_state_changed(int value)
{
	mdm_debug_on = value;
}

static void mdm_status_changed(struct mdm_modem_drv *mdm_drv, int value)
{
	pr_debug("%s: value:%d\n", __func__, value);

	if (value) {
		mdm_peripheral_disconnect(mdm_drv);
		mdm_peripheral_connect(mdm_drv);
	}
}

static struct mdm_ops mdm_cb = {
	.power_on_mdm_cb = power_on_mdm,
	.power_down_mdm_cb = power_down_mdm,
	.debug_state_changed_cb = debug_state_changed,
	.status_cb = mdm_status_changed,
};

static int __init mdm_modem_probe(struct platform_device *pdev)
{
	return mdm_common_create(pdev, &mdm_cb);
}

static int __devexit mdm_modem_remove(struct platform_device *pdev)
{
	return mdm_common_modem_remove(pdev);
}

static void mdm_modem_shutdown(struct platform_device *pdev)
{
	mdm_common_modem_shutdown(pdev);
}

static struct platform_driver mdm_modem_driver = {
	.remove         = mdm_modem_remove,
	.shutdown	= mdm_modem_shutdown,
	.driver         = {
		.name = "mdm2_modem",
		.owner = THIS_MODULE
	},
};

static int __init mdm_modem_init(void)
{
	return platform_driver_probe(&mdm_modem_driver, mdm_modem_probe);
}

static void __exit mdm_modem_exit(void)
{
	platform_driver_unregister(&mdm_modem_driver);
}

module_init(mdm_modem_init);
module_exit(mdm_modem_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("mdm modem driver");
MODULE_VERSION("2.0");
MODULE_ALIAS("mdm_modem");
