/*
 * Copyright (C) 2015 MediaTek Inc.
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

/**
* @file    mt_hotplug_strategy_core.c
* @brief   hotplug strategy(hps) - core
*/

#include <linux/kernel.h>	/* printk */
#include <linux/module.h>	/* MODULE_DESCRIPTION, MODULE_LICENSE */
#include <linux/init.h>		/* module_init, module_exit */
#include <linux/cpu.h>		/* cpu_up */
#include <linux/kthread.h>	/* kthread_create */
#include <linux/wakelock.h>	/* wake_lock_init */
#include <asm-generic/bug.h>	/* BUG_ON */

#include "mt_hotplug_strategy_internal.h"

/*
 * hps task main loop
 */
static int _hps_task_main(void *data)
{
	int cnt = 0;
	void (*algo_func_ptr)(void);

	hps_ctxt_print_basic(1);

	if (hps_ctxt.is_hmp)
		algo_func_ptr = hps_algo_hmp;
	else
		algo_func_ptr = hps_algo_smp;

	while (1) {
		/* TODO: showld we do dvfs? */
		/* struct cpufreq_policy *policy; */
		/* policy = cpufreq_cpu_get(0); */
		/* dbs_freq_increase(policy, policy->max); */
		/* cpufreq_cpu_put(policy); */

		(*algo_func_ptr)();

		/* hps_debug("before schedule, cnt:%08d\n", cnt++); */

		wait_event_timeout(hps_ctxt.wait_queue,
			atomic_read(&hps_ctxt.is_ondemand) != 0,
			msecs_to_jiffies(HPS_TIMER_INTERVAL_MS));

		/* hps_debug("after schedule, cnt:%08d\n", cnt++); */

		if (kthread_should_stop())
			break;
	} /* while(1) */

	log_info("leave _hps_task_main, cnt:%08d\n", cnt++);
	return 0;
}

/*
 * hps task control interface
 */
int hps_task_start(void)
{
	struct sched_param param = { .sched_priority = HPS_TASK_PRIORITY };

	if (hps_ctxt.tsk_struct_ptr == NULL) {
		hps_ctxt.tsk_struct_ptr =
			kthread_create(_hps_task_main, NULL, "hps_main");
		if (IS_ERR(hps_ctxt.tsk_struct_ptr))
			return PTR_ERR(hps_ctxt.tsk_struct_ptr);

		sched_setscheduler_nocheck(
			hps_ctxt.tsk_struct_ptr, SCHED_FIFO, &param);
		get_task_struct(hps_ctxt.tsk_struct_ptr);
		wake_up_process(hps_ctxt.tsk_struct_ptr);
		log_info("hps_task_start success, ptr: %p, pid: %d\n",
			hps_ctxt.tsk_struct_ptr, hps_ctxt.tsk_struct_ptr->pid);
	} else
		log_info("hps task already exist, ptr: %p, pid: %d\n",
			hps_ctxt.tsk_struct_ptr, hps_ctxt.tsk_struct_ptr->pid);

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
		wake_up(&hps_ctxt.wait_queue);
	}
}

void hps_task_wakeup(void)
{
	mutex_lock(&hps_ctxt.lock);

	hps_task_wakeup_nolock();

	mutex_unlock(&hps_ctxt.lock);
}

/*
 * init
 */
int hps_core_init(void)
{
	int r = 0;

	log_info("hps_core_init\n");

	/* init and start task */
	r = hps_task_start();
	if (r)
		hps_err("hps_task_start fail(%d)\n", r);

	return r;
}

/*
 * deinit
 */
int hps_core_deinit(void)
{
	int r = 0;

	log_info("hps_core_deinit\n");

	hps_task_stop();

	return r;
}
