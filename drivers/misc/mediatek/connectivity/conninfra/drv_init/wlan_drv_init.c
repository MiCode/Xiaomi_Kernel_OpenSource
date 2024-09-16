// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>

#include "wlan_drv_init.h"

int __attribute__((weak)) mtk_wcn_wlan_gen4_init()
{
	pr_info("no impl. mtk_wcn_wlan_gen4_init\n");
	return 0;
}

int __attribute__((weak)) mtk_wcn_wmt_wifi_init()
{
	pr_info("no impl. mtk_wcn_wmt_wifi_init");
	return 0;
}

int do_wlan_drv_init(int chip_id)
{
	int i_ret = 0;
	int ret = 0;

	pr_info("Start to do wlan module init 0x%x\n", chip_id);

	/* WMT-WIFI char dev init */
	ret = mtk_wcn_wmt_wifi_init();
	pr_info("WMT-WIFI char dev init, ret:%d\n", ret);
	i_ret += ret;

	/* WLAN driver init */
	ret = mtk_wcn_wlan_gen4_init();
	pr_info("WLAN-GEN4 driver init, ret:%d\n", ret);

	i_ret += ret;

	pr_info("Finish wlan module init\n");

	return i_ret;
}
