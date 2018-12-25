/*
 * Copyright (C) 2017 MediaTek Inc.
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
#include <linux/sched_energy.h>

#ifdef CONFIG_MTK_UNIFY_POWER
/*
 * MTK static specific energy cost model data. There are no unit requirements
 * for the data. Data can be normalized to any reference point, but the
 * normalization must be consistent. That is, one bogo-joule/watt must be the
 * same quantity for all data, but we don't care what it is.
 */
static struct idle_state default_idle_states[] = {
	{ .power = 0 }, /* 0: active idle = WFI, [P8].leak */
	{ .power = 0 }, /* 1: disabled */
	{ .power = 0 }, /* 2: disabled */
	{ .power = 0 }, /* 3: disabled */
	{ .power = 0 }, /* 4: MCDI */
	{ .power = 0 }, /* 5: disabled */
	{ .power = 0 }, /* 6: WFI/SPARK */
};

void init_sched_energy_costs(void)
{
	struct sched_group_energy *sge;
	int sd_level, cpu;

	for_each_possible_cpu(cpu) {

		for_each_possible_sd_level(sd_level) {
			sge = kcalloc(1, sizeof(struct sched_group_energy),
				GFP_NOWAIT);

			sge->nr_idle_states = ARRAY_SIZE(default_idle_states);
#ifdef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
			sge->idle_power = mtk_idle_power;
			sge->busy_power = mtk_busy_power;
#endif
			sge->idle_states = default_idle_states;
			sge_array[cpu][sd_level] = sge;
		}
	}

	return;

}
#endif

#ifdef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
#if defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6758)
/* MT6763: 2 gears. cluster 0 & 1 is buck shared. */
static int share_buck[3] = {1, 0, 2};
/* cpu7 is L+ */
int l_plus_cpu = 7;
#elif defined(CONFIG_MACH_MT6799)
/* MT6799: 3 gears. cluster 0 & 2 is buck shared. */
static int share_buck[3] = {2, 1, 0};
/* No L+ */
int l_plus_cpu = -1;
#elif defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6762)
static int share_buck[3] = {1, 0, 2};
int l_plus_cpu = -1;
#else
/* no buck shared */
static int share_buck[3] = {0, 1, 2};
int l_plus_cpu = -1;
#endif

#define VOLT_SCALE 10

bool is_share_buck(int cid, int *co_buck_cid)
{
	bool ret = false;

	if (share_buck[cid] != cid) {
		*co_buck_cid = share_buck[cid];
		ret = true;
	}

	return ret;
}

static unsigned long
mtk_cluster_max_usage(int cid, struct energy_env *eenv, int *max_cpu)
{
	unsigned long max_usage = 0;
	int cpu = -1;
	struct cpumask cls_cpus;
	int delta;

	*max_cpu = -1;

	arch_get_cluster_cpus(&cls_cpus, cid);

	for_each_cpu(cpu, &cls_cpus) {
		int cpu_usage = 0;

		if (!cpu_online(cpu))
			continue;

		delta = calc_util_delta(eenv, cpu);
		cpu_usage = __cpu_util(cpu, delta);

		if (cpu_usage >= max_usage) {
			max_usage = cpu_usage;
			*max_cpu = cpu;
		}
	}

	return max_usage;
}

int mtk_cluster_capacity_idx(int cid, struct energy_env *eenv)
{
	int idx;
	int cpu;
	unsigned long util = mtk_cluster_max_usage(cid, eenv, &cpu);
	const struct sched_group_energy *sge;
	struct sched_group *sg;
	struct sched_domain *sd;
	int sel_idx = -1; /* final selected index */
	unsigned long new_capacity = util;

	if (cpu == -1) /* maybe no online CPU */
		return -1;

	sd = rcu_dereference_check_sched_domain(cpu_rq(cpu)->sd);
	if (sd) {
		sg = sd->groups;
		sge = sg->sge;
	} else
		return -1;

#ifdef CONFIG_CPU_FREQ_GOV_SCHEDPLUS
	/* OPP idx to refer capacity margin */
	new_capacity = util * capacity_margin_dvfs >> SCHED_CAPACITY_SHIFT;
#endif

	for (idx = 0; idx < sge->nr_cap_states; idx++) {
		if (sge->cap_states[idx].cap >= new_capacity) {
			sel_idx = idx;
			break;
		}
	}

	mt_sched_printf(sched_eas_energy_calc,
		"cid=%d max_cpu=%d (util=%ld new=%ld) opp_idx=%d (cap=%lld)",
		cid, cpu, util, new_capacity, sel_idx,
		(sel_idx > -1) ? sge->cap_states[sel_idx].cap : 0);

	return sel_idx;
}

inline
int mtk_idle_power(int idle_state, int cpu, void *argu, int sd_level)
{
	int energy_cost = 0;
	struct sched_domain *sd;
	const struct sched_group_energy *sge, *_sge, *sge_core, *sge_clus;
#ifdef CONFIG_ARM64
	int cid = cpu_topology[cpu].cluster_id;
#else
	int cid = cpu_topology[cpu].socket_id;
#endif
	struct energy_env *eenv = (struct energy_env *)argu;
	int only_lv1 = 0;
	int cap_idx = eenv->opp_idx[cid];
	int co_buck_cid = -1;

	sd = rcu_dereference_check_sched_domain(cpu_rq(cpu)->sd);

	/* [FIXME] racing with hotplug */
	if (!sd)
		return 0;

	/* [FIXME] racing with hotplug */
	if (cap_idx == -1)
		return 0;

	if (is_share_buck(cid, &co_buck_cid)) {
		cap_idx = max(eenv->opp_idx[cid], eenv->opp_idx[co_buck_cid]);

		mt_sched_printf(sched_eas_energy_calc,
			"[share buck] %s cap_idx=%d is via max_opp(cid%d=%d,cid%d=%d)",
			__func__,
			cap_idx, cid, eenv->opp_idx[cid],
			co_buck_cid, eenv->opp_idx[co_buck_cid]);
	}

	sge = sd->groups->sge;

	sge = _sge = sge_core = sge_clus = NULL;

	/* To handle only 1 CPU in cluster by HPS */
	if (unlikely(!sd->child &&
	   (rcu_dereference(per_cpu(sd_scs, cpu)) == NULL))) {
		sge_core = cpu_core_energy(cpu);
		sge_clus = cpu_cluster_energy(cpu);

		only_lv1 = 1;
	} else {
		if (sd_level == 0)
			_sge = cpu_core_energy(cpu); /* for cpu */
		else
			_sge = cpu_cluster_energy(cpu); /* for cluster */
	}

	/* [DEBUG ] wfi always??? */
	idle_state = 0;

	switch (idle_state) {
	case 0: /* active idle: WFI */
		if (only_lv1) {
			struct upower_tbl_row *cpu_pwr_tbl, *clu_pwr_tbl;

			cpu_pwr_tbl = &sge_core->cap_states[cap_idx];
			clu_pwr_tbl = &sge_clus->cap_states[cap_idx];

			/* idle: core->leask_power + cluster->lkg_pwr */
			energy_cost = cpu_pwr_tbl->lkg_pwr[sge_core->lkg_idx] +
					clu_pwr_tbl->lkg_pwr[sge_clus->lkg_idx];

			mt_sched_printf(sched_eas_energy_calc,
				"%s: %s lv=%d tlb_cpu[%d].leak=%d tlb_clu[%d].leak=%d total=%d",
				__func__, "WFI", sd_level,
				cap_idx,
				cpu_pwr_tbl->lkg_pwr[sge_core->lkg_idx],
				cap_idx,
				clu_pwr_tbl->lkg_pwr[sge_clus->lkg_idx],
				energy_cost);
		} else {
			struct upower_tbl_row *pwr_tbl;

			pwr_tbl =  &_sge->cap_states[cap_idx];
			energy_cost = pwr_tbl->lkg_pwr[_sge->lkg_idx];

			mt_sched_printf(sched_eas_energy_calc,
				"%s: %s lv=%d tlb[%d].leak=%d total=%d",
				__func__, "WFI", sd_level,
				cap_idx,
				pwr_tbl->lkg_pwr[_sge->lkg_idx],
				energy_cost);
		}

	break;
	case 6: /* SPARK: */
		if (only_lv1) {
			/* idle: core->idle_power[spark] +
			 *           cluster->idle_power[spark]
			 */
			energy_cost = sge_core->idle_states[idle_state].power +
				sge_clus->idle_states[idle_state].power;

			mt_sched_printf(sched_eas_energy_calc,
				"%s: %s lv=%d idle_cpu[%d].power=%ld idle_clu[%d].power=(%ld) total=%d",
				__func__, "SPARK", sd_level,
				idle_state,
				sge_core->idle_states[idle_state].power,
				idle_state,
				sge_clus->idle_states[idle_state].power,
				energy_cost);
		} else {
			energy_cost = _sge->idle_states[idle_state].power;
			mt_sched_printf(sched_eas_energy_calc,
				"%s: %s lv=%d idle[%d].power=(%ld) total=%d",
				__func__, "SPARK", sd_level,
				idle_state, _sge->idle_states[idle_state].power,
				energy_cost);
		}

	break;
	case 4: /* MCDI: */
		energy_cost = 0;

		mt_sched_printf(sched_eas_energy_calc,
			"%s: %s idle:%d total=%d", __func__, "MCDI",
			idle_state, energy_cost);
	break;
	default: /* SODI, deep_idle: */
		energy_cost = 0;

		mt_sched_printf(sched_eas_energy_calc,
			"%s: unknown idle state=%d\n", __func__,  idle_state);
	break;
	}

	return energy_cost;
}

inline
int mtk_busy_power(int cpu, void *argu, int sd_level)
{
	struct energy_env *eenv = (struct energy_env *)argu;
	struct sched_domain *sd;
	const struct sched_group_energy *sge;
	int energy_cost = 0;
#ifdef CONFIG_ARM64
	int cid = cpu_topology[cpu].cluster_id;
#else
	int cid = cpu_topology[cpu].socket_id;
#endif
	int cap_idx = eenv->opp_idx[cid];
	int co_buck_cid = -1;
	unsigned long int volt_factor = 1;
	int shared = 0;

	sd = rcu_dereference_check_sched_domain(cpu_rq(cpu)->sd);
	/* [FIXME] racing with hotplug */
	if (!sd)
		return 0;

	/* [FIXME] racing with hotplug */
	if (cap_idx == -1)
		return 0;

	sge = sd->groups->sge;

	if (is_share_buck(cid, &co_buck_cid)) {

		mt_sched_printf(sched_eas_energy_calc,
			"[share buck] cap_idx of clu%d=%d cap_idx of clu%d=%d",
			cid, eenv->opp_idx[cid], co_buck_cid,
			eenv->opp_idx[co_buck_cid]);
		shared = 1;
	}

	if (unlikely(!sd->child &&
		(rcu_dereference(per_cpu(sd_scs, cpu)) == NULL))) {
		/* fix HPS defeats: only one CPU in this cluster */
		const struct sched_group_energy *sge_core;
		const struct sched_group_energy *sge_clus;
		int co_cap_idx = eenv->opp_idx[co_buck_cid];

		sge_core = cpu_core_energy(cpu);
		sge_clus = cpu_cluster_energy(cpu);

		if (shared && (co_cap_idx > cap_idx)) {
			unsigned long v_max;
			unsigned long v_min;
			unsigned long clu_dyn_pwr, cpu_dyn_pwr;
			unsigned long clu_lkg_pwr, cpu_lkg_pwr;
			struct upower_tbl_row *cpu_pwr_tbl, *clu_pwr_tbl;

			v_max = sge_core->cap_states[co_cap_idx].volt;
			v_min = sge_core->cap_states[cap_idx].volt;

			/*
			 * dynamic power = F*V^2
			 *
			 * dyn_pwr  = current_power * (v_max/v_min)^2
			 * lkg_pwr = tlb[idx of v_max].leak;
			 *
			 */
			volt_factor = ((v_max*v_max) << VOLT_SCALE)
						/ (v_min*v_min);

			cpu_dyn_pwr = sge_core->cap_states[cap_idx].dyn_pwr;
			clu_dyn_pwr = sge_clus->cap_states[cap_idx].dyn_pwr;

			energy_cost = ((cpu_dyn_pwr+clu_dyn_pwr)*volt_factor)
						>> VOLT_SCALE;

			/* + leak power of co_buck_cid's opp */
			cpu_pwr_tbl = &sge_core->cap_states[co_cap_idx];
			clu_pwr_tbl = &sge_clus->cap_states[co_cap_idx];
			cpu_lkg_pwr = cpu_pwr_tbl->lkg_pwr[sge_core->lkg_idx];
			clu_lkg_pwr = clu_pwr_tbl->lkg_pwr[sge_clus->lkg_idx];
			energy_cost += (cpu_lkg_pwr + clu_lkg_pwr);

			mt_sched_printf(sched_eas_energy_calc,
				"%s: %s lv=%d tlb[%d].dyn_pwr=(cpu:%d,clu:%d) tlb[%d].leak=(cpu:%d,clu:%d) vlt_f=%ld",
				__func__, "share_buck/only1CPU", sd_level,
				cap_idx,
				sge_core->cap_states[cap_idx].dyn_pwr,
				sge_clus->cap_states[cap_idx].dyn_pwr,
				co_cap_idx,
				cpu_pwr_tbl->lkg_pwr[sge_core->lkg_idx],
				clu_pwr_tbl->lkg_pwr[sge_clus->lkg_idx],
				volt_factor);
			mt_sched_printf(sched_eas_energy_calc,
				"%s: %s total=%d",
				__func__, "share_buck/only1CPU", energy_cost);
		} else {
			struct upower_tbl_row *cpu_pwr_tbl, *clu_pwr_tbl;

			cpu_pwr_tbl = &sge_core->cap_states[cap_idx];
			clu_pwr_tbl = &sge_clus->cap_states[cap_idx];

			energy_cost = cpu_pwr_tbl->dyn_pwr +
					cpu_pwr_tbl->lkg_pwr[sge_core->lkg_idx];

			energy_cost += clu_pwr_tbl->dyn_pwr +
					clu_pwr_tbl->lkg_pwr[sge_clus->lkg_idx];

			mt_sched_printf(sched_eas_energy_calc,
				"%s: %s lv=%d tlb_core[%d].dyn_pwr=(%d,%d) tlb_clu[%d]=(%d,%d) total=%d",
				__func__, "only1CPU", sd_level,
				cap_idx,
				cpu_pwr_tbl->dyn_pwr,
				cpu_pwr_tbl->lkg_pwr[sge_core->lkg_idx],
				cap_idx,
				clu_pwr_tbl->dyn_pwr,
				clu_pwr_tbl->lkg_pwr[sge_clus->lkg_idx],
				energy_cost);
		}
	} else {
		const struct sched_group_energy *_sge;
		int co_cap_idx = eenv->opp_idx[co_buck_cid];

		if (sd_level == 0)
			_sge = cpu_core_energy(cpu); /* for CPU */
		else
			_sge = cpu_cluster_energy(cpu); /* for cluster */

		if (shared && (co_cap_idx > cap_idx)) {
			/*
			 * calculated power with share-buck impact
			 *
			 * dynamic power = F*V^2
			 *
			 * dyn_pwr  = current_power * (v_max/v_min)^2
			 * lkg_pwr = tlb[idx of v_max].leak;
			 */
			unsigned long v_max = _sge->cap_states[co_cap_idx].volt;
			unsigned long v_min = _sge->cap_states[cap_idx].volt;
			unsigned long dyn_pwr;
			unsigned long lkg_pwr;
			int lkg_idx = _sge->lkg_idx;

			volt_factor = ((v_max*v_max) << VOLT_SCALE) /
					(v_min*v_min);

			dyn_pwr = (_sge->cap_states[cap_idx].dyn_pwr *
					volt_factor) >> VOLT_SCALE;
			lkg_pwr = _sge->cap_states[co_cap_idx].lkg_pwr[lkg_idx];
			energy_cost = dyn_pwr + lkg_pwr;

			mt_sched_printf(sched_eas_energy_calc,
				"%s: %s lv=%d  tlb[%d].pwr=%d volt_f=%ld buck.pwr=%ld tlb[%d].leak=(%d) total=%d",
				__func__, "share_buck", sd_level,
				cap_idx,
				_sge->cap_states[cap_idx].dyn_pwr,
				volt_factor, dyn_pwr,
				co_cap_idx,
				_sge->cap_states[co_cap_idx].lkg_pwr[lkg_idx],
				energy_cost);

		} else {
			/* No share buck impact */
			unsigned long dyn_pwr;
			unsigned long lkg_pwr;
			int lkg_idx = _sge->lkg_idx;

			dyn_pwr = _sge->cap_states[cap_idx].dyn_pwr;
			lkg_pwr = _sge->cap_states[cap_idx].lkg_pwr[lkg_idx];
			energy_cost = dyn_pwr + lkg_pwr;

			mt_sched_printf(sched_eas_energy_calc,
				"%s: lv=%d tlb[%d].pwr=%ld tlb[%d].leak=%ld total=%d",
				__func__, sd_level,
				cap_idx, dyn_pwr,
				cap_idx, lkg_pwr,
				energy_cost);
		}
	}

	return energy_cost;
}
#endif
