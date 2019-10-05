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
//BRISKET base_addr
#ifdef BRISKET_KRUG
#define CPU6_CONFIG_REG	(0x0C533000)
#define CPU7_CONFIG_REG	(0x0C533800)
#endif

#ifdef BRISKET_PETRUS
#define CPU4_CONFIG_REG	(0x0C532000)
#define CPU5_CONFIG_REG	(0x0C532800)
#define CPU6_CONFIG_REG	(0x0C533000)
#define CPU7_CONFIG_REG	(0x0C533800)
#endif


#define MCUSYS_REG_BASE_ADDR	(0x0C530000)
#define MCUSYS_RESERVED_REG0	(MCUSYS_REG_BASE_ADDR + 0xFFE0)

//BRISKET control register offset
#define BRISKET_CONTROL	(0x310)
#define BRISKET01		(0x600)
#define BRISKET02		(0x604)
#define BRISKET03		(0x608)
#define BRISKET04		(0x60C)
#define BRISKET05		(0x610)
#define BRISKET06		(0x614)
#define BRISKET07		(0x618)
#define BRISKET08		(0x61C)
#define BRISKET09		(0x620)
#define BRISKET_END		(0x624)
#define BRISKET_REG_SET	(10) // total sets of brisket control register

//BRISKET control register range
//BRISKET_CONTROL config
#define BRISKET_CONTROL_Pllclken	(0:0)
//BRISKET01 config
#define BRISKET01_ErrOnline			(23:13)
#define BRISKET01_ErrOffline		(12:2)
#define BRISKET01_fsm_state			(1:0)
//BRISKET02 config
#define BRISKET02_cctrl_ping		(23:18)
#define BRISKET02_fctrl_ping		(17:12)
#define BRISKET02_cctrl_pong		(11:6)
#define BRISKET02_fctrl_pong		(5:0)
//BRISKET03 config
#define BRISKET03_InFreq			(23:16)
#define BRISKET03_OutFreq			(15:8)
#define BRISKET03_CalFreq			(7:0)
//BRISKET04 config
#define BRISKET04_Status			(25:18)
#define BRISKET04_PhaseErr			(17:2)
#define BRISKET04_LockDet			(1:1)
#define BRISKET04_Clk26mDet			(0:0)
//BRISKET05 config
#define BRISKET05_Bren				(20:20)
#define BRISKET05_KpOnline			(19:16)
#define BRISKET05_KiOnline			(15:10)
#define BRISKET05_KpOffline			(9:6)
#define BRISKET05_KiOffline			(5:0)
//BRISKET06 config
#define BRISKET06_FreqErrWtOnline	(15:10)
#define BRISKET06_FreqErrWtOffline	(9:4)
#define BRISKET06_PhaseErrWt		(3:0)
//BRISKET07 config
#define BRISKET07_FreqErrCapOnline	(11:8)
#define BRISKET07_FreqErrCapOffline	(7:4)
#define BRISKET07_PhaseErrCap		(3:0)
//BRISKET08 config
#define BRISKET08_PingMaxThreshold	(12:7)
#define BRISKET08_PongMinThreshold	(6:1)
#define BRISKET08_StartInPong		(0:0)
//BRISKET09 config
#define BRISKET09_PhlockThresh		(13:11)
#define BRISKET09_PhlockCycles		(10:8)
#define BRISKET09_Control			(7:0)


#ifdef BRISKET_KRUG
static const unsigned int BRISKET_CPU_BASE[NR_BRISKET_CPU] = {
	CPU6_CONFIG_REG,
	CPU7_CONFIG_REG
};
#endif

#ifdef BRISKET_PETRUS
static const unsigned int BRISKET_CPU_BASE[NR_BRISKET_CPU] = {
	CPU4_CONFIG_REG,
	CPU5_CONFIG_REG,
	CPU6_CONFIG_REG,
	CPU7_CONFIG_REG
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
#define MTK_SIP_KERNEL_BRISKET_CONTROL				0xC2000340
#else
#define MTK_SIP_KERNEL_BRISKET_CONTROL				0x82000340
#endif
#endif //_MTK_BRISKET_
