/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */


#ifndef __MTK_LP_PLAT_APMCU_MBOX_H__
#define __MTK_LP_PLAT_APMCU_MBOX_H__

enum {
	MBOX_SSPM,
	MBOX_MCUPM,

	NF_MBOX,
};


enum APMCU_PM_IPI_UID {
	APMCU_PM_IPI_PMSR = 0,
	APMCU_PM_IPI_UID_LPM,
	APMCU_PM_IPI_UID_MCDI,
	APMCU_PM_IPI_UID_MAX,
};


/* mcdi ipi id definition */
enum MCDI_IPI_ID_TLB {
	MCDI_IPI_SHARE_SRAM_INFO_GET = 0,
	MCDI_IPI_ID_MAX
};

#define MCDI_IPI_MAGIC_NUM	0x4d

/* SSPM Mbox */
#define APMCU_SSPM_MBOX_ID           3
#define APMCU_SSPM_MBOX_SPM_CMD_SIZE 8

#define APMCU_SSPM_MBOX_SPM_CMD      0
#define APMCU_SSPM_MBOX_SPM_ARGS1    1
#define APMCU_SSPM_MBOX_SPM_ARGS2    2
#define APMCU_SSPM_MBOX_SPM_ARGS3    3
#define APMCU_SSPM_MBOX_SPM_ARGS4    4
#define APMCU_SSPM_MBOX_SPM_ARGS5    5
#define APMCU_SSPM_MBOX_SPM_ARGS6    6
#define APMCU_SSPM_MBOX_SPM_ARGS7    7
#define APMCU_SSPM_MBOX_AP_READY     17



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
#define APMCU_MCUPM_MBOX_RESERVED_9     9
#define APMCU_MCUPM_MBOX_RESERVED_10    10
#define APMCU_MCUPM_MBOX_RESERVED_11    11

/* CPC mode - Read/Write */
#define APMCU_MCUPM_MBOX_WAKEUP_CPU     12


/* Mbox Slot: APMCU_MCUPM_MBOX_PWR_CTRL_EN (4) */
#define MCUPM_MCUSYS_CTRL               (1 << 0)
#define MCUPM_BUCK_CTRL                 (1 << 1)
#define MCUPM_ARMPLL_CTRL               (1 << 2)
#define MCUPM_CM_CTRL                   (1 << 3)
#define MCUPM_PWR_CTRL_MASK             ((1 << 4) - 1)

/* Mbox Slot: APMCU_MCUPM_MBOX_L3_CACHE_MODE (5) */
#define MCUPM_L3_OFF_MODE               0 /* default */
#define MCUPM_L3_DORMANT_MODE           1
#define NF_MCUPM_L3_MODE                2U

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

bool mtk_lp_apmcu_cm_is_notified(void);

void mtk_set_lp_apmcu_pll_mode(unsigned int mode);
int mtk_get_lp_apmcu_pll_mode(void);

void mtk_set_lp_apmcu_buck_mode(unsigned int mode);
int mtk_get_lp_apmcu_buck_mode(void);

void mtk_set_preferred_cpu_wakeup(int cpu);
int mtk_get_preferred_cpu_wakeup(void);

bool mtk_lp_apmcu_is_ready(void);

void mtk_wait_mbox_init_done(void);
void mtk_notify_subsys_ap_ready(void);

int mtk_apmcu_mbox_init(void);

#endif
