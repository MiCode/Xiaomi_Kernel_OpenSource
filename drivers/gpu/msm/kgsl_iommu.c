/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/types.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/genalloc.h>
#include <linux/slab.h>
#include <linux/iommu.h>
#include <linux/msm_kgsl.h>
#include <mach/socinfo.h>

#include "kgsl.h"
#include "kgsl_device.h"
#include "kgsl_mmu.h"
#include "kgsl_sharedmem.h"
#include "kgsl_iommu.h"
#include "adreno_pm4types.h"
#include "adreno.h"
#include "kgsl_trace.h"

static struct kgsl_iommu_register_list kgsl_iommuv1_reg[KGSL_IOMMU_REG_MAX] = {
	{ 0, 0, 0 },				/* GLOBAL_BASE */
	{ 0x10, 0x0003FFFF, 14 },		/* TTBR0 */
	{ 0x14, 0x0003FFFF, 14 },		/* TTBR1 */
	{ 0x20, 0, 0 },				/* FSR */
	{ 0x800, 0, 0 },			/* TLBIALL */
};

static struct kgsl_iommu_register_list kgsl_iommuv2_reg[KGSL_IOMMU_REG_MAX] = {
	{ 0, 0, 0 },				/* GLOBAL_BASE */
	{ 0x20, 0x00FFFFFF, 14 },		/* TTBR0 */
	{ 0x28, 0x00FFFFFF, 14 },		/* TTBR1 */
	{ 0x58, 0, 0 },				/* FSR */
	{ 0x618, 0, 0 }				/* TLBIALL */
};

static int get_iommu_unit(struct device *dev, struct kgsl_mmu **mmu_out,
			struct kgsl_iommu_unit **iommu_unit_out)
{
	int i, j, k;

	for (i = 0; i < KGSL_DEVICE_MAX; i++) {
		struct kgsl_mmu *mmu;
		struct kgsl_iommu *iommu;

		if (kgsl_driver.devp[i] == NULL)
			continue;

		mmu = kgsl_get_mmu(kgsl_driver.devp[i]);
		if (mmu == NULL || mmu->priv == NULL)
			continue;

		iommu = mmu->priv;

		for (j = 0; j < iommu->unit_count; j++) {
			struct kgsl_iommu_unit *iommu_unit =
				&iommu->iommu_units[j];
			for (k = 0; k < iommu_unit->dev_count; k++) {
				if (iommu_unit->dev[k].dev == dev) {
					*mmu_out = mmu;
					*iommu_unit_out = iommu_unit;
					return 0;
				}
			}
		}
	}

	return -EINVAL;
}

static struct kgsl_iommu_device *get_iommu_device(struct kgsl_iommu_unit *unit,
		struct device *dev)
{
	int k;

	for (k = 0; unit && k < unit->dev_count; k++) {
		if (unit->dev[k].dev == dev)
			return &(unit->dev[k]);
	}

	return NULL;
}

static int kgsl_iommu_fault_handler(struct iommu_domain *domain,
	struct device *dev, unsigned long addr, int flags)
{
	int ret = 0;
	struct kgsl_mmu *mmu;
	struct kgsl_iommu *iommu;
	struct kgsl_iommu_unit *iommu_unit;
	struct kgsl_iommu_device *iommu_dev;
	unsigned int ptbase, fsr;

	ret = get_iommu_unit(dev, &mmu, &iommu_unit);
	if (ret)
		goto done;
	iommu_dev = get_iommu_device(iommu_unit, dev);
	if (!iommu_dev) {
		KGSL_CORE_ERR("Invalid IOMMU device %p\n", dev);
		ret = -ENOSYS;
		goto done;
	}
	iommu = mmu->priv;

	ptbase = KGSL_IOMMU_GET_CTX_REG(iommu, iommu_unit,
					iommu_dev->ctx_id, TTBR0);

	fsr = KGSL_IOMMU_GET_CTX_REG(iommu, iommu_unit,
		iommu_dev->ctx_id, FSR);

	KGSL_MEM_CRIT(iommu_dev->kgsldev,
		"GPU PAGE FAULT: addr = %lX pid = %d\n",
		addr, kgsl_mmu_get_ptname_from_ptbase(mmu, ptbase));
	KGSL_MEM_CRIT(iommu_dev->kgsldev, "context = %d FSR = %X\n",
		iommu_dev->ctx_id, fsr);

	trace_kgsl_mmu_pagefault(iommu_dev->kgsldev, addr,
			kgsl_mmu_get_ptname_from_ptbase(mmu, ptbase), 0);

done:
	return ret;
}

/*
 * kgsl_iommu_disable_clk - Disable iommu clocks
 * @mmu - Pointer to mmu structure
 *
 * Disables iommu clocks
 * Return - void
 */
static void kgsl_iommu_disable_clk(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = mmu->priv;
	struct msm_iommu_drvdata *iommu_drvdata;
	int i, j;

	for (i = 0; i < iommu->unit_count; i++) {
		struct kgsl_iommu_unit *iommu_unit = &iommu->iommu_units[i];
		for (j = 0; j < iommu_unit->dev_count; j++) {
			if (!iommu_unit->dev[j].clk_enabled)
				continue;
			iommu_drvdata = dev_get_drvdata(
					iommu_unit->dev[j].dev->parent);
			if (iommu_drvdata->aclk)
				clk_disable_unprepare(iommu_drvdata->aclk);
			if (iommu_drvdata->clk)
				clk_disable_unprepare(iommu_drvdata->clk);
			clk_disable_unprepare(iommu_drvdata->pclk);
			iommu_unit->dev[j].clk_enabled = false;
		}
	}
}

/*
 * kgsl_iommu_disable_clk_event - An event function that is executed when
 * the required timestamp is reached. It disables the IOMMU clocks if
 * the timestamp on which the clocks can be disabled has expired.
 * @device - The kgsl device pointer
 * @data - The data passed during event creation, it is the MMU pointer
 * @id - Context ID, should always be KGSL_MEMSTORE_GLOBAL
 * @ts - The current timestamp that has expired for the device
 *
 * Disables IOMMU clocks if timestamp has expired
 * Return - void
 */
static void kgsl_iommu_clk_disable_event(struct kgsl_device *device, void *data,
					unsigned int id, unsigned int ts)
{
	struct kgsl_mmu *mmu = data;
	struct kgsl_iommu *iommu = mmu->priv;

	if (!iommu->clk_event_queued) {
		if (0 > timestamp_cmp(ts, iommu->iommu_last_cmd_ts))
			KGSL_DRV_ERR(device,
			"IOMMU disable clock event being cancelled, "
			"iommu_last_cmd_ts: %x, retired ts: %x\n",
			iommu->iommu_last_cmd_ts, ts);
		return;
	}

	if (0 <= timestamp_cmp(ts, iommu->iommu_last_cmd_ts)) {
		kgsl_iommu_disable_clk(mmu);
		iommu->clk_event_queued = false;
	} else {
		/* add new event to fire when ts is reached, this can happen
		 * if we queued an event and someone requested the clocks to
		 * be disbaled on a later timestamp */
		if (kgsl_add_event(device, id, iommu->iommu_last_cmd_ts,
			kgsl_iommu_clk_disable_event, mmu, mmu)) {
				KGSL_DRV_ERR(device,
				"Failed to add IOMMU disable clk event\n");
				iommu->clk_event_queued = false;
		}
	}
}

/*
 * kgsl_iommu_disable_clk_on_ts - Sets up event to disable IOMMU clocks
 * @mmu - The kgsl MMU pointer
 * @ts - Timestamp on which the clocks should be disabled
 * @ts_valid - Indicates whether ts parameter is valid, if this parameter
 * is false then it means that the calling function wants to disable the
 * IOMMU clocks immediately without waiting for any timestamp
 *
 * Creates an event to disable the IOMMU clocks on timestamp and if event
 * already exists then updates the timestamp of disabling the IOMMU clocks
 * with the passed in ts if it is greater than the current value at which
 * the clocks will be disabled
 * Return - void
 */
static void
kgsl_iommu_disable_clk_on_ts(struct kgsl_mmu *mmu, unsigned int ts,
				bool ts_valid)
{
	struct kgsl_iommu *iommu = mmu->priv;

	if (iommu->clk_event_queued) {
		if (ts_valid && (0 <
			timestamp_cmp(ts, iommu->iommu_last_cmd_ts)))
			iommu->iommu_last_cmd_ts = ts;
	} else {
		if (ts_valid) {
			iommu->iommu_last_cmd_ts = ts;
			iommu->clk_event_queued = true;
			if (kgsl_add_event(mmu->device, KGSL_MEMSTORE_GLOBAL,
				ts, kgsl_iommu_clk_disable_event, mmu, mmu)) {
				KGSL_DRV_ERR(mmu->device,
				"Failed to add IOMMU disable clk event\n");
				iommu->clk_event_queued = false;
			}
		} else {
			kgsl_iommu_disable_clk(mmu);
		}
	}
}

/*
 * kgsl_iommu_enable_clk - Enable iommu clocks
 * @mmu - Pointer to mmu structure
 * @ctx_id - The context bank whose clocks are to be turned on
 *
 * Enables iommu clocks of a given context
 * Return: 0 on success else error code
 */
static int kgsl_iommu_enable_clk(struct kgsl_mmu *mmu,
				int ctx_id)
{
	int ret = 0;
	int i, j;
	struct kgsl_iommu *iommu = mmu->priv;
	struct msm_iommu_drvdata *iommu_drvdata;

	for (i = 0; i < iommu->unit_count; i++) {
		struct kgsl_iommu_unit *iommu_unit = &iommu->iommu_units[i];
		for (j = 0; j < iommu_unit->dev_count; j++) {
			if (iommu_unit->dev[j].clk_enabled ||
				ctx_id != iommu_unit->dev[j].ctx_id)
				continue;
			iommu_drvdata =
			dev_get_drvdata(iommu_unit->dev[j].dev->parent);
			ret = clk_prepare_enable(iommu_drvdata->pclk);
			if (ret)
				goto done;
			if (iommu_drvdata->clk) {
				ret = clk_prepare_enable(iommu_drvdata->clk);
				if (ret) {
					clk_disable_unprepare(
						iommu_drvdata->pclk);
					goto done;
				}
			}
			if (iommu_drvdata->aclk) {
				ret = clk_prepare_enable(iommu_drvdata->aclk);
				if (ret) {
					if (iommu_drvdata->clk)
						clk_disable_unprepare(
							iommu_drvdata->clk);
					clk_disable_unprepare(
							iommu_drvdata->pclk);
					goto done;
				}
			}
			iommu_unit->dev[j].clk_enabled = true;
		}
	}
done:
	if (ret)
		kgsl_iommu_disable_clk(mmu);
	return ret;
}

/*
 * kgsl_iommu_pt_equal - Check if pagetables are equal
 * @mmu - Pointer to mmu structure
 * @pt - Pointer to pagetable
 * @pt_base - Address of a pagetable that the IOMMU register is
 * programmed with
 *
 * Checks whether the pt_base is equal to the base address of
 * the pagetable which is contained in the pt structure
 * Return - Non-zero if the pagetable addresses are equal else 0
 */
static int kgsl_iommu_pt_equal(struct kgsl_mmu *mmu,
				struct kgsl_pagetable *pt,
				unsigned int pt_base)
{
	struct kgsl_iommu *iommu = mmu->priv;
	struct kgsl_iommu_pt *iommu_pt = pt ? pt->priv : NULL;
	unsigned int domain_ptbase = iommu_pt ?
				iommu_get_pt_base_addr(iommu_pt->domain) : 0;
	/* Only compare the valid address bits of the pt_base */
	domain_ptbase &=
		(iommu->iommu_reg_list[KGSL_IOMMU_CTX_TTBR0].reg_mask <<
		iommu->iommu_reg_list[KGSL_IOMMU_CTX_TTBR0].reg_shift);

	pt_base &=
		(iommu->iommu_reg_list[KGSL_IOMMU_CTX_TTBR0].reg_mask <<
		iommu->iommu_reg_list[KGSL_IOMMU_CTX_TTBR0].reg_shift);

	return domain_ptbase && pt_base &&
		(domain_ptbase == pt_base);
}

/*
 * kgsl_iommu_destroy_pagetable - Free up reaources help by a pagetable
 * @mmu_specific_pt - Pointer to pagetable which is to be freed
 *
 * Return - void
 */
static void kgsl_iommu_destroy_pagetable(void *mmu_specific_pt)
{
	struct kgsl_iommu_pt *iommu_pt = mmu_specific_pt;
	if (iommu_pt->domain)
		iommu_domain_free(iommu_pt->domain);
	kfree(iommu_pt);
}

/*
 * kgsl_iommu_create_pagetable - Create a IOMMU pagetable
 *
 * Allocate memory to hold a pagetable and allocate the IOMMU
 * domain which is the actual IOMMU pagetable
 * Return - void
 */
void *kgsl_iommu_create_pagetable(void)
{
	struct kgsl_iommu_pt *iommu_pt;

	iommu_pt = kzalloc(sizeof(struct kgsl_iommu_pt), GFP_KERNEL);
	if (!iommu_pt) {
		KGSL_CORE_ERR("kzalloc(%d) failed\n",
				sizeof(struct kgsl_iommu_pt));
		return NULL;
	}
	/* L2 redirect is not stable on IOMMU v2 */
	if (msm_soc_version_supports_iommu_v1())
		iommu_pt->domain = iommu_domain_alloc(&platform_bus_type,
					MSM_IOMMU_DOMAIN_PT_CACHEABLE);
	else
		iommu_pt->domain = iommu_domain_alloc(&platform_bus_type,
					0);
	if (!iommu_pt->domain) {
		KGSL_CORE_ERR("Failed to create iommu domain\n");
		kfree(iommu_pt);
		return NULL;
	} else {
		iommu_set_fault_handler(iommu_pt->domain,
			kgsl_iommu_fault_handler);
	}

	return iommu_pt;
}

/*
 * kgsl_detach_pagetable_iommu_domain - Detach the IOMMU unit from a
 * pagetable
 * @mmu - Pointer to the device mmu structure
 * @priv - Flag indicating whether the private or user context is to be
 * detached
 *
 * Detach the IOMMU unit with the domain that is contained in the
 * hwpagetable of the given mmu. After detaching the IOMMU unit is not
 * in use because the PTBR will not be set after a detach
 * Return - void
 */
static void kgsl_detach_pagetable_iommu_domain(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu_pt *iommu_pt;
	struct kgsl_iommu *iommu = mmu->priv;
	int i, j;

	for (i = 0; i < iommu->unit_count; i++) {
		struct kgsl_iommu_unit *iommu_unit = &iommu->iommu_units[i];
		iommu_pt = mmu->defaultpagetable->priv;
		for (j = 0; j < iommu_unit->dev_count; j++) {
			/*
			 * If there is a 2nd default pagetable then priv domain
			 * is attached with this pagetable
			 */
			if (mmu->priv_bank_table &&
				(KGSL_IOMMU_CONTEXT_PRIV == j))
				iommu_pt = mmu->priv_bank_table->priv;
			if (iommu_unit->dev[j].attached) {
				iommu_detach_device(iommu_pt->domain,
						iommu_unit->dev[j].dev);
				iommu_unit->dev[j].attached = false;
				KGSL_MEM_INFO(mmu->device, "iommu %p detached "
					"from user dev of MMU: %p\n",
					iommu_pt->domain, mmu);
			}
		}
	}
}

/*
 * kgsl_attach_pagetable_iommu_domain - Attach the IOMMU unit to a
 * pagetable, i.e set the IOMMU's PTBR to the pagetable address and
 * setup other IOMMU registers for the device so that it becomes
 * active
 * @mmu - Pointer to the device mmu structure
 * @priv - Flag indicating whether the private or user context is to be
 * attached
 *
 * Attach the IOMMU unit with the domain that is contained in the
 * hwpagetable of the given mmu.
 * Return - 0 on success else error code
 */
static int kgsl_attach_pagetable_iommu_domain(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu_pt *iommu_pt;
	struct kgsl_iommu *iommu = mmu->priv;
	int i, j, ret = 0;

	/*
	 * Loop through all the iommu devcies under all iommu units and
	 * attach the domain
	 */
	for (i = 0; i < iommu->unit_count; i++) {
		struct kgsl_iommu_unit *iommu_unit = &iommu->iommu_units[i];
		iommu_pt = mmu->defaultpagetable->priv;
		for (j = 0; j < iommu_unit->dev_count; j++) {
			/*
			 * If there is a 2nd default pagetable then priv domain
			 * is attached to this pagetable
			 */
			if (mmu->priv_bank_table &&
				(KGSL_IOMMU_CONTEXT_PRIV == j))
				iommu_pt = mmu->priv_bank_table->priv;
			if (!iommu_unit->dev[j].attached) {
				ret = iommu_attach_device(iommu_pt->domain,
							iommu_unit->dev[j].dev);
				if (ret) {
					KGSL_MEM_ERR(mmu->device,
						"Failed to attach device, err %d\n",
						ret);
					goto done;
				}
				iommu_unit->dev[j].attached = true;
				KGSL_MEM_INFO(mmu->device,
				"iommu pt %p attached to dev %p, ctx_id %d\n",
				iommu_pt->domain, iommu_unit->dev[j].dev,
				iommu_unit->dev[j].ctx_id);
			}
		}
	}
done:
	return ret;
}

/*
 * _get_iommu_ctxs - Get device pointer to IOMMU contexts
 * @mmu - Pointer to mmu device
 * data - Pointer to the platform data containing information about
 * iommu devices for one iommu unit
 * unit_id - The IOMMU unit number. This is not a specific ID but just
 * a serial number. The serial numbers are treated as ID's of the
 * IOMMU units
 *
 * Return - 0 on success else error code
 */
static int _get_iommu_ctxs(struct kgsl_mmu *mmu,
	struct kgsl_device_iommu_data *data, unsigned int unit_id)
{
	struct kgsl_iommu *iommu = mmu->priv;
	struct kgsl_iommu_unit *iommu_unit = &iommu->iommu_units[unit_id];
	int i;

	if (data->iommu_ctx_count > KGSL_IOMMU_MAX_DEVS_PER_UNIT) {
		KGSL_CORE_ERR("Too many iommu devices defined for an "
				"IOMMU unit\n");
		return -EINVAL;
	}

	for (i = 0; i < data->iommu_ctx_count; i++) {
		if (!data->iommu_ctxs[i].iommu_ctx_name)
			continue;

		iommu_unit->dev[iommu_unit->dev_count].dev =
			msm_iommu_get_ctx(data->iommu_ctxs[i].iommu_ctx_name);
		if (iommu_unit->dev[iommu_unit->dev_count].dev == NULL) {
			KGSL_CORE_ERR("Failed to get iommu dev handle for "
			"device %s\n", data->iommu_ctxs[i].iommu_ctx_name);
			return -EINVAL;
		}
		if (KGSL_IOMMU_CONTEXT_USER != data->iommu_ctxs[i].ctx_id &&
			KGSL_IOMMU_CONTEXT_PRIV != data->iommu_ctxs[i].ctx_id) {
			KGSL_CORE_ERR("Invalid context ID defined: %d\n",
					data->iommu_ctxs[i].ctx_id);
			return -EINVAL;
		}
		iommu_unit->dev[iommu_unit->dev_count].ctx_id =
						data->iommu_ctxs[i].ctx_id;
		iommu_unit->dev[iommu_unit->dev_count].kgsldev = mmu->device;

		KGSL_DRV_INFO(mmu->device,
				"Obtained dev handle %p for iommu context %s\n",
				iommu_unit->dev[iommu_unit->dev_count].dev,
				data->iommu_ctxs[i].iommu_ctx_name);

		iommu_unit->dev_count++;
	}

	return 0;
}

/*
 * kgsl_get_iommu_ctxt - Get device pointer to IOMMU contexts
 * @mmu - Pointer to mmu device
 *
 * Get the device pointers for the IOMMU user and priv contexts of the
 * kgsl device
 * Return - 0 on success else error code
 */
static int kgsl_get_iommu_ctxt(struct kgsl_mmu *mmu)
{
	struct platform_device *pdev =
		container_of(mmu->device->parentdev, struct platform_device,
				dev);
	struct kgsl_device_platform_data *pdata_dev = pdev->dev.platform_data;
	struct kgsl_iommu *iommu = mmu->device->mmu.priv;
	int i, ret = 0;

	/* Go through the IOMMU data and get all the context devices */
	if (KGSL_IOMMU_MAX_UNITS < pdata_dev->iommu_count) {
		KGSL_CORE_ERR("Too many IOMMU units defined\n");
		ret = -EINVAL;
		goto  done;
	}

	for (i = 0; i < pdata_dev->iommu_count; i++) {
		ret = _get_iommu_ctxs(mmu, &pdata_dev->iommu_data[i], i);
		if (ret)
			break;
	}
	iommu->unit_count = pdata_dev->iommu_count;
done:
	return ret;
}

/*
 * kgsl_set_register_map - Map the IOMMU regsiters in the memory descriptors
 * of the respective iommu units
 * @mmu - Pointer to mmu structure
 *
 * Return - 0 on success else error code
 */
static int kgsl_set_register_map(struct kgsl_mmu *mmu)
{
	struct platform_device *pdev =
		container_of(mmu->device->parentdev, struct platform_device,
				dev);
	struct kgsl_device_platform_data *pdata_dev = pdev->dev.platform_data;
	struct kgsl_iommu *iommu = mmu->device->mmu.priv;
	struct kgsl_iommu_unit *iommu_unit;
	int i = 0, ret = 0;

	for (; i < pdata_dev->iommu_count; i++) {
		struct kgsl_device_iommu_data data = pdata_dev->iommu_data[i];
		iommu_unit = &iommu->iommu_units[i];
		/* set up the IOMMU register map for the given IOMMU unit */
		if (!data.physstart || !data.physend) {
			KGSL_CORE_ERR("The register range for IOMMU unit not"
					" specified\n");
			ret = -EINVAL;
			goto err;
		}
		iommu_unit->reg_map.hostptr = ioremap(data.physstart,
					data.physend - data.physstart + 1);
		if (!iommu_unit->reg_map.hostptr) {
			KGSL_CORE_ERR("Failed to map SMMU register address "
				"space from %x to %x\n", data.physstart,
				data.physend - data.physstart + 1);
			ret = -ENOMEM;
			i--;
			goto err;
		}
		iommu_unit->reg_map.size = data.physend - data.physstart + 1;
		iommu_unit->reg_map.physaddr = data.physstart;
		ret = memdesc_sg_phys(&iommu_unit->reg_map, data.physstart,
				iommu_unit->reg_map.size);
		if (ret)
			goto err;
	}
	iommu->unit_count = pdata_dev->iommu_count;
	return ret;
err:
	/* Unmap any mapped IOMMU regions */
	for (; i >= 0; i--) {
		iommu_unit = &iommu->iommu_units[i];
		iounmap(iommu_unit->reg_map.hostptr);
		iommu_unit->reg_map.size = 0;
		iommu_unit->reg_map.physaddr = 0;
	}
	return ret;
}

/*
 * kgsl_iommu_get_pt_base_addr - Get the address of the pagetable that the
 * IOMMU ttbr0 register is programmed with
 * @mmu - Pointer to mmu
 * @pt - kgsl pagetable pointer that contains the IOMMU domain pointer
 *
 * Return - actual pagetable address that the ttbr0 register is programmed
 * with
 */
static unsigned int kgsl_iommu_get_pt_base_addr(struct kgsl_mmu *mmu,
						struct kgsl_pagetable *pt)
{
	struct kgsl_iommu *iommu = mmu->priv;
	struct kgsl_iommu_pt *iommu_pt = pt->priv;
	return iommu_get_pt_base_addr(iommu_pt->domain) &
			(iommu->iommu_reg_list[KGSL_IOMMU_CTX_TTBR0].reg_mask <<
			iommu->iommu_reg_list[KGSL_IOMMU_CTX_TTBR0].reg_shift);
}

/*
 * kgsl_iommu_get_pt_lsb - Return the lsb of the ttbr0 IOMMU register
 * @mmu - Pointer to mmu structure
 * @hostptr - Pointer to the IOMMU register map. This is used to match
 * the iommu device whose lsb value is to be returned
 * @ctx_id - The context bank whose lsb valus is to be returned
 * Return - returns the lsb which is the last 14 bits of the ttbr0 IOMMU
 * register. ttbr0 is the actual PTBR for of the IOMMU. The last 14 bits
 * are only programmed once in the beginning when a domain is attached
 * does not change.
 */
static int kgsl_iommu_get_pt_lsb(struct kgsl_mmu *mmu,
				unsigned int unit_id,
				enum kgsl_iommu_context_id ctx_id)
{
	struct kgsl_iommu *iommu = mmu->priv;
	int i, j;
	for (i = 0; i < iommu->unit_count; i++) {
		struct kgsl_iommu_unit *iommu_unit = &iommu->iommu_units[i];
		for (j = 0; j < iommu_unit->dev_count; j++)
			if (unit_id == i &&
				ctx_id == iommu_unit->dev[j].ctx_id)
				return iommu_unit->dev[j].pt_lsb;
	}
	return 0;
}

static void kgsl_iommu_setstate(struct kgsl_mmu *mmu,
				struct kgsl_pagetable *pagetable,
				unsigned int context_id)
{
	if (mmu->flags & KGSL_FLAGS_STARTED) {
		/* page table not current, then setup mmu to use new
		 *  specified page table
		 */
		if (mmu->hwpagetable != pagetable) {
			unsigned int flags = 0;
			mmu->hwpagetable = pagetable;
			flags |= kgsl_mmu_pt_get_flags(mmu->hwpagetable,
							mmu->device->id) |
							KGSL_MMUFLAGS_TLBFLUSH;
			kgsl_setstate(mmu, context_id,
				KGSL_MMUFLAGS_PTUPDATE | flags);
		}
	}
}

static int kgsl_iommu_init(struct kgsl_mmu *mmu)
{
	/*
	 * intialize device mmu
	 *
	 * call this with the global lock held
	 */
	int status = 0;
	struct kgsl_iommu *iommu;

	iommu = kzalloc(sizeof(struct kgsl_iommu), GFP_KERNEL);
	if (!iommu) {
		KGSL_CORE_ERR("kzalloc(%d) failed\n",
				sizeof(struct kgsl_iommu));
		return -ENOMEM;
	}

	mmu->priv = iommu;
	status = kgsl_get_iommu_ctxt(mmu);
	if (status)
		goto done;
	status = kgsl_set_register_map(mmu);
	if (status)
		goto done;

	iommu->iommu_reg_list = kgsl_iommuv1_reg;
	iommu->ctx_offset = KGSL_IOMMU_CTX_OFFSET_V1;

	if (msm_soc_version_supports_iommu_v1()) {
		iommu->iommu_reg_list = kgsl_iommuv1_reg;
		iommu->ctx_offset = KGSL_IOMMU_CTX_OFFSET_V1;
	} else {
		iommu->iommu_reg_list = kgsl_iommuv2_reg;
		iommu->ctx_offset = KGSL_IOMMU_CTX_OFFSET_V2;
	}

	/* A nop is required in an indirect buffer when switching
	 * pagetables in-stream */
	kgsl_sharedmem_writel(&mmu->setstate_memory,
				KGSL_IOMMU_SETSTATE_NOP_OFFSET,
				cp_nop_packet(1));

	dev_info(mmu->device->dev, "|%s| MMU type set for device is IOMMU\n",
			__func__);
done:
	if (status) {
		kfree(iommu);
		mmu->priv = NULL;
	}
	return status;
}

/*
 * kgsl_iommu_setup_defaultpagetable - Setup the initial defualtpagetable
 * for iommu. This function is only called once during first start, successive
 * start do not call this funciton.
 * @mmu - Pointer to mmu structure
 *
 * Create the  initial defaultpagetable and setup the iommu mappings to it
 * Return - 0 on success else error code
 */
static int kgsl_iommu_setup_defaultpagetable(struct kgsl_mmu *mmu)
{
	int status = 0;
	int i = 0;
	struct kgsl_iommu *iommu = mmu->priv;
	struct kgsl_pagetable *pagetable = NULL;

	/* If chip is not 8960 then we use the 2nd context bank for pagetable
	 * switching on the 3D side for which a separate table is allocated */
	if (!cpu_is_msm8960() && msm_soc_version_supports_iommu_v1()) {
		mmu->priv_bank_table =
			kgsl_mmu_getpagetable(KGSL_MMU_PRIV_BANK_TABLE_NAME);
		if (mmu->priv_bank_table == NULL) {
			status = -ENOMEM;
			goto err;
		}
	}
	mmu->defaultpagetable = kgsl_mmu_getpagetable(KGSL_MMU_GLOBAL_PT);
	/* Return error if the default pagetable doesn't exist */
	if (mmu->defaultpagetable == NULL) {
		status = -ENOMEM;
		goto err;
	}
	pagetable = mmu->priv_bank_table ? mmu->priv_bank_table :
				mmu->defaultpagetable;
	/* Map the IOMMU regsiters to only defaultpagetable */
	if (msm_soc_version_supports_iommu_v1()) {
		for (i = 0; i < iommu->unit_count; i++) {
			iommu->iommu_units[i].reg_map.priv |=
						KGSL_MEMFLAGS_GLOBAL;
			status = kgsl_mmu_map(pagetable,
				&(iommu->iommu_units[i].reg_map),
				GSL_PT_PAGE_RV | GSL_PT_PAGE_WV);
			if (status) {
				iommu->iommu_units[i].reg_map.priv &=
							~KGSL_MEMFLAGS_GLOBAL;
				goto err;
			}
		}
	}
	return status;
err:
	for (i--; i >= 0; i--) {
		kgsl_mmu_unmap(pagetable,
				&(iommu->iommu_units[i].reg_map));
		iommu->iommu_units[i].reg_map.priv &= ~KGSL_MEMFLAGS_GLOBAL;
	}
	if (mmu->priv_bank_table) {
		kgsl_mmu_putpagetable(mmu->priv_bank_table);
		mmu->priv_bank_table = NULL;
	}
	if (mmu->defaultpagetable) {
		kgsl_mmu_putpagetable(mmu->defaultpagetable);
		mmu->defaultpagetable = NULL;
	}
	return status;
}

static int kgsl_iommu_start(struct kgsl_mmu *mmu)
{
	int status;
	struct kgsl_iommu *iommu = mmu->priv;
	int i, j;

	if (mmu->flags & KGSL_FLAGS_STARTED)
		return 0;

	if (mmu->defaultpagetable == NULL) {
		status = kgsl_iommu_setup_defaultpagetable(mmu);
		if (status)
			return -ENOMEM;
	}
	/* We use the GPU MMU to control access to IOMMU registers on 8960 with
	 * a225, hence we still keep the MMU active on 8960 */
	if (cpu_is_msm8960()) {
		struct kgsl_mh *mh = &(mmu->device->mh);
		kgsl_regwrite(mmu->device, MH_MMU_CONFIG, 0x00000001);
		kgsl_regwrite(mmu->device, MH_MMU_MPU_END,
			mh->mpu_base +
			iommu->iommu_units[0].reg_map.gpuaddr);
	} else {
		kgsl_regwrite(mmu->device, MH_MMU_CONFIG, 0x00000000);
	}

	mmu->hwpagetable = mmu->defaultpagetable;

	status = kgsl_attach_pagetable_iommu_domain(mmu);
	if (status) {
		mmu->hwpagetable = NULL;
		goto done;
	}
	status = kgsl_iommu_enable_clk(mmu, KGSL_IOMMU_CONTEXT_USER);
	if (status) {
		KGSL_CORE_ERR("clk enable failed\n");
		goto done;
	}
	status = kgsl_iommu_enable_clk(mmu, KGSL_IOMMU_CONTEXT_PRIV);
	if (status) {
		KGSL_CORE_ERR("clk enable failed\n");
		goto done;
	}
	/* Get the lsb value of pagetables set in the IOMMU ttbr0 register as
	 * that value should not change when we change pagetables, so while
	 * changing pagetables we can use this lsb value of the pagetable w/o
	 * having to read it again
	 */
	for (i = 0; i < iommu->unit_count; i++) {
		struct kgsl_iommu_unit *iommu_unit = &iommu->iommu_units[i];
		for (j = 0; j < iommu_unit->dev_count; j++)
			iommu_unit->dev[j].pt_lsb = KGSL_IOMMMU_PT_LSB(iommu,
						KGSL_IOMMU_GET_CTX_REG(iommu,
						iommu_unit,
						iommu_unit->dev[j].ctx_id,
						TTBR0));
	}

	kgsl_iommu_disable_clk_on_ts(mmu, 0, false);
	mmu->flags |= KGSL_FLAGS_STARTED;

done:
	if (status) {
		kgsl_iommu_disable_clk_on_ts(mmu, 0, false);
		kgsl_detach_pagetable_iommu_domain(mmu);
	}
	return status;
}

static int
kgsl_iommu_unmap(void *mmu_specific_pt,
		struct kgsl_memdesc *memdesc,
		unsigned int *tlb_flags)
{
	int ret;
	unsigned int range = kgsl_sg_size(memdesc->sg, memdesc->sglen);
	struct kgsl_iommu_pt *iommu_pt = mmu_specific_pt;

	/* All GPU addresses as assigned are page aligned, but some
	   functions purturb the gpuaddr with an offset, so apply the
	   mask here to make sure we have the right address */

	unsigned int gpuaddr = memdesc->gpuaddr &  KGSL_MMU_ALIGN_MASK;

	if (range == 0 || gpuaddr == 0)
		return 0;

	ret = iommu_unmap_range(iommu_pt->domain, gpuaddr, range);
	if (ret)
		KGSL_CORE_ERR("iommu_unmap_range(%p, %x, %d) failed "
			"with err: %d\n", iommu_pt->domain, gpuaddr,
			range, ret);

#ifdef CONFIG_KGSL_PER_PROCESS_PAGE_TABLE
	/*
	 * Flushing only required if per process pagetables are used. With
	 * global case, flushing will happen inside iommu_map function
	 */
	if (!ret && msm_soc_version_supports_iommu_v1())
		*tlb_flags = UINT_MAX;
#endif
	return 0;
}

static int
kgsl_iommu_map(void *mmu_specific_pt,
			struct kgsl_memdesc *memdesc,
			unsigned int protflags,
			unsigned int *tlb_flags)
{
	int ret;
	unsigned int iommu_virt_addr;
	struct kgsl_iommu_pt *iommu_pt = mmu_specific_pt;
	int size = kgsl_sg_size(memdesc->sg, memdesc->sglen);

	BUG_ON(NULL == iommu_pt);


	iommu_virt_addr = memdesc->gpuaddr;

	ret = iommu_map_range(iommu_pt->domain, iommu_virt_addr, memdesc->sg,
				size, (IOMMU_READ | IOMMU_WRITE));
	if (ret) {
		KGSL_CORE_ERR("iommu_map_range(%p, %x, %p, %d, %d) "
				"failed with err: %d\n", iommu_pt->domain,
				iommu_virt_addr, memdesc->sg, size,
				(IOMMU_READ | IOMMU_WRITE), ret);
		return ret;
	}

	return ret;
}

static void kgsl_iommu_stop(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = mmu->priv;
	/*
	 *  stop device mmu
	 *
	 *  call this with the global lock held
	 */

	if (mmu->flags & KGSL_FLAGS_STARTED) {
		/* detach iommu attachment */
		kgsl_detach_pagetable_iommu_domain(mmu);
		mmu->hwpagetable = NULL;

		mmu->flags &= ~KGSL_FLAGS_STARTED;
	}

	/* switch off MMU clocks and cancel any events it has queued */
	iommu->clk_event_queued = false;
	kgsl_cancel_events(mmu->device, mmu);
	kgsl_iommu_disable_clk(mmu);
}

static int kgsl_iommu_close(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = mmu->priv;
	int i;
	for (i = 0; i < iommu->unit_count; i++) {
		struct kgsl_pagetable *pagetable = (mmu->priv_bank_table ?
			mmu->priv_bank_table : mmu->defaultpagetable);
		if (iommu->iommu_units[i].reg_map.gpuaddr)
			kgsl_mmu_unmap(pagetable,
			&(iommu->iommu_units[i].reg_map));
		if (iommu->iommu_units[i].reg_map.hostptr)
			iounmap(iommu->iommu_units[i].reg_map.hostptr);
		kgsl_sg_free(iommu->iommu_units[i].reg_map.sg,
				iommu->iommu_units[i].reg_map.sglen);
	}

	if (mmu->priv_bank_table)
		kgsl_mmu_putpagetable(mmu->priv_bank_table);
	if (mmu->defaultpagetable)
		kgsl_mmu_putpagetable(mmu->defaultpagetable);
	kfree(iommu);

	return 0;
}

static unsigned int
kgsl_iommu_get_current_ptbase(struct kgsl_mmu *mmu)
{
	unsigned int pt_base;
	struct kgsl_iommu *iommu = mmu->priv;
	/* We cannot enable or disable the clocks in interrupt context, this
	 function is called from interrupt context if there is an axi error */
	if (in_interrupt())
		return 0;
	/* Return the current pt base by reading IOMMU pt_base register */
	kgsl_iommu_enable_clk(mmu, KGSL_IOMMU_CONTEXT_USER);
	pt_base = KGSL_IOMMU_GET_CTX_REG(iommu, (&iommu->iommu_units[0]),
					KGSL_IOMMU_CONTEXT_USER,
					TTBR0);
	kgsl_iommu_disable_clk_on_ts(mmu, 0, false);
	return pt_base &
		(iommu->iommu_reg_list[KGSL_IOMMU_CTX_TTBR0].reg_mask <<
		iommu->iommu_reg_list[KGSL_IOMMU_CTX_TTBR0].reg_shift);
}

/*
 * kgsl_iommu_default_setstate - Change the IOMMU pagetable or flush IOMMU tlb
 * of the primary context bank
 * @mmu - Pointer to mmu structure
 * @flags - Flags indicating whether pagetable has to chnage or tlb is to be
 * flushed or both
 *
 * Based on flags set the new pagetable fo the IOMMU unit or flush it's tlb or
 * do both by doing direct register writes to the IOMMu registers through the
 * cpu
 * Return - void
 */
static void kgsl_iommu_default_setstate(struct kgsl_mmu *mmu,
					uint32_t flags)
{
	struct kgsl_iommu *iommu = mmu->priv;
	int temp;
	int i;
	unsigned int pt_base = kgsl_iommu_get_pt_base_addr(mmu,
						mmu->hwpagetable);
	unsigned int pt_val;

	if (kgsl_iommu_enable_clk(mmu, KGSL_IOMMU_CONTEXT_USER)) {
		KGSL_DRV_ERR(mmu->device, "Failed to enable iommu clocks\n");
		return;
	}
	/* Mask off the lsb of the pt base address since lsb will not change */
	pt_base &= (iommu->iommu_reg_list[KGSL_IOMMU_CTX_TTBR0].reg_mask <<
			iommu->iommu_reg_list[KGSL_IOMMU_CTX_TTBR0].reg_shift);

	if (flags & KGSL_MMUFLAGS_PTUPDATE) {
		kgsl_idle(mmu->device);
		for (i = 0; i < iommu->unit_count; i++) {
			/* get the lsb value which should not change when
			 * changing ttbr0 */
			pt_val = kgsl_iommu_get_pt_lsb(mmu, i,
						KGSL_IOMMU_CONTEXT_USER);
			pt_val += pt_base;

			KGSL_IOMMU_SET_CTX_REG(iommu, (&iommu->iommu_units[i]),
				KGSL_IOMMU_CONTEXT_USER, TTBR0, pt_val);

			mb();
			temp = KGSL_IOMMU_GET_CTX_REG(iommu,
				(&iommu->iommu_units[i]),
				KGSL_IOMMU_CONTEXT_USER, TTBR0);
		}
	}
	/* Flush tlb */
	if (flags & KGSL_MMUFLAGS_TLBFLUSH) {
		for (i = 0; i < iommu->unit_count; i++) {
			KGSL_IOMMU_SET_CTX_REG(iommu, (&iommu->iommu_units[i]),
				KGSL_IOMMU_CONTEXT_USER, TLBIALL, 1);
			mb();
		}
	}
	/* Disable smmu clock */
	kgsl_iommu_disable_clk_on_ts(mmu, 0, false);
}

/*
 * kgsl_iommu_get_reg_gpuaddr - Returns the gpu address of IOMMU regsiter
 * @mmu - Pointer to mmu structure
 * @iommu_unit - The iommu unit for which base address is requested
 * @ctx_id - The context ID of the IOMMU ctx
 * @reg - The register for which address is required
 *
 * Return - The number of iommu units which is also the number of register
 * mapped descriptor arrays which the out parameter will have
 */
static unsigned int kgsl_iommu_get_reg_gpuaddr(struct kgsl_mmu *mmu,
					int iommu_unit, int ctx_id, int reg)
{
	struct kgsl_iommu *iommu = mmu->priv;

	if (KGSL_IOMMU_GLOBAL_BASE == reg)
		return iommu->iommu_units[iommu_unit].reg_map.gpuaddr;
	else
		return iommu->iommu_units[iommu_unit].reg_map.gpuaddr +
			iommu->iommu_reg_list[reg].reg_offset +
			(ctx_id << KGSL_IOMMU_CTX_SHIFT) + iommu->ctx_offset;
}

static int kgsl_iommu_get_num_iommu_units(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = mmu->priv;
	return iommu->unit_count;
}

struct kgsl_mmu_ops iommu_ops = {
	.mmu_init = kgsl_iommu_init,
	.mmu_close = kgsl_iommu_close,
	.mmu_start = kgsl_iommu_start,
	.mmu_stop = kgsl_iommu_stop,
	.mmu_setstate = kgsl_iommu_setstate,
	.mmu_device_setstate = kgsl_iommu_default_setstate,
	.mmu_pagefault = NULL,
	.mmu_get_current_ptbase = kgsl_iommu_get_current_ptbase,
	.mmu_enable_clk = kgsl_iommu_enable_clk,
	.mmu_disable_clk_on_ts = kgsl_iommu_disable_clk_on_ts,
	.mmu_get_pt_lsb = kgsl_iommu_get_pt_lsb,
	.mmu_get_reg_gpuaddr = kgsl_iommu_get_reg_gpuaddr,
	.mmu_get_num_iommu_units = kgsl_iommu_get_num_iommu_units,
	.mmu_pt_equal = kgsl_iommu_pt_equal,
	.mmu_get_pt_base_addr = kgsl_iommu_get_pt_base_addr,
};

struct kgsl_mmu_pt_ops iommu_pt_ops = {
	.mmu_map = kgsl_iommu_map,
	.mmu_unmap = kgsl_iommu_unmap,
	.mmu_create_pagetable = kgsl_iommu_create_pagetable,
	.mmu_destroy_pagetable = kgsl_iommu_destroy_pagetable,
};
