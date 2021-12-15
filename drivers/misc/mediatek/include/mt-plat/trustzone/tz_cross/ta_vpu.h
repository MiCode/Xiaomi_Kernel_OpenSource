/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

/* An example test TA implementation.
 */

#ifndef __TRUSTZONE_TA_VPU__
#define __TRUSTZONE_TA_VPU__

#define TZ_TA_VPU_UUID   "0a1b2c3d-4e5f-6a7b-8c9d-0e1f2a3b4c5d"

/* Data Structure for VPU TA */
/* You should define data structure used both in REE/TEE here */

/* Command for VPU TA */
#define VPU_TZCMD_LOAD		0
#define VPU_TZCMD_EXECUTE	1
#define VPU_TZCMD_UNLOAD	2
#define VPU_TZCMD_TEST		3

#endif /* __TRUSTZONE_TA_VPU__ */
