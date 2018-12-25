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
#include <mt-plat/mtk_secure_api.h>
#include <mt-plat/mtk_auxadc_intf.h>
#include <linux/topology.h>
#include "mtk_hps_internal.h"
#if defined(CONFIG_MACH_MT6799) || defined(CONFIG_MACH_MT6759) \
|| defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6758)
/*#include "include/pmic_regulator.h"*/
/*#include "mtk_pmic_regulator.h"*/
#ifdef CONFIG_MTK_FREQ_HOPPING
#include "mt_freqhopping.h"
#else
#define FH_PLL0 0
#define FH_PLL1 1
#define FH_PLL4 4
#endif
#endif

#ifdef CONFIG_MTK_ICCS_SUPPORT
#include <mach/mtk_cpufreq_api.h>
#include <mtk_iccs.h>
#endif

#define BUCK_CTRL_DBLOG		(0)

/* FIXME : follow K44 first */
#define CPU_PRI_PERF 20
/* FIXME : follow K44 first */

static struct notifier_block cpu_hotplug_nb;
#if defined(CONFIG_MACH_MT6799) || defined(CONFIG_MACH_MT6759)
static struct notifier_block hps_pm_notifier_func;
#endif
static DECLARE_BITMAP(cpu_cluster0_bits, CONFIG_NR_CPUS);
struct cpumask *mtk_cpu_cluster0_mask = to_cpumask(cpu_cluster0_bits);

static DECLARE_BITMAP(cpu_cluster1_bits, CONFIG_NR_CPUS);
struct cpumask *mtk_cpu_cluster1_mask = to_cpumask(cpu_cluster1_bits);

static DECLARE_BITMAP(cpu_cluster2_bits, CONFIG_NR_CPUS);
struct cpumask *mtk_cpu_cluster2_mask = to_cpumask(cpu_cluster2_bits);

static unsigned long default_cluster0_mask = 0x000F;
static unsigned long default_cluster1_mask = 0x00F0;
static unsigned long default_cluster2_mask = 0x0300;

static int cpu_hotplug_cb_notifier(struct notifier_block *self,
					 unsigned long action, void *hcpu)
{
	unsigned int cpu = (long)hcpu;
	struct cpumask cpuhp_cpumask;
	struct cpumask cpu_online_cpumask;
	unsigned int first_cpu;
#ifdef CONFIG_MACH_MT6799
	int ret;
#endif
	switch (action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
		if (cpu < cpumask_weight(mtk_cpu_cluster0_mask)) {
			first_cpu = cpumask_first_and(cpu_online_mask,
						mtk_cpu_cluster0_mask);
			if (first_cpu == CONFIG_NR_CPUS) {
#ifdef CONFIG_MTK_ICCS_SUPPORT
				if (hps_get_iccs_pwr_status(cpu >> 2) == 0x7) {
					iccs_set_cache_shared_state(cpu >> 2,
					0);
					break;
				}
#endif
#if defined(CONFIG_MACH_MT6799) || defined(CONFIG_MACH_MT6759) \
|| defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6758)
#ifdef CONFIG_MTK_FREQ_HOPPING
				/*1. Turn on ARM PLL*/
				armpll_control(1, 1);
#endif
				/*2. Non-pause FQHP function*/
#if 0
				if (action == CPU_UP_PREPARE_FROZEN)
					mt_pause_armpll(FH_PLL0, 0);
				else
#endif
#ifdef CONFIG_MTK_FREQ_HOPPING
					mt_pause_armpll(FH_PLL0, 0);
#endif
				/*3. Switch to HW mode*/
				mp_enter_suspend(0, 1);
#endif

#if !defined(CONFIG_MACH_MT6765)
				mt_secure_call(MTK_SIP_POWER_UP_CLUSTER, 0, 0,
				0, 0);
#endif
			}
		} else if ((cpu >= cpumask_weight(mtk_cpu_cluster0_mask)) &&
			(cpu < (cpumask_weight(mtk_cpu_cluster0_mask) +
				  cpumask_weight(mtk_cpu_cluster1_mask)))) {
			first_cpu = cpumask_first_and(cpu_online_mask,
			mtk_cpu_cluster1_mask);
			if (first_cpu == CONFIG_NR_CPUS) {
#ifdef CONFIG_MTK_ICCS_SUPPORT
				if (hps_get_iccs_pwr_status(cpu >> 2) == 0x7) {
					iccs_set_cache_shared_state(cpu >> 2,
					0);
					break;
				}
#endif
#if defined(CONFIG_MACH_MT6799) || defined(CONFIG_MACH_MT6759) \
|| defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6758)
#if !defined(CONFIG_MACH_MT6763) && !defined(CONFIG_MACH_MT6758)
				if (hps_ctxt.init_state == INIT_STATE_DONE) {
#if CPU_BUCK_CTRL
					/*1. Power ON VSram*/
					ret = buck_enable(VSRAM_DVFS2, 1);
					if (ret != 1)
						WARN_ON(1);
					/*2. Set the stttle time to 3000us*/
					dsb(sy);
					mdelay(3);
					dsb(sy);
					/*3. Power ON Vproc2*/
					hps_power_on_vproc2();
					dsb(sy);
					mdelay(1);
					dsb(sy);
#endif
				}
#endif
					/*4. Turn on ARM PLL*/
#ifdef CONFIG_MTK_FREQ_HOPPING
					armpll_control(2, 1);
#endif
					/*5. Non-pause FQHP function*/
#if 0
					if (action == CPU_UP_PREPARE_FROZEN)
						mt_pause_armpll(FH_PLL1, 0);
					else
#endif
#ifdef CONFIG_MTK_FREQ_HOPPING
					mt_pause_armpll(FH_PLL1, 0);
#endif
					/*6. Switch to HW mode*/
					mp_enter_suspend(1, 1);
#endif

#if !defined(CONFIG_MACH_MT6765)
				mt_secure_call(MTK_SIP_POWER_UP_CLUSTER, 1, 0,
				0, 0);
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
#ifdef CONFIG_MTK_ICCS_SUPPORT
				if (hps_get_iccs_pwr_status(cpu >> 2) == 0x7) {
					iccs_set_cache_shared_state(cpu >> 2,
					0);
					break;
				}
#endif
#if defined(CONFIG_MACH_MT6799) || defined(CONFIG_MACH_MT6759)
				/*1. Turn on ARM PLL*/
#ifdef CONFIG_MTK_FREQ_HOPPING
				armpll_control(3, 1);
#endif
				/*2. Non-pause FQHP function*/
#if 0
				if (action == CPU_UP_PREPARE_FROZEN)
					mt_pause_armpll(FH_PLL2, 0);
				else
#endif
#ifdef CONFIG_MTK_FREQ_HOPPING
					mt_pause_armpll(FH_PLL2, 0);
#endif
				/*3. Switch to HW mode*/
				mp_enter_suspend(2, 1);
#endif
				mt_secure_call(MTK_SIP_POWER_UP_CLUSTER, 2, 0,
				0, 0);
			}
		}
		break;

#ifdef CONFIG_HOTPLUG_CPU
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		mt_secure_call(MTK_SIP_POWER_DOWN_CORE, cpu, 0, 0, 0);
		arch_get_cluster_cpus(&cpuhp_cpumask,
		arch_get_cluster_id(cpu));
		cpumask_and(&cpu_online_cpumask, &cpuhp_cpumask,
		cpu_online_mask);
		if (!cpumask_weight(&cpu_online_cpumask)) {
#ifdef CONFIG_MTK_ICCS_SUPPORT
			if (hps_get_iccs_pwr_status(cpu >> 2) == 0xb) {
				mt_cpufreq_set_iccs_frequency_by_cluster(1,
				cpu >> 2, iccs_get_shared_cluster_freq());
				iccs_set_cache_shared_state(cpu >> 2, 1);
				break;
			}
#endif
			mt_secure_call(MTK_SIP_POWER_DOWN_CLUSTER, cpu/4, 0,
			0, 0);
#if defined(CONFIG_MACH_MT6799) || defined(CONFIG_MACH_MT6759) \
|| defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6758)
			/*pr_info("End of power off cluster %d\n", cpu/4);*/
			switch (cpu/4) {/*Turn off ARM PLL*/
			case 0:
				/*1. Switch to SW mode*/
				mp_enter_suspend(0, 0);
#ifdef CONFIG_MTK_FREQ_HOPPING
				/*2. Pause FQHP function*/
				if (action == CPU_DEAD_FROZEN)
					mt_pause_armpll(FH_PLL0, 0x11);
				else
					mt_pause_armpll(FH_PLL0, 0x01);

				/*3. Turn off ARM PLL*/
				armpll_control(1, 0);
#endif
				break;
			case 1:
				/*1. Switch to SW mode*/
				mp_enter_suspend(1, 0);
#ifdef CONFIG_MTK_FREQ_HOPPING
				/*2. Pause FQHP function*/
				if (action == CPU_DEAD_FROZEN)
					mt_pause_armpll(FH_PLL1, 0x11);
				else
					mt_pause_armpll(FH_PLL1, 0x01);
				/*3. Turn off ARM PLL*/
				armpll_control(2, 0);
#endif
#if !defined(CONFIG_MACH_MT6763) && !defined(CONFIG_MACH_MT6758)
				if (hps_ctxt.init_state == INIT_STATE_DONE) {
#if CPU_BUCK_CTRL
					/*4. Power off Vproc2*/
					hps_power_off_vproc2();

					/*5. Turn off VSram*/
					ret = buck_enable(VSRAM_DVFS2, 0);
					if (ret == 1)
						WARN_ON(1);
#endif
				}
#endif
				break;
			case 2:
				 /*1. Switch to SW mode*/
				mp_enter_suspend(2, 0);
#ifdef CONFIG_MTK_FREQ_HOPPING
				/*2. Pause FQHP function*/
				if (action == CPU_DEAD_FROZEN)
					mt_pause_armpll(FH_PLL2, 0x11);
				else
					mt_pause_armpll(FH_PLL2, 0x01);

				/*3. Turn off ARM PLL*/
				armpll_control(3, 0);
#endif
				break;
			default:
				break;
			}
#endif
		}
		break;
#endif /* CONFIG_HOTPLUG_CPU */

	default:
		break;
	}
	return NOTIFY_OK;
}
#if defined(CONFIG_MACH_MT6799) || defined(CONFIG_MACH_MT6759)
/*HPS PM notifier*/
static int hps_pm_event(struct notifier_block *notifier,
	unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		mutex_lock(&hps_ctxt.lock);
		hps_ctxt.enabled_backup = hps_ctxt.enabled;
		hps_ctxt.enabled = 0;
		mutex_unlock(&hps_ctxt.lock);
		pr_info
	("[HPS]PM_SUSPEND_PREPARE hps_enabled %d, hps_enabled_backup %d\n",
			hps_ctxt.enabled, hps_ctxt.enabled_backup);
		break;
	case PM_POST_SUSPEND:
		mutex_lock(&hps_ctxt.lock);
		hps_ctxt.enabled = hps_ctxt.enabled_backup;
		mutex_unlock(&hps_ctxt.lock);
		pr_info
	("[HPS]PM_POST_SUSPEND hps_enabled %d, hps_enabled_backup %d\n",
			hps_ctxt.enabled, hps_ctxt.enabled_backup);
		break;
	default:
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}
#endif
static __init int hotplug_cb_init(void)
{
	int ret;
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
	cpu_hotplug_nb = (struct notifier_block) {
		.notifier_call	= cpu_hotplug_cb_notifier,
		.priority	= CPU_PRI_PERF + 1,
	};

	ret = register_cpu_notifier(&cpu_hotplug_nb);
	if (ret)
		return ret;
	pr_info("CPU Hotplug Low Power Notification\n");
#if defined(CONFIG_MACH_MT6799) || defined(CONFIG_MACH_MT6759)
	hps_pm_notifier_func = (struct notifier_block){
		.notifier_call = hps_pm_event,
		.priority = 0,
	};

	ret = register_pm_notifier(&hps_pm_notifier_func);
	if (ret) {
		pr_debug("Failed to register HPS PM notifier.\n");
		return ret;
	}
	pr_info("HPS PM Notification\n");
#endif
	return 0;
}
early_initcall(hotplug_cb_init);
