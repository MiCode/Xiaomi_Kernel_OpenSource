
/* xhci-intel.c - Intel xHCI glue driver
 *
 * Copyright (C) 2014 Intel Corp.
 *
 * Author: Jincan Zhuang <jin.can.zhuang@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

struct xhci_intel {
	struct device *dev;
	struct platform_device **ext_pdevs;
	int	dev_num;
};

static int xhci_intel_probe(struct platform_device *pdev)
{
	char **devices = (char **)pdev->id_entry->driver_data;
	struct device	*controller = pdev->dev.parent;
	struct xhci_intel *ihost;
	struct platform_device **ext_pdevs;
	int i, ret, dev_num = 0;

	if (!devices) {
		dev_err(&pdev->dev, "Can't probe without drvdata\n");
		return -EINVAL;
	}

	for (i = 0; devices[i]; i++)
		;
	dev_num = i;
	dev_info(&pdev->dev, "%d ext devices to be created\n", dev_num);

	ihost = devm_kzalloc(&pdev->dev, sizeof(*ihost), GFP_KERNEL);
	if (!ihost)
		return -ENOMEM;

	ext_pdevs = devm_kzalloc(&pdev->dev,
				sizeof(struct platform_device *) * dev_num,
				GFP_KERNEL);
	if (!ext_pdevs)
		return -ENOMEM;

	platform_set_drvdata(pdev, ihost);
	ihost->dev = &pdev->dev;
	ihost->ext_pdevs = ext_pdevs;
	ihost->dev_num = dev_num;

	/* Setup extended capability devices */
	for (i = 0; i < dev_num; i++) {

		dev_info(&pdev->dev, "create ext pdev %s\n", devices[i]);
		ext_pdevs[i] = platform_device_alloc(devices[i],
						PLATFORM_DEVID_AUTO);
		if (!ext_pdevs[i]) {
			dev_err(&pdev->dev, "can't create %s\n", devices[i]);
			i--;
			while (i >= 0)
				platform_device_put(ext_pdevs[i--]);
			return -ENOMEM;
		}
	}

	for (i = 0; i < dev_num; i++) {
		ext_pdevs[i]->dev.parent = controller;
		ret = platform_device_add(ext_pdevs[i]);
		if (ret) {
			dev_err(&pdev->dev, "can't add %s\n", devices[i]);
			i--;
			while (i >= 0)
				platform_device_del(ext_pdevs[i--]);
			goto err;
		}
	}

	pm_runtime_no_callbacks(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	dev_info(&pdev->dev, "xhci_intel_probe succeeds\n");
	return 0;

err:
	for (i = 0; i < dev_num; i++)
		platform_device_put(ext_pdevs[i]);

	return ret;
}

static int xhci_intel_remove(struct platform_device *pdev)
{
	struct xhci_intel *ihost = platform_get_drvdata(pdev);
	int dev_num = ihost->dev_num;
	int i;

	for (i = 0; i < dev_num; i++)
		platform_device_unregister(ihost->ext_pdevs[i]);

	return 0;
}

static const char * const xhci_cht[] = {
	"phy-intel-hsic",
	"phy-intel-ssic",
	NULL,
};

static struct platform_device_id xhci_intel_driver_ids[] = {
	{
		.name	= "xhci-cht",
		.driver_data = (kernel_ulong_t)&xhci_cht,
	},
	{},
};
MODULE_DEVICE_TABLE(platform, xhci_intel_driver_ids);

static struct platform_driver xhci_intel_driver = {
	.probe          = xhci_intel_probe,
	.remove         = xhci_intel_remove,
	.driver         = {
		.name   = "xhci-intel",
		.owner  = THIS_MODULE,
	},
	.id_table	= xhci_intel_driver_ids,
};

static int __init xhci_intel_init(void)
{
	return platform_driver_register(&xhci_intel_driver);
}
subsys_initcall(xhci_intel_init);

static void __exit xhci_intel_exit(void)
{
	platform_driver_unregister(&xhci_intel_driver);
}
module_exit(xhci_intel_exit);

MODULE_DESCRIPTION("Intel xHCI Glue");
MODULE_AUTHOR("Zhuang Jin Can <jin.can.zhuang@intel.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:xhci-intel");
