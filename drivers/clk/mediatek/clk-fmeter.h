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

#ifndef __CLK_FMETER_H
#define __CLK_FMETER_H

#if defined(CONFIG_COMMON_CLK_MT6885)
#include "clk-mt6885-fmeter.h"
#elif defined(CONFIG_COMMON_CLK_MT6873)
#include "clk-mt6873-fmeter.h"
#elif defined(CONFIG_COMMON_CLK_MT6853)
#include "clk-mt6853-fmeter.h"
#endif

#endif
