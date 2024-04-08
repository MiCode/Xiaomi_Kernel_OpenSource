#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/swap.h>
#include <linux/semaphore.h>
#include <linux/rwsem.h>
#include <linux/atomic.h>
#include <linux/cgroup.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sysfs.h>
#include <linux/proc_fs.h>
#include <linux/kobject.h>
#include <linux/cpu.h>
#include <linux/cred.h>
#include <linux/jiffies.h>
#include <linux/vmstat.h>
#include <linux/poll.h>
#include <linux/freezer.h>
#include <linux/kernel_stat.h>
#include <linux/tick.h>
#include <linux/version.h>
#include <trace/hooks/vmscan.h>
#include <trace/hooks/mm.h>

#include "low_free_memory_kill.h"

#define SYS_ATTR(_name, _mode, _show, _store) \
	struct kobj_attribute kobj_attr_##_name \
		= __ATTR(_name, _mode, _show, _store)

static memory_t g_memory;
static memory_t *pg_memory;
static int event_type = 0;
static DECLARE_WAIT_QUEUE_HEAD(event_wait);
static DEFINE_SPINLOCK(lmk_event_lock);

static inline unsigned long interval_time(unsigned long start)
{
	unsigned long end = jiffies;

	if (end >= start)
		return (unsigned long)(end - start);

	return (unsigned long)(end + (MAX_JIFFY_OFFSET - start) + 1);
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

static bool allow_wakeup_throttled_process(pg_data_t *pgdat)
{
	bool wmark_ok;
	int i;
	unsigned long pfmemalloc_reserve = 0;
	unsigned long free_pages = 0;

	for (i = 0; i <= ZONE_NORMAL; i++) {
		struct zone *zone;
		zone = &pgdat->node_zones[i];
		if (!managed_zone(zone))
			continue;

		if (!zone_can_reclaimable_pages(zone))
			continue;

		pfmemalloc_reserve += min_wmark_pages(zone);
		free_pages += zone_page_state(zone, NR_FREE_PAGES);
	}

	wmark_ok = free_pages > pfmemalloc_reserve;
	if (pg_memory->enable & DEBUG_ON)
		pr_warn("lfm: wmark %d, free_pages %lu pfmemalloc_reserve %lu\n",
				wmark_ok, free_pages, pfmemalloc_reserve);

	if (wmark_ok && waitqueue_active(&pgdat->pfmemalloc_wait)) {
		wake_up_all(&pgdat->pfmemalloc_wait);
		atomic_inc(&pg_memory->stat.wakeup_throttle_count);
		pr_warn("lfm: wake up:k\n");
	}

	return wmark_ok;
}

static int lmk_event_show(struct seq_file *s, void *unused)
{
	int temp_event_type;

	spin_lock(&lmk_event_lock);
	temp_event_type = event_type;
	event_type = 0;
	spin_unlock(&lmk_event_lock);

	seq_printf(s, "%d\n", temp_event_type);

	return 0;
}

static unsigned int lmk_event_poll(struct file *file, poll_table *wait)
{
	int ret = 0;

	poll_wait(file, &event_wait, wait);
	spin_lock(&lmk_event_lock);
	if (event_type > 0)
		ret = POLLIN;
	spin_unlock(&lmk_event_lock);

	return ret;
}

static int lmk_event_open(struct inode *inode, struct file *file)
{
	return single_open(file, lmk_event_show, inode->i_private);
}

static const struct proc_ops event_proc_ops = {
	.proc_open = lmk_event_open,
	.proc_poll = lmk_event_poll,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static void lmk_event_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("lowmemorykiller", 0, NULL, &event_proc_ops);
	if (!entry)
		pr_err("error creating kernel lmk event file\n");
}

void memory_alloc_handler(void *data, gfp_t gfp_mask, int order, int alloc_flags,
			int migratetype, struct page **page) {
	pg_data_t *pgdat = NODE_DATA(numa_node_id());

	if (!(pg_memory->enable & FUNC_ON))
		return;

	if (!allow_wakeup_throttled_process(pgdat)) {
		static unsigned long last_wakeup_time = 0;

		spin_lock(&lmk_event_lock);
		if (jiffies_to_msecs(interval_time(last_wakeup_time)) > MIN_INTERVAL_KILL_PROCESS) {
			event_type |= LOW_FREE_KILL;
			last_wakeup_time = jiffies;
			spin_unlock(&lmk_event_lock);
			wake_up_interruptible(&event_wait);
			atomic_inc(&pg_memory->stat.wakeup_lmkd_count);
			pr_warn("lfm: wake up:u\n");
			return;
		}
 		spin_unlock(&lmk_event_lock);
        }
}

static int vendor_hook_register(void) {
	int rc;

	rc = register_trace_android_vh_alloc_pages_reclaim_bypass(memory_alloc_handler, NULL);

	return rc;
}

static int vendor_hook_unregister(void) {
	int rc;

	rc = unregister_trace_android_vh_alloc_pages_reclaim_bypass(memory_alloc_handler, NULL);

	return rc;
}

static ssize_t enable_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "enable %d\nwakeup_throttle_count %d\nwakeup_lmkd_count %d\n",
			pg_memory->enable,
			atomic_read(&pg_memory->stat.wakeup_throttle_count),
			atomic_read(&pg_memory->stat.wakeup_lmkd_count));
}

static ssize_t enable_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t len)
{
	int enable;
	int ret;

	ret = kstrtoint(buf, 0, &enable);
	if (ret)
		return ret;

	pg_memory->enable = enable;

	return len;
}

static SYS_ATTR(enable, MODE_RW, enable_show, enable_store);

static struct attribute *attrs[] = {
	&kobj_attr_enable.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *sysfs_create(void)
{
	int err;
	struct kobject *kobj = kobject_create_and_add("memory", kernel_kobj);
	if (!kobj) {
		pr_err("failed to create mi reclaim node.\n");
		return NULL;
	}
	err = sysfs_create_group(kobj, &attr_group);
	if (err) {
		pr_err("failed to create mi reclaim attrs.\n");
		kobject_put(kobj);
		return NULL;
	}
	return kobj;
}

static void sysfs_destory(struct kobject *kobj)
{
	if (!kobj)
		return;
	kobject_put(kobj);
}

static int __init low_free_memory_kill_init(void)
{
	pg_memory = &g_memory;
	lmk_event_init();

	pg_memory->kobj = sysfs_create();
	if (!pg_memory->kobj)
		goto failed_to_create_sysfs;

	vendor_hook_register();
	pg_memory->enable = FUNC_ON;
	pr_info("low free memory killer init ok\n");
	return RET_OK;

failed_to_create_sysfs:
	return RET_FAIL;
}

static void __exit low_free_memory_kill_exit(void)
{
	sysfs_destory(pg_memory->kobj);
	pg_memory->kobj = NULL;

	vendor_hook_unregister();
}

module_init(low_free_memory_kill_init);
module_exit(low_free_memory_kill_exit);

MODULE_AUTHOR("zhangcang<zhangcang@xaiomi.com>");
MODULE_DESCRIPTION("memory");
MODULE_LICENSE("GPL");
