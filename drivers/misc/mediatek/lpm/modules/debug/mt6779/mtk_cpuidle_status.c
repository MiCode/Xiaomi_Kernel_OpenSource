// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/cpuidle.h>
#include <linux/sched/clock.h>
#include <linux/cpu.h>
#include <linux/pm_qos.h>
#include <linux/tick.h>
#include <linux/timer.h>

#include <mtk_lpm.h>

#include <mtk_lp_plat_reg.h>
#include <mtk_lp_plat_apmcu.h>

#include "mtk_cpuidle_status.h"
#include "mtk_cpuidle_cpc.h"

#define DUMP_INTERVAL       sec_to_ns(5)
static u64 last_dump_ns;

/* qos */
static struct pm_qos_request mtk_cpuidle_qos_req;

#define mtk_cpu_pm_init()\
	pm_qos_add_request(&mtk_cpuidle_qos_req,\
		PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE)
#define mtk_cpu_pm_block()\
	pm_qos_update_request(&mtk_cpuidle_qos_req, 2)
#define mtk_cpu_pm_allow()\
	pm_qos_update_request(&mtk_cpuidle_qos_req, PM_QOS_DEFAULT_VALUE)

#define ALL_CPU_WFI         (0)
#define ALL_CPU_OFF         (1)
#define ALL_CPU_CLUSTER_OFF (2)
#define ALL_CPU_MCUSYS_OFF  (3)

struct mtk_cpuidle_ratio {
	unsigned long long start_us;
	unsigned long long idle_time[CPUIDLE_STATE_MAX];
	unsigned int bp[CPUIDLE_STATE_MAX]; /* basis points : 0.01% */
};

struct mtk_cpuidle_info {
	u64 enter_time_ns;
	int idle_index;
	int cnt[CPUIDLE_STATE_MAX];
};

struct mtk_cpuidle_prof {
	unsigned int cnt;
	unsigned int max;
	union {
		u64 avg;
		u64 sum;
	};
};

/* status */
struct mtk_cpuidle_device {
	struct mtk_cpuidle_ratio ratio;
	struct mtk_cpuidle_info info;
	struct mtk_cpuidle_prof prof;
	struct hrtimer timer;
	int cpu;
	int state_count;
	bool tmr_running;
};

struct mtk_cpuidle_control {
	bool tmr_en;
	bool prof_en;
	bool log_en;
};

static DEFINE_PER_CPU(struct mtk_cpuidle_device, mtk_cpuidle_dev);
static struct mtk_cpuidle_control mtk_cpuidle_ctrl;
static struct mtk_cpuidle_ratio all_cpu_idle;

void mtk_cpuidle_ctrl_timer_en(bool enable)
{
	mtk_cpu_pm_block();
	mtk_cpuidle_ctrl.tmr_en = enable;
	mtk_cpu_pm_allow();
}

bool mtk_cpuidle_ctrl_timer_sta_get(void)
{
	return mtk_cpuidle_ctrl.tmr_en;
}

void mtk_cpuidle_ctrl_log_en(bool enable)
{
	mtk_cpu_pm_block();
	mtk_cpuidle_ctrl.log_en = enable;
	mtk_cpu_pm_allow();
}

bool mtk_cpuidle_ctrl_log_sta_get(void)
{
	return mtk_cpuidle_ctrl.log_en;
}

static void _mtk_cpuidle_prof_ratio_start(void *data)
{
	int i, cpu;
	struct cpuidle_device *dev = cpuidle_get_device();
	struct mtk_cpuidle_device *mtk_idle;

	cpu = get_cpu();
	mtk_idle = &per_cpu(mtk_cpuidle_dev, cpu);
	put_cpu();

	if (unlikely(!dev || !mtk_idle))
		return;

	mtk_idle->ratio.start_us = div64_u64(sched_clock(), 1000);

	for (i = 0; i < mtk_idle->state_count; i++)
		mtk_idle->ratio.idle_time[i] = 0;
}

void mtk_cpuidle_prof_ratio_start(void)
{
	all_cpu_idle.start_us = div64_u64(sched_clock(), 1000);
	all_cpu_idle.idle_time[ALL_CPU_CLUSTER_OFF] = 0;
	all_cpu_idle.idle_time[ALL_CPU_MCUSYS_OFF] = 0;

	mtk_cpuidle_ctrl.prof_en = true;

	on_each_cpu(_mtk_cpuidle_prof_ratio_start, NULL, 0);
}

static void _mtk_cpuidle_prof_ratio_stop(void *stop_time)
{
	int i, cpu;
	uint64_t dur_time;
	struct cpuidle_device *dev = cpuidle_get_device();
	struct mtk_cpuidle_device *mtk_idle;

	cpu = get_cpu();
	mtk_idle = &per_cpu(mtk_cpuidle_dev, cpu);
	put_cpu();

	if (unlikely(!dev || !mtk_idle))
		return;

	dur_time = div64_u64(sched_clock(), 1000) -
				mtk_idle->ratio.start_us;

	for (i = 0; i < mtk_idle->state_count; i++)
		mtk_idle->ratio.bp[i] = (unsigned int)div64_u64(
				mtk_idle->ratio.idle_time[i] * 10000,
				dur_time);
}

void mtk_cpuidle_prof_ratio_stop(void)
{
	uint64_t time;

	time = div64_u64(sched_clock(), 1000) - all_cpu_idle.start_us;

	mtk_cpuidle_ctrl.prof_en = false;

	all_cpu_idle.bp[ALL_CPU_CLUSTER_OFF] = (unsigned int)div64_u64(
		all_cpu_idle.idle_time[ALL_CPU_CLUSTER_OFF] * 10000, time);

	all_cpu_idle.bp[ALL_CPU_MCUSYS_OFF] = (unsigned int)div64_u64(
		all_cpu_idle.idle_time[ALL_CPU_MCUSYS_OFF] * 10000, time);

	on_each_cpu(_mtk_cpuidle_prof_ratio_stop, NULL, 0);
}

void mtk_cpuidle_prof_ratio_dump(struct seq_file *m)
{
	int i, cpu;
	struct mtk_cpuidle_device *mtk_idle;

	seq_printf(m, "%6s%12s%12s%12s\n",
			"",
			"cpu_off",
			"cluster_off",
			"mcusys_off");

	for_each_possible_cpu(cpu) {
		mtk_idle = &per_cpu(mtk_cpuidle_dev, cpu);

		if (!mtk_idle)
			continue;

		seq_printf(m, "cpu%d: ", cpu);
		for (i = 1; i < mtk_idle->state_count; i++) {
			seq_printf(m, "%5s%3u.%02u%%",
				"",
				mtk_idle->ratio.bp[i] / 100,
				mtk_idle->ratio.bp[i] % 100);

		}
		seq_puts(m, "\n");
	}

	seq_puts(m, "all core off ratio :\n");
	seq_printf(m, "cluster = %3u.%02u%%\n",
			all_cpu_idle.bp[ALL_CPU_CLUSTER_OFF] / 100,
			all_cpu_idle.bp[ALL_CPU_CLUSTER_OFF] % 100);
	seq_printf(m, " mcusys = %3u.%02u%%\n",
			all_cpu_idle.bp[ALL_CPU_MCUSYS_OFF] / 100,
			all_cpu_idle.bp[ALL_CPU_MCUSYS_OFF] % 100);
}

void mtk_cpuidle_state_enable(bool en)
{
	struct cpuidle_driver *drv;
	int i, cpu;

	mtk_cpu_pm_block();

	for_each_possible_cpu(cpu) {

		drv = cpuidle_get_cpu_driver(per_cpu(cpuidle_devices, cpu));

		if (!drv)
			continue;

		for (i = drv->state_count - 1; i > 0; i--)
			mtk_cpuidle_set_param(drv, i, IDLE_PARAM_EN, en);
	}

	mtk_cpu_pm_allow();
}

static bool mtk_cpuidle_need_dump(unsigned int idx)
{
	struct cpuidle_driver *drv;
	u64 curr_ns;
	bool dump = false;

	if (!mtk_cpuidle_ctrl.log_en || !idx)
		return false;

	if (unlikely(!drv))
		return false;

	curr_ns = sched_clock();

	if (curr_ns - last_dump_ns > DUMP_INTERVAL) {
		last_dump_ns = curr_ns;
		dump = true;
	}

	return dump;
}

static unsigned int mtk_cpuidle_get_cluster_off_cnt(void)
{
	unsigned int cnt = mtk_lpm_mcusys_read(CPC_DORMANT_COUNTER);

	/**
	 * Cluster off count
	 * bit[0:15] : memory retention
	 * bit[16:31] : memory off
	 */
	if ((cnt & 0x7FFF) == 0)
		cnt = ((cnt >> 16) & 0x7FFF);
	else
		cnt = cnt & 0x7FFF;

	cnt += mtk_lpm_syssram_read(SYSRAM_CPC_CPUSYS_CNT_BACKUP);

	cpc_cluster_cnt_clr();

	mtk_lpm_syssram_write(SYSRAM_CPC_CPUSYS_CNT_BACKUP, 0);

	mtk_lpm_syssram_write(SYSRAM_CPUSYS_CNT,
			mtk_lpm_syssram_read(SYSRAM_CPUSYS_CNT) + cnt);

	return cnt;
}

static unsigned int mtk_cpuidle_get_mcusys_off_cnt(void)
{
	unsigned int cnt = mtk_lpm_syssram_read(SYSRAM_CPC_MCUSYS_CNT_BACKUP);

	mtk_lpm_syssram_write(SYSRAM_CPC_MCUSYS_CNT_BACKUP, 0);

	mtk_lpm_syssram_write(SYSRAM_MCUSYS_CNT,
			mtk_lpm_syssram_read(SYSRAM_MCUSYS_CNT) + cnt);
	return cnt;
}

static void mtk_cpuidle_dump_info(void)
{
	struct mtk_cpuidle_device *mtk_idle;
	int cpu, idx, ofs;
	unsigned int avail_cpu_mask = 0;

	for_each_possible_cpu(cpu) {

		if (cpu_online(cpu))
			avail_cpu_mask |= (1 << cpu);

		mtk_idle = &per_cpu(mtk_cpuidle_dev, cpu);

		if (!mtk_idle)
			continue;

		ofs = SYSRAM_RECENT_CPU_CNT(cpu);

		mtk_lpm_syssram_write(ofs, 0);

		for (idx = 1; idx < mtk_idle->state_count ; idx++) {

			mtk_lpm_syssram_write(ofs, mtk_lpm_syssram_read(ofs)
						+ mtk_idle->info.cnt[idx]);
			mtk_idle->info.cnt[idx] = 0;
		}
	}

	mtk_lpm_syssram_write(SYSRAM_RECENT_CPUSYS_CNT,
				mtk_cpuidle_get_cluster_off_cnt());

	mtk_lpm_syssram_write(SYSRAM_RECENT_MCUSYS_CNT,
				mtk_cpuidle_get_mcusys_off_cnt());

	mtk_lpm_syssram_write(SYSRAM_CPU_ONLINE, avail_cpu_mask);

	mtk_lpm_syssram_write(SYSRAM_RECENT_CNT_TS_H,
			(unsigned int)((last_dump_ns >> 32) & 0xFFFFFFFF));

	mtk_lpm_syssram_write(SYSRAM_RECENT_CNT_TS_L,
			(unsigned int)(last_dump_ns & 0xFFFFFFFF));
}

static enum hrtimer_restart mtk_cpuidle_hrtimer_func(struct hrtimer *timer)
{
	return HRTIMER_NORESTART;
}

static void mtk_cpuidle_set_timer(struct mtk_cpuidle_device *mtk_idle)
{
	int index;
	int limit_sleep_us;
	struct cpuidle_driver *drv;

	if (!mtk_cpuidle_ctrl.tmr_en)
		return;

	index = mtk_idle->info.idle_index;

	/* Only support WFI/CPU_OFF state */
	if (index > 1)
		return;

	drv = cpuidle_get_driver();

	if (unlikely(!drv))
		return;

	if (index + 1 >= mtk_idle->state_count
		|| !tick_nohz_tick_stopped())
		return;

	limit_sleep_us = 4 * max_t(unsigned int,
				get_residency(drv, index),
				get_residency(drv, index + 1));

	if (index != 0)
		tick_broadcast_exit();

	RCU_NONIDLE(hrtimer_start(&mtk_idle->timer,
			ns_to_ktime(limit_sleep_us * NSEC_PER_USEC),
			HRTIMER_MODE_REL_PINNED));
	mtk_idle->tmr_running = true;

	if (index != 0)
		tick_broadcast_enter();
}

static void mtk_cpuidle_cancel_timer(struct mtk_cpuidle_device *mtk_idle)
{
	if (!mtk_cpuidle_ctrl.tmr_en)
		return;

	if (mtk_idle->tmr_running) {
		mtk_idle->tmr_running = true;
		RCU_NONIDLE(hrtimer_try_to_cancel(&mtk_idle->timer));
	}
}

static unsigned long long last_mcusys_off_us;
static unsigned long long last_cluster_off_us;

static void mtk_cpuidle_set_last_off_ts(int cpu)
{
	uint64_t curr_us = sched_clock();

	if (mtk_lp_plat_is_mcusys_off())
		last_mcusys_off_us = div64_u64(curr_us, 1000);

	if (mtk_lp_plat_is_cluster_off(cpu))
		last_cluster_off_us = div64_u64(curr_us, 1000);
}

static void mtk_cpuidle_save_idle_time(int cpu)
{
	uint64_t curr_us = sched_clock();

	if (mtk_lp_plat_is_mcusys_off())
		all_cpu_idle.idle_time[ALL_CPU_MCUSYS_OFF] +=
			(div64_u64(curr_us, 1000) - last_mcusys_off_us);

	if (mtk_lp_plat_is_cluster_off(cpu))
		all_cpu_idle.idle_time[ALL_CPU_CLUSTER_OFF] +=
			(div64_u64(curr_us, 1000) - last_cluster_off_us);
}

static int mtk_cpuidle_status_update(struct notifier_block *nb,
			unsigned long action, void *data)
{
	unsigned long long dur_us;
	struct mtk_cpuidle_device *mtk_idle;
	struct mtk_lpm_nb_data *nb_data = (struct mtk_lpm_nb_data *)data;

	if (action & MTK_LPM_NB_AFTER_PROMPT) {

		if (mtk_cpuidle_ctrl.prof_en)
			mtk_cpuidle_set_last_off_ts(nb_data->cpu);
	}

	if (action & MTK_LPM_NB_PREPARE) {
		mtk_idle = &per_cpu(mtk_cpuidle_dev, nb_data->cpu);
		mtk_idle->info.idle_index = nb_data->index;
		mtk_idle->info.enter_time_ns = sched_clock();

		mtk_cpuidle_set_timer(mtk_idle);
	}

	if (action & MTK_LPM_NB_RESUME) {
		mtk_idle = &per_cpu(mtk_cpuidle_dev, nb_data->cpu);
		mtk_idle->info.idle_index = -1;
		mtk_idle->info.cnt[nb_data->index]++;

		if (mtk_cpuidle_ctrl.prof_en) {
			dur_us = div64_u64(
				sched_clock() - mtk_idle->info.enter_time_ns,
				1000);

			mtk_idle->ratio.idle_time[nb_data->index] += dur_us;
		}

		mtk_cpuidle_cancel_timer(mtk_idle);
	}

	if (action & MTK_LPM_NB_BEFORE_REFLECT) {

		if (mtk_cpuidle_ctrl.prof_en)
			mtk_cpuidle_save_idle_time(nb_data->cpu);

		if (mtk_cpuidle_need_dump(nb_data->index))
			mtk_cpuidle_dump_info();
	}

	return NOTIFY_OK;
}

struct notifier_block mtk_cpuidle_status_nb = {
	.notifier_call = mtk_cpuidle_status_update,
};

static void mtk_cpuidle_init_per_cpu(void *info)
{
	struct mtk_cpuidle_device *mtk_idle;
	struct cpuidle_driver *drv = cpuidle_get_driver();
	int cpu;

	if (unlikely(!drv))
		return;

	cpu = get_cpu();

	mtk_idle = &per_cpu(mtk_cpuidle_dev, cpu);

	put_cpu();

	mtk_idle->cpu = cpu;
	mtk_idle->timer.function = mtk_cpuidle_hrtimer_func;
	mtk_idle->state_count = drv->state_count;

	hrtimer_init(&mtk_idle->timer,
		CLOCK_MONOTONIC, HRTIMER_MODE_REL);
}

int __init mtk_cpuidle_status_init(void)
{
	mtk_cpu_pm_init();

	mtk_cpuidle_ctrl.tmr_en = true;
	mtk_cpuidle_ctrl.prof_en = false;
	mtk_cpuidle_ctrl.log_en = true;

	mtk_cpu_pm_block();
	on_each_cpu(mtk_cpuidle_init_per_cpu, NULL, 0);
	mtk_cpu_pm_allow();

	mtk_lpm_notifier_register(&mtk_cpuidle_status_nb);
	return 0;
}
late_initcall_sync(mtk_cpuidle_status_init);

