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

#ifndef _CCU_SW_VER_H_
#define _CCU_SW_VER_H_

#include <mt-plat/mtk_chip.h>

extern enum chip_sw_ver g_ccu_sw_version;

int init_check_sw_ver(void);

#endif

#endif
