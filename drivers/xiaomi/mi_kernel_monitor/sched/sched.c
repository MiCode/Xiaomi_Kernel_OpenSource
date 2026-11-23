
// SPDX-License-Identifier: GPL-2.0+
/*
 * sch_monitor will show scheduler tracing info of the whole system.
 *
 * Copyright (C) 2024 Xiaomi Ltd.
 * Author: Liujie Xie <xieliujie@xiaomi.com>
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/cgroup.h>
#include <linux/tracepoint.h>
#include <linux/sched/cputime.h>
#include <trace/events/sched.h>
#include "../../../kernel/sched/sched.h"
#include "internal.h"
#include "kernel_monitor.h"

#define sch_warn(fmt, ...) \
		pr_info("<xiaomi_sched_monitor><%s> "fmt, __func__, ##__VA_ARGS__)

#define CONFIG_USE_SCHED_STAT_WAIT   1

static atomic_t irqoff_latency_cnt;
////static int rt_error_cnt = 0;
static atomic_t css_id_top;
static atomic_t css_id_fg;
static atomic_t css_id_bg;
static int sch_enable;
static int runtime_threshold = 30;

enum sch_type {
	SCH_TYPE_VIP = 0,
	SCH_TYPE_RT,
	SCH_TYPE_OT,
	SCH_TYPE_MAX
};

struct sch_config {
	char name[4];
	u32 low;    /* ms */
	u32 normal; /* ms */
	u32 high;   /* ms */
};

//////////////////////////////////////////////////////////////////////////////////////////////
#define THRESHOLD_DEFAULT		(20 * 1000 * 1000UL)
#define INVALID_PID			-1
#define INVALID_CPU			-1

#ifdef CONFIG_USE_SCHED_STAT_WAIT
#define PROBE_TRACEPOINTS	1
#else
#define PROBE_TRACEPOINTS	3
#endif

#define TASK_RUNNABLE_STATUS     0xF000000000000000
#define TASK_RUNNABLE_MASK     0xFFFFFFFFFFFFFFF

static atomic_t rt_preempt_cnt;
static atomic_t vip_preempt_cnt;
static atomic_t normal_preempt_cnt;
static atomic_t runable_preempt_cnt;
static atomic_t max_runtime_ms;

///////////////////////////////////////////////////////////////////////////////////////////////
struct sch_config sch_config[SCH_TYPE_MAX] = {
	{.name = "vip",	.low = 8,	.normal = 15,	.high = 30},
	{.name = "rt",	.low = 8,	.normal = 15,	.high = 30},
	{.name = "oth",	.low = 8,	.normal = 30,	.high = 60},
};

/* Stats counts for low,middle,high,fatal */
struct sch_entry {
	atomic_long_t min_counts;
	atomic_long_t l_counts;
	atomic_long_t m_counts;
	atomic_long_t h_counts;
	atomic_long_t f_counts;
	atomic_long_t all_costs_ms;
	atomic_long_t all_costs_us;
	atomic_long_t max_latency_ms;
};

struct sch_entry sch_entry[SCH_TYPE_MAX];
static u64 sch_sum_runtime[8][3] = {0};

extern int test_task_top_app(struct task_struct *task);
extern int waitinfo_monitor_init(struct proc_dir_entry *parent);
extern int waitinfo_monitor_exit(struct proc_dir_entry *parent);
extern int get_task_grp_idx(struct task_struct *tsk);
extern u64 lockstat_init_time;

static void clear_stats(void)
{
	int i = 0;

	for (i = 0; i < SCH_TYPE_MAX; i++) {
		atomic64_set(&sch_entry[i].min_counts, 0);
		atomic64_set(&sch_entry[i].l_counts, 0);
		atomic64_set(&sch_entry[i].m_counts, 0);
 		atomic64_set(&sch_entry[i].h_counts, 0);
 		atomic64_set(&sch_entry[i].f_counts, 0);
 		atomic64_set(&sch_entry[i].all_costs_us, 0);
 		atomic64_set(&sch_entry[i].all_costs_ms, 0);
		atomic64_set(&sch_entry[i].max_latency_ms, 0);
	}

	atomic_set(&rt_preempt_cnt, 0);
	atomic_set(&vip_preempt_cnt, 0);
	atomic_set(&normal_preempt_cnt, 0);
	atomic_set(&runable_preempt_cnt, 0);
	atomic_set(&max_runtime_ms, 0);
	atomic_set(&irqoff_latency_cnt, 0);
	memset(sch_sum_runtime, 0, sizeof(sch_sum_runtime));
}

static void update_sch_entry(int sch_type, u64 delay, struct task_struct *tsk)
{
	////u64 update_delay = (delay >> 20); /* ns -> us */
	u64 update_delay_us = (delay / 1000); /* ns -> ms */
	u64 update_delay_ms = (delay / 1000000); /* ns -> ms */

	if (update_delay_ms >= sch_config[sch_type].high) {
		atomic_long_inc(&sch_entry[sch_type].f_counts);
	}else if (update_delay_ms >= sch_config[sch_type].normal && update_delay_ms < sch_config[sch_type].high) {
		atomic_long_inc(&sch_entry[sch_type].h_counts);
	}else if (update_delay_ms >= sch_config[sch_type].low && update_delay_ms < sch_config[sch_type].normal) {
		atomic_long_inc(&sch_entry[sch_type].m_counts);
	}else if (update_delay_ms >= 1 && update_delay_ms < sch_config[sch_type].low) {
		atomic_long_inc(&sch_entry[sch_type].l_counts);
	} else{
		atomic_long_inc(&sch_entry[sch_type].min_counts);
	}

	if (update_delay_ms >= 1/*sch_config[sch_type].low*/)
	{
        atomic_long_add(update_delay_ms, &sch_entry[sch_type].all_costs_ms);
		if (update_delay_ms > atomic_long_read(&sch_entry[sch_type].max_latency_ms))
		{
			atomic_long_set(&sch_entry[sch_type].max_latency_ms, update_delay_ms);
		}
	}
	else
	{
        atomic_long_add(update_delay_us, &sch_entry[sch_type].all_costs_us);
	}
}

#ifdef CONFIG_FAIR_GROUP_SCHED
static void init_cgroup_id(void);
#endif

static void sched_delay_handler(struct task_struct *tsk, u64 delay, u64 clock)
{
	int sch_type = SCH_TYPE_OT;

	/*
	 * Skip the wrong data, see details in:
	 * 	https://xiaomi.f.mioffice.cn/docx/doxk4OJRiyDFseunVT86R95na1c
	 */
	if (mi_rt_task(tsk)) {
		sch_type = SCH_TYPE_RT;
		goto update;
	}

	if (is_mi_vip_task(tsk)) {
		sch_type = SCH_TYPE_VIP;
		goto update;
	}

update:
	if (sch_type >= 0 && sch_type < SCH_TYPE_OT)
	{
		update_sch_entry(sch_type, delay, tsk);
    }

	if (sch_type >= 0 && sch_type <= SCH_TYPE_RT && (delay >= SCHED_LAT_THRESHOLD))
	{
		record_kern_lat_event(SCHED_LAT, delay/1000000, tsk);
    }
}

#ifdef CONFIG_USE_SCHED_STAT_WAIT

static void sched_stat_wait_handler(void *unused, struct task_struct *tsk, u64 delay)
{
	struct rq *rq = NULL;
	u64 half_clock = 0;
	u64 clock = 0;

	if (is_idle_task(tsk) || is_screen_off())
	{
		return;
	}

	rq = task_rq(tsk);
	clock = rq_clock(rq);
	half_clock = clock >> 1;
	if (delay >= half_clock)
	{
		return;
	}

	sched_delay_handler(tsk, delay, clock);
}
#endif

static void trace_sched_stat_runtime_handler(void *priv, struct task_struct* task, u64 runtime)
{
	int cpu = smp_processor_id();
	int grp_id = 0;

    if (is_idle_task(task) || is_screen_off())
    {
    	return;
    }

	if (runtime > (4 * 1000 * 1000UL))
	{
		if (runtime/1000000 > atomic_read(&max_runtime_ms))
		{
			atomic_set(&max_runtime_ms, runtime/1000000);
		}
	}

    if (runtime >= (runtime_threshold*1000000))
    {
		atomic_inc(&irqoff_latency_cnt);
		if (runtime >= IRQ_OFF_LAT_THRESHOLD)
		{
            record_kern_lat_event(IRQ_OFF_LAT, runtime/1000000, task);
		    stack_trace_record_event(0, IRQ_OFF_LAT, runtime/1000000, task);
	    }
    }

    grp_id = get_task_grp_idx(task);
    sch_sum_runtime[cpu][grp_id] += runtime;
}

static void sch_enable_change(int enable)
{
	int ret;
	if (enable) {
#ifdef CONFIG_USE_SCHED_STAT_WAIT
		ret = register_trace_sched_stat_wait(sched_stat_wait_handler, NULL);
#endif
		ret = register_trace_sched_stat_runtime(trace_sched_stat_runtime_handler, NULL);
		sch_warn("register hook trace_sched_stat_wait, ret is %d\n", ret);
	} else {
#ifdef CONFIG_USE_SCHED_STAT_WAIT
		ret = unregister_trace_sched_stat_wait(sched_stat_wait_handler, NULL);
#endif
		ret = unregister_trace_sched_stat_runtime(trace_sched_stat_runtime_handler, NULL);
		sch_warn("unregister hook trace_sched_stat_wait, ret is %d\n", ret);
	}
}

static ssize_t sch_common_write(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	int *pval = (int *)pde_data(file_inode(file));
	char kbuf[4] = {0};
	int err;
	if (pval == NULL)
		goto out;
	memset(kbuf, 0, sizeof(kbuf));
	if (count > sizeof(kbuf) - 1)
		count = sizeof(kbuf) - 1;
	if (copy_from_user(kbuf, buf, count)) {
		sch_warn("failed to do copy_from_user\n");
		return -EFAULT;
	}
	err = kstrtoint(strstrip(kbuf), 0, pval);
	if (err < 0) {
		sch_warn("failed to do kstrtoint\n");
		return -EFAULT;
	}
	if (pval == &sch_enable)
		sch_enable_change(*pval);
out:
	return count;
}

static int sch_common_show(struct seq_file *m, void *v)
{
	u64 min_counts, l_counts, m_counts, h_counts, f_counts, all_costs_ms, all_costs_us;
	u64 total_ms, avg_ms, total_us, avg_us, max_ms;
	int i;
	if (m->private == &sch_enable) {
		seq_printf(m, "%d\n", *(int*) m->private);
		goto out;
	}
	/* m->private is NULL */
	for (i = 0; i < SCH_TYPE_MAX; ++i) {
		struct sch_entry entry = sch_entry[i];
		char str[32];
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
		snprintf(str, sizeof(str), "%s_lat_min_cnt", sch_config[i].name);
		seq_printf(m, "%-20s: %llu\n", str, min_counts);	
		snprintf(str, sizeof(str), "%s_lat_l_cnt", sch_config[i].name);
		seq_printf(m, "%-20s: %llu\n", str, l_counts);
		snprintf(str, sizeof(str), "%s_lat_m_cnt", sch_config[i].name);
		seq_printf(m, "%-20s: %llu\n", str, m_counts);
		snprintf(str, sizeof(str), "%s_lat_h_cnt", sch_config[i].name);
		seq_printf(m, "%-20s: %llu\n", str, h_counts);
		snprintf(str, sizeof(str), "%s_lat_f_cnt", sch_config[i].name);
		seq_printf(m, "%-20s: %llu\n", str, f_counts);
		snprintf(str, sizeof(str), "%s_lat_avg_%s", sch_config[i].name, "ms");
		seq_printf(m, "%-20s: %llu\n", str, avg_ms);
		snprintf(str, sizeof(str), "%s_lat_avg_%s", sch_config[i].name, "us");
		seq_printf(m, "%-20s: %llu\n", str, avg_us);		
		snprintf(str, sizeof(str), "%s_lat_max_%s", sch_config[i].name, "ms");
		seq_printf(m, "%-20s: %llu\n", str, max_ms);		
	}
	
	seq_printf(m, "%-20s: %d\n", "irqoff_latency_cnt", atomic_read(&irqoff_latency_cnt));
	seq_printf(m, "%-20s: %d\n", "max_runtime_ms", atomic_read(&max_runtime_ms));
	seq_printf(m, "%-20s: %d\n", "runable_preempt_cnt", atomic_read(&runable_preempt_cnt));
	seq_printf(m, "%-20s: %d\n", "vip_preempt_cnt", atomic_read(&vip_preempt_cnt));
	seq_printf(m, "%-20s: %d\n", "normal_preempt_cnt", atomic_read(&normal_preempt_cnt));
	seq_printf(m, "%-20s: %d\n", "rt_preempt_cnt", atomic_read(&rt_preempt_cnt));

out:
	return 0;
}

static int runtime_threshold_show(struct seq_file *m, void *ptr)
{
	seq_printf(m, "%d\n", runtime_threshold);

	return 0;
}

static ssize_t runtime_threshold_store(void *priv, const char __user *buf, size_t count)
{
	int val;

	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	runtime_threshold = val;

	return count;
}

DEFINE_PROC_ATTRIBUTE_RW(runtime_threshold);

static int sch_common_open(struct inode *inode, struct file *file)
{
	return single_open(file, sch_common_show, pde_data(inode));
}

static const struct proc_ops sch_common_proc_ops = {
	.proc_open		= sch_common_open,
	.proc_write		= sch_common_write,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};

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

static int cpu_runtime_show(struct seq_file *m, void *v)
{
	int i  = 0;
	u64 total_runtime[3] = {0};

	for (i = 0; i < 8; ++i) {
		char str[8];
		snprintf(str, sizeof(str), "cpu[%d]", i);
		seq_printf(m, "%s: vip_sum_runtime:%llu ms, rt_sum_runtime=%llu ms, ot_sum_runtime=%llu ms\n", 
			str, sch_sum_runtime[i][0]/1000000, sch_sum_runtime[i][1]/1000000, sch_sum_runtime[i][2]/1000000);
		total_runtime[0] += sch_sum_runtime[i][0];
		total_runtime[1] += sch_sum_runtime[i][1];
		total_runtime[2] += sch_sum_runtime[i][2];
	}

	seq_printf(m, "total: vip:%llu ms, rt=%llu ms, ot=%llu ms, period=%llu ms\n", 
		total_runtime[0]/1000000, total_runtime[1]/1000000, total_runtime[2]/1000000, (local_clock() - lockstat_init_time)/1000000);

	return 0;
}

static int cpu_runtime_open(struct inode* inode, struct file* file)
{
    return single_open(file, cpu_runtime_show, inode->i_private);
}

static const struct proc_ops cpu_runtime_fops = {
	.proc_open		= cpu_runtime_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};

#ifdef CONFIG_FAIR_GROUP_SCHED
static void init_cgroup_id(void)
{
	struct cgroup_subsys_state *css = &root_task_group.css;
	struct cgroup_subsys_state *top_css = css;
	if (css->cgroup && (css->cgroup->level == 0)) {
		sch_warn("root group, css->id=%d (expected 1)\n", css->id);
	}
	rcu_read_lock();
	css_for_each_child(css, top_css) {
		sch_warn("traversal css, level is %d, name is %-12s, id is %d\n",
				css->cgroup->level, css->cgroup->kn->name,
				css->id);
		if (!strcmp(css->cgroup->kn->name, "top-app"))
			atomic_set(&css_id_top, css->id);
		else if (!strcmp(css->cgroup->kn->name, "foreground"))
			atomic_set(&css_id_fg, css->id);
		else if (!strcmp(css->cgroup->kn->name, "background"))
			atomic_set(&css_id_bg, css->id);
	}
	rcu_read_unlock();
	sch_warn("traversal css end, top is %d, fg is %d, bg is %d\n",
			atomic_read(&css_id_top), atomic_read(&css_id_fg),
			atomic_read(&css_id_bg));
}
#else
static inline void init_cgroup_id(void) {}
#endif

static struct proc_dir_entry *sch_dir;
static void init_procs(void)
{
	sch_dir = xiaomi_proc_mkdir(PROC_TRACE_SCHED, NULL);
	if (!sch_dir) {
		printk("huqinghe failed to create dir: proc/sch_monitor\n");
		return;
	}

	proc_create_data("lat_info", S_IRUGO, sch_dir, &sch_common_proc_ops, NULL);
	proc_create_data("enable", S_IRUGO | S_IWUGO, sch_dir, &sch_common_proc_ops, &sch_enable);
	proc_create_data("runtime_threshold", 0666, sch_dir, &runtime_threshold_fops, NULL);
	proc_create_data("reset", 0666, sch_dir, &reset_fops, NULL);
	proc_create("cpu_runtime", 0666, sch_dir, &cpu_runtime_fops);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct xm_tracepoints_probe {
	struct tracepoint *tps[PROBE_TRACEPOINTS];
	const char *tp_names[PROBE_TRACEPOINTS];
	void *tp_probes[PROBE_TRACEPOINTS];
	void *priv;
	int num_initalized;
};

struct xm_task_entry {
	u64 latency;
	pid_t pid;
	int prio;
	int cpu;
	char comm[TASK_COMM_LEN];
	char parent_comm[TASK_COMM_LEN];
};

struct xm_runqlat_info {
	int cpu;		/* The target CPU */
	pid_t pid;		/* Trace this pid only */

	u64 rq_start;
	u64 run_start;
	u64 threshold;

	unsigned int nr_task;
	struct xm_task_entry *task_entries;

	raw_spinlock_t lock;
};

static struct xm_runqlat_info xm_runqlat_info = {
	.pid		= INVALID_PID,
	.cpu		= INVALID_CPU,
	.threshold	= THRESHOLD_DEFAULT,
};

static void preempt_cnt_stats(struct task_struct *prev, struct task_struct *next, int cpu)
{
	unsigned long prev_state;

	if (is_idle_task(prev))
	{
		return;
	}

	prev_state = READ_ONCE(prev->__state);
	if (prev_state != TASK_RUNNING)
	{
		return;
	}

	atomic_inc(&runable_preempt_cnt);

	if (mi_rt_task(prev) && mi_rt_task(next))
	{
		atomic_inc(&rt_preempt_cnt);
		return;
	}

	if (is_mi_vip_task(prev) && mi_rt_task(next))
	{
		atomic_inc(&vip_preempt_cnt);
		return;
	}

	if (is_mi_vip_task(prev) && mi_normal_task(next))
	{
		atomic_inc(&normal_preempt_cnt);
		return;
	}
}

extern u64 lockstat_init_time;
#define MI_TASK_SCHED_CLOCK_SET	0xabcdabcdacbdadbd
/* interrupts should be off from __schedule() */
static void probe_sched_switch(void *priv, bool preempt, struct task_struct *prev, struct task_struct *next, unsigned int pre_state)
{
	int cpu = smp_processor_id();
    preempt_cnt_stats(prev, next, cpu);
}

static struct xm_tracepoints_probe tps_probe = {
	.tp_names = {
		"sched_switch",
	},
	.tp_probes = {
		probe_sched_switch,
	},
	.priv = &xm_runqlat_info,
};

static inline bool is_tracepoint_lookup_success(struct xm_tracepoints_probe *tps)
{
	return tps->num_initalized == PROBE_TRACEPOINTS;
}

static void tracepoint_lookup(struct tracepoint *tp, void *priv)
{
	int i;
	struct xm_tracepoints_probe *tps = priv;

	if (is_tracepoint_lookup_success(tps))
		return;

	for (i = 0; i < ARRAY_SIZE(tps->tp_names); i++) {
		if (tps->tps[i] || strcmp(tp->name, tps->tp_names[i]))
			continue;
		tps->tps[i] = tp;
		tps->num_initalized++;
	}
}

static int trace_runq_preempt_init(void)
{
	int i = 0;
	int ret = -1;
	struct xm_tracepoints_probe *tps = &tps_probe;

	/* Lookup for the tracepoint that we needed */
	for_each_kernel_tracepoint(tracepoint_lookup, tps);

	if (!is_tracepoint_lookup_success(tps))
		return ret;

	for (i = 0; i < PROBE_TRACEPOINTS; i++) {
		ret = tracepoint_probe_register(tps->tps[i], tps->tp_probes[i],
						tps->priv);
		if (ret) {
			pr_err("sched trace: can not activate tracepoint "
			       "probe to %s\n", tps->tp_names[i]);
			while (i--)
				tracepoint_probe_unregister(tps->tps[i],
							    tps->tp_probes[i],
							    tps->priv);
			return ret;
		}
	}

	return 0;
}

static void  trace_runq_preempt_exit(void)
{
	int i;
	struct xm_tracepoints_probe *tps = &tps_probe;

	for (i = 0; i < PROBE_TRACEPOINTS; i++)
		tracepoint_probe_unregister(tps->tps[i], tps->tp_probes[i],
					    tps->priv);

	tracepoint_synchronize_unregister();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
int xiaomi_sched_trace_init(void)
{
	int ret = 0;
	init_cgroup_id();
	init_procs();
	waitinfo_monitor_init(sch_dir);
	sch_enable = 1;
	sch_enable_change(sch_enable);
	ret = trace_runq_preempt_init();

	return ret;
}

void xiaomi_sched_trace_exit(void)
{
	waitinfo_monitor_exit(sch_dir);
	if (sch_dir) {
		remove_proc_entry("lat_info", sch_dir);
		remove_proc_entry("enable", sch_dir);
		remove_proc_entry("runtime_threshold", sch_dir);
		remove_proc_entry("reset", sch_dir);
		remove_proc_entry("cpu_runtime", sch_dir);
		remove_proc_subtree(PROC_TRACE_SCHED, NULL);
		sch_dir = NULL;
	}
	
	sch_enable = 0;
	sch_enable_change(sch_enable);
	trace_runq_preempt_exit();

	return;
}