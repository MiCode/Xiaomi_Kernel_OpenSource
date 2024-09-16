// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include "bluetooth_drv_init.h"

#ifdef CONFIG_MTK_COMBO_BT
int __attribute__((weak)) mtk_wcn_stpbt_drv_init()
{
	pr_info("Not implement mtk_wcn_stpbt_drv_init\n");
	return 0;
}
#endif

int do_bluetooth_drv_init(int chip_id)
{
	int i_ret = -1;

#ifdef CONFIG_MTK_COMBO_BT
	pr_info("Start to do bluetooth driver init\n");
	i_ret = mtk_wcn_stpbt_drv_init();
	pr_info("Finish bluetooth driver init, i_ret:%d\n", i_ret);
#else
	pr_info("CONFIG_MTK_COMBO_BT is not defined\n");
#endif
	return i_ret;
}
