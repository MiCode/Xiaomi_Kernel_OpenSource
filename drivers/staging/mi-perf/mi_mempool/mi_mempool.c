#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/page-flags.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/swap.h>
#include <linux/slab.h>
#include <linux/sched/signal.h>
#include <linux/list.h>
#include <linux/version.h>
#include <linux/kthread.h>
#include <trace/hooks/mm.h>
#include <trace/hooks/sched.h>
#include "mi_mempool.h"

static struct mi_mempool g_mi_mempool;
static struct mi_mempool *pg_mi_mempool;
static struct rb_root proc_tree = RB_ROOT;
static DEFINE_SPINLOCK(proc_tree_lock);
static struct kmem_cache *proc_node_cachep;

static struct proc_node *get_proc_node_by_pid_locked(pid_t pid)
{
	struct rb_node *node = proc_tree.rb_node;
	struct proc_node *ret = NULL;

	while (node) {
		ret = rb_entry(node, struct proc_node, rb_node);
		if (ret->pid > pid)
			node = node->rb_right;
		else if (ret->pid < pid)
			node = node->rb_left;
		else
			return ret;
	}
	return NULL;
}

static struct proc_node *insert_proc_node_by_pid_locked(struct proc_node *p_node)
{
	struct rb_node **p = &proc_tree.rb_node;
	struct rb_node *parent = NULL;
	struct proc_node *ret = NULL;

	while (*p) {
		parent = *p;
		ret = rb_entry(*p, struct proc_node, rb_node);
		if (ret->pid > p_node->pid)
			p = &parent->rb_right;
		else if (ret->pid < p_node->pid)
			p = &parent->rb_left;
		else
			return ret;
	}
	rb_link_node(&p_node->rb_node, parent, p);
	rb_insert_color(&p_node->rb_node, &proc_tree);
	pg_mi_mempool->proc_tree_size++;
	return NULL;
}

struct proc_node *del_proc_node_by_pid_locked(pid_t pid)
{
	struct proc_node *node = get_proc_node_by_pid_locked(pid);

	if (node) {
		rb_erase(&node->rb_node, &proc_tree);
		pg_mi_mempool->proc_tree_size--;
	}
	return node;
}

static inline struct proc_node *proc_node_alloc(gfp_t gfp)
{
	return proc_node_cachep != NULL ? kmem_cache_alloc(proc_node_cachep, gfp) : NULL;
}

static void proc_node_free(struct proc_node *proc_node)
{
	if (proc_node_cachep != NULL)
		kmem_cache_free(proc_node_cachep, proc_node);
}

static bool proc_node_init(void)
{
	proc_node_cachep = kmem_cache_create("proc_node", sizeof(struct proc_node),
			0, 0, NULL);

	return proc_node_cachep != NULL;
}

static void insert_new_proc_node(pid_t pid, int process_id)
{
	struct proc_node *node = proc_node_alloc(GFP_ATOMIC);

	if (node) {
		unsigned long flags;

		node->pid = pid;
		node->process_id = process_id;
		spin_lock_irqsave(&proc_tree_lock, flags);
		if (insert_proc_node_by_pid_locked(node))
			proc_node_free(node);
		spin_unlock_irqrestore(&proc_tree_lock, flags);
	} else if (pg_mi_mempool->config & DEBUG_WHITELIST_CHECK) {
		atomic_inc(&pg_mi_mempool->proc_node_alloc_failure);
	}
}

static void reset_proc_tree(void)
{
	struct proc_node *pos = NULL, *n = NULL;
	unsigned long flags;

	spin_lock_irqsave(&proc_tree_lock, flags);
	rbtree_postorder_for_each_entry_safe(pos, n, &proc_tree, rb_node) {
		proc_node_free(pos);
	}
	proc_tree = RB_ROOT;
	spin_unlock_irqrestore(&proc_tree_lock, flags);
}

static void free_all_proc_nodes_and_caches(void)
{
	reset_proc_tree();
	kmem_cache_destroy(proc_node_cachep);
}

/* Based on prep_compound_page from mm/page_alloc.c since it is not exported. */
static void prep_compound_page(struct page *page, unsigned int order)
{
	int i;
	int nr_pages = 1 << order;

	__SetPageHead(page);
	for (i = 1; i < nr_pages; i++) {
		struct page *p = page + i;

		set_page_count(p, 0);
		p->mapping = TAIL_MAPPING;
		set_compound_head(p, page);
	}
	set_compound_page_dtor(page, COMPOUND_PAGE_DTOR);
	set_compound_order(page, order);
	atomic_set(compound_mapcount_ptr(page), -1);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 4, 0))
	if (hpage_pincount_available(page))
		atomic_set(compound_pincount_ptr(page), 0);
#endif
}

static inline void mi_dynamic_page_pool_add(struct mi_dynamic_page_pool *pool, struct page *page)
{
	unsigned long flags;

	spin_lock_irqsave(&pool->lock, flags);
	list_add(&page->lru, &pool->list);
	mod_node_page_state(page_pgdat(page), NR_KERNEL_MISC_RECLAIMABLE,
			    1 << pool->order);
	spin_unlock_irqrestore(&pool->lock, flags);
	atomic_inc(&pool->count);
	atomic_add(1 << pool->order, &pg_mi_mempool->total_page_num);
}

static inline struct page *mi_dynamic_page_pool_remove(struct mi_dynamic_page_pool *pool)
{
	struct page *page;
	unsigned long flags;

	spin_lock_irqsave(&pool->lock, flags);
	page = list_first_entry_or_null(&pool->list, struct page, lru);
	if (page) {
		list_del(&page->lru);
		mod_node_page_state(page_pgdat(page), NR_KERNEL_MISC_RECLAIMABLE,
			    -(1 << pool->order));
		spin_unlock_irqrestore(&pool->lock, flags);
		atomic_dec(&pool->count);
		atomic_sub(1 << pool->order, &pg_mi_mempool->total_page_num);
	} else
		spin_unlock_irqrestore(&pool->lock, flags);
	return page;
}

static inline struct mi_dynamic_page_pool *order_to_pool(int order)
{
	struct mi_dynamic_page_pool *pool = NULL;

	if (order < MAX_ORDER) {
		int pool_id = order_map[order];

		if (pool_id != -1)
			pool = pg_mi_mempool->pools[pool_id];
	}
	return pool;
}

static int check_whitelist(void)
{
	struct proc_node *node = NULL;
	unsigned long flags;

	if (pg_mi_mempool->config & DEBUG_WHITELIST_CHECK)
		atomic_inc(&pg_mi_mempool->whitelist_check_num);
	spin_lock_irqsave(&proc_tree_lock, flags);
	node = get_proc_node_by_pid_locked(current->group_leader->pid);
	spin_unlock_irqrestore(&proc_tree_lock, flags);
	if (!node) {
		int i;

		for (i = 0; comm_whitelist[i]; i++) {
			if (adj_whitelist[i] == INVALID_ADJ ||
				adj_whitelist[i] == current->signal->oom_score_adj)
				if (current->group_leader != NULL
					&& strncmp(current->group_leader->comm,
						comm_whitelist[i],
						comm_whitelist_sizes[i]) == 0) {
					insert_new_proc_node(current->group_leader->pid,
						i + 1);
					return i + 1;
				}
		}
		insert_new_proc_node(current->group_leader->pid, 0);
		return 0;
	}
	return node->process_id;
}

static struct mi_dynamic_page_pool *check_whitelist_and_order(int order, int *process_id)
{
	struct mi_dynamic_page_pool *pool = order_to_pool(order);

	*process_id = check_whitelist();
	if (!(*process_id))
		pool = NULL;
	return pool;
}

static bool check_last_refill_time_interval(void)
{
	s64 delta;

	delta = ktime_to_ms(ktime_get()) - atomic_long_read(&pg_mi_mempool->last_refill_time);
	return delta < MI_DYNAMIC_POOL_REFILL_MIN_INTERVAL_MS;
}

static bool check_watermark_interval(struct mi_dynamic_page_pool *pool)
{
	s64 delta;

	delta = ktime_ms_delta(ktime_get(), pool->last_check_watermark_time);
	return delta < MI_DYNAMIC_POOL_REFILL_MIN_CHECK_INTERVAL_MS;
}

/*
 * Based on __zone_watermark_ok() in mm/page_alloc.c since it is not exported.
 *
 * Return true if free base pages are above 'mark'. For high-order checks it
 * will return true of the order-0 watermark is reached and there is at least
 * one free page of a suitable size. Checking now avoids taking the zone lock
 * to check in the allocation paths if no pages are free.
 */
static bool __mi_dynamic_pool_zone_watermark_ok(struct zone *z, unsigned int order,
	unsigned long mark, int highest_zoneidx, long free_pages)
{
	long min = mark;
	long unusable_free;
	int o;

	/*
	 * Access to high atomic reserves is not required, and CMA should not be
	 * used, since these allocations are non-movable.
	 */
	unusable_free = ((1 << order) - 1) + z->nr_reserved_highatomic;
#ifdef CONFIG_CMA
	unusable_free += zone_page_state(z, NR_FREE_CMA_PAGES);
#endif

	/* free_pages may go negative - that's OK */
	free_pages -= unusable_free;

	/*
	 * Check watermarks for an order-0 allocation request. If these
	 * are not met, then a high-order request also cannot go ahead
	 * even if a suitable page happened to be free.
	 *
	 * 'min' can be taken as 'mark' since we do not expect these allocations
	 * to require disruptive actions (such as running the OOM killer) or
	 * a lot of effort.
	 */
	if (free_pages <= min + z->lowmem_reserve[highest_zoneidx])
		return false;

	/* If this is an order-0 request then the watermark is fine */
	if (!order)
		return true;

	/* For a high-order request, check at least one suitable page is free */
	for (o = order; o < MAX_ORDER; o++) {
		struct free_area *area = &z->free_area[o];
		int mt;

		if (!area->nr_free)
			continue;

		for (mt = 0; mt < MIGRATE_PCPTYPES; mt++) {
			if (!free_area_empty(area, mt))
				return true;
		}
	}

	return false;
}

/* Based on zone_watermark_ok_safe from mm/page_alloc.c since it is not exported. */
static bool mi_dynamic_pool_zone_watermark_ok_safe(struct zone *z, unsigned int order,
						unsigned long mark, int highest_zoneidx)
{
	long free_pages = zone_page_state(z, NR_FREE_PAGES);

	if (z->percpu_drift_mark && free_pages < z->percpu_drift_mark)
		free_pages = zone_page_state_snapshot(z, NR_FREE_PAGES);

	return __mi_dynamic_pool_zone_watermark_ok(z, order, mark, highest_zoneidx, free_pages);
}

/* Based on gfp_zone() in mm/mmzone.c since it is not exported. */
static enum zone_type mi_dynamic_pool_gfp_zone(gfp_t flags)
{
	enum zone_type z;
	gfp_t local_flags = flags;
	int bit;

	bit = (__force int) ((local_flags) & GFP_ZONEMASK);
	z = (GFP_ZONE_TABLE >> (bit * GFP_ZONES_SHIFT)) &
					 ((1 << GFP_ZONES_SHIFT) - 1);
	VM_BUG_ON((GFP_ZONE_BAD >> bit) & 1);
	return z;
}

static bool check_watermark(struct mi_dynamic_page_pool *pool)
{
	int mark, i;
	enum zone_type classzone_idx = mi_dynamic_pool_gfp_zone(pool->gfp_mask);
	int free_pages;

	if (check_watermark_interval(pool))
		return true;

	for (i = classzone_idx; i >= 0; i--) {
		struct zone *zone = &NODE_DATA(numa_node_id())->node_zones[i];

		if (!strcmp(zone->name, "DMA32"))
			continue;
		mark = low_wmark_pages(zone);
		mark += 1 << pool->order;
		free_pages = zone_page_state(zone, NR_FREE_PAGES);

		if (!mi_dynamic_pool_zone_watermark_ok_safe(zone, pool->order, mark,
						classzone_idx)) {
			if (pg_mi_mempool->config & DEBUG_LOG)
				pr_info("mi_mempool check_watermark name=%s pool->order=%d "
					"mark=%d free_pages=%d highatomic=%d cma=%d", zone->name,
					pool->order, mark, free_pages, zone->nr_reserved_highatomic,
					zone_page_state(zone, NR_FREE_CMA_PAGES));
			pool->last_check_watermark_time = ktime_get();
			return true;
		}
	}
	return false;
}

static inline bool mi_dynamic_page_pool_below_highmark(struct mi_dynamic_page_pool *pool)
{
	return atomic_read(&pool->count) < pool->high_mark;
}

static inline bool mi_dynamic_page_pool_below_lowmark(struct mi_dynamic_page_pool *pool)
{
	return atomic_read(&pool->count) <= pool->low_mark;
}

static int mi_dynamic_page_pool_refill(struct mi_dynamic_page_pool *pool)
{
	struct page *page = NULL;
	gfp_t gfp_mask = pool->gfp_mask;
	int count = 0;

	while (mi_dynamic_page_pool_below_highmark(pool) && !check_watermark(pool)) {
		page = alloc_pages(gfp_mask, pool->order);
		if (!page)
			break;
		mi_dynamic_page_pool_add(pool, page);
		count++;
	}
	return count;
}

/* Unified interface for using reserved memory */
static struct page *mi_dynamic_page_pool_alloc(int order, struct mi_dynamic_page_pool *pool,
	gfp_t gfp_mask, int alloc_flags, int migratetype, bool use_whitelist, bool is_oom)
{
	struct page *page = NULL;

	if (current == pg_mi_mempool->refill_worker)
		return page;
	if (pool) {
		enum zone_type classzone_idx = mi_dynamic_pool_gfp_zone(pool->gfp_mask);
		enum zone_type highest_zoneidx = mi_dynamic_pool_gfp_zone(gfp_mask);

		if (highest_zoneidx >= classzone_idx  && (!use_whitelist || check_whitelist())) {
			page = mi_dynamic_page_pool_remove(pool);
			if (page) {
				if (is_oom)
					atomic_inc(&pool->oom_use_count);
				else
					atomic_inc(&pool->reclaim_use_count);
			}
			if (page && order && (gfp_mask & __GFP_COMP))
				prep_compound_page(page, order);
			if (pg_mi_mempool->refill_worker != NULL &&
				mi_dynamic_page_pool_below_lowmark(pool) &&
				!check_last_refill_time_interval() &&
				!READ_ONCE(pg_mi_mempool->refill_worker_running)) {
				unsigned long flags;

				spin_lock_irqsave(&pg_mi_mempool->refill_wakeup_lock,
					flags);
				if (!READ_ONCE(pg_mi_mempool->refill_worker_running)) {
					WRITE_ONCE(pg_mi_mempool->refill_worker_running,
						true);
					wake_up_process(pg_mi_mempool->refill_worker);
				}
				spin_unlock_irqrestore(&pg_mi_mempool->refill_wakeup_lock,
					flags);
			}
		}
	}
	return page;
}

static unsigned long zone_can_reclaimable_pages(struct zone *zone)
{
	unsigned long nr;

	nr = zone_page_state_snapshot(zone, NR_ZONE_INACTIVE_FILE) +
		zone_page_state_snapshot(zone, NR_ZONE_ACTIVE_FILE);
	if (get_nr_swap_pages() > 0)
		nr += zone_page_state_snapshot(zone, NR_ZONE_INACTIVE_ANON) +
			zone_page_state_snapshot(zone, NR_ZONE_ACTIVE_ANON);

	return nr;
}

static bool allow_direct_reclaim(pg_data_t *pgdat)
{
	unsigned long free_pages = 0;
	int i;
	bool wmark_ok;

	for (i = 0; i <= ZONE_NORMAL; i++) {
		struct zone *zone = &pgdat->node_zones[i];

		if (!managed_zone(zone))
			continue;
		if (!zone_can_reclaimable_pages(zone))
			continue;
		free_pages += zone_page_state(zone, NR_FREE_PAGES);
	}
	wmark_ok = free_pages > pg_mi_mempool->direct_relcaim_pages_threshold;

	if (pg_mi_mempool->config & DEBUG_LOG)
		pr_info("mi_mempool allow_direct_reclaim comm=%s pid=%d free_pages=%d"
			" threshold=%d",
			current->comm, current->pid, free_pages,
			pg_mi_mempool->direct_relcaim_pages_threshold);

	return wmark_ok;
}

/*
 * Check if the process may get stuck in try_to_free_pages.
 * It's a approximation of throttle_direct_reclaim in
 * mm/vmscan.c.
 */
static bool throttle_check(gfp_t gfp_mask)
{
	pg_data_t *pgdat = NULL;

	if (current->flags & PF_KTHREAD)
		goto out;
	/*
	 * If a fatal signal is pending, this process should not throttle.
	 * It should return quickly so it can exit and free its memory
	 */
	if (fatal_signal_pending(current))
		goto out;

	pgdat = NODE_DATA(numa_node_id());

	if (allow_direct_reclaim(pgdat))
		goto out;

	return true;
out:
	return false;
}

static inline bool check_oom_low_mark(struct mi_dynamic_page_pool *pool, int process_id)
{
	return atomic_read(&pool->count) > pool->oom_low_mark[process_id];
}

/* Used for processes in whitelist to prevent them from getting stuck in try_to_free_pages.  */
void mi_mempool_alloc_reclaim(void *unused, gfp_t gfp_mask, int order, int alloc_flags,
	int migratetype, struct page **page)
{
	int process_id = 0;
	struct mi_dynamic_page_pool *pool = check_whitelist_and_order(order, &process_id);

	if (pool && throttle_check(gfp_mask)) {
		if (check_oom_low_mark(pool, process_id - 1))
			*page = mi_dynamic_page_pool_alloc(order, pool, gfp_mask, alloc_flags,
				migratetype, false, false);
		if (pg_mi_mempool->config & NEED_PROCESS_USE_COUNT) {
			if (*page)
				atomic_inc(&pool->process_use_counts
					[process_id - 1][PROCESS_RECLAIM_SUCCESS]);
			else
				atomic_inc(&pool->process_use_counts
					[process_id - 1][PROCESS_RECLAIM_FAILED]);
		}
	}
}

/* Used for important memory allocation failure. */
void mi_mempool_alloc_oom(void *unused, gfp_t gfp_mask, int order, int alloc_flags,
	int migratetype, struct page **page)
{
	struct mi_dynamic_page_pool *pool = NULL;

	if (!(gfp_mask & __GFP_NOWARN))
		pool = order_to_pool(order);
	if (pool) {
		if (pg_mi_mempool->config & DEBUG_LOG)
			pr_info("mi_mempool alloc_oom comm=%s pid=%d order=%d gfp_mask:%#x(%pGg)",
				current->comm, current->pid, order, gfp_mask, &gfp_mask);
		*page = mi_dynamic_page_pool_alloc(order, pool, gfp_mask, alloc_flags,
			migratetype, false, true);
		if (pg_mi_mempool->config & NEED_PROCESS_USE_COUNT) {
			int process_id = check_whitelist();

			if (process_id) {
				if (*page)
					atomic_inc(&pool->process_use_counts
						[process_id - 1][PROCESS_OOM_SUCCESS]);
				else
					atomic_inc(&pool->process_use_counts
						[process_id - 1][PROCESS_OOM_FAILED]);
			}
		}
	}
	if (!(gfp_mask & __GFP_NOWARN) && !(*page))
		atomic_inc(&pg_mi_mempool->oom_use_failure_count[order]);
}

/* Check if it is necessary to wake up the refill_ worker and free the whitelist cache. */
void  mi_mempool_refill_check(void *unused1, struct mm_struct *unused2)
{
	unsigned long flags;
	struct proc_node *node;

	if (!check_last_refill_time_interval() && pg_mi_mempool->has_lowmark_pool &&
		!READ_ONCE(pg_mi_mempool->refill_worker_running)) {
		unsigned long flags;

		spin_lock_irqsave(&pg_mi_mempool->refill_wakeup_lock, flags);
		if (!READ_ONCE(pg_mi_mempool->refill_worker_running)) {
			pg_mi_mempool->refill_check_wakeup = true;
			WRITE_ONCE(pg_mi_mempool->refill_worker_running, true);
			wake_up_process(pg_mi_mempool->refill_worker);
		}
		spin_unlock_irqrestore(&pg_mi_mempool->refill_wakeup_lock, flags);
	}
	spin_lock_irqsave(&proc_tree_lock, flags);
	node = del_proc_node_by_pid_locked(current->group_leader->pid);
	spin_unlock_irqrestore(&proc_tree_lock, flags);
	if (node)
		proc_node_free(node);
}

/* Main loop for refill_worker. */
static int mi_mempool_refill_work(void *unused)
{
	int i, tmp, refill_count;
	unsigned long flags;
	struct mi_dynamic_page_pool **pools = pg_mi_mempool->pools;
	char refill_logs[300];

	for (;;) {
		int pos = 0;

		atomic_long_set(&pg_mi_mempool->last_refill_time, ktime_to_ms(ktime_get()));
		pg_mi_mempool->last_refill_count = 0;
		pg_mi_mempool->history_refill_count++;
		pg_mi_mempool->has_lowmark_pool = false;
		for (i = 0; i < NUM_ORDERS; i++) {
			refill_count = 0;
			if (!pools[i]->first_filled ||
				mi_dynamic_page_pool_below_lowmark(pools[i])) {
				tmp = (mi_dynamic_page_pool_refill(pools[i])
					<< pools[i]->order);
				if (!pools[i]->first_filled) {
					if (mi_dynamic_page_pool_below_highmark(pools[i])) {
						pg_mi_mempool->has_lowmark_pool = true;
						pools[i]->first_fill_retries++;
						if (pools[i]->first_fill_retries >=
							MI_DYNAMIC_POOL_MAX_FIRST_REFILL_RETRIES)
							pools[i]->first_filled = true;
					} else {
						pools[i]->first_filled = true;
					}
				}			
				else if (mi_dynamic_page_pool_below_lowmark(pools[i])) {
					pg_mi_mempool->has_lowmark_pool = true;
				}

				pg_mi_mempool->last_refill_count += tmp;
				refill_count += tmp;
			}
			if ((pg_mi_mempool->config & DEBUG_LOG) && pos < 300)
				pos += scnprintf(refill_logs + pos, 300 - pos, "order-%d:%d ",
						pools[i]->order, refill_count);
		}
		if ((pg_mi_mempool->config & DEBUG_LOG)) {
			refill_logs[pos < 300 ? pos : pos - 1] = '\0';
			pr_info("mi_mempool refill cost %lu ms, refill_check_wakeup = %d, details: %s",
				ktime_to_ms(ktime_get()) -
                                atomic_long_read(&pg_mi_mempool->last_refill_time),
				pg_mi_mempool->refill_check_wakeup, refill_logs);
		}
		pg_mi_mempool->refill_check_wakeup = false;
		set_current_state(TASK_INTERRUPTIBLE);
		if (unlikely(kthread_should_stop())) {
			set_current_state(TASK_RUNNING);
			break;
		}

		spin_lock_irqsave(&pg_mi_mempool->refill_wakeup_lock, flags);
		WRITE_ONCE(pg_mi_mempool->refill_worker_running, false);
		spin_unlock_irqrestore(&pg_mi_mempool->refill_wakeup_lock, flags);

		schedule();
		set_current_state(TASK_RUNNING);
	}
	return 0;
}

static struct mi_dynamic_page_pool *mi_dynamic_page_pool_create(gfp_t gfp_mask,
	int order, int high_mark_mb)
{
	int i = 0, j = 0;
	struct mi_dynamic_page_pool *pool = (struct mi_dynamic_page_pool *)kmalloc(
		sizeof(struct mi_dynamic_page_pool), GFP_KERNEL);

	if (!pool)
		return NULL;
	pool->gfp_mask = gfp_mask;
	pool->order = order;
	pool->last_check_watermark_time = 0UL;
	INIT_LIST_HEAD(&pool->list);
	spin_lock_init(&pool->lock);
	atomic_set(&pool->count, 0);
	atomic_set(&pool->reclaim_use_count, 0);
	atomic_set(&pool->oom_use_count, 0);
	for (i = 0; i < WHITELIST_LEN; i++)
		for (j = 0; j < NR_PROCESS_COUNT_STATE; j++)
			atomic_set(&pool->process_use_counts[i][j], 0);
	pool->high_mark = (high_mark_mb * BYTES_PER_MB / PAGE_SIZE) >> order;
	pool->low_mark = pool->high_mark * MI_DYNAMIC_POOL_LOW_MARK_PERCENT / 100;
	pool->first_filled = false;
	pool->first_fill_retries = 0;
	return pool;
}

static void mi_dynamic_page_pool_destory(struct mi_dynamic_page_pool *pool)
{
	struct page *page;

	while ((page = mi_dynamic_page_pool_remove(pool)))
		__free_pages(page, pool->order);
	kfree(pool);
}

static void init_mi_dynamic_page_pools_oom_low_mark_policy(struct mi_dynamic_page_pool **pools)
{
	int i = 0, j = 0;
	struct mi_dynamic_page_pool *pool;

	for (i = 0; i < NUM_ORDERS; i++) {
		pool = pools[i];
		for (j = 0; j < WHITELIST_LEN; j++)
			pool->oom_low_mark[j] = pool->high_mark *
				MI_DYNAMIC_POOL_OOM_DEFAULT_LOW_PERCENT / 100;
	}

	for (i = 0; i < ARRAY_SIZE(oom_low_mark_policys); i++) {
		int pool_id = order_map[oom_low_mark_policys[i].order];
		int process_id = oom_low_mark_policys[i].process_id;
		int oom_low_mark_percent = oom_low_mark_policys[i].oom_low_mark_percent;
		int oom_low_mark = pools[pool_id]->high_mark * oom_low_mark_percent / 100;

		if (process_id == -1)
			for (j = 0; j < WHITELIST_LEN; j++)
				pools[pool_id]->oom_low_mark[j] = oom_low_mark;
		else
			pools[pool_id]->oom_low_mark[process_id] = oom_low_mark;
	}
}

static int mi_dynamic_page_pools_create(void)
{
	int i;

	pg_mi_mempool->pools = (struct mi_dynamic_page_pool **)kmalloc(
		sizeof(struct mi_dynamic_page_pool *) * NUM_ORDERS, GFP_KERNEL);
	if (!pg_mi_mempool->pools)
		return -1;
	for (i = 0; i < MAX_ORDER; i++)
		order_map[i] = -1;
	for (i = 0; i < NUM_ORDERS; i++) {
		pg_mi_mempool->pools[i] = mi_dynamic_page_pool_create(order_flags[i],
			orders[i], high_mark_mb[i]);
		order_map[orders[i]] = i;
		if (pg_mi_mempool->pools[i] == NULL)
			return -1;
	}
	for (i = 0; i < NUM_LIMITED_ORDER; i++) {
		int pool_id = order_map[refill_limited_order[i]];
		if (pool_id != -1)
			pg_mi_mempool->pools[pool_id]->low_mark = -1;
	}
	init_mi_dynamic_page_pools_oom_low_mark_policy(pg_mi_mempool->pools);
	return 0;
}

static void mi_dynamic_page_pools_destory(void)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++)
		mi_dynamic_page_pool_destory(pg_mi_mempool->pools[i]);
}

ssize_t process_use_count_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	int process_id = 0;
	int pool_id = 0;
	int reclaim_use_success_count, oom_use_success_count;
	int reclaim_use_fail_count, oom_use_fail_count;
	int ret = 0;
	struct mi_dynamic_page_pool *pool;

	for (; process_id < WHITELIST_LEN; process_id++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "comm:%s\n",
			comm_whitelist[process_id]);
		for (pool_id = 0; pool_id < NUM_ORDERS; pool_id++) {
			pool = pg_mi_mempool->pools[pool_id];
			reclaim_use_success_count = atomic_read(&pool->process_use_counts
				[process_id][PROCESS_RECLAIM_SUCCESS]);
			reclaim_use_fail_count = atomic_read(&pool->process_use_counts
				[process_id][PROCESS_RECLAIM_FAILED]);
			oom_use_success_count = atomic_read(&pool->process_use_counts
				[process_id][PROCESS_OOM_SUCCESS]);
			oom_use_fail_count = atomic_read(&pool->process_use_counts
				[process_id][PROCESS_OOM_FAILED]);
			ret += snprintf(buf + ret, PAGE_SIZE - ret,  "order-%d:%d %d %d %d %d\n",
				pool->order, reclaim_use_success_count, reclaim_use_fail_count,
				oom_use_success_count, oom_use_fail_count,
				pool->oom_low_mark[process_id]);
		}
	}
	return ret;
}

ssize_t direct_relcaim_threshold_mb_show(struct kobject *kobj, struct kobj_attribute *attr,
					char *buf)
{
	unsigned long direct_relcaim_threshold_mb = pg_mi_mempool->direct_relcaim_pages_threshold *
		PAGE_SIZE / BYTES_PER_MB;
	return sprintf(buf, "%lumb\n", direct_relcaim_threshold_mb);
}

ssize_t direct_relcaim_threshold_mb_store(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int val, ret;

	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return -EINVAL;
	pg_mi_mempool->direct_relcaim_pages_threshold = (val * BYTES_PER_MB) / PAGE_SIZE;

	return count;
}

ssize_t config_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", pg_mi_mempool->config);
}

ssize_t config_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val, ret;

	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return -EINVAL;
	pg_mi_mempool->config = val;
	return count;
}

ssize_t general_stat_info_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	int ret = 0, pool_id = 0, order = 0, total_oom_count = 0;

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "total_page_num: %d\n",
		atomic_read(&pg_mi_mempool->total_page_num));
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "last_refill: %lu %d %d\n",
		atomic_long_read(&pg_mi_mempool->last_refill_time),
		pg_mi_mempool->last_refill_count, pg_mi_mempool->history_refill_count);
	for (; pool_id < NUM_ORDERS; pool_id++) {
		struct mi_dynamic_page_pool *pool = pg_mi_mempool->pools[pool_id];
		int page_num = atomic_read(&pool->count);
		int reclaim_use_count = atomic_read(&pool->reclaim_use_count);
		int oom_use_count = atomic_read(&pool->oom_use_count);

		ret += snprintf(buf + ret, PAGE_SIZE - ret, "order-%d: %d %d %d %d %d\n",
			pool->order, page_num, reclaim_use_count, oom_use_count, pool->low_mark,
			pool->high_mark);
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "rb_tree: %d %d %d\n",
		atomic_read(&pg_mi_mempool->whitelist_check_num), pg_mi_mempool->proc_tree_size,
		atomic_read(&pg_mi_mempool->proc_node_alloc_failure));

	for (; order < MAX_ORDER; order++) {
		int tmp = atomic_read(&pg_mi_mempool->oom_use_failure_count[order]);
		total_oom_count += tmp;
		if (!order)
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "oom_use_failure_count: %d:%d",
				order, tmp);
		else
			ret += snprintf(buf + ret, PAGE_SIZE - ret, " %d:%d", order, tmp);
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret, " %d\n", total_oom_count);
	return ret;
}

static MI_MEMPOOL_ATTR_RO(process_use_count);
static MI_MEMPOOL_ATTR_RO(general_stat_info);
static MI_MEMPOOL_ATTR_RW(direct_relcaim_threshold_mb);
static MI_MEMPOOL_ATTR_RW(config);
static struct attribute *attrs[] = {&kobj_attr_process_use_count.attr,
				&kobj_attr_general_stat_info.attr, &kobj_attr_config.attr,
				&kobj_attr_direct_relcaim_threshold_mb.attr, NULL};

static struct kobject *mi_mempool_sysfs_create(void)
{
	int err;
	struct kobject *kobj = kobject_create_and_add("mi_mempool", kernel_kobj);

	if (!kobj) {
		pr_err("failed to create mi_mempool node.\n");
		return NULL;
	}
	err = sysfs_create_files(kobj, (const struct attribute **)attrs);
	if (err) {
		pr_err("failed to create mi_mempool attrs.\n");
		kobject_put(kobj);
		return NULL;
	}
	return kobj;
}

static void mi_mempool_sysfs_destory(void)
{
	if (pg_mi_mempool->kobj) {
		kobject_put(pg_mi_mempool->kobj);
		pg_mi_mempool->kobj = NULL;
	}
}

static inline void whitelist_init(void)
{
	int i;

	for (i = 0; comm_whitelist[i]; i++)
		comm_whitelist_sizes[i] = strlen(comm_whitelist[i]);
}

static int vendor_hook_register(void)
{
	int ret;

	ret = register_trace_android_vh_alloc_pages_reclaim_bypass(mi_mempool_alloc_reclaim,
									NULL);
	if (ret) {
		pr_err("Failed to register alloc_pages_reclaim_bypass hook.\n");
		return ret;
	}

	ret = register_trace_android_vh_alloc_pages_failure_bypass(mi_mempool_alloc_oom,
								NULL);
	if (ret) {
		unregister_trace_android_vh_alloc_pages_reclaim_bypass(mi_mempool_alloc_reclaim,
								NULL);
		pr_err("Failed to register alloc_pages_failure_bypass hook.\n");
		return ret;
	}

	ret = register_trace_android_vh_mmput(mi_mempool_refill_check,
							NULL);
	if (ret) {
		unregister_trace_android_vh_alloc_pages_reclaim_bypass(mi_mempool_alloc_reclaim,
								NULL);
		unregister_trace_android_vh_alloc_pages_failure_bypass(mi_mempool_alloc_oom,
								NULL);
		pr_err("Failed to register mmput hook.\n");
		return ret;
	}

	return 0;
}

static int vendor_hook_unregister(void)
{
	unregister_trace_android_vh_alloc_pages_reclaim_bypass(mi_mempool_alloc_reclaim,
									NULL);
	unregister_trace_android_vh_alloc_pages_failure_bypass(mi_mempool_alloc_oom,
									NULL);
	unregister_trace_android_vh_mmput(mi_mempool_refill_check,
						NULL);
	return 0;
}

int __init mi_mempool_init(void)
{
	int i = 0;

	pg_mi_mempool = &g_mi_mempool;
	pg_mi_mempool->kobj = mi_mempool_sysfs_create();
	if (!pg_mi_mempool->kobj)
		return -1;
	atomic_set(&pg_mi_mempool->total_page_num, 0);
	pg_mi_mempool->history_refill_count = 0;
	pg_mi_mempool->has_lowmark_pool = false;
	pg_mi_mempool->refill_check_wakeup = false;
	pg_mi_mempool->config = 0;
	WRITE_ONCE(pg_mi_mempool->refill_worker_running, true);
	spin_lock_init(&pg_mi_mempool->refill_wakeup_lock);
	atomic_set(&pg_mi_mempool->whitelist_check_num, 0);
	for (i = 0; i < MAX_ORDER; i++)
		atomic_set(&pg_mi_mempool->oom_use_failure_count[i], 0);

	atomic_set(&pg_mi_mempool->proc_node_alloc_failure, 0);
	pg_mi_mempool->proc_tree_size = 0;
	whitelist_init();
	pg_mi_mempool->direct_relcaim_pages_threshold = BYTES_PER_MB *
		MI_DYNAMIC_POOL_DEFAULT_DIRECT_RELCAIM_THRESHOLD_MB / PAGE_SIZE;
	if (mi_dynamic_page_pools_create())
		return -1;

	if (!proc_node_init()) {
		mi_mempool_sysfs_destory();
		mi_dynamic_page_pools_destory();
		pr_err("failed to init proc_node mem_cache.\n");
		return -1;
	}

	pg_mi_mempool->refill_worker =  kthread_run(mi_mempool_refill_work, NULL,
							"mi_mempool");
	if (IS_ERR(pg_mi_mempool->refill_worker)) {
		free_all_proc_nodes_and_caches();
		mi_mempool_sysfs_destory();
		mi_dynamic_page_pools_destory();
		pr_err("failed to start mi_mempool thread\n");
		return -1;
	}

	if (vendor_hook_register()) {
		free_all_proc_nodes_and_caches();
		mi_mempool_sysfs_destory();
		mi_dynamic_page_pools_destory();
		if (pg_mi_mempool->refill_worker)
			kthread_stop(pg_mi_mempool->refill_worker);
		return -1;
	}

	pr_info("mi_mempool init ok\n");
	return 0;
}

void __exit mi_mempool_exit(void)
{
	vendor_hook_unregister();
	free_all_proc_nodes_and_caches();
	mi_mempool_sysfs_destory();
	mi_dynamic_page_pools_destory();

	if (pg_mi_mempool->refill_worker)
		kthread_stop(pg_mi_mempool->refill_worker);
}

module_init(mi_mempool_init);
module_exit(mi_mempool_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhouwenhao");
MODULE_DESCRIPTION("A moudle to preserve some pages for some core apps"
	" or memory allocation failures.");
