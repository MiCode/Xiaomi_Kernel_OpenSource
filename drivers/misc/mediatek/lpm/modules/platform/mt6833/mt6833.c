// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <mtk_lp_plat_apmcu.h>

#include "mt6833.h"
#include <idles/mt6833_mcusys.h>
#include <suspend/mt6833_suspend.h>

static unsigned int mt6833_lp_pwr_state;

/*
 * Please make sure the race condition protection upfront
 * when calling mtk_lp_plat_do_mcusys_prepare_pdn()
 */
int mt6833_do_mcusys_prepare_pdn(unsigned int status,
					   unsigned int *resource_req)
{
	mt6833_lp_pwr_state |= (status | PLAT_GIC_MASKED
				| PLAT_MCUSYSOFF_PREPARED);
	return 0;
}

/*
 * Please make sure the race condition protection upfront
 * when calling mtk_lp_plat_do_mcusys_prepare_on()
 */
static int __mt6833_do_mcusys_prepare_on(unsigned int clr_status)
{
	mt6833_lp_pwr_state &= ~(clr_status | PLAT_GIC_MASKED
				  | PLAT_MCUSYSOFF_PREPARED);

	return 0;
}

int mt6833_do_mcusys_prepare_on(void)
{
	unsigned int status = mt6833_lp_pwr_state;

	return __mt6833_do_mcusys_prepare_on(status);
}

int mt6833_do_mcusys_prepare_on_ex(unsigned int clr_status)
{
	return __mt6833_do_mcusys_prepare_on(clr_status);
}

static int __init mt6833_init(void)
{
	mtk_lp_plat_apmcu_init();
	mt6833_model_mcusys_init();
	mt6833_model_suspend_init();
	return 0;
}
late_initcall_sync(mt6833_init);

static int __init mt6833_early_init(void)
{
	mtk_lp_plat_apmcu_early_init();
	return 0;
}
subsys_initcall(mt6833_early_init);
