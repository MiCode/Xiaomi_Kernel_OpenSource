/* Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
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
#include <linux/err.h>
#include <linux/init.h>
#include <linux/ipc_logging.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/ramdump.h>

#include <soc/qcom/smem.h>


#include "smem_private.h"

#define MODEM_SBL_VERSION_INDEX 7
#define SMEM_VERSION_INFO_SIZE (32 * 4)
#define SMEM_VERSION 0x000B

enum {
	MSM_SMEM_DEBUG = 1U << 0,
	MSM_SMEM_INFO = 1U << 1,
};

static int msm_smem_debug_mask = MSM_SMEM_INFO;
module_param_named(debug_mask, msm_smem_debug_mask,
			int, S_IRUGO | S_IWUSR | S_IWGRP);
static void *smem_ipc_log_ctx;
#define NUM_LOG_PAGES 4

#define IPC_LOG(x...) do {                                   \
		if (smem_ipc_log_ctx)                        \
			ipc_log_string(smem_ipc_log_ctx, x); \
	} while (0)


#define LOG_ERR(x...) do {  \
		pr_err(x);  \
		IPC_LOG(x); \
	} while (0)
#define SMEM_DBG(x...) do {                               \
		if (msm_smem_debug_mask & MSM_SMEM_DEBUG) \
			IPC_LOG(x);                       \
	} while (0)
#define SMEM_INFO(x...) do {                             \
		if (msm_smem_debug_mask & MSM_SMEM_INFO) \
			IPC_LOG(x);                      \
	} while (0)

#define SMEM_SPINLOCK_SMEM_ALLOC       "S:3"

static void *smem_ram_base;
static resource_size_t smem_ram_size;
static phys_addr_t smem_ram_phys;
static remote_spinlock_t remote_spinlock;
static uint32_t num_smem_areas;
static struct smem_area *smem_areas;
static struct ramdump_segment *smem_ramdump_segments;
static int spinlocks_initialized;
static void *smem_ramdump_dev;
static DEFINE_MUTEX(spinlock_init_lock);
static DEFINE_SPINLOCK(smem_init_check_lock);
static struct device *smem_dev;
static int smem_module_inited;
static RAW_NOTIFIER_HEAD(smem_module_init_notifier_list);
static DEFINE_MUTEX(smem_module_init_notifier_lock);
static bool probe_done;
uint32_t smem_max_items;

/* smem security feature components */
#define SMEM_TOC_IDENTIFIER 0x434f5424 /* "$TOC" */
#define SMEM_TOC_MAX_EXCLUSIONS 4
#define SMEM_PART_HDR_IDENTIFIER 0x54525024 /* "$PRT" */
#define SMEM_ALLOCATION_CANARY 0xa5a5

struct smem_toc_entry {
	uint32_t offset;
	uint32_t size;
	uint32_t flags;
	uint16_t host0;
	uint16_t host1;
	uint32_t size_cacheline;
	uint32_t reserved[3];
	uint32_t exclusion_sizes[SMEM_TOC_MAX_EXCLUSIONS];
};

struct smem_toc {
	/* Identifier is a constant, set to SMEM_TOC_IDENTIFIER. */
	uint32_t identifier;
	uint32_t version;
	uint32_t num_entries;
	uint32_t reserved[5];
	struct smem_toc_entry entry[];
};

struct smem_partition_header {
	/* Identifier is a constant, set to SMEM_PART_HDR_IDENTIFIER. */
	uint32_t identifier;
	uint16_t host0;
	uint16_t host1;
	uint32_t size;
	uint32_t offset_free_uncached;
	uint32_t offset_free_cached;
	uint32_t reserved[3];
};

struct smem_partition_allocation_header {
	/* Canary is a constant, set to SMEM_ALLOCATION_CANARY */
	uint16_t canary;
	uint16_t smem_type;
	uint32_t size; /* includes padding bytes */
	uint16_t padding_data;
	uint16_t padding_hdr;
	uint32_t reserved[1];
};

struct smem_partition_info {
	uint32_t partition_num;
	uint32_t offset;
	uint32_t size_cacheline;
};

static struct smem_partition_info partitions[NUM_SMEM_SUBSYSTEMS];

#define SMEM_COMM_PART_VERSION 0x000C
#define SMEM_COMM_HOST 0xFFFE
static bool use_comm_partition;
static struct smem_partition_info comm_partition;
/* end smem security feature components */

/* Identifier for the SMEM target info struct. */
#define SMEM_TARG_INFO_IDENTIFIER 0x49494953 /* "SIII" in little-endian. */

struct smem_targ_info_type {
	/* Identifier is a constant, set to SMEM_TARG_INFO_IDENTIFIER. */
	uint32_t identifier;
	uint32_t size;
	phys_addr_t phys_base_addr;
	uint32_t  max_items;
};

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
	{SMEM_DSPS, "slpi", .nb.notifier_call = restart_notifier_cb},
};

static int init_smem_remote_spinlock(void);

/**
 * is_probe_done() - Did the probe function successfully complete
 *
 * @return - true if probe successfully completed, false if otherwise
 *
 * Helper function for EPROBE_DEFER support.  If this function returns false,
 * the calling function should immediately return -EPROBE_DEFER.
 */
static bool is_probe_done(void)
{
	return probe_done;
}

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
		phys_addr = smem_ram_phys;
		size = smem_ram_size;

		if (base >= phys_addr && base + offset < phys_addr + size) {
			if (OVERFLOW_ADD_UNSIGNED(uintptr_t,
				(uintptr_t)smem_ram_base, offset)) {
				SMEM_INFO("%s: overflow %p %x\n", __func__,
					smem_ram_base, offset);
				return NULL;
			}

			return smem_ram_base + offset;
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
			SMEM_INFO("%s: overflow %p %x\n", __func__,
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
 * off to a DMA engine.  This function will not return a version of EPROBE_DEFER
 * if the driver is not ready since the caller should obtain @smem_address from
 * one of the other public APIs and get EPROBE_DEFER at that time, if
 * applicable.
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

/**
 * __smem_get_entry_nonsecure - Get pointer and size of existing SMEM item
 *
 * @id:              ID of SMEM item
 * @size:            Pointer to size variable for storing the result
 * @skip_init_check: True means do not verify that SMEM has been initialized
 * @use_rspinlock:   True to use the remote spinlock
 * @returns:         Pointer to SMEM item or NULL if it doesn't exist
 */
static void *__smem_get_entry_nonsecure(unsigned id, unsigned *size,
		bool skip_init_check, bool use_rspinlock)
{
	struct smem_shared *shared = smem_ram_base;
	struct smem_heap_entry *toc = shared->heap_toc;
	int use_spinlocks = spinlocks_initialized && use_rspinlock;
	void *ret = 0;
	unsigned long flags = 0;
	int rc;

	if (!skip_init_check && !smem_initialized_check())
		return ret;

	if (id >= smem_max_items)
		return ret;

	if (use_spinlocks) {
		do {
			rc = remote_spin_trylock_irqsave(&remote_spinlock,
				flags);
		} while (!rc);
	}
	/* toc is in device memory and cannot be speculatively accessed */
	if (toc[id].allocated) {
		phys_addr_t phys_base;

		*size = toc[id].size;
		barrier();

		phys_base = toc[id].reserved & BASE_ADDR_MASK;
		if (!phys_base)
			phys_base = smem_ram_phys;
		ret = smem_phys_to_virt(phys_base, toc[id].offset);
	} else {
		*size = 0;
	}
	if (use_spinlocks)
		remote_spin_unlock_irqrestore(&remote_spinlock, flags);

	return ret;
}

/**
 * __smem_get_entry_secure - Get pointer and size of existing SMEM item with
 *                   security support
 *
 * @id:              ID of SMEM item
 * @size:            Pointer to size variable for storing the result
 * @to_proc:         SMEM host that shares the item with apps
 * @flags:           Item attribute flags
 * @skip_init_check: True means do not verify that SMEM has been initialized
 * @use_rspinlock:   True to use the remote spinlock
 * @returns:         Pointer to SMEM item or NULL if it doesn't exist
 */
static void *__smem_get_entry_secure(unsigned id,
					unsigned *size,
					unsigned to_proc,
					unsigned flags,
					bool skip_init_check,
					bool use_rspinlock)
{
	struct smem_partition_header *hdr;
	unsigned long lflags = 0;
	void *item = NULL;
	struct smem_partition_allocation_header *alloc_hdr;
	uint32_t partition_num;
	uint32_t a_hdr_size;
	int rc;

	SMEM_DBG("%s(%u, %u, %u, %d, %d)\n", __func__, id, to_proc,
					flags, skip_init_check, use_rspinlock);

	if (!skip_init_check && !smem_initialized_check())
		return NULL;

	if (id >= smem_max_items) {
		SMEM_INFO("%s: invalid id %d\n", __func__, id);
		return NULL;
	}

	if (!(flags & SMEM_ANY_HOST_FLAG) && to_proc >= NUM_SMEM_SUBSYSTEMS) {
		SMEM_INFO("%s: id %u invalid to_proc %d\n", __func__, id,
								to_proc);
		return NULL;
	}

	if (flags & SMEM_ANY_HOST_FLAG || !partitions[to_proc].offset) {
		if (use_comm_partition) {
			partition_num = comm_partition.partition_num;
			hdr = smem_areas[0].virt_addr + comm_partition.offset;
		} else {
			return __smem_get_entry_nonsecure(id, size,
					skip_init_check, use_rspinlock);
		}
	} else {
		partition_num = partitions[to_proc].partition_num;
		hdr = smem_areas[0].virt_addr + partitions[to_proc].offset;
	}
	if (unlikely(!spinlocks_initialized)) {
		rc = init_smem_remote_spinlock();
		if (unlikely(rc)) {
			SMEM_INFO(
				"%s: id:%u remote spinlock init failed %d\n",
						__func__, id, rc);
			return NULL;
		}
	}
	if (use_rspinlock) {
		do {
			rc = remote_spin_trylock_irqsave(&remote_spinlock,
				lflags);
		} while (!rc);
	}
	if (hdr->identifier != SMEM_PART_HDR_IDENTIFIER) {
		LOG_ERR(
			"%s: SMEM corruption detected.  Partition %d to %d at %p\n",
								__func__,
								partition_num,
								to_proc,
								hdr);
		BUG();
	}

	if (flags & SMEM_ITEM_CACHED_FLAG) {
		a_hdr_size = ALIGN(sizeof(*alloc_hdr),
				partitions[to_proc].size_cacheline);
		for (alloc_hdr = (void *)(hdr) + hdr->size - a_hdr_size;
				(void *)(alloc_hdr) > (void *)(hdr) +
					hdr->offset_free_cached;
				alloc_hdr = (void *)(alloc_hdr) -
						alloc_hdr->size - a_hdr_size) {
			if (alloc_hdr->canary != SMEM_ALLOCATION_CANARY) {
				LOG_ERR(
					"%s: SMEM corruption detected.  Partition %d to %d at %p\n",
								__func__,
								partition_num,
								to_proc,
								alloc_hdr);
				BUG();

			}
			if (alloc_hdr->smem_type == id) {
				/* 8 byte alignment to match legacy */
				*size = ALIGN(alloc_hdr->size -
						alloc_hdr->padding_data, 8);
				item = (void *)(alloc_hdr) - alloc_hdr->size;
				break;
			}
		}
	} else {
		for (alloc_hdr = (void *)(hdr) + sizeof(*hdr);
				(void *)(alloc_hdr) < (void *)(hdr) +
					hdr->offset_free_uncached;
				alloc_hdr = (void *)(alloc_hdr) +
						sizeof(*alloc_hdr) +
						alloc_hdr->padding_hdr +
						alloc_hdr->size) {
			if (alloc_hdr->canary != SMEM_ALLOCATION_CANARY) {
				LOG_ERR(
					"%s: SMEM corruption detected.  Partition %d to %d at %p\n",
								__func__,
								partition_num,
								to_proc,
								alloc_hdr);
				BUG();

			}
			if (alloc_hdr->smem_type == id) {
				/* 8 byte alignment to match legacy */
				*size = ALIGN(alloc_hdr->size -
						alloc_hdr->padding_data, 8);
				item = (void *)(alloc_hdr) +
						sizeof(*alloc_hdr) +
						alloc_hdr->padding_hdr;
				break;
			}
		}
	}
	if (use_rspinlock)
		remote_spin_unlock_irqrestore(&remote_spinlock, lflags);

	return item;
}

static void *__smem_find(unsigned id, unsigned size_in, bool skip_init_check)
{
	unsigned size;
	void *ptr;

	ptr = __smem_get_entry_nonsecure(id, &size, skip_init_check, true);
	if (!ptr)
		return 0;

	size_in = ALIGN(size_in, 8);
	if (size_in != size) {
		SMEM_INFO("smem_find(%u, %u): wrong size %u\n",
			id, size_in, size);
		return 0;
	}

	return ptr;
}

/**
 * smem_find - Find existing item with security support
 *
 * @id:       ID of SMEM item
 * @size_in:  Size of the SMEM item
 * @to_proc:  SMEM host that shares the item with apps
 * @flags:    Item attribute flags
 * @returns:  Pointer to SMEM item, NULL if it doesn't exist, or -EPROBE_DEFER
 *	if the driver is not ready
 */
void *smem_find(unsigned id, unsigned size_in, unsigned to_proc, unsigned flags)
{
	unsigned size;
	void *ptr;

	SMEM_DBG("%s(%u, %u, %u, %u)\n", __func__, id, size_in, to_proc,
									flags);

	/*
	 * Handle the circular dependecy between SMEM and software implemented
	 * remote spinlocks.  SMEM must initialize the remote spinlocks in
	 * probe() before it is done.  EPROBE_DEFER handling will not resolve
	 * this code path, so we must be intellegent to know that the spinlock
	 * item is a special case.
	 */
	if (!is_probe_done() && id != SMEM_SPINLOCK_ARRAY)
		return ERR_PTR(-EPROBE_DEFER);

	ptr = smem_get_entry(id, &size, to_proc, flags);
	if (!ptr)
		return 0;

	size_in = ALIGN(size_in, 8);
	if (size_in != size) {
		SMEM_INFO("smem_find(%u, %u, %u, %u): wrong size %u\n",
			id, size_in, to_proc, flags, size);
		return 0;
	}

	return ptr;
}
EXPORT_SYMBOL(smem_find);

/**
 * alloc_item_nonsecure - Allocate an SMEM item in the nonsecure partition
 *
 * @id:              ID of SMEM item
 * @size_in:         Size to allocate
 * @returns:         Pointer to SMEM item or NULL for error
 *
 * Assumes the id parameter is valid and does not already exist.  Assumes
 * size_in is already adjusted for alignment, if necessary.  Requires the
 * remote spinlock to already be locked.
 */
static void *alloc_item_nonsecure(unsigned id, unsigned size_in)
{
	void *smem_base = smem_ram_base;
	struct smem_shared *shared = smem_base;
	struct smem_heap_entry *toc = shared->heap_toc;
	void *ret = NULL;

	if (shared->heap_info.heap_remaining >= size_in) {
		toc[id].offset = shared->heap_info.free_offset;
		toc[id].size = size_in;
		/*
		 * wmb() is necessary to ensure the allocation data is
		 * consistent before setting the allocated flag to prevent race
		 * conditions with remote processors
		 */
		wmb();
		toc[id].allocated = 1;

		shared->heap_info.free_offset += size_in;
		shared->heap_info.heap_remaining -= size_in;
		ret = smem_base + toc[id].offset;
		/*
		 * wmb() is necessary to ensure the heap data is consistent
		 * before continuing to prevent race conditions with remote
		 * processors
		 */
		wmb();
	} else {
		SMEM_INFO("%s: id %u not enough memory %u (required %u)\n",
			__func__, id, shared->heap_info.heap_remaining,
			size_in);
	}

	return ret;
}

/**
 * alloc_item_secure - Allocate an SMEM item in a secure partition
 *
 * @id:              ID of SMEM item
 * @size_in:         Size to allocate
 * @to_proc:         SMEM host that shares the item with apps
 * @flags:           Item attribute flags
 * @returns:         Pointer to SMEM item or NULL for error
 *
 * Assumes the id parameter is valid and does  not already exist.  Assumes
 * size_in is the raw size requested by the client.  Assumes to_proc is a valid
 * host, and a valid partition to that host exists.  Requires the remote
 * spinlock to already be locked.
 */
static void *alloc_item_secure(unsigned id, unsigned size_in, unsigned to_proc,
								unsigned flags)
{
	void *smem_base = smem_ram_base;
	struct smem_partition_header *hdr;
	struct smem_partition_allocation_header *alloc_hdr;
	uint32_t a_hdr_size;
	uint32_t a_data_size;
	uint32_t size_cacheline;
	uint32_t free_space;
	uint32_t partition_num;
	void *ret = NULL;

	if (to_proc == SMEM_COMM_HOST) {
		hdr = smem_base + comm_partition.offset;
		partition_num = comm_partition.partition_num;
		size_cacheline = comm_partition.size_cacheline;
	} else if (to_proc < NUM_SMEM_SUBSYSTEMS) {
		hdr = smem_base + partitions[to_proc].offset;
		partition_num = partitions[to_proc].partition_num;
		size_cacheline = partitions[to_proc].size_cacheline;
	} else {
		SMEM_INFO("%s: invalid to_proc %u for id %u\n", __func__,
								to_proc, id);
		return NULL;
	}

	if (hdr->identifier != SMEM_PART_HDR_IDENTIFIER) {
		LOG_ERR(
			"%s: SMEM corruption detected.  Partition %d to %d at %p\n",
								__func__,
								partition_num,
								to_proc,
								hdr);
		BUG();
	}

	free_space = hdr->offset_free_cached -
					hdr->offset_free_uncached;

	if (flags & SMEM_ITEM_CACHED_FLAG) {
		a_hdr_size = ALIGN(sizeof(*alloc_hdr), size_cacheline);
		a_data_size = ALIGN(size_in, size_cacheline);
		if (free_space < a_hdr_size + a_data_size) {
			SMEM_INFO(
				"%s: id %u not enough memory %u (required %u)\n",
						__func__, id, free_space,
						a_hdr_size + a_data_size);
			return ret;
		}
		alloc_hdr = (void *)(hdr) + hdr->offset_free_cached -
								a_hdr_size;
		alloc_hdr->canary = SMEM_ALLOCATION_CANARY;
		alloc_hdr->smem_type = id;
		alloc_hdr->size = a_data_size;
		alloc_hdr->padding_data = a_data_size - size_in;
		alloc_hdr->padding_hdr = a_hdr_size - sizeof(*alloc_hdr);
		hdr->offset_free_cached = hdr->offset_free_cached -
						a_hdr_size - a_data_size;
		ret = (void *)(alloc_hdr) - a_data_size;
		/*
		 * The SMEM protocol currently does not support cacheable
		 * areas within the smem region, but if it ever does in the
		 * future, then cache management needs to be done here.
		 * The area of memory this item is allocated from will need to
		 * be dynamically made cachable, and a cache flush of the
		 * allocation header using __cpuc_flush_dcache_area and
		 * outer_flush_area will need to be done.
		 */
	} else {
		a_hdr_size = sizeof(*alloc_hdr);
		a_data_size = ALIGN(size_in, 8);
		if (free_space < a_hdr_size + a_data_size) {
			SMEM_INFO(
				"%s: id %u not enough memory %u (required %u)\n",
						__func__, id, free_space,
						a_hdr_size + a_data_size);
			return ret;
		}
		alloc_hdr = (void *)(hdr) + hdr->offset_free_uncached;
		alloc_hdr->canary = SMEM_ALLOCATION_CANARY;
		alloc_hdr->smem_type = id;
		alloc_hdr->size = a_data_size;
		alloc_hdr->padding_data = a_data_size - size_in;
		alloc_hdr->padding_hdr = a_hdr_size - sizeof(*alloc_hdr);
		hdr->offset_free_uncached = hdr->offset_free_uncached +
						a_hdr_size + a_data_size;
		ret = alloc_hdr + 1;
	}
	/*
	 * wmb() is necessary to ensure the heap and allocation data is
	 * consistent before continuing to prevent race conditions with remote
	 * processors
	 */
	wmb();

	return ret;
}

/**
 * smem_alloc - Find an existing item, otherwise allocate it with security
 *		support
 *
 * @id:       ID of SMEM item
 * @size_in:  Size of the SMEM item
 * @to_proc:  SMEM host that shares the item with apps
 * @flags:    Item attribute flags
 * @returns:  Pointer to SMEM item, NULL if it couldn't be found/allocated,
 *	or -EPROBE_DEFER if the driver is not ready
 */
void *smem_alloc(unsigned id, unsigned size_in, unsigned to_proc,
								unsigned flags)
{
	unsigned long lflags;
	void *ret = NULL;
	int rc;
	unsigned size_out;
	unsigned a_size_in;

	SMEM_DBG("%s(%u, %u, %u, %u)\n", __func__, id, size_in, to_proc,
									flags);

	if (!is_probe_done())
		return ERR_PTR(-EPROBE_DEFER);

	if (!smem_initialized_check())
		return NULL;

	if (id >= smem_max_items) {
		SMEM_INFO("%s: invalid id %u\n", __func__, id);
		return NULL;
	}

	if (!(flags & SMEM_ANY_HOST_FLAG) && to_proc >= NUM_SMEM_SUBSYSTEMS) {
		SMEM_INFO("%s: invalid to_proc %u for id %u\n", __func__,
								to_proc, id);
		return NULL;
	}

	if (unlikely(!spinlocks_initialized)) {
		rc = init_smem_remote_spinlock();
		if (unlikely(rc)) {
			SMEM_INFO("%s: id:%u remote spinlock init failed %d\n",
							__func__, id, rc);
			return NULL;
		}
	}

	a_size_in = ALIGN(size_in, 8);
	do {
		rc = remote_spin_trylock_irqsave(&remote_spinlock, lflags);
	} while (!rc);

	ret = __smem_get_entry_secure(id, &size_out, to_proc, flags, true,
									false);
	if (ret) {
		SMEM_INFO("%s: %u already allocated\n", __func__, id);
		if (a_size_in == size_out) {
			remote_spin_unlock_irqrestore(&remote_spinlock, lflags);
			return ret;
		} else {
			remote_spin_unlock_irqrestore(&remote_spinlock, lflags);
			SMEM_INFO("%s: id %u wrong size %u (expected %u)\n",
				__func__, id, size_out, a_size_in);
			return NULL;
		}
	}

	if (id > SMEM_FIXED_ITEM_LAST) {
		SMEM_INFO("%s: allocating %u size %u to_proc %u flags %u\n",
					__func__, id, size_in, to_proc, flags);
		if (flags & SMEM_ANY_HOST_FLAG
			|| !partitions[to_proc].offset) {
			if (use_comm_partition)
				ret = alloc_item_secure(id, size_in,
							SMEM_COMM_HOST, flags);
			else
				ret = alloc_item_nonsecure(id, a_size_in);
		} else {
			ret = alloc_item_secure(id, size_in, to_proc, flags);
		}
	} else {
		SMEM_INFO("%s: attempted to allocate non-dynamic item %u\n",
								__func__, id);
	}

	remote_spin_unlock_irqrestore(&remote_spinlock, lflags);
	return ret;
}
EXPORT_SYMBOL(smem_alloc);

/**
 * smem_get_entry - Get existing item with security support
 *
 * @id:       ID of SMEM item
 * @size:     Pointer to size variable for storing the result
 * @to_proc:  SMEM host that shares the item with apps
 * @flags:    Item attribute flags
 * @returns:  Pointer to SMEM item, NULL if it doesn't exist, or -EPROBE_DEFER
 *	if the driver isn't ready
 */
void *smem_get_entry(unsigned id, unsigned *size, unsigned to_proc,
								unsigned flags)
{
	SMEM_DBG("%s(%u, %u, %u)\n", __func__, id, to_proc, flags);

	/*
	 * Handle the circular dependecy between SMEM and software implemented
	 * remote spinlocks.  SMEM must initialize the remote spinlocks in
	 * probe() before it is done.  EPROBE_DEFER handling will not resolve
	 * this code path, so we must be intellegent to know that the spinlock
	 * item is a special case.
	 */
	if (!is_probe_done() && id != SMEM_SPINLOCK_ARRAY)
		return ERR_PTR(-EPROBE_DEFER);

	return __smem_get_entry_secure(id, size, to_proc, flags, false, true);
}
EXPORT_SYMBOL(smem_get_entry);

/**
 * smem_get_entry_no_rlock - Get existing item without using remote spinlock
 *
 * @id:       ID of SMEM item
 * @size_out: Pointer to size variable for storing the result
 * @to_proc:  SMEM host that shares the item with apps
 * @flags:    Item attribute flags
 * @returns:  Pointer to SMEM item, NULL if it doesn't exist, or -EPROBE_DEFER
 *	if the driver isn't ready
 *
 * This function does not lock the remote spinlock and should only be used in
 * failure-recover cases such as retrieving the subsystem failure reason during
 * subsystem restart.
 */
void *smem_get_entry_no_rlock(unsigned id, unsigned *size_out, unsigned to_proc,
								unsigned flags)
{
	if (!is_probe_done())
		return ERR_PTR(-EPROBE_DEFER);

	return __smem_get_entry_secure(id, size_out, to_proc, flags, false,
									false);
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
 * smem_get_free_space() - Get the available allocation free space for a
 *				partition
 *
 * @to_proc: remote SMEM host.  Determines the applicable partition
 * @returns: size in bytes available to allocate
 *
 * Helper function for SMD so that SMD only scans the channel allocation
 * table for a partition when it is reasonably certain that a channel has
 * actually been created, because scanning can be expensive.  Creating a channel
 * will consume some of the free space in a partition, so SMD can compare the
 * last free space size against the current free space size to determine if
 * a channel may have been created.  SMD can't do this directly, because the
 * necessary partition internals are restricted to just SMEM.
 */
unsigned smem_get_free_space(unsigned to_proc)
{
	struct smem_partition_header *hdr;
	struct smem_shared *shared;

	if (to_proc >= NUM_SMEM_SUBSYSTEMS) {
		pr_err("%s: invalid to_proc:%d\n", __func__, to_proc);
		return UINT_MAX;
	}

	if (partitions[to_proc].offset) {
		if (unlikely(OVERFLOW_ADD_UNSIGNED(uintptr_t,
					(uintptr_t)smem_areas[0].virt_addr,
					partitions[to_proc].offset))) {
			pr_err("%s: unexpected overflow detected\n", __func__);
			return UINT_MAX;
		}
		hdr = smem_areas[0].virt_addr + partitions[to_proc].offset;
		return hdr->offset_free_cached - hdr->offset_free_uncached;
	} else {
		shared = smem_ram_base;
		return shared->heap_info.heap_remaining;
	}
}
EXPORT_SYMBOL(smem_get_free_space);

/**
 * smem_get_version() - Get the smem user version number
 *
 * @idx: SMEM user idx in SMEM_VERSION_INFO table.
 * @returns: smem version number if success otherwise zero.
 */
unsigned smem_get_version(unsigned idx)
{
	int *version_array;
	struct smem_shared *smem = smem_ram_base;

	if (idx > 32) {
		pr_err("%s: invalid idx:%d\n", __func__, idx);
		return 0;
	}

	if (use_comm_partition)
		version_array = smem->version;
	else
		version_array = __smem_find(SMEM_VERSION_INFO,
					SMEM_VERSION_INFO_SIZE, true);
	if (version_array == NULL)
		return 0;

	return version_array[idx];
}
EXPORT_SYMBOL(smem_get_version);

/**
 * init_smem_remote_spinlock - Reentrant remote spinlock initialization
 *
 * @returns: success or error code for failure
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
	unsigned ver;

	if (likely(checked)) {
		if (unlikely(!is_inited))
			LOG_ERR("%s: smem not initialized\n", __func__);
		return is_inited;
	}

	spin_lock_irqsave(&smem_init_check_lock, flags);
	if (checked) {
		spin_unlock_irqrestore(&smem_init_check_lock, flags);
		if (unlikely(!is_inited))
			LOG_ERR("%s: smem not initialized\n", __func__);
		return is_inited;
	}

	smem = smem_ram_base;

	if (smem->heap_info.initialized != 1)
		goto failed;
	if (smem->heap_info.reserved != 0)
		goto failed;

	/*
	 * The Modem SBL is now the Master SBL version and is required to
	 * pre-initialize SMEM and fill in any necessary configuration
	 * structures.  Without the extra configuration data, the SMEM driver
	 * cannot be properly initialized.
	 */
	ver = smem->version[MODEM_SBL_VERSION_INDEX];
	if (ver == SMEM_COMM_PART_VERSION << 16) {
		use_comm_partition = true;
	} else if (ver != SMEM_VERSION << 16) {
		pr_err("%s: SBL version not correct 0x%x\n",
				__func__, smem->version[7]);
		goto failed;
	}

	is_inited = 1;
	checked = 1;
	spin_unlock_irqrestore(&smem_init_check_lock, flags);
	return is_inited;

failed:
	is_inited = 0;
	checked = 1;
	spin_unlock_irqrestore(&smem_init_check_lock, flags);
	LOG_ERR(
		"%s: shared memory needs to be initialized by SBL before booting\n",
								__func__);
	return is_inited;
}
EXPORT_SYMBOL(smem_initialized_check);

static int restart_notifier_cb(struct notifier_block *this,
				unsigned long code,
				void *data)
{
	struct restart_notifier_block *notifier;
	struct notif_data *notifdata = data;
	int ret;

	switch (code) {

	case SUBSYS_AFTER_SHUTDOWN:
		notifier = container_of(this,
					struct restart_notifier_block, nb);
		SMEM_INFO("%s: ssrestart for processor %d ('%s')\n",
				__func__, notifier->processor,
				notifier->name);
		remote_spin_release(&remote_spinlock, notifier->processor);
		remote_spin_release_all(notifier->processor);
		break;
	case SUBSYS_SOC_RESET:
		if (!(smem_ramdump_dev && notifdata->enable_mini_ramdumps))
			break;
	case SUBSYS_RAMDUMP_NOTIFICATION:
		if (!(smem_ramdump_dev && (notifdata->enable_mini_ramdumps
						|| notifdata->enable_ramdump)))
			break;
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
			LOG_ERR("%s: unable to dump smem %d\n", __func__, ret);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static __init int modem_restart_late_init(void)
{
	int i;
	void *handle;
	struct restart_notifier_block *nb;

	if (smem_dev)
		smem_ramdump_dev = create_ramdump_device("smem", smem_dev);
	if (IS_ERR_OR_NULL(smem_ramdump_dev)) {
		LOG_ERR("%s: Unable to create smem ramdump device.\n",
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

/**
 * smem_init_security_partition - Init local structures for a secured smem
 *                   partition that has apps as one of the hosts
 *
 * @entry:           Entry in the security TOC for the partition to init
 * @num:             Partition ID
 *
 * Initialize local data structures to point to a secured smem partition
 * that is accessible by apps and another processor.  Assumes that one of the
 * listed hosts is apps.  Verifiess that the partition is valid, otherwise will
 * skip.  Checks for memory corruption and will BUG() if detected.  Assumes
 * smem_areas is already initialized and that smem_areas[0] corresponds to the
 * smem region with the secured partitions.
 */
static void smem_init_security_partition(struct smem_toc_entry *entry,
								uint32_t num)
{
	uint16_t remote_host = 0;
	struct smem_partition_header *hdr;
	bool is_comm_partition = false;

	if (!entry->offset) {
		SMEM_INFO("Skipping smem partition %d - bad offset\n", num);
		return;
	}
	if (!entry->size) {
		SMEM_INFO("Skipping smem partition %d - bad size\n", num);
		return;
	}
	if (!entry->size_cacheline) {
		SMEM_INFO("Skipping smem partition %d - bad cacheline\n", num);
		return;
	}

	if (entry->host0 == SMEM_COMM_HOST && entry->host1 == SMEM_COMM_HOST)
		is_comm_partition = true;

	if (!is_comm_partition) {
		if (entry->host0 == SMEM_APPS)
			remote_host = entry->host1;
		else
			remote_host = entry->host0;

		if (remote_host >= NUM_SMEM_SUBSYSTEMS) {
			SMEM_INFO(
				"Skipping smem partition %d - bad remote:%d\n",
				num, remote_host);
			return;
		}
		if (partitions[remote_host].offset) {
			SMEM_INFO(
				"Skipping smem partition %d - duplicate of %d\n",
				num, partitions[remote_host].partition_num);
			return;
		}

		if (entry->host0 != SMEM_APPS && entry->host1 != SMEM_APPS) {
			SMEM_INFO(
				"Non-APSS Partition %d offset:%x host0:%d host1:%d\n",
				num, entry->offset, entry->host0, entry->host1);
			return;
		}
	}

	hdr = smem_areas[0].virt_addr + entry->offset;

	if (hdr->identifier != SMEM_PART_HDR_IDENTIFIER) {
		LOG_ERR("Smem partition %d hdr magic is bad\n", num);
		BUG();
	}
	if (!hdr->size) {
		LOG_ERR("Smem partition %d size is 0\n", num);
		BUG();
	}
	if (hdr->offset_free_uncached > hdr->size) {
		LOG_ERR("Smem partition %d uncached heap exceeds size\n", num);
		BUG();
	}
	if (hdr->offset_free_cached > hdr->size) {
		LOG_ERR("Smem partition %d cached heap exceeds size\n", num);
		BUG();
	}
	if (is_comm_partition) {
		if (hdr->host0 == SMEM_COMM_HOST
			&& hdr->host1 == SMEM_COMM_HOST) {
			comm_partition.partition_num = num;
			comm_partition.offset = entry->offset;
			comm_partition.size_cacheline = entry->size_cacheline;
			SMEM_INFO("Common Partition %d offset:%x\n", num,
							entry->offset);
		} else {
			LOG_ERR("Smem Comm partition hosts don't match TOC\n");
			WARN_ON(1);
		}
		return;
	}
	if (hdr->host0 != SMEM_APPS && hdr->host1 != SMEM_APPS) {
		LOG_ERR("Smem partition %d hosts don't match TOC\n", num);
		BUG();
	}
	if (hdr->host0 != remote_host && hdr->host1 != remote_host) {
		LOG_ERR("Smem partition %d hosts don't match TOC\n", num);
		BUG();
	}

	partitions[remote_host].partition_num = num;
	partitions[remote_host].offset = entry->offset;
	partitions[remote_host].size_cacheline = entry->size_cacheline;
	SMEM_INFO("Partition %d offset:%x remote:%d\n", num, entry->offset,
								remote_host);
}

/**
 * smem_init_security - Init local support for secured smem
 *
 * Looks for a valid security TOC, and if one is found, parses it looking for
 * partitions that apps can access.  If any such partitions are found, do the
 * required local initialization to support them.  Assumes smem_areas is inited
 * and smem_area[0] corresponds to the smem region with the TOC.
 */
static void smem_init_security(void)
{
	struct smem_toc *toc;
	uint32_t i;

	SMEM_DBG("%s\n", __func__);

	toc = smem_areas[0].virt_addr + smem_areas[0].size - 4 * 1024;

	if (toc->identifier != SMEM_TOC_IDENTIFIER) {
		LOG_ERR("%s failed: invalid TOC magic\n", __func__);
		return;
	}

	for (i = 0; i < toc->num_entries; ++i) {
		SMEM_DBG("Partition %d host0:%d host1:%d\n", i,
							toc->entry[i].host0,
							toc->entry[i].host1);
		smem_init_security_partition(&toc->entry[i], i);
	}

	SMEM_DBG("%s done\n", __func__);
}

/**
 * smem_init_target_info - Init smem target information
 *
 * @info_addr : smem target info physical address.
 * @size : size of the smem target info structure.
 *
 * This function is used to initialize the smem_targ_info structure and checks
 * for valid identifier, if identifier is valid initialize smem variables.
 */
static int smem_init_target_info(phys_addr_t info_addr, resource_size_t size)
{
	struct smem_targ_info_type *smem_targ_info;
	void *smem_targ_info_addr;
	smem_targ_info_addr = ioremap_nocache(info_addr, size);
	if (!smem_targ_info_addr) {
		LOG_ERR("%s: failed ioremap_nocache() of addr:%pa size:%pa\n",
				__func__, &info_addr, &size);
		return -ENODEV;
	}
	smem_targ_info =
		(struct smem_targ_info_type __iomem *)smem_targ_info_addr;

	if (smem_targ_info->identifier != SMEM_TARG_INFO_IDENTIFIER) {
		LOG_ERR("%s failed: invalid TARGET INFO magic\n", __func__);
		return -ENODEV;
	}
	smem_ram_phys = smem_targ_info->phys_base_addr;
	smem_ram_size = smem_targ_info->size;
	if (smem_targ_info->max_items)
		smem_max_items = smem_targ_info->max_items;
	iounmap(smem_targ_info_addr);
	return 0;
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
	bool security_enabled;

	r = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"smem_targ_info_imem");
	if (r) {
		if (smem_init_target_info(r->start, resource_size(r)))
			goto smem_targ_info_legacy;
		goto smem_targ_info_done;
	}

	r = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"smem_targ_info_reg");
	if (r) {
		void *reg_base_addr;
		uint64_t base_addr;
		reg_base_addr = ioremap_nocache(r->start, resource_size(r));
		base_addr = (uint32_t)readl_relaxed(reg_base_addr);
		base_addr |=
			((uint64_t)readl_relaxed(reg_base_addr + 0x4) << 32);
		iounmap(reg_base_addr);
		if ((base_addr == 0) || ((base_addr >> 32) != 0)) {
			SMEM_INFO("%s: Invalid SMEM address\n", __func__);
			goto smem_targ_info_legacy;
		}
		if (smem_init_target_info(base_addr,
				sizeof(struct smem_targ_info_type)))
			goto smem_targ_info_legacy;
		goto smem_targ_info_done;
	}

smem_targ_info_legacy:
	SMEM_INFO("%s: reading dt-specified SMEM address\n", __func__);
	r = platform_get_resource_byname(pdev, IORESOURCE_MEM, "smem");
	if (r) {
		smem_ram_size = resource_size(r);
		smem_ram_phys = r->start;
	}

smem_targ_info_done:
	if (!smem_ram_phys || !smem_ram_size) {
		LOG_ERR("%s: Missing SMEM TARGET INFO\n", __func__);
		return -ENODEV;
	}

	smem_ram_base = ioremap_nocache(smem_ram_phys, smem_ram_size);

	if (!smem_ram_base) {
		LOG_ERR("%s: ioremap_nocache() of addr:%pa size: %pa\n",
				__func__,
				&smem_ram_phys, &smem_ram_size);
		return -ENODEV;
	}

	if (!smem_initialized_check())
		return -ENODEV;

	/*
	 * The software implementation requires smem_find(), which needs
	 * smem_ram_base to be intitialized.  The remote spinlock item is
	 * guarenteed to be allocated by the bootloader, so this is the
	 * safest and earliest place to init the spinlock.
	 */
	ret = init_smem_remote_spinlock();
	if (ret) {
		LOG_ERR("%s: remote spinlock init failed %d\n", __func__, ret);
		return ret;
	}

	key = "irq-reg-base";
	r = platform_get_resource_byname(pdev, IORESOURCE_MEM, key);
	if (!r) {
		LOG_ERR("%s: missing '%s'\n", __func__, key);
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
			LOG_ERR("%s: max num aux mem regions reached\n",
								__func__);
			break;
		}
	}
	/* Initialize main SMEM region and SSR ramdump region */
	smem_areas_tmp = kmalloc_array(num_smem_areas, sizeof(struct smem_area),
				GFP_KERNEL);
	if (!smem_areas_tmp) {
		LOG_ERR("%s: smem areas kmalloc failed\n", __func__);
		ret = -ENOMEM;
		goto free_smem_areas;
	}

	ramdump_segments_tmp = kcalloc(num_smem_areas,
			sizeof(struct ramdump_segment), GFP_KERNEL);
	if (!ramdump_segments_tmp) {
		LOG_ERR("%s: ramdump segment kmalloc failed\n", __func__);
		ret = -ENOMEM;
		goto free_smem_areas;
	}
	smem_areas_tmp[smem_idx].phys_addr =  smem_ram_phys;
	smem_areas_tmp[smem_idx].size = smem_ram_size;
	smem_areas_tmp[smem_idx].virt_addr = smem_ram_base;

	ramdump_segments_tmp[smem_idx].address = smem_ram_phys;
	ramdump_segments_tmp[smem_idx].size = smem_ram_size;
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
			LOG_ERR("%s: ioremap_nocache() of addr:%pa size: %pa\n",
				__func__,
				&smem_areas_tmp[smem_idx].phys_addr,
				&smem_areas_tmp[smem_idx].size);
			ret = -ENOMEM;
			goto free_smem_areas;
		}

		if (OVERFLOW_ADD_UNSIGNED(uintptr_t,
				(uintptr_t)smem_areas_tmp[smem_idx].virt_addr,
				smem_areas_tmp[smem_idx].size)) {
			LOG_ERR(
				"%s: invalid virtual address block %i: %p:%pa\n",
					__func__, smem_idx,
					smem_areas_tmp[smem_idx].virt_addr,
					&smem_areas_tmp[smem_idx].size);
			++smem_idx;
			ret = -EINVAL;
			goto free_smem_areas;
		}

		++smem_idx;
		if (smem_idx > 999) {
			LOG_ERR("%s: max num aux mem regions reached\n",
							__func__);
			break;
		}
	}

	smem_areas = smem_areas_tmp;
	smem_ramdump_segments = ramdump_segments_tmp;

	key = "qcom,mpu-enabled";
	security_enabled = of_property_read_bool(pdev->dev.of_node, key);
	if (security_enabled) {
		SMEM_INFO("smem security enabled\n");
		smem_init_security();
	}
	smem_dev = &pdev->dev;
	probe_done = true;

	ret = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (ret)
		LOG_ERR("%s: of_platform_populate failed %d\n", __func__, ret);

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
	smem_max_items = SMEM_NUM_ITEMS;
	smem_ipc_log_ctx = ipc_log_context_create(NUM_LOG_PAGES, "smem", 0);
	if (!smem_ipc_log_ctx) {
		pr_err("%s: unable to create logging context\n", __func__);
		msm_smem_debug_mask = 0;
	}

	rc = platform_driver_register(&msm_smem_driver);
	if (rc) {
		LOG_ERR("%s: msm_smem_driver register failed %d\n",
							__func__, rc);
		return rc;
	}

	smem_module_init_notify(0, NULL);

	return 0;
}

arch_initcall(msm_smem_init);
