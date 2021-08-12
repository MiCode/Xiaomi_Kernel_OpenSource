// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cgroup.h>
#include <linux/slab.h>
#include <linux/task_work.h>
#include <linux/sched.h>
#include <linux/percpu-defs.h>
#include <trace/events/task.h>

#include <trace/hooks/fpsimd.h>
#include <trace/hooks/cgroup.h>
#include <sched/sched.h>
#include "cpuqos_sys_common.h"

#define CREATE_TRACE_POINTS
#include <cpuqos_v3_trace.h>
#undef CREATE_TRACE_POINTS
#undef TRACE_INCLUDE_PATH

#define CREATE_TRACE_POINTS
#include <met_mpam_events.h>
#undef CREATE_TRACE_POINTS
#undef TRACE_INCLUDE_PATH

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jing-Ting Wu");

static int cpuqos_subsys_id = cpu_cgrp_id;

/*
 * cgroup path -> PARTID map
 *
 * Could be made better with cgroupv2: we could have some files that userspace
 * could write into which would give us a {path, PARTID} pair. We could then
 * translate the path to a cgroup with cgroup_get_from_path(), and save the
 * css->id mapping then.
 */
static char *mpam_path_partid_map[] = {
	"/",
	"/foreground",
	"/background",
	"/top-app",
	"/rt",
	"/system",
	"/system-background"
};

/*
 * cgroup css->id -> PARTID cache
 *
 * Not sure how stable those IDs are supposed to be. If we are supposed to
 * support cgroups being deleted, we may need more hooks to cache that.
 */
static int mpam_css_partid_map[50] = { [0 ... 49] = -1 };

/*
 * group number by mpam_path_partid_map -> css->id
 *
 */
static int mpam_group_css_map[50] = { [0 ... 49] = -1 };

/* The MPAM0_EL1.PARTID_D in use by a given CPU */
static DEFINE_PER_CPU(int, mpam_local_partid);

enum perf_mode {
	AGGRESSIVE,
	BALANCE,
	CONSERVATIVE,
	DISABLE
};

enum partid_grp {
	DEF_PARTID,
	SECURE_PARTID,
	CT_PARTID,
	NCT_PARTID
};

enum partid_rank {
	GROUP_RANK,
	TASK_RANK
};

static enum perf_mode cpuqos_perf_mode = BALANCE;

int task_curr_clone(const struct task_struct *p)
{
	return cpu_curr(task_cpu(p)) == p;
}

unsigned int get_task_partid(struct task_struct *p)
{
	unsigned int partid;

	partid = (unsigned int)p->android_vendor_data1[1];

	return partid;
}

unsigned int get_task_rank(struct task_struct *p)
{
	unsigned int rank;

	rank = (unsigned int)p->android_vendor_data1[2];

	return rank;
}

/* Get the css:partid mapping */
static int mpam_map_css_partid(struct cgroup_subsys_state *css)
{
	int partid;

	if (!css)
		goto no_match;

	/*
	 * The map is stable post init so no risk of two concurrent tasks
	 * cobbling each other
	 */
	partid = READ_ONCE(mpam_css_partid_map[css->id]);
	if (partid >= 0)
		return partid;

no_match:
	/* No match, use sensible default */
	partid = NCT_PARTID;

	return partid;
}

/*
 * Get the task_struct:partid mapping
 * This is the place to add special logic for a task-specific (rather than
 * cgroup-wide) PARTID.
 */
static int mpam_map_task_partid(struct task_struct *p)
{
	struct cgroup_subsys_state *css;
	int partid;
	int rank = get_task_rank(p);

	if (cpuqos_perf_mode == DISABLE) {
		/* disable mode */
		partid = DEF_PARTID;
		goto out;
	}

	if (rank == TASK_RANK) {
		/* task rank */
		partid = get_task_partid(p);
		goto out;
	} else {
		/* group rank */
		rcu_read_lock();
		css = task_css(p, cpuqos_subsys_id);
		partid = mpam_map_css_partid(css);
		rcu_read_unlock();
		goto out;
	}
out:
	return partid;
}

/*
 * Write the PARTID to use on the local CPU.
 */
static void mpam_write_partid(int partid)
{
	if (partid == this_cpu_read(mpam_local_partid))
		return;

	this_cpu_write(mpam_local_partid, partid);

	/* Write to e.g. MPAM0_EL1.PARTID_D here */

}

/*
 * Sync @p's associated PARTID with this CPU's register.
 */
static void mpam_sync_task(struct task_struct *p)
{
	struct cgroup_subsys_state *css;
	int old_partid = this_cpu_read(mpam_local_partid);

	rcu_read_lock();
	css = task_css(p, cpuqos_subsys_id);
	rcu_read_unlock();

	mpam_write_partid(mpam_map_task_partid(p));
	trace_cpuqos_cpu_partid(smp_processor_id(), p->pid,
				css->id, old_partid,
				this_cpu_read(mpam_local_partid),
				get_task_rank(p),
				cpuqos_perf_mode);

}

/*
 * Same as mpam_sync_task(), with a pre-filter for the current task.
 */
static void mpam_sync_current(void *task)
{
	int prev_partid;
	int next_partid;

	if (task && task != current)
		return;

	if (trace_MPAM_CT_task_enter_enabled() && trace_MPAM_CT_task_leave_enabled()) {
		prev_partid = this_cpu_read(mpam_local_partid);
		next_partid = mpam_map_task_partid(current);

		if ((prev_partid == CT_PARTID) && (next_partid == NCT_PARTID))
			trace_MPAM_CT_task_leave(current->pid, current->comm);

		if (next_partid == CT_PARTID)
			trace_MPAM_CT_task_enter(current->pid, current->comm);
	}

	mpam_sync_task(current);
}

/*
 * Same as mpam_sync_current(), with an explicit mb for partid mapping changes.
 * Note: technically not required for arm64+GIC since we get explicit barriers
 * when raising and handling an IPI. See:
 * f86c4fbd930f ("irqchip/gic: Ensure ordering between read of INTACK and shared data")
 */
static void mpam_sync_current_mb(void *task)
{
	if (task && task != current)
		return;

	/* Pairs with smp_wmb() following mpam_cgroup_partid_map[] updates */
	smp_rmb();
	mpam_sync_task(current);
}

static void mpam_kick_task(struct task_struct *p, int partid)
{
	if (partid >= 0)
		p->android_vendor_data1[1] = partid;

	/*
	 * If @p is no longer on the task_cpu(p) we see here when the smp_call
	 * actually runs, then it had a context switch, so it doesn't need the
	 * explicit update - no need to chase after it.
	 */
	if (task_curr_clone(p))
		smp_call_function_single(task_cpu(p), mpam_sync_current, p, 1);
}

/*
 * Set group is critical task(CT)/non-critical task(NCT)
 * group_id: depend on mpam_path_partid_map list
 *           0: "/",
 *           1: "/foreground"
 *           2: "/background"
 *	     3: "/top-app"
 *           4: "/rt",
 *           5: "/system",
 *           6: "/system-background"
 * set: if true, set group is CT;
 *      if false, set group is NCT.
 * Return: 0: success,
 *        -1: perf mode is disable / group_id is not exist.
 */
int set_ct_group(int group_id, bool set)
{
	int css_id = -1;
	int old_partid;
	int new_partid;

	if ((group_id >= ARRAY_SIZE(mpam_path_partid_map)) || (group_id < 0) ||
		(cpuqos_perf_mode == DISABLE))
		return -1;

	css_id = mpam_group_css_map[group_id];
	if (css_id < 0)
		return -1;

	old_partid = mpam_css_partid_map[css_id];

	if (set)
		new_partid = CT_PARTID;
	else
		new_partid = NCT_PARTID;

	trace_cpuqos_set_ct_group(group_id, css_id, set,
				old_partid, new_partid,
				cpuqos_perf_mode);

	if (new_partid != old_partid) {
		mpam_css_partid_map[css_id] = new_partid;

		/*
		 * Ensure the partid map update is visible before kicking the CPUs.
		 * Pairs with smp_rmb() in mpam_sync_current_mb().
		 */
		smp_wmb();
		smp_call_function(mpam_sync_current_mb, NULL, 1);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(set_ct_group);

/*
 * Set task is critical task(CT) or use its group partid
 * pid: task pid
 * set: if true, set task is CT(ignore group setting);
 *      if false, set task use its group partid.
 * Return: 0: success,
	   -1: perf mode is disable / p is not exist.
 */
int set_ct_task(int pid, bool set)
{
	struct task_struct *p;
	struct cgroup_subsys_state *css;
	int old_partid;
	int new_partid;

	if (cpuqos_perf_mode == DISABLE)
		return -1;

	rcu_read_lock();
	p = find_task_by_vpid(pid);

	if (p)
		get_task_struct(p);
	rcu_read_unlock();

	if (!p)
		return -1;

	rcu_read_lock();
	css = task_css(p, cpuqos_subsys_id);
	rcu_read_unlock();

	old_partid = mpam_map_task_partid(p); /* before rank change */

	if (set) { /* set task is critical task */
		p->android_vendor_data1[2] = TASK_RANK;
		if (mpam_map_task_partid(p) != CT_PARTID)
			new_partid = CT_PARTID;
		else
			new_partid = old_partid;
	} else { /* reset to group setting */
		p->android_vendor_data1[2] = GROUP_RANK;
		new_partid = mpam_map_task_partid(p); /* after rank change */
	}

	trace_cpuqos_set_ct_task(p->pid, css->id, set,
				old_partid, new_partid,
				get_task_rank(p), cpuqos_perf_mode);

	if (new_partid != old_partid)
		mpam_kick_task(p, new_partid);

	put_task_struct(p);

	return 0;
}
EXPORT_SYMBOL_GPL(set_ct_task);

int set_cpuqos_mode(int mode)
{
	switch (mode) {
	case AGGRESSIVE:
		cpuqos_perf_mode = AGGRESSIVE;
		break;
	case BALANCE:
		cpuqos_perf_mode = BALANCE;
		break;
	case CONSERVATIVE:
		cpuqos_perf_mode = CONSERVATIVE;
		break;
	case DISABLE:
		cpuqos_perf_mode = DISABLE;
		break;
	}

	trace_cpuqos_set_cpuqos_mode(mode);

	/*
	 * Ensure the partid map update is visible before kicking the CPUs.
	 * Pairs with smp_rmb() in mpam_sync_current_mb().
	 */
	smp_wmb();
	smp_call_function(mpam_sync_current_mb, NULL, 1);

	return 0;
}
EXPORT_SYMBOL_GPL(set_cpuqos_mode);

static ssize_t set_ct_group_ct(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *ubuf,
				size_t cnt)
{
	unsigned int group_id = -1;

	if (sscanf(ubuf, "%iu", &group_id) != 0) {
		if (group_id < nr_cpu_ids)
			set_ct_group(group_id, true);
	}

	return cnt;
}

static ssize_t set_ct_group_nct(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *ubuf,
				size_t cnt)
{
	unsigned int group_id = -1;

	if (sscanf(ubuf, "%iu", &group_id) != 0) {
		if (group_id < nr_cpu_ids)
			set_ct_group(group_id, false);
	}
	return cnt;
}

static ssize_t set_ct_task_ct(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *ubuf,
				size_t cnt)
{
	unsigned int task_pid = -1;

	if (sscanf(ubuf, "%iu", &task_pid) != 0)
		set_ct_task(task_pid, true);

	return cnt;
}

static ssize_t set_ct_task_nct(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *ubuf,
				size_t cnt)
{
	unsigned int task_pid = -1;

	if (sscanf(ubuf, "%iu", &task_pid) != 0)
		set_ct_task(task_pid, false);

	return cnt;
}

static ssize_t set_cpuqos_mode_debug(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *ubuf,
				size_t cnt)
{
	unsigned int mode = -1;

	if (sscanf(ubuf, "%iu", &mode) != 0) {
		if (mode < nr_cpu_ids)
			set_cpuqos_mode(mode);
	}

	return cnt;
}

struct kobj_attribute set_ct_group_ct_attr =
__ATTR(cpuqos_set_ct_group_ct, 0200, NULL, set_ct_group_ct);

struct kobj_attribute set_ct_group_nct_attr =
__ATTR(cpuqos_set_ct_group_nct, 0200, NULL, set_ct_group_nct);

struct kobj_attribute set_ct_task_ct_attr =
__ATTR(cpuqos_set_ct_task_ct, 0200, NULL, set_ct_task_ct);

struct kobj_attribute set_ct_task_nct_attr =
__ATTR(cpuqos_set_ct_task_nct, 0200, NULL, set_ct_task_nct);

struct kobj_attribute set_cpuqos_mode_attr =
__ATTR(cpuqos_set_cpuqos_mode, 0200, NULL, set_cpuqos_mode_debug);

static void mpam_hook_attach(void __always_unused *data,
			     struct cgroup_subsys *ss, struct cgroup_taskset *tset)
{
	struct cgroup_subsys_state *css;
	struct task_struct *p;

	if (ss->id != cpuqos_subsys_id)
		return;

	cgroup_taskset_first(tset, &css);
	cgroup_taskset_for_each(p, css, tset)
		mpam_kick_task(p, -1);
}

static void mpam_hook_switch(void __always_unused *data,
			     struct task_struct *prev, struct task_struct *next)
{
	int prev_partid;
	int next_partid;

	if (trace_MPAM_CT_task_enter_enabled() && trace_MPAM_CT_task_leave_enabled()) {
		prev_partid = this_cpu_read(mpam_local_partid);
		next_partid = mpam_map_task_partid(next);

		if (prev && (prev_partid == CT_PARTID))
			trace_MPAM_CT_task_leave(prev->pid, prev->comm);

		if (next && (next_partid == CT_PARTID))
			trace_MPAM_CT_task_enter(next->pid, next->comm);
	}

	mpam_sync_task(next);
}

static void mpam_task_newtask(void __always_unused *data,
				struct task_struct *p, unsigned long clone_flags)
{
	p->android_vendor_data1[1] = NCT_PARTID; /* partid */
	p->android_vendor_data1[2] = GROUP_RANK; /* rank */
}

/* Check if css' path matches any in mpam_path_partid_map and cache that */
static void __init __map_css_partid(struct cgroup_subsys_state *css, char *tmp, int pathlen)
{
	int i;

	cgroup_path(css->cgroup, tmp, pathlen);

	for (i = 0; i < ARRAY_SIZE(mpam_path_partid_map); i++) {
		if (!strcmp(mpam_path_partid_map[i], tmp)) {
			WRITE_ONCE(mpam_group_css_map[i], css->id);

			if (cpuqos_perf_mode == DISABLE)
				WRITE_ONCE(mpam_css_partid_map[css->id], DEF_PARTID);

			/* init group_partid */
			if (!strcmp(mpam_path_partid_map[i], "/top-app"))
				WRITE_ONCE(mpam_css_partid_map[css->id], CT_PARTID);
			else
				WRITE_ONCE(mpam_css_partid_map[css->id], NCT_PARTID);

			pr_info("group_id=%d, path=%s, mpam_path=%s, css_id=%d, group_css=%d, partid_map=%d\n",
				i, tmp, mpam_path_partid_map[i], css->id, mpam_group_css_map[i],
				mpam_css_partid_map[css->id]);
		}
	}
}

/* Recursive DFS */
static void __init __map_css_children(struct cgroup_subsys_state *css, char *tmp, int pathlen)
{
	struct cgroup_subsys_state *child;

	list_for_each_entry_rcu(child, &css->children, sibling) {
		if (!child || !child->cgroup)
			continue;

		__map_css_partid(child, tmp, pathlen);
		__map_css_children(child, tmp, pathlen);
	}
}

static int __init mpam_init_cgroup_partid_map(void)
{
	struct cgroup_subsys_state *css;
	struct cgroup *cgroup;
	char buf[50];
	int ret = 0;

	rcu_read_lock();
	/*
	 * cgroup_get_from_path() would be much cleaner, but that seems to be v2
	 * only. Getting current's cgroup is only a means to get a cgroup handle,
	 * use that to get to the root. Clearly doesn't work if several roots
	 * are involved.
	 */
	cgroup = task_cgroup(current, cpuqos_subsys_id);
	if (IS_ERR(cgroup)) {
		ret = PTR_ERR(cgroup);
		goto out_unlock;
	}

	cgroup = &cgroup->root->cgrp;
	css = rcu_dereference(cgroup->subsys[cpuqos_subsys_id]);
	if (IS_ERR_OR_NULL(css)) {
		ret = -ENOENT;
		goto out_unlock;
	}

	__map_css_partid(css, buf, 50);
	__map_css_children(css, buf, 50);

out_unlock:
	rcu_read_unlock();
	return ret;
}

static int __init mpam_proto_init(void)
{
	int ret;

	ret = init_cpuqos_common_sysfs();
	if (ret) {
		pr_info("init cpuqos sysfs failed\n");
		goto out;
	}

	ret = mpam_init_cgroup_partid_map();
	if (ret) {
		pr_info("init cpuqos failed\n");
		goto out;
	}

	ret = register_trace_android_vh_cgroup_attach(mpam_hook_attach, NULL);
	if (ret) {
		pr_info("register android_vh_cgroup_attach failed\n");
		goto out;
	}

	ret = register_trace_android_vh_is_fpsimd_save(mpam_hook_switch, NULL);
	if (ret) {
		pr_info("register android_vh_is_fpsimd_save failed\n");
		goto out_attach;
	}

	ret = register_trace_task_newtask(mpam_task_newtask, NULL);
	if (ret) {
		pr_info("register trace_task_newtask failed\n");
		goto out_attach;
	}

	/*
	 * Ensure the partid map update is visible before kicking the CPUs.
	 * Pairs with smp_rmb() in mpam_sync_current_mb().
	 */
	smp_wmb();
	/*
	 * Hooks are registered, kick every CPU to force sync currently running
	 * tasks.
	 */
	smp_call_function(mpam_sync_current_mb, NULL, 1);

	return 0;

out_attach:
	unregister_trace_android_vh_cgroup_attach(mpam_hook_attach, NULL);
out:
	return ret;
}

static void mpam_reset_partid(void __always_unused *info)
{
	mpam_write_partid(DEF_PARTID);
}

static void __init mpam_proto_exit(void)
{
	unregister_trace_android_vh_is_fpsimd_save(mpam_hook_switch, NULL);
	unregister_trace_android_vh_cgroup_attach(mpam_hook_attach, NULL);
	unregister_trace_task_newtask(mpam_task_newtask, NULL);

	smp_call_function(mpam_reset_partid, NULL, 1);
	cleanup_cpuqos_common_sysfs();
}

module_init(mpam_proto_init);
module_exit(mpam_proto_exit);
