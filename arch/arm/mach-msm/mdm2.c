/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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
static int power_on_count;
static int hsic_peripheral_status;
static DEFINE_MUTEX(hsic_status_lock);

static void mdm_peripheral_connect(struct mdm_modem_drv *mdm_drv)
{
	if (!mdm_drv->pdata->peripheral_platform_device)
		return;

	mutex_lock(&hsic_status_lock);
	if (hsic_peripheral_status)
		goto out;
	platform_device_add(mdm_drv->pdata->peripheral_platform_device);
	hsic_peripheral_status = 1;
out:
	mutex_unlock(&hsic_status_lock);
}

static void mdm_peripheral_disconnect(struct mdm_modem_drv *mdm_drv)
{
	if (!mdm_drv->pdata->peripheral_platform_device)
		return;

	mutex_lock(&hsic_status_lock);
	if (!hsic_peripheral_status)
		goto out;
	platform_device_del(mdm_drv->pdata->peripheral_platform_device);
	hsic_peripheral_status = 0;
out:
	mutex_unlock(&hsic_status_lock);
}

static void mdm_power_down_common(struct mdm_modem_drv *mdm_drv)
{
	int soft_reset_direction =
		mdm_drv->pdata->soft_reset_inverted ? 1 : 0;

	gpio_direction_output(mdm_drv->ap2mdm_soft_reset_gpio,
				soft_reset_direction);
	mdm_peripheral_disconnect(mdm_drv);
}

static void mdm_do_first_power_on(struct mdm_modem_drv *mdm_drv)
{
	int soft_reset_direction =
		mdm_drv->pdata->soft_reset_inverted ? 0 : 1;

	if (power_on_count != 1) {
		pr_err("%s: Calling fn when power_on_count != 1\n",
			   __func__);
		return;
	}

	pr_err("%s: Powering on modem for the first time\n", __func__);
	mdm_peripheral_disconnect(mdm_drv);

	/* If the device has a kpd pwr gpio then toggle it. */
	if (mdm_drv->ap2mdm_kpdpwr_n_gpio > 0) {
		/* Pull AP2MDM_KPDPWR gpio high and wait for PS_HOLD to settle,
		 * then	pull it back low.
		 */
		pr_debug("%s: Pulling AP2MDM_KPDPWR gpio high\n", __func__);
		gpio_direction_output(mdm_drv->ap2mdm_kpdpwr_n_gpio, 1);
		msleep(1000);
		gpio_direction_output(mdm_drv->ap2mdm_kpdpwr_n_gpio, 0);
	}

	/* De-assert the soft reset line. */
	pr_debug("%s: De-asserting soft reset gpio\n", __func__);
	gpio_direction_output(mdm_drv->ap2mdm_soft_reset_gpio,
						  soft_reset_direction);

	mdm_peripheral_connect(mdm_drv);
	msleep(200);
}

static void mdm_do_soft_power_on(struct mdm_modem_drv *mdm_drv)
{
	int soft_reset_direction =
		mdm_drv->pdata->soft_reset_inverted ? 0 : 1;

	/* De-assert the soft reset line. */
	pr_err("%s: soft resetting mdm modem\n", __func__);

	mdm_peripheral_disconnect(mdm_drv);

	gpio_direction_output(mdm_drv->ap2mdm_soft_reset_gpio,
		soft_reset_direction == 1 ? 0 : 1);
	usleep_range(5000, 10000);
	gpio_direction_output(mdm_drv->ap2mdm_soft_reset_gpio,
		soft_reset_direction == 1 ? 1 : 0);

	mdm_peripheral_connect(mdm_drv);
	msleep(200);
}

static void mdm_power_on_common(struct mdm_modem_drv *mdm_drv)
{
	power_on_count++;

	/* this gpio will be used to indicate apq readiness,
	 * de-assert it now so that it can be asserted later.
	 * May not be used.
	 */
	if (mdm_drv->ap2mdm_wakeup_gpio > 0)
		gpio_direction_output(mdm_drv->ap2mdm_wakeup_gpio, 0);

	/*
	 * If we did an "early power on" then ignore the very next
	 * power-on request because it would the be first request from
	 * user space but we're already powered on. Ignore it.
	 */
	if (mdm_drv->pdata->early_power_on &&
			(power_on_count == 2))
		return;

	if (power_on_count == 1)
		mdm_do_first_power_on(mdm_drv);
	else
		mdm_do_soft_power_on(mdm_drv);
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
		if (mdm_drv->ap2mdm_wakeup_gpio > 0)
			gpio_direction_output(mdm_drv->ap2mdm_wakeup_gpio, 1);
	}
}

static struct mdm_ops mdm_cb = {
	.power_on_mdm_cb = mdm_power_on_common,
	.reset_mdm_cb = mdm_power_on_common,
	.power_down_mdm_cb = mdm_power_down_common,
	.debug_state_changed_cb = debug_state_changed,
	.status_cb = mdm_status_changed,
};

static int __init mdm_modem_probe(struct platform_device *pdev)
{
	return mdm_common_create(pdev, &mdm_cb);
}

static int mdm_modem_remove(struct platform_device *pdev)
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
