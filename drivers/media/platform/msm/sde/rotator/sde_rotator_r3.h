/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SDE_ROTATOR_R3_H__
#define __SDE_ROTATOR_R3_H__

#include "sde_rotator_core.h"

/* Maximum allowed Rotator clock value */
#define ROT_R3_MAX_ROT_CLK			345000000

int sde_rotator_r3_init(struct sde_rot_mgr *mgr);

#endif /* __SDE_ROTATOR_R3_H__ */
