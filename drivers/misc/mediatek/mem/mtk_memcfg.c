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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/mm.h>
#include <linux/memory.h>
#include <linux/memblock.h>
#include <linux/sort.h>

#include <asm/setup.h>

#include <mt-plat/mtk_memcfg.h>
#include <mt-plat/mtk_memcfg_reserve_info.h>

/* kenerl memory fragmentation trigger */

static LIST_HEAD(frag_page_list);
static DEFINE_SPINLOCK(frag_page_list_lock);
static DEFINE_MUTEX(frag_task_lock);
static unsigned long mtk_memcfg_frag_round;
static struct kmem_cache *frag_page_cache;

/* kernel memory layout */
static void mtk_memcfg_show_layout_region(struct seq_file *m, const char *name,
		unsigned long long end, unsigned long long size,
		int nomap, int is_end)
{
	int i = 0;
	int name_length = strlen(name);
	int padding = (40 - name_length - 2) / 2;
	int odd = (40 - name_length - 2) % 2;

	seq_printf(m,
		"----------------------------------------  0x%08llx\n", end);
	seq_puts(m, "-");
	for (i = 0; i < padding; i++)
		seq_puts(m, " ");
	if (nomap) {
		seq_puts(m, "*");
		padding -= 1;
	}
	seq_printf(m, "%s", name);
	for (i = 0; i < padding + odd; i++)
		seq_puts(m, " ");
	seq_printf(m, "-  size : (0x%0llx)\n", size);

	if (is_end)
		seq_printf(m, "----------------------------------------  0x%0llx\n"
				, end - size);
}

static void mtk_memcfg_show_layout_region_kernel(struct seq_file *m,
		unsigned long long end, unsigned long long size)
{
	seq_printf(m,
		"----------------------------------------  0x%08llx\n", end);
	seq_printf(m,
		"-               kernel                 -  size : (0x%0llx)\n",
		size);
}

#define MAX_RESERVED_COUNT 30
static int mtk_memcfg_memory_layout_show(struct seq_file *m, void *v)
{
	int i, ret = 0;
	int count;
	struct reserved_mem_ext *reserved_mem = NULL;
	phys_addr_t dram_end = 0;
	struct reserved_mem_ext *rmem, *prmem;

	if (kptr_restrict == 2) {
		seq_puts(m, "Need kptr_restrict permission\n");
		goto debug_info;
	}

	count = get_reserved_mem_count();

	reserved_mem = kcalloc(MAX_RESERVED_COUNT,
				sizeof(struct reserved_mem_ext),
				GFP_KERNEL);
	if (!reserved_mem) {
		seq_puts(m, "Fail to allocate memory\n");
		return 0;
	}

	ret = memcfg_get_reserve_info(reserved_mem, count);
	if (ret) {
		pr_info("reserved_mem over limit!\n");
		kfree(reserved_mem);
		goto debug_info;
	}

	count = memcfg_remove_free_mem(reserved_mem, count);
	if (count <= 0 || count > MAX_RESERVED_REGIONS) {
		seq_printf(m, "count(%d) over limit after parsing!\n",
				count);
		kfree(reserved_mem);
		goto debug_info;
	}


	clear_reserve(reserved_mem, count, "zone-movable-cma-memory");

	sort(reserved_mem, count,
			sizeof(struct reserved_mem_ext),
			reserved_mem_ext_compare, NULL);

	seq_puts(m, "Reserve Memory Layout (prefix with \"*\" is no-map)\n");

	i = count - 1;
	rmem = &reserved_mem[i];
	dram_end = memblock_end_of_DRAM();

	if (dram_end > rmem->base + rmem->size) {
		mtk_memcfg_show_layout_region_kernel(m, dram_end,
		dram_end - (rmem->base + rmem->size));
	}

	for (i = count - 1; i >= 0; i--) {
		rmem = &reserved_mem[i];

		if (i == 0)
			prmem = NULL;
		else
			prmem = &reserved_mem[i - 1];

		if (rmem->size == 0 && rmem->base == 0)
			break;

		mtk_memcfg_show_layout_region(m, rmem->name,
				rmem->base + rmem->size,
				rmem->size, rmem->nomap, 0);

		if (prmem && prmem->base != 0 &&
			rmem->base > prmem->base + prmem->size) {

			mtk_memcfg_show_layout_region_kernel(m, rmem->base,
				rmem->base - (prmem->base + prmem->size));
		}
	}

	rmem = &reserved_mem[0];
	while (rmem->base == 0)
		rmem++;

	if (rmem->base != memblock_start_of_DRAM()) {
		unsigned long size = (rmem->base) - memblock_start_of_DRAM();

		mtk_memcfg_show_layout_region(m, "kernel",
				memblock_start_of_DRAM() + size,
				size, RESERVED_MAP, 1);
	}

	kfree(reserved_mem);

debug_info:
	seq_puts(m, "\n");
	seq_puts(m, "Debug Info:\n");
	seq_printf(m, "Memory: %lluK/%lluK available, %lluK kernel code, %lluK rwdata, %lluK rodata, %lluK init, %lluK bss, %lluK reserved"
#ifdef CONFIG_HIGHMEM
		", %lluK highmem"
#endif
		, kernel_reserve_meminfo.available >> 10
		, kernel_reserve_meminfo.total >> 10
		, kernel_reserve_meminfo.kernel_code >> 10
		, kernel_reserve_meminfo.rwdata >> 10
		, kernel_reserve_meminfo.rodata >> 10
		, kernel_reserve_meminfo.init >> 10
		, kernel_reserve_meminfo.bss >> 10
		, kernel_reserve_meminfo.reserved >> 10
#ifdef CONFIG_HIGHMEM
		, kernel_reserve_meminfo.highmem >> 10
#endif
		);
	seq_puts(m, "\n");

#ifdef CONFIG_SPARSEMEM_VMEMMAP
	seq_printf(m, "vmemmap : 0x%16lx - 0x%16lx   (%6ld MB actual)\n",
			(unsigned long)virt_to_page(PAGE_OFFSET),
			(unsigned long)virt_to_page(high_memory),
			((unsigned long)virt_to_page(high_memory) -
			 (unsigned long)virt_to_page(PAGE_OFFSET)) >> 20);
#else
#ifndef CONFIG_NEED_MULTIPLE_NODES
	seq_printf(m, "memmap : %lu MB\n", mem_map_size >> 20);
#endif
#endif
	return 0;
}

static int mtk_memcfg_memory_layout_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_memcfg_memory_layout_show, NULL);
}

static const struct file_operations mtk_memcfg_memory_layout_operations = {
	.open = mtk_memcfg_memory_layout_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
/* end of kernel memory layout */

struct frag_page {
	struct list_head list;
	struct page *page;
};

static int mtk_memcfg_frag_show(struct seq_file *m, void *v)
{
	int cnt = 0;
	struct frag_page *frag_page, *n_frag_page;

	spin_lock(&frag_page_list_lock);
	list_for_each_entry_safe(frag_page, n_frag_page,
			&frag_page_list, list) {
		cnt++;
	}
	spin_unlock(&frag_page_list_lock);
	seq_printf(m, "round: %lu, fragmentation-trigger held %d pages, %d MB\n",
		   mtk_memcfg_frag_round,
		   cnt, (cnt << PAGE_SHIFT) >> 20);

	return 0;
}

static int mtk_memcfg_frag_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_memcfg_frag_show, NULL);
}

static int do_fragmentation(void *n)
{
	struct frag_page *frag_page, *n_frag_page;
	struct page *page;
	gfp_t gfp_mask = GFP_ATOMIC;
	unsigned int max_order = 2;
	int cnt = 0, i;

	/* trigger fragmentation */
	/*
	 * Allocate an order-2-page, split it into 4 order-0-pages,
	 * and free 3 of them, repeatedly.
	 * In this way, we split all high order pages to
	 * order-0-pages and order-1-pages to create a
	 * fragmentation scenario.
	 *
	 * In current stage, we only trigger fragmentation in
	 * normal zone.
	 */
	while (1) {
#if 1
		if (cnt >= 10000) {
			/*
			 * release all memory and restart the fragmentation
			 * Allocating too much frag_page consumes
			 * too mush order-0 pages
			 */
			spin_lock(&frag_page_list_lock);
			list_for_each_entry_safe(frag_page, n_frag_page,
						 &frag_page_list, list) {
				list_del(&frag_page->list);
				__free_page(frag_page->page);
				kmem_cache_free(frag_page_cache, frag_page);
				cnt--;
			}
			spin_unlock(&frag_page_list_lock);
			pr_info("round: %lu, fragmentation-trigger free pages %d left\n",
				 mtk_memcfg_frag_round, cnt);
		}
#endif
		while (1) {
			frag_page = kmem_cache_alloc(frag_page_cache, gfp_mask);
			if (!frag_page)
				break;
			page = alloc_pages(gfp_mask, max_order);
			if (!page) {
				kfree(frag_page);
				break;
			}
			split_page(page, 0);
			INIT_LIST_HEAD(&frag_page->list);
			frag_page->page = page;
			spin_lock(&frag_page_list_lock);
			list_add(&frag_page->list, &frag_page_list);
			spin_unlock(&frag_page_list_lock);
			for (i = 1; i < (1 << max_order); i++)
				__free_page(page + i);
			cnt++;
		}
		mtk_memcfg_frag_round++;
		pr_info("round: %lu, fragmentation-trigger allocate %d pages %d MB\n",
			 mtk_memcfg_frag_round, cnt, (cnt << PAGE_SHIFT) >> 20);
		msleep(500);
	}

	return 0;
}

static ssize_t
mtk_memcfg_frag_write(struct file *file, const char __user *buffer,
		      size_t count, loff_t *pos)
{
	static char state;
	static struct task_struct *p;

	if (count > 0) {
		if (get_user(state, buffer))
			return -EFAULT;
		state -= '0';
		pr_info("%s state = %d\n", __func__, state);

		mutex_lock(&frag_task_lock);
		if (state && !p) {
			pr_info("activate do_fragmentation kthread\n");
			p = kthread_create(do_fragmentation, NULL,
					   "fragmentationd");
			if (!IS_ERR(p))
				wake_up_process(p);
			else
				p = 0;
		}
		mutex_unlock(&frag_task_lock);
	}
	return count;
}

static const struct file_operations mtk_memcfg_frag_operations = {
	.open = mtk_memcfg_frag_open,
	.write = mtk_memcfg_frag_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/* end of kenerl memory fragmentation trigger */

/* kenerl out-of-memory(oom) trigger */

static int mtk_memcfg_oom_show(struct seq_file *m, void *v)
{
	seq_puts(m, "oom-trigger\n");

	return 0;
}

static int mtk_memcfg_oom_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_memcfg_oom_show, NULL);
}

static void oom_reboot(unsigned long data)
{
	BUG();
}

static ssize_t
mtk_memcfg_oom_write(struct file *file, const char __user *buffer,
		      size_t count, loff_t *pos)
{
	static char state;
	struct timer_list timer;

	/* oom may cause system hang, reboot after 60 sec */
	init_timer(&timer);
	timer.function = oom_reboot;
	timer.expires = jiffies + 300 * HZ;
	add_timer(&timer);

	if (count > 0) {
		if (get_user(state, buffer))
			return -EFAULT;
		state -= '0';
		pr_info("%s state = %d\n", __func__, state);
		if (state) {
			pr_info("oom test, trying to kill system under oom scenario\n");
			/* exhaust all memory */
			for (;;)
				alloc_pages(GFP_HIGHUSER_MOVABLE, 0);
		}
	}
	return count;
}

static const struct file_operations mtk_memcfg_oom_operations = {
	.open = mtk_memcfg_oom_open,
	.write = mtk_memcfg_oom_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/* end of kenerl out-of-memory(oom) trigger */

#ifdef CONFIG_SLUB_DEBUG
/* kenerl slabtrace  */
static const struct file_operations proc_slabtrace_operations = {
	.open = slabtrace_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/* end of kernel slabtrace */
#endif
static int __init mtk_memcfg_late_init(void)
{
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtk_memcfg_dir = NULL;

	mtk_memcfg_dir = proc_mkdir("mtk_memcfg", NULL);

	if (!mtk_memcfg_dir) {
		pr_info("[%s]: mkdir /proc/mtk_memcfg failed\n", __func__);
	} else {
		/* display kernel memory layout */
		entry = proc_create("memory_layout",
				    0644, mtk_memcfg_dir,
				    &mtk_memcfg_memory_layout_operations);

		if (!entry)
			pr_info("create memory_layout proc entry failed\n");

		mtk_memcfg_reserve_info_init(mtk_memcfg_dir);

#ifdef CONFIG_MTK_ENG_BUILD

		/* fragmentation test */
		entry = proc_create("frag-trigger",
				    0644, mtk_memcfg_dir,
				    &mtk_memcfg_frag_operations);

		if (!entry)
			pr_info("create frag-trigger proc entry failed\n");

		frag_page_cache = kmem_cache_create("frag_page_cache",
						    sizeof(struct frag_page),
						    0, SLAB_PANIC, NULL);

		if (!frag_page_cache)
			pr_info("create frag_page_cache failed\n");

		/* oom test */
		entry = proc_create("oom-trigger",
				    0644, mtk_memcfg_dir,
				    &mtk_memcfg_oom_operations);

		if (!entry)
			pr_info("create oom entry failed\n");

#ifdef CONFIG_SLUB_DEBUG
		/* slabtrace - full slub object backtrace */
		entry = proc_create("slabtrace",
				    0400, mtk_memcfg_dir,
				    &proc_slabtrace_operations);

		if (!entry)
			pr_info("create slabtrace proc entry failed\n");
#endif
#endif /* end of CONFIG_MTK_ENG_BUILD */
	}

	return 0;
}

static int __init mtk_memcfg_init(void)
{
	return 0;
}

static void __exit mtk_memcfg_exit(void)
{
}
module_init(mtk_memcfg_init);
module_exit(mtk_memcfg_exit);
late_initcall(mtk_memcfg_late_init);
