/*
 * Copyright (C) 2018 MediaTek Inc.
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
