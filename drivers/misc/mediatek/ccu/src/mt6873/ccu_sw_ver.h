// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifdef CONFIG_MTK_CHIP

#ifndef _CCU_SW_VER_H_
#define _CCU_SW_VER_H_

#include <mt-plat/mtk_chip.h>

extern enum chip_sw_ver g_ccu_sw_version;

int init_check_sw_ver(void);

#endif

#endif
