/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#ifndef _SDE_HW_COLOR_PROCESSING_V1_7_H
#define _SDE_HW_COLOR_PROCESSING_V1_7_H

#include "sde_hw_sspp.h"
#include "sde_hw_dspp.h"

/**
 * sde_setup_pipe_pa_hue_v1_7 - setup SSPP hue feature in v1.7 hardware
 * @ctx: Pointer to pipe context
 * @cfg: Pointer to hue data
 */
void sde_setup_pipe_pa_hue_v1_7(struct sde_hw_pipe *ctx, void *cfg);

/**
 * sde_setup_pipe_pa_sat_v1_7 - setup SSPP saturation feature in v1.7 hardware
 * @ctx: Pointer to pipe context
 * @cfg: Pointer to saturation data
 */
void sde_setup_pipe_pa_sat_v1_7(struct sde_hw_pipe *ctx, void *cfg);

/**
 * sde_setup_pipe_pa_val_v1_7 - setup SSPP value feature in v1.7 hardware
 * @ctx: Pointer to pipe context
 * @cfg: Pointer to value data
 */
void sde_setup_pipe_pa_val_v1_7(struct sde_hw_pipe *ctx, void *cfg);

/**
 * sde_setup_pipe_pa_cont_v1_7 - setup SSPP contrast feature in v1.7 hardware
 * @ctx: Pointer to pipe context
 * @cfg: Pointer to contrast data
 */
void sde_setup_pipe_pa_cont_v1_7(struct sde_hw_pipe *ctx, void *cfg);

/**
 * sde_setup_pipe_pa_memcol_v1_7 - setup SSPP memory color in v1.7 hardware
 * @ctx: Pointer to pipe context
 * @type: Memory color type (Skin, sky, or foliage)
 * @cfg: Pointer to memory color config data
 */
void sde_setup_pipe_pa_memcol_v1_7(struct sde_hw_pipe *ctx,
				   enum sde_memcolor_type type,
				   void *cfg);

/**
 * sde_setup_dspp_pcc_v1_7 - setup DSPP PCC veature in v1.7 hardware
 * @ctx: Pointer to dspp context
 * @cfg: Pointer to PCC data
 */
void sde_setup_dspp_pcc_v1_7(struct sde_hw_dspp *ctx, void *cfg);

/**
 * sde_setup_dspp_pa_hsic_v17 - setup DSPP hsic feature in v1.7 hardware
 * @ctx: Pointer to DSPP context
 * @cfg: Pointer to hsic data
 */
void sde_setup_dspp_pa_hsic_v17(struct sde_hw_dspp *ctx, void *cfg);

/**
 * sde_setup_dspp_memcol_skin_v17 - setup DSPP memcol skin in v1.7 hardware
 * @ctx: Pointer to DSPP context
 * @cfg: Pointer to memcolor config data
 */
void sde_setup_dspp_memcol_skin_v17(struct sde_hw_dspp *ctx, void *cfg);

/**
 * sde_setup_dspp_memcol_sky_v17 - setup DSPP memcol sky in v1.7 hardware
 * @ctx: Pointer to DSPP context
 * @cfg: Pointer to memcolor config data
 */
void sde_setup_dspp_memcol_sky_v17(struct sde_hw_dspp *ctx, void *cfg);

/**
 * sde_setup_dspp_memcol_foliage_v17 - setup DSPP memcol fol in v1.7 hardware
 * @ctx: Pointer to DSPP context
 * @cfg: Pointer to memcolor config data
 */
void sde_setup_dspp_memcol_foliage_v17(struct sde_hw_dspp *ctx, void *cfg);

/**
 * sde_setup_dspp_memcol_prot_v17 - setup DSPP memcol prot in v1.7 hardware
 * @ctx: Pointer to DSPP context
 * @cfg: Pointer to memcolor config data
 */
void sde_setup_dspp_memcol_prot_v17(struct sde_hw_dspp *ctx, void *cfg);

/**
 * sde_setup_dspp_sixzone_v17 - setup DSPP sixzone feature in v1.7 hardware
 * @ctx: Pointer to DSPP context
 * @cfg: Pointer to sixzone data
 */
void sde_setup_dspp_sixzone_v17(struct sde_hw_dspp *ctx, void *cfg);

/**
 * sde_setup_dspp_pa_vlut_v1_7 - setup DSPP PA vLUT feature in v1.7 hardware
 * @ctx: Pointer to DSPP context
 * @cfg: Pointer to vLUT data
 */
void sde_setup_dspp_pa_vlut_v1_7(struct sde_hw_dspp *ctx, void *cfg);

/**
 * sde_setup_dspp_pa_vlut_v1_8 - setup DSPP PA vLUT feature in v1.8 hardware
 * @ctx: Pointer to DSPP context
 * @cfg: Pointer to vLUT data
 */
void sde_setup_dspp_pa_vlut_v1_8(struct sde_hw_dspp *ctx, void *cfg);

/**
 * sde_setup_dspp_gc_v1_7 - setup DSPP gc feature in v1.7 hardware
 * @ctx: Pointer to DSPP context
 * @cfg: Pointer to gc data
 */
void sde_setup_dspp_gc_v1_7(struct sde_hw_dspp *ctx, void *cfg);

/**
 * sde_setup_dspp_hist_v1_7 - setup DSPP histogram feature in v1.7 hardware
 * @ctx: Pointer to DSPP context
 * @cfg: Pointer to histogram control data
 */
void sde_setup_dspp_hist_v1_7(struct sde_hw_dspp *ctx, void *cfg);

/**
 * sde_read_dspp_hist_v1_7 - read DSPP histogram data in v1.7 hardware
 * @ctx: Pointer to DSPP context
 * @cfg: Pointer to histogram data
 */
void sde_read_dspp_hist_v1_7(struct sde_hw_dspp *ctx, void *cfg);

/**
 * sde_lock_dspp_hist_v1_7 - lock DSPP histogram buffer in v1.7 hardware
 * @ctx: Pointer to DSPP context
 */
void sde_lock_dspp_hist_v1_7(struct sde_hw_dspp *ctx, void *cfg);

/**
 * sde_setup_dspp_dither_v1_7 - setup DSPP dither feature in v1.7 hardware
 * @ctx: Pointer to DSPP context
 * @cfg: Pointer to dither data
 */
void sde_setup_dspp_dither_v1_7(struct sde_hw_dspp *ctx, void *cfg);
#endif
