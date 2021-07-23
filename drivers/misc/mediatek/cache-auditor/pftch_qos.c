/*
 * Copyright (C) 2018 MediaTek Inc.
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
#include <linux/moduleparam.h>
#include <linux/smp.h>
#include <linux/cpumask.h>
#include <linux/perf_event.h>
#include "internal.h"

#include <mt-plat/mtk_secure_api.h>
#include <linux/arm-smccc.h>

#include "trace_cache_auditor.h"

#undef pr_fmt
#define pr_fmt(fmt) "Cache-QoS: " fmt

#define LITTLE_NUM 4
#define PFTCH_CPU_WIDTH 4
#define PFTCH_MASK (0xf)
#define GET_PREFETCH_CONFIG(x, cpu) \
	(PFTCH_MASK & (x >> (PFTCH_CPU_WIDTH * cpu)))

/* Set hexadecimal parameter */
int param_set_hex(const char *val, const struct kernel_param *kp)
{
	return kstrtouint(val, 16, (unsigned int *)kp->arg);
}
/* Read hexadecimal parameter */
int param_get_hex(char *buffer, const struct kernel_param *kp)
{
	return scnprintf(buffer, PAGE_SIZE, "0x%x\n",
			*((unsigned int *)kp->arg));
}
/* Combine operations together */
const struct kernel_param_ops param_ops_hex = {
	.set = param_set_hex,
	.get = param_get_hex
};

struct pftch_environment pftch_env;

struct kernel_param_ops pftch_qos_cb = {
	/* Returns 0, or -errno.  arg is in kp->arg. */
	.set = set_pftch_qos_control,
	/* Returns length written or -errno.  Buffer is 4k (ie. be short!) */
	.get = param_get_int,
};
param_check_int(enable, &(pftch_env.is_enabled));
module_param_cb(enable, &pftch_qos_cb, &(pftch_env.is_enabled), 0664);
__MODULE_PARM_TYPE(enable, "int");

unsigned long ctl_pftch_thres_kb = (70 * 1024);
module_param(ctl_pftch_thres_kb, ulong, 0600);

unsigned long ctl_mcore_thres_kb = (550 * 1024 * 1024 / 16);
module_param(ctl_mcore_thres_kb, ulong, 0600);
/*
 * Macro for check type of variable, passed to `module_param`.
 * Just reuse already existed macro for `ulong` type.
 */
#define param_check_hex(name, p) param_check_uint(name, p)
unsigned int ctl_prefetch_mask;
module_param(ctl_prefetch_mask, hex, 0600);

static int ca_qos_callback(struct notifier_block *nb,
		unsigned long qos_status, void *v)
{
	struct qos_bound *bound;
	struct qos_bound_stat *stat;

	bound = (struct qos_bound *) v;
	stat = &bound->stats[bound->idx];
	trace_ca_callback(qos_status, stat->emibw_mon[QOS_EMIBM_TOTAL],
			stat->emibw_mon[QOS_EMIBM_CPU]);

	if (qos_status == QOS_BOUND_BW_FREE && pftch_env.is_congested) {
		pftch_env.is_congested = false;
		trace_ca_congested(false);
	} else if (qos_status > QOS_BOUND_BW_FREE && !pftch_env.is_congested) {
		pftch_env.is_congested = true;
		trace_ca_congested(true);
	}

	return NOTIFY_DONE;
}

struct notifier_block nb = {
	.notifier_call = ca_qos_callback,
};

static inline void pftch_control_ca55(int config)
{
	unsigned long bitmask = 0x0;
	int code;

	switch (config) {
	case 0:
		code = 4;
		break;
	case 1:
		code = 5;
		break;
	case 2:
		code = 6;
		break;
	case 3:
		code = 7;
		break;
	case 4:
		code = 0;
		break;
	case 5:
		code = 1;
		break;
	default:
		return;
	}
	bitmask = (code << 10);
	/* CPUECTLR_EL1 s3_0_c15_c1_4 */
	asm volatile(
			"mrs	x0, s3_0_c15_c1_4\n"
			"and x0, x0, #0xffffffffffffe3ff\n"
			"orr x0, x0, %0\n"
			"msr	s3_0_c15_c1_4, x0\n"
			:: "r" (bitmask)
			: "memory"
			);
}

static inline int should_throttle_pftch(struct pftch_environment *env)
{
	return env->is_congested && env->is_enabled;
}

static inline int is_pftch_disabled(void)
{
	unsigned long val = 5566;

	asm volatile(
			"mrs	%0, s3_0_c15_c1_4\n"
			: "=r"(val)
	);
	trace_ca_set_throttle(val);
	return !!(val & 0x80);
}

static inline void pftch_control_ca76(int config)
{
	unsigned long bitmask = 0x0;
	int code;
	struct arm_smccc_res res;

	code = config & 0x7;

	/*
	 * code is a bit map to config cache enable for:
	 * 0x1: RPF_DIS, [5] Disables region prefetcher.
	 * 0x2: PF_STS_DIS, [7]: Disables store-stride prefetches.
	 * 0x4: PF_DIS, [15]: Disables data-side hardware prefetching.
	 */
	bitmask |= (code & 0x1) << 5;
	bitmask |= (code & 0x2) << (7-1);
	bitmask |= (code & 0x4) << (15-2);
	/* CPUECTLR_EL1: s3_0_c15_c1_4 */
	arm_smccc_smc(MTK_SIP_CACHE_CONTROL,
		0, bitmask, 0, 0, 0, 0, 0, &res);

	trace_ca_cpu_throttled(is_pftch_disabled());
}

static inline void control_prefetch(int cpu)
{
	if (IS_BIG_CORE(cpu))
		pftch_control_ca76(GET_PREFETCH_CONFIG(ctl_prefetch_mask, cpu));
}

static u64 cpu_cnt;

void pftch_qos_tick(int cpu)
{
	struct ca_pmu_stats *cp_stats = &per_cpu(ca_pmu_stats, cpu);
	unsigned long long delta;
	unsigned long now = sched_clock();
	bool over_waste = false, pftch_disabled;
	int i;
	unsigned int total = 0;

	if (!IS_BIG_CORE(cpu))
		return;

	delta = now - cp_stats->prev_clock;
	for (i = 0; i < NR_PMU_COUNTERS; i++) {
		struct perf_event *event = cp_stats->events[i];
		int ret;
		u64 counter;

		if (!event || event->state != PERF_EVENT_STATE_ACTIVE)
			continue;

		ret = perf_event_read_local(event, &counter);
		cp_stats->counters[i] = (counter - cp_stats->prev_counters[i])
			* 1000000000/delta;
		cp_stats->prev_counters[i] = counter;
		cp_stats->prev_clock = now;
		trace_ca_pftch_mb(event, cp_stats->counters[i]);
	}

	/*
	 * condition for over wasted prefetch:
	 *     1. too much store-strid, and
	 *     2. store-stride over 1/2 unused pftch
	 */
	if (cp_stats->counters[L2_PF_ST] * 16 / 1024 > ctl_pftch_thres_kb
			&& cp_stats->counters[L2_PF_ST] >
			(cp_stats->counters[L2_PF_UNUSED] >> 1))
		over_waste = true;

	/* condition for multicore */
	if (cp_stats->counters[L2_PF_UNUSED] > ctl_mcore_thres_kb)
		cpu_cnt |= 0x1 << (cpu - LITTLE_NUM);
	else
		cpu_cnt &= ~(0x1 << (cpu - LITTLE_NUM));

	total = (cpu_cnt & 0x1) + ((cpu_cnt>>1) & 0x1) +
		((cpu_cnt>>2) & 0x1) + ((cpu_cnt>>3) & 0x1);

	if (total < 2)
		return;

	pftch_disabled = is_pftch_disabled();
	if ((over_waste || pftch_disabled)
			&& should_throttle_pftch(&pftch_env)) {
		ctl_prefetch_mask = PFTCH_DISABLE;
		trace_ca_pftch_enabled(true);
	} else if (!should_throttle_pftch(&pftch_env)) {
		ctl_prefetch_mask = PFTCH_ENABLE;
		trace_ca_pftch_enabled(false);
	} else
		trace_ca_pftch_enabled(pftch_disabled);

	control_prefetch(cpu);
}
