/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_IPI_SSPM_V1__
#define __MTK_IPI_SSPM_V1__

/* cmd flag for sspm sleep task */
enum {
	SSPM_SPM_SUSPEND,
	SSPM_SPM_RESUME,
	SSPM_SPM_DPIDLE_ENTER,
	SSPM_SPM_DPIDLE_LEAVE,
	SSPM_SPM_ENTER_SODI,
	SSPM_SPM_LEAVE_SODI,
	SSPM_SPM_ENTER_SODI3,
	SSPM_SPM_LEAVE_SODI3,
	SSPM_SPM_SUSPEND_PREPARE,
	SSPM_SPM_POST_SUSPEND,
	SSPM_SPM_DPIDLE_PREPARE,
	SSPM_SPM_POST_DPIDLE,
	SSPM_SPM_SODI_PREPARE,
	SSPM_SPM_POST_SODI,
	SSPM_SPM_SODI3_PREPARE,
	SSPM_SPM_POST_SODI3,
	SSPM_SPM_VCORE_PWARP_CMD,
	SSPM_SPM_PWR_CTRL_SUSPEND,
	SSPM_SPM_PWR_CTRL_DPIDLE,
	SSPM_SPM_PWR_CTRL_SODI,
	SSPM_SPM_PWR_CTRL_SODI3,
	SSPM_SPM_PWR_CTRL_VCOREFS,
	SSPM_SPM_TWAM_ENABLE,
	SSPM_SPM_PWR_CTRL_IDLE_DRAM,
	SSPM_SPM_PWR_CTRL_IDLE_SYSPLL,
	SSPM_SPM_PWR_CTRL_IDLE_BUS26M,
};
enum {
	PLAT_PWR_OPT_SLEEP_DPIDLE  = (1 << 0),
	PLAT_PWR_OPT_UNIVPLL_STAT  = (1 << 1),
	PLAT_PWR_OPT_GPS_STAT      = (1 << 2),
	PLAT_PWR_OPT_VCORE_LP_MODE = (1 << 3),
	PLAT_PWR_OPT_XO_UFS_OFF    = (1 << 4),
	PLAT_PWR_OPT_CLKBUF_ENTER_BBLPM = (1 << 5),
	NF_PWR_OPT
};
struct plat_spm_data {
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
#define SSPM_SPM_DATA_LEN   (8)

enum {
	SSPM_NOTIFY_ENTER,
	SSPM_NOTIFY_LEAVE,
	SSPM_NOTIFY_ENTER_ASYNC,
	SSPM_NOTIFY_LEAVE_ASYNC,
};

#endif

