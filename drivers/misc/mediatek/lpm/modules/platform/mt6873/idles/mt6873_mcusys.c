// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/cpuidle.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/cpu_pm.h>
#include <linux/sched/clock.h>

#include <asm/cpuidle.h>
#include <asm/suspend.h>

#include <mtk_lpm.h>
#include <mtk_lpm_call.h>
#include <mtk_lpm_type.h>
#include <mtk_lpm_call_type.h>
#include <mtk_lpm_module.h>
#include <mtk_lpm_common.h>
#include <mtk_resource_constraint_v1.h>
#include <mtk_lp_plat_apmcu.h>

#include "mt6873.h"

#define MCUSY_DUMP_INFO_INTERVAL_NS	5000000000

#define IS_NEED_ISSUER(delta_ns)\
		(delta_ns > MCUSY_DUMP_INFO_INTERVAL_NS)

static u64 mt6873_mcusysoff_last_ns;
static unsigned int mt6873_mcusys_status;

int mt6873_mcusys_prompt(int cpu, const struct mtk_lpm_issuer *issuer)
{
	int bRet = 0;

	mtk_lp_plat_set_mcusys_off(cpu);

	if (mtk_lp_plat_is_mcusys_off()) {
		unsigned int mcusys_status = 0;
		unsigned int smc_res = 0;

		smc_res = mtk_lpm_smc_cpu_pm(MCUSYS_STATUS, MT_LPM_SMC_ACT_GET,
					     MCUSYS_STATUS_PDN, 0);

		if (MT_RM_STATUS_CHECK(smc_res, VCORE_LP_CLK_26M_OFF))
			mcusys_status = (PLAT_VCORE_LP_MODE
					| PLAT_PMIC_VCORE_SRCLKEN0
					| PLAT_MCUSYS_PROTECTED);
		else if (MT_RM_STATUS_CHECK(smc_res, VCORE_LP_CLK_26M_ON))
			mcusys_status = (PLAT_VCORE_LP_MODE
					| PLAT_MAINPLL_OFF
					| PLAT_PMIC_VCORE_SRCLKEN2
					| PLAT_MCUSYS_PROTECTED);
		else if (MT_RM_STATUS_CHECK(smc_res, MAINPLL_OFF))
			mcusys_status = (PLAT_MAINPLL_OFF
					| PLAT_MCUSYS_PROTECTED);
		else if (MT_RM_STATUS_CHECK(smc_res, DRAM_OFF) |
			MT_RM_STATUS_CHECK(smc_res, CPU_BUCK_OFF))
			mcusys_status = (PLAT_MCUSYS_PROTECTED);
		else
			mcusys_status = 0;

		mt6873_mcusys_status = mcusys_status;
		if (mt6873_mcusys_status != 0)
			mt6873_do_mcusys_prepare_pdn(mt6873_mcusys_status,
						     &smc_res);
	}

	return bRet;
}

void mt6873_mcusys_reflect(int cpu, const struct mtk_lpm_issuer *issuer)
{
	if (mtk_lp_plat_is_mcusys_off()) {
		if (mt6873_mcusys_status != 0) {
			mt6873_do_mcusys_prepare_on();
			mt6873_mcusys_status = 0;
		}
		if (issuer) {
			u64 delta_ns = sched_clock() - mt6873_mcusysoff_last_ns;

			if (IS_NEED_ISSUER(delta_ns)) {
				issuer->log(MT_LPM_ISSUER_CPUIDLE,
						"MCUSYSOFF", NULL);
				mt6873_mcusysoff_last_ns = sched_clock();
			}
		}
	}
	mtk_lp_plat_clr_mcusys_off(cpu);
}

struct mtk_lpm_model mt6873_model_mcusys = {
	.flag = MTK_LP_REQ_NONE,
	.op = {
		.prompt = mt6873_mcusys_prompt,
		.reflect = mt6873_mcusys_reflect,
		.prepare_resume = NULL,
		.prepare_enter = NULL,
	}
};

int __init mt6873_model_mcusys_init(void)
{
	mtk_lp_model_register("mcusysoff", &mt6873_model_mcusys);
	return 0;
}

