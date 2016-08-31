/*
 * drivers/misc/tegra-profiler/main.c
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/err.h>
#include <linux/sched.h>

#include <linux/tegra_profiler.h>

#include "quadd.h"
#include "armv7_pmu.h"
#include "hrt.h"
#include "comm.h"
#include "mmap.h"
#include "debug.h"
#include "tegra.h"
#include "power_clk.h"
#include "auth.h"
#include "version.h"
#include "quadd_proc.h"
#include "eh_unwind.h"

#ifdef CONFIG_CACHE_L2X0
#include "pl310.h"
#endif

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

int tegra_profiler_try_lock(void)
{
	return atomic_cmpxchg(&ctx.tegra_profiler_lock, 0, 1);
}
EXPORT_SYMBOL_GPL(tegra_profiler_try_lock);

void tegra_profiler_unlock(void)
{
	atomic_set(&ctx.tegra_profiler_lock, 0);
}
EXPORT_SYMBOL_GPL(tegra_profiler_unlock);

static int start(void)
{
	int err;

	if (tegra_profiler_try_lock()) {
		pr_err("Error: tegra_profiler lock\n");
		return -EBUSY;
	}

	if (!atomic_cmpxchg(&ctx.started, 0, 1)) {
		if (ctx.pmu) {
			err = ctx.pmu->enable();
			if (err) {
				pr_err("error: pmu enable\n");
				goto errout;
			}
		}

		if (ctx.pl310) {
			err = ctx.pl310->enable();
			if (err) {
				pr_err("error: pl310 enable\n");
				goto errout;
			}
		}

		ctx.comm->reset();

		err = quadd_power_clk_start();
		if (err < 0) {
			pr_err("error: power_clk start\n");
			goto errout;
		}

		err = quadd_hrt_start();
		if (err) {
			pr_err("error: hrt start\n");
			goto errout;
		}
	}

	return 0;

errout:
	atomic_set(&ctx.started, 0);
	tegra_profiler_unlock();
	return err;
}

static void stop(void)
{
	if (atomic_cmpxchg(&ctx.started, 1, 0)) {
		quadd_hrt_stop();

		ctx.comm->reset();

		quadd_power_clk_stop();
		quadd_unwind_stop();

		if (ctx.pmu)
			ctx.pmu->disable();

		if (ctx.pl310)
			ctx.pl310->disable();

		tegra_profiler_unlock();
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
	unsigned int extra;

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

	for (i = 0; i < ARRAY_SIZE(param->reserved); i++)
		ctx.param.reserved[i] = param->reserved[i];

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
		ctx.collect_kernel_ips = 0;
	} else {
		ctx.collect_kernel_ips = 1;
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

	extra = param->reserved[QUADD_PARAM_IDX_EXTRA];

	if (extra & QUADD_PARAM_EXTRA_BT_UNWIND_TABLES)
		pr_info("unwinding: exception-handling tables\n");

	if (extra & QUADD_PARAM_EXTRA_BT_FP)
		pr_info("unwinding: frame pointers\n");

	quadd_unwind_start(task);

	pr_info("New parameters have been applied\n");

	return 0;
}

static void get_capabilities(struct quadd_comm_cap *cap)
{
	int i, event;
	unsigned int extra = 0;
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
				pr_err_once("%s: error: invalid event\n",
					    __func__);
				return;
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
				pr_err_once("%s: error: invalid event\n",
					    __func__);
				return;
			}
		}
	}

	cap->tegra_lp_cluster = quadd_is_cpu_with_lp_cluster();
	cap->power_rate = 1;
	cap->blocked_read = 1;

	extra |= QUADD_COMM_CAP_EXTRA_BT_KERNEL_CTX;
	extra |= QUADD_COMM_CAP_EXTRA_GET_MMAP;
	extra |= QUADD_COMM_CAP_EXTRA_GROUP_SAMPLES;

	cap->reserved[QUADD_COMM_CAP_IDX_EXTRA] = extra;
}

void quadd_get_state(struct quadd_module_state *state)
{
	unsigned int status = 0;

	quadd_hrt_get_state(state);

	if (ctx.comm->is_active())
		status |= QUADD_MOD_STATE_STATUS_IS_ACTIVE;

	if (quadd_auth_is_auth_open())
		status |= QUADD_MOD_STATE_STATUS_IS_AUTH_OPEN;

	state->reserved[QUADD_MOD_STATE_IDX_STATUS] = status;
}

static int
set_extab(struct quadd_extables *extabs)
{
	return quadd_unwind_set_extab(extabs);
}

static struct quadd_comm_control_interface control = {
	.start			= start,
	.stop			= stop,
	.set_parameters		= set_parameters,
	.get_capabilities	= get_capabilities,
	.get_state		= quadd_get_state,
	.set_extab		= set_extab,
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
	atomic_set(&ctx.tegra_profiler_lock, 0);

	get_default_properties();

	ctx.pmu_info.active = 0;
	ctx.pl310_info.active = 0;

	ctx.pmu = quadd_armv7_pmu_init();
	if (!ctx.pmu) {
		pr_err("PMU init failed\n");
		return -ENODEV;
	} else {
		events = ctx.pmu_info.supported_events;
		nr_events = ctx.pmu->get_supported_events(events,
							  QUADD_MAX_COUNTERS);
		ctx.pmu_info.nr_supported_events = nr_events;

		pr_info("PMU: amount of events: %d\n", nr_events);

		for (i = 0; i < nr_events; i++)
			pr_info("PMU event: %s\n",
				quadd_get_event_str(events[i]));
	}

#ifdef CONFIG_CACHE_L2X0
	ctx.pl310 = quadd_l2x0_events_init();
#else
	ctx.pl310 = NULL;
#endif
	if (ctx.pl310) {
		events = ctx.pl310_info.supported_events;
		nr_events = ctx.pl310->get_supported_events(events,
							    QUADD_MAX_COUNTERS);
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
	if (IS_ERR(ctx.hrt)) {
		pr_err("error: HRT init failed\n");
		return PTR_ERR(ctx.hrt);
	}

	err = quadd_power_clk_init(&ctx);
	if (err < 0) {
		pr_err("error: POWER CLK init failed\n");
		return err;
	}

	ctx.comm = quadd_comm_events_init(&control);
	if (IS_ERR(ctx.comm)) {
		pr_err("error: COMM init failed\n");
		return PTR_ERR(ctx.comm);
	}

	err = quadd_auth_init(&ctx);
	if (err < 0) {
		pr_err("error: auth failed\n");
		return err;
	}

	err = quadd_unwind_init();
	if (err < 0) {
		pr_err("error: EH unwinding init failed\n");
		return err;
	}

	get_capabilities(&ctx.cap);
	quadd_proc_init(&ctx);

	return 0;
}

static void __exit quadd_module_exit(void)
{
	pr_info("QuadD module exit\n");

	quadd_hrt_deinit();
	quadd_power_clk_deinit();
	quadd_comm_events_exit();
	quadd_auth_deinit();
	quadd_proc_deinit();
	quadd_armv7_pmu_deinit();
	quadd_unwind_deinit();
}

module_init(quadd_module_init);
module_exit(quadd_module_exit);

MODULE_LICENSE("GPL");

MODULE_AUTHOR("Nvidia Ltd");
MODULE_DESCRIPTION("Tegra profiler");
