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

/*
* @file    mt_cpufreq.c
* @brief   Driver for CPU DVFS
*
*/
#define __MT_CPUFREQ_C__

/* project includes */
#include "mach/mt_ppm_api.h"
#include "mach/mt_pbm.h"

/* local includes */
#include "mt_cpufreq_internal.h"
#include "mt_cpufreq_opp_table.h"
#include "mt_cpufreq_platform.h"
#include "mt_cpufreq_debug.h"

#include "mt_cpufreq_hybrid.h"

/*
 * Global Variables
 */
bool is_in_cpufreq = 0;
DEFINE_MUTEX(cpufreq_mutex);
DEFINE_MUTEX(cpufreq_para_mutex);
struct opp_idx_tbl opp_tbl_m[NR_OPP_IDX];
int disable_idvfs_flag = 0;
int dvfs_disable_flag = 0;

/* Prototype */
static int __cpuinit _mt_cpufreq_cpu_CB(struct notifier_block *nfb, unsigned long action,
	void *hcpu);

struct mt_cpu_dvfs *id_to_cpu_dvfs(enum mt_cpu_dvfs_id id)
{
	return (id < NR_MT_CPU_DVFS) ? &cpu_dvfs[id] : NULL;
}

struct buck_ctrl_t *id_to_buck_ctrl(enum mt_cpu_dvfs_buck_id id)
{
	return (id < NR_MT_BUCK) ? &buck_ctrl[id] : NULL;
}

struct pll_ctrl_t *id_to_pll_ctrl(enum mt_cpu_dvfs_pll_id id)
{
	return (id < NR_MT_PLL) ? &pll_ctrl[id] : NULL;
}

int get_turbo_freq(int chip_id, int freq)
{
	return freq;
}

int get_turbo_volt(int chip_id, int volt)
{
	return volt;
}

#ifndef DISABLE_PBM_FEATURE
void _kick_PBM_by_cpu(void)
{
	struct mt_cpu_dvfs *p;
	int i;
	struct cpumask dvfs_cpumask[NR_MT_CPU_DVFS], cpu_online_cpumask[NR_MT_CPU_DVFS];
	struct ppm_cluster_status ppm_data[NR_MT_CPU_DVFS];

	for_each_cpu_dvfs_only(i, p) {
		arch_get_cluster_cpus(&dvfs_cpumask[i], i);
		cpumask_and(&cpu_online_cpumask[i], &dvfs_cpumask[i], cpu_online_mask);

		ppm_data[i].core_num = cpumask_weight(&cpu_online_cpumask[i]);
		ppm_data[i].freq_idx = p->idx_opp_tbl;
		ppm_data[i].volt = cpu_dvfs_get_cur_volt(p) / 100;

		cpufreq_ver("%d: core = %d, idx = %d, volt = %d\n",
			i, ppm_data[i].core_num, ppm_data[i].freq_idx, ppm_data[i].volt);
	}

	aee_record_cpu_dvfs_cb(15);
	/* Fix me */
	/* mt_ppm_dlpt_kick_PBM(ppm_data, (unsigned int)arch_get_nr_clusters()); */

	aee_record_cpu_dvfs_cb(0);
}
#endif
static unsigned int mt_cpufreq_get_cur_phy_freq(enum mt_cpu_dvfs_id id)
{
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	struct pll_ctrl_t *pll_p = id_to_pll_ctrl(p->Pll_id);
	unsigned int freq = 0;
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_LOCAL);

	BUG_ON(NULL == p);

	cpufreq_lock(flags);
	freq = pll_p->pll_ops->get_cur_freq(pll_p);
	cpufreq_unlock(flags);

	FUNC_EXIT(FUNC_LV_LOCAL);

	return freq;
}

static unsigned int _calc_new_opp_idx(struct mt_cpu_dvfs *p, int new_opp_idx);
static unsigned int _calc_new_opp_idx_no_base(struct mt_cpu_dvfs *p, int new_opp_idx);

static int _search_available_freq_idx_under_v(struct mt_cpu_dvfs *p, unsigned int volt)
{
	int i;

	FUNC_ENTER(FUNC_LV_HELP);

	BUG_ON(NULL == p);

	/* search available voltage */
	for (i = 0; i < p->nr_opp_tbl; i++) {
		if (volt >= cpu_dvfs_get_volt_by_idx(p, i))
			break;
	}

	BUG_ON(i >= p->nr_opp_tbl);

	FUNC_EXIT(FUNC_LV_HELP);

	return i;
}

#ifdef CONFIG_HYBRID_CPU_DVFS
static int _cpufreq_set_locked_secure(struct cpufreq_policy *policy, struct mt_cpu_dvfs *p,
	unsigned int target_khz, int log)
{
	int ret = -1;
	unsigned int cur_khz;
	struct pll_ctrl_t *pll_p = id_to_pll_ctrl(p->Pll_id);

#ifdef CONFIG_CPU_FREQ
	struct cpufreq_freqs freqs;
	unsigned int target_khz_orig = target_khz;
#endif

	FUNC_ENTER(FUNC_LV_HELP);

#ifdef CONFIG_CPU_FREQ
	if (!policy) {
		cpufreq_err("Can't get policy of %s\n", cpu_dvfs_get_name(p));
		goto out;
	}
#endif

	cur_khz = pll_p->pll_ops->get_cur_freq(pll_p);

#ifdef CONFIG_CPU_FREQ
	freqs.old = cur_khz;
	freqs.new = target_khz_orig;
	if (policy) {
		freqs.cpu = policy->cpu;
		cpufreq_freq_transition_begin(policy, &freqs);
	}
#endif
	cpuhvfs_set_freq(arch_get_cluster_id(p->cpu_id), target_khz);

#ifdef CONFIG_CPU_FREQ
	if (policy)
		cpufreq_freq_transition_end(policy, &freqs, 0);
#endif

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;

out:
	aee_record_cpu_dvfs_step(0);

	return ret;
}
#else
static int _search_for_vco_dds(struct mt_cpu_dvfs *p, int idx,
	struct mt_cpu_freq_method *m)
{
	return (p->opp_tbl[idx].cpufreq_khz * m->pos_div * m->clk_div);
}

static int _search_available_freq_idx(struct mt_cpu_dvfs *p, unsigned int target_khz,
				      unsigned int relation)
{
	int new_opp_idx = -1;
	int i;

	FUNC_ENTER(FUNC_LV_HELP);

	if (CPUFREQ_RELATION_L == relation) {
		for (i = (signed)(p->nr_opp_tbl - 1); i >= 0; i--) {
			if (cpu_dvfs_get_freq_by_idx(p, i) >= target_khz) {
				new_opp_idx = i;
				break;
			}
		}
	} else {		/* CPUFREQ_RELATION_H */
		for (i = 0; i < (signed)p->nr_opp_tbl; i++) {
			if (cpu_dvfs_get_freq_by_idx(p, i) <= target_khz) {
				new_opp_idx = i;
				break;
			}
		}
	}

	FUNC_EXIT(FUNC_LV_HELP);

	return new_opp_idx;
}

static unsigned int _search_available_volt(struct mt_cpu_dvfs *p, unsigned int target_khz)
{
	int i;

	FUNC_ENTER(FUNC_LV_HELP);

	BUG_ON(NULL == p);

	/* search available voltage */
	for (i = p->nr_opp_tbl - 1; i >= 0; i--) {
		if (target_khz <= cpu_dvfs_get_freq_by_idx(p, i))
			break;
	}

	BUG_ON(i < 0);		/* i.e. target_khz > p->opp_tbl[0].cpufreq_khz */

	FUNC_EXIT(FUNC_LV_HELP);

	return cpu_dvfs_get_volt_by_idx(p, i);	/* mv * 100 */
}

static unsigned int _calc_new_cci_opp_idx(struct mt_cpu_dvfs *p, int new_opp_idx,
	unsigned int *target_cci_volt)
{
	int freq_idx[NR_MT_CPU_DVFS - 1] = {-1};
	unsigned int volt[NR_MT_CPU_DVFS - 1] = {0};
	int i;
	struct mt_cpu_dvfs *pp, *p_cci;

	unsigned int target_cci_freq = 0;
	int new_cci_opp_idx;

	/* This is for cci */
	p_cci = id_to_cpu_dvfs(MT_CPU_DVFS_CCI);

	/* Fill the V and F to determine cci state*/
	/* MCSI Algorithm for cci target F */
	for_each_cpu_dvfs_only(i, pp) {
		if (pp->armpll_is_available) {
			/* F */
			if (pp == p)
				freq_idx[i] = new_opp_idx;
			else
				freq_idx[i] = pp->idx_opp_tbl;

			target_cci_freq = MAX(target_cci_freq, cpu_dvfs_get_freq_by_idx(pp, freq_idx[i]));
			/* V */
			volt[i] = cpu_dvfs_get_volt_by_idx(pp, freq_idx[i]);
			/* cpufreq_ver("DVFS: MCSI: %s, freq = %d, volt = %d\n", cpu_dvfs_get_name(pp), */
			pr_crit("DVFS: MCSI: %s, freq = %d, volt = %d\n", cpu_dvfs_get_name(pp),
				cpu_dvfs_get_freq_by_idx(pp, freq_idx[i]), volt[i]);
		}
	}

	/* Most efficient frequency for CCI */
	target_cci_freq = target_cci_freq / 2;

	if (target_cci_freq > cpu_dvfs_get_freq_by_idx(p_cci, 0)) {
		target_cci_freq = cpu_dvfs_get_freq_by_idx(p_cci, 0);
		*target_cci_volt = cpu_dvfs_get_volt_by_idx(p_cci, 0);
	} else
		*target_cci_volt = _search_available_volt(p_cci, target_cci_freq);

	/* Determine dominating voltage */
	for_each_cpu_dvfs_in_buck(i, pp, p_cci)
		*target_cci_volt = MAX(*target_cci_volt, volt[i]);

	/* cpufreq_ver("DVFS: MCSI: target_cci (F,V) = (%d, %d)\n", target_cci_freq, *target_cci_volt); */
	pr_crit("DVFS: MCSI: target_cci (F,V) = (%d, %d)\n", target_cci_freq, *target_cci_volt);

	new_cci_opp_idx = _search_available_freq_idx_under_v(p_cci, *target_cci_volt);

	/* cpufreq_ver("DVFS: MCSI: Final Result, target_cci_volt = %d, target_cci_freq = %d\n", */
	pr_crit("DVFS: MCSI: Final Result, target_cci_volt = %d, target_cci_freq = %d\n",
		*target_cci_volt, cpu_dvfs_get_freq_by_idx(p_cci, new_cci_opp_idx));

	return new_cci_opp_idx;
}

void set_cur_freq_wrapper(struct mt_cpu_dvfs *p, unsigned int cur_khz, unsigned int target_khz)
{
	int idx, ori_idx;
	struct pll_ctrl_t *pll_p = id_to_pll_ctrl(p->Pll_id);
	int new_opp_idx;

	FUNC_ENTER(FUNC_LV_LOCAL);

	/* CUR_OPP_IDX */
	opp_tbl_m[CUR_OPP_IDX].p = p;
	ori_idx = _search_available_freq_idx(opp_tbl_m[CUR_OPP_IDX].p,
		cur_khz, CPUFREQ_RELATION_L);

	if (ori_idx != p->idx_opp_tbl) {
		cpufreq_err("%s: ori_freq = %d(%d) != p->idx_opp_tbl(%d), target = %d\n"
			, cpu_dvfs_get_name(p), cur_khz, ori_idx, p->idx_opp_tbl, target_khz);
		p->idx_opp_tbl = ori_idx;
	}
	opp_tbl_m[CUR_OPP_IDX].slot = &p->freq_tbl[p->idx_opp_tbl];
	cpufreq_ver("[CUR_OPP_IDX][NAME][IDX][FREQ] => %s:%d:%d\n",
		cpu_dvfs_get_name(p), p->idx_opp_tbl, cpu_dvfs_get_freq_by_idx(p, p->idx_opp_tbl));

	/* TARGET_OPP_IDX */
	opp_tbl_m[TARGET_OPP_IDX].p = p;
	idx = _search_available_freq_idx(opp_tbl_m[TARGET_OPP_IDX].p,
		target_khz, CPUFREQ_RELATION_L);
	opp_tbl_m[TARGET_OPP_IDX].slot = &p->freq_tbl[idx];
	cpufreq_ver("[TARGET_OPP_IDX][NAME][IDX][FREQ] => %s:%d:%d\n",
		cpu_dvfs_get_name(p), idx, cpu_dvfs_get_freq_by_idx(p, idx));

	if (!p->armpll_is_available) {
		cpufreq_err("%s: armpll not available, cur_khz = %d, target_khz = %d\n",
			cpu_dvfs_get_name(p), cur_khz, target_khz);
		BUG_ON(1);
	}

	if (cur_khz == target_khz)
		return;

	if (do_dvfs_stress_test)
		/* cpufreq_dbg("%s: %s: cur_khz = %d(%d), target_khz = %d(%d), clkdiv = %d->%d\n", */
		pr_crit("%s: %s: cur_khz = %d(%d), target_khz = %d(%d), clkdiv = %d->%d\n",
			__func__, cpu_dvfs_get_name(p), cur_khz, p->idx_opp_tbl, target_khz, idx,
			opp_tbl_m[CUR_OPP_IDX].slot->clk_div, opp_tbl_m[TARGET_OPP_IDX].slot->clk_div);

	aee_record_cpu_dvfs_step(4);

#ifdef DCM_ENABLE
	/* DCM (freq: high -> low) */
	if (cur_khz > target_khz) {
		if (cpu_dvfs_is(p, MT_CPU_DVFS_CCI))
			pll_p->pll_ops->set_sync_dcm(0);
		else
			pll_p->pll_ops->set_sync_dcm(target_khz/1000);
	}
#endif

	aee_record_cpu_dvfs_step(5);

	now[SET_FREQ] = ktime_get();

	/* post_div 1 -> 2 */
	if (opp_tbl_m[CUR_OPP_IDX].slot->pos_div < opp_tbl_m[TARGET_OPP_IDX].slot->pos_div)
		pll_p->pll_ops->set_armpll_posdiv(pll_p, opp_tbl_m[TARGET_OPP_IDX].slot->pos_div);

	aee_record_cpu_dvfs_step(6);

	/* armpll_div 1 -> 2 */
	if (opp_tbl_m[CUR_OPP_IDX].slot->clk_div < opp_tbl_m[TARGET_OPP_IDX].slot->clk_div)
		pll_p->pll_ops->set_armpll_clkdiv(pll_p, opp_tbl_m[TARGET_OPP_IDX].slot->clk_div);

#if 0
	pll_p->pll_ops->set_armpll_dds(pll_p, opp_tbl_m[TARGET_OPP_IDX].slot->vco_dds,
		opp_tbl_m[TARGET_OPP_IDX].slot->pos_div);
#else
	if (pll_p->hopping_id != -1) {
		/* actual FHCTL */
		aee_record_cpu_dvfs_step(7);
		pll_p->pll_ops->set_freq_hopping(pll_p,
			_cpu_dds_calc(_search_for_vco_dds(p, idx, opp_tbl_m[TARGET_OPP_IDX].slot)));
	} else {/* No hopping for B */
		aee_record_cpu_dvfs_step(8);
		pll_p->pll_ops->set_armpll_dds(pll_p, _search_for_vco_dds(p, idx, opp_tbl_m[TARGET_OPP_IDX].slot),
		opp_tbl_m[TARGET_OPP_IDX].slot->pos_div);
	}
#endif

	aee_record_cpu_dvfs_step(9);

	/* armpll_div 2 -> 1 */
	if (opp_tbl_m[CUR_OPP_IDX].slot->clk_div > opp_tbl_m[TARGET_OPP_IDX].slot->clk_div)
		pll_p->pll_ops->set_armpll_clkdiv(pll_p, opp_tbl_m[TARGET_OPP_IDX].slot->clk_div);

	aee_record_cpu_dvfs_step(10);

	/* post_div 2 -> 1 */
	if (opp_tbl_m[CUR_OPP_IDX].slot->pos_div > opp_tbl_m[TARGET_OPP_IDX].slot->pos_div)
		pll_p->pll_ops->set_armpll_posdiv(pll_p, opp_tbl_m[TARGET_OPP_IDX].slot->pos_div);

	aee_record_cpu_dvfs_step(11);

	delta[SET_FREQ] = ktime_sub(ktime_get(), now[SET_FREQ]);
	if (ktime_to_us(delta[SET_FREQ]) > ktime_to_us(max[SET_FREQ]))
		max[SET_FREQ] = delta[SET_FREQ];

#ifdef DCM_ENABLE
	/* DCM (freq: low -> high)*/
	if (cur_khz < target_khz)
		pll_p->pll_ops->set_sync_dcm(target_khz/1000);
#endif
	new_opp_idx = _search_available_freq_idx(p, target_khz, CPUFREQ_RELATION_L);
	p->idx_opp_tbl = new_opp_idx;

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static unsigned int _calc_pmic_settle_time(struct mt_cpu_dvfs *p, unsigned int old_vproc, unsigned int old_vsram,
					   unsigned int new_vproc, unsigned int new_vsram)
{
	unsigned delay = 100;
	struct buck_ctrl_t *vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);
	struct buck_ctrl_t *vsram_p = id_to_buck_ctrl(p->Vsram_buck_id);

	if (new_vproc == old_vproc && new_vsram == old_vsram)
		return 0;

	delay = MAX(vsram_p->buck_ops->settletime(old_vsram, new_vsram),
		vproc_p->buck_ops->settletime(old_vproc, new_vproc));

	if (delay < MIN_PMIC_SETTLE_TIME)
		delay = MIN_PMIC_SETTLE_TIME;

	return delay;
}

static void dump_opp_table(struct mt_cpu_dvfs *p)
{
	int i;

	cpufreq_err("[%s/%d]\n" "cpufreq_oppidx = %d\n", p->name, p->cpu_id, p->idx_opp_tbl);

	for (i = 0; i < p->nr_opp_tbl; i++) {
		cpufreq_err("\tOP(%d, %d),\n",
			    cpu_dvfs_get_freq_by_idx(p, i), cpu_dvfs_get_volt_by_idx(p, i)
		    );
	}
}

int set_cur_volt_wrapper(struct mt_cpu_dvfs *p, unsigned int volt)
{				/* volt: vproc (mv*100) */
	unsigned int cur_vsram;
	unsigned int cur_vproc;
	unsigned int delay_us = 0;
	int ret = 0;

	struct buck_ctrl_t *vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);
	struct buck_ctrl_t *vsram_p = id_to_buck_ctrl(p->Vsram_buck_id);

	BUG_ON(NULL == p);
	BUG_ON(NULL == vproc_p);
	BUG_ON(NULL == vsram_p);

	FUNC_ENTER(FUNC_LV_LOCAL);

	/* For avoiding i2c violation during suspend */
	if (p->dvfs_disable_by_suspend)
		return ret;

	cur_vproc = vproc_p->buck_ops->get_cur_volt(vproc_p);
	cur_vsram = vsram_p->buck_ops->get_cur_volt(vsram_p);

	pr_crit("DVS: Begin vproc %s = %d, vsram %s = %d\n", cpu_dvfs_get_name(vproc_p), cur_vproc,
		cpu_dvfs_get_name(vsram_p), cur_vsram);

	if (cur_vproc == 0) {
		cpufreq_err("@%s():%d, can not use ext buck!\n", __func__, __LINE__);
		return -1;
	}

	if (unlikely
	    (!((cur_vsram >= cur_vproc) && (MAX_DIFF_VSRAM_VPROC >= (cur_vsram - cur_vproc))))) {
#ifdef __KERNEL__
		aee_kernel_warning(TAG, "@%s():%d, cur_vsram = %d, cur_vproc = %d\n",
				   __func__, __LINE__, cur_vsram, cur_vproc);
#endif
		cpufreq_err("@%s():%d, cur_vsram = %d, cur_vproc = %d\n",
					__func__, __LINE__, cur_vsram, cur_vproc);
		dump_opp_table(p);
		BUG();
	}

	/* UP */
	if (volt > cur_vproc) {
		unsigned int target_vsram = volt + NORMAL_DIFF_VRSAM_VPROC;
		unsigned int next_vsram;

		do {
			unsigned int old_vproc = cur_vproc;
			unsigned int old_vsram = cur_vsram;

			next_vsram = MIN(((MAX_DIFF_VSRAM_VPROC - 2500) + cur_vproc), target_vsram);

			/* update vsram */
			cur_vsram = MAX(next_vsram, MIN_VSRAM_VOLT);

			if (cur_vsram > MAX_VSRAM_VOLT) {
				cur_vsram = MAX_VSRAM_VOLT;
				target_vsram = MAX_VSRAM_VOLT;	/* to end the loop */
			}

			if (unlikely
			    (!((cur_vsram >= cur_vproc)
			       && (MAX_DIFF_VSRAM_VPROC >= (cur_vsram - cur_vproc))))) {
				dump_opp_table(p);
				cpufreq_err("@%s():%d, old_vsram=%d, old_vproc=%d, cur_vsram = %d, cur_vproc = %d\n",
					__func__, __LINE__, old_vsram, old_vproc, cur_vsram, cur_vproc);
				BUG();
			}

			/* update vsram */
			vsram_p->buck_ops->set_cur_volt(vsram_p, cur_vsram);

			/* update vproc */
			if (next_vsram > MAX_VSRAM_VOLT)
				cur_vproc = volt;	/* Vsram was limited, set to target vproc directly */
			else
				cur_vproc = next_vsram - NORMAL_DIFF_VRSAM_VPROC;

			if (unlikely
			    (!((cur_vsram >= cur_vproc)
			       && (MAX_DIFF_VSRAM_VPROC >= (cur_vsram - cur_vproc))))) {
				dump_opp_table(p);
				cpufreq_err("@%s():%d, old_vsram=%d, old_vproc=%d, cur_vsram = %d, cur_vproc = %d\n",
					__func__, __LINE__, old_vsram, old_vproc, cur_vsram, cur_vproc);
				BUG();
			}

			/* update vproc */
			vproc_p->buck_ops->set_cur_volt(vproc_p, cur_vproc);

			delay_us =
			    _calc_pmic_settle_time(p, old_vproc, old_vsram, cur_vproc, cur_vsram);
			udelay(delay_us);

			cpufreq_ver
			    ("@%s(): UP --> old_vsram=%d, cur_vsram=%d, old_vproc=%d, cur_vproc=%d, delay=%d\n",
			     __func__, old_vsram, cur_vsram, old_vproc, cur_vproc, delay_us);
		} while (target_vsram > cur_vsram);
	}
	/* DOWN */
	else if (volt < cur_vproc) {
		unsigned int next_vproc;
		unsigned int next_vsram = cur_vproc + NORMAL_DIFF_VRSAM_VPROC;

		do {
			unsigned int old_vproc = cur_vproc;
			unsigned int old_vsram = cur_vsram;

			next_vproc = MAX((next_vsram - (MAX_DIFF_VSRAM_VPROC - 2500)), volt);

			/* update vproc */
			cur_vproc = next_vproc;

			if (unlikely
			    (!((cur_vsram >= cur_vproc)
			       && (MAX_DIFF_VSRAM_VPROC >= (cur_vsram - cur_vproc))))) {
				dump_opp_table(p);
				cpufreq_err("@%s():%d, old_vsram=%d, old_vproc=%d, cur_vsram = %d, cur_vproc = %d\n",
					__func__, __LINE__, old_vsram, old_vproc, cur_vsram, cur_vproc);
				BUG();
			}

			/* update vproc */
			vproc_p->buck_ops->set_cur_volt(vproc_p, cur_vproc);

			/* update vsram */
			next_vsram = cur_vproc + NORMAL_DIFF_VRSAM_VPROC;
			cur_vsram = MAX(next_vsram, MIN_VSRAM_VOLT);
			cur_vsram = MIN(cur_vsram, MAX_VSRAM_VOLT);

			if (unlikely
			    (!((cur_vsram >= cur_vproc)
			       && (MAX_DIFF_VSRAM_VPROC >= (cur_vsram - cur_vproc))))) {
				dump_opp_table(p);
				cpufreq_err("@%s():%d, old_vsram=%d, old_vproc=%d, cur_vsram = %d, cur_vproc = %d\n",
					__func__, __LINE__, old_vsram, old_vproc, cur_vsram, cur_vproc);
				BUG();
			}

			/* update vsram */
			vsram_p->buck_ops->set_cur_volt(vsram_p, cur_vsram);

			delay_us =
			    _calc_pmic_settle_time(p, old_vproc, old_vsram, cur_vproc, cur_vsram);
			udelay(delay_us);

			cpufreq_ver
			    ("@%s(): DOWN --> old_vsram=%d, cur_vsram=%d, old_vproc=%d, cur_vproc=%d, delay=%d\n",
			     __func__, old_vsram, cur_vsram, old_vproc, cur_vproc, delay_us);
		} while (cur_vproc > volt);
	}

	vsram_p->cur_volt = cur_vsram;
	vproc_p->cur_volt = cur_vproc;

	aee_record_cpu_volt(p, volt);

	notify_cpu_volt_sampler(p->cpu_id, volt);

	cpufreq_ver("DVFS: End @%s(): %s, cur_vsram = %d, cur_vproc = %d\n", __func__, cpu_dvfs_get_name(p),
		cur_vsram, cur_vproc);

	/* cpufreq_ver("DVFS: End2 @%s(): %s, cur_vsram = %d, cur_vproc = %d\n", __func__, cpu_dvfs_get_name(p), */
	pr_crit("DVS: End @%s(): %s, vsram(%s) = %d, cur_vproc(%s) = %d\n", __func__, cpu_dvfs_get_name(p),
		cpu_dvfs_get_name(vsram_p), vsram_p->buck_ops->get_cur_volt(vsram_p),
		cpu_dvfs_get_name(vproc_p), vproc_p->buck_ops->get_cur_volt(vproc_p));

	FUNC_EXIT(FUNC_LV_LOCAL);

	return ret;
}

static int _cpufreq_set_locked_cci(unsigned int target_cci_khz, unsigned int target_cci_volt)
{
	int ret = -1;
	int new_opp_idx;
	struct mt_cpu_dvfs *p_cci;
	unsigned int cur_cci_khz;
	unsigned int cur_cci_volt;

	struct buck_ctrl_t *vproc_p;
	struct buck_ctrl_t *vsram_p;
	struct pll_ctrl_t *pll_p;

	FUNC_ENTER(FUNC_LV_HELP);

#ifdef CONFIG_CPU_DVFS_AEE_RR_REC
	aee_rr_rec_cpu_dvfs_status(aee_rr_curr_cpu_dvfs_status() |
		(1 << CPU_DVFS_CCI_IS_DOING_DVFS));
#endif

	aee_record_cci_dvfs_step(1);

	p_cci = id_to_cpu_dvfs(MT_CPU_DVFS_CCI);

	vproc_p = id_to_buck_ctrl(p_cci->Vproc_buck_id);
	vsram_p = id_to_buck_ctrl(p_cci->Vsram_buck_id);
	pll_p = id_to_pll_ctrl(p_cci->Pll_id);

	cur_cci_khz = pll_p->pll_ops->get_cur_freq(pll_p);
	cur_cci_volt = vproc_p->buck_ops->get_cur_volt(vproc_p);

	if (cur_cci_khz != target_cci_khz)
		cpufreq_ver
		    ("@%s(), %s: cci_freq = (%d, %d), cci_volt = (%d, %d)\n",
		     __func__, cpu_dvfs_get_name(p_cci), cur_cci_khz, target_cci_khz, cur_cci_volt, target_cci_volt);

	aee_record_cci_dvfs_step(2);

	/* Set cci voltage (UP) */
	if (target_cci_volt > cur_cci_volt) {
		ret = set_cur_volt_wrapper(p_cci, target_cci_volt);
		if (ret)	/* set volt fail */
			goto out;
	}

	aee_record_cci_dvfs_step(3);

	/* set cci freq (UP/DOWN) */
	if (cur_cci_khz != target_cci_khz)
		set_cur_freq_wrapper(p_cci, cur_cci_khz, target_cci_khz);

	new_opp_idx = _search_available_freq_idx(p_cci, target_cci_khz, CPUFREQ_RELATION_L);

	p_cci->idx_opp_tbl = new_opp_idx;

	aee_record_cci_dvfs_step(4);

	/* Set cci voltage (DOWN) */
	if (target_cci_volt != 0 && (target_cci_volt < cur_cci_volt)) {
		ret = set_cur_volt_wrapper(p_cci, target_cci_volt);
		if (ret)	/* set volt fail */
			goto out;
	}

	aee_record_cci_dvfs_step(5);

	/*cpufreq_ver("@%s(): Vproc = %dmv, Vsram = %dmv, freq = %dKHz\n",
		    __func__,
		    (p_cci->ops->get_cur_volt(p_cci)) / 100,
		    (p_cci->ops->get_cur_vsram(p_cci) / 100), p_cci->ops->get_cur_phy_freq(p_cci));*/
out:
	aee_record_cci_dvfs_step(0);

#ifdef CONFIG_CPU_DVFS_AEE_RR_REC
	aee_rr_rec_cpu_dvfs_status(aee_rr_curr_cpu_dvfs_status() &
		~(1 << CPU_DVFS_CCI_IS_DOING_DVFS));
#endif
	return ret;
}

static void dump_all_opp_table(void)
{
	int i;
	struct mt_cpu_dvfs *p;

	for_each_cpu_dvfs(i, p) {
		cpufreq_err("[%s/%d] available = %d, oppidx = %d (%u, %u)\n",
			    p->name, p->cpu_id, p->armpll_is_available, p->idx_opp_tbl,
			    cpu_dvfs_get_freq_by_idx(p, p->idx_opp_tbl), cpu_dvfs_get_volt_by_idx(p, p->idx_opp_tbl));

		if (i == MT_CPU_DVFS_CCI) {
			for (i = 0; i < p->nr_opp_tbl; i++) {
				cpufreq_err("%-2d (%u, %u)\n",
					i, cpu_dvfs_get_freq_by_idx(p, i), cpu_dvfs_get_volt_by_idx(p, i));
			}
		}
	}
}

static int _cpufreq_set_locked(struct cpufreq_policy *policy, struct mt_cpu_dvfs *p,
	unsigned int target_khz, int log)
{
	int ret = -1;
	unsigned int target_volt;
	unsigned int cur_volt;
	/* CCI */
	struct mt_cpu_dvfs *p_cci;
	int new_cci_opp_idx;
	int new_opp_idx;
	unsigned int cur_cci_khz, target_cci_khz, target_cci_volt, cur_khz;
	unsigned long flags;

	struct buck_ctrl_t *vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);
	struct buck_ctrl_t *vsram_p = id_to_buck_ctrl(p->Vsram_buck_id);
	struct pll_ctrl_t *pll_p = id_to_pll_ctrl(p->Pll_id);
	struct pll_ctrl_t *pcci_p;

#ifdef CONFIG_CPU_FREQ
	struct cpufreq_freqs freqs;
	unsigned int target_khz_orig = target_khz;
#endif

	FUNC_ENTER(FUNC_LV_HELP);

	if (dvfs_disable_flag == 1)
		return 0;

#ifdef CONFIG_CPU_FREQ
	if (!policy) {
		cpufreq_err("Can't get policy of %s\n", cpu_dvfs_get_name(p));
		goto out;
	}
#endif

	/* MCSI Output */
	p_cci = id_to_cpu_dvfs(MT_CPU_DVFS_CCI);
	pcci_p = id_to_pll_ctrl(p_cci->Pll_id);
	new_opp_idx = _search_available_freq_idx(p, target_khz, CPUFREQ_RELATION_L);
	new_cci_opp_idx = _calc_new_cci_opp_idx(p, new_opp_idx, &target_cci_volt);
	cur_cci_khz = pcci_p->pll_ops->get_cur_freq(pcci_p);
	target_cci_khz = cpu_dvfs_get_freq_by_idx(p_cci, new_cci_opp_idx);

	cur_khz = pll_p->pll_ops->get_cur_freq(pll_p);

	/* Same buck with MCSI */
	if (p_cci->Vproc_buck_id == p->Vproc_buck_id) {
		target_volt = target_cci_volt;
		target_cci_volt = 0;
	} else
		target_volt = _search_available_volt(p, target_khz);

	cpufreq_para_lock(flags);
	if (cur_khz != get_turbo_freq(p->cpu_id, target_khz)) {
		/*if (log || do_dvfs_stress_test)
			cpufreq_dbg
				("@%s(), %s:(%d,%d): freq=%d(%d), volt =%d(%d), on=%d, cur=%d, cci(%d,%d)\n",
				 __func__, cpu_dvfs_get_name(p), p->idx_opp_ppm_base, p->idx_opp_ppm_limit,
				 target_khz, get_turbo_freq(p->cpu_id, target_khz), target_volt,
				 get_turbo_volt(p->cpu_id, target_volt), num_online_cpus(), cur_khz,
				 cur_cci_khz, target_cci_khz);
		else*/
			pr_crit
				("@%s(), %s:(%d,%d): freq=%d(%d), volt =%d(%d), on=%d, cur=%d, cci(%d,%d)\n",
				 __func__, cpu_dvfs_get_name(p), p->idx_opp_ppm_base, p->idx_opp_ppm_limit,
				 target_khz, get_turbo_freq(p->cpu_id, target_khz), target_volt,
				 get_turbo_volt(p->cpu_id, target_volt), num_online_cpus(), cur_khz,
				 cur_cci_khz, target_cci_khz);
	}
	cpufreq_para_unlock(flags);

	target_volt = get_turbo_volt(p->cpu_id, target_volt);
	target_khz = get_turbo_freq(p->cpu_id, target_khz);

	aee_record_cpu_dvfs_step(1);

	/* set volt (UP) */
	cur_volt = vproc_p->buck_ops->get_cur_volt(vproc_p);
	if (target_volt > cur_volt) {
		ret = set_cur_volt_wrapper(p, target_volt);
		if (ret)	/* set volt fail */
			goto out;
	}

	aee_record_cpu_dvfs_step(2);

#ifdef CONFIG_CPU_FREQ
	freqs.old = cur_khz;
	freqs.new = target_khz_orig;
	if (policy) {
		freqs.cpu = policy->cpu;
		cpufreq_freq_transition_begin(policy, &freqs);
	}
#endif

	aee_record_cpu_dvfs_step(3);

	/* set freq (UP/DOWN) */
	if (cur_khz != target_khz)
		set_cur_freq_wrapper(p, cur_khz, target_khz);

	aee_record_cpu_dvfs_step(12);

#ifdef CONFIG_CPU_FREQ
	if (policy)
		cpufreq_freq_transition_end(policy, &freqs, 0);
#endif

	aee_record_cpu_dvfs_step(13);

	/* set cci freq/volt */
	_cpufreq_set_locked_cci(target_cci_khz, target_cci_volt);

	aee_record_cpu_dvfs_step(14);

	/* set volt (DOWN) */
	cur_volt = vproc_p->buck_ops->get_cur_volt(vproc_p);
	if (cur_volt > target_volt) {
		ret = set_cur_volt_wrapper(p, target_volt);

		if (ret)	/* set volt fail */
			goto out;
	}

	aee_record_cpu_dvfs_step(15);

	cpufreq_ver("DVFS: @%s(): Vproc = %dmv, Vsram = %dmv, freq(%s) = %dKHz\n",
	    __func__,
	    (vproc_p->buck_ops->get_cur_volt(vproc_p)) / 100,
	    (vsram_p->buck_ops->get_cur_volt(vsram_p) / 100), p->name,
		pll_p->pll_ops->get_cur_freq(pll_p));

	/* trigger exception if freq/volt not correct during stress */
	if (do_dvfs_stress_test && !p->dvfs_disable_by_suspend) {
		unsigned int volt = vproc_p->buck_ops->get_cur_volt(vproc_p);
		unsigned int freq = pll_p->pll_ops->get_cur_freq(pll_p);

		if (volt < target_volt || freq != target_khz) {
			/* cpufreq_err("volt = %u, target_volt = %u, freq = %u, target_khz = %u\n", */
			pr_crit("volt = %u, target_volt = %u, freq = %u, target_khz = %u\n",
				volt, target_volt, freq, target_khz);
			dump_all_opp_table();
			BUG();
		}
	}

	FUNC_EXIT(FUNC_LV_HELP);

	aee_record_cpu_dvfs_step(0);

	return 0;

out:
	aee_record_cpu_dvfs_step(0);

	return ret;
}
#endif

static void _mt_cpufreq_set(struct cpufreq_policy *policy, struct mt_cpu_dvfs *p, int new_opp_idx,
	enum mt_cpu_dvfs_action_id action)
{
	unsigned int target_freq;
	int ret = -1;
	int log = 0;

	FUNC_ENTER(FUNC_LV_LOCAL);

	BUG_ON(NULL == p);
	BUG_ON(new_opp_idx >= p->nr_opp_tbl);

	if (p->dvfs_disable_by_suspend || p->armpll_is_available != 1)
		return;

	if (do_dvfs_stress_test && action == MT_CPU_DVFS_NORMAL)
		new_opp_idx = jiffies & 0xF;
	else if (action == MT_CPU_DVFS_ONLINE || action == MT_CPU_DVFS_DP)
		new_opp_idx = _calc_new_opp_idx_no_base(p, new_opp_idx);
	else
		new_opp_idx = _calc_new_opp_idx(p, new_opp_idx);

	target_freq = cpu_dvfs_get_freq_by_idx(p, new_opp_idx);

	now[SET_DVFS] = ktime_get();

	aee_record_cpu_dvfs_in(p);

#ifdef CONFIG_CPU_FREQ
#ifdef CONFIG_HYBRID_CPU_DVFS
	ret = _cpufreq_set_locked_secure(policy, p, target_freq, log);
#else
	ret = _cpufreq_set_locked(policy, p, target_freq, log);
#endif
#else
#ifdef CONFIG_HYBRID_CPU_DVFS
	ret = _cpufreq_set_locked_secure(NULL, p, target_freq, log);
#else
	ret = _cpufreq_set_locked(NULL, p, target_freq, log);
#endif
#endif

	aee_record_cpu_dvfs_out(p);

	delta[SET_DVFS] = ktime_sub(ktime_get(), now[SET_DVFS]);
	if (ktime_to_us(delta[SET_DVFS]) > ktime_to_us(max[SET_DVFS]))
		max[SET_DVFS] = delta[SET_DVFS];

	p->idx_opp_tbl = new_opp_idx;

	aee_record_freq_idx(p, p->idx_opp_tbl);

#ifndef DISABLE_PBM_FEATURE
	if (!ret && !p->dvfs_disable_by_suspend)
		_kick_PBM_by_cpu();
#endif

	FUNC_EXIT(FUNC_LV_LOCAL);
}

/* one action could be combinational set */
void _mt_cpufreq_dvfs_request_wrapper(struct mt_cpu_dvfs *p, int new_opp_idx,
	enum mt_cpu_dvfs_action_id action, void *data)
{
	unsigned long flags, para_flags;
	struct mt_cpu_dvfs *pp;
	int i, ignore_ppm = 0;
	/* PTP related */
	unsigned int **volt_tbl;
	struct buck_ctrl_t *vproc_p;

	cpufreq_lock(flags);
	/* action switch */
	switch (action) {
	case MT_CPU_DVFS_NORMAL:
		pr_crit("DVFS - %s, MT_CPU_DVFS_NORMAL to %d\n", cpu_dvfs_get_name(p), new_opp_idx);
		_mt_cpufreq_set(p->mt_policy, p, new_opp_idx, action);
		break;
	case MT_CPU_DVFS_PPM:
		pr_crit("DVFS - MT_CPU_DVFS_PPM\n");
		for_each_cpu_dvfs_only(i, pp) {
			if (pp->armpll_is_available) {
				cpufreq_para_lock(para_flags);
				if (pp->idx_opp_ppm_limit == -1)
					pp->mt_policy->max = cpu_dvfs_get_max_freq(pp);
				else
					pp->mt_policy->max = cpu_dvfs_get_freq_by_idx(pp, pp->idx_opp_ppm_limit);
				if (pp->idx_opp_ppm_base == -1)
					pp->mt_policy->min = cpu_dvfs_get_min_freq(pp);
				else
					pp->mt_policy->min = cpu_dvfs_get_freq_by_idx(pp, pp->idx_opp_ppm_base);

				ignore_ppm = 0;
				if ((pp->idx_opp_tbl >= pp->mt_policy->max)
					&& (pp->idx_opp_tbl <= pp->mt_policy->min)) {
					cpufreq_ver("idx = %d, idx_opp_ppm_base = %d, idx_opp_ppm_limit = %d\n",
						pp->idx_opp_tbl, pp->mt_policy->min, pp->mt_policy->max);
					ignore_ppm = 1;
				}

				cpufreq_para_unlock(para_flags);
				/* new_opp_idx == current idx */
				if (!ignore_ppm)
					_mt_cpufreq_set(pp->mt_policy, pp, pp->idx_opp_tbl, action);
			}
		}
		break;
	case MT_CPU_DVFS_PBM:
		/* no DVFS indeed */
		_mt_cpufreq_set(p->mt_policy, p, new_opp_idx, action);
		break;
	case MT_CPU_DVFS_EEM_UPDATE:
		volt_tbl = (unsigned int **)data;
		vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);

		/* Update public table */
		for (i = 0; i < p->nr_opp_tbl; i++)
			p->opp_tbl[i].cpufreq_volt = vproc_p->buck_ops->transfer2volt((*volt_tbl)[i]);

		for_each_cpu_dvfs_only(i, pp) {
			if ((pp->Vproc_buck_id == p->Vproc_buck_id) && pp->armpll_is_available) {
				_mt_cpufreq_set(pp->mt_policy, pp, pp->idx_opp_tbl, MT_CPU_DVFS_EEM_UPDATE);
				break;
			}
		}
		break;
	default:
		break;
	};
	cpufreq_unlock(flags);
}

static void _mt_cpufreq_dvfs_hps_request_wrapper(struct mt_cpu_dvfs *p, int new_opp_idx,
	unsigned long action, void *data)
{
	enum mt_cpu_dvfs_id *id = (enum mt_cpu_dvfs_id *)data;
	struct mt_cpu_dvfs *act_p;

	act_p = id_to_cpu_dvfs(*id);
	/* action switch */
	switch (action) {
	case CPU_ONLINE:
		if (act_p->armpll_is_available == 0 && act_p == p) {
			act_p->armpll_is_available = 1;
#ifdef CONFIG_HYBRID_CPU_DVFS
			cpuhvfs_set_cluster_on_off(arch_get_cluster_id(p->cpu_id), 1);
#endif
		}
		pr_crit("DVFS - %s, CPU_ONLINE to %d\n", cpu_dvfs_get_name(p), new_opp_idx);
		_mt_cpufreq_set(p->mt_policy, p, new_opp_idx, MT_CPU_DVFS_ONLINE);
		break;
	case CPU_DOWN_PREPARE:
		pr_crit("DVFS - %s, CPU_DOWN_PREPARE to %d\n", cpu_dvfs_get_name(p), new_opp_idx);
		if (cpu_dvfs_is(act_p, MT_CPU_DVFS_B))
			pr_crit("DVFS - %s armpll_is_available = %d\n", cpu_dvfs_get_name(act_p),
				act_p->armpll_is_available);
		_mt_cpufreq_set(p->mt_policy, p, new_opp_idx, MT_CPU_DVFS_DP);
		if (act_p->armpll_is_available == 1 && act_p == p) {
			act_p->armpll_is_available = 0;
#ifdef CONFIG_HYBRID_CPU_DVFS
			cpuhvfs_set_cluster_on_off(arch_get_cluster_id(p->cpu_id), 0);
#endif
			act_p->mt_policy = NULL;
		}
		break;
	default:
		break;
	};
}

static void _mt_cpufreq_cpu_CB_wrapper(enum mt_cpu_dvfs_id cluster_id, unsigned int cpus, unsigned long action)
{
	int i, j;
	struct mt_cpu_dvfs *p;
	unsigned long flags;
	unsigned int cur_volt;
	struct buck_ctrl_t *vproc_p;
	int new_opp_idx;

	for (i = 0; i < sizeof(cpu_dvfs_hp_action)/sizeof(cpu_dvfs_hp_action[0]); i++) {
		if (cpu_dvfs_hp_action[i].cluster == cluster_id &&
			action == cpu_dvfs_hp_action[i].action &&
			cpus == cpu_dvfs_hp_action[i].trigged_core) {
			cpufreq_lock(flags);
			for_each_cpu_dvfs(j, p) {
				if (cpu_dvfs_hp_action[i].hp_action_cfg[j].action_id != FREQ_NONE) {
					if (cpu_dvfs_hp_action[i].hp_action_cfg[j].action_id == FREQ_HIGH)
						_mt_cpufreq_dvfs_hps_request_wrapper(p, 0, action, (void *)&cluster_id);
					else if (cpu_dvfs_hp_action[i].hp_action_cfg[j].action_id == FREQ_LOW)
						_mt_cpufreq_dvfs_hps_request_wrapper(p, 15, action,
							(void *)&cluster_id);
					else if (cpu_dvfs_hp_action[i].hp_action_cfg[j].action_id == FREQ_DEPEND_VOLT) {
						vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);
						cur_volt = vproc_p->buck_ops->get_cur_volt(vproc_p);
						new_opp_idx = _search_available_freq_idx_under_v(p, cur_volt);
						pr_crit("DVFS - %s, search volt = %d, idx = %d\n",
							cpu_dvfs_get_name(p), cur_volt, new_opp_idx);
						_mt_cpufreq_dvfs_hps_request_wrapper(p, new_opp_idx, action,
							(void *)&cluster_id);
					} else if (cpu_dvfs_hp_action[i].hp_action_cfg[j].action_id == FREQ_USR_REQ)
						_mt_cpufreq_dvfs_hps_request_wrapper(p,
							cpu_dvfs_hp_action[i].hp_action_cfg[j].freq_idx, action,
							(void *)&cluster_id);
				}
			}
			cpufreq_unlock(flags);
		} else if (action == CPU_ONLINE || action == CPU_ONLINE_FROZEN ||
			action == CPU_DEAD || action == CPU_DEAD_FROZEN) {
				_mt_cpufreq_dvfs_request_wrapper(p, p->idx_opp_tbl, MT_CPU_DVFS_PBM, NULL);
		}
	}
}

static int __cpuinit _mt_cpufreq_cpu_CB(struct notifier_block *nfb, unsigned long action,
					void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;
	unsigned int online_cpus = num_online_cpus();
	struct device *dev;

	enum mt_cpu_dvfs_id cluster_id;

	/* CPU mask - Get on-line cpus per-cluster */
	struct cpumask dvfs_cpumask;
	struct cpumask cpu_online_cpumask;
	unsigned int cpus;
	/* Fix me */
	return NOTIFY_OK;

	if (dvfs_disable_flag == 1)
		return NOTIFY_OK;

	cluster_id = arch_get_cluster_id(cpu);
	arch_get_cluster_cpus(&dvfs_cpumask, cluster_id);
	cpumask_and(&cpu_online_cpumask, &dvfs_cpumask, cpu_online_mask);
	cpus = cpumask_weight(&cpu_online_cpumask);

	cpufreq_ver("@%s():%d, cpu = %d, action = %lu, num_online_cpus = %d\n"
	, __func__, __LINE__, cpu, action, online_cpus);

	dev = get_cpu_device(cpu);

	if (dev) {
		switch (action & ~CPU_TASKS_FROZEN) {
		case CPU_ONLINE:
		case CPU_DOWN_PREPARE:
		case CPU_DEAD:	/* PBM */
		case CPU_DOWN_FAILED:
			_mt_cpufreq_cpu_CB_wrapper(cluster_id, cpus, action);
			break;
		default:
			break;
		}
	}

	cpufreq_ver("@%s():%d, cpu = %d, action = %lu, num_online_cpus = %d\n"
	, __func__, __LINE__, cpu, action, online_cpus);

	return NOTIFY_OK;
}

static struct notifier_block __refdata _mt_cpufreq_cpu_notifier = {
	.notifier_call = _mt_cpufreq_cpu_CB,
};

static int _sync_opp_tbl_idx(struct mt_cpu_dvfs *p)
{
	int ret = -1;
	unsigned int freq;
	int i;
	struct pll_ctrl_t *pll_p = id_to_pll_ctrl(p->Pll_id);

	FUNC_ENTER(FUNC_LV_HELP);

	BUG_ON(NULL == p);
	BUG_ON(NULL == p->opp_tbl);

	freq = pll_p->pll_ops->get_cur_freq(pll_p);

	/* dbg_print("DVFS: _sync_opp_tbl_idx(from reg): %s freq = %d\n", cpu_dvfs_get_name(p), freq); */

	for (i = p->nr_opp_tbl - 1; i >= 0; i--) {
		if (freq <= cpu_dvfs_get_freq_by_idx(p, i)) {
			p->idx_opp_tbl = i;

			aee_record_freq_idx(p, p->idx_opp_tbl);

			break;
		}
	}

	if (i >= 0) {
		/* cpufreq_dbg("%s freq = %d\n", cpu_dvfs_get_name(p), cpu_dvfs_get_cur_freq(p)); */
		/* dbg_print("DVFS: _sync_opp_tbl_idx: %s freq = %d\n",
			cpu_dvfs_get_name(p), cpu_dvfs_get_cur_freq(p)); */
		ret = 0;
	} else
		cpufreq_warn("%s can't find freq = %d\n", cpu_dvfs_get_name(p), freq);

	FUNC_EXIT(FUNC_LV_HELP);

	return ret;
}

static int _mt_cpufreq_sync_opp_tbl_idx(struct mt_cpu_dvfs *p)
{
	int ret = -1;

	FUNC_ENTER(FUNC_LV_LOCAL);

	if (cpu_dvfs_is_available(p))
		ret = _sync_opp_tbl_idx(p);

	cpufreq_info("%s freq = %d\n", cpu_dvfs_get_name(p), cpu_dvfs_get_cur_freq(p));

	FUNC_EXIT(FUNC_LV_LOCAL);

	return ret;
}

static enum mt_cpu_dvfs_id _get_cpu_dvfs_id(unsigned int cpu_id)
{
	int cluster_id;

	cluster_id = arch_get_cluster_id(cpu_id);

	return cluster_id;
}

static int _mt_cpufreq_setup_freqs_table(struct cpufreq_policy *policy,
					 struct mt_cpu_freq_info *freqs, int num)
{
	struct mt_cpu_dvfs *p;
	int ret = 0;

	FUNC_ENTER(FUNC_LV_LOCAL);

	BUG_ON(NULL == policy);
	BUG_ON(NULL == freqs);

	p = id_to_cpu_dvfs(_get_cpu_dvfs_id(policy->cpu));

#ifdef CONFIG_CPU_FREQ
	ret = cpufreq_frequency_table_cpuinfo(policy, p->freq_tbl_for_cpufreq);

	if (!ret)
		policy->freq_table = p->freq_tbl_for_cpufreq;

	cpumask_copy(policy->cpus, topology_core_cpumask(policy->cpu));
	cpumask_copy(policy->related_cpus, policy->cpus);
#endif

	FUNC_EXIT(FUNC_LV_LOCAL);

	return 0;
}

static unsigned int _calc_new_opp_idx_no_base(struct mt_cpu_dvfs *p, int new_opp_idx)
{
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_HELP);

	BUG_ON(NULL == p);

	cpufreq_para_lock(flags);

	cpufreq_ver("new_opp_idx = %d, idx_opp_ppm_base = %d, idx_opp_ppm_limit = %d\n",
		new_opp_idx , p->idx_opp_ppm_base, p->idx_opp_ppm_limit);

	if ((p->idx_opp_ppm_limit != -1) && (new_opp_idx < p->idx_opp_ppm_limit))
		new_opp_idx = p->idx_opp_ppm_limit;

	cpufreq_para_unlock(flags);

	FUNC_EXIT(FUNC_LV_HELP);

	return new_opp_idx;
}

static unsigned int _calc_new_opp_idx(struct mt_cpu_dvfs *p, int new_opp_idx)
{
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_HELP);

	BUG_ON(NULL == p);

	cpufreq_para_lock(flags);
	cpufreq_ver("new_opp_idx = %d, idx_opp_ppm_base = %d, idx_opp_ppm_limit = %d\n",
		new_opp_idx , p->idx_opp_ppm_base, p->idx_opp_ppm_limit);

	if ((p->idx_opp_ppm_limit != -1) && (new_opp_idx < p->idx_opp_ppm_limit))
		new_opp_idx = p->idx_opp_ppm_limit;

	if ((p->idx_opp_ppm_base != -1) && (new_opp_idx > p->idx_opp_ppm_base))
		new_opp_idx = p->idx_opp_ppm_base;

	if ((p->idx_opp_ppm_base == p->idx_opp_ppm_limit) && p->idx_opp_ppm_base != -1)
		new_opp_idx = p->idx_opp_ppm_base;

	cpufreq_para_unlock(flags);

	FUNC_EXIT(FUNC_LV_HELP);

	return new_opp_idx;
}

/* Fix me, PPM */
#if 0
static void ppm_limit_callback(struct ppm_client_req req)
{
	struct ppm_client_req *ppm = (struct ppm_client_req *)&req;
	unsigned long flags;
	struct mt_cpu_dvfs *p;
	unsigned int i;

	cpufreq_ver("get feedback from PPM module\n");

	cpufreq_para_lock(flags);
	for (i = 0; i < ppm->cluster_num; i++) {
		cpufreq_ver("[%d]:cluster_id = %d, cpu_id = %d, min_cpufreq_idx = %d, max_cpufreq_idx = %d\n",
			i, ppm->cpu_limit[i].cluster_id, ppm->cpu_limit[i].cpu_id,
			ppm->cpu_limit[i].min_cpufreq_idx, ppm->cpu_limit[i].max_cpufreq_idx);
		cpufreq_ver("has_advise_freq = %d, advise_cpufreq_idx = %d\n",
			ppm->cpu_limit[i].has_advise_freq, ppm->cpu_limit[i].advise_cpufreq_idx);

		p = id_to_cpu_dvfs(i);

		if (ppm->cpu_limit[i].has_advise_freq) {
			p->idx_opp_ppm_base = ppm->cpu_limit[i].advise_cpufreq_idx;
			p->idx_opp_ppm_limit = ppm->cpu_limit[i].advise_cpufreq_idx;
		} else {
			p->idx_opp_ppm_base = ppm->cpu_limit[i].min_cpufreq_idx;	/* ppm update base */
			p->idx_opp_ppm_limit = ppm->cpu_limit[i].max_cpufreq_idx;	/* ppm update limit */
		}

#ifdef CONFIG_HYBRID_CPU_DVFS
		cpuhvfs_set_mix_max(arch_get_cluster_id(p->cpu_id), p->idx_opp_ppm_base, p->idx_opp_ppm_limit);
#endif
	}
	cpufreq_para_unlock(flags);

	/* Don't care the parameters */
	_mt_cpufreq_dvfs_request_wrapper(NULL, 0, MT_CPU_DVFS_PPM, NULL);
}
#endif
/*
 * cpufreq driver
 */
static int _mt_cpufreq_verify(struct cpufreq_policy *policy)
{
	struct mt_cpu_dvfs *p;
	int ret = 0;		/* cpufreq_frequency_table_verify() always return 0 */

	FUNC_ENTER(FUNC_LV_MODULE);

	p = id_to_cpu_dvfs(_get_cpu_dvfs_id(policy->cpu));

	BUG_ON(NULL == p);

#ifdef CONFIG_CPU_FREQ
	ret = cpufreq_frequency_table_verify(policy, p->freq_tbl_for_cpufreq);
#endif

	FUNC_EXIT(FUNC_LV_MODULE);

	return ret;
}

static int _mt_cpufreq_target(struct cpufreq_policy *policy, unsigned int target_freq,
			      unsigned int relation)
{
	enum mt_cpu_dvfs_id id = _get_cpu_dvfs_id(policy->cpu);
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	unsigned int new_opp_idx;

	FUNC_ENTER(FUNC_LV_MODULE);
/* Fix me */
	return 0;

#if 1
	if (dvfs_disable_flag == 1)
		return 0;
#endif

	if (policy->cpu >= num_possible_cpus()
	    || cpufreq_frequency_table_target(policy, id_to_cpu_dvfs(id)->freq_tbl_for_cpufreq,
					      target_freq, relation, &new_opp_idx)
	    || (id_to_cpu_dvfs(id) && id_to_cpu_dvfs(id)->dvfs_disable_by_procfs)
	    )
		return -EINVAL;

	_mt_cpufreq_dvfs_request_wrapper(p, new_opp_idx, MT_CPU_DVFS_NORMAL, NULL);

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static int _mt_cpufreq_init(struct cpufreq_policy *policy)
{
	int ret = -EINVAL;
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_MODULE);

	policy->shared_type = CPUFREQ_SHARED_TYPE_ANY;
	cpumask_setall(policy->cpus);

	policy->cpuinfo.transition_latency = 1000;

	{
		enum mt_cpu_dvfs_id id = _get_cpu_dvfs_id(policy->cpu);
		struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
		unsigned int lv = _mt_cpufreq_get_cpu_level();
		struct opp_tbl_info *opp_tbl_info;
		struct opp_tbl_m_info *opp_tbl_m_info;
		struct opp_tbl_m_info *opp_tbl_m_cci_info;
		struct mt_cpu_dvfs *p_cci;

		cpufreq_ver("DVFS: _mt_cpufreq_init: %s(cpu_id = %d)\n", cpu_dvfs_get_name(p), p->cpu_id);

		opp_tbl_info = &opp_tbls[id][CPU_LV_TO_OPP_IDX(lv)];

		BUG_ON(NULL == p);
		BUG_ON(!
		       (lv == CPU_LEVEL_0 || lv == CPU_LEVEL_1 || lv == CPU_LEVEL_2
			|| lv == CPU_LEVEL_3));

		p->cpu_level = lv;

		ret = _mt_cpufreq_setup_freqs_table(policy,
						    opp_tbl_info->opp_tbl, opp_tbl_info->size);

		policy->cpuinfo.max_freq = cpu_dvfs_get_max_freq(id_to_cpu_dvfs(id));
		policy->cpuinfo.min_freq = cpu_dvfs_get_min_freq(id_to_cpu_dvfs(id));

		policy->cur = mt_cpufreq_get_cur_phy_freq(id);	/* use cur phy freq is better */
		policy->max = cpu_dvfs_get_max_freq(id_to_cpu_dvfs(id));
		policy->min = cpu_dvfs_get_min_freq(id_to_cpu_dvfs(id));

		if (_mt_cpufreq_sync_opp_tbl_idx(p) >= 0)
			if (p->idx_normal_max_opp == -1)
				p->idx_normal_max_opp = p->idx_opp_tbl;

		opp_tbl_m_info = &opp_tbls_m[id][CPU_LV_TO_OPP_IDX(lv)];
		p->freq_tbl = opp_tbl_m_info->opp_tbl_m;

		cpufreq_lock(flags);
		p->mt_policy = policy;
		p->armpll_is_available = 1;
#ifdef CONFIG_HYBRID_CPU_DVFS
		cpuhvfs_set_cluster_on_off(arch_get_cluster_id(p->cpu_id), 1);
#endif
		cpufreq_unlock(flags);

		p_cci = id_to_cpu_dvfs(MT_CPU_DVFS_CCI);

		/* init cci freq idx */
		if (_mt_cpufreq_sync_opp_tbl_idx(p_cci) >= 0)
			if (p_cci->idx_normal_max_opp == -1)
				p_cci->idx_normal_max_opp = p_cci->idx_opp_tbl;

		opp_tbl_m_cci_info = &opp_tbls_m[MT_CPU_DVFS_CCI][CPU_LV_TO_OPP_IDX(lv)];
		p_cci->freq_tbl = opp_tbl_m_cci_info->opp_tbl_m;
		p_cci->mt_policy = NULL;
		p_cci->armpll_is_available = 1;
	}

	if (ret)
		cpufreq_err("failed to setup frequency table\n");

	FUNC_EXIT(FUNC_LV_MODULE);

	return ret;
}

static int _mt_cpufreq_exit(struct cpufreq_policy *policy)
{
#if 0
	enum mt_cpu_dvfs_id id = _get_cpu_dvfs_id(policy->cpu);
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
	unsigned long flags;

	cpufreq_lock(flags);
	p->mt_policy = NULL;
	p->armpll_is_available = 0;
	cpufreq_unlock(flags);
#endif

	return 0;
}

static unsigned int _mt_cpufreq_get(unsigned int cpu)
{
	struct mt_cpu_dvfs *p;

	FUNC_ENTER(FUNC_LV_MODULE);

	p = id_to_cpu_dvfs(_get_cpu_dvfs_id(cpu));

	BUG_ON(NULL == p);

	FUNC_EXIT(FUNC_LV_MODULE);

	return cpu_dvfs_get_cur_freq(p);
}

#ifdef CONFIG_CPU_FREQ
static struct freq_attr *_mt_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver _mt_cpufreq_driver = {
	.flags = CPUFREQ_ASYNC_NOTIFICATION,
	.verify = _mt_cpufreq_verify,
	.target = _mt_cpufreq_target,
	.init = _mt_cpufreq_init,
	.exit = _mt_cpufreq_exit,
	.get = _mt_cpufreq_get,
	.name = "mt-cpufreq",
	.attr = _mt_cpufreq_attr,
};
#endif

/*
 * Platform driver
 */
static int
_mt_cpufreq_pm_callback(struct notifier_block *nb,
		unsigned long action, void *ptr)
{
	struct mt_cpu_dvfs *p;
	int i;
	unsigned long flags;

	switch (action) {

	case PM_SUSPEND_PREPARE:
		cpufreq_ver("PM_SUSPEND_PREPARE\n");
		cpufreq_lock(flags);
		for_each_cpu_dvfs(i, p) {
			if (!cpu_dvfs_is_available(p))
				continue;
			p->dvfs_disable_by_suspend = true;
		}
		cpufreq_unlock(flags);
		break;
	case PM_HIBERNATION_PREPARE:
		break;

	case PM_POST_SUSPEND:
		cpufreq_ver("PM_POST_SUSPEND\n");
		cpufreq_lock(flags);
		for_each_cpu_dvfs(i, p) {
			if (!cpu_dvfs_is_available(p))
				continue;
			p->dvfs_disable_by_suspend = false;
		}
		cpufreq_unlock(flags);
		break;
	case PM_POST_HIBERNATION:
		break;

	default:
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static int _mt_cpufreq_suspend(struct device *dev)
{
#if 0
	struct mt_cpu_dvfs *p;
	int i;

	FUNC_ENTER(FUNC_LV_MODULE);

	for_each_cpu_dvfs(i, p) {
		if (!cpu_dvfs_is_available(p))
			continue;

		p->dvfs_disable_by_suspend = true;
	}

	FUNC_EXIT(FUNC_LV_MODULE);
#endif
	return 0;
}

static int _mt_cpufreq_resume(struct device *dev)
{
#if 0
	struct mt_cpu_dvfs *p;
	int i;

	FUNC_ENTER(FUNC_LV_MODULE);

	for_each_cpu_dvfs(i, p) {
		if (!cpu_dvfs_is_available(p))
			continue;

		p->dvfs_disable_by_suspend = false;
	}

	FUNC_EXIT(FUNC_LV_MODULE);
#endif
	return 0;
}

static int _mt_cpufreq_pdrv_probe(struct platform_device *pdev)
{
	/* For table preparing*/
	unsigned int lv = _mt_cpufreq_get_cpu_level();
	struct mt_cpu_dvfs *p;
	int i, j;
	struct opp_tbl_info *opp_tbl_info;
	struct cpufreq_frequency_table *table;
	/* For init voltage check */
	struct buck_ctrl_t *vproc_p;
	struct buck_ctrl_t *vsram_p;
	unsigned int cur_vproc, cur_vsram;

	FUNC_ENTER(FUNC_LV_MODULE);

	BUG_ON(!(lv == CPU_LEVEL_0 || lv == CPU_LEVEL_1 || lv == CPU_LEVEL_2 || lv == CPU_LEVEL_3));

	_mt_cpufreq_aee_init();

	/* Prepare OPP table for PPM in probe to avoid nested lock */
	for_each_cpu_dvfs(j, p) {
		/* Prepare pll related address once */
		prepare_pll_addr(p->Pll_id);
		/* Check all PMIC init voltage once */
		vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);
		vsram_p = id_to_buck_ctrl(p->Vsram_buck_id);
		cur_vsram = vsram_p->buck_ops->get_cur_volt(vsram_p);
		cur_vproc = vproc_p->buck_ops->get_cur_volt(vproc_p);

		if (unlikely(!((cur_vsram >= cur_vproc) &&
			(MAX_DIFF_VSRAM_VPROC >= (cur_vsram - cur_vproc))))) {

			aee_kernel_warning(TAG, "@%s():%d, cur_vsram(%s)=%d, cur_vproc(%s)=%d\n",
				__func__, __LINE__, cpu_dvfs_get_name(vsram_p),
				cur_vsram, cpu_dvfs_get_name(vproc_p), cur_vproc);
			BUG_ON(1);
		}

		opp_tbl_info = &opp_tbls[j][CPU_LV_TO_OPP_IDX(lv)];

		if (NULL == p->freq_tbl_for_cpufreq) {
			table = kzalloc((opp_tbl_info->size + 1) * sizeof(*table), GFP_KERNEL);

			if (NULL == table)
				return -ENOMEM;

			for (i = 0; i < opp_tbl_info->size; i++) {
				table[i].driver_data = i;
				table[i].frequency = opp_tbl_info->opp_tbl[i].cpufreq_khz;
			}

			table[opp_tbl_info->size].driver_data = i;
			table[opp_tbl_info->size].frequency = CPUFREQ_TABLE_END;

			p->opp_tbl = opp_tbl_info->opp_tbl;
			p->nr_opp_tbl = opp_tbl_info->size;
			p->freq_tbl_for_cpufreq = table;
		}
/* Fix me, PPM */
#if 0
		/* lv should be sync with DVFS_TABLE_TYPE_SB */
		if (j != MT_CPU_DVFS_CCI)
				mt_ppm_set_dvfs_table(p->cpu_id, p->freq_tbl_for_cpufreq, opp_tbl_info->size, lv);
#endif
	}

#ifdef CONFIG_CPU_FREQ
	cpufreq_register_driver(&_mt_cpufreq_driver);
#endif

	register_hotcpu_notifier(&_mt_cpufreq_cpu_notifier);
/* Fix me, PPM */
#if 0
	mt_ppm_register_client(PPM_CLIENT_DVFS, &ppm_limit_callback);
#endif
	pm_notifier(_mt_cpufreq_pm_callback, 0);
	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static int _mt_cpufreq_pdrv_remove(struct platform_device *pdev)
{
	FUNC_ENTER(FUNC_LV_MODULE);

	unregister_hotcpu_notifier(&_mt_cpufreq_cpu_notifier);
#ifdef CONFIG_CPU_FREQ
	cpufreq_unregister_driver(&_mt_cpufreq_driver);
#endif

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static const struct dev_pm_ops _mt_cpufreq_pm_ops = {
	.suspend = _mt_cpufreq_suspend,
	.resume = _mt_cpufreq_resume,
	.freeze = _mt_cpufreq_suspend,
	.thaw = _mt_cpufreq_resume,
	.restore = _mt_cpufreq_resume,
};

struct platform_device _mt_cpufreq_pdev = {
	.name = "mt-cpufreq",
	.id = -1,
};

static struct platform_driver _mt_cpufreq_pdrv = {
	.probe = _mt_cpufreq_pdrv_probe,
	.remove = _mt_cpufreq_pdrv_remove,
	.driver = {
		   .name = "mt-cpufreq",
		   .pm = &_mt_cpufreq_pm_ops,
		   .owner = THIS_MODULE,
		   },
};

/*
* Module driver
*/
static int __init _mt_cpufreq_pdrv_init(void)
{
	int ret = 0;
	struct cpumask cpu_mask;
	unsigned int cluster_num;
	int i;

	FUNC_ENTER(FUNC_LV_MODULE);

	mt_cpufreq_dts_map();

	cluster_num = (unsigned int)arch_get_nr_clusters();
	for (i = 0; i < cluster_num; i++) {
		arch_get_cluster_cpus(&cpu_mask, i);
		cpu_dvfs[i].cpu_id = cpumask_first(&cpu_mask);
		cpufreq_dbg("cluster_id = %d, cluster_cpuid = %d\n",
	       i, cpu_dvfs[i].cpu_id);
	}

#ifdef CONFIG_HYBRID_CPU_DVFS	/* before platform_driver_register */
	ret = cpuhvfs_module_init();
	/*if (ret)
		enable_cpuhvfs = 0;*/
#endif

	/* init proc */
	if (cpufreq_procfs_init())
		goto out;

	/* register platform device/driver */
	ret = platform_device_register(&_mt_cpufreq_pdev);

	if (ret) {
		cpufreq_err("fail to register cpufreq device @ %s()\n", __func__);
		goto out;
	}

	ret = platform_driver_register(&_mt_cpufreq_pdrv);

	if (ret) {
		cpufreq_err("fail to register cpufreq driver @ %s()\n", __func__);
		platform_device_unregister(&_mt_cpufreq_pdev);
	}

out:
	FUNC_EXIT(FUNC_LV_MODULE);

	return ret;
}

static void __exit _mt_cpufreq_pdrv_exit(void)
{
	FUNC_ENTER(FUNC_LV_MODULE);

	platform_driver_unregister(&_mt_cpufreq_pdrv);
	platform_device_unregister(&_mt_cpufreq_pdev);

	FUNC_EXIT(FUNC_LV_MODULE);
}

late_initcall(_mt_cpufreq_pdrv_init);
module_exit(_mt_cpufreq_pdrv_exit);

MODULE_DESCRIPTION("MediaTek CPU DVFS Driver v0.3");
MODULE_LICENSE("GPL");
