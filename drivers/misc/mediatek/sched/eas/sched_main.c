// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/seq_file.h>
#include <linux/energy_model.h>
#include <linux/jump_label.h>
#include <trace/events/sched.h>
#include <trace/events/task.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/hung_task.h>
#include <sched/sched.h>
#include "common.h"
#include "eas_plus.h"
#include "sched_sys_common.h"
#include "sugov/cpufreq.h"

#define CREATE_TRACE_POINTS
#include "eas_trace.h"

#define TAG "EAS_IOCTL"

int mtk_sched_asym_cpucapacity  =  1;

static inline void sched_asym_cpucapacity_init(void)
{
	struct perf_domain *pd;
	struct root_domain *rd;
	int pd_count = 0;

	preempt_disable();
	rd = cpu_rq(smp_processor_id())->rd;

	rcu_read_lock();
	pd = rcu_dereference(rd->pd);
	for (; pd; pd = pd->next) {
		pd_count++;
		is_most_powerful_pd(pd);
	}
	rcu_read_unlock();
	preempt_enable();
	if (pd_count <= 1) {
		mtk_sched_asym_cpucapacity = 0;
		clear_powerful_pd();
	}
}

static void sched_task_util_hook(void *data, struct sched_entity *se)
{
	if (trace_sched_task_util_enabled()) {
		struct task_struct *p;
		struct sched_avg *sa;

		if (!entity_is_task(se))
			return;

		p = container_of(se, struct task_struct, se);
		sa = &se->avg;

		trace_sched_task_util(p->pid,
				sa->util_avg, sa->util_est.enqueued, sa->util_est.ewma);
	}
}

static void sched_task_uclamp_hook(void *data, struct sched_entity *se)
{
	if (trace_sched_task_uclamp_enabled()) {
		struct task_struct *p;
		struct sched_avg *sa;
		struct util_est ue;
		struct uclamp_se *uc_min_req, *uc_max_req;
		unsigned long util;

		if (!entity_is_task(se))
			return;

		p = container_of(se, struct task_struct, se);
		sa = &se->avg;
		ue = READ_ONCE(se->avg.util_est);
		util = max(ue.ewma, ue.enqueued);
		util = max(util, READ_ONCE(se->avg.util_avg));
		uc_min_req = &p->uclamp_req[UCLAMP_MIN];
		uc_max_req = &p->uclamp_req[UCLAMP_MAX];

		trace_sched_task_uclamp(p->pid, util,
				p->uclamp[UCLAMP_MIN].active,
				p->uclamp[UCLAMP_MIN].value, p->uclamp[UCLAMP_MAX].value,
				uc_min_req->user_defined, uc_min_req->value,
				uc_max_req->user_defined, uc_max_req->value);
	}
}

static int enqueue;
static int dequeue;
static void sched_queue_task_hook(void *data, struct rq *rq, struct task_struct *p, int flags)
{
	int cpu = rq->cpu;
	int type = *(int *)data;
#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEBUG)
	u64 ts[2];

	ts[0] = sched_clock();
#endif

	if (trace_sched_queue_task_enabled()) {
		unsigned long util = READ_ONCE(rq->cfs.avg.util_avg);

		util = max_t(unsigned long, util,
			     READ_ONCE(rq->cfs.avg.util_est.enqueued));

		trace_sched_queue_task(cpu, p->pid, type, util,
				rq->uclamp[UCLAMP_MIN].value, rq->uclamp[UCLAMP_MAX].value,
				p->uclamp[UCLAMP_MIN].value, p->uclamp[UCLAMP_MAX].value);
	}

#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEBUG)
	ts[1] = sched_clock();
	if ((ts[1] - ts[0] > 500000ULL) && in_hardirq()) {
		printk_deferred("%s duration %llu, ts[0]=%llu, ts[1]=%llu\n",
				__func__, ts[1] - ts[0], ts[0], ts[1]);
	}
#endif
}

#if IS_ENABLED(CONFIG_DETECT_HUNG_TASK)
static void mtk_check_d_tasks(void *data, struct task_struct *p,
				unsigned long t, bool *need_check)
{
	unsigned long pending_stime = 0;
	unsigned long switch_count = p->nvcsw + p->nivcsw;

	*need_check = true;
	/*
	 * Reset the migration_pending start time
	 */
	if (!p->migration_pending || switch_count != p->last_switch_count) {
		p->android_vendor_data1[4] = 0;
		return;
	}
	/*
	 * Record the migration_pending start time
	 */
	if (p->migration_pending && p->android_vendor_data1[4] == 0) {
		p->android_vendor_data1[4] = jiffies;
		*need_check = false;
		return;
	}
	/*
	 * Check the whether migration time is time out or not
	 */
	pending_stime = p->android_vendor_data1[4];
	if (time_is_after_jiffies(pending_stime + t * HZ)) {
		*need_check = false;
	} else {
		*need_check = true;
		pr_info("task flags:0x%08lx migration_flags:0x%08lx mig_dis %d\n",
			p->flags, p->migration_flags, is_migration_disabled(p));
	}
}
#endif

static void mtk_sched_trace_init(void)
{
	int ret = 0;

	enqueue = 1;
	dequeue = -1;

	ret = register_trace_android_rvh_enqueue_task(sched_queue_task_hook, &enqueue);
	if (ret)
		pr_info("register android_rvh_enqueue_task failed!\n");
	ret = register_trace_android_rvh_dequeue_task(sched_queue_task_hook, &dequeue);
	if (ret)
		pr_info("register android_rvh_dequeue_task failed!\n");

	ret = register_trace_pelt_se_tp(sched_task_util_hook, NULL);
	if (ret)
		pr_info("register sched_task_util_hook failed!\n");

	ret = register_trace_pelt_se_tp(sched_task_uclamp_hook, NULL);
	if (ret)
		pr_info("register sched_task_uclamp_hook failed!\n");
}

static void mtk_sched_trace_exit(void)
{
	unregister_trace_pelt_se_tp(sched_task_util_hook, NULL);
	unregister_trace_pelt_se_tp(sched_task_uclamp_hook, NULL);
}

static unsigned long easctl_copy_from_user(void *pvTo,
		const void __user *pvFrom, unsigned long ulBytes)
{
	if (access_ok(pvFrom, ulBytes))
		return __copy_from_user(pvTo, pvFrom, ulBytes);

	return ulBytes;
}

static unsigned long easctl_copy_to_user(void __user *pvTo,
		const void *pvFrom, unsigned long ulBytes)
{
	if (access_ok(pvTo, ulBytes))
		return __copy_to_user(pvTo, pvFrom, ulBytes);

	return ulBytes;
}

/*--------------------SYNC------------------------*/
static int eas_show(struct seq_file *m, void *v)
{
	return 0;
}

static int eas_open(struct inode *inode, struct file *file)
{
	return single_open(file, eas_show, inode->i_private);
}

static long eas_ioctl_impl(struct file *filp,
		unsigned int cmd, unsigned long arg, void *pKM)
{
	ssize_t ret = 0;

	unsigned int sync;
	unsigned int val;
	struct cpumask *cpumask_ptr;

	switch (cmd) {
	case EAS_SYNC_SET:
		if (easctl_copy_from_user(&sync, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_wake_sync(sync);
		break;
	case EAS_SYNC_GET:
		sync = get_wake_sync();
		if (easctl_copy_to_user((void *)arg, &sync, sizeof(unsigned int)))
			return -1;
		break;
	case EAS_PERTASK_LS_SET:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_uclamp_min_ls(val);
		break;
	case EAS_PERTASK_LS_GET:
		val = get_uclamp_min_ls();
		if (easctl_copy_to_user((void *)arg, &val, sizeof(unsigned int)))
			return -1;
		break;
	case EAS_ACTIVE_MASK_GET:
		val = __cpu_active_mask.bits[0];
		if (easctl_copy_to_user((void *)arg, &val, sizeof(unsigned int)))
			return -1;
		break;
	case EAS_NEWLY_IDLE_BALANCE_INTERVAL_SET:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_newly_idle_balance_interval_us(val);
		break;
	case EAS_NEWLY_IDLE_BALANCE_INTERVAL_GET:
		val = get_newly_idle_balance_interval_us();
		if (easctl_copy_to_user((void *)arg, &val, sizeof(unsigned int)))
			return -1;
		break;
	case EAS_GET_THERMAL_HEADROOM_INTERVAL_SET:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_get_thermal_headroom_interval_tick(val);
		break;
	case EAS_GET_THERMAL_HEADROOM_INTERVAL_GET:
		val = get_thermal_headroom_interval_tick();
		if (easctl_copy_to_user((void *)arg, &val, sizeof(unsigned int)))
			return -1;
		break;
	case EAS_SET_SYSTEM_MASK:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_system_cpumask_int(val);
		break;
	case EAS_GET_SYSTEM_MASK:
		cpumask_ptr = get_system_cpumask();
		val = cpumask_ptr->bits[0];
		if (easctl_copy_to_user((void *)arg, &val, sizeof(unsigned int)))
			return -1;
		break;
	case EAS_SBB_ALL_SET:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_sbb(SBB_ALL, 0, true);
		break;
	case EAS_SBB_ALL_UNSET:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_sbb(SBB_ALL, 0, false);
		break;
	case EAS_SBB_GROUP_SET:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_sbb(SBB_GROUP, val, true);
		break;
	case EAS_SBB_GROUP_UNSET:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_sbb(SBB_GROUP, val, false);
		break;
	case EAS_SBB_TASK_SET:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_sbb(SBB_TASK, val, true);
		break;
	case EAS_SBB_TASK_UNSET:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_sbb(SBB_TASK, val, false);
		break;
	case EAS_SBB_ACTIVE_RATIO:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_sbb_active_ratio(val);
		break;
	default:
		pr_debug(TAG "%s %d: unknown cmd %x\n",
			__FILE__, __LINE__, cmd);
		ret = -1;
		goto ret_ioctl;
	}

ret_ioctl:
	return ret;
}

static long eas_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	return eas_ioctl_impl(filp, cmd, arg, NULL);
}

static const struct proc_ops eas_Fops = {
	.proc_ioctl = eas_ioctl,
	.proc_compat_ioctl = eas_ioctl,
	.proc_open = eas_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int __init mtk_scheduler_init(void)
{
	struct proc_dir_entry *pe, *parent;
	int ret = 0;

	ret = init_sched_common_sysfs();
	if (ret)
		return ret;

#if IS_ENABLED(CONFIG_MTK_EAS)
	mtk_freq_limit_notifier_register();

	ret = init_sram_info();
	if (ret)
		return ret;

	ret = register_trace_android_rvh_find_busiest_group(
			mtk_find_busiest_group, NULL);
	if (ret)
		pr_info("register android_rvh_find_busiest_group failed\n");

	ret = register_trace_android_rvh_find_energy_efficient_cpu(
			mtk_find_energy_efficient_cpu, NULL);
	if (ret)
		pr_info("register android_rvh_find_energy_efficient_cpu failed\n");


	ret = register_trace_android_rvh_cpu_overutilized(
			mtk_cpu_overutilized, NULL);
	if (ret)
		pr_info("register trace_android_rvh_cpu_overutilized failed\n");


	ret = register_trace_android_rvh_tick_entry(
			mtk_tick_entry, NULL);
	if (ret)
		pr_info("register android_rvh_tick_entry failed\n");


	ret = register_trace_android_vh_set_wake_flags(
			mtk_set_wake_flags, NULL);
	if (ret)
		pr_info("register android_vh_set_wake_flags failed\n");


	ret = register_trace_android_rvh_update_cpu_capacity(
			mtk_update_cpu_capacity, NULL);
	if (ret)
		pr_info("register android_rvh_update_cpu_capacity failed\n");

	ret = register_trace_pelt_rt_tp(mtk_pelt_rt_tp, NULL);
	if (ret)
		pr_info("register mtk_pelt_rt_tp hooks failed, returned %d\n", ret);

	ret = register_trace_android_rvh_schedule(mtk_sched_switch, NULL);
	if (ret)
		pr_info("register mtk_sched_switch hooks failed, returned %d\n", ret);

#if IS_ENABLED(CONFIG_MTK_NEWIDLE_BALANCE)
	ret = register_trace_android_rvh_sched_newidle_balance(
			mtk_sched_newidle_balance, NULL);
	if (ret)
		pr_info("register android_rvh_sched_newidle_balance failed\n");
#endif

	init_system_cpumask();

	pr_debug(TAG"Start to init eas_ioctl driver\n");
	parent = proc_mkdir("easmgr", NULL);
	pe = proc_create("eas_ioctl", 0664, parent, &eas_Fops);
	if (!pe) {
		pr_debug(TAG"%s failed with %d\n",
				"Creating file node ",
				ret);
		ret = -ENOMEM;
		goto out_wq;
	}

#endif

	ret = register_trace_android_vh_scheduler_tick(hook_scheduler_tick, NULL);
	if (ret)
		pr_info("scheduler: register scheduler_tick hooks failed, returned %d\n", ret);

	ret = register_trace_android_rvh_after_enqueue_task(mtk_hook_after_enqueue_task, NULL);
	if (ret)
		pr_info("register android_rvh_after_enqueue_task failed, returned %d\n", ret);

#if IS_ENABLED(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)
	ret = register_trace_android_rvh_new_task_stats(rotat_task_stats, NULL);
	if (ret)
		pr_info("register android_rvh_new_task_stats failed, returned %d\n", ret);

	ret = register_trace_task_newtask(rotat_task_newtask, NULL);
	if (ret)
		pr_info("register trace_task_newtask failed, returned %d\n", ret);
#endif
	ret = register_trace_android_rvh_select_task_rq_rt(mtk_select_task_rq_rt, NULL);
	if (ret)
		pr_info("register mtk_select_task_rq_rt hooks failed, returned %d\n", ret);


	ret = register_trace_android_rvh_find_lowest_rq(mtk_find_lowest_rq, NULL);
	if (ret)
		pr_info("register find_lowest_rq hooks failed, returned %d\n", ret);


#if IS_ENABLED(CONFIG_DETECT_HUNG_TASK)
	ret = register_trace_android_vh_check_uninterruptible_tasks(mtk_check_d_tasks, NULL);
	if (ret)
		pr_info("register mtk_check_d_tasks hooks failed, returned %d\n", ret);
#endif

	sched_asym_cpucapacity_init();

	mtk_sched_trace_init();

#if IS_ENABLED(CONFIG_MTK_CORE_PAUSE)
	sched_pause_init();
#endif


out_wq:
	return ret;

}

static void __exit mtk_scheduler_exit(void)
{
	mtk_sched_trace_exit();
	unregister_trace_android_vh_scheduler_tick(hook_scheduler_tick, NULL);
#if IS_ENABLED(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)
	unregister_trace_task_newtask(rotat_task_newtask, NULL);
#endif
	cleanup_sched_common_sysfs();
}

module_init(mtk_scheduler_init);
module_exit(mtk_scheduler_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek scheduler");
MODULE_AUTHOR("MediaTek Inc.");
