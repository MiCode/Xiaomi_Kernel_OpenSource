/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MTK_THERMAL_IPI_H__
#define __MTK_THERMAL_IPI_H__

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#define THERMAL_ENABLE_TINYSYS_SSPM (0)
#define THERMAL_ENABLE_ONLY_TZ_SSPM (1)
#define THERMAL_SSPM_THERMAL_THROTTLE_SWITCH

/*Only Big SW need to enable this definition
 *Big SW suspend/resume flow:
 *    suspend: kernel suspend => SSPM suspend
 *    resume: SSPM resume => kernel resume
 */

/* THERMAL_KERNEL_SUSPEND_RESUME_NOTIFY means notify tinysys
 *   to start/stop reading temperature when kernel suspend/resume/shutdown
 */
#define THERMAL_KERNEL_SUSPEND_RESUME_NOTIFY

/* THERMAL_KERNEL_SUSPEND_RESUME_NOTIFY_ONLY_AT_SHUTDOWN means
 * notify tinysys to stop reading temperature only when kernel shutodown
 */
#ifdef THERMAL_KERNEL_SUSPEND_RESUME_NOTIFY
#define THERMAL_KERNEL_SUSPEND_RESUME_NOTIFY_ONLY_AT_SHUTDOWN
#endif

#define THERMAL_CPUEB_USE_PLATFORM_IPI
#else
#define THERMAL_ENABLE_TINYSYS_SSPM (0)
#define THERMAL_ENABLE_ONLY_TZ_SSPM (0)
#endif

#if THERMAL_ENABLE_TINYSYS_SSPM || THERMAL_ENABLE_ONLY_TZ_SSPM
#include <mcupm_ipi_id.h>


#define THERMAL_SLOT_NUM (4)
#define BIG_CORE_THRESHOLD_ARRAY_SIZE (3)
/* IPI Msg type */
enum {
	THERMAL_IPI_INIT_GRP1,
	THERMAL_IPI_INIT_GRP2,
	THERMAL_IPI_INIT_GRP3,
	THERMAL_IPI_INIT_GRP4,
	THERMAL_IPI_INIT_GRP5,
	THERMAL_IPI_INIT_GRP6,
	THERMAL_IPI_LVTS_INIT_GRP1,
	THERMAL_IPI_GET_TEMP,
	THERMAL_IPI_SET_ATM_CFG_GRP1,
	THERMAL_IPI_SET_ATM_CFG_GRP2,
	THERMAL_IPI_SET_ATM_CFG_GRP3,
	THERMAL_IPI_SET_ATM_CFG_GRP4,
	THERMAL_IPI_SET_ATM_CFG_GRP5,
	THERMAL_IPI_SET_ATM_CFG_GRP6,
	THERMAL_IPI_SET_ATM_CFG_GRP7,
	THERMAL_IPI_SET_ATM_CFG_GRP8,
	THERMAL_IPI_SET_ATM_TTJ,
	THERMAL_IPI_SET_ATM_EN,
	THERMAL_IPI_GET_ATM_CPU_LIMIT,
	THERMAL_IPI_GET_ATM_GPU_LIMIT,
	THERMAL_IPI_SET_BIG_FREQ_THRESHOLD,
	THERMAL_IPI_GET_BIG_FREQ_THRESHOLD,
	THERMAL_IPI_SET_DIS_THERMAL_THROTTLE,
	THERMAL_IPI_SUSPEND_RESUME_NOTIFY,
	NR_THERMAL_IPI
};

/* IPI Msg data structure */
struct thermal_ipi_data {
	unsigned int cmd;
	union {
		struct {
			int arg[THERMAL_SLOT_NUM - 1];
		} data;
	} u;
};
extern unsigned int thermal_to_mcupm(unsigned int cmd,
	struct thermal_ipi_data *thermal_data);
extern int atm_to_sspm(unsigned int cmd, int data_len,
	struct thermal_ipi_data *thermal_data, int *ackData);
#endif /* THERMAL_ENABLE_TINYSYS_SSPM */
#endif
