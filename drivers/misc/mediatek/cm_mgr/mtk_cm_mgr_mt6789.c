// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
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
#if IS_ENABLED(CONFIG_MTK_DVFSRC)
#include "dvfsrc-exp.h"
#endif /* CONFIG_MTK_DVFSRC */
#include "mtk_cm_mgr_mt6789.h"
#include "mtk_cm_mgr_common.h"
#if IS_ENABLED(CONFIG_MTK_CM_IPI)
#include "mtk_cm_ipi.h"
#endif /* CONFIG_MTK_CM_IPI */

#define trace_CM_MGR__perf_hint(idx, en, opp, base, hint, force_hint)

#include <linux/pm_qos.h>

static int pm_qos_update_request_status;
static int cm_mgr_dram_opp = -1;
u32 *cm_mgr_perfs;
static struct cm_mgr_hook local_hk;

static unsigned int prev_freq_idx[CM_MGR_CPU_CLUSTER];
static unsigned int prev_freq[CM_MGR_CPU_CLUSTER];

static int cm_mgr_init_done;
static int cm_mgr_idx = -1;
spinlock_t cm_mgr_lock;

u32 cm_mgr_get_perfs_mt6789(int num)
{
	if (num < 0 || num >= cm_mgr_get_num_perf())
		return 0;
	return cm_mgr_perfs[num];
}
EXPORT_SYMBOL_GPL(cm_mgr_get_perfs_mt6789);

static int cm_mgr_check_dram_type(void)
{
#if IS_ENABLED(CONFIG_MTK_DRAMC)
	int ddr_type = mtk_dramc_get_ddr_type();
	int ddr_hz = mtk_dramc_get_steps_freq(0);

	cm_mgr_idx = CM_MGR_LP4;

	pr_info("#@# %s(%d) ddr_type 0x%x, ddr_hz %d, cm_mgr_idx 0x%x\n",
			__func__, __LINE__, ddr_type, ddr_hz, cm_mgr_idx);
#else
	cm_mgr_idx = CM_MGR_LP4;
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

static void cm_mgr_timeout_process(struct work_struct *work)
{
	icc_set_bw(cm_mgr_get_bw_path(), 0, 0);
}

static void cm_mgr_perf_timeout_timer_fn(struct timer_list *timer)
{
	if (pm_qos_update_request_status) {
		cm_mgr_dram_opp = -1;
		cm_mgr_set_dram_opp_base(cm_mgr_dram_opp);
		schedule_delayed_work(&cm_mgr_timeout_work, 1);

		pm_qos_update_request_status = 0;
		debounce_times_perf_down_local_set(-1);
		debounce_times_perf_down_force_local_set(-1);

		trace_CM_MGR__perf_hint(2, 0,
				cm_mgr_dram_opp, cm_mgr_get_dram_opp_base(),
				debounce_times_perf_down_local_get(),
				debounce_times_perf_down_force_local_get());
	}
}

#define PERF_TIME 100

static ktime_t perf_now;
void cm_mgr_perf_platform_set_status_mt6789(int enable)
{
	unsigned long expires;
	int down_local;

	if (enable || pm_qos_update_request_status) {
		expires = jiffies + CM_MGR_PERF_TIMEOUT_MS;
		mod_timer(&cm_mgr_perf_timeout_timer, expires);
	}

	if (enable) {
		if (!cm_mgr_get_perf_enable())
			return;

		debounce_times_perf_down_local_set(0);

		perf_now = ktime_get();

		if (cm_mgr_get_dram_opp_base() == -1) {
			cm_mgr_dram_opp = 0;
			cm_mgr_set_dram_opp_base(cm_mgr_get_num_perf());

		} else {
			if (cm_mgr_dram_opp > 0)
				cm_mgr_dram_opp--;
		}

#if IS_ENABLED(CONFIG_MTK_CM_IPI)
		cm_mgr_dram_opp = cm_mgr_judge_perfs_dram_opp(cm_mgr_dram_opp);
#endif
		icc_set_bw(cm_mgr_get_bw_path(), 0,
					cm_mgr_perfs[cm_mgr_dram_opp]);

		pm_qos_update_request_status = enable;
	} else {
		down_local = debounce_times_perf_down_local_get();
		if (down_local < 0)
			return;

		++down_local;
		debounce_times_perf_down_local_set(down_local);
		if (down_local <
				debounce_times_perf_down_get()) {
			if (cm_mgr_get_dram_opp_base() < 0) {
				icc_set_bw(cm_mgr_get_bw_path(), 0, 0);
				pm_qos_update_request_status = enable;
				debounce_times_perf_down_local_set(-1);
				goto trace;
			}
			if (ktime_ms_delta(ktime_get(), perf_now) < PERF_TIME)
				goto trace;
			cm_mgr_set_dram_opp_base(-1);
		}

		if ((cm_mgr_dram_opp < cm_mgr_get_dram_opp_base()) &&
				(debounce_times_perf_down_get() > 0)) {
			cm_mgr_dram_opp = cm_mgr_get_dram_opp_base() *
				debounce_times_perf_down_local_get() /
				debounce_times_perf_down_get();
			icc_set_bw(cm_mgr_get_bw_path(), 0,
					cm_mgr_perfs[cm_mgr_dram_opp]);
		} else {
			cm_mgr_dram_opp = -1;
			cm_mgr_set_dram_opp_base(cm_mgr_dram_opp);
			icc_set_bw(cm_mgr_get_bw_path(), 0, 0);
			pm_qos_update_request_status = enable;
			debounce_times_perf_down_local_set(-1);
		}
	}

trace:
	trace_CM_MGR__perf_hint(0, enable,
			cm_mgr_dram_opp, cm_mgr_get_dram_opp_base(),
			debounce_times_perf_down_local_get(),
			debounce_times_perf_down_force_local_get());
}
EXPORT_SYMBOL_GPL(cm_mgr_perf_platform_set_status_mt6789);

static void cm_mgr_perf_platform_set_force_status(int enable)
{
	unsigned long expires;
	int down_force_local;

	if (enable || pm_qos_update_request_status) {
		expires = jiffies + CM_MGR_PERF_TIMEOUT_MS;
		mod_timer(&cm_mgr_perf_timeout_timer, expires);
	}

	if (enable) {
		if (!cm_mgr_get_perf_enable())
			return;

		if (!cm_mgr_get_perf_force_enable())
			return;

		debounce_times_perf_down_force_local_set(0);

		if (cm_mgr_get_dram_opp_base() == -1) {
			cm_mgr_dram_opp = 0;
		} else {
			if (cm_mgr_dram_opp > 0)
				cm_mgr_dram_opp--;
		}

#if IS_ENABLED(CONFIG_MTK_CM_IPI)
		cm_mgr_dram_opp = cm_mgr_judge_perfs_dram_opp(cm_mgr_dram_opp);
#endif
		icc_set_bw(cm_mgr_get_bw_path(), 0,
					cm_mgr_perfs[cm_mgr_dram_opp]);

		pm_qos_update_request_status = enable;
	} else {
		down_force_local = debounce_times_perf_down_force_local_get();
		if (down_force_local < 0)
			return;

		if (!pm_qos_update_request_status)
			return;

		++down_force_local;
		debounce_times_perf_down_force_local_set(down_force_local);
		if ((!cm_mgr_get_perf_force_enable()) ||
				(down_force_local >=
				 debounce_times_perf_force_down_get())) {

			if ((cm_mgr_dram_opp < cm_mgr_get_dram_opp_base()) &&
					(debounce_times_perf_down_get() > 0)) {
				cm_mgr_dram_opp = cm_mgr_get_dram_opp_base() *
					down_force_local /
					debounce_times_perf_force_down_get();
				icc_set_bw(cm_mgr_get_bw_path(), 0,
						cm_mgr_perfs[cm_mgr_dram_opp]);
			} else {
				cm_mgr_dram_opp = -1;
				cm_mgr_set_dram_opp_base(cm_mgr_dram_opp);
				icc_set_bw(cm_mgr_get_bw_path(), 0,
						cm_mgr_perfs[cm_mgr_dram_opp]);
				pm_qos_update_request_status = enable;
				debounce_times_perf_down_force_local_set(-1);
			}
		}
	}

	trace_CM_MGR__perf_hint(1, enable,
			cm_mgr_dram_opp, cm_mgr_get_dram_opp_base(),
			debounce_times_perf_down_local_get(),
			debounce_times_perf_down_force_local_get());
}

void cm_mgr_perf_set_status_mt6789(int enable)
{
	if (cm_mgr_get_disable_fb() == 1 && cm_mgr_get_blank_status() == 1)
		enable = 0;

	cm_mgr_perf_platform_set_force_status(enable);

	if (cm_mgr_get_perf_force_enable())
		return;

	cm_mgr_perf_platform_set_status_mt6789(enable);
}
EXPORT_SYMBOL_GPL(cm_mgr_perf_set_status_mt6789);

void cm_mgr_perf_set_force_status_mt6789(int enable)
{
	if (enable != cm_mgr_get_perf_force_enable()) {
		cm_mgr_set_perf_force_enable(enable);
		if (!enable)
			cm_mgr_perf_platform_set_force_status(enable);
	}
}
EXPORT_SYMBOL_GPL(cm_mgr_perf_set_force_status_mt6789);

void check_cm_mgr_status_mt6789(unsigned int cluster, unsigned int freq,
		unsigned int idx)
{
	unsigned int bcpu_opp_max;
	unsigned long spinlock_save_flag;

	if (!cm_mgr_init_done)
		return;

	spin_lock_irqsave(&cm_mgr_lock, spinlock_save_flag);

	prev_freq_idx[cluster] = idx;
	prev_freq[cluster] = freq;

	bcpu_opp_max = prev_freq_idx[CM_MGR_B];

	spin_unlock_irqrestore(&cm_mgr_lock, spinlock_save_flag);
	cm_mgr_update_dram_by_cpu_opp(bcpu_opp_max);
}
EXPORT_SYMBOL_GPL(check_cm_mgr_status_mt6789);

static int platform_cm_mgr_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *node = pdev->dev.of_node;

#if IS_ENABLED(CONFIG_MTK_DVFSRC)
	int i;
#endif /* CONFIG_MTK_DVFSRC */
	struct icc_path *bw_path;

	spin_lock_init(&cm_mgr_lock);
	ret = cm_mgr_common_init();
	if (ret) {
		pr_info("[CM_MGR] FAILED TO INIT(%d)\n", ret);
		return ret;
	}

	(void)cm_mgr_get_idx();

	/* required-opps */
	ret = of_count_phandle_with_args(node, "required-opps", NULL);
	cm_mgr_set_num_perf(ret);
	pr_info("#@# %s(%d) cm_mgr_num_perf %d\n",
			__func__, __LINE__, ret);

	bw_path = of_icc_get(&pdev->dev, "cm-perf-bw");
	if (IS_ERR(bw_path)) {
		dev_info(&pdev->dev, "get cm-perf_bw fail\n");
		cm_mgr_set_bw_path(NULL);
	}

	cm_mgr_set_bw_path(bw_path);

	if (ret > 0) {
		cm_mgr_perfs = devm_kzalloc(&pdev->dev,
				ret * sizeof(u32),
				GFP_KERNEL);

#if IS_ENABLED(CONFIG_MTK_DVFSRC)
		for (i = 0; i < ret; i++) {
			cm_mgr_perfs[i] =
				dvfsrc_get_required_opp_peak_bw(node, i);
				pr_info("#@# cm_mgr_perfs[%d]=%d\n", i, cm_mgr_perfs[i]);
		}
#endif /* CONFIG_MTK_DVFSRC */
		cm_mgr_set_num_array(ret - 1);
	} else
		cm_mgr_set_num_array(0);
	pr_info("#@# %s(%d) cm_mgr_num_array %d\n",
			__func__, __LINE__, cm_mgr_get_num_array());

	ret = cm_mgr_check_dts_setting(pdev);
	if (ret) {
		pr_info("[CM_MGR] FAILED TO GET DTS DATA(%d)\n", ret);
		return ret;
	}

	INIT_DELAYED_WORK(&cm_mgr_timeout_work, cm_mgr_timeout_process);
	timer_setup(&cm_mgr_perf_timeout_timer, cm_mgr_perf_timeout_timer_fn,
			0);

	local_hk.cm_mgr_get_perfs =
		cm_mgr_get_perfs_mt6789;
	local_hk.cm_mgr_perf_set_force_status =
		cm_mgr_perf_set_force_status_mt6789;
	local_hk.check_cm_mgr_status =
		check_cm_mgr_status_mt6789;
	local_hk.cm_mgr_perf_platform_set_status =
		cm_mgr_perf_platform_set_status_mt6789;
	local_hk.cm_mgr_perf_set_status =
		cm_mgr_perf_set_status_mt6789;

	cm_mgr_register_hook(&local_hk);

	cm_mgr_set_pdev(pdev);

	dev_pm_genpd_set_performance_state(&pdev->dev, 0);

	cm_mgr_init_done = 1;

	pr_info("[CM_MGR] platform-cm_mgr_probe Done.\n");

	return 0;
}

static int platform_cm_mgr_remove(struct platform_device *pdev)
{
	cm_mgr_unregister_hook(&local_hk);
	cm_mgr_common_exit();
	icc_put(cm_mgr_get_bw_path());
	kfree(cm_mgr_perfs);

	return 0;
}

static const struct of_device_id platform_cm_mgr_of_match[] = {
	{ .compatible = "mediatek,mt6789-cm_mgr", },
	{},
};

static const struct platform_device_id platform_cm_mgr_id_table[] = {
	{ "mt6789-cm_mgr", 0},
	{ },
};

static struct platform_driver mtk_platform_cm_mgr_driver = {
	.probe = platform_cm_mgr_probe,
	.remove	= platform_cm_mgr_remove,
	.driver = {
		.name = "mt6789-cm_mgr",
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
MODULE_AUTHOR("Howard Hsu<howard.hsu@mediatek.com>");
MODULE_LICENSE("GPL");
