// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

/**
 * @file    mt_hotplug_strategy_main.c
 * @brief   hotplug strategy(hps) - main
 */

#include <linux/kernel.h>
#include <linux/module.h>		/* MODULE_DESCRIPTION, MODULE_LICENSE */
#include <linux/init.h>			/* module_init, module_exit */
#include <linux/platform_device.h>	/* platform_driver_register */

#include "mt_hotplug_strategy_internal.h"

#ifndef DEF_LOG_MASK
#define DEF_LOG_MASK		0
#endif
#ifndef EN_CPU_QUICK_LANDING
#define EN_CPU_QUICK_LANDING	1
#endif

struct hps_ctxt_struct hps_ctxt = {
	/* state */
	.init_state = INIT_STATE_NOT_READY,

	/* enabled */
	.enabled = EN_HPS,
	.log_mask = DEF_LOG_MASK,

	/* core */
	.lock = __MUTEX_INITIALIZER(hps_ctxt.lock),
		/* Synchronizes accesses to loads statistics */
	.tsk_struct_ptr = NULL,

#if HPS_PERIODICAL_BY_WAIT_QUEUE
	.wait_queue = __WAIT_QUEUE_HEAD_INITIALIZER(hps_ctxt.wait_queue),
#endif

	/* algo config */
	.up_threshold = DEF_CPU_UP_THRESHOLD,
	.up_times = DEF_CPU_UP_TIMES,
	.down_threshold = DEF_CPU_DOWN_THRESHOLD,
	.down_times = DEF_CPU_DOWN_TIMES,
	.rush_boost_enabled = EN_CPU_RUSH_BOOST,
	.rush_boost_threshold = DEF_CPU_RUSH_BOOST_THRESHOLD,
	.rush_boost_times = DEF_CPU_RUSH_BOOST_TIMES,
	.quick_landing_enabled = EN_CPU_QUICK_LANDING,
	.tlp_times = DEF_TLP_TIMES,

	/* algo statistics */
	.up_loads_sum = 0,
	.up_loads_count = 0,
	.up_loads_history = {0},
	.up_loads_history_index = 0,
	.down_loads_sum = 0,
	.down_loads_count = 0,
	.down_loads_history = {0},
	.down_loads_history_index = 0,
	.rush_count = 0,
	.tlp_sum = 0,
	.tlp_count = 0,
	.tlp_history = {0},
	.tlp_history_index = 0,

	/* algo action */
	.action = ACTION_NONE,
	.is_ondemand = ATOMIC_INIT(0),
};

DEFINE_PER_CPU(struct hps_cpu_ctxt_struct, hps_percpu_ctxt);

/*
 * hps struct hps_ctxt_struct control interface
 */
void hps_ctxt_reset_stas_nolock(void)
{
	hps_ctxt.up_loads_sum = 0;
	hps_ctxt.up_loads_count = 0;
	hps_ctxt.up_loads_history_index = 0;
	hps_ctxt.up_loads_history[hps_ctxt.up_times - 1] = 0;

	hps_ctxt.down_loads_sum = 0;
	hps_ctxt.down_loads_count = 0;
	hps_ctxt.down_loads_history_index = 0;
	hps_ctxt.down_loads_history[hps_ctxt.down_times - 1] = 0;

	hps_ctxt.rush_count = 0;
	hps_ctxt.tlp_sum = 0;
	hps_ctxt.tlp_count = 0;
	hps_ctxt.tlp_history_index = 0;
	hps_ctxt.tlp_history[hps_ctxt.tlp_times - 1] = 0;
}

void hps_ctxt_reset_stas(void)
{
	mutex_lock(&hps_ctxt.lock);

	hps_ctxt_reset_stas_nolock();

	mutex_unlock(&hps_ctxt.lock);
}

/*
 * hps struct hps_ctxt_struct print interface
 */
void hps_ctxt_print_basic(int toUart)
{
	log_info("hps_ctxt.init_state: %u\n", hps_ctxt.init_state);
	log_info("hps_ctxt.enabled: %u\n", hps_ctxt.enabled);
	log_info("hps_ctxt.is_hmp: %u\n", hps_ctxt.is_hmp);
	log_info("hps_ctxt.little_cpu_id_min: %u\n",
		hps_ctxt.little_cpu_id_min);
	log_info("hps_ctxt.little_cpu_id_max: %u\n",
		hps_ctxt.little_cpu_id_max);
	log_info("hps_ctxt.big_cpu_id_min: %u\n",
		hps_ctxt.big_cpu_id_min);
	log_info("hps_ctxt.big_cpu_id_max: %u\n",
		hps_ctxt.big_cpu_id_max);
}

void hps_ctxt_print_algo_config(int toUart)
{
	log_info("hps_ctxt.up_threshold: %u\n", hps_ctxt.up_threshold);
	log_info("hps_ctxt.up_times: %u\n", hps_ctxt.up_times);
	log_info("hps_ctxt.down_threshold: %u\n",
		hps_ctxt.down_threshold);
	log_info("hps_ctxt.down_times: %u\n", hps_ctxt.down_times);
	log_info("hps_ctxt.rush_boost_enabled: %u\n",
		hps_ctxt.rush_boost_enabled);
	log_info("hps_ctxt.rush_boost_threshold: %u\n",
		hps_ctxt.rush_boost_threshold);
	log_info("hps_ctxt.rush_boost_times: %u\n",
		hps_ctxt.rush_boost_times);
	log_info("hps_ctxt.quick_landing_enabled: %u\n",
		hps_ctxt.quick_landing_enabled);
	log_info("hps_ctxt.tlp_times: %u\n", hps_ctxt.tlp_times);
}

void hps_ctxt_print_algo_bound(int toUart)
{
	log_info("hps_ctxt.little_num_base_perf_serv: %u\n",
		hps_ctxt.little_num_base_perf_serv);
	log_info("hps_ctxt.little_num_base_custom1: %u\n",
		hps_ctxt.little_num_base_custom1);
	log_info("hps_ctxt.little_num_base_custom2: %u\n",
		hps_ctxt.little_num_base_custom2);
	log_info("hps_ctxt.little_num_limit_thermal: %u\n",
		hps_ctxt.little_num_limit_thermal);
	log_info("hps_ctxt.little_num_limit_low_battery: %u\n",
		hps_ctxt.little_num_limit_low_battery);
	log_info("hps_ctxt.little_num_limit_ultra_power_saving: %u\n",
		hps_ctxt.little_num_limit_ultra_power_saving);
	log_info("hps_ctxt.little_num_limit_power_serv: %u\n",
		hps_ctxt.little_num_limit_power_serv);
	log_info("hps_ctxt.big_num_base_perf_serv: %u\n",
		hps_ctxt.big_num_base_perf_serv);
	log_info("hps_ctxt.big_num_base_custom1: %u\n",
		hps_ctxt.big_num_base_custom1);
	log_info("hps_ctxt.big_num_base_custom2: %u\n",
		hps_ctxt.big_num_base_custom2);
	log_info("hps_ctxt.big_num_limit_thermal: %u\n",
		hps_ctxt.big_num_limit_thermal);
	log_info("hps_ctxt.big_num_limit_low_battery: %u\n",
		hps_ctxt.big_num_limit_low_battery);
	log_info("hps_ctxt.big_num_limit_ultra_power_saving: %u\n",
		hps_ctxt.big_num_limit_ultra_power_saving);
	log_info("hps_ctxt.big_num_limit_power_serv: %u\n",
		hps_ctxt.big_num_limit_power_serv);
}

void hps_ctxt_print_algo_stats_cur(int toUart)
{
	log_alog2("hps_ctxt.cur_loads: %u\n", hps_ctxt.cur_loads);
	log_alog2("hps_ctxt.cur_tlp: %u\n", hps_ctxt.cur_tlp);
	log_alog2("hps_ctxt.cur_iowait: %u\n", hps_ctxt.cur_iowait);
	log_alog2("hps_ctxt.cur_nr_heavy_task: %u\n",
		hps_ctxt.cur_nr_heavy_task);
}

void hps_ctxt_print_algo_stats_up(int toUart)
{
	log_alog2("hps_ctxt.up_loads_sum: %u\n", hps_ctxt.up_loads_sum);
	log_alog2("hps_ctxt.up_loads_count: %u\n",
		hps_ctxt.up_loads_count);
	log_alog2("hps_ctxt.up_loads_history_index: %u\n",
		hps_ctxt.up_loads_history_index);
}

void hps_ctxt_print_algo_stats_down(int toUart)
{
	log_alog2("hps_ctxt.down_loads_sum: %u\n",
		hps_ctxt.down_loads_sum);
	log_alog2("hps_ctxt.down_loads_count: %u\n",
		hps_ctxt.down_loads_count);
	log_alog2("hps_ctxt.down_loads_history_index: %u\n",
		hps_ctxt.down_loads_history_index);
}

void hps_ctxt_print_algo_stats_tlp(int toUart)
{
	log_alog2("hps_ctxt.tlp_sum: %u\n", hps_ctxt.tlp_sum);
	log_alog2("hps_ctxt.tlp_count: %u\n", hps_ctxt.tlp_count);
	log_alog2("hps_ctxt.tlp_history_index: %u\n",
		hps_ctxt.tlp_history_index);
	log_alog2("hps_ctxt.tlp_avg: %u\n", hps_ctxt.tlp_avg);
	log_alog2("hps_ctxt.rush_count: %u\n", hps_ctxt.rush_count);
}

/*
 * module init function
 */
static int __init hps_init(void)
{
	int r = 0;

	log_info("%s\n", __func__);

	/* hps_cpu_init() must before hps_core_init() */
	r = hps_cpu_init();
	if (r)
		hps_err("hps_cpu_init fail(%d)\n", r);

	r = hps_core_init();
	if (r)
		hps_err("hps_core_init fail(%d)\n", r);

	r = hps_procfs_init();
	if (r)
		hps_err("hps_procfs_init fail(%d)\n", r);

	hps_ctxt.init_state = INIT_STATE_DONE;

	return r;
}
module_init(hps_init);

/*
 * module exit function
 */
static void __exit hps_exit(void)
{
	int r = 0;

	log_info("%s\n", __func__);

	hps_ctxt.init_state = INIT_STATE_NOT_READY;

	r = hps_core_deinit();
	if (r)
		hps_err("hps_core_deinit fail(%d)\n", r);
}
module_exit(hps_exit);

MODULE_DESCRIPTION("MediaTek CPU Hotplug Stragegy Core v0.1");
MODULE_LICENSE("GPL");

