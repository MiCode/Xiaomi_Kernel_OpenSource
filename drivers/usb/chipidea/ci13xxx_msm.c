/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/usb/msm_hsusb_hw.h>
#include <linux/usb/ulpi.h>
#include <linux/usb/gadget.h>
#include <linux/usb/chipidea.h>
#include <linux/gpio.h>

#include "ci.h"

#define CI13XXX_MSM_MAX_LOG2_ITC	7

#define MSM_USB_BASE	(ci->hw_bank.abs)

struct ci13xxx_msm_context {
	struct platform_device *plat_ci;
	int wake_gpio;
	int wake_irq;
	bool wake_irq_state;
};

static void ci13xxx_msm_suspend(struct ci13xxx *ci)
{
	struct device *dev = ci->dev.parent;
	struct ci13xxx_msm_context *ctx = dev_get_drvdata(dev);

	dev_dbg(dev, "ci13xxx_msm_suspend\n");

	if (ctx->wake_irq && !ctx->wake_irq_state) {
		enable_irq_wake(ctx->wake_irq);
		enable_irq(ctx->wake_irq);
		ctx->wake_irq_state = true;
	}
}

static void ci13xxx_msm_resume(struct ci13xxx *ci)
{
	struct device *dev = ci->dev.parent;
	struct ci13xxx_msm_context *ctx = dev_get_drvdata(dev);

	dev_dbg(dev, "ci13xxx_msm_resume\n");

	if (ctx->wake_irq && ctx->wake_irq_state) {
		disable_irq_wake(ctx->wake_irq);
		disable_irq_nosync(ctx->wake_irq);
		ctx->wake_irq_state = false;
	}
}

static void ci13xxx_msm_disconnect(struct ci13xxx *ci)
{
	struct usb_phy *phy = ci->transceiver;

	if (phy && (phy->flags & ENABLE_DP_MANUAL_PULLUP))
		usb_phy_io_write(phy,
				ULPI_MISC_A_VBUSVLDEXT |
				ULPI_MISC_A_VBUSVLDEXTSEL,
				ULPI_CLR(ULPI_MISC_A));
}

/* Link power management will reduce power consumption by
 * short time HW suspend/resume.
 */
static void ci13xxx_msm_set_l1(struct ci13xxx *ci)
{
	int temp;
	struct device *dev = ci->gadget.dev.parent;

	dev_dbg(dev, "Enable link power management\n");

	/* Enable remote wakeup and L1 for IN EPs */
	writel_relaxed(0xffff0000, USB_L1_EP_CTRL);

	temp = readl_relaxed(USB_L1_CONFIG);
	temp |= L1_CONFIG_LPM_EN | L1_CONFIG_REMOTE_WAKEUP |
		L1_CONFIG_GATE_SYS_CLK | L1_CONFIG_PHY_LPM |
		L1_CONFIG_PLL;
	writel_relaxed(temp, USB_L1_CONFIG);
}

static void ci13xxx_msm_connect(struct ci13xxx *ci)
{
	struct usb_phy *phy = ci->transceiver;

	if (phy && (phy->flags & ENABLE_DP_MANUAL_PULLUP)) {
		int	temp;

		usb_phy_io_write(phy,
			ULPI_MISC_A_VBUSVLDEXT |
			ULPI_MISC_A_VBUSVLDEXTSEL,
			ULPI_SET(ULPI_MISC_A));

		temp = readl_relaxed(USB_GENCONFIG2);
		temp |= GENCFG2_SESS_VLD_CTRL_EN;
		writel_relaxed(temp, USB_GENCONFIG2);

		temp = readl_relaxed(USB_USBCMD);
		temp |= USBCMD_SESS_VLD_CTRL;
		writel_relaxed(temp, USB_USBCMD);

		/*
		 * Add memory barrier as it is must to complete
		 * above USB PHY and Link register writes before
		 * moving ahead with USB peripheral mode enumeration,
		 * otherwise USB peripheral mode may not work.
		 */
		mb();
	}
}

static void ci13xxx_msm_reset(struct ci13xxx* ci)
{
	struct usb_phy *phy = ci->transceiver;
	struct device *dev = ci->gadget.dev.parent;

	writel_relaxed(0, USB_AHBBURST);
	writel_relaxed(0x08, USB_AHBMODE);

	if (ci->gadget.l1_supported)
		ci13xxx_msm_set_l1(ci);

	if (phy && (phy->flags & ENABLE_SECONDARY_PHY)) {
		int	temp;

		dev_dbg(dev, "using secondary hsphy\n");
		temp = readl_relaxed(USB_PHY_CTRL2);
		temp |= (1<<16);
		writel_relaxed(temp, USB_PHY_CTRL2);

		/*
		 * Add memory barrier to make sure above LINK writes are
		 * complete before moving ahead with USB peripheral mode
		 * enumeration.
		 */
		mb();
	}
}

static void ci13xxx_msm_notify_event(struct ci13xxx *ci, unsigned event)
{
	struct device *dev = ci->gadget.dev.parent;

	switch (event) {
	case CI13XXX_CONTROLLER_RESET_EVENT:
		dev_info(dev, "CI13XXX_CONTROLLER_RESET_EVENT received\n");
		ci13xxx_msm_reset(ci);
		break;
	case CI13XXX_CONTROLLER_DISCONNECT_EVENT:
		dev_info(dev, "CI13XXX_CONTROLLER_DISCONNECT_EVENT received\n");
		ci13xxx_msm_disconnect(ci);
		ci13xxx_msm_resume(ci);
		break;
	case CI13XXX_CONTROLLER_CONNECT_EVENT:
		dev_info(dev, "CI13XXX_CONTROLLER_CONNECT_EVENT received\n");
		ci13xxx_msm_connect(ci);
		break;
	case CI13XXX_CONTROLLER_SUSPEND_EVENT:
		dev_info(dev, "CI13XXX_CONTROLLER_SUSPEND_EVENT received\n");
		ci13xxx_msm_suspend(ci);
		break;
	case CI13XXX_CONTROLLER_RESUME_EVENT:
		dev_info(dev, "CI13XXX_CONTROLLER_RESUME_EVENT received\n");
		ci13xxx_msm_resume(ci);
		break;
	default:
		dev_dbg(dev, "unknown ci13xxx event\n");
		break;
	}
}

static irqreturn_t ci13xxx_msm_resume_irq(int irq, void *data)
{
	struct ci13xxx *ci = data;

	if (ci->transceiver && ci->vbus_active && ci->suspended)
		usb_phy_set_suspend(ci->transceiver, 0);
	else if (!ci->suspended)
		ci13xxx_msm_resume(ci);

	return IRQ_HANDLED;
}

static struct ci13xxx_platform_data ci13xxx_msm_platdata = {
	.name			= "ci13xxx_msm",
	.flags			= CI13XXX_REGS_SHARED |
				  CI13XXX_REQUIRE_TRANSCEIVER |
				  CI13XXX_PULLUP_ON_VBUS |
				  CI13XXX_ZERO_ITC |
				  CI13XXX_DISABLE_STREAMING |
				  CI13XXX_IS_OTG,
	.nz_itc			= 0,
	.notify_event		= ci13xxx_msm_notify_event,
};

static int ci13xxx_msm_install_wake_gpio(struct platform_device *pdev,
				struct resource *res)
{
	int wake_irq;
	int ret;
	struct ci13xxx_msm_context *ctx = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "ci13xxx_msm_install_wake_gpio\n");

	ctx->wake_gpio = res->start;
	gpio_request(ctx->wake_gpio, "USB_RESUME");
	gpio_direction_input(ctx->wake_gpio);
	wake_irq = gpio_to_irq(ctx->wake_gpio);
	if (wake_irq < 0) {
		dev_err(&pdev->dev, "could not register USB_RESUME GPIO.\n");
		return -ENXIO;
	}

	dev_dbg(&pdev->dev, "ctx->gpio_irq = %d and irq = %d\n",
			ctx->wake_gpio, wake_irq);
	ret = request_irq(wake_irq, ci13xxx_msm_resume_irq,
		IRQF_TRIGGER_RISING | IRQF_ONESHOT, "usb resume", NULL);
	if (ret < 0) {
		dev_err(&pdev->dev, "could not register USB_RESUME IRQ.\n");
		goto gpio_free;
	}
	disable_irq(wake_irq);
	ctx->wake_irq = wake_irq;

	return 0;

gpio_free:
	gpio_free(ctx->wake_gpio);
	ctx->wake_gpio = 0;
	return ret;
}

static void ci13xxx_msm_uninstall_wake_gpio(struct platform_device *pdev)
{
	struct ci13xxx_msm_context *ctx = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "ci13xxx_msm_uninstall_wake_gpio\n");

	if (ctx->wake_gpio) {
		gpio_free(ctx->wake_gpio);
		ctx->wake_gpio = 0;
	}
}


static int ci13xxx_msm_probe(struct platform_device *pdev)
{
	struct ci13xxx_msm_context *ctx;
	struct platform_device *plat_ci;
	struct resource *res;
	int ret, rc;
	struct ci13xxx_platform_data *pdata = pdev->dev.platform_data;

	dev_dbg(&pdev->dev, "ci13xxx_msm_probe\n");

	ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	if (pdata) {
		/* Acceptable values for nz_itc are: 0,1,2,4,8,16,32,64 */
		if (pdata->log2_itc > CI13XXX_MSM_MAX_LOG2_ITC ||
			pdata->log2_itc <= 0)
			ci13xxx_msm_platdata.nz_itc = 0;
		else
			ci13xxx_msm_platdata.nz_itc = 1 << (pdata->log2_itc-1);
		ci13xxx_msm_platdata.l1_supported = pdata->l1_supported;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_IO, "USB_RESUME");
	if (res) {
		ret = ci13xxx_msm_install_wake_gpio(pdev, res);
		if (ret < 0) {
			dev_err(&pdev->dev, "gpio irq install failed\n");
			return ret;
		}
	}

	plat_ci = ci13xxx_add_device(&pdev->dev,
				pdev->resource, pdev->num_resources,
				&ci13xxx_msm_platdata);
	if (IS_ERR(plat_ci)) {
		dev_err(&pdev->dev, "ci13xxx_add_device failed!\n");
		goto gpio_uninstall;
	}

	platform_set_drvdata(pdev, ctx);

	pm_runtime_no_callbacks(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	return 0;

gpio_uninstall:
	ci13xxx_msm_uninstall_wake_gpio(pdev);
	return PTR_ERR(plat_ci);
}

static int ci13xxx_msm_remove(struct platform_device *pdev)
{
	struct ci13xxx_msm_context *ctx = platform_get_drvdata(pdev);
	struct platform_device *plat_ci = ctx->plat_ci;

	pm_runtime_disable(&pdev->dev);
	ci13xxx_remove_device(plat_ci);
	ci13xxx_msm_uninstall_wake_gpio(pdev);

	return 0;
}

void msm_hw_bam_disable(bool bam_disable)
{
	u32 val;

	if (bam_disable)
		val = readl_relaxed(USB_GENCONFIG) | GENCONFIG_BAM_DISABLE;
	else
		val = readl_relaxed(USB_GENCONFIG) & ~GENCONFIG_BAM_DISABLE;

	writel_relaxed(val, USB_GENCONFIG);
}

static struct platform_driver ci13xxx_msm_driver = {
	.probe = ci13xxx_msm_probe,
	.remove = ci13xxx_msm_remove,
	.driver = {
		.name = "msm_hsusb",
	},
};

module_platform_driver(ci13xxx_msm_driver);

MODULE_ALIAS("platform:msm_hsusb");
MODULE_LICENSE("GPL v2");
