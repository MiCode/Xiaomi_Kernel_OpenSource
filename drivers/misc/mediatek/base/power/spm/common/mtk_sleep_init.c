/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <mtk_spm.h>
#include <mtk_sleep_internal.h>

int __attribute__((weak)) mtk_spm_init(void)
{
	pr_info("%s not implemented\n", __func__);
	return 0;
}

static int __init mtk_sleep_init(void)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	slp_module_init();
	mtk_spm_init();
#endif
	return 0;
}

late_initcall(mtk_sleep_init);
