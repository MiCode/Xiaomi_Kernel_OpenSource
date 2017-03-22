/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _ADRENO_A6XX_H_
#define _ADRENO_A6XX_H_

#include "a6xx_reg.h"

#define CP_CLUSTER_FE		0x0
#define CP_CLUSTER_SP_VS	0x1
#define CP_CLUSTER_PC_VS	0x2
#define CP_CLUSTER_GRAS		0x3
#define CP_CLUSTER_SP_PS	0x4
#define CP_CLUSTER_PS		0x5


void a6xx_snapshot(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot);

void a6xx_crashdump_init(struct adreno_device *adreno_dev);

#endif
