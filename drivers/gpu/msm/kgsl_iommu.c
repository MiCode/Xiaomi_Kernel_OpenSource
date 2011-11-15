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

struct kgsl_iommu {
	struct device *iommu_user_dev;
	int iommu_user_dev_attached;
	struct device *iommu_priv_dev;
	int iommu_priv_dev_attached;
};

static int kgsl_iommu_pt_equal(struct kgsl_pagetable *pt,
					unsigned int pt_base)
{
	struct iommu_domain *domain = pt->priv;
	return pt && pt_base && ((unsigned int)domain == pt_base);
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

	BUG_ON(mmu->hwpagetable == NULL);
	BUG_ON(mmu->hwpagetable->priv == NULL);

	domain = mmu->hwpagetable->priv;

	if (iommu->iommu_user_dev_attached) {
		iommu_detach_device(domain, iommu->iommu_user_dev);
		iommu->iommu_user_dev_attached = 0;
		KGSL_MEM_INFO(mmu->device,
				"iommu %p detached from user dev of MMU: %p\n",
				domain, mmu);
	}
	if (iommu->iommu_priv_dev_attached) {
		iommu_detach_device(domain, iommu->iommu_priv_dev);
		iommu->iommu_priv_dev_attached = 0;
		KGSL_MEM_INFO(mmu->device,
				"iommu %p detached from priv dev of MMU: %p\n",
				domain, mmu);
	}
}

static int kgsl_attach_pagetable_iommu_domain(struct kgsl_mmu *mmu)
{
	struct iommu_domain *domain;
	int ret = 0;
	struct kgsl_iommu *iommu = mmu->priv;

	BUG_ON(mmu->hwpagetable == NULL);
	BUG_ON(mmu->hwpagetable->priv == NULL);

	domain = mmu->hwpagetable->priv;

	if (iommu->iommu_user_dev && !iommu->iommu_user_dev_attached) {
		ret = iommu_attach_device(domain, iommu->iommu_user_dev);
		if (ret) {
			KGSL_MEM_ERR(mmu->device,
			"Failed to attach device, err %d\n", ret);
			goto done;
		}
		iommu->iommu_user_dev_attached = 1;
		KGSL_MEM_INFO(mmu->device,
				"iommu %p attached to user dev of MMU: %p\n",
				domain, mmu);
	}
	if (iommu->iommu_priv_dev && !iommu->iommu_priv_dev_attached) {
		ret = iommu_attach_device(domain, iommu->iommu_priv_dev);
		if (ret) {
			KGSL_MEM_ERR(mmu->device,
				"Failed to attach device, err %d\n", ret);
			iommu_detach_device(domain, iommu->iommu_user_dev);
			iommu->iommu_user_dev_attached = 0;
			goto done;
		}
		iommu->iommu_priv_dev_attached = 1;
		KGSL_MEM_INFO(mmu->device,
				"iommu %p attached to priv dev of MMU: %p\n",
				domain, mmu);
	}
done:
	return ret;
}

static int kgsl_get_iommu_ctxt(struct kgsl_iommu *iommu,
				struct kgsl_device *device)
{
	int status = 0;
	struct platform_device *pdev =
		container_of(device->parentdev, struct platform_device, dev);
	struct kgsl_device_platform_data *pdata_dev = pdev->dev.platform_data;
	if (pdata_dev->iommu_user_ctx_name)
		iommu->iommu_user_dev = msm_iommu_get_ctx(
					pdata_dev->iommu_user_ctx_name);
	if (pdata_dev->iommu_priv_ctx_name)
		iommu->iommu_priv_dev = msm_iommu_get_ctx(
					pdata_dev->iommu_priv_ctx_name);
	if (!iommu->iommu_user_dev) {
		KGSL_CORE_ERR("Failed to get user iommu dev handle for "
				"device %s\n",
				pdata_dev->iommu_user_ctx_name);
		status = -EINVAL;
	}
	return status;
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

	iommu->iommu_priv_dev_attached = 0;
	iommu->iommu_user_dev_attached = 0;
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
