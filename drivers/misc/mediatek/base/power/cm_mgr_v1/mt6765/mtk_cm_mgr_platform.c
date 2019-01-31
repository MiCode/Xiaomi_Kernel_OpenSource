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

#include <linux/of.h>
#include <linux/of_address.h>
#include <mtk_cm_mgr.h>

#define CREATE_TRACE_POINTS
#include "mtk_cm_mgr_platform_events.h"

#include <linux/fb.h>
#include <linux/notifier.h>

#include <linux/pm_qos.h>
#include <helio-dvfsrc.h>
#ifdef USE_IDLE_NOTIFY
#include "mtk_idle.h"
#endif /* USE_IDLE_NOTIFY */

#include <linux/cpu_pm.h>
static int cm_mgr_idle_mask;
void __iomem *spm_sleep_base;

void __iomem *mcucfg_mp0_counter_base;

spinlock_t cm_mgr_cpu_mask_lock;

#define diff_value_overflow(diff, a, b) do {\
	if ((a) >= (b)) \
	diff = (a) - (b);\
	else \
	diff = 0xffffffff - (b) + (a); \
} while (0) \

#define CM_MGR_MAX(a, b) (((a) > (b)) ? (a) : (b))

#define USE_TIME_NS
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
void debug_stall(int cpu)
{
	pr_debug("%s: cpu number %d ################\n", __func__,
			cpu);
	pr_debug("%s: clustor[%d] 0x%08x\n", __func__,
			cpu / 4, pstall_all->clustor[cpu / 4]);
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
			cpu / 4, pstall_all->ratio_max[cpu / 4]);
	pr_debug("%s: cpu 0x%08x\n", __func__, pstall_all->cpu);
	pr_debug("%s: cpu_count[%d] 0x%08x\n", __func__,
			cpu / 4, pstall_all->cpu_count[cpu / 4]);
}

void debug_stall_all(void)
{
	int i;

	for (i = 0; i < CM_MGR_CPU_COUNT; i++)
		debug_stall(i);
}
#endif

static int cm_mgr_check_dram_type(void)
{
#ifdef CONFIG_MTK_DRAMC
	int ddr_type = get_ddr_type();
	int ddr_hz = dram_steps_freq(0);

	if (ddr_type == TYPE_LPDDR4X && ddr_hz == 3200)
		cm_mgr_idx = CM_MGR_LP4X_2CH_3200;
	else if (ddr_type == TYPE_LPDDR3 && ddr_hz == 1866)
		cm_mgr_idx = CM_MGR_LP3_1CH_1866;
	pr_info("#@# %s(%d) cm_mgr_idx 0x%x\n", __func__, __LINE__, cm_mgr_idx);
#else
	cm_mgr_idx = 0;
	pr_info("#@# %s(%d) NO CONFIG_MTK_DRAMC !!! set cm_mgr_idx to 0x%x\n",
			__func__, __LINE__, cm_mgr_idx);
#endif /* CONFIG_MTK_DRAMC */
	return cm_mgr_idx;
};

int cm_mgr_get_idx(void)
{
	if (cm_mgr_idx < 0)
		return cm_mgr_check_dram_type();
	else
		return cm_mgr_idx;
};

int cm_mgr_get_stall_ratio(int cpu)
{
	return pstall_all->ratio[cpu];
}

int cm_mgr_get_max_stall_ratio(int cluster)
{
	return pstall_all->ratio_max[cluster];
}

int cm_mgr_get_cpu_count(int cluster)
{
	return pstall_all->cpu_count[cluster];
}

static unsigned int cm_mgr_read_stall(int cpu)
{
	unsigned int val = 0;

	if (!spin_trylock(&cm_mgr_cpu_mask_lock))
		return val;

	if (cpu < 4) {
		if (cm_mgr_idle_mask & 0x0f)

			val = cm_mgr_read(MP0_CPU0_STALL_COUNTER + 4 * cpu);
	} else {
		if (cm_mgr_idle_mask & 0xf0)
			val = cm_mgr_read(MP1_CPU0_STALL_COUNTER +
					4 * (cpu - 4));
	}
	spin_unlock(&cm_mgr_cpu_mask_lock);

	return val;
}

int cm_mgr_check_stall_ratio(int mp0, int mp1)
{
	unsigned int i;
	unsigned int clustor;
	unsigned int stall_val_new;
#ifdef USE_TIME_NS
	unsigned long long time_ns_new;
#endif

	pstall_all->clustor[0] = mp0;
	pstall_all->clustor[1] = mp1;
	pstall_all->cpu = 0;
	for (i = 0; i < CM_MGR_CPU_CLUSTER; i++) {
		pstall_all->ratio_max[i] = 0;
		pstall_all->cpu_count[i] = 0;
	}

	for (i = 0; i < CM_MGR_CPU_COUNT; i++) {
		pstall_all->ratio[i] = 0;
		clustor = i / 4;

		stall_val_new = cm_mgr_read_stall(i);

		if (stall_val_new == 0 || stall_val_new == 0xdeadbeef) {
#ifdef USE_DEBUG_LOG
			pr_debug("%s: WARN!!! stall_val_new is 0x%08x\n",
					__func__, stall_val_new);
			debug_stall(i);
#endif
			continue;
		}

#ifdef USE_TIME_NS
		time_ns_new = sched_clock();
		pstall_all->time_ns_diff[i] =
			time_ns_new - pstall_all->time_ns[i];
		pstall_all->time_ns[i] = time_ns_new;
#endif

		diff_value_overflow(pstall_all->stall_val_diff[i],
				stall_val_new, pstall_all->stall_val[i]);
		pstall_all->stall_val[i] = stall_val_new;

		if (pstall_all->stall_val_diff[i] == 0) {
#ifdef USE_DEBUG_LOG
			pr_debug("%s: WARN!!! cpu:%d diff == 0\n", __func__, i);
			debug_stall(i);
#endif
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
#endif
		if (pstall_all->ratio[i] > 100) {
#ifdef USE_DEBUG_LOG
			pr_debug("%s: WARN!!! cpu:%d ratio > 100\n",
					__func__, i);
			debug_stall(i);
#endif
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
#endif
	}

#ifdef USE_DEBUG_LOG
	debug_stall_all();
#endif
	return 0;
}

static void init_cpu_stall_counter(int cluster)
{
	unsigned int val;

	if (cluster == 0) {
		val = 0x11000;
		cm_mgr_write(MP0_CPU_STALL_INFO, val);

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

#ifdef CONFIG_CPU_PM
static int cm_mgr_sched_pm_notifier(struct notifier_block *self,
			       unsigned long cmd, void *v)
{
	unsigned int cur_cpu = smp_processor_id();
	unsigned long spinlock_save_flags;

	spin_lock_irqsave(&cm_mgr_cpu_mask_lock, spinlock_save_flags);

	if (cmd == CPU_PM_EXIT) {
		if (((cm_mgr_idle_mask & 0x0f) == 0x0) && (cur_cpu < 4))
			init_cpu_stall_counter(0);
		else if (((cm_mgr_idle_mask & 0xf0) == 0x0) && (cur_cpu >= 4))
			init_cpu_stall_counter(1);
		cm_mgr_idle_mask |= (1 << cur_cpu);
	} else if (cmd == CPU_PM_ENTER)
		cm_mgr_idle_mask &= ~(1 << cur_cpu);

	spin_unlock_irqrestore(&cm_mgr_cpu_mask_lock, spinlock_save_flags);

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

static int cm_mgr_cpu_callback(struct notifier_block *nfb,
				   unsigned long action, void *hcpu)
{
	unsigned int cur_cpu = (long)hcpu;
	unsigned long spinlock_save_flags;

	spin_lock_irqsave(&cm_mgr_cpu_mask_lock, spinlock_save_flags);

	switch (action) {
	case CPU_ONLINE:
		if (((cm_mgr_idle_mask & 0x0f) == 0x0) && (cur_cpu < 4))
			init_cpu_stall_counter(0);
		else if (((cm_mgr_idle_mask & 0xf0) == 0x0) && (cur_cpu >= 4))
			init_cpu_stall_counter(1);
		cm_mgr_idle_mask |= (1 << cur_cpu);
		break;
	case CPU_DOWN_PREPARE:
		cm_mgr_idle_mask &= ~(1 << cur_cpu);
		break;
	}

	spin_unlock_irqrestore(&cm_mgr_cpu_mask_lock, spinlock_save_flags);

	return NOTIFY_OK;
}

/* FIXME: */
#define CPU_PRI_PERF 20

static struct notifier_block cm_mgr_cpu_notifier = {
	.notifier_call = cm_mgr_cpu_callback,
	.priority = CPU_PRI_PERF + 1,
};

static void cm_mgr_hotplug_cb_init(void)
{
	register_cpu_notifier(&cm_mgr_cpu_notifier);
}

static int cm_mgr_fb_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int blank;

	if (event != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;

	switch (blank) {
	case FB_BLANK_UNBLANK:
		pr_info("#@# %s(%d) SCREEN ON\n", __func__, __LINE__);
		cm_mgr_blank_status = 0;
		break;
	case FB_BLANK_POWERDOWN:
		pr_info("#@# %s(%d) SCREEN OFF\n", __func__, __LINE__);
		cm_mgr_blank_status = 1;
		dvfsrc_set_power_model_ddr_request(0);
		break;
	default:
		break;
	}

	return 0;
}

static struct notifier_block cm_mgr_fb_notifier = {
	.notifier_call = cm_mgr_fb_notifier_callback,
};

#ifdef USE_IDLE_NOTIFY
static int cm_mgr_idle_cb(struct notifier_block *nfb,
				  unsigned long id,
				  void *arg)
{
	switch (id) {
	case NOTIFY_SOIDLE_ENTER:
	case NOTIFY_SOIDLE3_ENTER:
		if (get_cur_ddr_opp() != CM_MGR_EMI_OPP)
			check_cm_mgr_status_internal();
		break;
	case NOTIFY_DPIDLE_ENTER:
	case NOTIFY_DPIDLE_LEAVE:
	case NOTIFY_SOIDLE_LEAVE:
	case NOTIFY_SOIDLE3_LEAVE:
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block cm_mgr_idle_notify = {
	.notifier_call = cm_mgr_idle_cb,
};
#endif /* USE_IDLE_NOTIFY */

#if 0
static int cm_mgr_is_lp_flavor(void)
{
	int r = 0;

#if defined(CONFIG_ARM64)
	int len;

	len = sizeof(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES);

	if (strncmp(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES + len - 4,
				"_lp", 3) == 0)
		r = 1;

	pr_info("flavor check: %s, is_lp: %d\n",
			CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES, r);
#endif

	return r;
}
#endif

struct timer_list cm_mgr_ratio_timer;
#define CM_MGR_RATIO_TIMER_MS	1

static void cm_mgr_ratio_timer_fn(unsigned long data)
{
	trace_CM_MGR__stall_raio_0(
			(unsigned int)cm_mgr_read(MP0_CPU_AVG_STALL_RATIO));
	trace_CM_MGR__stall_raio_1(
			(unsigned int)cm_mgr_read(MP1_CPU_AVG_STALL_RATIO));

	cm_mgr_ratio_timer.expires = jiffies +
		msecs_to_jiffies(CM_MGR_RATIO_TIMER_MS);
	add_timer(&cm_mgr_ratio_timer);
}

void cm_mgr_ratio_timer_en(int enable)
{
	if (enable) {
		cm_mgr_ratio_timer.expires = jiffies +
			msecs_to_jiffies(CM_MGR_RATIO_TIMER_MS);
		add_timer(&cm_mgr_ratio_timer);
	} else {
		del_timer(&cm_mgr_ratio_timer);
	}
}

void cm_mgr_perf_platform_set_status(int enable)
{
	if (enable) {
		cpu_power_ratio_up[0] = 500;
		cpu_power_ratio_up[1] = 500;
		debounce_times_up_adb[1] = 0;
	} else {
		cpu_power_ratio_up[0] = 100;
		cpu_power_ratio_up[1] = 100;
		debounce_times_up_adb[1] = 3;
	}
}

static struct pm_qos_request ddr_opp_req;
static int debounce_times_perf_down_local;
static int pm_qos_update_request_status;
void cm_mgr_perf_platform_set_force_status(int enable)
{
	if (enable) {
		debounce_times_perf_down_local = 0;

		if ((cm_mgr_perf_force_enable == 0) ||
				(pm_qos_update_request_status == 1))
			return;

		pm_qos_update_request(&ddr_opp_req, 0);
		pm_qos_update_request_status = enable;
	} else {
		if (pm_qos_update_request_status == 0)
			return;

		if ((cm_mgr_perf_force_enable == 0) ||
				(++debounce_times_perf_down_local >=
				 debounce_times_perf_down)) {
			pm_qos_update_request(&ddr_opp_req,
					PM_QOS_DDR_OPP_DEFAULT_VALUE);
			pm_qos_update_request_status = enable;

			debounce_times_perf_down_local = 0;
		}
	}
}

int cm_mgr_register_init(void)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL,
			"mediatek,mcucfg_mp0_counter");
	if (!node)
		pr_info("find mcucfg_mp0_counter node failed\n");
	mcucfg_mp0_counter_base = of_iomap(node, 0);
	if (!mcucfg_mp0_counter_base) {
		pr_info("base mcucfg_mp0_counter_base failed\n");
		return -1;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");
	if (!node)
		pr_info("find sleep node failed\n");
	spm_sleep_base = of_iomap(node, 0);
	if (!spm_sleep_base) {
		pr_info("base spm_sleep_base failed\n");
		return -1;
	}

	return 0;
}

int cm_mgr_platform_init(void)
{
	int r;

	r = cm_mgr_register_init();
	if (r) {
		pr_info("FAILED TO CREATE REGISTER(%d)\n", r);
		return r;
	}

	cm_mgr_sched_pm_init();

	r = fb_register_client(&cm_mgr_fb_notifier);
	if (r) {
		pr_info("FAILED TO REGISTER FB CLIENT (%d)\n", r);
		return r;
	}

	spin_lock_init(&cm_mgr_cpu_mask_lock);

	cm_mgr_hotplug_cb_init();

#ifdef USE_IDLE_NOTIFY
	mtk_idle_notifier_register(&cm_mgr_idle_notify);
#endif /* USE_IDLE_NOTIFY */

#if 0
	if (cm_mgr_is_lp_flavor())
		cm_mgr_enable = 1;
#endif

	init_timer_deferrable(&cm_mgr_ratio_timer);
	cm_mgr_ratio_timer.function = cm_mgr_ratio_timer_fn;
	cm_mgr_ratio_timer.data = 0;

	mt_cpufreq_set_governor_freq_registerCB(check_cm_mgr_status);

	pm_qos_add_request(&ddr_opp_req, PM_QOS_DDR_OPP,
			PM_QOS_DDR_OPP_DEFAULT_VALUE);

	return r;
}
