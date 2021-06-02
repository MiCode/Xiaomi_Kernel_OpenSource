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
#define BRISKET_PETRUS

#ifdef BRISKET_KRUG
#define NR_BRISKET_CPU 2
#define BRISKET_CPU_START_ID 6
#define BRISKET_CPU_END_ID 7
#endif

#ifdef BRISKET_PETRUS
#define NR_BRISKET_CPU 4
#define BRISKET_CPU_START_ID 4
#define BRISKET_CPU_END_ID 7
#endif

/************************************************
 * Register addr, offset, bits range
 ************************************************/
/* BRISKET base_addr */

#ifdef BRISKET_KRUG
#define BRISKET_CPU6_CONFIG_REG	(0x0C533000)
#define BRISKET_CPU7_CONFIG_REG	(0x0C533800)
#endif

#ifdef BRISKET_PETRUS
#define BRISKET_CPU4_CONFIG_REG	(0x0C532000)
#define BRISKET_CPU5_CONFIG_REG	(0x0C532800)
#define BRISKET_CPU6_CONFIG_REG	(0x0C533000)
#define BRISKET_CPU7_CONFIG_REG	(0x0C533800)
#endif

/* BRISKET control register offset */
#define BRISKET_CONTROL_OFFSET	(0x310)
#define BRISKET01_OFFSET		(0x600)
#define BRISKET02_OFFSET		(0x604)
#define BRISKET03_OFFSET		(0x608)
#define BRISKET04_OFFSET		(0x60C)
#define BRISKET05_OFFSET		(0x610)
#define BRISKET06_OFFSET		(0x614)
#define BRISKET07_OFFSET		(0x618)
#define BRISKET08_OFFSET		(0x61C)
#define BRISKET09_OFFSET		(0x620)

/* BRISKET control register range */
/* BRISKET_CONTROL config */
#define BRISKET_CONTROL_BITS_Pllclken		1
/* BRISKET01 config */
#define BRISKET01_BITS_ErrOnline			11
#define BRISKET01_BITS_ErrOffline			11
#define BRISKET01_BITS_fsm_state			2
/* BRISKET02 config */
#define BRISKET02_BITS_cctrl_ping			6
#define BRISKET02_BITS_fctrl_ping			6
#define BRISKET02_BITS_cctrl_pong			6
#define BRISKET02_BITS_fctrl_pong			6
/* BRISKET03 config */
#define BRISKET03_BITS_InFreq				8
#define BRISKET03_BITS_OutFreq				8
#define BRISKET03_BITS_CalFreq				8
/* BRISKET04 config */
#define BRISKET04_BITS_Status				8
#define BRISKET04_BITS_PhaseErr				16
#define BRISKET04_BITS_LockDet				1
#define BRISKET04_BITS_Clk26mDet			1
/* BRISKET05 config */
#define BRISKET05_BITS_Bren					1
#define BRISKET05_BITS_KpOnline				4
#define BRISKET05_BITS_KiOnline				6
#define BRISKET05_BITS_KpOffline			4
#define BRISKET05_BITS_KiOffline			6
/* BRISKET06 config */
#define BRISKET06_BITS_FreqErrWtOnline		6
#define BRISKET06_BITS_FreqErrWtOffline		6
#define BRISKET06_BITS_PhaseErrWt			4
/* BRISKET07 config */
#define BRISKET07_BITS_FreqErrCapOnline		4
#define BRISKET07_BITS_FreqErrCapOffline	4
#define BRISKET07_BITS_PhaseErrCap			4
/* BRISKET08 config */
#define BRISKET08_BITS_PingMaxThreshold		6
#define BRISKET08_BITS_PongMinThreshold		6
#define BRISKET08_BITS_StartInPong			1
/* BRISKET09 config */
#define BRISKET09_BITS_PhlockThresh			3
#define BRISKET09_BITS_PhlockCycles			3
#define BRISKET09_BITS_Control				8

/* BRISKET_CONTROL config */
#define BRISKET_CONTROL_SHIFT_Pllclken		0
/* BRISKET01 config */
#define BRISKET01_SHIFT_ErrOnline			13
#define BRISKET01_SHIFT_ErrOffline			2
#define BRISKET01_SHIFT_fsm_state			0
/* BRISKET02 config */
#define BRISKET02_SHIFT_cctrl_ping			18
#define BRISKET02_SHIFT_fctrl_ping			12
#define BRISKET02_SHIFT_cctrl_pong			6
#define BRISKET02_SHIFT_fctrl_pong			0
/* BRISKET03 config */
#define BRISKET03_SHIFT_InFreq				16
#define BRISKET03_SHIFT_OutFreq				8
#define BRISKET03_SHIFT_CalFreq				0
/* BRISKET04 config */
#define BRISKET04_SHIFT_Status				18
#define BRISKET04_SHIFT_PhaseErr			2
#define BRISKET04_SHIFT_LockDet				1
#define BRISKET04_SHIFT_Clk26mDet			0
/* BRISKET05 config */
#define BRISKET05_SHIFT_Bren				20
#define BRISKET05_SHIFT_KpOnline			16
#define BRISKET05_SHIFT_KiOnline			10
#define BRISKET05_SHIFT_KpOffline			6
#define BRISKET05_SHIFT_KiOffline			0
/* BRISKET06 config */
#define BRISKET06_SHIFT_FreqErrWtOnline		10
#define BRISKET06_SHIFT_FreqErrWtOffline	4
#define BRISKET06_SHIFT_PhaseErrWt			0
/* BRISKET07 config */
#define BRISKET07_SHIFT_FreqErrCapOnline	8
#define BRISKET07_SHIFT_FreqErrCapOffline	4
#define BRISKET07_SHIFT_PhaseErrCap			0
/* BRISKET08 config */
#define BRISKET08_SHIFT_PingMaxThreshold	7
#define BRISKET08_SHIFT_PongMinThreshold	1
#define BRISKET08_SHIFT_StartInPong			0
/* BRISKET09 config */
#define BRISKET09_SHIFT_PhlockThresh		11
#define BRISKET09_SHIFT_PhlockCycles		8
#define BRISKET09_SHIFT_Control				0


#ifdef BRISKET_KRUG
static const unsigned int BRISKET_CPU_BASE[NR_BRISKET_CPU] = {
	BRISKET_CPU6_CONFIG_REG,
	BRISKET_CPU7_CONFIG_REG
};
#endif

#ifdef BRISKET_PETRUS
static const unsigned int BRISKET_CPU_BASE[NR_BRISKET_CPU] = {
	BRISKET_CPU4_CONFIG_REG,
	BRISKET_CPU5_CONFIG_REG,
	BRISKET_CPU6_CONFIG_REG,
	BRISKET_CPU7_CONFIG_REG
};
#endif
#define brisket_per_cpu(cpu, reg) (BRISKET_CPU_BASE[cpu] + reg)

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
