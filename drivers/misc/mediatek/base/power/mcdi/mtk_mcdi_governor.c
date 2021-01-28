// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/math64.h>
#include <linux/pm_qos.h>
#include <linux/sched/clock.h>
#include <linux/spinlock.h>
#include <linux/timekeeping.h>
#include <linux/tick.h>
#include <mtk_idle.h>

#include <mtk_mcdi.h>
#include <mtk_mcdi_governor.h>
#include <mtk_mcdi_governor_lib.h>
#include <mtk_mcdi_util.h>
#include <mtk_mcdi_cpc.h>

#include <mtk_mcdi_plat.h>
#include <mtk_mcdi_reg.h>
#include <mtk_mcdi_state.h>
#include <mtk_mcdi_profile.h>

/* #include <trace/events/mtk_idle_event.h> */

#define BOOT_TIME_LIMIT             10      /* sec */
#define TMR_RESIDENCY_US           200

#define GET_STATE_RES(cpu, state) ({					\
		unsigned int res = 0;					\
									\
		if ((state) < NF_MCDI_STATE)				\
			res = (mcdi_state_tbl_get(cpu))			\
				->states[state].target_residency;	\
		res;							\
	})

static int last_core_token = -1;
static int core_cluster_off_token[NF_CLUSTER];
static int last_cpu_enter;
static int last_cpu_enter_in_cluster[NF_CLUSTER];
static int boot_time_check;

struct mcdi_status {
	bool valid;
	int state;
	unsigned int predict_us;
	unsigned int next_timer_us;
	unsigned long long enter_time_us;
};

struct mcdi_gov {
	int num_mcusys;
	int num_cluster[NF_CLUSTER];
	unsigned int avail_cpu_mask;
	unsigned int avail_cluster_mask;
	int avail_cnt_mcusys;
	int avail_cnt_cluster[NF_CLUSTER];
	struct mcdi_status status[NF_CPU];
};

struct all_cpu_idle {
	unsigned int refcnt;
	unsigned long long enter_tick;
	unsigned long long leave_tick;
	unsigned int dur;
	unsigned int window_len;
	int thd_percent;
};

static unsigned long any_core_cpu_cond_info[NF_ANY_CORE_CPU_COND_INFO];
static DEFINE_SPINLOCK(any_core_cpu_cond_spin_lock);

static struct mcdi_feature_status mcdi_feature_stat;
static DEFINE_SPINLOCK(mcdi_feature_stat_spin_lock);

static struct mcdi_cluster_dev mcdi_cluster;
static DEFINE_SPINLOCK(mcdi_cluster_spin_lock);

static struct mcdi_gov mcdi_gov_data;

static DEFINE_SPINLOCK(mcdi_gov_spin_lock);

static struct pm_qos_request mcdi_qos_request;

static struct all_cpu_idle all_cpu_idle_data = {
	0,
	0,
	0,
	0,
	500000000, /* window     = 500 ms */
	85         /* threshold >=  85  % */
};

static DEFINE_SPINLOCK(all_cpu_idle_spin_lock);

unsigned int get_menu_next_timer_us(void)
{
	return 0;
}

unsigned int get_menu_predict_us(void)
{
	return 0;
}

int __attribute__((weak)) mtk_idle_select(int cpu)
{
	return -1;
}

unsigned int mcdi_get_boot_time_check(void)
{
	return boot_time_check;
}
unsigned int mcdi_get_gov_data_num_mcusys(void)
{
	unsigned long flags;
	unsigned int ret;

	spin_lock_irqsave(&mcdi_gov_spin_lock, flags);

	ret = mcdi_gov_data.num_mcusys;

	spin_unlock_irqrestore(&mcdi_gov_spin_lock, flags);

	return ret;
}

void set_mcdi_enable_by_pm_qos(bool en)
{
	s32 latency_req = en ? PM_QOS_DEFAULT_VALUE : 2;

	pm_qos_update_request(&mcdi_qos_request, latency_req);
}

void any_core_cpu_cond_inc(int idx)
{
	unsigned long flags;

	if (!(idx >= 0 && idx < NF_ANY_CORE_CPU_COND_INFO))
		return;

	spin_lock_irqsave(&any_core_cpu_cond_spin_lock, flags);

	any_core_cpu_cond_info[idx]++;

	spin_unlock_irqrestore(&any_core_cpu_cond_spin_lock, flags);
}

void any_core_cpu_cond_get(unsigned long buf[NF_ANY_CORE_CPU_COND_INFO])
{
	unsigned long flags;
	int i;

	spin_lock_irqsave(&any_core_cpu_cond_spin_lock, flags);

	for (i = 0; i < NF_ANY_CORE_CPU_COND_INFO; i++)
		buf[i] = any_core_cpu_cond_info[i];

	spin_unlock_irqrestore(&any_core_cpu_cond_spin_lock, flags);
}

static inline bool is_anycore_dpidle_sodi_state(int state)
{
	return (state == MCDI_STATE_DPIDLE
			|| state == MCDI_STATE_SODI3
			|| state == MCDI_STATE_SODI);
}

static inline struct mcdi_status *get_mcdi_status(int cpu)
{
	return &mcdi_gov_data.status[cpu];
}

void set_mcdi_idle_state(int cpu, int state)
{
	get_mcdi_status(cpu)->state = state;

	if (state == MCDI_STATE_CLUSTER_OFF
		&& cpu != core_cluster_off_token[cluster_idx_get(cpu)])
		state = MCDI_STATE_CPU_OFF;

	/* Save a real entered state */
	mcdi_prof_set_idle_state(cpu, state);
}

int get_cluster_off_token(cpu)
{
	unsigned long flags;
	int cluster;
	int token;

	spin_lock_irqsave(&mcdi_gov_spin_lock, flags);

	cluster = cluster_idx_get(cpu);
	token = core_cluster_off_token[cluster];
	core_cluster_off_token[cluster] = -1;

	spin_unlock_irqrestore(&mcdi_gov_spin_lock, flags);

	return token;
}

static enum hrtimer_restart mcdi_hrtimer_func(struct hrtimer *timer)
{
	unsigned long flags;

	spin_lock_irqsave(&mcdi_cluster_spin_lock, flags);

	mcdi_cluster.tmr_running = false;
	mcdi_cluster.owner = -1;

	spin_unlock_irqrestore(&mcdi_cluster_spin_lock, flags);

	return HRTIMER_NORESTART;
}

static void mcdi_set_timer(int cpu)
{
	unsigned int time_us, thresh;
	unsigned long flags;

	if (!mcdi_cluster.tmr_en)
		return;

	if (!tick_nohz_tick_stopped())
		return;

	thresh = get_mcdi_status(cpu)->next_timer_us
			- get_mcdi_status(cpu)->predict_us;

	if (thresh < GET_STATE_RES(cpu, MCDI_STATE_CPU_OFF))
		return;

	time_us = GET_STATE_RES(cpu, MCDI_STATE_CLUSTER_OFF)
			+ TMR_RESIDENCY_US;

	if (time_us > get_mcdi_status(cpu)->next_timer_us)
		return;

	tick_broadcast_exit();

	spin_lock_irqsave(&mcdi_cluster_spin_lock, flags);

	mcdi_cluster.tmr_running = true;
	mcdi_cluster.owner = cpu;

	RCU_NONIDLE(hrtimer_start(&mcdi_cluster.timer,
			ns_to_ktime(time_us * NSEC_PER_USEC),
			HRTIMER_MODE_REL_PINNED));

	spin_unlock_irqrestore(&mcdi_cluster_spin_lock, flags);

	tick_broadcast_enter();
}

static void mcdi_cancel_timer(int cpu)
{
	unsigned long flags;

	if (!mcdi_cluster.tmr_en)
		return;

	if (mcdi_cluster.owner != cpu)
		return;

	spin_lock_irqsave(&mcdi_cluster_spin_lock, flags);

	if (mcdi_cluster.tmr_running) {
		mcdi_cluster.tmr_running = false;
		mcdi_cluster.owner = -1;
		RCU_NONIDLE(hrtimer_try_to_cancel(&mcdi_cluster.timer));
	}

	spin_unlock_irqrestore(&mcdi_cluster_spin_lock, flags);
}

static bool remain_sleep_residency_allowable(unsigned int cpu_mask, int state)
{
	int i;
	unsigned long long curr_time_us;
	unsigned long long remain_sleep_us;
	unsigned int target_residency;
	unsigned long flags;
	struct mcdi_status *sta = NULL;

	if (!mcdi_cluster.chk_res_each_core)
		return true;

	mcdi_cluster.chk_res_cnt++;

	curr_time_us = idle_get_current_time_us();

	spin_lock_irqsave(&mcdi_cluster_spin_lock, flags);

	for (i = 0; i < NF_CPU; i++) {

		if (!(cpu_mask & (1 << i)))
			continue;

		target_residency = GET_STATE_RES(i, state);
		sta = &mcdi_gov_data.status[i];

		if (mcdi_cluster.use_max_remain) {
			remain_sleep_us = sta->next_timer_us
					- (curr_time_us - sta->enter_time_us);
		} else {
			/**
			 * An inaccurate idle prediction might be too small
			 * to enter cluster off or more deeper idle state.
			 * Using these inaccurate predictions to calculate
			 * a remain sleep may cause governor always can not
			 * select cluster off state, so we just use the idle
			 * prediction.
			 */
			remain_sleep_us = sta->predict_us;
		}

		if (remain_sleep_us < target_residency) {

			mcdi_cluster.chk_res_fail++;
			spin_unlock_irqrestore(&mcdi_cluster_spin_lock, flags);
			return false;
		}
	}
	spin_unlock_irqrestore(&mcdi_cluster_spin_lock, flags);

	return true;
}

static bool other_core_idle_state_allowable(int cpu,
		unsigned int cpu_mask, bool (*is_last_core)(int))
{
	int i, state;

again:

	/* Check each CPU available controlled by MCDI */
	for (i = 0; i < NF_CPU; i++) {

		if (!(cpu_mask & (1 << i)))
			continue;

		state = get_mcdi_status(i)->state;

		/* Check CPU has decided entered state */
		if (state == -1)
			break;

		if (state < MCDI_STATE_CLUSTER_OFF)
			return false;
	}

	if (i != NF_CPU) {

		udelay(1);

		if (is_last_core(cpu))
			goto again;
		else
			return false;
	}

	return true;
}

bool is_last_core_in_mcusys(int cpu)
{
	int num_mcusys;
	int avail_cnt_mcusys;

	if (last_cpu_enter != cpu)
		return false;

	/* if other CPU(s) leave MCDI, means more than 1 CPU powered ON */
	num_mcusys = mcdi_gov_data.num_mcusys;
	avail_cnt_mcusys = mcdi_gov_data.avail_cnt_mcusys;

	return num_mcusys == avail_cnt_mcusys;
}

bool is_last_core_in_cluster(int cpu)
{
	int num_cluster;
	int avail_cnt_cluster;
	int cluster = cluster_idx_get(cpu);

	if (last_cpu_enter_in_cluster[cluster] != cpu)
		return false;

	/* if other CPU(s) leave MCDI, means more than 1 CPU powered ON */
	num_cluster = mcdi_gov_data.num_cluster[cluster];
	avail_cnt_cluster = mcdi_gov_data.avail_cnt_cluster[cluster];

	return num_cluster == avail_cnt_cluster;
}

void cluster_off_check(int cpu, int *state)
{
	unsigned int cpu_mask = 0;

	/* Check residency */
	if (get_mcdi_status(cpu)->predict_us
		< GET_STATE_RES(cpu, MCDI_STATE_CLUSTER_OFF)) {

		*state = MCDI_STATE_CPU_OFF;
		mcdi_set_timer(cpu);

		return;
	}

	cpu_mask = get_pwr_stat_check_map(ALL_CPU_IN_CLUSTER,
						cluster_idx_get(cpu));
	cpu_mask &= (mcdi_gov_data.avail_cpu_mask & ~(1 << cpu));

	if (!other_core_idle_state_allowable(
				cpu, cpu_mask, is_last_core_in_cluster))
		return;


	if (!remain_sleep_residency_allowable(
				cpu_mask, MCDI_STATE_CLUSTER_OFF)) {

		*state = MCDI_STATE_CPU_OFF;
		mcdi_set_timer(cpu);

		return;
	}

	if (acquire_cluster_last_core_prot(cpu) == 0) {

		mcdi_notify_cluster_off(cluster_idx_get(cpu));

		/* Token for profile mechanism */
		core_cluster_off_token[cluster_idx_get(cpu)] = cpu;
		mcdi_prof_core_cluster_off_token(cpu);
	}
}

bool any_core_deepidle_sodi_residency_check(int cpu)
{
	int spm_state;
	unsigned int cpu_mask = 0;

	spm_state = MCDI_STATE_CLUSTER_OFF + 1;

	if (get_mcdi_status(cpu)->predict_us
		< GET_STATE_RES(cpu, spm_state))
		return false;

	cpu_mask = mcdi_gov_data.avail_cpu_mask & ~(1 << cpu);

	if (!other_core_idle_state_allowable(
				cpu, cpu_mask, is_last_core_in_mcusys))
		return false;

	if (!remain_sleep_residency_allowable(cpu_mask, spm_state))
		return false;

	return true;
}

int any_core_deepidle_sodi_check(int cpu)
{
	int state;
	int mtk_idle_state;

	state = MCDI_STATE_CPU_OFF;

	/* Check residency */
	if (!any_core_deepidle_sodi_residency_check(cpu)) {

		/* trace_any_core_residency_rcuidle(cpu); */

		any_core_cpu_cond_inc(RESIDENCY_CNT);

		return state;
	}

	if (acquire_last_core_prot(cpu) != 0)
		return state;

	any_core_cpu_cond_inc(LAST_CORE_CNT);

	/* Check other deepidle/SODI criteria */
	mtk_idle_state = mtk_idle_select(cpu);

	state = mcdi_get_mcdi_idle_state(mtk_idle_state);

	if (!is_anycore_dpidle_sodi_state(state)) {
		release_last_core_prot();
		/* trace_mtk_idle_select_rcuidle(cpu, mtk_idle_state); */
	}

	return state;
}

bool is_mcdi_working(void)
{
	unsigned long flags;
	bool working = false;

	spin_lock_irqsave(&mcdi_feature_stat_spin_lock, flags);

	working = mcdi_feature_stat.enable && !mcdi_feature_stat.pause;

	spin_unlock_irqrestore(&mcdi_feature_stat_spin_lock, flags);

	return working;
}

/* Select deepidle/SODI/cluster OFF/CPU OFF/WFI */
int mcdi_governor_select(int cpu, int cluster_idx)
{
	unsigned long flags;
	int select_state = MCDI_STATE_WFI;
	bool last_core_in_mcusys = false;
	bool last_core_token_get = false;
	struct mcdi_status *mcdi_sta = NULL;
	struct cpuidle_driver *tbl = mcdi_state_tbl_get(cpu);
	int latency_req = pm_qos_request(PM_QOS_CPU_DMA_LATENCY);

	if (!is_mcdi_working())
		return MCDI_STATE_WFI;

	if (boot_time_check != 1) {
		struct timespec uptime;
		unsigned long val;

		get_monotonic_boottime(&uptime);
		val = (unsigned long)uptime.tv_sec;

		if (val >= BOOT_TIME_LIMIT) {
			boot_time_check = 1;
			mcdi_ap_ready();
			pr_info("MCDI bootup check: PASS\n");
		} else {
			return MCDI_STATE_WFI;
		}
	}

	if (!mcdi_usage_cpu_valid(cpu))
		return MCDI_STATE_WFI;

	spin_lock_irqsave(&mcdi_gov_spin_lock, flags);

	/* Need to update first for last core check */
	last_cpu_enter_in_cluster[cluster_idx] = cpu;
	last_cpu_enter = cpu;

	/* increase MCDI num (MCUSYS/cluster) */
	mcdi_gov_data.num_mcusys++;
	mcdi_gov_data.num_cluster[cluster_idx]++;

	/* Check if the last CPU in MCUSYS entering MCDI */
	last_core_in_mcusys = is_last_core_in_mcusys(cpu);

	/* update mcdi_status of this CPU */
	mcdi_sta = get_mcdi_status(cpu);

	mcdi_sta->valid         = true;
	mcdi_sta->enter_time_us = idle_get_current_time_us();
	mcdi_sta->predict_us    = get_menu_predict_us();
	mcdi_sta->next_timer_us = get_menu_next_timer_us();

	if (last_core_in_mcusys && last_core_token == -1) {
		last_core_token      = cpu;
		last_core_token_get  = true;
	}

	spin_unlock_irqrestore(&mcdi_gov_spin_lock, flags);

	/* Check if any core deepidle/SODI can entered */
	if (mcdi_feature_stat.any_core && last_core_token_get) {

		if (tbl->states[MCDI_STATE_CLUSTER_OFF + 1].exit_latency
				< latency_req) {

			/* trace_check_anycore_rcuidle(cpu, 1, -1); */

			select_state = any_core_deepidle_sodi_check(cpu);

			/* trace_check_anycore_rcuidle(cpu, 0, select_state); */

		} else {
			any_core_cpu_cond_inc(LATENCY_CNT);
		}

	}

	if (!is_anycore_dpidle_sodi_state(select_state)) {

		if (last_core_token_get) {

			spin_lock_irqsave(&mcdi_gov_spin_lock, flags);

			WARN_ON(last_core_token != cpu);

			last_core_token = -1;

			spin_unlock_irqrestore(&mcdi_gov_spin_lock, flags);
		}

		if (tbl->states[MCDI_STATE_CLUSTER_OFF].exit_latency
				< latency_req
			&& mcdi_feature_stat.cluster_off) {

			select_state = MCDI_STATE_CLUSTER_OFF;

			if (is_last_core_in_cluster(cpu))
				cluster_off_check(cpu, &select_state);

		} else {
			select_state = MCDI_STATE_CPU_OFF;
		}
	}

	return select_state;
}

void mcdi_governor_reflect(int cpu, int state)
{
	unsigned long flags;
	int cluster_idx = cluster_idx_get(cpu);
	struct mcdi_status *mcdi_sta = NULL;

	mcdi_cpc_reflect(cpu, last_core_token);

	/* decrease MCDI num (MCUSYS/cluster) */
	spin_lock_irqsave(&mcdi_gov_spin_lock, flags);

	if (state == MCDI_STATE_CPU_OFF
			|| state == MCDI_STATE_CLUSTER_OFF
			|| state == MCDI_STATE_SODI
			|| state == MCDI_STATE_DPIDLE
			|| state == MCDI_STATE_SODI3) {

		mcdi_gov_data.num_mcusys--;
		mcdi_gov_data.num_cluster[cluster_idx]--;
	}

	mcdi_sta = get_mcdi_status(cpu);

	mcdi_sta->valid         = false;
	mcdi_sta->state         = -1;
	mcdi_sta->enter_time_us = 0;
	mcdi_sta->predict_us    = 0;

	if (last_core_token == cpu)
		last_core_token = -1;

	spin_unlock_irqrestore(&mcdi_gov_spin_lock, flags);

	if (is_anycore_dpidle_sodi_state(state))
		release_last_core_prot();
	else if (state == MCDI_STATE_CLUSTER_OFF)
		release_cluster_last_core_prot();

	mcdi_cancel_timer(cpu);
}

void mcdi_avail_cpu_cluster_update(void)
{
	unsigned long flags;
	int i, cpu, cluster_idx;
	unsigned int cpu_mask = 0;

	spin_lock_irqsave(&mcdi_gov_spin_lock, flags);

	mcdi_gov_data.avail_cnt_mcusys = num_online_cpus();
	mcdi_gov_data.avail_cpu_mask = 0;
	mcdi_gov_data.avail_cluster_mask = 0;

	for (i = 0; i < NF_CLUSTER; i++)
		mcdi_gov_data.avail_cnt_cluster[i] = 0;

	for (cpu = 0; cpu < NF_CPU; cpu++) {

		cluster_idx = cluster_idx_get(cpu);

		if (cpu_online(cpu)) {
			mcdi_gov_data.avail_cnt_cluster[cluster_idx]++;
			mcdi_gov_data.avail_cpu_mask |= (1 << cpu);
		}
	}

	for (cluster_idx = 0; cluster_idx < NF_CLUSTER; cluster_idx++) {
		if ((mcdi_gov_data.avail_cpu_mask
				& get_pwr_stat_check_map(ALL_CPU_IN_CLUSTER,
								cluster_idx)))
			mcdi_gov_data.avail_cluster_mask |= (1 << cluster_idx);
	}

	cpu_mask = mcdi_gov_data.avail_cpu_mask;

	spin_unlock_irqrestore(&mcdi_gov_spin_lock, flags);

	mcdi_avail_cpu_mask(cpu_mask);
}

void set_mcdi_enable_status(bool enabled)
{
	unsigned long flags;

	spin_lock_irqsave(&mcdi_feature_stat_spin_lock, flags);

	mcdi_feature_stat.enable = enabled;

	spin_unlock_irqrestore(&mcdi_feature_stat_spin_lock, flags);

	set_mcdi_enable_by_pm_qos(enabled);

	/* if disabled, wakeup all cpus */
	if (!enabled)
		mcdi_wakeup_all_cpu();
}

void set_mcdi_s_state(int state)
{
	unsigned long flags;
	bool cluster_off = false;
	bool any_core = false;

	if (!(state >= MCDI_STATE_CPU_OFF && state <= MCDI_STATE_SODI3))
		return;

	switch (state) {
	case MCDI_STATE_CPU_OFF:
		cluster_off = false;
		any_core    = false;
		break;
	case MCDI_STATE_CLUSTER_OFF:
		cluster_off = true;
		any_core    = false;
		break;
	case MCDI_STATE_SODI:
	case MCDI_STATE_DPIDLE:
	case MCDI_STATE_SODI3:
		cluster_off = true;
		any_core    = true;
		break;
	default:
		return;
	}

	pr_info("%s = %d\n", __func__, state);

	spin_lock_irqsave(&mcdi_feature_stat_spin_lock, flags);

	mcdi_feature_stat.s_state     = state;
	mcdi_feature_stat.cluster_off = cluster_off;
	mcdi_feature_stat.any_core    = any_core;

	spin_unlock_irqrestore(&mcdi_feature_stat_spin_lock, flags);
}

static void mcdi_cluster_init(void)
{
	mcdi_cluster.timer.function = mcdi_hrtimer_func;
	mcdi_cluster.use_max_remain = true;
	mcdi_cluster.owner = -1;
	mcdi_cluster.chk_res_each_core = false;

	if (mcdi_is_cpc_mode())
		mcdi_cluster.tmr_en = true;
	else
		mcdi_cluster.tmr_en = false;

	hrtimer_init(&mcdi_cluster.timer,
			CLOCK_MONOTONIC, HRTIMER_MODE_REL);
}

void mcdi_governor_init(void)
{
	unsigned long flags;
	int i;

	pm_qos_add_request(&mcdi_qos_request,
		PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);

	mcdi_avail_cpu_cluster_update();

	spin_lock_irqsave(&mcdi_gov_spin_lock, flags);

	mcdi_gov_data.num_mcusys = 0;

	for (i = 0; i < NF_CLUSTER; i++) {
		mcdi_gov_data.num_cluster[i] = 0;
		last_cpu_enter_in_cluster[i] = -1;
		core_cluster_off_token[i] = -1;
	}

	for (i = 0; i < NF_CPU; i++) {
		mcdi_gov_data.status[i].state         = 0;
		mcdi_gov_data.status[i].enter_time_us = 0;
		mcdi_gov_data.status[i].predict_us    = 0;
	}

	spin_unlock_irqrestore(&mcdi_gov_spin_lock, flags);

	mcdi_status_init();

	set_mcdi_s_state(MCDI_STATE_SODI3);

	/* Note: [DVT] Select mcdi state w/o mcdi enable
	 * Include mtk_idle.h for MTK_IDLE_DVT_TEST_ONLY
	 */
	#if defined(MTK_IDLE_DVT_TEST_ONLY)
	set_mcdi_enable_by_pm_qos(true);
	#endif

	mcdi_cluster_init();
}

const struct mcdi_feature_status *get_mcdi_feature_stat(void)
{
	return &mcdi_feature_stat;
}

struct mcdi_cluster_dev *get_mcdi_cluster_dev(void)
{
	return &mcdi_cluster;
}

void get_mcdi_feature_status(struct mcdi_feature_status *stat)
{
	unsigned long flags;

	spin_lock_irqsave(&mcdi_feature_stat_spin_lock, flags);

	stat->enable      = mcdi_feature_stat.enable;
	stat->pause       = mcdi_feature_stat.pause;
	stat->cluster_off = mcdi_feature_stat.cluster_off;
	stat->any_core    = mcdi_feature_stat.any_core;
	stat->s_state     = mcdi_feature_stat.s_state;
	stat->pauseby     = mcdi_feature_stat.pauseby;

	spin_unlock_irqrestore(&mcdi_feature_stat_spin_lock, flags);
}

void get_mcdi_avail_mask(unsigned int *cpu_mask, unsigned int *cluster_mask)
{
	unsigned long flags;

	spin_lock_irqsave(&mcdi_gov_spin_lock, flags);

	*cpu_mask     = mcdi_gov_data.avail_cpu_mask;
	*cluster_mask = mcdi_gov_data.avail_cluster_mask;

	spin_unlock_irqrestore(&mcdi_gov_spin_lock, flags);
}

void mcdi_state_pause(unsigned int id, bool pause)
{
	unsigned long flags;

	spin_lock_irqsave(&mcdi_feature_stat_spin_lock, flags);

	if (pause)
		mcdi_feature_stat.pauseby |= (1 << id);
	else
		mcdi_feature_stat.pauseby &= ~(1 << id);

	mcdi_feature_stat.pause = !!mcdi_feature_stat.pauseby;

	spin_unlock_irqrestore(&mcdi_feature_stat_spin_lock, flags);

	if (mcdi_feature_stat.enable)
		set_mcdi_enable_by_pm_qos(!pause);
}

void idle_refcnt_inc(void)
{
	unsigned long flags;
	bool enter = false;

	spin_lock_irqsave(&all_cpu_idle_spin_lock, flags);

	all_cpu_idle_data.refcnt++;

	if (all_cpu_idle_data.refcnt == mcdi_gov_data.avail_cnt_mcusys) {
		enter = true;
		all_cpu_idle_data.enter_tick = sched_clock();
	}

	spin_unlock_irqrestore(&all_cpu_idle_spin_lock, flags);

	/* if (enter) */
		/* trace_all_cpu_idle_rcuidle(1); */
}

void idle_refcnt_dec(void)
{
	unsigned long flags;
	unsigned long long leave_tick;
	unsigned long long this_dur;
	unsigned long long temp;
	bool leave = false;

	spin_lock_irqsave(&all_cpu_idle_spin_lock, flags);

	all_cpu_idle_data.refcnt--;

	if (all_cpu_idle_data.refcnt == (mcdi_gov_data.avail_cnt_mcusys - 1)) {
		leave = true;
		leave_tick = sched_clock();

		this_dur = leave_tick - all_cpu_idle_data.enter_tick;

		if (((leave_tick - all_cpu_idle_data.leave_tick)
			> all_cpu_idle_data.window_len)) {
			all_cpu_idle_data.dur = this_dur;
		} else {
			temp =  all_cpu_idle_data.window_len;
			temp -= (leave_tick - all_cpu_idle_data.leave_tick);
			temp *= all_cpu_idle_data.dur;
			temp =  div64_u64(temp, all_cpu_idle_data.window_len);
			temp += this_dur;

			all_cpu_idle_data.dur = (unsigned int)temp;
		}

		all_cpu_idle_data.leave_tick = leave_tick;
	}

	spin_unlock_irqrestore(&all_cpu_idle_spin_lock, flags);

	/* if (leave) */
		/* trace_all_cpu_idle_rcuidle(0); */
}

int all_cpu_idle_ratio_get(void)
{
	unsigned long flags;
	unsigned long long curr_tick = 0;
	unsigned long long target_tick = 0;
	int all_idle_ratio = 0;
	unsigned long long leave_tick = 0;
	unsigned long long window_len = 0;
	unsigned long long dur = 0;

	spin_lock_irqsave(&all_cpu_idle_spin_lock, flags);

	leave_tick = all_cpu_idle_data.leave_tick;
	window_len = all_cpu_idle_data.window_len;
	dur        = all_cpu_idle_data.dur;

	spin_unlock_irqrestore(&all_cpu_idle_spin_lock, flags);


	curr_tick = sched_clock();

	target_tick =
		((curr_tick - leave_tick) > window_len) ?
			0 :
			div64_u64(
				((window_len - (curr_tick - leave_tick)) * dur),
				window_len
			)
		;

	target_tick = target_tick <= window_len ? target_tick : window_len;

	all_idle_ratio = (int)div64_u64(target_tick * 100, window_len);

	return all_idle_ratio;
}

bool is_all_cpu_idle_criteria(void)
{
	int all_idle_ratio = 0;
	unsigned long long start_tick = 0;
	unsigned long long end_tick = 0;
	unsigned long long thd_percent = 0;

	thd_percent = all_cpu_idle_data.thd_percent;

	start_tick = sched_clock();

	all_idle_ratio = all_cpu_idle_ratio_get();

	end_tick = sched_clock();

	/* trace_mtk_menu_rcuidle(smp_processor_id(), */
		/* all_idle_ratio, (int)(end_tick - start_tick)); */

	return (all_idle_ratio >= thd_percent);
}
