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
#include <linux/syscore_ops.h>
#include <asm/cpuidle.h>
#include <asm/suspend.h>

#include <mtk_lpm.h>
#include <mtk_lpm_module.h>
#include <mtk_lpm_call.h>
#include <mtk_lpm_type.h>
#include <mtk_lpm_call_type.h>

#include "mt6779.h"
#include "mt6779_suspend.h"

unsigned int mt6779_suspend_status;

static inline int mt6779_suspend_common_enter(unsigned int *susp_status)
{
	unsigned int status = PLAT_VCORE_LP_MODE
				| PLAT_PMIC_VCORE_SRCLKEN0
				| PLAT_SUSPEND;

	/* maybe need to stop sspm/mcupm mcdi task here */
	if (susp_status)
		*susp_status = status;

	return 0;
}


static inline int mt6779_suspend_common_resume(unsigned int susp_status)
{
	/* Implement suspend common flow here */
	return 0;
}

int mt6779_suspend_prompt(int cpu, const struct mtk_lpm_issuer *issuer)
{
	int ret = 0;
	unsigned int spm_res = 0;

	mt6779_suspend_status = 0;

	pr_info("[name:spm&][%s:%d] - prepare suspend enter\n",
			__func__, __LINE__);

	ret = mt6779_suspend_common_enter(&mt6779_suspend_status);

	if (ret)
		goto PLAT_LEAVE_SUSPEND;

	/* Legacy SSPM flow, spm sw resource request flow */
	mt6779_do_mcusys_prepare_pdn(mt6779_suspend_status, &spm_res);

	pr_info("[name:spm&][%s:%d] - suspend enter\n",
			__func__, __LINE__);

PLAT_LEAVE_SUSPEND:
	return ret;
}

void mt6779_suspend_reflect(int cpu,
					const struct mtk_lpm_issuer *issuer)
{
	pr_info("[name:spm&][%s:%d] - prepare suspend resume\n",
			__func__, __LINE__);

	mt6779_suspend_common_resume(mt6779_suspend_status);
	mt6779_do_mcusys_prepare_on();

	pr_info("[name:spm&][%s:%d] - suspend resume\n",
			__func__, __LINE__);

	if (issuer)
		issuer->log(MT_LPM_ISSUER_CPUIDLE, "SUSPEND", NULL);
}

struct mtk_lpm_model mt6779_model_suspend = {
	.flag = MTK_LP_REQ_NONE,
	.op = {
		.prompt = mt6779_suspend_prompt,
		.reflect = mt6779_suspend_reflect,
	}
};

int __init mt6779_model_suspend_init(void)
{
	mtk_lpm_suspend_registry("suspend", &mt6779_model_suspend);
	return 0;
}
