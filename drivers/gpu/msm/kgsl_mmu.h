/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2002,2007-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef __KGSL_MMU_H
#define __KGSL_MMU_H

#include <linux/platform_device.h>

/* Identifier for the global page table */
/*
 * Per process page tables will probably pass in the thread group
 *  as an identifier
 */
#define KGSL_MMU_GLOBAL_PT 0
#define KGSL_MMU_SECURE_PT 1

#define MMU_DEFAULT_TTBR0(_d) \
	(kgsl_mmu_pagetable_get_ttbr0((_d)->mmu.defaultpagetable))

/* Mask ASID fields to match 48bit ptbase address*/
#define MMU_SW_PT_BASE(_ttbr0) \
	(_ttbr0 & (BIT_ULL(KGSL_IOMMU_ASID_START_BIT) - 1))

#define KGSL_MMU_DEVICE(_mmu) \
	container_of((_mmu), struct kgsl_device, mmu)

/**
 * enum kgsl_ft_pagefault_policy_bits - KGSL pagefault policy bits
 * @KGSL_FT_PAGEFAULT_INT_ENABLE: No longer used, but retained for compatibility
 * @KGSL_FT_PAGEFAULT_GPUHALT_ENABLE: enable GPU halt on pagefaults
 * @KGSL_FT_PAGEFAULT_LOG_ONE_PER_PAGE: log one pagefault per page
 * @KGSL_FT_PAGEFAULT_LOG_ONE_PER_INT: log one pagefault per interrupt
 */
enum {
	KGSL_FT_PAGEFAULT_INT_ENABLE = 0,
	KGSL_FT_PAGEFAULT_GPUHALT_ENABLE = 1,
	KGSL_FT_PAGEFAULT_LOG_ONE_PER_PAGE = 2,
	KGSL_FT_PAGEFAULT_LOG_ONE_PER_INT = 3,
	/* KGSL_FT_PAGEFAULT_MAX_BITS is used to calculate the mask */
	KGSL_FT_PAGEFAULT_MAX_BITS,
};

#define KGSL_FT_PAGEFAULT_MASK GENMASK(KGSL_FT_PAGEFAULT_MAX_BITS - 1, 0)

#define KGSL_FT_PAGEFAULT_DEFAULT_POLICY 0

struct kgsl_device;

enum kgsl_mmutype {
	KGSL_MMU_TYPE_IOMMU = 0,
	KGSL_MMU_TYPE_NONE
};

#define KGSL_IOMMU_SMMU_V500 1

struct kgsl_pagetable {
	spinlock_t lock;
	struct kref refcount;
	struct list_head list;
	unsigned int name;
	struct kobject *kobj;
	struct work_struct destroy_ws;

	struct {
		atomic_t entries;
		atomic_long_t mapped;
		atomic_long_t max_mapped;
	} stats;
	const struct kgsl_mmu_pt_ops *pt_ops;
	uint64_t fault_addr;
	struct kgsl_mmu *mmu;
	/** @rbtree: all buffers mapped into the pagetable, indexed by gpuaddr */
	struct rb_root rbtree;
	/** @va_start: Start of virtual range used in this pagetable */
	u64 va_start;
	/** @va_end: End of virtual range */
	u64 va_end;
	/**
	 * @svm_start: Start of shared virtual memory range. Addresses in this
	 * range are also valid in the process's CPU address space.
	 */
	u64 svm_start;
	/** @svm_end: end of 32 bit compatible range */
	u64 svm_end;
	/**
	 * @compat_va_start - Start of the "compat" virtual address range for
	 * forced 32 bit allocations
	 */
	u64 compat_va_start;
	/**
	 * @compat_va_end - End of the "compat" virtual address range for
	 * forced 32 bit allocations
	 */
	u64 compat_va_end;
	u64 global_base;
};

struct kgsl_mmu;

struct kgsl_mmu_ops {
	void (*mmu_close)(struct kgsl_mmu *mmu);
	int (*mmu_start)(struct kgsl_mmu *mmu);
	uint64_t (*mmu_get_current_ttbr0)(struct kgsl_mmu *mmu, struct kgsl_context *context);
	void (*mmu_pagefault_resume)(struct kgsl_mmu *mmu, bool terminate);
	void (*mmu_clear_fsr)(struct kgsl_mmu *mmu);
	void (*mmu_enable_clk)(struct kgsl_mmu *mmu);
	void (*mmu_disable_clk)(struct kgsl_mmu *mmu);
	int (*mmu_set_pf_policy)(struct kgsl_mmu *mmu, unsigned long pf_policy);
	int (*mmu_init_pt)(struct kgsl_mmu *mmu, struct kgsl_pagetable *pt);
	struct kgsl_pagetable * (*mmu_getpagetable)(struct kgsl_mmu *mmu,
			unsigned long name);
	void (*mmu_map_global)(struct kgsl_mmu *mmu,
		struct kgsl_memdesc *memdesc, u32 padding);
	void (*mmu_flush_tlb)(struct kgsl_mmu *mmu);
};

struct kgsl_mmu_pt_ops {
	int (*mmu_map)(struct kgsl_pagetable *pt,
			struct kgsl_memdesc *memdesc);
	int (*mmu_map_child)(struct kgsl_pagetable *pt,
		struct kgsl_memdesc *memdesc, u64 offset,
		struct kgsl_memdesc *child, u64 child_offset,
		u64 length);
	int (*mmu_map_zero_page_to_range)(struct kgsl_pagetable *pt,
		struct kgsl_memdesc *memdesc, u64 start, u64 length);
	int (*mmu_unmap)(struct kgsl_pagetable *pt,
			struct kgsl_memdesc *memdesc);
	int (*mmu_unmap_range)(struct kgsl_pagetable *pt,
			struct kgsl_memdesc *memdesc, u64 offset, u64 length);
	void (*mmu_destroy_pagetable)(struct kgsl_pagetable *pt);
	u64 (*get_ttbr0)(struct kgsl_pagetable *pt);
	int (*get_context_bank)(struct kgsl_pagetable *pt, struct kgsl_context *context);
	int (*get_asid)(struct kgsl_pagetable *pt, struct kgsl_context *context);
	int (*get_gpuaddr)(struct kgsl_pagetable *pt,
				struct kgsl_memdesc *memdesc);
	void (*put_gpuaddr)(struct kgsl_memdesc *memdesc);
	uint64_t (*find_svm_region)(struct kgsl_pagetable *pt, uint64_t start,
		uint64_t end, uint64_t size, uint64_t align);
	int (*set_svm_region)(struct kgsl_pagetable *pt,
				uint64_t gpuaddr, uint64_t size);
	int (*svm_range)(struct kgsl_pagetable *pt, uint64_t *lo, uint64_t *hi,
			uint64_t memflags);
	bool (*addr_in_range)(struct kgsl_pagetable *pagetable,
			uint64_t gpuaddr, uint64_t size);
};

enum kgsl_mmu_feature {
	/* @KGSL_MMU_64BIT: Use 64 bit virtual address space */
	KGSL_MMU_64BIT,
	/* @KGSL_MMU_PAGED: Support paged memory */
	KGSL_MMU_PAGED,
	/*
	 * @KGSL_MMU_NEED_GUARD_PAGE: Set if a guard page is needed for each
	 * mapped region
	 */
	KGSL_MMU_NEED_GUARD_PAGE,
	/** @KGSL_MMU_IO_COHERENT: Set if a device supports I/O coherency */
	KGSL_MMU_IO_COHERENT,
	/** @KGSL_MMU_LLC_ENABLE: Set if LLC is activated for the target */
	KGSL_MMU_LLCC_ENABLE,
	/** @KGSL_MMU_SMMU_APERTURE: Set the SMMU aperture */
	KGSL_MMU_SMMU_APERTURE,
	/**
	 * @KGSL_MMU_IOPGTABLE: Set if the qcom,adreno-smmu implementation is
	 * available. Implies split address space and per-process pagetables
	 */
	KGSL_MMU_IOPGTABLE,
	/** @KGSL_MMU_SUPPORT_VBO: Non-secure VBOs are supported */
	KGSL_MMU_SUPPORT_VBO,
	/** @KGSL_MMU_PAGEFAULT_TERMINATE: Set to make pagefaults fatal */
	KGSL_MMU_PAGEFAULT_TERMINATE,
};

#include "kgsl_iommu.h"

/**
 * struct kgsl_mmu - Master definition for KGSL MMU devices
 * @flags: MMU device flags
 * @type: Type of MMU that is attached
 * @subtype: Sub Type of MMU that is attached
 * @defaultpagetable: Default pagetable object for the MMU
 * @securepagetable: Default secure pagetable object for the MMU
 * @mmu_ops: Function pointers for the MMU sub-type
 * @secured: True if the MMU needs to be secured
 * @feature: Static list of MMU features
 */
struct kgsl_mmu {
	unsigned long flags;
	enum kgsl_mmutype type;
	u32 subtype;
	struct kgsl_pagetable *defaultpagetable;
	struct kgsl_pagetable *securepagetable;
	const struct kgsl_mmu_ops *mmu_ops;
	bool secured;
	unsigned long features;
	/** @pfpolicy: The current pagefault policy for the device */
	unsigned long pfpolicy;
	/** mmu: Pointer to the IOMMU sub-device */
	struct kgsl_iommu iommu;
};

#define KGSL_IOMMU(d) (&((d)->mmu.iommu))

int __init kgsl_mmu_init(void);
void kgsl_mmu_exit(void);

int kgsl_mmu_start(struct kgsl_device *device);

void kgsl_print_global_pt_entries(struct seq_file *s);
void kgsl_mmu_putpagetable(struct kgsl_pagetable *pagetable);

int kgsl_mmu_map(struct kgsl_pagetable *pagetable,
		 struct kgsl_memdesc *memdesc);
int kgsl_mmu_map_child(struct kgsl_pagetable *pt,
		struct kgsl_memdesc *memdesc, u64 offset,
		struct kgsl_memdesc *child, u64 child_offset,
		u64 length);
int kgsl_mmu_map_zero_page_to_range(struct kgsl_pagetable *pt,
		struct kgsl_memdesc *memdesc, u64 start, u64 length);
int kgsl_mmu_unmap(struct kgsl_pagetable *pagetable,
		    struct kgsl_memdesc *memdesc);
int kgsl_mmu_unmap_range(struct kgsl_pagetable *pt,
		struct kgsl_memdesc *memdesc, u64 offset, u64 length);
unsigned int kgsl_mmu_log_fault_addr(struct kgsl_mmu *mmu,
		u64 ttbr0, uint64_t addr);
bool kgsl_mmu_gpuaddr_in_range(struct kgsl_pagetable *pt, uint64_t gpuaddr,
		uint64_t size);

int kgsl_mmu_get_region(struct kgsl_pagetable *pagetable,
		uint64_t gpuaddr, uint64_t size);

int kgsl_mmu_find_region(struct kgsl_pagetable *pagetable,
		uint64_t region_start, uint64_t region_end,
		uint64_t *gpuaddr, uint64_t size, unsigned int align);

uint64_t kgsl_mmu_find_svm_region(struct kgsl_pagetable *pagetable,
		uint64_t start, uint64_t end, uint64_t size,
		uint64_t alignment);

int kgsl_mmu_set_svm_region(struct kgsl_pagetable *pagetable, uint64_t gpuaddr,
		uint64_t size);

void kgsl_mmu_detach_pagetable(struct kgsl_pagetable *pagetable);

int kgsl_mmu_svm_range(struct kgsl_pagetable *pagetable,
		uint64_t *lo, uint64_t *hi, uint64_t memflags);

struct kgsl_pagetable *kgsl_get_pagetable(unsigned long name);

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

/**
 * kgsl_mmu_get_gpuaddr - Assign a GPU address to the memdesc
 * @pagetable: GPU pagetable to assign the address in
 * @memdesc: mem descriptor to assign the memory to
 *
 * Return: 0 on success or negative on failure
 */
static inline int kgsl_mmu_get_gpuaddr(struct kgsl_pagetable *pagetable,
		 struct kgsl_memdesc *memdesc)
{
	if (PT_OP_VALID(pagetable, get_gpuaddr))
		return pagetable->pt_ops->get_gpuaddr(pagetable, memdesc);

	return -ENOMEM;
}

/**
 * kgsl_mmu_put_gpuaddr - Remove a GPU address from a pagetable
 * @pagetable: Pagetable to release the memory from
 * @memdesc: Memory descriptor containing the GPU address to free
 *
 * Release a GPU address in the MMU virtual address space.
 */
static inline void kgsl_mmu_put_gpuaddr(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc)
{
	if (PT_OP_VALID(pagetable, put_gpuaddr))
		pagetable->pt_ops->put_gpuaddr(memdesc);
}

static inline u64 kgsl_mmu_get_current_ttbr0(struct kgsl_mmu *mmu, struct kgsl_context *context)
{
	if (MMU_OP_VALID(mmu, mmu_get_current_ttbr0))
		return mmu->mmu_ops->mmu_get_current_ttbr0(mmu, context);

	return 0;
}

static inline struct kgsl_pagetable *kgsl_mmu_getpagetable(struct kgsl_mmu *mmu,
		unsigned long name)
{
	if (MMU_OP_VALID(mmu, mmu_getpagetable))
		return mmu->mmu_ops->mmu_getpagetable(mmu, name);

	return NULL;
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

static inline int kgsl_mmu_set_pagefault_policy(struct kgsl_mmu *mmu,
						unsigned long pf_policy)
{
	if (MMU_OP_VALID(mmu, mmu_set_pf_policy))
		return mmu->mmu_ops->mmu_set_pf_policy(mmu, pf_policy);

	return 0;
}

static inline void kgsl_mmu_pagefault_resume(struct kgsl_mmu *mmu,
	bool terminate)
{
	if (MMU_OP_VALID(mmu, mmu_pagefault_resume))
		return mmu->mmu_ops->mmu_pagefault_resume(mmu, terminate);
}

static inline void kgsl_mmu_clear_fsr(struct kgsl_mmu *mmu)
{
	if (MMU_OP_VALID(mmu, mmu_clear_fsr))
		return mmu->mmu_ops->mmu_clear_fsr(mmu);
}

static inline bool kgsl_mmu_is_perprocess(struct kgsl_mmu *mmu)
{
	return test_bit(KGSL_MMU_IOPGTABLE, &mmu->features);
}

static inline bool kgsl_mmu_is_secured(struct kgsl_mmu *mmu)
{
	return mmu && (mmu->secured) && (!IS_ERR_OR_NULL(mmu->securepagetable));
}

static inline u64
kgsl_mmu_pagetable_get_ttbr0(struct kgsl_pagetable *pagetable)
{
	if (PT_OP_VALID(pagetable, get_ttbr0))
		return pagetable->pt_ops->get_ttbr0(pagetable);

	return 0;
}

static inline void kgsl_mmu_flush_tlb(struct kgsl_mmu *mmu)
{
	if (!test_bit(KGSL_MMU_IOPGTABLE, &mmu->features))
		return;

	if (MMU_OP_VALID(mmu, mmu_flush_tlb))
		return mmu->mmu_ops->mmu_flush_tlb(mmu);
}

/**
 * kgsl_mmu_map_global - Map a memdesc as a global buffer
 * @device: A KGSL GPU device handle
 * @memdesc: Pointer to a GPU memory descriptor
 * @padding: Any padding to add to the end of the VA allotment (in bytes)
 *
 * Map a buffer as globally accessible in all pagetable contexts
 */
void kgsl_mmu_map_global(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc, u32 padding);

/**
 * kgsl_mmu_pagetable_get_context_bank - Return the context bank number
 * @pagetable: A handle to a given pagetable
 * @context : LPAC or User context
 *
 * This function will find the context number of the given pagetable

 * Return: The context bank number the pagetable is attached to or
 * negative error on failure.
 */
int kgsl_mmu_pagetable_get_context_bank(struct kgsl_pagetable *pagetable,
			struct kgsl_context *context);

/**
 * kgsl_mmu_pagetable_get_asid - Return the ASID number
 * @pagetable: A handle to a given pagetable
 * @context: LPAC or GC context
 *
 * This function will find the ASID number of the given pagetable

 * Return: The ASID number the pagetable is attached to or
 * negative error on failure.
 */
int kgsl_mmu_pagetable_get_asid(struct kgsl_pagetable *pagetable,
		struct kgsl_context *context);

void kgsl_mmu_pagetable_init(struct kgsl_mmu *mmu,
		struct kgsl_pagetable *pagetable, u32 name);

void kgsl_mmu_pagetable_add(struct kgsl_mmu *mmu, struct kgsl_pagetable *pagetable);

#if IS_ENABLED(CONFIG_ARM_SMMU)
int kgsl_iommu_bind(struct kgsl_device *device, struct platform_device *pdev);
#else
static inline int kgsl_iommu_bind(struct kgsl_device *device, struct platform_device *pdev)
{
	return -ENODEV;
}
#endif
#endif /* __KGSL_MMU_H */
