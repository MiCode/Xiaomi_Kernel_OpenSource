// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/console.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscore_ops.h>

static int mtk_lpm_suspend_enter(void)
{
	return -EACCES;
}

static void mtk_lpm_suspend_leave(void)
{
}

static struct syscore_ops mtk_lpm_suspend = {
	.suspend = mtk_lpm_suspend_enter,
	.resume = mtk_lpm_suspend_leave,
};

static int __init mtk_lpm_init(void)
{
	register_syscore_ops(&mtk_lpm_suspend);
	console_suspend_enabled = false;

	return 0;
}
device_initcall(mtk_lpm_init);

