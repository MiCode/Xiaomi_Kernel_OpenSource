/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef _MTK_CLK_BUF_CTL_H_
#define _MTK_CLK_BUF_CTL_H_

#if defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT3967) || \
	defined(CONFIG_MACH_MT6779) || defined(CONFIG_MACH_MT6768) || \
	defined(CONFIG_MACH_MT6771) || defined(CONFIG_MACH_MT6785)

#include "clkbuf_v1/mtk_clkbuf_ctl.h"

#else

#error NO corresponding project of mtk_clkbuf_ctl.h header file can be found!!!

#endif

#endif /* _MTK_CLK_BUF_CTL_H_ */

