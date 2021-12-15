// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/random.h>
#include <linux/pm_opp.h>
#include <linux/energy_model.h>
//#include <mt-plat/met_drv.h>
/* project includes */
#include "../../include/mtk_ppm_api.h"

/* local includes */
#include "mtk_cpufreq_internal.h"
#include "mtk_cpufreq_platform.h"
#include "mtk_cpufreq_debug.h"
#include "mtk_cpufreq_hybrid.h"
#include "mtk_cpufreq_opp_table.h"

#define DCM_ENABLE 1

#define met_tag_oneshot(a, b, c) do {} while (0)
//#define _set_met_tag_oneshot(a, bz) do{}while(0)

/*
 * Global Variables
 */
bool is_in_cpufreq;
DEFINE_MUTEX(cpufreq_mutex);
DEFINE_MUTEX(cpufreq_para_mutex);
struct opp_idx_tbl opp_tbl_m[NR_OPP_IDX];
int dvfs_disable_flag;
int new_idx_bk;

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

static unsigned int _calc_new_opp_idx(struct mt_cpu_dvfs *p, int new_opp_idx);
static unsigned int _calc_new_opp_idx_no_base(struct mt_cpu_dvfs *p,
	int new_opp_idx);

int _search_available_freq_idx_under_v(struct mt_cpu_dvfs *p,
	unsigned int volt)
{
	int i;

	/* search available voltage */
	for (i = 0; i < p->nr_opp_tbl; i++) {
		if (volt >= cpu_dvfs_get_volt_by_idx(p, i))
			break;
	}

	WARN_ON(i >= p->nr_opp_tbl);

	return i;
}

int _search_available_freq_idx(struct mt_cpu_dvfs *p, unsigned int target_khz,
				      unsigned int relation)
{
	int new_opp_idx = -1;
	int i;

	if (relation == CPUFREQ_RELATION_L) {
		for (i = (signed int)(p->nr_opp_tbl - 1); i >= 0; i--) {
			if (cpu_dvfs_get_freq_by_idx(p, i) >= target_khz) {
				new_opp_idx = i;
				break;
			}
		}
	} else {		/* CPUFREQ_RELATION_H */
		for (i = 0; i < (signed int)p->nr_opp_tbl; i++) {
			if (cpu_dvfs_get_freq_by_idx(p, i) <= target_khz) {
				new_opp_idx = i;
				break;
			}
		}
	}

	return new_opp_idx;
}

int get_cur_volt_wrapper(struct mt_cpu_dvfs *p, struct buck_ctrl_t *volt_p)
{
	unsigned int volt;

	/* For avoiding i2c violation during suspend */
	if (p->dvfs_disable_by_suspend)
		volt = volt_p->cur_volt;
	else
		volt = volt_p->buck_ops->get_cur_volt(volt_p);

	return volt;
}

#ifdef CONFIG_HYBRID_CPU_DVFS
#ifdef DVFS_CLUSTER_REMAPPING
static void _set_met_tag_oneshot(int id, unsigned int target_khz)
{
	if (id == DVFS_CLUSTER_LL)
		met_tag_oneshot(0, "LL", target_khz);
	else if (id == DVFS_CLUSTER_L)
		met_tag_oneshot(0, "L", target_khz);
	else
		met_tag_oneshot(0, "B", target_khz);
}
#endif
static int _cpufreq_set_locked_secure(struct cpufreq_policy *policy,
	struct mt_cpu_dvfs *p, unsigned int target_khz, int log)
{
	int ret = -1;

	aee_record_cpu_dvfs_step(1);

	if (!policy) {
		tag_pr_notice("Can't get policy of %s\n",
		cpu_dvfs_get_name(p));
		goto out;
	}

#ifdef SINGLE_CLUSTER
	cpuhvfs_set_dvfs(cpufreq_get_cluster_id(p->cpu_id), target_khz);
#else
	cpuhvfs_set_dvfs(arch_get_cluster_id(p->cpu_id), target_khz);
#endif

#ifdef DVFS_CLUSTER_REMAPPING
	if (policy->cpu < 4)
		_set_met_tag_oneshot(0, target_khz);
	else if ((policy->cpu >= 4) && (policy->cpu < 8))
		_set_met_tag_oneshot(1, target_khz);
	else if (policy->cpu >= 8)
		_set_met_tag_oneshot(2, target_khz);
#else
	if (policy->cpu < 4)
		met_tag_oneshot(0, "LL", target_khz);
	else if (policy->cpu >= 4)
		met_tag_oneshot(0, "L", target_khz);
	else if (policy->cpu >= 8)
		met_tag_oneshot(0, "B", target_khz);
#endif

	return 0;

out:
	aee_record_cpu_dvfs_step(0);

	return ret;
}
#else
static int _search_for_vco_dds(struct mt_cpu_dvfs *p, int idx,
	struct mt_cpu_freq_method *m)
{
	return (cpu_dvfs_get_freq_by_idx(p, idx) * m->pos_div * m->clk_div);
}

static unsigned int _search_available_volt(struct mt_cpu_dvfs *p,
	unsigned int target_khz)
{
	int i;

	/* search available voltage */
	for (i = p->nr_opp_tbl - 1; i >= 0; i--) {
		if (target_khz <= cpu_dvfs_get_freq_by_idx(p, i))
			break;
	}

	WARN_ON(i < 0);	/* i.e. target_khz > p->opp_tbl[0].cpufreq_khz */

	return cpu_dvfs_get_volt_by_idx(p, i);	/* mv * 100 */
}

#ifndef ONE_CLUSTER
static unsigned int _calc_new_cci_opp_idx(struct mt_cpu_dvfs *p,
	int new_opp_idx, unsigned int *target_cci_volt)
{
	int i;
	struct mt_cpu_dvfs *pp;
	struct mt_cpu_dvfs *p_cci = id_to_cpu_dvfs(MT_CPU_DVFS_CCI);
	unsigned int max_volt = 0;
	unsigned int new_cci_opp_idx;

	for_each_cpu_dvfs_only(i, pp) {
		if (pp->armpll_is_available ||
		pp->idx_opp_tbl != pp->nr_opp_tbl - 1) {
			if (pp == p)
				max_volt = MAX(max_volt,
				cpu_dvfs_get_volt_by_idx(pp, new_opp_idx));
			else
				max_volt = MAX(max_volt,
				cpu_dvfs_get_cur_volt(pp));
		}
	}

	*target_cci_volt = max_volt;
	new_cci_opp_idx =
	_search_available_freq_idx_under_v(p_cci, *target_cci_volt);

	cpufreq_ver("target_cci_volt = %u, target_cci_freq = %u\n",
		    *target_cci_volt,
		    cpu_dvfs_get_freq_by_idx(p_cci, new_cci_opp_idx));

	return new_cci_opp_idx;
}
#endif

void set_cur_freq_wrapper(struct mt_cpu_dvfs *p, unsigned int cur_khz,
	unsigned int target_khz)
{
	int idx;
	struct pll_ctrl_t *pll_p = id_to_pll_ctrl(p->Pll_id);
	unsigned char cur_posdiv, cur_clkdiv;

	/* Get from physical */
	cur_posdiv = get_posdiv(pll_p);
	cur_clkdiv = get_clkdiv(pll_p);

	/* TARGET_OPP_IDX */
	opp_tbl_m[TARGET_OPP_IDX].p = p;
	idx = _search_available_freq_idx(opp_tbl_m[TARGET_OPP_IDX].p,
		target_khz, CPUFREQ_RELATION_L);
	if (idx < 0)
		idx = 0;
	opp_tbl_m[TARGET_OPP_IDX].slot = &p->freq_tbl[idx];
	cpufreq_ver("[TARGET_OPP_IDX][NAME][IDX][FREQ] => %s:%d:%d\n",
		cpu_dvfs_get_name(p), idx, cpu_dvfs_get_freq_by_idx(p, idx));

	if (!p->armpll_is_available) {
		tag_pr_notice
		("%s: armpll not available, cur_khz = %d, target_khz = %d\n",
			cpu_dvfs_get_name(p), cur_khz, target_khz);
	}

	if (cur_khz == target_khz)
		return;

	if (do_dvfs_stress_test)
		tag_pr_debug
("%s: %s: cur_khz = %d(%d), target_khz = %d(%d), clkdiv = %d->%d\n",
__func__, cpu_dvfs_get_name(p), cur_khz, p->idx_opp_tbl, target_khz, idx,
cur_clkdiv, opp_tbl_m[TARGET_OPP_IDX].slot->clk_div);

	aee_record_cpu_dvfs_step(4);

#ifdef DCM_ENABLE
	/* Set DCM to 0 (decrease freq: high -> low) */
	if (cur_khz > target_khz)
		pll_p->pll_ops->set_sync_dcm(0);
#endif

	aee_record_cpu_dvfs_step(5);

	now[SET_FREQ] = ktime_get();

	/* post_div 1 -> 2 */
	if (cur_posdiv < opp_tbl_m[TARGET_OPP_IDX].slot->pos_div)
		pll_p->pll_ops->set_armpll_posdiv(pll_p,
		opp_tbl_m[TARGET_OPP_IDX].slot->pos_div);

	aee_record_cpu_dvfs_step(6);

	/* armpll_div 1 -> 2 */
	if (cur_clkdiv < opp_tbl_m[TARGET_OPP_IDX].slot->clk_div)
		pll_p->pll_ops->set_armpll_clkdiv(pll_p,
		opp_tbl_m[TARGET_OPP_IDX].slot->clk_div);

#ifndef CONFIG_MTK_FREQ_HOPPING
	aee_record_cpu_dvfs_step(7);
	pll_p->pll_ops->set_armpll_dds(pll_p,
	_search_for_vco_dds(p, idx, opp_tbl_m[TARGET_OPP_IDX].slot),
		opp_tbl_m[TARGET_OPP_IDX].slot->pos_div);
#else
	aee_record_cpu_dvfs_step(8);

	pll_p->pll_ops->set_freq_hopping(pll_p,
		_cpu_dds_calc(_search_for_vco_dds(p, idx,
		opp_tbl_m[TARGET_OPP_IDX].slot)));
#endif

	aee_record_cpu_dvfs_step(9);

	/* armpll_div 2 -> 1 */
	if (cur_clkdiv > opp_tbl_m[TARGET_OPP_IDX].slot->clk_div)
		pll_p->pll_ops->set_armpll_clkdiv(pll_p,
		opp_tbl_m[TARGET_OPP_IDX].slot->clk_div);

	aee_record_cpu_dvfs_step(10);

	/* post_div 2 -> 1 */
	if (cur_posdiv > opp_tbl_m[TARGET_OPP_IDX].slot->pos_div)
		pll_p->pll_ops->set_armpll_posdiv(pll_p,
		opp_tbl_m[TARGET_OPP_IDX].slot->pos_div);

	aee_record_cpu_dvfs_step(11);

	delta[SET_FREQ] = ktime_sub(ktime_get(), now[SET_FREQ]);
	if (ktime_to_us(delta[SET_FREQ]) > ktime_to_us(max[SET_FREQ]))
		max[SET_FREQ] = delta[SET_FREQ];

#ifdef DCM_ENABLE
	/* Always set DCM after frequency change */
	pll_p->pll_ops->set_sync_dcm(target_khz/1000);
#endif

	p->idx_opp_tbl = idx;
	aee_record_freq_idx(p, p->idx_opp_tbl);
}

static unsigned int _calc_pmic_settle_time(struct mt_cpu_dvfs *p,
	unsigned int old_vproc, unsigned int old_vsram,
	unsigned int new_vproc, unsigned int new_vsram)
{
	unsigned int delay = 100;
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

static inline void assert_volt_valid(int line, unsigned int volt,
	unsigned int cur_vsram, unsigned int cur_vproc,
	unsigned int old_vsram, unsigned int old_vproc)
{
	if (unlikely(cur_vsram < cur_vproc ||
		     cur_vsram - cur_vproc > MAX_DIFF_VSRAM_VPROC)) {
		tag_pr_notice
		("@%d, volt = %u, cur_vsram = %u (%u), cur_vproc = %u (%u)\n",
		   line, volt, cur_vsram, old_vsram, cur_vproc, old_vproc);
		WARN_ON(1);
	}
}

int set_cur_volt_wrapper(struct mt_cpu_dvfs *p, unsigned int volt)
{				/* volt: vproc (mv*100) */
	unsigned int old_vproc;
	unsigned int old_vsram;
	unsigned int cur_vsram;
	unsigned int cur_vproc;
	unsigned int delay_us;

	struct buck_ctrl_t *vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);
	struct buck_ctrl_t *vsram_p = id_to_buck_ctrl(p->Vsram_buck_id);

	/* For avoiding i2c violation during suspend */
	if (p->dvfs_disable_by_suspend)
		return 0;

	cur_vproc = get_cur_volt_wrapper(p, vproc_p);
	cur_vsram = get_cur_volt_wrapper(p, vsram_p);

	cpufreq_ver("DVS: Begin vproc %s = %d, vsram %s = %d\n",
		cpu_dvfs_get_name(vproc_p), cur_vproc,
		cpu_dvfs_get_name(vsram_p), cur_vsram);

	if (cur_vproc == 0) {
		tag_pr_notice("@%s():%d, can not use ext buck!\n",
		__func__, __LINE__);
		return -1;
	}

	assert_volt_valid(__LINE__, volt, cur_vsram, cur_vproc,
					cur_vsram, cur_vproc);
#ifdef SUPPORT_VOLT_HW_AUTO_TRACK
	old_vproc = cur_vproc;
	old_vsram = cur_vsram;

	volt = MIN(volt, MAX_VPROC_VOLT);

	if (volt > old_vproc)
		notify_cpu_volt_sampler(p->id, volt, VOLT_UP, VOLT_PRECHANGE);
	else
		notify_cpu_volt_sampler(p->id, volt, VOLT_DOWN, VOLT_PRECHANGE);

	vproc_p->buck_ops->set_cur_volt(vproc_p, volt);

	delay_us =
		_calc_pmic_settle_time(p, old_vproc, old_vsram,
							volt, cur_vsram);
	udelay(delay_us);

	cpufreq_ver
("@%s():old_vsram=%d,cur_vsram=%d,old_vproc=%d,cur_vproc=%d,delay=%d\n",
__func__, old_vsram, cur_vsram, old_vproc, volt, delay_us);

	cur_vsram = get_cur_volt_wrapper(p, vsram_p);
	cur_vproc = get_cur_volt_wrapper(p, vproc_p);

	assert_volt_valid(__LINE__, volt, cur_vsram, cur_vproc,
		old_vsram, old_vproc);

	if (volt > old_vproc)
		notify_cpu_volt_sampler(p->id, volt, VOLT_UP,
			VOLT_POSTCHANGE);
	else
		notify_cpu_volt_sampler(p->id, volt, VOLT_DOWN,
			VOLT_POSTCHANGE);
#else
	/* UP */
	if (volt > cur_vproc) {
		unsigned int target_vsram = volt + NORMAL_DIFF_VRSAM_VPROC;
		unsigned int next_vsram;

		notify_cpu_volt_sampler(p->id, volt, VOLT_UP, VOLT_PRECHANGE);
		do {
			old_vproc = cur_vproc;
			old_vsram = cur_vsram;

			next_vsram = MIN((MAX_DIFF_VSRAM_VPROC - 2500) +
						cur_vproc, target_vsram);

			/* update vsram */
			cur_vsram = MAX(next_vsram, MIN_VSRAM_VOLT);

			if (cur_vsram > MAX_VSRAM_VOLT) {
				cur_vsram = MAX_VSRAM_VOLT;
				target_vsram = MAX_VSRAM_VOLT;
				/* to end the loop */
			}

			assert_volt_valid(__LINE__, volt, cur_vsram, cur_vproc,
							old_vsram, old_vproc);

			/* update vsram */
			vsram_p->buck_ops->set_cur_volt(vsram_p, cur_vsram);

			/* update vproc */
			if (next_vsram > MAX_VSRAM_VOLT)
				/* Vsram was limited, */
				/* set to target vproc directly */
				cur_vproc = volt;
			else
				cur_vproc = next_vsram -
						NORMAL_DIFF_VRSAM_VPROC;

			assert_volt_valid(__LINE__, volt, cur_vsram, cur_vproc,
							old_vsram, old_vproc);

			/* update vproc */
			vproc_p->buck_ops->set_cur_volt(vproc_p, cur_vproc);

			delay_us =
			    _calc_pmic_settle_time(p, old_vproc, old_vsram,
							cur_vproc, cur_vsram);
			udelay(delay_us);

			cpufreq_ver
("@%s():UP-->old_vsram=%d,cur_vsram=%d,old_vproc=%d,cur_vproc=%d,delay=%d\n",
__func__, old_vsram, cur_vsram, old_vproc, cur_vproc, delay_us);
		} while (cur_vsram < target_vsram);
		notify_cpu_volt_sampler(p->id, volt, VOLT_UP, VOLT_POSTCHANGE);
	}
	/* DOWN */
	else if (volt < cur_vproc) {
		unsigned int next_vproc;
		unsigned int next_vsram = cur_vproc + NORMAL_DIFF_VRSAM_VPROC;

		notify_cpu_volt_sampler(p->id, volt, VOLT_DOWN,
						VOLT_PRECHANGE);
		do {
			old_vproc = cur_vproc;
			old_vsram = cur_vsram;

			next_vproc =
			MAX(next_vsram - (MAX_DIFF_VSRAM_VPROC - 2500), volt);

			/* update vproc */
			cur_vproc = next_vproc;

			assert_volt_valid(__LINE__, volt, cur_vsram, cur_vproc,
						old_vsram, old_vproc);

			/* update vproc */
			vproc_p->buck_ops->set_cur_volt(vproc_p, cur_vproc);

			/* update vsram */
			next_vsram = cur_vproc + NORMAL_DIFF_VRSAM_VPROC;
			cur_vsram = MAX(next_vsram, MIN_VSRAM_VOLT);
			cur_vsram = MIN(cur_vsram, MAX_VSRAM_VOLT);

			assert_volt_valid(__LINE__, volt, cur_vsram, cur_vproc,
						old_vsram, old_vproc);

			/* update vsram */
			vsram_p->buck_ops->set_cur_volt(vsram_p, cur_vsram);

			delay_us =
			    _calc_pmic_settle_time(p, old_vproc, old_vsram,
						cur_vproc, cur_vsram);
			udelay(delay_us);

			cpufreq_ver
("@%s():DOWN-->old_vsram=%d,cur_vsram=%d,old_vproc=%d,cur_vproc=%d,delay=%d\n",
__func__, old_vsram, cur_vsram, old_vproc, cur_vproc, delay_us);
		} while (cur_vproc > volt);
		notify_cpu_volt_sampler(p->id, volt, VOLT_DOWN,
						VOLT_POSTCHANGE);
	}
#endif

	vsram_p->cur_volt = cur_vsram;
	vproc_p->cur_volt = cur_vproc;

	aee_record_cpu_volt(p, volt);

	cpufreq_ver("DVFS: End @%s(): %s, cur_vsram = %d, cur_vproc = %d\n",
		__func__, cpu_dvfs_get_name(p), cur_vsram, cur_vproc);

	cpufreq_ver("DVS: End @%s(): %s, vsram(%s) = %d, cur_vproc(%s) = %d\n",
		__func__, cpu_dvfs_get_name(p),
		cpu_dvfs_get_name(vsram_p), get_cur_volt_wrapper(p, vsram_p),
		cpu_dvfs_get_name(vproc_p), get_cur_volt_wrapper(p, vproc_p));

	return 0;
}

#ifndef ONE_CLUSTER
static int _cpufreq_set_locked_cci(unsigned int target_cci_khz,
	unsigned int target_cci_volt)
{
	int ret = -1;
	struct mt_cpu_dvfs *p_cci = id_to_cpu_dvfs(MT_CPU_DVFS_CCI);
	unsigned int cur_cci_khz;
	unsigned int cur_cci_volt;

	struct buck_ctrl_t *vproc_p;
	struct buck_ctrl_t *vsram_p;
	struct pll_ctrl_t *pll_p;

	aee_record_cpu_dvfs_in(p_cci);

	aee_record_cci_dvfs_step(1);

	vproc_p = id_to_buck_ctrl(p_cci->Vproc_buck_id);
	vsram_p = id_to_buck_ctrl(p_cci->Vsram_buck_id);
	pll_p = id_to_pll_ctrl(p_cci->Pll_id);

	cur_cci_khz = pll_p->pll_ops->get_cur_freq(pll_p);
	cur_cci_volt = get_cur_volt_wrapper(p_cci, vproc_p);

	if (cur_cci_khz != target_cci_khz)
		cpufreq_ver
		    ("@%s(), %s: cci_freq = (%d, %d), cci_volt = (%d, %d)\n",
		     __func__, cpu_dvfs_get_name(p_cci), cur_cci_khz,
		     target_cci_khz, cur_cci_volt, target_cci_volt);

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

	aee_record_cci_dvfs_step(4);

	/* Set cci voltage (DOWN) */
	if (target_cci_volt != 0 && (target_cci_volt < cur_cci_volt)) {
		ret = set_cur_volt_wrapper(p_cci, target_cci_volt);
		if (ret)	/* set volt fail */
			goto out;
	}

	aee_record_cci_dvfs_step(5);

	cpufreq_ver("@%s(): Vproc = %dmv, Vsram = %dmv, freq = %dKHz\n",
		    __func__,
			get_cur_volt_wrapper(p_cci, vproc_p) / 100,
			get_cur_volt_wrapper(p_cci, vsram_p) / 100,
			pll_p->pll_ops->get_cur_freq(pll_p));

out:
	aee_record_cci_dvfs_step(0);

	aee_record_cpu_dvfs_out(p_cci);

	return ret;
}
#endif

static void dump_all_opp_table(void)
{
	int i;
	struct mt_cpu_dvfs *p;

	for_each_cpu_dvfs(i, p) {
		tag_pr_notice("[%s/%d] available = %d, oppidx = %d (%u, %u)\n",
			      p->name, p->cpu_id,
			      p->armpll_is_available, p->idx_opp_tbl,
			      cpu_dvfs_get_freq_by_idx(p, p->idx_opp_tbl),
			      cpu_dvfs_get_volt_by_idx(p, p->idx_opp_tbl));

#ifndef ONE_CLUSTER
		if (i == MT_CPU_DVFS_CCI) {
			for (i = 0; i < p->nr_opp_tbl; i++) {
				tag_pr_notice("%-2d (%u, %u)\n",
					i, cpu_dvfs_get_freq_by_idx(p, i),
					cpu_dvfs_get_volt_by_idx(p, i));
			}
		}
#endif

	}
}

static int _cpufreq_set_locked(struct cpufreq_policy *policy,
	struct mt_cpu_dvfs *p, unsigned int target_khz, int log)
{
	int ret = -1;
	unsigned int target_volt;
#ifndef ONE_CLUSTER
	unsigned int cur_volt;
	/* CCI */
	struct mt_cpu_dvfs *p_cci;
	int new_cci_opp_idx;
	int new_opp_idx;
	unsigned int cur_cci_khz, target_cci_khz, target_cci_volt, cur_khz;
	unsigned long flags;
#else
	unsigned int cur_khz;
	unsigned long flags;
#endif

	struct buck_ctrl_t *vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);
	struct buck_ctrl_t *vsram_p = id_to_buck_ctrl(p->Vsram_buck_id);
	struct pll_ctrl_t *pll_p = id_to_pll_ctrl(p->Pll_id);
#ifndef ONE_CLUSTER
	struct pll_ctrl_t *pcci_p;
#endif

	struct cpufreq_freqs freqs;
	unsigned int target_khz_orig = target_khz;

	FUNC_ENTER(FUNC_LV_HELP);

	if (dvfs_disable_flag == 1)
		return 0;

	if (!policy) {
		tag_pr_notice("Can't get policy of %s\n",
				cpu_dvfs_get_name(p));
		goto out;
	}

	if (new_idx_bk == p->idx_opp_tbl)
		goto out;

#ifndef ONE_CLUSTER
	/* MCSI Output */
	p_cci = id_to_cpu_dvfs(MT_CPU_DVFS_CCI);
	pcci_p = id_to_pll_ctrl(p_cci->Pll_id);
	new_opp_idx =
	_search_available_freq_idx(p, target_khz, CPUFREQ_RELATION_L);
	if (new_opp_idx == -1) {
		tag_pr_info("%s cant find freq idx new_opp_idx = %d\n",
				__func__, new_opp_idx);
		goto out;
	}
	new_cci_opp_idx =
	_calc_new_cci_opp_idx(p, new_opp_idx, &target_cci_volt);
	cur_cci_khz = pcci_p->pll_ops->get_cur_freq(pcci_p);
	target_cci_khz = cpu_dvfs_get_freq_by_idx(p_cci, new_cci_opp_idx);
#endif

	cur_khz = pll_p->pll_ops->get_cur_freq(pll_p);

#ifndef ONE_CLUSTER
	/* Same buck with MCSI */
	if (p_cci->Vproc_buck_id == p->Vproc_buck_id) {
		target_volt = target_cci_volt;
		target_cci_volt = 0;
	} else
		target_volt = _search_available_volt(p, target_khz);
#else
		target_volt = _search_available_volt(p, target_khz);
#endif

	cpufreq_para_lock(flags);
	if (cur_khz != target_khz) {
		if (log || do_dvfs_stress_test)
#ifndef ONE_CLUSTER
			cpufreq_ver
("@%s(), %s:(%d,%d): freq=%d, volt =%d, on=%d, cur=%d, cci(%d,%d)\n",
__func__, cpu_dvfs_get_name(p), p->idx_opp_ppm_base, p->idx_opp_ppm_limit,
target_khz, target_volt, num_online_cpus(), cur_khz,
cur_cci_khz, target_cci_khz);
	}
#else
			cpufreq_ver
("@%s(), %s:(%d,%d): freq=%d, volt =%d, on=%d, cur=%d\n",
__func__, cpu_dvfs_get_name(p), p->idx_opp_ppm_base, p->idx_opp_ppm_limit,
target_khz, target_volt, num_online_cpus(), cur_khz);
	}
#endif
	cpufreq_para_unlock(flags);

	aee_record_cpu_dvfs_step(1);

	/* set volt (UP) */
#ifndef ONE_CLUSTER
	cur_volt = get_cur_volt_wrapper(p, vproc_p);
	if (target_volt > cur_volt) {
#else
	if (target_volt > vproc_p->cur_volt) {
#endif
		ret = set_cur_volt_wrapper(p, target_volt);
		if (ret)	/* set volt fail */
			goto out;
	}

	aee_record_cpu_dvfs_step(2);

	freqs.old = cur_khz;
	freqs.new = target_khz_orig;
	if (policy) {
		freqs.cpu = policy->cpu;
		cpufreq_freq_transition_begin(policy, &freqs);
	}

	aee_record_cpu_dvfs_step(3);

	/* set freq (UP/DOWN) */
	if (cur_khz != target_khz)
		set_cur_freq_wrapper(p, cur_khz, target_khz);

	aee_record_cpu_dvfs_step(12);

	if (policy)
		cpufreq_freq_transition_end(policy, &freqs, 0);

	aee_record_cpu_dvfs_step(13);

#ifndef ONE_CLUSTER
	/* set cci freq/volt */
	_cpufreq_set_locked_cci(target_cci_khz, target_cci_volt);
#endif

	aee_record_cpu_dvfs_step(14);

	/* set volt (DOWN) */
#ifndef ONE_CLUSTER
	cur_volt = get_cur_volt_wrapper(p, vproc_p);
	if (cur_volt > target_volt) {
#else
	if (vproc_p->cur_volt > target_volt) {
#endif
		ret = set_cur_volt_wrapper(p, target_volt);

		if (ret)	/* set volt fail */
			goto out;
	}

	aee_record_cpu_dvfs_step(15);

	cpufreq_ver
	("DVFS: @%s(): Vproc = %dmv, Vsram = %dmv, freq(%s) = %dKHz\n",
	    __func__,
		get_cur_volt_wrapper(p, vproc_p) / 100,
		get_cur_volt_wrapper(p, vsram_p) / 100, p->name,
		pll_p->pll_ops->get_cur_freq(pll_p));

	/* trigger exception if freq/volt not correct during stress */
	if (do_dvfs_stress_test && !p->dvfs_disable_by_suspend) {
		unsigned int volt = get_cur_volt_wrapper(p, vproc_p);
		unsigned int freq = pll_p->pll_ops->get_cur_freq(pll_p);

		if (volt < target_volt || freq != target_khz) {
			tag_pr_notice
		("volt = %u, target_volt = %u, freq = %u, target_khz = %u\n",
			volt, target_volt, freq, target_khz);
			dump_all_opp_table();
		}
	}

	FUNC_EXIT(FUNC_LV_HELP);

	aee_record_cpu_dvfs_step(0);

	return 0;

out:
	aee_record_cpu_dvfs_step(0);

	return ret;
}
#endif	/* CONFIG_HYBRID_CPU_DVFS */

static void _mt_cpufreq_set(struct cpufreq_policy *policy,
	struct mt_cpu_dvfs *p, int new_opp_idx,
	enum mt_cpu_dvfs_action_id action)
{
	unsigned int target_freq;
	int ret = -1;
	int log = 0;

	FUNC_ENTER(FUNC_LV_LOCAL);

	if (p->dvfs_disable_by_suspend || p->armpll_is_available != 1)
		return;

	if (do_dvfs_stress_test && action == MT_CPU_DVFS_NORMAL) {
		new_opp_idx = prandom_u32() % p->nr_opp_tbl;
		if (new_opp_idx == p->idx_opp_tbl)
			new_opp_idx = (p->nr_opp_tbl ? p->nr_opp_tbl - 1 : 0);
	}

	if (action == MT_CPU_DVFS_ONLINE || action == MT_CPU_DVFS_DP ||
	    (do_dvfs_stress_test && action == MT_CPU_DVFS_NORMAL))
		new_opp_idx = _calc_new_opp_idx_no_base(p, new_opp_idx);
	else
		new_opp_idx = _calc_new_opp_idx(p, new_opp_idx);

	if (action != MT_CPU_DVFS_EEM_UPDATE)
		new_idx_bk = new_opp_idx;
	else
		new_idx_bk = -1;

	target_freq = cpu_dvfs_get_freq_by_idx(p, new_opp_idx);

	now[SET_DVFS] = ktime_get();

	aee_record_cpu_dvfs_in(p);

#ifdef CONFIG_HYBRID_CPU_DVFS
	ret = _cpufreq_set_locked_secure(policy, p, target_freq, log);
#else
	ret = _cpufreq_set_locked(policy, p, target_freq, log);
#endif

	aee_record_cpu_dvfs_out(p);

	delta[SET_DVFS] = ktime_sub(ktime_get(), now[SET_DVFS]);
	if (ktime_to_us(delta[SET_DVFS]) > ktime_to_us(max[SET_DVFS]))
		max[SET_DVFS] = delta[SET_DVFS];

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
#ifdef CONFIG_HYBRID_CPU_DVFS
		_mt_cpufreq_set(p->mt_policy, p, new_opp_idx, action);
#else
		if (new_opp_idx != p->idx_opp_tbl) {
			/* cpufreq_ver("DVFS - %s, MT_CPU_DVFS_NORMAL to */
			/* %d\n", cpu_dvfs_get_name(p), new_opp_idx); */
			_mt_cpufreq_set(p->mt_policy, p, new_opp_idx, action);
		}
#endif
		break;
	case MT_CPU_DVFS_PPM:
		cpufreq_ver("DVFS - MT_CPU_DVFS_PPM\n");
		for_each_cpu_dvfs_only(i, pp) {
			if (pp->armpll_is_available &&
			pp->mt_policy->governor) {
				cpufreq_para_lock(para_flags);
				if (pp->idx_opp_ppm_limit == -1)
					pp->mt_policy->max =
					cpu_dvfs_get_max_freq(pp);
				else
					pp->mt_policy->max =
					cpu_dvfs_get_freq_by_idx(pp,
					pp->idx_opp_ppm_limit);
				if (pp->idx_opp_ppm_base == -1)
					pp->mt_policy->min =
					cpu_dvfs_get_min_freq(pp);
				else
					pp->mt_policy->min =
					cpu_dvfs_get_freq_by_idx(pp,
					pp->idx_opp_ppm_base);

				ignore_ppm = 0;
				if ((pp->idx_opp_tbl >= pp->mt_policy->max) &&
				(pp->idx_opp_tbl <= pp->mt_policy->min)) {
					cpufreq_ver
		("idx = %d, idx_opp_ppm_base = %d, idx_opp_ppm_limit = %d\n",
					pp->idx_opp_tbl, pp->mt_policy->min,
					pp->mt_policy->max);
					ignore_ppm = 1;
				}

				cpufreq_para_unlock(para_flags);
				/* new_opp_idx == current idx */
				if (!ignore_ppm)
					_mt_cpufreq_set(pp->mt_policy, pp,
					pp->idx_opp_tbl, action);
			}
		}
		break;
	case MT_CPU_DVFS_EEM_UPDATE:
		volt_tbl = (unsigned int **)data;
		vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);

		/* Update public table */
		for (i = 0; i < p->nr_opp_tbl; i++)
			p->opp_tbl[i].cpufreq_volt =
			vproc_p->buck_ops->transfer2volt((*volt_tbl)[i]);

#ifndef CONFIG_HYBRID_CPU_DVFS
		for_each_cpu_dvfs_only(i, pp) {
			if ((pp->Vproc_buck_id == p->Vproc_buck_id) &&
			pp->armpll_is_available	&& pp->mt_policy->governor) {
				_mt_cpufreq_set(pp->mt_policy, pp,
				pp->idx_opp_tbl, MT_CPU_DVFS_EEM_UPDATE);
				break;
			}
		}
#endif
		break;
	default:
		break;
	};
	cpufreq_unlock(flags);
}

static int _sync_opp_tbl_idx(struct mt_cpu_dvfs *p)
{
	unsigned int freq;
	int i;
	struct pll_ctrl_t *pll_p = id_to_pll_ctrl(p->Pll_id);

	freq = pll_p->pll_ops->get_cur_freq(pll_p);

	for (i = p->nr_opp_tbl - 1; i >= 0; i--) {
		if (freq <= cpu_dvfs_get_freq_by_idx(p, i))
			break;
	}

	if (WARN(i < 0, "CURR_FREQ %u IS OVER OPP0 %u\n",
				freq, cpu_dvfs_get_max_freq(p)))
		i = 0;

	p->idx_opp_tbl = i;
	aee_record_freq_idx(p, p->idx_opp_tbl);

	return 0;
}

static int _mt_cpufreq_sync_opp_tbl_idx(struct mt_cpu_dvfs *p)
{
	int ret = -1;

	FUNC_ENTER(FUNC_LV_LOCAL);

	if (cpu_dvfs_is_available(p)) {
		ret = _sync_opp_tbl_idx(p);
		cpufreq_ver("%s freq = %d\n", cpu_dvfs_get_name(p),
			cpu_dvfs_get_cur_freq(p));
	}

	FUNC_EXIT(FUNC_LV_LOCAL);

	return ret;
}

static enum mt_cpu_dvfs_id _get_cpu_dvfs_id(unsigned int cpu_id)
{
	int cluster_id;

#ifdef SINGLE_CLUSTER
	cluster_id = cpufreq_get_cluster_id(cpu_id);
#else
	cluster_id = arch_get_cluster_id(cpu_id);
#endif

	return cluster_id;
}

static int _mt_cpufreq_setup_freqs_table(struct cpufreq_policy *policy,
	struct mt_cpu_freq_info *freqs, int num)
{
	struct mt_cpu_dvfs *p;
	int ret = 0;
#ifdef SINGLE_CLUSTER
	struct cpumask cpu_mask;
#endif

	FUNC_ENTER(FUNC_LV_LOCAL);

	p = id_to_cpu_dvfs(_get_cpu_dvfs_id(policy->cpu));

	ret = cpufreq_frequency_table_cpuinfo(policy, p->freq_tbl_for_cpufreq);

	if (!ret)
		policy->freq_table = p->freq_tbl_for_cpufreq;

#ifdef SINGLE_CLUSTER
	cpufreq_get_cluster_cpus(&cpu_mask, _get_cpu_dvfs_id(policy->cpu));
	cpumask_copy(policy->cpus, &cpu_mask);
#else
	cpumask_copy(policy->cpus, topology_core_cpumask(policy->cpu));
#endif
	cpumask_copy(policy->related_cpus, policy->cpus);

	FUNC_EXIT(FUNC_LV_LOCAL);

	return 0;
}

static unsigned int _calc_new_opp_idx_no_base(struct mt_cpu_dvfs *p,
	int new_opp_idx)
{
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_HELP);

	cpufreq_para_lock(flags);

	cpufreq_ver
	("new_opp_idx = %d, idx_opp_ppm_base = %d, idx_opp_ppm_limit = %d\n",
		new_opp_idx, p->idx_opp_ppm_base, p->idx_opp_ppm_limit);

	if ((p->idx_opp_ppm_limit != -1) &&
	(new_opp_idx < p->idx_opp_ppm_limit))
		new_opp_idx = p->idx_opp_ppm_limit;

	cpufreq_para_unlock(flags);

	FUNC_EXIT(FUNC_LV_HELP);

	return new_opp_idx;
}

static unsigned int _calc_new_opp_idx(struct mt_cpu_dvfs *p, int new_opp_idx)
{
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_HELP);

	cpufreq_para_lock(flags);
	cpufreq_ver
	("new_opp_idx = %d, idx_opp_ppm_base = %d, idx_opp_ppm_limit = %d\n",
		new_opp_idx, p->idx_opp_ppm_base, p->idx_opp_ppm_limit);

	if ((p->idx_opp_ppm_limit != -1) &&
	(new_opp_idx < p->idx_opp_ppm_limit))
		new_opp_idx = p->idx_opp_ppm_limit;

	if ((p->idx_opp_ppm_base != -1) && (new_opp_idx > p->idx_opp_ppm_base))
		new_opp_idx = p->idx_opp_ppm_base;

	if ((p->idx_opp_ppm_base == p->idx_opp_ppm_limit) &&
	p->idx_opp_ppm_base != -1)
		new_opp_idx = p->idx_opp_ppm_base;

	cpufreq_para_unlock(flags);

	FUNC_EXIT(FUNC_LV_HELP);

	return new_opp_idx;
}

static void ppm_limit_callback(struct ppm_client_req req)
{
	struct ppm_client_req *ppm = (struct ppm_client_req *)&req;
	unsigned int i;

#ifdef CONFIG_HYBRID_CPU_DVFS
	for (i = 0; i < ppm->cluster_num; i++) {
		if (ppm->cpu_limit[i].has_advise_freq)
			cpuhvfs_set_min_max(i,
				ppm->cpu_limit[i].advise_cpufreq_idx,
				ppm->cpu_limit[i].advise_cpufreq_idx);
		else
			cpuhvfs_set_min_max(i,
				ppm->cpu_limit[i].min_cpufreq_idx,
				ppm->cpu_limit[i].max_cpufreq_idx);
	}
#else
	unsigned long flags;
	struct mt_cpu_dvfs *p;

	cpufreq_ver("get feedback from PPM module\n");

	cpufreq_para_lock(flags);
	for (i = 0; i < ppm->cluster_num; i++) {
		cpufreq_ver
("[%d]:cluster_id=%d,cpu_id=%d,min_cpufreq_idx=%d,max_cpufreq_idx=%d\n",
			i, ppm->cpu_limit[i].cluster_id,
			ppm->cpu_limit[i].cpu_id,
			ppm->cpu_limit[i].min_cpufreq_idx,
			ppm->cpu_limit[i].max_cpufreq_idx);
		cpufreq_ver("has_advise_freq = %d, advise_cpufreq_idx = %d\n",
			ppm->cpu_limit[i].has_advise_freq,
			ppm->cpu_limit[i].advise_cpufreq_idx);

		p = id_to_cpu_dvfs(i);

		if (ppm->cpu_limit[i].has_advise_freq) {
			p->idx_opp_ppm_base =
			ppm->cpu_limit[i].advise_cpufreq_idx;
			p->idx_opp_ppm_limit =
			ppm->cpu_limit[i].advise_cpufreq_idx;
		} else {
			p->idx_opp_ppm_base =
			ppm->cpu_limit[i].min_cpufreq_idx;
			/* ppm update base */
			p->idx_opp_ppm_limit =
			ppm->cpu_limit[i].max_cpufreq_idx;
			/* ppm update limit */
		}
	}
	cpufreq_para_unlock(flags);

	/* Don't care the parameters */
	_mt_cpufreq_dvfs_request_wrapper(NULL, 0, MT_CPU_DVFS_PPM, NULL);
#endif
}

/*
 * cpufreq driver
 */
static int _mt_cpufreq_verify(struct cpufreq_policy *policy)
{
	struct mt_cpu_dvfs *p;
	int ret; /* cpufreq_frequency_table_verify() always return 0 */

	p = id_to_cpu_dvfs(_get_cpu_dvfs_id(policy->cpu));
	if (!p)
		return -EINVAL;

	ret = cpufreq_frequency_table_verify(policy, p->freq_tbl_for_cpufreq);

	return ret;
}

static int _mt_cpufreq_target(struct cpufreq_policy *policy,
	unsigned int target_freq, unsigned int relation)
{
	struct mt_cpu_dvfs *p;
	unsigned int new_opp_idx;

	p = id_to_cpu_dvfs(_get_cpu_dvfs_id(policy->cpu));
	if (!p)
		return -EINVAL;

	new_opp_idx = cpufreq_frequency_table_target(policy, target_freq,
							relation);
	if (new_opp_idx >= p->nr_opp_tbl)
		return -EINVAL;

	if (dvfs_disable_flag || p->dvfs_disable_by_suspend ||
					p->dvfs_disable_by_procfs)
		return -EPERM;

	_mt_cpufreq_dvfs_request_wrapper(p, new_opp_idx, MT_CPU_DVFS_NORMAL,
									NULL);

	return 0;
}

#ifndef ONE_CLUSTER
int cci_is_inited;
#endif
int turbo_is_inited;
static int _mt_cpufreq_init(struct cpufreq_policy *policy)
{
	int ret = -EINVAL;
	int pd_ret = -EINVAL;
	unsigned long flags;
	struct device *cpu_dev;
	struct em_data_callback em_cb = EM_DATA_CB(of_dev_pm_opp_get_cpu_power);

	FUNC_ENTER(FUNC_LV_MODULE);

	policy->shared_type = CPUFREQ_SHARED_TYPE_ANY;
	cpumask_setall(policy->cpus);

	cpu_dev = get_cpu_device(policy->cpu);

	policy->cpuinfo.transition_latency = 1000;

	{
		enum mt_cpu_dvfs_id id = _get_cpu_dvfs_id(policy->cpu);
		struct mt_cpu_dvfs *p = id_to_cpu_dvfs(id);
		unsigned int lv = _mt_cpufreq_get_cpu_level();
		struct opp_tbl_info *opp_tbl_info;
		struct opp_tbl_m_info *opp_tbl_m_info;
#ifndef ONE_CLUSTER
		struct opp_tbl_m_info *opp_tbl_m_cci_info;
		struct mt_cpu_dvfs *p_cci;
#endif

		cpufreq_ver("DVFS: %s: %s(cpu_id = %d)\n",
				__func__, cpu_dvfs_get_name(p), p->cpu_id);

		opp_tbl_info = &opp_tbls[id][lv];

		p->cpu_level = lv;

		ret = _mt_cpufreq_setup_freqs_table(policy,
		opp_tbl_info->opp_tbl, opp_tbl_info->size);

		policy->cpuinfo.max_freq = cpu_dvfs_get_max_freq(p);
		policy->cpuinfo.min_freq = cpu_dvfs_get_min_freq(p);

		opp_tbl_m_info = &opp_tbls_m[id][lv];
		p->freq_tbl = opp_tbl_m_info->opp_tbl_m;

		cpufreq_lock(flags);
		/* Sync p */
		if (_mt_cpufreq_sync_opp_tbl_idx(p) >= 0)
			if (p->idx_normal_max_opp == -1)
				p->idx_normal_max_opp = p->idx_opp_tbl;

		/* use cur phy freq is better */
		policy->cur = cpu_dvfs_get_cur_freq(p);
		policy->max = cpu_dvfs_get_freq_by_idx(p,
				p->idx_opp_ppm_limit);
		policy->min = cpu_dvfs_get_freq_by_idx(p,
				p->idx_opp_ppm_base);
		p->mt_policy = policy;
		p->armpll_is_available = 1;

		if (turbo_flag && !turbo_is_inited) {
			turbo_is_inited = mt_cpufreq_turbo_config(id,
						cpu_dvfs_get_max_freq(p),
				p->opp_tbl[0].cpufreq_volt);
		}

#ifndef ONE_CLUSTER
		/* Sync cci */
		if (cci_is_inited == 0) {
			p_cci = id_to_cpu_dvfs(MT_CPU_DVFS_CCI);

			/* init cci freq idx */
			if (_mt_cpufreq_sync_opp_tbl_idx(p_cci) >= 0)
				if (p_cci->idx_normal_max_opp == -1)
					p_cci->idx_normal_max_opp =
					p_cci->idx_opp_tbl;

			opp_tbl_m_cci_info = &opp_tbls_m[MT_CPU_DVFS_CCI][lv];
			p_cci->freq_tbl = opp_tbl_m_cci_info->opp_tbl_m;
			p_cci->mt_policy = NULL;
			p_cci->armpll_is_available = 1;
			cci_is_inited = 1;
		}
#endif

#ifdef CONFIG_HYBRID_CPU_DVFS
#ifdef SINGLE_CLUSTER
		cpuhvfs_set_cluster_on_off(cpufreq_get_cluster_id(p->cpu_id),
									1);
#else
		cpuhvfs_set_cluster_on_off(arch_get_cluster_id(p->cpu_id), 1);
#endif
#endif
		cpufreq_unlock(flags);
	}

	if (dev_pm_opp_of_get_sharing_cpus(cpu_dev, policy->cpus))
		tag_pr_notice("failed to share opp framework table\n");

	if (dev_pm_opp_of_cpumask_add_table(policy->cpus))
		tag_pr_notice("failed to add opp framework table\n");

	pd_ret = em_register_perf_domain(policy->cpus, NR_FREQ, &em_cb);

	tag_pr_notice("energy model register result %d\n", pd_ret);

	if (pd_ret)
		tag_pr_notice("energy model regist fail\n");
	if (ret)
		tag_pr_notice("failed to setup frequency table\n");

	FUNC_EXIT(FUNC_LV_MODULE);

	return ret;
}

static int _mt_cpufreq_exit(struct cpufreq_policy *policy)
{
	return 0;
}

static unsigned int _mt_cpufreq_get(unsigned int cpu)
{
	struct mt_cpu_dvfs *p;

	p = id_to_cpu_dvfs(_get_cpu_dvfs_id(cpu));
	if (!p)
		return 0;

	return cpu_dvfs_get_cur_freq(p);
}

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
	return 0;
}

static int _mt_cpufreq_resume(struct device *dev)
{
	return 0;
}

static void _hps_request_wrapper(struct mt_cpu_dvfs *p,
	int new_opp_idx, enum hp_action action, void *data)
{
	enum mt_cpu_dvfs_id *id = (enum mt_cpu_dvfs_id *)data;
	struct mt_cpu_dvfs *act_p;

	act_p = id_to_cpu_dvfs(*id);
	/* action switch */
	/* switch (action & ~CPU_TASKS_FROZEN) { */
	switch (action) {
	case CPUFREQ_CPU_ONLINE:
		aee_record_cpu_dvfs_cb(2);
		if (act_p->armpll_is_available == 0 && act_p == p)
			act_p->armpll_is_available = 1;
#ifndef CONFIG_HYBRID_CPU_DVFS
		cpufreq_ver("DVFS - %s, CPU_ONLINE to %d\n",
			    cpu_dvfs_get_name(p), new_opp_idx);
		_mt_cpufreq_set(p->mt_policy, p, new_opp_idx,
				MT_CPU_DVFS_ONLINE);
#endif
		break;
	case CPUFREQ_CPU_DOWN_PREPARE:
		aee_record_cpu_dvfs_cb(3);
#ifndef CONFIG_HYBRID_CPU_DVFS
		cpufreq_ver("DVFS - %s, CPU_DOWN_PREPARE to %d\n",
			    cpu_dvfs_get_name(p), new_opp_idx);
		_mt_cpufreq_set(p->mt_policy, p, new_opp_idx, MT_CPU_DVFS_DP);
#endif
		if (act_p->armpll_is_available == 1 && act_p == p) {
			act_p->armpll_is_available = 0;
#ifdef CONFIG_HYBRID_CPU_DVFS
			aee_record_cpu_dvfs_cb(4);
#ifdef SINGLE_CLUSTER
			cpuhvfs_set_cluster_on_off(cpufreq_get_cluster_id(
				p->cpu_id), 0);
#else
			cpuhvfs_set_cluster_on_off(
				arch_get_cluster_id(p->cpu_id), 0);
#endif
			aee_record_cpu_dvfs_cb(9);
#endif
			act_p->mt_policy = NULL;
			aee_record_cpu_dvfs_cb(10);
		}
		break;
	default:
		break;
	};
}


static void _mt_cpufreq_cpu_CB_wrapper(enum mt_cpu_dvfs_id cluster_id,
	unsigned int cpus, enum hp_action action)
{
	int i, j;
	struct mt_cpu_dvfs *p;
	unsigned long flags;
	unsigned int cur_volt;
	struct buck_ctrl_t *vproc_p;
	int new_opp_idx;

	aee_record_cpu_dvfs_cb(1);

	for (i = 0; i < nr_hp_action; i++) {
		if (cpu_dvfs_hp_action[i].cluster == cluster_id &&
		    action == cpu_dvfs_hp_action[i].action &&
		    cpus == cpu_dvfs_hp_action[i].trigged_core) {
			cpufreq_lock(flags);
			for_each_cpu_dvfs(j, p) {
				if (
					cpu_dvfs_hp_action[i].hp_action_cfg[
						j].action_id != FREQ_NONE) {
					if (cpu_dvfs_hp_action[i].hp_action_cfg[
						j].action_id == FREQ_HIGH)
						_hps_request_wrapper(p,
							0, action,
							(void *)&cluster_id);
					else if (cpu_dvfs_hp_action[
						i].hp_action_cfg[j].action_id ==
						FREQ_LOW)
						_hps_request_wrapper(p,
							p->nr_opp_tbl - 1,
							action, (void *)&
							cluster_id);
					else if (cpu_dvfs_hp_action[
						i].hp_action_cfg[j].action_id ==
						FREQ_DEPEND_VOLT) {
						vproc_p = id_to_buck_ctrl(
							p->Vproc_buck_id);
						cur_volt =
							get_cur_volt_wrapper(
							p, vproc_p);
						new_opp_idx =
	_search_available_freq_idx_under_v(p, cur_volt);
						cpufreq_ver(
	"DVFS - %s, search volt = %d, idx = %d\n",
						cpu_dvfs_get_name(p), cur_volt,
						new_opp_idx);
						_hps_request_wrapper(p,
						new_opp_idx, action,
						(void *)&cluster_id);
					} else if (cpu_dvfs_hp_action[
					i].hp_action_cfg[j].action_id ==
					FREQ_USR_REQ)
						_hps_request_wrapper(p,
						cpu_dvfs_hp_action[
						i].hp_action_cfg[j].freq_idx,
						action, (void *)&cluster_id);
				}
			}
			cpufreq_unlock(flags);
		}
	}
	aee_record_cpu_dvfs_cb(0);
}

int turbo_flag;
static int _mt_cpufreq_cpu_CB(enum hp_action action,
			      unsigned int cpu)
{
	struct device *dev;
	enum mt_cpu_dvfs_id cluster_id;
	/* CPU mask - Get on-line cpus per-cluster */
	int i;
	struct mt_cpu_dvfs *p;
	struct cpumask dvfs_cpumask[NR_MT_CPU_DVFS];
	struct cpumask cpu_online_cpumask[NR_MT_CPU_DVFS];
	unsigned int cpus[NR_MT_CPU_DVFS];

	if (dvfs_disable_flag == 1)
		return NOTIFY_OK;

#ifdef SINGLE_CLUSTER
	cluster_id = cpufreq_get_cluster_id(cpu);
#else
	cluster_id = arch_get_cluster_id(cpu);
#endif

	for_each_cpu_dvfs_only(i, p) {
#ifdef SINGLE_CLUSTER
		cpufreq_get_cluster_cpus(&dvfs_cpumask[i], i);
#else
		arch_get_cluster_cpus(&dvfs_cpumask[i], i);
#endif
		cpumask_and(&cpu_online_cpumask[i], &dvfs_cpumask[i],
			    cpu_online_mask);
		cpus[i] = cpumask_weight(&cpu_online_cpumask[i]);
	}

	dev = get_cpu_device(cpu);

#ifdef ENABLE_TURBO_MODE_AP
	/* Turbo decision */
	if (dev && turbo_flag)
		mt_cpufreq_turbo_action(action, cpus, cluster_id);
#endif

	if (dev) {
		/* switch (action & ~CPU_TASKS_FROZEN) { */
		switch (action) {
		case CPUFREQ_CPU_ONLINE:
		case CPUFREQ_CPU_DOWN_PREPARE:
		case CPUFREQ_CPU_DOWN_FAIED:
			_mt_cpufreq_cpu_CB_wrapper(cluster_id,
						   cpus[cluster_id], action);
			break;
		default:
			break;
		}
	}

	return NOTIFY_OK;
}

static int cpuhp_cpufreq_online(unsigned int cpu)
{
	_mt_cpufreq_cpu_CB(CPUFREQ_CPU_ONLINE, cpu);
	return 0;
}

static int cpuhp_cpufreq_offline(unsigned int cpu)
{
	_mt_cpufreq_cpu_CB(CPUFREQ_CPU_DOWN_PREPARE, cpu);
	return 0;
}

static enum cpuhp_state hp_online;

static int _mt_cpufreq_pdrv_probe(struct platform_device *pdev)
{
	unsigned int lv = _mt_cpufreq_get_cpu_level();
	struct mt_cpu_dvfs *p;
	int j;
#ifndef CONFIG_HYBRID_CPU_DVFS
	/* For init voltage check */
	struct buck_ctrl_t *vproc_p;
	struct buck_ctrl_t *vsram_p;
	unsigned int cur_vproc, cur_vsram;
#endif

	FUNC_ENTER(FUNC_LV_MODULE);
	/* init proc */
	cpufreq_procfs_init();
	_mt_cpufreq_aee_init();

#ifdef CONFIG_HYBRID_CPU_DVFS
	/* For SSPM probe */
	cpuhvfs_set_init_sta();
	/* Default disable schedule assist DVFS */
	cpuhvfs_set_sched_dvfs_disable(1);
#endif

	mt_cpufreq_regulator_map(pdev);

	/* Prepare OPP table for PPM in probe to avoid nested lock */
	for_each_cpu_dvfs(j, p) {
		/* Prepare pll related address once */
		prepare_pll_addr(p->Pll_id);
		/* Prepare pmic related config once */
		prepare_pmic_config(p);
#ifndef CONFIG_HYBRID_CPU_DVFS
		/* Check all PMIC init voltage once */
		vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);
		vsram_p = id_to_buck_ctrl(p->Vsram_buck_id);
		cur_vsram = get_cur_volt_wrapper(p, vsram_p);
		cur_vproc = get_cur_volt_wrapper(p, vproc_p);
		vsram_p->cur_volt = cur_vsram;
		vproc_p->cur_volt = cur_vproc;

		if (unlikely(!((cur_vsram >= cur_vproc) &&
			(MAX_DIFF_VSRAM_VPROC >= (cur_vsram - cur_vproc))))) {
#ifdef CONFIG_MTK_AEE_FEATURE
			aee_kernel_warning(TAG,
			"@%s():%d, cur_vsram(%s)=%d, cur_vproc(%s)=%d\n",
			__func__, __LINE__, cpu_dvfs_get_name(vsram_p),
			cur_vsram, cpu_dvfs_get_name(vproc_p), cur_vproc);
#endif
		}
#endif
	}

	cpufreq_register_driver(&_mt_cpufreq_driver);

	hp_online = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
						"cpu_dvfs:online",
						cpuhp_cpufreq_online,
						cpuhp_cpufreq_offline);


	for_each_cpu_dvfs(j, p) {
		_sync_opp_tbl_idx(p);
#ifndef ONE_CLUSTER
		/* lv should be sync with DVFS_TABLE_TYPE_SB */
		if (j != MT_CPU_DVFS_CCI)
#endif
			mt_ppm_set_dvfs_table(p->cpu_id,
				p->freq_tbl_for_cpufreq,
				p->nr_opp_tbl, lv);
	}

	mt_ppm_register_client(PPM_CLIENT_DVFS, &ppm_limit_callback);

	pm_notifier(_mt_cpufreq_pm_callback, 0);

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static int _mt_cpufreq_pdrv_remove(struct platform_device *pdev)
{
	FUNC_ENTER(FUNC_LV_MODULE);

	cpuhp_remove_state_nocalls(hp_online);
	cpufreq_unregister_driver(&_mt_cpufreq_driver);

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

#ifndef CPU_DVFS_DT_REG
struct platform_device _mt_cpufreq_pdev = {
	.name = "mt-cpufreq",
	.id = -1,
};
#endif

#if defined(CONFIG_OF) && defined(CPU_DVFS_DT_REG)
static const struct of_device_id mt_cpufreq_match[] = {
	{ .compatible = "mediatek,mt-cpufreq", },
	{ },
};
#endif

static struct platform_driver _mt_cpufreq_pdrv = {
	.probe = _mt_cpufreq_pdrv_probe,
	.remove = _mt_cpufreq_pdrv_remove,
	.driver = {
		   .name = "mt-cpufreq",
		   .pm = &_mt_cpufreq_pm_ops,
		   .owner = THIS_MODULE,
#if defined(CONFIG_OF) && defined(CPU_DVFS_DT_REG)
		   .of_match_table = of_match_ptr(mt_cpufreq_match),
#endif
		   },
};

/* Module driver */
static int __init _mt_cpufreq_tbl_init(void)
{
	unsigned int lv = _mt_cpufreq_get_cpu_level();
	struct mt_cpu_dvfs *p;
	int i, j;
	struct opp_tbl_info *opp_tbl_info;
	struct cpufreq_frequency_table *table;

#ifdef CPU_DVFS_NOT_READY
	return 0;
#endif

	/* Prepare OPP table for EEM */
	for_each_cpu_dvfs(j, p) {
		opp_tbl_info = &opp_tbls[j][lv];

		if (!p->freq_tbl_for_cpufreq) {
			table = kzalloc(
			(opp_tbl_info->size + 1) * sizeof(*table), GFP_KERNEL);

			if (!table)
				return -ENOMEM;

			for (i = 0; i < opp_tbl_info->size; i++) {
				table[i].driver_data = i;
				table[i].frequency =
				opp_tbl_info->opp_tbl[i].cpufreq_khz;
			}

			table[opp_tbl_info->size].driver_data = i;
			table[opp_tbl_info->size].frequency =
			CPUFREQ_TABLE_END;

			p->opp_tbl = opp_tbl_info->opp_tbl;
			p->nr_opp_tbl = opp_tbl_info->size;
			p->freq_tbl_for_cpufreq = table;
		}
	}
	return 0;
}

static int __init _mt_cpufreq_pdrv_init(void)
{
	int ret = 0;
	struct cpumask cpu_mask;
	unsigned int cluster_num;
	int i;

#ifdef CPU_DVFS_NOT_READY
	return 0;
#endif


	mt_cpufreq_dts_map();

#ifdef SINGLE_CLUSTER
	cluster_num = (unsigned int)cpufreq_get_nr_clusters();
#else
	cluster_num = (unsigned int)arch_get_nr_clusters();
#endif

	for (i = 0; i < cluster_num; i++) {
#ifdef SINGLE_CLUSTER
		cpufreq_get_cluster_cpus(&cpu_mask, i);
#else
		arch_get_cluster_cpus(&cpu_mask, i);
#endif
		cpu_dvfs[i].cpu_id = cpumask_first(&cpu_mask);
		tag_pr_info("cluster_id = %d, cluster_cpuid = %d\n",
		i, cpu_dvfs[i].cpu_id);
	}

#ifdef CONFIG_HYBRID_CPU_DVFS	/* before platform_driver_register */
	ret = cpuhvfs_module_init();
#endif

#ifndef CPU_DVFS_DT_REG
	/* register platform device/driver */
	ret = platform_device_register(&_mt_cpufreq_pdev);

	if (ret) {
		tag_pr_notice("fail to register cpufreq device @ %s()\n",
		__func__);
	}
#endif

	ret = platform_driver_register(&_mt_cpufreq_pdrv);

	if (ret) {
		tag_pr_notice("fail to register cpufreq driver @ %s()\n",
		__func__);
#ifndef CPU_DVFS_DT_REG
		platform_device_unregister(&_mt_cpufreq_pdev);
#endif
	}

	return ret;
}

static void __exit _mt_cpufreq_pdrv_exit(void)
{
	platform_driver_unregister(&_mt_cpufreq_pdrv);
#ifndef CPU_DVFS_DT_REG
	platform_device_unregister(&_mt_cpufreq_pdev);
#endif
}

module_init(_mt_cpufreq_tbl_init);
late_initcall(_mt_cpufreq_pdrv_init);
module_exit(_mt_cpufreq_pdrv_exit);

MODULE_DESCRIPTION("MediaTek CPU DVFS Driver v0.3.1");
