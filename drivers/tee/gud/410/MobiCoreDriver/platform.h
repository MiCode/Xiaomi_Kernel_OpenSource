/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 TRUSTONIC LIMITED
 */

#ifndef _MC_DRV_PLATFORM_H_
#define _MC_DRV_PLATFORM_H_

/* Ensure consistency for Fastcall ID between NWd and TEE*/
#define MC_AARCH32_FC

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

#endif /* _MC_DRV_PLATFORM_H_ */
