/*
 * Copyright (C) 2016 MediaTek Inc.
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
