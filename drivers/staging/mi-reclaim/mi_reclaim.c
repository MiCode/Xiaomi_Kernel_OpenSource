#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/sched.h>
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

static inline unsigned long reclaim_time(unsigned long start)
{
	unsigned long end = jiffies;

	if (end >= start)
		return (unsigned long)(end - start);

	return (unsigned long)(end + (MAX_JIFFY_OFFSET - start) + 1);
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
	unsigned long real_reclaim;
	unsigned long pages;
	unsigned long start_jiffies;
	unsigned long nr_anon_pages;
	unsigned long wmark_low_max = 0;
	u32 anon_up_threshold;
	u32 nr_want_reclaim_pages;
	u64 reclaim_pages;
	u64 reclaim_times;
	u64 spent_time;
	struct zone *zone;
	struct sysinfo i;

	reclaim_type = 0;
	reclaim_pages = 0;
	reclaim_times = 0;
	spent_time = 0;
	zram_enable = (i.totalswap > 0);
	if (!zram_enable)
		return;

	for_each_zone(zone)
		if (zone->_watermark[WMARK_LOW] > wmark_low_max)
			wmark_low_max = zone->_watermark[WMARK_LOW];

	spin_lock(&pg_mi_reclaim->page_reclaim.reclaim_lock);
	force_reclaim_type = pg_mi_reclaim->page_reclaim.reclaim_type;
	nr_want_reclaim_pages = pg_mi_reclaim->page_reclaim.nr_reclaim;
	anon_up_threshold = pg_mi_reclaim->page_reclaim.anon_up_threshold;
	spin_unlock(&pg_mi_reclaim->page_reclaim.reclaim_lock);
	for (pages = 0; pages < nr_want_reclaim_pages; pages += MIN_RECLAIM_PAGES) {
		unsigned long freeram;
		real_reclaim = 0;
		start_jiffies = jiffies;

		freeram = global_zone_page_state(NR_FREE_PAGES);
		if (freeram < wmark_low_max)
			break;

		if (zram_enable) {
			nr_anon_pages = global_node_page_state(NR_INACTIVE_ANON);
			nr_anon_pages += global_node_page_state(NR_ACTIVE_ANON);
			if (nr_anon_pages > anon_up_threshold) {
				reclaim_type |= (UNMAP_PAGE | SWAP_PAGE);
				if (force_reclaim_type)
					reclaim_type = force_reclaim_type;
				real_reclaim = mi_reclaim_global(MIN_RECLAIM_PAGES, reclaim_type);
			}
		}

		if (pg_mi_reclaim->debug) {
			long available;
			available = si_mem_available();
			si_meminfo(&i);
			si_swapinfo(&i);
			pr_info("reclaim: reclaim_type=%d real_reclaim=%lu freeram=%llu freeswap=%llu MemAvailable=%ld\n",
				reclaim_type, real_reclaim, KB(i.freeram), KB(i.freeswap), KB(available));
		}

		reclaim_type = 0;
		reclaim_pages += real_reclaim;
		spent_time += reclaim_time(start_jiffies);
		if (reclaim_pages > nr_want_reclaim_pages)
			break;

		cond_resched();
	}

	spin_lock(&pg_mi_reclaim->page_reclaim.reclaim_lock);
	pg_mi_reclaim->page_reclaim.total_reclaim_pages += reclaim_pages;
	pg_mi_reclaim->page_reclaim.total_reclaim_times++;
	pg_mi_reclaim->page_reclaim.total_spent_time += spent_time;
	pg_mi_reclaim->need_reclaim = false;
	spin_unlock(&pg_mi_reclaim->page_reclaim.reclaim_lock);
}

static inline int mi_reclaim_thread_wakeup(void)
{

	if (!pg_mi_reclaim->switch_on)
		return RET_OK;

	pg_mi_reclaim->need_reclaim = true;
	wake_up(&pg_mi_reclaim->wait);
	return RET_OK;
}

static int mi_reclaim_thread(void *unused)
{
	struct task_struct *tsk = current;

	tsk->flags |= PF_MEMALLOC;
	set_freezable();

	while (!kthread_should_stop()) {
		try_to_freeze();

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
	u32 nr_reclaim;
	u32 anon_up_threshold;
	u64 totalram;

	spin_lock(&pg_mi_reclaim->page_reclaim.reclaim_lock);
	reclaim_swappiness = pg_mi_reclaim->page_reclaim.reclaim_swappiness;
	reclaim_type = pg_mi_reclaim->page_reclaim.reclaim_type;
	nr_reclaim = pg_mi_reclaim->page_reclaim.nr_reclaim;
	anon_up_threshold = pg_mi_reclaim->page_reclaim.anon_up_threshold;
	totalram = pg_mi_reclaim->sysinfo.totalram;
	spin_unlock(&pg_mi_reclaim->page_reclaim.reclaim_lock);

	ret = snprintf(buf, PAGE_SIZE,
		"swappiness %d\n"
		"reclaim_type %d\n"
		"nr_reclaim %lu\n"
		"anon_up %lu\n"
		"totalram %llu\n",
		reclaim_swappiness,
		reclaim_type,
		nr_reclaim,
		anon_up_threshold,
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

	/*  event which range is under fifty is event type,
	*  zero is cancel st mode, one is reclaim mode, two is st mode;
	*  More than 50 is number of reclaimed pages */
	if (event) {
		if (ST_MODE == event) {
			spin_lock(&pg_mi_reclaim->page_reclaim.reclaim_lock);
			pg_mi_reclaim->page_reclaim.event_type = event;
			spin_unlock(&pg_mi_reclaim->page_reclaim.reclaim_lock);
		} else if (RECLAIM_MODE == event) {
			mi_reclaim_thread_wakeup();
		} else {
			/*event which value is one represent wake-up reclaim thread
			 *other values represent quantity of reclaimed, unit is MB*/
			spin_lock(&pg_mi_reclaim->page_reclaim.reclaim_lock);
			pg_mi_reclaim->page_reclaim.nr_reclaim = PAGES(event);
			spin_unlock(&pg_mi_reclaim->page_reclaim.reclaim_lock);
		}
	} else {
		spin_lock(&pg_mi_reclaim->page_reclaim.reclaim_lock);
		pg_mi_reclaim->page_reclaim.event_type = CANCEL_ST;
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

	if (sscanf(buf, "%d %d %lu %lu", &reclaim_swappiness,
			&reclaim_type,
			&nr_reclaim,
			&anon_up_threshold) != 4) {
		return -EINVAL;
	}

	spin_lock(&pg_mi_reclaim->page_reclaim.reclaim_lock);
	pg_mi_reclaim->page_reclaim.reclaim_swappiness = reclaim_swappiness;
	pg_mi_reclaim->page_reclaim.reclaim_type = reclaim_type;
	pg_mi_reclaim->page_reclaim.nr_reclaim = nr_reclaim;
	pg_mi_reclaim->page_reclaim.anon_up_threshold = anon_up_threshold;
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
	pg_mi_reclaim->page_reclaim.nr_reclaim = ONCE_RECLAIM_PAGES;
	pg_mi_reclaim->page_reclaim.anon_up_threshold = DEFAULT_ANON_PAGE_UP_THRESHOLD_FOR_EIGHTGB;
	if (i.totalram > RAM_EIGHTGB_SIZE)
		pg_mi_reclaim->page_reclaim.anon_up_threshold = DEFAULT_ANON_PAGE_UP_THRESHOLD_FOR_TWELVEGB;
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
