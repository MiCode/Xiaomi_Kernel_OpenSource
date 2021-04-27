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

#include "mtk_cpufreq_internal.h"
#include "mtk_cpufreq_hybrid.h"
#include "mtk_cpufreq_platform.h"
#include <linux/cpufreq.h>
#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>
#include <linux/slab.h>
#include <trace/events/power.h>
#include <trace/events/sched.h>


#ifdef CONFIG_MTK_CM_MGR
cpuFreqsampler_func g_pCpuFreqSampler_func_cpi;
/* cpu governor freq sampler */
void mt_cpufreq_set_governor_freq_registerCB(cpuFreqsampler_func pCB)
{
	g_pCpuFreqSampler_func_cpi = pCB;
}
EXPORT_SYMBOL(mt_cpufreq_set_governor_freq_registerCB);
#endif /* CONFIG_MTK_CM_MGR */

int mt_cpufreq_set_by_wfi_load_cluster(unsigned int cluster_id,
	unsigned int freq)
{

#ifdef CONFIG_HYBRID_CPU_DVFS
	enum mt_cpu_dvfs_id id = (enum mt_cpu_dvfs_id) cluster_id;
#if defined(CONFIG_MACH_MT6893) || defined(CONFIG_MACH_MT6877) \
	|| defined(CONFIG_MACH_MT6781)
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	struct cpufreq_policy *policy = p->mt_policy;
	int cpu;
#endif

	if (freq < mt_cpufreq_get_freq_by_idx(id, 15))
		freq = mt_cpufreq_get_freq_by_idx(id, 15);
	if (freq > mt_cpufreq_get_freq_by_idx(id, 0))
		freq = mt_cpufreq_get_freq_by_idx(id, 0);
#ifdef CONFIG_MTK_CM_MGR
	if (g_pCpuFreqSampler_func_cpi)
		g_pCpuFreqSampler_func_cpi(id, freq);
#endif /* CONFIG_MTK_CM_MGR */
#if defined(CONFIG_MACH_MT6893) || defined(CONFIG_MACH_MT6877) \
	|| defined(CONFIG_MACH_MT6781)
	for_each_cpu(cpu, policy->cpus)
		trace_cpu_frequency(freq, cpu);
#endif
	cpuhvfs_set_dvfs(id, freq);
#if defined(CONFIG_MACH_MT6893) || defined(CONFIG_MACH_MT6877) \
	|| defined(CONFIG_MACH_MT6781)
	policy->cur = freq;
	arch_set_freq_scale(policy->cpus, freq, policy->cpuinfo.max_freq);
#endif
#endif

	return 0;
}
EXPORT_SYMBOL(mt_cpufreq_set_by_wfi_load_cluster);

int mt_cpufreq_set_by_schedule_load_cluster(unsigned int cluster_id,
	unsigned int freq)
{
#ifdef CONFIG_HYBRID_CPU_DVFS
	enum mt_cpu_dvfs_id id = (enum mt_cpu_dvfs_id) cluster_id;

	if (freq < mt_cpufreq_get_freq_by_idx(id, 15))
		freq = mt_cpufreq_get_freq_by_idx(id, 15);

	if (freq > mt_cpufreq_get_freq_by_idx(id, 0))
		freq = mt_cpufreq_get_freq_by_idx(id, 0);

	cpuhvfs_set_cluster_load_freq(id, freq);
#endif


	return 0;
}
EXPORT_SYMBOL(mt_cpufreq_set_by_schedule_load_cluster);

unsigned int mt_cpufreq_find_close_freq(unsigned int cluster_id,
	unsigned int freq)
{
	enum mt_cpu_dvfs_id id = (enum mt_cpu_dvfs_id) cluster_id;
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	int idx;


	if (p == NULL)
		return 0;

	idx = _search_available_freq_idx(p, freq, CPUFREQ_RELATION_L);
	if (idx < 0)
		idx = 0;

	return mt_cpufreq_get_freq_by_idx(id, idx);
}
#ifndef VBOOT_VOLT
#define VBOOT_VOLT 80000
#endif
unsigned int mt_cpufreq_find_Vboot_idx(unsigned int cluster_id)
{
	enum mt_cpu_dvfs_id id = (enum mt_cpu_dvfs_id) cluster_id;
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	int idx = 0;

	if (p == NULL)
		return 0;

	idx = _search_available_freq_idx_under_v(p, VBOOT_VOLT);

	if (idx > p->nr_opp_tbl)
		idx = p->nr_opp_tbl;

	return idx;
}

void mt_cpufreq_ctrl_cci_volt(unsigned int volt)
{
#ifdef CONFIG_HYBRID_CPU_DVFS
		/* 10uv */
		cpuhvfs_set_set_cci_volt(volt);
#endif
}

int mt_cpufreq_set_iccs_frequency_by_cluster(int en, unsigned int cluster_id,
	unsigned int freq)
{
#ifdef CONFIG_HYBRID_CPU_DVFS
	enum mt_cpu_dvfs_id id = (enum mt_cpu_dvfs_id) cluster_id;

	if (!en)
		freq = mt_cpufreq_get_freq_by_idx(id, 15);

	if (freq < mt_cpufreq_get_freq_by_idx(id, 15))
		freq = mt_cpufreq_get_freq_by_idx(id, 15);

	if (freq > mt_cpufreq_get_freq_by_idx(id, 0))
		freq = mt_cpufreq_get_freq_by_idx(id, 0);

	cpuhvfs_set_iccs_freq(id, freq);
#endif

	return 0;
}
EXPORT_SYMBOL(mt_cpufreq_set_iccs_frequency_by_cluster);

int is_in_suspend(void)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(0);

	return p->dvfs_disable_by_suspend;
}
EXPORT_SYMBOL(is_in_suspend);

int mt_cpufreq_update_volt(enum mt_cpu_dvfs_id id, unsigned int *volt_tbl,
	int nr_volt_tbl)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	FUNC_ENTER(FUNC_LV_API);

	_mt_cpufreq_dvfs_request_wrapper(p, p->idx_opp_tbl,
		MT_CPU_DVFS_EEM_UPDATE,	(void *)&volt_tbl);

#ifdef CONFIG_HYBRID_CPU_DVFS
	cpuhvfs_update_volt((unsigned int)id, volt_tbl, nr_volt_tbl);
#endif

	FUNC_EXIT(FUNC_LV_API);

	return 0;
}
EXPORT_SYMBOL(mt_cpufreq_update_volt);

void mt_cpufreq_update_cci_map_tbl(unsigned int idx_1, unsigned int idx_2,
	unsigned char result, unsigned int mode, unsigned int use_id)
{
#if defined(CONFIG_HYBRID_CPU_DVFS) && defined(CCI_MAP_TBL_SUPPORT)
	cpuhvfs_update_cci_map_tbl(idx_1, idx_2, result, mode, use_id);
#endif
}
EXPORT_SYMBOL(mt_cpufreq_update_cci_map_tbl);

void mt_cpufreq_update_cci_mode(unsigned int mode, unsigned int use_id)
{
#if defined(CONFIG_HYBRID_CPU_DVFS) && defined(CCI_MAP_TBL_SUPPORT)
	cpuhvfs_update_cci_mode(mode, use_id);
#endif
}
EXPORT_SYMBOL(mt_cpufreq_update_cci_mode);

cpuVoltsampler_func g_pCpuVoltSampler_met;
cpuVoltsampler_func g_pCpuVoltSampler_ocp;
void notify_cpu_volt_sampler(enum mt_cpu_dvfs_id id, unsigned int volt,
	int up, int event)
{
	unsigned int mv = volt / 100;

	if (g_pCpuVoltSampler_met)
		g_pCpuVoltSampler_met(id, mv, up, event);

	if (g_pCpuVoltSampler_ocp)
		g_pCpuVoltSampler_ocp(id, mv, up, event);
}

/* cpu voltage sampler */
void mt_cpufreq_setvolt_registerCB(cpuVoltsampler_func pCB)
{
	g_pCpuVoltSampler_met = pCB;
}
EXPORT_SYMBOL(mt_cpufreq_setvolt_registerCB);

/* ocp cpu voltage sampler */
void mt_cpufreq_setvolt_ocp_registerCB(cpuVoltsampler_func pCB)
{
	g_pCpuVoltSampler_ocp = pCB;
}
EXPORT_SYMBOL(mt_cpufreq_setvolt_ocp_registerCB);

/* for PTP-OD */
static mt_cpufreq_set_ptbl_funcPTP mt_cpufreq_update_private_tbl;

void mt_cpufreq_set_ptbl_registerCB(mt_cpufreq_set_ptbl_funcPTP pCB)
{
	mt_cpufreq_update_private_tbl = pCB;
}
EXPORT_SYMBOL(mt_cpufreq_set_ptbl_registerCB);


unsigned int mt_cpufreq_get_cur_volt(enum mt_cpu_dvfs_id id)
{
#ifdef CPU_DVFS_NOT_READY
	return 0;
#else
#if 0
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	struct buck_ctrl_t *vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);

	return vproc_p->buck_ops->get_cur_volt(vproc_p);
#else
#ifdef CONFIG_HYBRID_CPU_DVFS
#ifdef ENABLE_DOE
	if (!dvfs_doe.state)
		return 0;
#endif
	return cpuhvfs_get_cur_volt(id);
#else
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	struct buck_ctrl_t *vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);

#ifdef ENABLE_DOE
	if (!dvfs_doe.state)
		return 0;
#endif
	return vproc_p->cur_volt;
#endif
#endif
#endif
}
EXPORT_SYMBOL(mt_cpufreq_get_cur_volt);

unsigned int mt_cpufreq_get_cur_freq(unsigned int id)
{
#ifdef CPU_DVFS_NOT_READY
	return 0;
#else
#ifdef CONFIG_HYBRID_CPU_DVFS
	int freq_idx;
#ifdef ENABLE_DOE
	if (!dvfs_doe.state)
		return 0;
#endif

	freq_idx = cpuhvfs_get_cur_dvfs_freq_idx((enum mt_cpu_dvfs_id)id);

	if (freq_idx < 0)
		freq_idx = 0;

	if (freq_idx > 15)
		freq_idx = 15;

	return mt_cpufreq_get_freq_by_idx((enum mt_cpu_dvfs_id)id, freq_idx);
#else
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs((enum mt_cpu_dvfs_id)id);

	if (p == NULL)
		return 0;
#ifdef ENABLE_DOE
	if (!dvfs_doe.state)
		return 0;
#endif
	return cpu_dvfs_get_cur_freq(p);
#endif
#endif
}
EXPORT_SYMBOL(mt_cpufreq_get_cur_freq);

unsigned int mt_cpufreq_get_cur_freq_idx(enum mt_cpu_dvfs_id id)
{
#ifdef CPU_DVFS_NOT_READY
	return 0;
#else
#ifdef CONFIG_HYBRID_CPU_DVFS
	int freq_idx;
#ifdef ENABLE_DOE
	if (!dvfs_doe.state)
		return 0;
#endif

	freq_idx = cpuhvfs_get_cur_dvfs_freq_idx(id);

	if (freq_idx < 0)
		freq_idx = 0;

	if (freq_idx > 15)
		freq_idx = 15;

	return freq_idx;
#endif
	/* Not Support */
	return 0;
#endif
}
EXPORT_SYMBOL(mt_cpufreq_get_cur_freq_idx);

unsigned int mt_cpufreq_get_cur_cci_freq_idx(void)
{
#ifdef CPU_DVFS_NOT_READY
	return 0;
#else
#ifdef CONFIG_HYBRID_CPU_DVFS
	if (!dvfs_init_flag)
		return 7;
	else
		return mt_cpufreq_get_cur_freq_idx(MT_CPU_DVFS_CCI);

#endif
	/* Not Support */
	return 0;
#endif
}
EXPORT_SYMBOL(mt_cpufreq_get_cur_cci_freq_idx);


unsigned int mt_cpufreq_get_freq_by_idx(enum mt_cpu_dvfs_id id, int idx)
{
#ifdef CPU_DVFS_NOT_READY
	return 0;
#else
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

#ifdef ENABLE_DOE
	if (!dvfs_doe.state)
		return 0;
#endif
	FUNC_ENTER(FUNC_LV_API);

	if (p == NULL)
		return 0;

	if (!cpu_dvfs_is_available(p)) {
		FUNC_EXIT(FUNC_LV_API);
		return 0;
	}

	FUNC_EXIT(FUNC_LV_API);

	return cpu_dvfs_get_freq_by_idx(p, idx);
#endif
}
EXPORT_SYMBOL(mt_cpufreq_get_freq_by_idx);


unsigned int mt_cpufreq_get_cpu_freq(int cpu, int idx)
{
#ifndef CONFIG_NONLINEAR_FREQ_CTL
	return 0;
#else
	int cluster_id;
	struct mt_cpu_dvfs *p;

	cluster_id = cpufreq_get_cluster_id(cpu);
	p = id_to_cpu_dvfs(cluster_id);
	idx = (p->nr_opp_tbl - 1) - idx;
	if (idx >=  p->nr_opp_tbl || idx < 0)
		return 0;
	return mt_cpufreq_get_freq_by_idx(cluster_id, idx);
#endif
}
EXPORT_SYMBOL(mt_cpufreq_get_cpu_freq);


unsigned int mt_cpufreq_get_volt_by_idx(enum mt_cpu_dvfs_id id, int idx)
{
#ifdef CPU_DVFS_NOT_READY
	return 0;
#else
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

#ifdef ENABLE_DOE
	if (!dvfs_doe.state)
		return 0;
#endif
	FUNC_ENTER(FUNC_LV_API);

	if (!cpu_dvfs_is_available(p)) {
		FUNC_EXIT(FUNC_LV_API);
		return 0;
	}

	FUNC_EXIT(FUNC_LV_API);

	return cpu_dvfs_get_volt_by_idx(p, idx);
#endif
}
EXPORT_SYMBOL(mt_cpufreq_get_volt_by_idx);

unsigned int mt_cpufreq_get_cur_phy_freq_no_lock(enum mt_cpu_dvfs_id id)
{
#ifdef CPU_DVFS_NOT_READY
	return 0;
#else
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	unsigned int freq = 0;

#ifdef ENABLE_DOE
	if (!dvfs_doe.state)
		return 0;
#endif
	FUNC_ENTER(FUNC_LV_LOCAL);

	freq = cpu_dvfs_get_cur_freq(p);

	FUNC_EXIT(FUNC_LV_LOCAL);

	return freq;
#endif
}
EXPORT_SYMBOL(mt_cpufreq_get_cur_phy_freq_no_lock);

unsigned int mt_cpufreq_get_cur_phy_freq_idx_no_lock(enum mt_cpu_dvfs_id id)
{
#ifdef CPU_DVFS_NOT_READY
	return 0;
#else
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

#ifdef ENABLE_DOE
	if (!dvfs_doe.state)
		return 0;
#endif
	return p->idx_opp_tbl;
#endif
}
EXPORT_SYMBOL(mt_cpufreq_get_cur_phy_freq_idx_no_lock);

int mt_cpufreq_get_ppb_state(void)
{
	return dvfs_power_mode;
}

int mt_cpufreq_get_sched_enable(void)
{
	return sched_dvfs_enable;
}

unsigned int mt_cpufreq_get_cpu_level(void)
{
	unsigned int lv = _mt_cpufreq_get_cpu_level();

	return lv;
}
EXPORT_SYMBOL(mt_cpufreq_get_cpu_level);
#ifdef DFD_WORKAROUND
void dfd_workaround(void)
{
	_dfd_workaround();
}
EXPORT_SYMBOL(dfd_workaround);
#endif

#ifdef READ_SRAM_VOLT
int mt_cpufreq_update_legacy_volt(enum mt_cpu_dvfs_id id,
		unsigned int *volt_tbl, int nr_volt_tbl)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	FUNC_ENTER(FUNC_LV_API);

	_mt_cpufreq_dvfs_request_wrapper(p, p->idx_opp_tbl,
		MT_CPU_DVFS_EEM_UPDATE,	(void *)&volt_tbl);

	FUNC_EXIT(FUNC_LV_API);

	return 0;
}
EXPORT_SYMBOL(mt_cpufreq_update_legacy_volt);
#endif

