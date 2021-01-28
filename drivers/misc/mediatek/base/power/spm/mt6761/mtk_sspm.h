/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef __MTK_SSPM_H__
#define __MTK_SSPM_H__

/* op_cond enum, should be align to spm.h @ sspm */
enum {
	PWR_OPT_SLEEP_DPIDLE  = (1 << 0),
	PWR_OPT_UNIVPLL_STAT  = (1 << 1),
	PWR_OPT_GPS_STAT      = (1 << 2),
	PWR_OPT_VCORE_LP_MODE = (1 << 3),
	PWR_OPT_XO_UFS_OFF    = (1 << 4),
	PWR_OPT_CLKBUF_ENTER_BBLPM = (1 << 5),
	NF_PWR_OPT
};

struct spm_data {
	unsigned int cmd;
	union {
		struct {
			unsigned int sys_timestamp_l;
			unsigned int sys_timestamp_h;
			unsigned int sys_src_clk_l;
			unsigned int sys_src_clk_h;
			unsigned int spm_opt;
		} suspend;
		struct {
			unsigned int vcore_level0;
			unsigned int vcore_level1;
			unsigned int vcore_level2;
			unsigned int vcore_level3;
		} vcorefs;
		struct {
			unsigned int args1;
			unsigned int args2;
			unsigned int args3;
			unsigned int args4;
			unsigned int args5;
			unsigned int args6;
			unsigned int args7;
		} args;
	} u;
};

/**************************************
 * mtk_sspm.c
 **************************************/
int spm_to_sspm_command(u32 cmd, struct spm_data *spm_d);
int spm_to_sspm_command_async(u32 cmd, struct spm_data *spm_d);
int spm_to_sspm_command_async_wait(u32 cmd);
void sspm_timesync_ts_get(unsigned int *ts_h, unsigned int *ts_l);
void sspm_timesync_clk_get(unsigned int *clk_h, unsigned int *clk_l);


#endif /* __MTK_SSPM_H__ */
