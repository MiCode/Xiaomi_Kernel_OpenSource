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

	NF_CPC_SMC_EVENT
};

enum {
	CPC_SMC_CONFIG_PROF,
	CPC_SMC_CONFIG_AUTO_OFF,
	CPC_SMC_CONFIG_AUTO_OFF_THRES,
	CPC_SMC_CONFIG_CNT_CLR,
	CPC_SMC_CONFIG_TIME_SYNC,

	NF_CPC_SMC_CONFIG
};

/**
 * number of profile type :
 *  - (number of cpu type) + cluster + mcusys
 */
#define DEV_TYPE_NUM (NF_CPU_TYPE + 2)


/* smc */
#define cpc_smc(act, arg1, arg2)\
	lpm_smc_cpu_pm(CPC_COMMAND, act, arg1, arg2)

/* configuration */
#define cpc_prof_en()\
	cpc_smc(CPC_SMC_EVENT_CPC_CONFIG\
			, CPC_SMC_CONFIG_PROF, 1)

#define cpc_prof_dis()\
	cpc_smc(CPC_SMC_EVENT_CPC_CONFIG\
			, CPC_SMC_CONFIG_PROF, 0)

#define cpc_auto_off_en()\
	cpc_smc(CPC_SMC_EVENT_CPC_CONFIG\
			, CPC_SMC_CONFIG_AUTO_OFF, 1)

#define cpc_auto_off_dis()\
	cpc_smc(CPC_SMC_EVENT_CPC_CONFIG\
			, CPC_SMC_CONFIG_AUTO_OFF, 0)

#define cpc_set_auto_off_thres(us)\
	cpc_smc(CPC_SMC_EVENT_CPC_CONFIG\
			, CPC_SMC_CONFIG_AUTO_OFF_THRES, us)

#define cpc_cluster_cnt_clr()\
	cpc_smc(CPC_SMC_EVENT_CPC_CONFIG\
			, CPC_SMC_CONFIG_CNT_CLR, 0)

#define cpc_time_sync()\
	cpc_smc(CPC_SMC_EVENT_CPC_CONFIG\
			, CPC_SMC_CONFIG_TIME_SYNC, 0)

/* status */
#define cpc_get_prof_sta()\
	cpc_smc(CPC_SMC_EVENT_READ_CONFIG\
			, CPC_SMC_CONFIG_PROF, 0)

#define cpc_get_auto_off_sta()\
	cpc_smc(CPC_SMC_EVENT_READ_CONFIG\
			, CPC_SMC_CONFIG_AUTO_OFF, 0)

#define cpc_get_auto_off_thres()\
	cpc_smc(CPC_SMC_EVENT_READ_CONFIG\
			, CPC_SMC_CONFIG_AUTO_OFF_THRES, 0)

#define cpc_tick_to_us(val) ((val) / 13)

void mtk_cpc_prof_start(void);
void mtk_cpc_prof_stop(void);
void mtk_cpc_prof_lat_dump(char **ToUserBuf, size_t *sz);
void mtk_cpuidle_cpc_init(void);

int  mtk_cpc_init(void);
void mtk_cpc_exit(void);

#endif /* __MTK_CPUIDLE_CPC_H__ */
