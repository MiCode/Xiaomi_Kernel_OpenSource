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

#ifndef MTEE_REGIONS_H
#define MTEE_REGIONS_H

enum MTEE_MCHUNKS_ID {
	MTEE_MCHUNKS_PROT = 0,
	MTEE_MCHUNKS_HAPP = 1,
	MTEE_MCHUNKS_HAPP_EXTRA = 2,
	MTEE_MCHUNKS_SDSP = 3,
	MTEE_MCHUNKS_SDSP_SHARED_VPU_TEE = 4,
	MTEE_MCHUNKS_SDSP_SHARED_MTEE_TEE = 5,
	MTEE_MCHUNKS_SDSP_SHARED_VPU_MTEE_TEE = 6,
	MTEE_MCHUNKS_CELLINFO = 7,
	MTEE_MCHUNKS_SVP = 8,
	MTEE_MCHUNKS_WFD = 9,

	MTEE_MCUHNKS_INVALID = 0xFFFFFFFF,
};

#endif /* end of MTEE_REGIONS_H */
