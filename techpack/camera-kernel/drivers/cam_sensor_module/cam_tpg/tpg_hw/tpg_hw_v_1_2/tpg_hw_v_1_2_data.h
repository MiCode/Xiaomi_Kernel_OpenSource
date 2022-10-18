/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __TPG_HW_V_1_2_DATA_H__
#define __TPG_HW_V_1_2_DATA_H__

#include "../tpg_hw.h"
#include "tpg_hw_v_1_2.h"

struct tpg_hw_ops tpg_hw_v_1_2_ops = {
	.start = tpg_hw_v_1_2_start,
	.stop  = tpg_hw_v_1_2_stop,
	.init = tpg_hw_v_1_2_init,
	.process_cmd = tpg_hw_v_1_2_process_cmd,
	.dump_status = tpg_hw_v_1_2_dump_status,
};

struct tpg_hw_info tpg_v_1_2_hw_info = {
	.version = TPG_HW_VERSION_1_2,
	.max_vc_channels = 1,
	.max_dt_channels_per_vc = 1,
	.ops = &tpg_hw_v_1_2_ops,
};

#endif
