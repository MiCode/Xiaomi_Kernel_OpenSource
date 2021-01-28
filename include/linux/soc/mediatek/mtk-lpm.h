/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_LPM_H__
#define __MTK_LPM_H__

#include <linux/cpu_pm.h>
#include <linux/cpuidle.h>
#include <asm/cpuidle.h>

#define MTK_CPUIDLE_PM_NAME	"mtk_cpuidle_pm"

struct mtk_cpuidle_op {
	/*prompt to platform driver that cpu will enter idle state */
	int (*cpuidle_prepare)(struct cpuidle_driver *drv, int index);

	/*notify platform driver that cpu will leave idle state */
	void (*cpuidle_resume)(struct cpuidle_driver *drv, int index);
};

int mtk_lpm_drv_cpuidle_ops_set(struct mtk_cpuidle_op *op);
#endif
