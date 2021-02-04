/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <mach/mtk_gpt.h>
#include "mtk_cpufreq.h"
#include "mtk_idle_profile.h"
#include "mtk_spm_resource_req_internal.h"

#if SPM_MET_TAGGING
#include <core/met_drv.h>
#endif

#define IDLE_PROF_TAG     "Power/swap "
#define idle_prof_emerg(fmt, args...)   pr_notice(IDLE_PROF_TAG fmt, ##args)
#define idle_prof_alert(fmt, args...)   pr_notice(IDLE_PROF_TAG fmt, ##args)
#define idle_prof_crit(fmt, args...)    pr_notice(IDLE_PROF_TAG fmt, ##args)
#define idle_prof_err(fmt, args...)     pr_notice(IDLE_PROF_TAG fmt, ##args)
#define idle_prof_warn(fmt, args...)	pr_notice(IDLE_PROF_TAG fmt, ##args)
#define idle_prof_notice(fmt, args...)  pr_notice(IDLE_PROF_TAG fmt, ##args)
#define idle_prof_info(fmt, args...)    pr_debug(IDLE_PROF_TAG fmt, ##args)
#define idle_prof_ver(fmt, args...)     pr_debug(IDLE_PROF_TAG fmt, ##args)
#define idle_prof_dbg(fmt, args...)     pr_debug(IDLE_PROF_TAG fmt, ##args)


/* idle ratio */
static bool idle_ratio_en;
static unsigned long long idle_ratio_profile_start_time;
static unsigned long long idle_ratio_profile_duration;
static unsigned long long idle_ratio_start_time[NR_TYPES];
static unsigned long long idle_ratio_value[NR_TYPES];

/* idle block information */
static unsigned long long idle_block_log_prev_time;
static unsigned int idle_block_log_time_criteria = 5000;	/* 5 sec */
static unsigned long long idle_cnt_dump_prev_time;
static unsigned int idle_cnt_dump_criteria = 5000;			/* 5 sec */

static struct mtk_idle_buf idle_log;

#define reset_log()              reset_idle_buf(idle_log)
#define get_log()                get_idle_buf(idle_log)
#define append_log(fmt, args...) idle_buf_append(idle_log, fmt, ##args)

static DEFINE_SPINLOCK(idle_cnt_spin_lock);

struct mtk_idle_block {
	u64 prev_time;
	u32 time_critera;
	unsigned long last_cnt[NR_CPUS];
	unsigned long *cnt;
	unsigned long *block_cnt;
	unsigned int *block_mask;
	char *name;
	bool  init;
};

static struct mtk_idle_block idle_block[NR_TYPES] = {
	[IDLE_TYPE_DP] = {
		.name = "dpidle",
		.time_critera = 30000, /* 30sec */
		.init = false,
	},
	[IDLE_TYPE_SO3] = {
		.name = "soidle3",
		.time_critera = 30000, /* 30sec */
		.init = false,
	},
	[IDLE_TYPE_SO] = {
		.name = "soidle",
		.time_critera = 30000, /* 30sec */
		.init = false,
	},
	[IDLE_TYPE_MC] = {
		.name = "mcidle",
		.init = false,
	},
	[IDLE_TYPE_SL] = {
		.name = "slidle",
		.init = false,
	},
	[IDLE_TYPE_RG] = {
		.name = "rgidle",
		.init = false,
	}
};

/* SPM TAWM */
#define TRIGGER_TYPE                (2) /* b'10: high */
#define TWAM_PERIOD_MS              (1000)
#define WINDOW_LEN_SPEED            (TWAM_PERIOD_MS * 0x65B8)
#define WINDOW_LEN_NORMAL           (TWAM_PERIOD_MS * 0xD)
#define GET_EVENT_RATIO_SPEED(x)    ((x)/(WINDOW_LEN_SPEED/1000))
#define GET_EVENT_RATIO_NORMAL(x)   ((x)/(WINDOW_LEN_NORMAL/1000))

struct mtk_idle_twam idle_twam;

#if SPM_MET_TAGGING
#define idle_get_current_time_us(x) do {\
		struct timeval t;\
		do_gettimeofday(&t);\
		(x) = ((t.tv_sec & 0xFFF) * 1000000 + t.tv_usec);\
	} while (0)

static unsigned long long idle_met_timestamp[NR_TYPES];
static unsigned long long idle_met_prev_tag[NR_TYPES];
static const char *idle_met_label[NR_TYPES] = {
	[IDLE_TYPE_DP] = "deep idle residency",
	[IDLE_TYPE_SO3] = "SODI3 residency",
	[IDLE_TYPE_SO] = "SODI residency",
};
#endif


#ifdef SPM_SODI3_PROFILE_TIME
unsigned int			soidle3_profile[4];
#endif
#ifdef SPM_SODI_PROFILE_TIME
unsigned int			soidle_profile[4];
#endif
#ifdef SPM_DEEPIDLE_PROFILE_TIME
unsigned int            dpidle_profile[4];
#endif

struct mtk_idle_twam *mtk_idle_get_twam(void)
{
	return &idle_twam;
}

void  dpidle_profile_time(int idx)
{
#ifdef SPM_DEEPIDLE_PROFILE_TIME
#ifdef IDLE_PROF_USING_STD_TIMER
	dpidle_profile[idx] = lower_32_bits(mtk_timer_get_cnt(2));
#else
	gpt_get_cnt(SPM_DEEPIDLE_PROFILE_APXGPT, &dpidle_profile[idx]);
#endif
#endif

}

void  soidle3_profile_time(int idx)
{
#ifdef SPM_SODI3_PROFILE_TIME
#ifdef IDLE_PROF_USING_STD_TIMER
	soidle3_profile[idx] = lower_32_bits(mtk_timer_get_cnt(2));
#else
	gpt_get_cnt(SPM_SODI3_PROFILE_APXGPT, &soidle3_profile[idx]);
#endif
#endif

}

void  soidle_profile_time(int idx)
{
#ifdef SPM_SODI_PROFILE_TIME
#ifdef IDLE_PROF_USING_STD_TIMER
	soidle_profile[idx] = lower_32_bits(mtk_timer_get_cnt(2));
#else
	gpt_get_cnt(SPM_SODI_PROFILE_APXGPT, &soidle_profile[idx]);
#endif
#endif
}

void dpidle_show_profile_time(void)
{
#ifdef SPM_DEEPIDLE_PROFILE_TIME
	idle_prof_ver("1:%u, 2:%u, 3:%u, 4:%u\n",
			dpidle_profile[0], dpidle_profile[1], dpidle_profile[2], dpidle_profile[3]);
#endif
}

void soidle3_show_profile_time(void)
{
#ifdef SPM_SODI3_PROFILE_TIME
	idle_prof_ver("SODI3: cpu_freq:%u/%u, 1=>2:%u, 2=>3:%u, 3=>4:%u\n",
			mt_cpufreq_get_cur_phy_freq_no_lock(MT_CPU_DVFS_LL),
			mt_cpufreq_get_cur_phy_freq_no_lock(MT_CPU_DVFS_L),
			((soidle3_profile[1] - soidle3_profile[0])*1000)/APXGPT_RTC_TICKS_PER_MS,
			((soidle3_profile[2] - soidle3_profile[1])*1000)/APXGPT_RTC_TICKS_PER_MS,
			((soidle3_profile[3] - soidle3_profile[2])*1000)/APXGPT_RTC_TICKS_PER_MS);
#endif
}

void soidle_show_profile_time(void)
{
#ifdef SPM_SODI_PROFILE_TIME
	idle_prof_ver("SODI: cpu_freq:%u/%u, 1=>2:%u, 2=>3:%u, 3=>4:%u\n",
			mt_cpufreq_get_cur_phy_freq_no_lock(MT_CPU_DVFS_LL),
			mt_cpufreq_get_cur_phy_freq_no_lock(MT_CPU_DVFS_L),
			(soidle_profile[1] - soidle_profile[0])/APXGPT_SYS_TICKS_PER_US,
			(soidle_profile[2] - soidle_profile[1])/APXGPT_SYS_TICKS_PER_US,
			(soidle_profile[3] - soidle_profile[2])/APXGPT_SYS_TICKS_PER_US);
#endif
}

void mtk_idle_twam_callback(struct twam_sig *ts)
{
	idle_prof_warn("spm twam %s ratio: %5u/1000\n",
			(idle_twam.str)?idle_twam.str[idle_twam.event]:"unknown",
			(idle_twam.speed_mode)?GET_EVENT_RATIO_SPEED(ts->sig0):GET_EVENT_RATIO_NORMAL(ts->sig0));
}

void mtk_idle_twam_disable(void)
{
	if (idle_twam.running == false)
		return;
	spm_twam_register_handler(NULL);
	spm_twam_disable_monitor();
	idle_twam.running = false;
}

void mtk_idle_twam_enable(u32 event)
{
	struct twam_sig montype = {0};
	struct twam_sig twamsig = {0};

	if (idle_twam.event != event)
		mtk_idle_twam_disable();

	if (idle_twam.running == true)
		return;

	idle_twam.event = (event < 32)?event:29;
	twamsig.sig0 = idle_twam.event;
	montype.sig0 = TRIGGER_TYPE;

	spm_twam_set_mon_type(&montype);
	spm_twam_set_window_length((idle_twam.speed_mode)?WINDOW_LEN_SPEED:WINDOW_LEN_NORMAL);
	spm_twam_register_handler(mtk_idle_twam_callback);
	spm_twam_set_idle_select(0);
	spm_twam_enable_monitor(&twamsig, idle_twam.speed_mode);
	idle_twam.running = true;
}

void mtk_idle_ratio_calc_start(int type, int cpu)
{
	if (idle_ratio_en && type >= 0 && type < NR_TYPES && (cpu == 0 || cpu == 4))
		idle_ratio_start_time[type] = idle_get_current_time_ms();

#if SPM_MET_TAGGING
	if (type < IDLE_TYPE_MC)
		idle_get_current_time_us(idle_met_timestamp[type]);
#endif
}

void mtk_idle_ratio_calc_stop(int type, int cpu)
{
	if (idle_ratio_en && type >= 0 && type < NR_TYPES && (cpu == 0 || cpu == 4))
		idle_ratio_value[type] += idle_get_current_time_ms() - idle_ratio_start_time[type];

#if SPM_MET_TAGGING
	if (type < IDLE_TYPE_MC) {
		unsigned long long idle_met_curr;

		idle_get_current_time_us(idle_met_curr);

		met_tag_oneshot(0, idle_met_label[type],
			((idle_met_curr - idle_met_timestamp[type])*100)/(idle_met_curr - idle_met_prev_tag[type]));
		idle_met_prev_tag[type] = idle_met_curr;
	}
#endif
}

bool mtk_idle_get_ratio_status(void)
{
	return idle_ratio_en;
}

void mtk_idle_disable_ratio_calc(void)
{
	idle_ratio_en = false;
}

void mtk_idle_enable_ratio_calc(void)
{
	int idx;

	if (idle_ratio_en == false) {
		for (idx = 0; idx < NR_TYPES; idx++)
			idle_ratio_value[idx] = 0;
		idle_ratio_profile_start_time = idle_get_current_time_ms();
		idle_ratio_en = true;
	}
}

static void mtk_idle_dump_cnt(int type)
{
	static struct mtk_idle_buf buf;
	struct mtk_idle_block *p_idle;
	bool enter_idle = false;
	unsigned long idle_cnt;
	int i;

	p_idle = &idle_block[type];

	if (unlikely(p_idle->init == false)) {
		append_log("Not initialized --- ");
		return;
	}

	reset_idle_buf(buf);

	for (i = 0; i < nr_cpu_ids; i++) {
		idle_cnt = p_idle->cnt[i] - p_idle->last_cnt[i];
		if (idle_cnt != 0) {
			idle_buf_append(buf, "[%d] = %lu, ", i, idle_cnt);
			enter_idle = true;
		}
		p_idle->last_cnt[i] = p_idle->cnt[i];
	}

	if (enter_idle)
		append_log("%s --- ", get_idle_buf(buf));
	else
		append_log("No enter --- ");
}

void mtk_idle_dump_cnt_in_interval(void)
{
	int i = 0;
	unsigned long long idle_cnt_dump_curr_time = 0;
	unsigned long flags;
	bool dump_cnt_info = false;

	idle_cnt_dump_curr_time = idle_get_current_time_ms();

	if (idle_cnt_dump_prev_time == 0)
		idle_cnt_dump_prev_time = idle_cnt_dump_curr_time;

	spin_lock_irqsave(&idle_cnt_spin_lock, flags);

	dump_cnt_info = ((idle_cnt_dump_curr_time - idle_cnt_dump_prev_time) > idle_cnt_dump_criteria);

	/* update time base */
	if (dump_cnt_info)
		idle_cnt_dump_prev_time = idle_cnt_dump_curr_time;

	spin_unlock_irqrestore(&idle_cnt_spin_lock, flags);

	if (!dump_cnt_info)
		return;

	/* dump idle count */
	reset_log();

	append_log("DP: ");
	mtk_idle_dump_cnt(IDLE_TYPE_DP);

	append_log("SODI3: ");
	mtk_idle_dump_cnt(IDLE_TYPE_SO3);

	append_log("SODI: ");
	mtk_idle_dump_cnt(IDLE_TYPE_SO);

	/* dump log */
	idle_prof_warn("%s\n", get_log());

	/* dump idle ratio */
	if (idle_ratio_en) {
		idle_ratio_profile_duration = idle_get_current_time_ms() - idle_ratio_profile_start_time;
		idle_prof_warn("--- CPU 0 idle: %llu, DP = %llu, SO3 = %llu, SO = %llu, RG = %llu --- (ms)\n",
				idle_ratio_profile_duration,
				idle_ratio_value[IDLE_TYPE_DP],
				idle_ratio_value[IDLE_TYPE_SO3],
				idle_ratio_value[IDLE_TYPE_SO],
				idle_ratio_value[IDLE_TYPE_RG]);

		for (i = 0; i < NR_TYPES; i++)
			idle_ratio_value[i] = 0;
		idle_ratio_profile_start_time = idle_get_current_time_ms();
	}
}
static DEFINE_SPINLOCK(idle_blocking_spin_lock);

bool mtk_idle_state_pick(int type, int cpu, int reason)
{
	struct mtk_idle_buf idle_state_log;
	struct mtk_idle_block *p_idle;
	u64 curr_time;
	int i;
	unsigned long flags;
	bool dump_block_info;

	if (unlikely(type < 0 || type >= NR_TYPES))
		return false;

	p_idle = &idle_block[type];

	if (unlikely(p_idle->init == false))
		return true;

	curr_time = idle_get_current_time_ms();

	if (reason >= NR_REASONS) {
		p_idle->prev_time = curr_time;
		return true;
	}

	if (p_idle->prev_time == 0)
		p_idle->prev_time = curr_time;

	spin_lock_irqsave(&idle_blocking_spin_lock, flags);

	dump_block_info	= ((curr_time - p_idle->prev_time) > p_idle->time_critera)
			&& ((curr_time - idle_block_log_prev_time) > idle_block_log_time_criteria);

	if (dump_block_info) {
		p_idle->prev_time = curr_time;
		idle_block_log_prev_time = curr_time;
	}

	spin_unlock_irqrestore(&idle_blocking_spin_lock, flags);

	if (dump_block_info) {
		/* xxidle, rgidle count */
		reset_idle_buf(idle_state_log);

		idle_buf_append(idle_state_log, "CNT(%s,rgidle): ", p_idle->name);
		for (i = 0; i < nr_cpu_ids; i++)
			idle_buf_append(idle_state_log, "[%d] = (%lu,%lu), ",
				i, p_idle->cnt[i], idle_block[IDLE_TYPE_RG].cnt[i]);
		idle_prof_warn("%s\n", get_idle_buf(idle_state_log));

		/* block category */
		reset_idle_buf(idle_state_log);

		idle_buf_append(idle_state_log, "%s_block_cnt: ", p_idle->name);
		for (i = 0; i < NR_REASONS; i++)
			idle_buf_append(idle_state_log, "[%s] = %lu, ",
				reason_name[i], p_idle->block_cnt[i]);
		idle_prof_warn("%s\n", get_idle_buf(idle_state_log));

		reset_idle_buf(idle_state_log);

		idle_buf_append(idle_state_log, "%s_block_mask: ", p_idle->name);
		for (i = 0; i < NR_GRPS; i++)
			idle_buf_append(idle_state_log, "0x%08x, ", p_idle->block_mask[i]);
		idle_prof_warn("%s\n", get_idle_buf(idle_state_log));

		spm_resource_req_dump();

		memset(p_idle->block_cnt, 0, NR_REASONS * sizeof(p_idle->block_cnt[0]));
		p_idle->prev_time = idle_get_current_time_ms();
		idle_block_log_prev_time = p_idle->prev_time;
	}
	p_idle->block_cnt[reason]++;
	return false;
}

void mtk_idle_block_setting(int type, unsigned long *cnt, unsigned long *block_cnt, unsigned int *block_mask)
{
	if (unlikely(type < 0 || type >= NR_TYPES))
		return;

	idle_block[type].cnt        = cnt;
	idle_block[type].block_cnt  = block_cnt;
	idle_block[type].block_mask = block_mask;

	if (cnt && block_cnt && block_mask)
		idle_block[type].init = true;
	else
		idle_prof_err("IDLE BLOCKING INFO SETTING FAIL (type:%d)\n", type);
}

void mtk_idle_twam_init(void)
{
#if SPM_MET_TAGGING
	met_tag_init();
#endif
	spm_get_twam_table(&idle_twam.str);
	idle_twam.running = false;
	idle_twam.speed_mode = true;
	idle_twam.event = 29;
}

