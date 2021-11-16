/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_CX_IPEAK_H_
#define _CAM_CX_IPEAK_H_

#include "cam_soc_util.h"

#ifndef CONFIG_QCOM_CX_IPEAK
static inline int cam_cx_ipeak_register_cx_ipeak
	(struct cam_hw_soc_info *soc_info)
{
	return 0;
}

static inline int cam_cx_ipeak_update_vote_cx_ipeak
	(struct cam_hw_soc_info *soc_info, int32_t apply_level)
{
	return 0;
}

static inline int cam_cx_ipeak_unvote_cx_ipeak
	(struct cam_hw_soc_info *soc_info)
{
	return 0;
}
#else
int cam_cx_ipeak_register_cx_ipeak(struct cam_hw_soc_info *soc_info);

int cam_cx_ipeak_update_vote_cx_ipeak(struct cam_hw_soc_info *soc_info,
	int32_t apply_level);
int cam_cx_ipeak_unvote_cx_ipeak(struct cam_hw_soc_info *soc_info);
#endif

#endif /* _CAM_CX_IPEAK_H_ */
