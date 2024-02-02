/* Copyright (c) 2002,2007-2017,2020, The Linux Foundation. All rights reserved.
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
#include <linux/types.h>

#include "kgsl.h"
#include "kgsl_mmu.h"
#include "kgsl_device.h"
#include "kgsl_sharedmem.h"

static void pagetable_remove_sysfs_objects(struct kgsl_pagetable *pagetable);

static void _deferred_destroy(struct work_struct *ws)
{
	struct kgsl_pagetable *pagetable = container_of(ws,
					struct kgsl_pagetable, destroy_ws);

	if (PT_OP_VALID(pagetable, mmu_destroy_pagetable))
		pagetable->pt_ops->mmu_destroy_pagetable(pagetable);

	kfree(pagetable);
}

static void kgsl_destroy_pagetable(struct kref *kref)
{
	struct kgsl_pagetable *pagetable = container_of(kref,
		struct kgsl_pagetable, refcount);

	kgsl_mmu_detach_pagetable(pagetable);

	kgsl_schedule_work(&pagetable->destroy_ws);
}

static inline void kgsl_put_pagetable(struct kgsl_pagetable *pagetable)
{
	if (pagetable)
		kref_put(&pagetable->refcount, kgsl_destroy_pagetable);
}

struct kgsl_pagetable *
kgsl_get_pagetable(unsigned long name)
{
	struct kgsl_pagetable *pt, *ret = NULL;
	unsigned long flags;

	spin_lock_irqsave(&kgsl_driver.ptlock, flags);
	list_for_each_entry(pt, &kgsl_driver.pagetable_list, list) {
		if (name == pt->name && kref_get_unless_zero(&pt->refcount)) {
			ret = pt;
			break;
		}
	}

	spin_unlock_irqrestore(&kgsl_driver.ptlock, flags);
	return ret;
}

static struct kgsl_pagetable *
_get_pt_from_kobj(struct kobject *kobj)
{
	unsigned int ptname;

	if (!kobj)
		return NULL;

	if (kstrtou32(kobj->name, 0, &ptname))
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

	if (pt) {
		unsigned int val = atomic_read(&pt->stats.entries);

		ret += snprintf(buf, PAGE_SIZE, "%d\n", val);
	}

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

	if (pt) {
		uint64_t val = atomic_long_read(&pt->stats.mapped);

		ret += snprintf(buf, PAGE_SIZE, "%llu\n", val);
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

	if (pt) {
		uint64_t val = atomic_long_read(&pt->stats.max_mapped);

		ret += snprintf(buf, PAGE_SIZE, "%llu\n", val);
	}

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

static struct kobj_attribute attr_max_mapped = {
	.attr = { .name = "max_mapped", .mode = 0444 },
	.show = sysfs_show_max_mapped,
	.store = NULL,
};

static struct attribute *pagetable_attrs[] = {
	&attr_entries.attr,
	&attr_mapped.attr,
	&attr_max_mapped.attr,
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
	pagetable->kobj = NULL;
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

void
kgsl_mmu_detach_pagetable(struct kgsl_pagetable *pagetable)
{
	unsigned long flags;

	spin_lock_irqsave(&kgsl_driver.ptlock, flags);

	if (!list_empty(&pagetable->list))
		list_del_init(&pagetable->list);

	spin_unlock_irqrestore(&kgsl_driver.ptlock, flags);

	pagetable_remove_sysfs_objects(pagetable);
}

struct kgsl_pagetable *kgsl_mmu_get_pt_from_ptname(struct kgsl_mmu *mmu,
						int ptname)
{
	struct kgsl_pagetable *pt;

	spin_lock(&kgsl_driver.ptlock);
	list_for_each_entry(pt, &kgsl_driver.pagetable_list, list) {
		if (pt->name == ptname) {
			spin_unlock(&kgsl_driver.ptlock);
			return pt;
		}
	}
	spin_unlock(&kgsl_driver.ptlock);
	return NULL;

}
EXPORT_SYMBOL(kgsl_mmu_get_pt_from_ptname);

unsigned int
kgsl_mmu_log_fault_addr(struct kgsl_mmu *mmu, u64 pt_base,
		uint64_t addr)
{
	struct kgsl_pagetable *pt;
	unsigned int ret = 0;

	if (!MMU_OP_VALID(mmu, mmu_pt_equal))
		return 0;

	spin_lock(&kgsl_driver.ptlock);
	list_for_each_entry(pt, &kgsl_driver.pagetable_list, list) {
		if (mmu->mmu_ops->mmu_pt_equal(mmu, pt, pt_base)) {
			if ((addr & ~(PAGE_SIZE-1)) == pt->fault_addr) {
				ret = 1;
				break;
			}
			pt->fault_addr = (addr & ~(PAGE_SIZE-1));
			ret = 0;
			break;
		}
	}
	spin_unlock(&kgsl_driver.ptlock);

	return ret;
}
EXPORT_SYMBOL(kgsl_mmu_log_fault_addr);

int kgsl_mmu_init(struct kgsl_device *device)
{
	struct kgsl_mmu *mmu = &device->mmu;

	if (MMU_OP_VALID(mmu, mmu_init))
		return mmu->mmu_ops->mmu_init(mmu);

	return 0;
}
EXPORT_SYMBOL(kgsl_mmu_init);

int kgsl_mmu_start(struct kgsl_device *device)
{
	struct kgsl_mmu *mmu = &device->mmu;

	if (MMU_OP_VALID(mmu, mmu_start))
		return mmu->mmu_ops->mmu_start(mmu);

	return 0;
}
EXPORT_SYMBOL(kgsl_mmu_start);

struct kgsl_pagetable *
kgsl_mmu_createpagetableobject(struct kgsl_mmu *mmu, unsigned int name)
{
	int status = 0;
	struct kgsl_pagetable *pagetable = NULL;
	unsigned long flags;

	pagetable = kzalloc(sizeof(struct kgsl_pagetable), GFP_KERNEL);
	if (pagetable == NULL)
		return ERR_PTR(-ENOMEM);

	kref_init(&pagetable->refcount);

	spin_lock_init(&pagetable->lock);
	INIT_WORK(&pagetable->destroy_ws, _deferred_destroy);

	pagetable->mmu = mmu;
	pagetable->name = name;

	atomic_set(&pagetable->stats.entries, 0);
	atomic_long_set(&pagetable->stats.mapped, 0);
	atomic_long_set(&pagetable->stats.max_mapped, 0);

	if (MMU_OP_VALID(mmu, mmu_init_pt)) {
		status = mmu->mmu_ops->mmu_init_pt(mmu, pagetable);
		if (status) {
			kfree(pagetable);
			return ERR_PTR(status);
		}
	}

	spin_lock_irqsave(&kgsl_driver.ptlock, flags);
	list_add(&pagetable->list, &kgsl_driver.pagetable_list);
	spin_unlock_irqrestore(&kgsl_driver.ptlock, flags);

	/* Create the sysfs entries */
	pagetable_add_sysfs_objects(pagetable);

	return pagetable;
}

void kgsl_mmu_putpagetable(struct kgsl_pagetable *pagetable)
{
	kgsl_put_pagetable(pagetable);
}
EXPORT_SYMBOL(kgsl_mmu_putpagetable);

/**
 * kgsl_mmu_find_svm_region() - Find a empty spot in the SVM region
 * @pagetable: KGSL pagetable to search
 * @start: start of search range, must be within kgsl_mmu_svm_range()
 * @end: end of search range, must be within kgsl_mmu_svm_range()
 * @size: Size of the region to find
 * @align: Desired alignment of the address
 */
uint64_t kgsl_mmu_find_svm_region(struct kgsl_pagetable *pagetable,
		uint64_t start, uint64_t end, uint64_t size,
		uint64_t align)
{
	if (PT_OP_VALID(pagetable, find_svm_region))
		return pagetable->pt_ops->find_svm_region(pagetable, start,
			end, size, align);
	return -ENOMEM;
}

/**
 * kgsl_mmu_set_svm_region() - Check if a region is empty and reserve it if so
 * @pagetable: KGSL pagetable to search
 * @gpuaddr: GPU address to check/reserve
 * @size: Size of the region to check/reserve
 */
int kgsl_mmu_set_svm_region(struct kgsl_pagetable *pagetable, uint64_t gpuaddr,
		uint64_t size)
{
	if (PT_OP_VALID(pagetable, set_svm_region))
		return pagetable->pt_ops->set_svm_region(pagetable, gpuaddr,
			size);
	return -ENOMEM;
}

/**
 * kgsl_mmu_get_gpuaddr() - Assign a GPU address to the memdesc
 * @pagetable: GPU pagetable to assign the address in
 * @memdesc: mem descriptor to assign the memory to
 */
int
kgsl_mmu_get_gpuaddr(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc)
{
	if (PT_OP_VALID(pagetable, get_gpuaddr))
		return pagetable->pt_ops->get_gpuaddr(pagetable, memdesc);

	return -ENOMEM;
}
EXPORT_SYMBOL(kgsl_mmu_get_gpuaddr);

int
kgsl_mmu_map(struct kgsl_pagetable *pagetable,
				struct kgsl_memdesc *memdesc)
{
	int size;

	if (!memdesc->gpuaddr)
		return -EINVAL;
	if (!(memdesc->flags & (KGSL_MEMFLAGS_SPARSE_VIRT |
					KGSL_MEMFLAGS_SPARSE_PHYS))) {
		/* Only global mappings should be mapped multiple times */
		if (!kgsl_memdesc_is_global(memdesc) &&
				(KGSL_MEMDESC_MAPPED & memdesc->priv))
			return -EINVAL;
	}

	size = kgsl_memdesc_footprint(memdesc);

	if (PT_OP_VALID(pagetable, mmu_map)) {
		int ret;

		ret = pagetable->pt_ops->mmu_map(pagetable, memdesc);
		if (ret)
			return ret;

		atomic_inc(&pagetable->stats.entries);
		KGSL_STATS_ADD(size, &pagetable->stats.mapped,
				&pagetable->stats.max_mapped);

		/* This is needed for non-sparse mappings */
		memdesc->priv |= KGSL_MEMDESC_MAPPED;
	}

	return 0;
}
EXPORT_SYMBOL(kgsl_mmu_map);

/**
 * kgsl_mmu_put_gpuaddr() - Remove a GPU address from a pagetable
 * @pagetable: Pagetable to release the memory from
 * @memdesc: Memory descriptor containing the GPU address to free
 */
void kgsl_mmu_put_gpuaddr(struct kgsl_memdesc *memdesc)
{
	struct kgsl_pagetable *pagetable = memdesc->pagetable;
	int unmap_fail = 0;

	if (memdesc->size == 0 || memdesc->gpuaddr == 0)
		return;

	if (!kgsl_memdesc_is_global(memdesc))
		unmap_fail = kgsl_mmu_unmap(pagetable, memdesc);

	/*
	 * Do not free the gpuaddr/size if unmap fails. Because if we
	 * try to map this range in future, the iommu driver will throw
	 * a BUG_ON() because it feels we are overwriting a mapping.
	 */
	if (PT_OP_VALID(pagetable, put_gpuaddr) && (unmap_fail == 0))
		pagetable->pt_ops->put_gpuaddr(memdesc);

	if (!kgsl_memdesc_is_global(memdesc))
		memdesc->gpuaddr = 0;

	memdesc->pagetable = NULL;
}
EXPORT_SYMBOL(kgsl_mmu_put_gpuaddr);

/**
 * kgsl_mmu_svm_range() - Return the range for SVM (if applicable)
 * @pagetable: Pagetable to query the range from
 * @lo: Pointer to store the start of the SVM range
 * @hi: Pointer to store the end of the SVM range
 * @memflags: Flags from the buffer we are mapping
 */
int kgsl_mmu_svm_range(struct kgsl_pagetable *pagetable,
		uint64_t *lo, uint64_t *hi, uint64_t memflags)
{
	if (PT_OP_VALID(pagetable, svm_range))
		return pagetable->pt_ops->svm_range(pagetable, lo, hi,
			memflags);

	return -ENODEV;
}
EXPORT_SYMBOL(kgsl_mmu_svm_range);

int
kgsl_mmu_unmap(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc)
{
	int ret = 0;

	if (memdesc->size == 0)
		return -EINVAL;

	if (!(memdesc->flags & (KGSL_MEMFLAGS_SPARSE_VIRT |
					KGSL_MEMFLAGS_SPARSE_PHYS))) {
		/* Only global mappings should be mapped multiple times */
		if (!(KGSL_MEMDESC_MAPPED & memdesc->priv))
			return -EINVAL;
	}

	if (PT_OP_VALID(pagetable, mmu_unmap)) {
		uint64_t size;

		size = kgsl_memdesc_footprint(memdesc);

		ret = pagetable->pt_ops->mmu_unmap(pagetable, memdesc);

		atomic_dec(&pagetable->stats.entries);
		atomic_long_sub(size, &pagetable->stats.mapped);

		if (!kgsl_memdesc_is_global(memdesc))
			memdesc->priv &= ~KGSL_MEMDESC_MAPPED;
	}

	return ret;
}
EXPORT_SYMBOL(kgsl_mmu_unmap);

int kgsl_mmu_map_offset(struct kgsl_pagetable *pagetable,
			uint64_t virtaddr, uint64_t virtoffset,
			struct kgsl_memdesc *memdesc, uint64_t physoffset,
			uint64_t size, uint64_t flags)
{
	if (PT_OP_VALID(pagetable, mmu_map_offset)) {
		int ret;

		ret = pagetable->pt_ops->mmu_map_offset(pagetable, virtaddr,
				virtoffset, memdesc, physoffset, size, flags);
		if (ret)
			return ret;

		atomic_inc(&pagetable->stats.entries);
		KGSL_STATS_ADD(size, &pagetable->stats.mapped,
				&pagetable->stats.max_mapped);
	}

	return 0;
}
EXPORT_SYMBOL(kgsl_mmu_map_offset);

int kgsl_mmu_unmap_offset(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc, uint64_t addr, uint64_t offset,
		uint64_t size)
{
	if (PT_OP_VALID(pagetable, mmu_unmap_offset)) {
		int ret;

		ret = pagetable->pt_ops->mmu_unmap_offset(pagetable, memdesc,
				addr, offset, size);
		if (ret)
			return ret;

		atomic_dec(&pagetable->stats.entries);
		atomic_long_sub(size, &pagetable->stats.mapped);
	}

	return 0;
}
EXPORT_SYMBOL(kgsl_mmu_unmap_offset);

int kgsl_mmu_sparse_dummy_map(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc, uint64_t offset, uint64_t size)
{
	if (PT_OP_VALID(pagetable, mmu_sparse_dummy_map)) {
		int ret;

		ret = pagetable->pt_ops->mmu_sparse_dummy_map(pagetable,
				memdesc, offset, size);
		if (ret)
			return ret;

		atomic_dec(&pagetable->stats.entries);
		atomic_long_sub(size, &pagetable->stats.mapped);
	}

	return 0;
}
EXPORT_SYMBOL(kgsl_mmu_sparse_dummy_map);

void kgsl_mmu_remove_global(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc)
{
	struct kgsl_mmu *mmu = &device->mmu;

	if (MMU_OP_VALID(mmu, mmu_remove_global))
		mmu->mmu_ops->mmu_remove_global(mmu, memdesc);
}
EXPORT_SYMBOL(kgsl_mmu_remove_global);

void kgsl_mmu_add_global(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc, const char *name)
{
	struct kgsl_mmu *mmu = &device->mmu;

	if (MMU_OP_VALID(mmu, mmu_add_global))
		mmu->mmu_ops->mmu_add_global(mmu, memdesc, name);
}
EXPORT_SYMBOL(kgsl_mmu_add_global);

void kgsl_mmu_close(struct kgsl_device *device)
{
	struct kgsl_mmu *mmu = &(device->mmu);

	if (MMU_OP_VALID(mmu, mmu_close))
		mmu->mmu_ops->mmu_close(mmu);
}
EXPORT_SYMBOL(kgsl_mmu_close);

enum kgsl_mmutype kgsl_mmu_get_mmutype(struct kgsl_device *device)
{
	return device ? device->mmu.type : KGSL_MMU_TYPE_NONE;
}
EXPORT_SYMBOL(kgsl_mmu_get_mmutype);

bool kgsl_mmu_gpuaddr_in_range(struct kgsl_pagetable *pagetable,
		uint64_t gpuaddr)
{
	if (PT_OP_VALID(pagetable, addr_in_range))
		return pagetable->pt_ops->addr_in_range(pagetable, gpuaddr);

	return false;
}
EXPORT_SYMBOL(kgsl_mmu_gpuaddr_in_range);

struct kgsl_memdesc *kgsl_mmu_get_qdss_global_entry(struct kgsl_device *device)
{
	struct kgsl_mmu *mmu = &device->mmu;

	if (MMU_OP_VALID(mmu, mmu_get_qdss_global_entry))
		return mmu->mmu_ops->mmu_get_qdss_global_entry();

	return NULL;
}
EXPORT_SYMBOL(kgsl_mmu_get_qdss_global_entry);

struct kgsl_memdesc *kgsl_mmu_get_qtimer_global_entry(
		struct kgsl_device *device)
{
	struct kgsl_mmu *mmu = &device->mmu;

	if (MMU_OP_VALID(mmu, mmu_get_qtimer_global_entry))
		return mmu->mmu_ops->mmu_get_qtimer_global_entry();

	return NULL;
}
EXPORT_SYMBOL(kgsl_mmu_get_qtimer_global_entry);

/*
 * NOMMU definitions - NOMMU really just means that the MMU is kept in pass
 * through and the GPU directly accesses physical memory. Used in debug mode
 * and when a real MMU isn't up and running yet.
 */

static bool nommu_gpuaddr_in_range(struct kgsl_pagetable *pagetable,
		uint64_t gpuaddr)
{
	return (gpuaddr != 0) ? true : false;
}

static int nommu_get_gpuaddr(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc)
{
	if (memdesc->sgt->nents > 1) {
		WARN_ONCE(1,
			"Attempt to map non-contiguous memory with NOMMU\n");
		return -EINVAL;
	}

	memdesc->gpuaddr = (uint64_t) sg_phys(memdesc->sgt->sgl);

	if (memdesc->gpuaddr) {
		memdesc->pagetable = pagetable;
		return 0;
	}

	return -ENOMEM;
}

static struct kgsl_mmu_pt_ops nommu_pt_ops = {
	.get_gpuaddr = nommu_get_gpuaddr,
	.addr_in_range = nommu_gpuaddr_in_range,
};

static void nommu_add_global(struct kgsl_mmu *mmu,
		struct kgsl_memdesc *memdesc, const char *name)
{
	memdesc->gpuaddr = (uint64_t) sg_phys(memdesc->sgt->sgl);
}

static void nommu_remove_global(struct kgsl_mmu *mmu,
		struct kgsl_memdesc *memdesc)
{
	memdesc->gpuaddr = 0;
}

static int nommu_init_pt(struct kgsl_mmu *mmu, struct kgsl_pagetable *pt)
{
	if (pt == NULL)
		return -EINVAL;

	pt->pt_ops = &nommu_pt_ops;
	return 0;
}

static struct kgsl_pagetable *nommu_getpagetable(struct kgsl_mmu *mmu,
		unsigned long name)
{
	struct kgsl_pagetable *pagetable;

	pagetable = kgsl_get_pagetable(KGSL_MMU_GLOBAL_PT);

	if (pagetable == NULL)
		pagetable = kgsl_mmu_createpagetableobject(mmu,
			KGSL_MMU_GLOBAL_PT);

	return pagetable;
}

static int nommu_init(struct kgsl_mmu *mmu)
{
	mmu->features |= KGSL_MMU_GLOBAL_PAGETABLE;
	set_bit(KGSL_MMU_STARTED, &mmu->flags);
	return 0;
}

static int nommu_probe(struct kgsl_device *device)
{
	/* NOMMU always exists */
	return 0;
}

static struct kgsl_mmu_ops kgsl_nommu_ops = {
	.mmu_init = nommu_init,
	.mmu_add_global = nommu_add_global,
	.mmu_remove_global = nommu_remove_global,
	.mmu_init_pt = nommu_init_pt,
	.mmu_getpagetable = nommu_getpagetable,
	.probe = nommu_probe,
};

static struct {
	const char *name;
	unsigned int type;
	struct kgsl_mmu_ops *ops;
} kgsl_mmu_subtypes[] = {
#ifdef CONFIG_QCOM_KGSL_IOMMU
	{ "iommu", KGSL_MMU_TYPE_IOMMU, &kgsl_iommu_ops },
#endif
	{ "nommu", KGSL_MMU_TYPE_NONE, &kgsl_nommu_ops },
};

int kgsl_mmu_probe(struct kgsl_device *device, char *mmutype)
{
	struct kgsl_mmu *mmu = &device->mmu;
	int ret, i;

	if (mmutype != NULL) {
		for (i = 0; i < ARRAY_SIZE(kgsl_mmu_subtypes); i++) {
			if (strcmp(kgsl_mmu_subtypes[i].name, mmutype))
				continue;

			ret = kgsl_mmu_subtypes[i].ops->probe(device);

			if (ret == 0) {
				mmu->type = kgsl_mmu_subtypes[i].type;
				mmu->mmu_ops = kgsl_mmu_subtypes[i].ops;

				if (MMU_OP_VALID(mmu, mmu_init))
					return mmu->mmu_ops->mmu_init(mmu);
			}

			return ret;
		}

		KGSL_CORE_ERR("mmu: MMU type '%s' unknown\n", mmutype);
	}

	for (i = 0; i < ARRAY_SIZE(kgsl_mmu_subtypes); i++) {
		ret = kgsl_mmu_subtypes[i].ops->probe(device);

		if (ret == 0) {
			mmu->type = kgsl_mmu_subtypes[i].type;
			mmu->mmu_ops = kgsl_mmu_subtypes[i].ops;

			if (MMU_OP_VALID(mmu, mmu_init))
				return mmu->mmu_ops->mmu_init(mmu);

			return 0;
		}
	}

	KGSL_CORE_ERR("mmu: couldn't detect any known MMU types\n");
	return -ENODEV;
}
EXPORT_SYMBOL(kgsl_mmu_probe);
