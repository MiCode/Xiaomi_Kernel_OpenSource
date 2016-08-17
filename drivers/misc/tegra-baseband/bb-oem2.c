/*
 * drivers/misc/tegra-baseband/bb-oem2.c
 *
 * Copyright (c) 2012, NVIDIA Corporation.
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
#include <linux/platform_data/tegra_usb.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/tegra-bb-power.h>
#include <mach/usb_phy.h>

#include "bb-power.h"

static struct tegra_bb_gpio_data m7400_gpios[] = {
	{ { GPIO_INVALID, GPIOF_OUT_INIT_LOW, "MDM_PWR_ON" }, true },
	{ { GPIO_INVALID, GPIOF_IN, "MDM_PWRSTATUS" }, true },
	{ { GPIO_INVALID, GPIOF_OUT_INIT_HIGH, "MDM_SERVICE" }, true },
	{ { GPIO_INVALID, GPIOF_OUT_INIT_LOW, "MDM_USB_AWR" }, false },
	{ { GPIO_INVALID, GPIOF_IN, "MDM_USB_CWR" }, false },
	{ { GPIO_INVALID, GPIOF_IN, "MDM_RESOUT2" }, true },
	{ { GPIO_INVALID, GPIOF_OUT_INIT_LOW, "MDM_USB_ARR" }, false },
	{ { GPIO_INVALID, 0, NULL }, false },	/* End of table */
};
static bool ehci_registered;
static int modem_status;
static int gpio_awr;
static int gpio_cwr;
static int gpio_arr;
static struct usb_device *m7400_usb_device;

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

static int m7400_enum_handshake(void)
{
	int retval = 0;

	/* Wait for CP to indicate ready - by driving CWR high. */
	if (gpio_wait_timeout(gpio_cwr, 1, 10) != 0) {
			pr_info("%s: Error: timeout waiting for modem resume.\n",
							__func__);
			retval = -1;
	}

	/* Signal AP ready - Drive AWR and ARR high. */
	gpio_set_value(gpio_awr, 1);
	gpio_set_value(gpio_arr, 1);

	return retval;
}

static int m7400_apup_handshake(bool checkresponse)
{
	int retval = 0;

	/* Signal AP ready - Drive AWR and ARR high. */
	gpio_set_value(gpio_awr, 1);
	gpio_set_value(gpio_arr, 1);

	if (checkresponse) {
		/* Wait for CP ack - by driving CWR high. */
		if (gpio_wait_timeout(gpio_cwr, 1, 10) != 0) {
			pr_info("%s: Error: timeout waiting for modem ack.\n",
							__func__);
			retval = -1;
		}
	}
	return retval;
}

static void m7400_apdown_handshake(void)
{
	/* Signal AP going down to modem - Drive AWR low. */
	/* No need to wait for a CP response */
	gpio_set_value(gpio_awr, 0);
}

static void m7400_l2_suspend(void)
{
	/* Gets called for two cases :
		a) Port suspend.
		b) Bus suspend. */
	if (modem_status == BBSTATE_L2)
		return;

	/* Post bus suspend: Drive ARR low. */
	gpio_set_value(gpio_arr, 0);
	modem_status = BBSTATE_L2;
}

static void m7400_l2_resume(void)
{
	/* Gets called for two cases :
		a) L2 resume.
		b) bus resume phase of L3 resume. */
	if (modem_status == BBSTATE_L0)
		return;

	/* Pre bus resume: Drive ARR high. */
	gpio_set_value(gpio_arr, 1);

	/* If host initiated resume - Wait for CP ack (CWR goes high). */
	/* If device initiated resume - CWR will be already high. */
	if (gpio_wait_timeout(gpio_cwr, 1, 10) != 0) {
		pr_info("%s: Error: timeout waiting for modem ack.\n",
						__func__);
		return;
	}
	modem_status = BBSTATE_L0;
}

static void m7400_l3_suspend(void)
{
	m7400_apdown_handshake();
	modem_status = BBSTATE_L3;
}

static void m7400_l3_resume(void)
{
	m7400_apup_handshake(true);
	modem_status = BBSTATE_L0;
}

static irqreturn_t m7400_wake_irq(int irq, void *dev_id)
{
	struct usb_interface *intf;

	switch (modem_status) {
	case BBSTATE_L2:
		/* Resume usb host activity. */
		if (m7400_usb_device) {
			usb_lock_device(m7400_usb_device);
			intf = usb_ifnum_to_if(m7400_usb_device, 0);
			usb_autopm_get_interface(intf);
			usb_autopm_put_interface(intf);
			usb_unlock_device(m7400_usb_device);
		}
		break;
	default:
		break;
	}

	return IRQ_HANDLED;
}

static int m7400_power(int code)
{
	switch (code) {
	case PWRSTATE_L2L3:
	m7400_l3_suspend();
	break;
	case PWRSTATE_L3L0:
	m7400_l3_resume();
	break;
	default:
	break;
	}
	return 0;
}

static void m7400_ehci_customize(struct platform_device *pdev)
{
	struct tegra_usb_platform_data *ehci_pdata;

	ehci_pdata = (struct tegra_usb_platform_data *)
			pdev->dev.platform_data;

	/* Register PHY callbacks */
	ehci_pdata->ops->post_suspend = m7400_l2_suspend;
	ehci_pdata->ops->pre_resume = m7400_l2_resume;

	/* Override required settings */
	ehci_pdata->u_data.host.power_off_on_suspend = false;
}

static int m7400_attrib_write(struct device *dev, int value)
{
	struct tegra_bb_pdata *pdata;
	static struct platform_device *ehci_device;
	static bool first_enum = true;

	if (value > 1 || (!ehci_registered && !value)) {
		/* Supported values are 0/1. */
		return -1;
	}

	pdata = (struct tegra_bb_pdata *) dev->platform_data;
	if (value) {

		/* Check readiness for enumeration */
		if (first_enum)
			first_enum = false;
		else
			m7400_enum_handshake();

		/* Register ehci controller */
		ehci_device = pdata->ehci_register();
		if (ehci_device == NULL) {
			pr_info("%s - Error: ehci register failed.\n",
							 __func__);
			return -1;
		}

		/* Customize PHY setup/callbacks */
		m7400_ehci_customize(ehci_device);

		ehci_registered = true;
	} else {
		/* Unregister ehci controller */
		if (ehci_device != NULL)
			pdata->ehci_unregister(&ehci_device);

		/* Signal AP going down */
		m7400_apdown_handshake();
		ehci_registered = false;
	}

	return 0;
}

static int m7400_registered(struct usb_device *udev)
{
	m7400_usb_device = udev;
	modem_status = BBSTATE_L0;
	return 0;
}

static struct tegra_bb_gpio_irqdata m7400_gpioirqs[] = {
	{ GPIO_INVALID, "tegra_bb_wake", m7400_wake_irq,
				IRQF_TRIGGER_RISING, true, NULL },
	{ GPIO_INVALID, NULL, NULL, 0, NULL },	/* End of table */
};

static struct tegra_bb_power_gdata m7400_gdata = {
	.gpio = m7400_gpios,
	.gpioirq = m7400_gpioirqs,
};

static struct tegra_bb_power_mdata m7400_mdata = {
	.vid = 0x04cc,
	.pid = 0x230f,
	.wake_capable = true,
	.autosuspend_ready = true,
	.reg_cb = m7400_registered,
};

static struct tegra_bb_power_data m7400_data = {
	.gpio_data = &m7400_gdata,
	.modem_data = &m7400_mdata,
};

static void *m7400_init(void *pdata)
{
	struct tegra_bb_pdata *platdata = (struct tegra_bb_pdata *) pdata;
	union tegra_bb_gpio_id *id = platdata->id;

	/* Fill the gpio ids allocated by hardware */
	m7400_gpios[0].data.gpio = id->m7400.pwr_on;
	m7400_gpios[1].data.gpio = id->m7400.pwr_status;
	m7400_gpios[2].data.gpio = id->m7400.service;
	m7400_gpios[3].data.gpio = id->m7400.usb_awr;
	m7400_gpios[4].data.gpio = id->m7400.usb_cwr;
	m7400_gpios[5].data.gpio = id->m7400.resout2;
	m7400_gpios[6].data.gpio = id->m7400.uart_awr;
	m7400_gpioirqs[0].id = id->m7400.usb_cwr;

	if (!platdata->ehci_register || !platdata->ehci_unregister) {
		pr_info("%s - Error: ehci reg/unreg functions missing.\n"
							, __func__);
		return 0;
	}

	gpio_awr = m7400_gpios[3].data.gpio;
	gpio_cwr = m7400_gpios[4].data.gpio;
	gpio_arr = m7400_gpios[6].data.gpio;
	if (gpio_awr == GPIO_INVALID || gpio_cwr == GPIO_INVALID
			|| gpio_arr == GPIO_INVALID) {
		pr_info("%s - Error: Invalid gpio data.\n", __func__);
		return 0;
	}

	ehci_registered = false;
	modem_status = BBSTATE_UNKNOWN;
	return (void *) &m7400_data;
}

static void *m7400_deinit(void)
{
	return (void *) &m7400_data;
}

static struct tegra_bb_callback m7400_callbacks = {
	.init = m7400_init,
	.deinit = m7400_deinit,
	.attrib = m7400_attrib_write,
#ifdef CONFIG_PM
	.power = m7400_power,
#endif
};

void *m7400_get_cblist(void)
{
	return (void *) &m7400_callbacks;
}
