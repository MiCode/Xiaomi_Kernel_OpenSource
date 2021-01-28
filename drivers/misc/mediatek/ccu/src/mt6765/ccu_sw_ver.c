/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifdef CONFIG_MTK_CHIP

#include <linux/kernel.h>
#include "ccu_cmn.h"
#include "ccu_sw_ver.h"

enum chip_sw_ver g_ccu_sw_version = -1;

int init_check_sw_ver(void)
{
	g_ccu_sw_version = mt_get_chip_sw_ver();

	if ((g_ccu_sw_version != CHIP_SW_VER_02) &&
		(g_ccu_sw_version != CHIP_SW_VER_01)) {
		LOG_ERR("have a wrong software version:%x!\n",
			g_ccu_sw_version);
		return -EINVAL;
	}

	return 0;
}

#endif
