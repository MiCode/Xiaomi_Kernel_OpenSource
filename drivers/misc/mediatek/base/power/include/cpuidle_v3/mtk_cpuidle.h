/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */


#ifndef __MTK_CPUIDLE_H__
#define __MTK_CPUIDLE_H__

enum mtk_cpuidle_mode {
	MTK_STANDBY_MODE = 1,
	MTK_MCDI_CPU_MODE,
	MTK_MCDI_CLUSTER_MODE,
	MTK_SODI_MODE,
	MTK_SODI3_MODE,
	MTK_DPIDLE_MODE,
	MTK_SUSPEND_MODE,
};

#define NR_CPUIDLE_MODE  MTK_SUSPEND_MODE

int mtk_cpuidle_init(void);
int mtk_enter_idle_state(int mode);

#endif
