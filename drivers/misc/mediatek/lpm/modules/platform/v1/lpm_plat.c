// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <asm/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpuidle.h>

#include <lpm_plat_apmcu.h>
#include <lpm_module.h>
#include "lpm_plat.h"
#include "lpm_plat_suspend.h"

static unsigned int lpm_pwr_state;

/*
 * Please make sure the race condition protection upfront
 * when calling mtk_lp_plat_do_mcusys_prepare_pdn()
 */
int lpm_do_mcusys_prepare_pdn(unsigned int status,
					   unsigned int *resource_req)
{
	lpm_pwr_state |= (status | PLAT_GIC_MASKED
				| PLAT_MCUSYSOFF_PREPARED);
	return 0;
}

/*
 * Please make sure the race condition protection upfront
 * when calling mtk_lp_plat_do_mcusys_prepare_on()
 */
static int __lpm_do_mcusys_prepare_on(unsigned int clr_status)
{
	lpm_pwr_state &= ~(clr_status | PLAT_GIC_MASKED
				  | PLAT_MCUSYSOFF_PREPARED);

	return 0;
}

int lpm_do_mcusys_prepare_on(void)
{
	unsigned int status = lpm_pwr_state;

	return __lpm_do_mcusys_prepare_on(status);
}

static int __init lpm_early_initcall(void)
{
	lpm_plat_apmcu_early_init();
	return 0;
}

#ifndef MTK_LPM_MODE_MODULE
subsys_initcall(lpm_early_initcall);
#endif

static int __init lpm_device_initcall(void)
{
	return 0;
}

static int __init lpm_late_initcall(void)
{
	lpm_plat_apmcu_init();
	lpm_model_suspend_init();

	cpuidle_pause_and_lock();
	lpm_smc_cpu_pm(VALIDATE_PWR_STATE_CTRL, MT_LPM_SMC_ACT_SET,
				PSCI_E_SUCCESS, 0);
	cpuidle_resume_and_unlock();

	return 0;
}
#ifndef MTK_LPM_MODE_MODULE
late_initcall_sync(lpm_late_initcall);
#endif

static int __init lpm_plat_init(void)
{
	int ret = 0;
#ifdef MTK_LPM_MODE_MODULE
	ret = lpm_early_initcall();
#endif
	if (ret)
		goto lpm_plat_init_fail;

	ret = lpm_device_initcall();

	if (ret)
		goto lpm_plat_init_fail;

#ifdef MTK_LPM_MODE_MODULE
	ret = lpm_late_initcall();
#endif

	if (ret)
		goto lpm_plat_init_fail;

	return 0;

lpm_plat_init_fail:
	return -EAGAIN;
}

static void __exit lpm_plat_exit(void)
{
}

module_init(lpm_plat_init);
module_exit(lpm_plat_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MTK low power platform module");
MODULE_AUTHOR("MediaTek Inc.");
