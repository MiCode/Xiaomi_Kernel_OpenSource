/*
 * drivers/misc/tegra-profiler/main.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>

#include <linux/tegra_profiler.h>

#include "quadd.h"
#include "armv7_pmu.h"
#include "hrt.h"
#include "pl310.h"
#include "comm.h"
#include "mmap.h"
#include "debug.h"
#include "tegra.h"
#include "power_clk.h"
#include "auth.h"
#include "version.h"

static struct quadd_ctx ctx;

static int get_default_properties(void)
{
	ctx.param.freq = 100;
	ctx.param.ma_freq = 50;
	ctx.param.backtrace = 1;
	ctx.param.use_freq = 1;
	ctx.param.system_wide = 1;
	ctx.param.power_rate_freq = 0;
	ctx.param.debug_samples = 0;

	ctx.param.pids[0] = 0;
	ctx.param.nr_pids = 1;

	return 0;
}

static int start(void)
{
	int err;

	if (!atomic_cmpxchg(&ctx.started, 0, 1)) {
		if (ctx.pmu) {
			err = ctx.pmu->enable();
			if (err) {
				pr_err("error: pmu enable\n");
				return err;
			}
		}

		if (ctx.pl310) {
			err = ctx.pl310->enable();
			if (err) {
				pr_err("error: pl310 enable\n");
				return err;
			}
		}

		quadd_mmap_reset();
		ctx.comm->reset();

		err = quadd_power_clk_start();
		if (err < 0) {
			pr_err("error: power_clk start\n");
			return err;
		}

		err = quadd_hrt_start();
		if (err) {
			pr_err("error: hrt start\n");
			return err;
		}
	}

	return 0;
}

static void stop(void)
{
	if (atomic_cmpxchg(&ctx.started, 1, 0)) {
		quadd_hrt_stop();

		quadd_mmap_reset();
		ctx.comm->reset();

		quadd_power_clk_stop();

		if (ctx.pmu)
			ctx.pmu->disable();

		if (ctx.pl310)
			ctx.pl310->disable();
	}
}

static inline int is_event_supported(struct source_info *si, int event)
{
	int i;
	int nr = si->nr_supported_events;
	int *events = si->supported_events;

	for (i = 0; i < nr; i++) {
		if (event == events[i])
			return 1;
	}
	return 0;
}

static int set_parameters(struct quadd_parameters *param, uid_t *debug_app_uid)
{
	int i, err;
	int pmu_events_id[QUADD_MAX_COUNTERS];
	int pl310_events_id;
	int nr_pmu = 0, nr_pl310 = 0;
	int uid = 0;
	struct task_struct *task;

	if (ctx.param.freq != 100 && ctx.param.freq != 1000 &&
	    ctx.param.freq != 10000)
		return -EINVAL;

	ctx.param.freq = param->freq;
	ctx.param.ma_freq = param->ma_freq;
	ctx.param.backtrace = param->backtrace;
	ctx.param.use_freq = param->use_freq;
	ctx.param.system_wide = param->system_wide;
	ctx.param.power_rate_freq = param->power_rate_freq;
	ctx.param.debug_samples = param->debug_samples;

	/* Currently only one process */
	if (param->nr_pids != 1)
		return -EINVAL;

	rcu_read_lock();
	task = pid_task(find_vpid(param->pids[0]), PIDTYPE_PID);
	rcu_read_unlock();
	if (!task) {
		pr_err("Process not found: %u\n", param->pids[0]);
		return -ESRCH;
	}

	pr_info("owner/task uids: %u/%u\n", current_fsuid(), task_uid(task));
	if (!capable(CAP_SYS_ADMIN)) {
		if (current_fsuid() != task_uid(task)) {
			uid = quadd_auth_check_debug_flag(param->package_name);
			if (uid < 0) {
				pr_err("Error: QuadD security service\n");
				return uid;
			} else if (uid == 0) {
				pr_err("Error: app is not debuggable\n");
				return -EACCES;
			}

			*debug_app_uid = uid;
			pr_info("debug_app_uid: %u\n", uid);
		}
	}

	for (i = 0; i < param->nr_pids; i++)
		ctx.param.pids[i] = param->pids[i];

	ctx.param.nr_pids = param->nr_pids;

	for (i = 0; i < param->nr_events; i++) {
		int event = param->events[i];

		if (ctx.pmu && ctx.pmu_info.nr_supported_events > 0
			&& is_event_supported(&ctx.pmu_info, event)) {
			pmu_events_id[nr_pmu++] = param->events[i];

			pr_info("PMU active event: %s\n",
				quadd_get_event_str(event));
		} else if (ctx.pl310 &&
			   ctx.pl310_info.nr_supported_events > 0 &&
			   is_event_supported(&ctx.pl310_info, event)) {
			pl310_events_id = param->events[i];

			pr_info("PL310 active event: %s\n",
				quadd_get_event_str(event));

			if (nr_pl310++ > 1) {
				pr_err("error: multiply pl310 events\n");
				return -EINVAL;
			}
		} else {
			pr_err("Bad event: %s\n",
			       quadd_get_event_str(event));
			return -EINVAL;
		}
	}

	if (ctx.pmu) {
		if (nr_pmu > 0) {
			err = ctx.pmu->set_events(pmu_events_id, nr_pmu);
			if (err) {
				pr_err("PMU set parameters: error\n");
				return err;
			}
			ctx.pmu_info.active = 1;
		} else {
			ctx.pmu_info.active = 0;
			ctx.pmu->set_events(NULL, 0);
		}
	}

	if (ctx.pl310) {
		if (nr_pl310 == 1) {
			err = ctx.pl310->set_events(&pl310_events_id, 1);
			if (err) {
				pr_info("pl310 set_parameters: error\n");
				return err;
			}
			ctx.pl310_info.active = 1;
		} else {
			ctx.pl310_info.active = 0;
			ctx.pl310->set_events(NULL, 0);
		}
	}
	pr_info("New parameters have been applied\n");

	return 0;
}

static void get_capabilities(struct quadd_comm_cap *cap)
{
	int i, event;
	struct quadd_events_cap *events_cap = &cap->events_cap;

	cap->pmu = ctx.pmu ? 1 : 0;

	cap->l2_cache = 0;
	if (ctx.pl310) {
		cap->l2_cache = 1;
		cap->l2_multiple_events = 0;
	} else if (ctx.pmu) {
		struct source_info *s = &ctx.pmu_info;
		for (i = 0; i < s->nr_supported_events; i++) {
			event = s->supported_events[i];
			if (event == QUADD_EVENT_TYPE_L2_DCACHE_READ_MISSES ||
			    event == QUADD_EVENT_TYPE_L2_DCACHE_WRITE_MISSES ||
			    event == QUADD_EVENT_TYPE_L2_ICACHE_MISSES) {
				cap->l2_cache = 1;
				cap->l2_multiple_events = 1;
				break;
			}
		}
	}

	events_cap->cpu_cycles = 0;
	events_cap->l1_dcache_read_misses = 0;
	events_cap->l1_dcache_write_misses = 0;
	events_cap->l1_icache_misses = 0;

	events_cap->instructions = 0;
	events_cap->branch_instructions = 0;
	events_cap->branch_misses = 0;
	events_cap->bus_cycles = 0;

	events_cap->l2_dcache_read_misses = 0;
	events_cap->l2_dcache_write_misses = 0;
	events_cap->l2_icache_misses = 0;

	if (ctx.pl310) {
		struct source_info *s = &ctx.pl310_info;
		for (i = 0; i < s->nr_supported_events; i++) {
			int event = s->supported_events[i];

			switch (event) {
			case QUADD_EVENT_TYPE_L2_DCACHE_READ_MISSES:
				events_cap->l2_dcache_read_misses = 1;
				break;
			case QUADD_EVENT_TYPE_L2_DCACHE_WRITE_MISSES:
				events_cap->l2_dcache_write_misses = 1;
				break;
			case QUADD_EVENT_TYPE_L2_ICACHE_MISSES:
				events_cap->l2_icache_misses = 1;
				break;

			default:
				BUG();
				break;
			}
		}
	}

	if (ctx.pmu) {
		struct source_info *s = &ctx.pmu_info;
		for (i = 0; i < s->nr_supported_events; i++) {
			int event = s->supported_events[i];

			switch (event) {
			case QUADD_EVENT_TYPE_CPU_CYCLES:
				events_cap->cpu_cycles = 1;
				break;
			case QUADD_EVENT_TYPE_INSTRUCTIONS:
				events_cap->instructions = 1;
				break;
			case QUADD_EVENT_TYPE_BRANCH_INSTRUCTIONS:
				events_cap->branch_instructions = 1;
				break;
			case QUADD_EVENT_TYPE_BRANCH_MISSES:
				events_cap->branch_misses = 1;
				break;
			case QUADD_EVENT_TYPE_BUS_CYCLES:
				events_cap->bus_cycles = 1;
				break;

			case QUADD_EVENT_TYPE_L1_DCACHE_READ_MISSES:
				events_cap->l1_dcache_read_misses = 1;
				break;
			case QUADD_EVENT_TYPE_L1_DCACHE_WRITE_MISSES:
				events_cap->l1_dcache_write_misses = 1;
				break;
			case QUADD_EVENT_TYPE_L1_ICACHE_MISSES:
				events_cap->l1_icache_misses = 1;
				break;

			case QUADD_EVENT_TYPE_L2_DCACHE_READ_MISSES:
				events_cap->l2_dcache_read_misses = 1;
				break;
			case QUADD_EVENT_TYPE_L2_DCACHE_WRITE_MISSES:
				events_cap->l2_dcache_write_misses = 1;
				break;
			case QUADD_EVENT_TYPE_L2_ICACHE_MISSES:
				events_cap->l2_icache_misses = 1;
				break;

			default:
				BUG();
				break;
			}
		}
	}

	cap->tegra_lp_cluster = quadd_is_cpu_with_lp_cluster();
	cap->power_rate = 1;
	cap->blocked_read = 0;
}

static void get_state(struct quadd_module_state *state)
{
	quadd_hrt_get_state(state);
}

static struct quadd_comm_control_interface control = {
	.start			= start,
	.stop			= stop,
	.set_parameters		= set_parameters,
	.get_capabilities	= get_capabilities,
	.get_state		= get_state,
};

static int __init quadd_module_init(void)
{
	int i, nr_events, err;
	int *events;

	pr_info("Branch: %s\n", QUADD_MODULE_BRANCH);
	pr_info("Version: %s\n", QUADD_MODULE_VERSION);
	pr_info("Samples version: %d\n", QUADD_SAMPLES_VERSION);
	pr_info("IO version: %d\n", QUADD_IO_VERSION);

#ifdef QM_DEBUG_SAMPLES_ENABLE
	pr_info("############## DEBUG VERSION! ##############\n");
#endif
	atomic_set(&ctx.started, 0);

	get_default_properties();

	ctx.pmu_info.active = 0;
	ctx.pl310_info.active = 0;

	ctx.pmu = quadd_armv7_pmu_init();
	if (!ctx.pmu) {
		pr_err("PMU init failed\n");
		return -ENODEV;
	} else {
		events = ctx.pmu_info.supported_events;
		nr_events = ctx.pmu->get_supported_events(events);
		ctx.pmu_info.nr_supported_events = nr_events;

		pr_info("PMU: amount of events: %d\n", nr_events);

		for (i = 0; i < nr_events; i++)
			pr_info("PMU event: %s\n",
				quadd_get_event_str(events[i]));
	}

	ctx.pl310 = quadd_l2x0_events_init();
	if (ctx.pl310) {
		events = ctx.pl310_info.supported_events;
		nr_events = ctx.pl310->get_supported_events(events);
		ctx.pl310_info.nr_supported_events = nr_events;

		pr_info("pl310 success, amount of events: %d\n",
			nr_events);

		for (i = 0; i < nr_events; i++)
			pr_info("pl310 event: %s\n",
				quadd_get_event_str(events[i]));
	} else {
		pr_info("PL310 not found\n");
	}

	ctx.hrt = quadd_hrt_init(&ctx);
	if (!ctx.hrt) {
		pr_err("error: HRT init failed\n");
		return -ENODEV;
	}

	ctx.mmap = quadd_mmap_init(&ctx);
	if (!ctx.mmap) {
		pr_err("error: MMAP init failed\n");
		return -ENODEV;
	}

	err = quadd_power_clk_init(&ctx);
	if (err < 0) {
		pr_err("error: POWER CLK init failed\n");
		return err;
	}

	ctx.comm = quadd_comm_events_init(&control);
	if (!ctx.comm) {
		pr_err("error: COMM init failed\n");
		return -ENODEV;
	}

	err = quadd_auth_init(&ctx);
	if (err < 0) {
		pr_err("error: auth failed\n");
		return err;
	}

	return 0;
}

static void __exit quadd_module_exit(void)
{
	pr_info("QuadD module exit\n");

	quadd_hrt_deinit();
	quadd_mmap_deinit();
	quadd_power_clk_deinit();
	quadd_comm_events_exit();
	quadd_auth_deinit();
}

module_init(quadd_module_init);
module_exit(quadd_module_exit);

MODULE_LICENSE("GPL");

MODULE_AUTHOR("Nvidia Ltd");
MODULE_DESCRIPTION("Tegra profiler");
