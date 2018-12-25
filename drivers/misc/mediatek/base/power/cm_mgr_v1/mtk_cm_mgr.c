/*
 * Copyright (C) 2016 MediaTek Inc.
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

/* system includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/sched/rt.h>
#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/bitops.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/types.h>
#include <linux/suspend.h>
#include <linux/topology.h>
#include <linux/math64.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/mtk_io.h>
#include <mt-plat/aee.h>
#include <trace/events/mtk_events.h>
#include <mt-plat/met_drv.h>

#include <mtk_cm_mgr.h>
#include <mtk_cm_mgr_platform_data.h>
#include <mtk_cpufreq_api.h>

#include <linux/pm_qos.h>
#include <helio-dvfsrc.h>

spinlock_t cm_mgr_lock;

#ifdef ATF_SECURE_SMC
#include <mt-plat/mtk_secure_api.h>
#endif

static unsigned long long test_diff;
static unsigned long long cnt;
static unsigned int test_max;
static unsigned int prev_freq_idx[CM_MGR_CPU_CLUSTER];
static unsigned int prev_freq[CM_MGR_CPU_CLUSTER];
/* 0: < 50us */
/* 1: 50~100us */
/* 2: 100~200us */
/* 3: 200~300us */
/* 4: over 300us */
static unsigned int time_cnt_data[5];

static unsigned int cpu_power_up_array[CM_MGR_CPU_CLUSTER];
static unsigned int cpu_power_down_array[CM_MGR_CPU_CLUSTER];
static unsigned int cpu_power_up[CM_MGR_CPU_CLUSTER];
static unsigned int cpu_power_down[CM_MGR_CPU_CLUSTER];
static unsigned int v2f[CM_MGR_CPU_CLUSTER];
static unsigned int vcore_power_up;
static unsigned int vcore_power_down;
static int cpu_opp_cur[CM_MGR_CPU_CLUSTER];
static int ratio_max[CM_MGR_CPU_CLUSTER];
static int ratio[CM_MGR_CPU_COUNT];
static int count[CM_MGR_CPU_CLUSTER];
static int count_ack[CM_MGR_CPU_CLUSTER];
static int vcore_dram_opp;
static int vcore_dram_opp_cur;
int cm_mgr_abs_load;
int cm_mgr_rel_load;
static int total_bw;
static int cps_valid;
static int debounce_times_up;
static int debounce_times_down;
static int ratio_scale[CM_MGR_CPU_CLUSTER];
static int max_load[CM_MGR_CPU_CLUSTER];
static int cpu_load[NR_CPUS];
static int loading_acc[NR_CPUS];
static int loading_cnt;

/******************** MET BEGIN ********************/
typedef void (*cm_mgr_value_handler_t) (unsigned int cnt, unsigned int *value);

static struct cm_mgr_met_data met_data;
static cm_mgr_value_handler_t cm_mgr_power_dbg_handler;
static cm_mgr_value_handler_t cm_mgr_count_dbg_handler;
static cm_mgr_value_handler_t cm_mgr_opp_dbg_handler;
static cm_mgr_value_handler_t cm_mgr_loading_dbg_handler;
static cm_mgr_value_handler_t cm_mgr_ratio_dbg_handler;
static cm_mgr_value_handler_t cm_mgr_bw_dbg_handler;
static cm_mgr_value_handler_t cm_mgr_valid_dbg_handler;

#define CM_MGR_MET_REG_FN_VALUE(name)				\
	void cm_mgr_register_##name(cm_mgr_value_handler_t handler)	\
{								\
	name##_dbg_handler = handler;				\
}								\
EXPORT_SYMBOL(cm_mgr_register_##name)

CM_MGR_MET_REG_FN_VALUE(cm_mgr_power);
CM_MGR_MET_REG_FN_VALUE(cm_mgr_count);
CM_MGR_MET_REG_FN_VALUE(cm_mgr_opp);
CM_MGR_MET_REG_FN_VALUE(cm_mgr_loading);
CM_MGR_MET_REG_FN_VALUE(cm_mgr_ratio);
CM_MGR_MET_REG_FN_VALUE(cm_mgr_bw);
CM_MGR_MET_REG_FN_VALUE(cm_mgr_valid);
/********************* MET END *********************/

static void cm_mgr_update_met(void)
{
	int cpu;

	met_data.cm_mgr_power[0] = cpu_power_up_array[0];
	met_data.cm_mgr_power[1] = cpu_power_up_array[1];
	met_data.cm_mgr_power[2] = cpu_power_down_array[0];
	met_data.cm_mgr_power[3] = cpu_power_down_array[1];
	met_data.cm_mgr_power[4] = cpu_power_up[0];
	met_data.cm_mgr_power[5] = cpu_power_up[1];
	met_data.cm_mgr_power[6] = cpu_power_down[0];
	met_data.cm_mgr_power[7] = cpu_power_down[1];
	met_data.cm_mgr_power[8] = cpu_power_up[0] + cpu_power_up[1];
	met_data.cm_mgr_power[9] = cpu_power_down[0] + cpu_power_down[1];
	met_data.cm_mgr_power[10] = vcore_power_up;
	met_data.cm_mgr_power[11] = vcore_power_down;
	met_data.cm_mgr_power[12] = v2f[0];
	met_data.cm_mgr_power[13] = v2f[1];

	met_data.cm_mgr_count[0] = count[0];
	met_data.cm_mgr_count[1] = count[1];
	met_data.cm_mgr_count[2] = count_ack[0];
	met_data.cm_mgr_count[3] = count_ack[1];

	met_data.cm_mgr_opp[0] = vcore_dram_opp;
	met_data.cm_mgr_opp[1] = vcore_dram_opp_cur;
	met_data.cm_mgr_opp[2] = cpu_opp_cur[0];
	met_data.cm_mgr_opp[3] = cpu_opp_cur[1];
	met_data.cm_mgr_opp[4] = debounce_times_up;
	met_data.cm_mgr_opp[5] = debounce_times_down;

	met_data.cm_mgr_loading[0] = cm_mgr_abs_load;
	met_data.cm_mgr_loading[1] = cm_mgr_rel_load;
	met_data.cm_mgr_loading[2] = max_load[0];
	met_data.cm_mgr_loading[3] = max_load[1];
	for_each_possible_cpu(cpu) {
		if (cpu >= CM_MGR_CPU_COUNT)
			break;
		met_data.cm_mgr_loading[4 + cpu] = cpu_load[cpu];
	}

	met_data.cm_mgr_ratio[0] = ratio_max[0];
	met_data.cm_mgr_ratio[1] = ratio_max[1];
	met_data.cm_mgr_ratio[2] = ratio_scale[0];
	met_data.cm_mgr_ratio[3] = ratio_scale[1];
	met_data.cm_mgr_ratio[4] = ratio[0];
	met_data.cm_mgr_ratio[5] = ratio[1];
	met_data.cm_mgr_ratio[6] = ratio[2];
	met_data.cm_mgr_ratio[7] = ratio[3];
	met_data.cm_mgr_ratio[8] = ratio[4];
	met_data.cm_mgr_ratio[9] = ratio[5];
	met_data.cm_mgr_ratio[10] = ratio[6];
	met_data.cm_mgr_ratio[11] = ratio[7];

	met_data.cm_mgr_bw = total_bw;

	met_data.cm_mgr_valid = cps_valid;

	if (cm_mgr_power_dbg_handler)
		cm_mgr_power_dbg_handler(ARRAY_SIZE(met_data.cm_mgr_power),
				met_data.cm_mgr_power);
	if (cm_mgr_count_dbg_handler)
		cm_mgr_count_dbg_handler(ARRAY_SIZE(met_data.cm_mgr_count),
				met_data.cm_mgr_count);
	if (cm_mgr_opp_dbg_handler)
		cm_mgr_opp_dbg_handler(ARRAY_SIZE(met_data.cm_mgr_opp),
				met_data.cm_mgr_opp);
	if (cm_mgr_loading_dbg_handler)
		cm_mgr_loading_dbg_handler(ARRAY_SIZE(met_data.cm_mgr_loading),
				met_data.cm_mgr_loading);
	if (cm_mgr_ratio_dbg_handler)
		cm_mgr_ratio_dbg_handler(ARRAY_SIZE(met_data.cm_mgr_ratio),
				met_data.cm_mgr_ratio);
	if (cm_mgr_bw_dbg_handler)
		cm_mgr_bw_dbg_handler(1, &met_data.cm_mgr_bw);
	if (cm_mgr_valid_dbg_handler)
		cm_mgr_valid_dbg_handler(1, &met_data.cm_mgr_valid);
}

static void update_v2f(int update, int debug)
{
	int i, j;
	int _f, _v, _v2f;

	for (j = 0; j < CM_MGR_CPU_CLUSTER; j++) {
		for (i = 0; i < 16; i++) {
			_f = mt_cpufreq_get_freq_by_idx(j, i) / 1000;
			_v = mt_cpufreq_get_volt_by_idx(j, i) / 100;
			_v2f = (_v / 10) * (_v / 10) * _f / 100000;
			if (update)
				_v2f_all[i][j] = _v2f;
			if (debug)
				pr_debug("%d-i %.2d v %.8d f %.8d v2f %.8d\n",
						j, i, _v, _f, _v2f);
		}
	}
}

#ifdef USE_TIMER_CHECK
struct timer_list cm_mgr_timer;

static void cm_mgr_timer_fn(unsigned long data)
{
	if (cm_mgr_timer_enable)
		check_cm_mgr_status_internal();
}
#endif /* USE_TIMER_CHECK */

static int cm_mgr_check_up_status(int level, int *cpu_ratio_idx)
{
	int idx;
	int cpu_power_total;
	int i;

	idx = CM_MGR_CPU_CLUSTER * level;
	cpu_power_total = 0;
#ifdef PER_CPU_STALL_RATIO
#ifdef ATF_SECURE_SMC
	ret = mt_secure_call(MTK_SIP_KERNEL_PMU_EVA, 1, idx,
			cpu_opp_cur[0] || (cpu_opp_cur[1] << 16));
	cpu_power_up[0] = (ret & 0xffff) * v2f[0] / 100;
	cpu_power_up[1] = ((ret >> 16) & 0xffff) * v2f[1] / 100;

	for (i = 0; i < CM_MGR_CPU_CLUSTER; i++)
		cpu_power_total += cpu_power_up[i];
#else
	for (i = 0; i < CM_MGR_CPU_COUNT; i++) {
		if (i < 4)
			cpu_power_up_array[0] +=
				cpu_power_gain_opp(total_bw, IS_UP,
						cpu_opp_cur[0],
						cpu_ratio_idx[i], idx);
		else
			cpu_power_up_array[1] +=
				cpu_power_gain_opp(total_bw, IS_UP,
						cpu_opp_cur[1],
						cpu_ratio_idx[i], idx + 1);
	}

	for (i = 0; i < CM_MGR_CPU_CLUSTER; i++) {
		cpu_power_up[i] = cpu_power_up_array[i] * v2f[i] / 100;
		cpu_power_total += cpu_power_up[i];
	}
#endif
#else
	for (i = 0; i < CM_MGR_CPU_CLUSTER; i++) {
		cpu_power_up_array[i] =
			cpu_power_gain_opp(total_bw, IS_UP,
					cpu_opp_cur[i],
					max_ratio_idx[i], idx + i);
		cpu_power_up[i] = cpu_power_up_array[i] *
			count[i] * v2f[i] / 100;
		cpu_power_total += cpu_power_up[i];
	}
#endif

	if (cm_mgr_opp_enable == 0) {
		if (vcore_dram_opp != CM_MGR_EMI_OPP) {
			vcore_dram_opp = CM_MGR_EMI_OPP;
#ifdef DEBUG_CM_MGR
			pr_info("#@# %s(%d) vcore_dram_opp %d->%d\n",
					__func__, __LINE__,
					vcore_dram_opp_cur, vcore_dram_opp);
#endif /* DEBUG_CM_MGR */
			dvfsrc_set_power_model_ddr_request(
					CM_MGR_EMI_OPP - vcore_dram_opp);
		}

		return -1;
	}

	idx = level;
	vcore_power_up = vcore_power_gain(vcore_power_gain, total_bw, idx);
#ifdef DEBUG_CM_MGR
	pr_info("#@# %s(%d) vcore_power_up 0x%x cpu_power_total 0x%x\n",
			__func__, __LINE__,
			vcore_power_up, cpu_power_total);
#endif /* DEBUG_CM_MGR */
	if ((vcore_power_up * vcore_power_ratio_up[idx]) <
			(cpu_power_total * cpu_power_ratio_up[idx])) {
		debounce_times_down = 0;
		if (++debounce_times_up >= debounce_times_up_adb[idx]) {
			if (debounce_times_reset_adb)
				debounce_times_up = 0;
			vcore_dram_opp = vcore_dram_opp_cur - 1;
#ifdef DEBUG_CM_MGR
			pr_info("#@# %s(%d) vcore_dram_opp %d->%d\n",
					__func__, __LINE__,
					vcore_dram_opp_cur, vcore_dram_opp);
#endif /* DEBUG_CM_MGR */
			dvfsrc_set_power_model_ddr_request(
					CM_MGR_EMI_OPP - vcore_dram_opp);
		}

		return -1;
	}

	return 0;
}

static int cm_mgr_check_down_status(int level, int *cpu_ratio_idx)
{
	int idx;
	int cpu_power_total;
	int i;

	idx = CM_MGR_CPU_CLUSTER * (level - 1);
	cpu_power_total = 0;
#ifdef PER_CPU_STALL_RATIO
#ifdef ATF_SECURE_SMC
	ret = mt_secure_call(MTK_SIP_KERNEL_PMU_EVA, 2, idx,
			cpu_opp_cur[0] || (cpu_opp_cur[1] << 16));
	cpu_power_down[0] = (ret & 0xffff) * v2f[0] / 100;
	cpu_power_down[1] = ((ret >> 16) & 0xffff) * v2f[1] / 100;

	for (i = 0; i < CM_MGR_CPU_CLUSTER; i++)
		cpu_power_total += cpu_power_down[i];
#else
	for (i = 0; i < CM_MGR_CPU_COUNT; i++) {
		if (i < 4)
			cpu_power_down_array[0] +=
				cpu_power_gain_opp(total_bw, IS_DOWN,
						cpu_opp_cur[0],
						cpu_ratio_idx[i], idx);
		else
			cpu_power_down_array[1] +=
				cpu_power_gain_opp(total_bw, IS_DOWN,
						cpu_opp_cur[1],
						cpu_ratio_idx[i], idx + 1);
	}

	for (i = 0; i < CM_MGR_CPU_CLUSTER; i++) {
		cpu_power_down[i] = cpu_power_down_array[i] * v2f[i] / 100;
		cpu_power_total += cpu_power_down[i];
	}
#endif
#else
	for (i = 0; i < CM_MGR_CPU_CLUSTER; i++) {
		cpu_power_down_array[i] =
			cpu_power_gain_opp(total_bw, IS_DOWN,
					cpu_opp_cur[i],
					max_ratio_idx[i], idx + i);
		cpu_power_down[i] = cpu_power_down_array[i] *
			count[i] * v2f[i] / 100;
		cpu_power_total += cpu_power_down[i];
	}
#endif

	if (cm_mgr_opp_enable == 0) {
		if (vcore_dram_opp != CM_MGR_EMI_OPP) {
			vcore_dram_opp = CM_MGR_EMI_OPP;
#ifdef DEBUG_CM_MGR
			pr_info("#@# %s(%d) vcore_dram_opp %d->%d\n",
					__func__, __LINE__,
					vcore_dram_opp_cur, vcore_dram_opp);
#endif /* DEBUG_CM_MGR */
			dvfsrc_set_power_model_ddr_request(
					CM_MGR_EMI_OPP - vcore_dram_opp);
		}

		return -1;
	}

	idx = level - 1;
	vcore_power_down = vcore_power_gain(vcore_power_gain, total_bw, idx);
#ifdef DEBUG_CM_MGR
	pr_info("#@# %s(%d) vcore_power_down 0x%x cpu_power_total 0x%x\n",
			__func__, __LINE__,
			vcore_power_down, cpu_power_total);
#endif /* DEBUG_CM_MGR */
	if ((vcore_power_down * vcore_power_ratio_down[idx]) >
			(cpu_power_total * cpu_power_ratio_down[idx])) {
		debounce_times_up = 0;
		if (++debounce_times_down >= debounce_times_down_adb[idx]) {
			if (debounce_times_reset_adb)
				debounce_times_down = 0;
			vcore_dram_opp = vcore_dram_opp_cur + 1;
#ifdef DEBUG_CM_MGR
			pr_info("#@# %s(%d) vcore_dram_opp %d->%d\n",
					__func__, __LINE__,
					vcore_dram_opp_cur, vcore_dram_opp);
#endif /* DEBUG_CM_MGR */
			dvfsrc_set_power_model_ddr_request(
					CM_MGR_EMI_OPP - vcore_dram_opp);
		}
	}

	return 0;
}

int cm_mgr_perf_timer_enable;
int cm_mgr_perf_force_enable;
struct timer_list cm_mgr_perf_timer;
#define USE_TIMER_PERF_CHECK_TIME msecs_to_jiffies(50)

static void cm_mgr_perf_timer_fn(unsigned long data)
{
	if (cm_mgr_perf_timer_enable)
		check_cm_mgr_status_internal();
}

void cm_mgr_perf_set_status(int enable)
{
	cm_mgr_perf_platform_set_force_status(enable);

	if (cm_mgr_perf_force_enable)
		return;

	cm_mgr_perf_platform_set_status(enable);

	if (enable != cm_mgr_perf_timer_enable) {
		cm_mgr_perf_timer_enable = enable;

		if (enable == 1) {
			unsigned long expires;

			expires = jiffies + USE_TIMER_PERF_CHECK_TIME;
			mod_timer(&cm_mgr_perf_timer, expires);
		} else
			del_timer(&cm_mgr_perf_timer);
	}
}

void cm_mgr_perf_set_force_status(int enable)
{
	if (enable != cm_mgr_perf_force_enable) {
		cm_mgr_perf_force_enable = enable;
		if (enable == 0) {
			cm_mgr_perf_platform_set_force_status(enable);
			check_cm_mgr_status_internal();
		}
	}
}

void check_cm_mgr_status_internal(void)
{
	unsigned long long result = 0;
	ktime_t now, done;
	int level;

	if (cm_mgr_enable == 0)
		return;

	if (cm_mgr_disable_fb == 1 && cm_mgr_blank_status == 1)
		return;

	if (cm_mgr_perf_force_enable)
		return;

	if (spin_trylock(&cm_mgr_lock)) {
		int ret;
		int max_ratio_idx[CM_MGR_CPU_CLUSTER];
#if defined(LIGHT_LOAD) && defined(CONFIG_MTK_SCHED_RQAVG_US)
		unsigned int cpu;
		unsigned int rel_load, abs_load;
#endif
#ifdef PER_CPU_STALL_RATIO
		int cpu_ratio_idx[CM_MGR_CPU_COUNT];
#endif
		int i;

		vcore_dram_opp_cur = get_cur_ddr_opp();
		if (vcore_dram_opp_cur > CM_MGR_EMI_OPP) {
			spin_unlock(&cm_mgr_lock);
			return;
		}

		if (--cm_mgr_loop > 0) {
			cm_mgr_update_met();
			spin_unlock(&cm_mgr_lock);
			return;
		}
		cm_mgr_loop = cm_mgr_loop_count;

#if defined(LIGHT_LOAD) && defined(CONFIG_MTK_SCHED_RQAVG_US)
		cm_mgr_abs_load = 0;
		cm_mgr_rel_load = 0;

		for_each_online_cpu(cpu) {
			int tmp;

			if (cpu >= CM_MGR_CPU_COUNT)
				break;

			tmp = mt_cpufreq_get_cur_phy_freq_no_lock(cpu / 4) /
				100000;
			sched_get_percpu_load2(cpu, 1, &rel_load, &abs_load);
			cm_mgr_abs_load += abs_load * tmp;
			cm_mgr_rel_load += rel_load * tmp;

			cpu_load[cpu] = rel_load;
			loading_acc[cpu] += rel_load;
		}
		loading_cnt++;

		if ((cm_mgr_abs_load < light_load_cps) &&
				(vcore_dram_opp_cur == CM_MGR_EMI_OPP)) {
			cps_valid = 0;
			cm_mgr_update_met();
			spin_unlock(&cm_mgr_lock);
			return;
		}
#endif
		cps_valid = 1;

		now = ktime_get();

#ifdef ATF_SECURE_SMC
#ifdef USE_NEW_CPU_OPP
		ret = mt_secure_call(MTK_SIP_KERNEL_PMU_EVA, 0,
				prev_freq[0] / 1000,
				prev_freq[1] / 1000);
#else
#ifdef USE_AVG_PMU
		ret = mt_secure_call(MTK_SIP_KERNEL_PMU_EVA, 0,
				mt_cpufreq_get_cur_phy_freq_no_lock(0) / 1000,
				mt_cpufreq_get_cur_phy_freq_no_lock(1) / 1000);
#else
		ret = mt_secure_call(MTK_SIP_KERNEL_PMU_EVA, 0,
				mt_cpufreq_get_cur_freq(0) / 1000,
				mt_cpufreq_get_cur_freq(1) / 1000);
#endif
#endif /* USE_NEW_CPU_OPP */
#else
#ifdef USE_NEW_CPU_OPP
		ret = cm_mgr_check_stall_ratio(
				prev_freq[0] / 1000,
				prev_freq[1] / 1000);
#else
#ifdef USE_AVG_PMU
		ret = cm_mgr_check_stall_ratio(
				mt_cpufreq_get_cur_phy_freq_no_lock(0) / 1000,
				mt_cpufreq_get_cur_phy_freq_no_lock(1) / 1000);
#else
		ret = cm_mgr_check_stall_ratio(
				mt_cpufreq_get_cur_freq(0) / 1000,
				mt_cpufreq_get_cur_freq(1) / 1000);
#endif
#endif /* USE_NEW_CPU_OPP */
#endif
#ifdef CONFIG_MTK_QOS_SUPPORT
#if 0
		total_bw = dvfsrc_get_bw(QOS_TOTAL_AVE) / 512;
#else
		total_bw = dvfsrc_get_bw(QOS_TOTAL) / 512;
#endif
#else
		total_bw = 0;
#endif
		count_ack[0] = count_ack[1] = 0;

#ifdef ATF_SECURE_SMC
		ratio_max[0] = (ret & 0xff);
		ratio_max[1] = ((ret >> 8) & 0xff);
		count[0] = (ret >> 24) & 0xf;
		count[1] = (ret >> 28) & 0xf;
#endif

		if (total_bw_value)
			total_bw = total_bw_value;
		if (total_bw >= vcore_power_array_size(cm_mgr_get_idx()))
			total_bw = vcore_power_array_size(cm_mgr_get_idx()) - 1;

		if (update_v2f_table == 1) {
			update_v2f(1, 0);
			update_v2f_table++;
		}

		/* get max loading */
		max_load[0] = 0;
		max_load[1] = 0;
		for_each_possible_cpu(i) {
			int avg_load;

			if (i >= CM_MGR_CPU_COUNT)
				break;

			if (unlikely(loading_cnt == 0))
				break;
			avg_load = loading_acc[i] / loading_cnt;
			if (avg_load > max_load[i / 4])
				max_load[i / 4] = avg_load;
			loading_acc[i] = 0;
		}

		for (i = 0; i < CM_MGR_CPU_CLUSTER; i++) {
#ifndef ATF_SECURE_SMC
			count[i] = cm_mgr_get_cpu_count(i);
			ratio_max[i] = cm_mgr_get_max_stall_ratio(i);
#endif
			max_ratio_idx[i] = ratio_max[i] / 5;
			if (max_ratio_idx[i] > RATIO_COUNT)
				max_ratio_idx[i] = RATIO_COUNT;
#ifdef USE_NEW_CPU_OPP
			cpu_opp_cur[i] = prev_freq_idx[i];
#else
			cpu_opp_cur[i] = mt_cpufreq_get_cur_freq_idx(i);
#endif /* USE_NEW_CPU_OPP */
			v2f[i] = _v2f_all[cpu_opp_cur[i]][i];
			cpu_power_up_array[i] = cpu_power_up[i] = 0;
			cpu_power_down_array[i] = cpu_power_down[i] = 0;

			/* calc scaled ratio */
			ratio_scale[i] = (max_load[i] > 0) ?
				(ratio_max[i] * 100 / max_load[i]) :
				ratio_max[i];
			if (ratio_scale[i] > 100)
				ratio_scale[i] = 100;
		}
#ifdef DEBUG_CM_MGR
		print_hex_dump(KERN_INFO, "cpu_opp_cur: ", DUMP_PREFIX_NONE, 16,
				1, &cpu_opp_cur[0], sizeof(cpu_opp_cur), 0);
#endif /* DEBUG_CM_MGR */

		vcore_power_up = 0;
		vcore_power_down = 0;

		for (i = 0; i < CM_MGR_CPU_COUNT; i++) {
			ratio[i] = cm_mgr_get_stall_ratio(i);
			cpu_ratio_idx[i] = ratio[i] / 5;
			if (cpu_ratio_idx[i] > RATIO_COUNT)
				cpu_ratio_idx[i] = RATIO_COUNT;
		}
#ifdef DEBUG_CM_MGR
		print_hex_dump(KERN_INFO, "ratio: ", DUMP_PREFIX_NONE, 16,
				1, &ratio[0], sizeof(ratio), 0);
		print_hex_dump(KERN_INFO, "cpu_ratio_idx: ",
				DUMP_PREFIX_NONE, 16,
				1, &cpu_ratio_idx[0],
				sizeof(cpu_ratio_idx), 0);
#endif /* DEBUG_CM_MGR */

		level = CM_MGR_EMI_OPP - vcore_dram_opp_cur;
		if (vcore_dram_opp_cur != 0) {
			if (cm_mgr_check_up_status(level, cpu_ratio_idx) < 0)
				goto cm_mgr_opp_end;
		}
		if (vcore_dram_opp_cur != CM_MGR_EMI_OPP) {
			if (cm_mgr_check_down_status(level, cpu_ratio_idx) < 0)
				goto cm_mgr_opp_end;
		}

cm_mgr_opp_end:
		cm_mgr_update_met();

		if (cm_mgr_perf_timer_enable) {
			unsigned long expires;

			expires = jiffies + USE_TIMER_PERF_CHECK_TIME;
			mod_timer(&cm_mgr_perf_timer, expires);
		}

#ifdef USE_TIMER_CHECK
		if (cm_mgr_timer_enable) {
			if (vcore_dram_opp != CM_MGR_EMI_OPP) {
				unsigned long expires;

				expires = jiffies +
					USE_TIMER_CHECK_TIME;
				mod_timer(&cm_mgr_timer, expires);
			} else {
				del_timer(&cm_mgr_timer);
			}
		}
#endif /* USE_TIMER_CHECK */

		done = ktime_get();
		if (!ktime_after(done, now)) {
			/* pr_debug("ktime overflow!!\n"); */
			spin_unlock(&cm_mgr_lock);
			return;
		}
		result = ktime_to_us(ktime_sub(done, now));
		test_diff += result;
		cnt++;
		if (result > test_max)
			test_max = result;
		if (result >= 300)
			time_cnt_data[4]++;
		else if (result >= 200 && result < 300)
			time_cnt_data[3]++;
		else if (result >= 100 && result < 200)
			time_cnt_data[2]++;
		else if (result >= 50 && result < 100)
			time_cnt_data[1]++;
		else
			time_cnt_data[0]++;
		spin_unlock(&cm_mgr_lock);
	}
}

void check_cm_mgr_status(unsigned int cluster, unsigned int freq)
{
	int freq_idx = 0;
	struct mt_cpu_dvfs *p;

	p = id_to_cpu_dvfs(cluster);
	freq_idx = _search_available_freq_idx(p, freq, 0);

	if (freq_idx == prev_freq_idx[cluster])
		return;

	prev_freq_idx[cluster] = freq_idx;
	prev_freq[cluster] = freq;

	check_cm_mgr_status_internal();
}

static int dbg_cm_mgr_status_proc_show(struct seq_file *m, void *v)
{
#ifdef CONFIG_ARM64
	seq_printf(m, "diff/cnt/max/avg = %llu/%llu/%d/%llu\n",
			test_diff, cnt, test_max, test_diff / cnt);
#else
	seq_printf(m, "diff/cnt/max/avg = %llu/%llu/%d/%u\n",
			test_diff, cnt, test_max, do_div(test_diff, cnt));
#endif
	seq_printf(m, "< 50us    = %d\n", time_cnt_data[0]);
	seq_printf(m, "50~99us   = %d\n", time_cnt_data[1]);
	seq_printf(m, "100~199us = %d\n", time_cnt_data[2]);
	seq_printf(m, "200~299us = %d\n", time_cnt_data[3]);
	seq_printf(m, ">= 300us  = %d\n", time_cnt_data[4]);
	time_cnt_data[0] = 0;
	time_cnt_data[1] = 0;
	time_cnt_data[2] = 0;
	time_cnt_data[3] = 0;
	time_cnt_data[4] = 0;
	test_diff = 0;
	cnt = 0;
	test_max = 0;

	return 0;
}

PROC_FOPS_RO(dbg_cm_mgr_status);

static int dbg_cm_mgr_proc_show(struct seq_file *m, void *v)
{
	int i, j;
	int count;

	seq_printf(m, "cm_mgr_opp_enable %d\n", cm_mgr_opp_enable);
	seq_printf(m, "cm_mgr_enable %d\n", cm_mgr_enable);
#ifdef USE_TIMER_CHECK
	seq_printf(m, "cm_mgr_timer_enable %d\n", cm_mgr_timer_enable);
#endif /* USE_TIMER_CHECK */
	seq_printf(m, "cm_mgr_ratio_timer_enable %d\n",
			cm_mgr_ratio_timer_enable);
	seq_printf(m, "cm_mgr_perf_timer_enable %d\n",
			cm_mgr_perf_timer_enable);
	seq_printf(m, "cm_mgr_perf_force_enable %d\n",
			cm_mgr_perf_force_enable);
	seq_printf(m, "cm_mgr_disable_fb %d\n", cm_mgr_disable_fb);
	seq_printf(m, "light_load_cps %d\n", light_load_cps);
	seq_printf(m, "total_bw_value %d\n", total_bw_value);
	seq_printf(m, "cm_mgr_loop_count %d\n", cm_mgr_loop_count);

	seq_puts(m, "cpu_power_ratio_up");
	for (i = 0; i < CM_MGR_EMI_OPP; i++)
		seq_printf(m, " %d", cpu_power_ratio_up[i]);
	seq_puts(m, "\n");

	seq_puts(m, "cpu_power_ratio_down");
	for (i = 0; i < CM_MGR_EMI_OPP; i++)
		seq_printf(m, " %d", cpu_power_ratio_down[i]);
	seq_puts(m, "\n");

	seq_puts(m, "vcore_power_ratio_up");
	for (i = 0; i < CM_MGR_EMI_OPP; i++)
		seq_printf(m, " %d", vcore_power_ratio_up[i]);
	seq_puts(m, "\n");

	seq_puts(m, "vcore_power_ratio_down");
	for (i = 0; i < CM_MGR_EMI_OPP; i++)
		seq_printf(m, " %d", vcore_power_ratio_down[i]);
	seq_puts(m, "\n");

	seq_puts(m, "debounce_times_up_adb");
	for (i = 0; i < CM_MGR_EMI_OPP; i++)
		seq_printf(m, " %d", debounce_times_up_adb[i]);
	seq_puts(m, "\n");

	seq_puts(m, "debounce_times_down_adb");
	for (i = 0; i < CM_MGR_EMI_OPP; i++)
		seq_printf(m, " %d", debounce_times_down_adb[i]);
	seq_puts(m, "\n");

	seq_printf(m, "debounce_times_reset_adb %d\n",
			debounce_times_reset_adb);
	seq_printf(m, "debounce_times_perf_down %d\n",
			debounce_times_perf_down);
	seq_printf(m, "update_v2f_table %d\n", update_v2f_table);
	seq_printf(m, "update %d\n", update);

	for (count = 0; count < CM_MGR_MAX; count++) {
		seq_printf(m, "vcore_power_gain_%d\n", count);
		for (i = 0; i < vcore_power_array_size(count); i++) {
			seq_printf(m, "%.2d -", i);
			for (j = 0; j < VCORE_ARRAY_SIZE; j++)
				seq_printf(m, " %d",
						vcore_power_gain(
						vcore_power_gain_ptr(
							count), i, j));
			seq_puts(m, "\n");
		}
	}

#ifdef PER_CPU_STALL_RATIO
#ifndef ATF_SECURE_SMC
	for (count = 0; count < CM_MGR_MAX; count++) {
		cpu_power_gain_ptr(0, count, 0);

		seq_printf(m, "cpu_power_gain_UpLow%d\n", count);
		for (i = 0; i < 20; i++) {
			seq_printf(m, "%.2d -", i);
			for (j = 0; j < CM_MGR_CPU_ARRAY_SIZE; j++)
				seq_printf(m, " %d",
						cpu_power_gain(
							cpu_power_gain_up,
							i, j));
			seq_puts(m, "\n");
		}

		seq_printf(m, "cpu_power_gain_DownLow%d\n", count);
		for (i = 0; i < 20; i++) {
			seq_printf(m, "%.2d -", i);
			for (j = 0; j < CM_MGR_CPU_ARRAY_SIZE; j++)
				seq_printf(m, " %d",
						cpu_power_gain(
							cpu_power_gain_down,
							i, j));
			seq_puts(m, "\n");
		}

		cpu_power_gain_ptr(0, CM_MGR_LOWER_OPP, 0);

		seq_printf(m, "cpu_power_gain_UpHigh%d\n", count);
		for (i = 0; i < 20; i++) {
			seq_printf(m, "%.2d -", i);
			for (j = 0; j < CM_MGR_CPU_ARRAY_SIZE; j++)
				seq_printf(m, " %d",
						cpu_power_gain(
							cpu_power_gain_up,
							i, j));
			seq_puts(m, "\n");
		}

		seq_printf(m, "cpu_power_gain_DownHigh%d\n", count);
		for (i = 0; i < 20; i++) {
			seq_printf(m, "%.2d -", i);
			for (j = 0; j < CM_MGR_CPU_ARRAY_SIZE; j++)
				seq_printf(m, " %d",
						cpu_power_gain(
							cpu_power_gain_down,
							i, j));
			seq_puts(m, "\n");
		}
	}
#endif
#endif

	seq_puts(m, "_v2f_all\n");
	for (i = 0; i < 16; i++) {
		seq_printf(m, "%.2d -", i);
		for (j = 0; j < CM_MGR_CPU_CLUSTER; j++)
			seq_printf(m, " %d", _v2f_all[i][j]);
		seq_puts(m, "\n");
	}

	seq_puts(m, "\n");

	return 0;
}

#define CPU_FW_FILE "cpu_data.bin"
#include <linux/firmware.h>
static struct device cm_mgr_device = {
	.init_name = "cm_mgr_device ",
};

static void cm_mgr_update_fw(void)
{
	int j = 0;
	const struct firmware *fw;
	int err;
	int copy_size = 0;
	int offset = 0;
	int count;

	do {
		j++;
		pr_debug("try to request_firmware() %s - %d\n",
				CPU_FW_FILE, j);
		err = request_firmware(&fw, CPU_FW_FILE, &cm_mgr_device);
		if (err)
			pr_info("Failed to load %s, err = %d.\n",
					CPU_FW_FILE, err);
	} while (err == -EAGAIN && j < 5);
	if (err)
		pr_info("Failed to load %s, err = %d.\n",
				CPU_FW_FILE, err);

	if (!err) {
		pr_info("request_firmware() %s, size 0x%x\n",
				CPU_FW_FILE, (int)fw->size);
		update++;

		for (count = 0; count < CM_MGR_MAX; count++) {
			copy_size = vcore_power_array_size(count) *
				VCORE_ARRAY_SIZE * sizeof(unsigned int);
			pr_info("offset 0x%x, copy_size 0x%x\n",
					offset, copy_size);
			if (fw->size < (copy_size + offset)) {
				pr_info("vcore_power_gain_%d 0x%x, 0x%x",
						count, (int)fw->size,
						copy_size + offset);
				goto out_fw;
			}
			memcpy(vcore_power_gain_ptr(count),
					fw->data, copy_size);
		}

#ifdef PER_CPU_STALL_RATIO
#ifndef ATF_SECURE_SMC
		for (count = 0; count < CM_MGR_MAX; count++) {
			offset += copy_size;
			copy_size = sizeof(cpu_power_gain_UpLow0);
			pr_info("offset 0x%x, copy_size 0x%x\n",
					offset, copy_size);
			if (fw->size < (copy_size + offset)) {
				pr_info("cpu_power_gain_UpLow1 0x%x, 0x%x",
						(int)fw->size,
						copy_size + offset);
				goto out_fw;
			}
			memcpy(&cpu_power_gain_UpLow1,
					fw->data + offset, copy_size);

			offset += copy_size;
			copy_size = sizeof(cpu_power_gain_DownLow0);
			pr_info("offset 0x%x, copy_size 0x%x\n",
					offset, copy_size);
			if (fw->size < (copy_size + offset)) {
				pr_info("cpu_power_gain_DownLow1 0x%x, 0x%x",
						(int)fw->size,
						copy_size + offset);
				goto out_fw;
			}
			memcpy(&cpu_power_gain_DownLow1,
					fw->data + offset, copy_size);

			offset += copy_size;
			copy_size = sizeof(cpu_power_gain_UpHigh0);
			pr_info("offset 0x%x, copy_size 0x%x\n",
					offset, copy_size);
			if (fw->size < (copy_size + offset)) {
				pr_info("cpu_power_gain_UpHigh1 0x%x, 0x%x",
						(int)fw->size,
						copy_size + offset);
				goto out_fw;
			}
			memcpy(&cpu_power_gain_UpHigh1,
					fw->data + offset, copy_size);

			offset += copy_size;
			copy_size = sizeof(cpu_power_gain_DownHigh0);
			pr_info("offset 0x%x, copy_size 0x%x\n",
					offset, copy_size);
			if (fw->size < (copy_size + offset)) {
				pr_info("cpu_power_gain_DownHigh1 0x%x, 0x%x",
						(int)fw->size,
						copy_size + offset);
				goto out_fw;
			}
			memcpy(&cpu_power_gain_DownHigh1,
					fw->data + offset, copy_size);
		}
#endif
#endif

		offset += copy_size;
		copy_size = sizeof(_v2f_all);
		pr_info("offset 0x%x, copy_size 0x%x\n", offset, copy_size);
		if (fw->size < (copy_size + offset)) {
			pr_info("_v2f_all 0x%x, 0x%x",
					(int)fw->size, copy_size + offset);
			goto out_fw;
		}
		memcpy(&_v2f_all, fw->data + offset, copy_size);

		release_firmware(fw);
		fw = NULL;
	}
out_fw:
	if (fw)
		release_firmware(fw);
}

static ssize_t dbg_cm_mgr_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{

	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);
	char cmd[64];
	u32 val_1;
	u32 val_2;

	if (!buf)
		return -ENOMEM;

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	ret = sscanf(buf, "%63s %d %d", cmd, &val_1, &val_2);
	if (ret < 1) {
		ret = -EPERM;
		goto out;
	}

	if (!strcmp(cmd, "cm_mgr_opp_enable")) {
		cm_mgr_opp_enable = val_1;
		if (!cm_mgr_opp_enable)
			dvfsrc_set_power_model_ddr_request(0);
	} else if (!strcmp(cmd, "cm_mgr_enable")) {
		cm_mgr_enable = val_1;
		if (!cm_mgr_enable)
			dvfsrc_set_power_model_ddr_request(0);
#ifdef USE_TIMER_CHECK
	} else if (!strcmp(cmd, "cm_mgr_timer_enable")) {
		cm_mgr_timer_enable = val_1;
#endif /* USE_TIMER_CHECK */
	} else if (!strcmp(cmd, "cm_mgr_ratio_timer_enable")) {
		cm_mgr_ratio_timer_enable = val_1;
		cm_mgr_ratio_timer_en(val_1);
	} else if (!strcmp(cmd, "cm_mgr_perf_timer_enable")) {
		cm_mgr_perf_timer_enable = val_1;
		cm_mgr_perf_set_status(val_1);
	} else if (!strcmp(cmd, "cm_mgr_perf_force_enable")) {
		cm_mgr_perf_force_enable = val_1;
		cm_mgr_perf_set_force_status(val_1);
	} else if (!strcmp(cmd, "cm_mgr_disable_fb")) {
		cm_mgr_disable_fb = val_1;
		if (cm_mgr_disable_fb == 1 && cm_mgr_blank_status == 1)
			dvfsrc_set_power_model_ddr_request(0);
	} else if (!strcmp(cmd, "light_load_cps")) {
		light_load_cps = val_1;
	} else if (!strcmp(cmd, "total_bw_value")) {
		total_bw_value = val_1;
	} else if (!strcmp(cmd, "cm_mgr_loop_count")) {
		cm_mgr_loop_count = val_1;
	} else if (!strcmp(cmd, "cpu_power_ratio_up")) {
		if (ret == 3 && val_1 >= 0 && val_1 < CM_MGR_EMI_OPP)
			cpu_power_ratio_up[val_1] = val_2;
	} else if (!strcmp(cmd, "cpu_power_ratio_down")) {
		if (ret == 3 && val_1 >= 0 && val_1 < CM_MGR_EMI_OPP)
			cpu_power_ratio_down[val_1] = val_2;
	} else if (!strcmp(cmd, "vcore_power_ratio_up")) {
		if (ret == 3 && val_1 >= 0 && val_1 < CM_MGR_EMI_OPP)
			vcore_power_ratio_up[val_1] = val_2;
	} else if (!strcmp(cmd, "vcore_power_ratio_down")) {
		if (ret == 3 && val_1 >= 0 && val_1 < CM_MGR_EMI_OPP)
			vcore_power_ratio_down[val_1] = val_2;
	} else if (!strcmp(cmd, "debounce_times_up_adb")) {
		if (ret == 3 && val_1 >= 0 && val_1 < CM_MGR_EMI_OPP)
			debounce_times_up_adb[val_1] = val_2;
	} else if (!strcmp(cmd, "debounce_times_down_adb")) {
		if (ret == 3 && val_1 >= 0 && val_1 < CM_MGR_EMI_OPP)
			debounce_times_down_adb[val_1] = val_2;
	} else if (!strcmp(cmd, "debounce_times_reset_adb")) {
		debounce_times_reset_adb = val_1;
	} else if (!strcmp(cmd, "debounce_times_perf_down")) {
		debounce_times_perf_down = val_1;
	} else if (!strcmp(cmd, "update_v2f_table")) {
		update_v2f_table = !!val_1;
	} else if (!strcmp(cmd, "update")) {
		cm_mgr_update_fw();
	} else if (!strcmp(cmd, "1")) {
		/* cm_mgr_perf_force_enable */
		cm_mgr_perf_force_enable = 1;
		cm_mgr_perf_set_force_status(cm_mgr_perf_force_enable);
	} else if (!strcmp(cmd, "0")) {
		/* cm_mgr_perf_force_enable */
		cm_mgr_perf_force_enable = 0;
		cm_mgr_perf_set_force_status(cm_mgr_perf_force_enable);
	}

out:
	free_page((unsigned long)buf);

	return count;
}
MODULE_FIRMWARE(CPU_FW_FILE);

PROC_FOPS_RW(dbg_cm_mgr);

static int create_cm_mgr_debug_fs(void)
{
	int i;
	struct proc_dir_entry *dir = NULL;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
		void *data;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(dbg_cm_mgr_status),
		PROC_ENTRY(dbg_cm_mgr),
	};

	/* create /proc/cm_mgr */
	dir = proc_mkdir("cm_mgr", NULL);
	if (!dir) {
		pr_info("fail to create /proc/cm_mgr @ %s()\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create_data
		    (entries[i].name, 0664,
		     dir, entries[i].fops, entries[i].data))
			pr_info("%s(), create /proc/cm_mgr/%s failed\n",
					__func__,
				    entries[i].name);
	}

	return 0;
}

int __weak cm_mgr_platform_init(void)
{

	return 0;
}

int __init cm_mgr_module_init(void)
{
	int r;

	r = create_cm_mgr_debug_fs();
	if (r) {
		pr_info("FAILED TO CREATE DEBUG FILESYSTEM (%d)\n", r);
		return r;
	}

	r = device_register(&cm_mgr_device);
	if (r) {
		pr_info("FAILED TO CREATE DEVICE(%d)\n", r);
		return r;
	}

	/* SW Governor Report */
	spin_lock_init(&cm_mgr_lock);

	r = cm_mgr_platform_init();
	if (r) {
		pr_info("FAILED TO INIT PLATFORM(%d)\n", r);
		return r;
	}

	vcore_power_gain = vcore_power_gain_ptr(cm_mgr_get_idx());

	init_timer_deferrable(&cm_mgr_perf_timer);
	cm_mgr_perf_timer.function = cm_mgr_perf_timer_fn;
	cm_mgr_perf_timer.data = 0;

#ifdef USE_TIMER_CHECK
	init_timer_deferrable(&cm_mgr_timer);
	cm_mgr_timer.function = cm_mgr_timer_fn;
	cm_mgr_timer.data = 0;
#endif /* USE_TIMER_CHECK */

#ifdef ATF_SECURE_SMC
	pr_info("ATF_SECURE_SMC\n");
#else
	pr_info("!ATF_SECURE_SMC\n");
#endif

	return 0;
}

late_initcall(cm_mgr_module_init);

MODULE_DESCRIPTION("CM Manager Driver v0.1");
