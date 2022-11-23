// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <mtk_spm.h>
#include <mtk_sleep_internal.h>
#include <mtk_idle_module_plat.h>
#include <mtk_spm_suspend_internal.h>
#include <mtk_spm_resource_req_console.h>
#include <mtk_spm_internal.h>
#include <mtk_sleep.h>

static bool spm_drv_init;
#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
extern void ccci_set_spm_mdsrc_cb(void (*md_clock_src_cb)(u8 set));
#endif

bool mtk_spm_drv_ready(void)
{
	return spm_drv_init;
}

static int __init mtk_sleep_init(void)
{
	int ret = -1;
	mtk_cpuidle_framework_init();
	mtk_idle_cond_check_init();
	spm_resource_console_init();
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	slp_module_init();
	ret = mtk_spm_init();
#endif
	mtk_idle_module_initialize_plat();
	spm_logger_init();
	spm_drv_init = !ret;
#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
	ccci_set_spm_mdsrc_cb(&spm_ap_mdsrc_req);
#endif
	return 0;
}

static void __exit mtk_sleep_exit(void)
{
}

module_init(mtk_sleep_init);
module_exit(mtk_sleep_exit);
MODULE_LICENSE("GPL");
