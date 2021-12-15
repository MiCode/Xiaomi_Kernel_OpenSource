/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#ifndef __MTK_STATIC_POWER_H__
#define __MTK_STATIC_POWER_H__

#include <linux/types.h>

/* #define MTK_SPOWER_UT */
#if defined(CONFIG_MACH_MT6759)
#include "mtk_static_power_mt6759.h"
#endif

#if defined(CONFIG_MACH_MT6768)
#include "mtk_static_power_mt6768.h"
#endif

#if defined(CONFIG_MACH_MT6763)
#include "mtk_static_power_mt6763.h"
#endif

#if defined(CONFIG_MACH_MT6758)
#include "mtk_static_power_mt6758.h"
#endif

#if defined(CONFIG_MACH_MT6739)
#include "mtk_static_power_mt6739.h"
#endif

#if defined(CONFIG_MACH_MT6765)
#include "mtk_static_power_mt6765.h"
#endif

#if defined(CONFIG_MACH_MT6761)
#include "mtk_static_power_mt6761.h"
#endif

#if defined(CONFIG_MACH_MT3967)
#include "mtk_static_power_mt3967.h"
#endif

#if defined(CONFIG_MACH_MT6779)
#include "mtk_static_power_mt6779.h"
#endif

#if defined(CONFIG_MACH_MT6781)
#include "mtk_static_power_mt6781.h"
#endif

#if defined(CONFIG_MACH_MT6833)
#include "mtk_static_power_mt6833.h"
#endif
#if defined(CONFIG_MACH_MT6877)
#include "mtk_static_power_mt6877.h"
#endif
#if defined(CONFIG_MACH_MT6781)
#include "mtk_static_power_mt6781.h"
#endif
#undef  BIT
#define BIT(bit)	(1U << (bit))

#define MSB(range)	(1 ? range)
#define LSB(range)	(0 ? range)
#define GET_BITS_VAL(_bits_, _val_) \
	(((_val_) & (BITMASK(_bits_))) >> ((0) ? _bits_))

extern u32 get_devinfo_with_index(u32 index);

/*
 * @argument
 * dev: the enum of MT_SPOWER_xxx
 * voltage: the operating voltage, mV.
 * degree: the Tj. (degree C)
 * @return
 *  -1, means sptab is not yet ready.
 *  other value: the mW of leakage value.
 */
extern int mt_spower_get_leakage(int dev, unsigned int voltage, int degree);
extern int mt_spower_get_efuse_lkg(int dev);

extern int mt_spower_init(void);

#endif
