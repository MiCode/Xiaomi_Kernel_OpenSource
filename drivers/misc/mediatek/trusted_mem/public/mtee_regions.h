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

#if defined(CONFIG_MTK_MTEE_MULTI_CHUNK_SUPPORT)
enum MTEE_MCHUNKS_ID {
	MTEE_MCHUNKS_PROT = 0,
	MTEE_MCHUNKS_HAPP = 1,
	MTEE_MCHUNKS_HAPP_EXTRA = 2,
	MTEE_MCHUNKS_SDSP = 3,
	MTEE_MCHUNKS_SDSP_SHARED = 4,

	MTEE_MCUHNKS_INVALID = 0xFFFFFFFF,
};
#endif

#endif /* end of MTEE_REGIONS_H */
