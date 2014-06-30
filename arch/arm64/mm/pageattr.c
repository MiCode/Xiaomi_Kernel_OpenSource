/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>

#include <asm/pgtable.h>
#include <asm/tlbflush.h>

static pte_t clear_pte_bit(pte_t pte, pgprot_t prot)
{
	pte_val(pte) &= ~pgprot_val(prot);
	return pte;
}

static pte_t set_pte_bit(pte_t pte, pgprot_t prot)
{
	pte_val(pte) |= pgprot_val(prot);
	return pte;
}

static int __change_memory(pte_t *ptep, pgtable_t token, unsigned long addr,
			pgprot_t prot, bool set)
{
	pte_t pte;

	if (set)
		pte = set_pte_bit(*ptep, prot);
	else
		pte = clear_pte_bit(*ptep, prot);
	set_pte(ptep, pte);
	return 0;
}

static int set_page_range(pte_t *ptep, pgtable_t token, unsigned long addr,
			void *data)
{
	pgprot_t prot = (pgprot_t)data;

	return __change_memory(ptep, token, addr, prot, true);
}

static int clear_page_range(pte_t *ptep, pgtable_t token, unsigned long addr,
			void *data)
{
	pgprot_t prot = (pgprot_t)data;

	return __change_memory(ptep, token, addr, prot, false);
}

static int change_memory_common(unsigned long addr, int numpages,
				pgprot_t prot, bool set)
{
	unsigned long start = addr;
	unsigned long size = PAGE_SIZE*numpages;
	unsigned long end = start + size;
	int ret;

	if (!IS_ENABLED(CONFIG_FORCE_PAGES)) {
		if (start < MODULES_VADDR || start >= MODULES_END)
			return -EINVAL;

		if (end < MODULES_VADDR || end >= MODULES_END)
			return -EINVAL;
	}

	if (set)
		ret = apply_to_page_range(&init_mm, start, size,
					set_page_range, (void *)prot);
	else
		ret = apply_to_page_range(&init_mm, start, size,
					clear_page_range, (void *)prot);

	flush_tlb_kernel_range(start, end);
	isb();
	return ret;
}

static int change_memory_set_bit(unsigned long addr, int numpages,
					pgprot_t prot)
{
	return change_memory_common(addr, numpages, prot, true);
}

static int change_memory_clear_bit(unsigned long addr, int numpages,
					pgprot_t prot)
{
	return change_memory_common(addr, numpages, prot, false);
}

int set_memory_ro(unsigned long addr, int numpages)
{
	return change_memory_set_bit(addr, numpages, __pgprot(PTE_RDONLY));
}
EXPORT_SYMBOL(set_memory_ro);

int set_memory_rw(unsigned long addr, int numpages)
{
	return change_memory_clear_bit(addr, numpages, __pgprot(PTE_RDONLY));
}
EXPORT_SYMBOL(set_memory_rw);

int set_memory_nx(unsigned long addr, int numpages)
{
	return change_memory_set_bit(addr, numpages, __pgprot(PTE_PXN));
}
EXPORT_SYMBOL(set_memory_nx);

int set_memory_x(unsigned long addr, int numpages)
{
	return change_memory_clear_bit(addr, numpages, __pgprot(PTE_PXN));
}
EXPORT_SYMBOL(set_memory_x);
