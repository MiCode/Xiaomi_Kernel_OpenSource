// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 */

#ifndef __SDE_DSC_HELPER_H__
#define __SDE_DSC_HELPER_H__

#include "msm_drv.h"

#define DSC_1_1_PPS_PARAMETER_SET_ELEMENTS   88

int sde_dsc_populate_dsc_config(struct drm_dsc_config *dsc, int scr_ver);

int sde_dsc_populate_dsc_private_params(struct msm_display_dsc_info *dsc_info,
		int intf_width);

int sde_dsc_create_pps_buf_cmd(struct msm_display_dsc_info *dsc_info,
		char *buf, int pps_id, u32 len);

#endif /* __SDE_DSC_HELPER_H__ */

