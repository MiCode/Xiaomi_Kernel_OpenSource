/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_CX_IPEAK_H_
#define _CAM_CX_IPEAK_H_

#include "cam_soc_util.h"

int cam_cx_ipeak_register_cx_ipeak(struct cam_hw_soc_info *soc_info);

int cam_cx_ipeak_update_vote_cx_ipeak(struct cam_hw_soc_info *soc_info,
	int32_t apply_level);
int cam_cx_ipeak_unvote_cx_ipeak(struct cam_hw_soc_info *soc_info);

#endif /* _CAM_CX_IPEAK_H_ */
