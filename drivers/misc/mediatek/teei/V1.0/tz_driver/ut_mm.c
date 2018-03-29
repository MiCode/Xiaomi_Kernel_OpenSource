#include <linux/stddef.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/interrupt.h>
#include <linux/pagemap.h>
#include <linux/jiffies.h>
#include <linux/bootmem.h>
#include <linux/memblock.h>
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/kmemcheck.h>
#include <linux/module.h>
#include <linux/suspend.h>
#include <linux/pagevec.h>
#include <linux/blkdev.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>
#include <linux/oom.h>
#include <linux/notifier.h>
#include <linux/topology.h>
#include <linux/sysctl.h>
#include <linux/cpu.h>
#include <linux/cpuset.h>
#include <linux/memory_hotplug.h>
#include <linux/nodemask.h>
#include <linux/vmalloc.h>
#include <linux/vmstat.h>
#include <linux/mempolicy.h>
#include <linux/stop_machine.h>
#include <linux/sort.h>
#include <linux/pfn.h>
#include <linux/backing-dev.h>
#include <linux/fault-inject.h>
#include <linux/page-isolation.h>
#include <linux/page_cgroup.h>
#include <linux/debugobjects.h>
#include <linux/kmemleak.h>
#include <linux/compaction.h>
#include <trace/events/kmem.h>
#include <linux/ftrace_event.h>
#include <linux/memcontrol.h>
#include <linux/prefetch.h>
#include <linux/mm_inline.h>
#include <linux/migrate.h>
#include <linux/page-debug-flags.h>
#include <linux/hugetlb.h>
#include <linux/sched/rt.h>
#include <linux/mmdebug.h>

#include <asm/tlbflush.h>
#include <asm/div64.h>
#include <linux/mmzone.h>

#define ALLOC_NO_WATERMARKS     0x04
#define ALLOC_WMARK_MASK        (ALLOC_NO_WATERMARKS-1)
#define ALLOC_WMARK_LOW         WMARK_LOW
#define ALLOC_CPUSET            0x40

#define ZONE_RECLAIM_NOSCAN     -2
#define ZONE_RECLAIM_FULL       -1
#define ZONE_RECLAIM_SOME       0
#define ZONE_RECLAIM_SUCCESS    1

#define ALLOC_WMARK_MIN         WMARK_MIN


#define UT_MAX_MEM		0xD0000000

#define __ut_alloc_pages(gfp_mask, order) \
			__ut_alloc_pages_node(numa_node_id(), gfp_mask, order)

extern void expand(struct zone *zone, struct page *page,
			int low, int high, struct free_area *area,
			int migratetype);

extern int rmqueue_bulk(struct zone *zone, unsigned int order,
			unsigned long count, struct list_head *list,
			int migratetype, int cold);

extern int prep_new_page(struct page *page, int order, gfp_t gfp_flags);

extern int zlc_zone_worth_trying(struct zonelist *zonelist, struct zoneref *z,
				nodemask_t *allowednodes);

extern nodemask_t *zlc_setup(struct zonelist *zonelist, int alloc_flags);
extern bool zone_allows_reclaim(struct zone *local_zone, struct zone *zone);
extern void zlc_mark_zone_full(struct zonelist *zonelist, struct zoneref *z);


static inline void rmv_page_order(struct page *page)
{
	__ClearPageBuddy(page);
	set_page_private(page, 0);
}



/*
 * Go through the free lists for the given migratetype and remove
 * the smallest available page from the freelists
 */
static inline
struct page *ut_rmqueue_smallest(struct zone *zone, unsigned int order,
				int migratetype)
{
	unsigned int current_order;
	struct free_area *area;
	struct page *page;
	int page_found = 0;
	struct list_head *free_list_head = NULL;
	struct list_head *free_list_ent = NULL;

	/* Find a page of the appropriate size in the preferred list */
	for (current_order = order; current_order < MAX_ORDER; ++current_order) {
		area = &(zone->free_area[current_order]);

		if (list_empty(&area->free_list[migratetype]))
			continue;

		free_list_head = &area->free_list[migratetype];

		free_list_ent = free_list_head->next;

		do {
			page = list_entry(free_list_ent, struct page, lru);

			if ((unsigned long)(page_to_phys(page)) >= UT_MAX_MEM) {
				pr_err("[%s][%d] FAILED! ORDER = %d page_to_phys(page) = %lx\n", __func__, __LINE__, current_order, (unsigned long)page_to_phys(page));
				page_found = 0;
				free_list_ent = free_list_ent->next;

			} else {
				pr_debug("[%s][%d] SUCCESS! ORDER = %d page_to_phys(page) = %lx\n", __func__, __LINE__, current_order, (unsigned long)page_to_phys(page));
				page_found = 1;
				break;
			}
		} while (free_list_ent != free_list_head);

		if (page_found == 0)
			continue;

		list_del(&page->lru);
		rmv_page_order(page);
		area->nr_free--;
		expand(zone, page, order, current_order, area, migratetype);
		return page;
	}

	return NULL;
}



/*
 * Do the hard work of removing an element from the buddy allocator.
 * Call me with the zone->lock already held.
 */
static struct page *ut_rmqueue(struct zone *zone, unsigned int order,
				int migratetype)
{
	struct page *page;

	page = ut_rmqueue_smallest(zone, order, migratetype);

	trace_mm_page_alloc_zone_locked(page, order, migratetype);
	return page;
}



/*
 * Really, prep_compound_page() should be called from __rmqueue_bulk().  But
 * we cheat by calling it from here, in the order > 0 path.  Saves a branch
 * or two.
 */
static inline
struct page *ut_buffered_rmqueue(struct zone *preferred_zone,
				struct zone *zone, int order, gfp_t gfp_flags,
				int migratetype)
{
	unsigned long flags;
	struct page *page;
	int cold = !!(gfp_flags & __GFP_COLD);

again:

	if (likely(order == 0)) {
		struct per_cpu_pages *pcp;
		struct list_head *list;
		struct list_head *list_ent;
		int page_found = 0;

		local_irq_save(flags);
		pcp = &this_cpu_ptr(zone->pageset)->pcp;
		list = &pcp->lists[migratetype];

		if (list_empty(list)) {
			pcp->count += rmqueue_bulk(zone, 0,
						pcp->batch, list,
						migratetype, cold);

			if (unlikely(list_empty(list)))
				goto failed;
		}

		if (cold) {
			list_ent = list->prev;

			do {
				page = list_entry(list_ent, struct page, lru);

				if ((unsigned long)(page_to_phys(page)) >= UT_MAX_MEM) {
					page_found = 0;
				} else {
					page_found = 1;
					break;
				}

				list_ent = list_ent->prev;
			} while (list_ent != list);

			if (page_found == 0)
				goto singal_page_fail;

		} else {
			list_ent = list->next;

			do {
				page = list_entry(list_ent, struct page, lru);

				if ((unsigned long)(page_to_phys(page)) >= UT_MAX_MEM) {
					page_found = 0;
				} else {
					page_found = 1;
					break;
				}

				list_ent = list_ent->next;
			} while (list_ent != list);

			if (page_found == 0)
				goto singal_page_fail;

		}

		list_del(&page->lru);
		pcp->count--;
	} else {
		if (unlikely(gfp_flags & __GFP_NOFAIL)) {
			/*
			 * __GFP_NOFAIL is not to be used in new code.
			 *
			 * All __GFP_NOFAIL callers should be fixed so that they
			 * properly detect and handle allocation failures.
			 *
			 * We most definitely don't want callers attempting to
			 * allocate greater than order-1 page units with
			 * __GFP_NOFAIL.
			 */
			WARN_ON_ONCE(order > 1);
		}

		spin_lock_irqsave(&zone->lock, flags);

singal_page_fail:
		page = ut_rmqueue(zone, order, migratetype);
		spin_unlock(&zone->lock);

		if (!page)
			goto failed;

#if !defined(CONFIG_CMA) || !defined(CONFIG_MTK_SVP) /* SVP 16 */
		__mod_zone_freepage_state(zone, -(1 << order),
						get_pageblock_migratetype(page));
#else
		__mod_zone_page_state(zone, NR_FREE_PAGES, -(1 << order));
#endif
	}

	__count_zone_vm_events(PGALLOC, zone, 1 << order);
	zone_statistics(preferred_zone, zone, gfp_flags);
	local_irq_restore(flags);

	if (prep_new_page(page, order, gfp_flags))
		goto again;

	return page;

failed:
	local_irq_restore(flags);
	return NULL;
}


/*
 * get_page_from_freelist goes through the zonelist trying to allocate
 * a page.
 */
static struct page *
ut_get_page_from_freelist(gfp_t gfp_mask, nodemask_t *nodemask, unsigned int order,
				struct zonelist *zonelist, int high_zoneidx, int alloc_flags,
				struct zone *preferred_zone, int migratetype)
{
	struct zoneref *z;
	struct page *page = NULL;
	int classzone_idx;
	struct zone *zone;
	nodemask_t *allowednodes = NULL;/* zonelist_cache approximation */
	int zlc_active = 0;		/* set if using zonelist_cache */
	int did_zlc_setup = 0;		/* just call zlc_setup() one time */

	classzone_idx = zone_idx(preferred_zone);
zonelist_scan:
	/*
	 * Scan zonelist, looking for a zone with enough free.
	 * See also cpuset_zone_allowed() comment in kernel/cpuset.c.
	 */
	for_each_zone_zonelist_nodemask(zone, z, zonelist,
					high_zoneidx, nodemask) {
#if 1

		if (IS_ENABLED(CONFIG_NUMA) && zlc_active &&
		    !zlc_zone_worth_trying(zonelist, z, allowednodes))
			continue;

		if ((alloc_flags & ALLOC_CPUSET) &&
		    !cpuset_zone_allowed_softwall(zone, gfp_mask))
			continue;

		if ((alloc_flags & ALLOC_WMARK_LOW) &&
		    (gfp_mask & __GFP_WRITE) && !zone_dirty_ok(zone))
			goto this_zone_full;

		BUILD_BUG_ON(ALLOC_NO_WATERMARKS < NR_WMARK);
#endif

#if 1

		if (!(alloc_flags & ALLOC_NO_WATERMARKS)) {
			unsigned long mark;
			int ret;

			mark = zone->watermark[alloc_flags & ALLOC_WMARK_MASK];

			if (zone_watermark_ok(zone, order, mark,
						classzone_idx, alloc_flags))
				goto try_this_zone;

			if (IS_ENABLED(CONFIG_NUMA) &&
			    !did_zlc_setup && nr_online_nodes > 1) {
				/*
				 * we do zlc_setup if there are multiple nodes
				 * and before considering the first zone allowed
				 * by the cpuset.
				 */
				allowednodes = zlc_setup(zonelist, alloc_flags);
				zlc_active = 1;
				did_zlc_setup = 1;
			}

			if (zone_reclaim_mode == 0 ||
			    !zone_allows_reclaim(preferred_zone, zone))
				goto this_zone_full;

			/*
			 * As we may have just activated ZLC, check if the first
			 * eligible zone has failed zone_reclaim recently.
			 */
			if (IS_ENABLED(CONFIG_NUMA) && zlc_active &&
			    !zlc_zone_worth_trying(zonelist, z, allowednodes))
				continue;

			ret = zone_reclaim(zone, gfp_mask, order);

			switch (ret) {
			case ZONE_RECLAIM_NOSCAN:
				/* did not scan */
				continue;

			case ZONE_RECLAIM_FULL:
				/* scanned but unreclaimable */
				continue;

			default:

				/* did we reclaim enough */
				if (zone_watermark_ok(zone, order, mark,
							classzone_idx, alloc_flags))
					goto try_this_zone;

				/*
				 * Failed to reclaim enough to meet watermark.
				 * Only mark the zone full if checking the min
				 * watermark or if we failed to reclaim just
				 * 1<<order pages or else the page allocator
				 * fastpath will prematurely mark zones full
				 * when the watermark is between the low and
				 * min watermarks.
				 */
				if (((alloc_flags & ALLOC_WMARK_MASK) == ALLOC_WMARK_MIN) ||
				    ret == ZONE_RECLAIM_SOME)
					goto this_zone_full;

				continue;
			}
		}

#endif

try_this_zone:
		page = ut_buffered_rmqueue(preferred_zone, zone, order, gfp_mask, migratetype);

		if (page)
			break;

#if 1
this_zone_full:

		if (IS_ENABLED(CONFIG_NUMA))
			zlc_mark_zone_full(zonelist, z);

#endif
	}

	if (unlikely(IS_ENABLED(CONFIG_NUMA) && page == NULL && zlc_active)) {
		/* Disable zlc cache for second zonelist scan */
		zlc_active = 0;
		goto zonelist_scan;
	}

	if (page)
		/*
		 * page->pfmemalloc is set when ALLOC_NO_WATERMARKS was
		 * necessary to allocate the page. The expectation is
		 * that the caller is taking steps that will free more
		 * memory. The caller should avoid the page being used
		 * for !PFMEMALLOC purposes.
		 */
		page->pfmemalloc = !!(alloc_flags & ALLOC_NO_WATERMARKS);

#if defined(CONFIG_CMA) && defined(CONFIG_MTK_SVP)
#if defined(CONFIG_MT_ENG_BUILD)

	if (page) {
		if (_forbid_cma_alloc) {
			unsigned long vpfn = page_to_pfn(page);

			if (svp_is_in_range(vpfn)) {
				pr_alert("%s %d: pfn: %lu in _forbid_cma_alloc %d\n",
						__func__, __LINE__, vpfn, _forbid_cma_alloc);
				dump_stack();
			}
		}
	}

#endif
#endif

	return page;
}








/*
 * This is the 'heart' of the zoned buddy allocator.
 */
struct page *
ut_alloc_pages_nodemask(gfp_t gfp_mask, unsigned int order,
			struct zonelist *zonelist, nodemask_t *nodemask)
{
	enum zone_type high_zoneidx = gfp_zone(gfp_mask);
	struct zone *preferred_zone;
	struct page *page = NULL;
	int migratetype = allocflags_to_migratetype(gfp_mask);
	unsigned int cpuset_mems_cookie;
	int alloc_flags = ALLOC_WMARK_LOW|ALLOC_CPUSET;
	struct mem_cgroup *memcg = NULL;
#ifdef __LOG_PAGE_ALLOC_ORDER__
	struct stack_trace trace;
	unsigned long entries[6] = {0};
#endif

	gfp_mask &= gfp_allowed_mask;

	lockdep_trace_alloc(gfp_mask);

	might_sleep_if(gfp_mask & __GFP_WAIT);

	/*
	 * Check the zones suitable for the gfp_mask contain at least one
	 * valid zone. It's possible to have an empty zonelist as a result
	 * of GFP_THISNODE and a memoryless node
	 */
	if (unlikely(!zonelist->_zonerefs->zone))
		return NULL;

	/*
	 * Will only have any effect when __GFP_KMEMCG is set.  This is
	 * verified in the (always inline) callee
	 */
	if (!memcg_kmem_newpage_charge(gfp_mask, &memcg, order))
		return NULL;

retry_cpuset:
	cpuset_mems_cookie = get_mems_allowed();

	/* The preferred zone is used for statistics later */
	first_zones_zonelist(zonelist, high_zoneidx,
				nodemask ? : &cpuset_current_mems_allowed,
				&preferred_zone);

	if (!preferred_zone)
		goto out;

#if !defined(CONFIG_CMA) || !defined(CONFIG_MTK_SVP) /* SVP 15 */
#ifdef CONFIG_CMA

	if (allocflags_to_migratetype(gfp_mask) == MIGRATE_MOVABLE)
		alloc_flags |= ALLOC_CMA;

#endif
#endif

#ifdef CONFIG_MTKPASR
	/* Speed up allocation for MIGRATE_MOVABLE */
#ifdef CONFIG_HIGHMEM

	if (high_zoneidx >= ZONE_HIGHMEM) {
#endif

		if (migratetype == MIGRATE_MOVABLE) {
			migratetype = preferred_mt;
		}

#ifdef CONFIG_HIGHMEM
	}

#endif
#endif

	/* First allocation attempt */
	page = ut_get_page_from_freelist(gfp_mask|__GFP_HARDWALL, nodemask, order,
					zonelist, high_zoneidx, alloc_flags,
					preferred_zone, migratetype);

#ifdef __LOG_PAGE_ALLOC_ORDER__

#ifdef CONFIG_FREEZER /* Added skip debug log in IPOH */

	if (unlikely(!atomic_read(&system_freezing_cnt))) {
#endif

		if (order >= page_alloc_dump_order_threshold) {
			trace.nr_entries = 0;
			trace.max_entries = ARRAY_SIZE(entries);
			trace.entries = entries;
			trace.skip = 2;

			save_stack_trace(&trace);
			trace_dump_allocate_large_pages(page, order, gfp_mask, entries);
		} else if (order >= page_alloc_log_order_threshold) {
			trace_debug_allocate_large_pages(page, order, gfp_mask);
		}

#ifdef CONFIG_FREEZER
	}

#endif

#endif /* __LOG_PAGE_ALLOC_ORDER__ */



	trace_mm_page_alloc(page, order, gfp_mask, migratetype);
out:

	if (unlikely(!put_mems_allowed(cpuset_mems_cookie) && !page))
		goto retry_cpuset;

	return page;
}


static inline struct page *
ut_alloc_pages(gfp_t gfp_mask, unsigned int order, struct zonelist *zonelist)
{
	return ut_alloc_pages_nodemask(gfp_mask, order, zonelist, NULL);
}

static inline struct page *__ut_alloc_pages_node(int nid, gfp_t gfp_mask, unsigned int order)
{
	/* Unknown node is current node */
	if (nid < 0)
		nid = numa_node_id();

	return ut_alloc_pages(gfp_mask, order, node_zonelist(nid, gfp_mask));
}



unsigned long ut_get_free_pages(gfp_t gfp_mask, unsigned int order)
{
	struct page *page;

	/*
	 * ut_get_free_pages() returns a 32-bit address, which cannot represent
	 * a highmem page
	 */
	VM_BUG_ON((gfp_mask & __GFP_HIGHMEM) != 0);

	page = __ut_alloc_pages(gfp_mask, order);

	if (!page)
		return 0;

	return (unsigned long) page_address(page);

}


