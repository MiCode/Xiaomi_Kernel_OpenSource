// MIUI ADD: Performance_TurboSched
/*
 * Copyright (c) Xiaomi Technologies Co., Ltd. 2019. All rights reserved.
 *
 * File name: mist_dflt.c
 * Descrviption: ddr frame load tracker
 * Author: guchao1@xiaomi.com
 * Version: 1.0
 * Date:  2023/03/01
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) "mist-dflt: " fmt
#include "mist_dflt.h"
#include <trace/hooks/sched.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mi_soc.h>
#include <linux/fs.h>
#include <linux/cpufreq.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/pkg_stat.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/cpumask.h>
#include <linux/topology.h>

#define MEMLAT_FP_NAME "memlat_fp"
#define DFLT_FP_NAME "dflt_fp"
#define MIST_VERSION "3.5.0"
#define MAX_SPM_WINDOW_SIZE 20U
#define MAX_SPM_FREQ_MAP 2
#define MISS_DELTA_PCT_THRES 30U
#define SPM_FREQ_THRES 2000U
#define L2MISS_RATIO_THRES 150U
#define SPM_CPU_FREQ_IGN 200U
#define MIN_SPM_THRES 1U
#define MAX_SPM_THRES 1000U
#define SMP_CPU_MASK 0xFF
#define DFLT_BW_HW_TYPES 2
#define FRAME_STATS_RECORD 101
#define EXC_UTIL_F_DIVISOR 3
#define NUM_COLS 2
#define COREDEV_TBL_PROP	"qcom,cpufreq-memfreq-tbl"
#define MAX_CPU_NUM    8

struct mist_frame_stats {
	u64 real_freq[MAX_CLUSTER];
	u64 freq[MAX_CLUSTER];
	u32 ddr_freq;
	u64 q2q_time;
	u64 ipc[8];
	u64 miss[8];
	u64 instruction[8];
	u32 stall_ratio[8];
};
u32 cur_ddr_clk;

struct dflt_event_data {
	u32 event_id;
	u32 amu_id;
	u32 cid;
	unsigned long cpus;
};
struct cpufreq_memfreq_map {
	unsigned int cpufreq_mhz;
	unsigned int memfreq_khz;
};

u32 cur_ddr_clk;
static u32 last_vote_freq[NUM_DFLT_HW_TYPES] = {0, 0, 0};
static u32 cur_vote_freq[NUM_DFLT_HW_TYPES] = {0, 0, 0};
static u64 mc_cc;
static struct cpufreq_memfreq_map *mist_map[NUM_DFLT_HW_TYPES][NUM_CLUSTER_FREQ_MAPS];
static	cpumask_t mist_cpus[NUM_DFLT_HW_TYPES][NUM_DFLT_HW_TYPES];

/*structure used to save sampling data of memory latency*/
struct dflt_latency_cpu_stats {
	u64 prev_ctrs[NUM_DFLT_EVS];
	u64 curr_ctrs[NUM_DFLT_EVS];
	u64 delta_ctrs[NUM_DFLT_EVS];

	struct qcom_pmu_data raw_ctrs; //raw pmu data from qcom_pmu_lib
	bool idle_sample;
	ktime_t sample_ts;
	ktime_t last_sample_ts;
	spinlock_t ctrs_lock;
	u32 freq_mhz;
	u32 spm[NUM_DFLT_HW_TYPES]; //spm(stall per cache miss), used for identify memory latency scenarios
	u64 ll_miss;
	u32 ips;
	bool dflt_latency_sample_start; //flag used to determine whether current core needs to be sampled
};

/* spm algorithm related fields */
u32 spm_window_size;
u32 sampled_max_spm[MAX_SPM_WINDOW_SIZE];
u32 sampled_max_cpu_freq[MAX_SPM_WINDOW_SIZE];
u32 sampled_spm_idx;
u32 spm_vote_inc_steps;
u64 prev_max_miss;
u32 disable_spm_value;
u32 spm_max_freq;

struct cpu_list {
	u32 length;
	u32 cpus[8];
	u32 spm_thresh;
	u32 ips_thresh;
};
struct cpu_list cpu_list_map[MAX_CLUSTER];

struct cpufreq_memfreq_map spm_freq_map[MAX_SPM_FREQ_MAP];

static struct mist_frame_stats frame_stats_record[FRAME_STATS_RECORD];
static struct proc_dir_entry *frame_load_dir;
static struct proc_dir_entry *history_load_entry;
static int cur_record;
static int cluster[8] = {0, 0, 0, 0, 0, 0, 1, 1};

u32 mist_next_freq[] = { [0 ... MAX_CLUSTER-1] = 0};

u32 dflt_ev_ids[NUM_DFLT_EVS];
struct dflt_event_data dflt_ev_data[NUM_DFLT_EVS];
static DEFINE_PER_CPU(struct dflt_latency_cpu_stats *, sampling_stats);
static int dflt_latency_cur_vote_counter = 0;
static int dflt_bwmon_enable = 0;

static u64 frame_index = 0;

static atomic_t dflt_latency_enable = ATOMIC_INIT(0);
static int dflt_latency_enable_from_user = 0;

static spinlock_t dflt_commit_lock;
struct hrtimer *dflt_lat_vote_timer;

static int dflt_ftrace_debug = 0;
module_param(dflt_ftrace_debug, uint, 0644);

static int dflt_debug = 0;
module_param(dflt_debug, uint, 0644);

static int need_boost_in_frame = 0;
module_param(need_boost_in_frame, uint, 0644);

static unsigned int dflt_fpercent_other = 2;
module_param(dflt_fpercent_other, uint, 0644);

static uint max_ddr_ib = 4224000U;
module_param(max_ddr_ib, uint, 0644);

static uint spm_thres = 200U;
module_param(spm_thres, uint, 0644);

static uint dflt_ddr_ib_scale = 100U;
module_param(dflt_ddr_ib_scale, uint, 0644);

static int dflt_ddr_boost = 0;
module_param(dflt_ddr_boost, uint, 0644);

static ulong dflt_cpu_mask_debug = 0;
module_param(dflt_cpu_mask_debug, ulong, 0644);

static uint dflt_spm_low_threshold = 0;
module_param(dflt_spm_low_threshold, uint, 0644);

static uint dflt_ddr_ib_scale_low = 100u;
module_param(dflt_ddr_ib_scale_low, uint, 0644);

static uint dflt_ddr_ib_scale_high = 100u;
module_param(dflt_ddr_ib_scale_high, uint, 0644);

static uint dflt_spm_high_threshold = 10000;
module_param(dflt_spm_high_threshold, uint, 0644);

static int dflt_memlat_vote_type = 0;
module_param(dflt_memlat_vote_type, uint, 0644);

static int mist_memlat_vote_enable = 0;
module_param(mist_memlat_vote_enable, uint, 0644);

unsigned int dflt_stallratio_thresh[4] = {0, 0, 0, 0};
module_param_array(dflt_stallratio_thresh, uint, NULL, 0644);

unsigned int dflt_spm_thresh[4] = {150, 150, 150, 150};
module_param_array(dflt_spm_thresh, uint, NULL, 0644);

unsigned int dflt_ips_thresh[4] = {0, 0, 0, 0};
module_param_array(dflt_ips_thresh, uint, NULL, 0644);

static char *mist_version = MIST_VERSION;
module_param(mist_version, charp, 0444);

static uint be_procent = 100;
module_param(be_procent, uint, 0644);

static atomic_t mist_soc_api_enable = ATOMIC_INIT(0);

static uint mist_soc_min_stall = 200;
module_param(mist_soc_min_stall, uint, 0644);

static uint mist_set_uid = 0;
module_param(mist_set_uid, uint, 0644);

unsigned int mist_soc_freq_thresh[4] = {1440000, 1958400, 1843200, 2342400};
module_param_array(mist_soc_freq_thresh, uint, NULL, 0644);

static int dflt_bwmon_counter_enable = 0;
static uint dflt_bwmon_counter = 0;

bool inited;
u32 last_dflt_ib, cur_dflt_ib[DFLT_BW_HW_TYPES], last_ddr_freq, last_ddr_latency_ib;
bool dflt_bwmon_sample_start[DFLT_BW_HW_TYPES], dflt_latency_vote_start, dflt_memlat_sample_start;
static bool mist_memlat_vote_start = false;

static u32 ddr_freq_map[7] ={ 547000,
							  768000,
							  1555000,
							  2092000,
							  3187000,
							  3686000,
							  4224000
							};

struct hrtimer dflt_timer, dflt_vote_timer, memlat_vote_timer;
mi_dflt_bw_update_cb dflt_bw_update_cb = NULL;
mi_dflt_lat_update_cb dflt_lat_update_cb = NULL;
mi_dflt_lat_cal_cb dflt_lat_cal_cb = NULL;

static atomic_t mem_sample_counter = ATOMIC_INIT(0);
static int mist_commit_mask = 0;
static int mist_cluster_mask = 0;
ktime_t frame_start_time  = 0;
ktime_t last_frame_start_time  = 0;
//extern u32 get_cur_DDR_freq(void);

static struct workqueue_struct *dflt_wq;
static struct work_struct fast_vote_work;

extern int clk_debug_mc_cc_get(void *data, u64 *val);
extern int mist_gflt_init(void);

static int get_cluster_max_id(void)
{
	int cpu,max_cluster_id = 0;
	for_each_possible_cpu(cpu) {
		if (cpu_topology[cpu].cluster_id > max_cluster_id ) {
			max_cluster_id = cpu_topology[cpu].cluster_id;
		}
	}
	return max_cluster_id + 1 ;
}

static int set_dflt_bm_counter_enable(const char *buf,
				      const struct kernel_param *kp)
{
	unsigned int val = 0;
	if (sscanf(buf, "%u\n", &val) != 1) {
		return -EINVAL;
	}

	if (val) {
		dflt_bwmon_counter = 0;
	}

	dflt_bwmon_counter_enable = val;
	return 0;
}

static int get_dflt_bm_counter_enable(char *buf, const struct kernel_param *kp)
{
	return snprintf(buf, PAGE_SIZE, "%d:%u \n", dflt_bwmon_counter_enable,
			dflt_bwmon_counter);
}

static const struct kernel_param_ops param_ops_dflt_bm_counter_enable = {
	.set = set_dflt_bm_counter_enable,
	.get = get_dflt_bm_counter_enable,
};
module_param_cb(dflt_bm_counter_enable, &param_ops_dflt_bm_counter_enable, NULL,
		0644);

static int set_dflt_bwmon_enable(const char *buf, const struct kernel_param *kp)
{
	unsigned int val = 0;
	if (sscanf(buf, "%u\n", &val) != 1) {
		return -EINVAL;
	}
	dflt_bwmon_enable = val;
	return 0;
}

static int get_dflt_bwmon_enable(char *buf, const struct kernel_param *kp)
{
	return snprintf(buf, PAGE_SIZE, "%d \n", dflt_bwmon_enable);
}

static const struct kernel_param_ops param_ops_dflt_bwmon = {
	.set = set_dflt_bwmon_enable,
	.get = get_dflt_bwmon_enable,
};
module_param_cb(dflt_bw_enable, &param_ops_dflt_bwmon, NULL, 0644);

static int set_dflt_latency_enable(const char *buf,
				   const struct kernel_param *kp)
{
	unsigned int val = 0;
	if (sscanf(buf, "%u\n", &val) != 1) {
		return -EINVAL;
	}
	dflt_latency_enable_from_user = val;
	if(val == 0) {
		frame_index = 0;
	}
	return 0;
}

int mist_ddr_vote_enable(void)
{
	return mist_memlat_vote_enable;
}
EXPORT_SYMBOL_GPL(mist_ddr_vote_enable);

static int get_dflt_latency_enable(char *buf, const struct kernel_param *kp)
{
	return snprintf(buf, PAGE_SIZE, "%d \n", dflt_latency_enable_from_user);
}

static const struct kernel_param_ops param_ops_dflt_latency = {
	.set = set_dflt_latency_enable,
	.get = get_dflt_latency_enable,
};
module_param_cb(dflt_lat_enable, &param_ops_dflt_latency, NULL, 0644);

static int set_soc_api_enable(const char *buf,
				   const struct kernel_param *kp)
{
	unsigned int val = 0;
	if (kstrtouint(buf, 0, &val) != 0) {
        return -EINVAL;
    }
    atomic_set(&mist_soc_api_enable, val);
    return 0;
}
static int get_soc_api_enable(char *buf, const struct kernel_param *kp)
{
	return snprintf(buf, PAGE_SIZE, "%d \n", (int)atomic_read(&mist_soc_api_enable));
}

static const struct kernel_param_ops param_soc_api_enable = {
	.set = set_soc_api_enable,
	.get = get_soc_api_enable,
};
module_param_cb(mist_soc_api_enable, &param_soc_api_enable, &mist_soc_api_enable , 0644);

u32 cpufreq_to_memfreq(int cid, u32 next_freq, int hw_type)
{
	struct cpufreq_memfreq_map *map=NULL;
	u32 mem_khz = 0;
	int cpu, index;
	int cpus_id =0;
	int j;
	struct dflt_latency_cpu_stats *stats;

	for_each_possible_cpu(cpu) {
		if (cpu_topology[cpu].cluster_id == cid) {
			int found = 0;
			for ( j=0 ;j < NUM_CLUSTER_FREQ_MAPS ; j++ ){
				if (!mist_map[hw_type][j] )
					continue;
				if (cpumask_test_cpu(cpu, &mist_cpus[hw_type][j])) {
					cpus_id = j ;
					found = 1 ;
					break;
				}
			}
			if (found)
				break;
		}
	}
	if (!mist_map[hw_type][cpus_id])
		goto out;
	if (unlikely(dflt_debug))
		pr_err(" %s cid: %d, cpus_id: %d ,mist_cpus[%d][%d] :%0lx \n", __func__, cid, cpus_id,hw_type,j,mist_cpus[hw_type][j].bits[0]);

	map = mist_map[hw_type][cpus_id];
	while (map->cpufreq_mhz && map->cpufreq_mhz < next_freq) {

		if (unlikely(dflt_debug))
			pr_err(" %s map cpufreq_mhz :%d, memfreq_khz: %d!\n", __func__, map->cpufreq_mhz, map->memfreq_khz);

		map++;
	}

	for (index = 0; index < cpu_list_map[cid].length; index++) {
		cpu = cpu_list_map[cid].cpus[index];
		stats = per_cpu(sampling_stats, cpu);
		if (!map->cpufreq_mhz) {
			break;
		}
		if (unlikely(dflt_debug))
			pr_err(" %s cur_spm :%d!, ips: %d \n", __func__,stats->spm[0],  stats->ips);

		if (stats->spm[0] > dflt_spm_thresh[cid]) {
			if (unlikely(dflt_debug))
				pr_err(" %s cur spm :%d!\n", __func__,stats->spm[0]);

			map++;
			break;
		}

		if (stats->ips < dflt_ips_thresh[cid]) {
			if (unlikely(dflt_debug))
				pr_err(" %s cur ips:%d!\n", __func__, stats->ips);

			map++;
			break;
		}
	}

	if (!map->cpufreq_mhz)
		map--;

	mem_khz = map->memfreq_khz;

	if (unlikely(dflt_debug))
		pr_err(" %s next freq %d ,cpufreq_mhz %d mem_khz:%d!\n", __func__,next_freq, map->cpufreq_mhz, mem_khz);

	out:
	return mem_khz;
}

static void commit_fast_vote(struct work_struct *work)
{
	struct dcvs_freq voted_freqs[2], new_freq;
	u32 commit_mask = 0;
	unsigned long flags;
	int ret, hw_type;

	local_irq_save(flags);
	spin_lock_nested(&dflt_commit_lock, SINGLE_DEPTH_NESTING);
	for(hw_type = 0; hw_type < 2; hw_type ++) {
		if(cur_vote_freq[hw_type]) {
			commit_mask |= BIT(hw_type);
			voted_freqs[hw_type].hw_type = hw_type;
			voted_freqs[hw_type].ib = cur_vote_freq[hw_type];
			voted_freqs[hw_type].ab = 0;

		if (unlikely(dflt_debug))
			pr_info("%s enter hw_type: %d, cur_vote_freq:%u, commit_mask %d", __FUNCTION__, hw_type, cur_vote_freq[hw_type], commit_mask);

			if (unlikely(dflt_ftrace_debug))
				trace_printk("frame_id = %llu, hw_type: %d, cur_vote_freq = %u \n",frame_index ,hw_type, cur_vote_freq[hw_type]);
		}
	}

	ret = qcom_dcvs_update_votes(MEMLAT_FP_NAME, voted_freqs, commit_mask,
			DCVS_FAST_PATH);
	if (ret < 0) {
		spin_unlock(&dflt_commit_lock);
		local_irq_restore(flags);
		return;
	}

	if (unlikely(dflt_debug))
		pr_info("%s qcom_dcvs_update_votes ret %d", __FUNCTION__, ret);

	if (ret == 0) {
		for(hw_type = 0; hw_type < 2; hw_type ++) {
			if (!(commit_mask & BIT(hw_type)))
				continue;
			last_vote_freq[hw_type] = cur_vote_freq[hw_type];
		}
	}
	spin_unlock(&dflt_commit_lock);
	local_irq_restore(flags);
	if(cur_vote_freq[DFLT_L3]) {
		new_freq.ib = cur_vote_freq[DFLT_L3];
		new_freq.ab = 0;
		new_freq.hw_type = 2;
		ret = qcom_dcvs_update_votes("soc:qcom,memlat:l3", &new_freq, 1, DCVS_SLOW_PATH);
		if (ret < 0) {
			pr_err("%s qcom dcvs  l3 vote err: %d\n",__FUNCTION__, ret);
		} else {
			last_vote_freq[DFLT_L3] = cur_vote_freq[DFLT_L3];
			if (unlikely(dflt_ftrace_debug))
				trace_printk("frame_id = %llu, hw_type: %d, cur_vote_freq = %u \n",frame_index ,DFLT_L3, cur_vote_freq[DFLT_L3]);
		}
	}
}

int mi_dflt_get_latency_enable(void)
{
	return atomic_read(&dflt_latency_enable);
}
EXPORT_SYMBOL_GPL(mi_dflt_get_latency_enable);

int mist_update_ddr_freq(int cid,int target_freq)
{
	int ret = 0;
	int i, cpu;
	u32 min_stall = mist_soc_min_stall;
	struct mist_frame_stats *frame_record;
	void *data = NULL;

	if(!mi_dflt_get_latency_enable()) {
		return ret;
	}

	if(!mist_memlat_vote_start) {
		return ret;
	}

	mist_next_freq[cid] = target_freq;
	mist_commit_mask |= BIT(cid);
	frame_record = &frame_stats_record[cur_record];
	if(min_stall < 1024 && min_stall > 0) {
		for(i = 0; i < cpu_list_map[cid].length; i++) {
			cpu = cpu_list_map[cid].cpus[i];
			min_stall = MIN(frame_record->stall_ratio[cpu], min_stall);
		}
	}
	ret = target_freq*(1024 - min_stall)/1024;

	if (unlikely(dflt_debug))
		pr_info("%s enter cid: %d, target_freq:%d, frame_index %llu, new freq:%d, min_stall = %u, mist_commit_mask = %d mist_cluster_mask=%d \n",
				__FUNCTION__, cid, target_freq ,frame_index, ret, min_stall, mist_commit_mask,mist_cluster_mask);

	if(ret) {
		mist_next_freq[cid] = ret;
	}
	//start vote timer
	if( (mist_commit_mask ^ mist_cluster_mask) == 0 ) {
		mist_commit_mask = 0;
		mist_memlat_vote_start = false;
		if (unlikely(dflt_ftrace_debug)) {
			clk_debug_mc_cc_get(data, &mc_cc);
			trace_printk("frame_index = %llu, l_freq = %d, m_freq = %d, m2_freq = %d, b_freq = %d, ddr_freq = %llu \n",
									frame_index,
									mist_next_freq[0],
									mist_next_freq[1],
									mist_next_freq[2],
									mist_next_freq[3],
									mc_cc
									);
		}

		if (unlikely(dflt_debug))
			pr_info("%s ,frame_index = %llu, l_freq = %d, m_freq = %d, m2_freq = %d, b_freq = %d \n",
									__FUNCTION__,
									frame_index,
									mist_next_freq[0],
									mist_next_freq[1],
									mist_next_freq[2],
									mist_next_freq[3]
									);

		if (!mist_ddr_vote_enable()) {
			if (hrtimer_active(&dflt_vote_timer))
				return ret;
			hrtimer_start(&dflt_vote_timer, ms_to_ktime(1), HRTIMER_MODE_REL);
		}

	}

	// if (unlikely(dflt_ftrace_debug)) {
	// 	trace_printk("frame_index = %llu, l_freq = %d, m_freq = %d, m2_freq = %d, b_freq = %d \n",
	// 							frame_index,
	// 							mist_next_freq[0],
	// 							mist_next_freq[1],
	// 							mist_next_freq[2],
	// 							mist_next_freq[3]
	// 							);
	// }


	// //start vote timer
	// if(!mist_ddr_vote_enable) {
	// 	if (hrtimer_active(&dflt_vote_timer))
	// 		hrtimer_cancel(&dflt_vote_timer);

	// 	hrtimer_start(&dflt_vote_timer, ms_to_ktime(1), HRTIMER_MODE_REL);
	// }

	return ret;
}
EXPORT_SYMBOL_GPL(mist_update_ddr_freq);

bool mi_dflt_get_latency_sample_start(void)
{
	return dflt_memlat_sample_start;
}
EXPORT_SYMBOL_GPL(mi_dflt_get_latency_sample_start);

void mi_dflt_set_latency_sample_start(bool new_dflt_latency_sample_start)
{
	dflt_memlat_sample_start = new_dflt_latency_sample_start;
	if (!new_dflt_latency_sample_start) {
		dflt_latency_cur_vote_counter++;

		if (unlikely(dflt_debug))
			pr_info("%s dflt_latency_cur_vote_counter: %d",
				__FUNCTION__, dflt_latency_cur_vote_counter);
	}
}
EXPORT_SYMBOL_GPL(mi_dflt_set_latency_sample_start);

bool mi_dflt_get_latency_vote_start(void)
{
	return dflt_latency_vote_start;
}
EXPORT_SYMBOL_GPL(mi_dflt_get_latency_vote_start);

void mi_dflt_set_latency_vote_start(bool new_dflt_latency_vote_start)
{
	if (unlikely(dflt_debug)) {
		if (!new_dflt_latency_vote_start) {
			pr_info("%s enter", __FUNCTION__);
		}
	}

	if ((dflt_latency_cur_vote_counter > 1) &&
		(new_dflt_latency_vote_start)) {
		//pr_info ("%s invalid mi_dflt_set_latency_vote_start", __FUNCTION__);
		return;
	}

	dflt_latency_vote_start = new_dflt_latency_vote_start;
}
EXPORT_SYMBOL_GPL(mi_dflt_set_latency_vote_start);

int mi_dflt_get_latency_enable_from_user(void)
{
	return dflt_latency_enable_from_user;
}
EXPORT_SYMBOL_GPL(mi_dflt_get_latency_enable_from_user);

void mi_dflt_set_latency_enable(int new_dflt_latency_enable)
{
    atomic_set(&dflt_latency_enable, new_dflt_latency_enable);
}
EXPORT_SYMBOL_GPL(mi_dflt_set_latency_enable);

//get module working status
int mi_dflt_get_bwmon_enable(void)
{
	return dflt_bwmon_enable;
}
EXPORT_SYMBOL_GPL(mi_dflt_get_bwmon_enable);

void mi_dflt_set_bwmon_enable(int new_dflt_bwmon_enable)
{
	dflt_bwmon_enable = new_dflt_bwmon_enable;
}
EXPORT_SYMBOL_GPL(mi_dflt_set_bwmon_enable);

//get frame start
bool mi_dflt_get_bwmon_sample_start(int hw_type)
{
	if (unlikely(hw_type >= DFLT_BW_HW_TYPES))
		return false;

	if (unlikely(hw_type < 0))
		return false;

	return dflt_bwmon_sample_start[hw_type];
}
EXPORT_SYMBOL_GPL(mi_dflt_get_bwmon_sample_start);

void mi_dflt_set_bwmon_sample_start(int hw_type,
				    bool new_dflt_bwmon_sample_start)
{
	if (unlikely(hw_type >= DFLT_BW_HW_TYPES))
		return;

	if (unlikely(hw_type < 0))
		return;

	if (unlikely(dflt_debug))
		pr_info("%s enter new_dflt_bwmon_sample_start: %d, cur:%d",
			__FUNCTION__, new_dflt_bwmon_sample_start,
			dflt_bwmon_sample_start[hw_type]);

	if (unlikely(dflt_bwmon_counter_enable)) {
		if (hw_type == DFLT_DDR)
			dflt_bwmon_counter++;
	}

	dflt_bwmon_sample_start[hw_type] = new_dflt_bwmon_sample_start;
}
EXPORT_SYMBOL_GPL(mi_dflt_set_bwmon_sample_start);

u32* mi_dflt_get_ddr_frame_load(void)
{
	return &cur_dflt_ib[0];
}
EXPORT_SYMBOL_GPL(mi_dflt_get_ddr_frame_load);


int mist_perf_api_enable(void)
{
	return atomic_read(&mist_soc_api_enable);
}
EXPORT_SYMBOL_GPL(mist_perf_api_enable);





static unsigned int map_coeff_freq(unsigned int cpu, unsigned long max_freq, unsigned long util, unsigned long max )
{
	unsigned long target_freq = 0;
	u64 min_stall = mist_soc_min_stall;
	int i;
	struct mist_frame_stats *frame_record;
	int cid = cluster[cpu];

	frame_record = &frame_stats_record[cur_record];

	for(i = 0; i < cpu_list_map[cid].length; i++) {
		cpu = cpu_list_map[cid].cpus[i];
		min_stall = MIN(frame_record->stall_ratio[cpu], min_stall);
	}

	if(min_stall < 1024 && min_stall >= 0) {
		target_freq = mult_frac(max_freq * util, (1024 - min_stall), 1024 * max);
		target_freq = dflt_fpercent_other ?
			(target_freq + (target_freq >> dflt_fpercent_other)) : target_freq;
	}
	if (unlikely(dflt_debug))
		pr_info("%s [%u]: target_freq[%lu], util = %lu, max = %lu, min_stall %llu", __func__, cpu, target_freq, util, max, min_stall);

	if (target_freq && (target_freq < max_freq)) {
		return target_freq;
	} else{
		return max_freq;
	}
}

void mist_set_next_freq_hook(unsigned long util, unsigned long freq, unsigned long max,
	unsigned long *next_freq, struct cpufreq_policy *policy, bool *need_freq_update)
{
	unsigned long old_freq;
	unsigned int cpu = policy->cpu;
	int cid = cluster[cpu];

	unsigned int exc_util = max >> EXC_UTIL_F_DIVISOR;

	old_freq = *next_freq;

	if (old_freq <= mist_soc_freq_thresh[cid])
	{
		return;
	}


	if(util > exc_util) {
		*next_freq = map_coeff_freq(cpu, policy->cpuinfo.max_freq, util, max);
		if(*next_freq) {
			*next_freq = MIN(old_freq, *next_freq);
			if (unlikely(dflt_debug))
				pr_info("%s [%u]: *next_freq[%lu], old_freq = %lu, freq = %lu", __func__, cpu, *next_freq, old_freq, freq);

		}
	}
}
EXPORT_SYMBOL_GPL(mist_set_next_freq_hook);


//set dflt ib by bwmon
void mi_dflt_bwmon_update_cur_freq_hook(u32 new_dflt_ib,int hw_type)
{
	if (unlikely(dflt_debug))
		pr_info("%s new ib:%d, cur ib:%d, hw_type:%d", __FUNCTION__, new_dflt_ib,
			cur_dflt_ib[hw_type], hw_type);

	cur_dflt_ib[hw_type] = new_dflt_ib;

	if (need_boost_in_frame) {
		pr_info("%s ", __FUNCTION__);
		//queue_delayed_work(mist_wq, &ceiling_rem,
		//	msecs_to_jiffies(1));
	}
}
EXPORT_SYMBOL_GPL(mi_dflt_bwmon_update_cur_freq_hook);

static void frame_record_items_reset(int record_items) {
	struct mist_frame_stats *frame_record;
	int cluster, j;//, i;

	frame_record = &frame_stats_record[record_items];
    frame_record->q2q_time = 0;
    frame_record->ddr_freq = 0;

	for(cluster = 0; cluster < 3; cluster++) {
      frame_record->real_freq[cluster] = 0;
	}

	for (j = 0; j < 8; j++) {
      frame_record->instruction[j] = 0;
      frame_record->ipc[j] = 0;
      frame_record->miss[j] = 0;
      frame_record->stall_ratio[j] = 0;
	}
}

void trigger_mist_dflt(void)
{
	struct dflt_latency_cpu_stats *stats;
	int cpu = 0;
	struct mist_frame_stats *prev_frame_record;

	prev_frame_record = &frame_stats_record[cur_record];

	if ((!mi_dflt_get_latency_enable()) && (dflt_bwmon_enable <= 0)) {
		return;
	}

	if (unlikely(dflt_debug))
		pr_info("%s enter ", __FUNCTION__);

	dflt_latency_vote_start = false;
	//1.trigger BW monitor
	mi_dflt_set_bwmon_sample_start(DFLT_DDR, true);
	mi_dflt_set_bwmon_sample_start(DFLT_LLCC, true);

	//2.trigger memlat monitor
	dflt_latency_cur_vote_counter = 0;
	dflt_memlat_sample_start = true;
	mist_memlat_vote_start = true;
	mist_commit_mask = 0;
	atomic_set(&mem_sample_counter, 0);

	for_each_possible_cpu (cpu) {
		stats = per_cpu(sampling_stats, cpu);
		stats->dflt_latency_sample_start = true;
	}
	last_frame_start_time = frame_start_time;

	frame_start_time = ktime_get();
	frame_index ++;
	if(last_frame_start_time) {
		prev_frame_record-> q2q_time = ktime_us_delta(frame_start_time, last_frame_start_time);

		if (unlikely(dflt_ftrace_debug)) {
			trace_printk("frame_index = %llu, q2q_time = %llu \n",frame_index, prev_frame_record-> q2q_time);
		}
		//cur_ddr_clk = get_cur_DDR_freq();
		//prev_frame_record-> ddr_freq = cur_ddr_clk;

		//pr_err("%s ddr_fp on cur_ddr_clk  %u", __FUNCTION__, cur_ddr_clk);
		cur_record++;
		cur_record %= FRAME_STATS_RECORD;
	}
	if (!hrtimer_active(&dflt_timer))
		hrtimer_start(&dflt_timer, ms_to_ktime(0), HRTIMER_MODE_REL);

	if(mi_dflt_get_latency_enable() && mist_memlat_vote_enable) {
		if(hrtimer_active(&memlat_vote_timer))
			return;

		hrtimer_start(&memlat_vote_timer, ms_to_ktime(1), HRTIMER_MODE_REL);
	}
}
EXPORT_SYMBOL_GPL(trigger_mist_dflt);

void mist_update_rel_freq(struct cpufreq_policy *policy, unsigned long next_freq)
{
	struct mist_frame_stats  *prev_frame_record;
	unsigned int prev_record, cur_record_items = cur_record;
	int cid, cpu_cl = policy->cpu;

	if (unlikely(cur_record_items == 0))
		prev_record = FRAME_STATS_RECORD - 1;
	else
		prev_record = cur_record_items - 1;

	cid = cluster[cpu_cl];
	prev_frame_record = &frame_stats_record[prev_record];

	//record real util&freq
	prev_frame_record->real_freq[cid] = next_freq;
}
EXPORT_SYMBOL_GPL(mist_update_rel_freq);

void mi_dflt_process_raw_ctrs_hook(int dflt_cpu, u64 cycle_data, u64 stall_data)
{
	int cpu = dflt_cpu;
	struct mist_frame_stats *prev_frame_record;
	struct dflt_latency_cpu_stats *stats = per_cpu(sampling_stats, cpu);
	u64 delta_cpu = 0;
	u64 delta_stall = 0;
	u32 be = 0;

	prev_frame_record = &frame_stats_record[cur_record];

	if (unlikely(dflt_debug == 5))
		pr_info("%s cpu: %d cycle_data: %llu, stall_data: %llu",
			__FUNCTION__, cpu, cycle_data, stall_data);

	stats->curr_ctrs[CYC_EV_IDX] = cycle_data;
	stats->curr_ctrs[BE_STALL_IDX] = stall_data;
	delta_cpu = stats->curr_ctrs[CYC_EV_IDX] - stats->prev_ctrs[CYC_EV_IDX];
	delta_stall = stats->curr_ctrs[BE_STALL_IDX] - stats->prev_ctrs[BE_STALL_IDX];

	if(delta_cpu) {
		be = mult_frac(1024, delta_stall, delta_cpu);
		prev_frame_record->stall_ratio[dflt_cpu] = mult_frac(be, be_procent, 100);
		if (unlikely(dflt_debug))
			pr_info("%s cpu: %d stall_ratio: %u",__FUNCTION__, cpu, prev_frame_record->stall_ratio[dflt_cpu]);
	}
}
EXPORT_SYMBOL_GPL(mi_dflt_process_raw_ctrs_hook);

void mi_dflt_calculate_dflt_lantancy_hook(int hw_type, int cpu, s64 delta_us,
					  u64 instructions, u64 miss,
					  u64 cycles)
{
	struct dflt_latency_cpu_stats *stats = per_cpu(sampling_stats, cpu);
	int i;
	u64 valid_cycles;
	struct mist_frame_stats  *prev_frame_record;
	unsigned int prev_record, cur_record_items = cur_record;

	if (unlikely(cur_record_items == 0))
		prev_record = FRAME_STATS_RECORD - 1;
	else
		prev_record = cur_record_items - 1;

	prev_frame_record = &frame_stats_record[prev_record];

	if (unlikely(dflt_debug == 5))
		pr_info(" %s  cpu:%d, delta_us:%lld!, instructions:%llu, miss:%llu, cycles:%llu\n",
			__func__, cpu, delta_us, instructions, miss, cycles);

	if (!stats) {
		pr_info(" %s  stat\n", __func__);
		return;
	}

	if ((hw_type >= NUM_DFLT_HW_TYPES) || (hw_type < DFLT_DDR)) {
		if (unlikely(dflt_debug == 5))
			pr_info(" %s  invalid hw type: %d\n", __func__,
				hw_type);
		return;
	}

	if (hw_type == DFLT_DDR) {
		for (i = 0; i < NUM_DFLT_EVS; i++) {
			if (!dflt_ev_ids[i])
				continue;
			stats->delta_ctrs[i] =
				stats->curr_ctrs[i] - stats->prev_ctrs[i];
			if (unlikely(dflt_debug == 5))
				pr_info(" %s, id =  %d,curr_ctrs:%llu, del: %llu, prev:%llu!\n",
					__func__, i, stats->curr_ctrs[i],
					stats->delta_ctrs[i], stats->prev_ctrs[i]);
		}
		memcpy(&stats->prev_ctrs, &stats->curr_ctrs, sizeof(stats->curr_ctrs));
	}

	stats->spm[hw_type] = stats->delta_ctrs[BE_STALL_IDX];

	stats->freq_mhz = stats->delta_ctrs[CYC_EV_IDX] / delta_us;
	if (unlikely(dflt_debug))
		pr_info(" %s, orgin spm:%u, freq_mhz: %d, cycle %llu!\n",
			__func__, stats->spm[hw_type], stats->freq_mhz,
			stats->delta_ctrs[CYC_EV_IDX]);

	stats->ll_miss = miss;

	if (miss)
		stats->spm[hw_type] /= miss;

	// if (cycles)
	//  	stats->spm[hw_type] = mult_frac(1024, stats->delta_ctrs[BE_STALL_IDX], cycles);

	valid_cycles = cycles - stats->delta_ctrs[BE_STALL_IDX];
	stats->ips = mult_frac(100, instructions, cycles);
	if (unlikely(dflt_debug == 5))
		pr_info(" %s, orgin cycles :%llu, stall: %llu,valid_cycles: %llu, instructions: %llu,ips: %u!\n",
			__func__, cycles, stats->delta_ctrs[BE_STALL_IDX],
			valid_cycles, instructions, stats->ips);

	//mi_dflt_set_latency_sample_start(false);

	prev_frame_record->ipc[cpu] = mult_frac(100, instructions, cycles);
	prev_frame_record->miss[cpu] = miss;
	prev_frame_record->stall_ratio[cpu] = mult_frac(1024, stats->delta_ctrs[BE_STALL_IDX], cycles);
	prev_frame_record->instruction[cpu] = instructions;

	if (hw_type == DFLT_DDR)
		if (unlikely(dflt_ftrace_debug)) {
			trace_printk("frame_index = %llu, cpu = %d, ipc = %llu, miss = %llu, stall_ratio= %u, instruction = %llu\n", frame_index, cpu,
											prev_frame_record->ipc[cpu], prev_frame_record->miss[cpu], prev_frame_record->stall_ratio[cpu],
											prev_frame_record->instruction[cpu]);
		}


	if (unlikely(dflt_debug))
		pr_info(" %s cpu:%d, ips:%u, spm:%u!\n", __func__, cpu,
			stats->ips, stats->spm[hw_type]);
}
EXPORT_SYMBOL_GPL(mi_dflt_calculate_dflt_lantancy_hook);

void mi_dflt_calculate_mon_sampling_freq_hook(cpumask_t cpus, int hw_type)
{
	struct dflt_latency_cpu_stats *stats;
	int cpu = 0; //max_cpu = cpumask_first(&cpus);
	//u32 max_memfreq;
	u32 max_spm_cpufreq = 0;
	u32 miss_delta, miss_delta_pct;
	//u32 hw = hw_type;
	u32 max_spm = 0, avg_spm = 0; // max_l2miss_ratio = 0;
	u32 i, spm_max_vote_khz = 0; //j, vote_idx,
	u64 max_miss = 0;

	if (hw_type != DFLT_DDR) {
		if (unlikely(dflt_debug))
			pr_info(" %s  invalid hw type: %d\n", __func__,
				hw_type);

		return;
	}

	if (dflt_cpu_mask_debug) {
		for_each_cpu (cpu, to_cpumask(&dflt_cpu_mask_debug)) {
			stats = per_cpu(sampling_stats, cpu);
			if (unlikely(dflt_debug))
				pr_info(" %s  stat, cpu: %d\n", __func__, cpu);

			if (!stats) {
				return;
			}

			max_miss = max(stats->ll_miss, max_miss);
			max_spm_cpufreq = max(max_spm_cpufreq, stats->freq_mhz);
			max_spm = max(stats->spm[hw_type], max_spm);
		}
	} else {
		for_each_cpu (cpu, &cpus) {
			stats = per_cpu(sampling_stats, cpu);

			if (!stats) {
				pr_info(" %s  stat\n", __func__);
				return;
			}

			max_miss = max(stats->ll_miss, max_miss);
			max_spm_cpufreq = max(max_spm_cpufreq, stats->freq_mhz);
			max_spm = max(stats->spm[hw_type], max_spm);
		}
	}

	if (unlikely(dflt_debug))
		pr_info(" %s, max_spm:%d, max_spm_cpufreq: %d!\n", __func__,
			max_spm, max_spm_cpufreq);

	if (max_spm < dflt_spm_low_threshold) {
		dflt_ddr_ib_scale = dflt_ddr_ib_scale_low;
	} else if (max_spm > dflt_spm_high_threshold) {
		dflt_ddr_ib_scale = dflt_ddr_ib_scale_high;
	}

	sampled_max_spm[sampled_spm_idx] = max_spm; //1.record current spm
	sampled_max_cpu_freq[sampled_spm_idx] =
		max_spm_cpufreq; //2.record current cpu frequency
	sampled_spm_idx =
		(sampled_spm_idx + 1) %
		spm_window_size; //3.update spm index, window size = 20

	for (i = 0; i < spm_window_size; i++) {
		avg_spm += sampled_max_spm[i];
		max_spm_cpufreq = max(max_spm_cpufreq, sampled_max_cpu_freq[i]);
	} //4.update avg spm and max cpu freq

	avg_spm = avg_spm / spm_window_size; //5.cal avg spm
	if (unlikely(dflt_debug))
		pr_info(" %s, avg_spm:%d!\n", __func__, avg_spm);
	if ((avg_spm >= spm_thres) &&
	    (max_spm_cpufreq >=
	     SPM_FREQ_THRES)) //(max_l2miss_ratio && max_l2miss_ratio
		//< L2MISS_RATIO_THRES)
		spm_vote_inc_steps++; //6.update vote inc steps
	else if (avg_spm < disable_spm_value && spm_vote_inc_steps >= 1)
		spm_vote_inc_steps--;

	if (spm_vote_inc_steps && max_miss < prev_max_miss) {
		miss_delta = prev_max_miss - max_miss;
		miss_delta_pct = mult_frac(100, miss_delta, prev_max_miss);
		if (miss_delta_pct >= MISS_DELTA_PCT_THRES)
			spm_vote_inc_steps = 0;
	}
	prev_max_miss = max_miss;
	if (max_spm_cpufreq < SPM_CPU_FREQ_IGN)
		spm_vote_inc_steps = 0;
	if (spm_vote_inc_steps) {
		if (max_spm_cpufreq < spm_freq_map[0].cpufreq_mhz)
			spm_max_vote_khz = spm_freq_map[0].memfreq_khz;
		else if (max_spm_cpufreq < spm_freq_map[1].cpufreq_mhz)
			spm_max_vote_khz = spm_freq_map[1].memfreq_khz;
		else
			spm_max_vote_khz = max_ddr_ib; //7. calc spm vote freq
	}
	last_ddr_latency_ib = spm_max_vote_khz;
	if (unlikely(dflt_debug))
		pr_info(" %s  spm_max_vote_khz:%d! spm_vote_inc_steps : %d, prev_max_miss : %llu\n",
			__func__, spm_max_vote_khz, spm_vote_inc_steps,
			prev_max_miss);
}

EXPORT_SYMBOL_GPL(mi_dflt_calculate_mon_sampling_freq_hook);

void mi_dflt_update_memlat_fp_vote_hook(struct dcvs_freq *voted_freqs, int grp)
{
	int i;
	if (grp < 0) {
		return;
	}

		if (voted_freqs[grp].ib < 0)
			pr_err(" %s min ib:%d, grp:%d!\n", __func__,
				voted_freqs[grp].ib, grp);


	if (grp <= dflt_memlat_vote_type) {
		//voted_freqs[grp].ib = max(voted_freqs[grp].ib, last_ddr_latency_ib);
		voted_freqs[grp].ib = min(voted_freqs[grp].ib, max_ddr_ib);
		voted_freqs[grp].ib = (voted_freqs[grp].ib * dflt_ddr_ib_scale) / 100;
		if(dflt_ddr_boost) {
			for(i = 0; i < 7; i++) {
				if(voted_freqs[grp].ib < ddr_freq_map[i]) {
					voted_freqs[grp].ib = ddr_freq_map[i];
					break;
				}
			}
		}

		if (unlikely(dflt_debug))
			pr_info("%s  ib:%d, grp:%d!\n", __func__,
				voted_freqs[grp].ib, grp);
	}
}
EXPORT_SYMBOL_GPL(mi_dflt_update_memlat_fp_vote_hook);

void register_mi_dflt_bw_update_cb(mi_dflt_bw_update_cb f)
{
	dflt_bw_update_cb = f;
}
EXPORT_SYMBOL_GPL(register_mi_dflt_bw_update_cb);


void unregister_mi_dflt_bw_update_cb(void)
{
	dflt_bw_update_cb = NULL;
}
EXPORT_SYMBOL_GPL(unregister_mi_dflt_bw_update_cb);

void register_mi_dflt_lat_update_cb(mi_dflt_lat_update_cb f, struct hrtimer *timer, mi_dflt_lat_cal_cb cal_f)
{
	dflt_lat_update_cb = f;
	dflt_lat_vote_timer = timer;
	dflt_lat_cal_cb = cal_f;
}
EXPORT_SYMBOL_GPL(register_mi_dflt_lat_update_cb);

void unregister_mi_dflt_lat_update_cb(void)
{
	dflt_lat_update_cb = NULL;
	dflt_lat_vote_timer = NULL;
	dflt_lat_cal_cb = NULL;
}
EXPORT_SYMBOL_GPL(unregister_mi_dflt_lat_update_cb);

void dlft_get_latency(void)
{
	int ret, cpu, cur_cpu = smp_processor_id();
	struct dflt_latency_cpu_stats *stats;
	struct dflt_latency_cpu_stats *cur_stats = per_cpu(sampling_stats, cur_cpu);
	ktime_t now = ktime_get();

	if (!mi_dflt_get_latency_sample_start())
		return;

	if (unlikely(dflt_debug))
		pr_info(" %s enter, cur_cpu: %d\n", __func__, cur_cpu);

	if (dflt_lat_update_cb == NULL) {
		pr_info(" %s dflt_lat_update_cb is null \n", __func__);
		return;
	}

	//spin_lock_irqsave(&dflt_commit_lock, flags);
	mi_dflt_set_latency_sample_start(false);

	cur_stats->raw_ctrs.num_evs = 0;
	ret = qcom_pmu_read_all_local(&cur_stats->raw_ctrs);
	if (ret < 0 || cur_stats->raw_ctrs.num_evs == 0) {
		pr_err("error reading pmu counters on cpu%d: %d\n", cur_cpu, ret);
	} else {
		dflt_lat_update_cb(cur_cpu, now, &cur_stats->raw_ctrs);
		mi_dflt_process_raw_ctrs_hook(cur_cpu, cur_stats->raw_ctrs.ev_data[1], cur_stats->raw_ctrs.ev_data[3]);
	}

	for_each_possible_cpu(cpu) {
		if (cpu == cur_cpu)
			continue;

		stats = per_cpu(sampling_stats, cpu);
		stats->raw_ctrs.num_evs = 0;
		ret = qcom_pmu_read_all(cpu, &stats->raw_ctrs);
		if (ret < 0 || stats->raw_ctrs.num_evs == 0) {
			pr_err("error reading pmu counters on cpu%d: %d\n", cpu, ret);
			continue;
		}
		dflt_lat_update_cb(cpu, now, &stats->raw_ctrs);
		mi_dflt_process_raw_ctrs_hook(cpu, stats->raw_ctrs.ev_data[1], stats->raw_ctrs.ev_data[3]);
	}

	mi_dflt_set_latency_vote_start(true);
}
EXPORT_SYMBOL_GPL(dlft_get_latency);

static enum hrtimer_restart queue_dflt_sample_start(struct hrtimer *timer)
{
	if (unlikely(dflt_debug)) {
		pr_err(" start...%s %llu", __FUNCTION__,
		       ktime_to_us(ktime_get()));
	}

	if ((mi_dflt_get_bwmon_enable()) && (dflt_bw_update_cb)) {
		dflt_bw_update_cb();
	}

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart queue_memlat_vote_start(struct hrtimer *timer)
{
	if (unlikely(dflt_debug)) {
		pr_err(" start...%s %llu", __FUNCTION__,
		       ktime_to_us(ktime_get()));
	}

	if(mi_dflt_get_latency_enable()){
	    mi_dflt_set_latency_vote_start(true);
	}

	return HRTIMER_NORESTART;
}


static enum hrtimer_restart queue_dflt_vote_start(struct hrtimer *timer)
{
	u32 max_memfreq = 0;
	int cid, hw_type;

	if (unlikely(dflt_debug)) {
		pr_err(" start...%s %llu", __FUNCTION__,
		       ktime_to_us(ktime_get()));
	}

	if (dflt_lat_cal_cb == NULL) {
		pr_info(" %s dflt_lat_cal_cb is null \n", __func__);
		return HRTIMER_NORESTART;
	}

	dflt_lat_cal_cb();

	for(hw_type = 0; hw_type < NUM_DFLT_HW_TYPES; hw_type ++) {
		max_memfreq = 0;
		for (cid = 0; cid < get_cluster_max_id(); cid++) {
			max_memfreq = max(max_memfreq, cpufreq_to_memfreq(cid, mist_next_freq[cid], hw_type));
		}
		if (last_vote_freq[hw_type] == max_memfreq) {
			cur_vote_freq[hw_type] = 0;
			continue;
		}

		cur_vote_freq[hw_type] = max_memfreq;

		if (unlikely(dflt_debug))
			pr_err(" %s type: %d, max_memfreq: %d, cur_vote_freq: %d, last_vote_freq: %d!\n", __func__, hw_type, max_memfreq, cur_vote_freq[hw_type], last_vote_freq[hw_type]);
	}

	//TODO: L3  LLCC vote


	if (work_pending(&fast_vote_work))
		return HRTIMER_NORESTART;

	queue_work(dflt_wq, &fast_vote_work);



	return HRTIMER_NORESTART;
}


void mi_dflt_schedule_hook(void)
{
	int cpu = smp_processor_id();
	//int ret;
	//unsigned long flags;

	//ktime_t now = ktime_get();

	struct dflt_latency_cpu_stats *stats = per_cpu(sampling_stats, cpu);
	//pr_err("mi_dflt_get_latency_enable..%s.", __FUNCTION__);

	//if (mi_dflt_get_latency_enable() <= 3) {
	//	return;
	//}
        //if(atomic_read(&mem_sample_counter) >= 1) {
        //return;
	//}

	if (!stats->dflt_latency_sample_start)
		return;

	stats->dflt_latency_sample_start = false;

	if (unlikely(dflt_debug))
		pr_info(" %s enter, cur_cpu: %d\n", __func__, cpu);

	if (dflt_lat_update_cb == NULL) {
		pr_info(" %s dflt_lat_update_cb is null \n", __func__);
		return;
	}
	//spin_lock_irqsave(&dflt_commit_lock, flags);

	//atomic_inc(&mem_sample_counter);
	dflt_lat_update_cb(cpu, frame_start_time, &stats->raw_ctrs);

	//stats->raw_ctrs.num_evs = 0;
	//ret = qcom_pmu_read_all_local(&stats->raw_ctrs);
	//if (ret < 0 || stats->raw_ctrs.num_evs == 0) {
	//	pr_err("error reading pmu counters on cpu%d: %d\n", cpu, ret);
	//} else {
	//	if (unlikely(dflt_debug))
	//		pr_info(" %s qcom_pmu_read_all_local, num_evs: %d, mem_sample_counter %d\n",__func__, stats->raw_ctrs.num_evs ,atomic_read(&mem_sample_counter));
	//	dflt_lat_update_cb(cpu, now, &stats->raw_ctrs);
	//	mi_dflt_process_raw_ctrs_hook(cpu, stats->raw_ctrs.ev_data[1], stats->raw_ctrs.ev_data[3]);
	//}

    //if(atomic_read(&mem_sample_counter) >= 6) {
		//dflt_lat_update_cb(8, now, &stats->raw_ctrs);
	//}

	//spin_unlock_irqrestore(&dflt_commit_lock, flags);
}
EXPORT_SYMBOL_GPL(mi_dflt_schedule_hook);

static int dump_his_load_info(int index, struct seq_file *m, void *v)
{
	char cluster_name[4] = "lmb";
	int j, i = index % FRAME_STATS_RECORD;
	struct mist_frame_stats *cur_frame_stats_record = &frame_stats_record[i];

	seq_printf(m, "{q2q = %llu us\n",
			cur_frame_stats_record->q2q_time);

	seq_printf(m, "ddr_freq = %u \n", cur_frame_stats_record->ddr_freq);


	for (j = 0; j <= 2; j++) {
		seq_printf(m, "[%c]: ", cluster_name[j]);
		seq_printf(m, "%9llu  ", cur_frame_stats_record->real_freq[j]);
	}
	seq_printf(m, "\n");

	for (j = 0; j < 8; j++) {
		seq_printf(m, "[cpu %d]: ", j);
		seq_printf(m, "ins %llu ", cur_frame_stats_record->instruction[j]);
		seq_printf(m, "ipc %llu ", cur_frame_stats_record->ipc[j]);
		seq_printf(m, "miss %llu ", cur_frame_stats_record->miss[j]);
		seq_printf(m, "stall_ratio %u.\n", cur_frame_stats_record->stall_ratio[j]);
	}

	seq_printf(m, "}\n\n");
	return 0;
}


static int history_load_show(struct seq_file *m, void *v)
{
	int i, index = 0;

	index = cur_record;
	for (i = index + 1; i < FRAME_STATS_RECORD; i++)
		dump_his_load_info(i, m, v);
	for (i = 0; i <= index; i++)
		dump_his_load_info(i, m, v);
	return 0;
}


static int parse_cluster_freq_map(struct device_node *silver_node,int hw_type,int num_map,int *cnt)
{
	cpumask_t cpus ;
	int i,j,len,nf;
	int ret ;
	u32 data;

	if (!silver_node) {
		pr_err(" %s Invalid silver_node is null\n",__func__);
		return 0;
	}
	if(hw_type >= NUM_DFLT_HW_TYPES || num_map >= NUM_CLUSTER_FREQ_MAPS) {
		pr_err(" %s Invalid index: hw_type=%d, num_map=%d\n", __func__,hw_type, num_map);
		return 0;
	}
	if(!of_get_property(silver_node,"qcom,cpulist",&len))
	{
		pr_err("  Failed to get qcom,cpulist len %d\n",len);
		return 0;
	}

	cpumask_clear(&cpus);

	for (i = 0 ; i < len/sizeof(u32) ; i++) {
		cpumask_set_cpu((i + *cnt), &cpus);
	}
	*cnt += len/sizeof(u32);

	mist_cpus[hw_type][num_map] = cpus;

	if (!of_find_property(silver_node, COREDEV_TBL_PROP, &len)){
		return 0;
	}

	len /= sizeof(u32);
	if (len % NUM_COLS || len == 0)
			return 0;
	nf = len / NUM_COLS;
	mist_map[hw_type][num_map] = kzalloc( (nf + 1) * sizeof(struct cpufreq_memfreq_map),
				GFP_KERNEL);
	if (!mist_map[hw_type][num_map])
			return -1;
	for (i = 0, j = 0; i < nf; i++, j += 2) {
		ret = of_property_read_u32_index(silver_node, COREDEV_TBL_PROP,
							j, &data);
		if (ret < 0)
			return -1;
		mist_map[hw_type][num_map][i].cpufreq_mhz = data;

		ret = of_property_read_u32_index(silver_node, COREDEV_TBL_PROP,
							j + 1, &data);
		if (ret < 0)
			return -1;
		mist_map[hw_type][num_map][i].memfreq_khz = data;
		if (unlikely(dflt_debug))
			pr_err("%s mist_map[%d][%d][%d].memfreq_khz: %d,mist_map[%d][%d][%d].cpufreq_mhz: %d\n",__func__,
				hw_type,num_map,i,mist_map[hw_type][num_map][i].memfreq_khz,
				hw_type,num_map,i,mist_map[hw_type][num_map][i].cpufreq_mhz);
	}
	return 1;
}

static int history_load_open(struct inode *inode, struct file *file)
{
	return single_open(file, history_load_show, NULL);
}

static const struct proc_ops history_load_fops = {
	.proc_open      = history_load_open,
	.proc_read      = seq_read,
	.proc_lseek    = seq_lseek,
	.proc_release   = single_release,
};

static int parse_cluster_node_map(struct device_node *parent,int hw_type)
{
	int num_map = 0 ;
	int cnt = 0;
	struct device_node *child;
	int ret;
	for_each_available_child_of_node(parent, child) {
		ret = parse_cluster_freq_map(child,hw_type,num_map,&cnt);
		if (ret == -1) {
			return -1;
		}else if (ret == 0)
			continue;
		if(cnt >= MAX_CPU_NUM)
			break;
		num_map++;
	}

	return 1;
}

int mist_dflt_init(void)
{
	int ret = 0;
	struct dflt_latency_cpu_stats *stats;
	struct device_node *root_node;
	struct device_node *ddr_node,*llcc_node,*l3_node;
	int cpu = 0;
	int i;

	hrtimer_init(&dflt_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	dflt_timer.function = queue_dflt_sample_start;

	frame_index = 0;

	hrtimer_init(&dflt_vote_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	dflt_vote_timer.function = queue_dflt_vote_start;

    hrtimer_init(&memlat_vote_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	memlat_vote_timer.function = queue_memlat_vote_start;

	for_each_possible_cpu (cpu) {
		stats = kzalloc(sizeof(*stats), GFP_KERNEL);
		if (!stats) {
			ret = -ENOMEM;
			goto free_stats;
		}
		per_cpu(sampling_stats, cpu) = stats;
		spin_lock_init(&stats->ctrs_lock);
	}
	mist_cluster_mask = (1 << get_cluster_max_id()) - 1;
	root_node = of_find_node_by_name(NULL, "qcom,memlat");
	if (!root_node) {
		pr_err(" Failed to find node 'qcom,memlat'\n");
		return -ENODEV;
	}

	ddr_node = of_get_child_by_name(root_node,"ddr");

	llcc_node = of_get_child_by_name(root_node,"llcc");

	l3_node = of_get_child_by_name(root_node,"l3");

	of_node_put(root_node);

	if ( parse_cluster_node_map(ddr_node,DFLT_DDR) == -1)
		goto free_mistmap;
	if ( parse_cluster_node_map(llcc_node,DFLT_LLCC) == -1)
		goto free_mistmap;
	if (parse_cluster_node_map(l3_node,DFLT_L3) == -1)
		goto free_mistmap;
	of_node_put(ddr_node);
	of_node_put(llcc_node);
	of_node_put(l3_node);

	dflt_ev_ids[FE_STALL_IDX] = 0x23;
	dflt_ev_data[FE_STALL_IDX].event_id = 0x23;
	dflt_ev_data[FE_STALL_IDX].amu_id = 0xff;
	dflt_ev_data[FE_STALL_IDX].cpus = 0xff;
	dflt_ev_data[FE_STALL_IDX].cid = 0xff;

	dflt_ev_ids[BE_STALL_IDX] = 0x24;
	dflt_ev_data[BE_STALL_IDX].event_id = 0x24;
	dflt_ev_data[BE_STALL_IDX].amu_id = 0x3;
	dflt_ev_data[BE_STALL_IDX].cpus = 0xff;
	dflt_ev_data[BE_STALL_IDX].cid = 0x03;

	dflt_ev_ids[CYC_EV_IDX] = 0x11;
	dflt_ev_data[CYC_EV_IDX].event_id = 0x11;
	dflt_ev_data[CYC_EV_IDX].amu_id = 0x1;
	dflt_ev_data[CYC_EV_IDX].cpus = 0xff;
	dflt_ev_data[CYC_EV_IDX].cid = 0x00;

	spm_thres = 200u;
	//spm_drop_pct = 20;
	spm_window_size = 10;
	spm_freq_map[0].cpufreq_mhz = 1200;
	spm_freq_map[1].cpufreq_mhz = 2100;
	spm_freq_map[0].memfreq_khz = 1708000;
	spm_freq_map[1].memfreq_khz = 2092000;


	cpu_list_map[0].length = 6;
	cpu_list_map[0].spm_thresh = 80;
	cpu_list_map[0].cpus[0] = 0;
	cpu_list_map[0].cpus[1] = 1;
	cpu_list_map[0].cpus[2] = 2;
	cpu_list_map[0].cpus[3] = 3;
	cpu_list_map[0].cpus[4] = 4;
	cpu_list_map[0].cpus[5] = 5;

	cpu_list_map[1].length = 2;
	cpu_list_map[1].spm_thresh = 60;
	cpu_list_map[1].cpus[0] = 6;
	cpu_list_map[1].cpus[1] = 7;

	cpu_list_map[2].length = 2;
	cpu_list_map[2].spm_thresh = 40;
	cpu_list_map[2].cpus[0] = 5;
	cpu_list_map[2].cpus[1] = 6;

	cpu_list_map[3].length = 1;
	cpu_list_map[3].spm_thresh = 40;
	cpu_list_map[3].cpus[0] = 7;

	dflt_wq = alloc_workqueue("dflt_wq", WQ_HIGHPRI, 0);
	if (!dflt_wq) {
		ret = -EFAULT;
		goto free_mistmap;
	}
	INIT_WORK(&fast_vote_work, commit_fast_vote);

	pr_info("LINK: %s: inited!\n", __func__);
	spin_lock_init(&dflt_commit_lock);
	dflt_lat_vote_timer = NULL;

	for (i = 0; i < FRAME_STATS_RECORD; i++) {
		frame_record_items_reset(i);
	}

	frame_load_dir = proc_mkdir("mist", NULL);
	history_load_entry =
		proc_create("frame_history_load",
			0664, frame_load_dir,
			&history_load_fops);

	return ret;

free_mistmap:
	for (int i = 0; i < NUM_DFLT_HW_TYPES; i++) {
		for (int j = 0; j < NUM_CLUSTER_FREQ_MAPS; j++) {
			if (mist_map[i][j])
				kfree(mist_map[i][j]);
		}
	}
free_stats:
	for_each_possible_cpu (cpu) {
		stats = per_cpu(sampling_stats, cpu);
		if (stats) {
			kfree(stats);
		}
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mist_dflt_init);

static int __init mist_init(void)
{
	mist_dflt_init();
	mist_gflt_init();
	return 0;
}

static void __exit mist_exit(void)
{
	struct dflt_latency_cpu_stats *stats;
	int cpu = 0;

	for (int i = 0; i < NUM_DFLT_HW_TYPES; i++) {
		for (int j = 0; j < NUM_CLUSTER_FREQ_MAPS; j++) {
			if (mist_map[i][j])
				kfree(mist_map[i][j]);
		}
	}

	for_each_possible_cpu (cpu) {
		stats = per_cpu(sampling_stats, cpu);
		if (stats) {
			kfree(stats);
		}
	}

	if (frame_load_dir) {
		if (history_load_entry)
			proc_remove(history_load_entry);

		proc_remove(frame_load_dir);
	}
}

module_init(mist_init);
module_exit(mist_exit);
MODULE_LICENSE("GPL");
MODULE_VERSION(MIST_VERSION);
MODULE_DESCRIPTION("soc performance tuner developed by David");
// END Performance_TurboSched