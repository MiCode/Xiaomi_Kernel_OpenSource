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
#include <linux/interconnect.h>
#ifdef CONFIG_MTK_DVFSRC
#include "dvfsrc-exp.h"
#endif /* CONFIG_MTK_DVFSRC */
#include "mtk_cm_mgr_mt6873.h"
#include "mtk_cm_mgr_common.h"

#ifdef CONFIG_MTK_CPU_FREQ
#include <mtk_cpufreq_platform.h>
#include <mtk_cpufreq_common_api.h>
#endif /* CONFIG_MTK_CPU_FREQ */

/* #define CREATE_TRACE_POINTS */
/* #include "mtk_cm_mgr_events_mt6873.h" */
#define trace_CM_MGR__perf_hint(idx, en, opp, base, hint, force_hint)

#include <linux/pm_qos.h>

static struct delayed_work cm_mgr_work;
static int cm_mgr_cpu_to_dram_opp;

static unsigned int prev_freq_idx[CM_MGR_CPU_CLUSTER];
static unsigned int prev_freq[CM_MGR_CPU_CLUSTER];

static int cm_mgr_idx = -1;

static int cm_mgr_check_dram_type(void)
{
#ifdef CONFIG_MTK_DRAMC
	int ddr_type = mtk_dramc_get_ddr_type();
	int ddr_hz = mtk_dramc_get_steps_freq(0);

	if (ddr_type == TYPE_LPDDR4X || ddr_type == TYPE_LPDDR4)
		cm_mgr_idx = CM_MGR_LP4;
	pr_info("#@# %s(%d) ddr_type 0x%x, ddr_hz %d, cm_mgr_idx 0x%x\n",
			__func__, __LINE__, ddr_type, ddr_hz, cm_mgr_idx);
#else
	cm_mgr_idx = 0;
	pr_info("#@# %s(%d) NO CONFIG_MTK_DRAMC !!! set cm_mgr_idx to 0x%x\n",
			__func__, __LINE__, cm_mgr_idx);
#endif /* CONFIG_MTK_DRAMC */

	if (cm_mgr_idx >= 0)
		cm_mgr_to_sspm_command(IPI_CM_MGR_DRAM_TYPE, cm_mgr_idx);

	return cm_mgr_idx;
};

static int cm_mgr_get_idx(void)
{
	if (cm_mgr_idx < 0)
		return cm_mgr_check_dram_type();
	else
		return cm_mgr_idx;
};

struct timer_list cm_mgr_perf_timeout_timer;
static struct delayed_work cm_mgr_timeout_work;
#define CM_MGR_PERF_TIMEOUT_MS	msecs_to_jiffies(100)

static void cm_mgr_perf_timeout_timer_fn(struct timer_list *timer)
{
	if (pm_qos_update_request_status) {
		cm_mgr_dram_opp = cm_mgr_dram_opp_base = -1;
		schedule_delayed_work(&cm_mgr_timeout_work, 1);

		pm_qos_update_request_status = 0;
		debounce_times_perf_down_local = -1;
		debounce_times_perf_down_force_local = -1;

		trace_CM_MGR__perf_hint(2, 0,
				cm_mgr_dram_opp, cm_mgr_dram_opp_base,
				debounce_times_perf_down_local,
				debounce_times_perf_down_force_local);
	}
}

#define PERF_TIME 100

static ktime_t perf_now;
void cm_mgr_perf_platform_set_status(int enable)
{
	unsigned long expires;

	if (enable || pm_qos_update_request_status) {
		expires = jiffies + CM_MGR_PERF_TIMEOUT_MS;
		mod_timer(&cm_mgr_perf_timeout_timer, expires);
	}

	if (enable) {
		if (!cm_mgr_perf_enable)
			return;

		debounce_times_perf_down_local = 0;

		perf_now = ktime_get();

		if (cm_mgr_dram_opp_base == -1) {
			cm_mgr_dram_opp = 0;
			cm_mgr_dram_opp_base = cm_mgr_num_perf;
			icc_set_bw(cm_perf_bw_path, 0,
					cm_mgr_perfs[cm_mgr_dram_opp]);
		} else {
			if (cm_mgr_dram_opp > 0) {
				cm_mgr_dram_opp--;
				icc_set_bw(cm_perf_bw_path, 0,
						cm_mgr_perfs[cm_mgr_dram_opp]);
			}
		}

		pm_qos_update_request_status = enable;
	} else {
		if (debounce_times_perf_down_local < 0)
			return;

		if (++debounce_times_perf_down_local <
				debounce_times_perf_down) {
			if (cm_mgr_dram_opp_base < 0) {
				icc_set_bw(cm_perf_bw_path, 0, 0);
				pm_qos_update_request_status = enable;
				debounce_times_perf_down_local = -1;
				goto trace;
			}
			if (ktime_ms_delta(ktime_get(), perf_now) < PERF_TIME)
				goto trace;
			cm_mgr_dram_opp_base = -1;
		}

		if ((cm_mgr_dram_opp < cm_mgr_dram_opp_base) &&
				(debounce_times_perf_down > 0)) {
			cm_mgr_dram_opp = cm_mgr_dram_opp_base *
				debounce_times_perf_down_local /
				debounce_times_perf_down;
			icc_set_bw(cm_perf_bw_path, 0,
					cm_mgr_perfs[cm_mgr_dram_opp]);
		} else {
			cm_mgr_dram_opp = cm_mgr_dram_opp_base = -1;
			icc_set_bw(cm_perf_bw_path, 0, 0);
			pm_qos_update_request_status = enable;
			debounce_times_perf_down_local = -1;
		}
	}

trace:
	trace_CM_MGR__perf_hint(0, enable,
			cm_mgr_dram_opp, cm_mgr_dram_opp_base,
			debounce_times_perf_down_local,
			debounce_times_perf_down_force_local);
}
EXPORT_SYMBOL_GPL(cm_mgr_perf_platform_set_status);

void cm_mgr_perf_platform_set_force_status(int enable)
{
	unsigned long expires;

	if (enable || pm_qos_update_request_status) {
		expires = jiffies + CM_MGR_PERF_TIMEOUT_MS;
		mod_timer(&cm_mgr_perf_timeout_timer, expires);
	}

	if (enable) {
		if (!cm_mgr_perf_enable)
			return;

		if (!cm_mgr_perf_force_enable)
			return;

		debounce_times_perf_down_force_local = 0;

		if (cm_mgr_dram_opp_base == -1) {
			cm_mgr_dram_opp = 0;
			icc_set_bw(cm_perf_bw_path, 0,
					cm_mgr_perfs[cm_mgr_dram_opp]);
		} else {
			if (cm_mgr_dram_opp > 0) {
				cm_mgr_dram_opp--;
				icc_set_bw(cm_perf_bw_path, 0,
						cm_mgr_perfs[cm_mgr_dram_opp]);
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

			if ((cm_mgr_dram_opp < cm_mgr_dram_opp_base) &&
					(debounce_times_perf_down > 0)) {
				cm_mgr_dram_opp = cm_mgr_dram_opp_base *
					debounce_times_perf_down_force_local /
					debounce_times_perf_force_down;
				icc_set_bw(cm_perf_bw_path, 0,
						cm_mgr_perfs[cm_mgr_dram_opp]);
			} else {
				cm_mgr_dram_opp = cm_mgr_dram_opp_base = -1;
				icc_set_bw(cm_perf_bw_path, 0,
						cm_mgr_perfs[cm_mgr_dram_opp]);
				pm_qos_update_request_status = enable;
				debounce_times_perf_down_force_local = -1;
			}
		}
	}

	trace_CM_MGR__perf_hint(1, enable,
			cm_mgr_dram_opp, cm_mgr_dram_opp_base,
			debounce_times_perf_down_local,
			debounce_times_perf_down_force_local);
}
EXPORT_SYMBOL_GPL(cm_mgr_perf_platform_set_force_status);

static void cm_mgr_process(struct work_struct *work)
{
	icc_set_bw(cm_perf_bw_path, 0,
			cm_mgr_perfs[cm_mgr_cpu_to_dram_opp]);
}

void cm_mgr_update_dram_by_cpu_opp(int cpu_opp)
{
	int ret = 0;
	int dram_opp = 0;

	if (!cm_mgr_cpu_map_dram_enable) {
		if (cm_mgr_cpu_to_dram_opp != cm_mgr_num_perf) {
			cm_mgr_cpu_to_dram_opp = cm_mgr_num_perf;
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

void check_cm_mgr_status_mt6873(unsigned int cluster, unsigned int freq)
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
EXPORT_SYMBOL_GPL(check_cm_mgr_status_mt6873);

static void cm_mgr_add_cpu_opp_to_ddr_req(void)
{
	struct device *dev = &cm_mgr_pdev->dev;

	dev_pm_genpd_set_performance_state(dev, 0);

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
	pr_info("#@# %s(%d) cm_mgr_num_perf %d\n",
			__func__, __LINE__, cm_mgr_num_perf);

	cm_perf_bw_path = of_icc_get(&pdev->dev, "cm-perf-bw");
	if (IS_ERR(cm_perf_bw_path)) {
		dev_info(&pdev->dev, "get cm-perf_bw fail\n");
		cm_perf_bw_path = NULL;
	}

	if (cm_mgr_num_perf > 0) {
		cm_mgr_perfs = devm_kzalloc(&pdev->dev,
				cm_mgr_num_perf * sizeof(u32),
				GFP_KERNEL);

		if (!cm_mgr_num_perf) {
			ret = -ENOMEM;
			goto ERROR;
		}

#ifdef CONFIG_MTK_DVFSRC
		for (i = 0; i < cm_mgr_num_perf; i++) {
			cm_mgr_perfs[i] =
				dvfsrc_get_required_opp_peak_bw(node, i);
		}
#endif /* CONFIG_MTK_DVFSRC */
		cm_mgr_num_array = cm_mgr_num_perf - 2;
	} else
		cm_mgr_num_array = 0;
	pr_info("#@# %s(%d) cm_mgr_num_array %d\n",
			__func__, __LINE__, cm_mgr_num_array);

	ret = cm_mgr_check_dts_setting(pdev);
	if (ret) {
		pr_info("[CM_MGR] FAILED TO GET DTS DATA(%d)\n", ret);
		return ret;
	}

#ifdef CONFIG_MTK_CPU_FREQ
	mt_cpufreq_set_governor_freq_registerCB(check_cm_mgr_status_mt6873);
#endif /* CONFIG_MTK_CPU_FREQ */

	timer_setup(&cm_mgr_perf_timeout_timer, cm_mgr_perf_timeout_timer_fn,
			NULL);

	if (cm_mgr_use_cpu_to_dram_map) {
		cm_mgr_add_cpu_opp_to_ddr_req();

		INIT_DELAYED_WORK(&cm_mgr_work, cm_mgr_process);
	}

	cm_mgr_pdev = pdev;

	dev_pm_genpd_set_performance_state(&cm_mgr_pdev->dev, 0);

	pr_info("[CM_MGR] platform-cm_mgr_probe Done.\n");

	return 0;

ERROR:
	return ret;
}

static int platform_cm_mgr_remove(struct platform_device *pdev)
{
	cm_mgr_common_exit();
	icc_put(cm_perf_bw_path);
	kfree(cm_mgr_perfs);
	kfree(cm_mgr_cpu_opp_to_dram);
	kfree(cm_mgr_buf);

	return 0;
}

static const struct of_device_id platform_cm_mgr_of_match[] = {
	{ .compatible = "mediatek,mt6873-cm_mgr", },
	{},
};

static const struct platform_device_id platform_cm_mgr_id_table[] = {
	{ "mt6873-cm_mgr", 0},
	{ },
};

static struct platform_driver mtk_platform_cm_mgr_driver = {
	.probe = platform_cm_mgr_probe,
	.remove	= platform_cm_mgr_remove,
	.driver = {
		.name = "mt6873-cm_mgr",
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
