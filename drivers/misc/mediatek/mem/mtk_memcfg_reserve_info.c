/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/seq_file.h>
#include <linux/memblock.h>
#include <linux/of_fdt.h>
#include <linux/of.h>
#include <linux/sort.h>
#include <linux/slab.h>
#include <asm/setup.h>
#include <linux/mm.h>
#include <mt-plat/mtk_memcfg.h>
#include <mt-plat/mtk_memcfg_reserve_info.h>

struct kernel_reserve_meminfo kernel_reserve_meminfo;

#define DRAM_ALIGN_SIZE 0x20000000
static unsigned long long get_dram_size(void)
{
	phys_addr_t dram_start, dram_end, dram_size;

	dram_start = __ALIGN_MASK(memblock_start_of_DRAM(), DRAM_ALIGN_SIZE);
	dram_end = ALIGN(memblock_end_of_DRAM(), DRAM_ALIGN_SIZE);
	dram_size = dram_end - dram_start;

	return (unsigned long long)dram_size;
}

static unsigned long long get_memtotal(void)
{
	struct sysinfo i;
	unsigned long long memtotal;

	si_meminfo(&i);
	memtotal = (unsigned long long)i.totalram << PAGE_SHIFT;

	return memtotal;
}

struct freed_reserved_memory freed_reserved_memory[MAX_FREE_RESERVED];
int freed_reserved_memory_count;

void mtk_memcfg_record_freed_reserved(phys_addr_t start, phys_addr_t end)
{
	pr_info("free_reserved_memory: 0x%lx ~ 0x%lx\n", (unsigned long)start,
			(unsigned long)end);
	if (freed_reserved_memory_count < MAX_FREE_RESERVED) {
		freed_reserved_memory[freed_reserved_memory_count].start
			= start;
		freed_reserved_memory[freed_reserved_memory_count].end
			= end;
		freed_reserved_memory_count++;
	} else {
		pr_info("freed_reserved_memory_count over limit %d\n",
				MAX_FREE_RESERVED);
	}
}

int reserved_mem_ext_compare(const void *p1, const void *p2)
{
	if (((struct reserved_mem_ext *)p1)->base >
			((struct reserved_mem_ext *)p2)->base)
		return 1;
	return -1;
}

int freed_reserved_memory_compare(const void *p1, const void *p2)
{
	if (((struct freed_reserved_memory *)p1)->start >
			((struct freed_reserved_memory *)p2)->start)
		return 1;
	return -1;
}

void clear_reserve(struct reserved_mem_ext *rmem, int count, const char *name)
{
	int i = 0;

	for (i = 0; i < count; i++) {
		if (strcmp(name, (const char *)rmem[i].name) == 0) {
			rmem[i].size = 0;
			rmem[i].base = 0;
		}
	}
}

static void merge_same_reserved_memory(struct reserved_mem_ext *rmem, int count)
{
	int cur, tmp;
	struct reserved_mem_ext *cur_mem, *tmp_mem;

	for (cur = 0; cur < count; cur++) {
		cur_mem = &rmem[cur];
		for (tmp = cur + 1; tmp < count; tmp++) {
			tmp_mem = &rmem[tmp];
			if (!strcmp(cur_mem->name, tmp_mem->name)) {
				cur_mem->size += tmp_mem->size;
				tmp_mem->size = 0;
			}
		}
	}
}

int memcfg_get_reserve_info(struct reserved_mem_ext *reserved_mem, int count)
{
	int i = 0;
	struct reserved_mem *tmp;

	for (i = 0; i < count; i++) {
		tmp = get_reserved_mem(i);
		if (!tmp)
			return -1;
		reserved_mem[i].name = tmp->name;
		reserved_mem[i].base = tmp->base;
		reserved_mem[i].size = tmp->size;
		if (pfn_valid(__phys_to_pfn(reserved_mem[i].base)))
			reserved_mem[i].nomap = RESERVED_MAP;
		else
			reserved_mem[i].nomap = RESERVED_NOMAP;
	}
	return 0;
}

static int memcfg_add_reserve_mem(struct reserved_mem_ext *reserved_mem,
		int count, unsigned long start, unsigned long end,
		const char *name, int nomap)
{
	struct reserved_mem_ext *tmp = &reserved_mem[count];

	count += 1;

	if (count > MAX_RESERVED_REGIONS)
		return -1;

	tmp->base = start;
	tmp->size = end - start;
	tmp->name = name;
	tmp->nomap = nomap;

	return count;
}

int memcfg_remove_free_mem(struct reserved_mem_ext *rmem, int count)
{
	int reserved_count = count;
	int index, f_idx;
	unsigned long start, end, fstart, fend;
	struct reserved_mem_ext *mem;
	struct freed_reserved_memory *fmem;

	sort(freed_reserved_memory, freed_reserved_memory_count,
			sizeof(struct freed_reserved_memory),
			freed_reserved_memory_compare, NULL);

	for (index = 0; index < count; index++) {
		mem = &rmem[index];
		start = mem->base;
		end = mem->base + mem->size;

		if (start > end) {
			pr_info("start %lx > end %lx\n", start, end);
			pr_info("reserved_mem: %s, base: %llu, size: %llu, nomap: %d\n",
					mem->name,
					mem->base,
					mem->size,
					mem->nomap);
			WARN_ON(start > end);
		}

		for (f_idx = 0; f_idx < freed_reserved_memory_count; f_idx++) {
			fmem = &freed_reserved_memory[f_idx];
			fstart = (unsigned long)fmem->start;
			fend = (unsigned long)fmem->end;
			if (fstart >= start && fstart < end) {
				reserved_count = memcfg_add_reserve_mem(rmem,
					reserved_count,	start, fstart,
					mem->name, RESERVED_MAP);
				if (reserved_count == -1)
					return -1;
				start = fend;
			}
		}
		if (start != mem->base) {
			if (start != (mem->base + mem->size))
				reserved_count = memcfg_add_reserve_mem(rmem,
						reserved_count,	start,
						mem->base + mem->size,
						mem->name, RESERVED_MAP);
			if (reserved_count == -1)
				return -1;
			mem->base = 0;
			mem->size = 0;
		}
	}
	return reserved_count;
}

/* kernel total reserve */
static int mtk_memcfg_total_reserve_show(struct seq_file *m, void *v)
{
	unsigned long long dram_size, memtotal;

#define K(x) ((x) >> (10))
	memtotal = get_memtotal();
	dram_size = get_dram_size();

	seq_printf(m, "%llu kB\n", K(dram_size) - K(memtotal));

	return 0;
}

static int mtk_memcfg_total_reserve_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_memcfg_total_reserve_show, NULL);
}

static const struct file_operations mtk_memcfg_total_reserve_operations = {
	.open = mtk_memcfg_total_reserve_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
/* end of kernel total reserve*/

/* kernel reserve memory information */
static int mtk_memcfg_reserve_memory_show(struct seq_file *m, void *v)
{
	int i, ret = 0;
	int reserve_count;
	unsigned long long dram_size, memtotal, kernel_other, vmemmap_actual;
	unsigned long long non_kernel_reserve = 0;
	struct reserved_mem_ext *reserved_mem = NULL;
	struct reserved_mem_ext *tmp = NULL;

	reserved_mem = kcalloc(MAX_RESERVED_REGIONS,
				sizeof(struct reserved_mem_ext),
				GFP_KERNEL);
	if (!reserved_mem) {
		seq_puts(m, "Can't get memory to parse reserve memory.(Is it OOM?)\n");
		return 0;
	}

	reserve_count = get_reserved_mem_count();
	ret = memcfg_get_reserve_info(reserved_mem, reserve_count);
	if (ret) {
		pr_info("Can't get reserve memory.\n");
		kfree(reserved_mem);
		return 0;
	}

	reserve_count = memcfg_remove_free_mem(reserved_mem, reserve_count);
	if (reserve_count <= 0 || reserve_count > MAX_RESERVED_REGIONS) {
		seq_printf(m, "reserve_count(%d) over limit after parsing!\n",
				reserve_count);
		kfree(reserved_mem);
		return 0;
	}

	clear_reserve(reserved_mem, reserve_count, "zone-movable-cma-memory");
	merge_same_reserved_memory(reserved_mem, reserve_count);

	sort(reserved_mem, reserve_count,
			sizeof(struct reserved_mem_ext),
			reserved_mem_ext_compare, NULL);

	for (i = 0; i < reserve_count; i++) {
		tmp = &reserved_mem[i];
		if (tmp->size != 0) {
			non_kernel_reserve += tmp->size;
			if (tmp->nomap || strstr(tmp->name, "ccci"))
				seq_puts(m, "*");
			seq_printf(m, "%s: %llu\n", tmp->name, tmp->size);
		}
	}

	kfree(reserved_mem);

	memtotal = get_memtotal();
	dram_size = get_dram_size();
	vmemmap_actual = (unsigned long)virt_to_page(high_memory) -
			 (unsigned long)phys_to_page(memblock_start_of_DRAM());
	kernel_other = dram_size - memtotal - non_kernel_reserve -
		       kernel_reserve_meminfo.kernel_code -
		       kernel_reserve_meminfo.rwdata -
		       kernel_reserve_meminfo.rodata -
		       kernel_reserve_meminfo.bss -
		       vmemmap_actual;

	seq_printf(m, "kernel(text): %llu\n",
			kernel_reserve_meminfo.kernel_code);
	seq_printf(m, "kernel(data): %llu\n", kernel_reserve_meminfo.rwdata +
					      kernel_reserve_meminfo.rodata +
					      kernel_reserve_meminfo.bss);
	seq_printf(m, "kernel(page): %llu\n", vmemmap_actual);
	seq_printf(m, "kernel(other): %llu\n", kernel_other);

	return 0;
}

static int mtk_memcfg_reserve_memory_open(struct inode *inode,
					struct file *file)
{
	return single_open(file, mtk_memcfg_reserve_memory_show, NULL);
}

static const struct file_operations mtk_memcfg_reserve_memory_operations = {
	.open = mtk_memcfg_reserve_memory_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
/* end of kernel reserve memory */

int __init mtk_memcfg_reserve_info_init(struct proc_dir_entry *mtk_memcfg_dir)
{
	struct proc_dir_entry *entry = NULL;

	if (!mtk_memcfg_dir) {
		pr_info("/proc/mtk_memcfg not exist");
		return 0;
	}

	entry = proc_create("total_reserve",
			    0644, mtk_memcfg_dir,
			    &mtk_memcfg_total_reserve_operations);
	if (!entry)
		pr_info("create total_reserve_memory proc entry failed\n");

	entry = proc_create("reserve_memory",
			    0644, mtk_memcfg_dir,
			    &mtk_memcfg_reserve_memory_operations);
	if (!entry)
		pr_info("create reserve_memory proc entry failed\n");

	return 0;
}
