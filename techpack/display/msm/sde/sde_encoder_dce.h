// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2017, 2020 The Linux Foundation. All rights reserved.
 */

#ifndef __SDE_ENCODER_DCE_H__
#define __SDE_ENCODER_DCE_H__

#include "sde_encoder.h"

/**
 * sde_encoder_dce_set_bpp : set src_bpp and target_bpp in sde_crtc
 * @msm_mode_info: Mode info
 * @crtc: Pointer to drm crtc structure
 */
void sde_encoder_dce_set_bpp(
		struct msm_mode_info mode_info, struct drm_crtc *crtc);

/**
 * sde_encoder_dce_disable : function to disable compression
 * @sde_enc: pointer to virtual encoder structure
 */
void sde_encoder_dce_disable(struct sde_encoder_virt *sde_enc);

/**
 * sde_encoder_dce_setup : function to configure compression block
 * @sde_enc: pointer to virtual encoder structure
 * @params: pointer to kickoff params
 */
int sde_encoder_dce_setup(struct sde_encoder_virt *sde_enc,
		struct sde_encoder_kickoff_params *params);

/**
 * sde_encoder_dce_flush :function to flush the compression configuration
 * @sde_enc: pointer to virtual encoder structure
 */
void sde_encoder_dce_flush(struct sde_encoder_virt *sde_enc);

#endif /* __SDE_ENCODER_DCE_H__ */
