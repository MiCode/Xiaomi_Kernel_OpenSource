// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/file.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>       /* min() */
#include <linux/memblock.h>
#include <linux/suspend.h>
#include <asm/cacheflush.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/seq_file.h>
#include <asm/setup.h>
#include <linux/mm.h>
#include <asm/page.h>
#include <linux/atomic.h>
#include <linux/page_owner.h>
#include <linux/irq.h>
#include <linux/syscore_ops.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <asm/pgtable.h>
#include <linux/vmalloc.h>
#include <linux/page-flags.h>
#include <linux/mmzone.h>
#include <linux/cpu.h>
#include "ccci_util_lib_vmap_reserved_mem.h"


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
		page = pfn_to_page(pfn);
		pages[i] = page;
		addr += PAGE_SIZE;
	}

	vaddr = vmap(pages, page_count, VM_MAP, prot);
	vfree(pages);
	return vaddr;
}
EXPORT_SYMBOL(vmap_reserved_mem);
