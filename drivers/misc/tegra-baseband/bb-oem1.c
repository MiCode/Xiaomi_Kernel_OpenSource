/*
 * drivers/misc/tegra-baseband/bb-oem1.c
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/resource.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/usb.h>
#include <linux/wakelock.h>
#include <linux/suspend.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/tegra_usb_phy.h>
#include <mach/tegra-bb-power.h>
#include "bb-power.h"
#include "bb-oem1.h"

#define WAKE_TIMEOUT_MS 50

static struct tegra_bb_gpio_data bb_gpios[] = {
	{ { GPIO_INVALID, GPIOF_OUT_INIT_LOW, "MDM4_RST" }, true },
	{ { GPIO_INVALID, GPIOF_OUT_INIT_LOW, "MDM4_ON" }, true },
	{ { GPIO_INVALID, GPIOF_OUT_INIT_LOW, "MDM4_USB_AWR" }, true },
	{ { GPIO_INVALID, GPIOF_IN, "MDM4_USB_CWR" }, true },
	{ { GPIO_INVALID, GPIOF_IN, "MDM4_SPARE" }, true },
	{ { GPIO_INVALID, GPIOF_IN, "MDM4_WDI" }, true },
	{ { GPIO_INVALID, 0, NULL }, false },	/* End of table */
};

static struct sdata bb_sdata;
static struct opsdata bb_opdata;
static struct locks bb_locks;
static struct regulator *hsic_reg;
static bool ehci_registered;
static int dlevel;

static int gpio_wait_timeout(int gpio, int value, int timeout_msec)
{
	int count;
	for (count = 0; count < timeout_msec; ++count) {
		if (gpio_get_value(gpio) == value)
			return 0;
		mdelay(1);
	}
	return -1;
}

static void bb_setdata(struct opsdata *data)
{
	unsigned long flags;

	spin_lock_irqsave(&bb_locks.lock, flags);
	if (data) {
		if (data->powerstate != bb_opdata.powerstate)
			bb_opdata.powerstate = data->powerstate;
		if (data->usbdev != bb_opdata.usbdev)
			bb_opdata.usbdev = data->usbdev;
	}
	spin_unlock_irqrestore(&bb_locks.lock, flags);

	if (data && (data->powerstate == BBSTATE_UNKNOWN)) {
		if (!wake_lock_active(&bb_locks.wlock)) {
			wake_lock(&bb_locks.wlock);
			if (dlevel & DLEVEL_MISC)
				pr_info("%s: Taking wakelock.\n", __func__);
		} else
			pr_warn("%s: Active wakelock in UNK\n", __func__);
	} else {
		if (wake_lock_active(&bb_locks.wlock)) {
			wake_unlock(&bb_locks.wlock);
			if (dlevel & DLEVEL_MISC)
				pr_info("%s: Releasing wakelock.\n", __func__);
		}
	}
}

static void bb_getdata(struct opsdata *data)
{
	unsigned long flags;

	spin_lock_irqsave(&bb_locks.lock, flags);
	if (data) {
		data->powerstate = bb_opdata.powerstate;
		data->usbdev = bb_opdata.usbdev;
	}
	spin_unlock_irqrestore(&bb_locks.lock, flags);
}

static int bb_getpowerstate(void)
{
	struct opsdata data;

	bb_getdata(&data);
	return data.powerstate;
}

static void bb_setpowerstate(int status)
{
	struct opsdata data;

	bb_getdata(&data);
	data.powerstate = status;
	bb_setdata(&data);
}

static bool bb_crashed(void)
{
	bool flag;

	flag = ((gpio_get_value(bb_sdata.gpio_wdi)) ? false : true);
	return flag;
}

static bool bb_get_cwr(void)
{
	return gpio_get_value(bb_sdata.gpio_cwr);
}

static void bb_set_awr(int value)
{
	gpio_set_value(bb_sdata.gpio_awr, value);
}

static void bb_gpio_stat(void)
{
	pr_info("%s: AWR:%d, CWR:%d, WDI:%d.\n", __func__,
		gpio_get_value(bb_sdata.gpio_awr),
		gpio_get_value(bb_sdata.gpio_cwr),
		gpio_get_value(bb_sdata.gpio_wdi));
}

static int apup_handshake(bool checkresponse)
{
	int retval = 0;

	/* Signal AP ready - Drive USB_AWR high. */
	bb_set_awr(1);

	if (checkresponse) {
		/* Wait for CP ack - by driving USB_CWR high. */
		if (gpio_wait_timeout(bb_sdata.gpio_cwr, 1,
					WAKE_TIMEOUT_MS) != 0) {
			pr_err("%s: Error: Timeout waiting for modem ack.\n",
							__func__);
			retval = -1;
		}
	}
	return retval;
}

static void apdown_handshake(void)
{
	/* Signal AP going down to modem - Drive USB_AWR low. */
	/* No need to wait for a CP response */
	bb_set_awr(0);
}

static void pre_l2_suspend(void)
{
	bb_setpowerstate(BBSTATE_L02L2);
}

static void l2_suspend(void)
{
	int modemstate = bb_getpowerstate();

	if (dlevel & DLEVEL_PM)
		pr_info("%s.\n", __func__);

	if (modemstate != BBSTATE_L02L2) {
		pr_err("%s: Error. Unexp modemstate %x. Should be %d.\n",
					 __func__, modemstate, BBSTATE_L02L2);
		return;
	}
	bb_setpowerstate(BBSTATE_L2);
}

static void l2_resume(void)
{
	int modemstate = bb_getpowerstate();
	bool resumeok = false;

	if (dlevel & DLEVEL_PM)
		pr_info("%s.\n", __func__);

	/* Gets called for two cases :
		a) L2 resume.
		b) bus resume phase of L3 resume. */
	switch (modemstate) {
	case BBSTATE_L2:
		resumeok = true;
		break;
	case BBSTATE_L3:
		if (apup_handshake(true) == 0)
			resumeok = true;
		else {
			bb_gpio_stat();
			pr_err("%s: Error. Modem wakeup from L3 failed.\n",
								 __func__);
			/* TBD: Add code to unregister ehci. */
		}
		break;
	default:
		pr_err("%s: Error. Unexp modemstate %x. Should be %d or %d.\n",
				__func__, modemstate, BBSTATE_L2, BBSTATE_L3);
		break;
	}
	if (resumeok)
		bb_setpowerstate(BBSTATE_L0);
}

static void phy_ready(void)
{
	int modemstate = bb_getpowerstate();

	if (dlevel & DLEVEL_PM)
		pr_info("%s.\n", __func__);

	switch (modemstate) {
	case BBSTATE_UNKNOWN:
		/* Wait for CP to indicate ready - by driving USB_CWR high. */
		if (gpio_wait_timeout(bb_sdata.gpio_cwr, 1, 10) != 0) {
			pr_info("%s: Timeout 4 modem ready. Maybe 1st enum ?\n",
							 __func__);
			/* For first enumeration don't drive AWR high */
			break;
		}

		/* Signal AP ready - Drive USB_AWR high. */
		pr_info("%s : Driving AWR high.\n", __func__);
		bb_set_awr(1);
		break;
	default:
		pr_err("%s: Error. Unexp modemstate %x. Should be %d.\n",
				__func__, modemstate, BBSTATE_UNKNOWN);
		break;
	}
}

static void pre_phy_off(void)
{
	if (dlevel & DLEVEL_PM)
		pr_info("%s.\n", __func__);

	/* Signal AP going down */
	apdown_handshake();

	/* Gets called for two cases :
		a) L3 suspend.
		b) EHCI unregister. */
	if (bb_getpowerstate() == BBSTATE_L2)
		bb_setpowerstate(BBSTATE_L3);
}

static int l3_suspend(void)
{
	int pwrstate = bb_getpowerstate();
	bool wakeup_detected = bb_get_cwr();
	bool crashed = bb_crashed();

	if (dlevel & DLEVEL_PM)
		pr_info("%s.\n", __func__);

	/* If modem state during system suspend is not L3 (crashed)
		or modem is initiating a wakeup, abort system suspend. */
	if ((pwrstate != BBSTATE_L3) || wakeup_detected || crashed) {
		if (pwrstate != BBSTATE_L3)
			pr_err("%s: Unexp modemstate %x. Should be %d.\n",
				 __func__, pwrstate, BBSTATE_L3);
		if (wakeup_detected)
			pr_info("%s : CWR high.\n", __func__);
		if (crashed)
			pr_info("%s : WDI low.\n", __func__);
		pr_info("%s: Aborting suspend.\n", __func__);
		return 1;
	}
	return 0;
}

static int l3_suspend_noirq(void)
{
	bool wakeup_detected = bb_get_cwr();
	bool crashed = bb_crashed();

	/* If modem is initiating a wakeup, or it had crashed
	abort system suspend. */
	if (wakeup_detected || crashed) {
		pr_info("%s: Aborting suspend.\n", __func__);
		return 1;
	}
	return 0;
}

static int l3_resume(void)
{
	return 0;
}

static int l3_resume_noirq(void)
{
	return 0;
}

static irqreturn_t bb_wake_irq(int irq, void *dev_id)
{
	struct opsdata data;
	struct usb_interface *iface;
	int cwrlevel = bb_get_cwr();
	bool pwrstate_l2 = false;

	bb_getdata(&data);
	pwrstate_l2 = ((data.powerstate == BBSTATE_L02L2) ||
				(data.powerstate == BBSTATE_L2));
	if (cwrlevel && pwrstate_l2) {
		if (dlevel & DLEVEL_PM)
			pr_info("%s: Modem wakeup request from L2.\n",
							__func__);
		if (data.usbdev) {
			usb_lock_device(data.usbdev);
			iface = usb_ifnum_to_if(data.usbdev, 0);
			if (iface) {
				/* Resume usb host activity. */
				usb_autopm_get_interface(iface);
				usb_autopm_put_interface_no_suspend(iface);
			}
			usb_unlock_device(data.usbdev);
		}
	}

	if (!cwrlevel && data.powerstate == BBSTATE_UNKNOWN && data.usbdev) {
		data.powerstate = BBSTATE_L0;
		bb_setdata(&data);
		if (dlevel & DLEVEL_PM)
			pr_info("%s: Network interface up.\n", __func__);
	}
	return IRQ_HANDLED;
}

static int bb_power(int code)
{
	int retval = 0;

	switch (code) {
	case PWRSTATE_L2L3:
	retval = l3_suspend();
	break;
	case PWRSTATE_L2L3_NOIRQ:
	retval = l3_suspend_noirq();
	break;
	case PWRSTATE_L3L0_NOIRQ:
	retval = l3_resume_noirq();
	break;
	case PWRSTATE_L3L0:
	retval = l3_resume();
	break;
	default:
	break;
	}
	return retval;
}

static int bb_ehci_customize(struct tegra_bb_pdata *pdata)
{
	struct platform_device *pdev;
	struct tegra_usb_platform_data *usb_pdata;
	struct tegra_usb_phy_platform_ops *pops;

	if (pdata && pdata->device) {
		pdev = pdata->device;
		usb_pdata = (struct tegra_usb_platform_data *)
			pdev->dev.platform_data;
		pops = (struct tegra_usb_phy_platform_ops *)
			usb_pdata->ops;

		/* Register PHY platform callbacks */
		pops->pre_suspend = pre_l2_suspend;
		pops->post_suspend = l2_suspend;
		pops->pre_resume = l2_resume;
		pops->port_power = phy_ready;
		pops->pre_phy_off = pre_phy_off;

		/* Override required settings */
		usb_pdata->u_data.host.power_off_on_suspend = 0;

	} else {
		pr_err("%s: Error. Invalid platform data.\n", __func__);
		return 0;
	}
	return 1;
}

static int bb_sysfs_load(struct device *dev, int value)
{
	struct tegra_bb_pdata *pdata;
	static struct platform_device *ehci_device;
	struct opsdata data;

	if (dlevel & DLEVEL_SYS_CB)
		pr_info("%s: Called with value : %d\n", __func__, value);

	if (value > 1 || (!ehci_registered && !value) ||
				(ehci_registered && value)) {
		/* Supported values are 0/1. */
		pr_err("%s:  Error. Invalid data. Exiting.\n", __func__);
		return -1;
	}

	pdata = (struct tegra_bb_pdata *) dev->platform_data;
	if (value) {
		/* Register ehci controller */
		ehci_device = pdata->ehci_register(pdata->device);
		if (ehci_device == NULL) {
			pr_err("%s: Error. ehci register failed.\n",
							 __func__);
			return -1;
		}
		ehci_registered = true;
	} else {
		/* Mark usb device invalid */
		data.usbdev = NULL;
		data.powerstate = BBSTATE_UNKNOWN;
		bb_setdata(&data);
		ehci_registered = false;

		/* Unregister ehci controller */
		if (ehci_device != NULL)
			pdata->ehci_unregister(&ehci_device);

		/* Signal AP going down */
		apdown_handshake();
	}
	return 0;
}

static int bb_sysfs_dlevel(struct device *dev, int value)
{
	if (dlevel & DLEVEL_SYS_CB)
		pr_info("%s: Called with value : %d\n", __func__, value);

	dlevel = value;
	return 0;
}

static int bb_usbnotify(struct usb_device *udev, bool registered)
{
	struct opsdata data;

	data.powerstate = BBSTATE_UNKNOWN;
	if (registered) {
		data.usbdev = udev;
		if (dlevel & DLEVEL_MISC)
			pr_info("%s: Modem attached.\n", __func__);

	} else {
		data.usbdev = NULL;
		if (dlevel & DLEVEL_MISC)
			pr_info("%s: Modem detached.\n", __func__);
	}

	bb_setdata(&data);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int bb_pmnotify(unsigned long event)
{
	int pwrstate = bb_getpowerstate();
	int retval = NOTIFY_OK;

	if (dlevel & DLEVEL_PM)
		pr_info("%s: PM notification %ld.\n", __func__, event);

	switch (event) {
	case PM_SUSPEND_PREPARE:
	if (pwrstate == BBSTATE_UNKNOWN) {
		pr_err("%s: Suspend with pwrstate=%d. Aborting suspend.\n",
						 __func__, pwrstate);
		retval = NOTIFY_BAD;
	}
	break;
	case PM_POST_SUSPEND:
	break;
	default:
	retval = NOTIFY_DONE;
	break;
	}
	return retval;
}
#endif

static struct tegra_bb_gpio_irqdata bb_gpioirqs[] = {
	{ GPIO_INVALID, "tegra_bb_wake", bb_wake_irq,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, true, NULL },
	{ GPIO_INVALID, "tegra_bb_wdi", NULL,
		IRQF_TRIGGER_NONE, true, NULL },
	{ GPIO_INVALID, NULL, NULL, 0, NULL },	/* End of table */
};

static struct tegra_bb_power_gdata bb_gdata = {
	.gpio = bb_gpios,
	.gpioirq = bb_gpioirqs,
};

static struct tegra_bb_power_mdata bb_mdata = {
	.vid = 0x045B,
	.pid = 0x020F,
	.wake_capable = true,
	.autosuspend_ready = true,
};

static struct tegra_bb_power_data bb_data = {
	.gpio_data = &bb_gdata,
	.modem_data = &bb_mdata,
};


static void *bb_init(void *pdata)
{
	struct tegra_bb_pdata *platdata = (struct tegra_bb_pdata *) pdata;
	union tegra_bb_gpio_id *id = platdata->id;
	struct opsdata data;

	/* Fill the gpio ids allocated by hardware */
	bb_gpios[0].data.gpio = id->oem1.reset;
	bb_gpios[1].data.gpio = id->oem1.pwron;
	bb_gpios[2].data.gpio = id->oem1.awr;
	bb_gpios[3].data.gpio = id->oem1.cwr;
	bb_gpios[4].data.gpio = id->oem1.spare;
	bb_gpios[5].data.gpio = id->oem1.wdi;
	bb_gpioirqs[0].id = id->oem1.cwr;
	bb_gpioirqs[1].id = id->oem1.wdi;

	if (!platdata->ehci_register || !platdata->ehci_unregister) {
		pr_err("%s - Error: ehci reg/unreg functions missing.\n"
							, __func__);
		return 0;
	}

	bb_sdata.gpio_awr = bb_gpios[2].data.gpio;
	bb_sdata.gpio_cwr = bb_gpios[3].data.gpio;
	bb_sdata.gpio_wdi = bb_gpios[5].data.gpio;
	if (bb_sdata.gpio_awr == GPIO_INVALID ||
		bb_sdata.gpio_cwr == GPIO_INVALID ||
		bb_sdata.gpio_wdi == GPIO_INVALID) {
		pr_err("%s: Error. Invalid gpio data.\n", __func__);
		return 0;
	}

	/* Customize PHY setup/callbacks */
	if (!bb_ehci_customize(platdata))
		return 0;

	/* Board specific regulator init */
	if (platdata->regulator) {
		hsic_reg = regulator_get(NULL,
					(const char *)platdata->regulator);
		if (IS_ERR_OR_NULL(hsic_reg)) {
			pr_err("%s: Error. regulator_get failed.\n",
							 __func__);
			return 0;
		}

		if (regulator_enable(hsic_reg) < 0) {
			pr_err("%s: Error. regulator_enable failed.\n",
							 __func__);
			return 0;
		}
	}

	spin_lock_init(&bb_locks.lock);
	wake_lock_init(&bb_locks.wlock, WAKE_LOCK_SUSPEND,
						"tegra-bb-lock");
	dlevel = DLEVEL_INIT;
	ehci_registered = false;
	data.usbdev = NULL;
	data.powerstate = BBSTATE_UNKNOWN;
	bb_setdata(&data);

	return (void *) &bb_data;
}

static void *bb_deinit(void)
{
	/* destroy wake lock */
	wake_lock_destroy(&bb_locks.wlock);

	return (void *) &bb_data;
}

static struct tegra_bb_callback bb_callbacks = {
	.init = bb_init,
	.deinit = bb_deinit,
	.load = bb_sysfs_load,
	.dlevel = bb_sysfs_dlevel,
	.usbnotify = bb_usbnotify,
#ifdef CONFIG_PM
	.power = bb_power,
#endif
#ifdef CONFIG_PM_SLEEP
	.pmnotify = bb_pmnotify,
#endif
};

void *bb_oem1_get_cblist(void)
{
	return (void *) &bb_callbacks;
}
