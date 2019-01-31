/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef _MC_DRV_PLATFORM_H_
#define _MC_DRV_PLATFORM_H_

/* MobiCore Interrupt. */
/* Uncomment to override dts SSIQ setting */
/* #define MC_INTR_SSIQ	280 */

/* Ensure consistency for Fastcall ID between NWd and TEE*/
#define MC_AARCH32_FC


#define TBASE_CORE_SWITCHER

/* Values of MPIDR regs in  cpu0, cpu1, cpu2, cpu3*/
#define CPU_IDS {0x0000, 0x0001, 0x0002, 0x0003, \
	0x0100, 0x0101, 0x0102, 0x0103, 0x0200, 0x0201}
#define COUNT_OF_CPUS (CONFIG_NR_CPUS)

/* Enable Fastcall worker thread */
#define MC_FASTCALL_WORKER_THREAD

/* Enable LPAE */
#define CONFIG_TRUSTONIC_TEE_LPAE

/* For retriving SSIQ from dts */
#define MC_DEVICE_PROPNAME	"trustonic,mobicore"

#endif /* _MC_DRV_PLATFORM_H_ */
