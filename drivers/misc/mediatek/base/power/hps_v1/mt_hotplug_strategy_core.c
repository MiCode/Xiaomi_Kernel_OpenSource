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

#include <linux/kernel.h>
#include <linux/module.h>	/* MODULE_DESCRIPTION, MODULE_LICENSE */
#include <linux/init.h>		/* module_init, module_exit */
#include <linux/cpu.h>		/* cpu_up */
#include <linux/kthread.h>	/* kthread_create */
#include <asm-generic/bug.h>	/* BUG_ON */

#include "mt_hotplug_strategy_internal.h"

#ifndef HPS_TASK_RT
#define HPS_TASK_RT			0
#endif
#ifndef HPS_TASK_NICE
#define HPS_TASK_NICE			MIN_NICE
#endif

#if HPS_PERIODICAL_BY_WAIT_QUEUE

static void hps_periodical_by_wait_queue(void)
{
	wait_event_timeout(hps_ctxt.wait_queue,
		atomic_read(&hps_ctxt.is_ondemand) != 0,
		msecs_to_jiffies(HPS_TIMER_INTERVAL_MS));
}

#elif HPS_PERIODICAL_BY_TIMER

static u64 hps_get_current_time_ms(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return (u64)t.tv_sec * 1000 + t.tv_usec / 1000;
}

static void hps_timer_callback(unsigned long data)
{
	int ret;

	log_tmr("timer(%lu): %llu\n", data, hps_get_current_time_ms());

	if (hps_ctxt.tsk_struct_ptr) {
		ret = wake_up_process(hps_ctxt.tsk_struct_ptr);
		if (!ret)
			hps_err("hps task has waked up: %d\n", ret);
	}
}

static void hps_periodical_by_timer(void)
{
	unsigned int lo, bo;

	if (atomic_read(&hps_ctxt.is_ondemand) != 0)
		return;

	if (hps_get_num_online_cpus(&lo, &bo) < 0 || lo + bo > 1)
		hps_ctxt.active_hps_tmr = &hps_ctxt.hps_tmr;
	else
		hps_ctxt.active_hps_tmr = &hps_ctxt.hps_tmr_dfr;

	mod_timer(hps_ctxt.active_hps_tmr, (jiffies + msecs_to_jiffies(
						HPS_TIMER_INTERVAL_MS)));

	set_current_state(TASK_INTERRUPTIBLE);
	schedule();

	/* waked up */

	if (timer_pending(hps_ctxt.active_hps_tmr))
		del_timer(hps_ctxt.active_hps_tmr);
}

#endif /* HPS_PERIODICAL_BY_ */

/*
 * hps task main loop
 */
static int _hps_task_main(void *data)
{
	void (*algo_func_ptr)(void);

	hps_ctxt_print_basic(1);

	if (hps_ctxt.is_hmp)
		algo_func_ptr = hps_algo_hmp;
	else
		algo_func_ptr = hps_algo_smp;

	while (1) {
		(*algo_func_ptr)();

#if HPS_PERIODICAL_BY_WAIT_QUEUE
		hps_periodical_by_wait_queue();
#elif HPS_PERIODICAL_BY_TIMER
		hps_periodical_by_timer();
#else
	#error "Unknown HPS_PERIODICAL"
#endif

		if (kthread_should_stop())
			break;
	}

	log_info("leave %s\n", __func__);

	return 0;
}

/*
 * hps task control interface
 */
int hps_task_start(void)
{
#if HPS_TASK_RT
	struct sched_param param = { .sched_priority = HPS_TASK_PRIORITY };
#endif

	if (hps_ctxt.tsk_struct_ptr == NULL) {
		hps_ctxt.tsk_struct_ptr =
			kthread_create(_hps_task_main, NULL, "hps_main");
		if (IS_ERR(hps_ctxt.tsk_struct_ptr))
			return PTR_ERR(hps_ctxt.tsk_struct_ptr);

#if HPS_TASK_RT
		sched_setscheduler_nocheck(
			hps_ctxt.tsk_struct_ptr, SCHED_FIFO, &param);
#else
		set_user_nice(hps_ctxt.tsk_struct_ptr, HPS_TASK_NICE);
#endif

		get_task_struct(hps_ctxt.tsk_struct_ptr);
		wake_up_process(hps_ctxt.tsk_struct_ptr);
		log_info("%s success, ptr: %p, pid: %d\n", __func__,
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
	if (!hps_ctxt.tsk_struct_ptr)
		return;

	atomic_set(&hps_ctxt.is_ondemand, 1);

#if HPS_PERIODICAL_BY_WAIT_QUEUE
	wake_up(&hps_ctxt.wait_queue);
#elif HPS_PERIODICAL_BY_TIMER
	wake_up_process(hps_ctxt.tsk_struct_ptr);
#endif
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

	log_info("%s\n", __func__);

#if HPS_PERIODICAL_BY_TIMER
	init_timer_deferrable(&hps_ctxt.hps_tmr_dfr);
	hps_ctxt.hps_tmr_dfr.function = hps_timer_callback;
	hps_ctxt.hps_tmr_dfr.data = 1;

	init_timer(&hps_ctxt.hps_tmr);
	hps_ctxt.hps_tmr.function = hps_timer_callback;
	hps_ctxt.hps_tmr.data = 2;

	hps_ctxt.active_hps_tmr = &hps_ctxt.hps_tmr;
	hps_ctxt.active_hps_tmr->expires = jiffies +
				msecs_to_jiffies(HPS_TIMER_INTERVAL_MS);
	add_timer(hps_ctxt.active_hps_tmr);
#endif /* HPS_PERIODICAL_BY_TIMER */

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

	log_info("%s\n", __func__);

	hps_task_stop();

	return r;
}
