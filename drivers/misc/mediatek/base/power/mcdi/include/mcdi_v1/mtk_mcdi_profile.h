/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MTK_MCDI_PROFILE_H__
#define __MTK_MCDI_PROFILE_H__

#include <linux/proc_fs.h>
#include <mtk_mcdi_plat.h>

/* #define MCDI_PROFILE_BREAKDOWN */
/* #define MCDI_PWR_SEQ_PROF_BREAKDOWN */

enum {
	MCDI_PROF_FLAG_STOP,
	MCDI_PROF_FLAG_START,
	MCDI_PROF_FLAG_POLLING,
	NF_MCDI_PROF_FLAG
};

#ifdef MCDI_PROFILE_BREAKDOWN
enum {
	MCDI_PROFILE_GOV_SEL_ENTER = 0,
	MCDI_PROFILE_GOV_SEL_BOOT_CHK,
	MCDI_PROFILE_GOV_SEL_LOCK,
	MCDI_PROFILE_GOV_SEL_UPT_RES,
	MCDI_PROFILE_GOV_SEL_ANY_CORE,
	MCDI_PROFILE_GOV_SEL_CLUSTER,
	MCDI_PROFILE_GOV_SEL_LEAVE,
	MCDI_PROFILE_CPU_DORMANT_ENTER,
	MCDI_PROFILE_CPU_DORMANT_LEAVE,
	MCDI_PROFILE_LEAVE,
	NF_MCDI_PROFILE,
};
#else
enum {
	MCDI_PROFILE_GOV_SEL_ENTER = 0,
	MCDI_PROFILE_GOV_SEL_LEAVE,
	MCDI_PROFILE_CPU_DORMANT_ENTER,
	MCDI_PROFILE_CPU_DORMANT_LEAVE,
	MCDI_PROFILE_LEAVE,
	NF_MCDI_PROFILE,

	MCDI_PROFILE_GOV_SEL_BOOT_CHK,
	MCDI_PROFILE_GOV_SEL_LOCK,
	MCDI_PROFILE_GOV_SEL_UPT_RES,
	MCDI_PROFILE_GOV_SEL_ANY_CORE,
	MCDI_PROFILE_GOV_SEL_CLUSTER,
};
#endif

/* this define should sync with mcdi in sspm */
#ifdef MCDI_PWR_SEQ_PROF_BREAKDOWN
enum prof_bk_onoff {
	MCDI_PROF_BK_ON = 0,
	MCDI_PROF_BK_OFF,
	MCDI_PROF_BK_ONOFF_NUM,
};

enum prof_bk_ts {
	CLUSTER,
	CPU,
	ARMPLL,
	BUCK,

	MCDI_PROF_BK_NUM,
};

struct mcdi_prof_breakdown_item {
	unsigned int item[MCDI_PROF_BK_NUM][NF_CLUSTER];
	unsigned int count[MCDI_PROF_BK_NUM][NF_CLUSTER];
};

struct mcdi_prof_breakdown {
	struct mcdi_prof_breakdown_item onoff[MCDI_PROF_BK_ONOFF_NUM];
};
#endif

void set_mcdi_profile_target_cpu(int cpu);
void set_mcdi_profile_sampling(int count);
void mcdi_profile_ts(unsigned int idx);
void mcdi_profile_calc(void);

int get_mcdi_profile_cpu(void);
unsigned int get_mcdi_profile_cnt(void);
unsigned int get_mcdi_profile_sum_us(int idx);
unsigned int get_mcdi_profile_state(void);

void mcdi_procfs_profile_init(struct proc_dir_entry *mcdi_dir);
#endif /* __MTK_MCDI_PROFILE_H__ */
