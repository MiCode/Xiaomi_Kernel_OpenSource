/*
 *
 * Intel Management Engine Interface (Intel MEI) Linux driver
 * Copyright (c) 2003-2012, Intel Corporation.
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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/aio.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/sched.h>
#include <linux/compat.h>
#include <linux/jiffies.h>
#include <linux/dma-mapping.h>
#include <linux/sizes.h>

#include <linux/mei.h>
#include <linux/acpi.h>
#include <acpi/acpi_bus.h>

#include "mei_dev.h"
#include "hw-txe.h"

static acpi_status txei_walk_resource(struct acpi_resource *res, void *data)
{
	struct mei_device *dev = (struct mei_device *)data;
	struct mei_txe_hw *hw = to_txe_hw(dev);
	struct acpi_resource_fixed_memory32 *fixmem32;

	if (res->type != ACPI_RESOURCE_TYPE_FIXED_MEMORY32)
		return AE_OK;

	fixmem32 = &res->data.fixed_memory32;

	dev_dbg(&dev->pdev->dev, "TXE8086 MEMORY32 addr 0x%x len %d\n",
		fixmem32->address, fixmem32->address_length);

	if (!fixmem32->address || !fixmem32->address_length) {
		dev_err(&dev->pdev->dev, "TXE8086 MEMORY32 addr 0x%x len %d\n",
			fixmem32->address, fixmem32->address_length);
		return AE_NO_MEMORY;
	}

	hw->pool_paddr = fixmem32->address;
	hw->pool_size  = fixmem32->address_length;

	return AE_OK;
}

static void mei_release_dma_acpi(struct mei_txe_hw *hw)
{
	if (hw->pool_vaddr)
		iounmap(hw->pool_vaddr);

	hw->pool_vaddr = NULL;
	hw->pool_paddr = 0;
	hw->pool_size  = 0;
}

static int mei_reserver_dma_acpi(struct mei_device *dev)
{
	struct mei_txe_hw *hw = to_txe_hw(dev);
	acpi_handle handle;
	acpi_status ret;

	handle = ACPI_HANDLE(&dev->pdev->dev);
	if (!handle) {
		dev_err(&dev->pdev->dev, "TXE8086 acpi NULL handle received\n");
		return -ENODEV;
	}

	dev_dbg(&dev->pdev->dev, "TXE8086 acpi handle received\n");
	ret = acpi_walk_resources(handle, METHOD_NAME__CRS,
				txei_walk_resource, dev);

	if (ACPI_FAILURE(ret)) {
		dev_err(&dev->pdev->dev, "TXE8086: acpi_walk_resources FAILURE\n");
		return -ENOMEM;
	}

	if (!hw->pool_size) {
		dev_err(&dev->pdev->dev, "TXE8086: acpi __CRS resource not found\n");
		return -ENOMEM;
	}

	dev_dbg(&dev->pdev->dev, "DMA Memory reserved memory usage: size=%zd\n",
		hw->pool_size);

	/* limit the pool_size to SATT_RANGE_MAX */
	hw->pool_size = min_t(size_t, hw->pool_size, SATT_RANGE_MAX);

	hw->pool_vaddr = ioremap(hw->pool_paddr, hw->pool_size);
	/* FIXME: is this fatal ? */
	if (!hw->pool_vaddr)
		dev_err(&dev->pdev->dev, "TXE8086: acpi __CRS cannot remap\n");

	hw->pool_release = mei_release_dma_acpi;

	return 0;
}


static void mei_free_dma(struct  mei_txe_hw *hw)
{
	struct mei_device *dev = hw_txe_to_mei(hw);
	if (hw->pool_vaddr)
		dma_free_coherent(&dev->pdev->dev,
			hw->pool_size, hw->pool_vaddr, hw->pool_paddr);
	hw->pool_vaddr = NULL;
	hw->pool_paddr = 0;
	hw->pool_size  = 0;
}

static int mei_alloc_dma(struct mei_device *dev)
{
	struct mei_txe_hw *hw = to_txe_hw(dev);
	hw->pool_size = SZ_4M;
	dev_dbg(&dev->pdev->dev, "MEIMM: dma size %zd\n", hw->pool_size);

	if (hw->pool_size == 0)
		return 0;

	/*  Limit pools size to satt max range */
	hw->pool_size = min_t(size_t, hw->pool_size, SATT_RANGE_MAX);

	hw->pool_vaddr = dma_alloc_coherent(&dev->pdev->dev, hw->pool_size,
		&hw->pool_paddr, GFP_KERNEL);

	dev_dbg(&dev->pdev->dev, "DMA Memory Allocated vaddr=%p, paddr=%lu, size=%zd\n",
			hw->pool_vaddr,
			(unsigned long)hw->pool_paddr, hw->pool_size);

	if (!hw->pool_vaddr) {
		dev_err(&dev->pdev->dev, "DMA Memory Allocation failed for size %zd\n",
			hw->pool_size);
		return -ENOMEM;
	}

	if (hw->pool_paddr & ~DMA_BIT_MASK(36)) {
		dev_err(&dev->pdev->dev, "Phys Address is beyond DMA_MASK(32) 0x%0lX\n",
			(unsigned long)hw->pool_paddr);
	}

	hw->pool_release = mei_free_dma;

	return 0;
}

int mei_txe_dma_setup(struct mei_device *dev)
{
	struct mei_txe_hw *hw = to_txe_hw(dev);
	int err;

	err = mei_reserver_dma_acpi(dev);
	if (err)
		err = mei_alloc_dma(dev);

	if (err)
		return err;

	err = mei_txe_setup_satt2(dev,
		dma_to_phys(&dev->pdev->dev, hw->pool_paddr), hw->pool_size);

	if (err) {
		if (hw->pool_release)
			hw->pool_release(hw);
		return err;
	}

	hw->mdev = mei_mm_init(&dev->pdev->dev,
		hw->pool_vaddr, hw->pool_paddr, hw->pool_size);

	if (IS_ERR_OR_NULL(hw->mdev))
		return PTR_ERR(hw->mdev);

	return 0;
}

void mei_txe_dma_unset(struct mei_device *dev)
{
	struct mei_txe_hw *hw = to_txe_hw(dev);

	mei_mm_deinit(hw->mdev);

	/* FIXME: do we need to unset satt2 ? */
	if (hw->pool_release)
		hw->pool_release(hw);
}
