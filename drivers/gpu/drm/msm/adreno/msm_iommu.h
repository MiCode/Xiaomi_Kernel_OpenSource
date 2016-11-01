/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#ifndef __DRM_MSM_IOMMU_H
#define __DRM_MSM_IOMMU_H

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/dma-attrs.h>
#include <linux/uaccess.h>

/*
 * These defines control the address range for allocations that
 * are mapped into all pagetables.
 */
#define KGSL_IOMMU_GLOBAL_MEM_SIZE	SZ_8M
#define KGSL_IOMMU_GLOBAL_MEM_BASE	0xf8000000

#define KGSL_IOMMU_SECURE_SIZE SZ_256M
#define KGSL_IOMMU_SECURE_END KGSL_IOMMU_GLOBAL_MEM_BASE
#define KGSL_IOMMU_SECURE_BASE	\
	(KGSL_IOMMU_GLOBAL_MEM_BASE - KGSL_IOMMU_SECURE_SIZE)

#define KGSL_IOMMU_SVM_BASE32		0x300000
#define KGSL_IOMMU_SVM_END32		(0xC0000000 - SZ_16M)

#define KGSL_IOMMU_VA_BASE64		0x500000000ULL
#define KGSL_IOMMU_VA_END64		0x600000000ULL
/*
 * Note: currently we only support 36 bit addresses,
 * but the CPU supports 39. Eventually this range
 * should change to high part of the 39 bit address
 * space just like the CPU.
 */
#define KGSL_IOMMU_SVM_BASE64		0x700000000ULL
#define KGSL_IOMMU_SVM_END64		0x800000000ULL

/* TLBSTATUS register fields */
#define KGSL_IOMMU_CTX_TLBSTATUS_SACTIVE BIT(0)

/* IMPLDEF_MICRO_MMU_CTRL register fields */
#define KGSL_IOMMU_IMPLDEF_MICRO_MMU_CTRL_HALT  0x00000004
#define KGSL_IOMMU_IMPLDEF_MICRO_MMU_CTRL_IDLE  0x00000008

/* SCTLR fields */
#define KGSL_IOMMU_SCTLR_HUPCF_SHIFT		8
#define KGSL_IOMMU_SCTLR_CFCFG_SHIFT		7
#define KGSL_IOMMU_SCTLR_CFIE_SHIFT		6

enum kgsl_iommu_reg_map {
	KGSL_IOMMU_CTX_SCTLR = 0,
	KGSL_IOMMU_CTX_TTBR0,
	KGSL_IOMMU_CTX_CONTEXTIDR,
	KGSL_IOMMU_CTX_FSR,
	KGSL_IOMMU_CTX_FAR,
	KGSL_IOMMU_CTX_TLBIALL,
	KGSL_IOMMU_CTX_RESUME,
	KGSL_IOMMU_CTX_FSYNR0,
	KGSL_IOMMU_CTX_FSYNR1,
	KGSL_IOMMU_CTX_TLBSYNC,
	KGSL_IOMMU_CTX_TLBSTATUS,
	KGSL_IOMMU_REG_MAX
};

/* Max number of iommu clks per IOMMU unit */
#define KGSL_IOMMU_MAX_CLKS 5

enum kgsl_iommu_context_id {
	KGSL_IOMMU_CONTEXT_USER = 0,
	KGSL_IOMMU_CONTEXT_SECURE = 1,
	KGSL_IOMMU_CONTEXT_MAX,
};

/* offset at which a nop command is placed in setstate */
#define KGSL_IOMMU_SETSTATE_NOP_OFFSET	1024

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
/* The MMU supports non-contigious pages */
#define KGSL_MMU_PAGED BIT(8)
/* The device requires a guard page */
#define KGSL_MMU_NEED_GUARD_PAGE BIT(9)

/* offset at which a nop command is placed in setstate */
#define KGSL_IOMMU_SETSTATE_NOP_OFFSET	1024

/*
 * struct kgsl_iommu_context - Structure holding data about an iommu context
 * bank
 * @dev: pointer to the iommu context's device
 * @name: context name
 * @id: The id of the context, used for deciding how it is used.
 * @cb_num: The hardware context bank number, used for calculating register
 *		offsets.
 * @kgsldev: The kgsl device that uses this context.
 * @fault: Flag when set indicates that this iommu device has caused a page
 * fault
 * @gpu_offset: Offset of this context bank in the GPU register space
 * @default_pt: The default pagetable for this context,
 *		it may be changed by self programming.
 */
struct msm_iommu_context {
	struct device *dev;
	const char *name;
	enum kgsl_iommu_context_id id;
	unsigned int cb_num;
	int fault;
	void __iomem *regbase;
	unsigned int gpu_offset;
};

struct msm_iommu {
	struct msm_iommu_context ctx[KGSL_IOMMU_CONTEXT_MAX];
	void __iomem *regbase;
	unsigned long regstart;
	unsigned int regsize;
	atomic_t clk_enable_count;
	struct clk *clks[KGSL_IOMMU_MAX_CLKS];
	int clk_num;
	unsigned int micro_mmu_ctrl;
	unsigned int version;
	unsigned int protect_reg_base;
	unsigned int protect_reg_range;
	u32 pagefault_suppression_count;
};

/*
 * struct msm_iommu_pt - Iommu pagetable structure private to kgsl driver
 * @domain: Pointer to the iommu domain that contains the iommu pagetable
 * @ttbr0: register value to set when using this pagetable
 * @contextidr: register value to set when using this pagetable
 * @attached: is the pagetable attached?
 * @rbtree: all buffers mapped into the pagetable, indexed by gpuaddr
 * @va_start: Start of virtual range used in this pagetable.
 * @va_end: End of virtual range.
 * @svm_start: Start of shared virtual memory range. Addresses in this
 *		range are also valid in the process's CPU address space.
 * @svm_end: End of the shared virtual memory range.
 * @svm_start: 32 bit compatible range, for old clients who lack bits
 * @svm_end: end of 32 bit compatible range
 */
struct msm_iommu_pt {
	struct iommu_domain *domain;
	u64 ttbr0;
	u32 contextidr;
	bool attached;

	struct rb_root rbtree;

	uint64_t va_start;
	uint64_t va_end;
	uint64_t svm_start;
	uint64_t svm_end;
	uint64_t compat_va_start;
	uint64_t compat_va_end;
};

/*
 * offset of context bank 0 from the start of the SMMU register space.
 */
#define KGSL_IOMMU_CB0_OFFSET		0x8000
/* size of each context bank's register space */
#define KGSL_IOMMU_CB_SHIFT		12

/* Macros to read/write IOMMU registers */
extern const unsigned int msm_iommu_reg_list[KGSL_IOMMU_REG_MAX];

static inline void __iomem *
msm_iommu_reg(struct msm_iommu_context *ctx, enum kgsl_iommu_reg_map reg)
{
	BUG_ON(ctx->regbase == NULL);
	BUG_ON(reg >= KGSL_IOMMU_REG_MAX);
	return ctx->regbase + msm_iommu_reg_list[reg];
}

#define KGSL_IOMMU_SET_CTX_REG_Q(_ctx, REG, val) \
		writeq_relaxed((val), \
			msm_iommu_reg((_ctx), KGSL_IOMMU_CTX_##REG))

#define KGSL_IOMMU_GET_CTX_REG_Q(_ctx, REG) \
		readq_relaxed(msm_iommu_reg((_ctx), KGSL_IOMMU_CTX_##REG))

#define KGSL_IOMMU_SET_CTX_REG(_ctx, REG, val) \
		writel_relaxed((val), \
			msm_iommu_reg((_ctx), KGSL_IOMMU_CTX_##REG))

#define KGSL_IOMMU_GET_CTX_REG(_ctx, REG) \
		readl_relaxed(msm_iommu_reg((_ctx), KGSL_IOMMU_CTX_##REG))

/* Internal definitions for memdesc->priv */
#define KGSL_MEMDESC_GUARD_PAGE BIT(0)
/* Set if the memdesc is mapped into all pagetables */
#define KGSL_MEMDESC_GLOBAL BIT(1)
/* The memdesc is frozen during a snapshot */
#define KGSL_MEMDESC_FROZEN BIT(2)
/* The memdesc is mapped into a pagetable */
#define KGSL_MEMDESC_MAPPED BIT(3)
/* The memdesc is secured for content protection */
#define KGSL_MEMDESC_SECURE BIT(4)
/* Memory is accessible in privileged mode */
#define KGSL_MEMDESC_PRIVILEGED BIT(6)
/* The memdesc is TZ locked content protection */
#define KGSL_MEMDESC_TZ_LOCKED BIT(7)

struct msm_memdesc {
	void *hostptr;
	unsigned int hostptr_count;
	uint64_t gpuaddr;
	phys_addr_t physaddr;
	uint64_t size;
	unsigned int priv;
	struct sg_table *sgt;
	uint64_t flags;
	struct dma_attrs attrs;
	struct device *dev;
};

#endif /* __MSM_IOMMU_H */
