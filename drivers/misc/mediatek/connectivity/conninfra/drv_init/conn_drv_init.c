// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>

#include "conn_drv_init.h"
#include "fm_drv_init.h"
#include "wlan_drv_init.h"
#include "bluetooth_drv_init.h"
#include "gps_drv_init.h"

#ifdef MTK_WCN_REMOVE_KERNEL_MODULE
int do_connectivity_driver_init(int chip_id)
{
	int i_ret = 0;
	int tmp_ret = 0;
	static int init_before = 0;

	/* To avoid invoking more than once.*/
	if (init_before)
		return 0;
	init_before = 1;

	tmp_ret = do_bluetooth_drv_init(chip_id);
	i_ret += tmp_ret;
	if (tmp_ret)
		pr_err("Do BT driver init failed, ret:%d\n", tmp_ret);

	tmp_ret = do_gps_drv_init(chip_id);
	i_ret += tmp_ret;
	if (tmp_ret)
		pr_err("Do GPS driver init failed, ret:%d\n", tmp_ret);

	tmp_ret = do_fm_drv_init(chip_id);
	i_ret += tmp_ret;
	if (tmp_ret)
		pr_err("Do FM module init failed, ret:%d\n", tmp_ret);

	tmp_ret = do_wlan_drv_init(chip_id);
	i_ret += tmp_ret;
	if (tmp_ret)
		pr_err("Do wlan module init failed, ret:%d\n", tmp_ret);

	return i_ret;
}
#endif

