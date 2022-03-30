// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of_device.h>

#include <mtk_cpupm_dbg.h>
#include <lpm_dbg_common_v1.h>
#include <lpm_module.h>

#include <lpm_dbg_fs_common.h>
#include <lpm_dbg_logger.h>

#include <mt6983_lpm_trace_event.h>
#include <mt6983_dbg_fs.h>
#include <mt6983_dbg_power_gs.h>
#include <mt6983_lpm_logger.h>

static int __init mt6983_dbg_early_initcall(void)
{
	return 0;
}
#ifndef MTK_LPM_MODE_MODULE
subsys_initcall(mt6983_dbg_early_initcall);
#endif

static int __init mt6983_dbg_device_initcall(void)
{
	mt6983_dbg_ops_register();
	lpm_dbg_common_fs_init();
	mt6983_dbg_fs_init();
	mtk_cpupm_dbg_init();
#if IS_ENABLED(CONFIG_MTK_LPM_GS_DUMP_SUPPORT)
	mt6983_power_gs_init();
#endif
	return 0;
}

static int __init mt6983_dbg_late_initcall(void)
{
	mt6983_lpm_trace_init();
	lpm_logger_init();
	return 0;
}
#ifndef MTK_LPM_MODE_MODULE
late_initcall_sync(mt6983_dbg_late_initcall);
#endif

int __init mt6983_dbg_init(void)
{
	int ret = 0;
#ifdef MTK_LPM_MODE_MODULE
	ret = mt6983_dbg_early_initcall();
#endif
	if (ret)
		goto mt6983_dbg_init_fail;

	ret = mt6983_dbg_device_initcall();

	if (ret)
		goto mt6983_dbg_init_fail;

#ifdef MTK_LPM_MODE_MODULE
	ret = mt6983_dbg_late_initcall();
#endif

	if (ret)
		goto mt6983_dbg_init_fail;

	ret = lpm_dbg_pm_init();

	if (ret)
		goto mt6983_dbg_init_fail;

	return 0;
mt6983_dbg_init_fail:
	return -EAGAIN;
}

void __exit mt6983_dbg_exit(void)
{
#if IS_ENABLED(CONFIG_MTK_LPM_GS_DUMP_SUPPORT)
	mt6983_power_gs_deinit();
#endif
	lpm_dbg_pm_exit();
	lpm_dbg_common_fs_exit();
	mt6983_dbg_fs_exit();
	mtk_cpupm_dbg_exit();
	mt6983_lpm_trace_deinit();
	lpm_logger_deinit();
}

module_init(mt6983_dbg_init);
module_exit(mt6983_dbg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("mt6983 low power debug module");
MODULE_AUTHOR("MediaTek Inc.");
