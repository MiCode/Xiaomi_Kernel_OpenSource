/* ehci-msm.c - HSUSB Host Controller Driver Implementation
 *
 * Copyright (c) 2008-2018, The Linux Foundation. All rights reserved.
 *
 * Partly derived from ehci-fsl.c and ehci-hcd.c
 * Copyright (c) 2000-2004 by David Brownell
 * Copyright (c) 2005 MontaVista Software
 *
 * All source code in this file is licensed under the following license except
 * where indicated.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/dma-mapping.h>

#include <linux/usb/otg.h>
#include <linux/usb/msm_hsusb.h>
#include <linux/usb/msm_hsusb_hw.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>

#include "ehci.h"

#define MSM_USB_BASE (hcd->regs)

#define DRIVER_DESC "Qualcomm On-Chip EHCI Host Controller"

static const char hcd_name[] = "ehci-msm";
static struct hc_driver __read_mostly msm_hc_driver;

static int ehci_msm_reset(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	int retval;

	ehci->caps = USB_CAPLENGTH;
	hcd->has_tt = 1;
	ehci->no_testmode_suspend = true;

	retval = ehci_setup(hcd);
	if (retval)
		return retval;

	/* bursts of unspecified length. */
	writel_relaxed(0, USB_AHBBURST);
	/* Use the AHB transactor */
	writel_relaxed(0x08, USB_AHBMODE);
	/* Disable streaming mode and select host mode */
	writel_relaxed(0x13, USB_USBMODE);

	if (hcd->usb_phy->flags & ENABLE_SECONDARY_PHY) {
		ehci_dbg(ehci, "using secondary hsphy\n");
		writel_relaxed(readl_relaxed(USB_PHY_CTRL2) | (1<<16),
							USB_PHY_CTRL2);
	}

	/* Disable ULPI_TX_PKT_EN_CLR_FIX which is valid only for HSIC */
	writel_relaxed(readl_relaxed(USB_GENCONFIG_2) & ~(1<<19),
					USB_GENCONFIG_2);
	return 0;
}

static u64 msm_ehci_dma_mask = DMA_BIT_MASK(32);
static int ehci_msm_probe(struct platform_device *pdev)
{
	struct usb_hcd *hcd;
	struct resource *res;
	struct usb_phy *phy;
	int ret;

	dev_dbg(&pdev->dev, "ehci_msm proble\n");

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &msm_ehci_dma_mask;
	if (!pdev->dev.coherent_dma_mask)
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	hcd = usb_create_hcd(&msm_hc_driver, &pdev->dev,
			     dev_name(&pdev->dev));
	if (!hcd) {
		dev_err(&pdev->dev, "Unable to create HCD\n");
		return  -ENOMEM;
	}

	hcd_to_bus(hcd)->skip_resume = true;

	ret = platform_get_irq(pdev, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "Unable to get IRQ resource\n");
		goto put_hcd;
	}
	hcd->irq = ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	hcd->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(hcd->regs)) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = PTR_ERR(hcd->regs);
		goto put_hcd;
	}
	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);

	/*
	 * OTG driver takes care of PHY initialization, clock management,
	 * powering up VBUS, mapping of registers address space and power
	 * management.
	 */
	if (pdev->dev.of_node)
		phy = devm_usb_get_phy_by_phandle(&pdev->dev, "usb-phy", 0);
	else
		phy = devm_usb_get_phy(&pdev->dev, USB_PHY_TYPE_USB2);

	if (IS_ERR_OR_NULL(phy)) {
		dev_err(&pdev->dev, "unable to find transceiver\n");
		ret = -EPROBE_DEFER;
		goto put_hcd;
	}

	ret = otg_set_host(phy->otg, &hcd->self);
	if (ret < 0) {
		dev_err(&pdev->dev, "unable to register with transceiver\n");
		goto put_hcd;
	}

	hcd->usb_phy = phy;
	device_init_wakeup(&pdev->dev, 1);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	msm_bam_set_usb_host_dev(&pdev->dev);

	return 0;

put_hcd:
	usb_put_hcd(hcd);

	return ret;
}

static int ehci_msm_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);

	device_init_wakeup(&pdev->dev, 0);
	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);

	otg_set_host(hcd->usb_phy->otg, NULL);
	hcd->usb_phy = NULL;

	usb_put_hcd(hcd);

	return 0;
}

#ifdef CONFIG_PM
static int ehci_msm_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "ehci runtime suspend\n");

	return 0;
}

static int ehci_msm_runtime_resume(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	u32 portsc;

	dev_dbg(dev, "ehci runtime resume\n");

	portsc = readl_relaxed(USB_PORTSC);
	portsc &= ~PORT_RWC_BITS;
	portsc |= PORT_RESUME;
	writel_relaxed(portsc, USB_PORTSC);

	return 0;
}
#endif

static const struct dev_pm_ops ehci_msm_dev_pm_ops = {
	SET_RUNTIME_PM_OPS(ehci_msm_runtime_suspend, ehci_msm_runtime_resume,
			   NULL)
};

static const struct of_device_id msm_ehci_dt_match[] = {
	{ .compatible = "qcom,ehci-host", },
	{}
};
MODULE_DEVICE_TABLE(of, msm_ehci_dt_match);

static struct platform_driver ehci_msm_driver = {
	.probe	= ehci_msm_probe,
	.remove	= ehci_msm_remove,
	.shutdown = usb_hcd_platform_shutdown,
	.driver = {
		   .name = "msm_hsusb_host",
		   .pm = &ehci_msm_dev_pm_ops,
		   .of_match_table = msm_ehci_dt_match,
	},
};

static const struct ehci_driver_overrides msm_overrides __initconst = {
	.reset = ehci_msm_reset,
};

static int __init ehci_msm_init(void)
{
	if (usb_disabled())
		return -ENODEV;

	pr_info("%s: " DRIVER_DESC "\n", hcd_name);
	ehci_init_driver(&msm_hc_driver, &msm_overrides);
	return platform_driver_register(&ehci_msm_driver);
}
module_init(ehci_msm_init);

static void __exit ehci_msm_cleanup(void)
{
	platform_driver_unregister(&ehci_msm_driver);
}
module_exit(ehci_msm_cleanup);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_ALIAS("platform:msm-ehci");
MODULE_LICENSE("GPL");
