/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/kthread.h>
#include <asm-generic/bug.h>

#include "mtk_hps_internal.h"
#include "mtk_hps.h"
#include <trace/events/mtk_events.h>
#include <trace/events/sched.h>
#include <mt-plat/aee.h>
#include <mt-plat/met_drv.h>
#include <mt-plat/mtk_ram_console.h>

#include <mt-plat/met_drv.h>

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
static int _hps_timer_callback(unsigned long data)
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
	return HRTIMER_NORESTART;
}

static long int hps_get_current_time_ms(void)
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
#if 0 /*removed : #ifndef CONFIG_MTK_ACAO_SUPPORT*/
static void hps_get_sysinfo(void)
{
	unsigned int cpu;
	char str1[64];
	char str2[64];
	int i, j, idx;
	char *str1_ptr = str1;
	char *str2_ptr = str2;
	int scaled_tlp, avg_tlp;
	/* AHT: Average heavy task */
	int lastpoll_htask1 = 0, lastpoll_htask2 = 0;
	int avg_htask = 0, avg_htask_scal = 0;
	int max;
	int heavy_task_threshold = get_heavy_task_threshold();
	int avg_heavy_task_threshold = get_avg_heavy_task_threshold();
	int lastpoll_htask_idx1_1 = 0;
	int lastpoll_htask_idx1_2 = 0;
	int lastpoll_htask_idx2_1 = 0;
	int lastpoll_htask_idx2_2 = 0;
	int avg_htask_scal_idx1 = 0;
	int avg_htask_scal_idx2 = 0;
	int max_idx1 = 0;
	unsigned int big_task_L, big_task_B;
	/* sched-assist hotplug: */
	int sched_util = 0;
	unsigned int rel_load, abs_load;
	static int prev_tlp;
	int win_tlp;

	hps_ctxt.cur_loads = 0;
	str1_ptr = str1;
	str2_ptr = str2;
	for (idx = 0; idx < hps_sys.cluster_num; idx++) {
		/* sched-assist hotplug */
		unsigned long util, cap;

		sched_get_cluster_util(idx, &util, &cap);
		sched_util += util;

		hps_sys.cluster_info[idx].loading = 0;
		hps_sys.cluster_info[idx].rel_load = 0;
		hps_sys.cluster_info[idx].abs_load = 0;
		hps_sys.cluster_info[idx].sched_load = util;
	}
	/*for_each_possible_cpu(cpu) {*/
	for_each_online_cpu(cpu) {
		sched_get_percpu_load2(cpu, 1, &rel_load, &abs_load);
		if (cpu < 4) {
			per_cpu(hps_percpu_ctxt, cpu).load = rel_load;
			hps_sys.cluster_info[cpu/4].rel_load +=
			per_cpu(hps_percpu_ctxt, cpu).load;
			hps_sys.cluster_info[cpu/4].abs_load += abs_load;
		} else {
			per_cpu(hps_percpu_ctxt, cpu).load = abs_load;
			hps_sys.cluster_info[cpu/4].abs_load +=
			per_cpu(hps_percpu_ctxt, cpu).load;
			hps_sys.cluster_info[cpu/4].rel_load += rel_load;
		}
		hps_ctxt.cur_loads += per_cpu(hps_percpu_ctxt, cpu).load;

		for (idx = 0 ; idx < hps_sys.cluster_num; idx++) {
			if ((cpu >= hps_sys.cluster_info[idx].cpu_id_min) &&
				(cpu <= hps_sys.cluster_info[idx].cpu_id_max))
				hps_sys.cluster_info[idx].loading +=
				per_cpu(hps_percpu_ctxt, cpu).load;
		}

		if (hps_ctxt.cur_dump_enabled) {
			if (cpu_online(cpu))
				i = sprintf(str1_ptr, "%4u", 1);
			else
				i = sprintf(str1_ptr, "%4u", 0);
			str1_ptr += i;
			j = sprintf(str2_ptr, "%4u",
			per_cpu(hps_percpu_ctxt, cpu).load);
			str2_ptr += j;
		}
	}

	/*Get heavy task information */
	/*hps_ctxt.cur_nr_heavy_task = hps_cpu_get_nr_heavy_task(); */
	for (idx = 0; idx < hps_sys.cluster_num; idx++) {

		if (idx == 0) {
			met_tag_oneshot(0, "sched_util_cid0",
				hps_sys.cluster_info[idx].sched_load);
			met_tag_oneshot(0, "sched_load_rel0",
				hps_sys.cluster_info[idx].rel_load);
			met_tag_oneshot(0, "sched_load_abs0",
				hps_sys.cluster_info[idx].abs_load);
		} else if (idx == 1) {
			met_tag_oneshot(0, "sched_util_cid1",
				hps_sys.cluster_info[idx].sched_load);
			met_tag_oneshot(0, "sched_load_rel1",
				hps_sys.cluster_info[idx].rel_load);
			met_tag_oneshot(0, "sched_load_abs1",
				hps_sys.cluster_info[idx].abs_load);
		} else if (idx == 2) {
			met_tag_oneshot(0, "sched_util_cid2",
				hps_sys.cluster_info[idx].sched_load);
			met_tag_oneshot(0, "sched_load_rel2",
				hps_sys.cluster_info[idx].rel_load);
			met_tag_oneshot(0, "sched_load_abs2",
				hps_sys.cluster_info[idx].abs_load);
		}

		if (hps_ctxt.heavy_task_enabled)
#ifdef CONFIG_MTK_SCHED_RQAVG_US
		{
			if (idx == 0) {
				/* in cluster LL, heavy task by last_poll */
				hps_sys.cluster_info[idx].hvyTsk_value =
				sched_get_nr_heavy_task_by_threshold(idx,
				heavy_task_threshold);
			} else if (idx == 1) {
				/* in cluster L, heavy task by max of */
				/* average(w/o remainder) and last_poll */
				lastpoll_htask1 =
				sched_get_nr_heavy_task_by_threshold(idx,
				heavy_task_threshold);
				lastpoll_htask2 =
				sched_get_nr_heavy_running_avg(idx,
				&avg_htask_scal);
				lastpoll_htask_idx1_1 = lastpoll_htask1;
				lastpoll_htask_idx1_2 = lastpoll_htask2;
				avg_htask_scal_idx1 = avg_htask_scal;

				avg_htask = ((avg_htask_scal%100) >=
				avg_heavy_task_threshold) ?
				(avg_htask_scal/100+1):(avg_htask_scal/100);

				max_idx1 = max =  max(max(lastpoll_htask1,
				lastpoll_htask2), avg_htask);
				hps_sys.cluster_info[idx].hvyTsk_value = max;

				trace_sched_avg_heavy_task(lastpoll_htask1,
				lastpoll_htask2, avg_htask_scal, idx, max);
			} else if (idx == 2) {
				/* in cluster B, heavy task by max of */
				/* average(with L's remainder) and last_poll */
				lastpoll_htask1 =
				sched_get_nr_heavy_task_by_threshold(idx,
				heavy_task_threshold);
				lastpoll_htask2 =
				sched_get_nr_heavy_running_avg(idx,
				&avg_htask_scal);

				lastpoll_htask_idx2_1 = lastpoll_htask1 +
				lastpoll_htask_idx1_1;
				lastpoll_htask_idx2_2 = lastpoll_htask2 +
				lastpoll_htask_idx1_2;
				avg_htask_scal_idx2 = avg_htask_scal +
				avg_htask_scal_idx1;

				avg_htask = ((avg_htask_scal_idx2%100) >=
				avg_heavy_task_threshold) ?
				(avg_htask_scal_idx2/100+1) :
				(avg_htask_scal_idx2/100);

				max =  max(max(lastpoll_htask_idx2_1,
				lastpoll_htask_idx2_2), avg_htask);
				hps_sys.cluster_info[idx].hvyTsk_value =
				(max - max_idx1);

				trace_sched_avg_heavy_task(lastpoll_htask1,
				lastpoll_htask2, avg_htask_scal,
					idx, (max-max_idx1));
			} else
				WARN_ON(1);
		}
#else
		hps_sys.cluster_info[idx].hvyTsk_value = 0;
#endif
		else
			hps_sys.cluster_info[idx].hvyTsk_value = 0;
	}

#if 1 /*For workaround solution*/
	sched_big_task_nr(&big_task_L, &big_task_B);
	hps_sys.cluster_info[0].bigTsk_value = 0;
	hps_sys.cluster_info[1].bigTsk_value = big_task_L;
	hps_sys.cluster_info[2].bigTsk_value = big_task_B;
#endif
	/*Get sys TLP information */
	scaled_tlp = hps_cpu_get_tlp(&avg_tlp, &hps_ctxt.cur_iowait);

	/*
	 * scaled_tlp: tasks number of the last pill, which X 100.
	 * avg_tlp: average tasks number during the detection period.
	 * To pick max of scaled_tlp and avg_tlp.
	 */
	hps_ctxt.cur_tlp = max_t(int, scaled_tlp, (int)avg_tlp);

	/*
	 * For EAS evaluation
	 */

	/* consider TLP in 2 windows */
	win_tlp = (hps_ctxt.cur_tlp + prev_tlp)/2;
	/* LL: relative threshold */
#ifdef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
	if ((int)sched_util < sodi_limit && win_tlp < TLP_THRESHOLD) {
		/*
		 *  If CPU util && TLP in system is small or
		 *  only tiny task is running, a few CPU for it.
		 */
		hps_sys.cluster_info[0].up_threshold = DEF_CPU_UP_THRESHOLD;
		hps_sys.cluster_info[0].down_threshold =
			DEF_CPU_DOWN_THRESHOLD;
	} else {
#endif /* CONFIG_MTK_SCHED_EAS_POWER_SUPPORT */
		/* for more cores + low frequency policy */
		hps_sys.cluster_info[0].up_threshold = DEF_EAS_UP_THRESHOLD_0;
		hps_sys.cluster_info[0].down_threshold =
			DEF_EAS_DOWN_THRESHOLD_0;

		/* L: absolute threshold */
		hps_sys.cluster_info[1].up_threshold = DEF_EAS_UP_THRESHOLD_1;
		hps_sys.cluster_info[1].down_threshold =
			DEF_EAS_DOWN_THRESHOLD_1;

		/* B: absolute threshold */
		if (big_task_B > 1) {
			hps_sys.cluster_info[2].up_threshold =
				DEF_EAS_UP_THRESHOLD_2;
			hps_sys.cluster_info[2].down_threshold =
				DEF_EAS_DOWN_THRESHOLD_2;
		} else {
			hps_sys.cluster_info[2].up_threshold =
				DEF_CPU_UP_THRESHOLD;
			hps_sys.cluster_info[2].down_threshold =
				DEF_CPU_DOWN_THRESHOLD;
		}
#ifdef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
	}
#endif /* CONFIG_MTK_SCHED_EAS_POWER_SUPPORT */

	if (!hps_ctxt.eas_enabled) {
		for (idx = 0; idx < hps_sys.cluster_num; idx++) {
			hps_sys.cluster_info[idx].up_threshold =
				DEF_CPU_UP_THRESHOLD;
			hps_sys.cluster_info[idx].down_threshold =
				DEF_CPU_DOWN_THRESHOLD;
		}
	}

	prev_tlp = hps_ctxt.cur_tlp;

	/* [MET] debug for geekbench */
	met_tag_oneshot(0, "sched_tlp_cur", hps_ctxt.cur_tlp);

	mt_sched_printf(sched_log,
		"[heavy_task] :%s, scaled_tlp:%d, avg_tlp:%d, max:%d",
		__func__, scaled_tlp, (int)avg_tlp, (int)hps_ctxt.cur_tlp);
}
#endif
/*
 * hps task main loop
 */
static int _hps_task_main(void *data)
{
	int cnt = 0;
#if 0 /*removed : #ifndef CONFIG_MTK_ACAO_SUPPORT*/
	int idx;
	unsigned int total_big_task = 0;
	unsigned int total_hvy_task = 0;
#endif
	void (*algo_func_ptr)(void);

#if 1 /*removed : #ifdef CONFIG_MTK_ACAO_SUPPORT*/
	unsigned int cpu, first_cpu, i;
	ktime_t enter_ktime;

	enter_ktime = ktime_get();
	aee_rr_rec_hps_cb_enter_times((u64) ktime_to_ms(enter_ktime));
	aee_rr_rec_hps_cb_footprint(0);
	aee_rr_rec_hps_cb_fp_times(0);
#endif

	hps_ctxt_print_basic(1);

	algo_func_ptr = hps_algo_main;

	while (1) {
		/* TODO: showld we do dvfs? */
		/* struct cpufreq_policy *policy; */
		/* policy = cpufreq_cpu_get(0); */
		/* dbs_freq_increase(policy, policy->max); */
		/* cpufreq_cpu_put(policy); */
#ifdef CONFIG_CPU_ISOLATION
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
#if 1 /*removed : #ifdef CONFIG_MTK_ACAO_SUPPORT*/
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
#else
		/* if (!hps_ctxt.is_interrupt) { */

		/*Get sys status */
		mutex_lock(&hps_ctxt.lock);
		hps_get_sysinfo();
		total_hvy_task = total_big_task = 0;
		for (idx = 0; idx < hps_sys.cluster_num; idx++) {
			total_big_task +=
				hps_sys.cluster_info[idx].bigTsk_value;
			total_hvy_task +=
				hps_sys.cluster_info[idx].hvyTsk_value;
		}
		mutex_unlock(&hps_ctxt.lock);
		if (!hps_ctxt.is_interrupt ||
		    ((u64) ktime_to_ms(ktime_sub(ktime_get(),
				       hps_ctxt.hps_regular_ktime))) >=
				       HPS_TIMER_INTERVAL_MS) {

			mt_ppm_hica_update_algo_data(hps_ctxt.cur_loads, 0,
			hps_ctxt.cur_tlp);
			mt_smart_update_sysinfo(hps_ctxt.cur_loads,
			hps_ctxt.cur_tlp, total_big_task, total_hvy_task);
			/*Execute PPM main function */
			mt_ppm_main();

			if (hps_ctxt.is_interrupt)
				hps_ctxt.is_interrupt = 0;
			hps_ctxt.hps_regular_ktime = ktime_get();
		} else
			hps_ctxt.is_interrupt = 0;

		/*execute hotplug algorithm */
		(*algo_func_ptr) ();

#ifdef CONFIG_CPU_ISOLATION
HPS_WAIT_EVENT:
#endif
		if (hps_ctxt.periodical_by == HPS_PERIODICAL_BY_WAIT_QUEUE) {
			wait_event_timeout(hps_ctxt.wait_queue,
				atomic_read(&hps_ctxt.is_ondemand) != 0,
				msecs_to_jiffies(HPS_TIMER_INTERVAL_MS));
		} else if (hps_ctxt.periodical_by == HPS_PERIODICAL_BY_TIMER) {
			if (atomic_read(&hps_ctxt.is_ondemand) == 0) {
				mod_timer(&hps_ctxt.tmr_list,
				(jiffies +
				msecs_to_jiffies(HPS_TIMER_INTERVAL_MS)));
				set_current_state(TASK_INTERRUPTIBLE);
				schedule();
			}
		} else if (hps_ctxt.periodical_by ==
		HPS_PERIODICAL_BY_HR_TIMER) {

			if (atomic_read(&hps_ctxt.is_ondemand) == 0) {
				hrtimer_cancel(&hps_ctxt.hr_timer);
				hrtimer_start(&hps_ctxt.hr_timer,
					ktime, HRTIMER_MODE_REL);
				set_current_state(TASK_INTERRUPTIBLE);
				schedule();
			} else
				atomic_set(&hps_ctxt.is_ondemand, 0);
		}
#endif
		if (kthread_should_stop())
			break;

	}			/* while(1) */

	hps_warn("leave _hps_task_main, cnt:%08d\n", cnt++);
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
		hps_warn("hps_task_start success, ptr: %p, pid: %d\n",
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
#if 1 /*removed : #ifdef CONFIG_MTK_ACAO_SUPPORT*/
#if 1
	mutex_lock(&hps_ctxt.para_lock);
	memcpy(&hps_ctxt.online_core_req, p->online_core,
		sizeof(cpumask_var_t));
	mutex_unlock(&hps_ctxt.para_lock);
	hps_task_wakeup_nolock();
#else
	unsigned int cpu, first_cpu;
	int i = 0;

	pr_notice("[ACAO] PPM request....\n");
	for (i = 0 ; i < 8 ; i++) {
	if (cpumask_test_cpu(i, p->online_core))
		pr_notice("CPU %d ==>1\n", i);
	else
		pr_notice("CPU %d ==>0\n", i);
	}

	if (p->online_core) {
		first_cpu = cpumask_first(p->online_core);
	if (first_cpu >= setup_max_cpus) {
		pr_notice("PPM request without first cpu online!\n");
		return;
	}
	if (!cpu_online(first_cpu))
		cpu_up(first_cpu);
		for_each_possible_cpu(cpu) {
			if (cpumask_test_cpu(cpu, p->online_core)) {
				if (!cpu_online(cpu))
					cpu_up(cpu);
			} else {
				if (cpu_online(cpu))
					cpu_down(cpu);
			}
		}
	}
#endif
#else
	int i;

	mutex_lock(&hps_ctxt.para_lock);
	hps_sys.ppm_root_cluster = p->root_cluster;
	for (i = 0; i < p->cluster_num; i++) {
		/*
		 * hps_warn("ppm_limit_callback -> cluster%d:
		 *	has_advise_core = %d, [%d, %d]\n",
		 *	i, p->cpu_limit[i].has_advise_core,
		 *	p->cpu_limit[i].min_cpu_core,
		 *	p->cpu_limit[i].max_cpu_core);
		 */
#ifdef _TRACE_
		trace_ppm_limit_callback_update(i,
			p->cpu_limit[i].has_advise_core,
			p->cpu_limit[i].min_cpu_core,
			p->cpu_limit[i].max_cpu_core);
#endif
		if (!p->cpu_limit[i].has_advise_core) {
			hps_sys.cluster_info[i].ref_base_value =
				p->cpu_limit[i].min_cpu_core;
			hps_sys.cluster_info[i].ref_limit_value =
				p->cpu_limit[i].max_cpu_core;
		} else {
			hps_sys.cluster_info[i].ref_base_value =
			    hps_sys.cluster_info[i].ref_limit_value =
			    p->cpu_limit[i].advise_cpu_core;
		}
	}
	mutex_unlock(&hps_ctxt.para_lock);
	hps_ctxt.is_interrupt = 1;
	hps_task_wakeup_nolock();
#endif
}

/*
 * init
 */
int hps_core_init(void)
{
	int r = 0;

#if 0 /*removed : #ifndef CONFIG_MTK_ACAO_SUPPORT*/
	hps_warn("hps_core_init\n");
	if (hps_ctxt.periodical_by == HPS_PERIODICAL_BY_TIMER) {
		/*init timer */
		init_timer(&hps_ctxt.tmr_list);
		/*init_timer_deferrable(&hps_ctxt.tmr_list); */
		hps_ctxt.tmr_list.function = (void *)&_hps_timer_callback;
		hps_ctxt.tmr_list.data = (unsigned long)&hps_ctxt;
		hps_ctxt.tmr_list.expires = jiffies +
			msecs_to_jiffies(HPS_TIMER_INTERVAL_MS);
		add_timer(&hps_ctxt.tmr_list);
	} else if (hps_ctxt.periodical_by == HPS_PERIODICAL_BY_HR_TIMER) {
		ktime = ktime_set(0, MS_TO_NS(HPS_TIMER_INTERVAL_MS));
		/*init Hrtimer */
		hrtimer_init(&hps_ctxt.hr_timer, CLOCK_MONOTONIC,
			HRTIMER_MODE_REL);
		hps_ctxt.hr_timer.function = (void *)&_hps_timer_callback;
		hrtimer_start(&hps_ctxt.hr_timer, ktime, HRTIMER_MODE_REL);

	}
#endif
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

	hps_warn("hps_core_deinit\n");
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
#if 1
	if (!hps_cancel_time)
		hps_cancel_time = hps_get_current_time_ms();
	if (hps_ctxt.periodical_by == HPS_PERIODICAL_BY_TIMER) {
		/*deinit timer */
		del_timer_sync(&hps_ctxt.tmr_list);
	} else if (hps_ctxt.periodical_by == HPS_PERIODICAL_BY_HR_TIMER) {
		hrtimer_cancel(&hps_ctxt.hr_timer);
	}
#endif
	return 0;
}

int hps_restart_timer(void)
{
#if 1
	unsigned long long time_differ = 0;

	time_differ = hps_get_current_time_ms() - hps_cancel_time;
	if (hps_ctxt.periodical_by == HPS_PERIODICAL_BY_TIMER) {
		/*init timer */
		init_timer(&hps_ctxt.tmr_list);
		/*init_timer_deferrable(&hps_ctxt.tmr_list); */
		hps_ctxt.tmr_list.function = (void *)&_hps_timer_callback;
		hps_ctxt.tmr_list.data = (unsigned long)&hps_ctxt;

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
#if 1
		hrtimer_start(&hps_ctxt.hr_timer, ktime, HRTIMER_MODE_REL);
		if (time_differ >= HPS_TIMER_INTERVAL_MS) {
			hps_task_wakeup_nolock();
			hps_cancel_time = 0;
		}
#else
		if (time_differ >= HPS_TIMER_INTERVAL_MS) {
			/*init Hrtimer */
			hrtimer_start(&hps_ctxt.hr_timer, ktime,
				HRTIMER_MODE_REL);
			hps_task_wakeup_nolock();
			hps_cancel_time = 0;
		}
#endif
	}
#endif
	return 0;
}
