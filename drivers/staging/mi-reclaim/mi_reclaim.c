#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/swap.h>
#include <linux/semaphore.h>
#include <linux/rwsem.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/cpu.h>
#include <linux/jiffies.h>
#include <linux/vmstat.h>
#include <linux/freezer.h>
#include <linux/kernel_stat.h>
#include <linux/tick.h>
#include <linux/version.h>

#include "mi_reclaim.h"

#define MI_RECLAIM_ATTR(_name, _mode, _show, _store) \
	struct kobj_attribute kobj_attr_##_name \
		= __ATTR(_name, _mode, _show, _store)

mi_reclaim_t g_mi_reclaim;
mi_reclaim_t *pg_mi_reclaim;

extern unsigned long mi_reclaim_global(unsigned long nr_to_reclaim, int reclaim_type);

static inline unsigned long interval_time(unsigned long start)
{
	unsigned long end = jiffies;

	if (end >= start)
		return (unsigned long)(end - start);

	return (unsigned long)(end + (MAX_JIFFY_OFFSET - start) + 1);
}

static inline unsigned long reclaim_interval(unsigned long start)
{
	return interval_time(start);
}

static inline unsigned long reclaim_time(unsigned long start)
{
	return interval_time(start);
}

inline bool mi_reclaim(const char *name)
{
	return strncmp("mi_reclaim", name, strlen("mi_reclaim")) == 0;
}

inline bool mi_st(void)
{
	return ST_MODE == pg_mi_reclaim->page_reclaim.event_type;
}

inline int mi_reclaim_swappiness(void)
{
	return pg_mi_reclaim->page_reclaim.reclaim_swappiness;
}

static inline void do_reclaim(void)
{
	bool   zram_enable;
	int reclaim_type;
	int force_reclaim_type;
	int event_type;
	int reclaim_ratio_contiguous_low_count;
	long nr_cached;
	unsigned long real_reclaim;
	unsigned long pages;
	unsigned long nr_inactive_anon_pages;
	unsigned long nr_active_anon_pages;
	unsigned long nr_anon_pages;
#ifdef ZONE_WMART
	unsigned long wmark_low_max = 0;
	struct zone *zone;
#endif
	unsigned long once_reclaim_time_up;
	unsigned long anon_up_threshold;
	unsigned long file_up_threshold;
	unsigned long nr_want_reclaim_pages;
	u64 reclaim_pages;
	u64 reclaim_times;
	u64 spent_time;
	u64 start_time_ns;
	struct sysinfo i;

	reclaim_type = 0;
	reclaim_pages = 0;
	reclaim_times = 0;
	spent_time = 0;
	reclaim_ratio_contiguous_low_count = 0;

	si_meminfo(&i);
	si_swapinfo(&i);
	zram_enable = (i.totalswap > 0);
	if (!zram_enable || i.freeswap < FREE_SWAP_LIMIT)
		goto end_reclaim;
#ifdef ZONE_WMART
#if(LINUX_VERSION_CODE >= KERNEL_VERSION(4,19,0))
	for_each_zone(zone)
		if (zone->_watermark[WMARK_LOW] > wmark_low_max)
			wmark_low_max = zone->_watermark[WMARK_LOW];
#else
	for_each_zone(zone)
		if (zone->watermark[WMARK_LOW] > wmark_low_max)
			wmark_low_max = zone->watermark[WMARK_LOW];
#endif
#endif

	nr_cached = global_node_page_state(NR_FILE_PAGES) - total_swapcache_pages() - i.bufferram;
	if (nr_cached < DEFAULT_FILE_PAGE_DOWN_THRESHOLD)
		goto end_reclaim;

	spin_lock(&pg_mi_reclaim->page_reclaim.reclaim_lock);
	force_reclaim_type = pg_mi_reclaim->page_reclaim.reclaim_type;
	event_type = pg_mi_reclaim->page_reclaim.event_type;
	nr_want_reclaim_pages = pg_mi_reclaim->page_reclaim.nr_reclaim;
	anon_up_threshold = pg_mi_reclaim->page_reclaim.anon_up_threshold;
	file_up_threshold = pg_mi_reclaim->page_reclaim.file_up_threshold;
	once_reclaim_time_up = pg_mi_reclaim->page_reclaim.once_reclaim_time_up;
	spin_unlock(&pg_mi_reclaim->page_reclaim.reclaim_lock);

	if  (ST_MODE != event_type) {
		if (jiffies_to_msecs(reclaim_interval(pg_mi_reclaim->page_reclaim.last_reclaim_time)) < RECLAIM_INTERVAL_TIME)
			goto end_reclaim;
		if (nr_cached < file_up_threshold) {
			event_type = PRESSURE_MODE;
			nr_want_reclaim_pages = PRESSURE_ONCE_RECLAIM_PAGES;
		}
	}

	pg_mi_reclaim->page_reclaim.last_reclaim_time = jiffies;
	for (pages = 0; pages < nr_want_reclaim_pages; pages += MIN_RECLAIM_PAGES) {
		real_reclaim = 0;
		start_time_ns = ktime_get_ns();

#ifdef ZONE_WMART
		if (global_zone_page_state(NR_FREE_PAGES) < wmark_low_max)
			break;
#endif
		if (zram_enable) {
			nr_inactive_anon_pages = global_node_page_state(NR_INACTIVE_ANON);
			nr_active_anon_pages = global_node_page_state(NR_ACTIVE_ANON);
			nr_anon_pages = nr_inactive_anon_pages + nr_active_anon_pages;
			if (nr_anon_pages > anon_up_threshold || force_reclaim_type) {
				reclaim_type |= (UNMAP_PAGE | SWAP_PAGE);
				if (force_reclaim_type)
					reclaim_type = force_reclaim_type;
				real_reclaim = mi_reclaim_global(MIN_RECLAIM_PAGES, reclaim_type);
				if (real_reclaim < MIN_RECLAIM_PAGES / 3)
					reclaim_ratio_contiguous_low_count++;
				else
					reclaim_ratio_contiguous_low_count = 0;
				if (reclaim_ratio_contiguous_low_count > 512)
					break;
			}
		}

		if (pg_mi_reclaim->debug) {
			long available;
			available = si_mem_available();
			si_meminfo(&i);
			si_swapinfo(&i);
			pr_info("reclaim: reclaim_type=%d real_reclaim=%lu freeram=%llu freeswap=%llu"
				" MemAvailable=%ld nr_inactive_anon_pages=%lu nr_active_anon_pages=%lu"
				" reclaim_ratio_contiguous_low_count %d",
				reclaim_type, real_reclaim, KB(i.freeram), KB(i.freeswap),
				KB(available), nr_inactive_anon_pages, nr_active_anon_pages,
				reclaim_ratio_contiguous_low_count);
		}

		reclaim_type = 0;
		reclaim_pages += real_reclaim;
		spent_time += MAX(1, ktime_get_ns() - start_time_ns);
		if (reclaim_pages > nr_want_reclaim_pages)
			break;

		if (nr_anon_pages < anon_up_threshold)
			break;

		if (ST_MODE != event_type && PRESSURE_MODE != event_type &&
				spent_time > once_reclaim_time_up)
			break;

		cond_resched();
	}

end_reclaim:
	spin_lock(&pg_mi_reclaim->page_reclaim.reclaim_lock);
	pg_mi_reclaim->page_reclaim.total_reclaim_pages += reclaim_pages;
	pg_mi_reclaim->page_reclaim.total_reclaim_times++;
	pg_mi_reclaim->page_reclaim.total_spent_time += spent_time;
	pg_mi_reclaim->page_reclaim.nr_reclaim = DEFAULT_ONCE_RECLAIM_PAGES;
	pg_mi_reclaim->need_reclaim = false;
	spin_unlock(&pg_mi_reclaim->page_reclaim.reclaim_lock);
	if (pg_mi_reclaim->debug)
		pr_info("reclaim event_type %d nr_cached %ld spent_time %llu ns"
			" nr_want_reclaim_pages %lu reclaim_pages %llu"
			" once_reclaim_time_up %lu\n",
			event_type, nr_cached, spent_time,
			nr_want_reclaim_pages, reclaim_pages,
			once_reclaim_time_up);
}

static inline int mi_reclaim_thread_wakeup(void)
{
	struct task_struct *reclaim_tsk;

	if (!pg_mi_reclaim->switch_on)
		return RET_OK;

	reclaim_tsk = pg_mi_reclaim->task;
	if (reclaim_tsk && reclaim_tsk->state != TASK_RUNNING) {
		pg_mi_reclaim->need_reclaim = true;
		wake_up(&pg_mi_reclaim->wait);
	}
	return RET_OK;
}

static void mi_reclaim_thread_affinity(struct task_struct *task)
{
	DECLARE_BITMAP(cpu_bitmap, NR_CPUS);
	struct cpumask *newmask;
	long rc;

	// affinity in 0 to 3 cores
	cpu_bitmap[0] = 15;
	newmask = to_cpumask(cpu_bitmap);

#if(LINUX_VERSION_CODE >= KERNEL_VERSION(5,4,0))
	if (!cpumask_equal(newmask, &task->cpus_mask)) {
#else
	if (!cpumask_equal(newmask, &task->cpus_allowed)) {
#endif
		cpumask_t cpumask_temp;
		cpumask_and(&cpumask_temp, newmask, cpu_online_mask);

		if (0 == cpumask_weight(&cpumask_temp))
			return;

		rc = sched_setaffinity(task->pid, newmask);
		if (rc != 0)
			pr_err("mi_reclaim sched_setaffinity() failed");
	}

	return;
}

static int mi_reclaim_thread(void *unused)
{
	struct task_struct *tsk = current;

	tsk->flags |= PF_MEMALLOC;
	set_freezable();

	while (!kthread_should_stop()) {
		try_to_freeze();

		mi_reclaim_thread_affinity(tsk);
		wait_event_freezable(pg_mi_reclaim->wait,
			pg_mi_reclaim->need_reclaim || kthread_should_stop());

		do_reclaim();
	}

	tsk->flags &= ~PF_MEMALLOC;

	return RET_OK;
}

static int mi_reclaim_thread_start(void)
{
	if (pg_mi_reclaim->task)
		return -EPERM;

	pg_mi_reclaim->task = kthread_run(mi_reclaim_thread, NULL, "mi_reclaim");
	if (IS_ERR(pg_mi_reclaim->task)) {
		pr_err("failed to start pg_mi_reclaim thread\n");
		return RET_FAIL;
	}

	return RET_OK;
}

static void mi_reclaim_thread_stop(void)
{
	if (!pg_mi_reclaim->task)
		return;

	mi_reclaim_thread_wakeup();
	kthread_stop(pg_mi_reclaim->task);
	pg_mi_reclaim->task = NULL;
}

static ssize_t stat_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	u64 total_spent_time;
	u64 total_reclaim_pages;
	u64 total_reclaim_times;

	spin_lock(&pg_mi_reclaim->page_reclaim.reclaim_lock);
	total_spent_time = pg_mi_reclaim->page_reclaim.total_spent_time;
	total_reclaim_pages = pg_mi_reclaim->page_reclaim.total_reclaim_pages;
	total_reclaim_times = pg_mi_reclaim->page_reclaim.total_reclaim_times;
	spin_unlock(&pg_mi_reclaim->page_reclaim.reclaim_lock);

	return scnprintf(buf, PAGE_SIZE,
		"total_reclaim_pages: %llu\n"
		"total_reclaim_times: %llu\n"
		"total_spent_time: %llu\n",
		total_reclaim_pages,
		total_reclaim_times,
		total_spent_time);
}

static ssize_t enable_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", pg_mi_reclaim->switch_on);
}

static ssize_t event_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", pg_mi_reclaim->page_reclaim.event_type);
}

static ssize_t debug_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", pg_mi_reclaim->debug);
}

static ssize_t config_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int ret;
	int reclaim_swappiness;
	int reclaim_type;
	unsigned long once_reclaim_time_up;
	unsigned long nr_reclaim;
	unsigned long anon_up_threshold;
	unsigned long file_up_threshold;
	u64 totalram;

	spin_lock(&pg_mi_reclaim->page_reclaim.reclaim_lock);
	reclaim_swappiness = pg_mi_reclaim->page_reclaim.reclaim_swappiness;
	reclaim_type = pg_mi_reclaim->page_reclaim.reclaim_type;
	nr_reclaim = pg_mi_reclaim->page_reclaim.nr_reclaim;
	anon_up_threshold = pg_mi_reclaim->page_reclaim.anon_up_threshold;
	file_up_threshold = pg_mi_reclaim->page_reclaim.file_up_threshold;
	once_reclaim_time_up = pg_mi_reclaim->page_reclaim.once_reclaim_time_up;
	totalram = pg_mi_reclaim->sysinfo.totalram;
	spin_unlock(&pg_mi_reclaim->page_reclaim.reclaim_lock);

	ret = snprintf(buf, PAGE_SIZE,
		"swappiness %d\n"
		"reclaim_type %d\n"
		"nr_reclaim %lu\n"
		"anon_up %lu\n"
		"file_up %lu\n"
		"once_reclaim_time_up %lu\n"
		"totalram %llu\n",
		reclaim_swappiness,
		reclaim_type,
		nr_reclaim,
		anon_up_threshold,
		file_up_threshold,
		once_reclaim_time_up,
		totalram);

	return ret;
}

static ssize_t enable_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len)
{
	int enable;
	int ret;

	ret = kstrtoint(buf, 0, &enable);
	if (ret)
		return ret;

	if (enable) {
		pg_mi_reclaim->switch_on = true;

	} else {
		pg_mi_reclaim->switch_on = false;
	}

	return len;
}

static ssize_t event_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len)
{
	unsigned long event;
	int ret;

	ret = kstrtoul(buf, 0, &event);
	if (ret)
		return ret;

	/* event which range is under fifty is event type,
	*  zero  : cancel st mode
	*  one   : reclaim mode
	*  two   : st mode
	*  three : pressure mode
	*  More than 50 is number of reclaimed pages */
	if (event) {
		if (ST_MODE == event) {
			spin_lock(&pg_mi_reclaim->page_reclaim.reclaim_lock);
			pg_mi_reclaim->page_reclaim.event_type = event;
			pg_mi_reclaim->page_reclaim.nr_reclaim = ST_ONCE_RECLAIM_PAGES;
			spin_unlock(&pg_mi_reclaim->page_reclaim.reclaim_lock);
		} else if (RECLAIM_MODE == event) {
			mi_reclaim_thread_wakeup();
		} else {
			/*event which value is one represent wake-up reclaim thread
			 *other values represent quantity of reclaimed, unit is MB*/
			if (PAGES(event) >= MIN_ONCE_RECLAIM_PAGES) {
				spin_lock(&pg_mi_reclaim->page_reclaim.reclaim_lock);
				pg_mi_reclaim->page_reclaim.nr_reclaim = PAGES(event);
				spin_unlock(&pg_mi_reclaim->page_reclaim.reclaim_lock);
				mi_reclaim_thread_wakeup();
			}
		}
	} else {
		spin_lock(&pg_mi_reclaim->page_reclaim.reclaim_lock);
		pg_mi_reclaim->page_reclaim.event_type = CANCEL_ST;
		pg_mi_reclaim->page_reclaim.nr_reclaim = DEFAULT_ONCE_RECLAIM_PAGES;
		spin_unlock(&pg_mi_reclaim->page_reclaim.reclaim_lock);
	}

	return len;
}

static ssize_t debug_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len)
{
	int enable;
	int ret;

	ret = kstrtoint(buf, 0, &enable);
	if (ret)
		return ret;

	if (enable)
		pg_mi_reclaim->debug = true;
	else
		pg_mi_reclaim->debug = false;

	return len;
}


static ssize_t config_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len)
{
	int reclaim_swappiness;
	int reclaim_type;
	unsigned long nr_reclaim;
	unsigned long anon_up_threshold;
	unsigned long file_up_threshold;
	unsigned long once_reclaim_time_up;

	if (sscanf(buf, "%d %d %lu %lu %lu %lu",
			&reclaim_swappiness,
			&reclaim_type,
			&nr_reclaim,
			&anon_up_threshold,
			&file_up_threshold,
			&once_reclaim_time_up) != 6) {
		return -EINVAL;
	}

	spin_lock(&pg_mi_reclaim->page_reclaim.reclaim_lock);
	pg_mi_reclaim->page_reclaim.reclaim_swappiness = reclaim_swappiness;
	pg_mi_reclaim->page_reclaim.reclaim_type = reclaim_type;
	pg_mi_reclaim->page_reclaim.nr_reclaim = nr_reclaim;
	pg_mi_reclaim->page_reclaim.anon_up_threshold = anon_up_threshold;
	pg_mi_reclaim->page_reclaim.file_up_threshold = file_up_threshold;
	pg_mi_reclaim->page_reclaim.once_reclaim_time_up = once_reclaim_time_up;
	spin_unlock(&pg_mi_reclaim->page_reclaim.reclaim_lock);

	return len;
}

static MI_RECLAIM_ATTR(enable, MI_RECLAIM_MODE_RW, enable_show, enable_store);
static MI_RECLAIM_ATTR(event, MI_RECLAIM_MODE_RW, event_show, event_store);
static MI_RECLAIM_ATTR(debug, MI_RECLAIM_MODE_RW, debug_show, debug_store);
static MI_RECLAIM_ATTR(config, MI_RECLAIM_MODE_RW, config_show, config_store);
static MI_RECLAIM_ATTR(stat, MI_RECLAIM_MODE_RO, stat_show, NULL);

static struct attribute *mi_reclaim_attrs[] = {
	&kobj_attr_enable.attr,
	&kobj_attr_event.attr,
	&kobj_attr_debug.attr,
	&kobj_attr_config.attr,
	&kobj_attr_stat.attr,
	NULL,
};

static struct attribute_group mi_reclaim_attr_group = {
	.attrs = mi_reclaim_attrs,
};

static struct kobject *mi_reclaim_sysfs_create(void)
{
	int err;
	struct kobject *kobj = NULL;
	kobj = kobject_create_and_add("mi_reclaim", kernel_kobj);
	if (!kobj) {
		pr_err("failed to create mi reclaim node.\n");
		return NULL;
	}
	err = sysfs_create_group(kobj, &mi_reclaim_attr_group);
	if (err) {
		pr_err("failed to create mi reclaim attrs.\n");
		kobject_put(kobj);
		return NULL;
	}
	return kobj;
}

static void mi_reclaim_sysfs_destory(struct kobject *kobj)
{
	if (!kobj)
		return;
	kobject_put(kobj);
}

static void mi_reclaim_setup(void)
{
	struct sysinfo i;
	pg_mi_reclaim = &g_mi_reclaim;

	si_meminfo(&i);

	pg_mi_reclaim->sysinfo.totalram = i.totalram;
	pg_mi_reclaim->page_reclaim.last_reclaim_time = jiffies;
	pg_mi_reclaim->page_reclaim.reclaim_swappiness = RECLAIM_SWAPPINESS;
	pg_mi_reclaim->page_reclaim.nr_reclaim = DEFAULT_ONCE_RECLAIM_PAGES;
	pg_mi_reclaim->page_reclaim.once_reclaim_time_up = BACK_HOME_RECLAIM_TIME_UP;
	pg_mi_reclaim->page_reclaim.anon_up_threshold = DEFAULT_ANON_PAGE_UP_THRESHOLD;
	pg_mi_reclaim->page_reclaim.file_up_threshold = DEFAULT_FILE_PAGE_UP_THRESHOLD;
	init_waitqueue_head(&pg_mi_reclaim->wait);
	spin_lock_init(&pg_mi_reclaim->page_reclaim.reclaim_lock);
}

static int __init mi_reclaim_init(void)
{
	mi_reclaim_setup();

	pg_mi_reclaim->kobj = mi_reclaim_sysfs_create();
	if (!pg_mi_reclaim->kobj)
		goto failed_to_create_sysfs;

	if (mi_reclaim_thread_start() < 0)
		return RET_FAIL;

	pg_mi_reclaim->switch_on = true;
	pr_info("mi_reclaim init ok\n");
	return RET_OK;

failed_to_create_sysfs:
	return RET_FAIL;
}

static void __exit mi_reclaim_exit(void)
{
	mi_reclaim_sysfs_destory(pg_mi_reclaim->kobj);
	pg_mi_reclaim->kobj = NULL;

	mi_reclaim_thread_stop();
}

module_init(mi_reclaim_init);
module_exit(mi_reclaim_exit);

MODULE_AUTHOR("zhangcang<zhangcang@xaiomi.com>");
MODULE_DESCRIPTION("page reclaim");
MODULE_LICENSE("GPL");
