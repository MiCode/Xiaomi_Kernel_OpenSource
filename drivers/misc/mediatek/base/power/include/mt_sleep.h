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

#ifndef __MT_SLEEP_H__
#define __MT_SLEEP_H__

#if defined(CONFIG_ARCH_MT6755) || defined(CONFIG_ARCH_MT6757) || defined(CONFIG_ARCH_MT6797)

#include "spm_v2/mt_sleep.h"

#elif defined(CONFIG_ARCH_MT6735) || defined(CONFIG_ARCH_MT6735M) || defined(CONFIG_ARCH_MT6753)

#include "../mt6735/mt_sleep.h"

#elif defined(CONFIG_ARCH_MT6570) || defined(CONFIG_ARCH_MT6580)

#include "spm_v1/mt_sleep.h"

#elif defined(CONFIG_ARCH_ELBRUS)

#include "spm_v3/mt_sleep.h"

#endif

#endif /* __MT_SLEEP_H__ */

