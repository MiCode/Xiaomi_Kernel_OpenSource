// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "fbt_cpu_platform.h"
#include <sched/sched.h>
#include <mt-plat/fpsgo_common.h>
#include <linux/cpumask.h>
#include <linux/interconnect.h>
#include <linux/platform_device.h>
#include "dvfsrc-exp.h"
#include "fpsgo_base.h"

static int mask_int[FPSGO_PREFER_TOTAL];
static struct cpumask mask[FPSGO_PREFER_TOTAL];
static int mask_done;
static struct icc_path *bw_path;
static struct device_node *node;
static unsigned int peak_bw;
static int plat_gcc_enable;
static int plat_sbe_rescue_enable;
static int plat_cpu_limit;
static int plat_gcc_chk_avg_deq;

void fbt_notify_CM_limit(int reach_limit)
{
#if IS_ENABLED(CONFIG_MTK_CM_MGR)
	cm_mgr_perf_set_status(reach_limit);
#endif
	fpsgo_systrace_c_fbt_debug(-100, 0, reach_limit, "notify_cm");
}

static int generate_cpu_mask(void);
static int generate_sbe_rescue_enable(void);
static int platform_fpsgo_probe(struct platform_device *pdev)
{
	int ret = 0, retval = 0;

	node = pdev->dev.of_node;

	FPSGO_LOGE("%s\n", __func__);

	bw_path = of_icc_get(&pdev->dev, "fpsgo-perf-bw");
	if (IS_ERR(bw_path)) {
		dev_info(&pdev->dev, "get cm-perf_bw fail\n");
		FPSGO_LOGE("%s unable to get bw_path\n", __func__);
	}

#if IS_ENABLED(CONFIG_MTK_DVFSRC)
		peak_bw = dvfsrc_get_required_opp_peak_bw(node, 0);
#endif /* CONFIG_MTK_DVFSRC */

	ret = of_property_read_u32(node,
			 "gcc_enable", &retval);
	if (!ret)
		plat_gcc_enable = retval;
	else
		FPSGO_LOGE("%s unable to get plat_gcc_enable\n", __func__);

	ret = of_property_read_u32(node,
			 "gcc_chk_avg_deq", &retval);
	if (!ret)
		plat_gcc_chk_avg_deq = retval;
	else
		FPSGO_LOGE("%s unable to get plat_gcc_chk_avg_deq\n", __func__);

	ret = of_property_read_u32(node,
			 "cpu_limit", &retval);
	if (!ret)
		plat_cpu_limit = retval;

	generate_cpu_mask();
	generate_sbe_rescue_enable();

	return 0;
}

static int platform_fpsgo_remove(struct platform_device *pdev)
{
	icc_put(bw_path);

	return 0;
}

static const struct of_device_id platform_fpsgo_of_match[] = {
	{ .compatible = "mediatek,fpsgo", },
	{},
};

static const struct platform_device_id platform_fpsgo_id_table[] = {
	{ "fpsgo", 0},
	{ },
};

static struct platform_driver mtk_platform_fpsgo_driver = {
	.probe = platform_fpsgo_probe,
	.remove	= platform_fpsgo_remove,
	.driver = {
		.name = "fpsgo",
		.owner = THIS_MODULE,
		.of_match_table = platform_fpsgo_of_match,
	},
	.id_table = platform_fpsgo_id_table,
};

void init_fbt_platform(void)
{
	FPSGO_LOGE("%s\n", __func__);
	platform_driver_register(&mtk_platform_fpsgo_driver);
}

void exit_fbt_platform(void)
{
	platform_driver_unregister(&mtk_platform_fpsgo_driver);
}

void fbt_reg_dram_request(int reg)
{
}

void fbt_boost_dram(int boost)
{
	if (boost)
		icc_set_bw(bw_path, 0, peak_bw);
	else
		icc_set_bw(bw_path, 0, 0);

	fpsgo_systrace_c_fbt_debug(-100, 0, boost, "dram_boost");
}

void fbt_set_boost_value(unsigned int base_blc)
{
	base_blc = clamp(base_blc, 1U, 100U);
	fpsgo_sentcmd(FPSGO_SET_BOOST_TA, base_blc, -1);
	fpsgo_systrace_c_fbt_debug(-100, 0, base_blc, "TA_cap");
}

void fbt_clear_boost_value(void)
{
	fpsgo_sentcmd(FPSGO_SET_BOOST_TA, -1, -1);
	fpsgo_systrace_c_fbt_debug(-100, 0, 0, "TA_cap");

	fbt_notify_CM_limit(0);
	fbt_boost_dram(0);
}

void fbt_set_per_task_cap(int pid, unsigned int min_blc, unsigned int max_blc)
{
	int ret = -1;
	unsigned int min_blc_1024;
	unsigned int max_blc_1024;
	struct task_struct *p;
	struct sched_attr attr = {};
	unsigned long cur_min = 0, cur_max = 0;

	if (!pid)
		return;

	min_blc_1024 = (min_blc << 10) / 100U;
	min_blc_1024 = clamp(min_blc_1024, 1U, 1024U);

	max_blc_1024 = (max_blc << 10) / 100U;
	max_blc_1024 = clamp(max_blc_1024, 1U, 1024U);

	attr.sched_policy = -1;
	attr.sched_flags =
		SCHED_FLAG_KEEP_ALL |
		SCHED_FLAG_UTIL_CLAMP |
		SCHED_FLAG_RESET_ON_FORK;

	if (min_blc == 0 && max_blc == 100) {
		attr.sched_util_min = -1;
		attr.sched_util_max = -1;
	} else {
		attr.sched_util_min = (min_blc_1024 << 10) / 1280;
		attr.sched_util_max = (max_blc_1024 << 10) / 1280;
	}

	if (pid < 0)
		goto out;

	rcu_read_lock();
	p = find_task_by_vpid(pid);

	if (likely(p))
		get_task_struct(p);

	rcu_read_unlock();

	if (likely(p)) {
		cur_min = uclamp_eff_value(p, UCLAMP_MIN);
		cur_max = uclamp_eff_value(p, UCLAMP_MAX);
		if (cur_min != attr.sched_util_min || cur_max != attr.sched_util_max) {
			attr.sched_policy = p->policy;
			ret = sched_setattr_nocheck(p, &attr);
		}
		put_task_struct(p);
	}

out:
	if (ret != 0) {
		fpsgo_systrace_c_fbt(pid, 0, ret, "uclamp fail");
		fpsgo_systrace_c_fbt(pid, 0, 0, "uclamp fail");
		return;
	}

	fpsgo_systrace_c_fbt_debug(pid, 0, attr.sched_util_min, "min_cap");
	fpsgo_systrace_c_fbt_debug(pid, 0, attr.sched_util_max, "max_cap");
}

static int generate_cpu_mask(void)
{
	int i, ret, cpu;
	int temp_mask = 0;

	ret = of_property_read_u32_array(node, "fbt_cpu_mask",
			mask_int, FPSGO_PREFER_TOTAL);

	for (i = 0; i < FPSGO_PREFER_TOTAL; i++) {
		cpumask_clear(&mask[i]);
		temp_mask = mask_int[i];
		for_each_possible_cpu(cpu) {
			if (temp_mask & (1 << cpu))
				cpumask_set_cpu(cpu, &mask[i]);
		}
		FPSGO_LOGE("%s i:%d mask:%d %*pbl\n",
			__func__, i, mask_int[i], cpumask_pr_args(&mask[i]));
	}

	if (!ret)
		mask_done = 1;

	return ret;
}

static int generate_sbe_rescue_enable(void)
{
	int ret = 0, retval = 0;

	ret = of_property_read_u32(node,
		 "sbe_resceue_enable", &retval);
	if (!ret)
		plat_sbe_rescue_enable = retval;
	else
		FPSGO_LOGE("%s unable to get plat_sbe_rescue_enable\n", __func__);

	return ret;
}

void fbt_set_affinity(pid_t pid, unsigned int prefer_type)
{
	long ret = 0;

	if (!mask_done) {
		ret = -100;
		goto out;
	}

	ret = fpsgo_sched_setaffinity(pid, &mask[prefer_type]);

out:
	if (ret != 0) {
		fpsgo_systrace_c_fbt(pid, 0, ret, "setaffinity fail");
		fpsgo_systrace_c_fbt(pid, 0, 0, "setaffinity fail");
		return;
	}
	fpsgo_systrace_c_fbt(pid, 0, prefer_type, "set_affinity");
}

void fbt_set_cpu_prefer(int pid, unsigned int prefer_type)
{
}

int fbt_get_L_min_ceiling(void)
{
	return 0;
}

int fbt_get_default_boost_ta(void)
{
	return 0;
}

int fbt_get_default_adj_loading(void)
{
	return 0;
}

int fbt_get_default_adj_count(void)
{
	return 10;
}

int fbt_get_default_adj_tdiff(void)
{
	return 1000000;
}

int fbt_get_cluster_limit(int *cluster, int *freq, int *r_freq, int *cpu)
{
/*
 * Use return value to specify limit on frequency or core
 * 1. when return value is FPSGO_LIMIT_NO_LIMIT -> no limit
 * 2. when return value is FPSGO_LIMIT_FREQ -> frequency limit
 * 2.1 when cluster is set and freq is set -> ceiling limit
 * 2.2 when cluster is set and r_freq is set -> rescue ceiling limit
 * 3. when return value is FPSGO_LIMIT_CPU -> cpu limit
 * 3.1 when cpu is set valid -> cpu isolation
 */
	int limit = plat_cpu_limit;

	if (limit == FPSGO_LIMIT_CPU)
		*cpu = 7;
	else if (limit == FPSGO_LIMIT_FREQ) {
		*cluster = 2;
		*freq = 2600000;
	}

	return limit;
}

int fbt_get_default_uboost(void)
{
	return 75;
}

int fbt_get_default_qr_enable(void)
{
	return 1;
}

int fbt_get_default_gcc_enable(void)
{
	return plat_gcc_enable;
}

int fbt_get_default_sbe_rescue_enable(void)
{
	return plat_sbe_rescue_enable;
}

int fbt_get_l_min_bhropp(void)
{
	return 0;
}

int fbt_get_default_gcc_chk_avg_deq(void)
{
	return plat_gcc_chk_avg_deq;
}

