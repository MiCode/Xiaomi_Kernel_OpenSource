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
#ifndef _MTK_CREDIT_DIDT_
#define _MTK_CREDIT_DIDT_

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <mt-plat/sync_write.h>
#endif

#if defined(_MTK_CREDIT_DIDT_)
#include <mt-plat/mtk_secure_api.h>
#if defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI)
#define mt_secure_call_credit_didt	mt_secure_call
#else
/* This is workaround for credit_didt use */
static noinline int mt_secure_call_credit_didt(u64 function_id,
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
#define BIT(_bit_)                    (unsigned int)(1 << (_bit_))
#define BITS(_bits_, _val_) \
	((((unsigned int) -1 >> (31 - ((1) ? _bits_))) \
	& ~((1U << ((0) ? _bits_)) - 1)) \
	& ((_val_)<<((0) ? _bits_)))
#define BITMASK(_bits_) \
	(((unsigned int) -1 >> (31 - ((1) ? _bits_))) \
	& ~((1U << ((0) ? _bits_)) - 1))
#define GET_BITS_VAL(_bits_, _val_) \
	(((_val_) & (BITMASK(_bits_))) >> ((0) ? _bits_))

/************************************************
 * Need maintain for each project
 ************************************************/
/* Core ID control by project */
#define CREDIT_PETRUS

#ifdef CREDIT_KRUG
#define NR_CREDIT_DIDT_CPU 2
#define CREDIT_DIDT_CPU_START_ID 6
#define CREDIT_DIDT_CPU_END_ID 7
#endif

#ifdef CREDIT_PETRUS
#define NR_CREDIT_DIDT_CPU 4
#define CREDIT_DIDT_CPU_START_ID 4
#define CREDIT_DIDT_CPU_END_ID 7
#endif


/************************************************
 * Register addr, offset, bits range
 ************************************************/

/* CREDIT_DIDT AO control register */
#define CREDIT_DIDT_MCUSYS_REG_BASE_ADDR \
	(0x0C530000)
#define CREDIT_DIDT_CPU4_DIDT_REG \
	(CREDIT_DIDT_MCUSYS_REG_BASE_ADDR + 0xB830)
#define CREDIT_DIDT_CPU5_DIDT_REG \
	(CREDIT_DIDT_MCUSYS_REG_BASE_ADDR + 0xBA30)
#define CREDIT_DIDT_CPU6_DIDT_REG \
	(CREDIT_DIDT_MCUSYS_REG_BASE_ADDR + 0xBC30)
#define CREDIT_DIDT_CPU7_DIDT_REG \
	(CREDIT_DIDT_MCUSYS_REG_BASE_ADDR + 0xBE30)
#define CREDIT_DIDT_MCUSYS_RESERVED_REG0 \
	(CREDIT_DIDT_MCUSYS_REG_BASE_ADDR + 0xFFE0)
#define CREDIT_DIDT_CPU0_RESEREVED_REG \
	(CREDIT_DIDT_MCUSYS_REG_BASE_ADDR + 0xB1F8)
#define CREDIT_DIDT_CPU1_RESEREVED_REG \
	(CREDIT_DIDT_MCUSYS_REG_BASE_ADDR + 0xB3F8)
#define CREDIT_DIDT_CPU2_RESEREVED_REG \
	(CREDIT_DIDT_MCUSYS_REG_BASE_ADDR + 0xB5F8)

#define WANTS_TO_ENTER_MCUPM \
	(CREDIT_DIDT_CPU0_RESEREVED_REG + 0)
#define WANTS_TO_ENTER_CORE \
	(CREDIT_DIDT_CPU1_RESEREVED_REG + 0)
#define TURN \
	(CREDIT_DIDT_CPU2_RESEREVED_REG + 0)

#ifdef CREDIT_KRUG
static const unsigned int CREDIT_DIDT_CPU_AO_BASE[NR_CREDIT_DIDT_CPU] = {
	CREDIT_DIDT_CPU6_DIDT_REG,
	CREDIT_DIDT_CPU7_DIDT_REG
};
#endif

#ifdef CREDIT_PETRUS
static const unsigned int CREDIT_DIDT_CPU_AO_BASE[NR_CREDIT_DIDT_CPU] = {
	CREDIT_DIDT_CPU4_DIDT_REG,
	CREDIT_DIDT_CPU5_DIDT_REG,
	CREDIT_DIDT_CPU6_DIDT_REG,
	CREDIT_DIDT_CPU7_DIDT_REG
};
#endif

#define CREDIT_DIDT_BITS_RG_HACK_EN			1
#define CREDIT_DIDT_BITS_LS_CTRL_EN			1
#define CREDIT_DIDT_BITS_VX_CTRL_EN_CORE4	1
#define CREDIT_DIDT_BITS_VX_CTRL_EN_CORE5	1
#define CREDIT_DIDT_BITS_VX_CTRL_EN_CORE6	1
#define CREDIT_DIDT_BITS_VX_CTRL_EN_CORE7	1

#define CREDIT_DIDT_SHIFT_RG_HACK_EN		1
#define CREDIT_DIDT_SHIFT_LS_CTRL_EN		15
#define CREDIT_DIDT_SHIFT_VX_CTRL_EN_CORE4	0
#define CREDIT_DIDT_SHIFT_VX_CTRL_EN_CORE5	1
#define CREDIT_DIDT_SHIFT_VX_CTRL_EN_CORE6	2
#define CREDIT_DIDT_SHIFT_VX_CTRL_EN_CORE7	3


/* CREDIT_DIDT control register */
#define CREDIT_DIDT_CPU4_CONFIG_REG	(0x0C532000)
#define CREDIT_DIDT_CPU5_CONFIG_REG	(0x0C532800)
#define CREDIT_DIDT_CPU6_CONFIG_REG	(0x0C533000)
#define CREDIT_DIDT_CPU7_CONFIG_REG	(0x0C533800)

#ifdef CREDIT_KRUG
static const unsigned int CREDIT_DIDT_CPU_BASE[NR_CREDIT_DIDT_CPU] = {
	CREDIT_DIDT_CPU6_CONFIG_REG,
	CREDIT_DIDT_CPU7_CONFIG_REG
};
#endif

#ifdef CREDIT_PETRUS
static const unsigned int CREDIT_DIDT_CPU_BASE[NR_CREDIT_DIDT_CPU] = {
	CREDIT_DIDT_CPU4_CONFIG_REG,
	CREDIT_DIDT_CPU5_CONFIG_REG,
	CREDIT_DIDT_CPU6_CONFIG_REG,
	CREDIT_DIDT_CPU7_CONFIG_REG
};
#endif

#define CREDIT_DIDT_LS_DIDT_CONTROL		(0x314)
#define CREDIT_DIDT_VX_DIDT_CONTROL		(0x318)

#define CREDIT_DIDT_BITS_CFG_CREDIT			5
#define CREDIT_DIDT_BITS_CFG_PERIOD			3
#define CREDIT_DIDT_BITS_CFG_LOW_PERIOD		3
#define CREDIT_DIDT_BITS_CFG_LOW_FREQ_EN	1
#define CREDIT_DIDT_BITS_CFG_IS_LOW_FREQ	1

#define CREDIT_DIDT_SHIFT_CFG_CREDIT		0
#define CREDIT_DIDT_SHIFT_CFG_PERIOD		8
#define CREDIT_DIDT_SHIFT_CFG_LOW_PERIOD	16
#define CREDIT_DIDT_SHIFT_CFG_LOW_FREQ_EN	24
#define CREDIT_DIDT_SHIFT_CFG_IS_LOW_FREQ	25


/************************************************
 * config enum
 ************************************************/
enum CREDIT_DIDT_RW {
	CREDIT_DIDT_RW_READ,
	CREDIT_DIDT_RW_WRITE,

	NR_CREDIT_DIDT_RW,
};

enum CREDIT_DIDT_CHANNEL {
	CREDIT_DIDT_CHANNEL_LS,
	CREDIT_DIDT_CHANNEL_VX,

	NR_CREDIT_DIDT_CHANNEL,
};

enum CREDIT_DIDT_CFG {
	CREDIT_DIDT_CFG_PERIOD,
	CREDIT_DIDT_CFG_CREDIT,
	CREDIT_DIDT_CFG_LOW_PWR_PERIOD,
	CREDIT_DIDT_CFG_LOW_PWR_ENABLE,
	CREDIT_DIDT_CFG_ENABLE,

	NR_CREDIT_DIDT_CFG,
};

enum CREDIT_DIDT_PARAM {

	CREDIT_DIDT_PARAM_LS_PERIOD =
		CREDIT_DIDT_CHANNEL_LS * NR_CREDIT_DIDT_CFG,
	CREDIT_DIDT_PARAM_LS_CREDIT,
	CREDIT_DIDT_PARAM_LS_LOW_PWR_PERIOD,
	CREDIT_DIDT_PARAM_LS_LOW_PWR_ENABLE,
	CREDIT_DIDT_PARAM_LS_ENABLE,

	CREDIT_DIDT_PARAM_VX_PERIOD =
		CREDIT_DIDT_CHANNEL_VX * NR_CREDIT_DIDT_CFG,
	CREDIT_DIDT_PARAM_VX_CREDIT,
	CREDIT_DIDT_PARAM_VX_LOW_PWR_PERIOD,
	CREDIT_DIDT_PARAM_VX_LOW_PWR_ENABLE,
	CREDIT_DIDT_PARAM_VX_ENABLE,

	NR_CREDIT_DIDT_PARAM,
};

/************************************************
 * association with ATF use
 ************************************************/
#ifdef CONFIG_ARM64
#define MTK_SIP_KERNEL_CREDIT_DIDT_CONTROL			0xC200051A
#else
#define MTK_SIP_KERNEL_CREDIT_DIDT_CONTROL			0x8200051A
#endif
#endif //_MTK_CREDIT_DIDT_
