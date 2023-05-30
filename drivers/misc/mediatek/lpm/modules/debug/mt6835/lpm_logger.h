/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _LPM_LOGGER_H_
#define _LPM_LOGGER_H_

struct spm_req_sta_item {
	char name[16];
	unsigned int req_sta_num;
	unsigned int mask;
	unsigned int on;
};

struct spm_req_sta_list {
	struct spm_req_sta_item *spm_req;
	unsigned int spm_req_num;
	unsigned int lp_scenario_sta;
	unsigned int is_blocked;
	struct rtc_time *suspend_tm;
};

int dbg_ops_register(void);
struct spm_req_sta_list *spm_get_req_sta_list(void);
char *get_spm_scenario_str(unsigned int index);

extern unsigned int is_lp_blocked_threshold;

#endif
