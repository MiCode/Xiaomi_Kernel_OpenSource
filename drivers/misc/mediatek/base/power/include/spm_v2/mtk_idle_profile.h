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
#include "mtk_idle_internal.h"

#define SPM_MCSODI_PROFILE_RATIO (0)
#define SPM_ALLWFI_PROFILE_RATIO (0)
#define SPM_MET_TAGGING  0
#define IDLE_LOG_BUF_LEN 512

#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
#define IDLE_PROF_USING_STD_TIMER
#endif

#define APXGPT_SYS_TICKS_PER_US ((u32)(13))
#define APXGPT_RTC_TICKS_PER_MS ((u32)(32))

struct mtk_idle_twam {
	u32 event;
	const char **str;
	bool running;
	bool speed_mode;
};

struct mtk_idle_buf {
	char buf[IDLE_LOG_BUF_LEN];
	char *p_idx;
};

#define reset_idle_buf(idle) ((idle).p_idx = (idle).buf)
#define get_idle_buf(idle)   ((idle).buf)
#define idle_buf_append(idle, fmt, args...) \
	((idle).p_idx += scnprintf((idle).p_idx, IDLE_LOG_BUF_LEN - strlen((idle).buf), fmt, ##args))

extern const char *reason_name[NR_REASONS];

void mtk_idle_twam_callback(struct twam_sig *ts);
void mtk_idle_twam_disable(void);
void mtk_idle_twam_enable(u32 event);
struct mtk_idle_twam *mtk_idle_get_twam(void);

void  dpidle_profile_time(int idx);
void  soidle3_profile_time(int idx);
void  soidle_profile_time(int idx);
void dpidle_show_profile_time(void);
void soidle3_show_profile_time(void);
void soidle_show_profile_time(void);

bool mtk_idle_get_ratio_status(void);
void mtk_idle_ratio_calc_start(int type, int cpu);
void mtk_idle_ratio_calc_stop(int type, int cpu);
void mtk_idle_disable_ratio_calc(void);
void mtk_idle_enable_ratio_calc(void);

void mtk_idle_dump_cnt_in_interval(void);

bool mtk_idle_state_pick(int type, int cpu, int reason);
void mtk_idle_block_setting(int type, unsigned long *cnt, unsigned long *block_cnt, unsigned int *block_mask);
void mtk_idle_twam_init(void);


static inline long int idle_get_current_time_ms(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return ((t.tv_sec & 0xFFF) * 1000000 + t.tv_usec) / 1000;
}


#endif /* __MTK_IDLE_PROFILE_H__ */

