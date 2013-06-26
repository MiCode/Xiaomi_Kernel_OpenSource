/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/export.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/printk.h>
#include <linux/notifier.h>

#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <mach/msm_smem.h>
#include <mach/ramdump.h>
#include <mach/subsystem_notif.h>

#include "smem_private.h"

/**
 * OVERFLOW_ADD_UNSIGNED() - check for unsigned overflow
 *
 * @type: type to check for overflow
 * @a: left value to use
 * @b: right value to use
 * @returns: true if a + b will result in overflow; false otherwise
 */
#define OVERFLOW_ADD_UNSIGNED(type, a, b) \
	(((type)~0 - (a)) < (b) ? true : false)

#define MODEM_SBL_VERSION_INDEX 7
#define SMEM_VERSION_INFO_SIZE (32 * 4)
#define SMEM_VERSION 0x000B

enum {
	MSM_SMEM_DEBUG = 1U << 0,
	MSM_SMEM_INFO = 1U << 1,
};

static int msm_smem_debug_mask;
module_param_named(debug_mask, msm_smem_debug_mask,
			int, S_IRUGO | S_IWUSR | S_IWGRP);

#define SMEM_DBG(x...) do {                               \
		if (msm_smem_debug_mask & MSM_SMEM_DEBUG) \
			pr_debug(x);                      \
	} while (0)

#define SMEM_SPINLOCK_SMEM_ALLOC       "S:3"

static remote_spinlock_t remote_spinlock;
static uint32_t num_smem_areas;
static struct smem_area *smem_areas;
static struct ramdump_segment *smem_ramdump_segments;
static int spinlocks_initialized;
static void *smem_ramdump_dev;
static DEFINE_MUTEX(spinlock_init_lock);
static DEFINE_SPINLOCK(smem_init_check_lock);
static int smem_module_inited;
static RAW_NOTIFIER_HEAD(smem_module_init_notifier_list);
static DEFINE_MUTEX(smem_module_init_notifier_lock);


struct restart_notifier_block {
	unsigned processor;
	char *name;
	struct notifier_block nb;
};

static int restart_notifier_cb(struct notifier_block *this,
				unsigned long code,
				void *data);

static struct restart_notifier_block restart_notifiers[] = {
	{SMEM_MODEM, "modem", .nb.notifier_call = restart_notifier_cb},
	{SMEM_Q6, "lpass", .nb.notifier_call = restart_notifier_cb},
	{SMEM_WCNSS, "wcnss", .nb.notifier_call = restart_notifier_cb},
	{SMEM_DSPS, "dsps", .nb.notifier_call = restart_notifier_cb},
	{SMEM_MODEM, "gss", .nb.notifier_call = restart_notifier_cb},
	{SMEM_Q6, "adsp", .nb.notifier_call = restart_notifier_cb},
};

static int init_smem_remote_spinlock(void);

/**
 * smem_phys_to_virt() - Convert a physical base and offset to virtual address
 *
 * @base: physical base address to check
 * @offset: offset from the base to get the final address
 * @returns: virtual SMEM address; NULL for failure
 *
 * Takes a physical address and an offset and checks if the resulting physical
 * address would fit into one of the smem regions.  If so, returns the
 * corresponding virtual address.  Otherwise returns NULL.
 */
static void *smem_phys_to_virt(phys_addr_t base, unsigned offset)
{
	int i;
	phys_addr_t phys_addr;
	resource_size_t size;

	if (OVERFLOW_ADD_UNSIGNED(phys_addr_t, base, offset))
		return NULL;

	if (!smem_areas) {
		/*
		 * Early boot - no area configuration yet, so default
		 * to using the main memory region.
		 *
		 * To remove the MSM_SHARED_RAM_BASE and the static
		 * mapping of SMEM in the future, add dump_stack()
		 * to identify the early callers of smem_get_entry()
		 * (which calls this function) and replace those calls
		 * with a new function that knows how to lookup the
		 * SMEM base address before SMEM has been probed.
		 */
		phys_addr = msm_shared_ram_phys;
		size = MSM_SHARED_RAM_SIZE;

		if (base >= phys_addr && base + offset < phys_addr + size) {
			if (OVERFLOW_ADD_UNSIGNED(uintptr_t,
				(uintptr_t)MSM_SHARED_RAM_BASE, offset)) {
				pr_err("%s: overflow %p %x\n", __func__,
					MSM_SHARED_RAM_BASE, offset);
				return NULL;
			}

			return MSM_SHARED_RAM_BASE + offset;
		} else {
			return NULL;
		}
	}
	for (i = 0; i < num_smem_areas; ++i) {
		phys_addr = smem_areas[i].phys_addr;
		size = smem_areas[i].size;

		if (base < phys_addr || base + offset >= phys_addr + size)
			continue;

		if (OVERFLOW_ADD_UNSIGNED(uintptr_t,
				(uintptr_t)smem_areas[i].virt_addr, offset)) {
			pr_err("%s: overflow %p %x\n", __func__,
				smem_areas[i].virt_addr, offset);
			return NULL;
		}

		return smem_areas[i].virt_addr + offset;
	}

	return NULL;
}

/**
 * smem_virt_to_phys() - Convert SMEM address to physical address.
 *
 * @smem_address: Address of SMEM item (returned by smem_alloc(), etc)
 * @returns: Physical address (or NULL if there is a failure)
 *
 * This function should only be used if an SMEM item needs to be handed
 * off to a DMA engine.
 */
phys_addr_t smem_virt_to_phys(void *smem_address)
{
	phys_addr_t phys_addr = 0;
	int i;
	void *vend;

	if (!smem_areas)
		return phys_addr;

	for (i = 0; i < num_smem_areas; ++i) {
		vend = (void *)(smem_areas[i].virt_addr + smem_areas[i].size);

		if (smem_address >= smem_areas[i].virt_addr &&
				smem_address < vend) {
			phys_addr = smem_address - smem_areas[i].virt_addr;
			phys_addr +=  smem_areas[i].phys_addr;
			break;
		}
	}

	return phys_addr;
}
EXPORT_SYMBOL(smem_virt_to_phys);

/* smem_alloc returns the pointer to smem item if it is already allocated.
 * Otherwise, it returns NULL.
 */
void *smem_alloc(unsigned id, unsigned size)
{
	return smem_find(id, size);
}
EXPORT_SYMBOL(smem_alloc);

/**
 * __smem_get_entry - Get pointer and size of existing SMEM item
 *
 * @id:              ID of SMEM item
 * @size:            Pointer to size variable for storing the result
 * @skip_init_check: True means do not verify that SMEM has been initialized
 * @use_rspinlock:   True to use the remote spinlock
 * @returns:         Pointer to SMEM item or NULL if it doesn't exist
 */
static void *__smem_get_entry(unsigned id, unsigned *size,
		bool skip_init_check, bool use_rspinlock)
{
	struct smem_shared *shared = (void *) MSM_SHARED_RAM_BASE;
	struct smem_heap_entry *toc = shared->heap_toc;
	int use_spinlocks = spinlocks_initialized && use_rspinlock;
	void *ret = 0;
	unsigned long flags = 0;

	if (!skip_init_check && !smem_initialized_check())
		return ret;

	if (id >= SMEM_NUM_ITEMS)
		return ret;

	if (use_spinlocks)
		remote_spin_lock_irqsave(&remote_spinlock, flags);
	/* toc is in device memory and cannot be speculatively accessed */
	if (toc[id].allocated) {
		phys_addr_t phys_base;

		*size = toc[id].size;
		barrier();

		phys_base = toc[id].reserved & BASE_ADDR_MASK;
		if (!phys_base)
			phys_base = (phys_addr_t)msm_shared_ram_phys;
		ret = smem_phys_to_virt(phys_base, toc[id].offset);
	} else {
		*size = 0;
	}
	if (use_spinlocks)
		remote_spin_unlock_irqrestore(&remote_spinlock, flags);

	return ret;
}

static void *__smem_find(unsigned id, unsigned size_in, bool skip_init_check)
{
	unsigned size;
	void *ptr;

	ptr = __smem_get_entry(id, &size, skip_init_check, true);
	if (!ptr)
		return 0;

	size_in = ALIGN(size_in, 8);
	if (size_in != size) {
		pr_err("smem_find(%d, %d): wrong size %d\n",
			id, size_in, size);
		return 0;
	}

	return ptr;
}

void *smem_find(unsigned id, unsigned size_in)
{
	return __smem_find(id, size_in, false);
}
EXPORT_SYMBOL(smem_find);

/* smem_alloc2 returns the pointer to smem item.  If it is not allocated,
 * it allocates it and then returns the pointer to it.
 */
void *smem_alloc2(unsigned id, unsigned size_in)
{
	struct smem_shared *shared = (void *) MSM_SHARED_RAM_BASE;
	struct smem_heap_entry *toc = shared->heap_toc;
	unsigned long flags;
	void *ret = NULL;
	int rc;

	if (!smem_initialized_check())
		return NULL;

	if (id >= SMEM_NUM_ITEMS)
		return NULL;

	if (unlikely(!spinlocks_initialized)) {
		rc = init_smem_remote_spinlock();
		if (unlikely(rc)) {
			pr_err("%s: remote spinlock init failed %d\n",
								__func__, rc);
			return NULL;
		}
	}

	size_in = ALIGN(size_in, 8);
	remote_spin_lock_irqsave(&remote_spinlock, flags);
	if (toc[id].allocated) {
		SMEM_DBG("%s: %u already allocated\n", __func__, id);
		if (size_in != toc[id].size)
			pr_err("%s: wrong size %u (expected %u)\n",
				__func__, toc[id].size, size_in);
		else
			ret = (void *)(MSM_SHARED_RAM_BASE + toc[id].offset);
	} else if (id > SMEM_FIXED_ITEM_LAST) {
		SMEM_DBG("%s: allocating %u\n", __func__, id);
		if (shared->heap_info.heap_remaining >= size_in) {
			toc[id].offset = shared->heap_info.free_offset;
			toc[id].size = size_in;
			wmb();
			toc[id].allocated = 1;

			shared->heap_info.free_offset += size_in;
			shared->heap_info.heap_remaining -= size_in;
			ret = (void *)(MSM_SHARED_RAM_BASE + toc[id].offset);
		} else
			pr_err("%s: not enough memory %u (required %u)\n",
				__func__, shared->heap_info.heap_remaining,
				size_in);
	}
	wmb();
	remote_spin_unlock_irqrestore(&remote_spinlock, flags);
	return ret;
}
EXPORT_SYMBOL(smem_alloc2);

void *smem_get_entry(unsigned id, unsigned *size)
{
	return __smem_get_entry(id, size, false, true);
}
EXPORT_SYMBOL(smem_get_entry);

/**
 * smem_get_entry_no_rlock - Get existing item without using remote spinlock
 *
 * @id:       ID of SMEM item
 * @size_out: Pointer to size variable for storing the result
 * @returns:  Pointer to SMEM item or NULL if it doesn't exist
 *
 * This function does not lock the remote spinlock and should only be used in
 * failure-recover cases such as retrieving the subsystem failure reason during
 * subsystem restart.
 */
void *smem_get_entry_no_rlock(unsigned id, unsigned *size_out)
{
	return __smem_get_entry(id, size_out, false, false);
}
EXPORT_SYMBOL(smem_get_entry_no_rlock);

/**
 * smem_get_remote_spinlock - Remote spinlock pointer for unit testing.
 *
 * @returns: pointer to SMEM remote spinlock
 */
remote_spinlock_t *smem_get_remote_spinlock(void)
{
	if (unlikely(!spinlocks_initialized))
		init_smem_remote_spinlock();
	return &remote_spinlock;
}
EXPORT_SYMBOL(smem_get_remote_spinlock);

/**
 * init_smem_remote_spinlock - Reentrant remote spinlock initialization
 *
 * @returns: sucess or error code for failure
 */
static int init_smem_remote_spinlock(void)
{
	int rc = 0;

	/*
	 * Optimistic locking.  Init only needs to be done once by the first
	 * caller.  After that, serializing inits between different callers
	 * is unnecessary.  The second check after the lock ensures init
	 * wasn't previously completed by someone else before the lock could
	 * be grabbed.
	 */
	if (!spinlocks_initialized) {
		mutex_lock(&spinlock_init_lock);
		if (!spinlocks_initialized) {
			rc = remote_spin_lock_init(&remote_spinlock,
						SMEM_SPINLOCK_SMEM_ALLOC);
			if (!rc)
				spinlocks_initialized = 1;
		}
		mutex_unlock(&spinlock_init_lock);
	}
	return rc;
}

/**
 * smem_initialized_check - Reentrant check that smem has been initialized
 *
 * @returns: true if initialized, false if not.
 */
bool smem_initialized_check(void)
{
	static int checked;
	static int is_inited;
	unsigned long flags;
	struct smem_shared *smem;
	int *version_array;

	if (likely(checked)) {
		if (unlikely(!is_inited))
			pr_err("%s: smem not initialized\n", __func__);
		return is_inited;
	}

	spin_lock_irqsave(&smem_init_check_lock, flags);
	if (checked) {
		spin_unlock_irqrestore(&smem_init_check_lock, flags);
		if (unlikely(!is_inited))
			pr_err("%s: smem not initialized\n", __func__);
		return is_inited;
	}

	smem = (void *)MSM_SHARED_RAM_BASE;

	if (smem->heap_info.initialized != 1)
		goto failed;
	if (smem->heap_info.reserved != 0)
		goto failed;

	version_array = __smem_find(SMEM_VERSION_INFO, SMEM_VERSION_INFO_SIZE,
									true);
	if (version_array == NULL)
		goto failed;
	if (version_array[MODEM_SBL_VERSION_INDEX] != SMEM_VERSION << 16)
		goto failed;

	is_inited = 1;
	checked = 1;
	spin_unlock_irqrestore(&smem_init_check_lock, flags);
	return is_inited;

failed:
	is_inited = 0;
	checked = 1;
	spin_unlock_irqrestore(&smem_init_check_lock, flags);
	pr_err("%s: bootloader failure detected, shared memory not inited\n",
								__func__);
	return is_inited;
}
EXPORT_SYMBOL(smem_initialized_check);

static int restart_notifier_cb(struct notifier_block *this,
				unsigned long code,
				void *data)
{
	if (code == SUBSYS_AFTER_SHUTDOWN) {
		struct restart_notifier_block *notifier;

		notifier = container_of(this,
					struct restart_notifier_block, nb);
		SMEM_DBG("%s: ssrestart for processor %d ('%s')\n",
				__func__, notifier->processor,
				notifier->name);

		remote_spin_release(&remote_spinlock, notifier->processor);
		remote_spin_release_all(notifier->processor);

		if (smem_ramdump_dev) {
			int ret;

			SMEM_DBG("%s: saving ramdump\n", __func__);
			/*
			 * XPU protection does not currently allow the
			 * auxiliary memory regions to be dumped.  If this
			 * changes, then num_smem_areas + 1 should be passed
			 * into do_elf_ramdump() to dump all regions.
			 */
			ret = do_elf_ramdump(smem_ramdump_dev,
					smem_ramdump_segments, 1);
			if (ret < 0)
				pr_err("%s: unable to dump smem %d\n", __func__,
						ret);
		}
	}

	return NOTIFY_DONE;
}

static __init int modem_restart_late_init(void)
{
	int i;
	void *handle;
	struct restart_notifier_block *nb;

	smem_ramdump_dev = create_ramdump_device("smem", NULL);
	if (IS_ERR_OR_NULL(smem_ramdump_dev)) {
		pr_err("%s: Unable to create smem ramdump device.\n",
			__func__);
		smem_ramdump_dev = NULL;
	}

	for (i = 0; i < ARRAY_SIZE(restart_notifiers); i++) {
		nb = &restart_notifiers[i];
		handle = subsys_notif_register_notifier(nb->name, &nb->nb);
		SMEM_DBG("%s: registering notif for '%s', handle=%p\n",
				__func__, nb->name, handle);
	}

	return 0;
}
late_initcall(modem_restart_late_init);

int smem_module_init_notifier_register(struct notifier_block *nb)
{
	int ret;
	if (!nb)
		return -EINVAL;
	mutex_lock(&smem_module_init_notifier_lock);
	ret = raw_notifier_chain_register(&smem_module_init_notifier_list, nb);
	if (smem_module_inited)
		nb->notifier_call(nb, 0, NULL);
	mutex_unlock(&smem_module_init_notifier_lock);
	return ret;
}
EXPORT_SYMBOL(smem_module_init_notifier_register);

int smem_module_init_notifier_unregister(struct notifier_block *nb)
{
	int ret;
	if (!nb)
		return -EINVAL;
	mutex_lock(&smem_module_init_notifier_lock);
	ret = raw_notifier_chain_unregister(&smem_module_init_notifier_list,
						nb);
	mutex_unlock(&smem_module_init_notifier_lock);
	return ret;
}
EXPORT_SYMBOL(smem_module_init_notifier_unregister);

static void smem_module_init_notify(uint32_t state, void *data)
{
	mutex_lock(&smem_module_init_notifier_lock);
	smem_module_inited = 1;
	raw_notifier_call_chain(&smem_module_init_notifier_list,
					state, data);
	mutex_unlock(&smem_module_init_notifier_lock);
}

static int msm_smem_probe(struct platform_device *pdev)
{
	char *key;
	struct resource *r;
	phys_addr_t aux_mem_base;
	resource_size_t aux_mem_size;
	int temp_string_size = 11; /* max 3 digit count */
	char temp_string[temp_string_size];
	int ret;
	struct ramdump_segment *ramdump_segments_tmp = NULL;
	struct smem_area *smem_areas_tmp = NULL;
	int smem_idx = 0;

	if (!smem_initialized_check())
		return -ENODEV;

	key = "irq-reg-base";
	r = platform_get_resource_byname(pdev, IORESOURCE_MEM, key);
	if (!r) {
		pr_err("%s: missing '%s'\n", __func__, key);
		return -ENODEV;
	}

	num_smem_areas = 1;
	while (1) {
		scnprintf(temp_string, temp_string_size, "aux-mem%d",
				num_smem_areas);
		r = platform_get_resource_byname(pdev, IORESOURCE_MEM,
								temp_string);
		if (!r)
			break;

		++num_smem_areas;
		if (num_smem_areas > 999) {
			pr_err("%s: max num aux mem regions reached\n",
								__func__);
			break;
		}
	}
	/* Initialize main SMEM region and SSR ramdump region */
	key = "smem";
	r = platform_get_resource_byname(pdev, IORESOURCE_MEM, key);
	if (!r) {
		pr_err("%s: missing '%s'\n", __func__, key);
		return -ENODEV;
	}

	smem_areas_tmp = kmalloc_array(num_smem_areas, sizeof(struct smem_area),
				GFP_KERNEL);
	if (!smem_areas_tmp) {
		pr_err("%s: smem areas kmalloc failed\n", __func__);
		ret = -ENOMEM;
		goto free_smem_areas;
	}

	ramdump_segments_tmp = kmalloc_array(num_smem_areas,
			sizeof(struct ramdump_segment), GFP_KERNEL);
	if (!ramdump_segments_tmp) {
		pr_err("%s: ramdump segment kmalloc failed\n", __func__);
		ret = -ENOMEM;
		goto free_smem_areas;
	}
	smem_areas_tmp[smem_idx].phys_addr =  r->start;
	smem_areas_tmp[smem_idx].size = resource_size(r);
	smem_areas_tmp[smem_idx].virt_addr = MSM_SHARED_RAM_BASE;

	ramdump_segments_tmp[smem_idx].address = r->start;
	ramdump_segments_tmp[smem_idx].size = resource_size(r);
	++smem_idx;

	/* Configure auxiliary SMEM regions */
	while (1) {
		scnprintf(temp_string, temp_string_size, "aux-mem%d",
								smem_idx);
		r = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							temp_string);
		if (!r)
			break;
		aux_mem_base = r->start;
		aux_mem_size = resource_size(r);

		ramdump_segments_tmp[smem_idx].address = aux_mem_base;
		ramdump_segments_tmp[smem_idx].size = aux_mem_size;

		smem_areas_tmp[smem_idx].phys_addr = aux_mem_base;
		smem_areas_tmp[smem_idx].size = aux_mem_size;
		smem_areas_tmp[smem_idx].virt_addr = ioremap_nocache(
			(unsigned long)(smem_areas_tmp[smem_idx].phys_addr),
			smem_areas_tmp[smem_idx].size);
		SMEM_DBG("%s: %s = %pa %pa -> %p", __func__, temp_string,
				&aux_mem_base, &aux_mem_size,
				smem_areas_tmp[smem_idx].virt_addr);

		if (!smem_areas_tmp[smem_idx].virt_addr) {
			pr_err("%s: ioremap_nocache() of addr:%pa size: %pa\n",
				__func__,
				&smem_areas_tmp[smem_idx].phys_addr,
				&smem_areas_tmp[smem_idx].size);
			ret = -ENOMEM;
			goto free_smem_areas;
		}

		if (OVERFLOW_ADD_UNSIGNED(uintptr_t,
				(uintptr_t)smem_areas_tmp[smem_idx].virt_addr,
				smem_areas_tmp[smem_idx].size)) {
			pr_err("%s: invalid virtual address block %i: %p:%pa\n",
					__func__, smem_idx,
					smem_areas_tmp[smem_idx].virt_addr,
					&smem_areas_tmp[smem_idx].size);
			++smem_idx;
			ret = -EINVAL;
			goto free_smem_areas;
		}

		++smem_idx;
		if (smem_idx > 999) {
			pr_err("%s: max num aux mem regions reached\n",
							__func__);
			break;
		}
	}

	ret = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (ret)
		pr_err("%s: of_platform_populate failed %d\n", __func__, ret);

	smem_areas = smem_areas_tmp;
	smem_ramdump_segments = ramdump_segments_tmp;
	return 0;

free_smem_areas:
	for (smem_idx = smem_idx - 1; smem_idx >= 1; --smem_idx)
		iounmap(smem_areas_tmp[smem_idx].virt_addr);

	num_smem_areas = 0;
	kfree(ramdump_segments_tmp);
	kfree(smem_areas_tmp);
	return ret;
}

static struct of_device_id msm_smem_match_table[] = {
	{ .compatible = "qcom,smem" },
	{},
};

static struct platform_driver msm_smem_driver = {
	.probe = msm_smem_probe,
	.driver = {
		.name = "msm_smem",
		.owner = THIS_MODULE,
		.of_match_table = msm_smem_match_table,
	},
};

int __init msm_smem_init(void)
{
	static bool registered;
	int rc;

	if (registered)
		return 0;

	registered = true;

	rc = init_smem_remote_spinlock();
	if (rc) {
		pr_err("%s: remote spinlock init failed %d\n", __func__, rc);
		return rc;
	}

	rc = platform_driver_register(&msm_smem_driver);
	if (rc) {
		pr_err("%s: msm_smem_driver register failed %d\n",
							__func__, rc);
		return rc;
	}

	smem_module_init_notify(0, NULL);

	return 0;
}

module_init(msm_smem_init);
