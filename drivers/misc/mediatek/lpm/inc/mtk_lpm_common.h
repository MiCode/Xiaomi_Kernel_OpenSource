/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_LPM_COMMON_H__
#define __MTK_LPM_COMMON_H__

#include <linux/types.h>
#include <linux/notifier.h>

#define MT_LP_RQ_XO_FPM         (1 << 0L)
#define MT_LP_RQ_26M            (1 << 1L)
#define MT_LP_RQ_INFRA          (1 << 2L)
#define MT_LP_RQ_SYSPLL         (1 << 3L)
#define MT_LP_RQ_DRAM_S0        (1 << 4L)
#define MT_LP_RQ_DRAM_S1        (1 << 5L)
#define MT_LP_RQ_ALL		((1 << 6L) - 1)

enum MTK_LPM_NB_EVENT {
	MTK_LPM_NOTIFY_DRAM_OFF,
	MTK_LPM_NOTIFY_DRAM_ON,
	MTK_LPM_NOTIFY_MAINPLL_OFF,
	MTK_LPM_NOTIFY_MAINPLL_ON,
	MTK_LPM_NOTIFY_26M_OFF,
	MTK_LPM_NOTIFY_26M_ON,
};

extern int mtk_lpm_event_notifier_register(
					struct notifier_block *n);
extern int mtk_lpm_event_notifier_unregister(
					struct notifier_block *n);

#endif

