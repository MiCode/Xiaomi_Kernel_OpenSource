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
* @file    mt_hotplug_strategy_main.c
* @brief   hotplug strategy(hps) - main
*/

#include <linux/kernel.h>		/* printk */
#include <linux/module.h>		/* MODULE_DESCRIPTION, MODULE_LICENSE */
#include <linux/init.h>			/* module_init, module_exit */
#include <linux/platform_device.h>	/* platform_driver_register */
#include <linux/wakelock.h>		/* wake_lock_init */

#include "mt_hotplug_strategy_internal.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
static void hps_early_suspend(struct early_suspend *h);
static void hps_late_resume(struct early_suspend *h);
#endif

static int hps_probe(struct platform_device *pdev);
static int hps_suspend(struct device *dev);
static int hps_resume(struct device *dev);
static int hps_freeze(struct device *dev);
static int hps_restore(struct device *dev);

static const struct dev_pm_ops hps_dev_pm_ops = {
	.suspend    = hps_suspend,
	.resume     = hps_resume,
	.freeze     = hps_freeze,
	.restore    = hps_restore,
	.thaw       = hps_restore,
};

struct hps_ctxt_struct hps_ctxt = {
	/* state */
	.init_state = INIT_STATE_NOT_READY,
	.state = STATE_LATE_RESUME,

	/* enabled */
	.enabled = EN_HPS,
	.early_suspend_enabled = 1,
	.suspend_enabled = 1,
	.log_mask = 0,

	/* core */
	.lock = __MUTEX_INITIALIZER(hps_ctxt.lock),
		/* Synchronizes accesses to loads statistics */
	.tsk_struct_ptr = NULL,
	.wait_queue = __WAIT_QUEUE_HEAD_INITIALIZER(hps_ctxt.wait_queue),
#ifdef CONFIG_HAS_EARLYSUSPEND
	.es_handler = {
		.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 250,
		.suspend = hps_early_suspend,
		.resume  = hps_late_resume,
	},
#endif /* #ifdef CONFIG_HAS_EARLYSUSPEND */
	.pdrv = {
		.remove     = NULL,
		.shutdown   = NULL,
		.probe      = hps_probe,
		.driver     = {
			.name = "hps",
			.pm   = &hps_dev_pm_ops,
		},
	},

	/* algo config */
	.up_threshold = DEF_CPU_UP_THRESHOLD,
	.up_times = DEF_CPU_UP_TIMES,
	.down_threshold = DEF_CPU_DOWN_THRESHOLD,
	.down_times = DEF_CPU_DOWN_TIMES,
	.input_boost_enabled = EN_CPU_INPUT_BOOST,
	.input_boost_cpu_num = DEF_CPU_INPUT_BOOST_CPU_NUM,
	.rush_boost_enabled = EN_CPU_RUSH_BOOST,
	.rush_boost_threshold = DEF_CPU_RUSH_BOOST_THRESHOLD,
	.rush_boost_times = DEF_CPU_RUSH_BOOST_TIMES,
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
	/* memset(hps_ctxt.up_loads_history, 0,
		sizeof(hps_ctxt.up_loads_history)); */

	hps_ctxt.down_loads_sum = 0;
	hps_ctxt.down_loads_count = 0;
	hps_ctxt.down_loads_history_index = 0;
	hps_ctxt.down_loads_history[hps_ctxt.down_times - 1] = 0;
	/* memset(hps_ctxt.down_loads_history, 0,
		sizeof(hps_ctxt.down_loads_history)); */

	hps_ctxt.rush_count = 0;
	hps_ctxt.tlp_sum = 0;
	hps_ctxt.tlp_count = 0;
	hps_ctxt.tlp_history_index = 0;
	hps_ctxt.tlp_history[hps_ctxt.tlp_times - 1] = 0;
	/* memset(hps_ctxt.tlp_history, 0, sizeof(hps_ctxt.tlp_history)); */
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
	log_info("hps_ctxt.state: %u\n", hps_ctxt.state);
	log_info("hps_ctxt.enabled: %u\n", hps_ctxt.enabled);
	log_info("hps_ctxt.early_suspend_enabled: %u\n",
		hps_ctxt.early_suspend_enabled);
	log_info("hps_ctxt.suspend_enabled: %u\n",
		hps_ctxt.suspend_enabled);
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
	log_info("hps_ctxt.input_boost_enabled: %u\n",
		hps_ctxt.input_boost_enabled);
	log_info("hps_ctxt.input_boost_cpu_num: %u\n",
		hps_ctxt.input_boost_cpu_num);
	log_info("hps_ctxt.rush_boost_enabled: %u\n",
		hps_ctxt.rush_boost_enabled);
	log_info("hps_ctxt.rush_boost_threshold: %u\n",
		hps_ctxt.rush_boost_threshold);
	log_info("hps_ctxt.rush_boost_times: %u\n",
		hps_ctxt.rush_boost_times);
	log_info("hps_ctxt.tlp_times: %u\n", hps_ctxt.tlp_times);
}

void hps_ctxt_print_algo_bound(int toUart)
{
	log_info("hps_ctxt.little_num_base_perf_serv: %u\n",
		hps_ctxt.little_num_base_perf_serv);
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
 * early suspend callback
 */
#ifdef CONFIG_HAS_EARLYSUSPEND
static void hps_early_suspend(struct early_suspend *h)
{
	log_info("hps_early_suspend\n");

	mutex_lock(&hps_ctxt.lock);
	hps_ctxt.state = STATE_EARLY_SUSPEND;

	hps_ctxt.rush_boost_enabled_backup = hps_ctxt.rush_boost_enabled;
	hps_ctxt.rush_boost_enabled = 0;

	if (hps_ctxt.is_hmp && hps_ctxt.early_suspend_enabled) {
		unsigned int cpu;

		for (cpu = hps_ctxt.big_cpu_id_max;
			cpu >= hps_ctxt.big_cpu_id_min; --cpu) {
			if (cpu_online(cpu))
				hps_cpu_down(cpu);
		}
	}
	mutex_unlock(&hps_ctxt.lock);

	log_info(
		"state: %u, enabled: %u, early_suspend_enabled: %u, suspend_enabled: %u, rush_boost_enabled: %u\n",
		hps_ctxt.state, hps_ctxt.enabled,
		hps_ctxt.early_suspend_enabled, hps_ctxt.suspend_enabled,
		hps_ctxt.rush_boost_enabled);
}
#endif /* #ifdef CONFIG_HAS_EARLYSUSPEND */

/*
 * late resume callback
 */
#ifdef CONFIG_HAS_EARLYSUSPEND
static void hps_late_resume(struct early_suspend *h)
{
	log_info("hps_late_resume\n");

	mutex_lock(&hps_ctxt.lock);
	hps_ctxt.rush_boost_enabled = hps_ctxt.rush_boost_enabled_backup;

	hps_ctxt.state = STATE_LATE_RESUME;
	mutex_unlock(&hps_ctxt.lock);

	log_info(
		"state: %u, enabled: %u, early_suspend_enabled: %u, suspend_enabled: %u, rush_boost_enabled: %u\n",
		hps_ctxt.state, hps_ctxt.enabled,
		hps_ctxt.early_suspend_enabled, hps_ctxt.suspend_enabled,
		hps_ctxt.rush_boost_enabled);
}
#endif /* #ifdef CONFIG_HAS_EARLYSUSPEND */

/*
 * probe callback
 */
static int hps_probe(struct platform_device *pdev)
{
	log_info("hps_probe\n");

	return 0;
}

/*
 * suspend callback
 */
static int hps_suspend(struct device *dev)
{
	log_info("%s\n", __func__);

	if (!hps_ctxt.suspend_enabled)
		goto suspend_end;

	mutex_lock(&hps_ctxt.lock);
	hps_ctxt.enabled_backup = hps_ctxt.enabled;
	hps_ctxt.enabled = 0;
	mutex_unlock(&hps_ctxt.lock);

suspend_end:
	hps_ctxt.state = STATE_SUSPEND;
	log_info(
		"state: %u, enabled: %u, early_suspend_enabled: %u, suspend_enabled: %u, rush_boost_enabled: %u\n",
		hps_ctxt.state, hps_ctxt.enabled,
		hps_ctxt.early_suspend_enabled, hps_ctxt.suspend_enabled,
		hps_ctxt.rush_boost_enabled);

	return 0;
}

/*
 * resume callback
 */
static int hps_resume(struct device *dev)
{
	log_info("%s\n", __func__);

	if (!hps_ctxt.suspend_enabled)
		goto resume_end;

	mutex_lock(&hps_ctxt.lock);
	hps_ctxt.enabled = hps_ctxt.enabled_backup;
	mutex_unlock(&hps_ctxt.lock);

resume_end:
	hps_ctxt.state = STATE_EARLY_SUSPEND;
	log_info(
		"state: %u, enabled: %u, early_suspend_enabled: %u, suspend_enabled: %u, rush_boost_enabled: %u\n",
		hps_ctxt.state, hps_ctxt.enabled,
		hps_ctxt.early_suspend_enabled, hps_ctxt.suspend_enabled,
		hps_ctxt.rush_boost_enabled);

	return 0;
}

/*
 * freeze callback
 */
static int hps_freeze(struct device *dev)
{
	log_info("%s\n", __func__);

	if (!hps_ctxt.suspend_enabled)
		goto freeze_end;

	mutex_lock(&hps_ctxt.lock);
	hps_ctxt.enabled_backup = hps_ctxt.enabled;
	hps_ctxt.enabled = 0;

	/* prepare to hibernation restore */
	if (hps_ctxt.is_hmp) {
		unsigned int cpu;

		for (cpu = hps_ctxt.big_cpu_id_max;
			cpu >= hps_ctxt.big_cpu_id_min; --cpu) {
			if (cpu_online(cpu))
				hps_cpu_down(cpu);
		}
	}
	mutex_unlock(&hps_ctxt.lock);

freeze_end:
	hps_ctxt.state = STATE_SUSPEND;
	log_info(
		"state: %u, enabled: %u, early_suspend_enabled: %u, suspend_enabled: %u, rush_boost_enabled: %u\n",
		hps_ctxt.state, hps_ctxt.enabled,
		hps_ctxt.early_suspend_enabled, hps_ctxt.suspend_enabled,
		hps_ctxt.rush_boost_enabled);

	return 0;
}

/*
 * restore callback
 */
static int hps_restore(struct device *dev)
{
	log_info("%s\n", __func__);

	if (!hps_ctxt.suspend_enabled)
		goto restore_end;

	mutex_lock(&hps_ctxt.lock);
	hps_ctxt.enabled = hps_ctxt.enabled_backup;
	mutex_unlock(&hps_ctxt.lock);

restore_end:
	hps_ctxt.state = STATE_EARLY_SUSPEND;
	log_info(
		"state: %u, enabled: %u, early_suspend_enabled: %u, suspend_enabled: %u, rush_boost_enabled: %u\n",
		hps_ctxt.state, hps_ctxt.enabled,
		hps_ctxt.early_suspend_enabled, hps_ctxt.suspend_enabled,
		hps_ctxt.rush_boost_enabled);

	return 0;
}

/*
 * module init function
 */
static int __init hps_init(void)
{
	int r = 0;

	log_info("hps_init\n");

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

#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&hps_ctxt.es_handler);
#endif /* #ifdef CONFIG_HAS_EARLYSUSPEND */

	r = platform_driver_register(&hps_ctxt.pdrv);
	if (r)
		hps_err("platform_driver_register fail(%d)\n", r);

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

	log_info("hps_exit\n");

	hps_ctxt.init_state = INIT_STATE_NOT_READY;

	r = hps_core_deinit();
	if (r)
		hps_err("hps_core_deinit fail(%d)\n", r);
}
module_exit(hps_exit);

MODULE_DESCRIPTION("MediaTek CPU Hotplug Stragegy Core v0.1");
MODULE_LICENSE("GPL");

