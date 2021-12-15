/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <mtk_spm.h>
#include <mtk_sleep_internal.h>

static bool spm_drv_init;

bool mtk_spm_drv_ready(void)
{
	return spm_drv_init;
}

static int __init mtk_sleep_init(void)
{
	int ret = -1;

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	slp_module_init();
	ret = mtk_spm_init();
#endif
	spm_drv_init = !ret;
	return 0;
}

late_initcall(mtk_sleep_init);
