/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/wakelock.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/usb.h>

#include <linux/usb/gadget.h>
#include <linux/usb/msm_hsusb_hw.h>
#include <linux/usb/msm_hsusb.h>

#include <mach/clk.h>
#include <mach/msm_iomap.h>
#include <mach/msm_xo.h>

#include "ci13xxx_udc.c"

#define MSM_USB_BASE	(mhsic->regs)

#define ULPI_IO_TIMEOUT_USEC	(10 * 1000)
#define USB_PHY_VDD_DIG_VOL_MIN		1045000 /* uV */
#define USB_PHY_VDD_DIG_VOL_MAX		1320000 /* uV */
#define USB_PHY_VDD_DIG_LOAD		49360	/* uA */
#define LINK_RESET_TIMEOUT_USEC		(250 * 1000)
#define HSIC_CFG_REG 0x30
#define HSIC_IO_CAL_PER_REG 0x33
#define HSIC_DBG1_REG 0x38

struct msm_hsic_per *the_mhsic;

struct msm_hsic_per {
	struct device		*dev;
	struct clk			*iface_clk;
	struct clk			*core_clk;
	struct clk			*alt_core_clk;
	struct clk			*phy_clk;
	struct clk			*cal_clk;
	struct regulator	*hsic_vddcx;
	bool				async_int;
	void __iomem		*regs;
	int					irq;
};

static int msm_hsic_init_vddcx(struct msm_hsic_per *mhsic, int init)
{
	int ret = 0;

	if (!init)
		goto disable_reg;

	mhsic->hsic_vddcx = regulator_get(mhsic->dev, "HSIC_VDDCX");
	if (IS_ERR(mhsic->hsic_vddcx)) {
		dev_err(mhsic->dev, "unable to get hsic vddcx\n");
		return PTR_ERR(mhsic->hsic_vddcx);
	}

	ret = regulator_set_voltage(mhsic->hsic_vddcx,
			USB_PHY_VDD_DIG_VOL_MIN,
			USB_PHY_VDD_DIG_VOL_MAX);
	if (ret) {
		dev_err(mhsic->dev, "unable to set the voltage"
				"for hsic vddcx\n");
		goto reg_set_voltage_err;
	}

	ret = regulator_set_optimum_mode(mhsic->hsic_vddcx,
				USB_PHY_VDD_DIG_LOAD);
	if (ret < 0) {
		pr_err("%s: Unable to set optimum mode of the regulator:"
					"VDDCX\n", __func__);
		goto reg_optimum_mode_err;
	}

	ret = regulator_enable(mhsic->hsic_vddcx);
	if (ret) {
		dev_err(mhsic->dev, "unable to enable hsic vddcx\n");
		goto reg_enable_err;
	}

	return 0;

disable_reg:
	regulator_disable(mhsic->hsic_vddcx);
reg_enable_err:
	regulator_set_optimum_mode(mhsic->hsic_vddcx, 0);
reg_optimum_mode_err:
	regulator_set_voltage(mhsic->hsic_vddcx, 0,
				USB_PHY_VDD_DIG_VOL_MIN);
reg_set_voltage_err:
	regulator_put(mhsic->hsic_vddcx);

	return ret;

}

static int ulpi_write(struct msm_hsic_per *mhsic, u32 val, u32 reg)
{
	int cnt = 0;

	/* initiate write operation */
	writel_relaxed(ULPI_RUN | ULPI_WRITE |
	       ULPI_ADDR(reg) | ULPI_DATA(val),
	       USB_ULPI_VIEWPORT);

	/* wait for completion */
	while (cnt < ULPI_IO_TIMEOUT_USEC) {
		if (!(readl_relaxed(USB_ULPI_VIEWPORT) & ULPI_RUN))
			break;
		udelay(1);
		cnt++;
	}

	if (cnt >= ULPI_IO_TIMEOUT_USEC) {
		dev_err(mhsic->dev, "ulpi_write: timeout\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int msm_hsic_phy_clk_reset(struct msm_hsic_per *mhsic)
{
	int ret;

	ret = clk_reset(mhsic->core_clk, CLK_RESET_ASSERT);
	if (ret) {
		clk_disable(mhsic->alt_core_clk);
		dev_err(mhsic->dev, "usb phy clk assert failed\n");
		return ret;
	}
	usleep_range(10000, 12000);
	clk_disable(mhsic->alt_core_clk);

	ret = clk_reset(mhsic->core_clk, CLK_RESET_DEASSERT);
	if (ret)
		dev_err(mhsic->dev, "usb phy clk deassert failed\n");

	return ret;
}

static int msm_hsic_phy_reset(struct msm_hsic_per *mhsic)
{
	u32 val;
	int ret;

	ret = msm_hsic_phy_clk_reset(mhsic);
	if (ret)
		return ret;

	val = readl_relaxed(USB_PORTSC) & ~PORTSC_PTS_MASK;
	writel_relaxed(val | PORTSC_PTS_ULPI, USB_PORTSC);

	/*
	 * Ensure that RESET operation is completed before
	 * turning off clock.
	 */
	mb();
	dev_dbg(mhsic->dev, "phy_reset: success\n");

	return 0;
}

static int msm_hsic_enable_clocks(struct platform_device *pdev,
				struct msm_hsic_per *mhsic, bool enable)
{
	int ret = 0;

	if (!enable)
		goto put_clocks;

	mhsic->iface_clk = clk_get(&pdev->dev, "iface_clk");
	if (IS_ERR(mhsic->iface_clk)) {
		dev_err(mhsic->dev, "failed to get iface_clk\n");
		ret = PTR_ERR(mhsic->iface_clk);
		goto put_iface_clk;
	}

	mhsic->core_clk = clk_get(&pdev->dev, "core_clk");
	if (IS_ERR(mhsic->core_clk)) {
		dev_err(mhsic->dev, "failed to get core_clk\n");
		ret = PTR_ERR(mhsic->core_clk);
		goto put_core_clk;
	}

	mhsic->phy_clk = clk_get(&pdev->dev, "phy_clk");
	if (IS_ERR(mhsic->phy_clk)) {
		dev_err(mhsic->dev, "failed to get phy_clk\n");
		ret = PTR_ERR(mhsic->phy_clk);
		goto put_phy_clk;
	}

	mhsic->alt_core_clk = clk_get(&pdev->dev, "alt_core_clk");
	if (IS_ERR(mhsic->alt_core_clk)) {
		dev_err(mhsic->dev, "failed to get alt_core_clk\n");
		ret = PTR_ERR(mhsic->alt_core_clk);
		goto put_alt_core_clk;
	}

	mhsic->cal_clk = clk_get(&pdev->dev, "cal_clk");
	if (IS_ERR(mhsic->cal_clk)) {
		dev_err(mhsic->dev, "failed to get cal_clk\n");
		ret = PTR_ERR(mhsic->cal_clk);
		goto put_cal_clk;
	}

	clk_enable(mhsic->iface_clk);
	clk_enable(mhsic->core_clk);
	clk_enable(mhsic->phy_clk);
	clk_enable(mhsic->alt_core_clk);
	clk_enable(mhsic->cal_clk);

	return 0;

put_clocks:
	clk_disable(mhsic->iface_clk);
	clk_disable(mhsic->core_clk);
	clk_disable(mhsic->phy_clk);
	clk_disable(mhsic->alt_core_clk);
	clk_disable(mhsic->cal_clk);
put_cal_clk:
	clk_put(mhsic->cal_clk);
put_alt_core_clk:
	clk_put(mhsic->alt_core_clk);
put_phy_clk:
	clk_put(mhsic->phy_clk);
put_core_clk:
	clk_put(mhsic->core_clk);
put_iface_clk:
	clk_put(mhsic->iface_clk);

	return ret;
}

static int msm_hsic_reset(struct msm_hsic_per *mhsic)
{
	int cnt = 0;
	int ret;

	ret = msm_hsic_phy_reset(mhsic);
	if (ret) {
		dev_err(mhsic->dev, "phy_reset failed\n");
		return ret;
	}

	writel_relaxed(USBCMD_RESET, USB_USBCMD);
	while (cnt < LINK_RESET_TIMEOUT_USEC) {
		if (!(readl_relaxed(USB_USBCMD) & USBCMD_RESET))
			break;
		udelay(1);
		cnt++;
	}
	if (cnt >= LINK_RESET_TIMEOUT_USEC)
		return -ETIMEDOUT;

	/* Reset PORTSC and select ULPI phy */
	writel_relaxed(0x80000000, USB_PORTSC);
	return 0;
}

static void msm_hsic_start(void)
{
	int ret;

	/* programmable length of connect signaling (33.2ns) */
	ret = ulpi_write(the_mhsic, 3, HSIC_DBG1_REG);
	if (ret) {
		pr_err("%s: Unable to program length of connect signaling\n",
			    __func__);
	}

	/*set periodic calibration interval to ~2.048sec in HSIC_IO_CAL_REG */
	ret = ulpi_write(the_mhsic, 0xFF, HSIC_IO_CAL_PER_REG);

	if (ret) {
		pr_err("%s: Unable to set periodic calibration interval\n",
			    __func__);
	}

	/* Enable periodic IO calibration in HSIC_CFG register */
	ret = ulpi_write(the_mhsic, 0xE9, HSIC_CFG_REG);
	if (ret) {
		pr_err("%s: Unable to enable periodic IO calibration\n",
			    __func__);
	}
}

/**
 * Dummy match function - will be called only for HSIC msm
 * device (msm_device_gadget_hsic_peripheral).
 */
static inline int __match(struct device *dev, void *data) { return 1; }

static void msm_hsic_connect_peripheral(struct device *msm_udc_dev)
{
	struct device *dev;
	struct usb_gadget *gadget;

	dev = device_find_child(msm_udc_dev, NULL, __match);
	gadget = dev_to_usb_gadget(dev);
	usb_gadget_vbus_connect(gadget);
}

static irqreturn_t msm_udc_hsic_irq(int irq, void *data)
{
	return udc_irq();
}

static void ci13xxx_msm_hsic_notify_event(struct ci13xxx *udc, unsigned event)
{
	struct device *dev = udc->gadget.dev.parent;
	struct msm_hsic_per *mhsic = the_mhsic;

	switch (event) {
	case CI13XXX_CONTROLLER_RESET_EVENT:
		dev_dbg(dev, "CI13XXX_CONTROLLER_RESET_EVENT received\n");
		writel_relaxed(0, USB_AHBBURST);
		writel_relaxed(0, USB_AHBMODE);
		break;
	case CI13XXX_CONTROLLER_CONNECT_EVENT:
		dev_dbg(dev, "CI13XXX_CONTROLLER_CONNECT_EVENT received\n");
		msm_hsic_start();
		break;
	default:
		dev_dbg(dev, "unknown ci13xxx_udc event\n");
		break;
	}
}

static struct ci13xxx_udc_driver ci13xxx_msm_udc_hsic_driver = {
	.name			= "ci13xxx_msm_hsic",
	.flags			= CI13XXX_REGS_SHARED |
				  CI13XXX_PULLUP_ON_VBUS |
				  CI13XXX_DISABLE_STREAMING |
				  CI13XXX_ZERO_ITC,

	.notify_event		= ci13xxx_msm_hsic_notify_event,
};

static int __devinit msm_hsic_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct msm_hsic_per *mhsic;
	int ret = 0;

	dev_dbg(&pdev->dev, "msm-hsic probe\n");

	mhsic = kzalloc(sizeof(struct msm_hsic_per), GFP_KERNEL);
	if (!mhsic) {
		dev_err(&pdev->dev, "unable to allocate msm_hsic\n");
		return -ENOMEM;
	}
	the_mhsic = mhsic;
	platform_set_drvdata(pdev, mhsic);
	mhsic->dev = &pdev->dev;

	mhsic->irq = platform_get_irq(pdev, 0);
	if (mhsic->irq < 0) {
		dev_err(&pdev->dev, "Unable to get IRQ resource\n");
		ret = mhsic->irq;
		goto error;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Unable to get memory resource\n");
		ret = -ENODEV;
		goto error;
	}
	mhsic->regs = ioremap(res->start, resource_size(res));
	if (!mhsic->regs) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto unmap;
	}
	dev_info(&pdev->dev, "HSIC Peripheral regs = %p\n", mhsic->regs);

	ret = msm_hsic_enable_clocks(pdev, mhsic, true);

	if (ret) {
		dev_err(&pdev->dev, "msm_hsic_enable_clocks failed\n");
		ret = -ENODEV;
		goto deinit_clocks;
	}
	ret = msm_hsic_init_vddcx(mhsic, 1);
	if (ret) {
		dev_err(&pdev->dev, "unable to initialize VDDCX\n");
		ret = -ENODEV;
		goto deinit_vddcx;
	}

	ret = msm_hsic_reset(mhsic);
	if (ret) {
		dev_err(&pdev->dev, "msm_hsic_reset failed\n");
		ret = -ENODEV;
		goto deinit_vddcx;
	}

	ret = udc_probe(&ci13xxx_msm_udc_hsic_driver, &pdev->dev, mhsic->regs);
	if (ret < 0) {
		dev_err(&pdev->dev, "udc_probe failed\n");
		ret = -ENODEV;
		goto deinit_vddcx;
	}

	msm_hsic_connect_peripheral(&pdev->dev);

	ret = request_irq(mhsic->irq, msm_udc_hsic_irq,
					  IRQF_SHARED, pdev->name, pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "request_irq failed\n");
		ret = -ENODEV;
		goto udc_remove;
	}

	pm_runtime_no_callbacks(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	return 0;
udc_remove:
	udc_remove();
deinit_vddcx:
	msm_hsic_init_vddcx(mhsic, 0);
deinit_clocks:
	msm_hsic_enable_clocks(pdev, mhsic, 0);
unmap:
	iounmap(mhsic->regs);
error:
	kfree(mhsic);
	return ret;
}

static int __devexit hsic_msm_remove(struct platform_device *pdev)
{
	struct msm_hsic_per *mhsic = platform_get_drvdata(pdev);

	device_init_wakeup(&pdev->dev, 0);
	msm_hsic_init_vddcx(mhsic, 0);
	msm_hsic_enable_clocks(pdev, mhsic, 0);
	udc_remove();
	iounmap(mhsic->regs);
	kfree(mhsic);

	return 0;
}
static struct platform_driver msm_hsic_peripheral_driver = {
	.probe	= msm_hsic_probe,
	.remove	= __devexit_p(hsic_msm_remove),
	.driver = {
		.name = "msm_hsic_peripheral",
	},
};

static int __init msm_hsic_peripheral_init(void)
{
	return platform_driver_probe(&msm_hsic_peripheral_driver,
								msm_hsic_probe);
}

static void __exit msm_hsic_peripheral_exit(void)
{
	platform_driver_unregister(&msm_hsic_peripheral_driver);
}

module_init(msm_hsic_peripheral_init);
module_exit(msm_hsic_peripheral_exit);

