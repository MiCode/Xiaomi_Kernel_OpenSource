/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __MT_CLK_ID__H__
#define __MT_CLK_ID__H__

#if defined(CONFIG_ARCH_MT6755)

#include "mt_clk_id_mt6755.h"

#elif defined(CONFIG_ARCH_MT6757)

#include "mt_clk_id_mt6757.h"

#elif defined(CONFIG_ARCH_MT6797)

#include "mt_clk_id_mt6797.h"

#endif

#endif /* __MT_CLK_ID__H__ */

