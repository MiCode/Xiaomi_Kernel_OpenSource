/* Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
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
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/genalloc.h>
#include <linux/slab.h>
#include <linux/iommu.h>
#include <linux/msm_kgsl.h>
#include <soc/qcom/secure_buffer.h>
#include <stddef.h>

#include "kgsl.h"
#include "kgsl_device.h"
#include "kgsl_mmu.h"
#include "kgsl_sharedmem.h"
#include "kgsl_iommu.h"
#include "adreno_pm4types.h"
#include "adreno.h"
#include "kgsl_trace.h"
#include "kgsl_cffdump.h"
#include "kgsl_pwrctrl.h"

static struct kgsl_mmu_pt_ops iommu_pt_ops;

static struct kgsl_iommu_register_list kgsl_iommuv1_reg[KGSL_IOMMU_REG_MAX] = {
	{ 0, 0 },			/* GLOBAL_BASE */
	{ 0x0, 1 },			/* SCTLR */
	{ 0x20, 1 },			/* TTBR0 */
	{ 0x28, 1 },			/* TTBR1 */
	{ 0x58, 1 },			/* FSR */
	{ 0x60, 1 },			/* FAR_0 */
	{ 0x618, 1 },			/* TLBIALL */
	{ 0x008, 1 },			/* RESUME */
	{ 0x68, 1 },			/* FSYNR0 */
	{ 0x6C, 1 },			/* FSYNR1 */
	{ 0x7F0, 1 },			/* TLBSYNC */
	{ 0x7F4, 1 },			/* TLBSTATUS */
	{ 0x2000, 0 }			/* IMPLDEF_MICRO_MMU_CRTL */
};

static struct kgsl_iommu_register_list kgsl_iommuv2_reg[KGSL_IOMMU_REG_MAX] = {
	{ 0, 0 },			/* GLOBAL_BASE */
	{ 0x0, 1 },			/* SCTLR */
	{ 0x20, 1 },			/* TTBR0 */
	{ 0x28, 1 },			/* TTBR1 */
	{ 0x58, 1 },			/* FSR */
	{ 0x60, 1 },			/* FAR_0 */
	{ 0x618, 1 },			/* TLBIALL */
	{ 0x008, 1 },			/* RESUME */
	{ 0x68, 1 },			/* FSYNR0 */
	{ 0x6C, 1 },			/* FSYNR1 */
	{ 0x7F0, 1 },			/* TLBSYNC */
	{ 0x7F4, 1 },			/* TLBSTATUS */
	{ 0x6000, 0 }			/* IMPLDEF_MICRO_MMU_CRTL */
};

/*
 * struct kgsl_iommu_addr_entry - entry in the kgsl_iommu_pt rbtree.
 * @base: starting virtual address of the entry
 * @size: size of the entry
 * @node: the rbtree node
 *
 */
struct kgsl_iommu_addr_entry {
	uint64_t base;
	uint64_t size;
	struct rb_node node;
};

static struct kmem_cache *addr_entry_cache;

static int kgsl_iommu_flush_pt(struct kgsl_mmu *mmu);
static phys_addr_t
kgsl_iommu_get_current_ptbase(struct kgsl_mmu *mmu);

/*
 * kgsl_iommu_get_pt_base_addr - Get the physical address of the pagetable
 * @mmu - Pointer to mmu
 * @pt - kgsl pagetable pointer that contains the IOMMU domain pointer
 *
 */
static phys_addr_t kgsl_iommu_get_pt_base_addr(struct kgsl_mmu *mmu,
						struct kgsl_pagetable *pt)
{
	struct kgsl_iommu_pt *iommu_pt;

	if (pt == NULL)
		return 0;

	iommu_pt = pt->priv;

	return iommu_pt->pt_base;
}

/*
 * One page allocation for a guard region to protect against over-zealous
 * GPU pre-fetch
 */

static struct page *kgsl_guard_page;
static struct kgsl_memdesc kgsl_secure_guard_page_memdesc;

/* These functions help find the nearest allocated memory entries on either side
 * of a faulting address. If we know the nearby allocations memory we can
 * get a better determination of what we think should have been located in the
 * faulting region
 */

/*
 * A local structure to make it easy to store the interesting bits for the
 * memory entries on either side of the faulting address
 */

struct _mem_entry {
	uint64_t gpuaddr;
	uint64_t size;
	uint64_t flags;
	unsigned int priv;
	int pending_free;
	pid_t pid;
};

static void _get_entries(struct kgsl_process_private *private,
		uint64_t faultaddr, struct _mem_entry *prev,
		struct _mem_entry *next)
{
	int id;
	struct kgsl_mem_entry *entry;

	uint64_t prevaddr = 0;
	struct kgsl_mem_entry *p = NULL;

	uint64_t nextaddr = (uint64_t) -1;
	struct kgsl_mem_entry *n = NULL;

	idr_for_each_entry(&private->mem_idr, entry, id) {
		uint64_t addr = entry->memdesc.gpuaddr;

		if ((addr < faultaddr) && (addr > prevaddr)) {
			prevaddr = addr;
			p = entry;
		}

		if ((addr > faultaddr) && (addr < nextaddr)) {
			nextaddr = addr;
			n = entry;
		}
	}

	if (p != NULL) {
		prev->gpuaddr = p->memdesc.gpuaddr;
		prev->size = p->memdesc.size;
		prev->flags = p->memdesc.flags;
		prev->priv = p->memdesc.priv;
		prev->pending_free = p->pending_free;
		prev->pid = private->pid;
	}

	if (n != NULL) {
		next->gpuaddr = n->memdesc.gpuaddr;
		next->size = n->memdesc.size;
		next->flags = n->memdesc.flags;
		next->priv = n->memdesc.priv;
		next->pending_free = n->pending_free;
		next->pid = private->pid;
	}
}

static void _find_mem_entries(struct kgsl_mmu *mmu, uint64_t faultaddr,
	phys_addr_t ptbase, struct _mem_entry *preventry,
	struct _mem_entry *nextentry)
{
	struct kgsl_process_private *private = NULL, *p;
	int id = kgsl_mmu_get_ptname_from_ptbase(mmu, ptbase);

	memset(preventry, 0, sizeof(*preventry));
	memset(nextentry, 0, sizeof(*nextentry));

	/* Set the maximum possible size as an initial value */
	nextentry->gpuaddr = (uint64_t) -1;

	mutex_lock(&kgsl_driver.process_mutex);
	list_for_each_entry(p, &kgsl_driver.process_list, list) {
		if (p->pagetable && (p->pagetable->name == id)) {
			if (kgsl_process_private_get(p))
				private = p;
			break;
		}
	}
	mutex_unlock(&kgsl_driver.process_mutex);

	if (private != NULL) {
		spin_lock(&private->mem_lock);
		_get_entries(private, faultaddr, preventry, nextentry);
		spin_unlock(&private->mem_lock);

		kgsl_process_private_put(private);
	}
}

static void _print_entry(struct kgsl_device *device, struct _mem_entry *entry)
{
	char name[32];
	memset(name, 0, sizeof(name));

	kgsl_get_memory_usage(name, sizeof(name) - 1, entry->flags);

	KGSL_LOG_DUMP(device,
		"[%016llX - %016llX] %s %s (pid = %d) (%s)\n",
		entry->gpuaddr,
		entry->gpuaddr + entry->size,
		entry->priv & KGSL_MEMDESC_GUARD_PAGE ? "(+guard)" : "",
		entry->pending_free ? "(pending free)" : "",
		entry->pid, name);
}

static void _check_if_freed(struct kgsl_iommu_context *ctx,
	uint64_t addr, pid_t pid)
{
	uint64_t gpuaddr = addr;
	uint64_t size = 0;
	uint64_t flags = 0;

	char name[32];
	memset(name, 0, sizeof(name));

	if (kgsl_memfree_find_entry(pid, &gpuaddr, &size, &flags)) {
		kgsl_get_memory_usage(name, sizeof(name) - 1, flags);
		KGSL_LOG_DUMP(ctx->kgsldev, "---- premature free ----\n");
		KGSL_LOG_DUMP(ctx->kgsldev,
			"[%8.8llX-%8.8llX] (%s) was already freed by pid %d\n",
			gpuaddr, gpuaddr + size, name, pid);
	}
}

static int kgsl_iommu_fault_handler(struct iommu_domain *domain,
	struct device *dev, unsigned long addr, int flags, void *token)
{
	int ret = 0;
	struct kgsl_pagetable *default_pt = token;
	struct kgsl_mmu *mmu = default_pt->mmu;
	struct kgsl_iommu *iommu;
	struct kgsl_iommu_context *ctx;
	phys_addr_t ptbase;
	pid_t pid;
	struct _mem_entry prev, next;
	int write;
	struct kgsl_device *device;
	struct adreno_device *adreno_dev;
	unsigned int no_page_fault_log = 0;
	unsigned int curr_context_id = 0;
	struct kgsl_context *context;
	char *fault_type = "unknown";

	if (mmu == NULL || mmu->priv == NULL)
		return ret;

	iommu = mmu->priv;
	ctx = &iommu->ctx[KGSL_IOMMU_CONTEXT_USER];
	device = mmu->device;
	adreno_dev = ADRENO_DEVICE(device);

	/*
	 * set the fault bits and stuff before any printks so that if fault
	 * handler runs then it will know it's dealing with a pagefault.
	 * Read the global current timestamp because we could be in middle of
	 * RB switch and hence the cur RB may not be reliable but global
	 * one will always be reliable
	 */
	kgsl_sharedmem_readl(&device->memstore, &curr_context_id,
		KGSL_MEMSTORE_OFFSET(KGSL_MEMSTORE_GLOBAL, current_context));

	context = kgsl_context_get(device, curr_context_id);

	if (context != NULL) {
		/* save pagefault timestamp for GFT */
		set_bit(KGSL_CONTEXT_PRIV_PAGEFAULT, &context->priv);

		kgsl_context_put(context);
		context = NULL;
	}

	ctx->fault = 1;

	if (test_bit(KGSL_FT_PAGEFAULT_GPUHALT_ENABLE,
		&adreno_dev->ft_pf_policy) &&
		(flags & IOMMU_FAULT_TRANSACTION_STALLED)) {
		/*
		 * Turn off GPU IRQ so we don't get faults from it too.
		 * The device mutex must be held to change power state
		 */
		mutex_lock(&device->mutex);
		kgsl_pwrctrl_change_state(device, KGSL_STATE_AWARE);
		mutex_unlock(&device->mutex);
	}

	write = (flags & IOMMU_FAULT_WRITE) ? 1 : 0;
	if (flags & IOMMU_FAULT_TRANSLATION)
		fault_type = "translation";
	else if (flags & IOMMU_FAULT_PERMISSION)
		fault_type = "permission";

	ptbase = KGSL_IOMMU_GET_CTX_REG_Q(iommu, ctx->ctx_id, TTBR0)
			& KGSL_IOMMU_CTX_TTBR0_ADDR_MASK;

	pid = kgsl_mmu_get_ptname_from_ptbase(mmu, ptbase);

	if (test_bit(KGSL_FT_PAGEFAULT_LOG_ONE_PER_PAGE,
		&adreno_dev->ft_pf_policy))
		no_page_fault_log = kgsl_mmu_log_fault_addr(mmu, ptbase, addr);

	if (!no_page_fault_log) {
		KGSL_MEM_CRIT(ctx->kgsldev,
			"GPU PAGE FAULT: addr = %lX pid = %d\n", addr, pid);
		KGSL_MEM_CRIT(ctx->kgsldev,
			"context = %d TTBR0 = %pa (%s %s fault)\n",
			ctx->ctx_id, &ptbase,
			write ? "write" : "read", fault_type);

		_check_if_freed(ctx, addr, pid);

		KGSL_LOG_DUMP(ctx->kgsldev, "---- nearby memory ----\n");

		_find_mem_entries(mmu, addr, ptbase, &prev, &next);

		if (prev.gpuaddr)
			_print_entry(ctx->kgsldev, &prev);
		else
			KGSL_LOG_DUMP(ctx->kgsldev, "*EMPTY*\n");

		KGSL_LOG_DUMP(ctx->kgsldev, " <- fault @ %8.8lX\n", addr);

		if (next.gpuaddr != (uint64_t) -1)
			_print_entry(ctx->kgsldev, &next);
		else
			KGSL_LOG_DUMP(ctx->kgsldev, "*EMPTY*\n");

	}

	trace_kgsl_mmu_pagefault(ctx->kgsldev, addr,
			kgsl_mmu_get_ptname_from_ptbase(mmu, ptbase),
			write ? "write" : "read");

	/*
	 * We do not want the h/w to resume fetching data from an iommu
	 * that has faulted, this is better for debugging as it will stall
	 * the GPU and trigger a snapshot. Return EBUSY error.
	 */
	if (test_bit(KGSL_FT_PAGEFAULT_GPUHALT_ENABLE,
		&adreno_dev->ft_pf_policy) &&
		(flags & IOMMU_FAULT_TRANSACTION_STALLED)) {
		uint32_t sctlr_val;
		ret = -EBUSY;
		/* Disable context fault interrupts */
		sctlr_val = KGSL_IOMMU_GET_CTX_REG(iommu, ctx->ctx_id, SCTLR);
		sctlr_val &= ~(0x1 << KGSL_IOMMU_SCTLR_CFIE_SHIFT);
		KGSL_IOMMU_SET_CTX_REG(iommu, ctx->ctx_id, SCTLR, sctlr_val);

		adreno_set_gpu_fault(adreno_dev, ADRENO_IOMMU_PAGE_FAULT);
		/* Go ahead with recovery*/
		adreno_dispatcher_schedule(device);
	}

	return ret;
}

/*
 * kgsl_iommu_disable_clk() - Disable iommu clocks
 * Disable IOMMU clocks
 */
static void kgsl_iommu_disable_clk(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = mmu->priv;
	int j;

	atomic_dec(&iommu->clk_enable_count);
	BUG_ON(atomic_read(&iommu->clk_enable_count) < 0);

	for (j = (KGSL_IOMMU_MAX_CLKS - 1); j >= 0; j--)
		if (iommu->clks[j])
			clk_disable_unprepare(iommu->clks[j]);
}

/*
 * kgsl_iommu_enable_clk_prepare_enable - Enable the specified IOMMU clock
 * Try 4 times to enable it and then BUG() for debug
 */
static void kgsl_iommu_clk_prepare_enable(struct clk *clk)
{
	int num_retries = 4;

	while (num_retries--) {
		if (!clk_prepare_enable(clk))
			return;
	}

	/* Failure is fatal so BUG() to facilitate debug */
	KGSL_CORE_ERR("IOMMU clock enable failed\n");
	BUG();
}

/*
 * kgsl_iommu_enable_clk - Enable iommu clocks
 * Enable all the IOMMU clocks
 */
static void kgsl_iommu_enable_clk(struct kgsl_mmu *mmu)
{
	int j;
	struct kgsl_iommu *iommu = mmu->priv;

	for (j = 0; j < KGSL_IOMMU_MAX_CLKS; j++) {
		if (iommu->clks[j])
			kgsl_iommu_clk_prepare_enable(iommu->clks[j]);
	}
	atomic_inc(&iommu->clk_enable_count);
}

/*
 * kgsl_iommu_pt_equal - Check if pagetables are equal
 * @mmu - Pointer to mmu structure
 * @pt - Pointer to pagetable
 * @pt_base - Address of a pagetable that the IOMMU register is
 * programmed with
 *
 * Checks whether the pt_base is equal to the base address of
 * the pagetable which is contained in the pt structure
 * Return - Non-zero if the pagetable addresses are equal else 0
 */
static int kgsl_iommu_pt_equal(struct kgsl_mmu *mmu,
				struct kgsl_pagetable *pt,
				phys_addr_t pt_base)
{
	struct kgsl_iommu_pt *iommu_pt = pt ? pt->priv : NULL;
	phys_addr_t domain_ptbase;

	if (iommu_pt == NULL)
		return 0;

	domain_ptbase = kgsl_iommu_get_pt_base_addr(mmu, pt);

	return (domain_ptbase == pt_base);

}

/*
 * kgsl_iommu_get_ptbase - Get pagetable base address
 * @pt - Pointer to pagetable
 *
 * Returns the physical address of the pagetable base.
 *
 */
static phys_addr_t kgsl_iommu_get_ptbase(struct kgsl_pagetable *pt)
{
	struct kgsl_iommu_pt *iommu_pt = pt ? pt->priv : NULL;

	if (iommu_pt == NULL)
		return 0;

	return kgsl_iommu_get_pt_base_addr(pt->mmu, pt);
}

/*
 * kgsl_iommu_destroy_pagetable - Free up reaources help by a pagetable
 * @mmu_specific_pt - Pointer to pagetable which is to be freed
 *
 * Return - void
 */
static void kgsl_iommu_destroy_pagetable(struct kgsl_pagetable *pt)
{
	struct kgsl_iommu_pt *iommu_pt = pt->priv;
	struct kgsl_mmu *mmu = pt->mmu;

	BUG_ON(!list_empty(&pt->list));

	if (iommu_pt->domain) {
		phys_addr_t domain_ptbase =
					kgsl_iommu_get_pt_base_addr(mmu, pt);
		trace_kgsl_pagetable_destroy(domain_ptbase, pt->name);

		iommu_domain_free(iommu_pt->domain);
	}

	kfree(iommu_pt);
	iommu_pt = NULL;
}

/* currently only the MSM_IOMMU driver supports secure iommu */
#ifdef CONFIG_MSM_IOMMU
static inline struct bus_type *
get_secure_bus(void)
{
	return &msm_iommu_sec_bus_type;
}
#else
static inline struct bus_type *
get_secure_bus(void)
{
	return NULL;
}
#endif

/* kgsl_iommu_init_pt - Set up an IOMMU pagetable */
static int kgsl_iommu_init_pt(struct kgsl_mmu *mmu, struct kgsl_pagetable *pt)
{
	int ret = 0;
	struct kgsl_iommu_pt *iommu_pt = NULL;
	struct bus_type *bus = &platform_bus_type;
	int disable_htw = 1;

	if (pt == NULL)
		return -EINVAL;

	if (KGSL_MMU_SECURE_PT == pt->name) {
		if (!mmu->secured)
			return -EPERM;

		if (!MMU_FEATURE(mmu, KGSL_MMU_HYP_SECURE_ALLOC)) {
			bus = get_secure_bus();
			if (bus == NULL)
				return -EPERM;
		}
	}

	iommu_pt = kzalloc(sizeof(struct kgsl_iommu_pt), GFP_KERNEL);
	if (!iommu_pt)
		return -ENOMEM;

	iommu_pt->domain = iommu_domain_alloc(bus);
	if (iommu_pt->domain == NULL) {
		kfree(iommu_pt);
		return -ENOMEM;
	}

	/* Disable coherent HTW, it is not supported by SMMU driver */
	iommu_domain_set_attr(iommu_pt->domain,
			DOMAIN_ATTR_COHERENT_HTW_DISABLE, &disable_htw);

	/* Provide the secure vmid domain attribute to the SMMU driver */
	if (pt->name == KGSL_MMU_SECURE_PT) {
		int secure_vmid = VMID_CP_PIXEL;

		ret = iommu_domain_set_attr(iommu_pt->domain,
			DOMAIN_ATTR_SECURE_VMID, &secure_vmid);
		if (ret)
			goto err;
	}

	pt->pt_ops = &iommu_pt_ops;
	pt->priv = iommu_pt;

	iommu_pt->rbtree = RB_ROOT;

	if (mmu->secured) {
		if (pt->name == KGSL_MMU_SECURE_PT) {
			iommu_pt->va_start = KGSL_IOMMU_SECURE_MEM_BASE;
			iommu_pt->va_end =
				iommu_pt->va_start + KGSL_IOMMU_SECURE_MEM_SIZE;
		} else {
			iommu_pt->va_start = KGSL_IOMMU_SVM_START;
			iommu_pt->va_end = KGSL_IOMMU_SECURE_MEM_BASE;
		}
	} else {
		iommu_pt->va_start = KGSL_IOMMU_SVM_START;
		iommu_pt->va_end = KGSL_MMU_GLOBAL_MEM_BASE;
	}

	if (pt->name != KGSL_MMU_GLOBAL_PT && pt->name != KGSL_MMU_SECURE_PT) {
		iommu_pt->svm_start = KGSL_IOMMU_SVM_START;
		iommu_pt->svm_end = KGSL_IOMMU_SVM_END;
	}

	if (KGSL_MMU_GLOBAL_PT == pt->name)
		iommu_set_fault_handler(iommu_pt->domain,
				kgsl_iommu_fault_handler, pt);
	else if (KGSL_MMU_SECURE_PT != pt->name)
		ret = iommu_domain_get_attr(iommu_pt->domain,
				DOMAIN_ATTR_PT_BASE_ADDR,
				&iommu_pt->pt_base);
	else
		iommu_pt->pt_base = 0;
err:
	if (ret) {
		if (iommu_pt->domain != NULL)
			iommu_domain_free(iommu_pt->domain);
		kfree(iommu_pt);
	}

	return ret;
}

/*
 * kgsl_detach_pagetable_iommu_domain - Detach the IOMMU from a
 * pagetable
 * @mmu - Pointer to the device mmu structure
 * @priv - Flag indicating whether the private or user context is to be
 * detached
 *
 * Detach the IOMMU with the domain that is contained in the
 * hwpagetable of the given mmu. After detaching the IOMMU is not
 * in use because the PTBR will not be set after a detach
 * Return - void
 */
static void kgsl_detach_pagetable_iommu_domain(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu_pt *iommu_pt;
	struct kgsl_iommu *iommu = mmu->priv;
	struct kgsl_iommu_context *ctx;
	int i;

	iommu_pt = mmu->defaultpagetable->priv;
	for (i = 0; i < KGSL_IOMMU_CONTEXT_MAX; i++) {
		ctx = &iommu->ctx[i];
		if (ctx->dev == NULL)
			continue;
		if (mmu->securepagetable && (KGSL_IOMMU_CONTEXT_SECURE == i))
			iommu_pt = mmu->securepagetable->priv;
		if (ctx->attached) {
			iommu_detach_device(iommu_pt->domain, ctx->dev);
			ctx->attached = false;
			KGSL_MEM_INFO(mmu->device,
				"iommu %p detached from user dev of MMU: %p\n",
				iommu_pt->domain, mmu);
		}
	}
}

/*
 * _iommu_get_clks - get iommu clocks
 * @mmu - Pointer to the device mmu structure
 */
void _iommu_get_clks(struct kgsl_mmu *mmu)
{
	struct kgsl_device_platform_data *pdata =
		dev_get_platdata(&mmu->device->pdev->dev);
	struct kgsl_iommu *iommu = mmu->priv;
	struct kgsl_device_iommu_data *data = pdata->iommu_data;
#ifdef CONFIG_MSM_IOMMU
	struct kgsl_iommu_context  *ctx = &iommu->ctx[KGSL_IOMMU_CONTEXT_USER];
	struct msm_iommu_drvdata *drvdata = 0;
#endif
	int i;

	/* Init IOMMU clks here */
#ifdef CONFIG_MSM_IOMMU
	if (MMU_FEATURE(mmu, KGSL_MMU_DMA_API)) {
		for (i = 0; i < KGSL_IOMMU_MAX_CLKS; i++)
			iommu->clks[i] = data->clks[i];
	} else {
		drvdata = dev_get_drvdata(ctx->dev->parent);
		iommu->clks[0] = drvdata->pclk;
		iommu->clks[1] = drvdata->clk;
		iommu->clks[2] = drvdata->aclk;
		iommu->clks[3] = iommu->gtcu_iface_clk;
	}
#else
	for (i = 0; i < KGSL_IOMMU_MAX_CLKS; i++)
		iommu->clks[i] = data->clks[i];
#endif
}

/*
 * kgsl_attach_pagetable_iommu_domain - Attach the IOMMU to a
 * pagetable, i.e set the IOMMU's PTBR to the pagetable address and
 * setup other IOMMU registers for the device so that it becomes
 * active
 * @mmu - Pointer to the device mmu structure
 * @priv - Flag indicating whether the private or user context is to be
 * attached
 *
 * Attach the IOMMU with the domain that is contained in the
 * hwpagetable of the given mmu.
 * Return - 0 on success else error code
 */
static int kgsl_attach_pagetable_iommu_domain(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu_pt *iommu_pt;
	struct kgsl_iommu *iommu = mmu->priv;
	struct kgsl_iommu_context *ctx;
	int i, ret = 0;

	/*
	 * If retention is supported, iommu hw retains the configuration
	 * on power collapse. If retention is supported we need to attach
	 * only once.
	 */
	if (MMU_FEATURE(mmu, KGSL_MMU_RETENTION) &&
		iommu->ctx[KGSL_IOMMU_CONTEXT_USER].attached)
		return 0;

	/* Loop through all the iommu devices and attach the domain */
	iommu_pt = mmu->defaultpagetable->priv;
	for (i = 0; i < KGSL_IOMMU_CONTEXT_MAX; i++) {

		ctx = &iommu->ctx[i];
		if (ctx->dev == NULL || ctx->attached)
			continue;

		if (KGSL_IOMMU_CONTEXT_SECURE == i) {
			if (mmu->securepagetable)
				iommu_pt = mmu->securepagetable->priv;
			else
				continue;
		}

		ret = iommu_attach_device(iommu_pt->domain, ctx->dev);
		if (ret) {
			KGSL_MEM_ERR(mmu->device,
					"Failed to attach device, err %d\n",
					ret);
			goto done;
		}
		ctx->attached = true;
		KGSL_MEM_INFO(mmu->device,
				"iommu pt %p attached to dev %p, ctx_id %d\n",
				iommu_pt->domain, ctx->dev, ctx->ctx_id);
		if (KGSL_IOMMU_CONTEXT_SECURE != i) {
			ret = iommu_domain_get_attr(iommu_pt->domain,
					DOMAIN_ATTR_PT_BASE_ADDR,
					&iommu_pt->pt_base);
			if (ret) {
				KGSL_CORE_ERR(
				  "pt_base query failed, using global pt\n");
				mmu->features |= KGSL_MMU_GLOBAL_PAGETABLE;
				ret = 0;
			}
		}
	}

	_iommu_get_clks(mmu);

done:
	return ret;
}

/*
 * _get_iommu_ctxs - Get device pointer to IOMMU contexts
 * @mmu - Pointer to mmu device
 *
 * Return - 0 on success else error code
 */
static int _get_iommu_ctxs(struct kgsl_mmu *mmu)
{
	struct kgsl_device_platform_data *pdata =
		dev_get_platdata(&mmu->device->pdev->dev);
	struct kgsl_iommu *iommu = mmu->priv;
	struct kgsl_device_iommu_data *data = pdata->iommu_data;
	struct kgsl_iommu_context *ctx;
	int i;
	int ret = 0;

	for (i = 0; i < data->iommu_ctx_count; i++) {
		if (!strcmp("gfx3d_user", data->iommu_ctxs[i].iommu_ctx_name)) {
			ctx = &iommu->ctx[KGSL_IOMMU_CONTEXT_USER];
		} else if (!strcmp("gfx3d_secure",
				data->iommu_ctxs[i].iommu_ctx_name)) {
			ctx = &iommu->ctx[KGSL_IOMMU_CONTEXT_SECURE];
			mmu->secured = true;
		} else if (!strcmp("gfx3d_spare",
				data->iommu_ctxs[i].iommu_ctx_name)) {
			continue;
		} else if (!strcmp("gfx3d_priv",
				data->iommu_ctxs[i].iommu_ctx_name)) {
			continue;
		} else {
			KGSL_CORE_ERR("dt: IOMMU context %s is invalid\n",
				data->iommu_ctxs[i].iommu_ctx_name);
			return -EINVAL;
		}

		/* Add ctx name here */
		ctx->name = data->iommu_ctxs[i].iommu_ctx_name;

		/* Add device ptr here */
#ifdef CONFIG_MSM_IOMMU
		if (data->iommu_ctxs[i].dev)
			ctx->dev = data->iommu_ctxs[i].dev;
		else
			ctx->dev = msm_iommu_get_ctx(ctx->name);
#else
		if (data->iommu_ctxs[i].dev)
                        ctx->dev = data->iommu_ctxs[i].dev;
#endif

		/* Add ctx_id here */
		ctx->ctx_id = data->iommu_ctxs[i].ctx_id;

		ctx->kgsldev = mmu->device;

		if ((!ctx->dev) || IS_ERR(ctx->dev)) {
			ret = (!ctx->dev) ? -EINVAL : PTR_ERR(ctx->dev);
			memset(ctx, 0, sizeof(*ctx));
			KGSL_CORE_ERR(
			   "Failed to initialize iommu contexts, err: %d\n",
			   ret);
		}
	}

	return ret;
}

/*
 * _iommu_set_register_map - Map the IOMMU registers
 */
static int _iommu_set_register_map(struct kgsl_mmu *mmu)
{
	struct kgsl_device_platform_data *pdata =
		dev_get_platdata(&mmu->device->pdev->dev);
	struct kgsl_iommu *iommu = mmu->device->mmu.priv;
	struct kgsl_device_iommu_data *data = pdata->iommu_data;

	/* set iommu features */
	mmu->features = data->features;

	mmu->secure_align_mask = data->secure_align_mask;

	/* set up the IOMMU register map */
	if (!data->regstart || !data->regsize) {
		KGSL_CORE_ERR("The register range for IOMMU not specified\n");
		return -EINVAL;
	}

	iommu->regbase = ioremap(data->regstart, data->regsize);
	if (iommu->regbase == NULL) {
		KGSL_CORE_ERR("Could not map IOMMU registers 0x%X:0x%x\n",
			data->regstart, data->regsize);
		return -ENOMEM;
	}

	iommu->ahb_base_offset = data->regstart - mmu->device->reg_phys;

	return 0;
}

/*
 * kgsl_iommu_get_default_ttbr0 - Return the ttbr0 value programmed by
 * iommu driver
 * @mmu - Pointer to mmu structure
 * @hostptr - Pointer to the IOMMU register map. This is used to match
 * the iommu device whose lsb value is to be returned
 * @ctx_id - The context bank whose lsb valus is to be returned
 * Return - returns the ttbr0 value programmed by iommu driver
 */
static uint64_t kgsl_iommu_get_default_ttbr0(struct kgsl_mmu *mmu,
				enum kgsl_iommu_context_id ctx_id)
{
	struct kgsl_iommu *iommu = mmu->priv;

	if (iommu->ctx[ctx_id].dev)
		return iommu->ctx[ctx_id].default_ttbr0;

	return 0;
}

/*
 * kgsl_iommu_get_reg_ahbaddr - Returns the ahb address of the register
 * @mmu - Pointer to mmu structure
 * @id - The context ID of the IOMMU ctx
 * @reg - The register for which address is required
 *
 * Return - The address of register which can be used in type0 packet
 */
static unsigned int kgsl_iommu_get_reg_ahbaddr(struct kgsl_mmu *mmu,
		enum kgsl_iommu_context_id id, enum kgsl_iommu_reg_map reg)
{
	struct kgsl_iommu *iommu = mmu->priv;
	int ctx_id = iommu->ctx[id].ctx_id;

	if (iommu->iommu_reg_list[reg].ctx_reg)
		return iommu->ahb_base_offset +
			iommu->iommu_reg_list[reg].reg_offset +
			(ctx_id << KGSL_IOMMU_CTX_SHIFT) +
			iommu->ctx_ahb_offset;
	else
		return iommu->ahb_base_offset +
			iommu->iommu_reg_list[reg].reg_offset;
}

static int kgsl_iommu_init(struct kgsl_mmu *mmu)
{
	/*
	 * intialize device mmu
	 *
	 * call this with the global lock held
	 */
	struct adreno_device *adreno_dev = ADRENO_DEVICE(mmu->device);
	int status = 0;
	struct kgsl_iommu *iommu;
	struct platform_device *pdev = mmu->device->pdev;

	iommu = kzalloc(sizeof(struct kgsl_iommu), GFP_KERNEL);
	if (!iommu)
		return -ENOMEM;

	mmu->priv = iommu;
	status = _get_iommu_ctxs(mmu);
	if (status)
		goto done;
	status = _iommu_set_register_map(mmu);
	if (status)
		goto done;

	if (of_property_match_string(pdev->dev.of_node, "clock-names",
						"gtcu_iface_clk") >= 0)
		iommu->gtcu_iface_clk = clk_get(&pdev->dev, "gtcu_iface_clk");

	if (kgsl_msm_supports_iommu_v2()) {
		if (adreno_is_a530(adreno_dev)) {
			iommu->iommu_reg_list = kgsl_iommuv2_reg;
			iommu->ctx_offset = KGSL_IOMMU_CTX_OFFSET_V2_A530;
			iommu->ctx_ahb_offset = KGSL_IOMMU_CTX_OFFSET_V2_A530;
		} else {
			iommu->iommu_reg_list = kgsl_iommuv1_reg;
			iommu->ctx_ahb_offset = KGSL_IOMMU_CTX_AHB_OFFSET_V2;
			if (adreno_is_a405v2(adreno_dev))
				iommu->ctx_offset =
					KGSL_IOMMU_CTX_OFFSET_A405V2;
			else
				iommu->ctx_offset = KGSL_IOMMU_CTX_OFFSET_V2;
		}
	}  else {
		iommu->iommu_reg_list = kgsl_iommuv1_reg;
		iommu->ctx_offset = KGSL_IOMMU_CTX_OFFSET_V1;
		iommu->ctx_ahb_offset = KGSL_IOMMU_CTX_OFFSET_V1;
	}

	if (addr_entry_cache == NULL) {
		addr_entry_cache = KMEM_CACHE(kgsl_iommu_addr_entry, 0);
		if (addr_entry_cache == NULL) {
			status = -ENOMEM;
			goto done;
		}
	}

	if (kgsl_guard_page == NULL) {
		kgsl_guard_page = alloc_page(GFP_KERNEL | __GFP_ZERO |
				__GFP_HIGHMEM);
		if (kgsl_guard_page == NULL) {
			status = -ENOMEM;
			goto done;
		}
	}

done:
	if (status) {
		kfree(iommu);
		mmu->priv = NULL;
	}
	return status;
}

/*
 * kgsl_iommu_setup_defaultpagetable - Setup the initial defualtpagetable
 * for iommu. This function is only called once during first start, successive
 * start do not call this funciton.
 * @mmu - Pointer to mmu structure
 *
 * Create the  initial defaultpagetable and setup the iommu mappings to it
 * Return - 0 on success else error code
 */
static int kgsl_iommu_setup_defaultpagetable(struct kgsl_mmu *mmu)
{
	int status = 0;

	mmu->defaultpagetable = kgsl_mmu_getpagetable(mmu, KGSL_MMU_GLOBAL_PT);
	/* Return error if the default pagetable doesn't exist */
	if (mmu->defaultpagetable == NULL) {
		status = -ENOMEM;
		goto err;
	}

	if (mmu->secured) {
		mmu->securepagetable = kgsl_mmu_getpagetable(mmu,
				KGSL_MMU_SECURE_PT);
		/* Return error if the secure pagetable doesn't exist */
		if (mmu->securepagetable == NULL) {
			KGSL_DRV_ERR(mmu->device,
			"Unable to create secure pagetable, disable content protection\n");
			status = -ENOMEM;
			goto err;
		}
	}
	return status;
err:
	if (mmu->defaultpagetable) {
		kgsl_mmu_putpagetable(mmu->defaultpagetable);
		mmu->defaultpagetable = NULL;
	}
	return status;
}

static int kgsl_iommu_start(struct kgsl_mmu *mmu)
{
	int status;
	struct kgsl_iommu *iommu = mmu->priv;
	struct kgsl_iommu_context *ctx;
	int i;
	int sctlr_val = 0;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(mmu->device);

	if (mmu->defaultpagetable == NULL) {
		status = kgsl_iommu_setup_defaultpagetable(mmu);
		if (status)
			return -ENOMEM;
	}

	status = kgsl_attach_pagetable_iommu_domain(mmu);
	if (status)
		goto done;

	kgsl_map_global_pt_entries(mmu->defaultpagetable);

	kgsl_iommu_enable_clk(mmu);
	KGSL_IOMMU_SET_CTX_REG(iommu, 0, TLBIALL, 1);

	/* Get the lsb value of pagetables set in the IOMMU ttbr0 register as
	 * that value should not change when we change pagetables, so while
	 * changing pagetables we can use this lsb value of the pagetable w/o
	 * having to read it again
	 */
	for (i = 0; i < KGSL_IOMMU_CONTEXT_MAX; i++) {
		ctx = &iommu->ctx[i];

		if (ctx->dev == NULL)
			continue;

		/*
		 *  1) HLOS cannot program secure context bank.
		 *  2) If context bank is not attached skip.
		 */
		if (!ctx->attached || (KGSL_IOMMU_CONTEXT_SECURE == i))
			continue;

		/*
		 * For IOMMU V1, if pagefault policy is GPUHALT_ENABLE,
		 * 1) Program CFCFG to 1 to enable STALL mode
		 * 2) Program HUPCF to 0 (Stall or terminate subsequent
		 *    transactions in the presence of an outstanding fault)
		 * else
		 * 1) Program CFCFG to 0 to disable STALL mode (0=Terminate)
		 * 2) Program HUPCF to 1 (Process subsequent transactions
		 *    independently of any outstanding fault)
		 */

		sctlr_val = KGSL_IOMMU_GET_CTX_REG(iommu, ctx->ctx_id, SCTLR);
		if (test_bit(KGSL_FT_PAGEFAULT_GPUHALT_ENABLE,
					&adreno_dev->ft_pf_policy)) {
			sctlr_val |= (0x1 << KGSL_IOMMU_SCTLR_CFCFG_SHIFT);
			sctlr_val &= ~(0x1 << KGSL_IOMMU_SCTLR_HUPCF_SHIFT);
		} else {
			sctlr_val &= ~(0x1 << KGSL_IOMMU_SCTLR_CFCFG_SHIFT);
			sctlr_val |= (0x1 << KGSL_IOMMU_SCTLR_HUPCF_SHIFT);
		}
		KGSL_IOMMU_SET_CTX_REG(iommu, ctx->ctx_id, SCTLR, sctlr_val);
		ctx->default_ttbr0 = KGSL_IOMMU_GET_CTX_REG_Q(iommu,
				ctx->ctx_id, TTBR0);
	}

	kgsl_iommu_disable_clk(mmu);

done:
	return status;
}

/*
 * kgsl_iommu_flush_tlb_pt_current - Flush IOMMU TLB if pagetable is
 * currently used by GPU.
 * @pt - Pointer to kgsl pagetable structure
 *
 * Return - void
 */
static void kgsl_iommu_flush_tlb_pt_current(struct kgsl_pagetable *pt,
				struct kgsl_memdesc *memdesc)
{
	struct kgsl_iommu *iommu = pt->mmu->priv;

	if (kgsl_memdesc_is_secured(memdesc))
		return;

	mutex_lock(&pt->mmu->device->mutex);
	/*
	 * Flush the tlb only if the iommu device is attached and the pagetable
	 * hasn't been switched yet
	 */
	if (kgsl_mmu_is_perprocess(pt->mmu) &&
		iommu->ctx[KGSL_IOMMU_CONTEXT_USER].attached &&
		kgsl_iommu_pt_equal(pt->mmu, pt,
		kgsl_iommu_get_current_ptbase(pt->mmu)))
		kgsl_iommu_flush_pt(pt->mmu);
	mutex_unlock(&pt->mmu->device->mutex);
}

static int
kgsl_iommu_unmap(struct kgsl_pagetable *pt,
		struct kgsl_memdesc *memdesc)
{
	struct kgsl_device *device = pt->mmu->device;
	int ret = 0;
	uint64_t range = memdesc->size;
	size_t unmapped = 0;
	struct kgsl_iommu_pt *iommu_pt = pt->priv;

	/* All GPU addresses as assigned are page aligned, but some
	   functions purturb the gpuaddr with an offset, so apply the
	   mask here to make sure we have the right address */

	uint64_t gpuaddr = PAGE_ALIGN(memdesc->gpuaddr);

	if (range == 0 || gpuaddr == 0)
		return 0;

	if (kgsl_memdesc_has_guard_page(memdesc))
		range += kgsl_memdesc_guard_page_size(memdesc);

	if (kgsl_memdesc_is_secured(memdesc)) {

		if (!kgsl_mmu_is_secured(pt->mmu))
			return -EINVAL;

		mutex_lock(&device->mutex);
		ret = kgsl_active_count_get(device);
		if (!ret) {
			mutex_lock(&device->mutex_pc_smmu);
			unmapped = iommu_unmap(iommu_pt->domain, gpuaddr,
					range);
			mutex_unlock(&device->mutex_pc_smmu);
			kgsl_active_count_put(device);
		}
		mutex_unlock(&device->mutex);
	} else {
		mutex_lock(&device->mutex_pc_smmu);
		unmapped = iommu_unmap(iommu_pt->domain, gpuaddr, range);
		mutex_unlock(&device->mutex_pc_smmu);
	}
	if (unmapped != range) {
		KGSL_CORE_ERR(
			"iommu_unmap(%p, %llx, %lld) failed with unmapped size: %zd\n",
			iommu_pt->domain, gpuaddr, range, unmapped);
		return -EINVAL;
	}

	/*
	 * We only need to flush the TLB for non-global memory.
	 * This is because global mappings are only removed at pagetable destroy
	 * time, and the pagetable is not active in the TLB at this point.
	 */
	if (!kgsl_memdesc_is_global(memdesc))
		kgsl_iommu_flush_tlb_pt_current(pt, memdesc);

	return ret;
}

/**
 * _iommu_add_guard_page - Add iommu guard page
 * @pt - Pointer to kgsl pagetable structure
 * @memdesc - memdesc to add guard page
 * @gpuaddr - GPU addr of guard page
 * @protflags - flags for mapping
 *
 * Return 0 on success, error on map fail
 */
int _iommu_add_guard_page(struct kgsl_pagetable *pt,
						   struct kgsl_memdesc *memdesc,
						   uint64_t gpuaddr,
						   unsigned int protflags)
{
	struct kgsl_iommu_pt *iommu_pt = pt->priv;
	phys_addr_t physaddr = page_to_phys(kgsl_guard_page);
	int ret;

	if (kgsl_memdesc_has_guard_page(memdesc)) {

		/*
		 * Allocate guard page for secure buffers.
		 * This has to be done after we attach a smmu pagetable.
		 * Allocate the guard page when first secure buffer is.
		 * mapped to save 1MB of memory if CPZ is not used.
		 */
		if (kgsl_memdesc_is_secured(memdesc)) {
			unsigned int sgp_size = pt->mmu->secure_align_mask + 1;
			if (!kgsl_secure_guard_page_memdesc.physaddr) {
				if (kgsl_allocate_user(pt->mmu->device,
					&kgsl_secure_guard_page_memdesc, pt,
					sgp_size, sgp_size,
					KGSL_MEMFLAGS_SECURE)) {
					KGSL_CORE_ERR(
					"Secure guard page alloc failed\n");
					return -ENOMEM;
				}
			}

			physaddr = kgsl_secure_guard_page_memdesc.physaddr;
		}

		mutex_lock(&pt->mmu->device->mutex_pc_smmu);
		ret = iommu_map(iommu_pt->domain, gpuaddr, physaddr,
				kgsl_memdesc_guard_page_size(memdesc),
				protflags & ~IOMMU_WRITE);
		mutex_unlock(&pt->mmu->device->mutex_pc_smmu);
		if (ret) {
			KGSL_CORE_ERR(
			"iommu_map(%p, addr %016llX, flags %x) err: %d\n",
			iommu_pt->domain, gpuaddr, protflags & ~IOMMU_WRITE,
			ret);
			return ret;
		}
	}

	return 0;
}

static int
kgsl_iommu_map(struct kgsl_pagetable *pt,
			struct kgsl_memdesc *memdesc)
{
	int ret = 0;
	uint64_t addr;
	struct kgsl_iommu_pt *iommu_pt = pt->priv;
	uint64_t size = memdesc->size;
	unsigned int flags = 0;
	struct kgsl_device *device = pt->mmu->device;
	size_t mapped = 0;

	BUG_ON(NULL == iommu_pt);

	BUG_ON(memdesc->gpuaddr > UINT_MAX);

	addr = (unsigned int) memdesc->gpuaddr;

	flags = IOMMU_READ | IOMMU_WRITE;

	/* Set up the protection for the page(s) */
	if (memdesc->flags & KGSL_MEMFLAGS_GPUREADONLY)
		flags &= ~IOMMU_WRITE;

	if (memdesc->priv & KGSL_MEMDESC_PRIVILEGED)
		flags |= IOMMU_PRIV;

	if (kgsl_memdesc_is_secured(memdesc)) {

		if (!kgsl_mmu_is_secured(pt->mmu))
			return -EINVAL;

		mutex_lock(&device->mutex);
		ret = kgsl_active_count_get(device);
		if (!ret) {
			mutex_lock(&device->mutex_pc_smmu);
			mapped = iommu_map_sg(iommu_pt->domain, addr,
					memdesc->sgt->sgl, memdesc->sgt->nents,
					flags);
			mutex_unlock(&device->mutex_pc_smmu);
			kgsl_active_count_put(device);
		}
		mutex_unlock(&device->mutex);
	} else {
		mutex_lock(&device->mutex_pc_smmu);
		mapped = iommu_map_sg(iommu_pt->domain, addr,
				memdesc->sgt->sgl, memdesc->sgt->nents, flags);
		mutex_unlock(&device->mutex_pc_smmu);
	}


	if (mapped != size) {
		KGSL_CORE_ERR("iommu_map_sg(%p, %016llX, %lld, %x) err: %zd\n",
				iommu_pt->domain, addr, size,
				flags, mapped);
		return -ENODEV;
	}

	ret = _iommu_add_guard_page(pt, memdesc, addr + size, flags);
	if (ret) {
		/* cleanup the partial mapping */
		mutex_lock(&device->mutex_pc_smmu);
		iommu_unmap(iommu_pt->domain, addr, size);
		mutex_unlock(&device->mutex_pc_smmu);
	}

	/*
	 *  IOMMU V1 BFBs pre-fetch data beyond what is being used by the core.
	 *  This can include both allocated pages and un-allocated pages.
	 *  If an un-allocated page is cached, and later used (if it has been
	 *  newly dynamically allocated by SW) the SMMU HW should automatically
	 *  re-fetch the pages from memory (rather than using the cached
	 *  un-allocated page). This logic is known as the re-fetch logic.
	 *  In current chips we suspect this re-fetch logic is broken,
	 *  it can result in bad translations which can either cause downstream
	 *  bus errors, or upstream cores being hung (because of garbage data
	 *  being read) -> causing TLB sync stuck issues. As a result SW must
	 *  implement the invalidate+map.
	 *
	 * We only need to flush the TLB for non-global memory.
	 * This is because global mappings are only created at pagetable create
	 * time, and the pagetable is not active in the TLB at this point.
	 */

	if (MMU_FEATURE(pt->mmu, KGSL_MMU_FLUSH_TLB_ON_MAP)
		&& !kgsl_memdesc_is_global(memdesc))
		kgsl_iommu_flush_tlb_pt_current(pt, memdesc);

	return ret;
}

/* This function must be called with context bank attached */
static void kgsl_iommu_clear_fsr(struct kgsl_mmu *mmu)
{
	int i;
	struct kgsl_iommu *iommu = mmu->priv;
	struct kgsl_iommu_context  *ctx;

	kgsl_iommu_enable_clk(mmu);

	for (i = 0; i < KGSL_IOMMU_CONTEXT_MAX; i++) {
		ctx = &iommu->ctx[i];
		if (ctx->dev == NULL)
			continue;

		/*
		 *  1) HLOS cannot program secure context bank.
		 *  2) If context bank is not attached skip.
		 */
		if (!ctx->attached || (KGSL_IOMMU_CONTEXT_SECURE == i))
			continue;

		KGSL_IOMMU_SET_CTX_REG(iommu, ctx->ctx_id, FSR, 0xffffffff);
	}

	kgsl_iommu_disable_clk(mmu);
}

static void kgsl_iommu_pagefault_resume(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = mmu->priv;
	struct kgsl_iommu_context *ctx;
	unsigned int sctlr_val;
	int i;

	kgsl_iommu_enable_clk(mmu);
	for (i = 0; i < KGSL_IOMMU_CONTEXT_MAX; i++) {
		ctx = &iommu->ctx[i];
		if (ctx->dev == NULL)
			continue;

		/*
		 *  1) HLOS cannot program secure context bank.
		 *  2) If context bank is not attached skip.
		 */
		if (!ctx->attached || (KGSL_IOMMU_CONTEXT_SECURE == i))
			continue;

		if (ctx->fault) {
			/*
			 * Write 1 to RESUME.TnR to terminate the
			 * stalled transaction. Also, re-enable
			 * context fault interrupts by writing 1
			 * to SCTLR.CFIE
			 */
			sctlr_val = KGSL_IOMMU_GET_CTX_REG(iommu, ctx->ctx_id,
					SCTLR);
			sctlr_val |=
				(0x1 << KGSL_IOMMU_SCTLR_CFIE_SHIFT);
			KGSL_IOMMU_SET_CTX_REG(iommu, ctx->ctx_id, RESUME, 1);
			KGSL_IOMMU_SET_CTX_REG(iommu, ctx->ctx_id,
					SCTLR, sctlr_val);
			/*
			 * Make sure the above register writes
			 * are not reordered across the barrier
			 * as we use writel_relaxed to write them
			 */
			wmb();
			ctx->fault = 0;
		}
	}
	kgsl_iommu_disable_clk(mmu);
}


static void kgsl_iommu_stop(struct kgsl_mmu *mmu)
{
	/*
	 *  stop device mmu
	 *
	 *  call this with the global lock held
	 *  detach iommu attachment
	 */
	/* detach iommu attachment */
	if (!MMU_FEATURE(mmu, KGSL_MMU_RETENTION))
		kgsl_detach_pagetable_iommu_domain(mmu);
}

static int kgsl_iommu_close(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = mmu->priv;

	if (mmu->defaultpagetable != NULL)
		kgsl_mmu_putpagetable(mmu->defaultpagetable);

	if (iommu->regbase != NULL)
		iounmap(iommu->regbase);

	kfree(iommu);

	if (kgsl_guard_page != NULL) {
		__free_page(kgsl_guard_page);
		kgsl_guard_page = NULL;
	}

	kgsl_sharedmem_free(&kgsl_secure_guard_page_memdesc);

	return 0;
}

static phys_addr_t
kgsl_iommu_get_current_ptbase(struct kgsl_mmu *mmu)
{
	phys_addr_t pt_base;
	struct kgsl_iommu *iommu = mmu->priv;
	/* We cannot enable or disable the clocks in interrupt context, this
	 function is called from interrupt context if there is an axi error */
	if (in_interrupt())
		return 0;
	/* Return the current pt base by reading IOMMU pt_base register */
	kgsl_iommu_enable_clk(mmu);
	pt_base = KGSL_IOMMU_GET_CTX_REG_Q(iommu,
		iommu->ctx[KGSL_IOMMU_CONTEXT_USER].ctx_id, TTBR0);
	kgsl_iommu_disable_clk(mmu);
	return pt_base & KGSL_IOMMU_CTX_TTBR0_ADDR_MASK;
}

/*
 * kgsl_iommu_flush_pt - Flush the IOMMU pagetable
 * @mmu - Pointer to mmu structure
 */
static int kgsl_iommu_flush_pt(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = mmu->priv;
	unsigned long wait_for_flush;
	unsigned int tlbflush_ctxt =
		iommu->ctx[KGSL_IOMMU_CONTEXT_USER].ctx_id;
	int ret = 0;

	kgsl_iommu_enable_clk(mmu);

	KGSL_IOMMU_SET_CTX_REG(iommu, tlbflush_ctxt, TLBIALL, 1);
	mb();
	/*
	 * Wait for flush to complete by polling the flush
	 * status bit of TLBSTATUS register for not more than
	 * 2 s. After 2s just exit, at that point the SMMU h/w
	 * may be stuck and will eventually cause GPU to hang
	 * or bring the system down.
	 */
	wait_for_flush = jiffies + msecs_to_jiffies(2000);
	KGSL_IOMMU_SET_CTX_REG(iommu, tlbflush_ctxt, TLBSYNC, 0);
	while (KGSL_IOMMU_GET_CTX_REG(iommu, tlbflush_ctxt, TLBSTATUS) &
		(KGSL_IOMMU_CTX_TLBSTATUS_SACTIVE)) {
		if (time_after(jiffies, wait_for_flush)) {
			KGSL_DRV_WARN(mmu->device,
			"Wait limit reached for IOMMU tlb flush\n");
			break;
		}
		cpu_relax();
	}


	/* Disable smmu clock */
	kgsl_iommu_disable_clk(mmu);

	return ret;
}

/*
 * kgsl_iommu_set_pt - Change the IOMMU pagetable of the primary context bank
 * @mmu - Pointer to mmu structure
 * @pt - Pagetable to switch to
 *
 * Set the new pagetable for the IOMMU by doing direct register writes
 * to the IOMMU registers through the cpu
 *
 * Return - void
 */
static int kgsl_iommu_set_pt(struct kgsl_mmu *mmu,
				struct kgsl_pagetable *pt)
{
	struct kgsl_iommu *iommu = mmu->priv;
	uint ctx_id = iommu->ctx[KGSL_IOMMU_CONTEXT_USER].ctx_id;
	int temp;
	int ret = 0;
	phys_addr_t pt_base;
	uint64_t pt_val;

	kgsl_iommu_enable_clk(mmu);

	pt_base = kgsl_iommu_get_pt_base_addr(mmu, pt);

	/*
	 * Taking the liberty to spin idle since this codepath
	 * is invoked when we can spin safely for it to be idle
	 */
	ret = adreno_spin_idle(mmu->device);
	if (ret)
		return ret;

	/* get the lsb value which should not change when changing ttbr0 */
	pt_val = kgsl_iommu_get_default_ttbr0(mmu, KGSL_IOMMU_CONTEXT_USER);

	pt_base &= KGSL_IOMMU_CTX_TTBR0_ADDR_MASK;
	pt_val &= ~KGSL_IOMMU_CTX_TTBR0_ADDR_MASK;
	pt_val |= pt_base;
	KGSL_IOMMU_SET_CTX_REG_Q(iommu, ctx_id, TTBR0, pt_val);

	mb();
	temp = KGSL_IOMMU_GET_CTX_REG_Q(iommu, ctx_id, TTBR0);

	kgsl_iommu_flush_pt(mmu);

	/* Disable smmu clock */
	kgsl_iommu_disable_clk(mmu);

	return ret;
}

/*
 * kgsl_iommu_set_pf_policy() - Set the pagefault policy for IOMMU
 * @mmu: Pointer to mmu structure
 * @pf_policy: The pagefault polict to set
 *
 * Check if the new policy indicated by pf_policy is same as current
 * policy, if same then return else set the policy
 */
static int kgsl_iommu_set_pf_policy(struct kgsl_mmu *mmu,
				unsigned long pf_policy)
{
	int i;
	struct kgsl_iommu *iommu = mmu->priv;
	struct kgsl_iommu_context  *ctx;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(mmu->device);
	int ret = 0;
	unsigned int sctlr_val;

	if ((adreno_dev->ft_pf_policy &
		BIT(KGSL_FT_PAGEFAULT_GPUHALT_ENABLE)) ==
		(pf_policy & BIT(KGSL_FT_PAGEFAULT_GPUHALT_ENABLE)))
		return 0;

	kgsl_iommu_enable_clk(mmu);

	/* Need to idle device before changing options */
	ret = mmu->device->ftbl->idle(mmu->device);
	if (ret) {
		kgsl_iommu_disable_clk(mmu);
		return ret;
	}

	for (i = 0; i < KGSL_IOMMU_CONTEXT_MAX; i++) {
		ctx = &iommu->ctx[i];
		if (ctx->dev == NULL)
			continue;

		/*
		 *  1) HLOS cannot program secure context bank.
		 *  2) If context bank is not attached skip.
		 */
		if (!ctx->attached || (KGSL_IOMMU_CONTEXT_SECURE == i))
			continue;

		sctlr_val = KGSL_IOMMU_GET_CTX_REG(iommu, ctx->ctx_id, SCTLR);

		if (test_bit(KGSL_FT_PAGEFAULT_GPUHALT_ENABLE, &pf_policy)) {
			sctlr_val |= (0x1 << KGSL_IOMMU_SCTLR_CFCFG_SHIFT);
			sctlr_val &= ~(0x1 << KGSL_IOMMU_SCTLR_HUPCF_SHIFT);
		} else {
			sctlr_val &= ~(0x1 << KGSL_IOMMU_SCTLR_CFCFG_SHIFT);
			sctlr_val |= (0x1 << KGSL_IOMMU_SCTLR_HUPCF_SHIFT);
		}

		KGSL_IOMMU_SET_CTX_REG(iommu, ctx->ctx_id, SCTLR, sctlr_val);
	}

	kgsl_iommu_disable_clk(mmu);
	return ret;
}

struct kgsl_protected_registers *kgsl_iommu_get_prot_regs(struct kgsl_mmu *mmu)
{
	static struct kgsl_protected_registers iommuv1_regs = { 0x4000, 14 };
	static struct kgsl_protected_registers iommuv2_regs;

	if (kgsl_msm_supports_iommu_v2()) {

		struct kgsl_iommu *iommu = mmu->priv;

		iommuv2_regs.base = iommu->ahb_base_offset >> 2;
		iommuv2_regs.range = 10;
		return &iommuv2_regs;
	}
	else
		return &iommuv1_regs;
}

static struct kgsl_iommu_addr_entry *_find_gpuaddr(
		struct kgsl_pagetable *pagetable, uint64_t gpuaddr)
{
	struct kgsl_iommu_pt *pt = pagetable->priv;
	struct rb_node *node = pt->rbtree.rb_node;

	while (node != NULL) {
		struct kgsl_iommu_addr_entry *entry = rb_entry(node,
			struct kgsl_iommu_addr_entry, node);

		if (gpuaddr < entry->base)
			node = node->rb_left;
		else if (gpuaddr > entry->base)
			node = node->rb_right;
		else
			return entry;
	}

	return NULL;
}

static int _remove_gpuaddr(struct kgsl_pagetable *pagetable,
		uint64_t gpuaddr)
{
	struct kgsl_iommu_pt *pt = pagetable->priv;
	struct kgsl_iommu_addr_entry *entry;

	entry = _find_gpuaddr(pagetable, gpuaddr);

	if (entry != NULL) {
		rb_erase(&entry->node, &pt->rbtree);
		kmem_cache_free(addr_entry_cache, entry);
		return 0;
	}

	return -ENOMEM;
}

static int _insert_gpuaddr(struct kgsl_pagetable *pagetable,
		uint64_t gpuaddr, uint64_t size)
{
	struct kgsl_iommu_pt *pt = pagetable->priv;
	struct rb_node **node, *parent = NULL;
	struct kgsl_iommu_addr_entry *new =
		kmem_cache_alloc(addr_entry_cache, GFP_ATOMIC);

	if (new == NULL)
		return -ENOMEM;

	new->base = gpuaddr;
	new->size = size;

	node = &pt->rbtree.rb_node;

	while (*node != NULL) {
		struct kgsl_iommu_addr_entry *this;

		parent = *node;
		this = rb_entry(parent, struct kgsl_iommu_addr_entry, node);

		if (new->base < this->base)
			node = &parent->rb_left;
		else if (new->base > this->base)
			node = &parent->rb_right;
		else
			BUG();
	}

	rb_link_node(&new->node, parent, node);
	rb_insert_color(&new->node, &pt->rbtree);

	return 0;
}

static uint64_t _get_unmapped_area(struct kgsl_pagetable *pagetable,
		uint64_t bottom, uint64_t top, uint64_t size,
		uint64_t align)
{
	struct kgsl_iommu_pt *pt = pagetable->priv;
	struct rb_node *node = rb_first(&pt->rbtree);
	uint64_t start;

	bottom = ALIGN(bottom, align);
	start = bottom;

	while (node != NULL) {
		uint64_t gap;
		struct kgsl_iommu_addr_entry *entry = rb_entry(node,
			struct kgsl_iommu_addr_entry, node);

		/*
		 * Skip any entries that are outside of the range, but make sure
		 * to account for some that might straddle the lower bound
		 */
		if (entry->base < bottom) {
			if (entry->base + entry->size > bottom)
				start = ALIGN(entry->base + entry->size, align);
			node = rb_next(node);
			continue;
		}

		/* Stop if we went over the top */
		if (entry->base >= top)
			break;

		/* Make sure there is a gap to consider */
		if (start < entry->base) {
			gap = entry->base - start;

			if (gap >= size)
				return start;
		}

		/* Stop if there is no more room in the region */
		if (entry->base + entry->size >= top)
			return (uint64_t) -ENOMEM;

		/* Start the next cycle at the end of the current entry */
		start = ALIGN(entry->base + entry->size, align);
		node = rb_next(node);
	}

	if (start + size <= top)
		return start;

	return (uint64_t) -ENOMEM;
}

static uint64_t _get_unmapped_area_topdown(struct kgsl_pagetable *pagetable,
		uint64_t bottom, uint64_t top, uint64_t size,
		uint64_t align)
{
	struct kgsl_iommu_pt *pt = pagetable->priv;
	struct rb_node *node = rb_last(&pt->rbtree);
	uint64_t end = top;

	struct kgsl_iommu_addr_entry *entry;

	if (node == NULL)
		return (top - size) & ~(align - 1);

	entry = rb_entry(node, struct kgsl_iommu_addr_entry, node);

	if (ALIGN(entry->base + entry->size, align) < top) {
		if (top - ALIGN(entry->base + entry->size, align) >= size)
			return (top - size) & ~(align - 1);
	}

	while (node != NULL) {
		uint64_t gap;

		entry = rb_entry(node, struct kgsl_iommu_addr_entry, node);

		if ((entry->base + entry->size) < bottom)
			break;

		if ((entry->base + entry->size) < end) {
			gap = end - ALIGN(entry->base + entry->size, align);
			if (gap >= size)
				return ALIGN(entry->base + entry->size, align);
		}

		if (entry->base < bottom)
			return (uint64_t) -ENOMEM;

		end = entry->base;
		node = rb_prev(node);
	}

	if (((end - size) & ~(align - 1)) >= bottom)
		return (end - size) & ~(align - 1);

	return (uint64_t) -ENOMEM;
}

static uint64_t kgsl_iommu_find_svm_region(struct kgsl_pagetable *pagetable,
		uint64_t start, uint64_t end, uint64_t size,
		unsigned int alignment)
{
	struct kgsl_iommu_pt *pt = pagetable->priv;
	uint64_t addr;

	if (pt->svm_start == pt->svm_end)
		return (uint64_t) -ENOMEM;

	if (start < pt->svm_start || start >= pt->svm_end ||
	    end < pt->svm_start || end >= pt->svm_end)
		return (uint64_t) -EINVAL;

	spin_lock(&pagetable->lock);
	addr = _get_unmapped_area_topdown(pagetable,
			start, end, size, alignment);
	spin_unlock(&pagetable->lock);
	return addr;
}

static int kgsl_iommu_set_svm_region(struct kgsl_pagetable *pagetable,
		uint64_t gpuaddr, uint64_t size)
{
	int ret;
	struct kgsl_iommu_pt *pt = pagetable->priv;
	struct rb_node *node;

	if (gpuaddr < pt->svm_start || gpuaddr + size >= pt->svm_end)
		return -EINVAL;

	spin_lock(&pagetable->lock);
	node = pt->rbtree.rb_node;

	while (node != NULL) {
		uint64_t start, end;
		struct kgsl_iommu_addr_entry *entry = rb_entry(node,
			struct kgsl_iommu_addr_entry, node);

		start = entry->base;
		end = entry->base + entry->size;

		if (gpuaddr  + size <= start)
			node = node->rb_left;
		else if (end <= gpuaddr)
			node = node->rb_right;
		else
			goto out;
	}

	ret = _insert_gpuaddr(pagetable, gpuaddr, size);
out:
	spin_unlock(&pagetable->lock);
	return ret;
}


static int kgsl_iommu_get_gpuaddr(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc)
{
	struct kgsl_iommu_pt *pt = pagetable->priv;
	int ret = 0;
	uint64_t addr;
	uint64_t size = memdesc->size;
	unsigned int align;

	BUG_ON(kgsl_memdesc_use_cpu_map(memdesc));

	if (memdesc->flags & KGSL_MEMFLAGS_SECURE &&
			pagetable->name != KGSL_MMU_SECURE_PT)
		return -EINVAL;

	if (kgsl_memdesc_has_guard_page(memdesc))
		size += kgsl_memdesc_guard_page_size(memdesc);

	align = 1 << kgsl_memdesc_get_align(memdesc);

	spin_lock(&pagetable->lock);

	addr = _get_unmapped_area(pagetable, pt->va_start, pt->va_end,
		size, align);

	if (addr == (uint64_t) -ENOMEM) {
		ret = -ENOMEM;
		goto out;
	}

	ret = _insert_gpuaddr(pagetable, addr, size);
	if (ret == 0)
		memdesc->gpuaddr = addr;

out:
	spin_unlock(&pagetable->lock);
	return ret;
}

static void kgsl_iommu_put_gpuaddr(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc)
{
	spin_lock(&pagetable->lock);

	if (_remove_gpuaddr(pagetable, memdesc->gpuaddr))
		BUG();

	spin_unlock(&pagetable->lock);
}

static int kgsl_iommu_svm_range(struct kgsl_pagetable *pagetable,
		uint64_t *lo, uint64_t *hi)
{
	struct kgsl_iommu_pt *pt = pagetable->priv;

	if (lo != NULL)
		*lo = pt->svm_start;
	if (hi != NULL)
		*hi = pt->svm_end;

	return 0;
}

struct kgsl_mmu_ops kgsl_iommu_ops = {
	.mmu_init = kgsl_iommu_init,
	.mmu_close = kgsl_iommu_close,
	.mmu_start = kgsl_iommu_start,
	.mmu_stop = kgsl_iommu_stop,
	.mmu_set_pt = kgsl_iommu_set_pt,
	.mmu_pagefault_resume = kgsl_iommu_pagefault_resume,
	.mmu_clear_fsr = kgsl_iommu_clear_fsr,
	.mmu_get_current_ptbase = kgsl_iommu_get_current_ptbase,
	.mmu_enable_clk = kgsl_iommu_enable_clk,
	.mmu_disable_clk = kgsl_iommu_disable_clk,
	.mmu_get_default_ttbr0 = kgsl_iommu_get_default_ttbr0,
	.mmu_get_reg_ahbaddr = kgsl_iommu_get_reg_ahbaddr,
	.mmu_pt_equal = kgsl_iommu_pt_equal,
	.mmu_get_pt_base_addr = kgsl_iommu_get_pt_base_addr,
	/* These callbacks will be set on some chipsets */
	.mmu_set_pf_policy = kgsl_iommu_set_pf_policy,
	.mmu_pagefault_resume = kgsl_iommu_pagefault_resume,
	.mmu_get_prot_regs = kgsl_iommu_get_prot_regs,
	.mmu_init_pt = kgsl_iommu_init_pt,
};

static struct kgsl_mmu_pt_ops iommu_pt_ops = {
	.mmu_map = kgsl_iommu_map,
	.mmu_unmap = kgsl_iommu_unmap,
	.mmu_destroy_pagetable = kgsl_iommu_destroy_pagetable,
	.get_ptbase = kgsl_iommu_get_ptbase,
	.get_gpuaddr = kgsl_iommu_get_gpuaddr,
	.put_gpuaddr = kgsl_iommu_put_gpuaddr,
	.set_svm_region = kgsl_iommu_set_svm_region,
	.find_svm_region = kgsl_iommu_find_svm_region,
	.svm_range = kgsl_iommu_svm_range,
};
