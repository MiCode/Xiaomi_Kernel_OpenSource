/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef _VPU_POWER_CLOCK_H_
#define _VPU_POWER_CLOCK_H_

#include <linux/clk.h>
#include <mt-plat/mtk_secure_api.h>

#include "apu_log.h"

extern unsigned int mt_get_ckgen_freq(unsigned int ID);
extern void check_vpu_clk_sts(void);

/**********************************************
 * macro for clock management operation
 **********************************************/

/*move vcore cg ctl to atf*/
#define vcore_cg_ctl(poweron) \
	mt_secure_call(MTK_APU_VCORE_CG_CTL, poweron, 0, 0, 0)

#define PREPARE_MTCMOS(clk) \
	{ \
		clk = devm_clk_get(dev, #clk); \
		if (IS_ERR(clk)) { \
			ret = -ENOENT; \
			LOG_ERR("can not find mtcmos: %s\n", #clk); \
		} \
	}

#define PREPARE_CLK(clk) \
	{ \
		clk = devm_clk_get(dev, #clk); \
		if (IS_ERR(clk)) { \
			ret = -ENOENT; \
			LOG_ERR("can not find clock: %s\n", #clk); \
		} else if (clk_prepare(clk)) { \
			ret = -EBADE; \
			LOG_ERR("fail to prepare clock: %s\n", #clk); \
		} \
	}


#define UNPREPARE_CLK(clk) \
	{ \
		if (clk != NULL) { \
			clk_unprepare(clk); \
			clk = NULL; \
		} \
	}

#define ENABLE_MTCMOS(clk) \
	{ \
		if (clk != NULL) { \
			if (clk_prepare_enable(clk)) \
				LOG_ERR("fail to prepare&enable mtcmos:%s\n", \
									#clk); \
		} else { \
			LOG_WRN("mtcmos not existed: %s\n", #clk); \
		} \
	}

#define ENABLE_CLK(clk) \
	{ \
		if (clk != NULL) { \
			if (clk_enable(clk)) \
				LOG_ERR("fail to enable clock: %s\n", #clk); \
		} else { \
			LOG_WRN("clk not existed: %s\n", #clk); \
		} \
	}

#define DISABLE_CLK(clk) \
	{ \
		if (clk != NULL) { \
			clk_disable(clk); \
		} else { \
			LOG_WRN("clk not existed: %s\n", #clk); \
		} \
	}

#define DISABLE_MTCMOS(clk) \
	{ \
		if (clk != NULL) { \
			clk_disable_unprepare(clk); \
		} else { \
			LOG_WRN("mtcmos not existed: %s\n", #clk); \
		} \
	}

#endif // _VPU_POWER_CLOCK_H_
