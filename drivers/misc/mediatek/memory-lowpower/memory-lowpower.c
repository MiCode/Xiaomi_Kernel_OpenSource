/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "memory-lowpower: " fmt
#define CONFIG_MTK_MEMORY_LOWPOWER_DEBUG

#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/printk.h>
#include <linux/cma.h>
#include <linux/debugfs.h>
#include <linux/stat.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/memblock.h>
#include <asm/page.h>
#include <asm-generic/memory_model.h>

/* Memory lowpower private header file */
#include "internal.h"
#include "mtk_meminfo.h"

static struct cma *cma;
static struct page *cma_pages;
static unsigned long cma_usage_count;

static DEFINE_MUTEX(memory_lowpower_mutex);

#define MEMORY_LOWPOWER_FULLNESS
#ifdef MEMORY_LOWPOWER_FULLNESS
/*
 * Memory-lowpower needs to occupy the last portion of DRAM
 * physically. But this may conflict with DRAM dummy read/write
 * which will reserve a small size of aforementioned range.
 * Because this feature doesn't care about the correctness of
 * contents in the reserved range, so let memory-lowpower count
 * it for fullness. (in bytes)
 */
static phys_addr_t grab_lastsize;
#endif

/*
 * Check whether memory_lowpower is initialized
 */
bool memory_lowpower_inited(void)
{
	return (cma != NULL);
}

/*
 * memory_lowpower_base - query the memory_lowpower base
 */
phys_addr_t memory_lowpower_base(void)
{
	return cma_get_base(cma);
}

struct single_cma_registration memory_lowpower_registration;
/*
 * memory_lowpower_size - query memory_lowpower size
 */
phys_addr_t memory_lowpower_size(void)
{
#ifndef MEMORY_LOWPOWER_FULLNESS
	return memory_lowpower_registration.size;
#else
	return memory_lowpower_registration.size + grab_lastsize;
#endif
}

/*
 * memory_lowpower_real_size - query memory_lowpower actual size
 */
static phys_addr_t memory_lowpower_real_size(void)
{
	return memory_lowpower_registration.size;
}

/*
 * get_memory_lowpwer_cma_aligned - allocate aligned cma
 * memory belongs to lowpower cma
 * @count: Requested number of pages.
 * @align: Requested alignment of pages (in PAGE_SIZE order).
 * @pages: Pointer indicates allocated cma buffer.
 * It returns 0 in success, otherwise returns -1
 */
int get_memory_lowpower_cma_aligned(int count, unsigned int align,
					struct page **pages, bool last)
{
#ifdef MEMORY_LOWPOWER_FULLNESS
	if (last)
		count -= grab_lastsize >> PAGE_SHIFT;
#endif

	*pages = zmc_cma_alloc(cma, count, align,
			&memory_lowpower_registration);
	if (*pages == NULL) {
		pr_alert("lowpower cma allocation failed\n");
		return -1;
	}

	mutex_lock(&memory_lowpower_mutex);
	cma_usage_count += count;
	mutex_unlock(&memory_lowpower_mutex);

	return 0;
}

/*
 * put_memory_lowpwer_cma_aligned - free aligned cma memory belongs
 * to lowpower cma
 * @count: Requested number of pages.
 * @pages: Pointer indicates allocated cma buffer.
 * It returns 0 in success, otherwise returns -1
 */
int put_memory_lowpower_cma_aligned(int count, struct page *pages, bool last)
{
#ifdef MEMORY_LOWPOWER_FULLNESS
	if (last)
		count -= grab_lastsize >> PAGE_SHIFT;
#endif

	if (pages) {
		if (!zmc_cma_release(cma, pages, count)) {
			pr_err("%s incorrect pages: %p(%lx)\n",
					__func__,
					pages, page_to_pfn(pages));
			return -1;
		}
		mutex_lock(&memory_lowpower_mutex);
		cma_usage_count -= count;
		mutex_unlock(&memory_lowpower_mutex);
	}

	return 0;
}

/*
 * get_memory_lowpwer_cma - allocate all cma memory belongs to lowpower cma.
 * It returns 0 in success, otherwise returns -1
 * (TBD) reference count for cma_pages
 */
int get_memory_lowpower_cma(void)
{
	int count = memory_lowpower_real_size() >> PAGE_SHIFT;
	struct page *pages;

	mutex_lock(&memory_lowpower_mutex);
	pages = cma_pages;
	mutex_unlock(&memory_lowpower_mutex);
	if (pages) {
		pr_alert("cma already collected\n");
		goto out;
	}

	pages = zmc_cma_alloc(cma, count, 0, &memory_lowpower_registration);
	if (!pages) {
		pr_alert("lowpower cma allocation failed\n");
		return -1;
	}

	mutex_lock(&memory_lowpower_mutex);
	cma_pages = pages;
	pr_debug("%s:%d ok\n", __func__, __LINE__);
	cma_usage_count += count;
	mutex_unlock(&memory_lowpower_mutex);

out:
	return 0;
}

/*
 * put_memory_lowpwer_cma - free all cma memory belongs to lowpower cma.
 * It returns 0 in success, otherwise returns -1
 * (TBD) reference count for cma_pages
 */
int put_memory_lowpower_cma(void)
{
	int count = memory_lowpower_real_size() >> PAGE_SHIFT;
	struct page *pages;

	mutex_lock(&memory_lowpower_mutex);
	pages = cma_pages;
	cma_pages = 0;
	mutex_unlock(&memory_lowpower_mutex);

	if (pages) {
		if (!zmc_cma_release(cma, pages, count)) {
			pr_err("%s incorrect pages: %p(%lx)\n",
					__func__,
					pages, page_to_pfn(pages));
			return -1;
		}

		mutex_lock(&memory_lowpower_mutex);
		cma_usage_count -= count;
		mutex_unlock(&memory_lowpower_mutex);
	}

	return 0;
}
#ifdef MEMORY_LOWPOWER_FULLNESS
#define TEST_AND_RESERVE_MEMBLOCK(base, size) \
	(!memblock_is_region_reserved(base, size) && \
	memblock_reserve(base, size) == 0)
/* Grab the last page block for fullness */
static void memory_lowpower_fullness(phys_addr_t base, phys_addr_t size)
{
	phys_addr_t pageblock_size = 1 << (pageblock_order + PAGE_SHIFT);
	phys_addr_t expected_lastaddr, got_lastaddr = (phys_addr_t)ULLONG_MAX;

	/* If the hole is not the size of 1 pageblock, just return */
	if (memblock_end_of_DRAM() - (base + size) != pageblock_size) {
		pr_alert("%s, no need to do fullness\n", __func__);
		return;
	}

	expected_lastaddr = base + size;

	if (TEST_AND_RESERVE_MEMBLOCK(expected_lastaddr, pageblock_size) ||
		TEST_AND_RESERVE_MEMBLOCK(expected_lastaddr,
			pageblock_size - PAGE_SIZE))
		got_lastaddr = expected_lastaddr;

	if (expected_lastaddr == got_lastaddr) {
		pr_alert("%s, success to grab the \"last pageblock\"\n",
				__func__);
		grab_lastsize = pageblock_size;
	} else
		pr_alert("%s, failed to grab the last pageblock\n", __func__);
}
#endif

static void zmc_memory_lowpower_init(struct cma *zmc_cma)
{
	cma = zmc_cma;

#ifdef MEMORY_LOWPOWER_FULLNESS
	/* try to grab the last pageblock */
	pr_info("%s: memory-lowpower-fullness\n", __func__);
	if (cma != NULL)
		memory_lowpower_fullness(memory_lowpower_base(),
					memory_lowpower_size());
#endif
}

struct single_cma_registration memory_lowpower_registration = {
	.size = ULONG_MAX,
	.align = 0x10000000,
	.flag = ZMC_ALLOC_ALL,
	.name = "memory-lowpower",
	.init = zmc_memory_lowpower_init,
	.prio = ZMC_MLP,
};

static int __init memory_lowpower_init(struct reserved_mem *rmem)
{
	int ret;

	pr_alert("%s, name: %s, base: 0x%pa, size: 0x%pa\n",
		 __func__, rmem->name,
		 &rmem->base, &rmem->size);

	/* init cma area */
	ret = cma_init_reserved_mem(rmem->base, rmem->size, 0, &cma);

	if (ret) {
		pr_err("%s cma failed, ret: %d\n", __func__, ret);
		return 1;
	}

#ifdef MEMORY_LOWPOWER_FULLNESS
	/* try to grab the last pageblock */
	memory_lowpower_fullness(rmem->base, rmem->size);
#endif

	return 0;
}
RESERVEDMEM_OF_DECLARE(memory_lowpower, "mediatek,memory-lowpower",
			memory_lowpower_init);

/* Return 0 if success, -1 or -2 if fail */
int __init acquire_buffer_from_memory_lowpower(phys_addr_t *addr)
{
#ifdef MEMORY_LOWPOWER_FULLNESS
	if (grab_lastsize != 0) {
		*addr = memblock_end_of_DRAM() - PAGE_SIZE;
		return 0;
	} else {
		return -1;
	}
#else
	return -2;
#endif
}

#ifdef CONFIG_ZONE_MOVABLE_CMA
static int __init memory_lowpower_sanity_test(void)
{
	/* Just return */
	return 0;
}
late_initcall(memory_lowpower_sanity_test);
#endif

#ifdef CONFIG_MTK_MEMORY_LOWPOWER_DEBUG
static int memory_lowpower_show(struct seq_file *m, void *v)
{
	phys_addr_t cma_base = cma_get_base(cma);
	phys_addr_t cma_end = cma_base + memory_lowpower_real_size();

	mutex_lock(&memory_lowpower_mutex);

	if (cma_pages)
		seq_printf(m, "cma collected cma_pages: %p\n", cma_pages);
	else
		seq_puts(m, "cma freed NULL\n");

	seq_printf(m, "cma info: [%pa-%pa] (0x%lx)\n",
			&cma_base, &cma_end,
			(unsigned long)memory_lowpower_real_size());
	seq_printf(m, "cma usage: %lu\n", cma_usage_count);

	mutex_unlock(&memory_lowpower_mutex);

	return 0;
}

static int memory_lowpower_open(struct inode *inode, struct file *file)
{
	return single_open(file, &memory_lowpower_show, NULL);
}

static ssize_t memory_lowpower_write(struct file *file,
		const char __user *buffer,
		size_t count, loff_t *ppos)
{
	static char state;

	if (count > 0) {
		if (get_user(state, buffer))
			return -EFAULT;
		state -= '0';
		pr_alert("%s state = %d\n", __func__, state);
		if (state) {
			/* collect cma */
			get_memory_lowpower_cma();
		} else {
			/* undo collection */
			put_memory_lowpower_cma();
		}
	}

	return count;
}

static const struct file_operations memory_lowpower_fops = {
	.open		= memory_lowpower_open,
	.write		= memory_lowpower_write,
	.read		= seq_read,
	.release	= single_release,
};

static int __init memory_lowpower_debug_init(void)
{
	struct dentry *dentry;

	if (!memory_lowpower_inited()) {
		pr_err("memory-lowpower cma is not inited\n");
		return 1;
	}

	dentry = debugfs_create_file("memory-lowpower", 0444, NULL, NULL,
			&memory_lowpower_fops);
	if (!dentry)
		pr_warn("Failed to create debugfs memory_lowpower_debug_init file\n");

	return 0;
}

late_initcall(memory_lowpower_debug_init);
#endif /* CONFIG_MTK_MEMORY_LOWPOWER_DEBUG */
