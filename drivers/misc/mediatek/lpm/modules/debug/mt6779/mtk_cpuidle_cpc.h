/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CPUIDLE_CPC_H__
#define __MTK_CPUIDLE_CPC_H__

#include <mtk_lpm_module.h>

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

	NF_CPC_SMC_CONFIG
};

/* smc */
#define mtk_cpc_smc(act, arg1, arg2)\
	mtk_lpm_smc_cpu_pm(CPC_COMMAND, act, arg1, arg2)

/* configuration */
#define cpc_prof_en()\
	mtk_cpc_smc(CPC_SMC_EVENT_CPC_CONFIG\
			, CPC_SMC_CONFIG_PROF, 1)

#define cpc_prof_dis()\
	mtk_cpc_smc(CPC_SMC_EVENT_CPC_CONFIG\
			, CPC_SMC_CONFIG_PROF, 0)

#define cpc_auto_off_en()\
	mtk_cpc_smc(CPC_SMC_EVENT_CPC_CONFIG\
			, CPC_SMC_CONFIG_AUTO_OFF, 1)

#define cpc_auto_off_dis()\
	mtk_cpc_smc(CPC_SMC_EVENT_CPC_CONFIG\
			, CPC_SMC_CONFIG_AUTO_OFF, 0)

#define cpc_set_auto_off_thres(us)\
	mtk_cpc_smc(CPC_SMC_EVENT_CPC_CONFIG\
			, CPC_SMC_CONFIG_AUTO_OFF_THRES, us)

#define cpc_cluster_cnt_clr()\
	mtk_cpc_smc(CPC_SMC_EVENT_CPC_CONFIG\
			, CPC_SMC_CONFIG_CNT_CLR, 0)

/* status */
#define cpc_get_prof_sta()\
	mtk_cpc_smc(CPC_SMC_EVENT_READ_CONFIG\
			, CPC_SMC_CONFIG_PROF, 0)

#define cpc_get_auto_off_sta()\
	mtk_cpc_smc(CPC_SMC_EVENT_READ_CONFIG\
			, CPC_SMC_CONFIG_AUTO_OFF, 0)

#define cpc_get_auto_off_thres()\
	mtk_cpc_smc(CPC_SMC_EVENT_READ_CONFIG\
			, CPC_SMC_CONFIG_AUTO_OFF_THRES, 0)

void mtk_cpc_prof_start(void);
void mtk_cpc_prof_stop(void);

#endif /* __MTK_CPC_H__ */
