// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _POWER_CLOCK_API_H_
#define _POWER_CLOCK_API_H_

#include <linux/clk.h>

#include "apu_log.h"

#if 1
extern unsigned int mt_get_ckgen_freq(unsigned int ID);
extern unsigned int mt_get_abist_freq(unsigned int ID);
#else
static inline unsigned int mt_get_ckgen_freq(unsigned int ID) { return 0; }
static inline unsigned int mt_get_abist_freq(unsigned int ID) { return 0; }
#endif

/**********************************************
 * macro for clock management operation
 **********************************************/

#define PREPARE_CLK(clk) \
	{ \
		clk = devm_clk_get(dev, #clk); \
		if (IS_ERR(clk)) { \
			ret = -ENOENT; \
			LOG_ERR("can not find clk: %s\n", #clk); \
		} \
	}

#define UNPREPARE_CLK(clk) \
	{ \
		if (clk != NULL) \
			clk = NULL; \
	}


#define ENABLE_CLK(clk) \
	do { \
		ret = clk_prepare_enable(clk); \
		if (ret) { \
			LOG_ERR("fail to prepare & enable clk:%s\n", #clk); \
			apu_aee_warn("APUPWR_CLK_EN_FAIL", "clk:%s\n", #clk); \
		} \
		ret_all |= ret; \
	} while (0)

#define DISABLE_CLK(clk) \
	clk_disable_unprepare(clk)

#endif // _POWER_CLOCK_API_H_
