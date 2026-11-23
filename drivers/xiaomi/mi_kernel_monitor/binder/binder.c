
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <../drivers/android/binder_internal.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <trace/hooks/binder.h>
#include <trace/hooks/sched.h>
#include <linux/random.h>
#include <linux/of.h>
#include <linux/timekeeping.h>
#include <linux/ktime.h>
#include "internal.h"

enum lat_thres_type
{
	LAT_MIN_THRES=0,
    LAT_LOW_THRES,
	LAT_MID_THRES,
    LAT_HIGH_THRES,
    LAT_FATAL_THRES,
    LAT_MAX_THRES,
};

struct binder_config {
	char name[4];
	u32 min;
	u32 low;    /* ms */
	u32 normal; /* ms */
	u32 high;   /* ms */
};

/* Stats counts for low,middle,high,fatal */
struct binder_entry {
	atomic_long_t min_counts;
	atomic_long_t l_counts;
	atomic_long_t m_counts;
	atomic_long_t h_counts;
	atomic_long_t f_counts;
	atomic_long_t all_costs_ms;
	atomic_long_t all_costs_us;
	atomic_long_t max_latency_ms;
};

struct binder_config binder_config[GRP_TYPES] = {
	{.name = "vip",	.min=1,  .low = 8,	.normal = 15,	.high = 30},
	{.name = "rt",	.min=1,  .low = 8,	.normal = 15,	.high = 30},
	{.name = "oth",	.min=1,	 .low = 8,	.normal = 15,	.high = 30},
};

struct binder_entry binder_entry[GRP_TYPES];
static unsigned int binder_enable = 0;

extern int get_task_grp_idx(struct task_struct* task);
static struct proc_dir_entry *binder_dir;

static inline bool binder_is_sync_mode(u32 flags)
{
	return !(flags & TF_ONE_WAY);
}

static void clear_stats(void)
{
	int i = 0;

	for (i = 0; i < GRP_TYPES; i++) {
		atomic64_set(&binder_entry[i].min_counts, 0);
		atomic64_set(&binder_entry[i].l_counts, 0);
		atomic64_set(&binder_entry[i].m_counts, 0);
		atomic64_set(&binder_entry[i].h_counts, 0);
 		atomic64_set(&binder_entry[i].f_counts, 0);
 		atomic64_set(&binder_entry[i].all_costs_us, 0);
 		atomic64_set(&binder_entry[i].all_costs_ms, 0);
		atomic64_set(&binder_entry[i].max_latency_ms, 0);
	}
}

static void update_binder_entry(int binder_type, u64 delay)
{
	u64 update_delay_us = delay; /* us -> us */
	u64 update_delay_ms = (delay / 1000); /* ns -> ms */

	if (update_delay_ms >= binder_config[binder_type].high) {
		atomic_long_inc(&binder_entry[binder_type].f_counts);
	}else if (update_delay_ms >= binder_config[binder_type].normal && update_delay_ms < binder_config[binder_type].high) {
		atomic_long_inc(&binder_entry[binder_type].h_counts);
	}else if (update_delay_ms >= binder_config[binder_type].low && update_delay_ms < binder_config[binder_type].normal) {
		atomic_long_inc(&binder_entry[binder_type].m_counts);
	}else if (update_delay_ms >= binder_config[binder_type].min && update_delay_ms < binder_config[binder_type].low) {
		atomic_long_inc(&binder_entry[binder_type].l_counts);
	} else{
		atomic_long_inc(&binder_entry[binder_type].min_counts);
	}

	if (update_delay_ms >= binder_config[binder_type].min)
	{
        atomic_long_add(update_delay_ms, &binder_entry[binder_type].all_costs_ms);
		if (update_delay_ms > atomic_long_read(&binder_entry[binder_type].max_latency_ms))
		{
			atomic_long_set(&binder_entry[binder_type].max_latency_ms, update_delay_ms);
		}
	}
	else
	{
        atomic_long_add(update_delay_us, &binder_entry[binder_type].all_costs_us);
	}
}

static int handle_wait_stats(struct binder_transaction *t, u64 delta)
{
	int grp_idx = 0;
	struct task_struct* task = NULL;
	bool oneway = !!(t->flags & TF_ONE_WAY);
	
	if (oneway)
	{
	    return -1;
	}

	if (t->from == NULL)
	{
	    return -1;
	}

	task = t->from->task;    
	grp_idx = get_task_grp_idx(task);
	if (grp_idx == -1)
	    return -1;

	if (is_critical_ux_task(task))
    {
        if (delta >= BINDER_LAT_THRESHOLD/1000)
        {
            record_kern_lat_event(BINDER_LAT, delta/1000, task);
        }
	}

	update_binder_entry(grp_idx, delta);

	return 0;
}

static void android_vh_binder_transaction_received_handler(void *unused,
	struct binder_transaction *t, struct binder_proc *proc, struct binder_thread *thread, uint32_t cmd)
{

	ktime_t current_time = ktime_get();
	u64 delta = 0;

	if (t == NULL)
	{
		return;
	}

	delta = ktime_us_delta(current_time, t->start_time);
	handle_wait_stats(t, delta);
}
/*******************************************************************vendor_hook7 end*****************************************************************************/

void register_binder_sched_vendor_hooks(void)
{
	if (binder_enable == 1)
	{
	    register_trace_android_vh_binder_transaction_received(android_vh_binder_transaction_received_handler, NULL);
	    ////register_trace_android_vh_binder_transaction_init(android_vh_binder_transaction_init_handler, NULL);
    }
}

void unregister_binder_sched_vendor_hooks(void)
{
	if (binder_enable == 0)
	{
	    unregister_trace_android_vh_binder_transaction_received(android_vh_binder_transaction_received_handler, NULL);
	    ////unregister_trace_android_vh_binder_transaction_init(android_vh_binder_transaction_init_handler, NULL);
    }
}

void xiaomi_binder_sched_init(void)
{
	register_binder_sched_vendor_hooks();
	pr_info("huqinghe %s\n", __func__);
}

void xiaomi_binder_sched_exit(void)
{
	unregister_binder_sched_vendor_hooks();
	pr_info("huqinghe %s\n", __func__);
}

static int lat_info_show(struct seq_file *m, void *v)
{
	u64 min_counts, l_counts, m_counts, h_counts, f_counts, all_costs_ms, all_costs_us;
	u64 total_ms, avg_ms, total_us, avg_us, max_ms;
	int i;

	/* m->private is NULL */
	for (i = 0; i < GRP_TYPES; ++i) {
		struct binder_entry entry = binder_entry[i];
		char str[32] = {0};
		min_counts  = atomic_long_read(&entry.min_counts);
		l_counts  = atomic_long_read(&entry.l_counts);
		m_counts  = atomic_long_read(&entry.m_counts);
		h_counts  = atomic_long_read(&entry.h_counts);
		f_counts  = atomic_long_read(&entry.f_counts);
		all_costs_ms = atomic_long_read(&entry.all_costs_ms);
		all_costs_us = atomic_long_read(&entry.all_costs_us);
		max_ms = atomic_long_read(&entry.max_latency_ms);
		total_ms = l_counts + m_counts + h_counts + f_counts;
		total_us = min_counts + l_counts + m_counts + h_counts + f_counts;
		avg_ms = total_ms ? (all_costs_ms / total_ms) : 0;
		avg_us = total_us ? (all_costs_us / total_us) : 0;
		snprintf(str, sizeof(str), "%s_binder_min_cnt", binder_config[i].name);
		seq_printf(m, "%-20s: %llu\n", str, min_counts);
		snprintf(str, sizeof(str), "%s_binder_l_cnt", binder_config[i].name);
		seq_printf(m, "%-20s: %llu\n", str, l_counts);
		snprintf(str, sizeof(str), "%s_binder_m_cnt", binder_config[i].name);
		seq_printf(m, "%-20s: %llu\n", str, m_counts);
		snprintf(str, sizeof(str), "%s_binder_h_cnt", binder_config[i].name);
		seq_printf(m, "%-20s: %llu\n", str, h_counts);
		snprintf(str, sizeof(str), "%s_binder_f_cnt", binder_config[i].name);
		seq_printf(m, "%-20s: %llu\n", str, f_counts);
		snprintf(str, sizeof(str), "%s_binder_avg_%s", binder_config[i].name, "ms");
		seq_printf(m, "%-20s: %llu\n", str, avg_ms);
		snprintf(str, sizeof(str), "%s_binder_avg_%s", binder_config[i].name, "us");
		seq_printf(m, "%-20s: %llu\n", str, avg_us);
		snprintf(str, sizeof(str), "%s_binder_max_%s", binder_config[i].name, "ms");
		seq_printf(m, "%-20s: %llu\n", str, max_ms);
	}

	return 0;
}

static int lat_info_open(struct inode* inode, struct file* file)
{
    return single_open(file, lat_info_show, inode->i_private);
}

static const struct proc_ops lat_info_fops = {
	.proc_open		= lat_info_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};

/*******************************binder enable********************************/
static int binder_enable_show(struct seq_file *m, void *ptr)
{
	seq_printf(m,"%s\n",  (binder_enable == 1)?"Enabled":"Disabled");
	return 0;
}

static ssize_t binder_enable_store(void *priv, const char __user *buf, size_t count)
{
	int val;
	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	if (val) {
	    register_trace_android_vh_binder_transaction_received(android_vh_binder_transaction_received_handler, NULL);
	    ////register_trace_android_vh_binder_transaction_init(android_vh_binder_transaction_init_handler, NULL);
		binder_enable = 1;
	} 
	else {
	    unregister_trace_android_vh_binder_transaction_received(android_vh_binder_transaction_received_handler, NULL);
	    ////unregister_trace_android_vh_binder_transaction_init(android_vh_binder_transaction_init_handler, NULL);
		binder_enable = 0;
	}

	return count;
}
DEFINE_PROC_ATTRIBUTE_RW(binder_enable);

/*******************************reset enable********************************/

static int reset_show(struct seq_file *m, void *ptr)
{
	seq_printf(m,"%s\n", "Data Reset");
	return 0;
}

static ssize_t reset_store(void *priv, const char __user *buf, size_t count)
{
	int val;
	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	if (val) 
	{
		clear_stats();
	}

	return count;
}
DEFINE_PROC_ATTRIBUTE_RW(reset);

int xiaomi_binder_sysfs_init(void)
{
	struct proc_dir_entry *proc_node;

	binder_dir = xiaomi_proc_mkdir(PROC_TRACE_BINDER, NULL);
	if (!binder_dir) {
		pr_err("huqinghe failed to create proc dir xiaomi_binder\n");
		return -ENOENT;
	}

	proc_node = proc_create("lat_info", 0666, binder_dir, &lat_info_fops);
	if (!proc_node) {
		pr_err("huqinghe failed to create proc lat_info\n");
		remove_proc_subtree(PROC_TRACE_BINDER, NULL);
		return -ENOENT;
	}
			
	proc_node = proc_create("enable", S_IRUGO | S_IWUGO, binder_dir, &binder_enable_fops);
	if (!proc_node) {
		pr_err("huqinghe failed to create proc node enable\n");
		remove_proc_subtree(PROC_TRACE_BINDER, NULL);
		return -ENOENT;
	}			

	proc_node = proc_create("reset", S_IRUGO | S_IWUGO, binder_dir, &reset_fops);
	if (!proc_node) {
		pr_err("huqinghe failed to create proc node reset\n");
		remove_proc_subtree(PROC_TRACE_BINDER, NULL);
		return -ENOENT;
	}

	pr_info("%s success\n", __func__);
	return 0;
}

int xiaomi_binder_trace_init(void)
{
	int ret = 0;

    binder_enable = 1;
    xiaomi_binder_sysfs_init();
	xiaomi_binder_sched_init();

	return ret;
}

int xiaomi_binder_trace_exit(void)
{
	int ret = 0;

    binder_enable = 0;
	remove_proc_entry("lat_info", binder_dir);
	remove_proc_entry("enable", binder_dir);
	remove_proc_entry("reset", binder_dir);
    remove_proc_subtree(PROC_TRACE_BINDER, NULL);
	xiaomi_binder_sched_exit();

	return ret;
}
module_param_named(binder_sched_enable, binder_enable, uint, 0660);


