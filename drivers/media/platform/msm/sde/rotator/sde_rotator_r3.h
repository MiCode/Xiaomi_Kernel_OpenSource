/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __SDE_ROTATOR_R3_H__
#define __SDE_ROTATOR_R3_H__

#include "sde_rotator_core.h"

/* Maximum allowed Rotator clock value */
#define ROT_R3_MAX_ROT_CLK			345000000

int sde_rotator_r3_init(struct sde_rot_mgr *mgr);

#endif /* __SDE_ROTATOR_R3_H__ */
