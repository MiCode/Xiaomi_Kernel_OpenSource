/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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
#include <linux/msm_iommu_domains.h>
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


static struct kgsl_iommu_register_list kgsl_iommuv0_reg[KGSL_IOMMU_REG_MAX] = {
	{ 0, 0 },			/* GLOBAL_BASE */
	{ 0x0, 1 },			/* SCTLR */
	{ 0x10, 1 },			/* TTBR0 */
	{ 0x14, 1 },			/* TTBR1 */
	{ 0x20, 1 },			/* FSR */
	{ 0x28, 1 },			/* FAR */
	{ 0x800, 1 },			/* TLBIALL */
	{ 0x820, 1 },			/* RESUME */
	{ 0x03C, 1 },			/* TLBLKCR */
	{ 0x818, 1 },			/* V2PUR */
	{ 0x2C, 1 },			/* FSYNR0 */
	{ 0x30, 1 },			/* FSYNR1 */
	{ 0, 0 },			/* TLBSYNC, not in v0 */
	{ 0, 0 },			/* TLBSTATUS, not in v0 */
	{ 0, 0 }			/* IMPLDEF_MICRO_MMU_CRTL, not in v0 */
};

static struct kgsl_iommu_register_list kgsl_iommuv1_reg[KGSL_IOMMU_REG_MAX] = {
	{ 0, 0 },			/* GLOBAL_BASE */
	{ 0x0, 1 },			/* SCTLR */
	{ 0x20, 1 },			/* TTBR0 */
	{ 0x28, 1 },			/* TTBR1 */
	{ 0x58, 1 },			/* FSR */
	{ 0x60, 1 },			/* FAR_0 */
	{ 0x618, 1 },			/* TLBIALL */
	{ 0x008, 1 },			/* RESUME */
	{ 0, 0 },			/* TLBLKCR not in V1 */
	{ 0, 0 },			/* V2PUR not in V1 */
	{ 0x68, 1 },			/* FSYNR0 */
	{ 0x6C, 1 },			/* FSYNR1 */
	{ 0x7F0, 1 },			/* TLBSYNC */
	{ 0x7F4, 1 },			/* TLBSTATUS */
	{ 0x2000, 0 }			/* IMPLDEF_MICRO_MMU_CRTL */
};

static struct iommu_access_ops *iommu_access_ops;

static int kgsl_iommu_default_setstate(struct kgsl_mmu *mmu,
		uint32_t flags);
static phys_addr_t
kgsl_iommu_get_current_ptbase(struct kgsl_mmu *mmu);

static void _iommu_lock(struct kgsl_iommu const *iommu)
{
	if (iommu_access_ops && iommu_access_ops->iommu_lock_acquire)
		iommu_access_ops->iommu_lock_acquire(
						iommu->sync_lock_initialized);
}

static void _iommu_unlock(struct kgsl_iommu const *iommu)
{
	if (iommu_access_ops && iommu_access_ops->iommu_lock_release)
		iommu_access_ops->iommu_lock_release(
						iommu->sync_lock_initialized);
}

struct remote_iommu_petersons_spinlock kgsl_iommu_sync_lock_vars;

/*
 * One page allocation for a guard region to protect against over-zealous
 * GPU pre-fetch
 */

static struct page *kgsl_guard_page;

static int get_iommu_unit(struct device *dev, struct kgsl_mmu **mmu_out,
			struct kgsl_iommu_unit **iommu_unit_out)
{
	int i, j, k;

	for (i = 0; i < KGSL_DEVICE_MAX; i++) {
		struct kgsl_mmu *mmu;
		struct kgsl_iommu *iommu;

		if (kgsl_driver.devp[i] == NULL)
			continue;

		mmu = kgsl_get_mmu(kgsl_driver.devp[i]);
		if (mmu == NULL || mmu->priv == NULL)
			continue;

		iommu = mmu->priv;

		for (j = 0; j < iommu->unit_count; j++) {
			struct kgsl_iommu_unit *iommu_unit =
				&iommu->iommu_units[j];
			for (k = 0; k < iommu_unit->dev_count; k++) {
				if (iommu_unit->dev[k].dev == dev) {
					*mmu_out = mmu;
					*iommu_unit_out = iommu_unit;
					return 0;
				}
			}
		}
	}

	return -EINVAL;
}

static struct kgsl_iommu_device *get_iommu_device(struct kgsl_iommu_unit *unit,
		struct device *dev)
{
	int k;

	for (k = 0; unit && k < unit->dev_count; k++) {
		if (unit->dev[k].dev == dev)
			return &(unit->dev[k]);
	}

	return NULL;
}

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
	unsigned int gpuaddr;
	unsigned int size;
	unsigned int flags;
	unsigned int priv;
	pid_t pid;
};

/*
 * Find the closest alloated memory block with an smaller GPU address then the
 * given address
 */

static void _prev_entry(struct kgsl_process_private *priv,
	unsigned int faultaddr, struct _mem_entry *ret)
{
	struct rb_node *node;
	struct kgsl_mem_entry *entry;

	for (node = rb_first(&priv->mem_rb); node; ) {
		entry = rb_entry(node, struct kgsl_mem_entry, node);

		if (entry->memdesc.gpuaddr > faultaddr)
			break;

		/*
		 * If this is closer to the faulting address, then copy
		 * the entry
		 */

		if (entry->memdesc.gpuaddr > ret->gpuaddr) {
			ret->gpuaddr = entry->memdesc.gpuaddr;
			ret->size = entry->memdesc.size;
			ret->flags = entry->memdesc.flags;
			ret->priv = entry->memdesc.priv;
			ret->pid = priv->pid;
		}

		node = rb_next(&entry->node);
	}
}

/*
 * Find the closest alloated memory block with a greater starting GPU address
 * then the given address
 */

static void _next_entry(struct kgsl_process_private *priv,
	unsigned int faultaddr, struct _mem_entry *ret)
{
	struct rb_node *node;
	struct kgsl_mem_entry *entry;

	for (node = rb_last(&priv->mem_rb); node; ) {
		entry = rb_entry(node, struct kgsl_mem_entry, node);

		if (entry->memdesc.gpuaddr < faultaddr)
			break;

		/*
		 * If this is closer to the faulting address, then copy
		 * the entry
		 */

		if (entry->memdesc.gpuaddr < ret->gpuaddr) {
			ret->gpuaddr = entry->memdesc.gpuaddr;
			ret->size = entry->memdesc.size;
			ret->flags = entry->memdesc.flags;
			ret->priv = entry->memdesc.priv;
			ret->pid = priv->pid;
		}

		node = rb_prev(&entry->node);
	}
}

static void _find_mem_entries(struct kgsl_mmu *mmu, unsigned int faultaddr,
	unsigned int ptbase, struct _mem_entry *preventry,
	struct _mem_entry *nextentry)
{
	struct kgsl_process_private *private;
	int id = kgsl_mmu_get_ptname_from_ptbase(mmu, ptbase);

	memset(preventry, 0, sizeof(*preventry));
	memset(nextentry, 0, sizeof(*nextentry));

	/* Set the maximum possible size as an initial value */
	nextentry->gpuaddr = 0xFFFFFFFF;

	mutex_lock(&kgsl_driver.process_mutex);

	list_for_each_entry(private, &kgsl_driver.process_list, list) {

		if (private->pagetable && (private->pagetable->name != id))
			continue;

		spin_lock(&private->mem_lock);
		_prev_entry(private, faultaddr, preventry);
		_next_entry(private, faultaddr, nextentry);
		spin_unlock(&private->mem_lock);
	}

	mutex_unlock(&kgsl_driver.process_mutex);
}

static void _print_entry(struct kgsl_device *device, struct _mem_entry *entry)
{
	char name[32];
	memset(name, 0, sizeof(name));

	kgsl_get_memory_usage(name, sizeof(name) - 1, entry->flags);

	KGSL_LOG_DUMP(device,
		"[%8.8X - %8.8X] %s (pid = %d) (%s)\n",
		entry->gpuaddr,
		entry->gpuaddr + entry->size,
		entry->priv & KGSL_MEMDESC_GUARD_PAGE ? "(+guard)" : "",
		entry->pid, name);
}

static void _check_if_freed(struct kgsl_iommu_device *iommu_dev,
	unsigned long addr, unsigned int pid)
{
	unsigned long gpuaddr = addr;
	unsigned long size = 0;
	unsigned int flags = 0;

	char name[32];
	memset(name, 0, sizeof(name));

	if (kgsl_memfree_find_entry(pid, &gpuaddr, &size, &flags)) {
		kgsl_get_memory_usage(name, sizeof(name) - 1, flags);
		KGSL_LOG_DUMP(iommu_dev->kgsldev, "---- premature free ----\n");
		KGSL_LOG_DUMP(iommu_dev->kgsldev,
			"[%8.8lX-%8.8lX] (%s) was already freed by pid %d\n",
			gpuaddr, gpuaddr + size, name, pid);
	}
}

static int kgsl_iommu_fault_handler(struct iommu_domain *domain,
	struct device *dev, unsigned long addr, int flags, void *token)
{
	int ret = 0;
	struct kgsl_mmu *mmu;
	struct kgsl_iommu *iommu;
	struct kgsl_iommu_unit *iommu_unit;
	struct kgsl_iommu_device *iommu_dev;
	unsigned int ptbase, fsr;
	unsigned int pid;
	struct _mem_entry prev, next;
	unsigned int fsynr0, fsynr1;
	int write;
	struct kgsl_device *device;
	struct adreno_device *adreno_dev;
	unsigned int no_page_fault_log = 0;
	unsigned int curr_context_id = 0;
	unsigned int curr_global_ts = 0;
	struct kgsl_context *context;

	ret = get_iommu_unit(dev, &mmu, &iommu_unit);
	if (ret)
		goto done;

	device = mmu->device;
	adreno_dev = ADRENO_DEVICE(device);
	/*
	 * If mmu fault not set then set it and continue else
	 * exit this function since another thread has already set
	 * it and will execute rest of this function for the fault.
	 */
	if (1 == atomic_cmpxchg(&mmu->fault, 0, 1))
		goto done;

	iommu_dev = get_iommu_device(iommu_unit, dev);
	if (!iommu_dev) {
		KGSL_CORE_ERR("Invalid IOMMU device %p\n", dev);
		ret = -ENOSYS;
		goto done;
	}
	iommu = mmu->priv;

	fsr = KGSL_IOMMU_GET_CTX_REG(iommu, iommu_unit,
		iommu_dev->ctx_id, FSR);
	/*
	 * If fsr is not set then it means that we cleared the fault while the
	 * bottom half called from IOMMU driver is running
	 */
	if (!fsr) {
		atomic_set(&mmu->fault, 0);
		goto done;
	}
	/*
	 * set the fault bits and stuff before any printks so that if fault
	 * handler runs then it will know it's dealing with a pagefault
	 */
	kgsl_sharedmem_readl(&device->memstore, &curr_context_id,
		KGSL_MEMSTORE_OFFSET(KGSL_MEMSTORE_GLOBAL, current_context));

	context = kgsl_context_get(device, curr_context_id);

	if (context != NULL) {
		kgsl_sharedmem_readl(&device->memstore, &curr_global_ts,
			KGSL_MEMSTORE_OFFSET(KGSL_MEMSTORE_GLOBAL,
			eoptimestamp));

		/* save pagefault timestamp for GFT */
		set_bit(KGSL_CONTEXT_PRIV_PAGEFAULT, &context->priv);
		context->pagefault_ts = curr_global_ts;

		kgsl_context_put(context);
		context = NULL;
	}

	iommu_dev->fault = 1;

	if (adreno_dev->ft_pf_policy & KGSL_FT_PAGEFAULT_GPUHALT_ENABLE) {
		adreno_set_gpu_fault(adreno_dev, ADRENO_IOMMU_PAGE_FAULT);
		/* turn off GPU IRQ so we don't get faults from it too */
		kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);
		adreno_dispatcher_schedule(device);
	}

	ptbase = KGSL_IOMMU_GET_CTX_REG_Q(iommu, iommu_unit,
				iommu_dev->ctx_id, TTBR0);

	fsynr0 = KGSL_IOMMU_GET_CTX_REG(iommu, iommu_unit,
		iommu_dev->ctx_id, FSYNR0);
	fsynr1 = KGSL_IOMMU_GET_CTX_REG(iommu, iommu_unit,
		iommu_dev->ctx_id, FSYNR1);

	if (msm_soc_version_supports_iommu_v0())
		write = ((fsynr1 & (KGSL_IOMMU_FSYNR1_AWRITE_MASK <<
			KGSL_IOMMU_FSYNR1_AWRITE_SHIFT)) ? 1 : 0);
	else
		write = ((fsynr0 & (KGSL_IOMMU_V1_FSYNR0_WNR_MASK <<
			KGSL_IOMMU_V1_FSYNR0_WNR_SHIFT)) ? 1 : 0);

	pid = kgsl_mmu_get_ptname_from_ptbase(mmu, ptbase);

	if (adreno_dev->ft_pf_policy & KGSL_FT_PAGEFAULT_LOG_ONE_PER_PAGE)
		no_page_fault_log = kgsl_mmu_log_fault_addr(mmu, ptbase, addr);

	if (!no_page_fault_log) {
		KGSL_MEM_CRIT(iommu_dev->kgsldev,
			"GPU PAGE FAULT: addr = %lX pid = %d\n", addr, pid);
		KGSL_MEM_CRIT(iommu_dev->kgsldev,
		 "context = %d TTBR0 = %X FSR = %X FSYNR0 = %X FSYNR1 = %X(%s fault)\n",
			iommu_dev->ctx_id, ptbase, fsr, fsynr0, fsynr1,
			write ? "write" : "read");

		_check_if_freed(iommu_dev, addr, pid);

		KGSL_LOG_DUMP(iommu_dev->kgsldev, "---- nearby memory ----\n");

		_find_mem_entries(mmu, addr, ptbase, &prev, &next);

		if (prev.gpuaddr)
			_print_entry(iommu_dev->kgsldev, &prev);
		else
			KGSL_LOG_DUMP(iommu_dev->kgsldev, "*EMPTY*\n");

		KGSL_LOG_DUMP(iommu_dev->kgsldev, " <- fault @ %8.8lX\n", addr);

		if (next.gpuaddr != 0xFFFFFFFF)
			_print_entry(iommu_dev->kgsldev, &next);
		else
			KGSL_LOG_DUMP(iommu_dev->kgsldev, "*EMPTY*\n");

	}

	trace_kgsl_mmu_pagefault(iommu_dev->kgsldev, addr,
			kgsl_mmu_get_ptname_from_ptbase(mmu, ptbase),
			write ? "write" : "read");

	/*
	 * We do not want the h/w to resume fetching data from an iommu unit
	 * that has faulted, this is better for debugging as it will stall
	 * the GPU and trigger a snapshot. To stall the transaction return
	 * EBUSY error.
	 */
	if (adreno_dev->ft_pf_policy & KGSL_FT_PAGEFAULT_GPUHALT_ENABLE)
		ret = -EBUSY;
done:
	return ret;
}

/*
 * kgsl_iommu_disable_clk - Disable iommu clocks
 * @mmu - Pointer to mmu structure
 * @unit - Iommu unit
 *
 * Disables iommu clocks for an iommu unit
 * Return - void
 */
static void kgsl_iommu_disable_clk(struct kgsl_mmu *mmu, int unit)
{
	struct kgsl_iommu *iommu = mmu->priv;
	int i, j;

	for (i = 0; i < iommu->unit_count; i++) {
		struct kgsl_iommu_unit *iommu_unit = &iommu->iommu_units[i];

		/* Turn off the clks for IOMMU unit requested */
		if ((unit != i) && (unit != KGSL_IOMMU_MAX_UNITS))
			continue;

		atomic_dec(&iommu_unit->clk_enable_count);
		BUG_ON(atomic_read(&iommu_unit->clk_enable_count) < 0);

		for (j = (KGSL_IOMMU_MAX_CLKS - 1); j >= 0; j--)
			if (iommu_unit->clks[j])
				clk_disable_unprepare(iommu_unit->clks[j]);
	}
}

/*
 * kgsl_iommu_disable_clk_event() - Disable IOMMU clocks after timestamp
 * @device: The kgsl device pointer
 * @context: Pointer to the context that fired the event
 * @data: Pointer to the private data for the event
 * @type: Result of the callback (retired or cancelled)
 *
 * An event function that is executed when
 * the required timestamp is reached. It disables the IOMMU clocks if
 * the timestamp on which the clocks can be disabled has expired.
 *
 * Return - void
 */
static void kgsl_iommu_clk_disable_event(struct kgsl_device *device,
		struct kgsl_context *context, void *data, int type)
{
	struct kgsl_iommu_disable_clk_param *param = data;

	kgsl_iommu_disable_clk(param->mmu, param->unit);

	/* Free param we are done using it */
	kfree(param);
}



/*
 * kgsl_iommu_disable_clk_on_ts - Sets up event to disable IOMMU clocks
 * @mmu - The kgsl MMU pointer
 * @ts - Timestamp on which the clocks should be disabled
 * @ts_valid - Indicates whether ts parameter is valid, if this parameter
 * is false then it means that the calling function wants to disable the
 * IOMMU clocks immediately without waiting for any timestamp
 * @unit: IOMMU unit for which clocks are to be turned off
 *
 * Creates an event to disable the IOMMU clocks on timestamp and if event
 * already exists then updates the timestamp of disabling the IOMMU clocks
 * with the passed in ts if it is greater than the current value at which
 * the clocks will be disabled
 * Return - void
 */
static void
kgsl_iommu_disable_clk_on_ts(struct kgsl_mmu *mmu,
				unsigned int ts, int unit)
{
	struct kgsl_iommu_disable_clk_param *param;
	struct kgsl_iommu *iommu = mmu->priv;

	param = kzalloc(sizeof(*param), GFP_KERNEL);
	if (!param)
		return;

	param->mmu = mmu;
	param->unit = unit;
	param->ts = ts;

	if (kgsl_add_event(mmu->device, &iommu->events,
			ts, kgsl_iommu_clk_disable_event, param)) {
		KGSL_DRV_ERR(mmu->device,
			"Failed to add IOMMU disable clk event\n");
		kfree(param);
	}
}

/*
 * kgsl_iommu_enable_clk_prepare_enable - Enable iommu clock
 * @clk - clock to enable
 *
 * Prepare enables clock. Retries 3 times on enable failure, on 4th failure
 * returns an error.
 * Return: 0 on success else 1 on error
 */

static int kgsl_iommu_clk_prepare_enable(struct clk *clk)
{
	int num_retries = 4;

	while (num_retries--) {
		if (!clk_prepare_enable(clk))
			return 0;
	}

	return 1;
}

/*
 * kgsl_iommu_enable_clk - Enable iommu clocks
 * @mmu - Pointer to mmu structure
 * @unit - The iommu unit whose clocks are to be turned on
 *
 * Enables iommu clocks of a given iommu unit
 * Return: 0 on success else error code
 */
static void kgsl_iommu_enable_clk(struct kgsl_mmu *mmu,
				int unit)
{
	int i, j;
	struct kgsl_iommu *iommu = mmu->priv;

	for (i = 0; i < iommu->unit_count; i++) {
		struct kgsl_iommu_unit *iommu_unit = &iommu->iommu_units[i];

		/* Turn on the clks for IOMMU unit requested */
		if ((unit != i) && (unit != KGSL_IOMMU_MAX_UNITS))
			continue;

		for (j = 0; j < KGSL_IOMMU_MAX_CLKS; j++) {
			if (iommu_unit->clks[j])
				if (kgsl_iommu_clk_prepare_enable(
						iommu_unit->clks[j]))
						goto done;
		}
		atomic_inc(&iommu_unit->clk_enable_count);
	}
	return;
done:
	/*
	 * Any Clock enable failure should be fatal,
	 * System usually crashes when enabling clock fails
	 * BUG_ON here to catch the system in bad state for
	 * further debug
	 */
	KGSL_CORE_ERR("IOMMU clk enable failed\n");
	BUG();
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

	domain_ptbase = iommu_get_pt_base_addr(iommu_pt->domain)
			& KGSL_IOMMU_CTX_TTBR0_ADDR_MASK;

	pt_base &= KGSL_IOMMU_CTX_TTBR0_ADDR_MASK;

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
	phys_addr_t domain_ptbase;

	if (iommu_pt == NULL)
		return 0;

	domain_ptbase = iommu_get_pt_base_addr(iommu_pt->domain);

	return domain_ptbase & KGSL_IOMMU_CTX_TTBR0_ADDR_MASK;
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

	if (iommu_pt->domain) {
		phys_addr_t domain_ptbase =
			iommu_get_pt_base_addr(iommu_pt->domain);
		trace_kgsl_pagetable_destroy(domain_ptbase, pt->name);
		msm_unregister_domain(iommu_pt->domain);
	}

	kfree(iommu_pt);
	iommu_pt = NULL;
}

/*
 * kgsl_iommu_create_pagetable - Create a IOMMU pagetable
 *
 * Allocate memory to hold a pagetable and allocate the IOMMU
 * domain which is the actual IOMMU pagetable
 * Return - void
 */
static void *kgsl_iommu_create_pagetable(void)
{
	int domain_num;
	struct kgsl_iommu_pt *iommu_pt;

	struct msm_iova_layout kgsl_layout = {
		/* we manage VA space ourselves, so partitions aren't needed */
		.partitions = NULL,
		.npartitions = 0,
		.client_name = "kgsl",
		.domain_flags = 0,
	};

	iommu_pt = kzalloc(sizeof(struct kgsl_iommu_pt), GFP_KERNEL);
	if (!iommu_pt)
		return NULL;

	/* L2 redirect is not stable on IOMMU v1 */
	if (msm_soc_version_supports_iommu_v0())
		kgsl_layout.domain_flags = MSM_IOMMU_DOMAIN_PT_CACHEABLE;

	domain_num = msm_register_domain(&kgsl_layout);
	if (domain_num >= 0) {
		iommu_pt->domain = msm_get_iommu_domain(domain_num);

		if (iommu_pt->domain) {
			iommu_set_fault_handler(iommu_pt->domain,
				kgsl_iommu_fault_handler, NULL);

			return iommu_pt;
		}
	}

	KGSL_CORE_ERR("Failed to create iommu domain\n");
	kfree(iommu_pt);
	return NULL;
}

/*
 * kgsl_detach_pagetable_iommu_domain - Detach the IOMMU unit from a
 * pagetable
 * @mmu - Pointer to the device mmu structure
 * @priv - Flag indicating whether the private or user context is to be
 * detached
 *
 * Detach the IOMMU unit with the domain that is contained in the
 * hwpagetable of the given mmu. After detaching the IOMMU unit is not
 * in use because the PTBR will not be set after a detach
 * Return - void
 */
static void kgsl_detach_pagetable_iommu_domain(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu_pt *iommu_pt;
	struct kgsl_iommu *iommu = mmu->priv;
	int i, j;

	for (i = 0; i < iommu->unit_count; i++) {
		struct kgsl_iommu_unit *iommu_unit = &iommu->iommu_units[i];
		iommu_pt = mmu->defaultpagetable->priv;
		for (j = 0; j < iommu_unit->dev_count; j++) {
			/*
			 * If there is a 2nd default pagetable then priv domain
			 * is attached with this pagetable
			 */
			if (mmu->priv_bank_table &&
				(KGSL_IOMMU_CONTEXT_PRIV == j))
				iommu_pt = mmu->priv_bank_table->priv;
			if (mmu->securepagetable &&
				(KGSL_IOMMU_CONTEXT_SECURE == j))
				iommu_pt = mmu->securepagetable->priv;
			if (iommu_unit->dev[j].attached) {
				iommu_detach_device(iommu_pt->domain,
						iommu_unit->dev[j].dev);
				iommu_unit->dev[j].attached = false;
				KGSL_MEM_INFO(mmu->device, "iommu %p detached "
					"from user dev of MMU: %p\n",
					iommu_pt->domain, mmu);
			}
		}
	}
}

/*
 * kgsl_attach_pagetable_iommu_domain - Attach the IOMMU unit to a
 * pagetable, i.e set the IOMMU's PTBR to the pagetable address and
 * setup other IOMMU registers for the device so that it becomes
 * active
 * @mmu - Pointer to the device mmu structure
 * @priv - Flag indicating whether the private or user context is to be
 * attached
 *
 * Attach the IOMMU unit with the domain that is contained in the
 * hwpagetable of the given mmu.
 * Return - 0 on success else error code
 */
static int kgsl_attach_pagetable_iommu_domain(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu_pt *iommu_pt;
	struct kgsl_iommu *iommu = mmu->priv;
	struct msm_iommu_drvdata *drvdata = 0;
	int i, j, ret = 0;

	/*
	 * Loop through all the iommu devcies under all iommu units and
	 * attach the domain
	 */
	for (i = 0; i < iommu->unit_count; i++) {
		struct kgsl_iommu_unit *iommu_unit = &iommu->iommu_units[i];
		iommu_pt = mmu->defaultpagetable->priv;
		for (j = 0; j < iommu_unit->dev_count; j++) {
			/*
			 * If there is a 2nd default pagetable then priv domain
			 * is attached to this pagetable
			 */
			if (mmu->priv_bank_table &&
					(KGSL_IOMMU_CONTEXT_PRIV == j))
				iommu_pt = mmu->priv_bank_table->priv;
			if (mmu->securepagetable &&
					(KGSL_IOMMU_CONTEXT_SECURE == j))
				iommu_pt = mmu->securepagetable->priv;
			if (!iommu_unit->dev[j].attached) {
				ret = iommu_attach_device(iommu_pt->domain,
							iommu_unit->dev[j].dev);
				if (ret) {
					KGSL_MEM_ERR(mmu->device,
						"Failed to attach device, err %d\n",
						ret);
					goto done;
				}
				iommu_unit->dev[j].attached = true;
				KGSL_MEM_INFO(mmu->device,
				"iommu pt %p attached to dev %p, ctx_id %d\n",
				iommu_pt->domain, iommu_unit->dev[j].dev,
				iommu_unit->dev[j].ctx_id);
				/* Init IOMMU unit clks here */
				if (!drvdata) {
					drvdata = dev_get_drvdata(
					iommu_unit->dev[j].dev->parent);
					iommu_unit->clks[0] = drvdata->pclk;
					iommu_unit->clks[1] = drvdata->clk;
					iommu_unit->clks[2] = drvdata->aclk;
					iommu_unit->clks[3] =
							iommu->gtcu_iface_clk;
				}
			}
		}
	}
done:
	return ret;
}

/*
 * _get_iommu_ctxs - Get device pointer to IOMMU contexts
 * @mmu - Pointer to mmu device
 * data - Pointer to the platform data containing information about
 * iommu devices for one iommu unit
 * unit_id - The IOMMU unit number. This is not a specific ID but just
 * a serial number. The serial numbers are treated as ID's of the
 * IOMMU units
 *
 * Return - 0 on success else error code
 */
static int _get_iommu_ctxs(struct kgsl_mmu *mmu,
	struct kgsl_device_iommu_data *data, unsigned int unit_id)
{
	struct kgsl_iommu *iommu = mmu->priv;
	struct kgsl_iommu_unit *iommu_unit = &iommu->iommu_units[unit_id];
	int i, j;
	int found_ctx;
	int ret = 0;

	for (j = 0; j < KGSL_IOMMU_CONTEXT_MAX; j++) {
		found_ctx = 0;
		for (i = 0; i < data->iommu_ctx_count; i++) {
			if (j == data->iommu_ctxs[i].ctx_id) {
				found_ctx = 1;
				break;
			}
		}
		if (!found_ctx)
			break;
		if (!data->iommu_ctxs[i].iommu_ctx_name) {
			KGSL_CORE_ERR("Context name invalid\n");
			ret = -EINVAL;
			goto done;
		}
		atomic_set(&(iommu_unit->clk_enable_count), 0);

		iommu_unit->dev[iommu_unit->dev_count].dev =
			msm_iommu_get_ctx(data->iommu_ctxs[i].iommu_ctx_name);
		if (NULL == iommu_unit->dev[iommu_unit->dev_count].dev)
			ret = -EINVAL;
		if (IS_ERR(iommu_unit->dev[iommu_unit->dev_count].dev)) {
			ret = PTR_ERR(
				iommu_unit->dev[iommu_unit->dev_count].dev);
			iommu_unit->dev[iommu_unit->dev_count].dev = NULL;
		}
		if (ret)
			goto done;
		iommu_unit->dev[iommu_unit->dev_count].ctx_id =
						data->iommu_ctxs[i].ctx_id;
		iommu_unit->dev[iommu_unit->dev_count].kgsldev = mmu->device;

		KGSL_DRV_INFO(mmu->device,
				"Obtained dev handle %p for iommu context %s\n",
				iommu_unit->dev[iommu_unit->dev_count].dev,
				data->iommu_ctxs[i].iommu_ctx_name);

		iommu_unit->dev_count++;
	}
done:
	if (!iommu_unit->dev_count && !ret)
		ret = -EINVAL;
	if (ret) {
		/*
		 * If at least the first context is initialized on v1
		 * then we can continue
		 */
		if (!msm_soc_version_supports_iommu_v0() &&
			iommu_unit->dev_count)
			ret = 0;
		else
			KGSL_CORE_ERR(
			"Failed to initialize iommu contexts, err: %d\n", ret);
	}

	return ret;
}

/*
 * kgsl_iommu_start_sync_lock - Initialize some variables during MMU start up
 * for GPU CPU synchronization
 * @mmu - Pointer to mmu device
 *
 * Return - 0 on success else error code
 */
static int kgsl_iommu_start_sync_lock(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = mmu->priv;
	uint32_t lock_gpu_addr = 0;

	if (KGSL_DEVICE_3D0 != mmu->device->id ||
		!msm_soc_version_supports_iommu_v0() ||
		!kgsl_mmu_is_perprocess(mmu) ||
		iommu->sync_lock_vars)
		return 0;

	if (!(mmu->flags & KGSL_MMU_FLAGS_IOMMU_SYNC)) {
		KGSL_DRV_ERR(mmu->device,
		"The GPU microcode does not support IOMMUv1 sync opcodes\n");
		return -ENXIO;
	}
	/* Store Lock variables GPU address  */
	lock_gpu_addr = (iommu->sync_lock_desc.gpuaddr +
			iommu->sync_lock_offset);

	kgsl_iommu_sync_lock_vars.flag[PROC_APPS] = (lock_gpu_addr +
		(offsetof(struct remote_iommu_petersons_spinlock,
			flag[PROC_APPS])));
	kgsl_iommu_sync_lock_vars.flag[PROC_GPU] = (lock_gpu_addr +
		(offsetof(struct remote_iommu_petersons_spinlock,
			flag[PROC_GPU])));
	kgsl_iommu_sync_lock_vars.turn = (lock_gpu_addr +
		(offsetof(struct remote_iommu_petersons_spinlock, turn)));

	iommu->sync_lock_vars = &kgsl_iommu_sync_lock_vars;

	return 0;
}

#ifdef CONFIG_MSM_IOMMU_GPU_SYNC

/*
 * kgsl_get_sync_lock - Init Sync Lock between GPU and CPU
 * @mmu - Pointer to mmu device
 *
 * Return - 0 on success else error code
 */
static int kgsl_iommu_init_sync_lock(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = mmu->priv;
	int status = 0;
	uint32_t lock_phy_addr = 0;
	uint32_t page_offset = 0;

	if (!msm_soc_version_supports_iommu_v0() ||
		!kgsl_mmu_is_perprocess(mmu))
		return status;

	/* Return if already initialized */
	if (iommu->sync_lock_initialized)
		return status;

	iommu_access_ops = msm_get_iommu_access_ops();

	if (iommu_access_ops && iommu_access_ops->iommu_lock_initialize) {
		lock_phy_addr = (uint32_t)
				iommu_access_ops->iommu_lock_initialize();
		if (!lock_phy_addr) {
			iommu_access_ops = NULL;
			return status;
		}
		lock_phy_addr = lock_phy_addr - (uint32_t)MSM_SHARED_RAM_BASE +
				(uint32_t)msm_shared_ram_phys;
	}

	/* Align the physical address to PAGE boundary and store the offset */
	page_offset = (lock_phy_addr & (PAGE_SIZE - 1));
	lock_phy_addr = (lock_phy_addr & ~(PAGE_SIZE - 1));
	iommu->sync_lock_desc.physaddr = (unsigned int)lock_phy_addr;
	iommu->sync_lock_offset = page_offset;

	iommu->sync_lock_desc.size =
				PAGE_ALIGN(sizeof(kgsl_iommu_sync_lock_vars));
	status =  memdesc_sg_phys(&iommu->sync_lock_desc,
				 iommu->sync_lock_desc.physaddr,
				 iommu->sync_lock_desc.size);

	if (status) {
		iommu_access_ops = NULL;
		return status;
	}

	/* Add the entry to the global PT list */
	status = kgsl_add_global_pt_entry(mmu->device, &iommu->sync_lock_desc);
	if (status) {
		kgsl_sg_free(iommu->sync_lock_desc.sg,
			iommu->sync_lock_desc.sglen);
		iommu_access_ops = NULL;
		return status;
	}

	/* Flag Sync Lock is Initialized  */
	iommu->sync_lock_initialized = 1;

	return status;
}
#else
static int kgsl_iommu_init_sync_lock(struct kgsl_mmu *mmu)
{
	return 0;
}
#endif

/*
 * kgsl_iommu_sync_lock - Acquire Sync Lock between GPU and CPU
 * @mmu - Pointer to mmu device
 * @cmds - Pointer to array of commands
 *
 * Return - int - number of commands.
 */
static unsigned int kgsl_iommu_sync_lock(struct kgsl_mmu *mmu,
						unsigned int *cmds)
{
	struct kgsl_device *device = mmu->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_iommu *iommu = mmu->device->mmu.priv;
	struct remote_iommu_petersons_spinlock *lock_vars =
					iommu->sync_lock_vars;
	unsigned int *start = cmds;

	if (!iommu->sync_lock_initialized)
		return 0;

	*cmds++ = cp_type3_packet(CP_MEM_WRITE, 2);
	*cmds++ = lock_vars->flag[PROC_GPU];
	*cmds++ = 1;

	cmds += adreno_add_idle_cmds(adreno_dev, cmds);

	*cmds++ = cp_type3_packet(CP_WAIT_REG_MEM, 5);
	/* MEM SPACE = memory, FUNCTION = equals */
	*cmds++ = 0x13;
	*cmds++ = lock_vars->flag[PROC_GPU];
	*cmds++ = 0x1;
	*cmds++ = 0x1;
	*cmds++ = 0x1;

	/* WAIT_REG_MEM turns back on protected mode - push it off */
	*cmds++ = cp_type3_packet(CP_SET_PROTECTED_MODE, 1);
	*cmds++ = 0;

	*cmds++ = cp_type3_packet(CP_MEM_WRITE, 2);
	*cmds++ = lock_vars->turn;
	*cmds++ = 0;

	cmds += adreno_add_idle_cmds(adreno_dev, cmds);

	*cmds++ = cp_type3_packet(CP_WAIT_REG_MEM, 5);
	/* MEM SPACE = memory, FUNCTION = equals */
	*cmds++ = 0x13;
	*cmds++ = lock_vars->flag[PROC_GPU];
	*cmds++ = 0x1;
	*cmds++ = 0x1;
	*cmds++ = 0x1;

	/* WAIT_REG_MEM turns back on protected mode - push it off */
	*cmds++ = cp_type3_packet(CP_SET_PROTECTED_MODE, 1);
	*cmds++ = 0;

	*cmds++ = cp_type3_packet(CP_TEST_TWO_MEMS, 3);
	*cmds++ = lock_vars->flag[PROC_APPS];
	*cmds++ = lock_vars->turn;
	*cmds++ = 0;

	/* TEST_TWO_MEMS turns back on protected mode - push it off */
	*cmds++ = cp_type3_packet(CP_SET_PROTECTED_MODE, 1);
	*cmds++ = 0;

	cmds += adreno_add_idle_cmds(adreno_dev, cmds);

	return cmds - start;
}

/*
 * kgsl_iommu_sync_unlock - Release Sync Lock between GPU and CPU
 * @mmu - Pointer to mmu device
 * @cmds - Pointer to array of commands
 *
 * Return - int - number of commands.
 */
static unsigned int kgsl_iommu_sync_unlock(struct kgsl_mmu *mmu,
					unsigned int *cmds)
{
	struct kgsl_device *device = mmu->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_iommu *iommu = mmu->device->mmu.priv;
	struct remote_iommu_petersons_spinlock *lock_vars =
						iommu->sync_lock_vars;
	unsigned int *start = cmds;

	if (!iommu->sync_lock_initialized)
		return 0;

	*cmds++ = cp_type3_packet(CP_MEM_WRITE, 2);
	*cmds++ = lock_vars->flag[PROC_GPU];
	*cmds++ = 0;

	*cmds++ = cp_type3_packet(CP_WAIT_REG_MEM, 5);
	/* MEM SPACE = memory, FUNCTION = equals */
	*cmds++ = 0x13;
	*cmds++ = lock_vars->flag[PROC_GPU];
	*cmds++ = 0x0;
	*cmds++ = 0x1;
	*cmds++ = 0x1;

	/* WAIT_REG_MEM turns back on protected mode - push it off */
	*cmds++ = cp_type3_packet(CP_SET_PROTECTED_MODE, 1);
	*cmds++ = 0;

	cmds += adreno_add_idle_cmds(adreno_dev, cmds);

	return cmds - start;
}

/*
 * kgsl_get_iommu_ctxt - Get device pointer to IOMMU contexts
 * @mmu - Pointer to mmu device
 *
 * Get the device pointers for the IOMMU user and priv contexts of the
 * kgsl device
 * Return - 0 on success else error code
 */
static int kgsl_get_iommu_ctxt(struct kgsl_mmu *mmu)
{
	struct kgsl_device_platform_data *pdata =
		dev_get_platdata(&mmu->device->pdev->dev);
	struct kgsl_iommu *iommu = mmu->device->mmu.priv;
	int i, ret = 0;

	/* Go through the IOMMU data and get all the context devices */
	if (KGSL_IOMMU_MAX_UNITS < pdata->iommu_count) {
		KGSL_CORE_ERR("Too many IOMMU units defined\n");
		ret = -EINVAL;
		goto  done;
	}

	for (i = 0; i < pdata->iommu_count; i++) {
		ret = _get_iommu_ctxs(mmu, &pdata->iommu_data[i], i);
		if (ret)
			break;
	}
	iommu->unit_count = pdata->iommu_count;
done:
	return ret;
}

/*
 * kgsl_set_register_map - Map the IOMMU regsiters in the memory descriptors
 * of the respective iommu units
 * @mmu - Pointer to mmu structure
 *
 * Return - 0 on success else error code
 */
static int kgsl_set_register_map(struct kgsl_mmu *mmu)
{
	struct kgsl_device_platform_data *pdata =
		dev_get_platdata(&mmu->device->pdev->dev);
	struct kgsl_iommu *iommu = mmu->device->mmu.priv;
	struct kgsl_iommu_unit *iommu_unit;
	int i = 0, ret = 0;
	struct kgsl_device *device = mmu->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	for (; i < pdata->iommu_count; i++) {
		struct kgsl_device_iommu_data data = pdata->iommu_data[i];
		iommu_unit = &iommu->iommu_units[i];
		/* set up the IOMMU register map for the given IOMMU unit */
		if (!data.physstart || !data.physend) {
			KGSL_CORE_ERR("The register range for IOMMU unit not"
					" specified\n");
			ret = -EINVAL;
			goto err;
		}
		iommu_unit->reg_map.hostptr = ioremap(data.physstart,
					data.physend - data.physstart + 1);
		if (!iommu_unit->reg_map.hostptr) {
			KGSL_CORE_ERR("Failed to map SMMU register address "
				"space from %x to %x\n", data.physstart,
				data.physend - data.physstart + 1);
			ret = -ENOMEM;
			i--;
			goto err;
		}
		iommu_unit->reg_map.size = data.physend - data.physstart + 1;
		iommu_unit->reg_map.physaddr = data.physstart;
		ret = memdesc_sg_phys(&iommu_unit->reg_map, data.physstart,
				iommu_unit->reg_map.size);
		if (ret)
			goto err;

		/* Add the register map to the global PT list */
		kgsl_add_global_pt_entry(mmu->device, &iommu_unit->reg_map);

		if (!msm_soc_version_supports_iommu_v0())
			iommu_unit->iommu_halt_enable = 1;

		if (kgsl_msm_supports_iommu_v2())
			if (adreno_is_a405(adreno_dev)) {
				iommu_unit->ahb_base =
					KGSL_IOMMU_V2_AHB_BASE_A405;
			} else
				iommu_unit->ahb_base = KGSL_IOMMU_V2_AHB_BASE;
		else
			iommu_unit->ahb_base =
				data.physstart - mmu->device->reg_phys;
	}
	iommu->unit_count = pdata->iommu_count;
	return ret;
err:
	/* Unmap any mapped IOMMU regions */
	for (; i >= 0; i--) {
		iommu_unit = &iommu->iommu_units[i];

		kgsl_remove_global_pt_entry(&iommu_unit->reg_map);
		iounmap(iommu_unit->reg_map.hostptr);
		iommu_unit->reg_map.size = 0;
		iommu_unit->reg_map.physaddr = 0;
	}
	return ret;
}

/*
 * kgsl_iommu_get_pt_base_addr - Get the address of the pagetable that the
 * IOMMU ttbr0 register is programmed with
 * @mmu - Pointer to mmu
 * @pt - kgsl pagetable pointer that contains the IOMMU domain pointer
 *
 * Return - actual pagetable address that the ttbr0 register is programmed
 * with
 */
static phys_addr_t kgsl_iommu_get_pt_base_addr(struct kgsl_mmu *mmu,
						struct kgsl_pagetable *pt)
{
	struct kgsl_iommu_pt *iommu_pt = pt->priv;
	return iommu_get_pt_base_addr(iommu_pt->domain) &
			KGSL_IOMMU_CTX_TTBR0_ADDR_MASK;
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
				unsigned int unit_id,
				enum kgsl_iommu_context_id ctx_id)
{
	struct kgsl_iommu *iommu = mmu->priv;
	int i, j;
	for (i = 0; i < iommu->unit_count; i++) {
		struct kgsl_iommu_unit *iommu_unit = &iommu->iommu_units[i];
		for (j = 0; j < iommu_unit->dev_count; j++)
			if (unit_id == i &&
				ctx_id == iommu_unit->dev[j].ctx_id)
				return iommu_unit->dev[j].default_ttbr0;
	}
	return 0;
}

static int kgsl_iommu_setstate(struct kgsl_mmu *mmu,
				struct kgsl_pagetable *pagetable,
				unsigned int context_id)
{
	int ret = 0;

	/* page table not current, then setup mmu to use new
	 *  specified page table
	 */
	if (mmu->hwpagetable != pagetable) {
		unsigned int flags = 0;
		mmu->hwpagetable = pagetable;
		flags |= kgsl_mmu_pt_get_flags(mmu->hwpagetable,
						mmu->device->id) |
						KGSL_MMUFLAGS_TLBFLUSH;
		ret = kgsl_setstate(mmu, context_id,
			KGSL_MMUFLAGS_PTUPDATE | flags);
	}

	return ret;
}

/*
 * kgsl_iommu_get_reg_ahbaddr - Returns the ahb address of the register
 * @mmu - Pointer to mmu structure
 * @iommu_unit - The iommu unit for which base address is requested
 * @ctx_id - The context ID of the IOMMU ctx
 * @reg - The register for which address is required
 *
 * Return - The address of register which can be used in type0 packet
 */
static unsigned int kgsl_iommu_get_reg_ahbaddr(struct kgsl_mmu *mmu,
					int iommu_unit, int ctx_id,
					enum kgsl_iommu_reg_map reg)
{
	struct kgsl_iommu *iommu = mmu->priv;

	if (iommu->iommu_reg_list[reg].ctx_reg)
		return iommu->iommu_units[iommu_unit].ahb_base +
			iommu->iommu_reg_list[reg].reg_offset +
			(ctx_id << KGSL_IOMMU_CTX_SHIFT) +
			iommu->ctx_ahb_offset;
	else
		return iommu->iommu_units[iommu_unit].ahb_base +
			iommu->iommu_reg_list[reg].reg_offset;
}

static int iommu_readtimestamp(struct kgsl_device *device, void *priv,
		enum kgsl_timestamp_type type, unsigned int *timestamp)
{
	return kgsl_readtimestamp(device, NULL, type, timestamp);
}

static int kgsl_iommu_init(struct kgsl_mmu *mmu)
{
	/*
	 * intialize device mmu
	 *
	 * call this with the global lock held
	 */
	int status = 0;
	struct kgsl_iommu *iommu;
	struct platform_device *pdev = mmu->device->pdev;
	struct kgsl_device *device = mmu->device;
	size_t secured_pool_sz = 0;

	atomic_set(&mmu->fault, 0);
	iommu = kzalloc(sizeof(struct kgsl_iommu), GFP_KERNEL);
	if (!iommu)
		return -ENOMEM;

	kgsl_add_event_group(&iommu->events, NULL, "iommu",
		iommu_readtimestamp, iommu);

	mmu->priv = iommu;
	status = kgsl_get_iommu_ctxt(mmu);
	if (status)
		goto done;
	status = kgsl_set_register_map(mmu);
	if (status)
		goto done;

	if (KGSL_MMU_USE_PER_PROCESS_PT &&
		of_property_match_string(pdev->dev.of_node, "clock-names",
						"gtcu_iface_clk") >= 0)
		iommu->gtcu_iface_clk = clk_get(&pdev->dev, "gtcu_iface_clk");

	if (mmu->secured) {
		kgsl_regwrite(device, A4XX_RBBM_SECVID_TSB_CONTROL, 0x0);
		kgsl_regwrite(device, A4XX_RBBM_SECVID_TSB_TRUSTED_BASE,
					  KGSL_IOMMU_SECURE_MEM_BASE);
		kgsl_regwrite(device, A4XX_RBBM_SECVID_TSB_TRUSTED_SIZE,
					  KGSL_IOMMU_SECURE_MEM_SIZE);
		secured_pool_sz = KGSL_IOMMU_SECURE_MEM_SIZE;
	}

	mmu->pt_base = KGSL_MMU_MAPPED_MEM_BASE;
	mmu->pt_size = (KGSL_MMU_MAPPED_MEM_SIZE - secured_pool_sz);

	status = kgsl_iommu_init_sync_lock(mmu);
	if (status)
		goto done;

	if (kgsl_msm_supports_iommu_v2()) {
		iommu->iommu_reg_list = kgsl_iommuv1_reg;
		iommu->ctx_offset = KGSL_IOMMU_CTX_OFFSET_V2;
		iommu->ctx_ahb_offset = KGSL_IOMMU_CTX_AHB_OFFSET_V2;
	} else if (msm_soc_version_supports_iommu_v0()) {
		iommu->iommu_reg_list = kgsl_iommuv0_reg;
		iommu->ctx_offset = KGSL_IOMMU_CTX_OFFSET_V0;
		iommu->ctx_ahb_offset = KGSL_IOMMU_CTX_OFFSET_V0;
	} else {
		iommu->iommu_reg_list = kgsl_iommuv1_reg;
		iommu->ctx_offset = KGSL_IOMMU_CTX_OFFSET_V1;
		iommu->ctx_ahb_offset = KGSL_IOMMU_CTX_OFFSET_V1;
	}

	/* A nop is required in an indirect buffer when switching
	 * pagetables in-stream */
	kgsl_sharedmem_writel(mmu->device, &mmu->setstate_memory,
				KGSL_IOMMU_SETSTATE_NOP_OFFSET,
				cp_nop_packet(1));

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
		if (iommu)
			kgsl_del_event_group(&iommu->events);
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

	/* If chip is not 8960 then we use the 2nd context bank for pagetable
	 * switching on the 3D side for which a separate table is allocated */
	if (msm_soc_version_supports_iommu_v0()) {
		mmu->priv_bank_table =
			kgsl_mmu_getpagetable(mmu, KGSL_MMU_PRIV_PT);
		if (mmu->priv_bank_table == NULL) {
			status = -ENOMEM;
			goto err;
		}
	}
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
	if (mmu->priv_bank_table) {
		kgsl_mmu_putpagetable(mmu->priv_bank_table);
		mmu->priv_bank_table = NULL;
	}
	if (mmu->defaultpagetable) {
		kgsl_mmu_putpagetable(mmu->defaultpagetable);
		mmu->defaultpagetable = NULL;
	}
	return status;
}

/*
 * kgsl_iommu_lock_rb_in_tlb - Allocates tlb entries and locks the
 * virtual to physical address translation of ringbuffer for 3D
 * device into tlb.
 * @mmu - Pointer to mmu structure
 *
 * Return - void
 */
static void kgsl_iommu_lock_rb_in_tlb(struct kgsl_mmu *mmu)
{
	struct kgsl_device *device = mmu->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_ringbuffer *rb;
	struct kgsl_iommu *iommu = mmu->priv;
	unsigned int num_tlb_entries;
	unsigned int tlblkcr = 0;
	unsigned int v2pxx = 0;
	unsigned int vaddr = 0;
	int i, j, k, l;

	if (!iommu->sync_lock_initialized)
		return;

	rb = ADRENO_CURRENT_RINGBUFFER(adreno_dev);
	num_tlb_entries = rb->buffer_desc.size / PAGE_SIZE;

	for (i = 0; i < iommu->unit_count; i++) {
		struct kgsl_iommu_unit *iommu_unit = &iommu->iommu_units[i];
		for (j = 0; j < iommu_unit->dev_count; j++) {
			tlblkcr = 0;
			tlblkcr |= (((num_tlb_entries *
				iommu_unit->dev_count) &
				KGSL_IOMMU_TLBLKCR_FLOOR_MASK) <<
				KGSL_IOMMU_TLBLKCR_FLOOR_SHIFT);
			/* Do not invalidate locked entries on tlbiall flush */
			tlblkcr	|= ((1 & KGSL_IOMMU_TLBLKCR_TLBIALLCFG_MASK)
				<< KGSL_IOMMU_TLBLKCR_TLBIALLCFG_SHIFT);
			tlblkcr	|= ((1 & KGSL_IOMMU_TLBLKCR_TLBIASIDCFG_MASK)
				<< KGSL_IOMMU_TLBLKCR_TLBIASIDCFG_SHIFT);
			tlblkcr	|= ((1 & KGSL_IOMMU_TLBLKCR_TLBIVAACFG_MASK)
				<< KGSL_IOMMU_TLBLKCR_TLBIVAACFG_SHIFT);
			/* Enable tlb locking */
			tlblkcr |= ((1 & KGSL_IOMMU_TLBLKCR_LKE_MASK)
				<< KGSL_IOMMU_TLBLKCR_LKE_SHIFT);
			KGSL_IOMMU_SET_CTX_REG(iommu, iommu_unit,
					iommu_unit->dev[j].ctx_id,
					TLBLKCR, tlblkcr);
		}
		for (j = 0; j < iommu_unit->dev_count; j++) {
			/* Lock the ringbuffer virtual address into tlb */
			vaddr = rb->buffer_desc.gpuaddr;
			for (k = 0; k < num_tlb_entries; k++) {
				v2pxx = 0;
				v2pxx |= (((k + j * num_tlb_entries) &
					KGSL_IOMMU_V2PXX_INDEX_MASK)
					<< KGSL_IOMMU_V2PXX_INDEX_SHIFT);
				v2pxx |= vaddr & (KGSL_IOMMU_V2PXX_VA_MASK <<
						KGSL_IOMMU_V2PXX_VA_SHIFT);

				KGSL_IOMMU_SET_CTX_REG(iommu, iommu_unit,
						iommu_unit->dev[j].ctx_id,
						V2PUR, v2pxx);
				mb();
				vaddr += PAGE_SIZE;
				for (l = 0; l < iommu_unit->dev_count; l++) {
					tlblkcr = KGSL_IOMMU_GET_CTX_REG(iommu,
						iommu_unit,
						iommu_unit->dev[l].ctx_id,
						TLBLKCR);
					mb();
					tlblkcr &=
					~(KGSL_IOMMU_TLBLKCR_VICTIM_MASK
					<< KGSL_IOMMU_TLBLKCR_VICTIM_SHIFT);
					tlblkcr |= (((k + 1 +
					(j * num_tlb_entries)) &
					KGSL_IOMMU_TLBLKCR_VICTIM_MASK) <<
					KGSL_IOMMU_TLBLKCR_VICTIM_SHIFT);
					KGSL_IOMMU_SET_CTX_REG(iommu,
						iommu_unit,
						iommu_unit->dev[l].ctx_id,
						TLBLKCR, tlblkcr);
				}
			}
		}
		for (j = 0; j < iommu_unit->dev_count; j++) {
			tlblkcr = KGSL_IOMMU_GET_CTX_REG(iommu, iommu_unit,
						iommu_unit->dev[j].ctx_id,
						TLBLKCR);
			mb();
			/* Disable tlb locking */
			tlblkcr &= ~(KGSL_IOMMU_TLBLKCR_LKE_MASK
				<< KGSL_IOMMU_TLBLKCR_LKE_SHIFT);
			KGSL_IOMMU_SET_CTX_REG(iommu, iommu_unit,
				iommu_unit->dev[j].ctx_id, TLBLKCR, tlblkcr);
		}
	}
}

static int kgsl_iommu_start(struct kgsl_mmu *mmu)
{
	int status;
	struct kgsl_iommu *iommu = mmu->priv;
	int i, j;
	int sctlr_val = 0;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(mmu->device);

	if (mmu->defaultpagetable == NULL) {
		status = kgsl_iommu_setup_defaultpagetable(mmu);
		if (status)
			return -ENOMEM;
	}
	status = kgsl_iommu_start_sync_lock(mmu);
	if (status)
		return status;

	mmu->hwpagetable = mmu->defaultpagetable;

	status = kgsl_attach_pagetable_iommu_domain(mmu);
	if (status) {
		mmu->hwpagetable = NULL;
		goto done;
	}

	kgsl_iommu_enable_clk(mmu, KGSL_IOMMU_MAX_UNITS);

	/* Get the lsb value of pagetables set in the IOMMU ttbr0 register as
	 * that value should not change when we change pagetables, so while
	 * changing pagetables we can use this lsb value of the pagetable w/o
	 * having to read it again
	 */
	_iommu_lock(iommu);
	for (i = 0; i < iommu->unit_count; i++) {
		struct kgsl_iommu_unit *iommu_unit = &iommu->iommu_units[i];
		for (j = 0; j < iommu_unit->dev_count; j++) {

			/*
			 * For IOMMU V1 do not halt IOMMU on pagefault if
			 * FT pagefault policy is set accordingly
			 */
			if ((!msm_soc_version_supports_iommu_v0()) &&
				(!(adreno_dev->ft_pf_policy &
				   KGSL_FT_PAGEFAULT_GPUHALT_ENABLE))) {
				sctlr_val = KGSL_IOMMU_GET_CTX_REG(iommu,
						iommu_unit,
						iommu_unit->dev[j].ctx_id,
						SCTLR);
				sctlr_val |= (0x1 <<
						KGSL_IOMMU_SCTLR_HUPCF_SHIFT);
				KGSL_IOMMU_SET_CTX_REG(iommu,
						iommu_unit,
						iommu_unit->dev[j].ctx_id,
						SCTLR, sctlr_val);
			}
			iommu_unit->dev[j].default_ttbr0 =
				KGSL_IOMMU_GET_CTX_REG_Q(iommu,
					iommu_unit,
					iommu_unit->dev[j].ctx_id, TTBR0);
		}
	}
	kgsl_iommu_lock_rb_in_tlb(mmu);
	_iommu_unlock(iommu);

	/* For complete CFF */
	kgsl_cffdump_write(mmu->device, mmu->setstate_memory.gpuaddr +
				KGSL_IOMMU_SETSTATE_NOP_OFFSET,
				cp_nop_packet(1));

	kgsl_iommu_disable_clk(mmu, KGSL_IOMMU_MAX_UNITS);

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
	unsigned int flush_flags = KGSL_MMUFLAGS_TLBFLUSH;

	flush_flags |= ((kgsl_memdesc_is_secured(memdesc) ?
			KGSL_MMUFLAGS_TLBFLUSH_SECURE : 0));

	mutex_lock(&pt->mmu->device->mutex);
	/*
	 * Flush the tlb only if the iommu device is attached and the pagetable
	 * hasn't been switched yet
	 */
	if (kgsl_mmu_is_perprocess(pt->mmu) &&
		iommu->iommu_units[0].dev[KGSL_IOMMU_CONTEXT_USER].attached &&
		kgsl_iommu_pt_equal(pt->mmu, pt,
		kgsl_iommu_get_current_ptbase(pt->mmu)))
		kgsl_iommu_default_setstate(pt->mmu, flush_flags);
	mutex_unlock(&pt->mmu->device->mutex);
}

static int
kgsl_iommu_unmap(struct kgsl_pagetable *pt,
		struct kgsl_memdesc *memdesc,
		unsigned int *tlb_flags)
{
	int ret = 0;
	unsigned int range = memdesc->size;
	struct kgsl_iommu_pt *iommu_pt = pt->priv;

	/* All GPU addresses as assigned are page aligned, but some
	   functions purturb the gpuaddr with an offset, so apply the
	   mask here to make sure we have the right address */

	unsigned int gpuaddr = PAGE_ALIGN(memdesc->gpuaddr);

	if (range == 0 || gpuaddr == 0)
		return 0;

	if (kgsl_memdesc_has_guard_page(memdesc))
		range += PAGE_SIZE;

	ret = iommu_unmap_range(iommu_pt->domain, gpuaddr, range);
	if (ret) {
		KGSL_CORE_ERR("iommu_unmap_range(%p, %x, %d) failed "
			"with err: %d\n", iommu_pt->domain, gpuaddr,
			range, ret);
		return ret;
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

static int
kgsl_iommu_map(struct kgsl_pagetable *pt,
			struct kgsl_memdesc *memdesc,
			unsigned int *tlb_flags)
{
	int ret;
	unsigned int iommu_virt_addr;
	struct kgsl_iommu_pt *iommu_pt = pt->priv;
	size_t size = memdesc->size;
	unsigned int protflags;
	struct kgsl_device *device = pt->mmu->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	BUG_ON(NULL == iommu_pt);

	iommu_virt_addr = memdesc->gpuaddr;

	/* Set up the protection for the page(s) */
	protflags = IOMMU_READ;
	if (!(memdesc->flags & KGSL_MEMFLAGS_GPUREADONLY))
		protflags |= IOMMU_WRITE;

	ret = iommu_map_range(iommu_pt->domain, iommu_virt_addr, memdesc->sg,
				size, protflags);
	if (ret) {
		KGSL_CORE_ERR("iommu_map_range(%p, %x, %p, %zd, %x) err: %d\n",
			iommu_pt->domain, iommu_virt_addr, memdesc->sg, size,
			protflags, ret);
		return ret;
	}
	if (kgsl_memdesc_has_guard_page(memdesc)) {
		ret = iommu_map(iommu_pt->domain, iommu_virt_addr + size,
				page_to_phys(kgsl_guard_page), PAGE_SIZE,
				protflags & ~IOMMU_WRITE);
		if (ret) {
			KGSL_CORE_ERR("iommu_map(%p, %zx, guard, %x) err: %d\n",
				iommu_pt->domain, iommu_virt_addr + size,
				protflags & ~IOMMU_WRITE,
				ret);
			/* cleanup the partial mapping */
			iommu_unmap_range(iommu_pt->domain, iommu_virt_addr,
					  size);
		}
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

	if (ADRENO_FEATURE(adreno_dev, IOMMU_FLUSH_TLB_ON_MAP)
		&& !kgsl_memdesc_is_global(memdesc))
		kgsl_iommu_flush_tlb_pt_current(pt, memdesc);

	return ret;
}

static void kgsl_iommu_pagefault_resume(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = mmu->priv;
	int i, j;

	if (atomic_read(&mmu->fault)) {
		kgsl_iommu_enable_clk(mmu, KGSL_IOMMU_MAX_UNITS);
		for (i = 0; i < iommu->unit_count; i++) {
			struct kgsl_iommu_unit *iommu_unit =
						&iommu->iommu_units[i];
			for (j = 0; j < iommu_unit->dev_count; j++) {
				if (iommu_unit->dev[j].fault) {
					_iommu_lock(iommu);
					KGSL_IOMMU_SET_CTX_REG(iommu,
						iommu_unit,
						iommu_unit->dev[j].ctx_id,
						RESUME, 1);
					KGSL_IOMMU_SET_CTX_REG(iommu,
						iommu_unit,
						iommu_unit->dev[j].ctx_id,
						FSR, 0);
					_iommu_unlock(iommu);
					iommu_unit->dev[j].fault = 0;
				}
			}
		}
		kgsl_iommu_disable_clk(mmu, KGSL_IOMMU_MAX_UNITS);
		atomic_set(&mmu->fault, 0);
	}
}


static void kgsl_iommu_stop(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = mmu->priv;
	/*
	 *  stop device mmu
	 *
	 *  call this with the global lock held
	 *  detach iommu attachment
	 */
	kgsl_detach_pagetable_iommu_domain(mmu);
	mmu->hwpagetable = NULL;

	kgsl_iommu_pagefault_resume(mmu);
	kgsl_cancel_events(mmu->device, &iommu->events);
}

static int kgsl_iommu_close(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = mmu->priv;
	int i;

	if (mmu->priv_bank_table != NULL)
		kgsl_mmu_putpagetable(mmu->priv_bank_table);

	if (mmu->defaultpagetable != NULL)
		kgsl_mmu_putpagetable(mmu->defaultpagetable);

	for (i = 0; i < iommu->unit_count; i++) {
		struct kgsl_memdesc *reg_map = &iommu->iommu_units[i].reg_map;

		/* Remove the reg_map from the global list */
		kgsl_remove_global_pt_entry(reg_map);

		if (reg_map->hostptr)
			iounmap(reg_map->hostptr);
		kgsl_free(reg_map->sg);
		reg_map->priv &= ~KGSL_MEMDESC_GLOBAL;
	}
	/* clear IOMMU GPU CPU sync structures */

	kgsl_remove_global_pt_entry(&iommu->sync_lock_desc);
	kgsl_free(iommu->sync_lock_desc.sg);
	memset(&iommu->sync_lock_desc, 0, sizeof(iommu->sync_lock_desc));
	iommu->sync_lock_vars = NULL;

	kgsl_del_event_group(&iommu->events);

	kfree(iommu);

	if (kgsl_guard_page != NULL) {
		__free_page(kgsl_guard_page);
		kgsl_guard_page = NULL;
	}

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
	kgsl_iommu_enable_clk(mmu, KGSL_IOMMU_MAX_UNITS);
	pt_base = KGSL_IOMMU_GET_CTX_REG_Q(iommu,
				(&iommu->iommu_units[0]),
				KGSL_IOMMU_CONTEXT_USER, TTBR0);
	kgsl_iommu_disable_clk(mmu, KGSL_IOMMU_MAX_UNITS);
	return pt_base & KGSL_IOMMU_CTX_TTBR0_ADDR_MASK;
}

/*
 * kgsl_iommu_default_setstate - Change the IOMMU pagetable or flush IOMMU tlb
 * of the primary context bank
 * @mmu - Pointer to mmu structure
 * @flags - Flags indicating whether pagetable has to chnage or tlb is to be
 * flushed or both
 *
 * Based on flags set the new pagetable fo the IOMMU unit or flush it's tlb or
 * do both by doing direct register writes to the IOMMu registers through the
 * cpu
 * Return - void
 */
static int kgsl_iommu_default_setstate(struct kgsl_mmu *mmu,
					uint32_t flags)
{
	struct kgsl_iommu *iommu = mmu->priv;
	int temp;
	int i;
	int ret = 0;
	phys_addr_t pt_base = kgsl_iommu_get_pt_base_addr(mmu,
						mmu->hwpagetable);
	uint64_t pt_val;

	/* we must hold the mutex here to idle the GPU, and to write regs */
	BUG_ON(!mutex_is_locked(&mmu->device->mutex));

	kgsl_iommu_enable_clk(mmu, KGSL_IOMMU_MAX_UNITS);

	/* For v0 SMMU GPU needs to be idle for tlb invalidate as well */
	if (msm_soc_version_supports_iommu_v0()) {
		ret = kgsl_idle(mmu->device);
		if (ret)
			return ret;
	}

	/* Acquire GPU-CPU sync Lock here */
	_iommu_lock(iommu);

	if (flags & KGSL_MMUFLAGS_PTUPDATE) {
		if (!msm_soc_version_supports_iommu_v0()) {
			ret = kgsl_idle(mmu->device);
			if (ret)
				goto unlock;
		}
		for (i = 0; i < iommu->unit_count; i++) {
			/* get the lsb value which should not change when
			 * changing ttbr0 */
			pt_val = kgsl_iommu_get_default_ttbr0(mmu, i,
						KGSL_IOMMU_CONTEXT_USER);

			pt_base &= KGSL_IOMMU_CTX_TTBR0_ADDR_MASK;
			pt_val &= ~KGSL_IOMMU_CTX_TTBR0_ADDR_MASK;
			pt_val |= pt_base;
			KGSL_IOMMU_SET_CTX_REG_Q(iommu,
					(&iommu->iommu_units[i]),
					KGSL_IOMMU_CONTEXT_USER, TTBR0, pt_val);

			mb();
			temp = KGSL_IOMMU_GET_CTX_REG_Q(iommu,
				(&iommu->iommu_units[i]),
				KGSL_IOMMU_CONTEXT_USER, TTBR0);
		}
	}
	/* Flush tlb */
	if (flags & KGSL_MMUFLAGS_TLBFLUSH) {
		unsigned long wait_for_flush;
		unsigned int tlbflush_ctxt = KGSL_IOMMU_CONTEXT_USER;
		for (i = 0; i < iommu->unit_count; i++) {

			if (flags & KGSL_MMUFLAGS_TLBFLUSH_SECURE)
				tlbflush_ctxt = KGSL_IOMMU_CONTEXT_SECURE;

			KGSL_IOMMU_SET_CTX_REG(iommu, (&iommu->iommu_units[i]),
				tlbflush_ctxt, TLBIALL, 1);
			mb();
			/*
			 * Wait for flush to complete by polling the flush
			 * status bit of TLBSTATUS register for not more than
			 * 2 s. After 2s just exit, at that point the SMMU h/w
			 * may be stuck and will eventually cause GPU to hang
			 * or bring the system down.
			 */
			if (!msm_soc_version_supports_iommu_v0()) {
				wait_for_flush = jiffies +
						msecs_to_jiffies(2000);
				KGSL_IOMMU_SET_CTX_REG(iommu,
					(&iommu->iommu_units[i]),
					tlbflush_ctxt, TLBSYNC, 0);
				while (KGSL_IOMMU_GET_CTX_REG(iommu,
					(&iommu->iommu_units[i]),
					tlbflush_ctxt, TLBSTATUS) &
					(KGSL_IOMMU_CTX_TLBSTATUS_SACTIVE)) {
					if (time_after(jiffies,
						wait_for_flush)) {
						KGSL_DRV_ERR(mmu->device,
						"Wait limit reached for IOMMU tlb flush\n");
						break;
					}
					cpu_relax();
				}
			}
		}
	}
unlock:
	/* Release GPU-CPU sync Lock here */
	_iommu_unlock(iommu);

	/* Disable smmu clock */
	kgsl_iommu_disable_clk(mmu, KGSL_IOMMU_MAX_UNITS);

	return ret;
}

/*
 * kgsl_iommu_get_reg_gpuaddr - Returns the gpu address of IOMMU regsiter
 * @mmu - Pointer to mmu structure
 * @iommu_unit - The iommu unit for which base address is requested
 * @ctx_id - The context ID of the IOMMU ctx
 * @reg - The register for which address is required
 *
 * Return - The gpu address of register which can be used in type3 packet
 */
static unsigned int kgsl_iommu_get_reg_gpuaddr(struct kgsl_mmu *mmu,
					int iommu_unit, int ctx_id, int reg)
{
	struct kgsl_iommu *iommu = mmu->priv;

	if (KGSL_IOMMU_GLOBAL_BASE == reg)
		return iommu->iommu_units[iommu_unit].reg_map.gpuaddr;

	if (iommu->iommu_reg_list[reg].ctx_reg)
		return iommu->iommu_units[iommu_unit].reg_map.gpuaddr +
			iommu->iommu_reg_list[reg].reg_offset +
			(ctx_id << KGSL_IOMMU_CTX_SHIFT) + iommu->ctx_offset;
	else
		return iommu->iommu_units[iommu_unit].reg_map.gpuaddr +
			iommu->iommu_reg_list[reg].reg_offset;
}
/*
 * kgsl_iommu_hw_halt_supported - Returns whether IOMMU halt command is
 * supported
 * @mmu - Pointer to mmu structure
 * @iommu_unit - The iommu unit for which the property is requested
 */
static int kgsl_iommu_hw_halt_supported(struct kgsl_mmu *mmu, int iommu_unit)
{
	struct kgsl_iommu *iommu = mmu->priv;
	return iommu->iommu_units[iommu_unit].iommu_halt_enable;
}

static int kgsl_iommu_get_num_iommu_units(struct kgsl_mmu *mmu)
{
	struct kgsl_iommu *iommu = mmu->priv;
	return iommu->unit_count;
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
				unsigned int pf_policy)
{
	int i, j;
	struct kgsl_iommu *iommu = mmu->priv;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(mmu->device);
	int ret = 0;
	unsigned int sctlr_val;

	if ((adreno_dev->ft_pf_policy & KGSL_FT_PAGEFAULT_GPUHALT_ENABLE) ==
		(pf_policy & KGSL_FT_PAGEFAULT_GPUHALT_ENABLE))
		return ret;
	if (msm_soc_version_supports_iommu_v0())
		return ret;

	kgsl_iommu_enable_clk(mmu, KGSL_IOMMU_MAX_UNITS);

	/* Need to idle device before changing options */
	ret = mmu->device->ftbl->idle(mmu->device);
	if (ret) {
		kgsl_iommu_disable_clk(mmu, KGSL_IOMMU_MAX_UNITS);
		return ret;
	}

	for (i = 0; i < iommu->unit_count; i++) {
		struct kgsl_iommu_unit *iommu_unit = &iommu->iommu_units[i];
		for (j = 0; j < iommu_unit->dev_count; j++) {
			sctlr_val = KGSL_IOMMU_GET_CTX_REG(iommu,
					iommu_unit,
					iommu_unit->dev[j].ctx_id,
					SCTLR);
			if (pf_policy & KGSL_FT_PAGEFAULT_GPUHALT_ENABLE)
				sctlr_val &= ~(0x1 <<
					KGSL_IOMMU_SCTLR_HUPCF_SHIFT);
			else
				sctlr_val |= (0x1 <<
					KGSL_IOMMU_SCTLR_HUPCF_SHIFT);
			KGSL_IOMMU_SET_CTX_REG(iommu,
					iommu_unit,
					iommu_unit->dev[j].ctx_id,
					SCTLR, sctlr_val);
		}
	}

	kgsl_iommu_disable_clk(mmu, KGSL_IOMMU_MAX_UNITS);
	return ret;
}

/**
 * kgsl_iommu_set_pagefault() - Checks if a IOMMU device has faulted
 * @mmu: MMU pointer of the device
 *
 * This function is called to set the pagefault bits for the device so
 * that recovery can run with pagefault in consideration
 */
static void kgsl_iommu_set_pagefault(struct kgsl_mmu *mmu)
{
	int i, j;
	struct kgsl_iommu *iommu = mmu->priv;
	unsigned int fsr;

	/* fault already detected then return early */
	if (atomic_read(&mmu->fault))
		return;

	kgsl_iommu_enable_clk(mmu, KGSL_IOMMU_MAX_UNITS);

	/* Loop through all IOMMU devices to check for fault */
	for (i = 0; i < iommu->unit_count; i++) {
		for (j = 0; j < iommu->iommu_units[i].dev_count; j++) {
			fsr = KGSL_IOMMU_GET_CTX_REG(iommu,
				(&(iommu->iommu_units[i])),
				iommu->iommu_units[i].dev[j].ctx_id, FSR);
			if (fsr) {
				uint64_t far =
					KGSL_IOMMU_GET_CTX_REG_Q(iommu,
					(&(iommu->iommu_units[i])),
					iommu->iommu_units[i].dev[j].ctx_id,
					FAR);
				kgsl_iommu_fault_handler(NULL,
				iommu->iommu_units[i].dev[j].dev, far, 0, NULL);
				break;
			}
		}
	}

	kgsl_iommu_disable_clk(mmu, KGSL_IOMMU_MAX_UNITS);
}

struct kgsl_protected_registers *kgsl_iommu_get_prot_regs(struct kgsl_mmu *mmu)
{
	static struct kgsl_protected_registers iommuv1_regs = { 0x4000, 14 };
	static struct kgsl_protected_registers iommuv2_regs;

	if (msm_soc_version_supports_iommu_v0())
		return NULL;
	if (kgsl_msm_supports_iommu_v2()) {

		struct kgsl_iommu *iommu = mmu->priv;

		/* For V2 there is only one instance of iommu */
		iommuv2_regs.base = iommu->iommu_units[0].ahb_base >> 2;
		iommuv2_regs.range = 10;
		return &iommuv2_regs;
	}
	else
		return &iommuv1_regs;
}

struct kgsl_mmu_ops iommu_ops = {
	.mmu_init = kgsl_iommu_init,
	.mmu_close = kgsl_iommu_close,
	.mmu_start = kgsl_iommu_start,
	.mmu_stop = kgsl_iommu_stop,
	.mmu_setstate = kgsl_iommu_setstate,
	.mmu_device_setstate = kgsl_iommu_default_setstate,
	.mmu_pagefault_resume = kgsl_iommu_pagefault_resume,
	.mmu_get_current_ptbase = kgsl_iommu_get_current_ptbase,
	.mmu_enable_clk = kgsl_iommu_enable_clk,
	.mmu_disable_clk = kgsl_iommu_disable_clk,
	.mmu_disable_clk_on_ts = kgsl_iommu_disable_clk_on_ts,
	.mmu_get_default_ttbr0 = kgsl_iommu_get_default_ttbr0,
	.mmu_get_reg_gpuaddr = kgsl_iommu_get_reg_gpuaddr,
	.mmu_get_reg_ahbaddr = kgsl_iommu_get_reg_ahbaddr,
	.mmu_get_num_iommu_units = kgsl_iommu_get_num_iommu_units,
	.mmu_pt_equal = kgsl_iommu_pt_equal,
	.mmu_get_pt_base_addr = kgsl_iommu_get_pt_base_addr,
	.mmu_hw_halt_supported = kgsl_iommu_hw_halt_supported,
	/* These callbacks will be set on some chipsets */
	.mmu_sync_lock = kgsl_iommu_sync_lock,
	.mmu_sync_unlock = kgsl_iommu_sync_unlock,
	.mmu_set_pf_policy = kgsl_iommu_set_pf_policy,
	.mmu_set_pagefault = kgsl_iommu_set_pagefault,
	.mmu_get_prot_regs = kgsl_iommu_get_prot_regs
};

struct kgsl_mmu_pt_ops iommu_pt_ops = {
	.mmu_map = kgsl_iommu_map,
	.mmu_unmap = kgsl_iommu_unmap,
	.mmu_create_pagetable = kgsl_iommu_create_pagetable,
	.mmu_destroy_pagetable = kgsl_iommu_destroy_pagetable,
	.get_ptbase = kgsl_iommu_get_ptbase,
};
