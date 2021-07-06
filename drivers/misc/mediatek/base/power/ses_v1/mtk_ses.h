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

#define MTK_SES_DOE 0


#ifdef __KERNEL__
#include <linux/arm-smccc.h>
#include <linux/kernel.h>
#include <mt-plat/sync_write.h>
#endif

#ifdef CONFIG_ARM64
#define MTK_SIP_SES_CONTROL		0xC200051C
#else
#define MTK_SIP_SES_CONTROL		0x8200051C
#endif

#define MTK_SES_INIT			0
#define MTK_SES_ENABLE			1
#define MTK_SES_COUNT			2
#define MTK_SES_HWGATEPCT		3
#define MTK_SES_STEPSTART		4
#define MTK_SES_STEPTIME		5
#define MTK_SES_DLYFILT			6
#define MTK_SES_STATUS			7
#define MTK_SES_VOLT_RATIO		8
#define MTK_SES_TRIM			9

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

extern void mtk_ses_init(void);
extern void mtk_ses_trim(unsigned int value,
						unsigned int ses_node);
extern void mtk_ses_enable(unsigned int onOff,
						unsigned int ses_node);
extern unsigned int mtk_ses_event_count(unsigned int ByteSel,
						unsigned int ses_node);
extern void mtk_ses_stepstart(unsigned int HwGateSel,
						unsigned int ses_node);
extern void mtk_ses_steptime(unsigned int value,
						unsigned int ses_node);
extern void mtk_ses_dly_filt(unsigned int value,
						unsigned int ses_node);
extern void mtk_ses_hwgatepct(unsigned int HwGateSel,
						unsigned int value,
						unsigned int ses_node);
extern int mtk_ses_volt_ratio(unsigned int HiRatio,
						unsigned int LoRatio,
						unsigned int ses_node);
extern int mtk_ses_volt_ratio_atf(unsigned int Volt,
						unsigned int HiRatio,
						unsigned int LoRatio,
						unsigned int ses_node);
extern unsigned int mtk_ses_status(unsigned int value);

#endif
