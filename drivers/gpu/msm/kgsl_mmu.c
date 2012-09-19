/* Copyright (c) 2002,2007-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/export.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/genalloc.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/iommu.h>
#include <mach/iommu.h>
#include <mach/socinfo.h>

#include "kgsl.h"
#include "kgsl_mmu.h"
#include "kgsl_device.h"
#include "kgsl_sharedmem.h"

#define KGSL_MMU_ALIGN_SHIFT    13
#define KGSL_MMU_ALIGN_MASK     (~((1 << KGSL_MMU_ALIGN_SHIFT) - 1))

static enum kgsl_mmutype kgsl_mmu_type;

static void pagetable_remove_sysfs_objects(struct kgsl_pagetable *pagetable);

static int kgsl_cleanup_pt(struct kgsl_pagetable *pt)
{
	int i;
	/* For IOMMU only unmap the global structures to global pt */
	if ((KGSL_MMU_TYPE_NONE != kgsl_mmu_type) &&
		(KGSL_MMU_TYPE_IOMMU == kgsl_mmu_type) &&
		(KGSL_MMU_GLOBAL_PT !=  pt->name) &&
		(KGSL_MMU_PRIV_BANK_TABLE_NAME !=  pt->name))
		return 0;
	for (i = 0; i < KGSL_DEVICE_MAX; i++) {
		struct kgsl_device *device = kgsl_driver.devp[i];
		if (device)
			device->ftbl->cleanup_pt(device, pt);
	}
	return 0;
}


static int kgsl_setup_pt(struct kgsl_pagetable *pt)
{
	int i = 0;
	int status = 0;

	/* For IOMMU only map the global structures to global pt */
	if ((KGSL_MMU_TYPE_NONE != kgsl_mmu_type) &&
		(KGSL_MMU_TYPE_IOMMU == kgsl_mmu_type) &&
		(KGSL_MMU_GLOBAL_PT !=  pt->name) &&
		(KGSL_MMU_PRIV_BANK_TABLE_NAME !=  pt->name))
		return 0;
	for (i = 0; i < KGSL_DEVICE_MAX; i++) {
		struct kgsl_device *device = kgsl_driver.devp[i];
		if (device) {
			status = device->ftbl->setup_pt(device, pt);
			if (status)
				goto error_pt;
		}
	}
	return status;
error_pt:
	while (i >= 0) {
		struct kgsl_device *device = kgsl_driver.devp[i];
		if (device)
			device->ftbl->cleanup_pt(device, pt);
		i--;
	}
	return status;
}

static void kgsl_destroy_pagetable(struct kref *kref)
{
	struct kgsl_pagetable *pagetable = container_of(kref,
		struct kgsl_pagetable, refcount);
	unsigned long flags;

	spin_lock_irqsave(&kgsl_driver.ptlock, flags);
	list_del(&pagetable->list);
	spin_unlock_irqrestore(&kgsl_driver.ptlock, flags);

	pagetable_remove_sysfs_objects(pagetable);

	kgsl_cleanup_pt(pagetable);

	if (pagetable->kgsl_pool)
		gen_pool_destroy(pagetable->kgsl_pool);
	if (pagetable->pool)
		gen_pool_destroy(pagetable->pool);

	pagetable->pt_ops->mmu_destroy_pagetable(pagetable->priv);

	kfree(pagetable);
}

static inline void kgsl_put_pagetable(struct kgsl_pagetable *pagetable)
{
	if (pagetable)
		kref_put(&pagetable->refcount, kgsl_destroy_pagetable);
}

static struct kgsl_pagetable *
kgsl_get_pagetable(unsigned long name)
{
	struct kgsl_pagetable *pt, *ret = NULL;
	unsigned long flags;

	spin_lock_irqsave(&kgsl_driver.ptlock, flags);
	list_for_each_entry(pt, &kgsl_driver.pagetable_list, list) {
		if (pt->name == name) {
			ret = pt;
			kref_get(&ret->refcount);
			break;
		}
	}

	spin_unlock_irqrestore(&kgsl_driver.ptlock, flags);
	return ret;
}

static struct kgsl_pagetable *
_get_pt_from_kobj(struct kobject *kobj)
{
	unsigned long ptname;

	if (!kobj)
		return NULL;

	if (sscanf(kobj->name, "%ld", &ptname) != 1)
		return NULL;

	return kgsl_get_pagetable(ptname);
}

static ssize_t
sysfs_show_entries(struct kobject *kobj,
		   struct kobj_attribute *attr,
		   char *buf)
{
	struct kgsl_pagetable *pt;
	int ret = 0;

	pt = _get_pt_from_kobj(kobj);

	if (pt)
		ret += snprintf(buf, PAGE_SIZE, "%d\n", pt->stats.entries);

	kgsl_put_pagetable(pt);
	return ret;
}

static ssize_t
sysfs_show_mapped(struct kobject *kobj,
		  struct kobj_attribute *attr,
		  char *buf)
{
	struct kgsl_pagetable *pt;
	int ret = 0;

	pt = _get_pt_from_kobj(kobj);

	if (pt)
		ret += snprintf(buf, PAGE_SIZE, "%d\n", pt->stats.mapped);

	kgsl_put_pagetable(pt);
	return ret;
}

static ssize_t
sysfs_show_va_range(struct kobject *kobj,
		    struct kobj_attribute *attr,
		    char *buf)
{
	struct kgsl_pagetable *pt;
	int ret = 0;

	pt = _get_pt_from_kobj(kobj);

	if (pt) {
		ret += snprintf(buf, PAGE_SIZE, "0x%x\n",
			kgsl_mmu_get_ptsize());
	}

	kgsl_put_pagetable(pt);
	return ret;
}

static ssize_t
sysfs_show_max_mapped(struct kobject *kobj,
		      struct kobj_attribute *attr,
		      char *buf)
{
	struct kgsl_pagetable *pt;
	int ret = 0;

	pt = _get_pt_from_kobj(kobj);

	if (pt)
		ret += snprintf(buf, PAGE_SIZE, "%d\n", pt->stats.max_mapped);

	kgsl_put_pagetable(pt);
	return ret;
}

static ssize_t
sysfs_show_max_entries(struct kobject *kobj,
		       struct kobj_attribute *attr,
		       char *buf)
{
	struct kgsl_pagetable *pt;
	int ret = 0;

	pt = _get_pt_from_kobj(kobj);

	if (pt)
		ret += snprintf(buf, PAGE_SIZE, "%d\n", pt->stats.max_entries);

	kgsl_put_pagetable(pt);
	return ret;
}

static struct kobj_attribute attr_entries = {
	.attr = { .name = "entries", .mode = 0444 },
	.show = sysfs_show_entries,
	.store = NULL,
};

static struct kobj_attribute attr_mapped = {
	.attr = { .name = "mapped", .mode = 0444 },
	.show = sysfs_show_mapped,
	.store = NULL,
};

static struct kobj_attribute attr_va_range = {
	.attr = { .name = "va_range", .mode = 0444 },
	.show = sysfs_show_va_range,
	.store = NULL,
};

static struct kobj_attribute attr_max_mapped = {
	.attr = { .name = "max_mapped", .mode = 0444 },
	.show = sysfs_show_max_mapped,
	.store = NULL,
};

static struct kobj_attribute attr_max_entries = {
	.attr = { .name = "max_entries", .mode = 0444 },
	.show = sysfs_show_max_entries,
	.store = NULL,
};

static struct attribute *pagetable_attrs[] = {
	&attr_entries.attr,
	&attr_mapped.attr,
	&attr_va_range.attr,
	&attr_max_mapped.attr,
	&attr_max_entries.attr,
	NULL,
};

static struct attribute_group pagetable_attr_group = {
	.attrs = pagetable_attrs,
};

static void
pagetable_remove_sysfs_objects(struct kgsl_pagetable *pagetable)
{
	if (pagetable->kobj)
		sysfs_remove_group(pagetable->kobj,
				   &pagetable_attr_group);

	kobject_put(pagetable->kobj);
}

static int
pagetable_add_sysfs_objects(struct kgsl_pagetable *pagetable)
{
	char ptname[16];
	int ret = -ENOMEM;

	snprintf(ptname, sizeof(ptname), "%d", pagetable->name);
	pagetable->kobj = kobject_create_and_add(ptname,
						 kgsl_driver.ptkobj);
	if (pagetable->kobj == NULL)
		goto err;

	ret = sysfs_create_group(pagetable->kobj, &pagetable_attr_group);

err:
	if (ret) {
		if (pagetable->kobj)
			kobject_put(pagetable->kobj);

		pagetable->kobj = NULL;
	}

	return ret;
}

unsigned int kgsl_mmu_get_ptsize(void)
{
	/*
	 * For IOMMU, we could do up to 4G virtual range if we wanted to, but
	 * it makes more sense to return a smaller range and leave the rest of
	 * the virtual range for future improvements
	 */

	if (KGSL_MMU_TYPE_GPU == kgsl_mmu_type)
		return CONFIG_MSM_KGSL_PAGE_TABLE_SIZE;
	else if (KGSL_MMU_TYPE_IOMMU == kgsl_mmu_type)
		return SZ_2G - KGSL_PAGETABLE_BASE;
	else
		return 0;
}

int
kgsl_mmu_get_ptname_from_ptbase(struct kgsl_mmu *mmu, unsigned int pt_base)
{
	struct kgsl_pagetable *pt;
	int ptid = -1;

	if (!mmu->mmu_ops || !mmu->mmu_ops->mmu_pt_equal)
		return KGSL_MMU_GLOBAL_PT;
	spin_lock(&kgsl_driver.ptlock);
	list_for_each_entry(pt, &kgsl_driver.pagetable_list, list) {
		if (mmu->mmu_ops->mmu_pt_equal(mmu, pt, pt_base)) {
			ptid = (int) pt->name;
			break;
		}
	}
	spin_unlock(&kgsl_driver.ptlock);

	return ptid;
}
EXPORT_SYMBOL(kgsl_mmu_get_ptname_from_ptbase);

int kgsl_mmu_init(struct kgsl_device *device)
{
	int status = 0;
	struct kgsl_mmu *mmu = &device->mmu;

	mmu->device = device;
	status = kgsl_allocate_contiguous(&mmu->setstate_memory, PAGE_SIZE);
	if (status)
		return status;
	kgsl_sharedmem_set(&mmu->setstate_memory, 0, 0,
				mmu->setstate_memory.size);

	if (KGSL_MMU_TYPE_NONE == kgsl_mmu_type) {
		dev_info(device->dev, "|%s| MMU type set for device is "
				"NOMMU\n", __func__);
		goto done;
	} else if (KGSL_MMU_TYPE_GPU == kgsl_mmu_type)
		mmu->mmu_ops = &gpummu_ops;
	else if (KGSL_MMU_TYPE_IOMMU == kgsl_mmu_type)
		mmu->mmu_ops = &iommu_ops;

	status =  mmu->mmu_ops->mmu_init(mmu);
done:
	if (status)
		kgsl_sharedmem_free(&mmu->setstate_memory);
	return status;
}
EXPORT_SYMBOL(kgsl_mmu_init);

int kgsl_mmu_start(struct kgsl_device *device)
{
	struct kgsl_mmu *mmu = &device->mmu;

	if (kgsl_mmu_type == KGSL_MMU_TYPE_NONE) {
		kgsl_regwrite(device, MH_MMU_CONFIG, 0);
		/* Setup gpuaddr of global mappings */
		if (!mmu->setstate_memory.gpuaddr)
			kgsl_setup_pt(NULL);
		return 0;
	} else {
		return mmu->mmu_ops->mmu_start(mmu);
	}
}
EXPORT_SYMBOL(kgsl_mmu_start);

static void mh_axi_error(struct kgsl_device *device, const char* type)
{
	unsigned int reg, gpu_err, phys_err, pt_base;

	kgsl_regread(device, MH_AXI_ERROR, &reg);
	pt_base = kgsl_mmu_get_current_ptbase(&device->mmu);
	/*
	 * Read gpu virtual and physical addresses that
	 * caused the error from the debug data.
	 */
	kgsl_regwrite(device, MH_DEBUG_CTRL, 44);
	kgsl_regread(device, MH_DEBUG_DATA, &gpu_err);
	kgsl_regwrite(device, MH_DEBUG_CTRL, 45);
	kgsl_regread(device, MH_DEBUG_DATA, &phys_err);
	KGSL_MEM_CRIT(device,
			"axi %s error: %08x pt %08x gpu %08x phys %08x\n",
			type, reg, pt_base, gpu_err, phys_err);
}

void kgsl_mh_intrcallback(struct kgsl_device *device)
{
	unsigned int status = 0;

	kgsl_regread(device, MH_INTERRUPT_STATUS, &status);

	if (status & MH_INTERRUPT_MASK__AXI_READ_ERROR)
		mh_axi_error(device, "read");
	if (status & MH_INTERRUPT_MASK__AXI_WRITE_ERROR)
		mh_axi_error(device, "write");
	if (status & MH_INTERRUPT_MASK__MMU_PAGE_FAULT)
		device->mmu.mmu_ops->mmu_pagefault(&device->mmu);

	status &= KGSL_MMU_INT_MASK;
	kgsl_regwrite(device, MH_INTERRUPT_CLEAR, status);
}
EXPORT_SYMBOL(kgsl_mh_intrcallback);

static struct kgsl_pagetable *kgsl_mmu_createpagetableobject(
				unsigned int name)
{
	int status = 0;
	struct kgsl_pagetable *pagetable = NULL;
	unsigned long flags;
	unsigned int ptsize;

	pagetable = kzalloc(sizeof(struct kgsl_pagetable), GFP_KERNEL);
	if (pagetable == NULL) {
		KGSL_CORE_ERR("kzalloc(%d) failed\n",
			sizeof(struct kgsl_pagetable));
		return NULL;
	}

	kref_init(&pagetable->refcount);

	spin_lock_init(&pagetable->lock);

	ptsize = kgsl_mmu_get_ptsize();

	pagetable->name = name;
	pagetable->max_entries = KGSL_PAGETABLE_ENTRIES(ptsize);

	/*
	 * create a separate kgsl pool for IOMMU, global mappings can be mapped
	 * just once from this pool of the defaultpagetable
	 */
	if ((KGSL_MMU_TYPE_IOMMU == kgsl_mmu_get_mmutype()) &&
		((KGSL_MMU_GLOBAL_PT == name) ||
		(KGSL_MMU_PRIV_BANK_TABLE_NAME == name))) {
		pagetable->kgsl_pool = gen_pool_create(PAGE_SHIFT, -1);
		if (pagetable->kgsl_pool == NULL) {
			KGSL_CORE_ERR("gen_pool_create(%d) failed\n",
					KGSL_MMU_ALIGN_SHIFT);
			goto err_alloc;
		}
		if (gen_pool_add(pagetable->kgsl_pool,
			KGSL_IOMMU_GLOBAL_MEM_BASE,
			KGSL_IOMMU_GLOBAL_MEM_SIZE, -1)) {
			KGSL_CORE_ERR("gen_pool_add failed\n");
			goto err_kgsl_pool;
		}
	}

	pagetable->pool = gen_pool_create(KGSL_MMU_ALIGN_SHIFT, -1);
	if (pagetable->pool == NULL) {
		KGSL_CORE_ERR("gen_pool_create(%d) failed\n",
			      KGSL_MMU_ALIGN_SHIFT);
		goto err_kgsl_pool;
	}

	if (gen_pool_add(pagetable->pool, KGSL_PAGETABLE_BASE,
				ptsize, -1)) {
		KGSL_CORE_ERR("gen_pool_add failed\n");
		goto err_pool;
	}

	if (KGSL_MMU_TYPE_GPU == kgsl_mmu_type)
		pagetable->pt_ops = &gpummu_pt_ops;
	else if (KGSL_MMU_TYPE_IOMMU == kgsl_mmu_type)
		pagetable->pt_ops = &iommu_pt_ops;

	pagetable->priv = pagetable->pt_ops->mmu_create_pagetable();
	if (!pagetable->priv)
		goto err_pool;

	status = kgsl_setup_pt(pagetable);
	if (status)
		goto err_mmu_create;

	spin_lock_irqsave(&kgsl_driver.ptlock, flags);
	list_add(&pagetable->list, &kgsl_driver.pagetable_list);
	spin_unlock_irqrestore(&kgsl_driver.ptlock, flags);

	/* Create the sysfs entries */
	pagetable_add_sysfs_objects(pagetable);

	return pagetable;

err_mmu_create:
	pagetable->pt_ops->mmu_destroy_pagetable(pagetable->priv);
err_pool:
	gen_pool_destroy(pagetable->pool);
err_kgsl_pool:
	if (pagetable->kgsl_pool)
		gen_pool_destroy(pagetable->kgsl_pool);
err_alloc:
	kfree(pagetable);

	return NULL;
}

struct kgsl_pagetable *kgsl_mmu_getpagetable(unsigned long name)
{
	struct kgsl_pagetable *pt;

	if (KGSL_MMU_TYPE_NONE == kgsl_mmu_type)
		return (void *)(-1);

#ifndef CONFIG_KGSL_PER_PROCESS_PAGE_TABLE
	name = KGSL_MMU_GLOBAL_PT;
#endif
	/* We presently do not support per-process for IOMMU-v2 */
	if (!msm_soc_version_supports_iommu_v1())
		name = KGSL_MMU_GLOBAL_PT;

	pt = kgsl_get_pagetable(name);

	if (pt == NULL)
		pt = kgsl_mmu_createpagetableobject(name);

	return pt;
}

void kgsl_mmu_putpagetable(struct kgsl_pagetable *pagetable)
{
	kgsl_put_pagetable(pagetable);
}
EXPORT_SYMBOL(kgsl_mmu_putpagetable);

void kgsl_setstate(struct kgsl_mmu *mmu, unsigned int context_id,
			uint32_t flags)
{
	struct kgsl_device *device = mmu->device;
	if (KGSL_MMU_TYPE_NONE == kgsl_mmu_type)
		return;
	else if (device->ftbl->setstate)
		device->ftbl->setstate(device, context_id, flags);
	else if (mmu->mmu_ops->mmu_device_setstate)
		mmu->mmu_ops->mmu_device_setstate(mmu, flags);
}
EXPORT_SYMBOL(kgsl_setstate);

void kgsl_mh_start(struct kgsl_device *device)
{
	struct kgsl_mh *mh = &device->mh;
	/* force mmu off to for now*/
	kgsl_regwrite(device, MH_MMU_CONFIG, 0);
	kgsl_idle(device);

	/* define physical memory range accessible by the core */
	kgsl_regwrite(device, MH_MMU_MPU_BASE, mh->mpu_base);
	kgsl_regwrite(device, MH_MMU_MPU_END,
			mh->mpu_base + mh->mpu_range);
	kgsl_regwrite(device, MH_ARBITER_CONFIG, mh->mharb);

	if (mh->mh_intf_cfg1 != 0)
		kgsl_regwrite(device, MH_CLNT_INTF_CTRL_CONFIG1,
				mh->mh_intf_cfg1);

	if (mh->mh_intf_cfg2 != 0)
		kgsl_regwrite(device, MH_CLNT_INTF_CTRL_CONFIG2,
				mh->mh_intf_cfg2);

	/*
	 * Interrupts are enabled on a per-device level when
	 * kgsl_pwrctrl_irq() is called
	 */
}

static inline struct gen_pool *
_get_pool(struct kgsl_pagetable *pagetable, unsigned int flags)
{
	if (pagetable->kgsl_pool &&
		(KGSL_MEMFLAGS_GLOBAL & flags))
		return pagetable->kgsl_pool;
	return pagetable->pool;
}

int
kgsl_mmu_map(struct kgsl_pagetable *pagetable,
				struct kgsl_memdesc *memdesc,
				unsigned int protflags)
{
	int ret;
	struct gen_pool *pool;
	int size;

	if (kgsl_mmu_type == KGSL_MMU_TYPE_NONE) {
		if (memdesc->sglen == 1) {
			memdesc->gpuaddr = sg_dma_address(memdesc->sg);
			if (!memdesc->gpuaddr)
				memdesc->gpuaddr = sg_phys(memdesc->sg);
			if (!memdesc->gpuaddr) {
				KGSL_CORE_ERR("Unable to get a valid physical "
					"address for memdesc\n");
				return -EINVAL;
			}
			return 0;
		} else {
			KGSL_CORE_ERR("Memory is not contigious "
					"(sglen = %d)\n", memdesc->sglen);
			return -EINVAL;
		}
	}

	size = kgsl_sg_size(memdesc->sg, memdesc->sglen);

	/* Allocate from kgsl pool if it exists for global mappings */
	pool = _get_pool(pagetable, memdesc->priv);

	memdesc->gpuaddr = gen_pool_alloc(pool, size);
	if (memdesc->gpuaddr == 0) {
		KGSL_CORE_ERR("gen_pool_alloc(%d) failed from pool: %s\n",
			size,
			(pool == pagetable->kgsl_pool) ?
			"kgsl_pool" : "general_pool");
		KGSL_CORE_ERR(" [%d] allocated=%d, entries=%d\n",
				pagetable->name, pagetable->stats.mapped,
				pagetable->stats.entries);
		return -ENOMEM;
	}

	if (KGSL_MMU_TYPE_IOMMU != kgsl_mmu_get_mmutype())
		spin_lock(&pagetable->lock);
	ret = pagetable->pt_ops->mmu_map(pagetable->priv, memdesc, protflags,
						&pagetable->tlb_flags);
	if (KGSL_MMU_TYPE_IOMMU == kgsl_mmu_get_mmutype())
		spin_lock(&pagetable->lock);

	if (ret)
		goto err_free_gpuaddr;

	/* Keep track of the statistics for the sysfs files */

	KGSL_STATS_ADD(1, pagetable->stats.entries,
		       pagetable->stats.max_entries);

	KGSL_STATS_ADD(size, pagetable->stats.mapped,
		       pagetable->stats.max_mapped);

	spin_unlock(&pagetable->lock);

	return 0;

err_free_gpuaddr:
	spin_unlock(&pagetable->lock);
	gen_pool_free(pool, memdesc->gpuaddr, size);
	memdesc->gpuaddr = 0;
	return ret;
}
EXPORT_SYMBOL(kgsl_mmu_map);

int
kgsl_mmu_unmap(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc)
{
	struct gen_pool *pool;
	int size;

	if (memdesc->size == 0 || memdesc->gpuaddr == 0)
		return 0;

	if (kgsl_mmu_type == KGSL_MMU_TYPE_NONE) {
		memdesc->gpuaddr = 0;
		return 0;
	}

	size = kgsl_sg_size(memdesc->sg, memdesc->sglen);

	if (KGSL_MMU_TYPE_IOMMU != kgsl_mmu_get_mmutype())
		spin_lock(&pagetable->lock);
	pagetable->pt_ops->mmu_unmap(pagetable->priv, memdesc,
					&pagetable->tlb_flags);
	if (KGSL_MMU_TYPE_IOMMU == kgsl_mmu_get_mmutype())
		spin_lock(&pagetable->lock);
	/* Remove the statistics */
	pagetable->stats.entries--;
	pagetable->stats.mapped -= size;

	spin_unlock(&pagetable->lock);

	pool = _get_pool(pagetable, memdesc->priv);
	gen_pool_free(pool, memdesc->gpuaddr, size);

	/*
	 * Don't clear the gpuaddr on global mappings because they
	 * may be in use by other pagetables
	 */
	if (!(memdesc->priv & KGSL_MEMFLAGS_GLOBAL))
		memdesc->gpuaddr = 0;
	return 0;
}
EXPORT_SYMBOL(kgsl_mmu_unmap);

int kgsl_mmu_map_global(struct kgsl_pagetable *pagetable,
			struct kgsl_memdesc *memdesc, unsigned int protflags)
{
	int result = -EINVAL;
	unsigned int gpuaddr = 0;

	if (memdesc == NULL) {
		KGSL_CORE_ERR("invalid memdesc\n");
		goto error;
	}
	/* Not all global mappings are needed for all MMU types */
	if (!memdesc->size)
		return 0;

	gpuaddr = memdesc->gpuaddr;
	memdesc->priv |= KGSL_MEMFLAGS_GLOBAL;

	result = kgsl_mmu_map(pagetable, memdesc, protflags);
	if (result)
		goto error;

	/*global mappings must have the same gpu address in all pagetables*/
	if (gpuaddr && gpuaddr != memdesc->gpuaddr) {
		KGSL_CORE_ERR("pt %p addr mismatch phys 0x%08x"
			"gpu 0x%0x 0x%08x", pagetable, memdesc->physaddr,
			gpuaddr, memdesc->gpuaddr);
		goto error_unmap;
	}
	return result;
error_unmap:
	kgsl_mmu_unmap(pagetable, memdesc);
error:
	return result;
}
EXPORT_SYMBOL(kgsl_mmu_map_global);

int kgsl_mmu_close(struct kgsl_device *device)
{
	struct kgsl_mmu *mmu = &device->mmu;

	kgsl_sharedmem_free(&mmu->setstate_memory);
	if (kgsl_mmu_type == KGSL_MMU_TYPE_NONE)
		return 0;
	else
		return mmu->mmu_ops->mmu_close(mmu);
}
EXPORT_SYMBOL(kgsl_mmu_close);

int kgsl_mmu_pt_get_flags(struct kgsl_pagetable *pt,
			enum kgsl_deviceid id)
{
	unsigned int result = 0;

	if (pt == NULL)
		return 0;

	spin_lock(&pt->lock);
	if (pt->tlb_flags & (1<<id)) {
		result = KGSL_MMUFLAGS_TLBFLUSH;
		pt->tlb_flags &= ~(1<<id);
	}
	spin_unlock(&pt->lock);
	return result;
}
EXPORT_SYMBOL(kgsl_mmu_pt_get_flags);

void kgsl_mmu_ptpool_destroy(void *ptpool)
{
	if (KGSL_MMU_TYPE_GPU == kgsl_mmu_type)
		kgsl_gpummu_ptpool_destroy(ptpool);
	ptpool = 0;
}
EXPORT_SYMBOL(kgsl_mmu_ptpool_destroy);

void *kgsl_mmu_ptpool_init(int entries)
{
	if (KGSL_MMU_TYPE_GPU == kgsl_mmu_type)
		return kgsl_gpummu_ptpool_init(entries);
	else
		return (void *)(-1);
}
EXPORT_SYMBOL(kgsl_mmu_ptpool_init);

int kgsl_mmu_enabled(void)
{
	if (KGSL_MMU_TYPE_NONE != kgsl_mmu_type)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(kgsl_mmu_enabled);

enum kgsl_mmutype kgsl_mmu_get_mmutype(void)
{
	return kgsl_mmu_type;
}
EXPORT_SYMBOL(kgsl_mmu_get_mmutype);

void kgsl_mmu_set_mmutype(char *mmutype)
{
	/* Set the default MMU - GPU on <=8960 and nothing on >= 8064 */
	kgsl_mmu_type =
		cpu_is_apq8064() ? KGSL_MMU_TYPE_NONE : KGSL_MMU_TYPE_GPU;

	/* Use the IOMMU if it is found */
	if (iommu_present(&platform_bus_type))
		kgsl_mmu_type = KGSL_MMU_TYPE_IOMMU;

	if (mmutype && !strncmp(mmutype, "gpummu", 6))
		kgsl_mmu_type = KGSL_MMU_TYPE_GPU;
	if (iommu_present(&platform_bus_type) && mmutype &&
	    !strncmp(mmutype, "iommu", 5))
		kgsl_mmu_type = KGSL_MMU_TYPE_IOMMU;
	if (mmutype && !strncmp(mmutype, "nommu", 5))
		kgsl_mmu_type = KGSL_MMU_TYPE_NONE;
}
EXPORT_SYMBOL(kgsl_mmu_set_mmutype);

int kgsl_mmu_gpuaddr_in_range(unsigned int gpuaddr)
{
	if (KGSL_MMU_TYPE_NONE == kgsl_mmu_type)
		return 1;
	return ((gpuaddr >= KGSL_PAGETABLE_BASE) &&
		(gpuaddr < (KGSL_PAGETABLE_BASE + kgsl_mmu_get_ptsize())));
}
EXPORT_SYMBOL(kgsl_mmu_gpuaddr_in_range);

