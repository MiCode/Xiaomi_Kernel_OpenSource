/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/kernel.h>
#include <linux/math64.h>

#include <mtk_idle.h>
#include <mtk_idle_internal.h>
#include <linux/sched/clock.h>

#if defined(CONFIG_MTK_CPU_FREQ)
#include <mtk_cpufreq_api.h>
#endif

#include <mtk_spm_internal.h>

#define SPM_MET_TAGGING  0

#if SPM_MET_TAGGING
#include <core/met_drv.h>
#endif

#include <mtk_idle_profile.h>
#include <mtk_idle_module.h>

/* [ByChip] Internal weak function: implemented in mtk_idle_cond_check.c */
int __attribute__((weak)) mtk_idle_cond_append_info(
	bool short_log, unsigned int idle_type, char *logptr, unsigned int logsize);

/* idle ratio */
static bool idle_ratio_en;
static unsigned long long idle_ratio_profile_start_time;
static unsigned long long idle_ratio_profile_duration;

/* idle block information */
static unsigned long long idle_cnt_dump_prev_time;
static unsigned int idle_cnt_dump_criteria = 5000;          /* 5 sec */

/*External weak functions: implemented in mtk_cpufreq_api.c*/
unsigned int __attribute__((weak))
	mt_cpufreq_get_cur_freq(unsigned int id)
{
	return 0;
}

#define IDLE_LOG_BUF_LEN 4096
struct mtk_idle_buf {
	char buf[IDLE_LOG_BUF_LEN];
	char *p_idx;
};

static struct mtk_idle_buf idle_log;
static struct mtk_idle_buf idle_state_log;

#define reset_idle_buf(idle) \
	do { (idle).p_idx = (idle).buf; (idle).buf[0] = '\0'; } while (0)
#define get_idle_buf(idle)   ((idle).buf)
#define get_idle_buf_cur(idle)	((idle).p_idx)
#define idle_buf_append(idle, fmt, args...) \
	((idle).p_idx += scnprintf(\
			(idle).p_idx\
			, IDLE_LOG_BUF_LEN - strlen((idle).buf), fmt, ##args))

#define reset_log()              reset_idle_buf(idle_log)
#define get_log()                get_idle_buf(idle_log)
#define get_log_cur()				get_idle_buf_cur(idle_log)
#define append_log(fmt, args...) idle_buf_append(idle_log, fmt, ##args)

#define PROFILE_LATENCY_NUM	(200)

unsigned long long idle_get_current_time_ms(void)
{
	u64 idle_current_time = sched_clock();

	do_div(idle_current_time, 1000000);
	return idle_current_time;
}


bool mtk_idle_get_ratio_status(void)
{
	return idle_ratio_en;
}

void mtk_idle_disable_ratio_calc(void)
{
	mtk_idle_module_feature(MTK_IDLE_MODULE_RATIO_CAL, 0);
	idle_ratio_en = false;
}

void mtk_idle_enable_ratio_calc(void)
{
	mtk_idle_module_feature(MTK_IDLE_MODULE_RATIO_CAL, 1);
	idle_ratio_profile_start_time = idle_get_current_time_ms();
	idle_ratio_en = true;
}

static DEFINE_SPINLOCK(idle_dump_cnt_spin_lock);

void mtk_idle_dump_cnt_in_interval(void)
{
	unsigned long long idle_cnt_dump_curr_time = 0;
	unsigned long flags;
	bool dump_log = false;

	idle_cnt_dump_curr_time = idle_get_current_time_ms();

	if (idle_cnt_dump_prev_time == 0)
		idle_cnt_dump_prev_time = idle_cnt_dump_curr_time;

	spin_lock_irqsave(&idle_dump_cnt_spin_lock, flags);

	if (((idle_cnt_dump_curr_time - idle_cnt_dump_prev_time) >
		idle_cnt_dump_criteria)) {
		dump_log = true;
		idle_cnt_dump_prev_time = idle_cnt_dump_curr_time;
	}

	spin_unlock_irqrestore(&idle_dump_cnt_spin_lock, flags);

	if (!dump_log)
		return;

	/* dump idle count */
	reset_log();

	mtk_idle_module_info_dump(MTK_IDLE_MODULE_INFO_COUNT
				,  get_log(), IDLE_LOG_BUF_LEN);
	/* dump log */
	printk_deferred("[name:spm&]Power/swap %s\n", get_log());

	/* dump idle ratio */
	if (idle_ratio_en) {
		idle_ratio_profile_duration =
			 idle_get_current_time_ms()
			 - idle_ratio_profile_start_time;

		reset_log();
		append_log("--- CPU idle: %llu, ", idle_ratio_profile_duration);
		mtk_idle_module_info_dump(MTK_IDLE_MODULE_INFO_RATIO
			, get_log_cur()
			, IDLE_LOG_BUF_LEN - (get_log_cur() - get_log()));
		printk_deferred("[name:spm&]Power/swap %s --- (ms)\n"
			, get_log());
		idle_ratio_profile_start_time = idle_get_current_time_ms();
	}
}

void mtk_idle_block_reason_report(struct MTK_IDLE_MODEL_CLERK const *clerk)
{
	int i;
	/* xxidle, rgidle count */
	reset_idle_buf(idle_state_log);
	idle_buf_append(idle_state_log
		, "CNT(%s): ", clerk->name);

	for (i = 0; i < nr_cpu_ids; i++)
		idle_buf_append(idle_state_log
			, "[%d] = (%lu), "
			, i, clerk->status.cnt.enter[i]);

	printk_deferred("[name:spm&]Power/swap %s\n"
		, get_idle_buf(idle_state_log));

	/* block category */
	reset_idle_buf(idle_state_log);
	idle_buf_append(idle_state_log, "%s_block_cnt: ", clerk->name);
	for (i = 0; i < NR_REASONS; i++)
		if (clerk->status.cnt.block[i] > 0)
			idle_buf_append(idle_state_log
					, "[%s] = %lu, "
					, mtk_idle_block_reason_name(i)
					, clerk->status.cnt.block[i]);
	printk_deferred("[name:spm&]Power/swap %s\n"
		, get_idle_buf(idle_state_log));

	/* block mask */
	reset_idle_buf(idle_state_log);
	idle_buf_append(idle_state_log
			, "%s_block_mask: ", clerk->name);
	idle_state_log.p_idx += mtk_idle_cond_append_info(true, clerk->type,
		idle_state_log.p_idx,
		IDLE_LOG_BUF_LEN - strlen(idle_state_log.buf));
	printk_deferred("[name:spm&]Power/swap %s\n"
		, get_idle_buf(idle_state_log));
	spm_resource_req_block_dump();
}

void mtk_idle_recent_ratio_get(
	int *window_length_ms, struct mtk_idle_recent_ratio *ratio)
{
	int win_ms = 0;
	struct mtk_idle_ratio_recent_info Idle_recent;

	mtk_idle_mod_recent_ratio_get_plat(&Idle_recent, &win_ms);

	if (window_length_ms != NULL)
		*window_length_ms = win_ms;

	if (ratio != NULL) {
		ratio->value       = Idle_recent.ratio.scenario.value;
		ratio->value_dp    = Idle_recent.ratio.scenario.value_dp;
		ratio->value_so3   = Idle_recent.ratio.scenario.value_so3;
		ratio->value_so    = Idle_recent.ratio.scenario.value_so;
		ratio->last_end_ts = Idle_recent.ratio.scenario.last_end_ts;
	}
}

static bool profile_latency_enabled;
void mtk_idle_latency_profile_enable(bool enable)
{
	profile_latency_enabled = enable;
}

bool mtk_idle_latency_profile_is_on(void)
{
	return profile_latency_enabled;
}

static unsigned int ProfileLatency[NR_PIDX];
void mtk_idle_latency_profile(int idx)
{
	unsigned int cur_count = 0;

	if (!profile_latency_enabled)
		return;


	cur_count = lower_32_bits(sched_clock());

	if ((idx & 0x1))
		ProfileLatency[idx>>1] = cur_count - ProfileLatency[idx>>1];
	else
		ProfileLatency[idx>>1] = cur_count;
}

static char plog[256] = { 0 };
#define log(fmt, args...) \
		(p += scnprintf(p, sizeof(plog) - strlen(plog), fmt, ##args))

void mtk_idle_latency_profile_result(struct MTK_IDLE_MODEL_CLERK *clerk)
{
	unsigned int i;
	char *p = plog;

	if (!profile_latency_enabled || !clerk)
		return;

	#if defined(CONFIG_MTK_CPU_FREQ)
	log("%s (cpu%d/%u),", clerk->name
		, smp_processor_id()
		, mt_cpufreq_get_cur_freq(smp_processor_id()/4));
	#endif

	#define _LATENCY	clerk->status.latency
	for (i = 0; i < NR_PIDX; i++)
		log("%u%s", ProfileLatency[i], (i == NR_PIDX - 1) ? "":",");

	if (_LATENCY.count < PROFILE_LATENCY_NUM) {
		_LATENCY.total[0] += (ProfileLatency[0]);
		_LATENCY.total[1] += (ProfileLatency[1]);
		_LATENCY.total[2] += (ProfileLatency[2]);
		_LATENCY.count++;
	} else {
		printk_deferred("[name:spm&]Power/latency_profile avg %s: %u, %u, %u\n"
			, clerk->name
			, (unsigned int)_LATENCY.total[0]/PROFILE_LATENCY_NUM
			, (unsigned int)_LATENCY.total[1]/PROFILE_LATENCY_NUM
			, (unsigned int)_LATENCY.total[2]/PROFILE_LATENCY_NUM);
		_LATENCY.count = 0;
		_LATENCY.total[0] = _LATENCY.total[1] = _LATENCY.total[2] = 0;
	}

	printk_deferred("[name:spm&]Power/latency_profile %s\n", plog);
}

