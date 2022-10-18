/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_IFE_CSID_570_H_
#define _CAM_IFE_CSID_570_H_

#include <linux/module.h>
#include "camera_main.h"
#include "cam_ife_csid_dev.h"
#include "cam_ife_csid_common.h"
#include "cam_ife_csid_hw_ver1.h"

/* Settings for 570 CSID are leveraged from 480 */
static struct cam_ife_csid_ver1_reg_info cam_ife_csid_570_reg_info = {
	.cmn_reg          = &cam_ife_csid_480_cmn_reg_info,
	.csi2_reg         = &cam_ife_csid_480_csi2_reg_info,
	.ipp_reg          = &cam_ife_csid_480_ipp_reg_info,
	.ppp_reg          = &cam_ife_csid_480_ppp_reg_info,
	.rdi_reg = {
		&cam_ife_csid_480_rdi_0_reg_info,
		&cam_ife_csid_480_rdi_1_reg_info,
		&cam_ife_csid_480_rdi_2_reg_info,
		NULL,
		},
	.tpg_reg = &cam_ife_csid_480_tpg_reg_info,
	.width_fuse_max_val = 3,
	.fused_max_width = {5612, 6048, 7308, UINT_MAX},
};
#endif /*_CAM_IFE_CSID_570_H_ */
