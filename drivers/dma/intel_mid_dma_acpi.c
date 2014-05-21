
/* intel_mid_dma_acpi.c - Intel MID DMA driver init file for ACPI enumaration.
 *
 * Copyright (c) 2013, Intel Corporation.
 *
 *  Authors:	Ramesh Babu K V <Ramesh.Babu@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/intel_mid_dma.h>
#include <linux/pm_runtime.h>
#include <linux/acpi.h>
#include <acpi/acbuffer.h>
#include <acpi/platform/acenv.h>
#include <acpi/platform/aclinux.h>
#include <acpi/actypes.h>
#include <acpi/acpi_bus.h>

#include "intel_mid_dma_regs.h"

#define HID_MAX_SIZE 8

struct list_head dma_dev_list;

LIST_HEAD(dma_dev_list);

struct acpi_dma_dev_list {
	struct list_head dmadev_list;
	char dma_hid[HID_MAX_SIZE];
	struct device *acpi_dma_dev;
};

struct device *intel_mid_get_acpi_dma(const char *hid)
{
	struct acpi_dma_dev_list *listnode;
	if (list_empty(&dma_dev_list))
		return NULL;

	list_for_each_entry(listnode, &dma_dev_list, dmadev_list) {
		if (!(strncmp(listnode->dma_hid, hid, HID_MAX_SIZE)))
			return listnode->acpi_dma_dev;
	}
	return NULL;
}
EXPORT_SYMBOL_GPL(intel_mid_get_acpi_dma);

#if IS_ENABLED(CONFIG_ACPI)
static int mid_get_and_map_rsrc(void **dest, struct platform_device *pdev,
				unsigned int num)
{
	struct resource *rsrc;
	rsrc = platform_get_resource(pdev, IORESOURCE_MEM, num);
	if (!rsrc) {
		pr_err("%s: Invalid resource - %d", __func__, num);
		return -EIO;
	}
	pr_debug("rsrc #%d = %#x", num, (unsigned int) rsrc->start);
	*dest = devm_ioremap_nocache(&pdev->dev, rsrc->start, resource_size(rsrc));
	if (!*dest) {
		pr_err("%s: unable to map resource: %#x", __func__, (unsigned int)rsrc->start);
		return -EIO;
	}
	return 0;
}

static int mid_platform_get_resources_fdk(struct middma_device *mid_device,
				      struct platform_device *pdev)
{
	int ret;
	struct resource *rsrc;

	pr_debug("%s", __func__);

	/* All ACPI resource request here */
	/* Get DDR addr from platform resource table */
	ret = mid_get_and_map_rsrc(&mid_device->dma_base, pdev, 0);
	if (ret)
		return ret;
	pr_debug("dma_base:%p", mid_device->dma_base);

	/* only get the resource from device table
		mapping is performed in common code */
	rsrc = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!rsrc) {
		pr_warn("%s: Invalid resource for pimr", __func__);
	} else {
		/* add offset for ISRX register */
		mid_device->pimr_base = rsrc->start + SHIM_ISRX_OFFSET;
		pr_debug("pimr_base:%#x", mid_device->pimr_base);
	}

	mid_device->irq = platform_get_irq(pdev, 0);
	if (mid_device->irq < 0) {
		pr_err("invalid irq:%d", mid_device->irq);
		return mid_device->irq;
	}
	pr_debug("irq from pdev is:%d", mid_device->irq);

	return 0;
}

#define DMA_BASE_OFFSET 0x98000
#define DMA_BASE_SIZE 0x4000

static int mid_platform_get_resources_edk2(struct middma_device *mid_device,
				      struct platform_device *pdev)
{
	struct resource *rsrc;
	u32 dma_base_add;

	pr_debug("%s", __func__);
	/* All ACPI resource request here */
	/* Get DDR addr from platform resource table */
	rsrc = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!rsrc) {
		pr_warn("%s: Invalid resource for pimr", __func__);
		return -EINVAL;
	}

	pr_debug("rsrc %#x", (unsigned int)rsrc->start);
	dma_base_add = rsrc->start + DMA_BASE_OFFSET;
	mid_device->dma_base = devm_ioremap_nocache(&pdev->dev, dma_base_add, DMA_BASE_SIZE);
	if (!mid_device->dma_base) {
		pr_err("%s: unable to map resource: %#x", __func__, dma_base_add);
		return -EIO;
	}
	pr_debug("dma_base:%p", mid_device->dma_base);

	/* add offset for ISRX register */
	mid_device->pimr_base = rsrc->start + SHIM_OFFSET + SHIM_ISRX_OFFSET;
	pr_debug("pimr_base:%#x", mid_device->pimr_base);

	mid_device->irq = platform_get_irq(pdev, 0);
	if (mid_device->irq < 0) {
		pr_err("invalid irq:%d", mid_device->irq);
		return mid_device->irq;
	}
	pr_debug("irq from pdev is:%d", mid_device->irq);

	return 0;
}

static int mid_platform_get_resources_lpio(struct middma_device *mid_device,
				      struct platform_device *pdev)
{
	int ret;

	pr_debug("%s\n", __func__);

	/* No need to request PIMR resource here */
	ret = mid_get_and_map_rsrc(&mid_device->dma_base, pdev, 0);
	if (ret)
		return ret;
	pr_debug("dma_base:%p\n", mid_device->dma_base);

	mid_device->irq = platform_get_irq(pdev, 0);
	if (mid_device->irq < 0) {
		pr_err("invalid irq:%d\n", mid_device->irq);
		return mid_device->irq;
	}
	pr_debug("irq from pdev is:%d\n", mid_device->irq);

	return 0;
}

static int mid_platform_get_resources(const char *hid,
		struct middma_device *mid_device, struct platform_device *pdev)
{
	if (!strncmp(hid, "DMA0F28", 7))
		return mid_platform_get_resources_fdk(mid_device, pdev);
	if (!strncmp(hid, "INTL9C60", 8))
		return mid_platform_get_resources_lpio(mid_device, pdev);
	if (!strncmp(hid, "ADMA0F28", 8))
		return mid_platform_get_resources_edk2(mid_device, pdev);
	else if ((!strncmp(hid, "ADMA22A8", 8))) {
		return mid_platform_get_resources_edk2(mid_device, pdev);
	} else if ((!strncmp(hid, "80862286", 8))) {
		return mid_platform_get_resources_lpio(mid_device, pdev);
	} else if ((!strncmp(hid, "808622C0", 8))) {
		return mid_platform_get_resources_lpio(mid_device, pdev);
	} else {
		pr_err("Invalid device id..\n");
		return -EINVAL;
	}
}

int dma_acpi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	acpi_handle handle = ACPI_HANDLE(dev);
	struct acpi_device *device;
	struct middma_device *mid_device;
	struct intel_mid_dma_probe_info *info;
	const char *hid;
	int ret;
	struct acpi_dma_dev_list *listnode;

	ret = acpi_bus_get_device(handle, &device);
	if (ret) {
		pr_err("%s: could not get acpi device - %d\n", __func__, ret);
		return -ENODEV;
	}

	if (acpi_bus_get_status(device) || !device->status.present) {
		pr_err("%s: device has invalid status", __func__);
		return -ENODEV;
	}

	hid = acpi_device_hid(device);
	pr_info("%s for %s", __func__, hid);

	/* Apply default dma_mask if needed */
	if (!pdev->dev.dma_mask) {
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	}

	ret = dma_set_mask(&pdev->dev, DMA_BIT_MASK(32));
	if (ret) {
		pr_err("dma_set_mask failed with err:%d", ret);
		return ret;
	}

	ret = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));
	if (ret) {
		pr_err("_coherent_mask failed with err:%d", ret);
		return ret;
	}
	info = mid_get_acpi_driver_data(hid);
	if (!info) {
		pr_err("acpi driver data is null");
		goto err_dma;
	}

	mid_device = mid_dma_setup_context(&pdev->dev, info);
	if (!mid_device)
		goto err_dma;

	ret = mid_platform_get_resources(hid, mid_device, pdev);
	if (ret) {
		pr_err("Error while get resources:%d", ret);
		goto err_dma;
	}
	platform_set_drvdata(pdev, mid_device);
	ret = mid_setup_dma(&pdev->dev);
	if (ret)
		goto err_dma;
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_allow(&pdev->dev);

	listnode = devm_kzalloc(&pdev->dev, sizeof(*listnode), GFP_KERNEL);
	if (!listnode) {
		pr_err("dma dev list alloc failed\n");
		ret = -ENOMEM;
		goto err_dma;
	}

	strncpy(listnode->dma_hid, hid, HID_MAX_SIZE);
	listnode->acpi_dma_dev = &pdev->dev;
	list_add_tail(&listnode->dmadev_list, &dma_dev_list);

	pr_debug("%s:completed", __func__);
	return 0;
err_dma:
	pr_err("ERR_MDMA:Probe failed %d\n", ret);
	return ret;
}
#else
int dma_acpi_probe(struct platform_device *pdev)
{
	return -EIO;
}
#endif

int dma_acpi_remove(struct platform_device *pdev)
{
	pm_runtime_forbid(&pdev->dev);
	middma_shutdown(&pdev->dev);
	platform_set_drvdata(pdev, NULL);
	return 0;
}
