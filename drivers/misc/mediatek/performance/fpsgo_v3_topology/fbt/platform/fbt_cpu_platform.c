// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include "eas_ctrl.h"
#include "fbt_cpu_platform.h"
#include <fpsgo_common.h>
#include <linux/cpumask.h>
#include <linux/soc/mediatek/mtk-pm-qos.h>
#include <linux/of.h>

static struct mtk_pm_qos_request dram_req;
static struct cpumask mask[FPSGO_PREFER_TOTAL];
static int mask_done;

static int fbt_get_dts_value(char *name)
{
	struct device_node *node = NULL;
	int value = 0;
	int ret = 0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,performance_fpsgo");
	if (node)
		ret = of_property_read_u32(node, name, &value);
	return value;
}

void fbt_notify_CM_limit(int reach_limit)
{
#ifdef CONFIG_MTK_CM_MGR
	cm_mgr_perf_set_status(reach_limit);
#endif
	fpsgo_systrace_c_fbt_gm(-100, 0, reach_limit, "notify_cm");
}

void fbt_reg_dram_request(int reg)
{
	if (reg) {
		if (!mtk_pm_qos_request_active(&dram_req))
			mtk_pm_qos_add_request(&dram_req, MTK_PM_QOS_DDR_OPP,
					MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE);
	} else {
		if (mtk_pm_qos_request_active(&dram_req))
			mtk_pm_qos_remove_request(&dram_req);
	}
}

void fbt_boost_dram(int boost)
{
	if (!mtk_pm_qos_request_active(&dram_req)) {
		fbt_reg_dram_request(1);
		if (!mtk_pm_qos_request_active(&dram_req)) {
			fpsgo_systrace_c_fbt_gm(-100, 0, -1, "dram_boost");
			return;
		}
	}

	if (boost)
		mtk_pm_qos_update_request(&dram_req, 0);
	else
		mtk_pm_qos_update_request(&dram_req,
				MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE);

	fpsgo_systrace_c_fbt_gm(-100, 0, boost, "dram_boost");
}

void fbt_set_boost_value(unsigned int base_blc)
{
	base_blc = clamp(base_blc, 1U, 100U);
#if defined(CONFIG_UCLAMP_TASK_GROUP) && defined(CONFIG_SCHED_TUNE)
	update_eas_uclamp_min(EAS_UCLAMP_KIR_FPSGO, CGROUP_TA, (int)base_blc);
#endif
	fpsgo_systrace_c_fbt_gm(-100, 0, base_blc, "TA_cap");
}

void fbt_clear_boost_value(void)
{
#if defined(CONFIG_UCLAMP_TASK_GROUP) && defined(CONFIG_SCHED_TUNE)
	update_eas_uclamp_min(EAS_UCLAMP_KIR_FPSGO, CGROUP_TA, 0);
#endif
	fpsgo_systrace_c_fbt_gm(-100, 0, 0, "TA_cap");

	fbt_notify_CM_limit(0);
	fbt_boost_dram(0);
}

void fbt_set_per_task_min_cap(int pid, unsigned int base_blc)
{
	int ret = -1;
	unsigned int base_blc_1024;

	if (!pid)
		return;

	base_blc_1024 = (base_blc << 10) / 100U;
	base_blc_1024 = clamp(base_blc_1024, 1U, 1024U);

#ifdef CONFIG_UCLAMP_TASK
	ret = set_task_util_min(pid, base_blc_1024);
#endif
	if (ret != 0) {
		fpsgo_systrace_c_fbt(pid, 0, ret, "uclamp fail");
		fpsgo_systrace_c_fbt(pid, 0, 0, "uclamp fail");
		return;
	}

	fpsgo_systrace_c_fbt_gm(pid, 0, base_blc, "min_cap");
}

static int generate_cpu_mask(unsigned int prefer_type, struct cpumask *cpu_mask)
{
	struct device_node *node;
	int cpu_mask_num = 0;
	int ret  = 0;
	int num[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	int i  = 0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,performance_misc");
	if (node) {
		ret =  of_property_read_u32(node, "fbt_cpu_mask_num", &cpu_mask_num);
		if (ret)
			return -1;

		ret = of_property_read_u32_array(node, "fbt_cpu_mask", num, cpu_mask_num);
		if (ret)
			return -1;
	}

	if (prefer_type == FPSGO_PREFER_BIG) {
		cpumask_clear(cpu_mask);
		for (i = 0; i < cpu_mask_num; i++)
			cpumask_set_cpu(num[i], cpu_mask);
	} else if (prefer_type == FPSGO_PREFER_LITTLE) {
		cpumask_setall(cpu_mask);
		for (i = 0; i < cpu_mask_num; i++)
			cpumask_clear_cpu(num[i], cpu_mask);
	} else if (prefer_type == FPSGO_PREFER_NONE)
		cpumask_setall(cpu_mask);
	else
		return -1;

	mask_done = 1;

	return 0;
}

void fbt_set_affinity(pid_t pid, unsigned int prefer_type)
{
	long ret;

	if (!mask_done) {
		generate_cpu_mask(FPSGO_PREFER_LITTLE,
					&mask[FPSGO_PREFER_LITTLE]);
		generate_cpu_mask(FPSGO_PREFER_BIG, &mask[FPSGO_PREFER_BIG]);
		generate_cpu_mask(FPSGO_PREFER_NONE, &mask[FPSGO_PREFER_NONE]);
	}

	ret = sched_setaffinity(pid, &mask[prefer_type]);
	if (ret != 0) {
		fpsgo_systrace_c_fbt(pid, 0, ret, "setaffinity fail");
		fpsgo_systrace_c_fbt(pid, 0, 0, "setaffinity fail");
		return;
	}
	fpsgo_systrace_c_fbt(pid, 0, prefer_type, "set_affinity");
}

void fbt_set_cpu_prefer(int pid, unsigned int prefer_type)
{
#if defined(CONFIG_MTK_SCHED_CPU_PREFER)
	long ret;

	if (!pid)
		return;

	ret = sched_set_cpuprefer(pid, prefer_type);
	fpsgo_systrace_c_fbt(pid, 0, prefer_type, "set_cpuprefer");
#endif
}

int fbt_get_L_cluster_num(void)
{
	return fbt_get_dts_value("l_cluster_num");
}

int fbt_get_L_min_ceiling(void)
{
	return fbt_get_dts_value("l_min_ceiling");
}

int fbt_get_default_boost_ta(void)
{
	return fbt_get_dts_value("default_boost_ta");
}

int fbt_get_default_adj_loading(void)
{
	return fbt_get_dts_value("default_adj_loading");
}

int fbt_get_cluster_limit(int *cluster, int *freq, int *r_freq)
{
	return fbt_get_dts_value("cluster_limit");
}

int fbt_get_l_min_bhropp(void)
{
	return fbt_get_dts_value("l_min_bhropp");
}

int fbt_get_default_adj_count(void)
{
	return 30;
}

int fbt_get_default_adj_tdiff(void)
{
	return 2000000;
}

int fbt_get_default_uboost(void)
{
	return fbt_get_dts_value("default_uboost");
}


int fbt_get_default_qr_enable(void)
{
	return fbt_get_dts_value("default_qr_enable");
}


int fbt_get_default_gcc_enable(void)
{
	return fbt_get_dts_value("default_gcc_enable");
}


