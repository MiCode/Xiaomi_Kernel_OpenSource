/*
 * kernel/power/tuxonice_prepare_image.c
 *
 * Copyright (C) 2003-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 *
 * We need to eat memory until we can:
 * 1. Perform the save without changing anything (RAM_NEEDED < #pages)
 * 2. Fit it all in available space (toiActiveAllocator->available_space() >=
 *    main_storage_needed())
 * 3. Reload the pagedir and pageset1 to places that don't collide with their
 *    final destinations, not knowing to what extent the resumed kernel will
 *    overlap with the one loaded at boot time. I think the resumed kernel
 *    should overlap completely, but I don't want to rely on this as it is
 *    an unproven assumption. We therefore assume there will be no overlap at
 *    all (worse case).
 * 4. Meet the user's requested limit (if any) on the size of the image.
 *    The limit is in MB, so pages/256 (assuming 4K pages).
 *
 */

#include <linux/highmem.h>
#include <linux/freezer.h>
#include <linux/hardirq.h>
#include <linux/mmzone.h>
#include <linux/console.h>
#ifdef CONFIG_TOI_FIXUP
#include <linux/syscalls.h>	/* for sys_sync() */
#include <linux/oom.h>
#endif

#include "tuxonice_pageflags.h"
#include "tuxonice_modules.h"
#include "tuxonice_io.h"
#include "tuxonice_ui.h"
#include "tuxonice_prepare_image.h"
#include "tuxonice.h"
#include "tuxonice_extent.h"
#include "tuxonice_checksum.h"
#include "tuxonice_sysfs.h"
#include "tuxonice_alloc.h"
#include "tuxonice_atomic_copy.h"
#include "tuxonice_builtin.h"

static unsigned long num_nosave, main_storage_allocated, storage_limit, header_storage_needed;
unsigned long extra_pd1_pages_allowance = CONFIG_TOI_DEFAULT_EXTRA_PAGES_ALLOWANCE;
long image_size_limit = CONFIG_TOI_DEFAULT_IMAGE_SIZE_LIMIT;
static int no_ps2_needed;

struct attention_list {
	struct task_struct *task;
	struct attention_list *next;
};

static struct attention_list *attention_list;

#define PAGESET1 0
#define PAGESET2 1

void free_attention_list(void)
{
	struct attention_list *last = NULL;

	while (attention_list) {
		last = attention_list;
		attention_list = attention_list->next;
		toi_kfree(6, last, sizeof(*last));
	}
}

static int build_attention_list(void)
{
	int i, task_count = 0;
	struct task_struct *p;
	struct attention_list *next;
#ifdef CONFIG_TOI_FIXUP
	int task_count2 = 0, task_count3 = 0;
#endif
	/*
	 * Count all userspace process (with task->mm) marked PF_NOFREEZE.
	 */
	toi_read_lock_tasklist();
	for_each_process(p)
	    if ((p->flags & PF_NOFREEZE) || p == current)
		task_count++;
	toi_read_unlock_tasklist();

	/*
	 * Allocate attention list structs.
	 */
	for (i = 0; i < task_count; i++) {
		struct attention_list *this = toi_kzalloc(6, sizeof(struct attention_list),
							  TOI_WAIT_GFP);
		if (!this) {
			printk(KERN_INFO "Failed to allocate slab for " "attention list.\n");
			free_attention_list();
			return 1;
		}
		this->next = NULL;
		if (attention_list)
			this->next = attention_list;
		attention_list = this;
	}

	next = attention_list;
	toi_read_lock_tasklist();
	for_each_process(p)
	    if ((p->flags & PF_NOFREEZE) || p == current) {
#ifdef CONFIG_TOI_FIXUP
		task_count2++;
		if (next == NULL)
			goto ERR;
#endif
		next->task = p;
		next = next->next;
	}
	toi_read_unlock_tasklist();
	return 0;

#ifdef CONFIG_TOI_FIXUP
 ERR:
	hib_err("WARN (%d/%d)\n", task_count, task_count2);
	hib_err("DUMP tasks......\n");
	for_each_process(p)
	    if ((p->flags & PF_NOFREEZE) || p == current) {
		task_count3++;
		hib_err("%s(0x%08x)  ", p->comm, p->flags);
	}
	hib_err("DUMP tasks (#%d) done.\n", task_count3);
	toi_read_unlock_tasklist();
	return 1;
#endif
}

static void pageset2_full(void)
{
	struct zone *zone;
	struct page *page;
	unsigned long flags;
	int i;

	for_each_populated_zone(zone) {
#ifdef CONFIG_CGROUP_MEM_RES_CTLR
		struct mem_cgroup *memcg;
		struct lruvec *lruvec;
		/* Root memcg */
		memcg = mem_cgroup_iter(NULL, NULL, NULL);
		do {
			/* Find corresponding lruvec */
			lruvec = mem_cgroup_zone_lruvec(zone, memcg);

			/* Go through memcg lrus */
			spin_lock_irqsave(&zone->lru_lock, flags);
			for_each_lru(i) {
				/* Is this memcg lru[i] empty? */
				if (!mem_cgroup_zone_nr_lru_pages
				    (memcg, zone_to_nid(zone), zone_idx(zone), BIT(i)))
					continue;

				/* Scan this lru */
				list_for_each_entry(page, &lruvec->lists[i], lru) {
					struct address_space *mapping;

					mapping = page_mapping(page);
					if (!mapping || !mapping->host ||
					    !(mapping->host->i_flags & S_ATOMIC_COPY))
						SetPagePageset2(page);
				}
			}
			spin_unlock_irqrestore(&zone->lru_lock, flags);

			/* Next memcg */
			memcg = mem_cgroup_iter(NULL, memcg, NULL);
		} while (memcg);
#else
		spin_lock_irqsave(&zone->lru_lock, flags);
		for_each_lru(i) {
			if (!zone_page_state(zone, NR_LRU_BASE + i))
				continue;

			list_for_each_entry(page, &zone->lruvec.lists[i], lru) {
				struct address_space *mapping;

				mapping = page_mapping(page);
				if (!mapping || !mapping->host ||
				    !(mapping->host->i_flags & S_ATOMIC_COPY))
					SetPagePageset2(page);
			}
		}
		spin_unlock_irqrestore(&zone->lru_lock, flags);
#endif
	}
}

/*
 * toi_mark_task_as_pageset
 * Functionality   : Marks all the saveable pages belonging to a given process
 *		     as belonging to a particular pageset.
 */

static void toi_mark_task_as_pageset(struct task_struct *t, int pageset2)
{
	struct vm_area_struct *vma;
	struct mm_struct *mm;

	mm = t->active_mm;

#ifdef CONFIG_TOI_FIXUP
	if (mm == (void *)0x6b6b6b6b || mm == (void *)0x6b6b6b6b6b6b6b6b) {
		pr_err("[%s] use after free: task %s rq(%d)\n", __func__, t->comm, t->on_rq);
		WARN_ON(1);
		return;
	}
#endif

	if (!mm || !mm->mmap)
		return;

	if (!irqs_disabled())
		down_read(&mm->mmap_sem);

	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		unsigned long posn;

		if (!vma->vm_start || vma->vm_flags & (VM_IO | VM_DONTDUMP | VM_PFNMAP))
			continue;

		for (posn = vma->vm_start; posn < vma->vm_end; posn += PAGE_SIZE) {
			struct page *page = follow_page(vma, posn, 0);
			struct address_space *mapping;

			if (!page || !pfn_valid(page_to_pfn(page)))
				continue;

			mapping = page_mapping(page);
			if (mapping && mapping->host && mapping->host->i_flags & S_ATOMIC_COPY)
				continue;

			if (pageset2)
				SetPagePageset2(page);
			else {
				ClearPagePageset2(page);
				SetPagePageset1(page);
			}
		}
	}

	if (!irqs_disabled())
		up_read(&mm->mmap_sem);
}

static void mark_tasks(int pageset)
{
	struct task_struct *p;

	toi_read_lock_tasklist();
	for_each_process(p) {
		if (!p->mm)
			continue;

		if (p->flags & PF_KTHREAD)
			continue;

		toi_mark_task_as_pageset(p, pageset);
	}
	toi_read_unlock_tasklist();

}

/* mark_pages_for_pageset2
 *
 * Description:	Mark unshared pages in processes not needed for hibernate as
 *		being able to be written out in a separate pagedir.
 *		HighMem pages are simply marked as pageset2. They won't be
 *		needed during hibernate.
 */

static void toi_mark_pages_for_pageset2(void)
{
	struct attention_list *this = attention_list;

	memory_bm_clear(pageset2_map);

	if (test_action_state(TOI_NO_PAGESET2) || no_ps2_needed)
		return;

	if (test_action_state(TOI_PAGESET2_FULL))
		pageset2_full();
	else
		mark_tasks(PAGESET2);

	/*
	 * Because the tasks in attention_list are ones related to hibernating,
	 * we know that they won't go away under us.
	 */

	while (this) {
		if (!test_result_state(TOI_ABORTED))
			toi_mark_task_as_pageset(this->task, PAGESET1);
		this = this->next;
	}
}

/*
 * The atomic copy of pageset1 is stored in pageset2 pages.
 * But if pageset1 is larger (normally only just after boot),
 * we need to allocate extra pages to store the atomic copy.
 * The following data struct and functions are used to handle
 * the allocation and freeing of that memory.
 */

static unsigned long extra_pages_allocated;

struct extras {
	struct page *page;
	int order;
	struct extras *next;
};

static struct extras *extras_list;

/* toi_free_extra_pagedir_memory
 *
 * Description:	Free previously allocated extra pagedir memory.
 */
void toi_free_extra_pagedir_memory(void)
{
	/* Free allocated pages */
	while (extras_list) {
		struct extras *this = extras_list;
		int i;

		extras_list = this->next;

		for (i = 0; i < (1 << this->order); i++)
			ClearPageNosave(this->page + i);

#ifdef CONFIG_TOI_ENHANCE
		/* keep clean contents for better compression next time */
		clear_page(page_address(this->page));
#endif
		toi_free_pages(9, this->page, this->order);
		toi_kfree(7, this, sizeof(*this));
	}

	extra_pages_allocated = 0;
}

#ifdef CONFIG_TOI_FIXUP
#define TOI_BUFFER_RESERVED ((24*1024*1024) >> (PAGE_SHIFT))

static int is_memory_low(unsigned long wanted)
{
	struct zone *zone;

	for_each_populated_zone(zone) {
		unsigned long free_pages, min_pages, high_pages;
		if (!strcmp(zone->name, "Normal")) {
			free_pages = zone_page_state(zone, NR_FREE_PAGES);
			min_pages = min_wmark_pages(zone);
			high_pages = high_wmark_pages(zone);
			if (free_pages < (wanted + TOI_BUFFER_RESERVED)) {
				hib_warn("memory status: free(%lu) < wanted(%lu)+reserved(%d), min/high:(%lu/%lu)\n",
						 free_pages, wanted, TOI_BUFFER_RESERVED, min_pages, high_pages);
				HIB_SHOW_MEMINFO();
				return 1;
			} else
				return 0;
		}
	}

	return 0;
}
#endif

/* toi_allocate_extra_pagedir_memory
 *
 * Description:	Allocate memory for making the atomic copy of pagedir1 in the
 *		case where it is bigger than pagedir2.
 * Arguments:	int	num_to_alloc: Number of extra pages needed.
 * Result:	int.	Number of extra pages we now have allocated.
 */
static int toi_allocate_extra_pagedir_memory(int extra_pages_needed)
{
	int j, order, num_to_alloc = extra_pages_needed - extra_pages_allocated;
#ifdef CONFIG_TOI_FIXUP
	gfp_t flags = TOI_ATOMIC_GFP | __GFP_NO_KSWAPD;
#else
	gfp_t flags = TOI_ATOMIC_GFP;
#endif

	if (num_to_alloc < 1)
		return 0;

	order = fls(num_to_alloc);
	if (order >= MAX_ORDER)
		order = MAX_ORDER - 1;

	while (num_to_alloc) {
		struct page *newpage;
		unsigned long virt;
		struct extras *extras_entry;

		while ((1 << order) > num_to_alloc)
			order--;

#ifdef CONFIG_TOI_FIXUP
		if (is_memory_low(4)) {
			return extra_pages_allocated;
		}
#endif

		extras_entry = (struct extras *)toi_kzalloc(7,
							    sizeof(struct extras), TOI_ATOMIC_GFP);

		if (!extras_entry)
			return extra_pages_allocated;

		virt = toi_get_free_pages(9, flags, order);
		while (!virt && order) {
			order--;
			virt = toi_get_free_pages(9, flags, order);
		}
#ifdef CONFIG_TOI_FIXUP
		if (virt && is_memory_low(0)) {
			toi_free_pages(9, virt_to_page((void *)virt), order);
			virt = 0;
		}
#endif

		if (!virt) {
			toi_kfree(7, extras_entry, sizeof(*extras_entry));
			return extra_pages_allocated;
		}

		newpage = virt_to_page(virt);

		extras_entry->page = newpage;
		extras_entry->order = order;
		extras_entry->next = extras_list;

		extras_list = extras_entry;

		for (j = 0; j < (1 << order); j++) {
			SetPageNosave(newpage + j);
			SetPagePageset1Copy(newpage + j);
		}

		extra_pages_allocated += (1 << order);
		num_to_alloc -= (1 << order);
	}

	return extra_pages_allocated;
}

/*
 * real_nr_free_pages: Count pcp pages for a zone type or all zones
 * (-1 for all, otherwise zone_idx() result desired).
 */
unsigned long real_nr_free_pages(unsigned long zone_idx_mask)
{
	struct zone *zone;
	int result = 0, cpu;

	/* PCP lists */
	for_each_populated_zone(zone) {
		if (!(zone_idx_mask & (1 << zone_idx(zone))))
			continue;

		for_each_online_cpu(cpu) {
			struct per_cpu_pageset *pset = per_cpu_ptr(zone->pageset, cpu);
			struct per_cpu_pages *pcp = &pset->pcp;
			result += pcp->count;
		}

		result += zone_page_state(zone, NR_FREE_PAGES);
	}
	return result;
}
EXPORT_SYMBOL_GPL(real_nr_free_pages);

/*
 * Discover how much extra memory will be required by the drivers
 * when they're asked to hibernate. We can then ensure that amount
 * of memory is available when we really want it.
 */
static void get_extra_pd1_allowance(void)
{
	unsigned long orig_num_free = real_nr_free_pages(all_zones_mask), final;

	toi_prepare_status(CLEAR_BAR, "Finding allowance for drivers.");

	if (toi_go_atomic(PMSG_FREEZE, 1))
		return;

	final = real_nr_free_pages(all_zones_mask);
	toi_end_atomic(ATOMIC_ALL_STEPS, 1, 0);

	extra_pd1_pages_allowance = (orig_num_free > final) ?
	    orig_num_free - final + MIN_EXTRA_PAGES_ALLOWANCE : MIN_EXTRA_PAGES_ALLOWANCE;
}

/*
 * Amount of storage needed, possibly taking into account the
 * expected compression ratio and possibly also ignoring our
 * allowance for extra pages.
 */
static unsigned long main_storage_needed(int use_ecr, int ignore_extra_pd1_allow)
{
	return (pagedir1.size + pagedir2.size +
		(ignore_extra_pd1_allow ? 0 : extra_pd1_pages_allowance)) *
	    (use_ecr ? toi_expected_compression_ratio() : 100) / 100;
}

/*
 * Storage needed for the image header, in bytes until the return.
 */
unsigned long get_header_storage_needed(void)
{
	unsigned long bytes = sizeof(struct toi_header) +
	    toi_header_storage_for_modules() +
	    toi_pageflags_space_needed() + fs_info_space_needed();

	return DIV_ROUND_UP(bytes, PAGE_SIZE);
}
EXPORT_SYMBOL_GPL(get_header_storage_needed);

/*
 * When freeing memory, pages from either pageset might be freed.
 *
 * When seeking to free memory to be able to hibernate, for every ps1 page
 * freed, we need 2 less pages for the atomic copy because there is one less
 * page to copy and one more page into which data can be copied.
 *
 * Freeing ps2 pages saves us nothing directly. No more memory is available
 * for the atomic copy. Indirectly, a ps1 page might be freed (slab?), but
 * that's too much work to figure out.
 *
 * => ps1_to_free functions
 *
 * Of course if we just want to reduce the image size, because of storage
 * limitations or an image size limit either ps will do.
 *
 * => any_to_free function
 */

static unsigned long lowpages_usable_for_highmem_copy(void)
{
	unsigned long needed = get_lowmem_size(pagedir1) +
	    extra_pd1_pages_allowance + MIN_FREE_RAM +
	    toi_memory_for_modules(0),
	    available = get_lowmem_size(pagedir2) +
	    real_nr_free_low_pages() + extra_pages_allocated;

	return available > needed ? available - needed : 0;
}

static unsigned long highpages_ps1_to_free(void)
{
	unsigned long need = get_highmem_size(pagedir1),
	    available = get_highmem_size(pagedir2) +
	    real_nr_free_high_pages() + lowpages_usable_for_highmem_copy();

	return need > available ? DIV_ROUND_UP(need - available, 2) : 0;
}

static unsigned long lowpages_ps1_to_free(void)
{
	unsigned long needed = get_lowmem_size(pagedir1) +
	    extra_pd1_pages_allowance + MIN_FREE_RAM +
	    toi_memory_for_modules(0),
	    available = get_lowmem_size(pagedir2) +
	    real_nr_free_low_pages() + extra_pages_allocated;

	return needed > available ? DIV_ROUND_UP(needed - available, 2) : 0;
}

static unsigned long current_image_size(void)
{
	return pagedir1.size + pagedir2.size + header_storage_needed;
}

static unsigned long storage_still_required(void)
{
	unsigned long needed = main_storage_needed(1, 1);
	return needed > storage_limit ? needed - storage_limit : 0;
}

static unsigned long ram_still_required(void)
{
	unsigned long needed = MIN_FREE_RAM + toi_memory_for_modules(0) +
	    2 * extra_pd1_pages_allowance,
	    available = real_nr_free_low_pages() + extra_pages_allocated;
	return needed > available ? needed - available : 0;
}

unsigned long any_to_free(int use_image_size_limit)
{
	int use_soft_limit = use_image_size_limit && image_size_limit > 0;
	unsigned long current_size = current_image_size(),
	    soft_limit = use_soft_limit ? (image_size_limit << 8) : 0,
	    to_free = use_soft_limit ? (current_size > soft_limit ?
					current_size - soft_limit : 0) : 0,
	    storage_limit = storage_still_required(),
	    ram_limit = ram_still_required(), first_max = max(to_free, storage_limit);

	return max(first_max, ram_limit);
}

static int need_pageset2(void)
{
	return (real_nr_free_low_pages() + extra_pages_allocated -
		2 * extra_pd1_pages_allowance - MIN_FREE_RAM -
		toi_memory_for_modules(0) - pagedir1.size) < pagedir2.size;
}

/* amount_needed
 *
 * Calculates the amount by which the image size needs to be reduced to meet
 * our constraints.
 */
static unsigned long amount_needed(int use_image_size_limit)
{
	return max(highpages_ps1_to_free() + lowpages_ps1_to_free(),
		   any_to_free(use_image_size_limit));
}

static int image_not_ready(int use_image_size_limit)
{
	toi_message(TOI_EAT_MEMORY, TOI_LOW, 1,
		    "Amount still needed (%lu) > 0:%u,"
		    " Storage allocd: %lu < %lu: %u.\n",
		    amount_needed(use_image_size_limit),
		    (amount_needed(use_image_size_limit) > 0),
		    main_storage_allocated,
		    main_storage_needed(1, 1), main_storage_allocated < main_storage_needed(1, 1));

	toi_cond_pause(0, NULL);

	return (amount_needed(use_image_size_limit) > 0) ||
	    main_storage_allocated < main_storage_needed(1, 1);
}

static void display_failure_reason(int tries_exceeded)
{
	unsigned long storage_required = storage_still_required(),
	    ram_required = ram_still_required(),
	    high_ps1 = highpages_ps1_to_free(), low_ps1 = lowpages_ps1_to_free();

	printk(KERN_INFO "Failed to prepare the image because...\n");

	if (!storage_limit) {
		printk(KERN_INFO "- You need some storage available to be " "able to hibernate.\n");
		return;
	}

	if (tries_exceeded)
		printk(KERN_INFO "- The maximum number of iterations was "
		       "reached without successfully preparing the " "image.\n");

	if (storage_required) {
		printk(KERN_INFO " - We need at least %lu pages of storage "
		       "(ignoring the header), but only have %lu.\n",
		       main_storage_needed(1, 1), main_storage_allocated);
		set_abort_result(TOI_INSUFFICIENT_STORAGE);
	}

	if (ram_required) {
		printk(KERN_INFO " - We need %lu more free pages of low "
		       "memory.\n", ram_required);
		printk(KERN_INFO "     Minimum free     : %8d\n", MIN_FREE_RAM);
		printk(KERN_INFO "   + Reqd. by modules : %8lu\n", toi_memory_for_modules(0));
		printk(KERN_INFO "   + 2 * extra allow  : %8lu\n", 2 * extra_pd1_pages_allowance);
		printk(KERN_INFO "   - Currently free   : %8lu\n", real_nr_free_low_pages());
		printk(KERN_INFO "   - Pages allocd     : %8lu\n", extra_pages_allocated);
		printk(KERN_INFO "                      : ========\n");
		printk(KERN_INFO "     Still needed     : %8lu\n", ram_required);

		/* Print breakdown of memory needed for modules */
		toi_memory_for_modules(1);
		set_abort_result(TOI_UNABLE_TO_FREE_ENOUGH_MEMORY);
	}

	if (high_ps1) {
		printk(KERN_INFO "- We need to free %lu highmem pageset 1 " "pages.\n", high_ps1);
		set_abort_result(TOI_UNABLE_TO_FREE_ENOUGH_MEMORY);
	}

	if (low_ps1) {
		printk(KERN_INFO " - We need to free %ld lowmem pageset 1 " "pages.\n", low_ps1);
		set_abort_result(TOI_UNABLE_TO_FREE_ENOUGH_MEMORY);
	}
}

static void display_stats(int always, int sub_extra_pd1_allow)
{
	char buffer[255];
	snprintf(buffer, 254,
		 "Free:%lu(%lu). Sets:%lu(%lu),%lu(%lu). "
		 "Nosave:%lu-%lu=%lu. Storage:%lu/%lu(%lu=>%lu). "
		 "Needed:%lu,%lu,%lu(%u,%lu,%lu,%ld) (PS2:%s)\n",
		 /* Free */
		 real_nr_free_pages(all_zones_mask), real_nr_free_low_pages(),
		 /* Sets */
		 pagedir1.size, pagedir1.size - get_highmem_size(pagedir1),
		 pagedir2.size, pagedir2.size - get_highmem_size(pagedir2),
		 /* Nosave */
		 num_nosave, extra_pages_allocated, num_nosave - extra_pages_allocated,
		 /* Storage */
		 main_storage_allocated,
		 storage_limit,
		 main_storage_needed(1, sub_extra_pd1_allow), main_storage_needed(1, 1),
		 /* Needed */
		 lowpages_ps1_to_free(), highpages_ps1_to_free(),
		 any_to_free(1),
		 MIN_FREE_RAM, toi_memory_for_modules(0),
		 extra_pd1_pages_allowance, image_size_limit, need_pageset2() ? "yes" : "no");

	if (always)
		printk("%s", buffer);
	else
		toi_message(TOI_EAT_MEMORY, TOI_MEDIUM, 1, buffer);
}

/* generate_free_page_map
 *
 * Description:	This routine generates a bitmap of free pages from the
 *		lists used by the memory manager. We then use the bitmap
 *		to quickly calculate which pages to save and in which
 *		pagesets.
 */
static void generate_free_page_map(void)
{
	int order, cpu, t;
	unsigned long flags, i;
	struct zone *zone;
	struct list_head *curr;
	unsigned long pfn;
	struct page *page;

	for_each_populated_zone(zone) {

		if (!zone->spanned_pages)
			continue;

		spin_lock_irqsave(&zone->lock, flags);

		for (i = 0; i < zone->spanned_pages; i++) {
			pfn = zone->zone_start_pfn + i;

			if (!pfn_valid(pfn))
				continue;

			page = pfn_to_page(pfn);

			ClearPageNosaveFree(page);
		}

		for_each_migratetype_order(order, t) {
			list_for_each(curr, &zone->free_area[order].free_list[t]) {
				unsigned long j;

				pfn = page_to_pfn(list_entry(curr, struct page, lru));
				for (j = 0; j < (1UL << order); j++)
					SetPageNosaveFree(pfn_to_page(pfn + j));
			}
		}

		for_each_online_cpu(cpu) {
			struct per_cpu_pageset *pset = per_cpu_ptr(zone->pageset, cpu);
			struct per_cpu_pages *pcp = &pset->pcp;
			struct page *page;
			int t;

			for (t = 0; t < MIGRATE_PCPTYPES; t++)
				list_for_each_entry(page, &pcp->lists[t], lru)
				    SetPageNosaveFree(page);
		}

		spin_unlock_irqrestore(&zone->lock, flags);
	}
}

/* size_of_free_region
 *
 * Description:	Return the number of pages that are free, beginning with and
 *		including this one.
 */
static int size_of_free_region(struct zone *zone, unsigned long start_pfn)
{
	unsigned long this_pfn = start_pfn,
	    end_pfn = zone->zone_start_pfn + zone->spanned_pages - 1;

	while (pfn_valid(this_pfn) && this_pfn <= end_pfn && PageNosaveFree(pfn_to_page(this_pfn)))
		this_pfn++;

	return this_pfn - start_pfn;
}

/* flag_image_pages
 *
 * This routine generates our lists of pages to be stored in each
 * pageset. Since we store the data using extents, and adding new
 * extents might allocate a new extent page, this routine may well
 * be called more than once.
 */
static void flag_image_pages(int atomic_copy)
{
	int num_free = 0;
	unsigned long loop;
	struct zone *zone;

	pagedir1.size = 0;
	pagedir2.size = 0;

	set_highmem_size(pagedir1, 0);
	set_highmem_size(pagedir2, 0);

	num_nosave = 0;

	memory_bm_clear(pageset1_map);

	generate_free_page_map();

	/*
	 * Pages not to be saved are marked Nosave irrespective of being
	 * reserved.
	 */
	for_each_populated_zone(zone) {
		int highmem = is_highmem(zone);

		for (loop = 0; loop < zone->spanned_pages; loop++) {
			unsigned long pfn = zone->zone_start_pfn + loop;
			struct page *page;
			int chunk_size;

			if (!pfn_valid(pfn))
				continue;

			chunk_size = size_of_free_region(zone, pfn);
			if (chunk_size) {
				num_free += chunk_size;
				loop += chunk_size - 1;
				continue;
			}

			page = pfn_to_page(pfn);

			if (PageNosave(page)) {
				num_nosave++;
				continue;
			}

			page = highmem ? saveable_highmem_page(zone, pfn) :
			    saveable_page(zone, pfn);

			if (!page) {
				num_nosave++;
				continue;
			}

			if (PagePageset2(page)) {
				pagedir2.size++;
				if (PageHighMem(page))
					inc_highmem_size(pagedir2);
				else
					SetPagePageset1Copy(page);
				if (PageResave(page)) {
					SetPagePageset1(page);
					ClearPagePageset1Copy(page);
					pagedir1.size++;
					if (PageHighMem(page))
						inc_highmem_size(pagedir1);
				}
			} else {
				pagedir1.size++;
				SetPagePageset1(page);
				if (PageHighMem(page))
					inc_highmem_size(pagedir1);
			}
		}
	}

	if (!atomic_copy)
		toi_message(TOI_EAT_MEMORY, TOI_MEDIUM, 0,
			    "Count data pages: Set1 (%d) + Set2 (%d) + Nosave (%ld)"
			    " + NumFree (%d) = %d.\n",
			    pagedir1.size, pagedir2.size, num_nosave, num_free,
			    pagedir1.size + pagedir2.size + num_nosave + num_free);
}

void toi_recalculate_image_contents(int atomic_copy)
{
	memory_bm_clear(pageset1_map);
	if (!atomic_copy) {
		unsigned long pfn;
		memory_bm_position_reset(pageset2_map);
		for (pfn = memory_bm_next_pfn(pageset2_map);
		     pfn != BM_END_OF_MAP; pfn = memory_bm_next_pfn(pageset2_map))
			ClearPagePageset1Copy(pfn_to_page(pfn));
		/* Need to call this before getting pageset1_size! */
		toi_mark_pages_for_pageset2();
	}
	flag_image_pages(atomic_copy);

	if (!atomic_copy) {
		storage_limit = toiActiveAllocator->storage_available();
#ifdef CONFIG_TOI_FIXUP
		display_stats(1, 0);
#else
		display_stats(0, 0);
#endif
	}
}

int try_allocate_extra_memory(void)
{
#ifdef CONFIG_TOI_FIXUP
	int wanted = pagedir1.size + extra_pd1_pages_allowance - get_lowmem_size(pagedir2);
	if ((wanted > 0) && (wanted > extra_pages_allocated)) {
		int got = toi_allocate_extra_pagedir_memory(wanted);
		hib_warn("%s: Want %d extra pages for pageset1, got %d\n",
				 wanted > got ? "FAIL" : "PASS",  wanted, got);
		if (unlikely(wanted > got))
			return 1;
	} else
		hib_warn("PASS: Want %lu extra pages for pageset1, got %lu\n",
				 pagedir1.size + extra_pd1_pages_allowance,
				 extra_pages_allocated + get_lowmem_size(pagedir2));
#else /* buggy codes, (1) why wanted < got and return 1 ? ; (2) wanted might be negative value. */
	unsigned long wanted = pagedir1.size + extra_pd1_pages_allowance -
	    get_lowmem_size(pagedir2);
	if (wanted > extra_pages_allocated) {
		unsigned long got = toi_allocate_extra_pagedir_memory(wanted);
		if (wanted < got) {
			toi_message(TOI_EAT_MEMORY, TOI_LOW, 1,
				    "Want %d extra pages for pageset1, got %d.\n", wanted, got);
			return 1;
		}
	}
#endif
	return 0;
}


/* update_image
 *
 * Allocate [more] memory and storage for the image.
 */
static void update_image(int ps2_recalc)
{
	int old_header_req;
	unsigned long seek;

	if (try_allocate_extra_memory())
		return;

	if (ps2_recalc)
		goto recalc;

	thaw_kernel_threads();

	/*
	 * Allocate remaining storage space, if possible, up to the
	 * maximum we know we'll need. It's okay to allocate the
	 * maximum if the writer is the swapwriter, but
	 * we don't want to grab all available space on an NFS share.
	 * We therefore ignore the expected compression ratio here,
	 * thereby trying to allocate the maximum image size we could
	 * need (assuming compression doesn't expand the image), but
	 * don't complain if we can't get the full amount we're after.
	 */

	do {
		int result;

		old_header_req = header_storage_needed;
		toiActiveAllocator->reserve_header_space(header_storage_needed);

		/* How much storage is free with the reservation applied? */
		storage_limit = toiActiveAllocator->storage_available();
		seek = min(storage_limit, main_storage_needed(0, 0));

		result = toiActiveAllocator->allocate_storage(seek);
		if (result)
			printk("Failed to allocate storage (%d).\n", result);

		main_storage_allocated = toiActiveAllocator->storage_allocated();

		/* Need more header because more storage allocated? */
		header_storage_needed = get_header_storage_needed();

	} while (header_storage_needed > old_header_req);

	if (freeze_kernel_threads()) {
		hib_log("after calling freeze_kernel_threads() with failed result\n");
		set_abort_result(TOI_FREEZING_FAILED);
	}

 recalc:
	toi_recalculate_image_contents(0);
}

/* attempt_to_freeze
 *
 * Try to freeze processes.
 */

static int attempt_to_freeze(void)
{
	int result;

	/* Stop processes before checking again */
	toi_prepare_status(CLEAR_BAR, "Freezing processes & syncing " "filesystems.");
#ifdef CONFIG_TOI_FIXUP
	hib_warn("Syncing filesystems ... ");
	sys_sync();
	hib_warn("done.\n");
#endif

	result = freeze_processes();

	if (result)
		set_abort_result(TOI_FREEZING_FAILED);

	result = freeze_kernel_threads();

#ifdef CONFIG_MTK_HIBERNATION
  if (!result){
		shrink_all_memory(0); // purpose for early trigger PASR to release userspace memory pages.
	}
#endif

	if (result)
		set_abort_result(TOI_FREEZING_FAILED);

	return result;
}

/* eat_memory
 *
 * Try to free some memory, either to meet hard or soft constraints on the image
 * characteristics.
 *
 * Hard constraints:
 * - Pageset1 must be < half of memory;
 * - We must have enough memory free at resume time to have pageset1
 *   be able to be loaded in pages that don't conflict with where it has to
 *   be restored.
 * Soft constraints
 * - User specificied image size limit.
 */
static void eat_memory(void)
{
	unsigned long amount_wanted = 0;
	int did_eat_memory = 0;

	/*
	 * Note that if we have enough storage space and enough free memory, we
	 * may exit without eating anything. We give up when the last 10
	 * iterations ate no extra pages because we're not going to get much
	 * more anyway, but the few pages we get will take a lot of time.
	 *
	 * We freeze processes before beginning, and then unfreeze them if we
	 * need to eat memory until we think we have enough. If our attempts
	 * to freeze fail, we give up and abort.
	 */

	amount_wanted = amount_needed(1);

	switch (image_size_limit) {
	case -1:		/* Don't eat any memory */
		if (amount_wanted > 0) {
			set_abort_result(TOI_WOULD_EAT_MEMORY);
			return;
		}
		break;
	case -2:		/* Free caches only */
		drop_pagecache();
		toi_recalculate_image_contents(0);
		amount_wanted = amount_needed(1);
		break;
	default:
		break;
	}

	if (amount_wanted > 0 && !test_result_state(TOI_ABORTED) && image_size_limit != -1) {
		unsigned long request = amount_wanted;
		unsigned long high_req = max(highpages_ps1_to_free(),
					     any_to_free(1));
		unsigned long low_req = lowpages_ps1_to_free();
		unsigned long got = 0;

		toi_prepare_status(CLEAR_BAR,
				   "Seeking to free %ldMB of memory.", MB(amount_wanted));

		thaw_kernel_threads();

		/*
		 * Ask for too many because shrink_memory_mask doesn't
		 * currently return enough most of the time.
		 */

		if (low_req)
			got = shrink_memory_mask(low_req, GFP_KERNEL);
		if (high_req)
			shrink_memory_mask(high_req - got, GFP_HIGHUSER);

		did_eat_memory = 1;

		toi_recalculate_image_contents(0);

		amount_wanted = amount_needed(1);

		printk(KERN_DEBUG "Asked shrink_memory_mask for %ld low pages &"
		       " %ld pages from anywhere, got %ld.\n",
		       high_req, low_req, request - amount_wanted);

		toi_cond_pause(0, NULL);

		if (freeze_kernel_threads())
			set_abort_result(TOI_FREEZING_FAILED);
	}

	if (did_eat_memory)
		toi_recalculate_image_contents(0);
}

/* toi_prepare_image
 *
 * Entry point to the whole image preparation section.
 *
 * We do four things:
 * - Freeze processes;
 * - Ensure image size constraints are met;
 * - Complete all the preparation for saving the image,
 *   including allocation of storage. The only memory
 *   that should be needed when we're finished is that
 *   for actually storing the image (and we know how
 *   much is needed for that because the modules tell
 *   us).
 * - Make sure that all dirty buffers are written out.
 */
#define MAX_TRIES 2
int toi_prepare_image(void)
{
	int result = 1, tries = 1;

	main_storage_allocated = 0;
	no_ps2_needed = 0;

	if (attempt_to_freeze())
		return 1;
	hib_log("@line:%d return value (%d)\n", __LINE__, result);

	if (!extra_pd1_pages_allowance)
		get_extra_pd1_allowance();

	storage_limit = toiActiveAllocator->storage_available();

	if (!storage_limit) {
		printk(KERN_INFO "No storage available. Didn't try to prepare " "an image.\n");
		display_failure_reason(0);
		set_abort_result(TOI_NOSTORAGE_AVAILABLE);
		return 1;
	}

	if (build_attention_list()) {
		abort_hibernate(TOI_UNABLE_TO_PREPARE_IMAGE,
				"Unable to successfully prepare the image.\n");
		return 1;
	}

	toi_recalculate_image_contents(0);

	do {
		toi_prepare_status(CLEAR_BAR, "Preparing Image. Try %d.", tries);

		eat_memory();

		if (test_result_state(TOI_ABORTED))
			break;

		update_image(0);

		tries++;

	} while (image_not_ready(1) && tries <= MAX_TRIES && !test_result_state(TOI_ABORTED));
	hib_log("@line%d return value (%d)\n", __LINE__, result);

	result = image_not_ready(0);
	hib_log("@line:%d return value (%d)\n", __LINE__, result);

	if (!test_result_state(TOI_ABORTED)) {
		if (result) {
			display_stats(1, 0);
			display_failure_reason(tries > MAX_TRIES);
			abort_hibernate(TOI_UNABLE_TO_PREPARE_IMAGE,
					"Unable to successfully prepare the image.\n");
		} else {
			/* Pageset 2 needed? */
			if (!need_pageset2() && test_action_state(TOI_NO_PS2_IF_UNNEEDED)) {
				no_ps2_needed = 1;
				toi_recalculate_image_contents(0);
				update_image(1);
			}

			toi_cond_pause(1, "Image preparation complete.");
			hib_log("Image preparation complete.\n");
		}
	}

	hib_log("@line:%d return value (%d)\n", __LINE__, result);
	return result ? result : allocate_checksum_pages();
}
