// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>

#include "fm_drv_init.h"

#ifdef CONFIG_MTK_FMRADIO
int __attribute__((weak)) mtk_wcn_fm_init()
{
	pr_err("no impl. mtk_wcn_fm_init\n");
	return 0;
}
#endif

int do_fm_drv_init(int chip_id)
{
	pr_info("Start to do fm module init\n");

#ifdef CONFIG_MTK_FMRADIO
	mtk_wcn_fm_init();
#endif

	pr_info("Finish fm module init\n");
	return 0;
}
