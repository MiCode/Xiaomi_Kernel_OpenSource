// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/math64.h>

#include <mtk_idle.h>
#include <mtk_idle_internal.h>

#if defined(CONFIG_MTK_CPU_FREQ)
#include <mtk_cpufreq_api.h>
#endif

#include <mtk_spm_internal.h>
#include <linux/sched/clock.h>
#define SPM_MET_TAGGING  0

#if SPM_MET_TAGGING
#include <core/met_drv.h>
#endif

#define idle_get_current_time_us(x) do {\
		struct timeval t;\
		do_gettimeofday(&t);\
		(x) = ((t.tv_sec & 0xFFF) * 1000000 + t.tv_usec);\
	} while (0)


/* [ByChip] Internal weak function: implemented in mtk_idle_cond_check.c */
int __attribute__((weak)) mtk_idle_cond_append_info(
	bool short_log, int idle_type, char *logptr, unsigned int logsize);

/* idle ratio */
static bool idle_ratio_en;
static unsigned long long idle_ratio_profile_start_time;
static unsigned long long idle_ratio_profile_duration;

/* idle block information */
static unsigned long long idle_block_log_prev_time;
static unsigned int idle_block_log_time_criteria = 5000;    /* 5 sec */
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
#define idle_buf_append(idle, fmt, args...) \
	((idle).p_idx += scnprintf(\
			(idle).p_idx\
			, IDLE_LOG_BUF_LEN - strlen((idle).buf), fmt, ##args))

#define reset_log()              reset_idle_buf(idle_log)
#define get_log()                get_idle_buf(idle_log)
#define append_log(fmt, args...) idle_buf_append(idle_log, fmt, ##args)

struct mtk_idle_block {
	u64 prev_time;
	u32 time_critera;
	unsigned long last_cnt[NR_CPUS];
	unsigned long *cnt;
	unsigned long *block_cnt;
	char *name;
	bool init;
};

#define IDLE_RATIO_WINDOW_MS 1000        /* 1000 ms */
/* xxidle recent idle ratio */
static struct mtk_idle_recent_ratio recent_ratio = {
	.value = 0,
	.value_dp = 0,
	.value_so3 = 0,
	.value_so = 0,
	.last_end_ts = 0,
	.start_ts = 0,
	.end_ts = 0,
};

struct mtk_idle_ratio {
	char *name;
	unsigned long long start;
	unsigned long long value;
};

struct mtk_idle_prof {
	struct mtk_idle_ratio ratio;
	struct mtk_idle_block block;
};

#define DEFINE_ATTR(abbr_name, block_name, block_ms)   \
	{                                                  \
		.ratio = {                                     \
			.name = (abbr_name),                       \
		},                                             \
		.block = {                                     \
			.name = (block_name),                      \
			.time_critera = (block_ms),                \
			.init = false,                             \
		}                                              \
	}                                                  \

static struct mtk_idle_prof idle_prof[NR_IDLE_TYPES] = {
	[IDLE_TYPE_DP]  = DEFINE_ATTR("DP", "dpidle", 30000),
	[IDLE_TYPE_SO3] = DEFINE_ATTR("SODI3", "soidle3", 30000),
	[IDLE_TYPE_SO]  = DEFINE_ATTR("SODI", "soidle", 30000),
	[IDLE_TYPE_RG]  = DEFINE_ATTR("RG", "rgidle", ~0)
};

static DEFINE_SPINLOCK(recent_idle_ratio_spin_lock);

unsigned long long idle_get_current_time_ms(void)
{
	u64 idle_current_time = sched_clock();

	do_div(idle_current_time, 1000000);
	return idle_current_time;
}

void mtk_idle_ratio_calc_start(int type, int cpu)
{
	unsigned long flags;

	if (idle_ratio_en && type >= 0 && type < NR_IDLE_TYPES)
		idle_prof[type].ratio.start = idle_get_current_time_ms();

	if (type < IDLE_TYPE_RG) {
		spin_lock_irqsave(&recent_idle_ratio_spin_lock, flags);

		recent_ratio.start_ts = idle_get_current_time_ms();

		spin_unlock_irqrestore(&recent_idle_ratio_spin_lock, flags);
	}

	#if SPM_MET_TAGGING
	if (type < IDLE_TYPE_RG)
		idle_get_current_time_us(idle_met_timestamp[type]);
	#endif
}

void mtk_idle_ratio_calc_stop(int type, int cpu)
{
	if (idle_ratio_en && type >= 0 && type < NR_IDLE_TYPES)
		idle_prof[type].ratio.value +=
		(
			idle_get_current_time_ms() - idle_prof[type].ratio.start
		);

	if (type < IDLE_TYPE_RG) {
		struct mtk_idle_recent_ratio *ratio = NULL;
		unsigned long flags;
		unsigned long long interval = 0;
		unsigned long long last_idle_time = 0;
		unsigned long long last_ratio = 0;
		unsigned long long last_ratio_dp = 0;
		unsigned long long last_ratio_so3 = 0;
		unsigned long long last_ratio_so = 0;

		spin_lock_irqsave(&recent_idle_ratio_spin_lock, flags);

		ratio = &recent_ratio;

		ratio->end_ts = idle_get_current_time_ms();
		interval = ratio->end_ts - ratio->last_end_ts;
		last_idle_time = ratio->end_ts - ratio->start_ts;
		last_ratio = ratio->value;
		last_ratio_dp = ratio->value_dp;
		last_ratio_so3 = ratio->value_so3;
		last_ratio_so = ratio->value_so;

		if (interval >= IDLE_RATIO_WINDOW_MS) {
			ratio->value =
				(last_idle_time >= IDLE_RATIO_WINDOW_MS) ?
					IDLE_RATIO_WINDOW_MS : last_idle_time;
			ratio->value_dp =
				(type == IDLE_TYPE_DP) ? ratio->value : 0;
			ratio->value_so3 =
				(type == IDLE_TYPE_SO3) ? ratio->value : 0;
			ratio->value_so =
				(type == IDLE_TYPE_SO) ? ratio->value : 0;
		} else {
#if defined(__LP64__) || defined(_LP64)
			ratio->value =
				((IDLE_RATIO_WINDOW_MS - interval)
					* last_ratio / IDLE_RATIO_WINDOW_MS)
							+ last_idle_time;
			ratio->value_dp =
				((IDLE_RATIO_WINDOW_MS - interval)
					* last_ratio_dp / IDLE_RATIO_WINDOW_MS)
						+ ((type == IDLE_TYPE_DP) ?
							last_idle_time : 0);
			ratio->value_so3 =
				((IDLE_RATIO_WINDOW_MS - interval)
					* last_ratio_so3 / IDLE_RATIO_WINDOW_MS)
						+ ((type == IDLE_TYPE_SO3) ?
							last_idle_time : 0);
			ratio->value_so =
				((IDLE_RATIO_WINDOW_MS - interval)
					* last_ratio_so / IDLE_RATIO_WINDOW_MS)
						+ ((type == IDLE_TYPE_SO) ?
							last_idle_time : 0);
#else
			ratio->value =
				div_s64((IDLE_RATIO_WINDOW_MS - interval)
					* last_ratio, IDLE_RATIO_WINDOW_MS)
							+ last_idle_time;
			ratio->value_dp =
				div_s64((IDLE_RATIO_WINDOW_MS - interval)
					* last_ratio_dp, IDLE_RATIO_WINDOW_MS)
						+ ((type == IDLE_TYPE_DP) ?
							last_idle_time : 0);
			ratio->value_so3 =
				div_s64((IDLE_RATIO_WINDOW_MS - interval)
					* last_ratio_so3, IDLE_RATIO_WINDOW_MS)
						+ ((type == IDLE_TYPE_SO3) ?
							last_idle_time : 0);
			ratio->value_so =
				div_s64((IDLE_RATIO_WINDOW_MS - interval)
					* last_ratio_so, IDLE_RATIO_WINDOW_MS)
						+ ((type == IDLE_TYPE_SO) ?
							last_idle_time : 0);
#endif
		}
		ratio->last_end_ts = ratio->end_ts;
		spin_unlock_irqrestore(&recent_idle_ratio_spin_lock, flags);
	}

	#if SPM_MET_TAGGING
	if (type < IDLE_TYPE_RG) {
		unsigned long long idle_met_curr;

		idle_get_current_time_us(idle_met_curr);

		met_tag_oneshot(0, idle_met_label[type],
			((idle_met_curr - idle_met_timestamp[type])*100)/
			(idle_met_curr - idle_met_prev_tag[type]));
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
		for (idx = 0; idx < NR_IDLE_TYPES; idx++)
			idle_prof[idx].ratio.value = 0;
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
	unsigned long total_cnt = 0;

	p_idle = &idle_prof[type].block;

	if (unlikely(p_idle->init == false)) {
		append_log("Not initialized --- ");
		return;
	}

	reset_idle_buf(buf);

	append_log("%s: ", idle_prof[type].ratio.name);

	for (i = 0; i < nr_cpu_ids; i++) {
		idle_cnt = p_idle->cnt[i] - p_idle->last_cnt[i];
		if (idle_cnt != 0) {
			idle_buf_append(buf, "[%d] = %lu, ", i, idle_cnt);
			enter_idle = true;
		}
		total_cnt += idle_cnt;
		p_idle->last_cnt[i] = p_idle->cnt[i];
	}

	if (enter_idle && total_cnt > 0)
		idle_buf_append(buf, "Total = %lu, ", total_cnt);

	if (enter_idle)
		append_log("%s --- ", get_idle_buf(buf));
	else
		append_log("No enter --- ");
}

static DEFINE_SPINLOCK(idle_dump_cnt_spin_lock);

void mtk_idle_dump_cnt_in_interval(void)
{
	int i = 0;
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

	mtk_idle_dump_cnt(IDLE_TYPE_DP);
	mtk_idle_dump_cnt(IDLE_TYPE_SO3);
	mtk_idle_dump_cnt(IDLE_TYPE_SO);

	/* dump log */
	pr_notice("Power/swap %s\n", get_log());

	/* dump idle ratio */
	if (idle_ratio_en) {
		idle_ratio_profile_duration =
			 idle_get_current_time_ms()
			 - idle_ratio_profile_start_time;

		reset_log();
		append_log("--- CPU idle: %llu, ", idle_ratio_profile_duration);
		for (i = 0; i < NR_IDLE_TYPES; i++) {
			if (idle_prof[i].ratio.start == 0)
				continue;
			append_log("%s = %llu, "
				, idle_prof[i].ratio.name
				, idle_prof[i].ratio.value);
			idle_prof[i].ratio.value = 0;
		}
		append_log("--- (ms)\n");
		pr_notice("Power/swap %s\n", get_log());
		idle_ratio_profile_start_time = idle_get_current_time_ms();
	}
}

static DEFINE_SPINLOCK(idle_blocking_spin_lock);

bool mtk_idle_select_state(int type, int reason)
{
	struct mtk_idle_block *p_idle;
	u64 curr_time;
	int i;
	unsigned long flags;
	bool dump_block_info;

	if (unlikely(type < 0 || type >= NR_IDLE_TYPES))
		return false;

	p_idle = &idle_prof[type].block;

	if (unlikely(p_idle->init == false))
		return reason == NR_REASONS;

	curr_time = idle_get_current_time_ms();

	if (reason >= NR_REASONS) {
		p_idle->prev_time = curr_time;
		return true;
	}

	if (p_idle->prev_time == 0)
		p_idle->prev_time = curr_time;

	spin_lock_irqsave(&idle_blocking_spin_lock, flags);

	dump_block_info	=
		((curr_time - p_idle->prev_time) > p_idle->time_critera) &&
		((curr_time - idle_block_log_prev_time) >
		idle_block_log_time_criteria);

	if (dump_block_info) {
		p_idle->prev_time = curr_time;
		idle_block_log_prev_time = curr_time;
	}

	spin_unlock_irqrestore(&idle_blocking_spin_lock, flags);

	if (dump_block_info) {
		/* xxidle, rgidle count */
		reset_idle_buf(idle_state_log);
		idle_buf_append(idle_state_log
			, "CNT(%s): ", p_idle->name);

		for (i = 0; i < nr_cpu_ids; i++)
			idle_buf_append(idle_state_log
				, "[%d] = (%lu), "
				, i, p_idle->cnt[i]);

		pr_notice("Power/swap %s\n", get_idle_buf(idle_state_log));

		/* block category */
		reset_idle_buf(idle_state_log);
		idle_buf_append(idle_state_log, "%s_block_cnt: ", p_idle->name);
		for (i = 0; i < NR_REASONS; i++)
			if (p_idle->block_cnt[i] > 0)
				idle_buf_append(idle_state_log
						, "[%s] = %lu, "
						, mtk_idle_block_reason_name(i)
						, p_idle->block_cnt[i]);
		pr_notice("Power/swap %s\n", get_idle_buf(idle_state_log));

		/* block mask */
		reset_idle_buf(idle_state_log);
		idle_buf_append(idle_state_log
				, "%s_block_mask: ", p_idle->name);
		idle_state_log.p_idx += mtk_idle_cond_append_info(true, type,
			idle_state_log.p_idx,
			IDLE_LOG_BUF_LEN - strlen(idle_state_log.buf));
		pr_notice("Power/swap %s\n", get_idle_buf(idle_state_log));

		memset(p_idle->block_cnt, 0,
			NR_REASONS * sizeof(p_idle->block_cnt[0]));

		spm_resource_req_block_dump();
	}
	p_idle->block_cnt[reason]++;
	return false;
}

void mtk_idle_block_setting(
	int type, unsigned long *cnt, unsigned long *block_cnt)
{
	struct mtk_idle_block *p_idle;

	if (unlikely(type < 0 || type >= NR_IDLE_TYPES))
		return;

	p_idle = &idle_prof[type].block;

	p_idle->cnt = cnt;
	p_idle->block_cnt = block_cnt;

	if (cnt && block_cnt)
		p_idle->init = true;
	else
		pr_notice(
			"Power/swap IDLE BLOCKING INFO SETTING FAIL (type:%d)\n",
				type);

	#if SPM_MET_TAGGING
	if (type == IDLE_TYPE_RG)
		met_tag_init();
	#endif
}

void mtk_idle_recent_ratio_get(
	int *window_length_ms, struct mtk_idle_recent_ratio *ratio)
{
	unsigned long flags;

	spin_lock_irqsave(&recent_idle_ratio_spin_lock, flags);

	if (window_length_ms != NULL)
		*window_length_ms = IDLE_RATIO_WINDOW_MS;

	if (ratio != NULL) {
		ratio->value       = recent_ratio.value;
		ratio->value_dp    = recent_ratio.value_dp;
		ratio->value_so3   = recent_ratio.value_so3;
		ratio->value_so    = recent_ratio.value_so;
		ratio->last_end_ts = recent_ratio.last_end_ts;
	}

	spin_unlock_irqrestore(&recent_idle_ratio_spin_lock, flags);
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

static unsigned int idle_profile[NR_IDLE_TYPES][NR_PIDX];
void mtk_idle_latency_profile(unsigned int idle_type, int idx)
{
	unsigned int cur_count = 0;
	unsigned int *data;

	if (!profile_latency_enabled)
		return;

	data = &idle_profile[idle_type][0];

	cur_count = lower_32_bits(sched_clock());

	if (idx % 2 == 0)
		data[idx/2] = cur_count;
	else
		data[idx/2] = cur_count - data[idx/2];
}

static char plog[256] = { 0 };
#define log(fmt, args...) \
		(p += scnprintf(p, sizeof(plog) - strlen(plog), fmt, ##args))

#define PROFILE_LATENCY_NUMBER	(200)
struct idle_profile_data {
	unsigned long total[3];
	unsigned int count;
};

static struct idle_profile_data g_pdata[NR_IDLE_TYPES];

void mtk_idle_latency_profile_result(unsigned int idle_type)
{
	unsigned int i;
	char *p = plog;
	unsigned int *data;
	struct idle_profile_data *pdata;

	if (!profile_latency_enabled)
		return;

	data = &idle_profile[idle_type][0];
	pdata = &g_pdata[idle_type];

	#if defined(CONFIG_MTK_CPU_FREQ)
	log("%s (cpu%d/%u),", mtk_idle_name(idle_type)
		, smp_processor_id()
		, mt_cpufreq_get_cur_freq(smp_processor_id()/4));
	#endif

	for (i = 0; i < NR_PIDX; i++)
		log("%u%s", data[i], (i == NR_PIDX - 1) ? "":",");

	if (pdata->count < PROFILE_LATENCY_NUMBER) {
		pdata->total[0] += (data[0]);
		pdata->total[1] += (data[1]);
		pdata->total[2] += (data[2]);
		pdata->count++;
	} else {
		pr_notice("Power/latency_profile avg %s: %u, %u, %u\n"
			, mtk_idle_name(idle_type)
			, (unsigned int)pdata->total[0]/PROFILE_LATENCY_NUMBER
			, (unsigned int)pdata->total[1]/PROFILE_LATENCY_NUMBER
			, (unsigned int)pdata->total[2]/PROFILE_LATENCY_NUMBER);
		pdata->count = 0;
		pdata->total[0] = pdata->total[1] = pdata->total[2] = 0;
	}

	pr_notice("Power/latency_profile %s\n", plog);
}

