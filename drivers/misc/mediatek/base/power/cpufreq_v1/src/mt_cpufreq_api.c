/*
* Copyright (C) 2015 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*/
#include "mt_cpufreq_internal.h"

int mt_cpufreq_update_volt(enum mt_cpu_dvfs_id id, unsigned int *volt_tbl, int nr_volt_tbl)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(NULL == p);

	_mt_cpufreq_dvfs_request_wrapper(p, p->idx_opp_tbl, MT_CPU_DVFS_EEM_UPDATE,
		(void *)&volt_tbl);

	FUNC_EXIT(FUNC_LV_API);

	return 0;
}
EXPORT_SYMBOL(mt_cpufreq_update_volt);

cpuVoltsampler_func g_pCpuVoltSampler;
void notify_cpu_volt_sampler(enum mt_cpu_dvfs_id id, unsigned int volt)
{
	unsigned int mv = volt / 100;

	if (!g_pCpuVoltSampler)
		return;

	g_pCpuVoltSampler(id, mv);
}

/* for PTP-OD */
static mt_cpufreq_set_ptbl_funcPTP mt_cpufreq_update_private_tbl;

void mt_cpufreq_set_ptbl_registerCB(mt_cpufreq_set_ptbl_funcPTP pCB)
{
	mt_cpufreq_update_private_tbl = pCB;
}
EXPORT_SYMBOL(mt_cpufreq_set_ptbl_registerCB);


unsigned int mt_cpufreq_get_cur_volt(enum mt_cpu_dvfs_id id)
{
#if 0
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	struct buck_ctrl_t *vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);

	BUG_ON(NULL == p);
	BUG_ON(NULL == vproc_p);

	return vproc_p->buck_ops->get_cur_volt(vproc_p);
#else
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	struct buck_ctrl_t *vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);

	return vproc_p->cur_volt;
#endif
}
EXPORT_SYMBOL(mt_cpufreq_get_cur_volt);

unsigned int mt_cpufreq_get_cur_freq(enum mt_cpu_dvfs_id id)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	return cpu_dvfs_get_cur_freq(p);
}
EXPORT_SYMBOL(mt_cpufreq_get_cur_freq);

unsigned int mt_cpufreq_get_freq_by_idx(enum mt_cpu_dvfs_id id, int idx)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);

	FUNC_ENTER(FUNC_LV_API);

	BUG_ON(NULL == p);

	if (!cpu_dvfs_is_available(p)) {
		FUNC_EXIT(FUNC_LV_API);
		return 0;
	}

	BUG_ON(idx >= p->nr_opp_tbl);

	FUNC_EXIT(FUNC_LV_API);

	return cpu_dvfs_get_freq_by_idx(p, idx);
}
EXPORT_SYMBOL(mt_cpufreq_get_freq_by_idx);

unsigned int mt_cpufreq_get_cur_phy_freq_no_lock(enum mt_cpu_dvfs_id id)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	unsigned int freq = 0;

	FUNC_ENTER(FUNC_LV_LOCAL);

	BUG_ON(NULL == p);

	freq = cpu_dvfs_get_cur_freq(p);

	FUNC_EXIT(FUNC_LV_LOCAL);

	return freq;
}

/* for PBM */
unsigned int mt_cpufreq_get_leakage_mw(enum mt_cpu_dvfs_id id)
{
# if 0
#ifndef DISABLE_PBM_FEATURE
	struct mt_cpu_dvfs *p;
	int temp;
	int mw = 0;
	int i;

	if (id == 0) {
		for_each_cpu_dvfs_only(i, p) {
			if (cpu_dvfs_is(p, MT_CPU_DVFS_LL) && p->armpll_is_available) {
				temp = tscpu_get_temp_by_bank(THERMAL_BANK4) / 1000;
				mw += mt_spower_get_leakage(MT_SPOWER_CPULL, cpu_dvfs_get_cur_volt(p) / 100, temp);
			} else if (cpu_dvfs_is(p, MT_CPU_DVFS_L) && p->armpll_is_available) {
				temp = tscpu_get_temp_by_bank(THERMAL_BANK3) / 1000;
				mw += mt_spower_get_leakage(MT_SPOWER_CPUL, cpu_dvfs_get_cur_volt(p) / 100, temp);
			} else if (cpu_dvfs_is(p, MT_CPU_DVFS_B) && p->armpll_is_available) {
				temp = tscpu_get_temp_by_bank(THERMAL_BANK0) / 1000;
				mw += mt_spower_get_leakage(MT_SPOWER_CPUBIG, cpu_dvfs_get_cur_volt(p) / 100, temp);
			}
		}
	} else if (id > 0) {
		id = id - 1;
		p = id_to_cpu_dvfs(id);
		if (cpu_dvfs_is(p, MT_CPU_DVFS_LL)) {
			temp = tscpu_get_temp_by_bank(THERMAL_BANK4) / 1000;
			mw = mt_spower_get_leakage(MT_SPOWER_CPULL, cpu_dvfs_get_cur_volt(p) / 100, temp);
		} else if (cpu_dvfs_is(p, MT_CPU_DVFS_L)) {
			temp = tscpu_get_temp_by_bank(THERMAL_BANK3) / 1000;
			mw = mt_spower_get_leakage(MT_SPOWER_CPUL, cpu_dvfs_get_cur_volt(p) / 100, temp);
		} else if (cpu_dvfs_is(p, MT_CPU_DVFS_B)) {
			temp = tscpu_get_temp_by_bank(THERMAL_BANK0) / 1000;
			mw = mt_spower_get_leakage(MT_SPOWER_CPUBIG, cpu_dvfs_get_cur_volt(p) / 100, temp);
		}
	}
	return mw;
#else
	return 0;
#endif
#endif
	return 0;
}

int mt_cpufreq_get_ppb_state(void)
{
	return dvfs_power_mode;
}

/* cpu voltage sampler */
void mt_cpufreq_setvolt_registerCB(cpuVoltsampler_func pCB)
{
	g_pCpuVoltSampler = pCB;
}
EXPORT_SYMBOL(mt_cpufreq_setvolt_registerCB);
