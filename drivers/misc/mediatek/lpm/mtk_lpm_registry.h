/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_LPM_REGISTRY_H__
#define __MTK_LPM_REGISTRY_H__

#include <linux/cpumask.h>
#include <mtk_lpm.h>


typedef int (*blockcall)(int cpu, void *p);

enum MTK_LPM_REG_TYPE {
	MTK_LPM_REG_PER_CPU,
	MTK_LPM_REG_ALL_ONLINE,
};

int mtk_lpm_do_work(int type, blockcall call, void *dest);

#endif

