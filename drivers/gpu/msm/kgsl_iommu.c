/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
#include <mach/iommu.h>
#include <linux/msm_kgsl.h>

#include "kgsl.h"
#include "kgsl_device.h"
#include "kgsl_mmu.h"
#include "kgsl_sharedmem.h"

/*
 * On APQ8064, KGSL can control a maximum of 4 IOMMU devices: 2 user and 2
 * priv domains, 1 each for each of the AXI ports attached to the GPU.  8660
 * and 8960 have only one AXI port, so maximum allowable IOMMU devices for those
 * chips is 2.
 */

#define KGSL_IOMMU_MAX_DEV 4

struct kgsl_iommu_device {
	struct device *dev;
	int attached;
};

struct kgsl_iommu {
	struct kgsl_iommu_device dev[KGSL_IOMMU_MAX_DEV];
	int dev_count;
};

static int kgsl_iommu_pt_equal(struct kgsl_pagetable *pt,
					unsigned int pt_base)
{
	struct iommu_domain *domain = pt ? pt->priv : NULL;
	return domain && pt_base && ((unsigned int)domain == pt_base);
}

static void kgsl_iommu_destroy_pagetable(void *mmu_specific_pt)
{
	struct iommu_domain *domain = mmu_specific_pt;
	if (domain)
		iommu_domain_free(domain);
}

void *kgsl_iommu_create_pagetable(void)
{
	struct iommu_domain *domain = iommu_domain_alloc(0);
	if (!domain)
		KGSL_CORE_ERR("Failed to create iommu domain\n");

	return domain;
}

static void kgsl_detach_pagetable_iommu_domain(struct kgsl_mmu *mmu)
{
	struct iommu_domain *domain;
	struct kgsl_iommu *iommu = mmu->priv;
	int i;

	BUG_ON(mmu->hwpagetable == NULL);
	BUG_ON(mmu->hwpagetable->priv == NULL);

	domain = mmu->hwpagetable->priv;

	for (i = 0; i < iommu->dev_count; i++) {
		iommu_detach_device(domain, iommu->dev[i].dev);
		iommu->dev[i].attached = 0;
		KGSL_MEM_INFO(mmu->device,
			"iommu %p detached from user dev of MMU: %p\n",
			domain, mmu);
	}
}

static int kgsl_attach_pagetable_iommu_domain(struct kgsl_mmu *mmu)
{
	struct iommu_domain *domain;
	struct kgsl_iommu *iommu = mmu->priv;
	int i, ret = 0;

	BUG_ON(mmu->hwpagetable == NULL);
	BUG_ON(mmu->hwpagetable->priv == NULL);

	domain = mmu->hwpagetable->priv;

	for (i = 0; i < iommu->dev_count; i++) {
		if (iommu->dev[i].attached == 0) {
			ret = iommu_attach_device(domain, iommu->dev[i].dev);
			if (ret) {
				KGSL_MEM_ERR(mmu->device,
					"Failed to attach device, err %d\n",
						ret);
				goto done;
			}

			iommu->dev[i].attached = 1;
			KGSL_MEM_INFO(mmu->device,
				"iommu %p detached from user dev of MMU: %p\n",
				domain, mmu);
		}
	}

done:
	return ret;
}

static int _get_iommu_ctxs(struct kgsl_iommu *iommu, struct kgsl_device *device,
	struct kgsl_device_iommu_data *data)
{
	int i;

	for (i = 0; i < data->iommu_ctx_count; i++) {
		if (iommu->dev_count >= KGSL_IOMMU_MAX_DEV) {
			KGSL_CORE_ERR("Tried to attach too many IOMMU "
				"devices\n");
			return -ENOMEM;
		}

		if (!data->iommu_ctx_names[i])
			continue;

		iommu->dev[iommu->dev_count].dev =
			msm_iommu_get_ctx(data->iommu_ctx_names[i]);
		if (iommu->dev[iommu->dev_count].dev == NULL) {
			KGSL_CORE_ERR("Failed to iommu dev handle for "
				"device %s\n", data->iommu_ctx_names[i]);
			return -EINVAL;
		}

		iommu->dev_count++;
	}

	return 0;
}

static int kgsl_get_iommu_ctxt(struct kgsl_iommu *iommu,
				struct kgsl_device *device)
{
	struct platform_device *pdev =
		container_of(device->parentdev, struct platform_device, dev);
	struct kgsl_device_platform_data *pdata_dev = pdev->dev.platform_data;
	int i, ret = 0;

	/* Go through the IOMMU data and attach all the domains */

	for (i = 0; i < pdata_dev->iommu_count; i++) {
		ret = _get_iommu_ctxs(iommu, device,
			&pdata_dev->iommu_data[i]);
		if (ret)
			break;
	}

	return ret;
}

static void kgsl_iommu_setstate(struct kgsl_device *device,
				struct kgsl_pagetable *pagetable)
{
	struct kgsl_mmu *mmu = &device->mmu;

	if (mmu->flags & KGSL_FLAGS_STARTED) {
		/* page table not current, then setup mmu to use new
		 *  specified page table
		 */
		if (mmu->hwpagetable != pagetable) {
			kgsl_idle(device, KGSL_TIMEOUT_DEFAULT);
			kgsl_detach_pagetable_iommu_domain(mmu);
			mmu->hwpagetable = pagetable;
			if (mmu->hwpagetable)
				kgsl_attach_pagetable_iommu_domain(mmu);
		}
	}
}

static int kgsl_iommu_init(struct kgsl_device *device)
{
	/*
	 * intialize device mmu
	 *
	 * call this with the global lock held
	 */
	int status = 0;
	struct kgsl_mmu *mmu = &device->mmu;
	struct kgsl_iommu *iommu;

	mmu->device = device;

	iommu = kzalloc(sizeof(struct kgsl_iommu), GFP_KERNEL);
	if (!iommu) {
		KGSL_CORE_ERR("kzalloc(%d) failed\n",
				sizeof(struct kgsl_iommu));
		return -ENOMEM;
	}

	status = kgsl_get_iommu_ctxt(iommu, device);
	if (status) {
		kfree(iommu);
		iommu = NULL;
	}
	mmu->priv = iommu;

	dev_info(device->dev, "|%s| MMU type set for device is IOMMU\n",
			__func__);
	return status;
}

static int kgsl_iommu_start(struct kgsl_device *device)
{
	int status;
	struct kgsl_mmu *mmu = &device->mmu;

	if (mmu->flags & KGSL_FLAGS_STARTED)
		return 0;

	kgsl_regwrite(device, MH_MMU_CONFIG, 0x00000000);
	if (mmu->defaultpagetable == NULL)
		mmu->defaultpagetable =
			kgsl_mmu_getpagetable(KGSL_MMU_GLOBAL_PT);
	/* Return error if the default pagetable doesn't exist */
	if (mmu->defaultpagetable == NULL)
		return -ENOMEM;
	mmu->hwpagetable = mmu->defaultpagetable;

	status = kgsl_attach_pagetable_iommu_domain(mmu);
	if (!status)
		mmu->flags |= KGSL_FLAGS_STARTED;

	return status;
}

static int
kgsl_iommu_unmap(void *mmu_specific_pt,
		struct kgsl_memdesc *memdesc)
{
	int ret;
	unsigned int range = memdesc->size;
	struct iommu_domain *domain = (struct iommu_domain *)
					mmu_specific_pt;

	/* All GPU addresses as assigned are page aligned, but some
	   functions purturb the gpuaddr with an offset, so apply the
	   mask here to make sure we have the right address */

	unsigned int gpuaddr = memdesc->gpuaddr &  KGSL_MMU_ALIGN_MASK;

	if (range == 0 || gpuaddr == 0)
		return 0;

	ret = iommu_unmap_range(domain, gpuaddr, range);
	if (ret)
		KGSL_CORE_ERR("iommu_unmap_range(%p, %x, %d) failed "
			"with err: %d\n", domain, gpuaddr,
			range, ret);

	return 0;
}

static int
kgsl_iommu_map(void *mmu_specific_pt,
			struct kgsl_memdesc *memdesc,
			unsigned int protflags)
{
	int ret;
	unsigned int iommu_virt_addr;
	struct iommu_domain *domain = mmu_specific_pt;

	BUG_ON(NULL == domain);


	iommu_virt_addr = memdesc->gpuaddr;

	ret = iommu_map_range(domain, iommu_virt_addr, memdesc->sg,
				memdesc->size, 0);
	if (ret) {
		KGSL_CORE_ERR("iommu_map_range(%p, %x, %p, %d, %d) "
				"failed with err: %d\n", domain,
				iommu_virt_addr, memdesc->sg, memdesc->size,
				0, ret);
		return ret;
	}

	return ret;
}

static int kgsl_iommu_stop(struct kgsl_device *device)
{
	/*
	 *  stop device mmu
	 *
	 *  call this with the global lock held
	 */
	struct kgsl_mmu *mmu = &device->mmu;

	if (mmu->flags & KGSL_FLAGS_STARTED) {
		/* detach iommu attachment */
		kgsl_detach_pagetable_iommu_domain(mmu);

		mmu->flags &= ~KGSL_FLAGS_STARTED;
	}

	return 0;
}

static int kgsl_iommu_close(struct kgsl_device *device)
{
	struct kgsl_mmu *mmu = &device->mmu;
	if (mmu->defaultpagetable)
		kgsl_mmu_putpagetable(mmu->defaultpagetable);

	return 0;
}

static unsigned int
kgsl_iommu_get_current_ptbase(struct kgsl_device *device)
{
	/* Current base is always the hwpagetables domain as we
	 * do not use per process pagetables right not for iommu.
	 * This will change when we switch to per process pagetables.
	 */
	return (unsigned int)device->mmu.hwpagetable->priv;
}

struct kgsl_mmu_ops iommu_ops = {
	.mmu_init = kgsl_iommu_init,
	.mmu_close = kgsl_iommu_close,
	.mmu_start = kgsl_iommu_start,
	.mmu_stop = kgsl_iommu_stop,
	.mmu_setstate = kgsl_iommu_setstate,
	.mmu_device_setstate = NULL,
	.mmu_pagefault = NULL,
	.mmu_get_current_ptbase = kgsl_iommu_get_current_ptbase,
};

struct kgsl_mmu_pt_ops iommu_pt_ops = {
	.mmu_map = kgsl_iommu_map,
	.mmu_unmap = kgsl_iommu_unmap,
	.mmu_create_pagetable = kgsl_iommu_create_pagetable,
	.mmu_destroy_pagetable = kgsl_iommu_destroy_pagetable,
	.mmu_pt_equal = kgsl_iommu_pt_equal,
	.mmu_pt_get_flags = NULL,
};
