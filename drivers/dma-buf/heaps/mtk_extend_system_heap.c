// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/pid.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/jiffies.h>
#include <uapi/linux/sched/types.h>
#include "mtk_extend_system_heap.h"
#include "mtk_heap.h"

#define DYNAMIC_POOL_LOW_MARK_PERCENT 40UL
#define DYNAMIC_POOL_REFILL_DEFER_WINDOW_MS 10
#define DYNAMIC_POOL_KTHREAD_NICE_VAL 10
#define FOREGROUND_APP_ADJ 0
#define NATIVE_ADJ (-1000)
#define RESERVE_POOL_ATTR(_name, _mode, _show, _store) \
	struct kobj_attribute kobj_attr_##_name \
		= __ATTR(_name, _mode, _show, _store)
/*
 * 6GB totalram unit is pages
 * 6 * 1024 * 1024 / 4 = 1572864
 */
#define HIGHER_TWELVE_MEM_TYPE (totalram > 2097152)
#define RESERVE_VMID (100)

struct reserve_extend reserve_extend_info;

enum log_out {
	HIGH_ORDER_SYSTEM_ALLOC = 1,
	RESERVE_POOL = 2,
	RESERVE_POOL_START = 4,
	ALL_CALLER = 8,
	ALL_ORDER_SYSTEM_ALLOC = 16,
	BACK_TO_RESERVE_POOL = 32,
	CLOSE_RESERVE_POOL = 64,
	POOL_LOCK = 128,
};

/*sync with served_process*/
enum pool_type {
	RESERVE_PROVIDER = 0,
	ALLOCATOR,
	COMPOSER,
	POOL_PROCESS_NUMBER,
};

enum mem_type {
	EIGHT_MEM = 0,
	TWELVE_MEM,
	MEM_TYPE_MAX,
};

enum scene_type {
	CAMERA_FG = 1,
	CAMERA_BG,
};

enum reserve_pool_config {
	ORDER_NINE,
	ORDER_FOUR,
	ORDER_ZERO,
	POOL_SERVICE_TIME,
	SLOWPATH_TIME,
	POOL_CONFIG_TYPE_MAX,
};

enum config_show {
	SOURCE_POOL_FREE_ORDER = 0,
	RESERVE_POOL_USED_ORDER,
	RESERVE_POOL_FREE_ORDER,
	RESERVE_POOL_RESERVE_ORDER,
	RESERVE_POOL_REFILL_COUNT,
	RESERVE_POOL_REFILL_AMOUNT,
	RESERVE_POOL_REFILL_LAST_AMOUNT,
	CONFIG_SHOW_MAX,
};

static char *config_show_name[CONFIG_SHOW_MAX] = {"source_pool_free_order", "reserve_pool_used_order",
						"reserve_pool_free_order", "reserve_pool_reserve_order",
						"reserve_pool_refill_count", "reserve_pool_refill_acount",
						"reserve_pool_refill_last_amount"};
static atomic_t *config_show_val[CONFIG_SHOW_MAX];
static struct dynamic_page_pool **config_show_pool[CONFIG_SHOW_MAX];
static int pool_pid[POOL_PROCESS_NUMBER];
static int pool_pid_usable_index;
static bool use_name_cmp = true;
static char *served_process[POOL_PROCESS_NUMBER] = {"camerahalserver", "hics.allocator@",
						"phics.composer@"};
/* format: order 9 4 0 service time alloc time*/
static int g_reserve_config[MEM_TYPE_MAX][POOL_CONFIG_TYPE_MAX] = {{15, 3200, 38400, 100, 1000},
								{15, 3200, 76800, 100, 1000}};
static int g_camera_reserve_config[MEM_TYPE_MAX][POOL_CONFIG_TYPE_MAX] = {{15, 3200, 51200, 100, 500},
									{15, 3200, 76800, 100, 500}};
static int g_mem_type_index = EIGHT_MEM;

static bool reserve_pool_empty(void) {
	int i;

	for (i = 0; i < NUM_ORDERS; i++) {
		if (g_reserve_pools[i] == NULL) {
			pr_err("%s: global reserve pool have NULL, order=%d\n",
				 __func__, i);
			return true;
		}
	}
	return false;
}

static inline int order_to_index(unsigned int order)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++) {
		if (orders[i] == order)
			return i;
	}
	return NUM_ORDERS - 1;
}

static int get_dynamic_pool_fillmark(struct dynamic_page_pool *pool)
{
	return atomic_read(&reserve_extend_info
		.reserve_order[order_to_index(pool->order)]);
}
static bool dynamic_pool_fillmark_reached(struct dynamic_page_pool *pool)
{
	int count;
	unsigned long flags;

	spin_lock_irqsave(&pool->lock, flags);
	count = pool->count[POOL_LOWPAGE] + pool->count[POOL_HIGHPAGE];
	spin_unlock_irqrestore(&pool->lock, flags);

	return count >= get_dynamic_pool_fillmark(pool);
}
static int get_dynamic_pool_lowmark(struct dynamic_page_pool *pool)
{
	return (atomic_read(&reserve_extend_info
			.reserve_order[order_to_index(pool->order)]) *
			DYNAMIC_POOL_LOW_MARK_PERCENT) / 100;
}
static bool dynamic_pool_count_below_lowmark(struct dynamic_page_pool *pool)
{
	int count;
	unsigned long flags;

	spin_lock_irqsave(&pool->lock, flags);
	count = pool->count[POOL_LOWPAGE] + pool->count[POOL_HIGHPAGE];
	spin_unlock_irqrestore(&pool->lock, flags);

	return count < get_dynamic_pool_lowmark(pool);
}
/* Based on gfp_zone() in mm/mmzone.c since it is not exported. */
static enum zone_type dynamic_pool_gfp_zone(gfp_t flags)
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

/*
 * Based on __zone_watermark_ok() in mm/page_alloc.c since it is not exported.
 *
 * Return true if free base pages are above 'mark'. For high-order checks it
 * will return true of the order-0 watermark is reached and there is at least
 * one free page of a suitable size. Checking now avoids taking the zone lock
 * to check in the allocation paths if no pages are free.
 */
static bool __dynamic_pool_zone_watermark_ok(struct zone *z, unsigned int order, unsigned long mark,
					     int highest_zoneidx, long free_pages)
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
static bool dynamic_pool_zone_watermark_ok_safe(struct zone *z, unsigned int order,
						unsigned long mark, int highest_zoneidx)
{
	long free_pages = zone_page_state(z, NR_FREE_PAGES);
	if (z->percpu_drift_mark && free_pages < z->percpu_drift_mark)
		free_pages = zone_page_state_snapshot(z, NR_FREE_PAGES);
	return __dynamic_pool_zone_watermark_ok(z, order, mark, highest_zoneidx, free_pages);
}

/* do a simple check to see if we are in any low memory situation */
static bool dynamic_pool_refill_ok(struct dynamic_page_pool *pool)
{
	int i, mark;
	enum zone_type classzone_idx = dynamic_pool_gfp_zone(pool->gfp_mask);
	s64 delta;

	/* check if we are within the refill defer window */
	delta = ktime_ms_delta(ktime_get(), pool->last_low_watermark_ktime);
	if (delta < DYNAMIC_POOL_REFILL_DEFER_WINDOW_MS)
		return false;
	/*
	 * make sure that if we allocate a pool->order page from buddy,
	 * we don't put the zone watermarks below the high threshold.
	 * This makes sure there's no unwanted repetitive refilling and
	 * reclaiming of buddy pages on the pool.
	 */
	for (i = classzone_idx; i >= 0; i--) {
		struct zone *zone;
		zone = &NODE_DATA(numa_node_id())->node_zones[i];
		if (!strcmp(zone->name, "DMA32"))
			continue;
		mark = high_wmark_pages(zone);
		mark += 1 << pool->order;
		if (!dynamic_pool_zone_watermark_ok_safe(zone, pool->order, mark, classzone_idx)) {
			pool->last_low_watermark_ktime = ktime_get();
			return false;
		}
	}
	return true;
}

static void dynamic_page_pool_refill(struct dynamic_page_pool *pool)
{
	gfp_t gfp_refill = (pool->gfp_mask | __GFP_RECLAIM) & ~__GFP_NORETRY;
	bool refilled = false;

	/* skip refilling order 4 and 9 pools */
	if (pool->order > 0)
		return;
	while (reserve_extend_info.use_reserve_pool &&
			!dynamic_pool_fillmark_reached(pool) && dynamic_pool_refill_ok(pool)) {
		struct page *page;
		struct reserve_extend *p_re;
		p_re = &reserve_extend_info;
		if (!refilled) {
			atomic_inc(&p_re->refill_count[order_to_index(pool->order)]);
			atomic_set(&p_re->refill_amount_last[order_to_index(pool->order)], 0);
			refilled = true;
		}
		page = alloc_pages(gfp_refill, pool->order);
		if (!page)
			break;
		atomic_inc(&p_re->refill_amount_last[order_to_index(pool->order)]);
		atomic_inc(&p_re->refill_amount[order_to_index(pool->order)]);
		dynamic_page_pool_refill_add(pool, page);
	}
}

static int reserve_pool_refill_worker(void *data)
{
	struct dynamic_page_pool **pool_list = data;
	int i;

	for (;;) {
		for (i = 0; i < NUM_ORDERS; i++) {
			if (dynamic_pool_count_below_lowmark(pool_list[i])) {
				dynamic_page_pool_refill(pool_list[i]);
				if (reserve_extend_info.debug & RESERVE_POOL)
					pr_info("%s : pools<%d>  = POOL_LOWPAGE : %d "
						"+ POOL_HIGHPAGE : %d\n",
						"reserve_pool_lowmark",pool_list[i]->order,
						pool_list[i]->count[POOL_LOWPAGE],
						pool_list[i]->count[POOL_HIGHPAGE]);
			}
		}
		set_current_state(TASK_INTERRUPTIBLE);
		if (unlikely(kthread_should_stop())) {
			set_current_state(TASK_RUNNING);
			break;
		}
		schedule();
		set_current_state(TASK_RUNNING);
	}
	return 0;
}

bool need_free_to_reserve_pool(int order_index)
{
	if (reserve_extend_info.use_reserve_pool &&
			!dynamic_pool_fillmark_reached(g_reserve_pools[order_index])) {
		if (reserve_extend_info.debug & BACK_TO_RESERVE_POOL)
			pr_info("%s order[%d %d]", __func__, order_index,
					g_reserve_pools[order_index]->order);
		return true;
	} else
		return false;
}

static void reserve_page_pool_free(struct dynamic_page_pool **reserve_pool)
{
	int i;
	unsigned long flags;

	for (i = 0; i < NUM_ORDERS; i++) {
		LIST_HEAD(pages);
		int freed = 0;
		struct page *page, *tmp = NULL;
		struct dynamic_page_pool *pool;
		pool = reserve_pool[i];
		spin_lock_irqsave(&pool->lock, flags);
		while (true) {
			if (pool->count[POOL_LOWPAGE])
				page = dynamic_page_pool_remove(pool, POOL_LOWPAGE);
			else if (pool->count[POOL_HIGHPAGE])
				page = dynamic_page_pool_remove(pool, POOL_HIGHPAGE);
			else
				break;
			list_add(&page->lru, &pages);
			freed++;
		}
		spin_unlock_irqrestore(&pool->lock, flags);
		list_for_each_entry_safe(page, tmp, &pages, lru) {
			list_del(&page->lru);
			__free_pages(page, pool->order);
		}
		if (reserve_extend_info.debug & CLOSE_RESERVE_POOL)
			pr_info("reserve pool free %d order %d", freed, pool->order);
	}
}

static void reserve_config_init(int (*p_config)[POOL_CONFIG_TYPE_MAX])
{
	int i, j;

	reserve_extend_info.use_reserve_pool_max_time =
		p_config[g_mem_type_index][POOL_SERVICE_TIME];
	reserve_extend_info.start_service_alloc_time =
		p_config[g_mem_type_index][SLOWPATH_TIME];
	for (i = NUM_ORDERS - 1, j = ORDER_ZERO; i >= 0 && j >= 0; i--, j--)
		atomic_set(&reserve_extend_info.reserve_order[i],
			p_config[g_mem_type_index][j]);
}

static ssize_t enable_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", reserve_extend_info.use_reserve_pool);
}

static ssize_t enable_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len)
{
	int value;
	int ret;

	ret = kstrtoint(buf, 0, &value);
	if (ret)
		return ret;
	if (value) {
		reserve_extend_info.use_reserve_pool = true;
		reserve_pool_enable = true;
	}
	else {
		reserve_extend_info.use_reserve_pool = false;
		reserve_pool_enable = false;
		reserve_page_pool_free(g_reserve_pools);
	}
	return len;
}

static ssize_t debug_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", reserve_extend_info.debug);
}

static ssize_t debug_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len)
{
	int value;
	int ret;

	ret = kstrtoint(buf, 0, &value);
	if (ret)
		return ret;
	reserve_extend_info.debug = value;
	return len;
}

static ssize_t config_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int i, j, len = 0;

	for (i = 0; i < CONFIG_SHOW_MAX; i++) {
		char format[10] = {"%d "};
		len += snprintf(buf + len, PAGE_SIZE - len, "%s[", config_show_name[i]);
		for (j = 0; j < NUM_ORDERS; j++) {
			int val;
			if (j == NUM_ORDERS - 1)
				strncpy(format, "%d]\n", strlen("%d]\n"));
			if (i == SOURCE_POOL_FREE_ORDER ||
					i == RESERVE_POOL_FREE_ORDER)
				val = config_show_pool[i][j]->count[POOL_LOWPAGE]
					+ config_show_pool[i][j]->count[POOL_HIGHPAGE];
			else
				val = atomic_read(&(config_show_val[i][j]));
			if (len < PAGE_SIZE - 1)
				len += snprintf(buf + len, PAGE_SIZE - len, format, val);
		}
	}
	if (len < PAGE_SIZE - 1)
		len += snprintf(buf + len, PAGE_SIZE - len,
				"reserve_slowpath[%d]\n"
				"reserve_long_slowpath[%d]\n"
				"slowpath[%d]\n"
				"pool_service_time[%d]\n"
				"pool_start_service_alloc_time[%d]\n"
				"used_reserve_pool %d\n",
				atomic_read(&reserve_extend_info.reserve_slowpath),
				atomic_read(&reserve_extend_info.reserve_long_slowpath),
				atomic_read(&reserve_extend_info.slowpath),
				reserve_extend_info.use_reserve_pool_max_time,
				reserve_extend_info.start_service_alloc_time,
				reserve_extend_info.use_reserve_pool);
	return len;
}

#define TEMP_POOL_CONFIG_TYPE_MAX 5
static ssize_t config_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len)
{
	int val[TEMP_POOL_CONFIG_TYPE_MAX];
	int i;

	if (sscanf(buf, "%d %d %d %d %d",
			&val[0],
			&val[1],
			&val[2],
			&val[3],
			&val[4]) != TEMP_POOL_CONFIG_TYPE_MAX) {
		pr_info("usage: total five args, three reserve, one used time and one alloc time");
		return -EINVAL;
	}
	for (i = 0; i < NUM_ORDERS; i++)
		atomic_set(&reserve_extend_info.reserve_order[i],
				val[i]);
	reserve_extend_info.use_reserve_pool_max_time = val[3];
	reserve_extend_info.start_service_alloc_time = val[4];
	return len;
}

static ssize_t pid_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d %d %d cmp:%d\n",
			pool_pid[COMPOSER], pool_pid[ALLOCATOR], pool_pid[RESERVE_PROVIDER],
			use_name_cmp);
}

static bool update_pool_pid(int new_pid)
{
	int i;
	int new_pid_index;
	bool pool_pid_ok[POOL_PROCESS_NUMBER];

	memset(pool_pid_ok, false, sizeof(bool) * POOL_PROCESS_NUMBER);
	for (i = 0; i < POOL_PROCESS_NUMBER; i++) {
		struct task_struct *task;
		int j, pid;
		pid = pool_pid[i];
		rcu_read_lock();
		task = find_task_by_vpid(pid);
		if (!task) {
			rcu_read_unlock();
			pr_info("no find pid COV! %d -> %d", new_pid, pool_pid[i]);
			pool_pid[i] = new_pid;
			new_pid_index = i;
			continue;
		}
		rcu_read_unlock();
		for (j = 0; j < POOL_PROCESS_NUMBER; j++) {
			if (strncmp(task->group_leader->comm, served_process[j],
					strlen(served_process[j])) == 0) {
				pool_pid_ok[i] = true;
				break;
			}
		}
		if (j == POOL_PROCESS_NUMBER) {
			pr_info("pid COV! %d -> %d", new_pid, pool_pid[i]);
			pool_pid[i] = new_pid;
			new_pid_index = i;
		}
	}
	/*check new pid*/
	for (i = 0; i < POOL_PROCESS_NUMBER; i++) {
		if (i == new_pid_index)
			continue;
		if (pool_pid_ok[i] == false)
			return false;
	}
	return true;
}

static ssize_t pid_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len)
{
	int j, pid, ret;

	ret = kstrtoint(buf, 0, &pid);
	if (ret)
		return ret;
	if (pid > 0) {
		struct task_struct *task;
		int ppid;
		rcu_read_lock();
		ppid = pid_alive(current) ?
			task_tgid_nr_ns(rcu_dereference(current->real_parent), &init_pid_ns) : 0;
		task = find_task_by_vpid(pid);
		if (!task) {
			rcu_read_unlock();
			return -ESRCH;
		}
		rcu_read_unlock();
		get_task_struct(task);
		pr_info("caller pid %d %s ppid %d", current->pid, current->comm, ppid);
		if (pool_pid_usable_index < POOL_PROCESS_NUMBER) {
			pool_pid[pool_pid_usable_index++] = pid;
			pr_info("pid %d %s %d %s %d",
					pid, task->comm,
					task->group_leader->pid, task->group_leader->comm,
					pool_pid_usable_index);
		} else {
			pr_info("pid OVF! %d %s %d",
					pid, task->group_leader->comm, pool_pid_usable_index);
			if (!update_pool_pid(pid))
				pr_info("warning pid %d %s %d %s %d",
						pid, task->comm,
						task->group_leader->pid, task->group_leader->comm,
						pool_pid_usable_index);
		}
		put_task_struct(task);
	}
	for (j = 0; j < POOL_PROCESS_NUMBER; j++) {
		if (pool_pid[j] == 0) {
			use_name_cmp = true;
			break;
		}
	}
	if (j == POOL_PROCESS_NUMBER)
		use_name_cmp = false;
	return len;
}

static ssize_t scene_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n",reserve_extend_info.scene);
}

static ssize_t scene_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len)
{
	int scene, ret;
	int (*p_config)[POOL_CONFIG_TYPE_MAX];

	ret = kstrtoint(buf, 0, &scene);
	if (ret)
		return ret;
	if (scene == CAMERA_FG) {
		p_config = g_camera_reserve_config;
	} else if (scene ==CAMERA_BG) {
		p_config = g_reserve_config;
	} else {
		pr_info("error");
		return len;
	}
	reserve_extend_info.scene = scene;
	reserve_config_init(p_config);
	return len;
}

static RESERVE_POOL_ATTR(enable, 0660, enable_show, enable_store);
static RESERVE_POOL_ATTR(debug, 0660, debug_show, debug_store);
static RESERVE_POOL_ATTR(config, 0660, config_show, config_store);
static RESERVE_POOL_ATTR(pid, 0660, pid_show, pid_store);
static RESERVE_POOL_ATTR(scene, 0660, scene_show, scene_store);
static struct attribute *reserve_pool_attrs[] = {
	&kobj_attr_enable.attr,
	&kobj_attr_debug.attr,
	&kobj_attr_config.attr,
	&kobj_attr_pid.attr,
	&kobj_attr_scene.attr,
	NULL,
};

static struct attribute_group reserve_pool_attr_group = {
	.attrs = reserve_pool_attrs,
};

static void reserve_extend_info_init(struct dynamic_page_pool **global_pools)
{
	reserve_extend_info.debug = false;
	reserve_config_init(g_reserve_config);
	config_show_pool[SOURCE_POOL_FREE_ORDER] = global_pools;
	config_show_pool[RESERVE_POOL_FREE_ORDER] = g_reserve_pools;
	config_show_val[RESERVE_POOL_USED_ORDER] = reserve_extend_info.use_order;
	config_show_val[RESERVE_POOL_RESERVE_ORDER] = reserve_extend_info.reserve_order;
	config_show_val[RESERVE_POOL_REFILL_COUNT] = reserve_extend_info.refill_count;
	config_show_val[RESERVE_POOL_REFILL_AMOUNT] = reserve_extend_info.refill_amount;
	config_show_val[RESERVE_POOL_REFILL_LAST_AMOUNT] = reserve_extend_info.refill_amount_last;
}

static void reserve_pool_sysfs_create(void)
{
	int err;
	struct kobject *kobj = kobject_create_and_add("reserve_pool", kernel_kobj);

	if (!kobj) {
		pr_err("failed to create reserve pool node.\n");
		return;
	}
	err = sysfs_create_group(kobj, &reserve_pool_attr_group);
	if (err) {
		pr_err("failed to create reserve pool attrs.\n");
		kobject_put(kobj);
	}
}

static void set_mem_type_index(void)
{
	if (HIGHER_TWELVE_MEM_TYPE)
		g_mem_type_index = TWELVE_MEM;
	else
		g_mem_type_index = EIGHT_MEM;
}


void mtk_sys_heap_reserve_pool_init(struct dynamic_page_pool **global_pools)
{
	struct task_struct *refill_worker;
	struct sched_attr attr = { .sched_nice = DYNAMIC_POOL_KTHREAD_NICE_VAL };
	int ret = -ENOMEM;
	int i;
	set_mem_type_index();

	for (i = 0; i < NUM_ORDERS; i++) {
		g_reserve_pools[i] = dynamic_page_pool_create(order_flags[i], orders[i], true);
		if (IS_ERR_OR_NULL(g_reserve_pools[i])) {
			int j;
			pr_err("%s: page pool creation failed for the order %u pool!\n",
				__func__, orders[i]);
			for (j = 0; j < i; j++)
				dynamic_page_pool_destroy(g_reserve_pools[j]);
		}
	}

	reserve_extend_info_init(global_pools);
	reserve_extend_info.use_reserve_pool = true;
	reserve_pool_enable = true;
	reserve_pool_sysfs_create();

	if (IS_ENABLED(CONFIG_MTK_DMABUF_HEAPS_PAGE_POOL)) {
		refill_worker = kthread_run(reserve_pool_refill_worker,
				g_reserve_pools, "reserve-refill");
		if (IS_ERR(refill_worker)) {
			pr_err("%s: failed to create reserve-refill: %ld\n",
			       __func__, PTR_ERR(refill_worker));
		} else {
			ret = sched_setattr(refill_worker, &attr);
			if (ret) {
				pr_warn("%s: failed to set task priority for "
					"ret = %d\n",
					__func__, ret);
				kthread_stop(refill_worker);
			} else {
				int i;
				for (i = 0; i < NUM_ORDERS; i++)
					g_reserve_pools[i]->refill_worker = refill_worker;
			}
		}
	}
}

static is_server_app(struct dynamic_page_pool **reserve_pools)
{
	get_task_struct(current);
	if (reserve_pools != NULL && current->signal->oom_score_adj == NATIVE_ADJ) {
		int j;
		/* for performance, use pid comparison
		 * support by patch 2-4/4
		 */
		if (use_name_cmp) {
			for (j = 0; j < POOL_PROCESS_NUMBER; j++) {
				if (strncmp(current->group_leader->comm, served_process[j],
						strlen(served_process[j])) == 0) {
					put_task_struct(current);
					return true;
				}
			}
		} else {
			for (j = 0; j < POOL_PROCESS_NUMBER; j++) {
				if (current->group_leader->pid == pool_pid[j]) {
					put_task_struct(current);
					return true;
				}
			}
		}
	}
	put_task_struct(current);
	return false;
}

static inline void get_pool_count(struct dynamic_page_pool *spool,
		struct dynamic_page_pool *reserve_pool, int *spool_high_count,
		int *spool_low_count, int *reserve_pool_high_count,
		int *reserve_pool_low_count)
{
	unsigned long flags;
	spin_lock_irqsave(&spool->lock, flags);
	*spool_high_count = spool->count[POOL_HIGHPAGE];
	*spool_low_count = spool->count[POOL_LOWPAGE];
	spin_unlock_irqrestore(&spool->lock, flags);
	spin_lock_irqsave(&reserve_pool->lock, flags);
	*reserve_pool_high_count = reserve_pool->count[POOL_HIGHPAGE];
	*reserve_pool_low_count = reserve_pool->count[POOL_LOWPAGE];
	spin_unlock_irqrestore(&reserve_pool->lock, flags);
}

static inline void pr_info_normal(char *tag, struct page *page, bool served_app,
		struct dynamic_page_pool *spool, struct dynamic_page_pool *reserve_pool)
{
	int spool_high_count;
	int spool_low_count;
	int reserve_pool_high_count;
	int reserve_pool_low_count;

	get_pool_count(spool, reserve_pool, &spool_high_count, &spool_low_count,
			&reserve_pool_high_count, &reserve_pool_low_count);
	pr_info("%s: page %p %s["
		"%s %d] order %d "
		"spool[%d %d] reserve_pool[%d %d] "
		"served_app %d",
		tag, page, current->group_leader->comm,
		current->comm, current->pid, spool->order,
		spool_high_count, spool_low_count,
		reserve_pool_high_count, reserve_pool_low_count,
		served_app);
}

static inline void pr_info_with_alloc_info(char *tag, struct page *page, bool served_app,
		struct dynamic_page_pool *spool, struct dynamic_page_pool *reserve_pool,
		s64 delta, gfp_t *gfp_mask)
{
	int spool_high_count;
	int spool_low_count;
	int reserve_pool_high_count;
	int reserve_pool_low_count;

	get_pool_count(spool, reserve_pool, &spool_high_count, &spool_low_count,
			&reserve_pool_high_count, &reserve_pool_low_count);
	pr_info("%s: page %p %s[%s %d] order %d "
		"spool[%d %d] reserve_pool[%d %d] served_app %d "
		"time %ldus mode:%#x(%pGg)",
		tag, page, current->group_leader->comm, current->comm,
		current->pid, spool->order,
		spool_high_count, spool_low_count,
		reserve_pool_high_count, reserve_pool_low_count,
		served_app, delta,
		*gfp_mask, gfp_mask);
}

static inline struct page *get_page_from_pool(struct dynamic_page_pool *pool)
{
	struct page *page = NULL;
	unsigned long flags;

	spin_lock_irqsave(&pool->lock, flags);
	if (pool->count[POOL_HIGHPAGE])
		page = dynamic_page_pool_remove(pool, POOL_HIGHPAGE);
	else if (pool->count[POOL_LOWPAGE])
		page = dynamic_page_pool_remove(pool, POOL_LOWPAGE);
	spin_unlock_irqrestore(&pool->lock, flags);
	return page;
}

static inline struct page *get_page_from_reserve_pool(bool served_app,
			struct dynamic_page_pool *spool, struct dynamic_page_pool *reserve_pool)
{
	struct page *page = NULL;
	bool high_order = reserve_pool->order > 0;

	if (!reserve_pool_empty() && served_app) {
		u64 start = 0;
		if (!high_order)
			start = atomic64_read(&reserve_extend_info.use_reserve_pool_start);
		if (high_order || start != 0) {
			u64 delta = 0;
			if (!high_order)
				delta = ktime_to_ms(ktime_get()) - start;
			if (delta < reserve_extend_info.use_reserve_pool_max_time) {
				page = get_page_from_pool(reserve_pool);
				if (reserve_extend_info.debug & RESERVE_POOL)
					pr_info_normal("reserve pool", page,
							served_app, spool, reserve_pool);
				if (page != NULL)
					atomic_inc(&reserve_extend_info.use_order[order_to_index(reserve_pool->order)]);
			} else {
				atomic64_set(&reserve_extend_info.use_reserve_pool_start, 0);
				if (reserve_extend_info.debug & RESERVE_POOL_START)
					pr_info_normal("used reserve pool timeout",
							page, served_app, spool, reserve_pool);
			}
		}
	}
	return page;
}

static inline struct page *get_high_order_page_from_buddy( bool served_app,
			struct dynamic_page_pool *spool, struct dynamic_page_pool *reserve_pool)
{
	struct page *page = NULL;

	if (spool->order > 0) {
		page = alloc_pages(spool->gfp_mask, spool->order);
		if (reserve_extend_info.debug & HIGH_ORDER_SYSTEM_ALLOC)
			pr_info_normal("high order alloc", page, served_app,
					spool, reserve_pool);
	}
	return page;
}

static inline struct page *get_page_from_buddy(bool served_app, struct dynamic_page_pool *spool,
						struct dynamic_page_pool *reserve_pool)
{
	s64 delta;
	ktime_t start_ktime = ktime_get();
	struct page *page = alloc_pages(spool->gfp_mask, spool->order);

	delta = ktime_us_delta(ktime_get(), start_ktime);
	if (served_app &&
			delta > reserve_extend_info.start_service_alloc_time) {
		atomic64_set(&reserve_extend_info.use_reserve_pool_start, ktime_to_ms(ktime_get()));
		atomic_inc(&reserve_extend_info.reserve_long_slowpath);
		if (reserve_extend_info.debug & RESERVE_POOL_START)
			pr_info_with_alloc_info("reserve-sp", page, served_app,
					spool, reserve_pool, delta, &spool->gfp_mask);
	}
	if (reserve_extend_info.debug & ALL_ORDER_SYSTEM_ALLOC) {
		pr_info_with_alloc_info("All alloc", page, served_app,
				spool, reserve_pool, delta, &spool->gfp_mask);
	}
	if (spool->order == 0) {
		if (served_app)
			atomic_inc(&reserve_extend_info.reserve_slowpath);
		else
			atomic_inc(&reserve_extend_info.slowpath);
	}
	return page;
}

static inline void wakeup_refill_thread_by_wmark(bool served_app, struct dynamic_page_pool *spool,
		struct dynamic_page_pool *reserve_pool)
{
	if (IS_ENABLED(CONFIG_MTK_DMABUF_HEAPS_PAGE_POOL) &&
			spool->order &&
			dynamic_pool_count_below_lowmark(spool)) {
		if (reserve_extend_info.debug & RESERVE_POOL)
			pr_info("system_pool_lowmark: current_proc-order<%d, %d> wakeup"
				"system_pool-2\n", current->pid, spool->order);
		wake_up_process(spool->refill_worker);
	}
	if (served_app && reserve_pool != NULL && reserve_pool->refill_worker != NULL) {
		if (IS_ENABLED(CONFIG_MTK_DMABUF_HEAPS_PAGE_POOL) &&
				reserve_pool->order &&
				dynamic_pool_count_below_lowmark(reserve_pool)) {
			if (reserve_extend_info.debug & RESERVE_POOL)
				pr_info("reserve_pool_lowmark: current_proc-order<%d, order> wakeup"
					"reserve_pool-1\n", current->pid, reserve_pool->order);
			wake_up_process(reserve_pool->refill_worker);
		}
	}
}

struct page *mtk_extend_sys_heap_alloc_largest_available(struct dynamic_page_pool **global_pools,
						unsigned long size, unsigned int max_order)
{
	struct page *page = NULL;
	struct dynamic_page_pool **g_pools = global_pools;
	struct dynamic_page_pool **reserve_pools = g_reserve_pools;
	bool served_app = is_server_app(reserve_pools);
	int i;

	for (i = 0; i < NUM_ORDERS; i++) {
		if (size <  (PAGE_SIZE << orders[i]))
			continue;
		if (max_order < orders[i])
			continue;
		page = get_page_from_pool(g_pools[i]);
		if (!page)
			page = get_high_order_page_from_buddy(served_app,
					g_pools[i], reserve_pools[i]);
		if (!page)
			page = get_page_from_reserve_pool(served_app,
					g_pools[i], reserve_pools[i]);
		if (reserve_extend_info.debug & ALL_CALLER)
			pr_info_normal("All", page, served_app,
					g_pools[i], reserve_pools[i]);
		if (!page) {
			page = get_page_from_buddy(served_app,
					g_pools[i], reserve_pools[i]);
		}
		if (!page)
			continue;
		 wakeup_refill_thread_by_wmark(served_app, g_pools[i], reserve_pools[i]);
		return page;
	}
	return NULL;
}

