/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
#include "kgsl_gpummu.h"
#include "kgsl_device.h"
#include "kgsl_sharedmem.h"
#include "kgsl_trace.h"
#include "adreno.h"

#define KGSL_PAGETABLE_SIZE \
	ALIGN(KGSL_PAGETABLE_ENTRIES(CONFIG_MSM_KGSL_PAGE_TABLE_SIZE) * \
	KGSL_PAGETABLE_ENTRY_SIZE, PAGE_SIZE)

static ssize_t
sysfs_show_ptpool_entries(struct kobject *kobj,
			  struct kobj_attribute *attr,
			  char *buf)
{
	struct kgsl_ptpool *pool = (struct kgsl_ptpool *)
					kgsl_driver.ptpool;
	return snprintf(buf, PAGE_SIZE, "%d\n", pool->entries);
}

static ssize_t
sysfs_show_ptpool_min(struct kobject *kobj,
			 struct kobj_attribute *attr,
			 char *buf)
{
	struct kgsl_ptpool *pool = (struct kgsl_ptpool *)
					kgsl_driver.ptpool;
	return snprintf(buf, PAGE_SIZE, "%d\n",
			pool->static_entries);
}

static ssize_t
sysfs_show_ptpool_chunks(struct kobject *kobj,
			 struct kobj_attribute *attr,
			 char *buf)
{
	struct kgsl_ptpool *pool = (struct kgsl_ptpool *)
					kgsl_driver.ptpool;
	return snprintf(buf, PAGE_SIZE, "%d\n", pool->chunks);
}

static ssize_t
sysfs_show_ptpool_ptsize(struct kobject *kobj,
			 struct kobj_attribute *attr,
			 char *buf)
{
	struct kgsl_ptpool *pool = (struct kgsl_ptpool *)
					kgsl_driver.ptpool;
	return snprintf(buf, PAGE_SIZE, "%d\n", pool->ptsize);
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
_kgsl_ptpool_get_entry(struct kgsl_ptpool *pool, phys_addr_t *physaddr)
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

static int
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

static void *kgsl_ptpool_alloc(struct kgsl_ptpool *pool,
				phys_addr_t *physaddr)
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

static void kgsl_ptpool_free(struct kgsl_ptpool *pool, void *addr)
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

void kgsl_gpummu_ptpool_destroy(void *ptpool)
{
	struct kgsl_ptpool *pool = (struct kgsl_ptpool *)ptpool;
	struct kgsl_ptpool_chunk *chunk, *tmp;

	if (pool == NULL)
		return;

	mutex_lock(&pool->lock);
	list_for_each_entry_safe(chunk, tmp, &pool->list, list)
		_kgsl_ptpool_rm_chunk(chunk);
	mutex_unlock(&pool->lock);

	kfree(pool);
}

/**
 * kgsl_ptpool_init
 * @pool:  A pointer to a ptpool structure to initialize
 * @entries:  The number of inital entries to add to the pool
 *
 * Initalize a pool and allocate an initial chunk of entries.
 */
void *kgsl_gpummu_ptpool_init(int entries)
{
	int ptsize = KGSL_PAGETABLE_SIZE;
	struct kgsl_ptpool *pool;
	int ret = 0;

	pool = kzalloc(sizeof(struct kgsl_ptpool), GFP_KERNEL);
	if (!pool) {
		KGSL_CORE_ERR("Failed to allocate memory "
				"for ptpool\n");
		return NULL;
	}

	pool->ptsize = ptsize;
	mutex_init(&pool->lock);
	INIT_LIST_HEAD(&pool->list);

	if (entries) {
		ret = kgsl_ptpool_add(pool, entries);
		if (ret)
			goto err_ptpool_remove;
	}

	ret = sysfs_create_group(kgsl_driver.ptkobj, &ptpool_attr_group);
	if (ret) {
		KGSL_CORE_ERR("sysfs_create_group failed for ptpool "
				"statistics: %d\n", ret);
		goto err_ptpool_remove;
	}
	return (void *)pool;

err_ptpool_remove:
	kgsl_gpummu_ptpool_destroy(pool);
	return NULL;
}

int kgsl_gpummu_pt_equal(struct kgsl_mmu *mmu,
			struct kgsl_pagetable *pt,
			phys_addr_t pt_base)
{
	struct kgsl_gpummu_pt *gpummu_pt = pt ? pt->priv : NULL;
	return gpummu_pt && pt_base && (gpummu_pt->base.gpuaddr == pt_base);
}

void kgsl_gpummu_destroy_pagetable(struct kgsl_pagetable *pt)
{
	struct kgsl_gpummu_pt *gpummu_pt = pt->priv;
	kgsl_ptpool_free((struct kgsl_ptpool *)kgsl_driver.ptpool,
				gpummu_pt->base.hostptr);

	kgsl_driver.stats.coherent -= KGSL_PAGETABLE_SIZE;

	kfree(gpummu_pt->tlbflushfilter.base);

	kfree(gpummu_pt);
}

static inline uint32_t
kgsl_pt_entry_get(unsigned int va_base, uint32_t va)
{
	return (va - va_base) >> PAGE_SHIFT;
}

static inline void
kgsl_pt_map_set(struct kgsl_gpummu_pt *pt, uint32_t pte, uint32_t val)
{
	uint32_t *baseptr = (uint32_t *)pt->base.hostptr;
	BUG_ON(pte*sizeof(uint32_t) >= pt->base.size);
	baseptr[pte] = val;
}

static inline uint32_t
kgsl_pt_map_get(struct kgsl_gpummu_pt *pt, uint32_t pte)
{
	uint32_t *baseptr = (uint32_t *)pt->base.hostptr;
	BUG_ON(pte*sizeof(uint32_t) >= pt->base.size);
	return baseptr[pte] & GSL_PT_PAGE_ADDR_MASK;
}

static void kgsl_gpummu_pagefault(struct kgsl_mmu *mmu)
{
	unsigned int reg;
	unsigned int ptbase;
	struct kgsl_device *device;
	struct adreno_device *adreno_dev;
	unsigned int no_page_fault_log = 0;

	device = mmu->device;
	adreno_dev = ADRENO_DEVICE(device);

	kgsl_regread(device, MH_MMU_PAGE_FAULT, &reg);
	kgsl_regread(device, MH_MMU_PT_BASE, &ptbase);


	if (adreno_dev->ft_pf_policy & KGSL_FT_PAGEFAULT_LOG_ONE_PER_PAGE)
		no_page_fault_log = kgsl_mmu_log_fault_addr(mmu, ptbase, reg);

	if (!no_page_fault_log)
		KGSL_MEM_CRIT(mmu->device,
			"mmu page fault: page=0x%lx pt=%d op=%s axi=%d\n",
			reg & ~(PAGE_SIZE - 1),
			kgsl_mmu_get_ptname_from_ptbase(mmu, ptbase),
			reg & 0x02 ? "WRITE" : "READ", (reg >> 4) & 0xF);
	trace_kgsl_mmu_pagefault(mmu->device, reg & ~(PAGE_SIZE - 1),
			kgsl_mmu_get_ptname_from_ptbase(mmu, ptbase),
			reg & 0x02 ? "WRITE" : "READ");
}

static void *kgsl_gpummu_create_pagetable(void)
{
	struct kgsl_gpummu_pt *gpummu_pt;

	gpummu_pt = kzalloc(sizeof(struct kgsl_gpummu_pt),
				GFP_KERNEL);
	if (!gpummu_pt)
		return NULL;

	gpummu_pt->last_superpte = 0;

	gpummu_pt->tlbflushfilter.size = (CONFIG_MSM_KGSL_PAGE_TABLE_SIZE /
				(PAGE_SIZE * GSL_PT_SUPER_PTE * 8)) + 1;
	gpummu_pt->tlbflushfilter.base = (unsigned int *)
			kzalloc(gpummu_pt->tlbflushfilter.size, GFP_KERNEL);
	if (!gpummu_pt->tlbflushfilter.base) {
		KGSL_CORE_ERR("kzalloc(%d) failed\n",
			gpummu_pt->tlbflushfilter.size);
		goto err_free_gpummu;
	}
	GSL_TLBFLUSH_FILTER_RESET();

	gpummu_pt->base.hostptr = kgsl_ptpool_alloc((struct kgsl_ptpool *)
						kgsl_driver.ptpool,
						&gpummu_pt->base.physaddr);

	if (gpummu_pt->base.hostptr == NULL)
		goto err_flushfilter;

	/* Do a check before truncating phys_addr_t to unsigned 32 */
	if (sizeof(phys_addr_t) > sizeof(unsigned int)) {
		WARN_ONCE(1, "Cannot use LPAE with gpummu\n");
		goto err_flushfilter;
	}
	gpummu_pt->base.gpuaddr = gpummu_pt->base.physaddr;
	gpummu_pt->base.size = KGSL_PAGETABLE_SIZE;

	/* ptpool allocations are from coherent memory, so update the
	   device statistics acordingly */

	KGSL_STATS_ADD(KGSL_PAGETABLE_SIZE, kgsl_driver.stats.coherent,
		       kgsl_driver.stats.coherent_max);

	return (void *)gpummu_pt;

err_flushfilter:
	kfree(gpummu_pt->tlbflushfilter.base);
err_free_gpummu:
	kfree(gpummu_pt);

	return NULL;
}

static void kgsl_gpummu_default_setstate(struct kgsl_mmu *mmu,
					uint32_t flags)
{
	struct kgsl_gpummu_pt *gpummu_pt;
	if (!kgsl_mmu_enabled())
		return;

	if (flags & KGSL_MMUFLAGS_PTUPDATE) {
		kgsl_idle(mmu->device);
		gpummu_pt = mmu->hwpagetable->priv;
		kgsl_regwrite(mmu->device, MH_MMU_PT_BASE,
			gpummu_pt->base.gpuaddr);
	}

	if (flags & KGSL_MMUFLAGS_TLBFLUSH) {
		/* Invalidate all and tc */
		kgsl_regwrite(mmu->device, MH_MMU_INVALIDATE,  0x00000003);
	}
}

static void kgsl_gpummu_setstate(struct kgsl_mmu *mmu,
				struct kgsl_pagetable *pagetable,
				unsigned int context_id)
{
	if (mmu->flags & KGSL_FLAGS_STARTED) {
		/* page table not current, then setup mmu to use new
		 *  specified page table
		 */
		if (mmu->hwpagetable != pagetable) {
			mmu->hwpagetable = pagetable;
			/* Since we do a TLB flush the tlb_flags should
			 * be cleared by calling kgsl_mmu_pt_get_flags
			 */
			kgsl_mmu_pt_get_flags(pagetable, mmu->device->id);

			/* call device specific set page table */
			kgsl_setstate(mmu, context_id, KGSL_MMUFLAGS_TLBFLUSH |
				KGSL_MMUFLAGS_PTUPDATE);
		}
	}
}

static int kgsl_gpummu_init(struct kgsl_mmu *mmu)
{
	/*
	 * intialize device mmu
	 *
	 * call this with the global lock held
	 */
	int status = 0;

	mmu->pt_base = KGSL_PAGETABLE_BASE;
	mmu->pt_size = CONFIG_MSM_KGSL_PAGE_TABLE_SIZE;
	mmu->pt_per_process = KGSL_MMU_USE_PER_PROCESS_PT;
	mmu->use_cpu_map = false;

	/* sub-client MMU lookups require address translation */
	if ((mmu->config & ~0x1) > 0) {
		/*make sure virtual address range is a multiple of 64Kb */
		if (CONFIG_MSM_KGSL_PAGE_TABLE_SIZE & ((1 << 16) - 1)) {
			KGSL_CORE_ERR("Invalid pagetable size requested "
			"for GPUMMU: %x\n", CONFIG_MSM_KGSL_PAGE_TABLE_SIZE);
			return -EINVAL;
		}
	}

	dev_info(mmu->device->dev, "|%s| MMU type set for device is GPUMMU\n",
		__func__);
	return status;
}

static int kgsl_gpummu_start(struct kgsl_mmu *mmu)
{
	/*
	 * intialize device mmu
	 *
	 * call this with the global lock held
	 */

	struct kgsl_device *device = mmu->device;
	struct kgsl_gpummu_pt *gpummu_pt;

	if (mmu->flags & KGSL_FLAGS_STARTED)
		return 0;

	/* MMU not enabled */
	if ((mmu->config & 0x1) == 0)
		return 0;

	/* setup MMU and sub-client behavior */
	kgsl_regwrite(device, MH_MMU_CONFIG, mmu->config);

	/* idle device */
	kgsl_idle(device);

	/* enable axi interrupts */
	kgsl_regwrite(device, MH_INTERRUPT_MASK,
			GSL_MMU_INT_MASK | MH_INTERRUPT_MASK__MMU_PAGE_FAULT);

	kgsl_sharedmem_set(device, &mmu->setstate_memory, 0, 0,
			   mmu->setstate_memory.size);

	/* TRAN_ERROR needs a 32 byte (32 byte aligned) chunk of memory
	 * to complete transactions in case of an MMU fault. Note that
	 * we'll leave the bottom 32 bytes of the setstate_memory for other
	 * purposes (e.g. use it when dummy read cycles are needed
	 * for other blocks) */
	kgsl_regwrite(device, MH_MMU_TRAN_ERROR,
		mmu->setstate_memory.physaddr + 32);

	if (mmu->defaultpagetable == NULL)
		mmu->defaultpagetable =
			kgsl_mmu_getpagetable(mmu, KGSL_MMU_GLOBAL_PT);

	/* Return error if the default pagetable doesn't exist */
	if (mmu->defaultpagetable == NULL)
		return -ENOMEM;

	mmu->hwpagetable = mmu->defaultpagetable;
	gpummu_pt = mmu->hwpagetable->priv;
	kgsl_regwrite(mmu->device, MH_MMU_PT_BASE,
		      gpummu_pt->base.gpuaddr);
	kgsl_regwrite(mmu->device, MH_MMU_VA_RANGE,
		      (KGSL_PAGETABLE_BASE |
		      (CONFIG_MSM_KGSL_PAGE_TABLE_SIZE >> 16)));
	kgsl_setstate(mmu, KGSL_MEMSTORE_GLOBAL, KGSL_MMUFLAGS_TLBFLUSH);
	mmu->flags |= KGSL_FLAGS_STARTED;

	return 0;
}

static int
kgsl_gpummu_unmap(struct kgsl_pagetable *pt,
		struct kgsl_memdesc *memdesc,
		unsigned int *tlb_flags)
{
	unsigned int numpages;
	unsigned int pte, ptefirst, ptelast, superpte;
	unsigned int range = memdesc->size;
	struct kgsl_gpummu_pt *gpummu_pt = pt->priv;

	/* All GPU addresses as assigned are page aligned, but some
	   functions purturb the gpuaddr with an offset, so apply the
	   mask here to make sure we have the right address */

	unsigned int gpuaddr = memdesc->gpuaddr &  KGSL_MMU_ALIGN_MASK;

	numpages = (range >> PAGE_SHIFT);
	if (range & (PAGE_SIZE - 1))
		numpages++;

	ptefirst = kgsl_pt_entry_get(KGSL_PAGETABLE_BASE, gpuaddr);
	ptelast = ptefirst + numpages;

	superpte = ptefirst - (ptefirst & (GSL_PT_SUPER_PTE-1));
	GSL_TLBFLUSH_FILTER_SETDIRTY(superpte / GSL_PT_SUPER_PTE);
	for (pte = ptefirst; pte < ptelast; pte++) {
#ifdef VERBOSE_DEBUG
		/* check if PTE exists */
		if (!kgsl_pt_map_get(gpummu_pt, pte))
			KGSL_CORE_ERR("pt entry %x is already "
			"unmapped for pagetable %p\n", pte, gpummu_pt);
#endif
		kgsl_pt_map_set(gpummu_pt, pte, GSL_PT_PAGE_DIRTY);
		superpte = pte - (pte & (GSL_PT_SUPER_PTE - 1));
		if (pte == superpte)
			GSL_TLBFLUSH_FILTER_SETDIRTY(superpte /
				GSL_PT_SUPER_PTE);
	}

	/* Post all writes to the pagetable */
	wmb();

	return 0;
}

#define SUPERPTE_IS_DIRTY(_p) \
(((_p) & (GSL_PT_SUPER_PTE - 1)) == 0 && \
GSL_TLBFLUSH_FILTER_ISDIRTY((_p) / GSL_PT_SUPER_PTE))

static int
kgsl_gpummu_map(struct kgsl_pagetable *pt,
		struct kgsl_memdesc *memdesc,
		unsigned int protflags,
		unsigned int *tlb_flags)
{
	unsigned int pte;
	struct kgsl_gpummu_pt *gpummu_pt = pt->priv;
	struct scatterlist *s;
	int flushtlb = 0;
	int i;

	pte = kgsl_pt_entry_get(KGSL_PAGETABLE_BASE, memdesc->gpuaddr);

	/* Flush the TLB if the first PTE isn't at the superpte boundary */
	if (pte & (GSL_PT_SUPER_PTE - 1))
		flushtlb = 1;

	for_each_sg(memdesc->sg, s, memdesc->sglen, i) {
		unsigned int paddr = kgsl_get_sg_pa(s);
		unsigned int j;

		/* Each sg entry might be multiple pages long */
		for (j = paddr; j < paddr + s->length; pte++, j += PAGE_SIZE) {
			if (SUPERPTE_IS_DIRTY(pte))
				flushtlb = 1;
			kgsl_pt_map_set(gpummu_pt, pte, j | protflags);
		}
	}

	/* Flush the TLB if the last PTE isn't at the superpte boundary */
	if ((pte + 1) & (GSL_PT_SUPER_PTE - 1))
		flushtlb = 1;

	wmb();

	if (flushtlb) {
		/*set all devices as needing flushing*/
		*tlb_flags = UINT_MAX;
		GSL_TLBFLUSH_FILTER_RESET();
	}

	return 0;
}

static void kgsl_gpummu_stop(struct kgsl_mmu *mmu)
{
	mmu->flags &= ~KGSL_FLAGS_STARTED;
}

static int kgsl_gpummu_close(struct kgsl_mmu *mmu)
{
	/*
	 *  close device mmu
	 *
	 *  call this with the global lock held
	 */
	if (mmu->setstate_memory.gpuaddr)
		kgsl_sharedmem_free(&mmu->setstate_memory);

	if (mmu->defaultpagetable)
		kgsl_mmu_putpagetable(mmu->defaultpagetable);

	return 0;
}

static phys_addr_t
kgsl_gpummu_get_current_ptbase(struct kgsl_mmu *mmu)
{
	unsigned int ptbase;
	kgsl_regread(mmu->device, MH_MMU_PT_BASE, &ptbase);
	return ptbase;
}

static phys_addr_t
kgsl_gpummu_get_pt_base_addr(struct kgsl_mmu *mmu,
			struct kgsl_pagetable *pt)
{
	struct kgsl_gpummu_pt *gpummu_pt = pt->priv;
	return gpummu_pt->base.gpuaddr;
}

static int kgsl_gpummu_get_num_iommu_units(struct kgsl_mmu *mmu)
{
	return 1;
}

struct kgsl_mmu_ops gpummu_ops = {
	.mmu_init = kgsl_gpummu_init,
	.mmu_close = kgsl_gpummu_close,
	.mmu_start = kgsl_gpummu_start,
	.mmu_stop = kgsl_gpummu_stop,
	.mmu_setstate = kgsl_gpummu_setstate,
	.mmu_device_setstate = kgsl_gpummu_default_setstate,
	.mmu_pagefault = kgsl_gpummu_pagefault,
	.mmu_get_current_ptbase = kgsl_gpummu_get_current_ptbase,
	.mmu_pt_equal = kgsl_gpummu_pt_equal,
	.mmu_get_pt_base_addr = kgsl_gpummu_get_pt_base_addr,
	.mmu_enable_clk = NULL,
	.mmu_disable_clk_on_ts = NULL,
	.mmu_get_default_ttbr0 = NULL,
	.mmu_get_reg_gpuaddr = NULL,
	.mmu_get_reg_ahbaddr = NULL,
	.mmu_get_num_iommu_units = kgsl_gpummu_get_num_iommu_units,
	.mmu_hw_halt_supported = NULL,
};

struct kgsl_mmu_pt_ops gpummu_pt_ops = {
	.mmu_map = kgsl_gpummu_map,
	.mmu_unmap = kgsl_gpummu_unmap,
	.mmu_create_pagetable = kgsl_gpummu_create_pagetable,
	.mmu_destroy_pagetable = kgsl_gpummu_destroy_pagetable,
};
