/*
 * Copyright (C) 2015 MediaTek Inc.
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

/*
 * Mediatek GenieZone v1.2.0127
 * Header files for KREE memory related functions.
 */

#ifndef __SDSP_M4U_MVA_H__
#define __SDSP_M4U_MVA_H__

/*
 *total (vpu0 elf + vpu1 elf) max 16M
 *in each vpu elf's final 64K as log buf mva
 */
#define SDSP_VPU0_ELF_MVA		0x82600000
#define SDSP_VPU0_DTA_MVA		0x83600000	//max 48M


#endif				/* __SDSP_M4U_MVA_H__ */
