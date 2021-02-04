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
#include <linux/of_fdt.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/mm.h>
#include <linux/memory.h>
#include <linux/memblock.h>
#include <linux/oom.h>
#include <linux/swap.h>
#include <linux/sort.h>

#include <asm/setup.h>

#include <mt-plat/mtk_memcfg.h>
#include <mt-plat/mtk_memcfg_reserve_info.h>

#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
#endif

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

#ifdef CONFIG_MTK_ENG_BUILD
/* memblock reserve information */
static int mtk_memcfg_memblock_reserved_show(struct seq_file *m, void *v)
{
	int i = 0, j = 0, record_count = 0;
	unsigned long start, end, rstart, rend;
	unsigned long bt;
	struct memblock_stack_trace *trace;
	struct memblock_record *record;
	struct memblock_type *type = &memblock.reserved;
	struct memblock_region *region;
	unsigned long total_size = 0;

	record_count = min(memblock_reserve_count, MAX_MEMBLOCK_RECORD);

	for (i = 0; i < type->cnt; i++) {
		region = &type->regions[i];
		start = region->base;
		end = region->base + region->size;
		total_size += region->size;
		seq_printf(m, "region: %lx %lx-%lx\n",
			(unsigned long)region->size, start, end);
		for (j = 0; j < record_count; j++) {
			record = &memblock_record[j];
			trace = &memblock_stack_trace[j];
			rstart = record->base;
			rend = record->end;
			if ((rstart >= start && rstart < end) ||
				(rend > start && rend <= end) ||
				(rstart >= start && rend <= end)) {
				bt = trace->count - 3;
				seq_printf(m, "bt    : %lx %lx-%lx\n%pF %pF %pF %pF\n",
						(unsigned long)record->size,
						rstart, rend,
						(void *)trace->addrs[bt],
						(void *)trace->addrs[bt - 1],
						(void *)trace->addrs[bt - 2],
						(void *)trace->addrs[bt - 3]);
			}
		}
		seq_puts(m, "\n");
	}

	seq_printf(m, "Total memblock reserve count: %d\n",
		memblock_reserve_count);
	if (memblock_reserve_count >= MAX_MEMBLOCK_RECORD)
		seq_puts(m, "Total count > MAX_MEMBLOCK_RECORD\n");
	seq_printf(m, "Memblock reserve total size: 0x%lx\n", total_size);

	return 0;
}

static int mtk_memcfg_memblock_reserved_open(struct inode *inode,
	struct file *file)
{
	return single_open(file, mtk_memcfg_memblock_reserved_show, NULL);
}

static const struct file_operations mtk_memcfg_memblock_reserved_operations = {
	.open = mtk_memcfg_memblock_reserved_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif
/* end of memblock reserve information */

static const struct file_operations mtk_memcfg_memory_layout_operations = {
	.open = mtk_memcfg_memory_layout_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
/* end of kernel memory layout */

#ifndef CONFIG_MTK_GMO_RAM_OPTIMIZE
static bool vmpressure_no_trigger_warning(void) { return true; }
#else
static bool vmpressure_no_trigger_warning(void)
{
#define VMPRESSURE_CRITICAL	(40)

	unsigned long memory, memsw;

	memory = global_node_page_state(NR_INACTIVE_ANON) +
		 global_node_page_state(NR_ACTIVE_ANON) +
		 global_node_page_state(NR_INACTIVE_FILE) +
		 global_node_page_state(NR_ACTIVE_FILE) +
		 global_node_page_state(NR_UNEVICTABLE);

	memsw = memory + total_swap_pages -
		get_nr_swap_pages() -
		total_swapcache_pages();

	memory *= 100;
	memsw *= VMPRESSURE_CRITICAL;

	/* should trigger */
	if (memory < memsw)
		return false;

	return true;

#undef VMPRESSURE_CRITICAL
}
#endif

static unsigned long vmpressure_warn_timeout;
/* Inform system about vmpressure level */
void mtk_memcfg_inform_vmpressure(void)
{
#define OOM_SCORE_ADJ_NO_TRIGGER	(0)

	bool all_native = true;
	struct task_struct *p;

	if (vmpressure_no_trigger_warning())
		return;

	rcu_read_lock();
	for_each_process(p) {
		long adj;

		if (p->flags & PF_KTHREAD)
			continue;

		if (p->state & TASK_UNINTERRUPTIBLE)
			continue;

		get_task_struct(p);
		adj = (long)p->signal->oom_score_adj;
		put_task_struct(p);

		if (adj > OOM_SCORE_ADJ_NO_TRIGGER) {
			rcu_read_unlock();
			return;
		}

		if (adj == OOM_SCORE_ADJ_NO_TRIGGER)
			all_native = false;
	}
	rcu_read_unlock();

	if (all_native)
		return;

	if (time_before_eq(jiffies, vmpressure_warn_timeout))
		return;

	/* Trigger AEE warning */
	pr_info("%s: vmpressure trigger kernel warning\n", __func__);

#ifdef CONFIG_MTK_AEE_FEATURE
	aee_kernel_warning_api("LMK", 0,
			DB_OPT_DEFAULT | DB_OPT_DUMPSYS_ACTIVITY |
			DB_OPT_LOW_MEMORY_KILLER | DB_OPT_PID_MEMORY_INFO |
			DB_OPT_PROCESS_COREDUMP |
			DB_OPT_DUMPSYS_SURFACEFLINGER |
			DB_OPT_DUMPSYS_GFXINFO | DB_OPT_DUMPSYS_PROCSTATS,
			"Framework low memory\nCRDISPATCH_KEY:FLM_APAF",
			"please contact AP/AF memory module owner\n");
#endif

	vmpressure_warn_timeout = jiffies + 10 * HZ;

#undef OOM_SCORE_ADJ_NO_TRIGGER
}

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
		/* memblock reserved */
		entry = proc_create("memblock_reserved", 0644,
				mtk_memcfg_dir,
				&mtk_memcfg_memblock_reserved_operations);
		if (!entry)
			pr_info("create memblock_reserved proc entry failed\n");
		pr_info("create memblock_reserved proc entry success!!!!!\n");


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
