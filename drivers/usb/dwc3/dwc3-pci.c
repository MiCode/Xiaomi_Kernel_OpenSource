/**
 * dwc3-pci.c - PCI Specific glue layer
 *
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com
 *
 * Authors: Felipe Balbi <balbi@ti.com>,
 *	    Sebastian Andrzej Siewior <bigeasy@linutronix.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/platform_device.h>

#include <linux/usb/otg.h>
#include <linux/usb/usb_phy_gen_xceiv.h>

#include "platform_data.h"

/* FIXME define these in <linux/pci_ids.h> */
#define PCI_VENDOR_ID_SYNOPSYS		0x16c3
#define PCI_DEVICE_ID_SYNOPSYS_HAPSUSB3	0xabcd
#define PCI_DEVICE_ID_INTEL_BYT		0x0f37
#define PCI_DEVICE_ID_INTEL_MRFLD	0x119e
#define PCI_DEVICE_ID_INTEL_CHT		0x22b7

struct dwc3_pci {
	struct device		*dev;
	struct platform_device	*dwc3;
	struct platform_device	*usb2_phy;
	struct platform_device	*usb3_phy;
	atomic_t		suspend_depth;
};

static int dwc3_pci_register_phys(struct dwc3_pci *glue)
{
	struct usb_phy_gen_xceiv_platform_data pdata;
	struct platform_device	*pdev;
	struct pci_dev	*pci_dev;
	int			ret;

	memset(&pdata, 0x00, sizeof(pdata));

	pci_dev = to_pci_dev(glue->dev);

	if (pci_dev->vendor == PCI_VENDOR_ID_INTEL &&
			pci_dev->device == PCI_DEVICE_ID_INTEL_CHT)
		pdev = platform_device_alloc("intel-cht-otg", 0);
	else
		pdev = platform_device_alloc("usb_phy_gen_xceiv", 0);

	if (!pdev)
		return -ENOMEM;
	pdev->dev.parent = glue->dev;

	glue->usb2_phy = pdev;
	pdata.type = USB_PHY_TYPE_USB2;
	pdata.gpio_reset = -1;

	ret = platform_device_add_data(glue->usb2_phy, &pdata, sizeof(pdata));
	if (ret)
		goto err1;

	pdev = platform_device_alloc("usb_phy_gen_xceiv", 1);
	if (!pdev) {
		ret = -ENOMEM;
		goto err1;
	}

	glue->usb3_phy = pdev;
	pdata.type = USB_PHY_TYPE_USB3;

	ret = platform_device_add_data(glue->usb3_phy, &pdata, sizeof(pdata));
	if (ret)
		goto err2;

	ret = platform_device_add(glue->usb2_phy);
	if (ret)
		goto err2;

	ret = platform_device_add(glue->usb3_phy);
	if (ret)
		goto err3;

	return 0;

err3:
	platform_device_del(glue->usb2_phy);

err2:
	platform_device_put(glue->usb3_phy);

err1:
	platform_device_put(glue->usb2_phy);

	return ret;
}

#define GP_RWREG1			0xa0
#define GP_RWREG1_ULPI_REFCLK_DISABLE	(1 << 17)
static void dwc3_pci_enable_ulpi_refclock(struct pci_dev *pci)
{
	void __iomem	*reg;
	struct resource	res;
	struct device	*dev = &pci->dev;
	u32 		value;

	res.start	= pci_resource_start(pci, 1);
	res.end 	= pci_resource_end(pci, 1);
	res.name	= "dwc_usb3_bar1";
	res.flags	= IORESOURCE_MEM;

	reg = devm_ioremap_resource(dev, &res);
	if (IS_ERR(reg)) {
		dev_err(dev, "cannot check GP_RWREG1 to assert ulpi refclock\n");
		return;
	}

	value = readl(reg + GP_RWREG1);
	if (!(value & GP_RWREG1_ULPI_REFCLK_DISABLE))
		return; /* ULPI refclk already enabled */

	/* Let's clear ULPI refclk disable */
	dev_warn(dev, "ULPI refclock is disable from the BIOS, let's try to enable it\n");
	value &= ~GP_RWREG1_ULPI_REFCLK_DISABLE;
	writel(value, reg + GP_RWREG1);
}

static int dwc3_pci_probe(struct pci_dev *pci,
		const struct pci_device_id *id)
{
	struct resource		res[2];
	struct platform_device	*dwc3;
	struct dwc3_pci		*glue;
	int			ret = -ENOMEM;
	struct device		*dev = &pci->dev;
	struct dwc3_platform_data	pdata;

	memset(&pdata, 0x00, sizeof(pdata));

	glue = devm_kzalloc(dev, sizeof(*glue), GFP_KERNEL);
	if (!glue) {
		dev_err(dev, "not enough memory\n");
		return -ENOMEM;
	}

	glue->dev = dev;

	ret = pci_enable_device(pci);
	if (ret) {
		dev_err(dev, "failed to enable pci device\n");
		return -ENODEV;
	}

	pci_set_master(pci);

	ret = dwc3_pci_register_phys(glue);
	if (ret) {
		dev_err(dev, "couldn't register PHYs\n");
		return ret;
	}

	dwc3 = platform_device_alloc("dwc3", PLATFORM_DEVID_AUTO);
	if (!dwc3) {
		dev_err(dev, "couldn't allocate dwc3 device\n");
		ret = -ENOMEM;
		goto err1;
	}

	pdata.runtime_suspend = true;
	ret = platform_device_add_data(dwc3, &pdata, sizeof(pdata));
	if (ret)
		goto err1;

	memset(res, 0x00, sizeof(struct resource) * ARRAY_SIZE(res));

	res[0].start	= pci_resource_start(pci, 0);
	res[0].end	= pci_resource_end(pci, 0);
	res[0].name	= "dwc_usb3";
	res[0].flags	= IORESOURCE_MEM;

	res[1].start	= pci->irq;
	res[1].name	= "dwc_usb3";
	res[1].flags	= IORESOURCE_IRQ;

	ret = platform_device_add_resources(dwc3, res, ARRAY_SIZE(res));
	if (ret) {
		dev_err(dev, "couldn't add resources to dwc3 device\n");
		goto err1;
	}

	pci_set_drvdata(pci, glue);

	dma_set_coherent_mask(&dwc3->dev, dev->coherent_dma_mask);

	dwc3->dev.dma_mask = dev->dma_mask;
	dwc3->dev.dma_parms = dev->dma_parms;
	dwc3->dev.parent = dev;
	glue->dwc3 = dwc3;

	/*
	 * HACK: we found an issue when enabling DWC3 controller where the
	 * refclock to the phy is not being enabled.
	 * We need an extra step to make sure such clock is enabled.
	 */
	dwc3_pci_enable_ulpi_refclock(pci);

	ret = platform_device_add(dwc3);
	if (ret) {
		dev_err(dev, "failed to register dwc3 device\n");
		goto err3;
	}

	atomic_set(&glue->suspend_depth, 0);

	pm_runtime_set_autosuspend_delay(dev, 100);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_allow(dev);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	return 0;

err3:
	platform_device_put(dwc3);
err1:
	pci_disable_device(pci);

	return ret;
}

static void dwc3_pci_remove(struct pci_dev *pci)
{
	struct dwc3_pci	*glue = pci_get_drvdata(pci);

	pm_runtime_forbid(glue->dev);
	pm_runtime_set_suspended(glue->dev);

	platform_device_unregister(glue->dwc3);
	platform_device_unregister(glue->usb2_phy);
	platform_device_unregister(glue->usb3_phy);
	pci_disable_device(pci);
}

static const struct pci_device_id dwc3_pci_id_table[] = {
	{
		PCI_DEVICE(PCI_VENDOR_ID_SYNOPSYS,
				PCI_DEVICE_ID_SYNOPSYS_HAPSUSB3),
	},
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_BYT), },
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_MRFLD), },
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_CHT), },
	{  }	/* Terminating Entry */
};
MODULE_DEVICE_TABLE(pci, dwc3_pci_id_table);

#ifdef CONFIG_PM_SLEEP
static int dwc3_pci_suspend_common(struct device *dev)
{
	struct pci_dev	*pci = to_pci_dev(dev);
	struct dwc3_pci	*glue = pci_get_drvdata(pci);

	if (atomic_inc_return(&glue->suspend_depth) > 1)
		return 0;

	pci_disable_device(pci);

	return 0;
}

static int dwc3_pci_resume_common(struct device *dev)
{
	struct pci_dev	*pci = to_pci_dev(dev);
	struct dwc3_pci	*glue = pci_get_drvdata(pci);
	int		ret;

	if (atomic_dec_return(&glue->suspend_depth) > 0)
		return 0;

	ret = pci_enable_device(pci);
	if (ret) {
		dev_err(dev, "can't re-enable device --> %d\n", ret);
		return ret;
	}

	pci_set_master(pci);

	return 0;
}

static int dwc3_pci_suspend(struct device *dev)
{
	return dwc3_pci_suspend_common(dev);
}

static int dwc3_pci_resume(struct device *dev)
{
	return dwc3_pci_resume_common(dev);
}

#ifdef CONFIG_PM_RUNTIME

static int dwc3_pci_runtime_suspend(struct device *dev)
{
	return dwc3_pci_suspend_common(dev);
}

static int dwc3_pci_runtime_resume(struct device *dev)
{
	return dwc3_pci_resume_common(dev);
}

#else

#define dwc3_pci_runtime_suspend NULL
#define dwc3_pci_runtime_resume NULL

#endif

#endif /* CONFIG_PM_SLEEP */

static const struct dev_pm_ops dwc3_pci_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(dwc3_pci_suspend, dwc3_pci_resume)
	SET_RUNTIME_PM_OPS(dwc3_pci_runtime_suspend, dwc3_pci_runtime_resume, NULL)
};

static struct pci_driver dwc3_pci_driver = {
	.name		= "dwc3-pci",
	.id_table	= dwc3_pci_id_table,
	.probe		= dwc3_pci_probe,
	.remove		= dwc3_pci_remove,
	.driver		= {
		.pm	= &dwc3_pci_dev_pm_ops,
	},
};

MODULE_AUTHOR("Felipe Balbi <balbi@ti.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DesignWare USB3 PCI Glue Layer");

module_pci_driver(dwc3_pci_driver);
