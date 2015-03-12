/* Copyright (c) 2002,2007-2015, The Linux Foundation. All rights reserved.
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
#include <linux/qcom_iommu.h>
#include <linux/types.h>

#include "kgsl.h"
#include "kgsl_mmu.h"
#include "kgsl_device.h"
#include "kgsl_sharedmem.h"
#include "adreno.h"

static enum kgsl_mmutype kgsl_mmu_type;

static void pagetable_remove_sysfs_objects(struct kgsl_pagetable *pagetable);

/*
 * There are certain memory allocations (ringbuffer, memstore, etc) that need to
 * be present at the same address in every pagetable. We call these "global"
 * pagetable entries. There are relatively few of these and they are mostly
 * stable (defined at init time) but the actual number of globals can differ
 * slight depending on the target and implementation.
 *
 * Here we define an array and a simple allocator to keep track of the currently
 * active global entries. Each entry is assigned a unique address inside of a
 * MMU implementation specific "global" region. The addresses are assigned
 * sequentially and never re-used to avoid having to go back and reprogram
 * existing pagetables. The entire list of active entries are mapped and
 * unmapped into every new pagetable as it is created and destroyed.
 *
 * Because there are relatively few entries and they are defined at boot time we
 * don't need to go over the top to define a dynamic allocation scheme. It will
 * be less wasteful to pick a static number with a little bit of growth
 * potential.
 */

#define KGSL_MAX_GLOBAL_PT_ENTRIES 32

/**
 * struct kgsl_global_pt_entries - Collection of global pagetable entries
 * @offset - offset into the global PT space to be assigned to then next
 * allocation
 * @entries: Array of assigned memdesc entries
 * @count: Number of currently assigned entries
 *
 * Maintain a list of global pagetable entries. Pagetables are shared between
 * devices so the global pt entry list needs to be driver wide too
 */
static struct kgsl_global_pt_entries {
	unsigned int offset;
	struct kgsl_memdesc *entries[KGSL_MAX_GLOBAL_PT_ENTRIES];
	int count;
} kgsl_global_pt_entries;

/**
 * kgsl_search_global_pt_entries() - Check to see if the given GPU address
 * belongs to any of the global PT entries
 * @gpuaddr: GPU address to search for
 * @size: Size of the region to search for
 *
 * Search all the global pagetable entries for the GPU address and size and
 * return the memory descriptor
 */
struct kgsl_memdesc *kgsl_search_global_pt_entries(unsigned int gpuaddr,
		unsigned int size)
{
	int i;

	for (i = 0; i < KGSL_MAX_GLOBAL_PT_ENTRIES; i++) {
		struct kgsl_memdesc *memdesc =
			kgsl_global_pt_entries.entries[i];

		if (memdesc && kgsl_gpuaddr_in_memdesc(memdesc, gpuaddr, size))
			return memdesc;
	}

	return NULL;
}
EXPORT_SYMBOL(kgsl_search_global_pt_entries);

/**
 * kgsl_unmap_global_pt_entries() - Unmap all global entries from the given
 * pagetable
 * @pagetable: Pointer to a kgsl_pagetable structure
 *
 * Unmap all the current active global entries from the specified pagetable
 */
static void kgsl_unmap_global_pt_entries(struct kgsl_pagetable *pagetable)
{
	int i;

	for (i = 0; i < KGSL_MAX_GLOBAL_PT_ENTRIES; i++) {
		struct kgsl_memdesc *entry = kgsl_global_pt_entries.entries[i];
		/* entry was removed */
		if (entry == NULL)
			continue;

		/*
		 * Private entries are only in the private pagetable,
		 * but they are in the global list so that they have a unique
		 * address.
		 */
		if ((entry->priv & KGSL_MEMDESC_PRIVATE) &&
			(pagetable->name != KGSL_MMU_PRIV_PT))
			continue;

		kgsl_mmu_unmap(pagetable,
				kgsl_global_pt_entries.entries[i]);
	}
}

/**
 * kgsl_map_global_pt_entries() - Map all active global entries into the given
 * pagetable
 * @pagetable: Pointer to a kgsl_pagetable structure
 *
 * Map all the current global PT entries into the specified pagetable.
 * Returns error if an entry fails to map or 0 on success.
 */
static int kgsl_map_global_pt_entries(struct kgsl_pagetable *pagetable)
{
	int i, ret = 0;

	for (i = 0; !ret && i < KGSL_MAX_GLOBAL_PT_ENTRIES; i++) {
		struct kgsl_memdesc *entry = kgsl_global_pt_entries.entries[i];
		/* entry was removed */
		if (entry == NULL)
			continue;

		/*
		 * Private entries are only in the private pagetable,
		 * but they are in the global list so that they have a unique
		 * address.
		 */
		if ((entry->priv & KGSL_MEMDESC_PRIVATE) &&
			(pagetable->name != KGSL_MMU_PRIV_PT))
			continue;

		ret = kgsl_mmu_map(pagetable, entry);
		if (ret)
			break;

	}

	if (ret)
		kgsl_unmap_global_pt_entries(pagetable);

	return ret;
}

/**
 * kgsl_remove_global_pt_entry() - Remove a memory descriptor from the global PT
 * entry list
 * @memdesc: Pointer to the kgsl memory descriptor to remove
 *
 * Remove the specified memory descriptor from the current list of global
 * pagetable entries
 */
void kgsl_remove_global_pt_entry(struct kgsl_memdesc *memdesc)
{
	int i, j;

	if (memdesc->gpuaddr == 0)
		return;

	for (i = 0; i < kgsl_global_pt_entries.count; i++) {
		if (kgsl_global_pt_entries.entries[i] == memdesc) {
			memdesc->gpuaddr = 0;
			memdesc->priv &= ~(KGSL_MEMDESC_GLOBAL |
						KGSL_MEMDESC_PRIVATE);
			for (j = i; j < kgsl_global_pt_entries.count; j++)
				kgsl_global_pt_entries.entries[j] =
				kgsl_global_pt_entries.entries[j + 1];
			kgsl_global_pt_entries.entries[j - 1] = NULL;
			kgsl_global_pt_entries.count--;
			break;
		}
	}
}
EXPORT_SYMBOL(kgsl_remove_global_pt_entry);

/**
 * kgsl_add_global_pt_entry() - Add a new global PT entry to the active list
 * @mmu: Pointer to a kgsl_mmu structure for the active MMU implementation
 * @memdesc: Pointer to the kgsl memory descriptor to add
 *
 * Add a memory descriptor to the list of global pagetable entries.
 */
int kgsl_add_global_pt_entry(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc)
{
	int i;
	int index = 0;
	unsigned int gaddr = KGSL_MMU_GLOBAL_MEM_BASE;
	unsigned int size = ALIGN(memdesc->size, PAGE_SIZE);

	/* do we already have a mapping? */
	if (memdesc->gpuaddr != 0)
		return 0;

	if (kgsl_global_pt_entries.count == KGSL_MAX_GLOBAL_PT_ENTRIES)
		return -ENOMEM;

	/*
	 * search for the first free slot by going through all valid entries
	 * and checking for overlap. All entries are in increasing order of
	 * gpuaddr
	 */
	for (i = 0; i < kgsl_global_pt_entries.count; i++) {
		if (kgsl_addr_range_overlap(gaddr, size,
			kgsl_global_pt_entries.entries[i]->gpuaddr,
			kgsl_global_pt_entries.entries[i]->size))
			/* On a clash set gaddr to end of clashing entry */
			gaddr = kgsl_global_pt_entries.entries[i]->gpuaddr +
				kgsl_global_pt_entries.entries[i]->size;
		else
			break;
	}
	index = i;
	if ((gaddr + size) >= (KGSL_MMU_GLOBAL_MEM_BASE +
				KGSL_GLOBAL_PT_SIZE))
		return -ENOMEM;

	if (kgsl_mmu_type == KGSL_MMU_TYPE_NONE)
		memdesc->gpuaddr = memdesc->physaddr;
	else
		memdesc->gpuaddr = gaddr;

	memdesc->priv |= KGSL_MEMDESC_GLOBAL;
	/*
	 * Move the entries from index till the last entry 1 slot right leaving
	 * the slot at index empty for the newcomer
	 */
	for (i = kgsl_global_pt_entries.count - 1; i >= index; i--)
		kgsl_global_pt_entries.entries[i + 1] =
			kgsl_global_pt_entries.entries[i];
	kgsl_global_pt_entries.entries[index] = memdesc;
	kgsl_global_pt_entries.count++;

	return 0;
}
EXPORT_SYMBOL(kgsl_add_global_pt_entry);

static void kgsl_destroy_pagetable(struct kref *kref)
{
	struct kgsl_pagetable *pagetable = container_of(kref,
		struct kgsl_pagetable, refcount);

	kgsl_mmu_detach_pagetable(pagetable);

	kgsl_unmap_global_pt_entries(pagetable);

	if (pagetable->pool)
		gen_pool_destroy(pagetable->pool);

	pagetable->pt_ops->mmu_destroy_pagetable(pagetable);

	if (pagetable->mem_bitmap)
		vfree(pagetable->mem_bitmap);

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
	if (pagetable->list.next) {
		list_del(&pagetable->list);
		pagetable->list.next = NULL;
	}
	spin_unlock_irqrestore(&kgsl_driver.ptlock, flags);

	pagetable_remove_sysfs_objects(pagetable);
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
				pt->fault_addr =
					(addr & ~(PAGE_SIZE-1));
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

	/*
	 * Don't use kgsl_allocate_global here because we need to get the MMU
	 * set up before we can add the global entry but the MMU init needs the
	 * setstate block. Allocate the memory here and map it later
	 */

	status = kgsl_allocate_contiguous(device, &mmu->setstate_memory,
					PAGE_SIZE);
	if (status)
		return status;

	/* Mark the setstate memory as read only */
	mmu->setstate_memory.flags |= KGSL_MEMFLAGS_GPUREADONLY;

	kgsl_sharedmem_set(device, &mmu->setstate_memory, 0, 0,
				mmu->setstate_memory.size);

	if (KGSL_MMU_TYPE_IOMMU == kgsl_mmu_type) {
		mmu->mmu_ops = &kgsl_iommu_ops;
		status =  mmu->mmu_ops->mmu_init(mmu);
	}

	if (status)
		goto done;

	/* Add the setstate memory to the global PT entry list */
	status = kgsl_add_global_pt_entry(device, &mmu->setstate_memory);

done:
	if (status)
		kgsl_sharedmem_free(&mmu->setstate_memory);

	return status;
}
EXPORT_SYMBOL(kgsl_mmu_init);

int kgsl_mmu_start(struct kgsl_device *device)
{
	struct kgsl_mmu *mmu = &device->mmu;
	int ret = 0;

	if (kgsl_mmu_type != KGSL_MMU_TYPE_NONE)
		ret = mmu->mmu_ops->mmu_start(mmu);

	return ret;
}
EXPORT_SYMBOL(kgsl_mmu_start);

static struct kgsl_pagetable *
kgsl_mmu_createpagetableobject(struct kgsl_mmu *mmu,
				unsigned int name)
{
	int status = 0;
	struct kgsl_pagetable *pagetable = NULL;
	unsigned long flags;
	unsigned int ptbase, ptsize;
	char *pool_name;
	int nbits;

	pagetable = kzalloc(sizeof(struct kgsl_pagetable), GFP_KERNEL);
	if (pagetable == NULL)
		return NULL;

	kref_init(&pagetable->refcount);

	spin_lock_init(&pagetable->lock);

	pagetable->mmu = mmu;
	pagetable->name = name;
	pagetable->fault_addr = 0xFFFFFFFF;

	if (mmu->secured && (KGSL_MMU_SECURE_PT == name)) {
		ptbase = KGSL_IOMMU_SECURE_MEM_BASE;
		ptsize = KGSL_IOMMU_SECURE_MEM_SIZE;
		pool_name = "secured";
	} else {
		ptbase = mmu->pt_base;
		ptsize = mmu->pt_size;
		pool_name = "general";
	}

	pagetable->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (pagetable->pool == NULL) {
		KGSL_CORE_ERR("%s gen_pool_create(%d) failed ptname %d\n",
					pool_name, PAGE_SHIFT, name);
		goto err;
	}

	if (gen_pool_add(pagetable->pool, ptbase, ptsize, -1)) {
		KGSL_CORE_ERR("%s gen_pool_add failed ptname %d\n",
					pool_name, name);
		goto err;
	}

	/* allocate bitmap for virtual memory management */
	nbits = KGSL_SVM_UPPER_BOUND >> PAGE_SHIFT;
	pagetable->mem_bitmap = vmalloc(BITS_TO_LONGS(nbits) * sizeof(long));
	if (!pagetable->mem_bitmap)
		goto err;
	memset(pagetable->mem_bitmap, 0, BITS_TO_LONGS(nbits) * sizeof(long));

	if (KGSL_MMU_TYPE_IOMMU == kgsl_mmu_type)
		pagetable->pt_ops = &iommu_pt_ops;

	if (mmu->secured && (KGSL_MMU_SECURE_PT == name))
		pagetable->priv =
			pagetable->pt_ops->mmu_create_secure_pagetable();
	else {
		pagetable->priv = pagetable->pt_ops->mmu_create_pagetable();
		if (pagetable->priv) {
			status = kgsl_map_global_pt_entries(pagetable);
			if (status)
				goto err;
		}
	}

	if (!pagetable->priv)
		goto err;

	spin_lock_irqsave(&kgsl_driver.ptlock, flags);
	list_add(&pagetable->list, &kgsl_driver.pagetable_list);
	spin_unlock_irqrestore(&kgsl_driver.ptlock, flags);

	/* Create the sysfs entries */
	pagetable_add_sysfs_objects(pagetable);

	return pagetable;

err:
	if (pagetable->priv)
		pagetable->pt_ops->mmu_destroy_pagetable(pagetable);
	if (pagetable->pool)
		gen_pool_destroy(pagetable->pool);
	if (pagetable->mem_bitmap)
		vfree(pagetable->mem_bitmap);

	kfree(pagetable);

	return NULL;
}

struct kgsl_pagetable *kgsl_mmu_getpagetable(struct kgsl_mmu *mmu,
						unsigned long name)
{
	struct kgsl_pagetable *pt;

	if (KGSL_MMU_TYPE_NONE == kgsl_mmu_type)
		return (void *)(-1);

	if (!kgsl_mmu_is_perprocess(mmu) && (KGSL_MMU_SECURE_PT != name))
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

static int _nommu_get_gpuaddr(struct kgsl_memdesc *memdesc)
{
	if (memdesc->sglen > 1) {
		KGSL_CORE_ERR(
			"Attempt to map non-contiguous memory with NOMMU\n");
		return -EINVAL;
	}

	memdesc->gpuaddr = (uint64_t) sg_dma_address(memdesc->sg);

	if (memdesc->gpuaddr == 0)
		memdesc->gpuaddr = (uint64_t) sg_phys(memdesc->sg);

	if (memdesc->gpuaddr == 0) {
		KGSL_CORE_ERR("Unable to get a physical address\n");
		return -EINVAL;
	}

	return 0;
}

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
	int size;
	unsigned long bit;

	if (kgsl_mmu_type == KGSL_MMU_TYPE_NONE)
		return _nommu_get_gpuaddr(memdesc);

	/* Add space for the guard page when allocating the mmu VA. */
	size = memdesc->size;
	if (kgsl_memdesc_has_guard_page(memdesc))
		size += PAGE_SIZE;

	/*
	 * Allocate aligned virtual addresses for iommu. This allows
	 * more efficient pagetable entries if the physical memory
	 * is also aligned.
	 */

	if (kgsl_memdesc_use_cpu_map(memdesc)) {
		if (memdesc->gpuaddr == 0)
			return -EINVAL;
		bitmap_set(pagetable->mem_bitmap,
			(int) (memdesc->gpuaddr >> PAGE_SHIFT),
			(int) (size >> PAGE_SHIFT));
		memdesc->priv |= KGSL_MEMDESC_BITMAP_ALLOC;
		return 0;
	}

	/*
	 * Try to map external memory in the upper region first and then fall
	 * back to user region if that fails.  All memory allocated by the user
	 * goes into the user region first.
	 */
	if (((KGSL_MEMFLAGS_USERMEM_MASK | KGSL_MEMFLAGS_SECURE)
					& memdesc->flags) != 0) {
		unsigned int page_align = ilog2(PAGE_SIZE);

		if (kgsl_memdesc_get_align(memdesc) > 0)
			page_align = kgsl_memdesc_get_align(memdesc);

		memdesc->gpuaddr = gen_pool_alloc_aligned(pagetable->pool,
			size, page_align);

		if (memdesc->gpuaddr) {
			memdesc->priv |= KGSL_MEMDESC_GENPOOL_ALLOC;
			return 0;
		}
	}

	if (((KGSL_MEMFLAGS_SECURE) & memdesc->flags) && (!memdesc->gpuaddr))
		return -ENOMEM;

	bit = bitmap_find_next_zero_area(pagetable->mem_bitmap,
		KGSL_SVM_UPPER_BOUND >> PAGE_SHIFT, 1,
		(unsigned int) (size >> PAGE_SHIFT), 0);

	if (bit && (bit < (KGSL_SVM_UPPER_BOUND >> PAGE_SHIFT))) {
		bitmap_set(pagetable->mem_bitmap,
				(int) bit, (int) (size >> PAGE_SHIFT));
		memdesc->gpuaddr = (bit << PAGE_SHIFT);
		memdesc->priv |= KGSL_MEMDESC_BITMAP_ALLOC;
	}

	return (memdesc->gpuaddr == 0) ? -ENOMEM : 0;
}
EXPORT_SYMBOL(kgsl_mmu_get_gpuaddr);

int
kgsl_mmu_map(struct kgsl_pagetable *pagetable,
				struct kgsl_memdesc *memdesc)
{
	int ret = 0;
	int size;

	if (!memdesc->gpuaddr)
		return -EINVAL;
	/* Only global mappings should be mapped multiple times */
	if (!kgsl_memdesc_is_global(memdesc) &&
		(KGSL_MEMDESC_MAPPED & memdesc->priv))
		return -EINVAL;

	if (kgsl_mmu_get_mmutype() == KGSL_MMU_TYPE_NONE)
		return 0;

	/* Add space for the guard page when allocating the mmu VA. */
	size = memdesc->size;
	if (kgsl_memdesc_has_guard_page(memdesc))
		size += PAGE_SIZE;

	if (KGSL_MMU_TYPE_IOMMU != kgsl_mmu_get_mmutype())
		spin_lock(&pagetable->lock);
	ret = pagetable->pt_ops->mmu_map(pagetable, memdesc);
	if (KGSL_MMU_TYPE_IOMMU == kgsl_mmu_get_mmutype())
		spin_lock(&pagetable->lock);

	if (ret)
		goto done;

	KGSL_STATS_ADD(size, pagetable->stats.mapped,
		       pagetable->stats.max_mapped);
	pagetable->stats.entries++;

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

	if (KGSL_MEMDESC_BITMAP_ALLOC & memdesc->priv) {
		bitmap_clear(pagetable->mem_bitmap,
			memdesc->gpuaddr >> PAGE_SHIFT,
			size >> PAGE_SHIFT);
		memdesc->priv &= ~KGSL_MEMDESC_BITMAP_ALLOC;
		goto done;
	}

	if (!(KGSL_MEMDESC_GENPOOL_ALLOC & memdesc->priv))
		goto done;

	pool = pagetable->pool;

	if (pool) {
		gen_pool_free(pool, memdesc->gpuaddr, size);
		memdesc->priv &= ~KGSL_MEMDESC_GENPOOL_ALLOC;
	}
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
	pagetable->pt_ops->mmu_unmap(pagetable, memdesc);

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

int kgsl_mmu_close(struct kgsl_device *device)
{
	struct kgsl_mmu *mmu = &device->mmu;
	int ret = 0;

	kgsl_free_global(&mmu->setstate_memory);

	if (mmu->mmu_ops != NULL)
		ret = mmu->mmu_ops->mmu_close(mmu);

	return ret;
}
EXPORT_SYMBOL(kgsl_mmu_close);

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
	kgsl_mmu_type = iommu_present(&platform_bus_type) ?
		KGSL_MMU_TYPE_IOMMU : KGSL_MMU_TYPE_NONE;

	if (mmutype && !strncmp(mmutype, "nommu", 5))
		kgsl_mmu_type = KGSL_MMU_TYPE_NONE;
}
EXPORT_SYMBOL(kgsl_mmu_set_mmutype);

int kgsl_mmu_gpuaddr_in_range(struct kgsl_pagetable *pt, unsigned int gpuaddr)
{
	if (KGSL_MMU_TYPE_NONE == kgsl_mmu_type)
		return (gpuaddr != 0);

	if (gpuaddr > 0 && gpuaddr < KGSL_MMU_GLOBAL_MEM_BASE)
		return 1;

	return 0;
}
EXPORT_SYMBOL(kgsl_mmu_gpuaddr_in_range);
