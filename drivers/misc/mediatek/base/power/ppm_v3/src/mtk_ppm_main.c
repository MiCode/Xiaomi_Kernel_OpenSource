// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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
#include <linux/ktime.h>
#include <linux/string.h>
#include <linux/topology.h>
#include "mtk_ppm_internal.h"
#include <trace/events/mtk_events.h>
#include <linux/of.h>

/*==============================================================*/
/* Local Macros                                                 */
/*==============================================================*/
#define LOG_BUF_SIZE		(128)
#define LOG_CHECK_INTERVAL	(500)/* ms */
#define LOG_MAX_CNT		(5) /* max log cnt within a check interval */
#define LOG_MAX_DIFF_INTERVAL	(100)/* ms */

/*==============================================================*/
/* Local variables                                              */
/*==============================================================*/
/* log filter parameters to avoid log too much issue */
static ktime_t prev_log_time;
static ktime_t prev_check_time;
static unsigned int log_cnt;
static unsigned int filter_cnt;
/* force update limit to HPS since it's not ready at previous round */
static bool force_update_to_hps;
static bool is_in_game;

/*==============================================================*/
/* Local function declarition					*/
/*==============================================================*/
static int ppm_main_suspend(struct device *dev);
static int ppm_main_resume(struct device *dev);
static int ppm_main_pdrv_probe(struct platform_device *pdev);
static int ppm_main_pdrv_remove(struct platform_device *pdev);

/*==============================================================*/
/* Global variables                                             */
/*==============================================================*/
struct ppm_data ppm_main_info = {
	.is_enabled = true,
	.is_doe_enabled = 0,
	.is_in_suspend = false,
	.min_power_budget = ~0,
	.dvfs_tbl_type = DVFS_TABLE_TYPE_FY,

	.ppm_pm_ops = {
		.suspend = ppm_main_suspend,
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
};

int ppm_main_freq_to_idx(unsigned int cluster_id,
			unsigned int freq, unsigned int relation)
{
	struct ppm_data *p = &ppm_main_info;
	int i, size;
	int idx = -1;

	FUNC_ENTER(FUNC_LV_MAIN);

	if (!p->cluster_info[cluster_id].dvfs_tbl) {
		ppm_err("@%s: DVFS table of cluster %d is not exist!\n",
			__func__, cluster_id);
		idx = (relation == CPUFREQ_RELATION_L)
			? get_cluster_min_cpufreq_idx(cluster_id)
			: get_cluster_max_cpufreq_idx(cluster_id);
		return idx;
	}

	size = p->cluster_info[cluster_id].dvfs_opp_num;

	/* error handle */
	if (freq > get_cluster_max_cpufreq(cluster_id))
		freq = p->cluster_info[cluster_id].dvfs_tbl[0].frequency;

	if (freq < get_cluster_min_cpufreq(cluster_id))
		freq = p->cluster_info[cluster_id].dvfs_tbl[size-1].frequency;

	/* search idx */
	if (relation == CPUFREQ_RELATION_L) {
		for (i = (signed int)(size - 1); i >= 0; i--) {
			if (p->cluster_info[cluster_id].dvfs_tbl[i].frequency
				>= freq) {
				idx = i;
				break;
			}
		}
	} else { /* CPUFREQ_RELATION_H */
		for (i = 0; i < (signed int)size; i++) {
			if (p->cluster_info[cluster_id].dvfs_tbl[i].frequency
				<= freq) {
				idx = i;
				break;
			}
		}
	}

	if (idx == -1) {
		ppm_ver("freq %d KHz not found in DVFS table of cluster %d\n",
			freq, cluster_id);
		idx = (relation == CPUFREQ_RELATION_L)
			? get_cluster_min_cpufreq_idx(cluster_id)
			: get_cluster_max_cpufreq_idx(cluster_id);
	} else
		ppm_ver("@%s: The idx of %d KHz in cluster %d is %d\n",
			__func__, freq, cluster_id, idx);

	FUNC_EXIT(FUNC_LV_MAIN);

	return idx;
}

void ppm_clear_policy_limit(struct ppm_policy_data *policy)
{
	unsigned int i;

	FUNC_ENTER(FUNC_LV_MAIN);

	for (i = 0; i < policy->req.cluster_num; i++) {
		/* min = max for ACAO */
		policy->req.limit[i].min_cpu_core =
			get_cluster_max_cpu_core(i);
		policy->req.limit[i].max_cpu_core =
			get_cluster_max_cpu_core(i);
		policy->req.limit[i].min_cpufreq_idx =
			get_cluster_min_cpufreq_idx(i);
		policy->req.limit[i].max_cpufreq_idx =
			get_cluster_max_cpufreq_idx(i);
	}

	FUNC_EXIT(FUNC_LV_MAIN);
}

void ppm_main_clear_client_req(struct ppm_client_req *c_req)
{
	int i;

	for (i = 0; i < c_req->cluster_num; i++) {
		c_req->cpu_limit[i].min_cpufreq_idx =
			get_cluster_min_cpufreq_idx(i);
		c_req->cpu_limit[i].max_cpufreq_idx =
			get_cluster_max_cpufreq_idx(i);
		/* min = max for ACAO */
		c_req->cpu_limit[i].min_cpu_core = get_cluster_max_cpu_core(i);
		c_req->cpu_limit[i].max_cpu_core = get_cluster_max_cpu_core(i);
		c_req->cpu_limit[i].has_advise_freq = false;
		c_req->cpu_limit[i].advise_cpufreq_idx = -1;
		c_req->cpu_limit[i].has_advise_core = false;
		c_req->cpu_limit[i].advise_cpu_core = -1;
	}
}

int ppm_main_register_policy(struct ppm_policy_data *policy)
{
	struct list_head *pos;
	int ret = 0, i;

	FUNC_ENTER(FUNC_LV_MAIN);

	ppm_lock(&ppm_main_info.lock);

	/* init remaining members in policy data */
	policy->req.limit = kcalloc(ppm_main_info.cluster_num,
		sizeof(*policy->req.limit), GFP_KERNEL);
	if (!policy->req.limit) {
		ret = -ENOMEM;
		goto out;
	}
	policy->req.cluster_num = ppm_main_info.cluster_num;
	policy->req.power_budget = 0;
	policy->req.perf_idx = 0;
	/* init default limit */
	for (i = 0; i < policy->req.cluster_num; i++) {
		policy->req.limit[i].min_cpufreq_idx =
			get_cluster_min_cpufreq_idx(i);
		policy->req.limit[i].max_cpufreq_idx =
			get_cluster_max_cpufreq_idx(i);
		/* min = max for ACAO */
		policy->req.limit[i].min_cpu_core =
			get_cluster_max_cpu_core(i);
		policy->req.limit[i].max_cpu_core =
			get_cluster_max_cpu_core(i);
	}

	/* insert into global policy_list according to its priority */
	list_for_each(pos, &ppm_main_info.policy_list) {
		struct ppm_policy_data *data;

		data = list_entry(pos, struct ppm_policy_data, link);
		if (policy->priority > data->priority  ||
			(policy->priority == data->priority
				&& policy->policy > data->policy))
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
			struct ppm_client_limit *c_limit,
			struct ppm_cluster_limit *p_limit)
{
	FUNC_ENTER(FUNC_LV_MAIN);

	ppm_ver("Policy --> (%d)(%d)(%d)(%d)\n",
		p_limit->min_cpufreq_idx,
		p_limit->max_cpufreq_idx,
		p_limit->min_cpu_core,
		p_limit->max_cpu_core);
	ppm_ver("Original --> (%d)(%d)(%d)(%d) (%d)(%d)(%d)(%d)\n",
		c_limit->min_cpufreq_idx, c_limit->max_cpufreq_idx,
		c_limit->min_cpu_core, c_limit->max_cpu_core,
		c_limit->has_advise_freq, c_limit->advise_cpufreq_idx,
		c_limit->has_advise_core, c_limit->advise_cpu_core);

	switch (p->policy) {
	/* fix freq */
	case PPM_POLICY_PTPOD:
		c_limit->has_advise_freq = true;
		c_limit->advise_cpufreq_idx = p_limit->max_cpufreq_idx;
		c_limit->min_cpufreq_idx =
			c_limit->max_cpufreq_idx = p_limit->max_cpufreq_idx;
		break;
	/* fix freq and core */
	case PPM_POLICY_UT:
		if (p_limit->min_cpufreq_idx == p_limit->max_cpufreq_idx) {
			c_limit->has_advise_freq = true;
			c_limit->advise_cpufreq_idx = p_limit->max_cpufreq_idx;
			c_limit->min_cpufreq_idx = p_limit->max_cpufreq_idx;
			c_limit->max_cpufreq_idx = p_limit->max_cpufreq_idx;
		}

		if (p_limit->min_cpu_core == p_limit->max_cpu_core) {
			c_limit->has_advise_core = true;
			c_limit->advise_cpu_core = p_limit->max_cpu_core;
			c_limit->min_cpu_core = p_limit->max_cpu_core;
			c_limit->max_cpu_core = p_limit->max_cpu_core;
		}
		break;
	default:
		/* out of range! use policy's min/max cpufreq idx setting */
		if (c_limit->min_cpufreq_idx <  p_limit->max_cpufreq_idx ||
			c_limit->max_cpufreq_idx >  p_limit->min_cpufreq_idx) {
			/* no need to set min freq for power budget policy */
			if (p->priority != PPM_POLICY_PRIO_POWER_BUDGET_BASE)
				c_limit->min_cpufreq_idx =
					p_limit->min_cpufreq_idx;
			c_limit->max_cpufreq_idx = p_limit->max_cpufreq_idx;
		} else {
			c_limit->min_cpufreq_idx =
				MIN(c_limit->min_cpufreq_idx,
					p_limit->min_cpufreq_idx);
			c_limit->max_cpufreq_idx =
				MAX(c_limit->max_cpufreq_idx,
					p_limit->max_cpufreq_idx);
		}

		/* out of range! use policy's min/max cpu core setting */
		if (c_limit->min_cpu_core >  p_limit->max_cpu_core ||
			c_limit->max_cpu_core <  p_limit->min_cpu_core) {
			/* no need to set min core for power budget policy */
			if (p->priority != PPM_POLICY_PRIO_POWER_BUDGET_BASE)
				c_limit->min_cpu_core = p_limit->min_cpu_core;
			c_limit->max_cpu_core = p_limit->max_cpu_core;
		} else {
			c_limit->min_cpu_core =
				MAX(c_limit->min_cpu_core,
					p_limit->min_cpu_core);
			c_limit->max_cpu_core =
				MIN(c_limit->max_cpu_core,
					p_limit->max_cpu_core);
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
		c_limit->min_cpufreq_idx, c_limit->max_cpufreq_idx,
		c_limit->min_cpu_core, c_limit->max_cpu_core,
		c_limit->has_advise_freq, c_limit->advise_cpufreq_idx,
		c_limit->has_advise_core, c_limit->advise_cpu_core);

	FUNC_EXIT(FUNC_LV_MAIN);
}

static void ppm_main_calc_new_limit(void)
{
	struct ppm_policy_data *pos;
	int i;
	unsigned int max_freq_limit[NR_PPM_CLUSTERS] = {0};
	bool is_ptp_activate = false, is_all_cluster_zero = true;
	struct ppm_client_req *c_req = &(ppm_main_info.client_req);
	struct ppm_client_req *last_req = &(ppm_main_info.last_req);

	FUNC_ENTER(FUNC_LV_MAIN);

	/* set default limit */
	ppm_main_clear_client_req(c_req);

	ppm_main_info.min_power_budget = ~0;

	/* traverse policy list to get the final limit to client */
	list_for_each_entry(pos, &ppm_main_info.policy_list, link) {
		ppm_lock(&pos->lock);

		if (pos->is_enabled && pos->is_activated
			&& pos->is_limit_updated) {
			pos->is_limit_updated = false;

			for_each_ppm_clusters(i) {
				ppm_ver("apply policy %s cluster %d limit\n",
					pos->name, i);
				ppm_main_update_limit(pos,
					&c_req->cpu_limit[i],
					&pos->req.limit[i]);

				/* calculate max freq limit except userlimit */
				if (pos->policy != PPM_POLICY_USER_LIMIT)
					max_freq_limit[i] = MAX(
					max_freq_limit[i],
					pos->req.limit[i].max_cpufreq_idx);
			}

			is_ptp_activate = (pos->policy == PPM_POLICY_PTPOD)
				? true : false;

			/* calculate min power budget */
			switch (pos->policy) {
			case PPM_POLICY_THERMAL:
			case PPM_POLICY_DLPT:
			case PPM_POLICY_PWR_THRO:
				if (pos->req.power_budget)
					ppm_main_info.min_power_budget = MIN(
					pos->req.power_budget,
					ppm_main_info.min_power_budget);
				break;
			default:
				break;
			}
		}

		ppm_unlock(&pos->lock);
	}

	for_each_ppm_clusters(i)
		ppm_main_info.cluster_info[i].max_freq_except_userlimit
			= max_freq_limit[i];

	/* set freq idx to previous limit if nr_cpu in the cluster is 0 */
	for (i = 0; i < c_req->cluster_num; i++) {
		if (c_req->cpu_limit[i].max_cpu_core)
			is_all_cluster_zero = false;

		if ((!c_req->cpu_limit[i].min_cpu_core
			&& !c_req->cpu_limit[i].max_cpu_core)
			|| (c_req->cpu_limit[i].has_advise_core
			&& !c_req->cpu_limit[i].advise_cpu_core)) {
			c_req->cpu_limit[i].min_cpufreq_idx =
				last_req->cpu_limit[i].min_cpufreq_idx;
			c_req->cpu_limit[i].max_cpufreq_idx =
				last_req->cpu_limit[i].max_cpufreq_idx;
			c_req->cpu_limit[i].has_advise_freq =
				last_req->cpu_limit[i].has_advise_freq;
			c_req->cpu_limit[i].advise_cpufreq_idx =
				last_req->cpu_limit[i].advise_cpufreq_idx;
		}

		ppm_cobra_update_freq_limit(i,
			c_req->cpu_limit[i].max_cpufreq_idx);
#if PPM_COBRA_USE_CORE_LIMIT
		ppm_cobra_update_core_limit(i,
			c_req->cpu_limit[i].max_cpu_core);
#endif

		ppm_ver("Result:[%d]-->(%d)(%d)(%d)(%d) (%d)(%d)(%d)(%d)\n",
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

	/* always = 0 for ACAO */
	c_req->root_cluster = 0;

	/* fill ptpod activate flag */
	c_req->is_ptp_policy_activate = is_ptp_activate;

	/* DoE */
	if (!is_ptp_activate &&
			ppm_main_info.client_info[PPM_CLIENT_DVFS].limit_cb) {
		if (ppm_main_info.is_doe_enabled == 1) {
			for_each_ppm_clusters(i) {
				pr_debug_ratelimited(
					"[DoE] cl: %d max: %d min: %d\n",
					i,
					ppm_main_info.cluster_info[i].doe_max,
					ppm_main_info.cluster_info[i].doe_min
					);
				c_req->cpu_limit[i].max_cpufreq_idx =
					ppm_main_info.cluster_info[i].doe_max;
				c_req->cpu_limit[i].min_cpufreq_idx =
					ppm_main_info.cluster_info[i].doe_min;
				c_req->cpu_limit[i].has_advise_freq = false;
				c_req->cpu_limit[i].advise_cpufreq_idx = -1;
			}
		}
	}
	/* DoE */

	/* Trigger exception if all cluster max core limit is 0 */
	if (is_all_cluster_zero) {
		struct ppm_policy_data *pos;
		unsigned int i = 0;

		ppm_err("all cluster max core are 0, dump active policy:\n");
		list_for_each_entry(pos, &ppm_main_info.policy_list, link) {
			ppm_lock(&pos->lock);
			if (pos->is_activated) {
				ppm_info("[%d]%s:perf_idx=%d, pwr_bdgt=%d\n",
						pos->policy, pos->name,
						pos->req.perf_idx,
						pos->req.power_budget);
				for_each_ppm_clusters(i) {
					ppm_info("cl%d:(%d)(%d)(%d)(%d)\n",
					i,
					pos->req.limit[i].min_cpufreq_idx,
					pos->req.limit[i].max_cpufreq_idx,
					pos->req.limit[i].min_cpu_core,
					pos->req.limit[i].max_cpu_core);
				}
				ppm_info("\n");
			}
			ppm_unlock(&pos->lock);
		}

		WARN_ON(1);
	} else {
		/* update online core mask for hotplug */
		int j, k = 0;
		int nr_present_cpu = num_present_cpus();

		cpumask_clear(c_req->online_core);

		for (i = 0; i < c_req->cluster_num; i++) {
			for (j = 0; j < c_req->cpu_limit[i].max_cpu_core; j++)
				cpumask_set_cpu(k + j, c_req->online_core);
			k += ppm_main_info.cluster_info[i].core_num;
		}

		if (cpumask_weight(c_req->online_core) == nr_present_cpu ||
			!cpumask_weight(ppm_main_info.exclusive_core))
			goto end;

		/* check exclusive core */
		for (i = nr_present_cpu-1; i > 0; i--) {
			if (i % 4 == 0
				|| cpumask_test_cpu(i, c_req->online_core)
				|| !cpumask_test_cpu(i,
				ppm_main_info.exclusive_core))
				continue;

			/* find next online cpu in the same cluster */
			j = i - 1;
			do {
				/* find candidate to replace exclusive core */
				if (cpumask_test_cpu(j, c_req->online_core)
					&& !cpumask_test_cpu(j,
					ppm_main_info.exclusive_core)) {
					cpumask_clear_cpu(j,
						c_req->online_core);
					cpumask_set_cpu(i,
						c_req->online_core);
					break;
				}
				j--;
			} while (j/4 == i/4 && j >= 0);
		}
	}

end:
	FUNC_EXIT(FUNC_LV_MAIN);
}

void ppm_game_mode_change_cb(int is_game_mode)
{
	is_in_game = 0;
}

static void ppm_main_log_print(unsigned int policy_mask,
	unsigned int min_power_budget, unsigned int root_cluster, char *msg)
{
	bool filter_log;
	ktime_t cur_time = ktime_get();
	unsigned long long delta1, delta2;

	delta1 = ktime_to_ms(ktime_sub(cur_time, prev_check_time));
	delta2 = ktime_to_ms(ktime_sub(cur_time, prev_log_time));

	if (delta1 >= LOG_CHECK_INTERVAL
		|| delta2 >= LOG_MAX_DIFF_INTERVAL) {
		prev_check_time = cur_time;
		filter_log = false;
		log_cnt = 1;
		if (filter_cnt) {
			ppm_info("Shrink %d PPM logs from last %lld ms!\n",
				filter_cnt, delta1);
			filter_cnt = 0;
		}
	} else if (log_cnt < LOG_MAX_CNT) {
		filter_log = false;
		log_cnt++;
	} else {
		/* filter log */
		filter_log = true;
		filter_cnt++;
	}

	if (!filter_log)
		ppm_info("(0x%x)(%d)(%d)%s\n", policy_mask,
			min_power_budget, root_cluster, msg);
	else
		ppm_ver("(0x%x)(%d)(%d)%s\n", policy_mask,
			min_power_budget, root_cluster, msg);

	prev_log_time = cur_time;
}

int mt_ppm_main(void)
{
	struct ppm_policy_data *pos;
	struct ppm_client_req *c_req = &(ppm_main_info.client_req);
	struct ppm_client_req *last_req = &(ppm_main_info.last_req);
	unsigned int policy_mask = 0;
	int i;
	ktime_t now;
	unsigned long long delta;

	FUNC_ENTER(FUNC_LV_MAIN);

	ppm_lock(&ppm_main_info.lock);

	if (!ppm_main_info.is_enabled || ppm_main_info.is_in_suspend)
		goto end;

#if TODO /* TODO will remove later */
	if (!ppm_main_info.client_info[PPM_CLIENT_DVFS].limit_cb ||
		!ppm_main_info.client_info[PPM_CLIENT_HOTPLUG].limit_cb) {
		ppm_info("dvfs/hps clients not yet registed!\n");
		goto end;
	}
#endif /* TODO will remove later */

#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_ppm_step(1);
#endif

	/* update active policy's limit according to current state */
	list_for_each_entry(pos, &ppm_main_info.policy_list, link) {
		if ((pos->is_activated)
			&& pos->update_limit_cb) {
			int idx;

			ppm_lock(&pos->lock);
			policy_mask |= 1 << pos->policy;
			pos->update_limit_cb();
			pos->is_limit_updated = true;

			for (idx = 0; idx < pos->req.cluster_num; idx++) {
				trace_ppm_user_setting(
					pos->policy,
					idx,
					pos->req.limit[idx].min_cpufreq_idx,
					pos->req.limit[idx].max_cpufreq_idx
				);
			}

			ppm_unlock(&pos->lock);
		}
	}

#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_ppm_step(2);
#endif

	/* calculate final limit and fill-in client request structure */
	ppm_main_calc_new_limit();

#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_ppm_step(3);
#endif

	/* notify client and print debug message if limits are changed */
	if (memcmp(c_req->cpu_limit, last_req->cpu_limit,
		ppm_main_info.cluster_num * sizeof(*c_req->cpu_limit))
		|| !cpumask_equal(c_req->online_core, last_req->online_core)) {
		char buf[LOG_BUF_SIZE];
		char *ptr = buf;

		/* print debug message */
		ptr += snprintf(ptr, LOG_BUF_SIZE, "(%*pbl)",
			cpumask_pr_args(c_req->online_core));
		for (i = 0; i < c_req->cluster_num; i++) {
			ptr += snprintf(ptr, LOG_BUF_SIZE, "(%d)(%d)(%d)(%d) ",
				c_req->cpu_limit[i].min_cpufreq_idx,
				c_req->cpu_limit[i].max_cpufreq_idx,
				c_req->cpu_limit[i].min_cpu_core,
				c_req->cpu_limit[i].max_cpu_core
			);

			if (c_req->cpu_limit[i].has_advise_freq
				|| c_req->cpu_limit[i].has_advise_core)
				ptr += snprintf(ptr, LOG_BUF_SIZE,
					"[(%d)(%d)(%d)(%d)] ",
					c_req->cpu_limit[i].has_advise_freq,
					c_req->cpu_limit[i].advise_cpufreq_idx,
					c_req->cpu_limit[i].has_advise_core,
					c_req->cpu_limit[i].advise_cpu_core
				);
		}

#ifndef NO_MTK_TRACE
		trace_ppm_update(policy_mask,
			ppm_main_info.min_power_budget,
				c_req->root_cluster, buf);
#endif

#ifdef CONFIG_MTK_RAM_CONSOLE
		for (i = 0; i < c_req->cluster_num; i++) {
			aee_rr_rec_ppm_cluster_limit(i,
				(c_req->cpu_limit[i].min_cpufreq_idx << 24) |
				(c_req->cpu_limit[i].max_cpufreq_idx << 16) |
				(c_req->cpu_limit[i].min_cpu_core << 8) |
				(c_req->cpu_limit[i].max_cpu_core)
			);
		}
		aee_rr_rec_ppm_min_pwr_bgt(ppm_main_info.min_power_budget);
		aee_rr_rec_ppm_policy_mask(policy_mask);
#endif

#ifdef PPM_SSPM_SUPPORT
#ifdef CONFIG_MTK_RAM_CONSOLE
		aee_rr_rec_ppm_step(4);
#endif
		/* update limit to SSPM first */
		ppm_ipi_update_limit(*c_req);
#ifdef CONFIG_MTK_RAM_CONSOLE
		aee_rr_rec_ppm_step(5);
#endif
#endif

		{
			struct ppm_data *p = &ppm_main_info;
			bool notify_hps = false;
			bool notify_dvfs = false;
			bool log_print = false;
			int to;

			for (i = 0; i < c_req->cluster_num; i++) {
				if ((c_req->cpu_limit[i].min_cpu_core
				!= last_req->cpu_limit[i].min_cpu_core)
				|| (c_req->cpu_limit[i].max_cpu_core
				!= last_req->cpu_limit[i].max_cpu_core)
				|| force_update_to_hps) {
					notify_hps = true;
					log_print = true;
					force_update_to_hps = 0;
				}
				if ((c_req->cpu_limit[i].min_cpufreq_idx
				!= last_req->cpu_limit[i].min_cpufreq_idx)
				|| (c_req->cpu_limit[i].max_cpufreq_idx
				!= last_req->cpu_limit[i].max_cpufreq_idx)
				|| c_req->cpu_limit[i].has_advise_freq) {
					notify_dvfs = true;
					log_print = true;
				}

				if (notify_hps && notify_dvfs)
					break;
			}

			/* notify needed client only */
			if (notify_dvfs && !notify_hps) {
				to = PPM_CLIENT_DVFS;
				now = ktime_get();
				if (log_print)
					ppm_main_log_print(policy_mask,
						p->min_power_budget,
						c_req->root_cluster, buf);
				if (!p->client_info[to].limit_cb)
					goto nofity_end;

				p->client_info[to].limit_cb(*c_req);
				delta = ktime_to_us(
					ktime_sub(ktime_get(), now));
				ppm_profile_update_client_exec_time(to, delta);
				ppm_dbg(TIME_PROFILE,
					"notify dvfs time = %lld us\n", delta);
				goto nofity_end;
			} else if (notify_hps && !notify_dvfs) {
				to = PPM_CLIENT_HOTPLUG;
				if (log_print)
					ppm_main_log_print(policy_mask,
						ppm_main_info.min_power_budget,
						c_req->root_cluster, buf);
				now = ktime_get();

				if (!p->client_info[to].limit_cb) {
					/* force update to HPS next time */
					force_update_to_hps = 1;
					goto nofity_end;
				}

				ppm_main_info.client_info[to].limit_cb(*c_req);
				delta = ktime_to_us(
					ktime_sub(ktime_get(), now));
				ppm_profile_update_client_exec_time(to, delta);
				ppm_dbg(TIME_PROFILE,
					"notify hps time = %lld us\n", delta);
				goto nofity_end;
			}
		}

		ppm_main_log_print(policy_mask, ppm_main_info.min_power_budget,
				c_req->root_cluster, buf);

		/* send request to client */
		for_each_ppm_clients(i) {
			now = ktime_get();
			if (ppm_main_info.client_info[i].limit_cb)
				ppm_main_info.client_info[i].limit_cb(*c_req);
			else if (i == PPM_CLIENT_HOTPLUG)
				force_update_to_hps = 1;
			delta = ktime_to_us(ktime_sub(ktime_get(), now));
			ppm_profile_update_client_exec_time(i, delta);
			ppm_dbg(TIME_PROFILE,
				"%s callback done! time = %lld us\n",
				(i == PPM_CLIENT_DVFS)
				? "DVFS" : "HPS", delta);
		}

nofity_end:
		if (ppm_main_info.client_info[PPM_CLIENT_DVFS].limit_cb)
			memcpy(last_req->cpu_limit, c_req->cpu_limit,
			ppm_main_info.cluster_num * sizeof(*c_req->cpu_limit));
		if (ppm_main_info.client_info[PPM_CLIENT_HOTPLUG].limit_cb)
			cpumask_copy(last_req->online_core,
				c_req->online_core);
	}


#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_ppm_step(0);
#endif

end:
	ppm_unlock(&ppm_main_info.lock);

	FUNC_EXIT(FUNC_LV_MAIN);

	return 0;
}

static int ppm_main_suspend(struct device *dev)
{
	FUNC_ENTER(FUNC_LV_MODULE);

	ppm_info("%s: suspend callback in\n", __func__);

	ppm_lock(&ppm_main_info.lock);
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
#ifndef NO_SCHEDULE_API
	struct cpumask cpu_mask;
#endif
	int ret = 0;
	int i;

	FUNC_ENTER(FUNC_LV_MAIN);

	/* get cluster num */
#ifndef NO_SCHEDULE_API
	ppm_main_info.cluster_num = (unsigned int)arch_get_nr_clusters();
#else
	ppm_main_info.cluster_num = NR_PPM_CLUSTERS;
#endif
	ppm_info("cluster_num = %d\n", ppm_main_info.cluster_num);

	/* init exclusive core */
	cpumask_clear(ppm_main_info.exclusive_core);

	/* init cluster info */
	ppm_main_info.cluster_info =
		kcalloc(ppm_main_info.cluster_num,
			sizeof(*ppm_main_info.cluster_info), GFP_KERNEL);
	if (!ppm_main_info.cluster_info) {
		ppm_err("fail to allocate memory for cluster_info!\n");
		ret = -ENOMEM;
		goto out;
	}
	for_each_ppm_clusters(i) {
		ppm_main_info.cluster_info[i].cluster_id = i;
		/* OPP num will update after DVFS set table */
		ppm_main_info.cluster_info[i].dvfs_opp_num = DVFS_OPP_NUM;
		ppm_main_info.cluster_info[i].max_freq_except_userlimit = 0;

		/* get topology info */
#ifndef NO_SCHEDULE_API
		arch_get_cluster_cpus(&cpu_mask, i);
		ppm_main_info.cluster_info[i].core_num =
			cpumask_weight(&cpu_mask);
		ppm_main_info.cluster_info[i].cpu_id =
			cpumask_first(&cpu_mask);
#else
		ppm_main_info.cluster_info[i].core_num =
			get_cluster_cpu_core(i);
		if (i > 0)
			ppm_main_info.cluster_info[i].cpu_id =
				ppm_main_info.cluster_info[i-1].cpu_id +
				get_cluster_cpu_core(i-1);
		else
			ppm_main_info.cluster_info[i].cpu_id = 0;
#endif
		ppm_info("ppm cluster %d -> core_num = %d, cpu_id = %d\n",
				ppm_main_info.cluster_info[i].cluster_id,
				ppm_main_info.cluster_info[i].core_num,
				ppm_main_info.cluster_info[i].cpu_id
				);
	}

	/* init client request */
	ppm_main_info.client_req.cpu_limit =
		kzalloc(ppm_main_info.cluster_num
			* sizeof(*ppm_main_info.client_req.cpu_limit),
			GFP_KERNEL);
	if (!ppm_main_info.client_req.cpu_limit) {
		ppm_err("fail to allocate memory client_req!\n");
		ret = -ENOMEM;
		goto allocate_req_mem_fail;
	}

	ppm_main_info.last_req.cpu_limit =
		kzalloc(ppm_main_info.cluster_num
			* sizeof(*ppm_main_info.last_req.cpu_limit),
			GFP_KERNEL);
	if (!ppm_main_info.last_req.cpu_limit) {
		ppm_err("fail to allocate memory for last_req!\n");
		ret = -ENOMEM;
		goto allocate_last_req_mem_fail;
	}

	for_each_ppm_clusters(i) {
		ppm_main_info.client_req.cluster_num =
			ppm_main_info.cluster_num;
		ppm_main_info.client_req.root_cluster = 0;
		ppm_main_info.client_req.cpu_limit[i].cluster_id = i;
		ppm_main_info.client_req.cpu_limit[i].cpu_id =
			ppm_main_info.cluster_info[i].cpu_id;

		ppm_main_info.last_req.cluster_num =
			ppm_main_info.cluster_num;
	}

#ifdef CONFIG_MTK_RAM_CONSOLE
	/* init SRAM debug info */
	for_each_ppm_clusters(i)
		aee_rr_rec_ppm_cluster_limit(i, 0);
	aee_rr_rec_ppm_min_pwr_bgt(ppm_main_info.min_power_budget);
	aee_rr_rec_ppm_policy_mask(0);
	aee_rr_rec_ppm_step(0);
	aee_rr_rec_ppm_waiting_for_pbm(0);
#endif

	ppm_info("@%s: done!\n", __func__);

	FUNC_EXIT(FUNC_LV_MAIN);

	return ret;

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
	struct device_node *cn, *map, *c, *d;
	int max, min;
	char name[10];
	int i = 0;

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

	/* DoE */
	cn = of_find_node_by_path("/cpus");

	if (!cn)
		goto NO_DOE;

	map = of_get_child_by_name(cn, "virtual-cpu-map");

	if (!map) {
		map = of_get_child_by_name(cn, "cpu-map");

		if (!map)
			goto NO_DOE;
	}

	i = 0;

	do {
		snprintf(name, sizeof(name), "cluster%d", i);
		c = of_get_child_by_name(map, name);

		if (!c)
			goto NO_DOE;

		d = of_get_child_by_name(c, "doe");

		if (!d)
			goto NO_DOE;

		ret = of_property_read_u32(d, "max", &max);

		if (ret != 0)
			goto NO_DOE;

		ret = of_property_read_u32(d, "min", &min);

		if (ret != 0)
			goto NO_DOE;

		of_node_put(d);
		of_node_put(c);

		ppm_main_info.cluster_info[i].doe_max = max;
		ppm_main_info.cluster_info[i].doe_min = min;
		ppm_main_info.is_doe_enabled = 1;

		i++;

	} while (i < NR_PPM_CLUSTERS);

	of_node_put(map);

	ppm_info("DoE: %d\n", ppm_main_info.is_doe_enabled);
	i = 0;

	do {
		ppm_info("cl: %d max: %d min: %d\n",
			i,
			ppm_main_info.cluster_info[i].doe_max,
			ppm_main_info.cluster_info[i].doe_min);
		i++;
	} while (i < NR_PPM_CLUSTERS);

	/* DoE */

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
NO_DOE:
	of_node_put(cn);
	of_node_put(map);
	of_node_put(c);
	of_node_put(d);

	ppm_main_info.is_doe_enabled = 0;
	ppm_info("ppm driver init done (no DoE)!\n");

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

