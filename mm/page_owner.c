// SPDX-License-Identifier: GPL-2.0
#include <linux/debugfs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/memblock.h>
#include <linux/stacktrace.h>
#include <linux/page_owner.h>
#include <linux/jump_label.h>
#include <linux/migrate.h>
#include <linux/stackdepot.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#ifdef CONFIG_QCOM_MINIDUMP_PANIC_DUMP
#include <soc/qcom/minidump.h>
#include <linux/ctype.h>
#endif
#include "internal.h"

/*
 * TODO: teach PAGE_OWNER_STACK_DEPTH (__dump_page_owner and save_stack)
 * to use off stack temporal storage
 */
#define PAGE_OWNER_STACK_DEPTH (16)

struct page_owner {
	unsigned short order;
	short last_migrate_reason;
	gfp_t gfp_mask;
	depot_stack_handle_t handle;
	depot_stack_handle_t free_handle;
	int pid;
	u64 ts_nsec;
	u64 free_ts_nsec;
};

static bool page_owner_enabled = IS_ENABLED(CONFIG_PAGE_OWNER_ENABLE_DEFAULT);
DEFINE_STATIC_KEY_FALSE(page_owner_inited);

static depot_stack_handle_t dummy_handle;
static depot_stack_handle_t failure_handle;
static depot_stack_handle_t early_handle;

static void init_early_allocated_pages(void);

static int __init early_page_owner_param(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (strcmp(buf, "on") == 0)
		page_owner_enabled = true;

	if (strcmp(buf, "off") == 0)
		page_owner_enabled = false;

	return 0;
}
early_param("page_owner", early_page_owner_param);

static bool need_page_owner(void)
{
	return page_owner_enabled;
}

static __always_inline depot_stack_handle_t create_dummy_stack(void)
{
	unsigned long entries[4];
	unsigned int nr_entries;

	nr_entries = stack_trace_save(entries, ARRAY_SIZE(entries), 0);
	return stack_depot_save(entries, nr_entries, GFP_KERNEL);
}

static noinline void register_dummy_stack(void)
{
	dummy_handle = create_dummy_stack();
}

static noinline void register_failure_stack(void)
{
	failure_handle = create_dummy_stack();
}

static noinline void register_early_stack(void)
{
	early_handle = create_dummy_stack();
}

static void init_page_owner(void)
{
	if (!page_owner_enabled)
		return;

	register_dummy_stack();
	register_failure_stack();
	register_early_stack();
	static_branch_enable(&page_owner_inited);
	init_early_allocated_pages();
}

struct page_ext_operations page_owner_ops = {
	.size = sizeof(struct page_owner),
	.need = need_page_owner,
	.init = init_page_owner,
};

static inline struct page_owner *get_page_owner(struct page_ext *page_ext)
{
	return (void *)page_ext + page_owner_ops.offset;
}

static inline bool check_recursive_alloc(unsigned long *entries,
					 unsigned int nr_entries,
					 unsigned long ip)
{
	unsigned int i;

	for (i = 0; i < nr_entries; i++) {
		if (entries[i] == ip)
			return true;
	}
	return false;
}

static noinline depot_stack_handle_t save_stack(gfp_t flags)
{
	unsigned long entries[PAGE_OWNER_STACK_DEPTH];
	depot_stack_handle_t handle;
	unsigned int nr_entries;

	nr_entries = stack_trace_save(entries, ARRAY_SIZE(entries), 2);

	/*
	 * We need to check recursion here because our request to
	 * stackdepot could trigger memory allocation to save new
	 * entry. New memory allocation would reach here and call
	 * stack_depot_save_entries() again if we don't catch it. There is
	 * still not enough memory in stackdepot so it would try to
	 * allocate memory again and loop forever.
	 */
	if (check_recursive_alloc(entries, nr_entries, _RET_IP_))
		return dummy_handle;

	handle = stack_depot_save(entries, nr_entries, flags);
	if (!handle)
		handle = failure_handle;

	return handle;
}

void __reset_page_owner(struct page *page, unsigned int order)
{
	int i;
	struct page_ext *page_ext;
	depot_stack_handle_t handle = 0;
	struct page_owner *page_owner;
	u64 free_ts_nsec = local_clock();

	handle = save_stack(GFP_NOWAIT | __GFP_NOWARN);

	page_ext = lookup_page_ext(page);
	if (unlikely(!page_ext))
		return;
	for (i = 0; i < (1 << order); i++) {
		__clear_bit(PAGE_EXT_OWNER_ALLOCATED, &page_ext->flags);
#ifdef CONFIG_PAGE_EXTENSION_PAGE_FREE
		__set_bit(PAGE_EXT_PG_FREE, &page_ext->flags);
#endif
		page_owner = get_page_owner(page_ext);
		page_owner->free_handle = handle;
		page_owner->free_ts_nsec = free_ts_nsec;
		page_ext = page_ext_next(page_ext);
	}
}

static inline void __set_page_owner_handle(struct page *page,
	struct page_ext *page_ext, depot_stack_handle_t handle,
	unsigned int order, gfp_t gfp_mask)
{
	struct page_owner *page_owner;
	int i;

	for (i = 0; i < (1 << order); i++) {
		page_owner = get_page_owner(page_ext);
		page_owner->handle = handle;
		page_owner->order = order;
		page_owner->gfp_mask = gfp_mask;
		page_owner->last_migrate_reason = -1;
		page_owner->pid = current->pid;
		page_owner->ts_nsec = local_clock();
		page_owner->free_ts_nsec = 0;

		__set_bit(PAGE_EXT_OWNER, &page_ext->flags);
		__set_bit(PAGE_EXT_OWNER_ALLOCATED, &page_ext->flags);
#ifdef CONFIG_PAGE_EXTENSION_PAGE_FREE
		__clear_bit(PAGE_EXT_PG_FREE, &page_ext->flags);
#endif

		page_ext = page_ext_next(page_ext);
	}
}

noinline void __set_page_owner(struct page *page, unsigned int order,
					gfp_t gfp_mask)
{
	struct page_ext *page_ext = lookup_page_ext(page);
	depot_stack_handle_t handle;

	if (unlikely(!page_ext))
		return;

	handle = save_stack(gfp_mask);
	__set_page_owner_handle(page, page_ext, handle, order, gfp_mask);
}

void __set_page_owner_migrate_reason(struct page *page, int reason)
{
	struct page_ext *page_ext = lookup_page_ext(page);
	struct page_owner *page_owner;

	if (unlikely(!page_ext))
		return;

	page_owner = get_page_owner(page_ext);
	page_owner->last_migrate_reason = reason;
}

void __split_page_owner(struct page *page, unsigned int nr)
{
	int i;
	struct page_ext *page_ext = lookup_page_ext(page);
	struct page_owner *page_owner;

	if (unlikely(!page_ext))
		return;

	for (i = 0; i < nr; i++) {
		page_owner = get_page_owner(page_ext);
		page_owner->order = 0;
		page_ext = page_ext_next(page_ext);
	}
}

void __copy_page_owner(struct page *oldpage, struct page *newpage)
{
	struct page_ext *old_ext = lookup_page_ext(oldpage);
	struct page_ext *new_ext = lookup_page_ext(newpage);
	struct page_owner *old_page_owner, *new_page_owner;

	if (unlikely(!old_ext || !new_ext))
		return;

	old_page_owner = get_page_owner(old_ext);
	new_page_owner = get_page_owner(new_ext);
	new_page_owner->order = old_page_owner->order;
	new_page_owner->gfp_mask = old_page_owner->gfp_mask;
	new_page_owner->last_migrate_reason =
		old_page_owner->last_migrate_reason;
	new_page_owner->handle = old_page_owner->handle;
	new_page_owner->pid = old_page_owner->pid;
	new_page_owner->ts_nsec = old_page_owner->ts_nsec;
	new_page_owner->free_ts_nsec = old_page_owner->ts_nsec;

	/*
	 * We don't clear the bit on the oldpage as it's going to be freed
	 * after migration. Until then, the info can be useful in case of
	 * a bug, and the overal stats will be off a bit only temporarily.
	 * Also, migrate_misplaced_transhuge_page() can still fail the
	 * migration and then we want the oldpage to retain the info. But
	 * in that case we also don't need to explicitly clear the info from
	 * the new page, which will be freed.
	 */
	__set_bit(PAGE_EXT_OWNER, &new_ext->flags);
	__set_bit(PAGE_EXT_OWNER_ALLOCATED, &new_ext->flags);
}

void pagetypeinfo_showmixedcount_print(struct seq_file *m,
				       pg_data_t *pgdat, struct zone *zone)
{
	struct page *page;
	struct page_ext *page_ext;
	struct page_owner *page_owner;
	unsigned long pfn = zone->zone_start_pfn, block_end_pfn;
	unsigned long end_pfn = pfn + zone->spanned_pages;
	unsigned long count[MIGRATE_TYPES] = { 0, };
	int pageblock_mt, page_mt;
	int i;

	/* Scan block by block. First and last block may be incomplete */
	pfn = zone->zone_start_pfn;

	/*
	 * Walk the zone in pageblock_nr_pages steps. If a page block spans
	 * a zone boundary, it will be double counted between zones. This does
	 * not matter as the mixed block count will still be correct
	 */
	for (; pfn < end_pfn; ) {
		page = pfn_to_online_page(pfn);
		if (!page) {
			pfn = ALIGN(pfn + 1, MAX_ORDER_NR_PAGES);
			continue;
		}

		block_end_pfn = ALIGN(pfn + 1, pageblock_nr_pages);
		block_end_pfn = min(block_end_pfn, end_pfn);

		pageblock_mt = get_pageblock_migratetype(page);

		for (; pfn < block_end_pfn; pfn++) {
			if (!pfn_valid_within(pfn))
				continue;

			/* The pageblock is online, no need to recheck. */
			page = pfn_to_page(pfn);

			if (page_zone(page) != zone)
				continue;

			if (PageBuddy(page)) {
				unsigned long freepage_order;

				freepage_order = page_order_unsafe(page);
				if (freepage_order < MAX_ORDER)
					pfn += (1UL << freepage_order) - 1;
				continue;
			}

			if (PageReserved(page))
				continue;

			page_ext = lookup_page_ext(page);
			if (unlikely(!page_ext))
				continue;

			if (!test_bit(PAGE_EXT_OWNER_ALLOCATED, &page_ext->flags))
				continue;

			page_owner = get_page_owner(page_ext);
			page_mt = gfpflags_to_migratetype(
					page_owner->gfp_mask);
			if (pageblock_mt != page_mt) {
				if (is_migrate_cma(pageblock_mt))
					count[MIGRATE_MOVABLE]++;
				else
					count[pageblock_mt]++;

				pfn = block_end_pfn;
				break;
			}
			pfn += (1UL << page_owner->order) - 1;
		}
	}

	/* Print counts */
	seq_printf(m, "Node %d, zone %8s ", pgdat->node_id, zone->name);
	for (i = 0; i < MIGRATE_TYPES; i++)
		seq_printf(m, "%12lu ", count[i]);
	seq_putc(m, '\n');
}

static ssize_t
print_page_owner(char __user *buf, size_t count, unsigned long pfn,
		struct page *page, struct page_owner *page_owner,
		depot_stack_handle_t handle)
{
	int ret, pageblock_mt, page_mt;
	unsigned long *entries;
	unsigned int nr_entries;
	char *kbuf;

	count = min_t(size_t, count, PAGE_SIZE);
	kbuf = kmalloc(count, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	ret = snprintf(kbuf, count,
			"Page allocated via order %u, mask %#x(%pGg), pid %d, ts %llu ns\n",
			page_owner->order, page_owner->gfp_mask,
			&page_owner->gfp_mask, page_owner->pid,
			page_owner->ts_nsec);

	if (ret >= count)
		goto err;

	/* Print information relevant to grouping pages by mobility */
	pageblock_mt = get_pageblock_migratetype(page);
	page_mt  = gfpflags_to_migratetype(page_owner->gfp_mask);
	ret += snprintf(kbuf + ret, count - ret,
			"PFN %lu type %s Block %lu type %s Flags %#lx(%pGp)\n",
			pfn,
			migratetype_names[page_mt],
			pfn >> pageblock_order,
			migratetype_names[pageblock_mt],
			page->flags, &page->flags);

	if (ret >= count)
		goto err;

	nr_entries = stack_depot_fetch(handle, &entries);
	ret += stack_trace_snprint(kbuf + ret, count - ret, entries, nr_entries, 0);
	if (ret >= count)
		goto err;

	if (page_owner->last_migrate_reason != -1) {
		ret += snprintf(kbuf + ret, count - ret,
			"Page has been migrated, last migrate reason: %s\n",
			migrate_reason_names[page_owner->last_migrate_reason]);
		if (ret >= count)
			goto err;
	}

	ret += snprintf(kbuf + ret, count - ret, "\n");
	if (ret >= count)
		goto err;

	if (copy_to_user(buf, kbuf, ret))
		ret = -EFAULT;

	kfree(kbuf);
	return ret;

err:
	kfree(kbuf);
	return -ENOMEM;
}

#ifdef CONFIG_QCOM_MINIDUMP_PANIC_DUMP

static unsigned long page_owner_filter = 0xF;
static unsigned long page_owner_handles_size =  SZ_16K;
static int nr_handles;
static LIST_HEAD(accounted_call_site_list);
static DEFINE_MUTEX(accounted_call_site_lock);
struct accounted_call_site {
	struct list_head list;
	char name[50];
};

bool is_page_owner_enabled(void)
{
	return page_owner_enabled;
}

static bool found_stack(depot_stack_handle_t handle,
		 char *md_pageowner_dump_addr, char *cur)
{
	int *handles, i;

	handles = (int *) (md_pageowner_dump_addr +
			md_pageowner_dump_size - page_owner_handles_size);

	for (i = 0; i < nr_handles; i++)
		if (handle == handles[i])
			return true;

	if ((handles + nr_handles)
		< (int *)(md_pageowner_dump_addr +
			md_pageowner_dump_size)) {
		handles[nr_handles] = handle;
		nr_handles += 1;
	} else {
		pr_err_ratelimited("Can't stores handles increase page_owner_handles_size\n");
	}
	return false;
}

static bool check_unaccounted(char *buf, ssize_t count,
		struct page *page, depot_stack_handle_t handle)
{
	int i, ret = 0;
	unsigned long *entries;
	unsigned int nr_entries;
	struct accounted_call_site *call_site;

	if ((page->flags &
		((1UL << PG_lru) | (1UL << PG_slab) | (1UL << PG_swapbacked))))
		return false;

	nr_entries = stack_depot_fetch(handle, &entries);
	for (i = 0; i < nr_entries; i++) {
		ret = scnprintf(buf, count, "%pS\n",
				(void *)entries[i]);
		if (ret == count)
			return false;

		mutex_lock(&accounted_call_site_lock);
		list_for_each_entry(call_site,
				&accounted_call_site_list, list) {
			if (strnstr(buf, call_site->name,
					strlen(buf))) {
				mutex_unlock(&accounted_call_site_lock);
				return false;
			}
		}
		mutex_unlock(&accounted_call_site_lock);
	}
	return true;
}

static ssize_t
dump_page_owner_md(char *buf, size_t count, unsigned long pfn,
		struct page *page, struct page_owner *page_owner,
		depot_stack_handle_t handle)
{
	int i, bit, ret = 0;
	unsigned long *entries;
	unsigned int nr_entries;

	if (page_owner_filter == 0xF)
		goto dump;

	for (bit = 1; page_owner_filter >= bit; bit *= 2) {
		if (page_owner_filter & bit) {
			switch (bit) {
			case 0x1:
				if (check_unaccounted(buf, count, page, handle))
					goto dump;
				break;
			case 0x2:
				if (page->flags & (1UL << PG_slab))
					goto dump;
				break;
			case 0x4:
				if (page->flags & (1UL << PG_swapbacked))
					goto dump;
				break;
			case 0x8:
				if ((page->flags & (1UL << PG_lru)) &&
					~(page->flags & (1UL << PG_swapbacked)))
					goto dump;
				break;
			default:
				break;
			}
		}
		if (bit >= 0x8)
			return ret;
	}

	if (bit > page_owner_filter)
		return ret;
dump:
	nr_entries = stack_depot_fetch(handle, &entries);
	if ((buf > (md_pageowner_dump_addr +
			md_pageowner_dump_size - page_owner_handles_size))
			|| !found_stack(handle, md_pageowner_dump_addr, buf)) {
		ret = scnprintf(buf, count, "%lu %u %u\n",
				pfn, handle, nr_entries);
		if (ret == count)
			goto err;

		for (i = 0; i < nr_entries; i++) {
			ret += scnprintf(buf + ret, count - ret,
					"%p\n", (void *)entries[i]);
			if (ret == count)
				goto err;
		}
	} else {
		ret = scnprintf(buf, count, "%lu %u %u\n",  pfn, handle, 0);
	}
err:
	return ret;
}
#endif

void __dump_page_owner(struct page *page)
{
	struct page_ext *page_ext = lookup_page_ext(page);
	struct page_owner *page_owner;
	depot_stack_handle_t handle;
	unsigned long *entries;
	unsigned int nr_entries;
	gfp_t gfp_mask;
	int mt;

	if (unlikely(!page_ext)) {
		pr_alert("There is not page extension available.\n");
		return;
	}

	page_owner = get_page_owner(page_ext);
	gfp_mask = page_owner->gfp_mask;
	mt = gfpflags_to_migratetype(gfp_mask);

	if (!test_bit(PAGE_EXT_OWNER, &page_ext->flags)) {
		pr_alert("page_owner info is not present (never set?)\n");
		return;
	}

	if (test_bit(PAGE_EXT_OWNER_ALLOCATED, &page_ext->flags))
		pr_alert("page_owner tracks the page as allocated\n");
	else
		pr_alert("page_owner tracks the page as freed\n");

	pr_alert("page last allocated via order %u, migratetype %s, gfp_mask %#x(%pGg), pid %d, ts %llu ns\n",
		 page_owner->order, migratetype_names[mt], gfp_mask, &gfp_mask,
		 page_owner->pid, page_owner->ts_nsec);

	handle = READ_ONCE(page_owner->handle);
	if (!handle) {
		pr_alert("page_owner allocation stack trace missing\n");
	} else {
		nr_entries = stack_depot_fetch(handle, &entries);
		stack_trace_print(entries, nr_entries, 0);
	}

	handle = READ_ONCE(page_owner->free_handle);
	if (!handle) {
		pr_alert("page_owner free stack trace missing\n");
	} else {
		nr_entries = stack_depot_fetch(handle, &entries);
		pr_alert("page last free stack trace:\n");
		stack_trace_print(entries, nr_entries, 0);
	}

	if (page_owner->last_migrate_reason != -1)
		pr_alert("page has been migrated, last migrate reason: %s\n",
			migrate_reason_names[page_owner->last_migrate_reason]);
}

static ssize_t
read_page_owner(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	unsigned long pfn;
	struct page *page;
	struct page_ext *page_ext;
	struct page_owner *page_owner;
	depot_stack_handle_t handle;

#ifdef CONFIG_QCOM_MINIDUMP_PANIC_DUMP
	char *addr;
	ssize_t size;

	addr = md_pageowner_dump_addr;
#endif
	if (!static_branch_unlikely(&page_owner_inited))
		return -EINVAL;

	page = NULL;
	pfn = min_low_pfn + *ppos;

	/* Find a valid PFN or the start of a MAX_ORDER_NR_PAGES area */
	while (!pfn_valid(pfn) && (pfn & (MAX_ORDER_NR_PAGES - 1)) != 0)
		pfn++;

	if (file)
		drain_all_pages(NULL);

	/* Find an allocated page */
	for (; pfn < max_pfn; pfn++) {
		/*
		 * If the new page is in a new MAX_ORDER_NR_PAGES area,
		 * validate the area as existing, skip it if not
		 */
		if ((pfn & (MAX_ORDER_NR_PAGES - 1)) == 0 && !pfn_valid(pfn)) {
			pfn += MAX_ORDER_NR_PAGES - 1;
			continue;
		}

		/* Check for holes within a MAX_ORDER area */
		if (!pfn_valid_within(pfn))
			continue;

		page = pfn_to_page(pfn);
		if (PageBuddy(page)) {
			unsigned long freepage_order = page_order_unsafe(page);

			if (freepage_order < MAX_ORDER)
				pfn += (1UL << freepage_order) - 1;
			continue;
		}

		page_ext = lookup_page_ext(page);
		if (unlikely(!page_ext))
			continue;

		/*
		 * Some pages could be missed by concurrent allocation or free,
		 * because we don't hold the zone lock.
		 */
		if (!test_bit(PAGE_EXT_OWNER, &page_ext->flags))
			continue;

		/*
		 * Although we do have the info about past allocation of free
		 * pages, it's not relevant for current memory usage.
		 */
		if (!test_bit(PAGE_EXT_OWNER_ALLOCATED, &page_ext->flags))
			continue;

		page_owner = get_page_owner(page_ext);

		/*
		 * Don't print "tail" pages of high-order allocations as that
		 * would inflate the stats.
		 */
		if (!IS_ALIGNED(pfn, 1 << page_owner->order))
			continue;

		/*
		 * Access to page_ext->handle isn't synchronous so we should
		 * be careful to access it.
		 */
		handle = READ_ONCE(page_owner->handle);
		if (!handle)
			continue;

		/* Record the next PFN to read in the file offset */
		*ppos = (pfn - min_low_pfn) + 1;

		if (file) {
			return print_page_owner(buf, count, pfn, page,
				page_owner, handle);
		} else {
#ifdef CONFIG_QCOM_MINIDUMP_PANIC_DUMP
			size = dump_page_owner_md(addr, count, pfn, page,
				page_owner, handle);
			if (size == count) {
				pr_err("pageowner minidump region exhausted\n");
				return 0;
			}
			count -= size;
			addr += size;
#endif
		}
	}
	return 0;
}

#ifdef CONFIG_QCOM_MINIDUMP_PANIC_DUMP
void md_dump_pageowner(void)
{
	loff_t k = 0;

	read_page_owner(NULL, NULL, md_pageowner_dump_size, &k);
}
#endif

static void init_pages_in_zone(pg_data_t *pgdat, struct zone *zone)
{
	unsigned long pfn = zone->zone_start_pfn;
	unsigned long end_pfn = zone_end_pfn(zone);
	unsigned long count = 0;

	/*
	 * Walk the zone in pageblock_nr_pages steps. If a page block spans
	 * a zone boundary, it will be double counted between zones. This does
	 * not matter as the mixed block count will still be correct
	 */
	for (; pfn < end_pfn; ) {
		unsigned long block_end_pfn;

		if (!pfn_valid(pfn)) {
			pfn = ALIGN(pfn + 1, MAX_ORDER_NR_PAGES);
			continue;
		}

		block_end_pfn = ALIGN(pfn + 1, pageblock_nr_pages);
		block_end_pfn = min(block_end_pfn, end_pfn);

		for (; pfn < block_end_pfn; pfn++) {
			struct page *page;
			struct page_ext *page_ext;

			if (!pfn_valid_within(pfn))
				continue;

			page = pfn_to_page(pfn);

			if (page_zone(page) != zone)
				continue;

			/*
			 * To avoid having to grab zone->lock, be a little
			 * careful when reading buddy page order. The only
			 * danger is that we skip too much and potentially miss
			 * some early allocated pages, which is better than
			 * heavy lock contention.
			 */
			if (PageBuddy(page)) {
				unsigned long order = page_order_unsafe(page);

				if (order > 0 && order < MAX_ORDER)
					pfn += (1UL << order) - 1;
				continue;
			}

			if (PageReserved(page))
				continue;

			page_ext = lookup_page_ext(page);
			if (unlikely(!page_ext))
				continue;

			/* Maybe overlapping zone */
			if (test_bit(PAGE_EXT_OWNER, &page_ext->flags))
				continue;

			/* Found early allocated page */
			__set_page_owner_handle(page, page_ext, early_handle,
						0, 0);
			count++;
		}
		cond_resched();
	}

	pr_info("Node %d, zone %8s: page owner found early allocated %lu pages\n",
		pgdat->node_id, zone->name, count);
}

static void init_zones_in_node(pg_data_t *pgdat)
{
	struct zone *zone;
	struct zone *node_zones = pgdat->node_zones;

	for (zone = node_zones; zone - node_zones < MAX_NR_ZONES; ++zone) {
		if (!populated_zone(zone))
			continue;

		init_pages_in_zone(pgdat, zone);
	}
}

static void init_early_allocated_pages(void)
{
	pg_data_t *pgdat;

	for_each_online_pgdat(pgdat)
		init_zones_in_node(pgdat);
}

static const struct file_operations proc_page_owner_operations = {
	.read		= read_page_owner,
};

#ifdef CONFIG_QCOM_MINIDUMP_PANIC_DUMP
static ssize_t page_owner_filter_write(struct file *file,
					  const char __user *ubuf,
					  size_t count, loff_t *offset)
{
	unsigned long filter;

	if (kstrtoul_from_user(ubuf, count, 0, &filter)) {
		pr_err_ratelimited("Invalid format for filter\n");
		return -EINVAL;
	}

	if (filter & (~0xF)) {
		pr_err_ratelimited("Invalid filter : use following filters or any combinations of these\n"
				"0x1 - unaccounted\n"
				"0x2 - slab\n"
				"0x4 - Anon\n"
				"0x8 - File\n");
		return -EINVAL;
	}
	page_owner_filter = filter;
	return count;
}

static ssize_t page_owner_filter_read(struct file *file, char __user *ubuf,
				       size_t count, loff_t *offset)
{
	char buf[64];

	snprintf(buf, sizeof(buf), "0x%lx\n", page_owner_filter);
	return simple_read_from_buffer(ubuf, count, offset, buf, strlen(buf));
}

static const struct file_operations proc_page_owner_filter_ops = {
	.open	= simple_open,
	.write	= page_owner_filter_write,
	.read	= page_owner_filter_read,
};

static ssize_t page_owner_handle_write(struct file *file,
					  const char __user *ubuf,
					  size_t count, loff_t *offset)
{
	unsigned long size;

	if (kstrtoul_from_user(ubuf, count, 0, &size)) {
		pr_err_ratelimited("Invalid format for handle size\n");
		return -EINVAL;
	}

	if (size) {
		if (size > (md_pageowner_dump_size / SZ_16K)) {
			pr_err_ratelimited("size : %lu KB exceeds max size : %lu KB\n",
				size, (md_pageowner_dump_size / SZ_16K));
			goto err;
		}
		page_owner_handles_size = size * SZ_1K;
	}
err:
	return count;
}

static ssize_t page_owner_handle_read(struct file *file, char __user *ubuf,
				       size_t count, loff_t *offset)
{
	char buf[64];

	snprintf(buf, sizeof(buf), "%lu KB\n",
			(page_owner_handles_size / SZ_1K));
	return simple_read_from_buffer(ubuf, count, offset, buf, strlen(buf));
}

static const struct file_operations proc_page_owner_handle_ops = {
	.open	= simple_open,
	.write	= page_owner_handle_write,
	.read	= page_owner_handle_read,
};

static ssize_t page_owner_call_site_write(struct file *file,
					  const char __user *ubuf,
					  size_t count, loff_t *offset)
{
	struct accounted_call_site *call_site;
	char buf[50];

	if (count >= 50) {
		pr_err_ratelimited("Input string size too large\n");
		return -EINVAL;
	}

	memset(buf, 0, 50);

	if (copy_from_user(buf, ubuf, count)) {
		pr_err_ratelimited("Couldn't copy from user\n");
		return -EFAULT;
	}

	if (!isalpha(buf[0]) && buf[0] != '_') {
		pr_err_ratelimited("Invalid call site name\n");
		return -EINVAL;
	}

	call_site = kzalloc(sizeof(*call_site), GFP_KERNEL);
	if (!call_site)
		return -ENOMEM;

	strlcpy(call_site->name, buf, strlen(buf));
	mutex_lock(&accounted_call_site_lock);
	list_add_tail(&call_site->list, &accounted_call_site_list);
	mutex_unlock(&accounted_call_site_lock);

	return count;
}

static ssize_t page_owner_call_site_read(struct file *file, char __user *ubuf,
				       size_t count, loff_t *offset)
{
	char *kbuf;
	struct accounted_call_site *call_site;
	int i = 1, ret = 0;
	size_t size = PAGE_SIZE;

	kbuf = kmalloc(size, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	ret = scnprintf(kbuf, count, "%s\n", "Accounted call sites:");
	mutex_lock(&accounted_call_site_lock);
	list_for_each_entry(call_site, &accounted_call_site_list, list) {
		ret += scnprintf(kbuf + ret, size - ret,
			"%d. %s\n", i, call_site->name);
		i += 1;
		if (ret == size) {
			ret = -ENOMEM;
			mutex_unlock(&accounted_call_site_lock);
			goto err;
		}
	}
	mutex_unlock(&accounted_call_site_lock);
	ret = simple_read_from_buffer(ubuf, count, offset, kbuf, strlen(kbuf));
err:
	kfree(kbuf);
	return ret;
}

static const struct file_operations proc_page_owner_call_site_ops = {
	.open	= simple_open,
	.write	= page_owner_call_site_write,
	.read	= page_owner_call_site_read,
};
#endif

static int __init pageowner_init(void)
{
	if (!static_branch_unlikely(&page_owner_inited)) {
		pr_info("page_owner is disabled\n");
		return 0;
	}

	debugfs_create_file("page_owner", 0400, NULL, NULL,
			    &proc_page_owner_operations);

#ifdef CONFIG_QCOM_MINIDUMP_PANIC_DUMP
	debugfs_create_file("page_owner_filter", 0400, NULL, NULL,
			    &proc_page_owner_filter_ops);
	debugfs_create_file("page_owner_handles_size_kb", 0400, NULL, NULL,
			    &proc_page_owner_handle_ops);
	debugfs_create_file("page_owner_call_sites", 0400, NULL, NULL,
			    &proc_page_owner_call_site_ops);
#endif
	return 0;
}
late_initcall(pageowner_init)
