/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#ifndef __CLK_FMETER_H
#define __CLK_FMETER_H

#if IS_ENABLED(CONFIG_COMMON_CLK_MT6893)
#include "clk-mt6893-fmeter.h"
#endif

#define FM_SYS(_id)		((_id & (0xFF00)) >> 8)
#define FM_ID(_id)		(_id & (0xFF))

const struct fmeter_clk *get_fmeter_clks(void);
#endif
