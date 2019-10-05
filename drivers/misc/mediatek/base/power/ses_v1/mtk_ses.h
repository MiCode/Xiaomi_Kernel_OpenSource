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
#ifndef _MTK_SES_
#define _MTK_SES_

#define MTK_SES_ON 1


#ifdef __KERNEL__
#include <linux/kernel.h>
#include <mt-plat/sync_write.h>
#endif

#if defined(__MTK_SES_C__)
#include <mt-plat/mtk_secure_api.h>
#if defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI)
#define mt_secure_call_ses	mt_secure_call
#else
/* This is workaround for ses use */
static noinline int mt_secure_call_ses(u64 function_id,
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

#ifdef CONFIG_ARM64
#define MTK_SIP_KERNEL_SES_INIT				0xC20003A4
#define MTK_SIP_KERNEL_SES_ENABLE			0xC20003A5
#define MTK_SIP_KERNEL_SES_COUNT			0xC20003A6
#define MTK_SIP_KERNEL_SES_HWGATEPCT		0xC20003A7
#define MTK_SIP_KERNEL_SES_STEPSTART		0xC20003A8
#define MTK_SIP_KERNEL_SES_STEPTIME			0xC20003A9
#define MTK_SIP_KERNEL_SES_DLYFILT			0xC20003AA
#define MTK_SIP_KERNEL_SES_STATUS			0xC20003AB
#else
#define MTK_SIP_KERNEL_SES_INIT				0x820003A4
#define MTK_SIP_KERNEL_SES_ENABLE			0x820003A5
#define MTK_SIP_KERNEL_SES_COUNT			0x820003A6
#define MTK_SIP_KERNEL_SES_HWGATEPCT		0x820003A7
#define MTK_SIP_KERNEL_SES_STEPSTART		0x820003A8
#define MTK_SIP_KERNEL_SES_STEPTIME			0x820003A9
#define MTK_SIP_KERNEL_SES_DLYFILT			0x820003AA
#define MTK_SIP_KERNEL_SES_STATUS			0x820003AB
#endif

#define SES_STATUS_ADDR		(0x0C530000)
#define SESV6_BG_CTRL		(SES_STATUS_ADDR + 0xAC40)
#define SESV6_DSU_REG0		(SES_STATUS_ADDR + 0xAC50)
#define SESV6_DSU_REG1		(SES_STATUS_ADDR + 0xAC54)
#define SESV6_DSU_REG2		(SES_STATUS_ADDR + 0xAC58)
#define SESV6_DSU_REG3		(SES_STATUS_ADDR + 0xAC5C)
#define SESV6_DSU_REG4		(SES_STATUS_ADDR + 0xAC60)
#define SESV6_REG0			(SES_STATUS_ADDR + 0xB010)
#define SESV6_REG1			(SES_STATUS_ADDR + 0xB014)
#define SESV6_REG2			(SES_STATUS_ADDR + 0xB018)
#define SESV6_REG3			(SES_STATUS_ADDR + 0xB01C)
#define SESV6_REG4			(SES_STATUS_ADDR + 0xB020)


extern void mtk_drcc_enable(unsigned int onOff, unsigned int drcc_num);
extern void mtk_ses_init(void);
extern void mtk_ses_enable(unsigned int onOff, unsigned int ses_node);
extern unsigned int mtk_ses_event_count(unsigned int ByteSel,
						unsigned int ses_node);
extern void mtk_ses_hwgatepct(unsigned int HwGateSel,
						unsigned int value,
						unsigned int ses_node);
extern void mtk_ses_stepstart(unsigned int HwGateSel, unsigned int ses_node);
extern void mtk_ses_steptime(unsigned int value, unsigned int ses_node);
extern void mtk_ses_dly_filt(unsigned int value, unsigned int ses_node);


#endif
