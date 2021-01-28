/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/preempt.h>
#include <linux/trace_events.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/module.h>

#include "mtk_upower.h"
#include <trace/events/fpsgo.h>

#include <mt-plat/mtk_perfobserver.h>
#include "pob_qos.h"

#include <linux/cpufreq.h>
#include "mtk_ppm_api.h"

#include <mt-plat/mtk_gpu_utility.h>
#include <mtk_gpufreq.h>

#include "rs_usage.h"
#include "rs_pfm.h"
#include "rs_trace.h"
#include "rs_base.h"

#define TAG "RSUsage"

//#define __BW_USE_EMIALL__ 1

#define RSU_SYSTRACE_LIST(macro) \
	macro(MANDATORY, 0), \
	macro(QOS, 1), \
	macro(CPU, 2), \
	macro(GPU, 3), \
	macro(APU, 4), \
	macro(MDLA, 5), \
	macro(VPU, 6), \
	macro(MAX, 7), \

#define RSU_GENERATE_ENUM(name, shft) RSU_DEBUG_##name = 1U << shft
enum {
	RSU_SYSTRACE_LIST(RSU_GENERATE_ENUM)
};

#define RSU_GENERATE_STRING(name, unused) #name
static const char * const mask_string[] = {
	RSU_SYSTRACE_LIST(RSU_GENERATE_STRING)
};

#define rsu_systrace_c(mask, pid, val, fmt...) \
	do { \
		if (rsu_systrace_mask & mask) \
			__rs_systrace_c(pid, val, fmt); \
	} while (0)

#define rsu_systrace_c_uint64(mask, pid, val, fmt...) \
	do { \
		if (rsu_systrace_mask & mask) \
			__rs_systrace_c_uint64(pid, val, fmt); \
	} while (0)

#define rsu_systrace_b(mask, tgid, fmt, ...) \
	do { \
		if (rsu_systrace_mask & mask) \
			__rs_systrace_b(tgid, fmt); \
	} while (0)

#define rsu_systrace_e(mask) \
	do { \
		if (rsu_systrace_mask & mask) \
			__rs_systrace_e(); \
	} while (0)

#define rsu_systrace_c_log(pid, val, fmt...) \
	rsu_systrace_c(RSU_DEBUG_MANDATORY, pid, val, fmt)

#define rsu_systrace_c_uint64_log(pid, val, fmt...) \
	rsu_systrace_c_uint64(RSU_DEBUG_MANDATORY, pid, val, fmt)

#ifdef NR_FREQ_CPU
struct rsu_cpu_cluster_info {
	unsigned int power[NR_FREQ_CPU];
	unsigned int capacity_ratio[NR_FREQ_CPU];

	unsigned int lastfreq;
	unsigned int ceiling_obv;
};

struct rsu_cpu_info {
	struct rsu_cpu_cluster_info *cluster;

	u64 prev_idle_time;
	u64 prev_wall_time;
};
#endif

#ifdef __BW_USE_EMIALL__
enum RSU_NTF_PUSH_TYPE {
	RSU_NTF_QOSONOFF_SWITCH	= 0x00,
};

struct RSU_NTF_PUSH_TAG {
	enum RSU_NTF_PUSH_TYPE ePushType;

	int value;

	struct work_struct sWork;
};
#endif

static int nr_cpuclusters;
static int nr_cpus;

static struct rsu_cpu_info *cpu_info;
static struct rsu_cpu_cluster_info *cpucluster_info;

static struct kobject *rsu_kobj;

static DEFINE_SPINLOCK(freq_slock);

#ifdef __BW_USE_EMIALL__
static long long fpsgo_time_us;
static long long nn_time_us;

static DEFINE_MUTEX(rsu_qos_ntf_mutex);
static int fpsgo_active;
static int nn_active;

static atomic_t last_bw_usage;

static struct workqueue_struct *_gpRSUNotifyWorkQueue;
static int rsu_qos_isON;
#endif

static DEFINE_MUTEX(rsu_vpu_ntf_mutex);
static int *vpu_opp;

static DEFINE_MUTEX(rsu_mdla_ntf_mutex);
static int *mdla_opp;

static uint32_t rsu_systrace_mask;

static void rsu_cpu_update_pwd_tbl(void)
{
	int cluster, opp;
	struct cpumask cluster_cpus;
	int cpu;
	const struct sched_group_energy *core_energy = NULL;
	unsigned long long cap = 0ULL;
	unsigned int temp;

	for (cluster = 0; cluster < nr_cpuclusters; cluster++) {
		for (opp = 0; opp < NR_FREQ_CPU; opp++) {
			cpucluster_info[cluster].power[opp] =
				mt_cpufreq_get_freq_by_idx(cluster, opp);

			arch_get_cluster_cpus(&cluster_cpus, cluster);
			for_each_cpu(cpu, &cluster_cpus) {
				core_energy = cpu_core_energy(cpu);
				cpu_info[cpu].cluster =
					&cpucluster_info[cluster];
			}

			if (!core_energy)
				break;

			cap = core_energy->cap_states[
				NR_FREQ_CPU - opp - 1].cap;
			cap = (cap * 100) >> 10;
			temp = (unsigned int)cap;
			temp = clamp(temp, 1U, 100U);
			cpucluster_info[cluster].capacity_ratio[opp] = temp;
		}
	}
}

void rsu_notify_cpufreq(int cid, unsigned long freq)
{
	unsigned long flags;
	int cpu;

	spin_lock_irqsave(&freq_slock, flags);

	cpucluster_info[cid].lastfreq = freq;

	for_each_possible_cpu(cpu) {
		cpu_info[cpu].prev_idle_time =
		get_cpu_idle_time(cpu, &(cpu_info[cpu].prev_wall_time), 1);
	}

	spin_unlock_irqrestore(&freq_slock, flags);

	rsu_systrace_c(RSU_DEBUG_CPU, 0, freq, "RSU_CPUFREQ_CB[%d]", cid);
}

static int rsu_get_cpu_usage(__u32 pid)
{
	unsigned long flags;
	int cpu_cnt = 0;
	int cpu;
	u64 cur_idle_time_i, cur_wall_time_i;
	u64 cpu_idle_time = 0, cpu_wall_time = 0;
	u64 curr_obv;
	unsigned int *ceiling_idx;
	int clus_max_idx;
	int i;
	int opp;
	int ret = 0;
	struct cpufreq_policy *policy;
	struct cpumask *cpus_mask;

	ceiling_idx =
		kcalloc(nr_cpuclusters, sizeof(unsigned int), GFP_KERNEL);

	if (!ceiling_idx)
		return -1;

	for (i = 0; i < nr_cpuclusters; i++) {
		if (mt_ppm_userlimit_freq_limit_by_others)
			clus_max_idx = mt_ppm_userlimit_freq_limit_by_others(i);
		else {

			arch_get_cluster_cpus(cpus_mask, i);
			policy = cpufreq_cpu_get(
				cpumask_first(cpus_mask));

			for (opp = 0; opp < NR_FREQ_CPU; opp++)
				if (policy->max ==
					mt_cpufreq_get_freq_by_idx(i, opp)) {
					clus_max_idx = opp;
					break;
				}
			cpufreq_cpu_put(policy);
		}

		clus_max_idx = clamp(clus_max_idx, 0, NR_FREQ_CPU - 1);

		ceiling_idx[i] =
			cpucluster_info[i].capacity_ratio[clus_max_idx];

		rsu_systrace_c(RSU_DEBUG_CPU, pid, ceiling_idx[i],
				"RSU_CEILING_IDX[%d]", i);
	}

	spin_lock_irqsave(&freq_slock, flags);

	for (i = 0; i < nr_cpuclusters; i++) {

		rsu_systrace_c_log(pid, cpucluster_info[i].lastfreq,
					"LASTFREQ[%d]", i);

		for (opp = (NR_FREQ_CPU - 1); opp > 0; opp--) {
			if (cpucluster_info[i].power[opp] >=
				cpucluster_info[i].lastfreq)
				break;
		}
		curr_obv =
			((u64) cpucluster_info[i].capacity_ratio[opp]) * 100;

		rsu_systrace_c(RSU_DEBUG_CPU, pid, (unsigned int) curr_obv,
				"CURR_OBV[%d]", i);
		do_div(curr_obv, ceiling_idx[i]);
		cpucluster_info[i].ceiling_obv = (unsigned int)
				clamp(((unsigned int) curr_obv), 0U, 100U);
		rsu_systrace_c(RSU_DEBUG_CPU, pid, (unsigned int) curr_obv,
				"CEILING_OBV[%d]", i);
	}

	for_each_online_cpu(cpu) {
		cur_idle_time_i = get_cpu_idle_time(cpu, &cur_wall_time_i, 1);

		cpu_idle_time = cur_idle_time_i - cpu_info[cpu].prev_idle_time;
		cpu_wall_time = cur_wall_time_i - cpu_info[cpu].prev_wall_time;

		if (cpu_wall_time > 0 && cpu_wall_time >= cpu_idle_time) {
			ret +=
			div_u64((cpu_wall_time - cpu_idle_time) * 100,
				cpu_wall_time) *
					cpu_info[cpu].cluster->ceiling_obv;

			cpu_cnt++;
		}

		rsu_systrace_c(RSU_DEBUG_CPU, pid, cpu_idle_time,
				"CPU_IDLE_TIME[%d]", cpu);
		rsu_systrace_c(RSU_DEBUG_CPU, pid, cpu_wall_time,
				"CPU_WALL_TIME[%d]", cpu);
		rsu_systrace_c(RSU_DEBUG_CPU, pid, ret,
				"CPU_USAGE_TOTAL[%d]", cpu);
	}

	spin_unlock_irqrestore(&freq_slock, flags);

	kfree(ceiling_idx);

	rsu_systrace_c(RSU_DEBUG_CPU, pid, cpu_cnt, "CPU_CNT");

	if (cpu_cnt)
		ret /= (cpu_cnt * 100);
	else
		ret = 100;

	ret = clamp(ret, 0, 100);

	rsu_systrace_c_log(pid, ret, "RSU_CPUUSAGE");

	return ret;
}

static int rsu_get_gpu_usage(__u32 pid)
{
	unsigned int gloading = 0;
	unsigned int cur = 0;
	unsigned int ceiling = 0;

	mtk_get_gpu_loading(&gloading);
	cur = mt_gpufreq_get_cur_freq();
	ceiling = mt_gpufreq_get_freq_by_idx(mt_gpufreq_get_cur_ceiling_idx());

	rsu_systrace_c(RSU_DEBUG_GPU, pid, gloading,
				"gpu_loading");
	rsu_systrace_c(RSU_DEBUG_GPU, pid, cur,
				"gpu_cur");
	rsu_systrace_c(RSU_DEBUG_GPU, pid, ceiling,
				"gpu_ceiling");

	cur *= 100;

	if (ceiling) {
		cur /= ceiling;
		cur *= gloading;
		cur /= 100;

		cur = clamp(cur, 0U, 100U);
	} else
		cur = -1;

	rsu_systrace_c_log(pid, cur, "RSU_GPUUSAGE");

	return cur;
}

static int rsu_get_mdla_usage(__u32 pid)
{
	int ret = -1;

	if (!rs_mdla_support_idletime()) {
		int ceiling;
		int curr;
		int core_num;
		int i;
		int vRet = 0;

		core_num = rs_get_mdla_core_num();

		if (!core_num)
			goto final;

		for (i = 0; i < core_num; i++) {
			mutex_lock(&rsu_mdla_ntf_mutex);
			curr = mdla_opp[i];
			mutex_unlock(&rsu_mdla_ntf_mutex);

			if (curr == -1)
				continue;

			do {
				ceiling = rs_get_mdla_ceiling_opp(i);
				curr = rs_get_mdla_curr_opp(i);
			} while (ceiling > rs_get_mdla_opp_max(i) ||
					curr > rs_get_mdla_opp_max(i));

			rsu_systrace_c(RSU_DEBUG_MDLA, pid, ceiling,
					"RSU_MDLA_CEILING_OPP[%d]", i);
			rsu_systrace_c(RSU_DEBUG_MDLA, pid, curr,
					"RSU_MDLA_CURR_OPP[%d]", i);

			ceiling = rs_mdla_opp_to_freq(i, ceiling);
			curr = rs_mdla_opp_to_freq(i, curr);

			rsu_systrace_c(RSU_DEBUG_MDLA, pid, ceiling,
					"RSU_MDLA_CEILING_FREQ[%d]", i);
			rsu_systrace_c(RSU_DEBUG_MDLA, pid, curr,
					"RSU_MDLA_CURR_FREQ[%d]", i);

			if (!ceiling)
				continue;

			curr *= 100;
			vRet += (curr / ceiling);
		}

		ret = vRet / core_num;
	}

final:
	rsu_systrace_c_log(pid, ret, "RSU_MDLAUSAGE");

	return ret;
}

static int rsu_get_vpu_usage(__u32 pid)
{
	int ret = -1;

	if (!rs_vpu_support_idletime()) {
		int ceiling;
		int curr;
		int core_num;
		int i;
		int vRet = 0;

		core_num = rs_get_vpu_core_num();

		if (!core_num)
			goto final;

		for (i = 0; i < core_num; i++) {
			mutex_lock(&rsu_vpu_ntf_mutex);
			curr = vpu_opp[i];
			mutex_unlock(&rsu_vpu_ntf_mutex);

			if (curr == -1)
				continue;

			do {
				ceiling = rs_get_vpu_ceiling_opp(i);
				curr = rs_get_vpu_curr_opp(i);
			} while (ceiling > rs_get_vpu_opp_max(i) ||
					curr > rs_get_vpu_opp_max(i));

			rsu_systrace_c(RSU_DEBUG_VPU, pid, ceiling,
					"RSU_VPU_CEILING_OPP[%d]", i);
			rsu_systrace_c(RSU_DEBUG_VPU, pid, curr,
					"RSU_VPU_CURR_OPP[%d]", i);

			ceiling = rs_vpu_opp_to_freq(i, ceiling);
			curr = rs_vpu_opp_to_freq(i, curr);

			rsu_systrace_c(RSU_DEBUG_VPU, pid, ceiling,
					"RSU_VPU_CEILING_FREQ[%d]", i);
			rsu_systrace_c(RSU_DEBUG_VPU, pid, curr,
					"RSU_VPU_CURR_FREQ[%d]", i);

			if (!ceiling)
				continue;

			curr *= 100;
			vRet += (curr / ceiling);
		}

		ret = vRet / core_num;
	}

final:
	rsu_systrace_c_log(pid, ret, "RSU_VPUUSAGE");

	return ret;
}

static int rsu_get_apu_usage(__u32 pid)
{
	int ret = -1;
	int mdlal = -1;
	int vpul = -1;

	vpul = rsu_get_vpu_usage(pid);
	mdlal = rsu_get_mdla_usage(pid);

	ret = vpul;

	if (mdlal > ret)
		ret = mdlal;

	rsu_systrace_c_log(pid, ret, "RSU_APUUSAGE");

	return ret;
}

static int rsu_get_bw_usage(__u32 pid)
{
#ifdef __BW_USE_EMIALL__
	int ret = atomic_read(&last_bw_usage);
#else
	int threshold = pob_qos_get_max_bw_threshold();
	int ret = pob_qosbm_get_last_avg(32,
					PQBT_TOTAL,
					PQBP_EMI,
					PQBS_MON,
					0, 0);

	rsu_systrace_c(RSU_DEBUG_QOS, 0,
			ret, "BW Avg");
	rsu_systrace_c(RSU_DEBUG_QOS, 0,
			threshold, "BW th");

	if (!threshold)
		return 0;

	if (ret > 0) {
		ret *= 100;
		ret /= threshold;

		ret = clamp(ret, 0, 100);
	}
#endif

	rsu_systrace_c_log(pid, ret, "RSU_BWUSAGE");

	return ret;
}

void rsu_getusage(__s32 *devusage, __u32 *bwusage, __u32 pid)
{
	switch (*devusage) {
	case USAGE_DEVTYPE_CPU:
		*devusage = rsu_get_cpu_usage(pid);
		break;
	case USAGE_DEVTYPE_GPU:
		*devusage = rsu_get_gpu_usage(pid);
		break;
	case USAGE_DEVTYPE_APU:
		*devusage = rsu_get_apu_usage(pid);
		break;
	case USAGE_DEVTYPE_MDLA:
		*devusage = rsu_get_mdla_usage(pid);
		break;
	case USAGE_DEVTYPE_VPU:
		*devusage = rsu_get_vpu_usage(pid);
		break;
	default:
		pr_debug(TAG "unknown cmd %d\n", *devusage);
		*devusage = -1;
		break;
	}

	*bwusage = rsu_get_bw_usage(pid);
}

#ifdef __BW_USE_EMIALL__
static int rsu_pob_qos_cb(struct notifier_block *nb,
			unsigned long val, void *data);
static struct notifier_block rsu_qos_notifier = {
	.notifier_call = rsu_pob_qos_cb,
};

static void rsu_ntf_wq_cb(struct work_struct *psWork)
{
	struct RSU_NTF_PUSH_TAG *vpPush =
		RS_CONTAINER_OF(psWork,
				struct RSU_NTF_PUSH_TAG, sWork);

	if (!psWork)
		return;

	switch (vpPush->ePushType) {
	case RSU_NTF_QOSONOFF_SWITCH:
		if (rsu_qos_isON && !vpPush->value)
			pob_qos_unregister_client(&rsu_qos_notifier);
		else if (!rsu_qos_isON && vpPush->value)
			pob_qos_register_client(&rsu_qos_notifier);

		rsu_qos_isON = vpPush->value;

		break;
	default:
		break;
	}

	rs_free(vpPush);
}

static void rsu_qosenable_switch(int value)
{
	struct RSU_NTF_PUSH_TAG *vpPush = NULL;

	if (!_gpRSUNotifyWorkQueue)
		return;

	vpPush =
		(struct RSU_NTF_PUSH_TAG *)
		rs_alloc_atomic(sizeof(struct RSU_NTF_PUSH_TAG));

	if (!vpPush)
		return;

	vpPush->ePushType = RSU_NTF_QOSONOFF_SWITCH;
	vpPush->value = value;

	INIT_WORK(&vpPush->sWork, rsu_ntf_wq_cb);
	queue_work(_gpRSUNotifyWorkQueue, &vpPush->sWork);
}

static inline void rsu_pob_qos_enable(void)
{
	rsu_qosenable_switch(1);
}

static inline void rsu_pob_qos_disable(void)
{
	rsu_qosenable_switch(0);
}

static void rsu_qos_check_active(void)
{
	ktime_t cur_time;
	long long cur_time_us;
	long long last_time_us;

	/*get current time*/
	cur_time = ktime_get();
	cur_time_us = ktime_to_us(cur_time);

	mutex_lock(&rsu_qos_ntf_mutex);

	if (fpsgo_active || nn_active) {

		last_time_us = fpsgo_time_us;

		if (last_time_us < nn_time_us)
			last_time_us = nn_time_us;

		if (cur_time_us > last_time_us &&
			cur_time_us - last_time_us > 1100000) {
			fpsgo_active = 0;
			nn_active = 0;
			rsu_pob_qos_disable();
			atomic_set(&last_bw_usage, 0);
		}
	}

	mutex_unlock(&rsu_qos_ntf_mutex);
}

static int rsu_pob_qos_cb(struct notifier_block *nb,
			unsigned long val, void *data)
{
	int total = 0;
	int threshold = pob_qos_get_max_bw_threshold();
	int item;

	rsu_systrace_c(RSU_DEBUG_QOS, 0, threshold, "BW Thres");

	if (threshold == -1)
		return NOTIFY_OK;

	switch (val) {
	case POB_QOS_EMI_ALL:
		if (fpsgo_active || nn_active) {
			int i;
			struct pob_qos_info *pqi =
				(struct pob_qos_info *) data;

			for (i = 0; i < pqi->size; i++) {
				item = pob_qosbm_get_stat(pqi->pstats,
							i,
							PQBT_TOTAL,
							PQBP_EMI,
							PQBS_MON,
							0, 0);

				if (item < 0)
					item = 0;
				else
					item *= 100;

				rsu_systrace_c(RSU_DEBUG_QOS, 0,
						item, "BW item");

				do_div(item, threshold);

				total += clamp(item, 0, 100);

				rsu_systrace_c(RSU_DEBUG_QOS, 0,
						total, "BW Total");
			}

			do_div(total, pqi->size);

			atomic_set(&last_bw_usage, total);
			rsu_qos_check_active();
		}

		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static void rsu_fpsgo_active(long long cur_time_us)
{
	mutex_lock(&rsu_qos_ntf_mutex);

	fpsgo_time_us = cur_time_us;

	if (!fpsgo_active) {
		fpsgo_active = 1;

		if (!nn_active)
			rsu_pob_qos_enable();
	}

	mutex_unlock(&rsu_qos_ntf_mutex);
}

static void rsu_nn_active(long long cur_time_us)
{
	mutex_lock(&rsu_qos_ntf_mutex);

	nn_time_us = cur_time_us;

	if (!nn_active) {
		nn_active = 1;

		if (!fpsgo_active)
			rsu_pob_qos_enable();
	}

	mutex_unlock(&rsu_qos_ntf_mutex);
}

static int rsu_pob_fpsgo_cb(struct notifier_block *nb,
			unsigned long val, void *data)
{
	ktime_t cur_time;
	long long cur_time_us;

	switch (val) {
	case POB_FPSGO_QTSK_DELALL:
		mutex_lock(&rsu_qos_ntf_mutex);
		fpsgo_active = 0;
		if (!nn_active) {
			rsu_pob_qos_disable();
			atomic_set(&last_bw_usage, 0);
		}
		mutex_unlock(&rsu_qos_ntf_mutex);
		break;
	default:
		/*get current time*/
		cur_time = ktime_get();
		cur_time_us = ktime_to_us(cur_time);

		rsu_fpsgo_active(cur_time_us);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block rsu_pob_fpsgo_notifier = {
	.notifier_call = rsu_pob_fpsgo_cb,
};

static int rsu_pob_nn_cb(struct notifier_block *nb,
			unsigned long val, void *data)
{
	ktime_t cur_time;
	long long cur_time_us;

	switch (val) {
	case POB_NN_BEGIN:
	case POB_NN_END:
		/*get current time*/
		cur_time = ktime_get();
		cur_time_us = ktime_to_us(cur_time);

		rsu_nn_active(cur_time_us);

		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block rsu_pob_nn_notifier = {
	.notifier_call = rsu_pob_nn_cb,
};
#endif

static int rsu_pob_xpufreq_cb(struct notifier_block *nb,
			unsigned long val, void *data)
{
	switch (val) {
	case POB_XPUFREQ_VPU:
		{
			struct pob_xpufreq_info *pxi =
				(struct pob_xpufreq_info *) data;

			if (pxi->id >= rs_get_vpu_core_num())
				break;

			mutex_lock(&rsu_vpu_ntf_mutex);
			vpu_opp[pxi->id] = pxi->opp;
			mutex_unlock(&rsu_vpu_ntf_mutex);

			rsu_systrace_c(RSU_DEBUG_VPU, 0, pxi->opp,
					"RSU_VPUFREQ_CB[%d]", pxi->id);
		}
		break;
	case POB_XPUFREQ_MDLA:
		{
			struct pob_xpufreq_info *pxi =
				(struct pob_xpufreq_info *) data;

			if (pxi->id >= rs_get_mdla_core_num())
				break;

			mutex_lock(&rsu_mdla_ntf_mutex);
			mdla_opp[pxi->id] = pxi->opp;
			mutex_unlock(&rsu_mdla_ntf_mutex);

			rsu_systrace_c(RSU_DEBUG_MDLA, 0, pxi->opp,
					"RSU_MDLAFREQ_CB[%d]", pxi->id);
		}
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block rsu_pob_xpufreq_notifier = {
	.notifier_call = rsu_pob_xpufreq_cb,
};

static ssize_t rs_usage_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char temp[RS_SYSFS_MAX_BUFF_SIZE] = "";
	int posi = 0;
	int length;

	length = scnprintf(temp + posi,
		RS_SYSFS_MAX_BUFF_SIZE - posi,
		"cpu\tgpu\tbw\tapu\tvpu\tmdla\n");
	posi += length;

	length = scnprintf(temp + posi,
		RS_SYSFS_MAX_BUFF_SIZE - posi,
		"%d\t%d\t%d\t%d\t%d\t%d\n",
		rsu_get_cpu_usage(0), rsu_get_gpu_usage(0),
		rsu_get_bw_usage(0), rsu_get_apu_usage(0),
		rsu_get_vpu_usage(0), rsu_get_mdla_usage(0));
	posi += length;

	return scnprintf(buf, PAGE_SIZE, "%s\n", temp);
}

KOBJ_ATTR_RO(rs_usage);

static ssize_t rs_mask_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int i;
	char temp[RS_SYSFS_MAX_BUFF_SIZE];
	int pos = 0;
	int length;

	length = scnprintf(temp + pos, RS_SYSFS_MAX_BUFF_SIZE - pos,
			" Current enabled systrace:\n");
	pos += length;

	for (i = 0; (1U << i) < RSU_DEBUG_MAX; i++) {
		length = scnprintf(temp + pos, RS_SYSFS_MAX_BUFF_SIZE - pos,
			"  %-*s ... %s\n", 12, mask_string[i],
		   rsu_systrace_mask & (1U << i) ?
		   "On" : "Off");
		pos += length;

	}

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static ssize_t rs_mask_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char acBuffer[RS_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < RS_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, RS_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				return count;
		}
	}

	val = val & (RSU_DEBUG_MAX - 1U);

	rsu_systrace_mask = val;

	return count;
}

KOBJ_ATTR_RW(rs_mask);

int __init rs_usage_init(void)
{
	int i;

#ifdef __BW_USE_EMIALL__
	_gpRSUNotifyWorkQueue = create_singlethread_workqueue("rsu_ntf_wq");
	if (_gpRSUNotifyWorkQueue == NULL)
		return -EFAULT;
#endif

	nr_cpuclusters = arch_get_nr_clusters();
	nr_cpus = num_possible_cpus();

	cpucluster_info =
		kcalloc(nr_cpuclusters, sizeof(struct rsu_cpu_cluster_info),
			GFP_KERNEL);

	cpu_info =
		kcalloc(nr_cpus, sizeof(struct rsu_cpu_info), GFP_KERNEL);

	rsu_cpu_update_pwd_tbl();

	if (rs_get_vpu_core_num()) {
		vpu_opp =
		kcalloc(rs_get_vpu_core_num(), sizeof(int), GFP_KERNEL);

		for (i = 0; i < rs_get_vpu_core_num(); i++)
			vpu_opp[i] = -1;
	}

	if (rs_get_mdla_core_num()) {
		mdla_opp =
		kcalloc(rs_get_mdla_core_num(), sizeof(int), GFP_KERNEL);

		for (i = 0; i < rs_get_mdla_core_num(); i++)
			mdla_opp[i] = -1;
	}

	rsu_systrace_mask = RSU_DEBUG_MANDATORY;

#ifdef CONFIG_MTK_FPSGO_V3
	rsu_cpufreq_notifier_fp = rsu_notify_cpufreq;
#endif
	rsu_getusage_fp = rsu_getusage;

#ifdef __BW_USE_EMIALL__
	pob_fpsgo_register_client(&rsu_pob_fpsgo_notifier);
	pob_nn_register_client(&rsu_pob_nn_notifier);
#endif
	pob_xpufreq_register_client(&rsu_pob_xpufreq_notifier);

	if (rs_sysfs_create_dir(NULL, "usage", &rsu_kobj))
		return -ENODEV;

	rs_sysfs_create_file(rsu_kobj, &kobj_attr_rs_usage);
	rs_sysfs_create_file(rsu_kobj, &kobj_attr_rs_mask);

	return 0;
}

void __exit rs_usage_exit(void)
{
	rs_sysfs_remove_file(rsu_kobj, &kobj_attr_rs_usage);
	rs_sysfs_remove_file(rsu_kobj, &kobj_attr_rs_mask);
	rs_sysfs_remove_dir(&rsu_kobj);
}

