/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __LPM_REGISTRY_H__
#define __LPM_REGISTRY_H__

#include <linux/cpumask.h>
#include <lpm.h>


typedef int (*blockcall)(int cpu, void *p);

enum LPM_REG_TYPE {
	LPM_REG_PER_CPU,
	LPM_REG_ALL_ONLINE,
};

int lpm_do_work(int type, blockcall call, void *dest);

#endif

