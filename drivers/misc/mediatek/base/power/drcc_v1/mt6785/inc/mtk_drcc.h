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
#ifndef _MTK_DRCC_
#define _MTK_DRCC_

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <mt-plat/sync_write.h>
#endif

#if defined(__MTK_DRCC_C__)
#include <mt-plat/mtk_secure_api.h>
#if defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI)
#define mt_secure_call_drcc	mt_secure_call
#else
/* This is workaround for drcc use */
static noinline int mt_secure_call_drcc(u64 function_id,
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

#define DRCC_AO_REG_BASE 0x0C53B000
#define DRCC_BASEADDR 0x0C530000
#define DRCC_CONF0 (DRCC_BASEADDR + 0x280)
#define DRCC_CONF1 (DRCC_BASEADDR + 0x284)
#define DRCC_CONF2 (DRCC_BASEADDR + 0x288)
#define DRCC_CONF3 (DRCC_BASEADDR + 0x28C)

#ifdef CONFIG_ARM64
#define MTK_SIP_KERNEL_DRCC_INIT		0xC2000300
#define MTK_SIP_KERNEL_DRCC_ENABLE		0xC2000301
#define MTK_SIP_KERNEL_DRCC_TRIG		0xC2000302
#define MTK_SIP_KERNEL_DRCC_COUNT		0xC2000303
#define MTK_SIP_KERNEL_DRCC_MODE		0xC2000304
#define MTK_SIP_KERNEL_DRCC_CODE		0xC2000305
#define MTK_SIP_KERNEL_DRCC_HWGATEPCT		0xC2000306
#define MTK_SIP_KERNEL_DRCC_VREFFILT		0xC2000307
#define MTK_SIP_KERNEL_DRCC_AUTOCALIBDELAY	0xC2000308
#define MTK_SIP_KERNEL_DRCC_FORCETRIM		0xC2000309
#define MTK_SIP_KERNEL_DRCC_PROTECT		0xC200030A
#define MTK_SIP_KERNEL_DRCC_READ		0xC200030B
#define MTK_SIP_KERNEL_DRCC_K_RST		0xC200030C
#else
#define MTK_SIP_KERNEL_DRCC_INIT		0x82000300
#define MTK_SIP_KERNEL_DRCC_ENABLE		0x82000301
#define MTK_SIP_KERNEL_DRCC_TRIG		0x82000302
#define MTK_SIP_KERNEL_DRCC_COUNT		0x82000303
#define MTK_SIP_KERNEL_DRCC_MODE		0x82000304
#define MTK_SIP_KERNEL_DRCC_CODE		0x82000305
#define MTK_SIP_KERNEL_DRCC_HWGATEPCT		0x82000306
#define MTK_SIP_KERNEL_DRCC_VREFFILT		0x82000307
#define MTK_SIP_KERNEL_DRCC_AUTOCALIBDELAY	0x82000308
#define MTK_SIP_KERNEL_DRCC_FORCETRIM		0x82000309
#define MTK_SIP_KERNEL_DRCC_PROTECT		0x8200030A
#define MTK_SIP_KERNEL_DRCC_READ		0x8200030B
#define MTK_SIP_KERNEL_DRCC_K_RST		0x8200030C
#endif

extern void mtk_drcc_enable(unsigned int onOff, unsigned int drcc_num);
extern int mtk_drcc_feature_enabled_check(void);
/* SRAM debugging */
#ifdef CONFIG_MTK_RAM_CONSOLE
extern void aee_rr_rec_drcc_0(u32 val);
extern void aee_rr_rec_drcc_1(u32 val);
extern void aee_rr_rec_drcc_2(u32 val);
extern void aee_rr_rec_drcc_3(u32 val);
extern void aee_rr_rec_drcc_dbg_info(uint32_t ret,
	uint32_t off, uint64_t ts);

extern u8 aee_rr_curr_drcc_0(void);
extern u8 aee_rr_curr_drcc_1(void);
extern u8 aee_rr_curr_drcc_2(void);
extern u8 aee_rr_curr_drcc_3(void);
#endif

#endif
