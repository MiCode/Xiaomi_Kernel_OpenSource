/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __MTK_STATIC_POWER_H__
#define __MTK_STATIC_POWER_H__

#include <linux/types.h>

/* #define MTK_SPOWER_UT */
#if defined(CONFIG_MACH_MT6759)
#include "mtk_static_power_mt6759.h"
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

#if defined(CONFIG_MACH_MT6771)
#include "mtk_static_power_mt6771.h"
#endif

#if defined(CONFIG_MACH_MT6775)
#include "mtk_static_power_mt6775.h"
#endif

#if defined(CONFIG_MACH_MT6768)
#include "mtk_static_power_mt6768.h"
#endif

#if defined(CONFIG_MACH_MT6785)
#include "mtk_static_power_mt6785.h"
#endif

#if defined(CONFIG_MACH_MT6885)
#include "mtk_static_power_mt6885.h"
#endif

/*
 * bit operation
 */
#undef  BIT
#define BIT(bit)	(1U << (bit))

#define MSB(range)	(1 ? range)
#define LSB(range)	(0 ? range)
/**
 * Genearte a mask wher MSB to LSB are all 0b1
 * @r:	Range in the form of MSB:LSB
 */
#define BITMASK(r)	\
	(((unsigned int) -1 >> (31 - MSB(r))) & ~((1U << LSB(r)) - 1))

/**
 * Set value at MSB:LSB. For example, BITS(7:3, 0x5A)
 * will return a value where bit 3 to bit 7 is 0x5A
 * @r:	Range in the form of MSB:LSB
 */
/* BITS(MSB:LSB, value) => Set value at MSB:LSB  */
#define BITS(r, val)	((val << LSB(r)) & BITMASK(r))

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
