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
/**
* @file    mt_hotplug_strategy_core.c
* @brief   hotplug strategy(hps) - core
*/

/*============================================================================*/
/* Include files */
/*============================================================================*/
/* system includes */
#include <linux/kernel.h>
#include <linux/module.h>	/* MODULE_DESCRIPTION, MODULE_LICENSE */
#include <linux/init.h>		/* module_init, module_exit */
#include <linux/cpu.h>		/* cpu_up */
#include <linux/kthread.h>	/* kthread_create */
#include <linux/wakelock.h>	/* wake_lock_init */
#include <asm-generic/bug.h>	/* BUG_ON */

/* local includes */
#include "mt_hotplug_strategy_internal.h"
#include "mt_hotplug_strategy.h"

/* forward references */

/*============================================================================*/
/* Macro definition */
/*============================================================================*/
/*
 * static
 */
#define STATIC
/* #define STATIC static */
#define MS_TO_NS(x)     (x * 1E6L)
/*
 * config
 */

/*============================================================================*/
/* Local type definition */
/*============================================================================*/

/*============================================================================*/
/* Local function declarition */
/*============================================================================*/

/*============================================================================*/
/* Local variable definition */
/*============================================================================*/

/*============================================================================*/
/* Global variable definition */
/*============================================================================*/
static unsigned long long hps_cancel_time;
static ktime_t ktime;
/*============================================================================*/
/* Local function definition */
/*============================================================================*/
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
			pr_err("[INFO] hps task has waked up[%d]\n", ret);
	} else {
		pr_err("hps ptr is NULL\n");
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


static void hps_get_sysinfo(void)
{
	unsigned int cpu;
	char str1[64];
	char str2[64];
	int i, j, idx;
	char *str1_ptr = str1;
	char *str2_ptr = str2;

	/*
	 * calculate cpu loading
	 */
	hps_ctxt.cur_loads = 0;
	str1_ptr = str1;
	str2_ptr = str2;

	for_each_possible_cpu(cpu) {
		per_cpu(hps_percpu_ctxt, cpu).load = hps_cpu_get_percpu_load(cpu);
		hps_ctxt.cur_loads += per_cpu(hps_percpu_ctxt, cpu).load;

		if (hps_ctxt.cur_dump_enabled) {
			if (cpu_online(cpu))
				i = sprintf(str1_ptr, "%4u", 1);
			else
				i = sprintf(str1_ptr, "%4u", 0);
			str1_ptr += i;
			j = sprintf(str2_ptr, "%4u", per_cpu(hps_percpu_ctxt, cpu).load);
			str2_ptr += j;
		}
	}

	/*Get heavy task information */
	/*hps_ctxt.cur_nr_heavy_task = hps_cpu_get_nr_heavy_task(); */
	for (idx = 0; idx < hps_sys.cluster_num; idx++) {
		if (hps_ctxt.heavy_task_enabled)
			hps_sys.cluster_info[idx].hvyTsk_value = sched_get_nr_heavy_task2(idx);
		else
			hps_sys.cluster_info[idx].hvyTsk_value = 0;
	}

	/*Get sys TLP information */
	hps_cpu_get_tlp(&hps_ctxt.cur_tlp, &hps_ctxt.cur_iowait);

#if 0
	ppm_lock(&ppm_main_info.lock);
	ppm_hps_algo_data.ppm_cur_loads = hps_ctxt.cur_loads;
	ppm_hps_algo_data.ppm_cur_nr_heavy_task = hps_ctxt.cur_nr_heavy_task;
	ppm_hps_algo_data.ppm_cur_tlp = hps_ctxt.cur_tlp;
	ppm_unlock(&ppm_main_info.lock);
#endif
}

struct hrtimer cpuhp_timer;
static int cpuhp_timer_func(unsigned long data)
{
	if (hps_ctxt.tsk_struct_ptr) {
		pr_err("HPS task info (run on CPU %d)\n", task_cpu(hps_ctxt.tsk_struct_ptr));
		if (hps_ctxt.tsk_struct_ptr->on_cpu == 0)
			show_stack(hps_ctxt.tsk_struct_ptr, NULL);
	} else {
		pr_err("%s: no hps_main task\n", __func__);
	}

	BUG_ON(1);

	return HRTIMER_NORESTART;
}

/*
 * hps task main loop
 */
static int _hps_task_main(void *data)
{
	int cnt = 0;
	void (*algo_func_ptr)(void);

	hps_ctxt_print_basic(1);

	hrtimer_init(&cpuhp_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cpuhp_timer.function = (void *)&cpuhp_timer_func;
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
			cpumask_clear_cpu(hps_ctxt.root_cpu, &cpu_down_cpumask);
			cpu_down_by_mask(&cpu_down_cpumask);

			hps_ctxt.wake_up_by_fasthotplug = 0;
			mutex_unlock(&hps_ctxt.lock);
			goto HPS_WAIT_EVENT;
		}
#endif

		/* if (!hps_ctxt.is_interrupt) { */
		/*Get sys status */
		hps_get_sysinfo();

		if (!hps_ctxt.is_interrupt ||
		    ((u64) ktime_to_ms(ktime_sub(ktime_get(),
				       hps_ctxt.hps_regular_ktime))) >=
				       HPS_TIMER_INTERVAL_MS) {
#ifdef _PPM_
			mt_ppm_hica_update_algo_data(hps_ctxt.cur_loads, 0, hps_ctxt.cur_tlp);

			/*Execute PPM main function */
			mt_ppm_main();
#endif
			if (hps_ctxt.is_interrupt)
				hps_ctxt.is_interrupt = 0;
			hps_ctxt.hps_regular_ktime = ktime_get();
		} else
			hps_ctxt.is_interrupt = 0;

		/*execute hotplug algorithm */
		if (!hrtimer_active(&cpuhp_timer))
			hrtimer_start(&cpuhp_timer, ns_to_ktime(CPUHP_INTERVAL), HRTIMER_MODE_REL);
		(*algo_func_ptr) ();
		hrtimer_cancel(&cpuhp_timer);

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
					  (jiffies + msecs_to_jiffies(HPS_TIMER_INTERVAL_MS)));
				set_current_state(TASK_INTERRUPTIBLE);
				schedule();
			}
		} else if (hps_ctxt.periodical_by == HPS_PERIODICAL_BY_HR_TIMER) {

			if (atomic_read(&hps_ctxt.is_ondemand) == 0) {
				hrtimer_cancel(&hps_ctxt.hr_timer);
				hrtimer_start(&hps_ctxt.hr_timer, ktime, HRTIMER_MODE_REL);
				set_current_state(TASK_INTERRUPTIBLE);
				schedule();
			}
		}

		if (kthread_should_stop())
			break;

	}			/* while(1) */

	hps_warn("leave _hps_task_main, cnt:%08d\n", cnt++);
	return 0;
}

/*============================================================================*/
/* Gobal function definition */
/*============================================================================*/
/*
 * hps task control interface
 */
int hps_task_start(void)
{

	if (hps_ctxt.tsk_struct_ptr == NULL) {
		/*struct sched_param param = {.sched_priority = HPS_TASK_RT_PRIORITY };*/
		hps_ctxt.tsk_struct_ptr = kthread_create(_hps_task_main, NULL, "hps_main");
		if (IS_ERR(hps_ctxt.tsk_struct_ptr))
			return PTR_ERR(hps_ctxt.tsk_struct_ptr);

		/*sched_setscheduler_nocheck(hps_ctxt.tsk_struct_ptr, SCHED_FIFO, &param); */
		set_user_nice(hps_ctxt.tsk_struct_ptr, HPS_TASK_NORMAL_PRIORITY);
		get_task_struct(hps_ctxt.tsk_struct_ptr);
		wake_up_process(hps_ctxt.tsk_struct_ptr);
		hps_warn("hps_task_start success, ptr: %p, pid: %d\n", hps_ctxt.tsk_struct_ptr,
			 hps_ctxt.tsk_struct_ptr->pid);
	} else {
		hps_warn("hps task already exist, ptr: %p, pid: %d\n", hps_ctxt.tsk_struct_ptr,
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
	if (hps_ctxt.tsk_struct_ptr) {
		atomic_set(&hps_ctxt.is_ondemand, 1);
		if (hps_ctxt.periodical_by == HPS_PERIODICAL_BY_WAIT_QUEUE)
			wake_up(&hps_ctxt.wait_queue);
		else if ((hps_ctxt.periodical_by == HPS_PERIODICAL_BY_TIMER)
			 || (hps_ctxt.periodical_by == HPS_PERIODICAL_BY_HR_TIMER))
			wake_up_process(hps_ctxt.tsk_struct_ptr);
	}
}

void hps_task_wakeup(void)
{
	mutex_lock(&hps_ctxt.lock);

	hps_task_wakeup_nolock();

	mutex_unlock(&hps_ctxt.lock);
}

#ifdef _PPM_
static void ppm_limit_callback(struct ppm_client_req req)
{
	struct ppm_client_req *p = (struct ppm_client_req *)&req;
	int i;

	mutex_lock(&hps_ctxt.para_lock);
	for (i = 0; i < p->cluster_num; i++) {
		if (!p->cpu_limit[i].has_advise_core) {
			hps_sys.cluster_info[i].ref_base_value = p->cpu_limit[i].min_cpu_core;
			hps_sys.cluster_info[i].ref_limit_value = p->cpu_limit[i].max_cpu_core;
		} else {
			hps_sys.cluster_info[i].ref_base_value =
			    hps_sys.cluster_info[i].ref_limit_value =
			    p->cpu_limit[i].advise_cpu_core;
		}
	}
	mutex_unlock(&hps_ctxt.para_lock);
	hps_ctxt.is_interrupt = 1;
	hps_task_wakeup_nolock();

}
#endif
/*
 * init
 */
int hps_core_init(void)
{
	int r = 0;

	hps_warn("hps_core_init\n");
	if (hps_ctxt.periodical_by == HPS_PERIODICAL_BY_TIMER) {
		/*init timer */
		init_timer(&hps_ctxt.tmr_list);
		/*init_timer_deferrable(&hps_ctxt.tmr_list); */
		hps_ctxt.tmr_list.function = (void *)&_hps_timer_callback;
		hps_ctxt.tmr_list.data = (unsigned long)&hps_ctxt;
		hps_ctxt.tmr_list.expires = jiffies + msecs_to_jiffies(HPS_TIMER_INTERVAL_MS);
		add_timer(&hps_ctxt.tmr_list);
	} else if (hps_ctxt.periodical_by == HPS_PERIODICAL_BY_HR_TIMER) {
		ktime = ktime_set(0, MS_TO_NS(HPS_TIMER_INTERVAL_MS));
		/*init Hrtimer */
		hrtimer_init(&hps_ctxt.hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		hps_ctxt.hr_timer.function = (void *)&_hps_timer_callback;
		hrtimer_start(&hps_ctxt.hr_timer, ktime, HRTIMER_MODE_REL);

	}
	/* init and start task */
	r = hps_task_start();
	if (r) {
		hps_error("hps_task_start fail(%d)\n", r);
		return r;
	}

#ifdef _PPM_
	mt_ppm_register_client(PPM_CLIENT_HOTPLUG, &ppm_limit_callback);	/* register PPM callback */
#endif
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
			    jiffies + msecs_to_jiffies(HPS_TIMER_INTERVAL_MS - time_differ);
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
			hrtimer_start(&hps_ctxt.hr_timer, ktime, HRTIMER_MODE_REL);
			hps_task_wakeup_nolock();
			hps_cancel_time = 0;
		}
#endif
	}
#endif
	return 0;
}
