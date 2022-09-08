// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/cpumask.h>

#include <mtk_idle.h>

#include <mtk_mcdi.h>
#include <mtk_mcdi_state.h>
#include <mtk_mcdi_plat.h>

static int mcdi_idle_state_mapping[NR_TYPES] = {
	MCDI_STATE_DPIDLE,		/* IDLE_TYPE_DP */
	MCDI_STATE_SODI3,		/* IDLE_TYPE_SO3 */

	MCDI_STATE_SODI,		/* IDLE_TYPE_SO */
	MCDI_STATE_CLUSTER_OFF	/* IDLE_TYPE_RG */
};

int mcdi_state_table_idx_map[NF_CPU] = {
	MCDI_STATE_TABLE_SET_0,
	MCDI_STATE_TABLE_SET_0,
	MCDI_STATE_TABLE_SET_0,
	MCDI_STATE_TABLE_SET_0,
	MCDI_STATE_TABLE_SET_0,
	MCDI_STATE_TABLE_SET_0,
	MCDI_STATE_TABLE_SET_1,
	MCDI_STATE_TABLE_SET_1
};

static DECLARE_BITMAP(cpu_set_0_bits, CONFIG_NR_CPUS);
struct cpumask *cpu_set_0_mask = to_cpumask(cpu_set_0_bits);

static DECLARE_BITMAP(cpu_set_1_bits, CONFIG_NR_CPUS);
struct cpumask *cpu_set_1_mask = to_cpumask(cpu_set_1_bits);

static unsigned long default_set_0_mask = 0x003F;
static unsigned long default_set_1_mask = 0x00C0;

static int mtk_rgidle_enter(struct cpuidle_device *dev,
			struct cpuidle_driver *drv, int index);
static int mtk_mcidle_enter(struct cpuidle_device *dev,
			struct cpuidle_driver *drv, int index);

static struct cpuidle_driver mtk_cpuidle_driver_set_0 = {
	.name             = "mtk_acao_cpuidle_set_0",
	.owner            = THIS_MODULE,
	.states[0] = {
		.enter			= mtk_rgidle_enter,
		.exit_latency		= 1,
		.target_residency	= 1,
		.name			= "rgidle",
		.desc			= "wfi"
	},
	.states[1] = {
		.enter			= mtk_mcidle_enter,
		.exit_latency		= 150,
		.target_residency	= 1600,
		.flags			= CPUIDLE_FLAG_TIMER_STOP,
		.name			= "mcdi",
		.desc			= "mcdi",
	},
	.state_count = 2,
	.safe_state_index = 0,
};

static struct cpuidle_driver mtk_cpuidle_driver_set_1 = {
	.name             = "mtk_acao_cpuidle_set_1",
	.owner            = THIS_MODULE,
	.states[0] = {
		.enter			= mtk_rgidle_enter,
		.exit_latency		= 1,
		.target_residency	= 1,
		.name			= "rgidle",
		.desc			= "wfi"
	},
	.states[1] = {
		.enter			= mtk_mcidle_enter,
		.exit_latency		= 150,
		.target_residency	= 1400,
		.flags			= CPUIDLE_FLAG_TIMER_STOP,
		.name			= "mcdi",
		.desc			= "mcdi",
	},
	.state_count = 2,
	.safe_state_index = 0,
};

/*
 * Used for mcdi_governor
 * only use exit_latency & target_residency
 */
static struct cpuidle_driver
	mtk_acao_mcdi_state[NF_MCDI_STATE_TABLE_TYPE] = {
	[0] = {
		.name             = "mtk_acao_mcdi_set_0",
		.owner            = THIS_MODULE,
		.states[0] = {
			.enter              = NULL,
			.exit_latency       = 1,
			.target_residency   = 1,
			.name               = "wfi",
			.desc               = "wfi"
		},
		.states[1] = {
			.enter              = NULL,
			.exit_latency       = 150,
			.target_residency   = 1600,
			.name               = "cpu_off",
			.desc               = "cpu_off",
		},
		.states[2] = {
			.enter              = NULL,
			.exit_latency       = 300,
			.target_residency   = 12500,
			.name               = "cluster_off",
			.desc               = "cluster_off",
		},
		.states[3] = {
			.enter              = NULL,
			.exit_latency       = 1300,
			.target_residency   = 1900,
			.name               = "IdleDram",
			.desc               = "IdleDram",
		},
		.states[4] = {
			.enter              = NULL,
			.exit_latency       = 1300,
			.target_residency   = 1900,
			.name               = "IdleSyspll",
			.desc               = "IdleSyspll",
		},
		.states[5] = {
			.enter              = NULL,
			.exit_latency       = 4800,
			.target_residency   = 1,
			.name               = "IdleBus26m",
			.desc               = "IdleBus26m",
		},
		.states[6] = {
			.enter              = NULL,
			.exit_latency       = 4800,
			.target_residency   = 1,
			.name               = "IdleBus26m",
			.desc               = "IdleBus26m",
		},
		.state_count = 6,
		.safe_state_index = 0,
	},
	[1] = {
		.name             = "mtk_acao_mcdi_set_1",
		.owner            = THIS_MODULE,
		.states[0] = {
			.enter              = NULL,
			.exit_latency       = 1,
			.target_residency   = 1,
			.name               = "wfi",
			.desc               = "wfi"
		},
		.states[1] = {
			.enter              = NULL,
			.exit_latency       = 150,
			.target_residency   = 1400,
			.name               = "cpu_off",
			.desc               = "cpu_off",
		},
		.states[2] = {
			.enter              = NULL,
			.exit_latency       = 300,
			.target_residency   = 12500,
			.name               = "cluster_off",
			.desc               = "cluster_off",
		},
		.states[3] = {
			.enter              = NULL,
			.exit_latency       = 1300,
			.target_residency   = 2300,
			.name               = "IdleDram",
			.desc               = "IdleDram",
		},
		.states[4] = {
			.enter              = NULL,
			.exit_latency       = 1300,
			.target_residency   = 2300,
			.name               = "IdleSyspll",
			.desc               = "IdleSyspll",
		},
		.states[5] = {
			.enter              = NULL,
			.exit_latency       = 4800,
			.target_residency   = 1,
			.name               = "IdleBus26m",
			.desc               = "IdleBus26m",
		},
		.states[6] = {
			.enter              = NULL,
			.exit_latency       = 4800,
			.target_residency   = 1,
			.name               = "IdleBus26m",
			.desc               = "IdleBus26m",

		},
		.state_count = 6,
		.safe_state_index = 0,
	}
};

#define invalid_type_and_state(type, state)                \
	(state <= MCDI_STATE_WFI || state >= NF_MCDI_STATE \
		|| type < 0 || type >= NF_CPU_TYPE)        \

#define __mcdi_set_state(type, i, member, val)                           \
do {                                                                     \
	mtk_acao_mcdi_state[type].states[i].member = val;                \
	if (i == MCDI_STATE_CPU_OFF) {                                   \
		if (type == CPU_TYPE_L)                                  \
			mtk_cpuidle_driver_set_0.states[i].member = val; \
		else if (type == CPU_TYPE_B)                             \
			mtk_cpuidle_driver_set_1.states[i].member = val; \
	}                                                                \
} while (0)

void mcdi_set_state_lat(int cpu_type, int state, unsigned int val)
{
	if (invalid_type_and_state(cpu_type, state))
		return;

	__mcdi_set_state(cpu_type, state, exit_latency, val);
}

void mcdi_set_state_res(int cpu_type, int state, unsigned int val)
{
	if (invalid_type_and_state(cpu_type, state))
		return;

	__mcdi_set_state(cpu_type, state, target_residency, val);
}

static int mtk_rgidle_enter(struct cpuidle_device *dev,
			      struct cpuidle_driver *drv, int index)
{
	wfi_enter(dev->cpu);
	return index;
}

static int mtk_mcidle_enter(struct cpuidle_device *dev,
			      struct cpuidle_driver *drv, int index)
{
	mcdi_enter(dev->cpu);
	return index;
}

int mcdi_get_mcdi_idle_state(int idx)
{
	int state = MCDI_STATE_CLUSTER_OFF;

	if (idx >= 0 && idx < NR_TYPES)
		state = mcdi_idle_state_mapping[idx];

	return state;
}

struct cpuidle_driver *mcdi_state_tbl_get(int cpu)
{
	int tbl_idx;

	tbl_idx = mcdi_state_table_idx_map[cpu];

	return &mtk_acao_mcdi_state[tbl_idx];
}
static long mcdi_do_work(void *pData)
{
	int cpu;
	int idx;
	struct cpuidle_driver *drv;

	cpu = smp_processor_id();
	cpumask_clear(cpu_set_0_mask);
	cpumask_clear(cpu_set_1_mask);
	cpu_set_0_mask->bits[0] = default_set_0_mask;
	cpu_set_1_mask->bits[0] = default_set_1_mask;
	drv = cpuidle_get_driver();
	if (!drv) {
		pr_info("%s, cpuidle_idle_drv = NULL\n", __func__);
		return 0;
	}
	drv->cpumask = cpu_set_1_mask;
	drv->states[0].exit_latency = mtk_acao_mcdi_state[0].states[0].exit_latency;
	drv->states[0].exit_latency_ns =
			mtk_acao_mcdi_state[0].states[0].exit_latency * NSEC_PER_USEC;
	for (idx = 1; idx < drv->state_count; ++idx) {
		pr_info("%s,register cpu idle for cpu %d\n", __func__, cpu);
		drv->states[0].enter = mtk_rgidle_enter;
		drv->states[idx].enter = mtk_mcidle_enter;
		drv->states[idx].exit_latency = mtk_acao_mcdi_state[0].states[idx].exit_latency;
		drv->states[idx].exit_latency_ns =
				mtk_acao_mcdi_state[0].states[idx].exit_latency * NSEC_PER_USEC;
	}
		return 0;
}

int mtk_cpuidle_register_driver(void)
{
	int cpu;

    cpuidle_pause_and_lock();
	for_each_online_cpu(cpu) {
		work_on_cpu(cpu, mcdi_do_work, NULL);
	}
    cpuidle_resume_and_unlock();

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_cpuidle_register_driver);


