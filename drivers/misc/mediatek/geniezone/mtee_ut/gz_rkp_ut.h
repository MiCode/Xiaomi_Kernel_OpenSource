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

int test_rkp_mix_op(void *args);
int test_rkp_stress_by_vreg(void *args);
int test_rkp_stress_by_smc(void *args);
int test_rkp_by_malloc_buf(void *args);
int test_rkp_basic_by_vreg(void *args);
int test_rkp_basic_by_smc(void *args);

int test_rkp_by_gz_driver(void *args);


#endif
