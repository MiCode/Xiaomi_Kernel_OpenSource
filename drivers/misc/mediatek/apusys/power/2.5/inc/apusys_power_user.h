/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _APUSYS_POWER_USER_H_
#define _APUSYS_POWER_USER_H_

enum POWER_CALLBACK_USER {
	IOMMU = 0,
	REVISOR = 1,
	MNOC = 2,
	DEVAPC = 3,
	APUSYS_POWER_CALLBACK_USER_NUM,
};

enum DVFS_USER {
	MDLA,
	MDLA0,
	MDLA1,
	VPU,
	VPU0,
	VPU1, /* 5 */
	VPU2,
	EDMA,	/* 7 */
	EDMA2,   // power user only
	REVISER, /* 9 */
	APUCB,
	APUMNOC,
	APUIOMMU,
	APUCONN,
	APUCORE,
	APUSYS_POWER_USER_NUM,
};

#endif
