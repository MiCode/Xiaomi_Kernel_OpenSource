/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __LPM_PLAT_APMCU_MBOX_H__
#define __LPM_PLAT_APMCU_MBOX_H__

enum {
	MBOX_MCUPM,

	NF_MBOX,
};

/* MCUPM Mbox */
#define APMCU_MCUPM_MBOX_ID             3

/* Read/Write */
#define APMCU_MCUPM_MBOX_AP_READY       0
#define APMCU_MCUPM_MBOX_RESERVED_1     1
#define APMCU_MCUPM_MBOX_RESERVED_2     2
#define APMCU_MCUPM_MBOX_RESERVED_3     3
#define APMCU_MCUPM_MBOX_PWR_CTRL_EN    4
#define APMCU_MCUPM_MBOX_L3_CACHE_MODE  5
#define APMCU_MCUPM_MBOX_BUCK_MODE      6
#define APMCU_MCUPM_MBOX_ARMPLL_MODE    7
/* Read only */
#define APMCU_MCUPM_MBOX_TASK_STA       8

/* Mbox Slot: APMCU_MCUPM_MBOX_PWR_CTRL_EN (4) */
#define MCUPM_MCUSYS_CTRL               (1 << 0)
#define MCUPM_BUCK_CTRL                 (1 << 1)
#define MCUPM_ARMPLL_CTRL               (1 << 2)
#define MCUPM_CM_CTRL                   (1 << 3)
#define MCUPM_PWR_CTRL_MASK             ((1 << 4) - 1)

/* Mbox Slot: APMCU_MCUPM_MBOX_BUCK_MODE (6) */
#define MCUPM_BUCK_NORMAL_MODE          0 /* default */
#define MCUPM_BUCK_LP_MODE              1
#define MCUPM_BUCK_OFF_MODE             2
#define NF_MCUPM_BUCK_MODE              3U

/* Mbox Slot: APMCU_MCUPM_MBOX_ARMPLL_MODE (7) */
#define MCUPM_ARMPLL_ON                 0 /* default */
#define MCUPM_ARMPLL_GATING             1
#define MCUPM_ARMPLL_OFF                2
#define NF_MCUPM_ARMPLL_MODE            3U

/* Mbox Slot: APMCU_MCUPM_MBOX_TASK_STA (9) */
#define MCUPM_TASK_UNINIT               0
#define MCUPM_TASK_INIT                 1
#define MCUPM_TASK_INIT_FINISH          2
#define MCUPM_TASK_WAIT                 3
#define MCUPM_TASK_RUN                  4
#define MCUPM_TASK_PAUSE                5

#endif
