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
#include <asm/pgtable.h>
#include <linux/vmalloc.h>
#include <linux/page-flags.h>

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
