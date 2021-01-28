// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/random.h>
#ifdef CONFIG_ARM64
#include <asm/cpu_ops.h>
#endif

#include "mtk_hps_internal.h"

/*
 * static
 */
#define STATIC
/* #define STATIC static */

/*
 * hps cpu interface - cpumask
 */
int hps_cpu_is_cpu_big(int cpu)
{
	if (!cpumask_empty(&hps_ctxt.big_cpumask)) {
		if (cpumask_test_cpu(cpu, &hps_ctxt.big_cpumask))
			return 1;
		else
			return 0;
	} else {
		return 0;
	}
}

int hps_cpu_is_cpu_little(int cpu)
{
	if (!cpumask_empty(&hps_ctxt.little_cpumask)) {
		if (cpumask_test_cpu(cpu, &hps_ctxt.little_cpumask))
			return 1;
		else
			return 0;
	} else {
		return 0;
	}
}


unsigned int num_online_little_cpus(void)
{
	struct cpumask dst_cpumask;

	cpumask_and(&dst_cpumask, &hps_ctxt.little_cpumask, cpu_online_mask);
	return cpumask_weight(&dst_cpumask);
}

unsigned int num_online_big_cpus(void)
{
	struct cpumask dst_cpumask;

	cpumask_and(&dst_cpumask, &hps_ctxt.big_cpumask, cpu_online_mask);
	return cpumask_weight(&dst_cpumask);
}

/* int hps_cpu_get_arch_type(void) */
/* { */
/* if(!cluster_numbers) */
/* return ARCH_TYPE_NO_CLUSTER; */
/* if(cpumask_empty(&hps_ctxt.little_cpumask) && */
/*cpumask_empty(&hps_ctxt.big_cpumask) ) */
/* return ARCH_TYPE_NOT_READY; */
/* if(!cpumask_empty(&hps_ctxt.little_cpumask) && */
/*!cpumask_empty(&hps_ctxt.big_cpumask)) */
/* return ARCH_TYPE_big_LITTLE; */
/* if(!cpumask_empty(&hps_ctxt.little_cpumask) && */
/*cpumask_empty(&hps_ctxt.big_cpumask)) */
/* return ARCH_TYPE_LITTLE_LITTLE; */
/* return ARCH_TYPE_NOT_READY; */
/* } */

/*
 * hps cpu interface - scheduler
 */
unsigned int hps_cpu_get_percpu_load(int cpu, int get_abs)
{
#ifdef CONFIG_MTK_SCHED_RQAVG_US
	if (!get_abs)
		return sched_get_percpu_load(cpu, 1, 1);
	else
		return sched_get_percpu_load(cpu, 1, 0);
#else
	return 100;
#endif
}

unsigned int hps_cpu_get_nr_heavy_task(void)
{
#ifdef CONFIG_MTK_SCHED_RQAVG_US
	return sched_get_nr_heavy_task();
#else
	return 0;
#endif
}

int hps_cpu_get_tlp(unsigned int *avg, unsigned int *iowait_avg)
{
	int scaled_tlp = 0; /* The scaled tasks number of the last poll  */
#ifdef CONFIG_MTK_SCHED_RQAVG_KS
	scaled_tlp = sched_get_nr_running_avg((int *)avg, (int *)iowait_avg);

	return scaled_tlp;
#else
	*avg = 0;
	*iowait_avg = 0;
	return scaled_tlp;
#endif
}

/*
 * init
 */
int hps_cpu_init(void)
{
	int r = 0;
	int i, j;
	/* char str1[32]; */
	struct cpumask cpu_mask;

	i = j = 0;
	hps_warn("%s\n", __func__);

	for (i = setup_max_cpus; i < num_possible_cpus(); i++) {
#ifdef CONFIG_ARM64
		if (!cpu_ops[i])
			WARN_ON(1);
		if (cpu_ops[i]->cpu_prepare(i))
			WARN_ON(1);
#endif
		set_cpu_present(i, true);
	}

	/* ==== New algo. definition ==== */
	hps_sys.cluster_num = (unsigned int)arch_get_nr_clusters();
	hps_warn("[New algo.] hps_sys.cluster_num %d\n", hps_sys.cluster_num);

	/* init cluster info of hps_sys */
	hps_sys.cluster_info =
		kcalloc(hps_sys.cluster_num,  sizeof(*hps_sys.cluster_info),
		GFP_KERNEL);
	if (!hps_sys.cluster_info) {
		hps_warn("@%s: fail to allocate memory for cluster_info!\n",
								__func__);
		return -ENOMEM;
	}

	for (i = 0; i < hps_sys.cluster_num; i++) {
		hps_sys.cluster_info[i].pwr_seq =
			hps_sys.cluster_info[i].cluster_id = i;
		/* get topology info of hps */
		arch_get_cluster_cpus(&cpu_mask, i);
		hps_sys.cluster_info[i].core_num = cpumask_weight(&cpu_mask);
		hps_sys.cluster_info[i].cpu_id_min = cpumask_first(&cpu_mask);
		hps_sys.cluster_info[i].cpu_id_max =
			hps_sys.cluster_info[i].cpu_id_min +
			hps_sys.cluster_info[i].core_num - 1;

		/* setting initial value */
		if (!hps_sys.is_set_root_cluster) {
			hps_sys.cluster_info[i].is_root = 1;
			hps_sys.root_cluster_id = i;
			hps_sys.is_set_root_cluster = 1;
		}
		hps_sys.cluster_info[i].limit_value =
			hps_sys.cluster_info[i].core_num;
		hps_sys.cluster_info[i].base_value = 0;
		hps_sys.cluster_info[i].target_core_num = 0;
		hps_sys.cluster_info[i].hvyTsk_value = 0;
		hps_sys.cluster_info[i].up_threshold = DEF_CPU_UP_THRESHOLD;
		hps_sys.cluster_info[i].down_threshold =
			DEF_CPU_DOWN_THRESHOLD;
		for (j = 1; j <= 4; j++) {
			if (j == 1)
				hps_sys.cluster_info[i].down_times[j] =
				hps_sys.cluster_info[i].down_time_val[j] =
					DEF_ROOT_CPU_DOWN_TIMES;
			else
				hps_sys.cluster_info[i].down_times[j] =
				hps_sys.cluster_info[i].down_time_val[j] =
					DEF_CPU_DOWN_TIMES;
		}
	}

	/* Change default power sequence of HPS*/
	if (hps_sys.cluster_num == 2) {
		hps_sys.cluster_info[0].pwr_seq = 0;
		hps_sys.cluster_info[1].pwr_seq = 1;
	}
	if (hps_sys.cluster_num == 3) {
		hps_sys.cluster_info[0].pwr_seq = 0;
		hps_sys.cluster_info[1].pwr_seq = 2;
		hps_sys.cluster_info[2].pwr_seq = 1;
	}

	/*
	 * For EAS evaluation
	 */
	/* LL: relative threshold, for more cores + low frequency policy */
	hps_sys.cluster_info[0].up_threshold = DEF_EAS_UP_THRESHOLD_0;
	hps_sys.cluster_info[0].down_threshold = DEF_EAS_DOWN_THRESHOLD_0;

	/* L: absolute threshold */
	if (hps_sys.cluster_num == 2) {
		hps_sys.cluster_info[1].up_threshold = DEF_EAS_UP_THRESHOLD_1;
		hps_sys.cluster_info[1].down_threshold =
			DEF_EAS_DOWN_THRESHOLD_1;
	}

	if (hps_sys.cluster_num == 3) {
		/* B: absolute threshold */
		hps_sys.cluster_info[2].up_threshold = DEF_EAS_UP_THRESHOLD_2;
		hps_sys.cluster_info[2].down_threshold =
			DEF_EAS_DOWN_THRESHOLD_2;
	}
	hps_ops_init();

	return r;
}

/*
 * deinit
 */
int hps_cpu_deinit(void)
{
	int r = 0;

	hps_warn("%s\n", __func__);

	return r;
}
