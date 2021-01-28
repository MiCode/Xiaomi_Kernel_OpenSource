/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


#if defined(CONFIG_MACH_MT6768) || defined(CONFIG_MACH_MT6765)
#include <helio-dvfsrc_v2.h>
#elif defined(CONFIG_MACH_MT6785)
#include <helio-dvfsrc_v3.h>
#else
#include <helio-dvfsrc_v1.h>
#endif
