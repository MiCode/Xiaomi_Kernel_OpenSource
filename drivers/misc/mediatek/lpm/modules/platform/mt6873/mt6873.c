// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <asm/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <mtk_lp_plat_apmcu.h>

#include "mt6873.h"
#include <suspend/mt6873_suspend.h>

static unsigned int mt6873_lp_pwr_state;

/*
 * Please make sure the race condition protection upfront
 * when calling mtk_lp_plat_do_mcusys_prepare_pdn()
 */
int mt6873_do_mcusys_prepare_pdn(unsigned int status,
					   unsigned int *resource_req)
{
	mt6873_lp_pwr_state |= (status | PLAT_GIC_MASKED
				| PLAT_MCUSYSOFF_PREPARED);
	return 0;
}

/*
 * Please make sure the race condition protection upfront
 * when calling mtk_lp_plat_do_mcusys_prepare_on()
 */
static int __mt6873_do_mcusys_prepare_on(unsigned int clr_status)
{
	mt6873_lp_pwr_state &= ~(clr_status | PLAT_GIC_MASKED
				  | PLAT_MCUSYSOFF_PREPARED);

	return 0;
}

int mt6873_do_mcusys_prepare_on(void)
{
	unsigned int status = mt6873_lp_pwr_state;

	return __mt6873_do_mcusys_prepare_on(status);
}

int mt6873_do_mcusys_prepare_on_ex(unsigned int clr_status)
{
	return __mt6873_do_mcusys_prepare_on(clr_status);
}

static int __init mt6873_early_initcall(void)
{
	mtk_lp_plat_apmcu_early_init();
	return 0;
}
#ifndef MTK_LPM_MODE_MODULE
subsys_initcall(mt6873_early_initcall);
#endif

static int __init mt6873_device_initcall(void)
{
	return 0;
}

static int __init mt6873_late_initcall(void)
{
	mtk_lp_plat_apmcu_init();
	//mt6873_model_suspend_init();
	return 0;
}
#ifndef MTK_LPM_MODE_MODULE
late_initcall_sync(mt6873_late_initcall);
#endif

int __init mt6873_init(void)
{
	int ret = 0;
#ifdef MTK_LPM_MODE_MODULE
	ret = mt6873_early_initcall();
#endif
	if (ret)
		goto mt6873_init_fail;

	ret = mt6873_device_initcall();

	if (ret)
		goto mt6873_init_fail;

#ifdef MTK_LPM_MODE_MODULE
	ret = mt6873_late_initcall();
#endif

	if (ret)
		goto mt6873_init_fail;

	return 0;

mt6873_init_fail:
	return -EAGAIN;
}

void __exit mt6873_exit(void)
{
}

module_init(mt6873_init);
module_exit(mt6873_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MT6873 low power platform module");
MODULE_AUTHOR("MediaTek Inc.");


