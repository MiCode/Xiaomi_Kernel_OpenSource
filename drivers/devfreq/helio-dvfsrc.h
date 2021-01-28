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
#endif
#endif
