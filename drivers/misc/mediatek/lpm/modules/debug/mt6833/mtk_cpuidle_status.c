// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/cpuidle.h>
#include <linux/sched/clock.h>
#include <linux/cpu.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/pm_qos.h>
#include <linux/tick.h>
#include <linux/timer.h>

//#include <mtk_ram_console.h>
#include <mtk_lpm.h>
#include <mtk_lp_plat_reg.h>
#include <mt6833_spm_comm.h>
#include "mtk_cpupm_dbg.h"
#include "mtk_cpuidle_status.h"
#include "mtk_cpuidle_cpc.h"

#define DUMP_INTERVAL       sec_to_ns(5)
static u64 last_dump_ns;
static unsigned long long mtk_lpm_last_cpuidle_dis;

/* stress test */
static unsigned int timer_interval = 10 * 1000;
static struct task_struct *stress_tsk[NR_CPUS];

/* CPU off state : core, cluster and mcusys */
#define CPU_OFF_MAX_LV  (3)

/* all core idle profiling */
struct all_cpu_idle_data {
	unsigned long long last_ns;
	unsigned long long dur_ns;
	unsigned int cnt;
	unsigned int bp; /* basis points : 0.01% */
};

struct all_cpu_idle {
	unsigned long long start_us;
	struct all_cpu_idle_data lv[CPU_OFF_MAX_LV];
};

static struct all_cpu_idle all_core_off;

/* idle ratio profiling */
struct mtk_cpuidle_ratio {
	unsigned long long start_us;
	unsigned long long idle_time_ns[CPUIDLE_STATE_MAX];
	unsigned int bp[CPUIDLE_STATE_MAX];
};

/* idle latency profiling */
struct mtk_cpuidle_prof {
	unsigned int cnt;
	unsigned int max;
	union {
		u64 avg;
		u64 sum;
	};
};

/* idle information */
struct mtk_cpuidle_info {
	u64 enter_time_ns;
	int idle_index;
	int cnt[CPUIDLE_STATE_MAX];
};

/* mtk cpu idle status */
struct mtk_cpuidle_device {
	struct mtk_cpuidle_ratio ratio;
	struct mtk_cpuidle_info info;
	struct mtk_cpuidle_prof prof;
	struct hrtimer timer;
	int cpu;
	int state_count;
	bool tmr_running;
};

static DEFINE_PER_CPU(struct mtk_cpuidle_device, mtk_cpuidle_dev);

/* mtk cpu idle configuration */
struct mtk_cpuidle_control {
	bool tmr_en;
	bool prof_en;
	bool log_en;
	bool stress_en;
};

static struct mtk_cpuidle_control mtk_cpuidle_ctrl;

static int mtk_cpuidle_stress_task(void *arg)
{
	while (mtk_cpuidle_ctrl.stress_en)
		usleep_range(timer_interval - 10, timer_interval + 10);

	return 0;
}

static void mtk_cpuidle_stress_start(void)
{
	int i;
	char name[20] = {0};
	int ret = 0;

	if (mtk_cpuidle_ctrl.stress_en)
		return;

	mtk_cpuidle_ctrl.stress_en = true;

	for_each_online_cpu(i) {
		ret = scnprintf(name, sizeof(name), "mtk_cpupm_stress_%d", i);
		stress_tsk[i] =
			kthread_create(mtk_cpuidle_stress_task, NULL, name);

		if (!IS_ERR(stress_tsk[i])) {
			kthread_bind(stress_tsk[i], i);
			wake_up_process(stress_tsk[i]);
		}
	}
}

static void mtk_cpuidle_stress_stop(void)
{
	mtk_cpuidle_ctrl.stress_en = false;
	msleep(20);
}

void mtk_cpuidle_set_stress_test(bool en)
{
	if (en)
		mtk_cpuidle_stress_start();
	else
		mtk_cpuidle_stress_stop();
}

bool mtk_cpuidle_get_stress_status(void)
{
	return mtk_cpuidle_ctrl.stress_en;
}

void mtk_cpuidle_set_stress_time(unsigned int val)
{
	timer_interval = clamp_val(val, 100, 20000);
}

unsigned int mtk_cpuidle_get_stress_time(void)
{
	return timer_interval;
}

void mtk_cpuidle_ctrl_timer_en(bool enable)
{
	mtk_cpupm_block();
	mtk_cpuidle_ctrl.tmr_en = enable;
	mtk_cpupm_allow();
}

bool mtk_cpuidle_ctrl_timer_sta_get(void)
{
	return mtk_cpuidle_ctrl.tmr_en;
}

void mtk_cpuidle_ctrl_log_en(bool enable)
{
	mtk_cpupm_block();
	mtk_cpuidle_ctrl.log_en = enable;
	mtk_cpupm_allow();
}

bool mtk_cpuidle_ctrl_log_sta_get(void)
{
	return mtk_cpuidle_ctrl.log_en;
}

static void _mtk_cpuidle_prof_ratio_start(void *data)
{
	int i, cpu;
	struct mtk_cpuidle_device *mtk_idle;

	cpu = get_cpu();
	mtk_idle = &per_cpu(mtk_cpuidle_dev, cpu);
	put_cpu();

	if (unlikely(!mtk_idle))
		return;

	mtk_idle->ratio.start_us = div64_u64(sched_clock(), 1000);

	for (i = 0; i < mtk_idle->state_count; i++)
		mtk_idle->ratio.idle_time_ns[i] = 0;
}

void mtk_cpuidle_prof_ratio_start(void)
{
	int i;

	if (mtk_cpuidle_ctrl.prof_en)
		return;

	mtk_cpupm_block();

	for (i = 0; i < CPU_OFF_MAX_LV; i++) {
		all_core_off.lv[i].dur_ns = 0;
		all_core_off.lv[i].cnt = 0;
	}

	all_core_off.start_us = div64_u64(sched_clock(), 1000);

	on_each_cpu(_mtk_cpuidle_prof_ratio_start, NULL, 1);

	mtk_cpuidle_ctrl.prof_en = true;

	mtk_cpupm_allow();
}

static void _mtk_cpuidle_prof_ratio_stop(void *stop_time)
{
	int i, cpu;
	uint64_t dur_us;
	struct mtk_cpuidle_device *mtk_idle;

	cpu = get_cpu();
	mtk_idle = &per_cpu(mtk_cpuidle_dev, cpu);
	put_cpu();

	if (unlikely(!mtk_idle))
		return;

	dur_us = div64_u64(sched_clock(), 1000) -
				mtk_idle->ratio.start_us;

	for (i = 0; i < mtk_idle->state_count; i++)
		mtk_idle->ratio.bp[i] = (unsigned int)div64_u64(
				mtk_idle->ratio.idle_time_ns[i] * 10,
				dur_us);
}

void mtk_cpuidle_prof_ratio_stop(void)
{
	int i;
	uint64_t time_us;

	if (!mtk_cpuidle_ctrl.prof_en)
		return;

	mtk_cpupm_block();

	on_each_cpu(_mtk_cpuidle_prof_ratio_stop, NULL, 1);

	mtk_cpuidle_ctrl.prof_en = false;

	time_us = div64_u64(sched_clock(), 1000) - all_core_off.start_us;

	for (i = 0; i < CPU_OFF_MAX_LV; i++)
		all_core_off.lv[i].bp = (unsigned int)div64_u64(
			all_core_off.lv[i].dur_ns * 10, time_us);

	mtk_cpupm_allow();
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

	seq_puts(m, "All core off ratio :\n");

	for (i = 0; i < CPU_OFF_MAX_LV; i++)
		seq_printf(m, "\t C_%d = %3u.%02u%%\n",
				i + 1,
				all_core_off.lv[i].bp / 100,
				all_core_off.lv[i].bp % 100);
}

void mtk_cpuidle_state_enable(bool en)
{
	struct cpuidle_driver *drv;
	int i, cpu;
	int suspend_type = mtk_lpm_suspend_type_get();

	mtk_cpupm_block();

	for_each_possible_cpu(cpu) {

		drv = cpuidle_get_cpu_driver(per_cpu(cpuidle_devices, cpu));

		if (!drv)
			continue;

		for (i = drv->state_count - 1; i > 0; i--) {
			if ((suspend_type == MTK_LPM_SUSPEND_S2IDLE) &&
			    !strcmp(drv->states[i].name, S2IDLE_STATE_NAME))
				continue;

			mtk_cpuidle_set_param(drv, i, IDLE_PARAM_EN, en);
		}
	}

	if (!en)
		mtk_lpm_last_cpuidle_dis = sched_clock();

	mtk_cpupm_allow();
}

unsigned long long mtk_cpuidle_state_last_dis_ms(void)
{
#if BITS_PER_LONG == 32
	return div_u64(mtk_lpm_last_cpuidle_dis, 1000000);
#else
	return (mtk_lpm_last_cpuidle_dis / 1000000);
#endif
}

static bool mtk_cpuidle_need_dump(unsigned int idx)
{
	u64 curr_ns;
	bool dump = false;

	if (!mtk_cpuidle_ctrl.log_en || !idx)
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
	unsigned int cnt = mtk_cpupm_mcusys_read(CPC_DORMANT_COUNTER);

	/**
	 * Cluster off count
	 * bit[0:15] : memory retention
	 * bit[16:31] : memory off
	 */
	if ((cnt & 0x7FFF) == 0)
		cnt = ((cnt >> 16) & 0x7FFF);
	else
		cnt = cnt & 0x7FFF;

	cnt += mtk_cpupm_syssram_read(SYSRAM_CPC_CPUSYS_CNT_BACKUP);

	cpc_cluster_cnt_clr();

	mtk_cpupm_syssram_write(SYSRAM_CPC_CPUSYS_CNT_BACKUP, 0);

	/**
	 * Add mcusys off count because CPC cluster counter will be cleared
	 * when mcusys power off.
	 */
	cnt += mtk_cpupm_syssram_read(SYSRAM_MCUPM_MCUSYS_COUNTER);

	mtk_cpupm_syssram_write(SYSRAM_CPUSYS_CNT,
			mtk_cpupm_syssram_read(SYSRAM_CPUSYS_CNT) + cnt);

	return cnt;
}

static unsigned int mtk_cpuidle_get_mcusys_off_cnt(void)
{
	unsigned int cnt = mtk_cpupm_syssram_read(SYSRAM_MCUPM_MCUSYS_COUNTER);

	mtk_cpupm_syssram_write(SYSRAM_MCUPM_MCUSYS_COUNTER, 0);

	mtk_cpupm_syssram_write(SYSRAM_MCUSYS_CNT,
			mtk_cpupm_syssram_read(SYSRAM_MCUSYS_CNT) + cnt);
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

		mtk_cpupm_syssram_write(ofs, 0);

		for (idx = 1; idx < mtk_idle->state_count ; idx++) {

			mtk_cpupm_syssram_write(ofs, mtk_cpupm_syssram_read(ofs)
						+ mtk_idle->info.cnt[idx]);
			mtk_idle->info.cnt[idx] = 0;
		}
	}

	/* Should calculate cluster off count before mcusys counter reset */
	mtk_cpupm_syssram_write(SYSRAM_RECENT_CPUSYS_CNT,
				mtk_cpuidle_get_cluster_off_cnt());

	mtk_cpupm_syssram_write(SYSRAM_RECENT_MCUSYS_CNT,
				mtk_cpuidle_get_mcusys_off_cnt());

	mtk_cpupm_syssram_write(SYSRAM_CPU_ONLINE, avail_cpu_mask);

	mtk_cpupm_syssram_write(SYSRAM_RECENT_CNT_TS_H,
			(unsigned int)((last_dump_ns >> 32) & 0xFFFFFFFF));

	mtk_cpupm_syssram_write(SYSRAM_RECENT_CNT_TS_L,
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

static void mtk_cpuidle_set_last_off_ts(int idx)
{
	int i;
	uint64_t curr_ns = sched_clock();

	if (!idx)
		return;

	if (idx > CPU_OFF_MAX_LV)
		idx = CPU_OFF_MAX_LV;

	for (i = 0; i < idx; i++) {

		all_core_off.lv[i].cnt++;

		if (all_core_off.lv[i].cnt == num_online_cpus())
			all_core_off.lv[i].last_ns = curr_ns;
	}
}

static void mtk_cpuidle_save_idle_time(int idx)
{
	int i;
	uint64_t curr_ns = sched_clock();

	if (!idx)
		return;

	if (idx > CPU_OFF_MAX_LV)
		idx = CPU_OFF_MAX_LV;

	for (i = 0; i < idx; i++) {

		if (all_core_off.lv[i].cnt == num_online_cpus())
			all_core_off.lv[i].dur_ns += (curr_ns
						- all_core_off.lv[i].last_ns);

		all_core_off.lv[i].cnt--;
	}
}

static int mtk_cpuidle_status_update(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct mtk_cpuidle_device *mtk_idle;
	struct mtk_lpm_nb_data *nb_data = (struct mtk_lpm_nb_data *)data;

	if (action & MTK_LPM_NB_BEFORE_REFLECT) {

		/* prevent race conditions by mtk_lp_mod_locker */
		if (mtk_cpuidle_ctrl.prof_en)
			mtk_cpuidle_save_idle_time(nb_data->index);

		if (mtk_cpuidle_need_dump(nb_data->index))
			mtk_cpuidle_dump_info();

	} else if (action & MTK_LPM_NB_RESUME) {

/* 		aee_rr_rec_mcdi_val(nb_data->cpu,
				(nb_data->index << 16) | 0x0); */
		mtk_idle = &per_cpu(mtk_cpuidle_dev, nb_data->cpu);
		mtk_idle->info.idle_index = -1;
		mtk_idle->info.cnt[nb_data->index]++;

		if (mtk_cpuidle_ctrl.prof_en)
			mtk_idle->ratio.idle_time_ns[nb_data->index] +=
				(sched_clock() - mtk_idle->info.enter_time_ns);

		mtk_cpuidle_cancel_timer(mtk_idle);

	} else if (action & MTK_LPM_NB_AFTER_PROMPT) {

		/* prevent race conditions by mtk_lp_mod_locker */
		if (mtk_cpuidle_ctrl.prof_en)
			mtk_cpuidle_set_last_off_ts(nb_data->index);

	} else if (action & MTK_LPM_NB_PREPARE) {

		mtk_idle = &per_cpu(mtk_cpuidle_dev, nb_data->cpu);
		mtk_idle->info.idle_index = nb_data->index;
		mtk_idle->info.enter_time_ns = sched_clock();

		mtk_cpuidle_set_timer(mtk_idle);

/* 		aee_rr_rec_mcdi_val(nb_data->cpu,
				(nb_data->index << 16) | 0xff); */
	}

	return NOTIFY_OK;
}

struct notifier_block mtk_cpuidle_status_nb = {
	.notifier_call = mtk_cpuidle_status_update,
};

static void mtk_cpuidle_init_per_cpu(void *info)
{
	struct mtk_cpuidle_device *mtk_idle;
	int cpu;

	cpu = get_cpu();

	mtk_idle = &per_cpu(mtk_cpuidle_dev, cpu);

	put_cpu();

	hrtimer_init(&mtk_idle->timer,
		CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	mtk_idle->cpu = cpu;
	mtk_idle->timer.function = mtk_cpuidle_hrtimer_func;
	mtk_idle->state_count = mtk_cpupm_get_idle_state_count(cpu);
}

int __init mtk_cpuidle_status_init(void)
{
	mtk_cpuidle_ctrl.tmr_en = true;
	mtk_cpuidle_ctrl.prof_en = false;
	mtk_cpuidle_ctrl.log_en = true;
	mtk_cpuidle_ctrl.stress_en = false;

	mtk_cpupm_block();
	on_each_cpu(mtk_cpuidle_init_per_cpu, NULL, 0);
	mtk_cpupm_allow();

	mtk_lpm_notifier_register(&mtk_cpuidle_status_nb);
	return 0;
}

void __exit mtk_cpuidle_status_exit(void)
{
	mtk_lpm_notifier_unregister(&mtk_cpuidle_status_nb);
}


