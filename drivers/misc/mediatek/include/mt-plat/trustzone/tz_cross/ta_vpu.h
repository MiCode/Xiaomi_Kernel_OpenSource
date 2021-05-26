/* * Copyright (C) 2015 MediaTek Inc.
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
