/*
 * kernel/power/tuxonice_pagedir.c
 *
 * Copyright (C) 1998-2001 Gabor Kuti <seasons@fornax.hu>
 * Copyright (C) 1998,2001,2002 Pavel Machek <pavel@suse.cz>
 * Copyright (C) 2002-2003 Florent Chabaud <fchabaud@free.fr>
 * Copyright (C) 2006-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 *
 * Routines for handling pagesets.
 * Note that pbes aren't actually stored as such. They're stored as
 * bitmaps and extents.
 */

#include <linux/suspend.h>
#include <linux/highmem.h>
#include <linux/bootmem.h>
#include <linux/hardirq.h>
#include <linux/sched.h>
#include <linux/cpu.h>
#include <asm/tlbflush.h>

#include "tuxonice_pageflags.h"
#include "tuxonice_ui.h"
#include "tuxonice_pagedir.h"
#include "tuxonice_prepare_image.h"
#include "tuxonice.h"
#include "tuxonice_builtin.h"
#include "tuxonice_alloc.h"

static int ptoi_pfn;
static struct pbe *this_low_pbe;
static struct pbe **last_low_pbe_ptr;

void toi_reset_alt_image_pageset2_pfn(void)
{
	memory_bm_position_reset(pageset2_map);
}

static struct page *first_conflicting_page;

/*
 * free_conflicting_pages
 */

static void free_conflicting_pages(void)
{
	while (first_conflicting_page) {
		struct page *next = *((struct page **)kmap(first_conflicting_page));
		kunmap(first_conflicting_page);
		toi__free_page(29, first_conflicting_page);
		first_conflicting_page = next;
	}
}

/* __toi_get_nonconflicting_page
 *
 * Description: Gets order zero pages that won't be overwritten
 *		while copying the original pages.
 */

struct page *___toi_get_nonconflicting_page(int can_be_highmem)
{
	struct page *page;
	gfp_t flags = TOI_ATOMIC_GFP;
	if (can_be_highmem)
		flags |= __GFP_HIGHMEM;


	if (test_toi_state(TOI_LOADING_ALT_IMAGE) && pageset2_map && (ptoi_pfn != BM_END_OF_MAP)) {
		do {
			ptoi_pfn = memory_bm_next_pfn(pageset2_map);
			if (ptoi_pfn != BM_END_OF_MAP) {
				page = pfn_to_page(ptoi_pfn);
				if (!PagePageset1(page) && (can_be_highmem || !PageHighMem(page)))
					return page;
			}
		} while (ptoi_pfn != BM_END_OF_MAP);
	}

	do {
		page = toi_alloc_page(29, flags);
		if (!page) {
			printk(KERN_INFO "Failed to get nonconflicting " "page.\n");
			return NULL;
		}
		if (PagePageset1(page)) {
			struct page **next = (struct page **)kmap(page);
			*next = first_conflicting_page;
			first_conflicting_page = page;
			kunmap(page);
		}
	} while (PagePageset1(page));

	return page;
}

unsigned long __toi_get_nonconflicting_page(void)
{
	struct page *page = ___toi_get_nonconflicting_page(0);
	return page ? (unsigned long)page_address(page) : 0;
}

static struct pbe *get_next_pbe(struct page **page_ptr, struct pbe *this_pbe, int highmem)
{
	if (((((unsigned long)this_pbe) & (PAGE_SIZE - 1))
	     + 2 * sizeof(struct pbe)) > PAGE_SIZE) {
		struct page *new_page = ___toi_get_nonconflicting_page(highmem);
		if (!new_page)
			return ERR_PTR(-ENOMEM);
		this_pbe = (struct pbe *)kmap(new_page);
		memset(this_pbe, 0, PAGE_SIZE);
		*page_ptr = new_page;
	} else
		this_pbe++;

	return this_pbe;
}

/**
 * get_pageset1_load_addresses - generate pbes for conflicting pages
 *
 * We check here that pagedir & pages it points to won't collide
 * with pages where we're going to restore from the loaded pages
 * later.
 *
 * Returns:
 *	Zero on success, one if couldn't find enough pages (shouldn't
 *	happen).
 **/
int toi_get_pageset1_load_addresses(void)
{
	int pfn, highallocd = 0, lowallocd = 0;
	int low_needed = pagedir1.size - get_highmem_size(pagedir1);
	int high_needed = get_highmem_size(pagedir1);
	int low_pages_for_highmem = 0;
	gfp_t flags = GFP_ATOMIC | __GFP_NOWARN | __GFP_HIGHMEM;
	struct page *page, *high_pbe_page = NULL, *last_high_pbe_page = NULL,
	    *low_pbe_page, *last_low_pbe_page = NULL;
	struct pbe **last_high_pbe_ptr = &restore_highmem_pblist, *this_high_pbe = NULL;
	unsigned long orig_low_pfn, orig_high_pfn;
	int high_pbes_done = 0, low_pbes_done = 0;
	int low_direct = 0, high_direct = 0, result = 0, i;
	int high_page = 1, high_offset = 0, low_page = 1, low_offset = 0;

	memory_bm_set_iterators(pageset1_map, 3);
	memory_bm_position_reset(pageset1_map);

	memory_bm_set_iterators(pageset1_copy_map, 2);
	memory_bm_position_reset(pageset1_copy_map);

	last_low_pbe_ptr = &restore_pblist;

	/* First, allocate pages for the start of our pbe lists. */
	if (high_needed) {
		high_pbe_page = ___toi_get_nonconflicting_page(1);
		if (!high_pbe_page) {
			result = -ENOMEM;
			goto out;
		}
		this_high_pbe = (struct pbe *)kmap(high_pbe_page);
		memset(this_high_pbe, 0, PAGE_SIZE);
	}

	low_pbe_page = ___toi_get_nonconflicting_page(0);
	if (!low_pbe_page) {
		result = -ENOMEM;
		goto out;
	}
	this_low_pbe = (struct pbe *)page_address(low_pbe_page);

	/*
	 * Next, allocate the number of pages we need.
	 */

	i = low_needed + high_needed;

	do {
		int is_high;

		if (i == low_needed)
			flags &= ~__GFP_HIGHMEM;

		page = toi_alloc_page(30, flags);
		BUG_ON(!page);

		SetPagePageset1Copy(page);
		is_high = PageHighMem(page);

		if (PagePageset1(page)) {
			if (is_high)
				high_direct++;
			else
				low_direct++;
		} else {
			if (is_high)
				highallocd++;
			else
				lowallocd++;
		}
	} while (--i);

	high_needed -= high_direct;
	low_needed -= low_direct;

	/*
	 * Do we need to use some lowmem pages for the copies of highmem
	 * pages?
	 */
	if (high_needed > highallocd) {
		low_pages_for_highmem = high_needed - highallocd;
		high_needed -= low_pages_for_highmem;
		low_needed += low_pages_for_highmem;
	}

	/*
	 * Now generate our pbes (which will be used for the atomic restore),
	 * and free unneeded pages.
	 */
	memory_bm_position_reset(pageset1_copy_map);
	for (pfn = memory_bm_next_pfn_index(pageset1_copy_map, 1); pfn != BM_END_OF_MAP;
	     pfn = memory_bm_next_pfn_index(pageset1_copy_map, 1)) {
		int is_high;
		page = pfn_to_page(pfn);
		is_high = PageHighMem(page);

		if (PagePageset1(page))
			continue;

		/* Nope. We're going to use this page. Add a pbe. */
		if (is_high || low_pages_for_highmem) {
			struct page *orig_page;
			high_pbes_done++;
			if (!is_high)
				low_pages_for_highmem--;
			do {
				orig_high_pfn = memory_bm_next_pfn_index(pageset1_map, 1);
				BUG_ON(orig_high_pfn == BM_END_OF_MAP);
				orig_page = pfn_to_page(orig_high_pfn);
			} while (!PageHighMem(orig_page) || PagePageset1Copy(orig_page));

			this_high_pbe->orig_address = (void *)orig_high_pfn;
			this_high_pbe->address = page;
			this_high_pbe->next = NULL;
			toi_message(TOI_PAGEDIR, TOI_VERBOSE, 0, "High pbe %d/%d: %p(%d)=>%p",
				    high_page, high_offset, page, orig_high_pfn, orig_page);
			if (last_high_pbe_page != high_pbe_page) {
				*last_high_pbe_ptr = (struct pbe *)high_pbe_page;
				if (last_high_pbe_page) {
					kunmap(last_high_pbe_page);
					high_page++;
					high_offset = 0;
				} else
					high_offset++;
				last_high_pbe_page = high_pbe_page;
			} else {
				*last_high_pbe_ptr = this_high_pbe;
				high_offset++;
			}
			last_high_pbe_ptr = &this_high_pbe->next;
			this_high_pbe = get_next_pbe(&high_pbe_page, this_high_pbe, 1);
			if (IS_ERR(this_high_pbe)) {
				printk(KERN_INFO "This high pbe is an error.\n");
				return -ENOMEM;
			}
		} else {
			struct page *orig_page;
			low_pbes_done++;
			do {
				orig_low_pfn = memory_bm_next_pfn_index(pageset1_map, 2);
				BUG_ON(orig_low_pfn == BM_END_OF_MAP);
				orig_page = pfn_to_page(orig_low_pfn);
			} while (PageHighMem(orig_page) || PagePageset1Copy(orig_page));

			this_low_pbe->orig_address = page_address(orig_page);
			this_low_pbe->address = page_address(page);
			this_low_pbe->next = NULL;
			toi_message(TOI_PAGEDIR, TOI_VERBOSE, 0, "Low pbe %d/%d: %p(%d)=>%p",
				    low_page, low_offset, this_low_pbe->orig_address,
				    orig_low_pfn, this_low_pbe->address);
			*last_low_pbe_ptr = this_low_pbe;
			last_low_pbe_ptr = &this_low_pbe->next;
			this_low_pbe = get_next_pbe(&low_pbe_page, this_low_pbe, 0);
			if (low_pbe_page != last_low_pbe_page) {
				if (last_low_pbe_page) {
					low_page++;
					low_offset = 0;
				}
				last_low_pbe_page = low_pbe_page;
			} else
				low_offset++;
			if (IS_ERR(this_low_pbe)) {
				printk(KERN_INFO "this_low_pbe is an error.\n");
				return -ENOMEM;
			}
		}
	}

	if (high_pbe_page)
		kunmap(high_pbe_page);

	if (last_high_pbe_page != high_pbe_page) {
		if (last_high_pbe_page)
			kunmap(last_high_pbe_page);
		toi__free_page(29, high_pbe_page);
	}

	free_conflicting_pages();

 out:
	memory_bm_set_iterators(pageset1_map, 1);
	memory_bm_set_iterators(pageset1_copy_map, 1);
	return result;
}

int add_boot_kernel_data_pbe(void)
{
	this_low_pbe->address = (char *)__toi_get_nonconflicting_page();
	if (!this_low_pbe->address) {
		printk(KERN_INFO "Failed to get bkd atomic restore buffer.");
		return -ENOMEM;
	}

	toi_bkd.size = sizeof(toi_bkd);
	memcpy(this_low_pbe->address, &toi_bkd, sizeof(toi_bkd));

	*last_low_pbe_ptr = this_low_pbe;
	this_low_pbe->orig_address = (char *)boot_kernel_data_buffer;
	this_low_pbe->next = NULL;
	return 0;
}
