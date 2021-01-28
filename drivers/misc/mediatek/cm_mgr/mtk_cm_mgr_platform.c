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

#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <linux/soc/mediatek/mtk_dvfsrc.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include "mtk_cm_mgr_platform.h"
#include "mtk_cm_mgr_common.h"
#include "dvfsrc-opp.h"

#ifdef CONFIG_MTK_DRAMC_LEGACY
#include <mtk_dramc.h>
#endif /* CONFIG_MTK_DRAMC_LEGACY */

static int cm_mgr_idx = -1;
static int *cm_mgr_buf;

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
	pr_info("#@# %s(%d) NO CONFIG_MTK_DRAMC !!! set cm_mgr_idx to 0x%x\n",
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

/* static struct pm_qos_request ddr_opp_req; */
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
	struct device *dev = &cm_mgr_pdev->dev;

	if (enable) {
		if (!cm_mgr_perf_enable)
			return;

		if (!cm_mgr_perf_force_enable)
			return;

		debounce_times_perf_down_force_local = 0;

		if (cm_mgr_dram_opp_base == -1) {
			cm_mgr_dram_opp = cm_mgr_dram_opp_base =
				cm_mgr_get_dram_opp();
			dev_pm_genpd_set_performance_state(dev,
					cm_mgr_perfs[cm_mgr_dram_opp]);
		} else {
			if (cm_mgr_dram_opp > 0) {
				cm_mgr_dram_opp--;
				dev_pm_genpd_set_performance_state(dev,
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

			if (cm_mgr_dram_opp < cm_mgr_dram_opp_base) {
				cm_mgr_dram_opp++;
				dev_pm_genpd_set_performance_state(dev,
						cm_mgr_perfs[cm_mgr_dram_opp]);
			} else {
				cm_mgr_dram_opp = cm_mgr_dram_opp_base = -1;
				dev_pm_genpd_set_performance_state(dev, 0);

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

int cm_mgr_get_dram_opp(void)
{
	int dram_opp_cur;

	dram_opp_cur = mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_DRAM_OPP);
	if (dram_opp_cur < 0 || dram_opp_cur > cm_mgr_num_perf)
		dram_opp_cur = 0;

	return phy_to_virt_dram_opp[dram_opp_cur];
}
EXPORT_SYMBOL_GPL(cm_mgr_get_dram_opp);

static int platform_cm_mgr_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct device_node *node = pdev->dev.of_node;
	const char *buf;
	int i;

	ret = cm_mgr_common_init();
	if (ret) {
		pr_info("[CM_MGR] FAILED TO INIT(%d)\n", ret);
		return ret;
	}

	(void)cm_mgr_get_idx();

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cm_mgr_base");
	cm_mgr_base = devm_ioremap_resource(dev, res);

	if (IS_ERR((void const *) cm_mgr_base)) {
		pr_info("[CM_MGR] Unable to ioremap registers\n");
		return -1;
	}

	pr_info("[CM_MGR] platform-cm_mgr cm_mgr_base=%p\n",
			cm_mgr_base);

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

		for (i = 0; i < cm_mgr_num_perf; i++) {
			cm_mgr_perfs[i] =
			dvfsrc_get_required_opp_performance_state(node, i);
		}
		cm_mgr_num_array = cm_mgr_num_perf - 2;
	} else
		cm_mgr_num_perf = 0;

	ret = of_property_read_string(node,
			"status", (const char **)&buf);

	if (!ret) {
		if (!strcmp(buf, "enable"))
			cm_mgr_enable = 1;
		else
			cm_mgr_enable = 0;
	}

	cm_mgr_buf = devm_kzalloc(dev, sizeof(int) * 6 * cm_mgr_num_array,
			GFP_KERNEL);
	if (!cm_mgr_buf) {
		ret = -ENOMEM;
		goto ERROR1;
	}

	cpu_power_ratio_down = cm_mgr_buf;
	cpu_power_ratio_up = cpu_power_ratio_down + cm_mgr_num_array;
	debounce_times_down_adb = cpu_power_ratio_up + cm_mgr_num_array;
	debounce_times_up_adb = debounce_times_down_adb + cm_mgr_num_array;
	vcore_power_ratio_down = debounce_times_up_adb + cm_mgr_num_array;
	vcore_power_ratio_up = vcore_power_ratio_down + cm_mgr_num_array;

	ret = of_property_read_u32_array(node, "cm_mgr,cp_down",
			cpu_power_ratio_down, cm_mgr_num_array);
	ret = of_property_read_u32_array(node, "cm_mgr,cp_up",
			cpu_power_ratio_up, cm_mgr_num_array);
	ret = of_property_read_u32_array(node, "cm_mgr,dt_down",
			debounce_times_down_adb, cm_mgr_num_array);
	ret = of_property_read_u32_array(node, "cm_mgr,dt_up",
			debounce_times_up_adb, cm_mgr_num_array);
	ret = of_property_read_u32_array(node, "cm_mgr,vp_down",
			vcore_power_ratio_down, cm_mgr_num_array);
	ret = of_property_read_u32_array(node, "cm_mgr,vp_up",
			vcore_power_ratio_up, cm_mgr_num_array);

	cm_mgr_pdev = pdev;

	pr_info("[CM_MGR] platform-cm_mgr_probe Done.\n");

	return 0;

ERROR1:
	kfree(cm_mgr_perfs);
ERROR:
	return ret;
}

static int platform_cm_mgr_remove(struct platform_device *pdev)
{
	cm_mgr_common_exit();

	kfree(cm_mgr_perfs);
	kfree(cm_mgr_buf);

	return 0;
}

static const struct of_device_id platform_cm_mgr_of_match[] = {
	{ .compatible = "mediatek,mt6779-cm_mgr", },
	{ .compatible = "mediatek,mt6761-cm_mgr", },
	{},
};

static const struct platform_device_id platform_cm_mgr_id_table[] = {
	{ "mt6779-cm_mgr", 0},
	{ "mt6761-cm_mgr", 0},
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
