// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/* system includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/slab.h>
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
#include <linux/types.h>
#include <linux/suspend.h>
#include <linux/topology.h>
#include <linux/math64.h>
#include <mt-plat/sync_write.h>

#include <linux/of.h>
#include <linux/of_address.h>
#ifdef CONFIG_MTK_DVFSRC
#include <linux/soc/mediatek/mtk_dvfsrc.h>
#include <dvfsrc-exp.h>
#endif /* CONFIG_MTK_DVFSRC */
#include "mtk_cm_mgr_mt6765.h"
#include "mtk_cm_mgr_common.h"
#include "mtk_cm_mgr_data_mt6765.h"

#include <linux/fb.h>
#include <linux/notifier.h>

#include <linux/soc/mediatek/mtk-pm-qos.h>
#include <mtk_qos_sram.h>
#ifdef CONFIG_MTK_CPU_FREQ
#include <mtk_cpufreq_platform.h>
#include <mtk_cpufreq_common_api.h>
#endif /* CONFIG_MTK_CPU_FREQ */

static struct delayed_work cm_mgr_work;
static int cm_mgr_work_ready;
static struct mtk_pm_qos_request ddr_opp_req_by_cpu_opp;
static int cm_mgr_cpu_to_dram_opp;

spinlock_t cm_mgr_lock;

static unsigned int prev_freq_idx[CM_MGR_CPU_CLUSTER];
static unsigned int prev_freq[CM_MGR_CPU_CLUSTER];

static unsigned int cpu_power_up_array[CM_MGR_CPU_CLUSTER];
static unsigned int cpu_power_down_array[CM_MGR_CPU_CLUSTER];
static unsigned int cpu_power_up[CM_MGR_CPU_CLUSTER];
static unsigned int cpu_power_down[CM_MGR_CPU_CLUSTER];
static unsigned int v2f[CM_MGR_CPU_CLUSTER];
static int vcore_power_up;
static int vcore_power_down;
static int cpu_opp_cur[CM_MGR_CPU_CLUSTER];
static int ratio_max[CM_MGR_CPU_CLUSTER];
static int ratio[CM_MGR_CPU_COUNT];
static int count[CM_MGR_CPU_CLUSTER];
static int count_ack[CM_MGR_CPU_CLUSTER];
static int vcore_dram_opp;
static int vcore_dram_opp_cur;
static int cm_mgr_abs_load;
static int cm_mgr_rel_load;
static int total_bw;
static int cps_valid;
static unsigned int debounce_times_up;
static unsigned int debounce_times_down;
static int ratio_scale[CM_MGR_CPU_CLUSTER];
static int max_load[CM_MGR_CPU_CLUSTER];
static int cpu_load[NR_CPUS];
static int loading_acc[NR_CPUS];
static int loading_cnt;

struct cm_mgr_met_data {
	unsigned int cm_mgr_power[14];
	unsigned int cm_mgr_count[4];
	unsigned int cm_mgr_opp[6];
	unsigned int cm_mgr_loading[12];
	unsigned int cm_mgr_ratio[12];
	unsigned int cm_mgr_bw;
	unsigned int cm_mgr_valid;
};

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
	met_data.cm_mgr_power[10] = (unsigned int)vcore_power_up;
	met_data.cm_mgr_power[11] = (unsigned int)vcore_power_down;
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

#include <linux/cpu_pm.h>
static unsigned int cm_mgr_idle_mask;

spinlock_t cm_mgr_cpu_mask_lock;

#define diff_value_overflow(diff, a, b) do { \
	if ((a) >= (b)) \
		diff = (a) - (b); \
	else \
		diff = 0xffffffff - (b) + (a); \
} while (0) \

#define CM_MGR_MAX(a, b) (((a) > (b)) ? (a) : (b))

/* #define USE_DEBUG_LOG */

struct stall_s {
	unsigned int clustor[CM_MGR_CPU_CLUSTER];
	unsigned long long stall_val[CM_MGR_CPU_COUNT];
	unsigned long long stall_val_diff[CM_MGR_CPU_COUNT];
	unsigned long long time_ns[CM_MGR_CPU_COUNT];
	unsigned long long time_ns_diff[CM_MGR_CPU_COUNT];
	unsigned long long ratio[CM_MGR_CPU_COUNT];
	unsigned int ratio_max[CM_MGR_CPU_COUNT];
	unsigned int cpu;
	unsigned int cpu_count[CM_MGR_CPU_CLUSTER];
};

static struct stall_s stall_all;
static struct stall_s *pstall_all = &stall_all;
static int cm_mgr_idx = -1;

#ifdef USE_DEBUG_LOG
static void debug_stall(int cpu)
{
	pr_debug("%s: cpu number %d ################\n", __func__,
			cpu);
	pr_debug("%s: clustor[%d] 0x%08x\n", __func__,
			cpu / CM_MGR_CPU_LIMIT,
			pstall_all->clustor[cpu / CM_MGR_CPU_LIMIT]);
	pr_debug("%s: stall_val[%d] 0x%016llx\n", __func__,
			cpu, pstall_all->stall_val[cpu]);
	pr_debug("%s: stall_val_diff[%d] 0x%016llx\n", __func__,
			cpu, pstall_all->stall_val_diff[cpu]);
	pr_debug("%s: time_ns[%d] 0x%016llx\n", __func__,
			cpu, pstall_all->time_ns[cpu]);
	pr_debug("%s: time_ns_diff[%d] 0x%016llx\n", __func__,
			cpu, pstall_all->time_ns_diff[cpu]);
	pr_debug("%s: ratio[%d] 0x%016llx\n", __func__,
			cpu, pstall_all->ratio[cpu]);
	pr_debug("%s: ratio_max[%d] 0x%08x\n", __func__,
			cpu / CM_MGR_CPU_LIMIT,
			pstall_all->ratio_max[cpu / CM_MGR_CPU_LIMIT]);
	pr_debug("%s: cpu 0x%08x\n", __func__, pstall_all->cpu);
	pr_debug("%s: cpu_count[%d] 0x%08x\n", __func__,
			cpu / CM_MGR_CPU_LIMIT,
			pstall_all->cpu_count[cpu / CM_MGR_CPU_LIMIT]);
}

static void debug_stall_all(void)
{
	int i;

	for (i = 0; i < CM_MGR_CPU_COUNT; i++)
		debug_stall(i);
}
#endif /* USE_DEBUG_LOG */

static int cm_mgr_check_dram_type(void)
{
#ifdef CONFIG_MTK_DRAMC_LEGACY
	int ddr_type = get_ddr_type();
	int ddr_hz = dram_steps_freq(0);

	if (ddr_type == TYPE_LPDDR4X || ddr_type == TYPE_LPDDR4)
		cm_mgr_idx = CM_MGR_LP4X_2CH_3200;
	else if (ddr_type == TYPE_LPDDR3)
		cm_mgr_idx = CM_MGR_LP3_1CH_1866;
	pr_info("#@# %s(%d) ddr_type 0x%x, ddr_hz %d, cm_mgr_idx 0x%x\n",
			__func__, __LINE__, ddr_type, ddr_hz, cm_mgr_idx);
#else
	cm_mgr_idx = 0;
	pr_info("#@# %s(%d) NO CONFIG_MTK_DRAMC_LEGACY !!! set cm_mgr_idx to 0x%x\n",
			__func__, __LINE__, cm_mgr_idx);
#endif /* CONFIG_MTK_DRAMC_LEGACY */

	return cm_mgr_idx;
};

static int cm_mgr_get_idx(void)
{
	if (cm_mgr_idx < 0)
		return cm_mgr_check_dram_type();
	else
		return cm_mgr_idx;
};

static int cm_mgr_get_stall_ratio(int cpu)
{
	return pstall_all->ratio[cpu];
}

static int cm_mgr_get_max_stall_ratio(int cluster)
{
	return pstall_all->ratio_max[cluster];
}

static int cm_mgr_get_cpu_count(int cluster)
{
	return pstall_all->cpu_count[cluster];
}

static ktime_t cm_mgr_init_time;
static int cm_mgr_init_flag;

static unsigned int cm_mgr_read_stall(int cpu)
{
	unsigned int val = 0;
	unsigned long spinlock_save_flags;

	if (cm_mgr_init_flag) {
		if (ktime_ms_delta(ktime_get(), cm_mgr_init_time) <
				CM_MGR_INIT_DELAY_MS)
			return val;
		cm_mgr_init_flag = 0;
	}

	if (!spin_trylock_irqsave(&cm_mgr_cpu_mask_lock, spinlock_save_flags))
		return val;

	if (cpu < CM_MGR_CPU_LIMIT) {
		if (cm_mgr_idle_mask & CLUSTER0_MASK)
			val = cm_mgr_read(MP0_CPU0_STALL_COUNTER + 4 * cpu);
	} else {
		if (cm_mgr_idle_mask & CLUSTER1_MASK)
			val = cm_mgr_read(MP1_CPU0_STALL_COUNTER +
					4 * (cpu - CM_MGR_CPU_LIMIT));
	}
	spin_unlock_irqrestore(&cm_mgr_cpu_mask_lock, spinlock_save_flags);

	return val;
}

static int cm_mgr_check_stall_ratio(int mp0, int mp1)
{
	unsigned int i;
	unsigned int clustor;
	unsigned int stall_val_new;
	unsigned long long time_ns_new;

	pstall_all->clustor[0] = mp0;
	pstall_all->clustor[1] = mp1;
	pstall_all->cpu = 0;
	for (i = 0; i < CM_MGR_CPU_CLUSTER; i++) {
		pstall_all->ratio_max[i] = 0;
		pstall_all->cpu_count[i] = 0;
	}

	for (i = 0; i < CM_MGR_CPU_COUNT; i++) {
		pstall_all->ratio[i] = 0;
		clustor = i / CM_MGR_CPU_LIMIT;

		if (pstall_all->clustor[clustor] == 0)
			continue;

		stall_val_new = cm_mgr_read_stall(i);

		if (stall_val_new == 0 || stall_val_new == 0xdeadbeef) {
#ifdef USE_DEBUG_LOG
			pr_debug("%s: WARN!!! stall_val_new is 0x%08x\n",
					__func__, stall_val_new);
			debug_stall(i);
#endif /* USE_DEBUG_LOG */
			continue;
		}

		time_ns_new = sched_clock();
		pstall_all->time_ns_diff[i] =
			time_ns_new - pstall_all->time_ns[i];
		pstall_all->time_ns[i] = time_ns_new;
		if (pstall_all->time_ns_diff[i] == 0)
			continue;

		diff_value_overflow(pstall_all->stall_val_diff[i],
				stall_val_new, pstall_all->stall_val[i]);
		pstall_all->stall_val[i] = stall_val_new;

		if (pstall_all->stall_val_diff[i] == 0) {
#ifdef USE_DEBUG_LOG
			pr_debug("%s: WARN!!! cpu:%d diff == 0\n", __func__, i);
			debug_stall(i);
#endif /* USE_DEBUG_LOG */
			continue;
		}

#ifdef CONFIG_ARM64
		pstall_all->ratio[i] = pstall_all->stall_val_diff[i] * 100000 /
			pstall_all->time_ns_diff[i] /
			pstall_all->clustor[clustor];
#else
		pstall_all->ratio[i] = pstall_all->stall_val_diff[i] * 100000;
		do_div(pstall_all->ratio[i], pstall_all->time_ns_diff[i]);
		do_div(pstall_all->ratio[i], pstall_all->clustor[clustor]);
#endif /* CONFIG_ARM64 */
		if (pstall_all->ratio[i] > 100) {
#ifdef USE_DEBUG_LOG
			pr_debug("%s: WARN!!! cpu:%d ratio > 100\n",
					__func__, i);
			debug_stall(i);
#endif /* USE_DEBUG_LOG */
			pstall_all->ratio[i] = 100;
			/* continue; */
		}

		pstall_all->cpu |= (1 << i);
		pstall_all->cpu_count[clustor]++;
		pstall_all->ratio_max[clustor] =
			CM_MGR_MAX(pstall_all->ratio[i],
					pstall_all->ratio_max[clustor]);
#ifdef USE_DEBUG_LOG
		debug_stall(i);
#endif /* USE_DEBUG_LOG */
	}

#ifdef USE_DEBUG_LOG
	debug_stall_all();
#endif /* USE_DEBUG_LOG */
	return 0;
}

static void init_cpu_stall_counter(int cluster)
{
	unsigned int val;

	if (!timekeeping_suspended) {
		cm_mgr_init_time = ktime_get();
		cm_mgr_init_flag = 1;
	}

	if (cluster == 0) {
		val = 0x11000;
		cm_mgr_write(MP0_CPU_STALL_INFO, val);

		/* please check CM_MGR_INIT_DELAY_MS value */
		val = RG_FMETER_EN;
		val |= RG_MP0_AVG_STALL_PERIOD_1MS;
		val |= RG_CPU0_AVG_STALL_RATIO_EN |
			RG_CPU0_STALL_COUNTER_EN |
			RG_CPU0_NON_WFX_COUNTER_EN;
		cm_mgr_write(MP0_CPU0_AVG_STALL_RATIO_CTRL, val);

		val = RG_CPU0_AVG_STALL_RATIO_EN |
			RG_CPU0_STALL_COUNTER_EN |
			RG_CPU0_NON_WFX_COUNTER_EN;
		cm_mgr_write(MP0_CPU1_AVG_STALL_RATIO_CTRL, val);

		val = RG_CPU0_AVG_STALL_RATIO_EN |
			RG_CPU0_STALL_COUNTER_EN |
			RG_CPU0_NON_WFX_COUNTER_EN;
		cm_mgr_write(MP0_CPU2_AVG_STALL_RATIO_CTRL, val);

		val = RG_CPU0_AVG_STALL_RATIO_EN |
			RG_CPU0_STALL_COUNTER_EN |
			RG_CPU0_NON_WFX_COUNTER_EN;
		cm_mgr_write(MP0_CPU3_AVG_STALL_RATIO_CTRL, val);
	} else {
		val = 0x11000;
		cm_mgr_write(MP1_CPU_STALL_INFO, val);

		/* please check CM_MGR_INIT_DELAY_MS value */
		val = RG_FMETER_EN;
		val |= RG_MP0_AVG_STALL_PERIOD_1MS;
		val |= RG_CPU0_AVG_STALL_RATIO_EN |
			RG_CPU0_STALL_COUNTER_EN |
			RG_CPU0_NON_WFX_COUNTER_EN;
		cm_mgr_write(MP1_CPU0_AVG_STALL_RATIO_CTRL, val);

		val = RG_CPU0_AVG_STALL_RATIO_EN |
			RG_CPU0_STALL_COUNTER_EN |
			RG_CPU0_NON_WFX_COUNTER_EN;
		cm_mgr_write(MP1_CPU1_AVG_STALL_RATIO_CTRL, val);

		val = RG_CPU0_AVG_STALL_RATIO_EN |
			RG_CPU0_STALL_COUNTER_EN |
			RG_CPU0_NON_WFX_COUNTER_EN;
		cm_mgr_write(MP1_CPU2_AVG_STALL_RATIO_CTRL, val);

		val = RG_CPU0_AVG_STALL_RATIO_EN |
			RG_CPU0_STALL_COUNTER_EN |
			RG_CPU0_NON_WFX_COUNTER_EN;
		cm_mgr_write(MP1_CPU3_AVG_STALL_RATIO_CTRL, val);
	}
}

static int cm_mgr_cpuhp_online(unsigned int cpu)
{
	unsigned long spinlock_save_flags;

	spin_lock_irqsave(&cm_mgr_cpu_mask_lock, spinlock_save_flags);

	if (((cm_mgr_idle_mask & CLUSTER0_MASK) == 0x0) &&
			(cpu < CM_MGR_CPU_LIMIT))
		init_cpu_stall_counter(0);
	else if (((cm_mgr_idle_mask & CLUSTER1_MASK) == 0x0) &&
			(cpu >= CM_MGR_CPU_LIMIT))
		init_cpu_stall_counter(1);
	cm_mgr_idle_mask |= (1 << cpu);

	spin_unlock_irqrestore(&cm_mgr_cpu_mask_lock, spinlock_save_flags);

	return 0;
}

static int cm_mgr_cpuhp_offline(unsigned int cpu)
{
	unsigned long spinlock_save_flags;

	spin_lock_irqsave(&cm_mgr_cpu_mask_lock, spinlock_save_flags);

	cm_mgr_idle_mask &= ~(1 << cpu);

	spin_unlock_irqrestore(&cm_mgr_cpu_mask_lock, spinlock_save_flags);

	return 0;
}

#ifdef CONFIG_CPU_PM
static int cm_mgr_sched_pm_notifier(struct notifier_block *self,
			       unsigned long cmd, void *v)
{
	unsigned int cpu = smp_processor_id();

	if (cmd == CPU_PM_EXIT)
		cm_mgr_cpuhp_online(cpu);
	else if (cmd == CPU_PM_ENTER)
		cm_mgr_cpuhp_offline(cpu);

	return NOTIFY_OK;
}

static struct notifier_block cm_mgr_sched_pm_notifier_block = {
	.notifier_call = cm_mgr_sched_pm_notifier,
};

static void cm_mgr_sched_pm_init(void)
{
	cpu_pm_register_notifier(&cm_mgr_sched_pm_notifier_block);
}

#else
static inline void cm_mgr_sched_pm_init(void) { }
#endif /* CONFIG_CPU_PM */

static void cm_mgr_set_dram_level(int level)
{
	int dram_level;

	if (cm_mgr_disable_fb == 1 && cm_mgr_blank_status == 1 && level != 0)
		dram_level = 0;
	else
		dram_level = level;
#ifdef CONFIG_MTK_DVFSRC
	dvfsrc_set_power_model_ddr_request(dram_level);
#endif /* CONFIG_MTK_DVFSRC */
}

static int cm_mgr_get_dram_opp(void)
{
	int dram_opp_cur;

#ifdef CONFIG_MTK_DVFSRC
	dram_opp_cur = mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_DRAM_OPP);
#else
	dram_opp_cur = 0;
#endif /* CONFIG_MTK_DVFSRC */
	if (dram_opp_cur < 0 || dram_opp_cur > CM_MGR_EMI_OPP)
		dram_opp_cur = 0;

	return dram_opp_cur;
}

static int cm_mgr_get_bw(void)
{
#ifdef CONFIG_MTK_QOS_FRAMEWORK
	return qos_sram_read(QOS_TOTAL_BW);
#else
	return 0;
#endif /* CONFIG_MTK_QOS_FRAMEWORK */
}

static int cm_mgr_check_bw_status(void)
{
	if (cm_mgr_get_bw() > CM_MGR_BW_VALUE)
		return 1;
	else
		return 0;
}

static struct mtk_pm_qos_request ddr_opp_req;
void cm_mgr_perf_platform_set_status(int enable)
{
	if (enable) {
		debounce_times_perf_down_local = 0;

		if (cm_mgr_perf_enable == 0)
			return;

		cpu_power_ratio_up[0] = 500;
		cpu_power_ratio_up[1] = 500;
		debounce_times_up_adb[1] = 0;
	} else {
		if (++debounce_times_perf_down_local < debounce_times_perf_down)
			return;

		cpu_power_ratio_up[0] = 100;
		cpu_power_ratio_up[1] = 100;
		debounce_times_up_adb[1] = 3;

		debounce_times_perf_down_local = 0;
	}
}
EXPORT_SYMBOL_GPL(cm_mgr_perf_platform_set_status);

void cm_mgr_perf_platform_set_force_status(int enable)
{
	if (enable) {
		debounce_times_perf_down_local = 0;

		if (cm_mgr_perf_enable == 0)
			return;

		if ((cm_mgr_perf_force_enable == 0) ||
				(pm_qos_update_request_status == 1))
			return;

		mtk_pm_qos_update_request(&ddr_opp_req, 0);
		pm_qos_update_request_status = enable;
	} else {
		if (pm_qos_update_request_status == 0)
			return;

		if ((cm_mgr_perf_force_enable == 0) ||
				(++debounce_times_perf_down_local >=
				 debounce_times_perf_force_down)) {
			mtk_pm_qos_update_request(&ddr_opp_req,
					MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE);
			pm_qos_update_request_status = enable;

			debounce_times_perf_down_local = 0;
		}
	}
}
EXPORT_SYMBOL_GPL(cm_mgr_perf_platform_set_force_status);

static void update_v2f(int update, int debug)
{
	int i, j;
	int _f, _v, _v2f;

	for (j = 0; j < CM_MGR_CPU_CLUSTER; j++) {
		for (i = 0; i < 16; i++) {
#ifdef CONFIG_MTK_CPU_FREQ
			_f = mt_cpufreq_get_freq_by_idx(j, i) / 1000;
			_v = mt_cpufreq_get_volt_by_idx(j, i) / 100;
#else
			_f = 0;
			_v = 0;
#endif /* CONFIG_MTK_CPU_FREQ */
			_v2f = (_v / 10) * (_v / 10) * _f / 100000;
			if (update)
				_v2f_all[i][j] = _v2f;
			if (debug)
				pr_debug("%d-i %.2d v %.8d f %.8d v2f %.8d\n",
						j, i, _v, _f, _v2f);
		}
	}
}

static void check_cm_mgr_status_internal(void);
#ifdef USE_TIMER_CHECK
struct timer_list cm_mgr_timer;

static void cm_mgr_timer_fn(struct timer_list *t)
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
	for (i = 0; i < CM_MGR_CPU_COUNT; i++) {
		if (i < CM_MGR_CPU_LIMIT)
			cpu_power_up_array[0] +=
				cpu_power_gain_opp(total_bw, IS_UP,
						cpu_opp_cur[0],
						cpu_ratio_idx[i], idx);
#ifndef USE_SINGLE_CLUSTER
		else
			cpu_power_up_array[1] +=
				cpu_power_gain_opp(total_bw, IS_UP,
						cpu_opp_cur[1],
						cpu_ratio_idx[i], idx + 1);
#endif /* ! USE_SINGLE_CLUSTER */
	}

	for (i = 0; i < CM_MGR_CPU_CLUSTER; i++) {
		cpu_power_up[i] = cpu_power_up_array[i] * v2f[i] / 100;
		cpu_power_total += cpu_power_up[i];
	}
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
#endif /* PER_CPU_STALL_RATIO */

	if (cm_mgr_opp_enable == 0) {
		if (vcore_dram_opp != CM_MGR_EMI_OPP) {
			vcore_dram_opp = CM_MGR_EMI_OPP;
#ifdef DEBUG_CM_MGR
			pr_info("#@# %s(%d) vcore_dram_opp %d->%d\n",
					__func__, __LINE__,
					vcore_dram_opp_cur, vcore_dram_opp);
#endif /* DEBUG_CM_MGR */
			cm_mgr_set_dram_level(
					CM_MGR_EMI_OPP - vcore_dram_opp);
		}

		return -1;
	}

	idx = level;
	vcore_power_up = vcore_power_gain(vcore_power_gain, total_bw, idx);
#ifdef DEBUG_CM_MGR
	pr_info("#@# vcore_power_up %d < cpu_power_total %d\n",
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
			pr_info("#@# %s(%d) vcore_dram_opp up %d->%d\n",
					__func__, __LINE__,
					vcore_dram_opp_cur, vcore_dram_opp);
#endif /* DEBUG_CM_MGR */
			cm_mgr_set_dram_level(
					CM_MGR_EMI_OPP - vcore_dram_opp);
		} else {
			if (debounce_times_reset_adb)
				debounce_times_up = 0;
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
	for (i = 0; i < CM_MGR_CPU_COUNT; i++) {
		if (i < CM_MGR_CPU_LIMIT)
			cpu_power_down_array[0] +=
				cpu_power_gain_opp(total_bw, IS_DOWN,
						cpu_opp_cur[0],
						cpu_ratio_idx[i], idx);
#ifndef USE_SINGLE_CLUSTER
		else
			cpu_power_down_array[1] +=
				cpu_power_gain_opp(total_bw, IS_DOWN,
						cpu_opp_cur[1],
						cpu_ratio_idx[i], idx + 1);
#endif /* ! USE_SINGLE_CLUSTER */
	}

	for (i = 0; i < CM_MGR_CPU_CLUSTER; i++) {
		cpu_power_down[i] = cpu_power_down_array[i] * v2f[i] / 100;
		cpu_power_total += cpu_power_down[i];
	}
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
#endif /* PER_CPU_STALL_RATIO */

	if (cm_mgr_opp_enable == 0) {
		if (vcore_dram_opp != CM_MGR_EMI_OPP) {
			vcore_dram_opp = CM_MGR_EMI_OPP;
#ifdef DEBUG_CM_MGR
			pr_info("#@# %s(%d) vcore_dram_opp %d->%d\n",
					__func__, __LINE__,
					vcore_dram_opp_cur, vcore_dram_opp);
#endif /* DEBUG_CM_MGR */
			cm_mgr_set_dram_level(
					CM_MGR_EMI_OPP - vcore_dram_opp);
		}

		return -1;
	}

	idx = level - 1;
	vcore_power_down = vcore_power_gain(vcore_power_gain, total_bw, idx);
#ifdef DEBUG_CM_MGR
	pr_info("#@# vcore_power_down %d > cpu_power_total %d\n",
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
			pr_info("#@# %s(%d) vcore_dram_opp down %d->%d\n",
					__func__, __LINE__,
					vcore_dram_opp_cur, vcore_dram_opp);
#endif /* DEBUG_CM_MGR */
			cm_mgr_set_dram_level(
					CM_MGR_EMI_OPP - vcore_dram_opp);
		} else {
			if (debounce_times_reset_adb)
				debounce_times_down = 0;
		}

		return -1;
	}

	return 0;
}

struct timer_list cm_mgr_perf_timer;
#define USE_TIMER_PERF_CHECK_TIME msecs_to_jiffies(50)

static void cm_mgr_perf_timer_fn(struct timer_list *t)
{
	if (cm_mgr_perf_timer_enable)
		check_cm_mgr_status_internal();
}

static void check_cm_mgr_status_internal(void)
{
	int level;
	unsigned long flags;

	if (cm_mgr_enable == 0) {
		cm_mgr_set_dram_level(0);
		return;
	}

	if (cm_mgr_disable_fb == 1 && cm_mgr_blank_status == 1) {
		cm_mgr_set_dram_level(0);
		return;
	}

	if (cm_mgr_perf_force_enable)
		return;

	if (!cm_mgr_check_bw_status()) {
		cm_mgr_set_dram_level(0);
		return;
	}

	if (spin_trylock_irqsave(&cm_mgr_lock, flags)) {
		int ret;
		int max_ratio_idx[CM_MGR_CPU_CLUSTER];
#if defined(LIGHT_LOAD) && defined(CONFIG_MTK_SCHED_RQAVG_US)
		unsigned int cpu;
		unsigned int rel_load, abs_load;
#endif /* defined(LIGHT_LOAD) && defined(CONFIG_MTK_SCHED_RQAVG_US) */
#ifdef PER_CPU_STALL_RATIO
		int cpu_ratio_idx[CM_MGR_CPU_COUNT];
#endif /* PER_CPU_STALL_RATIO */
		int i;

		vcore_dram_opp_cur = cm_mgr_get_dram_opp();
		if (vcore_dram_opp_cur > CM_MGR_EMI_OPP) {
			spin_unlock_irqrestore(&cm_mgr_lock, flags);
			return;
		}

		if (--cm_mgr_loop > 0)
			goto cm_mgr_opp_end;
		cm_mgr_loop = cm_mgr_loop_count;

#if defined(LIGHT_LOAD) && defined(CONFIG_MTK_SCHED_RQAVG_US)
		cm_mgr_abs_load = 0;
		cm_mgr_rel_load = 0;

		for_each_online_cpu(cpu) {
			int tmp;

			if (cpu >= CM_MGR_CPU_COUNT)
				break;

#ifdef CONFIG_MTK_CPU_FREQ
			tmp = mt_cpufreq_get_cur_phy_freq_no_lock(
					cpu / CM_MGR_CPU_LIMIT) /
				100000;
#else
			tmp = 0;
#endif /* CONFIG_MTK_CPU_FREQ */
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
			goto cm_mgr_opp_end;
		}
#endif /* defined(LIGHT_LOAD) && defined(CONFIG_MTK_SCHED_RQAVG_US) */
		cps_valid = 1;

#ifdef USE_NEW_CPU_OPP
#ifdef USE_SINGLE_CLUSTER
		ret = cm_mgr_check_stall_ratio(
				prev_freq[0] / 1000,
				0);
#else
		ret = cm_mgr_check_stall_ratio(
				prev_freq[0] / 1000,
				prev_freq[1] / 1000);
#endif /* USE_SINGLE_CLUSTER */
#else
#ifdef CONFIG_MTK_CPU_FREQ
#ifdef USE_AVG_PMU
#ifdef USE_SINGLE_CLUSTER
		ret = cm_mgr_check_stall_ratio(
				mt_cpufreq_get_cur_phy_freq_no_lock(0) / 1000,
				0);
#else
		ret = cm_mgr_check_stall_ratio(
				mt_cpufreq_get_cur_phy_freq_no_lock(0) / 1000,
				mt_cpufreq_get_cur_phy_freq_no_lock(1) / 1000);
#endif /* USE_SINGLE_CLUSTER */
#else
#ifdef USE_SINGLE_CLUSTER
		ret = cm_mgr_check_stall_ratio(
				mt_cpufreq_get_cur_freq(0) / 1000,
				0);
#else
		ret = cm_mgr_check_stall_ratio(
				mt_cpufreq_get_cur_freq(0) / 1000,
				mt_cpufreq_get_cur_freq(1) / 1000);
#endif /* USE_SINGLE_CLUSTER */
#endif /* USE_AVG_PMU */
#else
		ret = 0;
#endif /* CONFIG_MTK_CPU_FREQ */
#endif /* USE_NEW_CPU_OPP */
		total_bw = cm_mgr_get_bw() / 512;
		memset(count_ack, 0, ARRAY_SIZE(count_ack));

		if (total_bw_value)
			total_bw = total_bw_value;
		if (total_bw >= vcore_power_array_size(cm_mgr_get_idx()))
			total_bw = vcore_power_array_size(cm_mgr_get_idx()) - 1;
		if (total_bw < 0)
			total_bw = 0;

		if (update_v2f_table == 1) {
			update_v2f(1, 0);
			update_v2f_table++;
		}

		/* get max loading */
		memset(max_load, 0, ARRAY_SIZE(count_ack));

		for_each_possible_cpu(i) {
			int avg_load;

			if (i >= CM_MGR_CPU_COUNT)
				break;

			if (unlikely(loading_cnt == 0))
				break;
			avg_load = loading_acc[i] / loading_cnt;
			if (avg_load > max_load[i / CM_MGR_CPU_LIMIT])
				max_load[i / CM_MGR_CPU_LIMIT] = avg_load;
			loading_acc[i] = 0;
		}

		for (i = 0; i < CM_MGR_CPU_CLUSTER; i++) {
			count[i] = cm_mgr_get_cpu_count(i);
			ratio_max[i] = cm_mgr_get_max_stall_ratio(i);
			max_ratio_idx[i] = ratio_max[i] / 5;
			if (max_ratio_idx[i] > RATIO_COUNT)
				max_ratio_idx[i] = RATIO_COUNT;
#ifdef USE_NEW_CPU_OPP
			cpu_opp_cur[i] = prev_freq_idx[i];
#else
#ifdef CONFIG_MTK_CPU_FREQ
			cpu_opp_cur[i] = mt_cpufreq_get_cur_freq_idx(i);
#else
			cpu_opp_cur[i] = 0;
#endif /* CONFIG_MTK_CPU_FREQ */
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

		vcore_dram_opp = vcore_dram_opp_cur;
		if (vcore_dram_opp == CM_MGR_EMI_OPP)
			cm_mgr_set_dram_level(0);

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

		spin_unlock_irqrestore(&cm_mgr_lock, flags);
	}
}

static void cm_mgr_process(struct work_struct *work)
{
	mtk_pm_qos_update_request(&ddr_opp_req_by_cpu_opp,
			cm_mgr_cpu_to_dram_opp);
}

static void cm_mgr_update_dram_by_cpu_opp(int cpu_opp)
{
	int ret = 0;
	int dram_opp = 0;

	if (cm_mgr_disable_fb == 1 && cm_mgr_blank_status == 1) {
		if (cm_mgr_cpu_to_dram_opp !=
				MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE) {
			cm_mgr_cpu_to_dram_opp =
				MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE;
			ret = schedule_delayed_work(&cm_mgr_work, 1);
		}
		return;
	}

	if (!cm_mgr_work_ready)
		return;

	if (!cm_mgr_cpu_map_dram_enable) {
		if (cm_mgr_cpu_to_dram_opp !=
				MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE) {
			cm_mgr_cpu_to_dram_opp =
				MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE;
			ret = schedule_delayed_work(&cm_mgr_work, 1);
		}
		return;
	}

	if ((cpu_opp >= 0) && (cpu_opp < cm_mgr_cpu_opp_size))
		dram_opp = cm_mgr_cpu_opp_to_dram[cpu_opp];

	if (cm_mgr_cpu_to_dram_opp == dram_opp)
		return;

	cm_mgr_cpu_to_dram_opp = dram_opp;

	ret = schedule_delayed_work(&cm_mgr_work, 1);
}

void check_cm_mgr_status_mt6765(unsigned int cluster, unsigned int freq)
{
#ifdef CONFIG_MTK_CPU_FREQ
	int freq_idx = 0;
	struct mt_cpu_dvfs *p;

	p = id_to_cpu_dvfs(cluster);
	if (p)
		freq_idx = _search_available_freq_idx(p, freq, 0);

	if (freq_idx == prev_freq_idx[cluster])
		return;

	prev_freq_idx[cluster] = freq_idx;
	prev_freq[cluster] = freq;
#else
	prev_freq_idx[cluster] = 0;
	prev_freq[cluster] = 0;
#endif /* CONFIG_MTK_CPU_FREQ */

	if (cm_mgr_use_cpu_to_dram_map)
		cm_mgr_update_dram_by_cpu_opp
			(prev_freq_idx[CM_MGR_CPU_CLUSTER - 1]);

	check_cm_mgr_status_internal();
}
EXPORT_SYMBOL_GPL(check_cm_mgr_status_mt6765);

static void cm_mgr_add_cpu_opp_to_ddr_req(void)
{
	char owner[20] = "cm_mgr_cpu_to_dram";

	mtk_pm_qos_add_request(&ddr_opp_req_by_cpu_opp, MTK_PM_QOS_DDR_OPP,
			MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE);

	strncpy(ddr_opp_req_by_cpu_opp.owner,
			owner, sizeof(ddr_opp_req_by_cpu_opp.owner) - 1);

	if (cm_mgr_use_cpu_to_dram_map_new)
		cm_mgr_cpu_map_update_table();
}

static int platform_cm_mgr_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *node = pdev->dev.of_node;
#ifdef CONFIG_MTK_DVFSRC
	int i;
#endif /* CONFIG_MTK_DVFSRC */

	ret = cm_mgr_common_init();
	if (ret) {
		pr_info("[CM_MGR] FAILED TO INIT(%d)\n", ret);
		return ret;
	}

	(void)cm_mgr_get_idx();

	/* required-opps */
	cm_mgr_num_perf = of_count_phandle_with_args(node,
			"required-opps", NULL);
	pr_info("#@# %s(%d) cm_mgr_num_perf %d\n",
			__func__, __LINE__, cm_mgr_num_perf);

	if (cm_mgr_num_perf > 0) {
		cm_mgr_perfs = devm_kzalloc(&pdev->dev,
				cm_mgr_num_perf * sizeof(int),
				GFP_KERNEL);

		if (!cm_mgr_num_perf) {
			ret = -ENOMEM;
			goto ERROR;
		}

#ifdef CONFIG_MTK_DVFSRC
		for (i = 0; i < cm_mgr_num_perf; i++) {
			cm_mgr_perfs[i] =
				dvfsrc_get_required_opp_performance_state
				(node, i);
		}
#endif /* CONFIG_MTK_DVFSRC */
		cm_mgr_num_array = cm_mgr_num_perf - 1;
	} else
		cm_mgr_num_array = 0;
	pr_info("#@# %s(%d) cm_mgr_num_array %d\n",
			__func__, __LINE__, cm_mgr_num_array);

	ret = cm_mgr_check_dts_setting(pdev);
	if (ret) {
		pr_info("[CM_MGR] FAILED TO GET DTS DATA(%d)\n", ret);
		return ret;
	}

	cm_mgr_pdev = pdev;

	pr_info("[CM_MGR] platform-cm_mgr_probe Done.\n");

	spin_lock_init(&cm_mgr_cpu_mask_lock);

	cm_mgr_sched_pm_init();

	cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
			"cm_mgr:online",
			cm_mgr_cpuhp_online,
			cm_mgr_cpuhp_offline);

	timer_setup(&cm_mgr_perf_timer, cm_mgr_perf_timer_fn,
			TIMER_DEFERRABLE);

#ifdef USE_TIMER_CHECK
	timer_setup(&cm_mgr_timer, cm_mgr_timer_fn,
			TIMER_DEFERRABLE);
#endif /* USE_TIMER_CHECK */

#ifdef CONFIG_MTK_CPU_FREQ
	mt_cpufreq_set_governor_freq_registerCB(check_cm_mgr_status_mt6765);
#endif /* CONFIG_MTK_CPU_FREQ */

	mtk_pm_qos_add_request(&ddr_opp_req, MTK_PM_QOS_DDR_OPP,
			MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE);

	if (cm_mgr_use_cpu_to_dram_map) {
		cm_mgr_add_cpu_opp_to_ddr_req();

		INIT_DELAYED_WORK(&cm_mgr_work, cm_mgr_process);
		cm_mgr_work_ready = 1;
	}

	spin_lock_init(&cm_mgr_lock);

	vcore_power_gain = vcore_power_gain_ptr(cm_mgr_get_idx());

	return 0;

ERROR:
	return ret;
}

static int platform_cm_mgr_remove(struct platform_device *pdev)
{
	cm_mgr_work_ready = 0;

	cm_mgr_common_exit();

	kfree(cm_mgr_perfs);
	kfree(cm_mgr_cpu_opp_to_dram);
	kfree(cm_mgr_buf);

	return 0;
}

static const struct of_device_id platform_cm_mgr_of_match[] = {
	{ .compatible = "mediatek,mt6765-cm_mgr", },
	{},
};

static const struct platform_device_id platform_cm_mgr_id_table[] = {
	{ "mt6765-cm_mgr", 0},
	{ },
};

static struct platform_driver mtk_platform_cm_mgr_driver = {
	.probe = platform_cm_mgr_probe,
	.remove	= platform_cm_mgr_remove,
	.driver = {
		.name = "mt6765-cm_mgr",
		.owner = THIS_MODULE,
		.of_match_table = platform_cm_mgr_of_match,
	},
	.id_table = platform_cm_mgr_id_table,
};

/*
 * driver initialization entry point
 */
static int __init platform_cm_mgr_init(void)
{
	return platform_driver_register(&mtk_platform_cm_mgr_driver);
}

static void __exit platform_cm_mgr_exit(void)
{
	platform_driver_unregister(&mtk_platform_cm_mgr_driver);
	pr_info("[CM_MGR] platform-cm_mgr Exit.\n");
}

late_initcall(platform_cm_mgr_init);
module_exit(platform_cm_mgr_exit);

MODULE_DESCRIPTION("Mediatek cm_mgr driver");
MODULE_AUTHOR("Morven-CF Yeh<morven-cf.yeh@mediatek.com>");
MODULE_LICENSE("GPL");
