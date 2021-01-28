/*
 * Copyright (C) 2016 MediaTek Inc.
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
#ifndef _MTK_BRISKET_
#define _MTK_BRISKET_

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <mt-plat/sync_write.h>
#endif

#if defined(_MTK_BRISKET_)
#include <mt-plat/mtk_secure_api.h>
#if defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI)
#define mt_secure_call_brisket	mt_secure_call
#else
/* This is workaround for brisket use */
static noinline int mt_secure_call_brisket(u64 function_id,
				u64 arg0, u64 arg1, u64 arg2, u64 arg3)
{
	register u64 reg0 __asm__("x0") = function_id;
	register u64 reg1 __asm__("x1") = arg0;
	register u64 reg2 __asm__("x2") = arg1;
	register u64 reg3 __asm__("x3") = arg2;
	register u64 reg4 __asm__("x4") = arg3;
	int ret = 0;

	asm volatile(
	"smc    #0\n"
		: "+r" (reg0)
		: "r" (reg1), "r" (reg2), "r" (reg3), "r" (reg4));

	ret = (int)reg0;
	return ret;
}
#endif
#endif

/************************************************
 * BIT Operation
 ************************************************/
#undef  BIT
#define BIT(_bit_) (unsigned int)(1 << (_bit_))
#define BITS(_bits_, _val_) \
	((((unsigned int) -1 >> (31 - ((1) ? _bits_))) \
	& ~((1U << ((0) ? _bits_)) - 1)) & ((_val_)<<((0) ? _bits_)))
#define BITMASK(_bits_) \
	(((unsigned int) -1 >> (31 - ((1) ? _bits_))) \
	& ~((1U << ((0) ? _bits_)) - 1))
#define GET_BITS_VAL(_bits_, _val_) \
	(((_val_) & (BITMASK(_bits_))) >> ((0) ? _bits_))

/************************************************
 * Need maintain for each project
 ************************************************/
/* Core ID control by project */
#define BRISKET_MARGAUX

#ifdef BRISKET_MARGAUX
#define NR_BRISKET_CPU 8
#define BRISKET_CPU_START_ID 0
#define BRISKET_CPU_END_ID 7
#endif

/************************************************
 * config enum
 ************************************************/
enum BRISKET_RW {
	BRISKET_RW_READ,
	BRISKET_RW_WRITE,

	NR_BRISKET_RW,
};

enum BRISKET_GROUP {
	BRISKET_GROUP_CONTROL,
	BRISKET_GROUP_01,
	BRISKET_GROUP_02,
	BRISKET_GROUP_03,
	BRISKET_GROUP_04,
	BRISKET_GROUP_05,
	BRISKET_GROUP_06,
	BRISKET_GROUP_07,
	BRISKET_GROUP_08,
	BRISKET_GROUP_09,

	NR_BRISKET_GROUP,
};

/************************************************
 * association with ATF use
 ************************************************/
#ifdef CONFIG_ARM64
#define MTK_SIP_KERNEL_BRISKET_CONTROL				0xC200051B
#else
#define MTK_SIP_KERNEL_BRISKET_CONTROL				0x8200051B
#endif
#endif //_MTK_BRISKET_
