/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_DBG_COMMON_H__
#define __MTK_DBG_COMMON_H__

/* For sysfs get the last wake status */
enum mt6885_spm_wake_status_enum {
	WAKE_STA_R12,
	WAKE_STA_R12_EXT,
	WAKE_STA_RAW_STA,
	WAKE_STA_RAW_EXT_STA,
	WAKE_STA_WAKE_MISC,
	WAKE_STA_TIMER_OUT,
	WAKE_STA_R13,
	WAKE_STA_IDLE_STA,
	WAKE_STA_REQ_STA,
	WAKE_STA_DEBUG_FLAG,
	WAKE_STA_DEBUG_FLAG1,
	WAKE_STA_ISR,

	WAKE_STA_MAX_COUNT,
};

enum {
	WR_NONE = 0,
	WR_UART_BUSY = 1,
	WR_ABORT = 2,
	WR_PCM_TIMER = 3,
	WR_WAKE_SRC = 4,
	WR_DVFSRC = 5,
	WR_PMSR = 6,
	WR_TWAM = 7,
	WR_SPM_ACK_CHK = 8,
	WR_UNKNOWN = 9,
};


/* enum for smc resource request arg */
enum MT_SPM_RES_TYPE {
	MT_SPM_RES_XO_FPM,
	MT_SPM_RES_CK_26M,
	MT_SPM_RES_INFRA,
	MT_SPM_RES_SYSPLL,
	MT_SPM_RES_DRAM_S0,
	MT_SPM_RES_DRAM_S1,
	MT_SPM_RES_MAX,
};

enum dbg_ctrl_enum {
	DBG_CTRL_COUNT,
	DBG_CTRL_DURATION,
	DBG_CTRL_MAX,
};

struct dbg_ctrl {
	u32 count;
	u32 duration;
};

/* Determine for operand bit */
#define MTK_DUMP_LP_GOLDEN	(1 << 0L)
#define MTK_DUMP_GPIO		(1 << 1L)

extern void mtk_suspend_gpio_dbg(void);
extern void mtk_suspend_clk_dbg(void);
extern u32 mt_irq_get_pending(unsigned int irq);

#endif /* __MTK_DBG_COMMON_H__ */
