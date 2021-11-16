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


#ifndef __GZ_RKP_UT_H__
#define __GZ_RKP_UT_H__

int vreg_test(void *args);
int test_rkp_by_buffer(void *args);
int test_rkp_by_gzdriver(void *args);
int test_rkp_gzkernel_memory(void *args);
int test_rkp(void *args);

#endif
