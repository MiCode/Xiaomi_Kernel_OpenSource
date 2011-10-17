/* Copyright (c) 2002,2007-2011, Code Aurora Forum. All rights reserved.
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
#include <linux/sched.h>
#include <linux/iommu.h>

#include "kgsl.h"
#include "kgsl_mmu.h"
#include "kgsl_device.h"
#include "kgsl_sharedmem.h"
#include "adreno_postmortem.h"

#define KGSL_MMU_ALIGN_SHIFT    13
#define KGSL_MMU_ALIGN_MASK     (~((1 << KGSL_MMU_ALIGN_SHIFT) - 1))

static enum kgsl_mmutype kgsl_mmu_type;

static void pagetable_remove_sysfs_objects(struct kgsl_pagetable *pagetable);

static int kgsl_cleanup_pt(struct kgsl_pagetable *pt)
{
	int i;
	for (i = 0; i < KGSL_DEVICE_MAX; i++) {
		struct kgsl_device *device = kgsl_driver.devp[i];
		if (device)
			device->ftbl->cleanup_pt(device, pt);
	}
	return 0;
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

	if (pt)
		ret += snprintf(buf, PAGE_SIZE, "0x%x\n",
			CONFIG_MSM_KGSL_PAGE_TABLE_SIZE);

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

unsigned int kgsl_mmu_get_current_ptbase(struct kgsl_device *device)
{
	struct kgsl_mmu *mmu = &device->mmu;
	if (KGSL_MMU_TYPE_NONE == kgsl_mmu_type)
		return 0;
	else
		return mmu->mmu_ops->mmu_get_current_ptbase(device);
}
EXPORT_SYMBOL(kgsl_mmu_get_current_ptbase);

int
kgsl_mmu_get_ptname_from_ptbase(unsigned int pt_base)
{
	struct kgsl_pagetable *pt;
	int ptid = -1;

	spin_lock(&kgsl_driver.ptlock);
	list_for_each_entry(pt, &kgsl_driver.pagetable_list, list) {
		if (pt->pt_ops->mmu_pt_equal(pt, pt_base)) {
			ptid = (int) pt->name;
			break;
		}
	}
	spin_unlock(&kgsl_driver.ptlock);

	return ptid;
}
EXPORT_SYMBOL(kgsl_mmu_get_ptname_from_ptbase);

void kgsl_mmu_setstate(struct kgsl_device *device,
			struct kgsl_pagetable *pagetable)
{
	struct kgsl_mmu *mmu = &device->mmu;

	if (KGSL_MMU_TYPE_NONE == kgsl_mmu_type)
		return;
	else
		mmu->mmu_ops->mmu_setstate(device,
					pagetable);
}
EXPORT_SYMBOL(kgsl_mmu_setstate);

int kgsl_mmu_init(struct kgsl_device *device)
{
	struct kgsl_mmu *mmu = &device->mmu;

	mmu->device = device;

	if (KGSL_MMU_TYPE_NONE == kgsl_mmu_type) {
		dev_info(device->dev, "|%s| MMU type set for device is "
			"NOMMU\n", __func__);
		return 0;
	} else if (KGSL_MMU_TYPE_GPU == kgsl_mmu_type)
		mmu->mmu_ops = &gpummu_ops;
	else if (KGSL_MMU_TYPE_IOMMU == kgsl_mmu_type)
		mmu->mmu_ops = &iommu_ops;

	return mmu->mmu_ops->mmu_init(device);
}
EXPORT_SYMBOL(kgsl_mmu_init);

int kgsl_mmu_start(struct kgsl_device *device)
{
	struct kgsl_mmu *mmu = &device->mmu;

	if (kgsl_mmu_type == KGSL_MMU_TYPE_NONE) {
		kgsl_regwrite(device, MH_MMU_CONFIG, 0);
		return 0;
	} else {
		return mmu->mmu_ops->mmu_start(device);
	}
}
EXPORT_SYMBOL(kgsl_mmu_start);

void kgsl_mh_intrcallback(struct kgsl_device *device)
{
	unsigned int status = 0;
	unsigned int reg;

	kgsl_regread(device, MH_INTERRUPT_STATUS, &status);
	kgsl_regread(device, MH_AXI_ERROR, &reg);

	if (status & MH_INTERRUPT_MASK__AXI_READ_ERROR)
		KGSL_MEM_CRIT(device, "axi read error interrupt: %08x\n", reg);
	if (status & MH_INTERRUPT_MASK__AXI_WRITE_ERROR)
		KGSL_MEM_CRIT(device, "axi write error interrupt: %08x\n", reg);
	if (status & MH_INTERRUPT_MASK__MMU_PAGE_FAULT)
		device->mmu.mmu_ops->mmu_pagefault(device);

	status &= KGSL_MMU_INT_MASK;
	kgsl_regwrite(device, MH_INTERRUPT_CLEAR, status);
}
EXPORT_SYMBOL(kgsl_mh_intrcallback);

static int kgsl_setup_pt(struct kgsl_pagetable *pt)
{
	int i = 0;
	int status = 0;

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

static struct kgsl_pagetable *kgsl_mmu_createpagetableobject(
				unsigned int name)
{
	int status = 0;
	struct kgsl_pagetable *pagetable = NULL;
	unsigned long flags;

	pagetable = kzalloc(sizeof(struct kgsl_pagetable), GFP_KERNEL);
	if (pagetable == NULL) {
		KGSL_CORE_ERR("kzalloc(%d) failed\n",
			sizeof(struct kgsl_pagetable));
		return NULL;
	}

	kref_init(&pagetable->refcount);

	spin_lock_init(&pagetable->lock);
	pagetable->name = name;
	pagetable->max_entries = KGSL_PAGETABLE_ENTRIES(
					CONFIG_MSM_KGSL_PAGE_TABLE_SIZE);

	pagetable->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (pagetable->pool == NULL) {
		KGSL_CORE_ERR("gen_pool_create(%d) failed\n", PAGE_SHIFT);
		goto err_alloc;
	}

	if (gen_pool_add(pagetable->pool, KGSL_PAGETABLE_BASE,
				CONFIG_MSM_KGSL_PAGE_TABLE_SIZE, -1)) {
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
err_alloc:
	kfree(pagetable);

	return NULL;
}

struct kgsl_pagetable *kgsl_mmu_getpagetable(unsigned long name)
{
	struct kgsl_pagetable *pt;

	if (KGSL_MMU_TYPE_NONE == kgsl_mmu_type)
		return (void *)(-1);

#ifdef CONFIG_KGSL_PER_PROCESS_PAGE_TABLE
	if (KGSL_MMU_TYPE_IOMMU == kgsl_mmu_type)
		name = KGSL_MMU_GLOBAL_PT;
#else
		name = KGSL_MMU_GLOBAL_PT;
#endif
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

void kgsl_setstate(struct kgsl_device *device, uint32_t flags)
{
	struct kgsl_mmu *mmu = &device->mmu;
	if (KGSL_MMU_TYPE_NONE == kgsl_mmu_type)
		return;
	else if (device->ftbl->setstate)
		device->ftbl->setstate(device, flags);
	else if (mmu->mmu_ops->mmu_device_setstate)
		mmu->mmu_ops->mmu_device_setstate(device, flags);
}
EXPORT_SYMBOL(kgsl_setstate);

void kgsl_mmu_device_setstate(struct kgsl_device *device, uint32_t flags)
{
	struct kgsl_mmu *mmu = &device->mmu;
	if (KGSL_MMU_TYPE_NONE == kgsl_mmu_type)
		return;
	else if (mmu->mmu_ops->mmu_device_setstate)
		mmu->mmu_ops->mmu_device_setstate(device, flags);
}
EXPORT_SYMBOL(kgsl_mmu_device_setstate);

void kgsl_mh_start(struct kgsl_device *device)
{
	struct kgsl_mh *mh = &device->mh;
	/* force mmu off to for now*/
	kgsl_regwrite(device, MH_MMU_CONFIG, 0);
	kgsl_idle(device,  KGSL_TIMEOUT_DEFAULT);

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

int
kgsl_mmu_map(struct kgsl_pagetable *pagetable,
				struct kgsl_memdesc *memdesc,
				unsigned int protflags)
{
	int ret;

	if (kgsl_mmu_type == KGSL_MMU_TYPE_NONE) {
		memdesc->gpuaddr = memdesc->physaddr;
		return 0;
	}
	memdesc->gpuaddr = gen_pool_alloc_aligned(pagetable->pool,
		memdesc->size, KGSL_MMU_ALIGN_SHIFT);

	if (memdesc->gpuaddr == 0) {
		KGSL_CORE_ERR("gen_pool_alloc(%d) failed\n", memdesc->size);
		KGSL_CORE_ERR(" [%d] allocated=%d, entries=%d\n",
				pagetable->name, pagetable->stats.mapped,
				pagetable->stats.entries);
		return -ENOMEM;
	}

	spin_lock(&pagetable->lock);
	ret = pagetable->pt_ops->mmu_map(pagetable->priv, memdesc, protflags);

	if (ret)
		goto err_free_gpuaddr;

	/* Keep track of the statistics for the sysfs files */

	KGSL_STATS_ADD(1, pagetable->stats.entries,
		       pagetable->stats.max_entries);

	KGSL_STATS_ADD(memdesc->size, pagetable->stats.mapped,
		       pagetable->stats.max_mapped);

	spin_unlock(&pagetable->lock);

	return 0;

err_free_gpuaddr:
	spin_unlock(&pagetable->lock);
	gen_pool_free(pagetable->pool, memdesc->gpuaddr, memdesc->size);
	memdesc->gpuaddr = 0;
	return ret;
}
EXPORT_SYMBOL(kgsl_mmu_map);

int
kgsl_mmu_unmap(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc)
{
	if (memdesc->size == 0 || memdesc->gpuaddr == 0)
		return 0;

	if (kgsl_mmu_type == KGSL_MMU_TYPE_NONE) {
		memdesc->gpuaddr = 0;
		return 0;
	}
	spin_lock(&pagetable->lock);
	pagetable->pt_ops->mmu_unmap(pagetable->priv, memdesc);
	/* Remove the statistics */
	pagetable->stats.entries--;
	pagetable->stats.mapped -= memdesc->size;

	spin_unlock(&pagetable->lock);

	gen_pool_free(pagetable->pool,
			memdesc->gpuaddr & KGSL_MMU_ALIGN_MASK,
			memdesc->size);

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

int kgsl_mmu_stop(struct kgsl_device *device)
{
	struct kgsl_mmu *mmu = &device->mmu;

	if (kgsl_mmu_type == KGSL_MMU_TYPE_NONE)
		return 0;
	else
		return mmu->mmu_ops->mmu_stop(device);
}
EXPORT_SYMBOL(kgsl_mmu_stop);

int kgsl_mmu_close(struct kgsl_device *device)
{
	struct kgsl_mmu *mmu = &device->mmu;

	if (kgsl_mmu_type == KGSL_MMU_TYPE_NONE)
		return 0;
	else
		return mmu->mmu_ops->mmu_close(device);
}
EXPORT_SYMBOL(kgsl_mmu_close);

int kgsl_mmu_pt_get_flags(struct kgsl_pagetable *pt,
			enum kgsl_deviceid id)
{
	if (KGSL_MMU_TYPE_GPU == kgsl_mmu_type)
		return pt->pt_ops->mmu_pt_get_flags(pt, id);
	else
		return 0;
}
EXPORT_SYMBOL(kgsl_mmu_pt_get_flags);

void kgsl_mmu_ptpool_destroy(void *ptpool)
{
	if (KGSL_MMU_TYPE_GPU == kgsl_mmu_type)
		kgsl_gpummu_ptpool_destroy(ptpool);
	ptpool = 0;
}
EXPORT_SYMBOL(kgsl_mmu_ptpool_destroy);

void *kgsl_mmu_ptpool_init(int ptsize, int entries)
{
	if (KGSL_MMU_TYPE_GPU == kgsl_mmu_type)
		return kgsl_gpummu_ptpool_init(ptsize, entries);
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

int kgsl_mmu_pt_equal(struct kgsl_pagetable *pt,
			unsigned int pt_base)
{
	if (KGSL_MMU_TYPE_NONE == kgsl_mmu_type)
		return true;
	else
		return pt->pt_ops->mmu_pt_equal(pt, pt_base);
}
EXPORT_SYMBOL(kgsl_mmu_pt_equal);

enum kgsl_mmutype kgsl_mmu_get_mmutype(void)
{
	return kgsl_mmu_type;
}
EXPORT_SYMBOL(kgsl_mmu_get_mmutype);

void kgsl_mmu_set_mmutype(char *mmutype)
{
	kgsl_mmu_type = iommu_found() ? KGSL_MMU_TYPE_IOMMU : KGSL_MMU_TYPE_GPU;
	if (mmutype && !strncmp(mmutype, "gpummu", 6))
		kgsl_mmu_type = KGSL_MMU_TYPE_GPU;
	if (iommu_found() && mmutype && !strncmp(mmutype, "iommu", 5))
		kgsl_mmu_type = KGSL_MMU_TYPE_IOMMU;
	if (mmutype && !strncmp(mmutype, "nommu", 5))
		kgsl_mmu_type = KGSL_MMU_TYPE_NONE;
}
EXPORT_SYMBOL(kgsl_mmu_set_mmutype);
