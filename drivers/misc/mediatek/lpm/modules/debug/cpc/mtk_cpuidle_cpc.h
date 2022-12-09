/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CPUIDLE_CPC_H__
#define __MTK_CPUIDLE_CPC_H__

#include <linux/types.h>
#include <linux/init.h>
#include <lpm_module.h>

enum {
	CPU_TYPE_L,
	CPU_TYPE_B,

	NF_CPU_TYPE
};

enum {
	CPC_SMC_EVENT_DUMP_TRACE_DATA,
	CPC_SMC_EVENT_GIC_DPG_SET,
	CPC_SMC_EVENT_CPC_CONFIG,
	CPC_SMC_EVENT_READ_CONFIG,
	CPC_SMC_EVENT_CPC_PROFILE,

	NF_CPC_SMC_EVENT
};

enum {
	CPC_SMC_CONFIG_PROF,
	CPC_SMC_CONFIG_CNT_CLR,
	CPC_SMC_CONFIG_TIME_SYNC,

	NF_CPC_SMC_CONFIG
};

enum {
	CPC_PROF_ENABLE,
	CPC_PROF_IS_ENABLED,
	CPC_PROF_DEV_NUM,
	CPC_PROF_DEV_NAME,
	CPC_PROF_OFF_CNT,
	CPC_PROF_OFF_AVG,
	CPC_PROF_OFF_MAX,
	CPC_PROF_OFF_MIN,
	CPC_PROF_ON_CNT,
	CPC_PROF_ON_AVG,
	CPC_PROF_ON_MAX,
	CPC_PROF_ON_MIN,

	CPC_PROF_NUM
};

/**
 * number of profile type :
 *  - (number of cpu type) + cluster + mcusys
 */
#define DEV_TYPE_NUM (NF_CPU_TYPE + 2)


/* smc */
#define cpc_smc(act, arg1, arg2)\
	lpm_smc_cpu_pm_lp(LP_CPC_COMMAND, act, arg1, arg2)

#define cpc_smc_prof(arg1, arg2)\
	cpc_smc(CPC_SMC_EVENT_CPC_PROFILE, arg1, arg2)\

#define cpc_time_sync()\
	cpc_smc(CPC_SMC_EVENT_CPC_CONFIG\
			, CPC_SMC_CONFIG_TIME_SYNC, 0)

void mtk_cpc_prof_start(void);
void mtk_cpc_prof_stop(void);
void mtk_cpc_prof_lat_dump(char **ToUserBuf, size_t *sz);

#endif /* __MTK_CPUIDLE_CPC_H__ */
