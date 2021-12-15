/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_SWPM_INTERFACE_H__
#define __MTK_SWPM_INTERFACE_H__

#include <linux/types.h>

enum swpm_return_type {
	SWPM_SUCCESS = 0,
	SWPM_INIT_ERR = 1,
	SWPM_PLAT_ERR = 2,
	SWPM_ARGS_ERR = 3,
};

enum swpm_type {
	CPU_SWPM_TYPE,
	GPU_SWPM_TYPE,
	CORE_SWPM_TYPE,
	MEM_SWPM_TYPE,
	ISP_SWPM_TYPE,
	ME_SWPM_TYPE,

	NR_SWPM_TYPE,
};

enum swpm_pmu_user {
	SWPM_PMU_CPU_DVFS,
	SWPM_PMU_INTERNAL,

	NR_SWPM_PMU_USER,
};

/* swpm interface to request share memory address by SWPM TYPE */
/* return:      0  (SWPM_SUCCESS)
 *              otherwise (ERROR)
 */
extern int swpm_mem_addr_request(enum swpm_type id,
				 phys_addr_t **ptr);

/* swpm interface to enable/disable swpm related pmu */
/* return:	0  (SWPM_SUCCESS)
 *		otherwise (ERROR)
 */
extern int swpm_pmu_enable(enum swpm_pmu_user id,
			   unsigned int enable);

/* TODO: for API default compatible */
#define swpm_pmu_enable(x) swpm_pmu_enable(SWPM_PMU_CPU_DVFS, x)

#endif /* __MTK_SWPM_INTERFACE_H__ */

