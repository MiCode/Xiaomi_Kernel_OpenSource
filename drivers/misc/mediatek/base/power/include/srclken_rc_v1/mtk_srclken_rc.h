/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
*/

/**
 * @file    mtk_srclken_rc.h
 * @brief   Driver for subys request resource control
 *
 */
#ifndef __MTK_SRCLKEN_RC_H__
#define __MTK_SRCLKEN_RC_H__

#include <linux/kernel.h>
#include <linux/mutex.h>

#if defined(CONFIG_MACH_MT6779)
#include "mt6779/mtk_srclken_rc_hw.h"
#elif defined(CONFIG_MACH_MT6873)
#include "mt6873/mtk_srclken_rc_hw.h"
#elif defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6893)
#include "mt6885/mtk_srclken_rc_hw.h"
#elif defined(CONFIG_MACH_MT6853) || defined(CONFIG_MACH_MT6833)
#include "mt6853/mtk_srclken_rc_hw.h"
#elif defined(CONFIG_MACH_MT6877)
#include "mt6853/mtk_srclken_rc_hw.h"
#endif

enum srclken_config {
	SRCLKEN_BRINGUP = 0,
	SRCLKEN_NOT_SUPPORT,
	SRCLKEN_BT_ONLY,
	SRCLKEN_FULL_SET,
	SRCLKEN_ERR,
};

enum srclken_config srclken_get_stage(void);
bool srclken_get_debug_cfg(void);
void srclken_dump_sta_log(void);
void srclken_dump_cfg_log(void);
void srclken_dump_last_sta_log(void);
int srclken_init(void);

#endif

