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
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <linux/interconnect.h>
#if IS_ENABLED(CONFIG_MTK_DVFSRC)
#include "dvfsrc-exp.h"
#endif /* CONFIG_MTK_DVFSRC */
#include "mtk_cm_mgr_mt6895.h"
#include "mtk_cm_mgr_common.h"
#if IS_ENABLED(CONFIG_MTK_CM_IPI)
#include "mtk_cm_ipi.h"
#endif /* CONFIG_MTK_CM_IPI */

/* #define CREATE_TRACE_POINTS */
/* #include "mtk_cm_mgr_events_mt6895.h" */
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

#if IS_ENABLED(CONFIG_MTK_CM_IPI)
void __iomem *csram_base;
static unsigned int mcl50_flag;


static void cm_get_base_addr(void)
{
	int ret = 0;
	struct device_node *dn = NULL;
	struct platform_device *pdev = NULL;
	struct resource *csram_res = NULL;

	/* get cpufreq driver base address */
	dn = of_find_node_by_name(NULL, "cpuhvfs");
	if (!dn) {
		ret = -ENOMEM;
		pr_info("find cpuhvfs node failed\n");
		return;
	}

	pdev = of_find_device_by_node(dn);
	of_node_put(dn);
	if (!pdev) {
		ret = -ENODEV;
		pr_info("cpuhvfs is not ready\n");
		return;
	}

	csram_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!csram_res) {
		ret = -ENODEV;
		pr_info("cpuhvfs resource is not found\n");
		return;
	}

	csram_base = ioremap(csram_res->start, resource_size(csram_res));
	if (IS_ERR_OR_NULL((void *)csram_base)) {
		ret = -ENOMEM;
		pr_info("find csram base failed\n");
		return;
	}
}

unsigned int csram_read(unsigned int offs)
{
	if (IS_ERR_OR_NULL((void *)csram_base))
		return 0;
	return __raw_readl(csram_base + (offs));
}

void csram_write(unsigned int offs, unsigned int val)
{
	if (IS_ERR_OR_NULL((void *)csram_base))
		return;
	__raw_writel(val, csram_base + (offs));
}
#endif

u32 cm_mgr_get_perfs_mt6895(int num)
{
	if (num < 0 || num >= cm_mgr_get_num_perf())
		return 0;
	return cm_mgr_perfs[num];
}
EXPORT_SYMBOL_GPL(cm_mgr_get_perfs_mt6895);

static int cm_mgr_check_dram_type(void)
{
#if IS_ENABLED(CONFIG_MTK_DRAMC)
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
#if IS_ENABLED(CONFIG_MTK_CM_IPI)
static unsigned int dsu_cnt;
#endif
void cm_mgr_perf_platform_set_status_mt6895(int enable)
{
	unsigned long expires;
	int down_local;
	int cm_thresh;


	cm_thresh = get_cm_step_num();
	csram_write(OFFS_CM_THRESH, cm_thresh);

	if (enable || pm_qos_update_request_status) {
		expires = jiffies + CM_MGR_PERF_TIMEOUT_MS;
		mod_timer(&cm_mgr_perf_timeout_timer, expires);
	}
#if IS_ENABLED(CONFIG_MTK_CM_IPI)
	if (mcl50_flag) {
		csram_write(OFFS_CCI_TBL_MODE, 1);
		dsu_cnt++;
		if (dsu_cnt >= PERF_TIME)
			mcl50_flag = 0;
	}
#endif
	if (enable) {
		if (!cm_mgr_get_perf_enable())
			return;

		debounce_times_perf_down_local_set(0);

		perf_now = ktime_get();

		csram_write(OFFS_CM_HINT, 0x3);
		if (cm_mgr_get_dram_opp_base() == -1) {
			cm_mgr_dram_opp = 0;
			cm_mgr_set_dram_opp_base(cm_mgr_get_num_perf());
			if (!cm_thresh)
				icc_set_bw(cm_mgr_get_bw_path(), 0,
					cm_mgr_perfs[cm_mgr_dram_opp]);
		} else {
			if (cm_mgr_dram_opp > 0) {
				cm_mgr_dram_opp--;
				if (!cm_thresh)
					icc_set_bw(cm_mgr_get_bw_path(), 0,
						cm_mgr_perfs[cm_mgr_dram_opp]);
			}
		}

		pm_qos_update_request_status = enable;
	} else {
		csram_write(OFFS_CM_HINT, 0x2);
		down_local = debounce_times_perf_down_local_get();
		if (down_local < 0)
			return;

		++down_local;
		debounce_times_perf_down_local_set(down_local);
		if (down_local <
				debounce_times_perf_down_get()) {
			if (cm_mgr_get_dram_opp_base() < 0) {
				if (!cm_thresh)
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
			if (!cm_thresh)
				icc_set_bw(cm_mgr_get_bw_path(), 0,
					cm_mgr_perfs[cm_mgr_dram_opp]);
		} else {
			cm_mgr_dram_opp = -1;
			cm_mgr_set_dram_opp_base(cm_mgr_dram_opp);
			if (!cm_thresh)
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
EXPORT_SYMBOL_GPL(cm_mgr_perf_platform_set_status_mt6895);

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
			icc_set_bw(cm_mgr_get_bw_path(), 0,
					cm_mgr_perfs[cm_mgr_dram_opp]);
		} else {
			if (cm_mgr_dram_opp > 0) {
				cm_mgr_dram_opp--;
				icc_set_bw(cm_mgr_get_bw_path(), 0,
						cm_mgr_perfs[cm_mgr_dram_opp]);
			}
		}

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

void cm_mgr_perf_set_status_mt6895(int enable)
{
	if (cm_mgr_get_disable_fb() == 1 && cm_mgr_get_blank_status() == 1)
		enable = 0;

	cm_mgr_perf_platform_set_force_status(enable);

	if (cm_mgr_get_perf_force_enable())
		return;

	cm_mgr_perf_platform_set_status_mt6895(enable);
}
EXPORT_SYMBOL_GPL(cm_mgr_perf_set_status_mt6895);

void cm_mgr_perf_set_force_status_mt6895(int enable)
{
	if (enable != cm_mgr_get_perf_force_enable()) {
		cm_mgr_set_perf_force_enable(enable);
		if (!enable)
			cm_mgr_perf_platform_set_force_status(enable);
	}
}
EXPORT_SYMBOL_GPL(cm_mgr_perf_set_force_status_mt6895);

void check_cm_mgr_status_mt6895(unsigned int cluster, unsigned int freq,
		unsigned int idx)
{
	unsigned int bcpu_opp_max;
	unsigned long spinlock_save_flag;

	if (!cm_mgr_init_done)
		return;

	if (cluster > CM_MGR_CPU_CLUSTER)
		return;

	spin_lock_irqsave(&cm_mgr_lock, spinlock_save_flag);

	prev_freq_idx[cluster] = idx;
	prev_freq[cluster] = freq;

	if (prev_freq_idx[CM_MGR_B] < prev_freq_idx[CM_MGR_BB])
		bcpu_opp_max = prev_freq_idx[CM_MGR_B];
	else
		bcpu_opp_max = prev_freq_idx[CM_MGR_BB];

	spin_unlock_irqrestore(&cm_mgr_lock, spinlock_save_flag);
	cm_mgr_update_dram_by_cpu_opp(bcpu_opp_max);
}
EXPORT_SYMBOL_GPL(check_cm_mgr_status_mt6895);

static int platform_cm_mgr_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *node = pdev->dev.of_node;
#if IS_ENABLED(CONFIG_MTK_CM_IPI)
	struct device_node *mcl_node = NULL;
	int err;
	char *buf;
#endif
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
		}
#endif /* CONFIG_MTK_DVFSRC */
		cm_mgr_set_num_array(ret - 1);
	} else
		cm_mgr_set_num_array(0);
	pr_info("#@# %s(%d) cm_mgr_num_array %d\n",
			__func__, __LINE__, cm_mgr_get_num_array());

#if IS_ENABLED(CONFIG_MTK_CM_IPI)
	cm_get_base_addr();
	mcl_node = of_find_compatible_node(NULL, NULL, "mediatek,cpufreq-hw");
	if (mcl_node) {

		err = of_property_read_string(mcl_node,
			"dvfs-config", (const char **)&buf);
		if (!err) {
			if (!strcmp(buf, "mcl50")) {
				pr_info("@[CM_MGR] send dsu perf to sspm\n");
				cm_mgr_to_sspm_command(IPI_CM_MGR_DSU_MODE, 1);
				pr_info("@[CM_MGR] dsu mode %d\n", csram_read(OFFS_CCI_TBL_MODE));
				mcl50_flag = 1;

			}
		} else
			pr_info("@[CM_MGR] not mcl50 load %d\n", err);
	} else
		pr_info("CM_MGR cant find mcl node\n");

#endif


	ret = cm_mgr_check_dts_setting(pdev);
	if (ret) {
		pr_info("[CM_MGR] FAILED TO GET DTS DATA(%d)\n", ret);
		return ret;
	}
	INIT_DELAYED_WORK(&cm_mgr_timeout_work, cm_mgr_timeout_process);
	timer_setup(&cm_mgr_perf_timeout_timer, cm_mgr_perf_timeout_timer_fn,
			0);

	local_hk.cm_mgr_get_perfs =
		cm_mgr_get_perfs_mt6895;
	local_hk.cm_mgr_perf_set_force_status =
		cm_mgr_perf_set_force_status_mt6895;
	local_hk.check_cm_mgr_status =
		check_cm_mgr_status_mt6895;
	local_hk.cm_mgr_perf_platform_set_status =
		cm_mgr_perf_platform_set_status_mt6895;
	local_hk.cm_mgr_perf_set_status =
		cm_mgr_perf_set_status_mt6895;

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
	{ .compatible = "mediatek,mt6895-cm_mgr", },
	{},
};

static const struct platform_device_id platform_cm_mgr_id_table[] = {
	{ "mt6895-cm_mgr", 0},
	{ },
};

static struct platform_driver mtk_platform_cm_mgr_driver = {
	.probe = platform_cm_mgr_probe,
	.remove	= platform_cm_mgr_remove,
	.driver = {
		.name = "mt6895-cm_mgr",
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
