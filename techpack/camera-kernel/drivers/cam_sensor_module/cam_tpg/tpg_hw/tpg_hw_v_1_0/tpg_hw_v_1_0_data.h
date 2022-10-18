/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef __TPG_HW_V_1_0_DATA_H__
#define __TPG_HW_V_1_0_DATA_H__

#include "../tpg_hw.h"
#include "tpg_hw_v_1_0.h"

struct tpg_hw_ops tpg_hw_v_1_0_ops = {
	.start = tpg_hw_v_1_0_start,
	.stop  = tpg_hw_v_1_0_stop,
	.init = tpg_hw_v_1_0_init,
};

struct tpg_hw_info tpg_v_1_0_hw_info = {
	.version = TPG_HW_VERSION_1_0,
	.max_vc_channels = 2,
	.max_dt_channels_per_vc = 4,
	.ops = &tpg_hw_v_1_0_ops,
};

#endif
