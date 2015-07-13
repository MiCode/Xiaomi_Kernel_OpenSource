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
#ifndef __KGSL_MMU_H
#define __KGSL_MMU_H

#include <linux/qcom_iommu.h>
#include "kgsl_iommu.h"
/*
 * These defines control the address range for allocations that
 * are mapped into all pagetables.
 */
#define KGSL_GLOBAL_PT_SIZE	SZ_4M
#define KGSL_MMU_GLOBAL_MEM_BASE	0xf8000000

/*
 * These defines control the address range for allocations that
 * are mapped into secure pagetable.
 */
#define KGSL_IOMMU_SECURE_MEM_BASE     0xe8000000
#define KGSL_IOMMU_SECURE_MEM_SIZE     SZ_256M

/* Identifier for the global page table */
/* Per process page tables will probably pass in the thread group
   as an identifier */
#define KGSL_MMU_GLOBAL_PT 0
#define KGSL_MMU_SECURE_PT 1
#define KGSL_MMU_PRIV_PT   0xFFFFFFFF

struct kgsl_device;

enum kgsl_mmutype {
	KGSL_MMU_TYPE_IOMMU = 0,
	KGSL_MMU_TYPE_NONE
};

struct kgsl_pagetable {
	spinlock_t lock;
	struct kref refcount;
	struct list_head list;
	unsigned int name;
	struct kobject *kobj;

	struct {
		unsigned int entries;
		uint64_t mapped;
		uint64_t max_mapped;
	} stats;
	const struct kgsl_mmu_pt_ops *pt_ops;
	unsigned int fault_addr;
	void *priv;
	struct kgsl_mmu *mmu;
	unsigned long *mem_bitmap;
	unsigned int bitmap_size;
	bool globals_mapped;
};

struct kgsl_mmu;

struct kgsl_mmu_ops {
	int (*mmu_init) (struct kgsl_mmu *mmu);
	int (*mmu_close) (struct kgsl_mmu *mmu);
	int (*mmu_start) (struct kgsl_mmu *mmu);
	void (*mmu_stop) (struct kgsl_mmu *mmu);
	int (*mmu_set_pt) (struct kgsl_mmu *mmu, struct kgsl_pagetable *pt);
	phys_addr_t (*mmu_get_current_ptbase)
			(struct kgsl_mmu *mmu);
	void (*mmu_pagefault_resume)
			(struct kgsl_mmu *mmu);
	void (*mmu_enable_clk)
		(struct kgsl_mmu *mmu);
	void (*mmu_disable_clk)
		(struct kgsl_mmu *mmu);
	uint64_t (*mmu_get_default_ttbr0)(struct kgsl_mmu *mmu,
				enum kgsl_iommu_context_id ctx_id);
	unsigned int (*mmu_get_reg_ahbaddr)(struct kgsl_mmu *mmu,
			enum kgsl_iommu_context_id ctx_id,
			enum kgsl_iommu_reg_map reg);
	int (*mmu_pt_equal) (struct kgsl_mmu *mmu,
			struct kgsl_pagetable *pt,
			phys_addr_t pt_base);
	phys_addr_t (*mmu_get_pt_base_addr)
			(struct kgsl_mmu *mmu,
			struct kgsl_pagetable *pt);
	int (*mmu_set_pf_policy)(struct kgsl_mmu *mmu, unsigned long pf_policy);
	void (*mmu_set_pagefault)(struct kgsl_mmu *mmu);
	struct kgsl_protected_registers *(*mmu_get_prot_regs)
			(struct kgsl_mmu *mmu);
	int (*mmu_init_pt)(struct kgsl_mmu *mmu, struct kgsl_pagetable *);
};

struct kgsl_mmu_pt_ops {
	int (*mmu_map) (struct kgsl_pagetable *pt,
			struct kgsl_memdesc *memdesc);
	int (*mmu_unmap) (struct kgsl_pagetable *pt,
			struct kgsl_memdesc *memdesc);
	void (*mmu_destroy_pagetable) (struct kgsl_pagetable *);
	phys_addr_t (*get_ptbase) (struct kgsl_pagetable *);
};

/*
 * MMU_FEATURE - return true if the specified feature is supported by the GPU
 * MMU
 */
#define MMU_FEATURE(_mmu, _bit) \
	((_mmu)->features & (_bit))

/* MMU can use DMA API */
#define KGSL_MMU_DMA_API    BIT(0)
/* MMU has register retention */
#define KGSL_MMU_RETENTION  BIT(1)
/* MMU requires the TLB to be flushed on map */
#define KGSL_MMU_FLUSH_TLB_ON_MAP BIT(2)
/* MMU uses global pagetable */
#define KGSL_MMU_GLOBAL_PAGETABLE BIT(3)
/* MMU uses hypervisor for content protection */
#define KGSL_MMU_HYP_SECURE_ALLOC BIT(4)

struct kgsl_mmu {
	uint32_t      flags;
	struct kgsl_device     *device;
	struct kgsl_memdesc    setstate_memory;
	/* current page table object being used by device mmu */
	struct kgsl_pagetable  *defaultpagetable;
	/* secure global pagetable device mmu */
	struct kgsl_pagetable  *securepagetable;
	const struct kgsl_mmu_ops *mmu_ops;
	void *priv;
	atomic_t fault;
	bool secured;
	uint features;
};

extern struct kgsl_mmu_ops kgsl_iommu_ops;

struct kgsl_pagetable *kgsl_mmu_getpagetable(struct kgsl_mmu *,
						unsigned long name);

struct kgsl_pagetable *kgsl_mmu_getpagetable_ptbase(struct kgsl_mmu *,
						phys_addr_t ptbase);

void kgsl_mmu_putpagetable(struct kgsl_pagetable *pagetable);
int kgsl_mmu_init(struct kgsl_device *device);
int kgsl_mmu_start(struct kgsl_device *device);
int kgsl_mmu_close(struct kgsl_device *device);
int kgsl_mmu_map(struct kgsl_pagetable *pagetable,
		 struct kgsl_memdesc *memdesc);
int kgsl_mmu_get_gpuaddr(struct kgsl_pagetable *pagetable,
		 struct kgsl_memdesc *memdesc);
int kgsl_mmu_map_global(struct kgsl_pagetable *pagetable,
			struct kgsl_memdesc *memdesc);
int kgsl_mmu_unmap(struct kgsl_pagetable *pagetable,
		    struct kgsl_memdesc *memdesc);
int kgsl_mmu_put_gpuaddr(struct kgsl_pagetable *pagetable,
		 struct kgsl_memdesc *memdesc);
unsigned int kgsl_virtaddr_to_physaddr(void *virtaddr);
int kgsl_mmu_get_ptname_from_ptbase(struct kgsl_mmu *mmu,
					phys_addr_t pt_base);
unsigned int kgsl_mmu_log_fault_addr(struct kgsl_mmu *mmu,
			phys_addr_t pt_base, unsigned int addr);
int kgsl_mmu_enabled(void);
void kgsl_mmu_set_mmutype(char *mmutype);
enum kgsl_mmutype kgsl_mmu_get_mmutype(void);
int kgsl_mmu_gpuaddr_in_range(struct kgsl_pagetable *pt, uint64_t gpuaddr);

int kgsl_add_global_pt_entry(struct kgsl_device *device,
	struct kgsl_memdesc *memdesc);
void kgsl_remove_global_pt_entry(struct kgsl_memdesc *memdesc);
void kgsl_map_global_pt_entries(struct kgsl_pagetable *pagetable);

struct kgsl_memdesc *kgsl_search_global_pt_entries(unsigned int gpuaddr,
		unsigned int size);
struct kgsl_pagetable *kgsl_mmu_get_pt_from_ptname(struct kgsl_mmu *mmu,
							int ptname);

void kgsl_mmu_detach_pagetable(struct kgsl_pagetable *pagetable);
/*
 * Static inline functions of MMU that simply call the SMMU specific
 * function using a function pointer. These functions can be thought
 * of as wrappers around the actual function
 */

static inline phys_addr_t kgsl_mmu_get_current_ptbase(struct kgsl_mmu *mmu)
{
	if (mmu->mmu_ops && mmu->mmu_ops->mmu_get_current_ptbase)
		return mmu->mmu_ops->mmu_get_current_ptbase(mmu);
	else
		return 0;
}

static inline int kgsl_mmu_set_pt(struct kgsl_mmu *mmu,
					struct kgsl_pagetable *pagetable)
{
	if (mmu->mmu_ops && mmu->mmu_ops->mmu_set_pt)
		return mmu->mmu_ops->mmu_set_pt(mmu, pagetable);

	return 0;
}

static inline void kgsl_mmu_stop(struct kgsl_mmu *mmu)
{
	if (mmu->mmu_ops && mmu->mmu_ops->mmu_stop)
		mmu->mmu_ops->mmu_stop(mmu);
}

static inline int kgsl_mmu_pt_equal(struct kgsl_mmu *mmu,
			struct kgsl_pagetable *pt,
			phys_addr_t pt_base)
{
	if (mmu->mmu_ops && mmu->mmu_ops->mmu_pt_equal)
		return mmu->mmu_ops->mmu_pt_equal(mmu, pt, pt_base);
	else
		return 1;
}

static inline phys_addr_t kgsl_mmu_get_pt_base_addr(struct kgsl_mmu *mmu,
						struct kgsl_pagetable *pt)
{
	if (mmu->mmu_ops && mmu->mmu_ops->mmu_get_pt_base_addr)
		return mmu->mmu_ops->mmu_get_pt_base_addr(mmu, pt);
	else
		return 0;
}

static inline phys_addr_t kgsl_mmu_get_default_ttbr0(struct kgsl_mmu *mmu,
					enum kgsl_iommu_context_id ctx_id)
{
	if (mmu->mmu_ops && mmu->mmu_ops->mmu_get_default_ttbr0)
		return mmu->mmu_ops->mmu_get_default_ttbr0(mmu, ctx_id);
	else
		return 0;
}

static inline void kgsl_mmu_enable_clk(struct kgsl_mmu *mmu)
{
	if (mmu->mmu_ops && mmu->mmu_ops->mmu_enable_clk)
		mmu->mmu_ops->mmu_enable_clk(mmu);
	else
		return;
}

static inline void kgsl_mmu_disable_clk(struct kgsl_mmu *mmu)
{
	if (mmu->mmu_ops && mmu->mmu_ops->mmu_disable_clk)
		mmu->mmu_ops->mmu_disable_clk(mmu);
}

/*
 * kgsl_mmu_get_reg_ahbaddr() - Calls the mmu specific function pointer to
 * return the address that GPU can use to access register
 * @mmu:		Pointer to the device mmu
 * @ctx_id:		The MMU HW context ID
 * @reg:		Register whose address is to be returned
 *
 * Returns the ahb address of reg else 0
 */
static inline unsigned int kgsl_mmu_get_reg_ahbaddr(struct kgsl_mmu *mmu,
				enum kgsl_iommu_context_id ctx_id,
				enum kgsl_iommu_reg_map reg)
{
	if (mmu->mmu_ops && mmu->mmu_ops->mmu_get_reg_ahbaddr)
		return mmu->mmu_ops->mmu_get_reg_ahbaddr(mmu, ctx_id, reg);
	else
		return 0;
}

/*
 * kgsl_mmu_is_perprocess() - Runtime check for per-process
 * pagetables.
 * @mmu: the mmu
 *
 * Returns true if per-process pagetables are enabled,
 * false if not.
 */
static inline int kgsl_mmu_is_perprocess(struct kgsl_mmu *mmu)
{
	return MMU_FEATURE(mmu, KGSL_MMU_GLOBAL_PAGETABLE) ? 0 : 1;
}

/*
 * kgsl_mmu_use_cpu_map() - Runtime check for matching the CPU
 * address space on the GPU.
 * @mmu: the mmu
 *
 * Returns true if supported false if not.
 */
static inline int kgsl_mmu_use_cpu_map(struct kgsl_mmu *mmu)
{
	return kgsl_mmu_is_perprocess(mmu) &&
		kgsl_mmu_get_mmutype() != KGSL_MMU_TYPE_NONE;
}

static inline int kgsl_mmu_set_pagefault_policy(struct kgsl_mmu *mmu,
						unsigned long pf_policy)
{
	if (mmu->mmu_ops && mmu->mmu_ops->mmu_set_pf_policy)
		return mmu->mmu_ops->mmu_set_pf_policy(mmu, pf_policy);
	else
		return 0;
}

static inline void kgsl_mmu_set_pagefault(struct kgsl_mmu *mmu)
{
	if (mmu->mmu_ops && mmu->mmu_ops->mmu_set_pagefault)
		return mmu->mmu_ops->mmu_set_pagefault(mmu);
}

static inline struct kgsl_protected_registers *kgsl_mmu_get_prot_regs
						(struct kgsl_mmu *mmu)
{
	if (mmu->mmu_ops && mmu->mmu_ops->mmu_get_prot_regs)
		return mmu->mmu_ops->mmu_get_prot_regs(mmu);
	else
		return NULL;
}

static inline int kgsl_mmu_is_secured(struct kgsl_mmu *mmu)
{
	return mmu && (mmu->secured) && (mmu->securepagetable);
}

static inline phys_addr_t
kgsl_mmu_pagetable_get_ptbase(struct kgsl_pagetable *pagetable)
{
	if (pagetable && pagetable->pt_ops->get_ptbase)
		return pagetable->pt_ops->get_ptbase(pagetable);
	return 0;
}



#endif /* __KGSL_MMU_H */
