// SPDX-License-Identifier: GPL-2.0
/*
 * cpu_load_monitor.c - CPU load monitor
 *
 * Copyright (c) 2026 Xiaomi, Inc., All rights reserved.
 */

#include <linux/cpumask.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/tick.h>

#include <kernel/sched/sched.h>
#include "cpu_netlink.h"
#include "mimon_trace.h"

/*
 * Thresholds and timing constants
 */
#define HIGHLOAD_VALUE			98
#define HIGHLOAD_PERCENT		100
#define HIGHLOAD_TIMER_RATE		5
#define HIGHLOAD_HIGH_THRESHOLD		4
#define HIGHLOAD_LOW_THRESHOLD		0

#define HIGHLOAD_DETECT_PERIOD		1000000		/* us */
#define HIGHLOAD_PROC_DURING		4000000		/* us */
#define HIGHLOAD_PROC_PERIOD		400000		/* us */

#define HIGHLOAD_BIG_HIGHCYCLE		6
#define HIGHLOAD_BIG_LOWCYCLE		3

#define HIGHLOAD_CPUSET_ROOT_UP		95
#define HIGHLOAD_CPUSET_ROOT_DOWN	60
#define HIGHLOAD_CPUSET_BG_UP		95
#define HIGHLOAD_CPUSET_BG_DOWN		60

#define HIGHLOAD_MAX_PIDS		16

#define HIGHLOAD_PROC_THRESHOLD_MAX	1024
#define HIGHLOAD_PROC_THRESHOLD_ROOT	4
#define HIGHLOAD_PROC_THRESHOLD_BG	5

#define HIGHLOAD_SYSFS_MODE		0600
#define HIGHLOAD_SYSFS_BUF_LEN		10

/*
 * Types
 */
enum highload_level {
	LOAD_LOW	= 1,
	LOAD_HIGH	= 2,
};

enum highload_type {
	LOAD_ALL	= 0,
	LOAD_CPUSET_ROOT,
	LOAD_CPUSET_BG,
	NR_LOAD_TYPES
};

enum highload_switch_bit {
	LOAD_SWITCH_DEFAULT	= 0,
	LOAD_SWITCH_ROOT,
	LOAD_SWITCH_BG,
};

struct highload_data {
	int	cmd;
	int	pid[HIGHLOAD_MAX_PIDS];
};

struct highload_action {
	u64	bits_high;
	u64	bits_type;
};

struct highload_pid_node {
	struct rb_node	node;
	pid_t		pid;
	int		count;
};

struct highload_pid_mgr {
	int			idx;
	int			nr_max;
	struct highload_pid_node *pool;
	struct rb_root		root;
	struct highload_data	data;
};

/*
 * Static data
 */
static int highload_nr_intervals;
static int highload_proc_elapsed;
static int highload_cnt[NR_LOAD_TYPES];
static int highload_last_status[NR_LOAD_TYPES];
static int highload_cycle_high_cnt[NR_LOAD_TYPES];
static int highload_cycle_normal_cnt[NR_LOAD_TYPES];
static int highload_cycle_count[NR_LOAD_TYPES];

static unsigned long highload_switch;
static struct delayed_work highload_detect_work;
static struct delayed_work highload_proc_work;
static unsigned long highload_cpumask_bg = 0x0000000cUL;
static unsigned long highload_cpumask_root = 0x000000ffUL;

static struct highload_action highload_action_bits;
static struct highload_pid_mgr highload_pid_mgr[NR_LOAD_TYPES];

static pid_t highload_top_pids[NR_LOAD_TYPES][HIGHLOAD_MAX_PIDS];
static int highload_top_pid_cnt[NR_LOAD_TYPES];

static u64 per_cpu_total[CONFIG_NR_CPUS];
static u64 per_cpu_busy[CONFIG_NR_CPUS];
static int per_cpu_online[CONFIG_NR_CPUS];

static struct kobject *highload_kobj;

/* ---- Netlink notification helpers ---- */

static inline void highload_action_set(u32 type, bool is_high)
{
	highload_action_bits.bits_type |= BIT(type);

	if (is_high)
		highload_action_bits.bits_high |= BIT(type);
}

static void highload_notify_high(u32 type, int nr, const int *pids)
{
	struct highload_data msg;
	int i;

	memset(&msg, 0, sizeof(msg));
	msg.cmd = (type << 1) - 1;

	if (nr > HIGHLOAD_MAX_PIDS)
		nr = HIGHLOAD_MAX_PIDS;

	for (i = 0; i < nr; i++)
		msg.pid[i] = pids[i];

	send_to_user(PROC_LOAD, sizeof(msg) / sizeof(int), (int *)&msg);
}

static void highload_notify_low(u32 type)
{
	struct highload_data msg;

	memset(&msg, 0, sizeof(msg));
	msg.cmd = (type << 1);
	send_to_user(PROC_LOAD, sizeof(msg) / sizeof(int), (int *)&msg);
}

static int highload_notify_netlink(int data)
{
	int dt[] = { data };

	return send_to_user(CPU_HIGH_LOAD, 1, dt);
}

/* ---- PID statistics rb-tree management ---- */

static struct highload_pid_node *highload_pid_alloc(u32 type)
{
	struct highload_pid_mgr *mgr = &highload_pid_mgr[type];

	if (mgr->idx >= mgr->nr_max - 1)
		return NULL;

	return &mgr->pool[mgr->idx++];
}

static void highload_pid_reset(u32 type)
{
	struct highload_pid_mgr *mgr;

	if (type >= NR_LOAD_TYPES)
		return;

	mgr = &highload_pid_mgr[type];
	memset(mgr->pool, 0, mgr->idx * sizeof(*mgr->pool));
	memset(&mgr->data, 0, sizeof(mgr->data));
	mgr->idx = 0;
	mgr->root = RB_ROOT;
}

static void highload_pid_insert(pid_t key, u32 type)
{
	struct highload_pid_mgr *mgr = &highload_pid_mgr[type];
	struct rb_node **link = &mgr->root.rb_node;
	struct rb_node *parent = NULL;
	struct highload_pid_node *entry;
	struct highload_pid_node *new;

	while (*link) {
		entry = container_of(*link, struct highload_pid_node, node);
		parent = *link;

		if (key < entry->pid) {
			link = &(*link)->rb_left;
		} else if (key > entry->pid) {
			link = &(*link)->rb_right;
		} else {
			entry->count++;
			return;
		}
	}

	new = highload_pid_alloc(type);
	if (!new)
		return;

	new->pid = key;
	new->count = 1;
	rb_link_node(&new->node, parent, link);
	rb_insert_color(&new->node, &mgr->root);
}

static int highload_pid_mgr_init(void)
{
	int i;

	for (i = 0; i < NR_LOAD_TYPES; i++) {
		struct highload_pid_mgr *mgr = &highload_pid_mgr[i];

		mgr->idx = 0;
		mgr->nr_max = (HIGHLOAD_PROC_DURING / HIGHLOAD_PROC_PERIOD)
				* CONFIG_NR_CPUS;
		mgr->pool = kmalloc_array(mgr->nr_max, sizeof(*mgr->pool),
					  GFP_KERNEL);
		if (!mgr->pool) {
			while (--i >= 0)
				kfree(highload_pid_mgr[i].pool);
			return -ENOMEM;
		}

		mgr->root = RB_ROOT;
		memset(mgr->pool, 0, mgr->nr_max * sizeof(*mgr->pool));
		memset(&mgr->data, 0, sizeof(mgr->data));
	}

	pr_info("cpuload: pid manager init ok\n");
	return 0;
}

static void highload_pid_mgr_free(void)
{
	int i;

	for (i = 0; i < NR_LOAD_TYPES; i++) {
		kfree(highload_pid_mgr[i].pool);
		memset(&highload_pid_mgr[i], 0, sizeof(highload_pid_mgr[i]));
	}
}

/* ---- CPU time accounting ---- */

static u64 highload_get_idle_time(int cpu)
{
	u64 idle_time = -1ULL;

	if (cpu_online(cpu))
		idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		return kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];

	return idle_time * NSEC_PER_USEC;
}

static u64 highload_get_iowait_time(int cpu)
{
	u64 iowait_time = -1ULL;

	if (cpu_online(cpu))
		iowait_time = get_cpu_iowait_time_us(cpu, NULL);

	if (iowait_time == -1ULL)
		return kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];

	return iowait_time * NSEC_PER_USEC;
}

static void highload_snapshot_cpu_time(void)
{
	int i = 0;
	u64 total, busy;

	memset(per_cpu_online, 0, sizeof(per_cpu_online));

	for_each_online_cpu(i) {
		if (i >= CONFIG_NR_CPUS || i < 0)
			break;

		busy  = kcpustat_cpu(i).cpustat[CPUTIME_USER];
		busy += kcpustat_cpu(i).cpustat[CPUTIME_NICE];
		busy += kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];

		total  = busy;
		total += highload_get_idle_time(i);
		total += highload_get_iowait_time(i);
		total += kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
		total += kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];
		total += kcpustat_cpu(i).cpustat[CPUTIME_STEAL];
		total += kcpustat_cpu(i).cpustat[CPUTIME_GUEST];
		total += kcpustat_cpu(i).cpustat[CPUTIME_GUEST_NICE];

		per_cpu_total[i] = total;
		per_cpu_busy[i] = busy;
		per_cpu_online[i] = 1;
	}
}

static void highload_calc_load(u64 *total, u64 *busy, unsigned long mask)
{
	u32 i;

	for (i = 0; i < CONFIG_NR_CPUS; i++) {
		if (!per_cpu_online[i])
			continue;
		if (!(BIT(i) & mask))
			continue;

		*total += per_cpu_total[i];
		*busy += per_cpu_busy[i];
	}
}

/* ---- Type-to-config mapping helpers ---- */

static unsigned long highload_type_to_mask(u32 type)
{
	switch (type) {
	case LOAD_CPUSET_ROOT:
		return highload_cpumask_root;
	case LOAD_CPUSET_BG:
		return highload_cpumask_bg;
	default:
		return 0;
	}
}

static int highload_type_to_threshold(u32 type)
{
	switch (type) {
	case LOAD_CPUSET_ROOT:
		return HIGHLOAD_PROC_THRESHOLD_ROOT;
	case LOAD_CPUSET_BG:
		return HIGHLOAD_PROC_THRESHOLD_BG;
	default:
		return HIGHLOAD_PROC_THRESHOLD_MAX;
	}
}

/* ---- Load detection logic ---- */

static void highload_count_reset(void)
{
	highload_nr_intervals = 0;
	highload_cnt[LOAD_ALL] = 0;
	highload_cnt[LOAD_CPUSET_ROOT] = 0;
	highload_cnt[LOAD_CPUSET_BG] = 0;
}

static void highload_cycle_reset(u32 type)
{
	highload_cycle_high_cnt[type] = 0;
	highload_cycle_normal_cnt[type] = 0;
	highload_cycle_count[type] = 0;
}

static bool highload_cycle_check_high(u32 type)
{
	bool ret = false;

	if (highload_last_status[type] == LOAD_HIGH &&
	    highload_cycle_high_cnt[type] >
	    HIGHLOAD_BIG_HIGHCYCLE * HIGHLOAD_HIGH_THRESHOLD) {
		pr_info("cpuload: cycle sustained HIGH_LOAD\n");
		highload_action_set(type, true);
		ret = true;
	}

	highload_cycle_reset(type);
	return ret;
}

static bool highload_cycle_check_low(u32 type)
{
	if (highload_last_status[type] != LOAD_HIGH)
		return false;

	if (highload_cycle_normal_cnt[type] >
	    HIGHLOAD_BIG_LOWCYCLE * HIGHLOAD_HIGH_THRESHOLD) {
		pr_info("cpuload: cycle sustained LOW_LOAD\n");
		highload_last_status[type] = LOAD_LOW;
		highload_action_set(type, false);
		highload_cycle_reset(type);
		return true;
	}

	return false;
}

static bool highload_check_all(void)
{
	static int last_report = LOAD_LOW;
	static u64 last_total;
	static u64 last_busy;
	u64 total = 0, busy = 0, delta;
	bool reported = false;

	highload_calc_load(&total, &busy, 0xffffffffUL);
	delta = total - last_total;

	if (delta) {
		if ((busy - last_busy) * HIGHLOAD_PERCENT >=
		    HIGHLOAD_VALUE * delta)
			highload_cnt[LOAD_ALL]++;
	}

	if (highload_nr_intervals >= HIGHLOAD_TIMER_RATE) {
		if (highload_cnt[LOAD_ALL] >= HIGHLOAD_HIGH_THRESHOLD &&
		    last_report != LOAD_HIGH) {
			highload_notify_netlink(LOAD_HIGH);
			last_report = LOAD_HIGH;
			reported = true;
			pr_info("cpuload: all-cpu HIGH_LOAD\n");
		} else if (highload_cnt[LOAD_ALL] == HIGHLOAD_LOW_THRESHOLD &&
			   last_report != LOAD_LOW) {
			highload_notify_netlink(LOAD_LOW);
			last_report = LOAD_LOW;
			reported = true;
			pr_info("cpuload: all-cpu LOW_LOAD\n");
		}
	}

	last_total = total;
	last_busy = busy;
	return reported;
}

static bool highload_check_masked(unsigned long mask, u32 type,
				  int thresh_up, int thresh_down)
{
	static u64 last_total[NR_LOAD_TYPES];
	static u64 last_busy[NR_LOAD_TYPES];
	u64 total = 0, busy = 0, delta, busy_pct;

	highload_calc_load(&total, &busy, mask);

	delta = total - last_total[type];
	if (delta) {
		busy_pct = (busy - last_busy[type]) * HIGHLOAD_PERCENT;

		if (busy_pct >= (u64)thresh_up * delta) {
			highload_cnt[type]++;
			highload_cycle_high_cnt[type]++;
		}
		if (busy_pct < (u64)thresh_down * delta)
			highload_cycle_normal_cnt[type]++;
	}

	last_total[type] = total;
	last_busy[type] = busy;

	if (highload_nr_intervals < HIGHLOAD_TIMER_RATE)
		return false;

	highload_cycle_count[type]++;

	if (highload_cnt[type] >= HIGHLOAD_HIGH_THRESHOLD &&
	    highload_last_status[type] != LOAD_HIGH) {
		pr_info("cpuload: masked HIGH_LOAD\n");
		highload_last_status[type] = LOAD_HIGH;
		highload_action_set(type, true);
		highload_cycle_reset(type);
		return true;
	}

	if (!(highload_cycle_count[type] % HIGHLOAD_BIG_LOWCYCLE) &&
	    highload_cycle_check_low(type))
		return true;

	if (highload_cycle_count[type] >= HIGHLOAD_BIG_HIGHCYCLE &&
	    highload_cycle_check_high(type))
		return true;

	return false;
}

/* ---- Process statistics collection ---- */

static void highload_sample_tasks(u32 type)
{
	unsigned long mask = highload_type_to_mask(type);
	pid_t local_pid = current->pid;
	u32 cpu;

	for (cpu = 0; cpu < CONFIG_NR_CPUS; cpu++) {
		struct task_struct *p;
		struct rq *rq;
		pid_t tgid;

		if (!cpu_online(cpu))
			continue;
		if (!(BIT(cpu) & mask))
			continue;

		rq = cpu_rq(cpu);
		if (!rq)
			continue;

		rcu_read_lock();
		p = rcu_dereference(rq->curr);
		if (p)
			get_task_struct(p);
		rcu_read_unlock();

		if (!p)
			continue;

		tgid = p->tgid;
		put_task_struct(p);

		if (!tgid || tgid == local_pid)
			continue;

		highload_pid_insert(tgid, type);
	}
}

static void highload_proc_report_low(void)
{
	u32 type;

	for (type = 1; type < NR_LOAD_TYPES; type++) {
		if (!(highload_action_bits.bits_type & BIT(type)))
			continue;
		if (highload_action_bits.bits_high & BIT(type))
			continue;

		pr_info("cpuload: clear pids size:%d type:%u\n",
			highload_top_pid_cnt[type], type);
		highload_notify_low(type);

		memset(highload_top_pids[type], 0,
		       sizeof(highload_top_pids[type]));
		highload_pid_reset(type);
		highload_top_pid_cnt[type] = 0;
	}
}

static void highload_proc_report_high(u32 type)
{
	struct highload_pid_mgr *mgr = &highload_pid_mgr[type];
	struct highload_pid_node *entry;
	int threshold = highload_type_to_threshold(type);
	int i, idx;

	for (i = 0, entry = mgr->pool; i < mgr->idx; i++, entry++) {
		if (entry->count <= threshold)
			continue;

		pr_info("cpuload: pid:%d count:%d type:%u\n",
			entry->pid, entry->count, type);

		if (highload_top_pid_cnt[type] < HIGHLOAD_MAX_PIDS) {
			idx = highload_top_pid_cnt[type]++;
			highload_top_pids[type][idx] = entry->pid;
		}
	}

	pr_info("cpuload: top_pid_cnt:%d type:%u\n",
		highload_top_pid_cnt[type], type);

	if (highload_top_pid_cnt[type])
		highload_notify_high(type, highload_top_pid_cnt[type],
				     highload_top_pids[type]);

	highload_pid_reset(type);
	memset(highload_top_pids[type], 0, sizeof(highload_top_pids[type]));
	highload_top_pid_cnt[type] = 0;
}

/* ---- Periodic work functions ---- */

static void highload_proc_work_fn(struct work_struct *work)
{
	u32 type;

	mimon_trace_format("enter highload_proc_work_fn\n");

	if (!highload_proc_elapsed)
		highload_proc_report_low();

	highload_proc_elapsed += HIGHLOAD_PROC_PERIOD;

	for (type = 1; type < NR_LOAD_TYPES; type++) {
		if (!(highload_action_bits.bits_type & BIT(type)))
			continue;
		if (!(highload_action_bits.bits_high & BIT(type)))
			continue;

		highload_sample_tasks(type);

		if (highload_proc_elapsed >= HIGHLOAD_PROC_DURING)
			highload_proc_report_high(type);
	}

	if (highload_proc_elapsed >= HIGHLOAD_PROC_DURING) {
		highload_proc_elapsed = 0;
	} else {
		schedule_delayed_work_on(0, &highload_proc_work,
					usecs_to_jiffies(HIGHLOAD_PROC_PERIOD));
	}

	mimon_trace_format("exit highload_proc_work_fn\n");
}

static void highload_detect_work_fn(struct work_struct *work)
{
	mimon_trace_format("enter highload_detect_work_fn\n");

	++highload_nr_intervals;

	if (highload_nr_intervals >= HIGHLOAD_TIMER_RATE) {
		highload_action_bits.bits_type = 0;
		highload_action_bits.bits_high = 0;
	}

	highload_snapshot_cpu_time();

	if (highload_switch & BIT(LOAD_SWITCH_DEFAULT)) {
		if (highload_check_all())
			goto out;
	}

	if (highload_switch & BIT(LOAD_SWITCH_ROOT))
		highload_check_masked(0xf0, LOAD_CPUSET_ROOT,
				      HIGHLOAD_CPUSET_ROOT_UP,
				      HIGHLOAD_CPUSET_ROOT_DOWN);

	if (highload_switch & BIT(LOAD_SWITCH_BG))
		highload_check_masked(0x0c, LOAD_CPUSET_BG,
				      HIGHLOAD_CPUSET_BG_UP,
				      HIGHLOAD_CPUSET_BG_DOWN);

	if (highload_nr_intervals >= HIGHLOAD_TIMER_RATE) {
		if (highload_action_bits.bits_type)
			schedule_delayed_work_on(0, &highload_proc_work,
				usecs_to_jiffies(HIGHLOAD_PROC_PERIOD));
	}

out:
	if (highload_nr_intervals >= HIGHLOAD_TIMER_RATE)
		highload_count_reset();

	schedule_delayed_work(&highload_detect_work,
			      usecs_to_jiffies(HIGHLOAD_DETECT_PERIOD));

	mimon_trace_format("exit highload_detect_work_fn\n");
}

/* ---- Sysfs interface: /sys/kernel/highload/ ---- */

static ssize_t enable_show(struct kobject *kobj,
			    struct kobj_attribute *attr, char *buf)
{
	if (!buf)
		return -EINVAL;

	return snprintf(buf, PAGE_SIZE, "%ld\n", highload_switch);
}

static ssize_t enable_store(struct kobject *kobj,
			     struct kobj_attribute *attr,
			     const char *buf, size_t count)
{
	unsigned long val;
	int ret;

	if (!buf)
		return -EINVAL;

	ret = kstrtoul(buf, HIGHLOAD_SYSFS_BUF_LEN, &val);
	if (ret)
		return -EINVAL;

	if (val) {
		if (!highload_switch)
			schedule_delayed_work(&highload_detect_work,
				usecs_to_jiffies(HIGHLOAD_DETECT_PERIOD));
	} else {
		if (highload_switch) {
			highload_count_reset();
			cancel_delayed_work_sync(&highload_detect_work);
		}
	}

	highload_switch = val;
	return count;
}

static struct kobj_attribute highload_enable_attr =
	__ATTR(enable, HIGHLOAD_SYSFS_MODE, enable_show, enable_store);

static ssize_t cpumask_root_show(struct kobject *kobj,
				 struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%lx\n", highload_cpumask_root);
}

static ssize_t cpumask_root_store(struct kobject *kobj,
				  struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	highload_cpumask_root = val;
	return count;
}

static struct kobj_attribute highload_cpumask_root_attr =
	__ATTR(cpumask_root, HIGHLOAD_SYSFS_MODE,
	       cpumask_root_show, cpumask_root_store);

static ssize_t cpumask_bg_show(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%lx\n", highload_cpumask_bg);
}

static ssize_t cpumask_bg_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	highload_cpumask_bg = val;
	return count;
}

static struct kobj_attribute highload_cpumask_bg_attr =
	__ATTR(cpumask_bg, HIGHLOAD_SYSFS_MODE,
	       cpumask_bg_show, cpumask_bg_store);

/* ---- Init / Exit ---- */

int cpu_load_monitor_init(void)
{
	int ret, i;

	highload_nr_intervals = 0;
	highload_proc_elapsed = 0;

	highload_kobj = kobject_create_and_add("highload", kernel_kobj);
	if (!highload_kobj)
		return -ENOMEM;

	ret = sysfs_create_file(highload_kobj, &highload_enable_attr.attr);
	if (ret)
		goto err_sysfs;

	ret = sysfs_create_file(highload_kobj,
				&highload_cpumask_root_attr.attr);
	if (ret)
		goto err_mask_root;

	ret = sysfs_create_file(highload_kobj,
				&highload_cpumask_bg_attr.attr);
	if (ret)
		goto err_mask_bg;

	ret = highload_pid_mgr_init();
	if (ret)
		goto err_pid;

	INIT_DEFERRABLE_WORK(&highload_detect_work, highload_detect_work_fn);
	INIT_DEFERRABLE_WORK(&highload_proc_work, highload_proc_work_fn);

	for (i = 0; i < NR_LOAD_TYPES; i++)
		highload_last_status[i] = LOAD_LOW;

	return 0;

err_pid:
	sysfs_remove_file(highload_kobj, &highload_cpumask_bg_attr.attr);
err_mask_bg:
	sysfs_remove_file(highload_kobj, &highload_cpumask_root_attr.attr);
err_mask_root:
	sysfs_remove_file(highload_kobj, &highload_enable_attr.attr);
err_sysfs:
	kobject_put(highload_kobj);
	highload_kobj = NULL;
	return ret;
}

void cpu_load_monitor_exit(void)
{
	cancel_delayed_work_sync(&highload_detect_work);
	cancel_delayed_work_sync(&highload_proc_work);

	if (highload_kobj) {
		sysfs_remove_file(highload_kobj,
				  &highload_cpumask_bg_attr.attr);
		sysfs_remove_file(highload_kobj,
				  &highload_cpumask_root_attr.attr);
		sysfs_remove_file(highload_kobj, &highload_enable_attr.attr);
		kobject_put(highload_kobj);
		highload_kobj = NULL;
	}

	highload_pid_mgr_free();
}
