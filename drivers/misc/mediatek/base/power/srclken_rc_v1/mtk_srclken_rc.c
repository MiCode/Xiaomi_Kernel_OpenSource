/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*
 * @file    mtk_srclken_rc.c
 * @brief   Driver for subys request resource control
 *
 */
#include <linux/syscore_ops.h>

#include <mtk_spm.h>
#include <mtk_srclken_rc.h>
#include <mtk_srclken_rc_common.h>

bool is_srclken_initiated;

void __attribute__((weak)) srclken_stage_init(void)
{
	pr_info("%s: dummy func\n", __func__);
}

enum srclken_config __attribute__((weak)) srclken_get_stage(void)
{
	pr_info("%s: dummy func\n", __func__);
	return SRCLKEN_NOT_SUPPORT;
}

int __attribute__((weak)) srclken_dts_map(void)
{
	pr_info("%s: dummy func\n", __func__);
	return 0;
}

bool __attribute__((weak)) srclken_get_debug_cfg(void)
{
	pr_info("%s: dummy func\n", __func__);
	return false;
}

void __attribute__((weak)) srclken_dump_cfg_log(void)
{
	pr_info("%s: dummy func\n", __func__);
}

void __attribute__((weak)) srclken_dump_sta_log(void)
{
	pr_info("%s: dummy func\n", __func__);
}

void __attribute__((weak)) srclken_dump_last_sta_log(void)
{
	pr_info("%s: dummy func\n", __func__);
}

int __attribute__((weak)) srclken_fs_init(void)
{
	pr_info("%s: dummy func\n", __func__);
	return 0;
}

static int srclken_chk_syscore_suspend(void)
{
	if (srclken_get_debug_cfg()) {
		srclken_dump_cfg_log();
		srclken_dump_sta_log();
	}

	return 0;
}

static void srclken_chk_syscore_resume(void)
{
	if (srclken_get_debug_cfg()) {
		srclken_dump_cfg_log();
		srclken_dump_sta_log();
		srclken_dump_last_sta_log();
	}
}

static struct syscore_ops srclken_chk_syscore_ops = {
	.suspend = srclken_chk_syscore_suspend,
	.resume = srclken_chk_syscore_resume,
};

int srclken_init(void)
{
	if (srclken_dts_map()) {
		pr_notice("%s: failed due to DTS failed\n", __func__);
		return -1;
	}

	srclken_stage_init();

	if (srclken_get_stage() == SRCLKEN_NOT_SUPPORT)
		return 0;

	if (srclken_get_stage() == SRCLKEN_BRINGUP) {
		srclken_fs_init();
		return 0;
	}

	if (srclken_get_stage() == SRCLKEN_ERR)
		return -1;

	if (is_srclken_initiated)
		return -1;

	srclken_dump_cfg_log();

	if (srclken_fs_init())
		return -1;

	register_syscore_ops(&srclken_chk_syscore_ops);

	is_srclken_initiated = true;

	return 0;
}
fs_initcall_sync(srclken_init);

