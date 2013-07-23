/* Copyright (c) 2002,2007-2013, The Linux Foundation. All rights reserved.
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
#include "kgsl_gpummu.h"
#include "kgsl_device.h"
#include "kgsl_sharedmem.h"
#include "adreno.h"

static enum kgsl_mmutype kgsl_mmu_type;

static void pagetable_remove_sysfs_objects(struct kgsl_pagetable *pagetable);

static int kgsl_cleanup_pt(struct kgsl_pagetable *pt)
{
	int i;
	struct kgsl_device *device;

	for (i = 0; i < KGSL_DEVICE_MAX; i++) {
		device = kgsl_driver.devp[i];
		if (device)
			device->ftbl->cleanup_pt(device, pt);
	}
	/* Only the 3d device needs mmu specific pt entries */
	device = kgsl_driver.devp[KGSL_DEVICE_3D0];
	if (device->mmu.mmu_ops->mmu_cleanup_pt != NULL)
		device->mmu.mmu_ops->mmu_cleanup_pt(&device->mmu, pt);

	return 0;
}


static int kgsl_setup_pt(struct kgsl_pagetable *pt)
{
	int i = 0;
	int status = 0;
	struct kgsl_device *device;

	for (i = 0; i < KGSL_DEVICE_MAX; i++) {
		device = kgsl_driver.devp[i];
		if (device) {
			status = device->ftbl->setup_pt(device, pt);
			if (status)
				goto error_pt;
		}
	}
	/* Only the 3d device needs mmu specific pt entries */
	device = kgsl_driver.devp[KGSL_DEVICE_3D0];
	if (device->mmu.mmu_ops->mmu_setup_pt != NULL) {
		status = device->mmu.mmu_ops->mmu_setup_pt(&device->mmu, pt);
		if (status) {
			i = KGSL_DEVICE_MAX - 1;
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

	pagetable->pt_ops->mmu_destroy_pagetable(pagetable);

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
			kgsl_mmu_get_ptsize(pt->mmu));
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

int
kgsl_mmu_get_ptname_from_ptbase(struct kgsl_mmu *mmu, phys_addr_t pt_base)
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

unsigned int
kgsl_mmu_log_fault_addr(struct kgsl_mmu *mmu, phys_addr_t pt_base,
					unsigned int addr)
{
	struct kgsl_pagetable *pt;
	unsigned int ret = 0;

	if (!mmu->mmu_ops || !mmu->mmu_ops->mmu_pt_equal)
		return 0;
	spin_lock(&kgsl_driver.ptlock);
	list_for_each_entry(pt, &kgsl_driver.pagetable_list, list) {
		if (mmu->mmu_ops->mmu_pt_equal(mmu, pt, pt_base)) {
			if ((addr & ~(PAGE_SIZE-1)) == pt->fault_addr) {
				ret = 1;
				break;
			} else {
				pt->fault_addr = (addr & ~(PAGE_SIZE-1));
				ret = 0;
				break;
			}

		}
	}
	spin_unlock(&kgsl_driver.ptlock);

	return ret;
}
EXPORT_SYMBOL(kgsl_mmu_log_fault_addr);

int kgsl_mmu_init(struct kgsl_device *device)
{
	int status = 0;
	struct kgsl_mmu *mmu = &device->mmu;

	mmu->device = device;
	status = kgsl_allocate_contiguous(&mmu->setstate_memory, PAGE_SIZE);
	if (status)
		return status;
	kgsl_sharedmem_set(device, &mmu->setstate_memory, 0, 0,
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
	unsigned int reg, gpu_err, phys_err;
	phys_addr_t pt_base;

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
			"axi %s error: %08x pt %pa gpu %08x phys %08x\n",
			type, reg, &pt_base, gpu_err, phys_err);
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

static struct kgsl_pagetable *
kgsl_mmu_createpagetableobject(struct kgsl_mmu *mmu,
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

	ptsize = kgsl_mmu_get_ptsize(mmu);
	pagetable->mmu = mmu;
	pagetable->name = name;
	pagetable->max_entries = KGSL_PAGETABLE_ENTRIES(ptsize);
	pagetable->fault_addr = 0xFFFFFFFF;

	/*
	 * create a separate kgsl pool for IOMMU, global mappings can be mapped
	 * just once from this pool of the defaultpagetable
	 */
	if ((KGSL_MMU_TYPE_IOMMU == kgsl_mmu_get_mmutype()) &&
		((KGSL_MMU_GLOBAL_PT == name) ||
		(KGSL_MMU_PRIV_BANK_TABLE_NAME == name))) {
		pagetable->kgsl_pool = gen_pool_create(ilog2(SZ_8K), -1);
		if (pagetable->kgsl_pool == NULL) {
			KGSL_CORE_ERR("gen_pool_create(%d) failed\n",
					ilog2(SZ_8K));
			goto err_alloc;
		}
		if (gen_pool_add(pagetable->kgsl_pool,
			KGSL_IOMMU_GLOBAL_MEM_BASE,
			KGSL_IOMMU_GLOBAL_MEM_SIZE, -1)) {
			KGSL_CORE_ERR("gen_pool_add failed\n");
			goto err_kgsl_pool;
		}
	}

	pagetable->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (pagetable->pool == NULL) {
		KGSL_CORE_ERR("gen_pool_create(%d) failed\n",
			      PAGE_SHIFT);
		goto err_kgsl_pool;
	}

	if (gen_pool_add(pagetable->pool, kgsl_mmu_get_base_addr(mmu),
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
	pagetable->pt_ops->mmu_destroy_pagetable(pagetable);
err_pool:
	gen_pool_destroy(pagetable->pool);
err_kgsl_pool:
	if (pagetable->kgsl_pool)
		gen_pool_destroy(pagetable->kgsl_pool);
err_alloc:
	kfree(pagetable);

	return NULL;
}

struct kgsl_pagetable *kgsl_mmu_getpagetable(struct kgsl_mmu *mmu,
						unsigned long name)
{
	struct kgsl_pagetable *pt;

	if (KGSL_MMU_TYPE_NONE == kgsl_mmu_type)
		return (void *)(-1);

	if (!kgsl_mmu_is_perprocess(mmu))
		name = KGSL_MMU_GLOBAL_PT;

	pt = kgsl_get_pagetable(name);

	if (pt == NULL)
		pt = kgsl_mmu_createpagetableobject(mmu, name);

	return pt;
}

void kgsl_mmu_putpagetable(struct kgsl_pagetable *pagetable)
{
	kgsl_put_pagetable(pagetable);
}
EXPORT_SYMBOL(kgsl_mmu_putpagetable);

int kgsl_setstate(struct kgsl_mmu *mmu, unsigned int context_id,
			uint32_t flags)
{
	struct kgsl_device *device = mmu->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (!(flags & (KGSL_MMUFLAGS_TLBFLUSH | KGSL_MMUFLAGS_PTUPDATE))
		&& !adreno_is_a2xx(adreno_dev))
		return 0;

	if (KGSL_MMU_TYPE_NONE == kgsl_mmu_type)
		return 0;
	else if (device->ftbl->setstate)
		return device->ftbl->setstate(device, context_id, flags);
	else if (mmu->mmu_ops->mmu_device_setstate)
		return mmu->mmu_ops->mmu_device_setstate(mmu, flags);

	return 0;
}
EXPORT_SYMBOL(kgsl_setstate);

void kgsl_mh_start(struct kgsl_device *device)
{
	struct kgsl_mh *mh = &device->mh;
	/* force mmu off to for now*/
	kgsl_regwrite(device, MH_MMU_CONFIG, 0);

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
EXPORT_SYMBOL(kgsl_mh_start);

/**
 * kgsl_mmu_get_gpuaddr - Assign a memdesc with a gpuadddr from the gen pool
 * @pagetable - pagetable whose pool is to be used
 * @memdesc - memdesc to which gpuaddr is assigned
 *
 * returns - 0 on success else error code
 */
int
kgsl_mmu_get_gpuaddr(struct kgsl_pagetable *pagetable,
			struct kgsl_memdesc *memdesc)
{
	struct gen_pool *pool = NULL;
	int size;
	int page_align = ilog2(PAGE_SIZE);

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

	/* Add space for the guard page when allocating the mmu VA. */
	size = memdesc->size;
	if (kgsl_memdesc_has_guard_page(memdesc))
		size += PAGE_SIZE;

	pool = pagetable->pool;

	if (KGSL_MMU_TYPE_IOMMU == kgsl_mmu_get_mmutype()) {
		/* Allocate aligned virtual addresses for iommu. This allows
		 * more efficient pagetable entries if the physical memory
		 * is also aligned. Don't do this for GPUMMU, because
		 * the address space is so small.
		 */
		if (kgsl_memdesc_get_align(memdesc) > 0)
			page_align = kgsl_memdesc_get_align(memdesc);
		if (kgsl_memdesc_is_global(memdesc)) {
			/*
			 * Only the default pagetable has a kgsl_pool, and
			 * it is responsible for creating the mapping for
			 * each global buffer. The mapping will be reused
			 * in all other pagetables and it must already exist
			 * when we're creating other pagetables which do not
			 * have a kgsl_pool.
			 */
			pool = pagetable->kgsl_pool;
			if (pool == NULL && memdesc->gpuaddr == 0) {
				KGSL_CORE_ERR(
				  "No address for global mapping into pt %d\n",
				  pagetable->name);
				return -EINVAL;
			}
		} else if (kgsl_memdesc_use_cpu_map(memdesc)) {
			if (memdesc->gpuaddr == 0)
				return -EINVAL;
			pool = NULL;
		}
	}
	if (pool) {
		memdesc->gpuaddr = gen_pool_alloc_aligned(pool, size,
							  page_align);
		if (memdesc->gpuaddr == 0) {
			KGSL_CORE_ERR("gen_pool_alloc(%d) failed, pool: %s\n",
					size,
					(pool == pagetable->kgsl_pool) ?
					"kgsl_pool" : "general_pool");
			KGSL_CORE_ERR(" [%d] allocated=%d, entries=%d\n",
					pagetable->name,
					pagetable->stats.mapped,
					pagetable->stats.entries);
			return -ENOMEM;
		}
	}
	return 0;
}
EXPORT_SYMBOL(kgsl_mmu_get_gpuaddr);

int
kgsl_mmu_map(struct kgsl_pagetable *pagetable,
				struct kgsl_memdesc *memdesc)
{
	int ret = 0;
	int size;
	unsigned int protflags = kgsl_memdesc_protflags(memdesc);

	if (!memdesc->gpuaddr)
		return -EINVAL;
	/* Only global mappings should be mapped multiple times */
	if (!kgsl_memdesc_is_global(memdesc) &&
		(KGSL_MEMDESC_MAPPED & memdesc->priv))
		return -EINVAL;
	/* Add space for the guard page when allocating the mmu VA. */
	size = memdesc->size;
	if (kgsl_memdesc_has_guard_page(memdesc))
		size += PAGE_SIZE;

	if (KGSL_MMU_TYPE_IOMMU != kgsl_mmu_get_mmutype())
		spin_lock(&pagetable->lock);
	ret = pagetable->pt_ops->mmu_map(pagetable, memdesc, protflags,
						&pagetable->tlb_flags);
	if (KGSL_MMU_TYPE_IOMMU == kgsl_mmu_get_mmutype())
		spin_lock(&pagetable->lock);

	if (ret)
		goto done;

	/* Keep track of the statistics for the sysfs files */

	KGSL_STATS_ADD(1, pagetable->stats.entries,
		       pagetable->stats.max_entries);

	KGSL_STATS_ADD(size, pagetable->stats.mapped,
		       pagetable->stats.max_mapped);

	spin_unlock(&pagetable->lock);
	memdesc->priv |= KGSL_MEMDESC_MAPPED;

	return 0;

done:
	spin_unlock(&pagetable->lock);
	return ret;
}
EXPORT_SYMBOL(kgsl_mmu_map);

/**
 * kgsl_mmu_put_gpuaddr - Free a gpuaddress from memory pool
 * @pagetable - pagetable whose pool memory is freed from
 * @memdesc - memdesc whose gpuaddress is freed
 *
 * returns - 0 on success else error code
 */
int
kgsl_mmu_put_gpuaddr(struct kgsl_pagetable *pagetable,
			struct kgsl_memdesc *memdesc)
{
	struct gen_pool *pool;
	int size;

	if (memdesc->size == 0 || memdesc->gpuaddr == 0)
		return 0;

	if (kgsl_mmu_type == KGSL_MMU_TYPE_NONE)
		goto done;

	/* Add space for the guard page when freeing the mmu VA. */
	size = memdesc->size;
	if (kgsl_memdesc_has_guard_page(memdesc))
		size += PAGE_SIZE;

	pool = pagetable->pool;

	if (KGSL_MMU_TYPE_IOMMU == kgsl_mmu_get_mmutype()) {
		if (kgsl_memdesc_is_global(memdesc))
			pool = pagetable->kgsl_pool;
		else if (kgsl_memdesc_use_cpu_map(memdesc))
			pool = NULL;
	}
	if (pool)
		gen_pool_free(pool, memdesc->gpuaddr, size);
	/*
	 * Don't clear the gpuaddr on global mappings because they
	 * may be in use by other pagetables
	 */
done:
	if (!kgsl_memdesc_is_global(memdesc))
		memdesc->gpuaddr = 0;
	return 0;
}
EXPORT_SYMBOL(kgsl_mmu_put_gpuaddr);

int
kgsl_mmu_unmap(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc)
{
	int size;
	unsigned int start_addr = 0;
	unsigned int end_addr = 0;

	if (memdesc->size == 0 || memdesc->gpuaddr == 0 ||
		!(KGSL_MEMDESC_MAPPED & memdesc->priv))
		return -EINVAL;

	if (kgsl_mmu_type == KGSL_MMU_TYPE_NONE)
		return 0;

	/* Add space for the guard page when freeing the mmu VA. */
	size = memdesc->size;
	if (kgsl_memdesc_has_guard_page(memdesc))
		size += PAGE_SIZE;

	start_addr = memdesc->gpuaddr;
	end_addr = (memdesc->gpuaddr + size);

	if (KGSL_MMU_TYPE_IOMMU != kgsl_mmu_get_mmutype())
		spin_lock(&pagetable->lock);
	pagetable->pt_ops->mmu_unmap(pagetable, memdesc,
					&pagetable->tlb_flags);

	/* If buffer is unmapped 0 fault addr */
	if ((pagetable->fault_addr >= start_addr) &&
		(pagetable->fault_addr < end_addr))
		pagetable->fault_addr = 0;

	if (KGSL_MMU_TYPE_IOMMU == kgsl_mmu_get_mmutype())
		spin_lock(&pagetable->lock);
	/* Remove the statistics */
	pagetable->stats.entries--;
	pagetable->stats.mapped -= size;

	spin_unlock(&pagetable->lock);
	if (!kgsl_memdesc_is_global(memdesc))
		memdesc->priv &= ~KGSL_MEMDESC_MAPPED;
	return 0;
}
EXPORT_SYMBOL(kgsl_mmu_unmap);

int kgsl_mmu_map_global(struct kgsl_pagetable *pagetable,
			struct kgsl_memdesc *memdesc)
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
	memdesc->priv |= KGSL_MEMDESC_GLOBAL;

	result = kgsl_mmu_get_gpuaddr(pagetable, memdesc);
	if (result)
		goto error;
	result = kgsl_mmu_map(pagetable, memdesc);
	if (result)
		goto error_put_gpuaddr;

	/*global mappings must have the same gpu address in all pagetables*/
	if (gpuaddr && gpuaddr != memdesc->gpuaddr) {
		KGSL_CORE_ERR("pt %p addr mismatch phys %pa gpu 0x%0x 0x%08x",
		     pagetable, &memdesc->physaddr, gpuaddr, memdesc->gpuaddr);
		goto error_unmap;
	}
	return result;
error_unmap:
	kgsl_mmu_unmap(pagetable, memdesc);
error_put_gpuaddr:
	kgsl_mmu_put_gpuaddr(pagetable, memdesc);
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

int kgsl_mmu_gpuaddr_in_range(struct kgsl_pagetable *pt, unsigned int gpuaddr)
{
	if (KGSL_MMU_TYPE_NONE == kgsl_mmu_type)
		return 1;
	if (gpuaddr >= kgsl_mmu_get_base_addr(pt->mmu) &&
		gpuaddr < kgsl_mmu_get_base_addr(pt->mmu) +
		kgsl_mmu_get_ptsize(pt->mmu))
		return 1;
	if (kgsl_mmu_get_mmutype() == KGSL_MMU_TYPE_IOMMU
		&& kgsl_mmu_is_perprocess(pt->mmu))
		return (gpuaddr > 0 && gpuaddr < TASK_SIZE);
	return 0;
}
EXPORT_SYMBOL(kgsl_mmu_gpuaddr_in_range);

