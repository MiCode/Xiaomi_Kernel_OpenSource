// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/* system includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/sched/rt.h>
#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/bitops.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/suspend.h>
#include <linux/topology.h>
#include <linux/math64.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#ifdef CONFIG_MTK_DVFSRC
#include <linux/soc/mediatek/mtk_dvfsrc.h>
#include "dvfsrc-exp.h"
#endif /* CONFIG_MTK_DVFSRC */
#include "mtk_cm_mgr_mt6779.h"
#include "mtk_cm_mgr_common.h"

#include <linux/soc/mediatek/mtk-pm-qos.h>
#include <mtk_qos_sram.h>

#ifndef CONFIG_MTK_BASE_POWER
/* FIXME: */
#undef CONFIG_MTK_CPU_FREQ
#endif /* CONFIG_MTK_BASE_POWER */

#ifdef CONFIG_MTK_CPU_FREQ
#include <mtk_cpufreq_platform.h>
#include <mtk_cpufreq_common_api.h>
#endif /* CONFIG_MTK_CPU_FREQ */

static struct delayed_work cm_mgr_work;
static struct mtk_pm_qos_request ddr_opp_req_by_cpu_opp;
static int cm_mgr_cpu_to_dram_opp;

static unsigned int prev_freq_idx[CM_MGR_CPU_CLUSTER];
static unsigned int prev_freq[CM_MGR_CPU_CLUSTER];

static int cm_mgr_idx = -1;

static int cm_mgr_check_dram_type(void)
{
#ifdef CONFIG_MTK_DRAMC_LEGACY
	int ddr_type = get_ddr_type();
	int ddr_hz = dram_steps_freq(0);

	if (ddr_type == TYPE_LPDDR4X || ddr_type == TYPE_LPDDR4)
		cm_mgr_idx = CM_MGR_LP4X_2CH;
	pr_info("#@# %s(%d) ddr_type 0x%x, ddr_hz %d, cm_mgr_idx 0x%x\n",
			__func__, __LINE__, ddr_type, ddr_hz, cm_mgr_idx);
#else
	cm_mgr_idx = 0;
	pr_info("#@# %s(%d) NO CONFIG_MTK_DRAMC_LEGACY !!! set cm_mgr_idx to 0x%x\n",
			__func__, __LINE__, cm_mgr_idx);
#endif /* CONFIG_MTK_DRAMC_LEGACY */

#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	if (cm_mgr_idx >= 0)
		cm_mgr_to_sspm_command(IPI_CM_MGR_DRAM_TYPE, cm_mgr_idx);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

	return cm_mgr_idx;
};

static int cm_mgr_get_idx(void)
{
	if (cm_mgr_idx < 0)
		return cm_mgr_check_dram_type();
	else
		return cm_mgr_idx;
};

static int cm_mgr_get_dram_opp(void);
static struct mtk_pm_qos_request ddr_opp_req;
static ktime_t perf_now;
void cm_mgr_perf_platform_set_status(int enable)
{
	if (enable) {
		if (!cm_mgr_perf_enable)
			return;

		debounce_times_perf_down_local = 0;

		perf_now = ktime_get();

		vcore_power_ratio_up[0] = 30;
		vcore_power_ratio_up[1] = 30;
		vcore_power_ratio_up[2] = 30;
		vcore_power_ratio_up[3] = 30;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
		cm_mgr_to_sspm_command(IPI_CM_MGR_VCORE_POWER_RATIO_UP,
				0 << 16 | vcore_power_ratio_up[0]);
		cm_mgr_to_sspm_command(IPI_CM_MGR_VCORE_POWER_RATIO_UP,
				1 << 16 | vcore_power_ratio_up[1]);
		cm_mgr_to_sspm_command(IPI_CM_MGR_VCORE_POWER_RATIO_UP,
				2 << 16 | vcore_power_ratio_up[2]);
		cm_mgr_to_sspm_command(IPI_CM_MGR_VCORE_POWER_RATIO_UP,
				3 << 16 | vcore_power_ratio_up[3]);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	} else {
		if (debounce_times_perf_down_local < 0)
			return;

		if (++debounce_times_perf_down_local <
				debounce_times_perf_down) {
			if (ktime_ms_delta(ktime_get(), perf_now) < PERF_TIME)
				return;
			cm_mgr_dram_opp_base = -1;
		}

		vcore_power_ratio_up[0] = 100;
		vcore_power_ratio_up[1] = 100;
		vcore_power_ratio_up[2] = 100;
		vcore_power_ratio_up[3] = 100;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
		cm_mgr_to_sspm_command(IPI_CM_MGR_VCORE_POWER_RATIO_UP,
				0 << 16 | vcore_power_ratio_up[0]);
		cm_mgr_to_sspm_command(IPI_CM_MGR_VCORE_POWER_RATIO_UP,
				1 << 16 | vcore_power_ratio_up[1]);
		cm_mgr_to_sspm_command(IPI_CM_MGR_VCORE_POWER_RATIO_UP,
				2 << 16 | vcore_power_ratio_up[2]);
		cm_mgr_to_sspm_command(IPI_CM_MGR_VCORE_POWER_RATIO_UP,
				3 << 16 | vcore_power_ratio_up[3]);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

		debounce_times_perf_down_local = -1;
	}
}
EXPORT_SYMBOL_GPL(cm_mgr_perf_platform_set_status);

void cm_mgr_perf_platform_set_force_status(int enable)
{
	if (enable) {
		if (!cm_mgr_perf_enable)
			return;

		if (!cm_mgr_perf_force_enable)
			return;

		debounce_times_perf_down_force_local = 0;

		if (cm_mgr_dram_opp_base == -1) {
			cm_mgr_dram_opp = cm_mgr_dram_opp_base =
				cm_mgr_get_dram_opp();
			mtk_pm_qos_update_request(&ddr_opp_req,
					cm_mgr_dram_opp);
		} else {
			if (cm_mgr_dram_opp > 0) {
				cm_mgr_dram_opp--;
				mtk_pm_qos_update_request(&ddr_opp_req,
						cm_mgr_dram_opp);
			}
		}

		pm_qos_update_request_status = enable;
	} else {
		if (debounce_times_perf_down_force_local < 0)
			return;

		if (!pm_qos_update_request_status)
			return;

		if ((!cm_mgr_perf_force_enable) ||
				(++debounce_times_perf_down_force_local >=
				 debounce_times_perf_force_down)) {

			if (cm_mgr_dram_opp < cm_mgr_dram_opp_base) {
				cm_mgr_dram_opp++;
				mtk_pm_qos_update_request(&ddr_opp_req,
						cm_mgr_dram_opp);
			} else {
				cm_mgr_dram_opp = cm_mgr_dram_opp_base = -1;
				mtk_pm_qos_update_request(&ddr_opp_req,
					MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE);

				pm_qos_update_request_status = enable;
				debounce_times_perf_down_force_local = -1;
			}
		}
	}
}
EXPORT_SYMBOL_GPL(cm_mgr_perf_platform_set_force_status);

/* no 1200 */
static int phy_to_virt_dram_opp[] = {
	0x0, 0x1, 0x2, 0x3, 0x5, 0x5
};

static int cm_mgr_get_dram_opp(void)
{
	int dram_opp_cur;

#ifdef CONFIG_MTK_DVFSRC
	dram_opp_cur = mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_DRAM_OPP);
#else
	dram_opp_cur = 0;
#endif /* CONFIG_MTK_DVFSRC */
	if (dram_opp_cur < 0 || dram_opp_cur > cm_mgr_num_perf)
		dram_opp_cur = 0;

	return phy_to_virt_dram_opp[dram_opp_cur];
}

static void cm_mgr_process(struct work_struct *work)
{
	mtk_pm_qos_update_request(&ddr_opp_req_by_cpu_opp,
			cm_mgr_cpu_to_dram_opp);
}

static void cm_mgr_update_dram_by_cpu_opp(int cpu_opp)
{
	int ret = 0;
	int dram_opp = 0;

	if (!cm_mgr_cpu_map_dram_enable) {
		if (cm_mgr_cpu_to_dram_opp !=
				MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE) {
			cm_mgr_cpu_to_dram_opp =
				MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE;
			ret = schedule_delayed_work(&cm_mgr_work, 1);
		}
		return;
	}

	if ((cpu_opp >= 0) && (cpu_opp < cm_mgr_cpu_opp_size))
		dram_opp = cm_mgr_cpu_opp_to_dram[cpu_opp];

	if (cm_mgr_cpu_to_dram_opp == dram_opp)
		return;

	cm_mgr_cpu_to_dram_opp = dram_opp;

	ret = schedule_delayed_work(&cm_mgr_work, 1);
}

void check_cm_mgr_status_mt6779(unsigned int cluster, unsigned int freq)
{
#ifdef CONFIG_MTK_CPU_FREQ
	int freq_idx = 0;
	struct mt_cpu_dvfs *p;

	p = id_to_cpu_dvfs(cluster);
	if (p)
		freq_idx = _search_available_freq_idx(p, freq, 0);

	if (freq_idx == prev_freq_idx[cluster])
		return;

	prev_freq_idx[cluster] = freq_idx;
	prev_freq[cluster] = freq;
#else
	prev_freq_idx[cluster] = 0;
	prev_freq[cluster] = 0;
#endif /* CONFIG_MTK_CPU_FREQ */

	if (cm_mgr_use_cpu_to_dram_map)
		cm_mgr_update_dram_by_cpu_opp
			(prev_freq_idx[CM_MGR_CPU_CLUSTER - 1]);
}
EXPORT_SYMBOL_GPL(check_cm_mgr_status_mt6779);

static void cm_mgr_add_cpu_opp_to_ddr_req(void)
{
	char owner[20] = "cm_mgr_cpu_to_dram";

	mtk_pm_qos_add_request(&ddr_opp_req_by_cpu_opp, MTK_PM_QOS_DDR_OPP,
			MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE);

	strncpy(ddr_opp_req_by_cpu_opp.owner,
			owner, sizeof(ddr_opp_req_by_cpu_opp.owner) - 1);

	if (cm_mgr_use_cpu_to_dram_map_new)
		cm_mgr_cpu_map_update_table();
}

static int platform_cm_mgr_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *node = pdev->dev.of_node;
#ifdef CONFIG_MTK_DVFSRC
	int i;
#endif /* CONFIG_MTK_DVFSRC */

	ret = cm_mgr_common_init();
	if (ret) {
		pr_info("[CM_MGR] FAILED TO INIT(%d)\n", ret);
		return ret;
	}

	(void)cm_mgr_get_idx();

	/* required-opps */
	cm_mgr_num_perf = of_count_phandle_with_args(node,
			"required-opps", NULL);

	if (cm_mgr_num_perf > 0) {
		cm_mgr_perfs = devm_kzalloc(&pdev->dev,
				cm_mgr_num_perf * sizeof(int),
				GFP_KERNEL);

		if (!cm_mgr_num_perf) {
			ret = -ENOMEM;
			goto ERROR;
		}

#ifdef CONFIG_MTK_DVFSRC
		for (i = 0; i < cm_mgr_num_perf; i++) {
			cm_mgr_perfs[i] =
				dvfsrc_get_required_opp_performance_state
				(node, i);
		}
#endif /* CONFIG_MTK_DVFSRC */
		cm_mgr_num_array = cm_mgr_num_perf - 2;
	} else
		cm_mgr_num_perf = 0;

	ret = cm_mgr_check_dts_setting(pdev);
	if (ret) {
		pr_info("[CM_MGR] FAILED TO GET DTS DATA(%d)\n", ret);
		return ret;
	}

	cm_mgr_pdev = pdev;

	pr_info("[CM_MGR] platform-cm_mgr_probe Done.\n");

#ifdef CONFIG_MTK_CPU_FREQ
	mt_cpufreq_set_governor_freq_registerCB(check_cm_mgr_status_mt6779);
#endif /* CONFIG_MTK_CPU_FREQ */

	mtk_pm_qos_add_request(&ddr_opp_req, MTK_PM_QOS_DDR_OPP,
			MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE);

	if (cm_mgr_use_cpu_to_dram_map) {
		cm_mgr_add_cpu_opp_to_ddr_req();

		INIT_DELAYED_WORK(&cm_mgr_work, cm_mgr_process);
	}

	return 0;

ERROR:
	return ret;
}

static int platform_cm_mgr_remove(struct platform_device *pdev)
{
	cm_mgr_common_exit();

	kfree(cm_mgr_perfs);
	kfree(cm_mgr_cpu_opp_to_dram);
	kfree(cm_mgr_buf);

	return 0;
}

static const struct of_device_id platform_cm_mgr_of_match[] = {
	{ .compatible = "mediatek,mt6779-cm_mgr", },
	{},
};

static const struct platform_device_id platform_cm_mgr_id_table[] = {
	{ "mt6779-cm_mgr", 0},
	{ },
};

static struct platform_driver mtk_platform_cm_mgr_driver = {
	.probe = platform_cm_mgr_probe,
	.remove	= platform_cm_mgr_remove,
	.driver = {
		.name = "mt6779-cm_mgr",
		.owner = THIS_MODULE,
		.of_match_table = platform_cm_mgr_of_match,
	},
	.id_table = platform_cm_mgr_id_table,
};

/*
 * driver initialization entry point
 */
static int __init platform_cm_mgr_init(void)
{
	return platform_driver_register(&mtk_platform_cm_mgr_driver);
}

static void __exit platform_cm_mgr_exit(void)
{
	platform_driver_unregister(&mtk_platform_cm_mgr_driver);
	pr_info("[CM_MGR] platform-cm_mgr Exit.\n");
}

subsys_initcall(platform_cm_mgr_init);
module_exit(platform_cm_mgr_exit);

MODULE_DESCRIPTION("Mediatek cm_mgr driver");
MODULE_AUTHOR("Morven-CF Yeh<morven-cf.yeh@mediatek.com>");
MODULE_LICENSE("GPL");
