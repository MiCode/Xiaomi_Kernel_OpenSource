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
#ifndef __MTK_IDLE_PROFILE_H__
#define __MTK_IDLE_PROFILE_H__

#include <linux/ktime.h>
#include "mtk_spm_internal.h"

#define SPM_MET_TAGGING  0
#define IDLE_LOG_BUF_LEN 1024

#define APXGPT_SYS_TICKS_PER_US ((u32)(13))
#define APXGPT_RTC_TICKS_PER_MS ((u32)(32))

struct mtk_idle_twam {
	u32 event;
	u32 sel;
	bool running;
	bool speed_mode;
};

struct mtk_idle_buf {
	char buf[IDLE_LOG_BUF_LEN];
	char *p_idx;
};

struct mtk_idle_recent_ratio {
	unsigned long long value;
	unsigned long long value_dp;
	unsigned long long value_so3;
	unsigned long long value_so;
	unsigned long long last_end_ts;
	unsigned long long start_ts;
	unsigned long long end_ts;
};

#define reset_idle_buf(idle) \
	do { (idle).p_idx = (idle).buf; (idle).buf[0] = '\0'; } while (0)
#define get_idle_buf(idle)   ((idle).buf)
#define idle_buf_append(idle, fmt, args...) \
	((idle).p_idx += scnprintf((idle).p_idx, \
				   IDLE_LOG_BUF_LEN - strlen((idle).buf), \
				   fmt, ##args))

void mtk_idle_twam_callback(struct twam_sig *ts);
void mtk_idle_twam_disable(void);
void mtk_idle_twam_enable(u32 event);
struct mtk_idle_twam *mtk_idle_get_twam(void);

bool mtk_idle_get_ratio_status(void);
void mtk_idle_ratio_calc_start(int type, int cpu);
void mtk_idle_ratio_calc_stop(int type, int cpu);
void mtk_idle_disable_ratio_calc(void);
void mtk_idle_enable_ratio_calc(void);

void mtk_idle_dump_cnt_in_interval(void);

bool mtk_idle_select_state(int type, int reason);
void mtk_idle_block_setting(
	int type, unsigned long *cnt,
	unsigned long *block_cnt, unsigned int *block_mask);
void mtk_idle_twam_init(void);

u64 idle_get_current_time_ms(void);

void mtk_idle_recent_ratio_get(
	int *window_length_ms, struct mtk_idle_recent_ratio *ratio);

enum {
	DPIDLE_PROFILE_IDLE_SELECT_START = 0,
	DPIDLE_PROFILE_CAN_ENTER_START,
	DPIDLE_PROFILE_CAN_ENTER_END,
	DPIDLE_PROFILE_IDLE_SELECT_END,
	DPIDLE_PROFILE_ENTER,
	DPIDLE_PROFILE_ENTER_UFS_CB_BEFORE_XXIDLE_START,
	DPIDLE_PROFILE_ENTER_UFS_CB_BEFORE_XXIDLE_END,
	DPIDLE_PROFILE_IDLE_NOTIFIER_END,
	DPIDLE_PROFILE_TIMER_DEL_END,
	DPIDLE_PROFILE_NOTIFY_SSPM_BEFORE_WFI_START,
	DPIDLE_PROFILE_NOTIFY_SSPM_BEFORE_WFI_END,
	DPIDLE_PROFILE_CIRQ_ENABLE_END,
	DPIDLE_PROFILE_SETUP_BEFORE_WFI_END,
	DPIDLE_PROFILE_NOTIFY_SSPM_BEFORE_WFI_ASYNC_WAIT_START,
	DPIDLE_PROFILE_NOTIFY_SSPM_BEFORE_WFI_ASYNC_WAIT_END,
	DPIDLE_PROFILE_BEFORE_WFI,
	DPIDLE_PROFILE_AFTER_WFI,
	DPIDLE_PROFILE_NOTIFY_SSPM_AFTER_WFI_START,
	DPIDLE_PROFILE_NOTIFY_SSPM_AFTER_WFI_END,
	DPIDLE_PROFILE_SETUP_AFTER_WFI_START,
	DPIDLE_PROFILE_SETUP_AFTER_WFI_END,
	DPIDLE_PROFILE_OUTPUT_WAKEUP_REASON_END,
	DPIDLE_PROFILE_CIRQ_DISABLE_END,
	DPIDLE_PROFILE_TIMER_RESTORE_START,
	DPIDLE_PROFILE_TIMER_RESTORE_END,
	DPIDLE_PROFILE_UFS_CB_AFTER_XXIDLE_START,
	DPIDLE_PROFILE_UFS_CB_AFTER_XXIDLE_END,
	DPIDLE_PROFILE_NOTIFY_SSPM_AFTER_WFI_ASYNC_WAIT_END,
	DPIDLE_PROFILE_LEAVE,

	NB_DPIDLE_PROFILE,
};

void dpidle_set_profile_sampling(unsigned int time);
void dpidle_profile_time(int idx);
void dpidle_show_profile_time(void);
void dpidle_show_profile_result(void);

/* ---------- SODI/SODI3 Profiling ---------- */

enum {
	PIDX_SELECT_TO_ENTER,
	PIDX_ENTER_TOTAL,
	PIDX_LEAVE_TOTAL,
	PIDX_IDLE_NOTIFY_ENTER,
	PIDX_PRE_HANDLER,
	PIDX_SSPM_BEFORE_WFI,
	PIDX_PRE_IRQ_PROCESS,
	PIDX_PCM_SETUP_BEFORE_WFI,
	PIDX_SSPM_BEFORE_WFI_ASYNC_WAIT,
	PIDX_SSPM_AFTER_WFI,
	PIDX_PCM_SETUP_AFTER_WFI,
	PIDX_POST_IRQ_PROCESS,
	PIDX_POST_HANDLER,
	PIDX_SSPM_AFTER_WFI_ASYNC_WAIT,
	PIDX_IDLE_NOTIFY_LEAVE,
	NR_PIDX
};

void mtk_idle_latency_profile_enable(bool enable);
bool mtk_idle_latency_profile_is_on(void);
void mtk_idle_latency_profile(unsigned int idle_type, int idx);
void mtk_idle_latency_profile_result(unsigned int idle_type);

#define profile_so_start(idx) \
	do { \
		if (mtk_idle_latency_profile_is_on()) \
			mtk_idle_latency_profile(IDLE_TYPE_SO, 2*idx); \
	} while (0)

#define profile_so_end(idx) \
	do { \
		if (mtk_idle_latency_profile_is_on()) \
			mtk_idle_latency_profile(IDLE_TYPE_SO, 2*idx+1); \
	} while (0)

#define profile_so_dump() \
	do { \
		if (mtk_idle_latency_profile_is_on()) \
			mtk_idle_latency_profile_result(IDLE_TYPE_SO); \
	} while (0)

#define profile_so3_start(idx) \
	do { \
		if (mtk_idle_latency_profile_is_on()) \
			mtk_idle_latency_profile(IDLE_TYPE_SO3, 2*idx); \
	} while (0)

#define profile_so3_end(idx) \
	do { \
		if (mtk_idle_latency_profile_is_on()) \
			mtk_idle_latency_profile(IDLE_TYPE_SO3, 2*idx+1); \
	} while (0)

#define profile_so3_dump() \
	do { \
		if (mtk_idle_latency_profile_is_on()) \
			mtk_idle_latency_profile_result(IDLE_TYPE_SO3); \
	} while (0)


#if 0
void idle_profile_delay(unsigned int);
#endif

#endif /* __MTK_IDLE_PROFILE_H__ */

