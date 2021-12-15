/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __HELIO_DVFSRC_H
#define __HELIO_DVFSRC_H
#if IS_ENABLED(CONFIG_MTK_DVFSRC)
extern void dvfsrc_enable_dvfs_freq_hopping(int on);
#else
static inline void dvfsrc_enable_dvfs_freq_hopping(int on)
{ }
#if defined(CONFIG_MACH_MT6768)
#include <helio-dvfsrc_v2.h>
#elif defined(CONFIG_MACH_MT6785) || defined(CONFIG_MACH_MT6885)
#include <helio-dvfsrc_v3.h>
#elif defined(CONFIG_MACH_MT6873)
#include <helio-dvfsrc_v3.h>
#elif defined(CONFIG_MACH_MT6853)
#include <helio-dvfsrc_v3.h>
#elif defined(CONFIG_MACH_MT6893)
#include <helio-dvfsrc_v3.h>
#elif defined(CONFIG_MACH_MT6833)
#include <helio-dvfsrc_v3.h>
#elif defined(CONFIG_MACH_MT6877)
#include <helio-dvfsrc_v3.h>
#elif defined(CONFIG_MACH_MT6781)
#include <helio-dvfsrc_v3.h>
#else
#include <helio-dvfsrc_v1.h>
#endif
#endif
#endif
