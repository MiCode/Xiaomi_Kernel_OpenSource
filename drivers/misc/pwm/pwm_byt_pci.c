/*
 * Intel Baytrail PWM driver.
 *
 * Copyright (C) 2013 Intel corporation.
 *
 * ----------------------------------------------------------------------------
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/pm_runtime.h>
#include <linux/pwm.h>
#include "pwm_byt_core.h"

static int pwm_byt_pci_probe(struct pci_dev *pdev,
			    const struct pci_device_id *id)
{
	static int pwm_num;
	void __iomem **tbl;
	int r;
	r = pcim_enable_device(pdev);
	if (r) {
		dev_err(&pdev->dev, "Failed to enable PWM PCI device (%d)\n",
			r);
		return r;
	}

	r = pcim_iomap_regions(pdev, 1 << 0, pci_name(pdev));
	if (r) {
		dev_err(&pdev->dev, "I/O memory remapping failed\n");
		return r;
	}

	tbl = (void __iomem **) pcim_iomap_table(pdev);
	if (!tbl) {
		dev_err(&pdev->dev, "IO map table doesn't exist\n");
		pcim_iounmap_regions(pdev, 1 << 0);
		return -EFAULT;
	}

	r = pwm_byt_init(&pdev->dev, tbl[0], pwm_num, id->driver_data);
	if (r) {
		pcim_iounmap_regions(pdev, 1 << 0);
		dev_info(&pdev->dev, "PWM device probe failed!\n");
		return r;
	}

	pm_runtime_set_autosuspend_delay(&pdev->dev, 5);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_allow(&pdev->dev);

	++pwm_num;
	return 0;
}

static void pwm_byt_pci_remove(struct pci_dev *pdev)
{
	pm_runtime_forbid(&pdev->dev);
	pwm_byt_remove(&pdev->dev);
	pcim_iounmap_regions(pdev, 1 << 0);
	pci_disable_device(pdev);
	pci_dev_put(pdev);
}

static const struct pci_device_id pwm_byt_pci_ids[] = {
	{ PCI_VDEVICE(INTEL, 0x2288), PWM_CHT_CLK_KHZ},
	{ PCI_VDEVICE(INTEL, 0x0F08), PWM_BYT_CLK_KHZ},
	{ PCI_VDEVICE(INTEL, 0x0F09), PWM_BYT_CLK_KHZ},
	{ 0,}
};
MODULE_DEVICE_TABLE(pci, pwm_byt_pci_ids);

static struct pci_driver pwm_byt_driver = {
	.name	= "pwm-byt-pci",
	.id_table	= pwm_byt_pci_ids,
	.probe	= pwm_byt_pci_probe,
	.remove	= pwm_byt_pci_remove,
	.driver = {
		.pm = &pwm_byt_pm,
	},
};

module_pci_driver(pwm_byt_driver);

MODULE_ALIAS("pwm-byt-pci");
MODULE_AUTHOR("Wang, Zhifeng<zhifeng.wang@intel.com>");
MODULE_DESCRIPTION("Intel Baytrail PWM driver");
MODULE_LICENSE("GPL");
