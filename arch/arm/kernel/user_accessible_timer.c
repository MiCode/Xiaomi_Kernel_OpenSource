/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <linux/mm.h>
#include <linux/sched.h>
#include <asm/user_accessible_timer.h>
#include <asm/traps.h>

#define USER_ACCESS_TIMER_OFFSET	0xf30
#define USER_ACCESS_FEATURE_OFFSET	0xf34
#define USER_ACCESS_FEATURE_FLAG	0xffff0f20

static struct vm_area_struct user_timers_vma;
static int __init user_timers_vma_init(void)
{
	user_timers_vma.vm_start        = CONFIG_ARM_USER_ACCESSIBLE_TIMER_BASE;
	user_timers_vma.vm_end          = CONFIG_ARM_USER_ACCESSIBLE_TIMER_BASE
						+ PAGE_SIZE;
	user_timers_vma.vm_page_prot    = PAGE_READONLY;
	user_timers_vma.vm_flags        = VM_READ | VM_MAYREAD;
	return 0;
}
arch_initcall(user_timers_vma_init);

int in_user_timers_area(struct mm_struct *mm, unsigned long addr)
{
	return (addr >= user_timers_vma.vm_start) &&
		(addr < user_timers_vma.vm_end);
}
EXPORT_SYMBOL(in_user_timers_area);

struct vm_area_struct *get_user_timers_vma(struct mm_struct *mm)
{
	return &user_timers_vma;
}
EXPORT_SYMBOL(get_user_timers_vma);

int get_user_timer_page(struct vm_area_struct *vma,
	struct mm_struct *mm, unsigned long start, unsigned int gup_flags,
	struct page **pages, int idx, int *goto_next_page)
{
	/* Replicates the earlier work done in mm/memory.c */
	unsigned long pg = start & PAGE_MASK;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	/* Unset this flag -- this only gets activated if the
	 * caller should go straight to the next_page label on
	 * return.
	 */
	*goto_next_page = 0;

	/* user gate pages are read-only */
	if (gup_flags & FOLL_WRITE)
		return idx ? : -EFAULT;
	if (pg > TASK_SIZE)
		pgd = pgd_offset_k(pg);
	else
		pgd = pgd_offset_gate(mm, pg);
	BUG_ON(pgd_none(*pgd));
	pud = pud_offset(pgd, pg);
	BUG_ON(pud_none(*pud));
	pmd = pmd_offset(pud, pg);
	if (pmd_none(*pmd))
		return idx ? : -EFAULT;
	VM_BUG_ON(pmd_trans_huge(*pmd));
	pte = pte_offset_map(pmd, pg);
	if (pte_none(*pte)) {
		pte_unmap(pte);
		return idx ? : -EFAULT;
	}
	vma = get_user_timers_vma(mm);
	if (pages) {
		struct page *page;

		page = vm_normal_page(vma, start, *pte);
		if (!page) {
			if (!(gup_flags & FOLL_DUMP) &&
				zero_pfn == pte_pfn(*pte))
				page = pte_page(*pte);
			else {
				pte_unmap(pte);
				return idx ? : -EFAULT;
			}
		}
		pages[idx] = page;
		get_page(page);
	}
	pte_unmap(pte);
	/* In this case, set the next page */
	*goto_next_page = 1;
	return 0;
}
EXPORT_SYMBOL(get_user_timer_page);

void setup_user_timer_offset(unsigned long addr)
{
#if defined(CONFIG_CPU_USE_DOMAINS)
	unsigned long vectors = CONFIG_VECTORS_BASE;
#else
	unsigned long vectors = (unsigned long)vectors_page;
#endif
	unsigned long *timer_offset = (unsigned long *)(vectors +
		USER_ACCESS_TIMER_OFFSET);
	*timer_offset = addr;
}
EXPORT_SYMBOL(setup_user_timer_offset);

void set_user_accessible_timer_flag(bool flag)
{
#if defined(CONFIG_CPU_USE_DOMAINS)
	unsigned long vectors = CONFIG_VECTORS_BASE;
#else
	unsigned long vectors = (unsigned long)vectors_page;
#endif
	unsigned long *timer_offset = (unsigned long *)(vectors +
		USER_ACCESS_FEATURE_OFFSET);
	*timer_offset = (flag ? USER_ACCESS_FEATURE_FLAG : 0);
}
EXPORT_SYMBOL(set_user_accessible_timer_flag);
