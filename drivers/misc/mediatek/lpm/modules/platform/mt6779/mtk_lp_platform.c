// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>

#include "mtk_lp_plat_apmcu.h"

static int __init mtk_lp_plat_init_mt6779(void)
{
	mtk_lp_plat_apmcu_init();

	return 0;
}
late_initcall_sync(mtk_lp_plat_init_mt6779);

static int __init mtk_lp_plat_early_init_mt6779(void)
{
	mtk_lp_plat_apmcu_early_init();

	return 0;
}
subsys_initcall(mtk_lp_plat_early_init_mt6779);
