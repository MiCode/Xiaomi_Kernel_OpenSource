// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/notifier.h>
/* #include <linux/regulator/consumer.h> */
/* #include <linux/device.h> */

#include "mtk_ppm_platform.h"
#include "mtk_ppm_internal.h"



int __attribute__((weak)) mt_spower_get_leakage(
	int dev, unsigned int voltage, int degree)
{
	return 0;
}

int __attribute__((weak)) get_immediate_cpuL_wrap(void)
{
	return 0;
}

int __attribute__((weak)) get_immediate_cpuB_wrap(void)
{
	return 0;
}


struct cpufreq_frequency_table mtk_dvfs_tbl_cl0[DVFS_OPP_NUM];
struct cpufreq_frequency_table mtk_dvfs_tbl_cl1[DVFS_OPP_NUM];
/* static struct device *cpu_dev[NR_PPM_CLUSTERS]; */
/* static struct regulator *proc_reg[NR_PPM_CLUSTERS]; */

/* APIs */
void mt_ppm_set_dvfs_table(unsigned int cpu,
	struct cpufreq_frequency_table *tbl,
	unsigned int num, enum dvfs_table_type type)
{
#ifdef CONFIG_CPU_FREQ
	struct ppm_data *p = &ppm_main_info;
	int i, j;
	struct cpufreq_policy *policy;

	FUNC_ENTER(FUNC_LV_API);

	for_each_ppm_clusters(i) {

		cpu = p->cluster_info[i].cpu_id;
		policy = cpufreq_cpu_get(cpu);

		/* return if table is existed */
		if (p->cluster_info[i].dvfs_tbl)
			return;

		/* ppm_lock(&(p->lock)); */

		p->dvfs_tbl_type = 0;
		/* p->cluster_info[i].dvfs_tbl = policy->freq_table; */
		p->cluster_info[i].dvfs_tbl =
			i ? mtk_dvfs_tbl_cl0 : mtk_dvfs_tbl_cl1;

		p->cluster_info[i].dvfs_opp_num =
			cpufreq_table_count_valid_entries(policy);
		/* dump dvfs table */
		ppm_info("DVFS table type = %d\n", type);
		ppm_info("DVFS table of cluster %d:\n",
			p->cluster_info[i].cluster_id);
		num = p->cluster_info[i].dvfs_opp_num;

		/* swap dvfs table for MTK reverse ordering */

		for (j = 0; j < num; j++) {
			/* swap */
			p->cluster_info[i].dvfs_tbl[j] =
						policy->freq_table[15-j];
			ppm_info("%d: %d KHz\n",
				j, p->cluster_info[i].dvfs_tbl[j].frequency);
		}

		/* ppm_unlock(&(p->lock)); */
	}
	FUNC_EXIT(FUNC_LV_API);
#endif
}

unsigned int ppm_get_cur_freq(enum ppm_cluster cluster)
{
	if (cluster >= NR_PPM_CLUSTERS)
		return 0;

	return ppm_main_info.cluster_info[cluster].freq_khz;
}

static unsigned int ppm_get_cur_volt(enum ppm_cluster cluster)
{
#ifdef PPM_TODO
	int volt;

	if (cluster >= NR_PPM_CLUSTERS)
		return 0;

	if (cpu_dev[cluster] == NULL || proc_reg[cluster] == NULL)
		return 0;

	volt = regulator_get_voltage(proc_reg[cluster]);
	if (volt < 0) {
		ppm_err("%s: invalid cl %d Vproc value: %d\n",
			__func__, cluster, volt);
		return ppm_main_info.cluster_info[cluster].volt_uv;
	}

	ppm_main_info.cluster_info[cluster].volt_uv = volt;
	ppm_warn("cl %d volt = %d uV\n", cluster,
		ppm_main_info.cluster_info[cluster].volt_uv);

	return volt;
#else
	return 1000;
#endif
}

static void ppm_get_cluster_status(struct ppm_cluster_status *cl_status)
{
	struct cpumask cluster_cpu, online_cpu;
	int i;

	for_each_ppm_clusters(i) {
		arch_get_cluster_cpus(&cluster_cpu, i);
		cpumask_and(&online_cpu, &cluster_cpu, cpu_online_mask);

		cl_status[i].core_num = cpumask_weight(&online_cpu);
		cl_status[i].volt = ppm_get_cur_volt(i) / 1000;
		cl_status[i].freq_idx =
			ppm_main_freq_to_idx(i,
				ppm_get_cur_freq(i), CPUFREQ_RELATION_L);
	}
}

#ifdef CONFIG_CPU_FREQ
static int ppm_cpu_freq_callback(struct notifier_block *nb,
			unsigned long val, void *data)
{
	struct ppm_cluster_status cl_status[NR_PPM_CLUSTERS];
	struct cpufreq_freqs *freq = data;
	int cpu = freq->cpu;
	int i, is_root_cpu = 0;

	if (freq->flags & CPUFREQ_CONST_LOOPS)
		return NOTIFY_OK;

	/* we only need to notify DLPT when root cpu's freq is changed */
	for_each_ppm_clusters(i) {
		if (cpu == ppm_main_info.cluster_info[i].cpu_id) {
			is_root_cpu = 1;
			break;
		}
	}
	if (!is_root_cpu)
		return NOTIFY_OK;

	switch (val) {
	case CPUFREQ_POSTCHANGE:
		ppm_main_info.cluster_info[i].freq_khz = freq->new;
		ppm_dbg(DLPT, "%s: POSTCHANGE!, cpu = %d, freq = %dKHz\n",
			__func__, cpu,
			ppm_main_info.cluster_info[i].freq_khz);
		ppm_get_cluster_status(cl_status);
		mt_ppm_dlpt_kick_PBM(cl_status, ppm_main_info.cluster_num);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block ppm_cpu_freq_notifier = {
	.notifier_call = ppm_cpu_freq_callback,
};
#endif


#ifdef CONFIG_THERMAL
static unsigned int ppm_get_cpu_temp(enum ppm_cluster cluster)
{
	unsigned int temp = 85;

	switch (cluster) {
	case PPM_CLUSTER_L:
		temp = get_immediate_cpuL_wrap() / 1000;
		break;
	case PPM_CLUSTER_B:
		temp = get_immediate_cpuB_wrap() / 1000;
		break;
	default:
		ppm_err("@%s: invalid cluster id = %d\n", __func__, cluster);
		break;
	}

	return temp;
}
#endif

static int ppm_get_spower_devid(enum ppm_cluster cluster)
{
	return 0;
}


int ppm_platform_init(void)
{
#ifdef PPM_TODO
	unsigned int cpu;
	int i;

	for_each_ppm_clusters(i) {
		cpu = ppm_main_info.cluster_info[i].cpu_id;
		cpu_dev[i] = get_cpu_device(cpu);
		if (!cpu_dev[i]) {
			ppm_err("failed to get cpu%d device\n", cpu);
			return 0;
		}

		proc_reg[i] = regulator_get_exclusive(cpu_dev[i], "proc");
		if (IS_ERR(proc_reg[i])) {
			ppm_err("failed to get cpu%d proc regulator\n", cpu);
			return 0;
		}
	}
#endif

#ifdef CONFIG_CPU_FREQ
	cpufreq_register_notifier(
		&ppm_cpu_freq_notifier, CPUFREQ_TRANSITION_NOTIFIER);
#endif

	return 0;
}

void ppm_update_req_by_pwr(struct ppm_policy_req *req)
{
	ppm_cobra_update_limit(req);
}

int ppm_find_pwr_idx(struct ppm_cluster_status *cluster_status)
{
	unsigned int pwr_idx = 0;
	int i;

	if (!cobra_init_done) {
		ppm_warn("@%s: cobra_init_done is 0!\n", __func__);
		return -1; /* wait cobra init */
	}

	for_each_ppm_clusters(i) {
		int core = cluster_status[i].core_num;
		int opp = cluster_status[i].freq_idx;


#ifdef CONFIG_MTK_UNIFY_POWER
		if (core > 0 && opp >= 0 && opp < DVFS_OPP_NUM) {
			pwr_idx += cobra_tbl->basic_pwr_tbl
				[CORE_NUM_L*i+core-1][opp].power_idx;
		}
#else
		pwr_idx += 100;
#endif

		ppm_ver("[%d] core = %d, opp = %d\n", i, core, opp);
	}


	if (!pwr_idx) {
		ppm_warn("@%s: pwr_idx is 0!\n", __func__);
		return -1; /* not found */
	}

	ppm_ver("@%s: pwr_idx = %d\n", __func__, pwr_idx);

	return pwr_idx;
}

int ppm_get_min_pwr_idx(void)
{
	struct ppm_cluster_status status[NR_PPM_CLUSTERS];
	int i;

	for_each_ppm_clusters(i) {
		/* use max core for ACAO */
		status[i].core_num = get_cluster_max_cpu_core(i);
		status[i].freq_idx = get_cluster_min_cpufreq_idx(i);
	}

	return ppm_find_pwr_idx(status);
}

int ppm_get_max_pwr_idx(void)
{
	struct ppm_cluster_status status[NR_PPM_CLUSTERS];
	int i;

	for_each_ppm_clusters(i) {
		status[i].core_num = get_cluster_max_cpu_core(i);
		status[i].freq_idx = get_cluster_max_cpufreq_idx(i);
	}

	return ppm_find_pwr_idx(status);
}

unsigned int ppm_calc_total_power(struct ppm_cluster_status *cluster_status,
	unsigned int cluster_num, unsigned int percentage)
{
	unsigned int dynamic, lkg, total, budget = 0;
	int i;
	ktime_t now, delta;

	for (i = 0; i < cluster_num; i++) {
		int core = cluster_status[i].core_num;
		int opp = cluster_status[i].freq_idx;

		if (core != 0 && opp >= 0 && opp < DVFS_OPP_NUM) {
			now = ktime_get();
#ifdef CONFIG_MTK_UNIFY_POWER
			dynamic =
				upower_get_power(i, opp, UPOWER_DYN) / 1000;
			lkg =
				mt_ppm_get_leakage_mw((enum ppm_cluster_lkg)i);
			total =
				((((dynamic * 100 + percentage - 1) /
					percentage) + lkg) * core) +
				((upower_get_power(
					i + NR_PPM_CLUSTERS,
					opp,
					UPOWER_DYN) +
				upower_get_power(
					i + NR_PPM_CLUSTERS,
					opp,
					UPOWER_LKG)) / 1000);
#else
			dynamic = 100;
			lkg = 50;
			total = dynamic + lkg;
#endif
			budget += total;
			delta = ktime_sub(ktime_get(), now);

			ppm_dbg(DLPT,
				"(%d):OPP/V/core/Lkg/total = %d/%d/%d/%d/%d(time = %lldus)\n",
				i, opp, cluster_status[i].volt,
				cluster_status[i].core_num,
				lkg, total, ktime_to_us(delta));
		}
	}

	if (!budget) {
		ppm_warn("@%s: pwr_idx is 0!\n", __func__);
		return -1; /* not found */
	}

	ppm_dbg(DLPT, "@%s: total budget = %d\n", __func__, budget);

	return budget;
}

unsigned int mt_ppm_get_leakage_mw(enum ppm_cluster_lkg cluster)
{
	int temp, dev_id, volt;
	unsigned int mw = 0;

	/* read total leakage */
	if (cluster >= TOTAL_CLUSTER_LKG) {
		struct ppm_cluster_status cl_status[NR_PPM_CLUSTERS];
		int i;

		ppm_get_cluster_status(cl_status);

		for_each_ppm_clusters(i) {
			if (!cl_status[i].core_num)
				continue;
#ifdef CONFIG_THERMAL
			temp = ppm_get_cpu_temp((enum ppm_cluster)i);
#else
			temp = 85;
#endif
			volt = ppm_get_cur_volt(i) / 1000;
			dev_id = ppm_get_spower_devid((enum ppm_cluster)i);
			if (dev_id < 0)
				return 0;

			mw += mt_spower_get_leakage(dev_id, volt, temp);
		}
	} else {
#ifdef CONFIG_THERMAL
		temp = ppm_get_cpu_temp((enum ppm_cluster)cluster);
#else
		temp = 85;
#endif
		volt = ppm_get_cur_volt((enum ppm_cluster)cluster) / 1000;
		dev_id = ppm_get_spower_devid((enum ppm_cluster)cluster);
		if (dev_id < 0)
			return 0;

		mw = mt_spower_get_leakage(dev_id, volt, temp);
	}

	return mw;
}
