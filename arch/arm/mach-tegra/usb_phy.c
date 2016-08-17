/*
 * arch/arm/mach-tegra/usb_phy.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author:
 *	Erik Gilling <konkers@google.com>
 *	Benoit Goby <benoit@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/resource.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/usb/otg.h>

#include <mach/iomap.h>
#include <mach/gpio-tegra.h>
#include <mach/hardware.h>

#include "clock.h"
#include "tegra_usb_phy.h"
#include "fuse.h"

#define ERR(stuff...)		pr_err("usb_phy: " stuff)
#define WARNING(stuff...)	pr_warning("usb_phy: " stuff)
#define INFO(stuff...)		pr_info("usb_phy: " stuff)

#define AHB_MEM_PREFETCH_CFG3		0xe0
#define AHB_MEM_PREFETCH_CFG4		0xe4
#define AHB_MEM_PREFETCH_CFG1		0xec
#define AHB_MEM_PREFETCH_CFG2		0xf0
#define PREFETCH_ENB			(1 << 31)

#ifdef DEBUG
#define DBG(stuff...)		pr_info("usb_phy: " stuff)
#else
#define DBG(stuff...)		do {} while (0)
#endif

static void print_usb_plat_data_info(struct tegra_usb_phy *phy)
{
	struct tegra_usb_platform_data *pdata = phy->pdata;
	char op_mode[][50] = {
		"TEGRA_USB_OPMODE_DEVICE",
		"TEGRA_USB_OPMODE_HOST"
	};
	char phy_intf[][50] = {
		"USB_PHY_INTF_UTMI",
		"USB_PHY_INTF_ULPI_LINK",
		"USB_PHY_INTF_ULPI_NULL",
		"USB_PHY_INTF_HSIC",
		"USB_PHY_INTF_ICUSB"
	};

	pr_info("tegra USB phy - inst[%d] platform info:\n", phy->inst);
	pr_info("port_otg: %s\n", pdata->port_otg ? "yes" : "no");
	pr_info("has_hostpc: %s\n", pdata->has_hostpc ? "yes" : "no");
	pr_info("phy_interface: %s\n", phy_intf[pdata->phy_intf]);
	pr_info("op_mode: %s\n", op_mode[pdata->op_mode]);
	if (pdata->op_mode == TEGRA_USB_OPMODE_DEVICE) {
		pr_info("vbus_pmu_irq: %d\n", pdata->u_data.dev.vbus_pmu_irq);
		pr_info("vbus_gpio: %d\n", pdata->u_data.dev.vbus_gpio);
		pr_info("charging: %s\n", pdata->u_data.dev.charging_supported ?
				"enabled" : "disabled");
		pr_info("remote_wakeup: %s\n", pdata->u_data.dev.remote_wakeup_supported
				? "enabled" : "disabled");
	} else {
		pr_info("vbus_gpio: %d\n", pdata->u_data.host.vbus_gpio);
		pr_info("hot_plug: %s\n", pdata->u_data.host.hot_plug ?
				"enabled" : "disabled");
		pr_info("remote_wakeup: %s\n", pdata->u_data.host.remote_wakeup_supported
				? "enabled" : "disabled");
	}
}

struct tegra_usb_phy *get_tegra_phy(struct usb_phy *x)
{
	return (struct tegra_usb_phy *)x;
}

static void usb_host_vbus_enable(struct tegra_usb_phy *phy, bool enable)
{

	/* OTG driver will take care for OTG port */
	if (phy->pdata->port_otg)
		return;

	if (phy->vbus_reg) {
		if (enable)
			regulator_enable(phy->vbus_reg);
		else
			regulator_disable(phy->vbus_reg);
	} else {
		int gpio = phy->pdata->u_data.host.vbus_gpio;
		if (gpio == -1)
			return;
		gpio_set_value_cansleep(gpio, enable ? 1 : 0);
	}
}

void tegra_usb_enable_vbus(struct tegra_usb_phy *phy, bool enable)
{
	usb_host_vbus_enable(phy, enable);
}
EXPORT_SYMBOL_GPL(tegra_usb_enable_vbus);

int usb_phy_reg_status_wait(void __iomem *reg, u32 mask,
					u32 result, u32 timeout)
{
	do {
		if ((readl(reg) & mask) == result)
			return 0;
		udelay(1);
		timeout--;
	} while (timeout);

	return -1;
}

static int tegra_usb_phy_init_ops(struct tegra_usb_phy *phy)
{
	int err = 0;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	if (phy->pdata->has_hostpc)
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
		err = tegra11x_usb_phy_init_ops(phy);
#else
		err = tegra3_usb_phy_init_ops(phy);
#endif
	else
		err = tegra2_usb_phy_init_ops(phy);

	return err;
}

static irqreturn_t usb_phy_dev_vbus_pmu_irq_thr(int irq, void *pdata)
{
	struct tegra_usb_phy *phy = pdata;

	if (phy->vdd_reg && !phy->vdd_reg_on) {
		regulator_enable(phy->vdd_reg);
		phy->vdd_reg_on = true;
		/*
		 * Optimal time to get the regulator turned on
		 * before detecting vbus interrupt.
		 */
		mdelay(15);
	}

	/* clk is disabled during phy power off and not here*/
	if (!phy->ctrl_clk_on) {
		tegra_clk_prepare_enable(phy->ctrlr_clk);
		phy->ctrl_clk_on = true;
	}

	return IRQ_HANDLED;
}

static void tegra_usb_phy_release_clocks(struct tegra_usb_phy *phy)
{
	clk_put(phy->emc_clk);
	clk_put(phy->sys_clk);
	if (phy->pdata->op_mode == TEGRA_USB_OPMODE_HOST)
		if (phy->pdata->u_data.host.hot_plug ||
			phy->pdata->u_data.host.remote_wakeup_supported)
			tegra_clk_disable_unprepare(phy->ctrlr_clk);
	clk_put(phy->ctrlr_clk);
	tegra_clk_disable_unprepare(phy->pllu_clk);
	clk_put(phy->pllu_clk);
}

static int tegra_usb_phy_get_clocks(struct tegra_usb_phy *phy)
{
	int err = 0;

	phy->pllu_reg = regulator_get(&phy->pdev->dev, "avdd_usb_pll");
	if (IS_ERR_OR_NULL(phy->pllu_reg)) {
		ERR("Couldn't get regulator avdd_usb_pll: %ld\n",
			PTR_ERR(phy->pllu_reg));
		err = PTR_ERR(phy->pllu_reg);
		phy->pllu_reg = NULL;
		goto fail_pllu_reg;
	}
	regulator_enable(phy->pllu_reg);

	phy->pllu_clk = clk_get_sys(NULL, "pll_u");
	if (IS_ERR(phy->pllu_clk)) {
		ERR("inst:[%d] Can't get pllu_clk clock\n", phy->inst);
		err = PTR_ERR(phy->pllu_clk);
		goto fail_pll;
	}
	tegra_clk_prepare_enable(phy->pllu_clk);

	phy->ctrlr_clk = clk_get(&phy->pdev->dev, NULL);
	if (IS_ERR(phy->ctrlr_clk)) {
		dev_err(&phy->pdev->dev, "Can't get controller clock\n");
		err = PTR_ERR(phy->ctrlr_clk);
		goto fail_ctrlr_clk;
	}

	if (phy->pdata->op_mode == TEGRA_USB_OPMODE_HOST)
		if (phy->pdata->u_data.host.hot_plug ||
			phy->pdata->u_data.host.remote_wakeup_supported)
			tegra_clk_prepare_enable(phy->ctrlr_clk);

	phy->sys_clk = clk_get(&phy->pdev->dev, "sclk");
	if (IS_ERR(phy->sys_clk)) {
		dev_err(&phy->pdev->dev, "Can't get sclk clock\n");
		err = PTR_ERR(phy->sys_clk);
		goto fail_sclk;
	}
	clk_set_rate(phy->sys_clk, 80000000);

	phy->emc_clk = clk_get(&phy->pdev->dev, "emc");
	if (IS_ERR(phy->emc_clk)) {
		dev_err(&phy->pdev->dev, "Can't get emc clock\n");
		err = PTR_ERR(phy->emc_clk);
		goto fail_emc;
	}

	if(phy->pdata->has_hostpc)
		clk_set_rate(phy->emc_clk, 100000000);
	else
		clk_set_rate(phy->emc_clk, 300000000);

	return err;

fail_emc:
	clk_put(phy->sys_clk);

fail_sclk:
	clk_put(phy->ctrlr_clk);

fail_ctrlr_clk:
	tegra_clk_disable_unprepare(phy->pllu_clk);
	clk_put(phy->pllu_clk);

fail_pll:
	regulator_disable(phy->pllu_reg);
	regulator_put(phy->pllu_reg);

fail_pllu_reg:
	return err;
}

void tegra_usb_phy_close(struct usb_phy *x)
{
	struct tegra_usb_phy *phy = get_tegra_phy(x);

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	if (phy->ops && phy->ops->close)
		phy->ops->close(phy);

	if (phy->pdata->ops && phy->pdata->ops->close)
		phy->pdata->ops->close();

	if (phy->pdata->op_mode == TEGRA_USB_OPMODE_DEVICE) {
		if (phy->pdata->u_data.dev.vbus_pmu_irq)
			free_irq(phy->pdata->u_data.dev.vbus_pmu_irq, phy);
		else
			tegra_clk_disable_unprepare(phy->ctrlr_clk);
	} else {
		usb_host_vbus_enable(phy, false);

		if (phy->vbus_reg)
			regulator_put(phy->vbus_reg);
		else {
			int gpio = phy->pdata->u_data.host.vbus_gpio;
			if (gpio != -1) {
				gpio_set_value_cansleep(gpio, 0);
				gpio_free(gpio);
			}
		}
	}

	if (phy->vdd_reg) {
		if (phy->vdd_reg_on)
			regulator_disable(phy->vdd_reg);
		regulator_put(phy->vdd_reg);
	}

	tegra_usb_phy_release_clocks(phy);

	if (phy->pllu_reg) {
		regulator_disable(phy->pllu_reg);
		regulator_put(phy->pllu_reg);
	}

	devm_kfree(&phy->pdev->dev, phy->pdata);
	devm_kfree(&phy->pdev->dev, phy);
}

irqreturn_t tegra_usb_phy_irq(struct tegra_usb_phy *phy)
{
	irqreturn_t status = IRQ_HANDLED;

	if (phy->ops && phy->ops->irq)
		status = phy->ops->irq(phy);

	return status;
}
EXPORT_SYMBOL_GPL(tegra_usb_phy_irq);

int tegra_usb_phy_init(struct usb_phy *x)
{
	int status = 0;
	struct tegra_usb_phy *phy = get_tegra_phy(x);

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	if (phy->pdata->ops && phy->pdata->ops->init)
		phy->pdata->ops->init();

	if (phy->ops && phy->ops->init)
		status = phy->ops->init(phy);

	return status;
}

int tegra_usb_phy_power_off(struct tegra_usb_phy *phy)
{
	int err = 0;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	if (!phy->phy_power_on)
		return err;

	if (phy->ops && phy->ops->power_off) {
		if (phy->pdata->ops && phy->pdata->ops->pre_phy_off)
			phy->pdata->ops->pre_phy_off();
		err = phy->ops->power_off(phy);
		if (phy->pdata->ops && phy->pdata->ops->post_phy_off)
			phy->pdata->ops->post_phy_off();
	}

	tegra_clk_disable_unprepare(phy->emc_clk);
	tegra_clk_disable_unprepare(phy->sys_clk);
	if (phy->pdata->op_mode == TEGRA_USB_OPMODE_HOST) {
		if (!phy->pdata->u_data.host.hot_plug &&
			!phy->pdata->u_data.host.remote_wakeup_supported) {
			tegra_clk_disable_unprepare(phy->ctrlr_clk);
			phy->ctrl_clk_on = false;
			if (phy->vdd_reg && phy->vdd_reg_on) {
				regulator_disable(phy->vdd_reg);
				phy->vdd_reg_on = false;
			}
		}
	} else {
		/* In device mode clock regulator/clocks will be turned off
		 * only if pmu interrupt is present on the board and host mode
		 * support through OTG is supported on the board.
		 */
		if (phy->pdata->u_data.dev.vbus_pmu_irq &&
			phy->pdata->id_det_type == TEGRA_USB_VIRTUAL_ID) {
			tegra_clk_disable_unprepare(phy->ctrlr_clk);
			phy->ctrl_clk_on = false;
			if (phy->vdd_reg && phy->vdd_reg_on) {
				regulator_disable(phy->vdd_reg);
				phy->vdd_reg_on = false;
			}
		}
	}

	phy->phy_power_on = false;

	return err;
}
EXPORT_SYMBOL_GPL(tegra_usb_phy_power_off);

int tegra_usb_phy_power_on(struct tegra_usb_phy *phy)
{
	int status = 0;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	if (phy->phy_power_on)
		return status;

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	if (phy->vdd_reg && !phy->vdd_reg_on) {
		regulator_enable(phy->vdd_reg);
		phy->vdd_reg_on = true;
	}
#endif

	/* In device mode clock is turned on by pmu irq handler
	 * if pmu irq is not available clocks will not be turned off/on
	 */
	if (phy->pdata->op_mode == TEGRA_USB_OPMODE_HOST) {
		if (!phy->pdata->u_data.host.hot_plug &&
			!phy->pdata->u_data.host.remote_wakeup_supported)
			tegra_clk_prepare_enable(phy->ctrlr_clk);
	} else {
		if (phy->pdata->u_data.dev.vbus_pmu_irq &&
			!phy->ctrl_clk_on) {
			tegra_clk_prepare_enable(phy->ctrlr_clk);
			phy->ctrl_clk_on = true;
		}
	}
	tegra_clk_prepare_enable(phy->sys_clk);
	tegra_clk_prepare_enable(phy->emc_clk);

	if (phy->ops && phy->ops->power_on) {
		if (phy->pdata->ops && phy->pdata->ops->pre_phy_on)
			phy->pdata->ops->pre_phy_on();
		status = phy->ops->power_on(phy);
		if (phy->pdata->ops && phy->pdata->ops->post_phy_on)
			phy->pdata->ops->post_phy_on();
	}

	phy->phy_power_on = true;

	return status;
}
EXPORT_SYMBOL_GPL(tegra_usb_phy_power_on);

int tegra_usb_phy_reset(struct tegra_usb_phy *phy)
{
	int status = 0;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	if (phy->ops && phy->ops->reset)
		status = phy->ops->reset(phy);

	return status;
}
EXPORT_SYMBOL_GPL(tegra_usb_phy_reset);

int tegra_usb_phy_pre_suspend(struct tegra_usb_phy *phy)
{
	int status = 0;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	if (phy->pdata->ops && phy->pdata->ops->pre_suspend)
		phy->pdata->ops->pre_suspend();

	if (phy->ops && phy->ops->pre_suspend)
		status = phy->ops->pre_suspend(phy);

	return status;
}
EXPORT_SYMBOL_GPL(tegra_usb_phy_pre_suspend);

int tegra_usb_phy_suspend(struct tegra_usb_phy *phy)
{
	int err = 0;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	if (phy->ops && phy->ops->suspend)
		err = phy->ops->suspend(phy);

	if (!err && phy->pdata->u_data.host.power_off_on_suspend)
		tegra_usb_phy_power_off(phy);

	return err;
}

int tegra_usb_phy_post_suspend(struct tegra_usb_phy *phy)
{
	int status = 0;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	if (phy->ops && phy->ops->post_suspend)
		status = phy->ops->post_suspend(phy);

	if (phy->pdata->ops && phy->pdata->ops->post_suspend)
		phy->pdata->ops->post_suspend();

	return status;
}
EXPORT_SYMBOL_GPL(tegra_usb_phy_post_suspend);

int tegra_usb_phy_pre_resume(struct tegra_usb_phy *phy, bool remote_wakeup)
{
	int status = 0;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	if (phy->pdata->ops && phy->pdata->ops->pre_resume)
		phy->pdata->ops->pre_resume();

	if (phy->ops && phy->ops->pre_resume)
		status = phy->ops->pre_resume(phy, remote_wakeup);

	return status;
}
EXPORT_SYMBOL_GPL(tegra_usb_phy_pre_resume);

int tegra_usb_phy_resume(struct tegra_usb_phy *phy)
{
	int err = 0;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	if (phy->pdata->u_data.host.power_off_on_suspend) {
		err = tegra_usb_phy_power_on(phy);
	}

	if (!err && phy->ops && phy->ops->resume)
		err = phy->ops->resume(phy);

	return err;

}

int tegra_usb_phy_post_resume(struct tegra_usb_phy *phy)
{
	int status = 0;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	if (phy->ops && phy->ops->post_resume)
		status = phy->ops->post_resume(phy);

	if (phy->pdata->ops && phy->pdata->ops->post_resume)
		phy->pdata->ops->post_resume();

	return status;
}
EXPORT_SYMBOL_GPL(tegra_usb_phy_post_resume);

int tegra_usb_phy_port_power(struct tegra_usb_phy *phy)
{
	int status = 0;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	if (phy->ops && phy->ops->port_power)
		status = phy->ops->port_power(phy);

	return status;
}
EXPORT_SYMBOL_GPL(tegra_usb_phy_port_power);

int tegra_usb_phy_bus_reset(struct tegra_usb_phy *phy)
{
	int status = 0;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	if (phy->ops && phy->ops->bus_reset)
		status = phy->ops->bus_reset(phy);

	return status;
}
EXPORT_SYMBOL_GPL(tegra_usb_phy_bus_reset);

bool tegra_usb_phy_charger_detected(struct tegra_usb_phy *phy)
{
	bool status = 0;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	if (phy->ops && phy->ops->charger_detect)
		status = phy->ops->charger_detect(phy);

	return status;
}

bool tegra_usb_phy_nv_charger_detected(struct tegra_usb_phy *phy)
{
	bool status = 0;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	if (phy->ops && phy->ops->nv_charger_detect)
		status = phy->ops->nv_charger_detect(phy);

	return status;
}

bool tegra_usb_phy_hw_accessible(struct tegra_usb_phy *phy)
{
	if (!phy->hw_accessible)
		DBG("%s(%d) inst:[%d] Not Accessible\n", __func__,
						__LINE__, phy->inst);

	return phy->hw_accessible;
}
EXPORT_SYMBOL_GPL(tegra_usb_phy_hw_accessible);

bool tegra_usb_phy_pmc_wakeup(struct tegra_usb_phy *phy)
{
	return phy->pmc_remote_wakeup || phy->pmc_hotplug_wakeup;
}
EXPORT_SYMBOL_GPL(tegra_usb_phy_pmc_wakeup);

void tegra_usb_phy_memory_prefetch_on(struct tegra_usb_phy *phy)
{
	void __iomem *ahb_gizmo = IO_ADDRESS(TEGRA_AHB_GIZMO_BASE);
	unsigned long val;

	if (phy->inst == 0 && phy->pdata->op_mode == TEGRA_USB_OPMODE_DEVICE) {
		val = readl(ahb_gizmo + AHB_MEM_PREFETCH_CFG1);
		val |= PREFETCH_ENB;
		writel(val, ahb_gizmo + AHB_MEM_PREFETCH_CFG1);
		val = readl(ahb_gizmo + AHB_MEM_PREFETCH_CFG2);
		val |= PREFETCH_ENB;
		writel(val, ahb_gizmo + AHB_MEM_PREFETCH_CFG2);
	}
}

void tegra_usb_phy_memory_prefetch_off(struct tegra_usb_phy *phy)
{
	void __iomem *ahb_gizmo = IO_ADDRESS(TEGRA_AHB_GIZMO_BASE);
	unsigned long val;

	if (phy->inst == 0 && phy->pdata->op_mode == TEGRA_USB_OPMODE_DEVICE) {
		val = readl(ahb_gizmo + AHB_MEM_PREFETCH_CFG1);
		val &= ~(PREFETCH_ENB);
		writel(val, ahb_gizmo + AHB_MEM_PREFETCH_CFG1);
		val = readl(ahb_gizmo + AHB_MEM_PREFETCH_CFG2);
		val &= ~(PREFETCH_ENB);
		writel(val, ahb_gizmo + AHB_MEM_PREFETCH_CFG2);
	}
}


int tegra_usb_phy_set_suspend(struct usb_phy *x, int suspend)
{
	struct tegra_usb_phy *phy = get_tegra_phy(x);

	if (suspend)
		return tegra_usb_phy_suspend(phy);
	else
		return tegra_usb_phy_resume(phy);
}

struct tegra_usb_phy *tegra_usb_phy_open(struct platform_device *pdev)
{
	struct tegra_usb_phy *phy;
	struct tegra_usb_platform_data *pdata;
	struct resource *res;
	int err;
	int plat_data_size = sizeof(struct tegra_usb_platform_data);

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, pdev->id);
	pdata = dev_get_platdata(&pdev->dev);
	if (!pdata) {
		dev_err(&pdev->dev, "inst:[%d] Platform data missing\n",
								pdev->id);
		err = -EINVAL;
		goto fail_inval;
	}

	phy = devm_kzalloc(&pdev->dev, sizeof(struct tegra_usb_phy), GFP_KERNEL);
	if (!phy) {
		ERR("inst:[%d] malloc usb phy failed\n", pdev->id);
		err = -ENOMEM;
		goto fail_nomem;
	}

	phy->pdata = devm_kzalloc(&pdev->dev, plat_data_size, GFP_KERNEL);
	if (!phy->pdata) {
		ERR("inst:[%d] malloc usb phy pdata failed\n", pdev->id);
		devm_kfree(&pdev->dev, phy);
		err = -ENOMEM;
		goto fail_nomem;
	}

	memcpy(phy->pdata, pdata, plat_data_size);

	phy->pdev = pdev;
	phy->inst = pdev->id;

	if (phy->pdata->op_mode == TEGRA_USB_OPMODE_HOST)
		phy->hot_plug = phy->pdata->u_data.host.hot_plug;

	print_usb_plat_data_info(phy);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ERR("inst:[%d] failed to get I/O memory\n", phy->inst);
		err = -ENXIO;
		goto fail_io;
	}

	phy->regs = ioremap(res->start, resource_size(res));
	if (!phy->regs) {
		ERR("inst:[%d] Failed to remap I/O memory\n", phy->inst);
		err = -ENOMEM;
		goto fail_io;
	}

	phy->vdd_reg = regulator_get(&pdev->dev, "avdd_usb");
	if (IS_ERR_OR_NULL(phy->vdd_reg)) {
		ERR("inst:[%d] couldn't get regulator avdd_usb: %ld\n",
			phy->inst, PTR_ERR(phy->vdd_reg));
		phy->vdd_reg = NULL;
	}

	err = tegra_usb_phy_get_clocks(phy);
	if (err) {
		ERR("inst:[%d] Failed to init clocks\n", phy->inst);
		goto fail_clk;
	}

	if (phy->pdata->op_mode == TEGRA_USB_OPMODE_DEVICE) {
		if (phy->pdata->u_data.dev.vbus_pmu_irq) {
			err = request_threaded_irq(
					phy->pdata->u_data.dev.vbus_pmu_irq,
					NULL, usb_phy_dev_vbus_pmu_irq_thr,
					IRQF_SHARED, "usb_pmu_vbus_irq", phy);
			if (err) {
				ERR("inst:[%d] Failed to register IRQ\n",
								phy->inst);
				goto fail_init;
			}
		} else {
			tegra_clk_prepare_enable(phy->ctrlr_clk);
		}
	} else {
		int gpio = phy->pdata->u_data.host.vbus_gpio;
		if (gpio != -1) {
			if (gpio_request(gpio, "usb_host_vbus") < 0) {
				ERR("inst:[%d] host vbus gpio req failed\n",
								phy->inst);
				goto fail_init;
			}
			if (gpio_direction_output(gpio, 1) < 0) {
				ERR("inst:[%d] host vbus gpio dir failed\n",
								phy->inst);
				goto fail_init;
			}
		} else {
			phy->vbus_reg = regulator_get(&pdev->dev, "usb_vbus");
			if (IS_ERR_OR_NULL(phy->vbus_reg)) {
				ERR("failed to get regulator vdd_vbus_usb:" \
				"%ld,instance : %d\n", PTR_ERR(phy->vbus_reg),
				phy->inst);
				phy->vbus_reg = NULL;
			}
		}
		usb_host_vbus_enable(phy, true);
		/* Fixme: Need delay to stablize the vbus on USB1
		   this must be fixed properly */
		if (phy->inst == 0)
			msleep(1000);
	}
	err = tegra_usb_phy_init_ops(phy);
	if (err) {
		ERR("inst:[%d] Failed to init ops\n", phy->inst);
		goto fail_init;
	}

	if (phy->pdata->ops && phy->pdata->ops->open)
		phy->pdata->ops->open();

	if (phy->ops && phy->ops->open) {
		err = phy->ops->open(phy);
		if (err) {
			ERR("inst:[%d] Failed to open hw ops\n", phy->inst);
			goto fail_init;
		}
	}

	/* Set usb_phy func pointers */
	phy->phy.init = tegra_usb_phy_init;
	phy->phy.shutdown = tegra_usb_phy_close;
	phy->phy.set_suspend = tegra_usb_phy_set_suspend;

	return phy;

fail_init:
	tegra_usb_phy_release_clocks(phy);

	if (phy->pdata->op_mode == TEGRA_USB_OPMODE_DEVICE) {
		if (phy->pdata->u_data.dev.vbus_pmu_irq)
			free_irq(phy->pdata->u_data.dev.vbus_pmu_irq, phy);
	} else {
		usb_host_vbus_enable(phy, false);

		if (phy->vbus_reg)
			regulator_put(phy->vbus_reg);
		else {
			int gpio = phy->pdata->u_data.host.vbus_gpio;
			if (gpio != -1) {
				gpio_set_value_cansleep(gpio, 0);
				gpio_free(gpio);
			}
		}
	}

fail_clk:
	regulator_put(phy->vdd_reg);
	iounmap(phy->regs);
fail_io:
	devm_kfree(&pdev->dev, phy->pdata);
	devm_kfree(&pdev->dev, phy);

fail_nomem:
fail_inval:
	return ERR_PTR(err);
}
EXPORT_SYMBOL_GPL(tegra_usb_phy_open);

void tegra_usb_phy_pmc_disable(struct tegra_usb_phy *phy)
{
	if (phy->ops && phy->ops->pmc_disable)
		phy->ops->pmc_disable(phy);
}
EXPORT_SYMBOL_GPL(tegra_usb_phy_pmc_disable);
