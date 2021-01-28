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
#include <mt-plat/mtk_meminfo.h>

#ifdef CONFIG_OF
static u64 mntl_base;
static u64 mntl_size;

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

#endif /* end of CONFIG_OF */

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
