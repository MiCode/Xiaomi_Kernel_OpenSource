/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <mtk_spm.h>
#include <mtk_sleep.h>
#if !defined(SPM_K414_EARLY_PORTING)
//#include <mtk_cpuidle.h>
#endif
#include <mtk_spm_resource_req_internal.h>
#include <mtk_spm_sleep_internal.h>

int __attribute__ ((weak)) mtk_cpuidle_init(void) { return -EOPNOTSUPP; }

static int __init mt_spm_init(void)
{
	mtk_cpuidle_framework_init();
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	mtk_cpuidle_init();

	spm_module_init();
	slp_module_init();
#endif

	spm_resource_req_init();

	register_spm_resource_req_func(&spm_resource_req);

	return 0;
}

static void __exit mt_sleep_exit(void)
{
}

#if IS_BUILTIN(CONFIG_MTK_SPM_V4)
late_initcall(mt_spm_init);
#else
module_init(mt_spm_init);
#endif
module_exit(mt_sleep_exit);
MODULE_LICENSE("GPL");
