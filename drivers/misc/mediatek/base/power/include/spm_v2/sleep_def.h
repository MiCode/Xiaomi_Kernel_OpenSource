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

#ifndef __SLEEP_DEF_H__
#define __SLEEP_DEF_H__

#if defined(CONFIG_ARCH_MT6755)

#include "sleep_def_mt6755.h"

#elif defined(CONFIG_ARCH_MT6757)

#include "sleep_def_mt6757.h"

#elif defined(CONFIG_ARCH_MT6797)

#include "sleep_def_mt6797.h"

#endif

#endif /* __SLEEP_DEF_H__ */

