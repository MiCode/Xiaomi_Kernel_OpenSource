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

#include "kgsl_iommu.h"

/* Identifier for the global page table */
/* Per process page tables will probably pass in the thread group
   as an identifier */
#define KGSL_MMU_GLOBAL_PT 0
#define KGSL_MMU_SECURE_PT 1

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
		atomic_t entries;
		atomic_long_t mapped;
		atomic_long_t max_mapped;
	} stats;
	const struct kgsl_mmu_pt_ops *pt_ops;
	unsigned int fault_addr;
	void *priv;
	struct kgsl_mmu *mmu;
};

struct kgsl_mmu;

struct kgsl_mmu_ops {
	int (*mmu_init) (struct kgsl_mmu *mmu);
	void (*mmu_close)(struct kgsl_mmu *mmu);
	int (*mmu_start) (struct kgsl_mmu *mmu);
	void (*mmu_stop) (struct kgsl_mmu *mmu);
	int (*mmu_set_pt) (struct kgsl_mmu *mmu, struct kgsl_pagetable *pt);
	uint64_t (*mmu_get_current_ttbr0)(struct kgsl_mmu *mmu);
	void (*mmu_pagefault_resume)(struct kgsl_mmu *mmu);
	void (*mmu_clear_fsr)(struct kgsl_mmu *mmu);
	void (*mmu_enable_clk)(struct kgsl_mmu *mmu);
	void (*mmu_disable_clk)(struct kgsl_mmu *mmu);
	unsigned int (*mmu_get_reg_ahbaddr)(struct kgsl_mmu *mmu,
			enum kgsl_iommu_context_id ctx_id,
			enum kgsl_iommu_reg_map reg);
	int (*mmu_set_pf_policy)(struct kgsl_mmu *mmu, unsigned long pf_policy);
	struct kgsl_protected_registers *(*mmu_get_prot_regs)
			(struct kgsl_mmu *mmu);
	int (*mmu_init_pt)(struct kgsl_mmu *mmu, struct kgsl_pagetable *);
	void (*mmu_add_global)(struct kgsl_mmu *mmu,
			struct kgsl_memdesc *memdesc);
	void (*mmu_remove_global)(struct kgsl_mmu *mmu,
			struct kgsl_memdesc *memdesc);
};

struct kgsl_mmu_pt_ops {
	int (*mmu_map)(struct kgsl_pagetable *pt,
			struct kgsl_memdesc *memdesc);
	int (*mmu_unmap)(struct kgsl_pagetable *pt,
			struct kgsl_memdesc *memdesc);
	void (*mmu_destroy_pagetable) (struct kgsl_pagetable *);
	u64 (*get_ttbr0)(struct kgsl_pagetable *);
	u32 (*get_contextidr)(struct kgsl_pagetable *);
	int (*get_gpuaddr)(struct kgsl_pagetable *, struct kgsl_memdesc *);
	void (*put_gpuaddr)(struct kgsl_pagetable *, struct kgsl_memdesc *);
	uint64_t (*find_svm_region)(struct kgsl_pagetable *, uint64_t, uint64_t,
		uint64_t, uint64_t);
	int (*set_svm_region)(struct kgsl_pagetable *, uint64_t, uint64_t);
	int (*svm_range)(struct kgsl_pagetable *, uint64_t *, uint64_t *,
			uint64_t);
	bool (*addr_in_range)(struct kgsl_pagetable *pagetable, uint64_t);
};

/*
 * MMU_FEATURE - return true if the specified feature is supported by the GPU
 * MMU
 */
#define MMU_FEATURE(_mmu, _bit) \
	((_mmu)->features & (_bit))

/* MMU has register retention */
#define KGSL_MMU_RETENTION  BIT(1)
/* MMU requires the TLB to be flushed on map */
#define KGSL_MMU_FLUSH_TLB_ON_MAP BIT(2)
/* MMU uses global pagetable */
#define KGSL_MMU_GLOBAL_PAGETABLE BIT(3)
/* MMU uses hypervisor for content protection */
#define KGSL_MMU_HYP_SECURE_ALLOC BIT(4)
/* Force 32 bit, even if the MMU can do 64 bit */
#define KGSL_MMU_FORCE_32BIT BIT(5)
/* 64 bit address is live */
#define KGSL_MMU_64BIT BIT(6)
/* MMU can do coherent hardware table walks */
#define KGSL_MMU_COHERENT_HTW BIT(7)

struct kgsl_mmu {
	uint32_t      flags;
	/* current page table object being used by device mmu */
	struct kgsl_pagetable  *defaultpagetable;
	/* secure global pagetable device mmu */
	struct kgsl_pagetable  *securepagetable;
	const struct kgsl_mmu_ops *mmu_ops;
	void *priv;
	bool secured;
	uint features;
	unsigned int secure_align_mask;
};

extern struct kgsl_mmu_ops kgsl_iommu_ops;

struct kgsl_pagetable *kgsl_mmu_getpagetable(struct kgsl_mmu *,
						unsigned long name);

struct kgsl_pagetable *kgsl_mmu_getpagetable_ptbase(struct kgsl_mmu *,
						u64 ptbase);

void kgsl_add_global_secure_entry(struct kgsl_device *device,
					struct kgsl_memdesc *memdesc);
void kgsl_mmu_putpagetable(struct kgsl_pagetable *pagetable);
int kgsl_mmu_init(struct kgsl_device *device, char *mmutype);
int kgsl_mmu_start(struct kgsl_device *device);
void kgsl_mmu_close(struct kgsl_device *device);
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
int kgsl_mmu_get_ptname_from_ptbase(struct kgsl_mmu *mmu, u64 pt_base);
unsigned int kgsl_mmu_log_fault_addr(struct kgsl_mmu *mmu,
			phys_addr_t pt_base, unsigned int addr);
int kgsl_mmu_enabled(void);
void kgsl_mmu_set_mmutype(enum kgsl_mmutype type);
enum kgsl_mmutype kgsl_mmu_get_mmutype(void);
bool kgsl_mmu_gpuaddr_in_range(struct kgsl_pagetable *pt, uint64_t gpuaddr);

int kgsl_mmu_get_region(struct kgsl_pagetable *pagetable,
		uint64_t gpuaddr, uint64_t size);

int kgsl_mmu_find_region(struct kgsl_pagetable *pagetable,
		uint64_t region_start, uint64_t region_end,
		uint64_t *gpuaddr, uint64_t size, unsigned int align);

void kgsl_mmu_add_global(struct kgsl_device *device,
	struct kgsl_memdesc *memdesc);
void kgsl_mmu_remove_global(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc);

struct kgsl_pagetable *kgsl_mmu_get_pt_from_ptname(struct kgsl_mmu *mmu,
							int ptname);

uint64_t kgsl_mmu_find_svm_region(struct kgsl_pagetable *pagetable,
		uint64_t start, uint64_t end, uint64_t size,
		uint64_t alignment);

int kgsl_mmu_set_svm_region(struct kgsl_pagetable *pagetable, uint64_t gpuaddr,
		uint64_t size);

void kgsl_mmu_detach_pagetable(struct kgsl_pagetable *pagetable);

int kgsl_mmu_svm_range(struct kgsl_pagetable *pagetable,
		uint64_t *lo, uint64_t *hi, uint64_t memflags);

/*
 * Static inline functions of MMU that simply call the SMMU specific
 * function using a function pointer. These functions can be thought
 * of as wrappers around the actual function
 */

#define MMU_OP_VALID(_mmu, _field) \
	(((_mmu) != NULL) && \
	 ((_mmu)->mmu_ops != NULL) && \
	 ((_mmu)->mmu_ops->_field != NULL))

#define PT_OP_VALID(_pt, _field) \
	(((_pt) != NULL) && \
	 ((_pt)->pt_ops != NULL) && \
	 ((_pt)->pt_ops->_field != NULL))

static inline u64 kgsl_mmu_get_current_ttbr0(struct kgsl_mmu *mmu)
{
	if (MMU_OP_VALID(mmu, mmu_get_current_ttbr0))
		return mmu->mmu_ops->mmu_get_current_ttbr0(mmu);

	return 0;
}

static inline int kgsl_mmu_set_pt(struct kgsl_mmu *mmu,
					struct kgsl_pagetable *pagetable)
{
	if (MMU_OP_VALID(mmu, mmu_set_pt))
		return mmu->mmu_ops->mmu_set_pt(mmu, pagetable);

	return 0;
}

static inline void kgsl_mmu_stop(struct kgsl_mmu *mmu)
{
	if (MMU_OP_VALID(mmu, mmu_stop))
		mmu->mmu_ops->mmu_stop(mmu);
}

static inline void kgsl_mmu_enable_clk(struct kgsl_mmu *mmu)
{
	if (MMU_OP_VALID(mmu, mmu_enable_clk))
		mmu->mmu_ops->mmu_enable_clk(mmu);
}

static inline void kgsl_mmu_disable_clk(struct kgsl_mmu *mmu)
{
	if (MMU_OP_VALID(mmu, mmu_disable_clk))
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
	if (MMU_OP_VALID(mmu, mmu_get_reg_ahbaddr))
		return mmu->mmu_ops->mmu_get_reg_ahbaddr(mmu, ctx_id, reg);

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
	if (MMU_OP_VALID(mmu, mmu_set_pf_policy))
		return mmu->mmu_ops->mmu_set_pf_policy(mmu, pf_policy);

	return 0;
}

static inline void kgsl_mmu_pagefault_resume(struct kgsl_mmu *mmu)
{
	if (MMU_OP_VALID(mmu, mmu_pagefault_resume))
		return mmu->mmu_ops->mmu_pagefault_resume(mmu);
}

static inline void kgsl_mmu_clear_fsr(struct kgsl_mmu *mmu)
{
	if (MMU_OP_VALID(mmu, mmu_clear_fsr))
		return mmu->mmu_ops->mmu_clear_fsr(mmu);
}

static inline struct kgsl_protected_registers *kgsl_mmu_get_prot_regs
						(struct kgsl_mmu *mmu)
{
	if (MMU_OP_VALID(mmu, mmu_get_prot_regs))
		return mmu->mmu_ops->mmu_get_prot_regs(mmu);

	return NULL;
}

static inline int kgsl_mmu_is_secured(struct kgsl_mmu *mmu)
{
	return mmu && (mmu->secured) && (mmu->securepagetable);
}

static inline u64
kgsl_mmu_pagetable_get_ttbr0(struct kgsl_pagetable *pagetable)
{
	if (PT_OP_VALID(pagetable, get_ttbr0))
		return pagetable->pt_ops->get_ttbr0(pagetable);

	return 0;
}

static inline u32
kgsl_mmu_pagetable_get_contextidr(struct kgsl_pagetable *pagetable)
{
	if (PT_OP_VALID(pagetable, get_contextidr))
		return pagetable->pt_ops->get_contextidr(pagetable);

	return 0;
}

#ifdef CONFIG_MSM_IOMMU
#include <linux/qcom_iommu.h>
static inline bool kgsl_mmu_bus_secured(struct device *dev)
{
	struct bus_type *bus = msm_iommu_get_bus(dev);

	return (bus == &msm_iommu_sec_bus_type) ? true : false;
}
static inline struct bus_type *kgsl_mmu_get_bus(struct device *dev)
{
	return msm_iommu_get_bus(dev);
}
static inline struct device *kgsl_mmu_get_ctx(const char *name)
{
	return msm_iommu_get_ctx(name);
}
#else
static inline bool kgsl_mmu_bus_secured(struct device *dev)
{
	return false;
}

static inline struct bus_type *kgsl_mmu_get_bus(struct device *dev)
{
	return &platform_bus_type;
}
static inline struct device *kgsl_mmu_get_ctx(const char *name)
{
	return ERR_PTR(-ENODEV);
}
#endif

#endif /* __KGSL_MMU_H */
