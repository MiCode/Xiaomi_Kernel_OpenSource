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

#include <asm/page.h>
#include <asm/setup.h>
#include <asm/pgtable.h>
#include <linux/module.h>
#include <linux/of_fdt.h>
#include <linux/vmalloc.h>
#include <linux/mm_types.h>
#include <linux/page-flags.h>
#include <mt-plat/mtk_memcfg.h>
#include <mt-plat/mtk_meminfo.h>

#ifdef CONFIG_OF
/* return the actual physical DRAM size */
static u64 kernel_mem_sz;
static u64 phone_dram_sz;	/* original phone DRAM size */
static u64 mntl_base;
static u64 mntl_size;
static int __init dt_scan_memory(unsigned long node, const char *uname,
				int depth, void *data)
{
	const char *type = of_get_flat_dt_prop(node, "device_type", NULL);
	int i;
	const __be32 *reg, *endp;
	int l;
	struct dram_info *dram_info;

	/* We are scanning "memory" nodes only */
	if (type == NULL) {
		/*
		 * The longtrail doesn't have a device_type on the
		 * /memory node, so look for the node called /memory@0.
		 */
		if (depth != 1 || strcmp(uname, "memory@0") != 0)
			return 0;
	} else if (strcmp(type, "memory") != 0) {
		return 0;
	}

	/*
	 * Use kernel_mem_sz if phone_dram_sz is not available (workaround)
	 * Projects use device tree should have orig_dram_info entry in their
	 * device tree.
	 * After the porting is done, kernel_mem_sz will be removed.
	 */
	reg = of_get_flat_dt_prop(node, "reg", &l);
	if (reg == NULL)
		return 0;

	endp = reg + (l / sizeof(__be32));
	while ((endp - reg) >= (dt_root_addr_cells + dt_root_size_cells)) {
		u64 base, size;

		base = dt_mem_next_cell(dt_root_addr_cells, &reg);
		size = dt_mem_next_cell(dt_root_size_cells, &reg);

		if (size == 0)
			continue;

		kernel_mem_sz += size;
	}

	/* orig_dram_info */
	dram_info = (struct dram_info *)of_get_flat_dt_prop(node,
			"orig_dram_info", NULL);
	if (dram_info) {
		for (i = 0; i < dram_info->rank_num; i++)
			phone_dram_sz += dram_info->rank_info[i].size;
	}

	return node;
}

static int __init init_get_max_DRAM_size(void)
{
	if (!phone_dram_sz && !kernel_mem_sz) {
		if (of_scan_flat_dt(dt_scan_memory, NULL)) {
			pr_info("init_get_max_DRAM_size done. phone_dram_sz: 0x%llx, kernel_mem_sz: 0x%llx\n",
				 (unsigned long long)phone_dram_sz,
				 (unsigned long long)kernel_mem_sz);
		} else {
			pr_info("init_get_max_DRAM_size fail\n");
			BUG();
		}
	}
	return 0;
}

phys_addr_t get_max_DRAM_size(void)
{
	if (!phone_dram_sz && !kernel_mem_sz) {
		pr_info("%s is called too early\n", __func__);
		BUG();
	}
	return phone_dram_sz ?
		(phys_addr_t)phone_dram_sz : (phys_addr_t)kernel_mem_sz;
}
early_initcall(init_get_max_DRAM_size);

static int __init __fdt_scan_reserved_mem(unsigned long node, const char *uname,
					  int depth, void *data)
{
	static int found;
	const __be32 *reg, *endp;
	int l;

	if (!found && depth == 1 && strcmp(uname, "reserved-memory") == 0) {
		found = 1;
		/* scan next node */
		return 0;
	} else if (!found) {
		/* scan next node */
		return 0;
	} else if (found && depth < 2) {
		/* scanning of /reserved-memory has been finished */
		return 1;
	}

	if (!strstr(uname, "KOBuffer"))
		return 0;

	reg = of_get_flat_dt_prop(node, "reg", &l);
	if (reg == NULL)
		return 0;

	endp = reg + (l / sizeof(__be32));
	while ((endp - reg) >= (dt_root_addr_cells + dt_root_size_cells)) {
		mntl_base = dt_mem_next_cell(dt_root_addr_cells, &reg);
		mntl_size = dt_mem_next_cell(dt_root_size_cells, &reg);
	}

	return 0;
}

static int __init init_fdt_mntl_buf(void)
{
	of_scan_flat_dt(__fdt_scan_reserved_mem, NULL);

	return 0;
}
early_initcall(init_fdt_mntl_buf);

int get_mntl_buf(u64 *base, u64 *size)
{
	*base = mntl_base;
	*size = mntl_size;

	return 0;
}

#else
phys_addr_t get_max_DRAM_size(void)
{
	return mtk_get_max_DRAM_size();
}
#endif /* end of CONFIG_OF */
EXPORT_SYMBOL(get_max_DRAM_size);

/*
 * Return the DRAM size used by Linux kernel.
 * In current stage, use phone DRAM size directly
 */
phys_addr_t get_memory_size(void)
{
	return get_max_DRAM_size();
}
EXPORT_SYMBOL(get_memory_size);

phys_addr_t get_phys_offset(void)
{
	return PHYS_OFFSET;
}
EXPORT_SYMBOL(get_phys_offset);

phys_addr_t get_zone_movable_cma_base(void)
{
#ifdef CONFIG_MTK_MEMORY_LOWPOWER
	return memory_lowpower_base();
#endif /* end CONFIG_MTK_MEMORY_LOWPOWER */
	return (~(phys_addr_t)0);
}

phys_addr_t get_zone_movable_cma_size(void)
{
#ifdef CONFIG_MTK_MEMORY_LOWPOWER
	return memory_lowpower_size();
#endif /* end CONFIG_MTK_MEMORY_LOWPOWER */
	return 0;
}

/**
 *	vmap_reserved_mem - map reserved memory into virtually contiguous space
 *	@start:		start of reserved memory
 *	@size:		size of reserved memory
 *	@prot:		page protection for the mapping
 */
void *vmap_reserved_mem(phys_addr_t start, phys_addr_t size, pgprot_t prot)
{
	long i;
	long page_count;
	unsigned long pfn;
	void *vaddr = NULL;
	phys_addr_t addr = start;
	struct page *page;
	struct page **pages;

	page_count = DIV_ROUND_UP(size, PAGE_SIZE);
	pages = vmalloc(page_count * sizeof(struct page *));

	if (!pages)
		return NULL;

	for (i = 0; i < page_count; i++) {
		pfn = __phys_to_pfn(addr);
		if (unlikely(!pfn_valid(pfn)))
			goto out_free;

		page = pfn_to_page(pfn);
		if (unlikely(!PageReserved(page)))
			goto out_free;

		pages[i] = page;
		addr += PAGE_SIZE;
	}

	vaddr = vmap(pages, page_count, VM_MAP, prot);

out_free:
	vfree(pages);
	return vaddr;
}
EXPORT_SYMBOL(vmap_reserved_mem);
