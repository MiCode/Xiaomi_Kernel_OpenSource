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

#include <trace/hooks/fpsimd.h>
#include <trace/hooks/cgroup.h>
#include <sched/sched.h>
#include "cpuqos_sys_common.h"

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
#if IS_ENABLED(CONFIG_CGROUP_SCHED)
	struct cgroup_subsys_state *css;
	int partid;
	int rank = p->android_vendor_data1[2];

	if (cpuqos_perf_mode == DISABLE) {
		/* disable mode */
		partid = DEF_PARTID;
		//pr_info("disable_mode: p=%d, partid=%d, rank=%d, cpuqos_perf_mode=%d\n",
		//	p->pid, partid, rank, cpuqos_perf_mode);
		goto out;
	}

	if (rank == TASK_RANK) {
		/* task rank */
		partid = get_task_partid(p);
		//pr_info("task_rank: p=%d, partid=%d, rank=%d, cpuqos_perf_mode=%d\n",
		//	p->pid, partid, rank, cpuqos_perf_mode);
		goto out;
	} else {
		/* group rank */
		rcu_read_lock();
		css = task_css(p, cpuqos_subsys_id);
		partid = mpam_map_css_partid(css);
		rcu_read_unlock();
		//pr_info("group_rank: p=%d, css->id=%d, partid=%d, rank=%d, cpuqos_perf_mode=%d\n",
		//	p->pid, css->id, partid, rank, cpuqos_perf_mode);
		goto out;
	}
out:
	return partid;
#else
	return -1;
#endif
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
	//pr_info("partid=%d\n", partid);
}

/*
 * Sync @p's associated PARTID with this CPU's register.
 */
static void mpam_sync_task(struct task_struct *p)
{
	mpam_write_partid(mpam_map_task_partid(p));
}

/*
 * Same as mpam_sync_task(), with a pre-filter for the current task.
 */
static void mpam_sync_current(void *task)
{
	if (task && task != current)
		return;

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
	int new_partid;

	if ((group_id >= ARRAY_SIZE(mpam_path_partid_map)) || (group_id < 0))
		return -1;

	css_id = mpam_group_css_map[group_id];
	if (css_id < 0)
		return -1;

	if (cpuqos_perf_mode == DISABLE) {
		new_partid = DEF_PARTID;
	} else {
		if (set)
			new_partid = CT_PARTID;
		else
			new_partid = NCT_PARTID;
	}

	if (mpam_css_partid_map[css_id] != new_partid)
		mpam_css_partid_map[css_id] = new_partid;

	pr_info("%s: group_id=%d, css->id=%d, set=%d, cpuqos_perf_mode=%d, new_partid=%d, mpam_partid=%d\n",
		__func__, group_id, css_id, set, cpuqos_perf_mode,
		new_partid, mpam_css_partid_map[css_id]);

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

	if (cpuqos_perf_mode == DISABLE)
		return -1;

	rcu_read_lock();
	p = find_task_by_vpid(pid);

	if (p)
		get_task_struct(p);
	rcu_read_unlock();

	if (!p)
		return -1;

	if (set) {
		p->android_vendor_data1[2] = TASK_RANK;
		if (get_task_partid(p) != CT_PARTID)
			mpam_kick_task(p, CT_PARTID);
	} else {
		p->android_vendor_data1[2] = GROUP_RANK;
		mpam_kick_task(p, mpam_map_task_partid(p));
	}

	pr_info("%s: p=%d, set=%d, cpuqos_perf_mode=%d, partid=%d\n",
		__func__, p->pid, set, cpuqos_perf_mode,
		mpam_map_task_partid(p));
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

	pr_info("%s: cpuqos mode=%d", __func__, mode);
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
	mpam_sync_task(next);
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

			pr_info("i=%d, path=%s, mpam_path=%s, css->id=%d, group_css=%d, partid_map=%d\n",
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

	smp_call_function(mpam_reset_partid, NULL, 1);
	cleanup_cpuqos_common_sysfs();
}

module_init(mpam_proto_init);
module_exit(mpam_proto_exit);
