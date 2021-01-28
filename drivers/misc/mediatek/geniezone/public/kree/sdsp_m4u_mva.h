/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 * GenieZone (hypervisor-based seucrity platform) enables hardware protected
 * and isolated security execution environment, includes
 * 1. GZ hypervisor
 * 2. Hypervisor-TEE OS (built-in Trusty OS)
 * 3. Drivers (ex: debug, communication and interrupt) for GZ and
 *    hypervisor-TEE OS
 * 4. GZ and hypervisor-TEE and GZ framework (supporting multiple TEE
 *    ecosystem, ex: M-TEE, Trusty, GlobalPlatform, ...)
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
