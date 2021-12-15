/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef TEE_REGIONS_H
#define TEE_REGIONS_H

/* Refer to drSecMemApi.h */
enum TEE_SMEM_TYPE {
	TEE_SMEM_SVP = 0,
	TEE_SMEM_PROT = 1,
	TEE_SMEM_2D_FR = 2,
	TEE_SMEM_WFD = 3,
	TEE_SMEM_SDSP_SHARED = 4,
	TEE_SMEM_SDSP_FIRMWARE = 5,
	TEE_SMEM_HAPP_ELF = 6,
	TEE_SMEM_HAPP_EXTRA = 7,

	TEE_SMEM_INVALID = 0xFFFFFFFF,
};

#endif /* end of TEE_REGIONS_H */
