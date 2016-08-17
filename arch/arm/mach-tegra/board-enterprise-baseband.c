/*
 * arch/arm/mach-tegra/board-enterprise-baseband.c
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/err.h>
#include <linux/platform_data/tegra_usb.h>

#include <mach/pinmux-tegra30.h>
#include <linux/platform_data/tegra_usb_modem_power.h>
#include <mach/gpio-tegra.h>

#include "devices.h"
#include "gpio-names.h"

/* Tegra3 BB GPIO */
#define MODEM_PWR_ON    TEGRA_GPIO_PE0
#define MODEM_RESET     TEGRA_GPIO_PE1
#define BB_RST_OUT      TEGRA_GPIO_PV1

/* Icera modem handshaking GPIO */
#define AP2MDM_ACK      TEGRA_GPIO_PE3
#define MDM2AP_ACK      TEGRA_GPIO_PU5
#define AP2MDM_ACK2     TEGRA_GPIO_PE2
#define MDM2AP_ACK2     TEGRA_GPIO_PV0

static struct gpio modem_gpios[] = {
	{MODEM_PWR_ON, GPIOF_OUT_INIT_LOW, "MODEM PWR ON"},
	{MODEM_RESET, GPIOF_IN, "MODEM RESET"},
	{AP2MDM_ACK2, GPIOF_OUT_INIT_HIGH, "AP2MDM ACK2"},
	{AP2MDM_ACK, GPIOF_OUT_INIT_LOW, "AP2MDM ACK"},
};

static void baseband_post_phy_on(void);
static void baseband_pre_phy_off(void);

static struct tegra_usb_phy_platform_ops ulpi_null_plat_ops = {
	.pre_phy_off = baseband_pre_phy_off,
	.post_phy_on = baseband_post_phy_on,
};

static struct tegra_usb_platform_data tegra_ehci2_ulpi_null_pdata = {
	.port_otg = false,
	.has_hostpc = true,
	.phy_intf = TEGRA_USB_PHY_INTF_ULPI_NULL,
	.op_mode	= TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = -1,
		.hot_plug = true,
		.remote_wakeup_supported = false,
		.power_off_on_suspend = true,
	},
	.u_cfg.ulpi = {
		.shadow_clk_delay = 10,
		.clock_out_delay = 1,
		.data_trimmer = 1,
		.stpdirnxt_trimmer = 1,
		.dir_trimmer = 1,
		.clk = NULL,
		.phy_restore_gpio = MDM2AP_ACK,
	},
	.ops = &ulpi_null_plat_ops,
};

static void baseband_post_phy_on(void)
{
	/* set AP2MDM_ACK2 low */
	gpio_set_value(AP2MDM_ACK2, 0);
}

static void baseband_pre_phy_off(void)
{
	/* set AP2MDM_ACK2 high */
	gpio_set_value(AP2MDM_ACK2, 1);
}

static void baseband_start(void)
{
	/*
	 *  Leave baseband powered OFF.
	 *  User-space daemons will take care of powering it up.
	 */
	pr_info("%s\n", __func__);
	gpio_set_value(MODEM_PWR_ON, 0);
}

static void baseband_reset(void)
{
	/* Initiate power cycle on baseband sub system */
	pr_info("%s\n", __func__);
	gpio_set_value(MODEM_PWR_ON, 0);
	mdelay(200);
	gpio_set_value(MODEM_PWR_ON, 1);
}

static void baseband_stop(void)
{
	/* Baseband power off */
	pr_info("%s\n", __func__);
	gpio_set_value(MODEM_PWR_ON, 0);
	mdelay(1);
}

static int baseband_init(void)
{
	int ret;

	ret = gpio_request_array(modem_gpios, ARRAY_SIZE(modem_gpios));
	if (ret)
		return ret;

	/* enable pull-up for ULPI STP */
	tegra_pinmux_set_pullupdown(TEGRA_PINGROUP_ULPI_STP,
				    TEGRA_PUPD_PULL_UP);

	/* enable pull-up for MDM2AP_ACK2 */
	tegra_pinmux_set_pullupdown(TEGRA_PINGROUP_GPIO_PV0,
				    TEGRA_PUPD_PULL_UP);

	/* export GPIO for user space access through sysfs */
	gpio_export(MODEM_PWR_ON, false);

	return 0;
}

static const struct tegra_modem_operations baseband_operations = {
	.init = baseband_init,
	.start = baseband_start,
	.reset = baseband_reset,
	.stop = baseband_stop,
};

static struct tegra_usb_modem_power_platform_data baseband_pdata = {
	.ops = &baseband_operations,
	.wake_gpio = MDM2AP_ACK2,
	.wake_irq_flags = IRQF_TRIGGER_FALLING,
	.boot_gpio = BB_RST_OUT,
	.boot_irq_flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	.autosuspend_delay = 2000,
	.short_autosuspend_delay = 50,
	.tegra_ehci_device = &tegra_ehci2_device,
	.tegra_ehci_pdata = &tegra_ehci2_ulpi_null_pdata,
};

static struct platform_device icera_baseband_device = {
	.name = "tegra_usb_modem_power",
	.id = -1,
	.dev = {
		.platform_data = &baseband_pdata,
	},
};

int __init enterprise_modem_init(void)
{
	platform_device_register(&icera_baseband_device);
	return 0;
}
