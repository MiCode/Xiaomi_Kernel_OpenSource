// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/kthread.h>
#include <linux/sched/task.h>
#include <asm-generic/bug.h>

#include "mtk_hps_internal.h"
#include "mtk_hps.h"
#include <trace/events/sched.h>
#include <mt-plat/aee.h>
/*
 *#include <trace/events/mtk_events.h>
 *#include <mt-plat/met_drv.h>
 *#include <mt-plat/mtk_ram_console.h>
 *#include <mt-plat/met_drv.h>
 */

#define TLP_THRESHOLD 250
/*
 * static
 */
#define STATIC
/* #define STATIC static */
#define MS_TO_NS(x)     (x * 1E6L)
static unsigned long long hps_cancel_time;
static ktime_t ktime;
static unsigned int hps_cpu_load_info[10];
static int hps_load_cnt[10];
static DEFINE_SPINLOCK(load_info_lock);

/*
 * hps timer callback
 */
static void  _hps_timer_callback(struct timer_list *t)
{
	int ret;
	/*hps_warn("_hps_timer_callback\n"); */
	if (hps_ctxt.tsk_struct_ptr) {
		ret = wake_up_process(hps_ctxt.tsk_struct_ptr);
		if (!ret)
			pr_notice("[INFO] hps task has waked up[%d]\n", ret);
	} else {
		pr_notice("hps ptr is NULL\n");
	}
}

static long hps_get_current_time_ms(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return ((t.tv_sec & 0xFFF) * 1000000 + t.tv_usec) / 1000;
}

unsigned int hps_get_hvytsk(unsigned int cluster_id)
{
	if (cluster_id >= hps_sys.cluster_num)
		return 0xFFEE;
	else
		return hps_sys.cluster_info[cluster_id].hvyTsk_value;
}

unsigned int hps_get_bigtsk(unsigned int cluster_id)
{
	if (cluster_id >= hps_sys.cluster_num)
		return 0xFFEE;
	else
		return hps_sys.cluster_info[cluster_id].bigTsk_value;
}

unsigned int hps_get_per_cpu_load(int cpu, int isReset)
{
	unsigned int ret;

	spin_lock(&load_info_lock);
	if (hps_load_cnt[cpu])
		ret = hps_cpu_load_info[cpu] / hps_load_cnt[cpu];
	else
		ret = hps_cpu_load_info[cpu];
	if (isReset) {
		hps_cpu_load_info[cpu] = 0;
		hps_load_cnt[cpu] = 0;
	}
	spin_unlock(&load_info_lock);
	return ret;
}
/*
 * hps task main loop
 */
static int _hps_task_main(void *data)
{
	int cnt = 0;
	void (*algo_func_ptr)(void);

	unsigned int cpu, first_cpu, i;
	ktime_t enter_ktime;

	enter_ktime = ktime_get();
	aee_rr_rec_hps_cb_enter_times((u64) ktime_to_ms(enter_ktime));
	aee_rr_rec_hps_cb_footprint(0);
	aee_rr_rec_hps_cb_fp_times(0);

	hps_ctxt_print_basic(1);

	algo_func_ptr = hps_algo_main;

	while (1) {
		/* TODO: showld we do dvfs? */
		/* struct cpufreq_policy *policy; */
		/* policy = cpufreq_cpu_get(0); */
		/* dbs_freq_increase(policy, policy->max); */
		/* cpufreq_cpu_put(policy); */
#ifdef CONFIG_MTK_CPU_ISOLATION
		if (hps_ctxt.wake_up_by_fasthotplug) {

			mutex_lock(&hps_ctxt.lock);
			struct cpumask cpu_down_cpumask;

			cpumask_setall(&cpu_down_cpumask);
			cpumask_clear_cpu(hps_ctxt.root_cpu,
				&cpu_down_cpumask);
			cpu_down_by_mask(&cpu_down_cpumask);

			hps_ctxt.wake_up_by_fasthotplug = 0;
			mutex_unlock(&hps_ctxt.lock);
			goto HPS_WAIT_EVENT;
		}
#endif
ACAO_HPS_START:
	aee_rr_rec_hps_cb_footprint(1);
	aee_rr_rec_hps_cb_fp_times((u64) ktime_to_ms(ktime_get()));

	mutex_lock(&hps_ctxt.para_lock);
	memcpy(&hps_ctxt.online_core, &hps_ctxt.online_core_req,
		sizeof(cpumask_var_t));
	mutex_unlock(&hps_ctxt.para_lock);

	aee_rr_rec_hps_cb_footprint(2);
	aee_rr_rec_hps_cb_fp_times((u64) ktime_to_ms(ktime_get()));
	/*Debgu message dump*/
	for (i = 0 ; i < 8 ; i++) {
		if (cpumask_test_cpu(i, hps_ctxt.online_core))
			pr_info("CPU %d ==>1\n", i);
		else
			pr_info("CPU %d ==>0\n", i);
	}

	if (!cpumask_empty(hps_ctxt.online_core)) {
		aee_rr_rec_hps_cb_footprint(3);
		aee_rr_rec_hps_cb_fp_times((u64) ktime_to_ms(ktime_get()));
		first_cpu = cpumask_first(hps_ctxt.online_core);
		if (first_cpu >= setup_max_cpus) {
			pr_notice("PPM request without first cpu online!\n");
			goto ACAO_HPS_END;
		}

		if (!cpu_online(first_cpu))
			cpu_up(first_cpu);
		aee_rr_rec_hps_cb_footprint(4);
		aee_rr_rec_hps_cb_fp_times((u64) ktime_to_ms(ktime_get()));

		for_each_possible_cpu(cpu) {
			if (cpumask_test_cpu(cpu, hps_ctxt.online_core)) {
				if (!cpu_online(cpu)) {
					aee_rr_rec_hps_cb_footprint(5);
					aee_rr_rec_hps_cb_fp_times(
					(u64) ktime_to_ms(ktime_get()));
					cpu_up(cpu);

					aee_rr_rec_hps_cb_footprint(6);
					aee_rr_rec_hps_cb_fp_times(
					(u64) ktime_to_ms(ktime_get()));
				}
			} else {
				if (cpu_online(cpu)) {
					aee_rr_rec_hps_cb_footprint(7);
					aee_rr_rec_hps_cb_fp_times(
					(u64) ktime_to_ms(ktime_get()));
					cpu_down(cpu);
					aee_rr_rec_hps_cb_footprint(8);
					aee_rr_rec_hps_cb_fp_times(
					(u64) ktime_to_ms(ktime_get()));
				}
			}
			if (!cpumask_equal(hps_ctxt.online_core,
			hps_ctxt.online_core_req))
				goto ACAO_HPS_START;
		}
	}
	aee_rr_rec_hps_cb_footprint(9);
	aee_rr_rec_hps_cb_fp_times((u64) ktime_to_ms(ktime_get()));

ACAO_HPS_END:
	aee_rr_rec_hps_cb_footprint(10);
	aee_rr_rec_hps_cb_fp_times((u64) ktime_to_ms(ktime_get()));
	set_current_state(TASK_INTERRUPTIBLE);
	aee_rr_rec_hps_cb_footprint(11);
	aee_rr_rec_hps_cb_fp_times((u64) ktime_to_ms(ktime_get()));
	schedule();
		if (kthread_should_stop())
			break;

	}			/* while(1) */

	hps_warn("%s, cnt:%08d\n", __func__, cnt++);
	return 0;
}

/*
 * hps task control interface
 */
int hps_task_start(void)
{

	if (hps_ctxt.tsk_struct_ptr == NULL) {
		/*struct sched_param param = */
		/*{.sched_priority = HPS_TASK_RT_PRIORITY };*/
		hps_ctxt.tsk_struct_ptr =
		kthread_create(_hps_task_main, NULL, "hps_main");
		if (IS_ERR(hps_ctxt.tsk_struct_ptr))
			return PTR_ERR(hps_ctxt.tsk_struct_ptr);

		/*sched_setscheduler_nocheck(*/
		/*hps_ctxt.tsk_struct_ptr, SCHED_FIFO, &param); */
		set_user_nice(hps_ctxt.tsk_struct_ptr,
				HPS_TASK_NORMAL_PRIORITY);
		get_task_struct(hps_ctxt.tsk_struct_ptr);
		wake_up_process(hps_ctxt.tsk_struct_ptr);
		hps_warn("%s, ptr: %p, pid: %d\n",
			__func__,
			hps_ctxt.tsk_struct_ptr,
			hps_ctxt.tsk_struct_ptr->pid);
	} else {
		hps_warn("hps task already exist, ptr: %p, pid: %d\n",
			hps_ctxt.tsk_struct_ptr,
			hps_ctxt.tsk_struct_ptr->pid);
	}
	return 0;
}

void hps_task_stop(void)
{
	if (hps_ctxt.tsk_struct_ptr) {
		kthread_stop(hps_ctxt.tsk_struct_ptr);
		put_task_struct(hps_ctxt.tsk_struct_ptr);
		hps_ctxt.tsk_struct_ptr = NULL;
	}
}

void hps_task_wakeup_nolock(void)
{
	int ret;

	if (hps_ctxt.tsk_struct_ptr) {
		atomic_set(&hps_ctxt.is_ondemand, 1);
		if (hps_ctxt.periodical_by == HPS_PERIODICAL_BY_WAIT_QUEUE)
			wake_up(&hps_ctxt.wait_queue);
		else if ((hps_ctxt.periodical_by == HPS_PERIODICAL_BY_TIMER)
			 || (hps_ctxt.periodical_by ==
			HPS_PERIODICAL_BY_HR_TIMER)) {
			ret = wake_up_process(hps_ctxt.tsk_struct_ptr);
			if (!ret) {
				pr_notice(
				"[%s]hps task is in running state, ret = %d\n",
				__func__, ret);
				atomic_set(&hps_ctxt.is_ondemand, 0);
			}
		}
	}
}

void hps_task_wakeup(void)
{
	mutex_lock(&hps_ctxt.lock);

	hps_task_wakeup_nolock();

	mutex_unlock(&hps_ctxt.lock);
}

static void ppm_limit_callback(struct ppm_client_req req)
{
	struct ppm_client_req *p = (struct ppm_client_req *)&req;

	mutex_lock(&hps_ctxt.para_lock);
	memcpy(&hps_ctxt.online_core_req, p->online_core,
		sizeof(cpumask_var_t));
	mutex_unlock(&hps_ctxt.para_lock);
	hps_task_wakeup_nolock();
}

/*
 * init
 */
int hps_core_init(void)
{
	int r = 0;

	/* init and start task */
	r = hps_task_start();
	if (r) {
		hps_error("hps_task_start fail(%d)\n", r);
		return r;
	}
	/* register PPM callback */
	mt_ppm_register_client(PPM_CLIENT_HOTPLUG, &ppm_limit_callback);

	return r;
}

/*
 * deinit
 */
int hps_core_deinit(void)
{
	int r = 0;

	hps_warn("%s\n", __func__);
	if (hps_ctxt.periodical_by == HPS_PERIODICAL_BY_TIMER) {
		/*deinit timer */
		del_timer_sync(&hps_ctxt.tmr_list);
	} else if (hps_ctxt.periodical_by == HPS_PERIODICAL_BY_HR_TIMER) {
		/*deinit timer */
		r = hrtimer_cancel(&hps_ctxt.hr_timer);
		if (r)
			hps_error("hps hr timer delete error!\n");
	}

	hps_task_stop();
	return r;
}

int hps_del_timer(void)
{
	if (!hps_cancel_time)
		hps_cancel_time = hps_get_current_time_ms();
	if (hps_ctxt.periodical_by == HPS_PERIODICAL_BY_TIMER) {
		/*deinit timer */
		del_timer_sync(&hps_ctxt.tmr_list);
	} else if (hps_ctxt.periodical_by == HPS_PERIODICAL_BY_HR_TIMER) {
		hrtimer_cancel(&hps_ctxt.hr_timer);
	}
	return 0;
}

int hps_restart_timer(void)
{
	unsigned long long time_differ = 0;

	time_differ = hps_get_current_time_ms() - hps_cancel_time;
	if (hps_ctxt.periodical_by == HPS_PERIODICAL_BY_TIMER) {
		/*init timer */
		timer_setup(&hps_ctxt.tmr_list, _hps_timer_callback, 0);
		/*init_timer_deferrable(&hps_ctxt.tmr_list); */

		if (time_differ >= HPS_TIMER_INTERVAL_MS) {
			hps_ctxt.tmr_list.expires =
			    jiffies + msecs_to_jiffies(HPS_TIMER_INTERVAL_MS);
			add_timer(&hps_ctxt.tmr_list);
			hps_task_wakeup_nolock();
			hps_cancel_time = 0;
		} else {
			hps_ctxt.tmr_list.expires =
			jiffies +
			msecs_to_jiffies(HPS_TIMER_INTERVAL_MS -
							time_differ);
			add_timer(&hps_ctxt.tmr_list);
		}
	} else if (hps_ctxt.periodical_by == HPS_PERIODICAL_BY_HR_TIMER) {
		hrtimer_start(&hps_ctxt.hr_timer, ktime, HRTIMER_MODE_REL);
		if (time_differ >= HPS_TIMER_INTERVAL_MS) {
			hps_task_wakeup_nolock();
			hps_cancel_time = 0;
		}
	}
	return 0;
}
