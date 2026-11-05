// SPDX-License-Identifier: GPL-2.0
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/pid.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/workqueue.h>
#include <trace/hooks/mm.h>

#include "qcom_dynamic_page_pool.h"
#include "qcom_system_heap.h"
#include "camera_dynamic_reserve_pool.h"

#define CAMERA_RESERVE_POOL_ATTR(_name, _mode, _show, _store) \
	struct kobj_attribute camera_kobj_attr_##_name \
		= __ATTR(_name, _mode, _show, _store)

#define CAM_DYNAMIC_RESERVE_VMID (101)
#define MAX_EXTRA_PAGES (76800)
#define FREE_MEM_MIN_PAGES (0)
#define DYNAMIC_POOL_LIMIT (25600)
#define BYPASS_RATIO_MAX (10)
#define WMARK_PAGES_MAX (25600) /* 100MB */
#define REFILL_CACHE_INTERVAL_MS (100)

enum reserve_pool_config {
	ORDER_NINE,
	ORDER_FOUR,
	ORDER_ZERO,
};

enum log_out {
	RESERVE_POOL = 1,
	REFILL_POOL = 2
};

enum scene_type {
	CAMERA_FG = 1,
	CAMERA_BG,
};

enum free_mem_level {
	FREE_TIGHT = 0,
	FREE_OK = 1,
	FREE_PLENTY = 2,
	FREE_LEVEL_MAX = 3,
};

enum pool_level {
	POOL_LOW = 0,
	POOL_HIGH = 1,
	POOL_LEVEL_MAX = 2,
};

/*
 * Combined ratio matrix: ratio_matrix[free_level][pool_level]
 * Lower ratio = more aggressive fill (more pages diverted to pool).
 *                  POOL_LOW  POOL_HIGH
 *  FREE_TIGHT  :      4         5
 *  FREE_OK     :      3         4
 *  FREE_PLENTY :      2         3
 */
static const int ratio_matrix[FREE_LEVEL_MAX][POOL_LEVEL_MAX] = {
	/* TIGHT  */ { 4, 5 },
	/* OK     */ { 3, 4 },
	/* PLENTY */ { 2, 3 },
};

#define FREE_PLENTY_THRESHOLD (200 * 1024 * 1024 >> PAGE_SHIFT)
#define FREE_OK_THRESHOLD     (120 * 1024 * 1024 >> PAGE_SHIFT)
#define FREE_TIGHT_THRESHOLD  ( 60 * 1024 * 1024 >> PAGE_SHIFT)

static struct qcom_system_heap *g_sys_heap;

static atomic_t g_camera_reserved_pages = ATOMIC_INIT(0); //min reserve pages for camera
static atomic_t g_camera_provider_pid = ATOMIC_INIT(-1); // camera provider pid
static atomic_t g_allocator_pid = ATOMIC_INIT(-1);  // allocator-servi pid
static atomic_t g_extra_pages = ATOMIC_INIT(0);
static atomic_t g_reserve_refill_paused = ATOMIC_INIT(0);
static atomic_t g_scene = ATOMIC_INIT(CAMERA_BG);
static atomic_t g_debug = ATOMIC_INIT(0);

static atomic64_t g_use_order[NUM_ORDERS];
static atomic64_t g_refill_amount[NUM_ORDERS];
static atomic_t g_free_filio_count = ATOMIC_INIT(0); /* refill counter for bypass ratio */
static atomic_t g_bypass_ratio = ATOMIC_INIT(2); /* fill 1 page per N reclaimed (default: 2 = current skip-1 behavior) */
static atomic_t g_bypass_ratio_user_set = ATOMIC_INIT(0); /* 1 = user has set bypass_ratio via sysfs */
static atomic_t g_wmark_pages = ATOMIC_INIT(FREE_MEM_MIN_PAGES); /* watermark threshold in pages */

/* Cached values for hot-path decisions, updated every REFILL_CACHE_INTERVAL_MS. */
static atomic_t g_cached_ratio = ATOMIC_INIT(2);
static struct delayed_work g_refill_cache_work;

extern void prep_new_page(struct page *page, unsigned int order, gfp_t gfp_flags, unsigned int alloc_flags);

static inline int order_to_index(unsigned int order)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++) {
		if (orders[i] == order)
			return i;
	}

	WARN_ON_ONCE(1);
	return NUM_ORDERS - 1;
}

/*
 * Return true if the current process is allowed to access the camera
 * reserve pool: either it is the camera provider, or it is the allocator
 * service running in the CAMERA_FG scene.
 */
static bool is_camera_process(void)
{
	int pid = task_tgid_vnr(current);
	int provider = atomic_read(&g_camera_provider_pid);
	int allocator = atomic_read(&g_allocator_pid);

	if (provider > 0 && pid == provider)
		return true;
	if (allocator > 0 && atomic_read(&g_scene) == CAMERA_FG && pid == allocator)
		return true;
	return false;
}

inline bool is_cam_dynamic_reserve_vmid(int vmid)
{
	return vmid == CAM_DYNAMIC_RESERVE_VMID;
}

static void reserve_page_pool_free(struct dynamic_page_pool **reserve_pool, int count)
{
	int i;
	unsigned long flags;

	for (i = 0; i < NUM_ORDERS; i++) {
		LIST_HEAD(pages);
		int freed = 0;
		int to_free_count = 0;
		int ret = DYNAMIC_POOL_SUCCESS;
		struct page *page, *tmp;
		struct dynamic_page_pool *pool;

		/*
		 * When freeing a specific count, only free order-0 pages.
		 * When count == INT_MAX, free all orders (full drain).
		 */
		if (i == ORDER_ZERO)
			to_free_count = count;
		else if (count == INT_MAX)
			to_free_count = INT_MAX;
		else
			continue;

		pool = reserve_pool[i];
		spin_lock_irqsave(&pool->lock, flags);
		while (freed < to_free_count) {
			if (pool->low_count)
				page = dynamic_page_pool_remove(pool, false);
			else if (pool->high_count)
				page = dynamic_page_pool_remove(pool, true);
			else
				break;

			list_add(&page->lru, &pages);
			freed++;
		}
		spin_unlock_irqrestore(&pool->lock, flags);

		if (freed && pool->prerelease_callback)
			ret = pool->prerelease_callback(pool, &pages, freed);

		if (ret != DYNAMIC_POOL_SUCCESS)
			pr_err("Failed to reclaim reserve page pool pages!\n");

		list_for_each_entry_safe(page, tmp, &pages, lru) {
			list_del(&page->lru);
			__free_pages(page, pool->order);
		}
		if (atomic_read(&g_debug) & RESERVE_POOL)
			pr_info("reserve pool free %d order %d\n", freed, pool->order);
	}
}

/*
 * Return sum of min_wmark_pages across all managed zones.
 * Used for wmark validation and free memory level classification.
 */
static unsigned long get_min_wmark_pages(void)
{
	pg_data_t *pgdat = NODE_DATA(numa_node_id());
	unsigned long min_pages = 0;
	int i;

	for (i = 0; i <= ZONE_NORMAL; i++) {
		struct zone *zone = &pgdat->node_zones[i];
		if (managed_zone(zone))
			min_pages += min_wmark_pages(zone);
	}
	return min_pages;
}

/*
 * Return available free pages: NR_FREE_PAGES - CMA - highatomic.
 */
static unsigned long get_avail_free_pages(void)
{
	pg_data_t *pgdat = NODE_DATA(numa_node_id());
	unsigned long free = 0, cma = 0, highatomic = 0;
	int i;

	for (i = 0; i <= ZONE_NORMAL; i++) {
		struct zone *zone = &pgdat->node_zones[i];
		if (!managed_zone(zone))
			continue;
		free += zone_page_state(zone, NR_FREE_PAGES);
		cma += zone_page_state(zone, NR_FREE_CMA_PAGES);
		highatomic += zone->nr_reserved_highatomic;
	}
	/* 饱和减法：free 不足 cma+highatomic 时返回 0，避免无符号下溢 */
	return free > (cma + highatomic) ? free - cma - highatomic : 0;
}

/*
 * Classify current free memory into 3 tiers for bypass ratio calculation.
 * Returns -1 when free memory is critically low (≤ FREE_TIGHT_THRESHOLD + wmark),
 * meaning the caller should block filling entirely.
 */
static int get_free_mem_level(void)
{
	unsigned long avail = get_avail_free_pages();
	unsigned long wmark = (unsigned long)atomic_read(&g_wmark_pages);

	if (avail <= FREE_TIGHT_THRESHOLD + wmark)
		return -1;
	if (avail <= FREE_OK_THRESHOLD + wmark)
		return FREE_TIGHT;
	if (avail <= FREE_PLENTY_THRESHOLD + wmark)
		return FREE_OK;
	return FREE_PLENTY;
}

static ssize_t debug_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%u\n", atomic_read(&g_debug));
}

static ssize_t debug_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len)
{
	int value;
	int ret;

	ret = kstrtoint(buf, 0, &value);
	if (ret)
		return ret;

	atomic_set(&g_debug, value);
	pr_info("%s set debug %d\n", __func__, value);
	return len;
}

static ssize_t config_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int i, len = 0;
	s64 val;

	if (g_sys_heap && g_sys_heap->cam_dynamic_reserve_pool_list) {
		len += sysfs_emit_at(buf, len, "camera_free_order[");
		for (i = 0; i < NUM_ORDERS; i++) {
			struct dynamic_page_pool *pool =
				g_sys_heap->cam_dynamic_reserve_pool_list[i];
			if (pool)
				len += sysfs_emit_at(buf, len, "%s%d",
						     i ? " " : "",
						     atomic_read(&pool->count));
		}
		len += sysfs_emit_at(buf, len, "]\n");
	}

	len += sysfs_emit_at(buf, len, "camera_use_order[");
	for (i = 0; i < NUM_ORDERS; i++) {
		val = atomic64_read(&g_use_order[i]);
		len += sysfs_emit_at(buf, len, "%s%lld", i ? " " : "", val);
	}
	len += sysfs_emit_at(buf, len, "]\n");

	len += sysfs_emit_at(buf, len, "camera_refill_amount[");
	for (i = 0; i < NUM_ORDERS; i++) {
		val = atomic64_read(&g_refill_amount[i]);
		len += sysfs_emit_at(buf, len, "%s%lld", i ? " " : "", val);
	}
	len += sysfs_emit_at(buf, len, "]\n");

	len += sysfs_emit_at(buf, len, "bypass_ratio[%d]\n",
			     atomic_read(&g_cached_ratio));

	return len;
}

static ssize_t scene_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%d\n", atomic_read(&g_scene));
}

static void update_refill_cache(void);

static ssize_t scene_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len)
{
	int scene, ret;

	ret = kstrtoint(buf, 0, &scene);
	if (ret)
		return ret;
	if (scene < 0 || scene > CAMERA_BG) {
		pr_err("%s invalid scene %d\n", __func__, scene);
		return -EINVAL;
	}
	atomic_set(&g_scene, scene);

	/* Start cache refresh timer when camera is active, stop otherwise. */
	if (scene == CAMERA_FG) {
		update_refill_cache();
		schedule_delayed_work(&g_refill_cache_work,
				msecs_to_jiffies(REFILL_CACHE_INTERVAL_MS));
	} else {
		cancel_delayed_work_sync(&g_refill_cache_work);
	}

	return len;
}

static ssize_t cam_reserved_pages_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%d\n", atomic_read(&g_camera_reserved_pages));
}

static ssize_t cam_reserved_pages_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len)
{
	int pages, ret;

	ret = kstrtoint(buf, 0, &pages);
	if (ret) {
		pr_err("%s read camera_reserved_pages error\n", __func__);
		return ret;
	}

	atomic_set(&g_camera_reserved_pages, pages);
	pr_info("%s set cam reserved pages %d\n", __func__, pages);
	return len;
}

static ssize_t extra_pages_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%d\n", atomic_read(&g_extra_pages));
}

static ssize_t extra_pages_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len)
{
	struct dynamic_page_pool *extra_pool_0;
	int new_pages, ret;
	int old_pages = atomic_read(&g_extra_pages);

	ret = kstrtoint(buf, 0, &new_pages);
	if (ret)
		return ret;

	if (new_pages < 0 || new_pages > MAX_EXTRA_PAGES) {
		pr_err("%s invalid extra_pages: %d (valid: 0-%d)\n",
			__func__, new_pages, MAX_EXTRA_PAGES);
		return -EINVAL;
	}

	if (new_pages == old_pages)
		return len;

	/* Validate pool and scene before touching any state. */
	if (!g_sys_heap || !g_sys_heap->cam_dynamic_reserve_pool_list)
		return -EINVAL;

	extra_pool_0 = g_sys_heap->cam_dynamic_reserve_pool_list[ORDER_ZERO];
	if (!extra_pool_0)
		return -EINVAL;

	/*
	 * Pause → update → drain → reconfig → resume.  The pause must
	 * come before g_extra_pages so that a concurrent free_folio_bypass
	 * either sees the old value (and the subsequent drain handles it)
	 * or sees the pause flag and backs off.
	 */
	atomic_set(&g_reserve_refill_paused, 1);
	smp_mb();

	atomic_set(&g_extra_pages, new_pages);
	if (old_pages > new_pages)
		reserve_page_pool_free(g_sys_heap->cam_dynamic_reserve_pool_list,
				       old_pages - new_pages);

	//TODO: initial cam_extra_pages_reserve_pool?
	pr_info("%s total_cam_pages %d, cam_extra_pages_pool_count %d\n",
		__func__, new_pages,
		atomic_read(&extra_pool_0->count));

	atomic_set(&g_reserve_refill_paused, 0);
	smp_mb();

	return len;
}

static ssize_t bypass_ratio_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%d (%s)\n",
			 atomic_read(&g_bypass_ratio),
			 atomic_read(&g_bypass_ratio_user_set) ? "manual" : "auto");
}

static ssize_t bypass_ratio_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len)
{
	int ratio, ret;

	ret = kstrtoint(buf, 0, &ratio);
	if (ret)
		return ret;

	/* 0 = revert to dynamic auto mode */
	if (ratio == 0) {
		atomic_set(&g_bypass_ratio_user_set, 0);
		atomic_set(&g_free_filio_count, 0);
		pr_info("%s bypass_ratio reverted to auto (dynamic)\n", __func__);
		return len;
	}

	if (ratio > BYPASS_RATIO_MAX) {
		pr_err("%s invalid bypass_ratio %d, must be 0~%d\n",
			__func__, ratio, BYPASS_RATIO_MAX);
		return -EINVAL;
	}

	atomic_set(&g_bypass_ratio, ratio);
	atomic_set(&g_bypass_ratio_user_set, 1);
	atomic_set(&g_free_filio_count, 0);
	pr_info("%s set bypass_ratio %d (manual override)\n", __func__, ratio);
	return len;
}

static ssize_t wmark_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%d\n", atomic_read(&g_wmark_pages));
}

static ssize_t wmark_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len)
{
	int pages, ret;
	unsigned long min_pages;

	ret = kstrtoint(buf, 0, &pages);
	if (ret)
		return ret;

	if (pages < -WMARK_PAGES_MAX || pages > WMARK_PAGES_MAX) {
		pr_err("%s invalid wmark pages %d, must be %d~%d\n",
			__func__, pages, -WMARK_PAGES_MAX, WMARK_PAGES_MAX);
		return -EINVAL;
	}

	min_pages = get_min_wmark_pages();
	if ((unsigned long)(pages + FREE_TIGHT_THRESHOLD) < min_pages) {
		pr_err("%s wmark %d too low, tight threshold %lu below min_pages %lu\n",
			__func__, pages,
			(unsigned long)(pages + FREE_TIGHT_THRESHOLD), min_pages);
		return -EINVAL;
	}

	atomic_set(&g_wmark_pages, pages);
	pr_info("%s set wmark_pages %d\n", __func__, pages);
	return len;
}

static ssize_t allocator_pid_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%d\n", atomic_read(&g_allocator_pid));
}

static ssize_t allocator_pid_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len)
{
	int pid, ret;

	ret = kstrtoint(buf, 0, &pid);
	if (ret)
		return ret;

	if (pid <= 0) {
		pr_err("%s invalid allocator pid %d\n", __func__, pid);
		return -EINVAL;
	}

	atomic_set(&g_allocator_pid, pid);

	return len;
}

static ssize_t cam_provider_pid_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%d\n", atomic_read(&g_camera_provider_pid));
}

static ssize_t cam_provider_pid_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len)
{
	int pid, ret;

	ret = kstrtoint(buf, 0, &pid);
	if (ret)
		return ret;

	if (pid <= 0) {
		pr_err("%s invalid camera provider pid %d\n", __func__, pid);
		return -EINVAL;
	}

	atomic_set(&g_camera_provider_pid, pid);

	return len;
}

/*
 * Adjust fill aggressiveness based on both pool water level and system
 * free memory.  Returns a ratio from ratio_matrix[free_level][pool_level],
 * or -1 when free memory is critically low (hard block, stop fill).
 *
 * User-set ratio (via sysfs) always takes priority.
 */
static int calculate_ratio(void)
{
	struct dynamic_page_pool *pool_0, *reserve_0;
	long count = 0, target = 0;
	int pool_lv, free_lv;
	int ratio;

	/* User-set value takes priority over dynamic calculation. */
	if (atomic_read(&g_bypass_ratio_user_set))
		return atomic_read(&g_bypass_ratio);

	if (!g_sys_heap)
		return atomic_read(&g_bypass_ratio);

	/* --- pool dimension: accumulate count/target from main + reserve pools --- */
	pool_0 = g_sys_heap->pool_list ? g_sys_heap->pool_list[ORDER_ZERO] : NULL;
	if (pool_0) {
		count += atomic_read(&pool_0->count);
		target += DYNAMIC_POOL_LIMIT;
	}

	reserve_0 = g_sys_heap->cam_dynamic_reserve_pool_list ?
		    g_sys_heap->cam_dynamic_reserve_pool_list[ORDER_ZERO] : NULL;
	if (reserve_0) {
		count += atomic_read(&reserve_0->count);
		target += atomic_read(&g_extra_pages);
	}

	if (target <= 0)
		return atomic_read(&g_bypass_ratio);

	pool_lv = (count >= target / 2) ? POOL_HIGH : POOL_LOW;

	/* --- free memory dimension --- */
	free_lv = get_free_mem_level();
	ratio = (free_lv < 0) ? -1 : ratio_matrix[free_lv][pool_lv];

	if (atomic_read(&g_debug) & REFILL_POOL)
		pr_info("calculate_ratio: count %ld, target %ld, pool_lv %d, free_lv %d, ratio %d\n",
			count, target, pool_lv, free_lv, ratio);

	return ratio;
}

/*
 * Refresh cached ratio.  Called from a periodic delayed_work so that the
 * heavy zone iteration in calculate_ratio() never runs in the vendor-hook
 * / page-free hot path.  ratio < 0 means free memory is critically low.
 */
static void update_refill_cache(void)
{
	atomic_set(&g_cached_ratio, calculate_ratio());
}

static void refill_cache_timer_fn(struct work_struct *work)
{
	update_refill_cache();

	/* Self-stop when camera is no longer in foreground. */
	if (atomic_read(&g_scene) != CAMERA_FG)
		return;

	schedule_delayed_work(&g_refill_cache_work,
		      msecs_to_jiffies(REFILL_CACHE_INTERVAL_MS));
}

static void free_unref_page_bypass(void *unused, struct page *page, int order,
				   int migratetype, bool *bypass)
{
	struct dynamic_page_pool *pool;
	int free_count, target;
	int idx, ratio;
	s64 refilled;

	/* Only intercept order-0 frees. */
	if (*bypass)
		return;

	*bypass = false;
	if (order != 0)
		return;

	/* Feature gate: extra_pages must be configured and refill not paused. */
	if (atomic_read(&g_extra_pages) <= 0)
		return;
	/*
	 * Order the g_extra_pages read above against the g_reserve_refill_paused
	 * read below.  Pairs with the smp_mb() in extra_pages_store so that if
	 * we observe !paused here we are guaranteed to see the g_extra_pages
	 * value that was written before the pause was cleared.
	 */
	smp_rmb();
	if (atomic_read(&g_reserve_refill_paused) == 1)
		return;

	/* Reserve pool must be initialized and enabled. */
	if (!g_sys_heap ||
	    !g_sys_heap->cam_dynamic_reserve_pool_list)
		return;

	/* Pool must exist and not already at fill mark. */
	idx = order_to_index(order);
	pool = g_sys_heap->cam_dynamic_reserve_pool_list[idx];
	if (!pool)
		return;

	free_count = atomic_read(&pool->count);
	target = atomic_read(&g_extra_pages);

	if (free_count >= target)
		return;

	/*
	 * 只接收 ZONE_NORMAL 中迁移类型为 UNMOVABLE/MOVABLE 的页，
	 * 跳过 CMA、HIGHMEM 及其他特殊 zone/迁移类型，避免抢占可能
	 * 造成碎片化或属于保留区域的页。
	 */
	if (migratetype > MIGRATE_MOVABLE || zone_idx(page_zone(page)) != ZONE_NORMAL)
		return;

	/*
	 * Bypass ratio: only add 1 page per g_bypass_ratio reclaimed.
	 * E.g. ratio=3 means reclaim 3, fill 1 (skip 2).
	 * ratio < 0 means free memory is critically low, stop fill entirely.
	 */
	ratio = atomic_read(&g_cached_ratio);
	if (ratio < 0)
		return;
	if (atomic_inc_return(&g_free_filio_count) % ratio != 0)
		return;

	prep_new_page(page, order, pool->gfp_mask & (~__GFP_DIRECT_RECLAIM), 0);
	set_page_count(page, 1);
	dynamic_page_pool_add(pool, page);
	*bypass = true;
	refilled = atomic64_inc_return(&g_refill_amount[idx]);

	if ((refilled & 2047) == 0)
		pr_info("%s: camera_reserve_pool filled %lld, order %d, pool_count %d, free_count %d, target %d\n",
			__func__, refilled, order, atomic_read(&pool->count), free_count, target);
}

static CAMERA_RESERVE_POOL_ATTR(config, 0660, config_show, NULL);
static CAMERA_RESERVE_POOL_ATTR(debug, 0660, debug_show, debug_store);
static CAMERA_RESERVE_POOL_ATTR(scene, 0660, scene_show, scene_store);
static CAMERA_RESERVE_POOL_ATTR(cam_reserved_pages, 0660, cam_reserved_pages_show, cam_reserved_pages_store);
static CAMERA_RESERVE_POOL_ATTR(extra_pages, 0660, extra_pages_show, extra_pages_store);
static CAMERA_RESERVE_POOL_ATTR(allocator_pid, 0660, allocator_pid_show, allocator_pid_store);
static CAMERA_RESERVE_POOL_ATTR(cam_provider_pid, 0660, cam_provider_pid_show, cam_provider_pid_store);
static CAMERA_RESERVE_POOL_ATTR(bypass_ratio, 0660, bypass_ratio_show, bypass_ratio_store);
static CAMERA_RESERVE_POOL_ATTR(wmark, 0660, wmark_show, wmark_store);

static struct attribute *camera_reserve_pool_attrs[] = {
	&camera_kobj_attr_config.attr,
	&camera_kobj_attr_debug.attr,
	&camera_kobj_attr_scene.attr,
	&camera_kobj_attr_cam_reserved_pages.attr,
	&camera_kobj_attr_extra_pages.attr,
	&camera_kobj_attr_allocator_pid.attr,
	&camera_kobj_attr_cam_provider_pid.attr,
	&camera_kobj_attr_bypass_ratio.attr,
	&camera_kobj_attr_wmark.attr,
	NULL,
};

static struct attribute_group camera_reserve_pool_attr_group = {
	.attrs = camera_reserve_pool_attrs,
};

static struct kobject *camera_reserve_pool_kobj;

static void camera_reserve_pool_sysfs_create(void)
{
	int err;

	camera_reserve_pool_kobj = kobject_create_and_add("camera_reserve_pool", kernel_kobj);
	if (!camera_reserve_pool_kobj) {
		pr_err("failed to create camera reserve pool node.\n");
		return;
	}
	err = sysfs_create_group(camera_reserve_pool_kobj, &camera_reserve_pool_attr_group);
	if (err) {
		pr_err("failed to create camera reserve pool attrs.\n");
		kobject_put(camera_reserve_pool_kobj);
		camera_reserve_pool_kobj = NULL;
	}
}

void camera_sys_heap_reserve_pool_deinit(struct qcom_system_heap *sys_heap)
{
	cancel_delayed_work_sync(&g_refill_cache_work);
	unregister_trace_android_vh_free_unref_page_bypass(free_unref_page_bypass, NULL);

	if (camera_reserve_pool_kobj) {
		sysfs_remove_group(camera_reserve_pool_kobj,
				   &camera_reserve_pool_attr_group);
		kobject_put(camera_reserve_pool_kobj);
		camera_reserve_pool_kobj = NULL;
	}

	if (sys_heap->cam_dynamic_reserve_pool_list) {
		dynamic_page_pool_release_pools(sys_heap->cam_dynamic_reserve_pool_list);
		sys_heap->cam_dynamic_reserve_pool_list = NULL;
	}

	g_sys_heap = NULL;
}

void camera_sys_heap_reserve_pool_init(const char *name, struct qcom_system_heap *sys_heap)
{
	int i, ret;

	g_sys_heap = sys_heap;
	sys_heap->cam_dynamic_reserve_pool_list = dynamic_page_pool_create_pools(CAM_DYNAMIC_RESERVE_VMID, NULL);

	pr_info("%s %d %s\n", current->comm, current->pid, name);
	if (IS_ERR(sys_heap->cam_dynamic_reserve_pool_list)) {
		sys_heap->cam_dynamic_reserve_pool_list = NULL;
		g_sys_heap = NULL;
		return;
	}
	for (i = 0; i < NUM_ORDERS; i++) {
		atomic64_set(&g_use_order[i], 0);
		atomic64_set(&g_refill_amount[i], 0);
	}

	camera_reserve_pool_sysfs_create();
	INIT_DEFERRABLE_WORK(&g_refill_cache_work, refill_cache_timer_fn);
	ret = register_trace_android_vh_free_unref_page_bypass(free_unref_page_bypass, NULL);
	if (ret) {
		pr_err("Failed to register free_unref_page_bypass hook.\n");
		goto err_init;
	}

	return;

err_init:
	camera_sys_heap_reserve_pool_deinit(sys_heap);
}

static inline struct page *get_page_from_pool(struct dynamic_page_pool *pool)
{
	struct page *page = NULL;
	unsigned long flags;

	spin_lock_irqsave(&pool->lock, flags);
	if (pool->high_count)
		page = dynamic_page_pool_remove(pool, true);
	else if (pool->low_count)
		page = dynamic_page_pool_remove(pool, false);
	spin_unlock_irqrestore(&pool->lock, flags);

	return page;
}

struct page *get_page_from_camera_dynamic_reserve_pool(struct qcom_system_heap *sys_heap, bool served_app,
		struct dynamic_page_pool *cam_reserve_pool)
{
	struct page *page = NULL;

	if (!sys_heap->cam_dynamic_reserve_pool_list || !served_app)
		return NULL;
	if (cam_reserve_pool->order != 0)
		return NULL;

	if (atomic_read(&g_debug) & RESERVE_POOL)
		pr_info("%s: pool_count: %d, scene %d, pid: %d\n",
			__func__, atomic_read(&cam_reserve_pool->count),
			atomic_read(&g_scene), task_tgid_vnr(current));

	if (!is_camera_process())
		return NULL;

	if (atomic_read(&g_debug) & RESERVE_POOL)
		pr_info("%s: allow used camera_extra_pages reserve pool\n", __func__);

	page = get_page_from_pool(cam_reserve_pool);
	if (page)
		atomic64_inc(&g_use_order[ORDER_ZERO]);
	return page;
}

bool is_allow_from_reserve_pool(struct dynamic_page_pool *reserve_pool)
{
	int camera_reserved_pages = atomic_read(&g_camera_reserved_pages);
	unsigned long pool_count;

	if (reserve_pool->order != 0 || camera_reserved_pages <= 0)
		return true;

	pool_count = atomic_read(&reserve_pool->count);

	if (atomic_read(&g_debug) & RESERVE_POOL)
		pr_info("%s: camera_reserved_pages: %d, scene: %d, pool_count: %lu, pid: %d\n",
			__func__, camera_reserved_pages, atomic_read(&g_scene),
			pool_count, task_tgid_vnr(current));

	if (pool_count > camera_reserved_pages)
		return true;

	/* Pool is at or below camera watermark — only camera processes may consume. */
	return is_camera_process();
}
