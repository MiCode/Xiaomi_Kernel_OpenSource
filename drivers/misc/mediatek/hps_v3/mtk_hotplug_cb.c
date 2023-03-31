// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/bug.h>
#include <linux/suspend.h>
#include <asm/cacheflush.h>
/*
 *#include <mt-plat/mtk_secure_api.h>
 *#include <mt-plat/mtk_auxadc_intf.h>
 */
#include <linux/topology.h>
#include "mtk_hps_internal.h"

#if IS_ENABLED(CONFIG_MTK_ICCS_SUPPORT)
#include <mach/mtk_cpufreq_api.h>
#include <mtk_iccs.h>
#endif

#define BUCK_CTRL_DBLOG		(0)

/* FIXME : follow K44 first */
#define CPU_PRI_PERF 20
/* FIXME : follow K44 first */

static DECLARE_BITMAP(cpu_cluster0_bits, CONFIG_NR_CPUS);
struct cpumask *mtk_cpu_cluster0_mask = to_cpumask(cpu_cluster0_bits);

static DECLARE_BITMAP(cpu_cluster1_bits, CONFIG_NR_CPUS);
struct cpumask *mtk_cpu_cluster1_mask = to_cpumask(cpu_cluster1_bits);

static DECLARE_BITMAP(cpu_cluster2_bits, CONFIG_NR_CPUS);
struct cpumask *mtk_cpu_cluster2_mask = to_cpumask(cpu_cluster2_bits);

static unsigned long default_cluster0_mask = 0x000F;
static unsigned long default_cluster1_mask = 0x00F0;
static unsigned long default_cluster2_mask = 0x0300;

static int cpu_hotplug_cb_notifier(unsigned long action, int cpu)
{
	struct cpumask cpuhp_cpumask;
	struct cpumask cpu_online_cpumask;
	unsigned int first_cpu;

	switch (action) {
	case CPU_UP_PREPARE:
		if (cpu < cpumask_weight(mtk_cpu_cluster0_mask)) {
			first_cpu = cpumask_first_and(cpu_online_mask,
						mtk_cpu_cluster0_mask);
			if (first_cpu == CONFIG_NR_CPUS) {
#if IS_ENABLED(CONFIG_MTK_ICCS_SUPPORT)
				if (hps_get_iccs_pwr_status(cpu >> 2) == 0x7) {
					iccs_set_cache_shared_state(cpu >> 2,
					0);
					break;
				}
#endif
			}
		} else if ((cpu >= cpumask_weight(mtk_cpu_cluster0_mask)) &&
			(cpu < (cpumask_weight(mtk_cpu_cluster0_mask) +
				  cpumask_weight(mtk_cpu_cluster1_mask)))) {
			first_cpu = cpumask_first_and(cpu_online_mask,
			mtk_cpu_cluster1_mask);
			if (first_cpu == CONFIG_NR_CPUS) {
#if IS_ENABLED(CONFIG_MTK_ICCS_SUPPORT)
				if (hps_get_iccs_pwr_status(cpu >> 2) == 0x7) {
					iccs_set_cache_shared_state(cpu >> 2,
					0);
					break;
				}
#endif
			}
		} else if ((cpu >= (cpumask_weight(mtk_cpu_cluster0_mask) +
				cpumask_weight(mtk_cpu_cluster1_mask))) &&
				(cpu < (cpumask_weight(mtk_cpu_cluster0_mask) +
				cpumask_weight(mtk_cpu_cluster1_mask) +
				cpumask_weight(mtk_cpu_cluster2_mask))))  {
			first_cpu = cpumask_first_and(cpu_online_mask,
			mtk_cpu_cluster2_mask);
			if (first_cpu == CONFIG_NR_CPUS) {
#if IS_ENABLED(CONFIG_MTK_ICCS_SUPPORT)
				if (hps_get_iccs_pwr_status(cpu >> 2) == 0x7) {
					iccs_set_cache_shared_state(cpu >> 2,
					0);
					break;
				}
#endif
				mt_secure_call(MTK_SIP_POWER_UP_CLUSTER, 2, 0,
				0, 0);
			}
		}
		break;

#if IS_ENABLED(CONFIG_HOTPLUG_CPU)
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		mt_secure_call(MTK_SIP_POWER_DOWN_CORE, cpu, 0, 0, 0);
		arch_get_cluster_cpus(&cpuhp_cpumask,
		arch_get_cluster_id(cpu));
		cpumask_and(&cpu_online_cpumask, &cpuhp_cpumask,
		cpu_online_mask);
		if (!cpumask_weight(&cpu_online_cpumask)) {
#if IS_ENABLED(CONFIG_MTK_ICCS_SUPPORT)
			if (hps_get_iccs_pwr_status(cpu >> 2) == 0xb) {
				mt_cpufreq_set_iccs_frequency_by_cluster(1,
				cpu >> 2, iccs_get_shared_cluster_freq());
				iccs_set_cache_shared_state(cpu >> 2, 1);
				break;
			}
#endif
			mt_secure_call(MTK_SIP_POWER_DOWN_CLUSTER, cpu/4, 0,
			0, 0);

		}
		break;
#endif /* CONFIG_HOTPLUG_CPU */

	default:
		break;
	}
	return NOTIFY_OK;
}

static int cpuhp_cpu_up(unsigned int cpu)
{
	cpu_hotplug_cb_notifier(CPU_UP_PREPARE, cpu);
	return 0;
}

static int cpuhp_cpu_dead(unsigned int cpu)
{
	cpu_hotplug_cb_notifier(CPU_DEAD, cpu);
	return 0;
}

int hotplug_cb_init(void)
{
	int i;

	mp_enter_suspend(0, 1);/*Switch LL cluster to HW mode*/

	cpumask_clear(mtk_cpu_cluster0_mask);
	cpumask_clear(mtk_cpu_cluster1_mask);
	cpumask_clear(mtk_cpu_cluster2_mask);
	mtk_cpu_cluster0_mask->bits[0] = default_cluster0_mask;
	mtk_cpu_cluster1_mask->bits[0] = default_cluster1_mask;
	mtk_cpu_cluster2_mask->bits[0] = default_cluster2_mask;
	for (i = 0; i < num_possible_cpus(); i++)
		set_cpu_present(i, true);

	cpuhp_setup_state_nocalls(CPUHP_BP_PREPARE_DYN,
				"hps/cpuhotplug",
				cpuhp_cpu_up,
				cpuhp_cpu_dead);
	return 0;
}

