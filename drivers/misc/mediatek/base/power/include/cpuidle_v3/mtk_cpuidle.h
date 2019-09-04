/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __MTK_CPUIDLE_H__
#define __MTK_CPUIDLE_H__

enum mtk_cpuidle_mode {
	MTK_STANDBY_MODE = 1,
	MTK_MCDI_CPU_MODE,
	MTK_MCDI_CLUSTER_MODE,
	MTK_SODI_MODE,
	MTK_SODI3_MODE,
	MTK_DPIDLE_MODE,
	/* TODO: Need verify later */
	MTK_IDLEDRAM_MODE = 4,
	MTK_IDLESYSPLL_MODE = 5,
	MTK_IDLEBUS26M_MODE = 6,
	MTK_SUSPEND_MODE = 7,
};

#define NR_CPUIDLE_MODE  MTK_SUSPEND_MODE

int mtk_cpuidle_init(void);
int mtk_enter_idle_state(int mode);

#endif
