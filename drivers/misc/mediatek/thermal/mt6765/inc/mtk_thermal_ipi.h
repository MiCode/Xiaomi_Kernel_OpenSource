/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __MTK_THERMAL_IPI_H__
#define __MTK_THERMAL_IPI_H__

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#define THERMAL_ENABLE_TINYSYS_SSPM (0)
#else
#define THERMAL_ENABLE_TINYSYS_SSPM (0)
#endif

#if THERMAL_ENABLE_TINYSYS_SSPM
#include "sspm_ipi.h"
#include <sspm_reservedmem_define.h>

#define THERMAL_SLOT_NUM (4)

/* IPI Msg type */
enum {
	THERMAL_IPI_INIT_GRP1,
	THERMAL_IPI_INIT_GRP2,
	THERMAL_IPI_INIT_GRP3,
	THERMAL_IPI_INIT_GRP4,
	THERMAL_IPI_INIT_GRP5,
	THERMAL_IPI_INIT_GRP6,
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
extern unsigned int thermal_to_sspm(unsigned int cmd,
	struct thermal_ipi_data *thermal_data);
extern int atm_to_sspm(unsigned int cmd, int data_len,
	struct thermal_ipi_data *thermal_data, int *ackData);
#endif /* THERMAL_ENABLE_TINYSYS_SSPM */
#endif
