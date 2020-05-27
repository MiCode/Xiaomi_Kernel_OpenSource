/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2013-2017 TRUSTONIC LIMITED
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

/* Ensure consistency for Fastcall ID between NWd and TEE*/
#define MC_AARCH32_FC

/* Enable Runtime Power Management */

/* Enable Fastcall worker thread */
#define MC_FASTCALL_WORKER_THREAD

#if defined(CONFIG_ARM64)
/* Enable LPAE on 64-bit platforms */
#ifndef CONFIG_TRUSTONIC_TEE_LPAE
#define CONFIG_TRUSTONIC_TEE_LPAE
#endif
#endif

/* For retrieving SSIQ from dts */
#define MC_DEVICE_PROPNAME	"trustonic,mobicore"

/* Enforce/restrict statically CPUs potentially running TEE
 * (Customize to match platform CPU layout... 0xF0 for big cores only for ex).
 * If not defined TEE dynamically using all platform CPUs (recommended)
 */
/* #define PLAT_DEFAULT_TEE_AFFINITY_MASK (0xXX)  */

#endif /* _MC_DRV_PLATFORM_H_ */
