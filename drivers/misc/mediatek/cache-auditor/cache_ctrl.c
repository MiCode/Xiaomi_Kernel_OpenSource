/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#undef pr_fmt
#define pr_fmt(fmt) "cache-ctl: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/cgroup-defs.h>
#include <linux/cgroup.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <asm/sysreg.h>
#include <cache_ctrl.h>
#include <mt-plat/l3cc_common.h>
#ifdef CONFIG_MTK_TASK_TURBO
#include <mt-plat/turbo_common.h>
#endif

#define CREATE_TRACE_POINTS
#include "trace_cache_ctrl.h"

#define AID_SYSTEM	1000

DECLARE_PER_CPU(struct boost_groups, cpu_boost_groups);
static DEFINE_PER_CPU(bool, in_partition_control);
static int is_cache_ctrl_enabled;

static int ctl_min_score = 500; // default is service adj
module_param(ctl_min_score, int, 0600);

static unsigned int ctl_suppress_part = 1;
module_param(ctl_suppress_part, uint, 0600);

static int ctl_turbo_group = 1;
module_param(ctl_turbo_group, int, 0600);

unsigned int ctl_suppress_group = 1 << GROUP_BG; // default is BG for depress
module_param(ctl_suppress_group, uint, 0600);

static struct dentry *l3cc_debugfs;

struct partition_stats {
	int state[CONFIG_NR_CPUS];
};

#define CLUSTERPARTCR_EL1	sys_reg(3, 0, 15, 4, 3)
#define CLUSTERTHREADSID_EL1	sys_reg(3, 0, 15, 4, 0)

static inline void cache_ctrl_write_partcr(u32 val)
{
	write_sysreg_s(val, CLUSTERPARTCR_EL1);
	isb();
}

static inline u32 cache_ctrl_read_partcr(void)
{
	return read_sysreg_s(CLUSTERPARTCR_EL1);
}

static inline void cache_ctrl_write_threadsid(u32 val)
{
	write_sysreg_s(val, CLUSTERTHREADSID_EL1);
	/*
	 * prefer not to use isb(). Since cache
	 * limit is applied to next task, so
	 * it's no need to take effect immediately
	 */
}

static inline u32 cache_ctrl_read_threadsid(void)
{
	return read_sysreg_s(CLUSTERTHREADSID_EL1);
}

void dump_partition_config_per_cpu(void *info)
{
	struct partition_stats *stats;
	int cpu = smp_processor_id();

	register int val asm("x3");

	stats = (struct partition_stats *)info;
	if (IS_ERR(stats) || !stats) {
		pr_debug("%s: invalid info address\n", __func__);
		return;
	}
	val = cache_ctrl_read_threadsid();
	stats->state[cpu] = val;
}

static int l3cc_info_show(struct seq_file *m, void *unused)
{
	struct partition_stats stats;
	int partcr_val;
	int i;

	seq_puts(m, "dump L3 partition register.\n");
	for (i = 0; i < nr_cpu_ids; i++) {
		smp_call_function_single(i,
				dump_partition_config_per_cpu, &stats, 1);

		seq_printf(m, "\tCPU%d: group[%d]\n", i, stats.state[i]);
	}

	partcr_val = cache_ctrl_read_partcr();
	seq_printf(m, "partcr=0x%x\n", partcr_val);
	return 0;
}

static int l3cc_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, l3cc_info_show, inode->i_private);
}

static const struct file_operations l3cc_debug_fops = {
	.owner = THIS_MODULE,
	.open = l3cc_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static bool is_bw_congested;

static int qos_callback(struct notifier_block *nb,
			unsigned long qos_status, void *v)
{
	if (qos_status == QOS_BOUND_BW_FREE && is_bw_congested)
		is_bw_congested = false;
	else if (qos_status > QOS_BOUND_BW_FREE && !is_bw_congested)
		is_bw_congested = true;
	return NOTIFY_DONE;
}

static struct notifier_block nb = {
	.notifier_call = qos_callback,
};

void config_partition(int gid)
{
	/*
	 * per-cpu: CLUSTERTHREADSID_EL1
	 * group id:
	 * group 0: 0xf, full cache way
	 * group 1: 0x1, 0b0001 cache way
	 */
	cache_ctrl_write_threadsid(gid);
}

#if IS_ENABLED(CONFIG_SCHED_TUNE)
static bool group_active(unsigned int idx, struct boost_groups *bg)
{
	if (bg->group[idx].tasks)
		return true;
	return false;
}

static bool important_group_active(int cpu)
{
	struct boost_groups *bg;
	unsigned int idx;

	bg = &per_cpu(cpu_boost_groups, cpu);
	for (idx = 1; idx < BOOSTGROUPS_COUNT; ++idx) {
		if (ctl_suppress_group & (1 << idx))
			continue;

		if (group_active(idx, bg)) {
			// trace_ca_apply_control(cpu, 5);
			return true;
		}
	}
	return false;
}
#else
static bool important_group_active(int cpu) {return false; }
#endif

static bool check_important_runnable(int cpu)
{
	struct rq *rq = cpu_rq(cpu);

	/* check if runnable task */
	if (rq->cfs.h_nr_running > 2 &&
	    important_group_active(cpu))
		return true;

	return false;
}

static inline int get_stune_id(struct task_struct *task)
{
#if IS_ENABLED(CONFIG_SCHED_TUNE)
	const int subsys_id = schedtune_cgrp_id;
	struct cgroup *grp;

	rcu_read_lock();
	grp = task_cgroup(task, subsys_id);
	rcu_read_unlock();
	return grp->id;
#else
	return 0;
#endif
}

static inline bool is_important(struct task_struct *task)
{
	int grp_id = get_stune_id(task);

#ifdef CONFIG_MTK_TASK_TURBO
	if (ctl_turbo_group && is_turbo_task(task))
		return true;
#endif
	if (ctl_suppress_group & (1 << grp_id))
		return false;
	return true;
}

/*
 * reset cache control of the cpu.
 */
static inline void reset_cache_ctl(void)
{
	if (__this_cpu_read(in_partition_control) == true) {
		__this_cpu_write(in_partition_control, false);
		config_partition(0);
	}
}

static bool audit_next_task(struct task_struct *next)
{
	int cpu = smp_processor_id();

	/* important task, no need to control */
	if (is_important(next))
		return false;

	/* DO NOT audit kernel threads */
	if (!next->mm)
		return false;

	if (!fair_policy(next->policy))
		return false;

	/* Skip tasks of AID_SYSTEM */
	if (from_kuid(current_user_ns(), task_euid(next)) == AID_SYSTEM)
		return false;

	/* Prefer not to suppress when memstall happened */
	if (next->flags & PF_MEMSTALL)
		return false;

	/* prevent to depresss service of ta app */
	if (next->signal->oom_score_adj <= ctl_min_score)
		return false;

	if (is_bw_congested)
		return false;

	if (check_important_runnable(cpu))
		return false;

	return true;
}

inline void restrict_next_task(struct task_struct *next)
{
	int cpu = smp_processor_id();

	if (!audit_next_task(next)) {
		trace_skip_cache_control(next, is_bw_congested);
		return;
	}

	/* perform to limit l3-cache use */
	__this_cpu_write(in_partition_control, true);
	config_partition(ctl_suppress_part);
	trace_apply_cache_control(next, cpu, ctl_suppress_part);
}

void hook_ca_context_switch(struct rq *rq, struct task_struct *prev,
				    struct task_struct *next)
{
	reset_cache_ctl();
	if (is_cache_ctrl_enabled == 0)
		return;
	restrict_next_task(next);
}
EXPORT_SYMBOL(hook_ca_context_switch);

static int set_cache_control(const char *buf, const struct kernel_param *kp)
{
	int ret = 0;
	int val = 0;

	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return ret;

	if (val == is_cache_ctrl_enabled) {
		pr_debug("No need to change config\n");
		return 0;
	}

	pr_info("Changing state from %d to %d ...\n", is_cache_ctrl_enabled, val);

	if (val == 0 || val == 1)
		ret = param_set_int(buf, kp);
	else
		ret = -EINVAL;

	if (ret < 0)
		return ret;

	pr_info("[Change complete] %s with mode %d\n",
		is_cache_ctrl_enabled ? "enable":"disable",
		is_cache_ctrl_enabled);
	return 0;
}

struct kernel_param_ops cache_ctrl_enable_cb = {
	.set = set_cache_control,
	.get = param_get_int,
};
param_check_int(enabled, &is_cache_ctrl_enabled);
module_param_cb(enable, &cache_ctrl_enable_cb, &is_cache_ctrl_enabled, 0664);
__MODULE_PARM_TYPE(enabled, "int");

static int __init cache_ctrl_init(void)
{
	register_qos_notifier(&nb);
	cache_ctrl_write_partcr(0x8F);

	l3cc_debugfs = debugfs_create_file("l3cc_info",
					   0444,
					   NULL,
					   NULL,
					   &l3cc_debug_fops);
	return 0;
}

static void __exit cache_ctrl_exit(void)
{
	unregister_qos_notifier(&nb);
}

module_init(cache_ctrl_init);
module_exit(cache_ctrl_exit);
