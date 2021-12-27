#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mmzone.h>
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

#include "cam_reclaim.h"

#define CAM_RECLAIM_ATTR(_name, _mode, _show, _store) \
	struct kobj_attribute kobj_attr_##_name \
		= __ATTR(_name, _mode, _show, _store)

cam_reclaim_t g_cam_reclaim;
cam_reclaim_t *pg_cam_reclaim;

extern unsigned long cam_reclaim_global(unsigned long nr_to_reclaim, int reclaim_type);

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

inline bool cam_reclaim(const char *name)
{
	return strncmp("cam_reclaim", name, strlen("cam_reclaim")) == 0;
}

inline int cam_reclaim_swappiness(void)
{
	return pg_cam_reclaim->page_reclaim.reclaim_swappiness;
}

inline int cam_reclaim_anonprivate(void)
{
	return pg_cam_reclaim->page_reclaim.reclaim_anonprivate;
}

static int sysmeminfo_process(int force_reclaim_type, u64 reclaim_pages, unsigned long real_reclaim, u64 total_reclaim_pages, bool debug)
{
	struct sysinfo i;
	long nr_cached;
	unsigned long pages[NR_LRU_LISTS], anon_mem;
	int lru;
	bool   zram_enable;

	for (lru = LRU_BASE; lru < NR_LRU_LISTS; lru++)
		pages[lru] = global_node_page_state(NR_LRU_BASE + lru);
	si_meminfo(&i);
	nr_cached = global_node_page_state(NR_FILE_PAGES) - total_swapcache_pages() - i.bufferram;
	si_swapinfo(&i);

	if (debug) {
		pr_info("reclaim: reclaim_type=%d total_reclaim_pages=%llu reclaim_pages=%llu real_reclaim=%lu freeram=%llu freeswap=%llu nr_cached=%llu"
			" Active(anon)=%llu Inactive(anon)=%llu Active(file)=%llu Inactive(file)=%llu KReclaimable=%llu\n",
			force_reclaim_type, total_reclaim_pages, reclaim_pages, real_reclaim, KB(i.freeram), KB(i.freeswap), KB(nr_cached), KB(pages[LRU_ACTIVE_ANON]), KB(pages[LRU_INACTIVE_ANON]), KB(pages[LRU_ACTIVE_FILE]), KB(pages[LRU_INACTIVE_FILE]), KB(global_node_page_state(NR_SLAB_RECLAIMABLE) + global_node_page_state(NR_KERNEL_MISC_RECLAIMABLE)));
	}

        if ((force_reclaim_type == 1) && (KB(nr_cached) < 104800)) {
		pr_info("file cached is too small, stop reclaim work\n");
		return 0;
	} else if (force_reclaim_type == 3) {
		anon_mem = KB(pages[LRU_ACTIVE_ANON]) + KB(pages[LRU_INACTIVE_ANON]);
		zram_enable = (i.totalswap > 0);
		if (anon_mem < 206000 || !zram_enable || i.freeswap < FREE_SWAP_LIMIT) {
			pr_info("anon_mem is not right, stop reclaim work, zram enable %d\n", zram_enable);
			return 0;
		}
	}

	return 1;
}

static inline void do_reclaim(int force_reclaim_type)
{
	int event_type;
	int reclaim_need;
	unsigned long real_reclaim;
	unsigned long pages;

	unsigned long once_reclaim_time_up;
	unsigned long nr_want_reclaim_pages;
        unsigned long per_want_reclaim_pages;
	u64 reclaim_pages;
	u64 spent_time;
	u64 start_time_ns;

	real_reclaim = 0;
	reclaim_pages = 0;
	spent_time = 0;

	spin_lock(&pg_cam_reclaim->page_reclaim.reclaim_lock);
	nr_want_reclaim_pages = pg_cam_reclaim->page_reclaim.nr_reclaim;
        per_want_reclaim_pages = pg_cam_reclaim->page_reclaim.per_reclaim;
	once_reclaim_time_up = pg_cam_reclaim->page_reclaim.once_reclaim_time_up;
	spin_unlock(&pg_cam_reclaim->page_reclaim.reclaim_lock);

	reclaim_need = sysmeminfo_process(force_reclaim_type, reclaim_pages, real_reclaim, pg_cam_reclaim->page_reclaim.total_reclaim_pages, pg_cam_reclaim->debug);
	if (reclaim_need == 0)
		return;

	for (pages = 0; pages < nr_want_reclaim_pages; pages += real_reclaim) {

		real_reclaim = 0;

		spin_lock(&pg_cam_reclaim->page_reclaim.reclaim_lock);
		event_type = pg_cam_reclaim->page_reclaim.event_type;
		if (event_type == CANCEL_ST) {
			pr_info("cam reclaim work stopped\n");
			spin_unlock(&pg_cam_reclaim->page_reclaim.reclaim_lock);
			break;
		}
		spin_unlock(&pg_cam_reclaim->page_reclaim.reclaim_lock);

		start_time_ns = ktime_get_ns();

		if (force_reclaim_type == 1)
			trace_printk("tracing_mark_write: B|%d|do_file_reclaim\n", current->tgid);
		else
			trace_printk("tracing_mark_write: B|%d|do_anon_reclaim\n", current->tgid);

		real_reclaim = cam_reclaim_global(per_want_reclaim_pages, force_reclaim_type);
		trace_printk("tracing_mark_write: E\n");

		reclaim_need = sysmeminfo_process(force_reclaim_type, reclaim_pages, real_reclaim, pg_cam_reclaim->page_reclaim.total_reclaim_pages, pg_cam_reclaim->debug);
		if (reclaim_need == 0)
			break;

		reclaim_pages += real_reclaim;
		spent_time +=  ktime_get_ns() - start_time_ns;
		if (reclaim_pages > nr_want_reclaim_pages)
			break;

		if (spent_time > once_reclaim_time_up) {
			pr_info("cam reclaim timeout, work stopped\n");
			break;
		}

		cond_resched();
	}

	spin_lock(&pg_cam_reclaim->page_reclaim.reclaim_lock);
	pg_cam_reclaim->page_reclaim.total_reclaim_pages += reclaim_pages;
	pg_cam_reclaim->page_reclaim.total_reclaim_times++;
	pg_cam_reclaim->page_reclaim.total_spent_time += spent_time;
	pg_cam_reclaim->need_reclaim = false;
	spin_unlock(&pg_cam_reclaim->page_reclaim.reclaim_lock);
	if (pg_cam_reclaim->debug)
		pr_info("reclaim event_type %d spent_time %llu ns"
			" nr_want_reclaim_pages %lu per_want_reclaim_pages %lu reclaim_pages %llu"
			" once_reclaim_time_up %lu\n",
			event_type, spent_time,
			nr_want_reclaim_pages, per_want_reclaim_pages, reclaim_pages,
			once_reclaim_time_up);
}

static inline int cam_reclaim_thread_wakeup(void)
{

	if (!pg_cam_reclaim->switch_on)
		return RET_OK;

	pg_cam_reclaim->need_reclaim = true;
	wake_up(&pg_cam_reclaim->wait);
	return RET_OK;
}

static void cam_reclaim_thread_affinity(struct task_struct *task)
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
			pr_err("cam_reclaim sched_setaffinity() failed");
	}

	return;
}

static int cam_reclaim_thread(void *unused)
{
	struct task_struct *tsk = current;

	tsk->flags |= PF_MEMALLOC;
	set_freezable();

	while (!kthread_should_stop()) {
		try_to_freeze();

		cam_reclaim_thread_affinity(tsk);
		wait_event_freezable(pg_cam_reclaim->wait,
			pg_cam_reclaim->need_reclaim || kthread_should_stop());

		do_reclaim(1);
		do_reclaim(3);
	}

	tsk->flags &= ~PF_MEMALLOC;

	return RET_OK;
}

static int cam_reclaim_thread_start(void)
{
	if (pg_cam_reclaim->task)
		return -EPERM;

	pg_cam_reclaim->task = kthread_run(cam_reclaim_thread, NULL, "cam_reclaim");
	if (IS_ERR(pg_cam_reclaim->task)) {
		pr_err("failed to start pg_cam_reclaim thread\n");
		return RET_FAIL;
	}

	return RET_OK;
}

static void cam_reclaim_thread_stop(void)
{
	if (!pg_cam_reclaim->task)
		return;

	cam_reclaim_thread_wakeup();
	kthread_stop(pg_cam_reclaim->task);
	pg_cam_reclaim->task = NULL;
}

static ssize_t stat_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	u64 total_spent_time;
	u64 total_reclaim_pages;
	u64 total_reclaim_times;

	spin_lock(&pg_cam_reclaim->page_reclaim.reclaim_lock);
	total_spent_time = pg_cam_reclaim->page_reclaim.total_spent_time;
	total_reclaim_pages = pg_cam_reclaim->page_reclaim.total_reclaim_pages;
	total_reclaim_times = pg_cam_reclaim->page_reclaim.total_reclaim_times;
	spin_unlock(&pg_cam_reclaim->page_reclaim.reclaim_lock);

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
	return scnprintf(buf, PAGE_SIZE, "%u\n", pg_cam_reclaim->switch_on);
}

static ssize_t event_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", pg_cam_reclaim->page_reclaim.event_type);
}

static ssize_t debug_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", pg_cam_reclaim->debug);
}

static ssize_t config_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int ret;
	int reclaim_swappiness;
	int reclaim_anonprivate;
	int reclaim_type;
	unsigned long once_reclaim_time_up;
	unsigned long nr_reclaim, per_reclaim;
	unsigned long anon_up_threshold;
	unsigned long file_up_threshold;
	u64 totalram;

	spin_lock(&pg_cam_reclaim->page_reclaim.reclaim_lock);
	reclaim_swappiness = pg_cam_reclaim->page_reclaim.reclaim_swappiness;
	reclaim_anonprivate = pg_cam_reclaim->page_reclaim.reclaim_anonprivate;
	reclaim_type = pg_cam_reclaim->page_reclaim.reclaim_type;
	nr_reclaim = pg_cam_reclaim->page_reclaim.nr_reclaim;
        per_reclaim = pg_cam_reclaim->page_reclaim.per_reclaim;
	anon_up_threshold = pg_cam_reclaim->page_reclaim.anon_up_threshold;
	file_up_threshold = pg_cam_reclaim->page_reclaim.file_up_threshold;
	once_reclaim_time_up = pg_cam_reclaim->page_reclaim.once_reclaim_time_up;
	totalram = pg_cam_reclaim->sysinfo.totalram;
	spin_unlock(&pg_cam_reclaim->page_reclaim.reclaim_lock);

	ret = snprintf(buf, PAGE_SIZE,
		"swappiness %d\n"
		"anonprivate %d\n"
		"reclaim_type %d\n"
		"nr_reclaim %lu\n"
                "per_reclaim %lu\n"
		"anon_up %lu\n"
		"file_up %lu\n"
		"once_reclaim_time_up %lu\n"
		"totalram %llu\n",
		reclaim_swappiness,
		reclaim_anonprivate,
		reclaim_type,
		nr_reclaim,
		per_reclaim,
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
		pg_cam_reclaim->switch_on = true;

	} else {
		pg_cam_reclaim->switch_on = false;
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

	spin_lock(&pg_cam_reclaim->page_reclaim.reclaim_lock);
	pg_cam_reclaim->page_reclaim.event_type = (int)event;
	spin_unlock(&pg_cam_reclaim->page_reclaim.reclaim_lock);

	if (event == RECLAIM_MODE)
		cam_reclaim_thread_wakeup();

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
		pg_cam_reclaim->debug = true;
	else
		pg_cam_reclaim->debug = false;

	return len;
}


static ssize_t config_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len)
{
	int reclaim_swappiness;
	int reclaim_anonprivate;
	int reclaim_type;
	unsigned long nr_reclaim, per_reclaim;
	unsigned long anon_up_threshold;
	unsigned long file_up_threshold;
	unsigned long once_reclaim_time_up;

	if (sscanf(buf, "%d %d %d %lu %lu %lu %lu %lu",
			&reclaim_swappiness,
			&reclaim_anonprivate,
			&reclaim_type,
			&nr_reclaim,
			&per_reclaim,
			&anon_up_threshold,
			&file_up_threshold,
			&once_reclaim_time_up) != 8) {
		return -EINVAL;
	}

	spin_lock(&pg_cam_reclaim->page_reclaim.reclaim_lock);
	pg_cam_reclaim->page_reclaim.reclaim_swappiness = reclaim_swappiness;
	pg_cam_reclaim->page_reclaim.reclaim_anonprivate = reclaim_anonprivate;
	pg_cam_reclaim->page_reclaim.reclaim_type = reclaim_type;
	pg_cam_reclaim->page_reclaim.nr_reclaim = nr_reclaim;
	pg_cam_reclaim->page_reclaim.per_reclaim = per_reclaim;
	pg_cam_reclaim->page_reclaim.anon_up_threshold = anon_up_threshold;
	pg_cam_reclaim->page_reclaim.file_up_threshold = file_up_threshold;
	pg_cam_reclaim->page_reclaim.once_reclaim_time_up = once_reclaim_time_up;
	spin_unlock(&pg_cam_reclaim->page_reclaim.reclaim_lock);

	return len;
}

static CAM_RECLAIM_ATTR(enable, CAM_RECLAIM_MODE_RW, enable_show, enable_store);
static CAM_RECLAIM_ATTR(event, CAM_RECLAIM_MODE_RW, event_show, event_store);
static CAM_RECLAIM_ATTR(debug, CAM_RECLAIM_MODE_RW, debug_show, debug_store);
static CAM_RECLAIM_ATTR(config, CAM_RECLAIM_MODE_RW, config_show, config_store);
static CAM_RECLAIM_ATTR(stat, CAM_RECLAIM_MODE_RO, stat_show, NULL);

static struct attribute *cam_reclaim_attrs[] = {
	&kobj_attr_enable.attr,
	&kobj_attr_event.attr,
	&kobj_attr_debug.attr,
	&kobj_attr_config.attr,
	&kobj_attr_stat.attr,
	NULL,
};

static struct attribute_group cam_reclaim_attr_group = {
	.attrs = cam_reclaim_attrs,
};

static struct kobject *cam_reclaim_sysfs_create(void)
{
	int err;
	struct kobject *kobj = NULL;
	kobj = kobject_create_and_add("cam_reclaim", kernel_kobj);
	if (!kobj) {
		pr_err("failed to create mi reclaim node.\n");
		return NULL;
	}
	err = sysfs_create_group(kobj, &cam_reclaim_attr_group);
	if (err) {
		pr_err("failed to create mi reclaim attrs.\n");
		kobject_put(kobj);
		return NULL;
	}
	return kobj;
}

static void cam_reclaim_sysfs_destory(struct kobject *kobj)
{
	if (!kobj)
		return;
	kobject_put(kobj);
}

static void cam_reclaim_setup(void)
{
	struct sysinfo i;
	pg_cam_reclaim = &g_cam_reclaim;

	si_meminfo(&i);

	pg_cam_reclaim->sysinfo.totalram = i.totalram;
	pg_cam_reclaim->page_reclaim.last_reclaim_time = jiffies;
	pg_cam_reclaim->page_reclaim.reclaim_swappiness = RECLAIM_SWAPPINESS;
	pg_cam_reclaim->page_reclaim.reclaim_anonprivate = 0;
	pg_cam_reclaim->page_reclaim.reclaim_type = 0;
	pg_cam_reclaim->page_reclaim.nr_reclaim = DEFAULT_ONCE_RECLAIM_PAGES;
	pg_cam_reclaim->page_reclaim.per_reclaim = 400000;
	pg_cam_reclaim->page_reclaim.once_reclaim_time_up = BACK_HOME_RECLAIM_TIME_UP;
	pg_cam_reclaim->page_reclaim.anon_up_threshold = 0;
	pg_cam_reclaim->page_reclaim.file_up_threshold = 0;
	pg_cam_reclaim->page_reclaim.event_type = CANCEL_ST;
	init_waitqueue_head(&pg_cam_reclaim->wait);
	spin_lock_init(&pg_cam_reclaim->page_reclaim.reclaim_lock);
}

static int __init cam_reclaim_init(void)
{
	cam_reclaim_setup();

	pg_cam_reclaim->kobj = cam_reclaim_sysfs_create();
	if (!pg_cam_reclaim->kobj)
		goto failed_to_create_sysfs;

	if (cam_reclaim_thread_start() < 0)
		return RET_FAIL;

	pg_cam_reclaim->switch_on = true;
	pg_cam_reclaim->debug = true;
	pr_info("cam_reclaim init ok\n");
	return RET_OK;

failed_to_create_sysfs:
	return RET_FAIL;
}

static void __exit cam_reclaim_exit(void)
{
	cam_reclaim_sysfs_destory(pg_cam_reclaim->kobj);
	pg_cam_reclaim->kobj = NULL;

	cam_reclaim_thread_stop();
}

module_init(cam_reclaim_init);
module_exit(cam_reclaim_exit);

MODULE_AUTHOR("lixiaohui<lixiaohui1@xaiomi.com>");
MODULE_DESCRIPTION("cam page reclaim");
MODULE_LICENSE("GPL");
