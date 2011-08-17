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

#include "kgsl.h"
#include "kgsl_mmu.h"
#include "kgsl_device.h"
#include "kgsl_sharedmem.h"

#define KGSL_MMU_ALIGN_SHIFT    13
#define KGSL_MMU_ALIGN_MASK     (~((1 << KGSL_MMU_ALIGN_SHIFT) - 1))

#define GSL_PT_PAGE_BITS_MASK	0x00000007
#define GSL_PT_PAGE_ADDR_MASK	PAGE_MASK

static void pagetable_remove_sysfs_objects(struct kgsl_pagetable *pagetable);

static ssize_t
sysfs_show_ptpool_entries(struct kobject *kobj,
			  struct kobj_attribute *attr,
			  char *buf)
{
	return sprintf(buf, "%d\n", kgsl_driver.ptpool.entries);
}

static ssize_t
sysfs_show_ptpool_min(struct kobject *kobj,
			 struct kobj_attribute *attr,
			 char *buf)
{
	return sprintf(buf, "%d\n", kgsl_driver.ptpool.static_entries);
}

static ssize_t
sysfs_show_ptpool_chunks(struct kobject *kobj,
			 struct kobj_attribute *attr,
			 char *buf)
{
	return sprintf(buf, "%d\n", kgsl_driver.ptpool.chunks);
}

static ssize_t
sysfs_show_ptpool_ptsize(struct kobject *kobj,
			 struct kobj_attribute *attr,
			 char *buf)
{
	return sprintf(buf, "%d\n", kgsl_driver.ptpool.ptsize);
}

static struct kobj_attribute attr_ptpool_entries = {
	.attr = { .name = "ptpool_entries", .mode = 0444 },
	.show = sysfs_show_ptpool_entries,
	.store = NULL,
};

static struct kobj_attribute attr_ptpool_min = {
	.attr = { .name = "ptpool_min", .mode = 0444 },
	.show = sysfs_show_ptpool_min,
	.store = NULL,
};

static struct kobj_attribute attr_ptpool_chunks = {
	.attr = { .name = "ptpool_chunks", .mode = 0444 },
	.show = sysfs_show_ptpool_chunks,
	.store = NULL,
};

static struct kobj_attribute attr_ptpool_ptsize = {
	.attr = { .name = "ptpool_ptsize", .mode = 0444 },
	.show = sysfs_show_ptpool_ptsize,
	.store = NULL,
};

static struct attribute *ptpool_attrs[] = {
	&attr_ptpool_entries.attr,
	&attr_ptpool_min.attr,
	&attr_ptpool_chunks.attr,
	&attr_ptpool_ptsize.attr,
	NULL,
};

static struct attribute_group ptpool_attr_group = {
	.attrs = ptpool_attrs,
};

static int
_kgsl_ptpool_add_entries(struct kgsl_ptpool *pool, int count, int dynamic)
{
	struct kgsl_ptpool_chunk *chunk;
	size_t size = ALIGN(count * pool->ptsize, PAGE_SIZE);

	BUG_ON(count == 0);

	if (get_order(size) >= MAX_ORDER) {
		KGSL_CORE_ERR("ptpool allocation is too big: %d\n", size);
		return -EINVAL;
	}

	chunk = kzalloc(sizeof(*chunk), GFP_KERNEL);
	if (chunk == NULL) {
		KGSL_CORE_ERR("kzalloc(%d) failed\n", sizeof(*chunk));
		return -ENOMEM;
	}

	chunk->size = size;
	chunk->count = count;
	chunk->dynamic = dynamic;

	chunk->data = dma_alloc_coherent(NULL, size,
					 &chunk->phys, GFP_KERNEL);

	if (chunk->data == NULL) {
		KGSL_CORE_ERR("dma_alloc_coherent(%d) failed\n", size);
		goto err;
	}

	chunk->bitmap = kzalloc(BITS_TO_LONGS(count) * 4, GFP_KERNEL);

	if (chunk->bitmap == NULL) {
		KGSL_CORE_ERR("kzalloc(%d) failed\n",
			BITS_TO_LONGS(count) * 4);
		goto err_dma;
	}

	list_add_tail(&chunk->list, &pool->list);

	pool->chunks++;
	pool->entries += count;

	if (!dynamic)
		pool->static_entries += count;

	return 0;

err_dma:
	dma_free_coherent(NULL, chunk->size, chunk->data, chunk->phys);
err:
	kfree(chunk);
	return -ENOMEM;
}

static void *
_kgsl_ptpool_get_entry(struct kgsl_ptpool *pool, unsigned int *physaddr)
{
	struct kgsl_ptpool_chunk *chunk;

	list_for_each_entry(chunk, &pool->list, list) {
		int bit = find_first_zero_bit(chunk->bitmap, chunk->count);

		if (bit >= chunk->count)
			continue;

		set_bit(bit, chunk->bitmap);
		*physaddr = chunk->phys + (bit * pool->ptsize);

		return chunk->data + (bit * pool->ptsize);
	}

	return NULL;
}

/**
 * kgsl_ptpool_add
 * @pool:  A pointer to a ptpool structure
 * @entries: Number of entries to add
 *
 * Add static entries to the pagetable pool.
 */

int
kgsl_ptpool_add(struct kgsl_ptpool *pool, int count)
{
	int ret = 0;
	BUG_ON(count == 0);

	mutex_lock(&pool->lock);

	/* Only 4MB can be allocated in one chunk, so larger allocations
	   need to be split into multiple sections */

	while (count) {
		int entries = ((count * pool->ptsize) > SZ_4M) ?
			SZ_4M / pool->ptsize : count;

		/* Add the entries as static, i.e. they don't ever stand
		   a chance of being removed */

		ret =  _kgsl_ptpool_add_entries(pool, entries, 0);
		if (ret)
			break;

		count -= entries;
	}

	mutex_unlock(&pool->lock);
	return ret;
}

/**
 * kgsl_ptpool_alloc
 * @pool:  A pointer to a ptpool structure
 * @addr: A pointer to store the physical address of the chunk
 *
 * Allocate a pagetable from the pool.  Returns the virtual address
 * of the pagetable, the physical address is returned in physaddr
 */

void *kgsl_ptpool_alloc(struct kgsl_ptpool *pool, unsigned int *physaddr)
{
	void *addr = NULL;
	int ret;

	mutex_lock(&pool->lock);
	addr = _kgsl_ptpool_get_entry(pool, physaddr);
	if (addr)
		goto done;

	/* Add a chunk for 1 more pagetable and mark it as dynamic */
	ret = _kgsl_ptpool_add_entries(pool, 1, 1);

	if (ret)
		goto done;

	addr = _kgsl_ptpool_get_entry(pool, physaddr);
done:
	mutex_unlock(&pool->lock);
	return addr;
}

static inline void _kgsl_ptpool_rm_chunk(struct kgsl_ptpool_chunk *chunk)
{
	list_del(&chunk->list);

	if (chunk->data)
		dma_free_coherent(NULL, chunk->size, chunk->data,
			chunk->phys);
	kfree(chunk->bitmap);
	kfree(chunk);
}

/**
 * kgsl_ptpool_free
 * @pool:  A pointer to a ptpool structure
 * @addr: A pointer to the virtual address to free
 *
 * Free a pagetable allocated from the pool
 */

void kgsl_ptpool_free(struct kgsl_ptpool *pool, void *addr)
{
	struct kgsl_ptpool_chunk *chunk, *tmp;

	if (pool == NULL || addr == NULL)
		return;

	mutex_lock(&pool->lock);
	list_for_each_entry_safe(chunk, tmp, &pool->list, list)  {
		if (addr >=  chunk->data &&
		    addr < chunk->data + chunk->size) {
			int bit = ((unsigned long) (addr - chunk->data)) /
				pool->ptsize;

			clear_bit(bit, chunk->bitmap);
			memset(addr, 0, pool->ptsize);

			if (chunk->dynamic &&
				bitmap_empty(chunk->bitmap, chunk->count))
				_kgsl_ptpool_rm_chunk(chunk);

			break;
		}
	}

	mutex_unlock(&pool->lock);
}

void kgsl_ptpool_destroy(struct kgsl_ptpool *pool)
{
	struct kgsl_ptpool_chunk *chunk, *tmp;

	if (pool == NULL)
		return;

	mutex_lock(&pool->lock);
	list_for_each_entry_safe(chunk, tmp, &pool->list, list)
		_kgsl_ptpool_rm_chunk(chunk);
	mutex_unlock(&pool->lock);

	memset(pool, 0, sizeof(*pool));
}

/**
 * kgsl_ptpool_init
 * @pool:  A pointer to a ptpool structure to initialize
 * @ptsize: The size of each pagetable entry
 * @entries:  The number of inital entries to add to the pool
 *
 * Initalize a pool and allocate an initial chunk of entries.
 */

int kgsl_ptpool_init(struct kgsl_ptpool *pool, int ptsize, int entries)
{
	int ret = 0;
	BUG_ON(ptsize == 0);

	pool->ptsize = ptsize;
	mutex_init(&pool->lock);
	INIT_LIST_HEAD(&pool->list);

	if (entries) {
		ret = kgsl_ptpool_add(pool, entries);
		if (ret)
			return ret;
	}

	return sysfs_create_group(kgsl_driver.ptkobj, &ptpool_attr_group);
}

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

	kgsl_ptpool_free(&kgsl_driver.ptpool, pagetable->base.hostptr);

	kgsl_driver.stats.coherent -= KGSL_PAGETABLE_SIZE;

	if (pagetable->pool)
		gen_pool_destroy(pagetable->pool);

	kfree(pagetable->tlbflushfilter.base);
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
		ret += sprintf(buf, "%d\n", pt->stats.entries);

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
		ret += sprintf(buf, "%d\n", pt->stats.mapped);

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
		ret += sprintf(buf, "0x%x\n", pt->va_range);

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
		ret += sprintf(buf, "%d\n", pt->stats.max_mapped);

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
		ret += sprintf(buf, "%d\n", pt->stats.max_entries);

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

static inline uint32_t
kgsl_pt_entry_get(struct kgsl_pagetable *pt, uint32_t va)
{
	return (va - pt->va_base) >> PAGE_SHIFT;
}

static inline void
kgsl_pt_map_set(struct kgsl_pagetable *pt, uint32_t pte, uint32_t val)
{
	uint32_t *baseptr = (uint32_t *)pt->base.hostptr;

	writel_relaxed(val, &baseptr[pte]);
}

static inline uint32_t
kgsl_pt_map_getaddr(struct kgsl_pagetable *pt, uint32_t pte)
{
	uint32_t *baseptr = (uint32_t *)pt->base.hostptr;
	uint32_t ret = readl_relaxed(&baseptr[pte]) & GSL_PT_PAGE_ADDR_MASK;
	return ret;
}

void kgsl_mh_intrcallback(struct kgsl_device *device)
{
	unsigned int status = 0;
	unsigned int reg;

	kgsl_regread(device, MH_INTERRUPT_STATUS, &status);
	kgsl_regread(device, MH_AXI_ERROR, &reg);

	if (status & MH_INTERRUPT_MASK__AXI_READ_ERROR)
		KGSL_MEM_CRIT(device, "axi read error interrupt: %08x\n", reg);
	else if (status & MH_INTERRUPT_MASK__AXI_WRITE_ERROR)
		KGSL_MEM_CRIT(device, "axi write error interrupt: %08x\n", reg);
	else if (status & MH_INTERRUPT_MASK__MMU_PAGE_FAULT) {
		unsigned int ptbase;
		struct kgsl_pagetable *pt;
		int ptid = -1;

		kgsl_regread(device, MH_MMU_PAGE_FAULT, &reg);
		kgsl_regread(device, MH_MMU_PT_BASE, &ptbase);

		spin_lock(&kgsl_driver.ptlock);
		list_for_each_entry(pt, &kgsl_driver.pagetable_list, list) {
			if (ptbase == pt->base.gpuaddr) {
				ptid = (int) pt->name;
				break;
			}
		}
		spin_unlock(&kgsl_driver.ptlock);

		KGSL_MEM_CRIT(device,
			"mmu page fault: page=0x%lx pt=%d op=%s axi=%d\n",
			reg & ~(PAGE_SIZE - 1), ptid,
			reg & 0x02 ? "WRITE" : "READ", (reg >> 4) & 0xF);
	} else
		KGSL_MEM_WARN(device,
			"bad bits in REG_MH_INTERRUPT_STATUS %08x\n", status);

	kgsl_regwrite(device, MH_INTERRUPT_CLEAR, status);

	/*TODO: figure out how to handle errror interupts.
	* specifically, page faults should probably nuke the client that
	* caused them, but we don't have enough info to figure that out yet.
	*/
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
	pagetable->tlb_flags = 0;
	pagetable->name = name;
	pagetable->va_base = KGSL_PAGETABLE_BASE;
	pagetable->va_range = CONFIG_MSM_KGSL_PAGE_TABLE_SIZE;
	pagetable->last_superpte = 0;
	pagetable->max_entries = KGSL_PAGETABLE_ENTRIES(pagetable->va_range);

	pagetable->tlbflushfilter.size = (pagetable->va_range /
				(PAGE_SIZE * GSL_PT_SUPER_PTE * 8)) + 1;
	pagetable->tlbflushfilter.base = (unsigned int *)
			kzalloc(pagetable->tlbflushfilter.size, GFP_KERNEL);
	if (!pagetable->tlbflushfilter.base) {
		KGSL_CORE_ERR("kzalloc(%d) failed\n",
			pagetable->tlbflushfilter.size);
		goto err_alloc;
	}
	GSL_TLBFLUSH_FILTER_RESET();

	pagetable->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (pagetable->pool == NULL) {
		KGSL_CORE_ERR("gen_pool_create(%d) failed\n", PAGE_SHIFT);
		goto err_flushfilter;
	}

	if (gen_pool_add(pagetable->pool, pagetable->va_base,
				pagetable->va_range, -1)) {
		KGSL_CORE_ERR("gen_pool_add failed\n");
		goto err_pool;
	}

	pagetable->base.hostptr = kgsl_ptpool_alloc(&kgsl_driver.ptpool,
		&pagetable->base.physaddr);

	if (pagetable->base.hostptr == NULL)
		goto err_pool;

	/* ptpool allocations are from coherent memory, so update the
	   device statistics acordingly */

	KGSL_STATS_ADD(KGSL_PAGETABLE_SIZE, kgsl_driver.stats.coherent,
		       kgsl_driver.stats.coherent_max);

	pagetable->base.gpuaddr = pagetable->base.physaddr;
	pagetable->base.size = KGSL_PAGETABLE_SIZE;

	status = kgsl_setup_pt(pagetable);
	if (status)
		goto err_free_sharedmem;

	spin_lock_irqsave(&kgsl_driver.ptlock, flags);
	list_add(&pagetable->list, &kgsl_driver.pagetable_list);
	spin_unlock_irqrestore(&kgsl_driver.ptlock, flags);

	/* Create the sysfs entries */
	pagetable_add_sysfs_objects(pagetable);

	return pagetable;

err_free_sharedmem:
	kgsl_ptpool_free(&kgsl_driver.ptpool, &pagetable->base.hostptr);
err_pool:
	gen_pool_destroy(pagetable->pool);
err_flushfilter:
	kfree(pagetable->tlbflushfilter.base);
err_alloc:
	kfree(pagetable);

	return NULL;
}

struct kgsl_pagetable *kgsl_mmu_getpagetable(unsigned long name)
{
	struct kgsl_pagetable *pt;

	pt = kgsl_get_pagetable(name);

	if (pt == NULL)
		pt = kgsl_mmu_createpagetableobject(name);

	return pt;
}

void kgsl_mmu_putpagetable(struct kgsl_pagetable *pagetable)
{
	kgsl_put_pagetable(pagetable);
}

void kgsl_default_setstate(struct kgsl_device *device, uint32_t flags)
{
	if (!kgsl_mmu_enabled())
		return;

	if (flags & KGSL_MMUFLAGS_PTUPDATE) {
		kgsl_idle(device, KGSL_TIMEOUT_DEFAULT);
		kgsl_regwrite(device, MH_MMU_PT_BASE,
			device->mmu.hwpagetable->base.gpuaddr);
	}

	if (flags & KGSL_MMUFLAGS_TLBFLUSH) {
		/* Invalidate all and tc */
		kgsl_regwrite(device, MH_MMU_INVALIDATE,  0x00000003);
	}
}
EXPORT_SYMBOL(kgsl_default_setstate);

void kgsl_setstate(struct kgsl_device *device, uint32_t flags)
{
	if (device->ftbl->setstate)
		device->ftbl->setstate(device, flags);
}
EXPORT_SYMBOL(kgsl_setstate);

void kgsl_mmu_setstate(struct kgsl_device *device,
				struct kgsl_pagetable *pagetable)
{
	struct kgsl_mmu *mmu = &device->mmu;

	if (mmu->flags & KGSL_FLAGS_STARTED) {
		/* page table not current, then setup mmu to use new
		 *  specified page table
		 */
		if (mmu->hwpagetable != pagetable) {
			mmu->hwpagetable = pagetable;
			spin_lock(&mmu->hwpagetable->lock);
			mmu->hwpagetable->tlb_flags &= ~(1<<device->id);
			spin_unlock(&mmu->hwpagetable->lock);

			/* call device specific set page table */
			kgsl_setstate(mmu->device, KGSL_MMUFLAGS_TLBFLUSH |
				KGSL_MMUFLAGS_PTUPDATE);
		}
	}
}
EXPORT_SYMBOL(kgsl_mmu_setstate);

int kgsl_mmu_init(struct kgsl_device *device)
{
	/*
	 * intialize device mmu
	 *
	 * call this with the global lock held
	 */
	int status = 0;
	struct kgsl_mmu *mmu = &device->mmu;

	mmu->device = device;

	/* make sure aligned to pagesize */
	BUG_ON(mmu->mpu_base & (PAGE_SIZE - 1));
	BUG_ON((mmu->mpu_base + mmu->mpu_range) & (PAGE_SIZE - 1));

	/* sub-client MMU lookups require address translation */
	if ((mmu->config & ~0x1) > 0) {
		/*make sure virtual address range is a multiple of 64Kb */
		BUG_ON(CONFIG_MSM_KGSL_PAGE_TABLE_SIZE & ((1 << 16) - 1));

		/* allocate memory used for completing r/w operations that
		 * cannot be mapped by the MMU
		 */
		status = kgsl_allocate_contiguous(&mmu->dummyspace, 64);
		if (!status)
			kgsl_sharedmem_set(&mmu->dummyspace, 0, 0,
					   mmu->dummyspace.size);
	}

	return status;
}

int kgsl_mmu_start(struct kgsl_device *device)
{
	/*
	 * intialize device mmu
	 *
	 * call this with the global lock held
	 */

	struct kgsl_mmu *mmu = &device->mmu;

	if (mmu->flags & KGSL_FLAGS_STARTED)
		return 0;

	/* MMU not enabled */
	if ((mmu->config & 0x1) == 0)
		return 0;

	mmu->flags |= KGSL_FLAGS_STARTED;

	/* setup MMU and sub-client behavior */
	kgsl_regwrite(device, MH_MMU_CONFIG, mmu->config);

	/*
	 * Interrupts are enabled on a per-device level when
	 * kgsl_pwrctrl_irq() is called
	*/

	/* idle device */
	kgsl_idle(device,  KGSL_TIMEOUT_DEFAULT);

	/* define physical memory range accessible by the core */
	kgsl_regwrite(device, MH_MMU_MPU_BASE, mmu->mpu_base);
	kgsl_regwrite(device, MH_MMU_MPU_END,
			mmu->mpu_base + mmu->mpu_range);

	/* sub-client MMU lookups require address translation */
	if ((mmu->config & ~0x1) > 0) {

		kgsl_sharedmem_set(&mmu->dummyspace, 0, 0,
				   mmu->dummyspace.size);

		/* TRAN_ERROR needs a 32 byte (32 byte aligned) chunk of memory
		 * to complete transactions in case of an MMU fault. Note that
		 * we'll leave the bottom 32 bytes of the dummyspace for other
		 * purposes (e.g. use it when dummy read cycles are needed
		 * for other blocks */
		kgsl_regwrite(device, MH_MMU_TRAN_ERROR,
			mmu->dummyspace.physaddr + 32);

		if (mmu->defaultpagetable == NULL)
			mmu->defaultpagetable =
				kgsl_mmu_getpagetable(KGSL_MMU_GLOBAL_PT);

		/* Return error if the default pagetable doesn't exist */
		if (mmu->defaultpagetable == NULL)
			return -ENOMEM;

		mmu->hwpagetable = mmu->defaultpagetable;

		kgsl_regwrite(device, MH_MMU_PT_BASE,
			      mmu->hwpagetable->base.gpuaddr);
		kgsl_regwrite(device, MH_MMU_VA_RANGE,
			      (mmu->hwpagetable->va_base |
			      (mmu->hwpagetable->va_range >> 16)));
		kgsl_setstate(device, KGSL_MMUFLAGS_TLBFLUSH);
	}

	return 0;
}
EXPORT_SYMBOL(kgsl_mmu_start);

unsigned int kgsl_virtaddr_to_physaddr(void *virtaddr)
{
	unsigned int physaddr = 0;
	pgd_t *pgd_ptr = NULL;
	pmd_t *pmd_ptr = NULL;
	pte_t *pte_ptr = NULL, pte;

	pgd_ptr = pgd_offset(current->mm, (unsigned long) virtaddr);
	if (pgd_none(*pgd) || pgd_bad(*pgd)) {
		KGSL_CORE_ERR("Invalid pgd entry\n");
		return 0;
	}

	pmd_ptr = pmd_offset(pgd_ptr, (unsigned long) virtaddr);
	if (pmd_none(*pmd_ptr) || pmd_bad(*pmd_ptr)) {
		KGSL_CORE_ERR("Invalid pmd entry\n");
		return 0;
	}

	pte_ptr = pte_offset_map(pmd_ptr, (unsigned long) virtaddr);
	if (!pte_ptr) {
		KGSL_CORE_ERR("pt_offset_map failed\n");
		return 0;
	}
	pte = *pte_ptr;
	physaddr = pte_pfn(pte);
	pte_unmap(pte_ptr);
	physaddr <<= PAGE_SHIFT;
	return physaddr;
}

int
kgsl_mmu_map(struct kgsl_pagetable *pagetable,
				struct kgsl_memdesc *memdesc,
				unsigned int protflags)
{
	int numpages;
	unsigned int pte, ptefirst, ptelast, physaddr;
	int flushtlb;
	unsigned int offset = 0;

	BUG_ON(protflags & ~(GSL_PT_PAGE_RV | GSL_PT_PAGE_WV));
	BUG_ON(protflags == 0);

	memdesc->gpuaddr = gen_pool_alloc_aligned(pagetable->pool,
		memdesc->size, KGSL_MMU_ALIGN_SHIFT);

	if (memdesc->gpuaddr == 0) {
		KGSL_CORE_ERR("gen_pool_alloc(%d) failed\n", memdesc->size);
		KGSL_CORE_ERR(" [%d] allocated=%d, entries=%d\n",
				pagetable->name, pagetable->stats.mapped,
				pagetable->stats.entries);
		return -ENOMEM;
	}

	numpages = (memdesc->size >> PAGE_SHIFT);

	ptefirst = kgsl_pt_entry_get(pagetable, memdesc->gpuaddr);
	ptelast = ptefirst + numpages;

	pte = ptefirst;
	flushtlb = 0;

	/* tlb needs to be flushed when the first and last pte are not at
	* superpte boundaries */
	if ((ptefirst & (GSL_PT_SUPER_PTE - 1)) != 0 ||
		((ptelast + 1) & (GSL_PT_SUPER_PTE-1)) != 0)
		flushtlb = 1;

	spin_lock(&pagetable->lock);
	for (pte = ptefirst; pte < ptelast; pte++, offset += PAGE_SIZE) {
#ifdef VERBOSE_DEBUG
		/* check if PTE exists */
		uint32_t val = kgsl_pt_map_getaddr(pagetable, pte);
		BUG_ON(val != 0 && val != GSL_PT_PAGE_DIRTY);
#endif
		if ((pte & (GSL_PT_SUPER_PTE-1)) == 0)
			if (GSL_TLBFLUSH_FILTER_ISDIRTY(pte / GSL_PT_SUPER_PTE))
				flushtlb = 1;
		/* mark pte as in use */

		physaddr = memdesc->ops->physaddr(memdesc, offset);
		BUG_ON(physaddr == 0);
		kgsl_pt_map_set(pagetable, pte, physaddr | protflags);
	}

	/* Keep track of the statistics for the sysfs files */

	KGSL_STATS_ADD(1, pagetable->stats.entries,
		       pagetable->stats.max_entries);

	KGSL_STATS_ADD(memdesc->size, pagetable->stats.mapped,
		       pagetable->stats.max_mapped);

	/* Post all writes to the pagetable */
	wmb();

	/* Invalidate tlb only if current page table used by GPU is the
	 * pagetable that we used to allocate */
	if (flushtlb) {
		/*set all devices as needing flushing*/
		pagetable->tlb_flags = UINT_MAX;
		GSL_TLBFLUSH_FILTER_RESET();
	}
	spin_unlock(&pagetable->lock);

	return 0;
}

int
kgsl_mmu_unmap(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc)
{
	unsigned int numpages;
	unsigned int pte, ptefirst, ptelast, superpte;
	unsigned int range = memdesc->size;

	/* All GPU addresses as assigned are page aligned, but some
	   functions purturb the gpuaddr with an offset, so apply the
	   mask here to make sure we have the right address */

	unsigned int gpuaddr = memdesc->gpuaddr &  KGSL_MMU_ALIGN_MASK;

	if (range == 0 || gpuaddr == 0)
		return 0;

	numpages = (range >> PAGE_SHIFT);
	if (range & (PAGE_SIZE - 1))
		numpages++;

	ptefirst = kgsl_pt_entry_get(pagetable, gpuaddr);
	ptelast = ptefirst + numpages;

	spin_lock(&pagetable->lock);
	superpte = ptefirst - (ptefirst & (GSL_PT_SUPER_PTE-1));
	GSL_TLBFLUSH_FILTER_SETDIRTY(superpte / GSL_PT_SUPER_PTE);
	for (pte = ptefirst; pte < ptelast; pte++) {
#ifdef VERBOSE_DEBUG
		/* check if PTE exists */
		BUG_ON(!kgsl_pt_map_getaddr(pagetable, pte));
#endif
		kgsl_pt_map_set(pagetable, pte, GSL_PT_PAGE_DIRTY);
		superpte = pte - (pte & (GSL_PT_SUPER_PTE - 1));
		if (pte == superpte)
			GSL_TLBFLUSH_FILTER_SETDIRTY(superpte /
				GSL_PT_SUPER_PTE);
	}

	/* Remove the statistics */
	pagetable->stats.entries--;
	pagetable->stats.mapped -= range;

	/* Post all writes to the pagetable */
	wmb();

	spin_unlock(&pagetable->lock);

	gen_pool_free(pagetable->pool, gpuaddr, range);

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
	/*
	 *  stop device mmu
	 *
	 *  call this with the global lock held
	 */
	struct kgsl_mmu *mmu = &device->mmu;

	if (mmu->flags & KGSL_FLAGS_STARTED) {
		/* disable MMU */
		kgsl_regwrite(device, MH_MMU_CONFIG, 0x00000000);

		mmu->flags &= ~KGSL_FLAGS_STARTED;
	}

	return 0;
}
EXPORT_SYMBOL(kgsl_mmu_stop);

int kgsl_mmu_close(struct kgsl_device *device)
{
	/*
	 *  close device mmu
	 *
	 *  call this with the global lock held
	 */
	struct kgsl_mmu *mmu = &device->mmu;

	if (mmu->dummyspace.gpuaddr)
		kgsl_sharedmem_free(&mmu->dummyspace);

	if (mmu->defaultpagetable)
		kgsl_mmu_putpagetable(mmu->defaultpagetable);

	return 0;
}
