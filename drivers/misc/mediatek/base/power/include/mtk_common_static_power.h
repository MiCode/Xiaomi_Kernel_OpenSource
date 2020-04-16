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

#ifndef _COMMON_MTK_STATIC_POWER_H
#define _COMMON_MTK_STATIC_POWER_H

#if defined(CONFIG_MACH_MT6759) || defined(CONFIG_MACH_MT6763) \
	|| defined(CONFIG_MACH_MT6758) || defined(CONFIG_MACH_MT6739) \
	|| defined(CONFIG_MACH_MT6771) || defined(CONFIG_MACH_MT6775) \
	|| defined(CONFIG_MACH_MT6768) || defined(CONFIG_MACH_MT6785) \
	|| defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6765)
#include "leakage_table_v2/mtk_static_power.h"
#endif

#endif /* _COMMON_MTK_STATIC_POWER_H */

