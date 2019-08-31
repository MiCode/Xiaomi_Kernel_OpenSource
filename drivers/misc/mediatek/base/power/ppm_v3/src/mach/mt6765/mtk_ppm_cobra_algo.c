/*
 * Copyright (C) 2016 MediaTek Inc.
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

#include <linux/slab.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/cpumask.h>
#include <linux/topology.h>

#include "mtk_ppm_platform.h"
#include "mtk_ppm_internal.h"
#ifdef CONFIG_MTK_UNIFY_POWER
#include "mtk_upower.h"
#endif


struct ppm_cobra_data cobra_tbl;
struct ppm_cobra_lookup cobra_lookup_data;

static int Core_limit[NR_PPM_CLUSTERS] = {CORE_LL, CORE_L};
static int cobra_init_done;

#define ACT_CORE(cluster)	(active_core[PPM_CLUSTER_##cluster])
#define CORE_LIMIT(cluster)	(core_limit_tmp[PPM_CLUSTER_##cluster])


static int get_delta_pwr_LxLL(unsigned int L_core, unsigned int LL_core,
	unsigned int opp, int prefer)
{
#if PPM_COBRA_RUNTIME_CALC_DELTA
	struct ppm_cobra_data *p = &cobra_tbl;
	unsigned int idx_L = get_cluster_max_cpu_core(PPM_CLUSTER_LL);
	unsigned int idx_LL = 0;
	unsigned int cur_opp, cur_pwr, prev_pwr;
	int delta_pwr;
#endif

	if (L_core > get_cluster_max_cpu_core(PPM_CLUSTER_L)
		|| LL_core > get_cluster_max_cpu_core(PPM_CLUSTER_LL)
		|| opp > get_cluster_min_cpufreq_idx(PPM_CLUSTER_L)) {
		ppm_err("%s: Invalid input: L_core=%d, LL_core=%d, opp=%d\n",
			__func__, L_core, LL_core, opp);
		WARN_ON(1);
	}

#if PPM_COBRA_RUNTIME_CALC_DELTA
	if (L_core == 0 && LL_core == 0)
		return 0;

	cur_opp = opp;
	cur_pwr = (L_core)
		? p->ptbl[idx_L+L_core-1][cur_opp].power_idx
		: 0; /* L */
	cur_pwr += (LL_core)
		? p->ptbl[idx_LL+LL_core-1][cur_opp].power_idx
		: 0; /* L+LL */

	if (opp == COBRA_OPP_NUM - 1) {
		if (prefer == PPM_CLUSTER_LL) {
			prev_pwr = (LL_core) ? ((LL_core > 1)
			? (p->ptbl[idx_LL+LL_core-2][cur_opp].power_idx +
			((L_core)
			? p->ptbl[idx_L+L_core-1][cur_opp].power_idx : 0))
			: ((L_core)
			? p->ptbl[idx_L+L_core-1][cur_opp].power_idx : 0))
			: ((L_core > 1)
			? p->ptbl[idx_L+L_core-2][cur_opp].power_idx : 0);
		} else {
			prev_pwr = (L_core) ? ((L_core > 1)
			? (p->ptbl[idx_L+L_core-2][cur_opp].power_idx +
			((LL_core)
			? p->ptbl[idx_LL+LL_core-1][cur_opp].power_idx : 0))
			: ((LL_core)
			? p->ptbl[idx_LL+LL_core-1][cur_opp].power_idx : 0))
			: ((LL_core > 1)
			? p->ptbl[idx_LL+LL_core-2][cur_opp].power_idx : 0);
		}
	} else {
		unsigned int prev_opp = opp + 1;

		prev_pwr = (L_core)
			? p->ptbl[idx_L+L_core-1][prev_opp].power_idx
			: 0;
		prev_pwr += (LL_core)
			? p->ptbl[idx_LL+LL_core-1][prev_opp].power_idx
			: 0;
	}

	delta_pwr = cur_pwr - prev_pwr;

	return delta_pwr;
#else
	return cobra_tbl.delta_tbl[L_core][LL_core][opp].delta_pwr;
#endif
}

void ppm_cobra_update_core_limit(unsigned int cluster, int limit)
{
	if (cluster >= NR_PPM_CLUSTERS) {
		ppm_err("%s: Invalid cluster id = %d\n", __func__, cluster);
		WARN_ON(1);
		return;
	}

	if (limit < 0 || limit > get_cluster_max_cpu_core(cluster)) {
		ppm_err("%s: Invalid core limit for cluster%d = %d\n",
			__func__, cluster, limit);
		WARN_ON(1);
		return;
	}

	Core_limit[cluster] = limit;
}

void ppm_cobra_update_freq_limit(unsigned int cluster, int limit)
{
	/* NO NEED */
}

/* use global variable to avoid stack overflow */
static int f_ex_core[NR_PPM_CLUSTERS];
static int ignore_ex_core;

void ppm_cobra_update_limit(void *user_req)
{
	struct ppm_policy_req *req;
	int power_budget;
	int opp[NR_PPM_CLUSTERS];
	int active_core[NR_PPM_CLUSTERS];
#if PPM_COBRA_USE_CORE_LIMIT
	int core_limit_tmp[NR_PPM_CLUSTERS];
	int max_thro_core[NR_PPM_CLUSTERS];
#endif
	int i;
#ifndef NO_SCHEDULE_API
	struct cpumask cluster_cpu, online_cpu;
#endif
	int delta_power;
	int LxLL;
	/* Get power index of current OPP */
	int curr_power = 0;
	struct ppm_cluster_status cl_status[NR_PPM_CLUSTERS];

	/* skip if DVFS is not ready (we cannot get current freq...) */
	/* skip if COBRA is not init yet */
	if (!ppm_main_info.client_info[PPM_CLIENT_DVFS].limit_cb
		|| !cobra_init_done)
		return;

	if (!user_req)
		return;

	req = (struct ppm_policy_req *)user_req;
	power_budget = req->power_budget;

	if (power_budget >= ppm_get_max_pwr_idx())
		return;

	ppm_dbg(COBRA, "[PREV]Core_Limit=%d%d, policy_limit=%d%d\n",
			Core_limit[PPM_CLUSTER_LL],
			Core_limit[PPM_CLUSTER_L],
			req->limit[PPM_CLUSTER_LL].max_cpu_core,
			req->limit[PPM_CLUSTER_L].max_cpu_core);

	for_each_ppm_clusters(i) {
#ifndef NO_SCHEDULE_API
		ppm_get_cl_cpus(&cluster_cpu, i);
		cpumask_and(&online_cpu, &cluster_cpu, cpu_online_mask);

		cl_status[i].core_num = cpumask_weight(&online_cpu);
#else
		cl_status[i].core_num = get_cluster_max_cpu_core(i);
#endif
		cl_status[i].volt = 0;	/* don't care */
		if (!cl_status[i].core_num)
			cl_status[i].freq_idx = -1;
		else
			cl_status[i].freq_idx = ppm_main_freq_to_idx(i,
				mt_cpufreq_get_cur_phy_freq_no_lock(i),
				CPUFREQ_RELATION_L);

		ppm_ver("[%d] core = %d, freq_idx = %d\n",
			i, cl_status[i].core_num, cl_status[i].freq_idx);
	}

#if PPM_COBRA_USE_CORE_LIMIT
	for_each_ppm_clusters(i) {
		if (cl_status[i].core_num > Core_limit[i])
			cl_status[i].core_num = Core_limit[i];
		if (req->limit[i].max_cpu_core < Core_limit[i])
			Core_limit[i] = req->limit[i].max_cpu_core;
	}
#endif

	if (cl_status[PPM_CLUSTER_LL].core_num == 0
		&& cl_status[PPM_CLUSTER_L].core_num == 0) {
		if (Core_limit[PPM_CLUSTER_LL] > 0) {
			cl_status[PPM_CLUSTER_LL].core_num = 1;
			cl_status[PPM_CLUSTER_LL].freq_idx =
				get_cluster_max_cpufreq_idx(PPM_CLUSTER_LL);
		} else {
			cl_status[PPM_CLUSTER_L].core_num = 1;
			cl_status[PPM_CLUSTER_L].freq_idx =
				get_cluster_max_cpufreq_idx(PPM_CLUSTER_L);
		}
	}


	/* use L cluster frequency */
	if (cl_status[PPM_CLUSTER_L].core_num > 0)
		cl_status[PPM_CLUSTER_LL].freq_idx =
			cl_status[PPM_CLUSTER_L].freq_idx;

	curr_power = ppm_find_pwr_idx(cl_status);
	if (curr_power < 0)
		curr_power = mt_ppm_thermal_get_max_power();
	delta_power = power_budget - curr_power;

	for_each_ppm_clusters(i) {
		opp[i] = cl_status[i].freq_idx;

		/* Get Active Core number of each cluster */
		active_core[i] = (cl_status[i].core_num >= 0)
			? cl_status[i].core_num : 0;

#if PPM_COBRA_USE_CORE_LIMIT
		core_limit_tmp[i] = Core_limit[i];
		req->limit[i].max_cpu_core = core_limit_tmp[i];
#ifndef NO_SCHEDULE_API
		{
			cpumask_var_t cpumask_cluster, target;

			/* calculate exclusive core num for each cluster */
			arch_get_cluster_cpus(cpumask_cluster, i);
			cpumask_and(target, cpumask_cluster,
				ppm_main_info.exclusive_core);
			max_thro_core[i] =
				(active_core[i] > cpumask_weight(target))
				? (active_core[i]-cpumask_weight(target)) : 0;
		}
#else
		max_thro_core[i] = get_cluster_max_cpu_core(i);
#endif
#endif
	}

#if PPM_COBRA_USE_CORE_LIMIT
	ppm_dbg(COBRA, "[COBRA] max throttle core num = %d,%d\n",
		max_thro_core[PPM_CLUSTER_LL],
		max_thro_core[PPM_CLUSTER_L]);
#endif

	/* Which Cluster in L and LL is active (1: L is on, 0: LL is on) */
	LxLL = (ACT_CORE(L) > 0) ? PPM_CLUSTER_L : PPM_CLUSTER_LL;

	ppm_dbg(COBRA,
		"[IN](bgt/del/cur)=(%d/%d/%d),(opp/act/c)=(%d,%d/%d%d/%d%d)\n",
		power_budget, delta_power, curr_power,
		opp[PPM_CLUSTER_LL], opp[PPM_CLUSTER_L],
		ACT_CORE(LL), ACT_CORE(L),
		CORE_LIMIT(LL), CORE_LIMIT(L));

	/* increase ferquency limit */
	if (delta_power >= 0) {
		while (1) {
			int ChoosenCl = -1, ChoosenPwr = 0;
			int target_delta_pwr;

			/* LxLL Cluster */
			if (opp[LxLL] > 0) {
				target_delta_pwr =
					get_delta_pwr_LxLL(
						ACT_CORE(L), ACT_CORE(LL),
						opp[LxLL]-1, LxLL);
				if (delta_power >= target_delta_pwr) {
					ChoosenCl = 1;
					ChoosenPwr = target_delta_pwr;

					if (ACT_CORE(L) > 0)
						opp[PPM_CLUSTER_L] -= 1;
					if (ACT_CORE(LL) > 0)
						opp[PPM_CLUSTER_LL] -= 1;
				}
			}

			if (ChoosenCl != -1)
				goto prepare_next_round;

#if PPM_COBRA_USE_CORE_LIMIT
			/* no enough budget */
			if (opp[LxLL] != 0)
				goto end;

			/* give budget to LL */
			while (CORE_LIMIT(LL) <
				get_cluster_max_cpu_core(PPM_CLUSTER_LL)) {
				target_delta_pwr =
					get_delta_pwr_LxLL(
						ACT_CORE(L),
						CORE_LIMIT(LL)+1,
						COBRA_OPP_NUM-1, LxLL);
				if (delta_power < target_delta_pwr)
					break;

				delta_power -= target_delta_pwr;
				req->limit[PPM_CLUSTER_LL].max_cpu_core =
					++CORE_LIMIT(LL);
			}

			/* give budget to L */
			while (CORE_LIMIT(L) <
				get_cluster_max_cpu_core(PPM_CLUSTER_L)) {
				target_delta_pwr =
					get_delta_pwr_LxLL(CORE_LIMIT(L)+1,
						ACT_CORE(LL),
						COBRA_OPP_NUM-1, LxLL);
				if (delta_power < target_delta_pwr)
					break;

				delta_power -= target_delta_pwr;
				req->limit[PPM_CLUSTER_L].max_cpu_core =
					++CORE_LIMIT(L);
			}

end:
#endif
			ppm_dbg(COBRA,
				"+ChoosenCl=-1!del=%d,(opp/c)=(%d,%d/%d%d)\n",
				delta_power, opp[PPM_CLUSTER_LL],
				opp[PPM_CLUSTER_L],
				CORE_LIMIT(LL), CORE_LIMIT(L));

			break;

prepare_next_round:
			delta_power -= ChoosenPwr;

			ppm_dbg(COBRA,
				"+(delta/Cl/Pwr)=(%d,%d,%d), opp=%d,%d\n",
				delta_power, ChoosenCl, ChoosenPwr,
				opp[PPM_CLUSTER_LL], opp[PPM_CLUSTER_L]);
		}
	} else {
		/* init global variables */
		for_each_ppm_clusters(i)
			f_ex_core[i] = (active_core[i]) ? 0 : 1;
		ignore_ex_core = 0;

		while (delta_power < 0) {
			int ChoosenCl = -1;
			int ChoosenPwr = 0;

			ppm_dbg(COBRA,
				"exclusive_core_flag = %d,%d(LxLL=%d)\n",
				f_ex_core[PPM_CLUSTER_LL],
				f_ex_core[PPM_CLUSTER_L], LxLL);

			/* LxLL */
			if (opp[LxLL] >= 0
				&& opp[LxLL] < PPM_COBRA_MAX_FREQ_IDX
				&& !f_ex_core[LxLL]) {
				ChoosenCl = 1;
				ChoosenPwr = get_delta_pwr_LxLL(ACT_CORE(L),
					ACT_CORE(LL), opp[LxLL], LxLL);

				if (ACT_CORE(L) > 0
					&& opp[PPM_CLUSTER_L]
					< PPM_COBRA_MAX_FREQ_IDX)
					opp[PPM_CLUSTER_L] += 1;
				if (ACT_CORE(LL) > 0
					&& opp[PPM_CLUSTER_LL]
					< PPM_COBRA_MAX_FREQ_IDX)
					opp[PPM_CLUSTER_LL] += 1;
			}

			if (ChoosenCl == -1) {
				ppm_err("bg/dp/cp=%d/%d/%d,op/c=%d%d/%d%d\n",
					power_budget, delta_power, curr_power,
					opp[PPM_CLUSTER_LL],
					opp[PPM_CLUSTER_L],
					ACT_CORE(LL), ACT_CORE(L));
				break;
			}

#if PPM_COBRA_USE_CORE_LIMIT
			/* Turned off core */
			if (opp[LxLL] == PPM_COBRA_MAX_FREQ_IDX) {
				if (!ignore_ex_core
					&& max_thro_core[LxLL] <= 0) {
					f_ex_core[LxLL] = 1;
					opp[PPM_CLUSTER_L] =
						(ACT_CORE(L) > 0)
						? opp[PPM_CLUSTER_L] - 1
						: opp[PPM_CLUSTER_L];
					opp[PPM_CLUSTER_LL] =
						(ACT_CORE(LL) > 0)
						? opp[PPM_CLUSTER_LL] - 1
						: opp[PPM_CLUSTER_LL];
					/* check LL next round */
					LxLL = (LxLL == PPM_CLUSTER_L)
						? PPM_CLUSTER_LL : LxLL;
					goto check_exclusive_core_flag;
				}

				if ((ignore_ex_core ||
					!f_ex_core[PPM_CLUSTER_L])
					&& (ACT_CORE(L) > 1
					|| (ACT_CORE(LL) > 0
					&& ACT_CORE(L) > 0))) {
					req->limit[PPM_CLUSTER_L].max_cpu_core
						= --ACT_CORE(L);
					max_thro_core[PPM_CLUSTER_L]--;
					if (!ignore_ex_core
						&& max_thro_core[PPM_CLUSTER_L]
						<= 0) {
						f_ex_core[PPM_CLUSTER_L] = 1;
						LxLL = PPM_CLUSTER_LL;
					}
				} else if ((ignore_ex_core
					|| !f_ex_core[PPM_CLUSTER_LL])
					&& ACT_CORE(LL) > 1) {
					req->limit[PPM_CLUSTER_LL].max_cpu_core
						= --ACT_CORE(LL);
					max_thro_core[PPM_CLUSTER_LL]--;
					if (!ignore_ex_core &&
						max_thro_core[PPM_CLUSTER_LL]
						<= 0)
						f_ex_core[PPM_CLUSTER_LL] = 1;
				}
				if (ACT_CORE(L) > 0)
					opp[PPM_CLUSTER_L] =
						PPM_COBRA_MAX_FREQ_IDX - 1;
				else
					LxLL = PPM_CLUSTER_LL;
				if (ACT_CORE(LL) > 0)
					opp[PPM_CLUSTER_LL] =
						PPM_COBRA_MAX_FREQ_IDX - 1;
			}
#endif

			delta_power += ChoosenPwr;
			curr_power -= ChoosenPwr;

check_exclusive_core_flag:
			/* if all non-exclusive core are throttled */
			/* but delta_pwr is still < 0, we have to */
			/* throttle exclusive core next round */
			if (f_ex_core[PPM_CLUSTER_LL]
				&& f_ex_core[PPM_CLUSTER_L]) {
				ignore_ex_core = 1;
				f_ex_core[PPM_CLUSTER_LL] = 0;
				f_ex_core[PPM_CLUSTER_L] = 0;
				if (ACT_CORE(L) > 0)
					/* re-scan from L */
					LxLL = PPM_CLUSTER_L;
			}

			ppm_dbg(COBRA,
				"-delta/Cl/P=%d,%d,%d! opp/act=%d,%d/%d%d\n",
				delta_power, ChoosenCl, ChoosenPwr,
				opp[PPM_CLUSTER_LL], opp[PPM_CLUSTER_L],
				ACT_CORE(LL), ACT_CORE(L));
		}
	}

	/* Set frequency limit */
	/* For non share buck */
#if 0
	if (opp[PPM_CLUSTER_LL] >= 0 && ACT_CORE(LL) > 0)
		req->limit[PPM_CLUSTER_LL].max_cpufreq_idx =
			freq_idx_mapping_tbl[opp[PPM_CLUSTER_LL]];
	if (opp[PPM_CLUSTER_L] >= 0 && ACT_CORE(L) > 0)
		req->limit[PPM_CLUSTER_L].max_cpufreq_idx =
			freq_idx_mapping_tbl[opp[PPM_CLUSTER_L]];
#endif

	/* Set all frequency limit of the cluster */
	/* Set OPP of Cluser n to opp[n] */
	for_each_ppm_clusters(i) {
		req->limit[i].max_cpufreq_idx = opp[LxLL];
	}

	ppm_dbg(COBRA,
		"[OUT]delta=%d,opp/act/c_lmt/f_lmt=%d,%d/%d%d/%d%d/%d,%d\n",
		delta_power,
		opp[PPM_CLUSTER_LL], opp[PPM_CLUSTER_L],
		ACT_CORE(LL), ACT_CORE(L),
		req->limit[PPM_CLUSTER_LL].max_cpu_core,
		req->limit[PPM_CLUSTER_L].max_cpu_core,
		req->limit[PPM_CLUSTER_LL].max_cpufreq_idx,
		req->limit[PPM_CLUSTER_L].max_cpufreq_idx);

	/* error check */
	for_each_ppm_clusters(i) {
		if (req->limit[i].max_cpufreq_idx >
			req->limit[i].min_cpufreq_idx)
			req->limit[i].min_cpufreq_idx =
				req->limit[i].max_cpufreq_idx;
		if (req->limit[i].max_cpu_core <
			req->limit[i].min_cpu_core)
			req->limit[i].min_cpu_core =
				req->limit[i].max_cpu_core;
	}
}

void ppm_cobra_init(void)
{
	struct ppm_cobra_data *p = &cobra_tbl;
	int i, j;
#if !PPM_COBRA_RUNTIME_CALC_DELTA
	int k;
#endif

#ifdef CONFIG_MTK_UNIFY_POWER
	{
		unsigned int core, dyn, lkg, dyn_c, lkg_c, cap;

		/* generate basic power table */
		ppm_info("basic power table:\n");
		for (i = 0; i < TOTAL_CORE_NUM; i++) {
			for (j = 0; j < DVFS_OPP_NUM; j++) {
				core = (i % 4) + 1;
				dyn = upower_get_power(i/4, j, UPOWER_DYN);
				lkg = upower_get_power(i/4, j, UPOWER_LKG);
				dyn_c = upower_get_power(i/4+NR_PPM_CLUSTERS,
					j, UPOWER_DYN);
				lkg_c = upower_get_power(i/4+NR_PPM_CLUSTERS,
					j, UPOWER_LKG);
				cap = upower_get_power(i/4,
					j, UPOWER_CPU_STATES);

				p->ptbl[i][j].power_idx =
					((dyn + lkg) * core + (dyn_c + lkg_c))
					/ 1000;
				p->ptbl[i][j].perf_idx = cap * core;

				ppm_info("[%d][%d] = (%d, %d)\n", i, j,
					p->ptbl[i][j].power_idx,
					p->ptbl[i][j].perf_idx);
			}
		}
	}
#else
	for (i = 0; i < TOTAL_CORE_NUM; i++) {
		for (j = 0; j < DVFS_OPP_NUM; j++) {
			p->ptbl[i][j].power_idx = 0;
			p->ptbl[i][j].perf_idx = 0;
		}
	}
#endif

#if !PPM_COBRA_RUNTIME_CALC_DELTA
	/* generate delta power and delta perf table for LxLL */
	ppm_info("LxLL delta table:\n");
	for (i = 0; i <= get_cluster_max_cpu_core(PPM_CLUSTER_L); i++) {
		for (j = 0;
			j <= get_cluster_max_cpu_core(PPM_CLUSTER_LL); j++) {
			for (k = 0; k < COBRA_OPP_NUM; k++) {
				int idx_L = get_cluster_max_cpu_core(
					PPM_CLUSTER_LL);
				int idx_LL = 0;
				int opp = k;
				int cur_pwr, prev_pwr, pre_op;

				if (i == 0 && j == 0) {
					p->delta_tbl[i][j][k].delta_pwr	= 0;

					ppm_info("[%d][%d][%d] = (0)\n",
						i, j, k);
					continue;
				}

				cur_pwr = (i)
					? p->ptbl[idx_L+i-1][opp].power_idx
					: 0; /* L */
				cur_pwr += (j)
					? p->ptbl[idx_LL+j-1][opp].power_idx
					: 0; /* L+LL */

				if (k == COBRA_OPP_NUM - 1) {
					prev_pwr = (i) ?
					((i > 1)
					? (p->ptbl[idx_L+i-2][opp].power_idx +
					((j)
					? p->ptbl[idx_LL+j-1][opp].power_idx
					: 0))
					: ((j)
					? p->ptbl[idx_LL+j-1][opp].power_idx
					: 0))
					: ((j > 1)
					? p->ptbl[idx_LL+j-2][opp].power_idx
					: 0);

					p->delta_tbl[i][j][k].delta_pwr =
						cur_pwr - prev_pwr;
				} else {
					pre_op = k+1;
					prev_pwr = (i)
					? p->ptbl[idx_L+i-1][pre_op].power_idx
					: 0;
					prev_pwr += (j)
					? p->ptbl[idx_LL+j-1][pre_op].power_idx
					: 0;

					p->delta_tbl[i][j][k].delta_pwr =
						cur_pwr - prev_pwr;
				}

				ppm_info("[%d][%d][%d] = (%d)\n", i, j, k,
					p->delta_tbl[i][j][k].delta_pwr);
			}
		}
	}
#endif

	cobra_init_done = 1;

	ppm_info("COBRA init done!\n");
}


void ppm_cobra_dump_tbl(struct seq_file *m)
{
#if 1
	struct ppm_cluster_status cluster_status[NR_PPM_CLUSTERS];
	int i;
	int power;

	seq_puts(m, "\n========================================================\n");
	seq_puts(m, "(LL_core, LL_opp, L_core, L_opp) = power");
	seq_puts(m, "\n========================================================\n");

	/* only list power for all core online case */
	for_each_ppm_clusters(i)
		cluster_status[i].core_num = get_cluster_max_cpu_core(i);

	for (i = 0; i < DVFS_OPP_NUM; i++) {
		/* same index due to share buck */
		cluster_status[PPM_CLUSTER_LL].freq_idx = i;
		cluster_status[PPM_CLUSTER_L].freq_idx = i;

		power = ppm_find_pwr_idx(cluster_status);

		if (power) {
			seq_printf(m, "(%d, %2d, %d, %2d) = %4d\n",
				cluster_status[PPM_CLUSTER_LL].core_num,
				cluster_status[PPM_CLUSTER_LL].freq_idx,
				cluster_status[PPM_CLUSTER_L].core_num,
				cluster_status[PPM_CLUSTER_L].freq_idx,
				power
			);
		}
	}
#else
	int i, j, k;

	seq_puts(m, "\n==========================================\n");
	seq_puts(m, "basic power table (pwr, perf)");
	seq_puts(m, "\n==========================================\n");
	for (i = 0; i < TOTAL_CORE_NUM; i++) {
		for (j = 0; j < DVFS_OPP_NUM; j++) {
			seq_printf(m, "[%d][%d] = (%d, %d)\n", i, j,
				cobra_tbl.ptbl[i][j].power_idx,
				cobra_tbl.ptbl[i][j].perf_idx);
		}
	}

	if (ppm_debug <= 0)
		return;

	seq_puts(m, "\n============================================\n");
	seq_puts(m, "LxLL delta table (delta_pwr)");
	seq_puts(m, "\n============================================\n");
	for (i = 0; i <= get_cluster_max_cpu_core(PPM_CLUSTER_L); i++) {
		for (j = 0;
			j <= get_cluster_max_cpu_core(PPM_CLUSTER_LL); j++) {
			for (k = 0; k < COBRA_OPP_NUM; k++) {
				seq_printf(m, "[%d][%d][%d] = (%d)\n", i, j, k,
					get_delta_pwr_LxLL(i, j, k,
						PPM_CLUSTER_L));
			}
		}
	}
#endif
}

static unsigned int get_limit_opp_and_budget(void)
{
	unsigned int power = 0;
	int i, j, idx;

	for (i = 0; i <= get_cluster_min_cpufreq_idx(PPM_CLUSTER_LL); i++) {
		cobra_lookup_data.limit[PPM_CLUSTER_LL].opp = i;
		cobra_lookup_data.limit[PPM_CLUSTER_L].opp = i;

		for_each_ppm_clusters(j) {
			if (!cobra_lookup_data.limit[j].core)
				continue;

			idx = j * 4 + cobra_lookup_data.limit[j].core - 1;
			power += cobra_tbl.ptbl[idx][i].power_idx;
		}

		if (power <= cobra_lookup_data.budget)
			return power;

		power = 0;
	}

	return power;
}

static void ppm_cobra_lookup_by_budget(struct seq_file *m)
{
	struct ppm_cobra_lookup *p = &cobra_lookup_data;
	int i, j;
	unsigned int power;

	seq_puts(m, "\n=================================================\n");
	seq_puts(m, "(LL_core, LL_opp, L_core, L_opp) = power");
	seq_puts(m, "\n=================================================\n");

	seq_printf(m, "Input budget = %d\n\n", p->budget);

	for (i = get_cluster_max_cpu_core(PPM_CLUSTER_L); i >= 0; i--) {
		for (j = get_cluster_max_cpu_core(PPM_CLUSTER_LL);
			j >= 0; j--) {
			if (!i && !j)
				continue;

			p->limit[PPM_CLUSTER_LL].core = j;
			p->limit[PPM_CLUSTER_L].core = i;
			power = get_limit_opp_and_budget();

			if (power) {
				seq_printf(m, "(%d, %2d, %d, %2d) = %4d\n",
					j, p->limit[PPM_CLUSTER_LL].opp,
					i, p->limit[PPM_CLUSTER_L].opp,
					power);
			}
		}
	}
}

static void ppm_cobra_lookup_by_limit(struct seq_file *m)
{
	struct ppm_cobra_lookup *p = &cobra_lookup_data;
	unsigned int budget = 0, core, opp, i;

	for_each_ppm_clusters(i) {
		core = (p->limit[i].core > get_cluster_max_cpu_core(i))
			? get_cluster_max_cpu_core(i)
			: p->limit[i].core;
		opp = (p->limit[i].opp > get_cluster_min_cpufreq_idx(i))
			? get_cluster_min_cpufreq_idx(i)
			: p->limit[i].opp;

		if (core)
			budget += cobra_tbl.ptbl[4*i+core-1][opp].power_idx;

		seq_printf(m, "Cluster %d: core = %d, opp = %d\n",
			i, core, opp);
	}

	seq_printf(m, "Budget = %d mW\n", budget);
}

void ppm_cobra_lookup_get_result(struct seq_file *m,
	enum ppm_cobra_lookup_type type)
{
	switch (type) {
	case LOOKUP_BY_BUDGET:
		ppm_cobra_lookup_by_budget(m);
		break;
	case LOOKUP_BY_LIMIT:
		ppm_cobra_lookup_by_limit(m);
		break;
	default:
		ppm_err("Invalid lookup type %d\n", type);
		break;
	}
}

