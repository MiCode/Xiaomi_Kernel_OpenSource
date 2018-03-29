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

#define DEBUG 1
/* system includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/string.h>
#include <asm/topology.h>
#include <trace/events/mtk_events.h>

#include "mt_ppm_internal.h"


#define PPM_TIMER_INTERVAL_MS		(1000)

/*==============================================================*/
/* Local function declarition					*/
/*==============================================================*/
static int ppm_main_suspend(struct device *dev);
static int ppm_main_resume(struct device *dev);
static int ppm_main_pdrv_probe(struct platform_device *pdev);
static int ppm_main_pdrv_remove(struct platform_device *pdev);

/*==============================================================*/
/* Global variables						*/
/*==============================================================*/
struct ppm_data ppm_main_info = {
	.is_enabled = true,
	.is_in_suspend = false,

	.cur_mode = PPM_MODE_PERFORMANCE,
	.cur_power_state = PPM_POWER_STATE_NONE,
	.fixed_root_cluster = -1,
	.min_power_budget = ~0,

#ifdef PPM_VPROC_5A_LIMIT_CHECK
	.is_5A_limit_enable = true,
	.is_5A_limit_on = false,
#endif

	.dvfs_tbl_type = DVFS_TABLE_TYPE_FY,

	.ppm_pm_ops = {
		.suspend	= ppm_main_suspend,
		.resume	= ppm_main_resume,
		.freeze	= ppm_main_suspend,
		.thaw	= ppm_main_resume,
		.restore	= ppm_main_resume,
	},
	.ppm_pdev = {
		.name	= "mt-ppm",
		.id	= -1,
	},
	.ppm_pdrv = {
		.probe	= ppm_main_pdrv_probe,
		.remove	= ppm_main_pdrv_remove,
		.driver	= {
			.name	= "mt-ppm",
			.pm	= &ppm_main_info.ppm_pm_ops,
			.owner	= THIS_MODULE,
		},
	},

	.lock = __MUTEX_INITIALIZER(ppm_main_info.lock),
	.policy_list = LIST_HEAD_INIT(ppm_main_info.policy_list),
	.ppm_task = NULL,
	.ppm_wq = __WAIT_QUEUE_HEAD_INITIALIZER(ppm_main_info.ppm_wq),
	.ppm_event = ATOMIC_INIT(0),
};

#ifdef PPM_USE_EFFICIENCY_TABLE
int prev_max_cpufreq_idx[3] = {0, 0, 0};
int Core_limit[3] = {4, 4, 2};

void ppm_main_update_req_by_pwr(enum ppm_power_state new_state, struct ppm_policy_req *req)
{
	int power_budget = req->power_budget;
	/* lookup efficiency table */
	int opp[3];
	int i;
	struct cpumask cluster_cpu, online_cpu;
	int delta_power;
	int activeCoreNumB;
	int activeCoreNumL;
	int activeCoreNumLL;
	int LxLL;
	unsigned int power;
	/* Get power index of current OPP */
	unsigned int curr_power = 0/* = mt_ppm_thermal_get_cur_power()*/;
	struct ppm_cluster_status cluster_status[3];
	struct ppm_cluster_status cluster_status_rebase[3];
	int LxLLisLimited = 0;
#ifdef PPM_EFFICIENCY_TABLE_USE_CORE_LIMIT
	int B_limit;
	int L_limit;
	int LL_limit;
#endif

	/* skip if DVFS is not ready (we cannot get current freq...) */
	if (!ppm_main_info.client_info[PPM_CLIENT_DVFS].limit_cb)
		return;

	for_each_ppm_clusters(i) {
		arch_get_cluster_cpus(&cluster_cpu, i);
		cpumask_and(&online_cpu, &cluster_cpu, cpu_online_mask);

		cluster_status[i].core_num = cpumask_weight(&online_cpu);
		cluster_status[i].volt = 0;	/* don't care */
		if (!cluster_status[i].core_num)
			cluster_status[i].freq_idx = -1;
		else
#ifdef PPM_FAST_ATM_SUPPORT
			cluster_status[i].freq_idx = ppm_main_freq_to_idx(i,
					mt_cpufreq_get_cur_phy_freq_no_lock(i), CPUFREQ_RELATION_L);
#else
			cluster_status[i].freq_idx = ppm_main_freq_to_idx(i,
					mt_cpufreq_get_cur_phy_freq(i), CPUFREQ_RELATION_L);
#endif
		ppm_ver("[%d] core = %d, freq_idx = %d\n",
			i, cluster_status[i].core_num, cluster_status[i].freq_idx);
		/* copy cluster_status data structure */
		cluster_status_rebase[i] = cluster_status[i];
	}

#ifdef PPM_EFFICIENCY_TABLE_USE_CORE_LIMIT
	for (i = 0; i <= 2; i++) {
		if (cluster_status_rebase[i].core_num > Core_limit[i])
			cluster_status_rebase[i].core_num = Core_limit[i];
		if (req->limit[i].max_cpu_core < Core_limit[i])
			Core_limit[i] = req->limit[i].max_cpu_core;
	}
#endif
	if (cluster_status_rebase[0].core_num == 0 && cluster_status_rebase[1].core_num == 0) {
		if (Core_limit[0] > 0) {
			cluster_status_rebase[0].core_num = 1;
			cluster_status_rebase[0].freq_idx = 0;
		} else {
			cluster_status_rebase[1].core_num = 1;
			cluster_status_rebase[1].freq_idx = 0;
		}
	}

	/* use L cluster frequency */
	if (cluster_status_rebase[1].core_num > 0)
		cluster_status_rebase[0].freq_idx = cluster_status_rebase[1].freq_idx;

	if (cluster_status_rebase[0].freq_idx <= prev_max_cpufreq_idx[0])
		LxLLisLimited = 1;
	if (cluster_status_rebase[1].freq_idx <= prev_max_cpufreq_idx[1])
		LxLLisLimited = 1;

	power = ppm_find_pwr_idx(cluster_status_rebase);
	curr_power = (power == -1) ? mt_ppm_thermal_get_max_power() : (unsigned int)power;

	/* convert current OPP(frequency only) from 16 to 8 */
	if (cluster_status_rebase[2].freq_idx >= 0)
		opp[0] = freq_idx_mapping_tbl_big_r[cluster_status_rebase[2].freq_idx];
	else
		opp[0] = -1;
	if (cluster_status_rebase[1].freq_idx >= 0)
		opp[1] = freq_idx_mapping_tbl_r[cluster_status_rebase[1].freq_idx];
	else
		opp[1] = -1;
	if (cluster_status_rebase[0].freq_idx >= 0)
		opp[2] = freq_idx_mapping_tbl_r[cluster_status_rebase[0].freq_idx];
	else
		opp[2] = -1;

	delta_power = power_budget - curr_power;

	/* Get Active Core number of each cluster */
	activeCoreNumB = cluster_status_rebase[2].core_num;
	activeCoreNumL = cluster_status_rebase[1].core_num;
	activeCoreNumLL = cluster_status_rebase[0].core_num;

	/* Which Cluster in L and LL is active (1: L is on 2: L is off) */
	LxLL = 0;
	if (activeCoreNumL > 0)
		LxLL = 1;
	else if (activeCoreNumLL > 0)
		LxLL = 2;

#ifdef PPM_EFFICIENCY_TABLE_USE_CORE_LIMIT
	B_limit = Core_limit[2];
	L_limit = Core_limit[1];
	LL_limit = Core_limit[0];
	req->limit[0].max_cpu_core = LL_limit;
	req->limit[1].max_cpu_core = L_limit;
	req->limit[2].max_cpu_core = B_limit;
#endif

	if (activeCoreNumB < 0)
		activeCoreNumB = 0;
	if (activeCoreNumL < 0)
		activeCoreNumL = 0;
	if (activeCoreNumLL < 0)
		activeCoreNumLL = 0;

	ppm_dbg(ET_ALGO, "power_budget %d delta_power %d curr_power %d opp %d%d%d %d%d%d\n",
				power_budget, delta_power, curr_power, opp[0], opp[1], opp[2],
				activeCoreNumB, activeCoreNumL, activeCoreNumLL);

	/* increase ferquency limit */
	if (delta_power >= 0) {
		while (1) {
			int ChoosenCluster = -1, MaxEfficiency = 0, ChoosenPower = 0;

			if (opp[LxLL] == 0 && activeCoreNumB == 0 && delta_power > delta_power_B[1][7]) {
				activeCoreNumB = 1;
				delta_power -= delta_power_B[1][7];
				opp[0] = 7;
			}

			/* Big Clister */
			if (activeCoreNumB > 0 && opp[0] > 0
				&& delta_power > delta_power_B[activeCoreNumB][opp[0]-1]) {
				MaxEfficiency = efficiency_B[activeCoreNumB][opp[0]-1];
				ChoosenCluster = 0;
				ChoosenPower = delta_power_B[activeCoreNumB][opp[0]-1];
			}

			ppm_dbg(ET_ALGO, "power_budget %d delta_power %d curr_power %d opp %d%d%d %d%d%d %d %d\n",
					power_budget, delta_power, curr_power, opp[0], opp[1], opp[2],
					activeCoreNumB, activeCoreNumL, activeCoreNumLL,
					prev_max_cpufreq_idx[LxLL], cluster_status_rebase[2-LxLL].freq_idx);
			ppm_dbg(ET_ALGO, "prev_max_cpufreq_idx[LxLL] %d opp[LxLL] %d\n",
					prev_max_cpufreq_idx[LxLL], opp[LxLL]);

			/* LxLL Cluster */
			if (LxLL && opp[LxLL] > 0
				&& delta_power > delta_power_LxLL[activeCoreNumL][activeCoreNumLL][opp[LxLL]-1]
				&& efficiency_LxLL[activeCoreNumL][activeCoreNumLL][opp[LxLL]-1] > MaxEfficiency
				&& (LxLLisLimited || opp[0] == 0 || activeCoreNumB == 0)) {
				MaxEfficiency = efficiency_LxLL[activeCoreNumL][activeCoreNumLL][opp[LxLL]-1];
				ChoosenCluster = 1;
				ChoosenPower = delta_power_LxLL[activeCoreNumL][activeCoreNumLL][opp[LxLL]-1];
			}

			/* exceed power budget or all active core is highest freq. */
			if (ChoosenCluster == -1) {
#ifdef PPM_EFFICIENCY_TABLE_USE_CORE_LIMIT
				if (opp[LxLL] != 0)
					goto end;

				/* PPM state L_ONLY --> LL core remain turned off */
				if (new_state != PPM_POWER_STATE_L_ONLY) {
					while (LL_limit < 4 && delta_power >
							delta_power_LxLL[activeCoreNumL][LL_limit+1][7]) {
						delta_power -= delta_power_LxLL[activeCoreNumL][LL_limit + 1][7];
						req->limit[0].max_cpu_core = ++LL_limit;
					}
				}
				/* PPM state LL_ONLY --> L core remain turned off */
				if (new_state != PPM_POWER_STATE_LL_ONLY) {
					while (L_limit < 4 && delta_power >
							delta_power_LxLL[L_limit+1][activeCoreNumLL][7]) {
						delta_power -= delta_power_LxLL[L_limit+1][activeCoreNumLL][7];
						req->limit[1].max_cpu_core = ++L_limit;
					}
				}
				/* PPM state L_ONLY or LL_ONLY--> B core remain turned off */
				if (new_state != PPM_POWER_STATE_LL_ONLY && new_state != PPM_POWER_STATE_L_ONLY) {
					while (B_limit < 2 && delta_power > delta_power_B[B_limit+1][7]) {
						delta_power -= delta_power_B[B_limit+1][7];
						req->limit[2].max_cpu_core = ++B_limit;
					}
				}
end:
#endif
				break;
			}
			/* if choose Cluster LxLL, change opp of active cluster only */
			if (ChoosenCluster == 1) {
				if (activeCoreNumL > 0)
					opp[1] -= 1;
				if (activeCoreNumLL > 0)
					opp[2] -= 1;
			} else
				opp[ChoosenCluster] -= 1;

			delta_power -= ChoosenPower;
			/* curr_power += ChoosenPower; */

		}
	} else {
		while (delta_power < 0) {
			int ChoosenCluster = -1;
			int MinEfficiency = 10000;	/* should be bigger than max value of efficiency_* array */
			int ChoosenPower = 0;

			/* B */
			if (activeCoreNumB > 0 && opp[0] < PPM_EFFICIENCY_TABLE_MAX_FREQ_IDX) {
				MinEfficiency = efficiency_B[activeCoreNumB][opp[0]];
				ChoosenCluster = 0;
				ChoosenPower = delta_power_B[activeCoreNumB][opp[0]];
			}
			/* LxLL */
			if (LxLL && opp[LxLL] < PPM_EFFICIENCY_TABLE_MAX_FREQ_IDX
				&& efficiency_LxLL[activeCoreNumL][activeCoreNumLL][opp[LxLL]] < MinEfficiency) {
				MinEfficiency = efficiency_LxLL[activeCoreNumL][activeCoreNumLL][opp[LxLL]];
				ChoosenCluster = 1;
				ChoosenPower = delta_power_LxLL[activeCoreNumL][activeCoreNumLL][opp[LxLL]];
			}
			if (ChoosenCluster == -1) {
				ppm_err("Failed to find lower OPP: budget %d delta %d curr_power %d opp %d%d%d %d%d%d\n",
					power_budget, delta_power, curr_power, opp[0], opp[1], opp[2],
					activeCoreNumB, activeCoreNumL, activeCoreNumLL);
				break;
			}

			/* if choose Cluster LxLL, change opp of active cluster only */
			if (ChoosenCluster == 1) {
				if (activeCoreNumL > 0)
					opp[1] += 1;
				if (activeCoreNumLL > 0)
					opp[2] += 1;
			} else
				opp[ChoosenCluster] += 1;
			/* Limit Core */
#ifdef PPM_EFFICIENCY_TABLE_USE_CORE_LIMIT
			if (opp[0] == 8 && activeCoreNumB > 0) {
				req->limit[2].max_cpu_core = --activeCoreNumB;
				opp[0] = 7;
			} else if (opp[LxLL] == 8) {
				if (activeCoreNumL > 1 || (activeCoreNumLL > 0 && activeCoreNumL > 0))
					req->limit[1].max_cpu_core = --activeCoreNumL;
				else if (activeCoreNumLL > 1)
					req->limit[0].max_cpu_core = --activeCoreNumLL;
				if (activeCoreNumL > 0)
					opp[1] = 7;
				if (activeCoreNumLL > 0)
					opp[2] = 7;
			}
#endif

			delta_power += ChoosenPower;
			curr_power -= ChoosenPower;
		}
	}

	/* Set frequency limit */
	/* For non share buck */
#if 0
	if (opp[2] >= 0 && activeCoreNumLL > 0)
		req->limit[0].max_cpufreq_idx = freq_idx_mapping_tbl[opp[2]];
	if (opp[1] >= 0 && activeCoreNumL > 0)
		req->limit[1].max_cpufreq_idx = freq_idx_mapping_tbl[opp[1]];
#endif

	/* Set all frequency limit of the cluster */
	/* Set OPP of Cluser n to opp[n] */
	req->limit[0].max_cpufreq_idx = freq_idx_mapping_tbl[opp[LxLL]];
	req->limit[1].max_cpufreq_idx = freq_idx_mapping_tbl[opp[LxLL]];

	if (opp[0] >= 0 && activeCoreNumB > 0)
		req->limit[2].max_cpufreq_idx = freq_idx_mapping_tbl_big[opp[0]];
	else
		req->limit[2].max_cpufreq_idx = 15;

	ppm_dbg(ET_ALGO , "power_budget %d delta_power %d curr_power %d opp freq limit B:%d L:%d LL%d\n",
			power_budget, delta_power, curr_power, opp[0], opp[1], opp[2]);
	ppm_dbg(ET_ALGO, "online core B:%d L:%d LL:%d out: F_limit LL:%d L%d B%d Core_limit LL:%d L%d B%d\n",
			activeCoreNumB, activeCoreNumL, activeCoreNumLL, req->limit[0].max_cpufreq_idx,
			req->limit[1].max_cpufreq_idx, req->limit[2].max_cpufreq_idx,
			req->limit[0].max_cpu_core, req->limit[1].max_cpu_core, req->limit[2].max_cpu_core);

	for (i = 0; i < req->cluster_num; i++) {
		/* error check */
		if (req->limit[i].max_cpufreq_idx > req->limit[i].min_cpufreq_idx)
			req->limit[i].min_cpufreq_idx = req->limit[i].max_cpufreq_idx;
		if (req->limit[i].max_cpu_core < req->limit[i].min_cpu_core)
			req->limit[i].min_cpu_core = req->limit[i].max_cpu_core;
	}
}
#else
void ppm_main_update_req_by_pwr(enum ppm_power_state new_state, struct ppm_policy_req *req)
{
	int index, i;
#ifdef PPM_THERMAL_ENHANCEMENT
	struct ppm_power_tbl_data power_table;

	if (ppm_is_need_to_check_tlp3_table(new_state, req->power_budget)) {
		index = ppm_get_tlp3_table_idx_by_pwr(new_state, req->power_budget);
		if (index != -1) {
			power_table = ppm_get_tlp3_power_table();
			goto tbl_lookup_done;
		}
	}

	power_table = ppm_get_power_table();
	index = ppm_get_table_idx_by_pwr(new_state, req->power_budget);

tbl_lookup_done:
#else
	struct ppm_power_tbl_data power_table = ppm_get_power_table();

	index = ppm_get_table_idx_by_pwr(new_state, req->power_budget);
#endif
	if (index != -1) {
		for (i = 0; i < req->cluster_num; i++) {
			req->limit[i].max_cpu_core =
				power_table.power_tbl[index].cluster_cfg[i].core_num;
			if (power_table.power_tbl[index].cluster_cfg[i].core_num == 0)
				req->limit[i].min_cpu_core = 0;
			else
				req->limit[i].max_cpufreq_idx =	power_table.power_tbl[index].cluster_cfg[i].opp_lv;

			/* error check */
			if (req->limit[i].max_cpu_core < req->limit[i].min_cpu_core)
				req->limit[i].min_cpu_core = req->limit[i].max_cpu_core;
			if (req->limit[i].max_cpufreq_idx > req->limit[i].min_cpufreq_idx)
				req->limit[i].min_cpufreq_idx = req->limit[i].max_cpufreq_idx;
		}
	} else
		ppm_dbg(MAIN, "@%s: index not found!", __func__);
}
#endif

int ppm_main_freq_to_idx(unsigned int cluster_id,
					unsigned int freq, unsigned int relation)
{
	int i, size;
	int idx = -1;

	FUNC_ENTER(FUNC_LV_MAIN);

	if (!ppm_main_info.cluster_info[cluster_id].dvfs_tbl) {
		ppm_err("@%s: DVFS table of cluster %d is not exist!\n", __func__, cluster_id);
		idx = (relation == CPUFREQ_RELATION_L)
			? get_cluster_min_cpufreq_idx(cluster_id) : get_cluster_max_cpufreq_idx(cluster_id);
		return idx;
	}

	size = ppm_main_info.cluster_info[cluster_id].dvfs_opp_num;

	/* error handle */
	if (freq > get_cluster_max_cpufreq(cluster_id))
		freq = ppm_main_info.cluster_info[cluster_id].dvfs_tbl[0].frequency;

	if (freq < get_cluster_min_cpufreq(cluster_id))
		freq = ppm_main_info.cluster_info[cluster_id].dvfs_tbl[size-1].frequency;

	/* search idx */
	if (relation == CPUFREQ_RELATION_L) {
		for (i = (signed)(size - 1); i >= 0; i--) {
			if (ppm_main_info.cluster_info[cluster_id].dvfs_tbl[i].frequency >= freq) {
				idx = i;
				break;
			}
		}
	} else { /* CPUFREQ_RELATION_H */
		for (i = 0; i < (signed)size; i++) {
			if (ppm_main_info.cluster_info[cluster_id].dvfs_tbl[i].frequency <= freq) {
				idx = i;
				break;
			}
		}
	}

	if (idx == -1) {
		ppm_err("@%s: freq %d KHz not found in DVFS table of cluster %d\n", __func__, freq, cluster_id);
		idx = (relation == CPUFREQ_RELATION_L)
			? get_cluster_min_cpufreq_idx(cluster_id)
			: get_cluster_max_cpufreq_idx(cluster_id);
	} else
		ppm_ver("@%s: The idx of %d KHz in cluster %d is %d\n", __func__, freq, cluster_id, idx);

	FUNC_EXIT(FUNC_LV_MAIN);

	return idx;
}

void ppm_main_clear_client_req(struct ppm_client_req *c_req)
{
	int i;

	for (i = 0; i < c_req->cluster_num; i++) {
		c_req->cpu_limit[i].min_cpufreq_idx = get_cluster_min_cpufreq_idx(i);
		c_req->cpu_limit[i].max_cpufreq_idx = get_cluster_max_cpufreq_idx(i);
		c_req->cpu_limit[i].min_cpu_core = get_cluster_min_cpu_core(i);
		c_req->cpu_limit[i].max_cpu_core = get_cluster_max_cpu_core(i);
		c_req->cpu_limit[i].has_advise_freq = false;
		c_req->cpu_limit[i].advise_cpufreq_idx = -1;
		c_req->cpu_limit[i].has_advise_core = false;
		c_req->cpu_limit[i].advise_cpu_core = -1;
	}

#ifdef PPM_DISABLE_CLUSTER_MIGRATION
	/* keep at least 1 LL */
	c_req->cpu_limit[0].min_cpu_core = 1;
#endif
}

void ppm_task_wakeup(void)
{
	FUNC_ENTER(FUNC_LV_MAIN);

#if 0
	ppm_lock(&ppm_main_info.lock);
	if (ppm_main_info.ppm_task) {
		atomic_set(&ppm_main_info.ppm_event, 1);
		wake_up(&ppm_main_info.ppm_wq);
	}
	ppm_unlock(&ppm_main_info.lock);
#else
	mt_ppm_main();
#endif

	FUNC_EXIT(FUNC_LV_MAIN);
}

int ppm_main_register_policy(struct ppm_policy_data *policy)
{
	struct list_head *pos;
	int ret = 0, i;

	FUNC_ENTER(FUNC_LV_MAIN);

	ppm_lock(&ppm_main_info.lock);

	/* init remaining members in policy data */
	policy->req.limit = kcalloc(ppm_main_info.cluster_num, sizeof(*policy->req.limit), GFP_KERNEL);
	if (!policy->req.limit) {
		ret = -ENOMEM;
		goto out;
	}
	policy->req.cluster_num = ppm_main_info.cluster_num;
	policy->req.power_budget = 0;
	policy->req.perf_idx = 0;
	/* init default limit */
	for (i = 0; i < policy->req.cluster_num; i++) {
		policy->req.limit[i].min_cpufreq_idx = get_cluster_min_cpufreq_idx(i);
		policy->req.limit[i].max_cpufreq_idx = get_cluster_max_cpufreq_idx(i);
		policy->req.limit[i].min_cpu_core = get_cluster_min_cpu_core(i);
		policy->req.limit[i].max_cpu_core = get_cluster_max_cpu_core(i);
	}

	/* insert into global policy_list according to its priority */
	list_for_each(pos, &ppm_main_info.policy_list) {
		struct ppm_policy_data *data;

		data = list_entry(pos, struct ppm_policy_data, link);
		if (policy->priority > data->priority  ||
			(policy->priority == data->priority && policy->policy > data->policy))
			break;
	}
	list_add_tail(&policy->link, pos);

	policy->is_enabled = true;

out:
	ppm_unlock(&ppm_main_info.lock);

	FUNC_ENTER(FUNC_LV_MAIN);

	return ret;
}

void ppm_main_unregister_policy(struct ppm_policy_data *policy)
{
	FUNC_ENTER(FUNC_LV_MAIN);

	ppm_lock(&ppm_main_info.lock);
	kfree(policy->req.limit);
	list_del(&policy->link);
	policy->is_enabled = false;
	policy->is_activated = false;
	ppm_unlock(&ppm_main_info.lock);

	FUNC_EXIT(FUNC_LV_MAIN);
}

static void ppm_main_update_limit(struct ppm_policy_data *p,
			struct ppm_client_limit *c_limit, struct ppm_cluster_limit *p_limit)
{
	FUNC_ENTER(FUNC_LV_MAIN);

	ppm_ver("Policy --> (%d)(%d)(%d)(%d)\n", p_limit->min_cpufreq_idx,
		p_limit->max_cpufreq_idx, p_limit->min_cpu_core, p_limit->max_cpu_core);
	ppm_ver("Original --> (%d)(%d)(%d)(%d) (%d)(%d)(%d)(%d)\n",
		c_limit->min_cpufreq_idx, c_limit->max_cpufreq_idx, c_limit->min_cpu_core,
		c_limit->max_cpu_core, c_limit->has_advise_freq, c_limit->advise_cpufreq_idx,
		c_limit->has_advise_core, c_limit->advise_cpu_core);

	switch (p->policy) {
	/* fix freq */
	case PPM_POLICY_PTPOD:
		c_limit->has_advise_freq = true;
		c_limit->advise_cpufreq_idx = p_limit->max_cpufreq_idx;
		break;
	/* fix freq and core */
	case PPM_POLICY_UT:
		if (p_limit->min_cpufreq_idx == p_limit->max_cpufreq_idx) {
			c_limit->has_advise_freq = true;
			c_limit->advise_cpufreq_idx = p_limit->max_cpufreq_idx;
			c_limit->min_cpufreq_idx = c_limit->max_cpufreq_idx = p_limit->max_cpufreq_idx;
		}

		if (p_limit->min_cpu_core == p_limit->max_cpu_core) {
			c_limit->has_advise_core = true;
			c_limit->advise_cpu_core = p_limit->max_cpu_core;
			c_limit->min_cpu_core = c_limit->max_cpu_core = p_limit->max_cpu_core;
		}
		break;
	/* base */
	case PPM_POLICY_HICA:
		c_limit->min_cpufreq_idx = p_limit->min_cpufreq_idx;
		c_limit->max_cpufreq_idx = p_limit->max_cpufreq_idx;
		c_limit->min_cpu_core = p_limit->min_cpu_core;
		c_limit->max_cpu_core = p_limit->max_cpu_core;
		/* reset advise */
		c_limit->has_advise_freq = false;
		c_limit->advise_cpufreq_idx = -1;
		c_limit->has_advise_core = false;
		c_limit->advise_cpu_core = -1;
		break;
	default:
		/* out of range! use policy's min/max cpufreq idx setting */
		if (c_limit->min_cpufreq_idx <  p_limit->max_cpufreq_idx ||
			c_limit->max_cpufreq_idx >  p_limit->min_cpufreq_idx) {
			/* no need to set min freq for power budget related policy */
			if (p->priority != PPM_POLICY_PRIO_POWER_BUDGET_BASE)
				c_limit->min_cpufreq_idx = p_limit->min_cpufreq_idx;
			c_limit->max_cpufreq_idx = p_limit->max_cpufreq_idx;
		} else {
			c_limit->min_cpufreq_idx = MIN(c_limit->min_cpufreq_idx, p_limit->min_cpufreq_idx);
			c_limit->max_cpufreq_idx = MAX(c_limit->max_cpufreq_idx, p_limit->max_cpufreq_idx);
		}

		/* out of range! use policy's min/max cpu core setting */
		if (c_limit->min_cpu_core >  p_limit->max_cpu_core ||
			c_limit->max_cpu_core <  p_limit->min_cpu_core) {
			/* no need to set min core for power budget related policy */
			if (p->priority != PPM_POLICY_PRIO_POWER_BUDGET_BASE)
				c_limit->min_cpu_core = p_limit->min_cpu_core;
			c_limit->max_cpu_core = p_limit->max_cpu_core;
		} else {
			c_limit->min_cpu_core = MAX(c_limit->min_cpu_core, p_limit->min_cpu_core);
			c_limit->max_cpu_core = MIN(c_limit->max_cpu_core, p_limit->max_cpu_core);
		}

		/* clear previous advise if it is not in current limit range */
		if (c_limit->has_advise_freq &&
			(c_limit->advise_cpufreq_idx > c_limit->min_cpufreq_idx ||
			c_limit->advise_cpufreq_idx < c_limit->max_cpufreq_idx)) {
			c_limit->has_advise_freq = false;
			c_limit->advise_cpufreq_idx = -1;
		}
		if (c_limit->has_advise_core &&
			(c_limit->advise_cpu_core < c_limit->min_cpu_core ||
			c_limit->advise_cpu_core > c_limit->max_cpu_core)) {
			c_limit->has_advise_core = false;
			c_limit->advise_cpu_core = -1;
		}
		break;
	}

	/* error check */
	if (c_limit->min_cpu_core > c_limit->max_cpu_core)
		c_limit->min_cpu_core = c_limit->max_cpu_core;
	if (c_limit->min_cpufreq_idx < c_limit->max_cpufreq_idx)
		c_limit->min_cpufreq_idx = c_limit->max_cpufreq_idx;

	ppm_ver("Result --> (%d)(%d)(%d)(%d) (%d)(%d)(%d)(%d)\n",
		c_limit->min_cpufreq_idx, c_limit->max_cpufreq_idx, c_limit->min_cpu_core,
		c_limit->max_cpu_core, c_limit->has_advise_freq, c_limit->advise_cpufreq_idx,
		c_limit->has_advise_core, c_limit->advise_cpu_core);

	FUNC_EXIT(FUNC_LV_MAIN);
}

static void ppm_main_calc_new_limit(void)
{
	struct ppm_policy_data *pos;
	int i, active_cnt = 0;
	bool is_ptp_activate = false, is_all_cluster_zero = true;
	struct ppm_client_req *c_req = &(ppm_main_info.client_req);
	struct ppm_client_req *last_req = &(ppm_main_info.last_req);

	FUNC_ENTER(FUNC_LV_MAIN);

	/* traverse policy list to get the final limit to client */
	list_for_each_entry(pos, &ppm_main_info.policy_list, link) {
		ppm_lock(&pos->lock);

		if ((pos->is_enabled && pos->is_activated && pos->is_limit_updated)
				|| pos->policy == PPM_POLICY_HICA) {
			pos->is_limit_updated = false;

			for_each_ppm_clusters(i) {
				ppm_ver("@%s: applying policy %s cluster %d limit...\n", __func__, pos->name, i);
				ppm_main_update_limit(pos,
					&c_req->cpu_limit[i], &pos->req.limit[i]);
			}

			is_ptp_activate = (pos->policy == PPM_POLICY_PTPOD) ? true : false;

			active_cnt++;
		}

		ppm_unlock(&pos->lock);
	}

	/* send default limit to client */
	if (active_cnt == 0)
		ppm_main_clear_client_req(c_req);

#ifdef PPM_DISABLE_BIG_FOR_LP_MODE
	/* limit B core if currnt mode is LOW_POWER */
	if (ppm_main_info.cur_mode == PPM_MODE_LOW_POWER) {
		c_req->cpu_limit[PPM_CLUSTER_B].min_cpu_core = 0;
		c_req->cpu_limit[PPM_CLUSTER_B].max_cpu_core = 0;
		c_req->cpu_limit[PPM_CLUSTER_B].has_advise_core = false;
		c_req->cpu_limit[PPM_CLUSTER_B].advise_cpu_core = 0;
	}
#endif

	/* set freq idx to previous limit if nr_cpu in the cluster is 0 */
	for (i = 0; i < c_req->cluster_num; i++) {
		if (c_req->cpu_limit[i].max_cpu_core)
			is_all_cluster_zero = false;

		if ((!c_req->cpu_limit[i].min_cpu_core && !c_req->cpu_limit[i].max_cpu_core)
			|| (c_req->cpu_limit[i].has_advise_core && !c_req->cpu_limit[i].advise_cpu_core)) {
			c_req->cpu_limit[i].min_cpufreq_idx = last_req->cpu_limit[i].min_cpufreq_idx;
			c_req->cpu_limit[i].max_cpufreq_idx = last_req->cpu_limit[i].max_cpufreq_idx;
			c_req->cpu_limit[i].has_advise_freq = last_req->cpu_limit[i].has_advise_freq;
			c_req->cpu_limit[i].advise_cpufreq_idx = last_req->cpu_limit[i].advise_cpufreq_idx;
		}
#ifdef PPM_USE_EFFICIENCY_TABLE
		prev_max_cpufreq_idx[i] = c_req->cpu_limit[i].max_cpufreq_idx;
		Core_limit[i] = c_req->cpu_limit[i].max_cpu_core;
#endif

		ppm_ver("Final Result: [%d] --> (%d)(%d)(%d)(%d) (%d)(%d)(%d)(%d)\n",
			i,
			c_req->cpu_limit[i].min_cpufreq_idx,
			c_req->cpu_limit[i].max_cpufreq_idx,
			c_req->cpu_limit[i].min_cpu_core,
			c_req->cpu_limit[i].max_cpu_core,
			c_req->cpu_limit[i].has_advise_freq,
			c_req->cpu_limit[i].advise_cpufreq_idx,
			c_req->cpu_limit[i].has_advise_core,
			c_req->cpu_limit[i].advise_cpu_core
		);
	}

#ifdef PPM_VPROC_5A_LIMIT_CHECK
	if (ppm_main_info.is_5A_limit_enable && ppm_main_info.is_5A_limit_on) {
		for (i = 0; i < c_req->cluster_num; i++) {
			if (c_req->cpu_limit[i].max_cpufreq_idx > get_cluster_max_cpufreq_idx(i)
				|| c_req->cpu_limit[i].max_cpu_core < get_cluster_max_cpu_core(i))
				break;
		}

		/* apply 5A throttle since freq and core are not limit yet */
		if (i == c_req->cluster_num) {
			for (i = 0; i < c_req->cluster_num; i++) {
				c_req->cpu_limit[i].max_cpufreq_idx = PPM_5A_LIMIT_FREQ_IDX;
				if (c_req->cpu_limit[i].min_cpufreq_idx < PPM_5A_LIMIT_FREQ_IDX)
					c_req->cpu_limit[i].min_cpufreq_idx = PPM_5A_LIMIT_FREQ_IDX;
				if (c_req->cpu_limit[i].has_advise_freq &&
					c_req->cpu_limit[i].advise_cpufreq_idx < PPM_5A_LIMIT_FREQ_IDX)
					c_req->cpu_limit[i].advise_cpufreq_idx = PPM_5A_LIMIT_FREQ_IDX;
			}
		}
	}
#endif

	/* fill root cluster */
	c_req->root_cluster = ppm_get_root_cluster_by_state(ppm_main_info.cur_power_state);

	/* fill ptpod activate flag */
	c_req->is_ptp_policy_activate = is_ptp_activate;

	/* Trigger exception if all cluster max core limit is 0 */
	if (is_all_cluster_zero) {
		struct ppm_policy_data *pos;
		unsigned int i = 0;

		ppm_err("all cluster max core limit are 0, dump all active policy data...\n");

		/* dump all policy data for debugging */
		ppm_info("Current state = %s\n", ppm_get_power_state_name(ppm_main_info.cur_power_state));

		list_for_each_entry(pos, &ppm_main_info.policy_list, link) {
			ppm_lock(&pos->lock);
			if (pos->is_activated) {
				ppm_info("[%d] %s: perf_idx = %d, pwr_bdgt = %d\n",
						pos->policy, pos->name, pos->req.perf_idx, pos->req.power_budget);
				for_each_ppm_clusters(i) {
					ppm_info("cluster %d: (%d)(%d)(%d)(%d)\n", i,
						pos->req.limit[i].min_cpufreq_idx, pos->req.limit[i].max_cpufreq_idx,
						pos->req.limit[i].min_cpu_core, pos->req.limit[i].max_cpu_core);
				}
				ppm_info("\n");
			}
			ppm_unlock(&pos->lock);
		}

		BUG();
	}

	FUNC_EXIT(FUNC_LV_MAIN);
}

static enum ppm_power_state ppm_main_hica_state_decision(void)
{
	enum ppm_power_state cur_hica_state = ppm_hica_get_cur_state();
	enum ppm_power_state final_state;
	enum ppm_power_state perf_state = PPM_POWER_STATE_NONE;
	struct ppm_policy_data *pos;

	FUNC_ENTER(FUNC_LV_MAIN);

	ppm_ver("@%s: Current state = %s\n", __func__, ppm_get_power_state_name(cur_hica_state));

	final_state = cur_hica_state;

	ppm_main_info.min_power_budget = ~0;

#ifdef PPM_IC_SEGMENT_CHECK
	if (ppm_main_info.fix_state_by_segment != PPM_POWER_STATE_NONE) {
		final_state = ppm_main_info.fix_state_by_segment;
		goto skip_pwr_check;
	}
#endif

	/* For power budget related policy: find the min power budget */
	/* For other policies: use callback to decide the state for each policy */
	list_for_each_entry(pos, &ppm_main_info.policy_list, link) {
		ppm_lock(&pos->lock);
		if (pos->is_activated) {
			switch (pos->policy) {
			case PPM_POLICY_THERMAL:
			case PPM_POLICY_DLPT:
			case PPM_POLICY_PWR_THRO:
				if (pos->req.power_budget)
					ppm_main_info.min_power_budget =
						MIN(pos->req.power_budget, ppm_main_info.min_power_budget);
				break;
			case PPM_POLICY_PTPOD:
				/* skip power budget related policy check if PTPOD policy is activated */
				if (pos->get_power_state_cb) {
					final_state = pos->get_power_state_cb(cur_hica_state);
					ppm_unlock(&pos->lock);
					goto skip_pwr_check;
				}
				break;
			case PPM_POLICY_USER_LIMIT:
				if (pos->get_power_state_cb) {
					enum ppm_power_state state = (perf_state == PPM_POWER_STATE_NONE)
									? pos->get_power_state_cb(cur_hica_state)
									: pos->get_power_state_cb(perf_state);

					if (state != cur_hica_state)
						final_state = state;
				}
				break;
			default:
				/* overwrite original HICA state */
				if (pos->get_power_state_cb) {
					enum ppm_power_state state = pos->get_power_state_cb(cur_hica_state);

					if (state != cur_hica_state)
						final_state = state;
					if (pos->policy == PPM_POLICY_PERF_SERV)
						perf_state = state;
				}
				break;
			}
		}
		ppm_unlock(&pos->lock);
	}

	ppm_ver("@%s: final state (before) = %s, min_power_budget = %d\n",
		__func__, ppm_get_power_state_name(final_state), ppm_main_info.min_power_budget);

	/* decide final state */
	if (ppm_main_info.min_power_budget != ~0) {
		enum ppm_power_state state_by_pwr_idx =
			ppm_hica_get_state_by_pwr_budget(final_state, ppm_main_info.min_power_budget);
		final_state = MIN(final_state, state_by_pwr_idx);
	}

skip_pwr_check:
	ppm_ver("@%s: final state (after) = %s\n", __func__, ppm_get_power_state_name(final_state));

	FUNC_EXIT(FUNC_LV_MAIN);

	return final_state;
}

#define LOG_BUF_SIZE	128
static void ppm_main_log_print(unsigned int policy_mask, unsigned int min_power_budget,
	unsigned int root_cluster, char *msg)
{
#ifdef PPM_OUTPUT_TRANS_LOG_TO_UART
	ppm_info("(0x%x)(%d)(%d)%s\n", policy_mask, min_power_budget, root_cluster, msg);
#else
	ppm_dbg(MAIN, "(0x%x)(%d)(%d)%s\n", policy_mask, min_power_budget, root_cluster, msg);
#endif
}

int mt_ppm_main(void)
{
	struct ppm_policy_data *pos;
	struct ppm_client_req *c_req = &(ppm_main_info.client_req);
	struct ppm_client_req *last_req = &(ppm_main_info.last_req);
	enum ppm_power_state prev_state;
	enum ppm_power_state next_state;
	unsigned int policy_mask = 0;
	int i, notify_hps_first = 0;
	ktime_t now;
	unsigned long long delta;

	FUNC_ENTER(FUNC_LV_MAIN);

	ppm_lock(&ppm_main_info.lock);
	/* atomic_set(&ppm_main_info.ppm_event, 0); */

	if (!ppm_main_info.is_enabled || ppm_main_info.is_in_suspend)
		goto end;

#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_ppm_step(1);
#endif

	prev_state = ppm_main_info.cur_power_state;

	/* select new state */
	next_state = ppm_main_hica_state_decision();
	ppm_main_info.cur_power_state = next_state;

#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_ppm_step(2);
#endif

#ifdef PPM_EFFICIENCY_TABLE_USE_CORE_LIMIT
	/* reset Core_limit if state changed */
	if (prev_state != next_state) {
		struct ppm_power_state_data *state_info = ppm_get_power_state_info();

		for (i = 0; i < ppm_main_info.cluster_num; i++) {
			if (next_state >= PPM_POWER_STATE_NONE)
				Core_limit[i] = get_cluster_max_cpu_core(i);
			else
				Core_limit[i] = state_info[next_state].cluster_limit->state_limit[i].max_cpu_core;
		}
	}
#endif

	/* update active policy's limit according to current state */
	list_for_each_entry(pos, &ppm_main_info.policy_list, link) {
		if ((pos->is_activated || pos->policy == PPM_POLICY_HICA)
			&& pos->update_limit_cb) {
			ppm_lock(&pos->lock);
			policy_mask |= 1 << pos->policy;
			pos->update_limit_cb(next_state);
			pos->is_limit_updated = true;
			ppm_unlock(&pos->lock);
		}
	}

#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_ppm_step(3);
#endif

	/* calculate final limit and fill-in client request structure */
	ppm_main_calc_new_limit();

#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_ppm_step(4);
#endif

#ifdef PPM_CLUSTER_MIGRATION_BOOST
	if (prev_state == PPM_POWER_STATE_L_ONLY && next_state == PPM_POWER_STATE_LL_ONLY) {
		unsigned int freq_L = mt_cpufreq_get_cur_phy_freq_no_lock(PPM_CLUSTER_L);
		int freq_idx_LL = ppm_main_freq_to_idx(PPM_CLUSTER_LL, freq_L, CPUFREQ_RELATION_L);

		if (freq_idx_LL != -1)
			c_req->cpu_limit[PPM_CLUSTER_LL].min_cpufreq_idx =
				(freq_idx_LL > c_req->cpu_limit[PPM_CLUSTER_LL].max_cpufreq_idx)
				? freq_idx_LL : c_req->cpu_limit[PPM_CLUSTER_LL].max_cpufreq_idx;

		ppm_ver("boost when L -> LL! L freq = %dKHz(LL min idx = %d)\n", freq_L, freq_idx_LL);
	}
#endif

	/* notify client and print debug message if limits are changed */
	if (memcmp(c_req->cpu_limit, last_req->cpu_limit,
		ppm_main_info.cluster_num * sizeof(*c_req->cpu_limit))) {
		char buf[LOG_BUF_SIZE];
		char *ptr = buf;

		/* print debug message */
		if (prev_state != next_state)
			ptr += snprintf(ptr, LOG_BUF_SIZE, "[%s]->[%s]: ",
						ppm_get_power_state_name(prev_state),
						ppm_get_power_state_name(next_state));
		else
			ptr += snprintf(ptr, LOG_BUF_SIZE, "[%s]: ", ppm_get_power_state_name(next_state));

		for (i = 0; i < c_req->cluster_num; i++) {
			ptr += snprintf(ptr, LOG_BUF_SIZE, "(%d)(%d)(%d)(%d) ",
				c_req->cpu_limit[i].min_cpufreq_idx,
				c_req->cpu_limit[i].max_cpufreq_idx,
				c_req->cpu_limit[i].min_cpu_core,
				c_req->cpu_limit[i].max_cpu_core
			);

			if (c_req->cpu_limit[i].has_advise_freq || c_req->cpu_limit[i].has_advise_core)
				ptr += snprintf(ptr, LOG_BUF_SIZE, "[(%d)(%d)(%d)(%d)] ",
					c_req->cpu_limit[i].has_advise_freq,
					c_req->cpu_limit[i].advise_cpufreq_idx,
					c_req->cpu_limit[i].has_advise_core,
					c_req->cpu_limit[i].advise_cpu_core
				);
		}

		trace_ppm_update(policy_mask, ppm_main_info.min_power_budget, c_req->root_cluster, buf);

#ifdef CONFIG_MTK_RAM_CONSOLE
		for (i = 0; i < c_req->cluster_num; i++) {
			aee_rr_rec_ppm_cluster_limit(i,
				(c_req->cpu_limit[i].min_cpufreq_idx << 24) |
				(c_req->cpu_limit[i].max_cpufreq_idx << 16) |
				(c_req->cpu_limit[i].min_cpu_core << 8) |
				(c_req->cpu_limit[i].max_cpu_core)
			);
		}
		aee_rr_rec_ppm_cur_state(next_state);
		aee_rr_rec_ppm_min_pwr_bgt(ppm_main_info.min_power_budget);
		aee_rr_rec_ppm_policy_mask(policy_mask);
#endif

#ifdef PPM_THERMAL_ENHANCEMENT
		{
			bool notify_hps = false, notify_dvfs = false, log_print = false;

			for (i = 0; i < c_req->cluster_num; i++) {
				if (c_req->cpu_limit[i].min_cpu_core != last_req->cpu_limit[i].min_cpu_core
					|| c_req->cpu_limit[i].max_cpu_core != last_req->cpu_limit[i].max_cpu_core
					|| c_req->cpu_limit[i].has_advise_core) {
					notify_hps = true;
					log_print = true;
				}
				if (c_req->cpu_limit[i].min_cpufreq_idx != last_req->cpu_limit[i].min_cpufreq_idx
					|| c_req->cpu_limit[i].max_cpufreq_idx != last_req->cpu_limit[i].max_cpufreq_idx
					|| c_req->cpu_limit[i].has_advise_freq) {
					int min_freq_ori = last_req->cpu_limit[i].min_cpufreq_idx;
					int max_freq_ori = last_req->cpu_limit[i].max_cpufreq_idx;
					int min_freq = c_req->cpu_limit[i].min_cpufreq_idx;
					int max_freq = c_req->cpu_limit[i].max_cpufreq_idx;

					notify_dvfs = true;

					/* check for log reduction */
					if (c_req->cpu_limit[i].max_cpu_core != 0
						&& (abs(max_freq - max_freq_ori) >= 5
						|| abs(min_freq - min_freq_ori) >= 5))
						log_print = true;
				}

				if (notify_hps && notify_dvfs)
					break;
			}

			/* notify needed client only */
			if (notify_dvfs && !notify_hps) {
				now = ktime_get();
				if (log_print)
					ppm_main_log_print(policy_mask, ppm_main_info.min_power_budget,
							c_req->root_cluster, buf);
				if (ppm_main_info.client_info[PPM_CLIENT_DVFS].limit_cb)
					ppm_main_info.client_info[PPM_CLIENT_DVFS].limit_cb(*c_req);
				delta = ktime_to_us(ktime_sub(ktime_get(), now));
				ppm_profile_update_client_exec_time(PPM_CLIENT_DVFS, delta);
				ppm_dbg(TIME_PROFILE, "Done! notify dvfs only! time = %lld us\n", delta);
				goto nofity_end;
			} else if (notify_hps && !notify_dvfs) {
				if (log_print)
					ppm_main_log_print(policy_mask, ppm_main_info.min_power_budget,
							c_req->root_cluster, buf);
				now = ktime_get();
				if (ppm_main_info.client_info[PPM_CLIENT_HOTPLUG].limit_cb)
					ppm_main_info.client_info[PPM_CLIENT_HOTPLUG].limit_cb(*c_req);
				delta = ktime_to_us(ktime_sub(ktime_get(), now));
				ppm_profile_update_client_exec_time(PPM_CLIENT_HOTPLUG, delta);
				ppm_dbg(TIME_PROFILE, "Done! notify hps only! time = %lld us\n", delta);
				goto nofity_end;
			}
		}
#endif

		ppm_main_log_print(policy_mask, ppm_main_info.min_power_budget,
				c_req->root_cluster, buf);

		/* check need to notify hps first or not
		   1. one or more power budget related policy is activate
		   2. no cluster migration or cluster on
		   3. Limits change from low freq with more core to high freq with less core
		*/
		if (ppm_main_info.min_power_budget != ~0 && prev_state >= next_state
			&& !(prev_state == PPM_POWER_STATE_L_ONLY && next_state == PPM_POWER_STATE_LL_ONLY)) {
			/* traverse each cluster's limit */
			for (i = 0; i < c_req->cluster_num; i++) {
				if ((c_req->cpu_limit[i].max_cpu_core != 0) &&
				(c_req->cpu_limit[i].max_cpufreq_idx < last_req->cpu_limit[i].max_cpufreq_idx) &&
				(c_req->cpu_limit[i].max_cpu_core < last_req->cpu_limit[i].max_cpu_core)) {
					notify_hps_first = 1;
					ppm_ver("notify hps first for cluster %d: F(%d)->(%d) C(%d)->(%d)\n",
						i,
						last_req->cpu_limit[i].max_cpufreq_idx,
						c_req->cpu_limit[i].max_cpufreq_idx,
						last_req->cpu_limit[i].max_cpu_core,
						c_req->cpu_limit[i].max_cpu_core
						);
					break;
				}
			}
		}

		/* send request to client */
		if (notify_hps_first) {
			for (i = NR_PPM_CLIENTS - 1; i >= 0; i--) {
				now = ktime_get();
				if (ppm_main_info.client_info[i].limit_cb)
					ppm_main_info.client_info[i].limit_cb(*c_req);
				delta = ktime_to_us(ktime_sub(ktime_get(), now));
				ppm_profile_update_client_exec_time(i, delta);
				ppm_dbg(TIME_PROFILE, "%s callback done! time = %lld us\n",
					(i == PPM_CLIENT_DVFS) ? "DVFS" : "HPS", delta);
			}
		} else {
			for_each_ppm_clients(i) {
				now = ktime_get();
				if (ppm_main_info.client_info[i].limit_cb)
					ppm_main_info.client_info[i].limit_cb(*c_req);
				delta = ktime_to_us(ktime_sub(ktime_get(), now));
				ppm_profile_update_client_exec_time(i, delta);
				ppm_dbg(TIME_PROFILE, "%s callback done! time = %lld us\n",
					(i == PPM_CLIENT_DVFS) ? "DVFS" : "HPS", delta);
			}
		}

#ifdef PPM_THERMAL_ENHANCEMENT
nofity_end:
#endif
		memcpy(last_req->cpu_limit, c_req->cpu_limit,
			ppm_main_info.cluster_num * sizeof(*c_req->cpu_limit));
	}

	if (prev_state != next_state)
		ppm_profile_state_change_notify(prev_state, next_state);

#if PPM_UPDATE_STATE_DIRECT_TO_MET
	if (NULL != g_pSet_PPM_State && prev_state != next_state)
		g_pSet_PPM_State((unsigned int)next_state);
#endif

#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_ppm_step(0);
#endif

end:
	ppm_unlock(&ppm_main_info.lock);

	FUNC_EXIT(FUNC_LV_MAIN);

	return 0;
}

#if 0
static int ppm_main_task(void *data)
{
	return 0;
	struct ppm_policy_data *pos;

	while (1) {
		if (ppm_main_info.is_enabled) {
			enum ppm_power_state state;

			ppm_lock(&ppm_main_info.lock);

			atomic_set(&ppm_main_info.ppm_event, 0);

			/* select new state */
			state = ppm_main_hica_state_decision();
			ppm_main_info.cur_hica_state = state;

			/* update active policy's limit according to current state */
			list_for_each_entry(pos, &ppm_main_info.policy_list, link) {
				if (pos->is_activated)
					ppm_hica_update_limit_by_state(state, pos);
			}

			/* calculate final limit and send to clients */
			ppm_main_calc_and_send_new_limit();

			ppm_unlock(&ppm_main_info.lock);
		}

		wait_event_timeout(ppm_main_info.ppm_wq,
			atomic_read(&ppm_main_info.ppm_event) != 0,
			msecs_to_jiffies(PPM_TIMER_INTERVAL_MS));

		if (kthread_should_stop())
			break;
	}

	return 0;
}
#endif

static void ppm_main_send_request_for_suspend(void)
{
	struct ppm_client_req *c_req = &(ppm_main_info.client_req);
	struct ppm_client_req *last_req = &(ppm_main_info.last_req);
	int i, j;

	FUNC_ENTER(FUNC_LV_MAIN);

	ppm_ver("@%s:\n", __func__);

	/* modify advise freq to DVFS */
	for (i = 0; i < c_req->cluster_num; i++) {
		c_req->cpu_limit[i].has_advise_freq = true;
		c_req->cpu_limit[i].advise_cpufreq_idx =
			ppm_main_freq_to_idx(i, get_cluster_suspend_fix_freq(i), CPUFREQ_RELATION_L);

		ppm_ver("Result: [%d] --> (%d)(%d)(%d)(%d) (%d)(%d)(%d)(%d)\n",
			i,
			c_req->cpu_limit[i].min_cpufreq_idx,
			c_req->cpu_limit[i].max_cpufreq_idx,
			c_req->cpu_limit[i].min_cpu_core,
			c_req->cpu_limit[i].max_cpu_core,
			c_req->cpu_limit[i].has_advise_freq,
			c_req->cpu_limit[i].advise_cpufreq_idx,
			c_req->cpu_limit[i].has_advise_core,
			c_req->cpu_limit[i].advise_cpu_core
		);
	}

	/* send request to DVFS only */
	for_each_ppm_clients(j) {
		if (ppm_main_info.client_info[j].limit_cb
			&& ppm_main_info.client_info[j].client == PPM_CLIENT_DVFS)
			ppm_main_info.client_info[j].limit_cb(*c_req);
	}

	memcpy(last_req->cpu_limit, c_req->cpu_limit,
		ppm_main_info.cluster_num * sizeof(*c_req->cpu_limit));

	ppm_info("send fix idx to DVFS before suspend!\n");

	FUNC_EXIT(FUNC_LV_MAIN);
}

static int ppm_main_suspend(struct device *dev)
{
	FUNC_ENTER(FUNC_LV_MODULE);

	ppm_info("%s: suspend callback in\n", __func__);

	ppm_lock(&ppm_main_info.lock);
	ppm_main_send_request_for_suspend();
	ppm_main_info.is_in_suspend = true;
	ppm_unlock(&ppm_main_info.lock);

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static int ppm_main_resume(struct device *dev)
{
	FUNC_ENTER(FUNC_LV_MODULE);

	ppm_info("%s: resume callback in\n", __func__);

	ppm_lock(&ppm_main_info.lock);
	ppm_main_info.is_in_suspend = false;
	ppm_unlock(&ppm_main_info.lock);

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static int ppm_main_data_init(void)
{
	struct cpumask cpu_mask;
#if 0
	struct sched_param param = { .sched_priority = MAX_RT_PRIO-5 };
#endif
	int ret = 0;
	int i;

	FUNC_ENTER(FUNC_LV_MAIN);

	/* init static power table */
#if 0
	mt_spower_init();
#endif

	/* get cluster num */
	ppm_main_info.cluster_num = (unsigned int)arch_get_nr_clusters();
	ppm_info("@%s: cluster_num = %d\n", __func__, ppm_main_info.cluster_num);

	/* init cluster info (DVFS table will be updated after DVFS driver registered) */
	ppm_main_info.cluster_info =
		kzalloc(ppm_main_info.cluster_num * sizeof(*ppm_main_info.cluster_info), GFP_KERNEL);
	if (!ppm_main_info.cluster_info) {
		ppm_err("@%s: fail to allocate memory for cluster_info!\n", __func__);
		ret = -ENOMEM;
		goto out;
	}
	for_each_ppm_clusters(i) {
		ppm_main_info.cluster_info[i].cluster_id = i;
		/* OPP num will update after DVFS set table */
		ppm_main_info.cluster_info[i].dvfs_opp_num = DVFS_OPP_NUM;

		/* get topology info */
		arch_get_cluster_cpus(&cpu_mask, i);
		ppm_main_info.cluster_info[i].core_num = cpumask_weight(&cpu_mask);
		ppm_main_info.cluster_info[i].cpu_id = cpumask_first(&cpu_mask);
		ppm_info("@%s: ppm cluster %d -> core_num = %d, cpu_id = %d\n",
				__func__,
				ppm_main_info.cluster_info[i].cluster_id,
				ppm_main_info.cluster_info[i].core_num,
				ppm_main_info.cluster_info[i].cpu_id
				);
	}

	/* init client request */
	ppm_main_info.client_req.cpu_limit =
		kzalloc(ppm_main_info.cluster_num * sizeof(*ppm_main_info.client_req.cpu_limit), GFP_KERNEL);
	if (!ppm_main_info.client_req.cpu_limit) {
		ppm_err("@%s: fail to allocate memory client_req!\n", __func__);
		ret = -ENOMEM;
		goto allocate_req_mem_fail;
	}

	ppm_main_info.last_req.cpu_limit =
		kzalloc(ppm_main_info.cluster_num * sizeof(*ppm_main_info.last_req.cpu_limit), GFP_KERNEL);
	if (!ppm_main_info.last_req.cpu_limit) {
		ppm_err("@%s: fail to allocate memory for last_req!\n", __func__);
		ret = -ENOMEM;
		goto allocate_last_req_mem_fail;
	}

	for_each_ppm_clusters(i) {
		ppm_main_info.client_req.cluster_num = ppm_main_info.cluster_num;
		ppm_main_info.client_req.root_cluster = 0;
		ppm_main_info.client_req.cpu_limit[i].cluster_id = i;
		ppm_main_info.client_req.cpu_limit[i].cpu_id = ppm_main_info.cluster_info[i].cpu_id;

		ppm_main_info.last_req.cluster_num = ppm_main_info.cluster_num;
	}

#if 0
	ppm_main_info.ppm_task = kthread_create(ppm_main_task, NULL, "ppm_main");
	if (IS_ERR(ppm_main_info.ppm_task)) {
		ppm_err("@%s: ppm task create failed\n", __func__);
		ret = PTR_ERR(ppm_main_info.ppm_task);
		goto task_create_fail;
	}

	sched_setscheduler_nocheck(ppm_main_info.ppm_task, SCHED_FIFO, &param);
	get_task_struct(ppm_main_info.ppm_task);
	wake_up_process(ppm_main_info.ppm_task);
	ppm_info("@%s: ppm task start success, pid: %d\n", __func__,
			ppm_main_info.ppm_task->pid);
#endif

#ifdef PPM_IC_SEGMENT_CHECK
	ppm_main_info.fix_state_by_segment = ppm_check_fix_state_by_segment();
#endif

#ifdef CONFIG_MTK_RAM_CONSOLE
	/* init SRAM debug info */
	for_each_ppm_clusters(i)
		aee_rr_rec_ppm_cluster_limit(i, 0);
	aee_rr_rec_ppm_cur_state(ppm_main_info.cur_power_state);
	aee_rr_rec_ppm_min_pwr_bgt(ppm_main_info.min_power_budget);
	aee_rr_rec_ppm_policy_mask(0);
	aee_rr_rec_ppm_step(0);
	aee_rr_rec_ppm_waiting_for_pbm(0);
#endif

	ppm_info("@%s: done!\n", __func__);

	FUNC_EXIT(FUNC_LV_MAIN);

	return ret;

#if 0
task_create_fail:
	kfree(ppm_main_info.last_req.cpu_limit);
#endif

allocate_last_req_mem_fail:
	kfree(ppm_main_info.client_req.cpu_limit);

allocate_req_mem_fail:
	kfree(ppm_main_info.cluster_info);

out:
	FUNC_EXIT(FUNC_LV_MODULE);

	return ret;
}

static void ppm_main_data_deinit(void)
{
	struct ppm_policy_data *pos;

	FUNC_ENTER(FUNC_LV_MAIN);

	ppm_main_info.is_enabled = false;

	if (ppm_main_info.ppm_task) {
		kthread_stop(ppm_main_info.ppm_task);
		put_task_struct(ppm_main_info.ppm_task);
		ppm_main_info.ppm_task = NULL;
	}

	ppm_profile_exit();

	/* free policy req mem */
	list_for_each_entry(pos, &ppm_main_info.policy_list, link) {
		kfree(pos->req.limit);
	}

	kfree(ppm_main_info.client_req.cpu_limit);
	kfree(ppm_main_info.last_req.cpu_limit);
	kfree(ppm_main_info.cluster_info);

	FUNC_EXIT(FUNC_LV_MAIN);
}

static int ppm_main_pdrv_probe(struct platform_device *pdev)
{
	FUNC_ENTER(FUNC_LV_MODULE);

	ppm_info("@%s: ppm probe done!\n", __func__);

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static int ppm_main_pdrv_remove(struct platform_device *pdev)
{
	FUNC_ENTER(FUNC_LV_MODULE);

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static int __init ppm_main_init(void)
{
	int ret = 0;

	FUNC_ENTER(FUNC_LV_MODULE);

	/* ppm data init */
	ret = ppm_main_data_init();
	if (ret) {
		ppm_err("fail to init ppm data @ %s()\n", __func__);
		goto fail;
	}

#ifdef CONFIG_PROC_FS
	/* init proc */
	ret = ppm_procfs_init();
	if (ret) {
		ppm_err("fail to create ppm procfs @ %s()\n", __func__);
		goto fail;
	}
#endif /* CONFIG_PROC_FS */

	/* register platform device/driver */
	ret = platform_device_register(&ppm_main_info.ppm_pdev);
	if (ret) {
		ppm_err("fail to register ppm device @ %s()\n", __func__);
		goto fail;
	}

	ret = platform_driver_register(&ppm_main_info.ppm_pdrv);
	if (ret) {
		ppm_err("fail to register ppm driver @ %s()\n", __func__);
		goto reg_platform_driver_fail;
	}

	if (ppm_profile_init()) {
		ppm_err("@%s: ppm_profile_init fail!\n", __func__);
		ret = -EFAULT;
		goto profile_init_fail;
	}

	ppm_info("ppm driver init done!\n");

	return ret;

profile_init_fail:
	platform_driver_unregister(&ppm_main_info.ppm_pdrv);

reg_platform_driver_fail:
	platform_device_unregister(&ppm_main_info.ppm_pdev);

fail:
	ppm_main_info.is_enabled = false;
	ppm_err("ppm driver init fail!\n");

	FUNC_EXIT(FUNC_LV_MODULE);

	return ret;
}

static void __exit ppm_main_exit(void)
{
	FUNC_ENTER(FUNC_LV_MODULE);

	platform_driver_unregister(&ppm_main_info.ppm_pdrv);
	platform_device_unregister(&ppm_main_info.ppm_pdev);

	ppm_main_data_deinit();

	FUNC_EXIT(FUNC_LV_MODULE);
}

arch_initcall(ppm_main_init);
module_exit(ppm_main_exit);

MODULE_DESCRIPTION("MediaTek PPM Driver v0.1");
MODULE_LICENSE("GPL");

