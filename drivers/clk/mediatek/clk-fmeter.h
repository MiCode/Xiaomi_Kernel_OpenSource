/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __CLK_FMETER_H
#define __CLK_FMETER_H

#if defined(CONFIG_COMMON_CLK_MT6885)
#include "clk-mt6893-fmeter.h"
#elif defined(CONFIG_COMMON_CLK_MT6873)
#include "clk-mt6873-fmeter.h"
#elif defined(CONFIG_COMMON_CLK_MT6853)
#include "clk-mt6853-fmeter.h"
#elif defined(CONFIG_COMMON_CLK_MT6833)
#include "clk-mt6833-fmeter.h"
#elif defined(CONFIG_COMMON_CLK_MT6877)
#include "clk-mt6877-fmeter.h"
#elif defined(CONFIG_COMMON_CLK_MT6893)
#include "clk-mt6893-fmeter.h"
#endif

#define FM_SYS(_id)		((_id & (0xFF00)) >> 8)
#define FM_ID(_id)		(_id & (0xFF))
#endif
