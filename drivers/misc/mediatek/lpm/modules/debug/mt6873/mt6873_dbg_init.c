// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of_device.h>


#include <mt6873_dbg_fs_common.h>
#include <mt6873_logger.h>
#include <mtk_cpupm_dbg.h>
#include <mtk_dbg_common_v1.h>
#include <mtk_lpm_module.h>

static int __init mt6873_dbg_early_initcall(void)
{
	return 0;
}
#ifndef MTK_LPM_MODE_MODULE
subsys_initcall(mt6873_dbg_early_initcall);
#endif

static int __init mt6873_dbg_device_initcall(void)
{
	mtk_dbg_common_fs_init();
	mt6873_dbg_fs_init();
	mtk_cpupm_dbg_init();
	return 0;
}

static int __init mt6873_dbg_late_initcall(void)
{
	mt6873_lpm_trace_init();
	mt6873_logger_init();
	return 0;
}
#ifndef MTK_LPM_MODE_MODULE
late_initcall_sync(mt6873_dbg_late_initcall);
#endif

int __init mt6873_dbg_init(void)
{
	int ret = 0;
#ifdef MTK_LPM_MODE_MODULE
	ret = mt6873_dbg_early_initcall();
#endif
	if (ret)
		goto mt6873_dbg_init_fail;

	ret = mt6873_dbg_device_initcall();

	if (ret)
		goto mt6873_dbg_init_fail;

#ifdef MTK_LPM_MODE_MODULE
	ret = mt6873_dbg_late_initcall();
#endif

	if (ret)
		goto mt6873_dbg_init_fail;

	return 0;
mt6873_dbg_init_fail:
	return -EAGAIN;
}

void __exit mt6873_dbg_exit(void)
{
	mtk_dbg_common_fs_exit();
	mt6873_dbg_fs_exit();
	mtk_cpupm_dbg_exit();
	mt6873_lpm_trace_deinit();
	mt6873_logger_deinit();
}

module_init(mt6873_dbg_init);
module_exit(mt6873_dbg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MT6873 low power debug module");
MODULE_AUTHOR("MediaTek Inc.");
