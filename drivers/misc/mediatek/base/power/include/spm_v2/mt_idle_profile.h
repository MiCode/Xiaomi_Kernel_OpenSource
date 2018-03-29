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
#ifndef __MT_IDLE_PROFILE_H__
#define __MT_IDLE_PROFILE_H__

#include <linux/ktime.h>
#include "mt_spm_internal.h"
#include "mt_idle_internal.h"

#define SPM_MET_TAGGING  0
#define IDLE_LOG_BUF_LEN 512

#define APXGPT_SYS_TICKS_PER_US ((u32)(13))
#define APXGPT_RTC_TICKS_PER_MS ((u32)(32))

typedef struct mt_idle_twam {
	u32 event;
	const char **str;
	bool running;
	bool speed_mode;
} idle_twam_t, *p_idle_twam_t;

struct mt_idle_buf {
	char buf[IDLE_LOG_BUF_LEN];
	char *p_idx;
};

#define reset_idle_buf(idle) ((idle).p_idx = (idle).buf)
#define get_idle_buf(idle)   ((idle).buf)
#define idle_buf_append(idle, fmt, args...) \
	((idle).p_idx += snprintf((idle).p_idx, IDLE_LOG_BUF_LEN - strlen((idle).buf), fmt, ##args))

extern const char *reason_name[NR_REASONS];

void mt_idle_twam_callback(struct twam_sig *ts);
void mt_idle_twam_disable(void);
void mt_idle_twam_enable(u32 event);
p_idle_twam_t mt_idle_get_twam(void);

void  dpidle_profile_time(int idx);
void  soidle3_profile_time(int idx);
void  soidle_profile_time(int idx);
void dpidle_show_profile_time(void);
void soidle3_show_profile_time(void);
void soidle_show_profile_time(void);

bool mt_idle_get_ratio_status(void);
void mt_idle_ratio_calc_start(int type, int cpu);
void mt_idle_ratio_calc_stop(int type, int cpu);
void mt_idle_disable_ratio_calc(void);
void mt_idle_enable_ratio_calc(void);

void mt_idle_dump_cnt_in_interval(void);

bool mt_idle_state_pick(int type, int cpu, int reason);
void mt_idle_block_setting(int type, unsigned long *cnt, unsigned long *block_cnt, unsigned int *block_mask);
void mt_idle_twam_init(void);


static inline long int idle_get_current_time_ms(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return ((t.tv_sec & 0xFFF) * 1000000 + t.tv_usec) / 1000;
}


#endif /* __MT_IDLE_PROFILE_H__ */

