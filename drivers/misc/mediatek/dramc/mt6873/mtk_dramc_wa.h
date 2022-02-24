// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MTK_DRAMC_WA_H__
#define __MTK_DRAMC_WA_H__

struct mt_gpt_timers {
	int tmr_irq;
	void __iomem *tmr_regs;
};

#define AP_XGPT_BASE	gpt_timers.tmr_regs

#define GPT3_CON            (AP_XGPT_BASE+0x0030)
#define GPT3_CLK            (AP_XGPT_BASE+0x0034)
#define GPT3_CMP_L          (AP_XGPT_BASE+0x003C)
#define GPT3_SPE            (AP_XGPT_BASE+0x0080)
#define GPT_IRQ_EN          (AP_XGPT_BASE+0x0000)
#define GPT_IRQ_ACK         (AP_XGPT_BASE+0x0008)

#define GPT3_ONE_SHOT_EN	0x0001
#define GPT3_RTC_CLK		0x0010
#define GPT3_IRQ_BIT		(1<<2)
#define GPT3_STOP_CLEAR		(1<<1)
#define GPT3_IRQ_SPE		(0<<2)

#endif   /*__MTK_DRAMC_WA_H__*/
