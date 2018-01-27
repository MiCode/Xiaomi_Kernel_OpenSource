/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
#ifndef _SDE_HW_REG_DMA_V1_COLOR_PROC_H
#define _SDE_HW_REG_DMA_V1_COLOR_PROC_H

#include "sde_hw_util.h"
#include "sde_hw_catalog.h"
#include "sde_hw_dspp.h"

/**
 * reg_dmav1_init_dspp_op_v4() - initialize the dspp feature op for sde v4
 *                               using reg dma v1.
 * @feature: dspp feature
 * idx: dspp idx
 */
int reg_dmav1_init_dspp_op_v4(int feature, enum sde_dspp idx);

/**
 * reg_dmav1_dspp_feature_support() - check if dspp feature using REG_DMA
 *                                    or not.
 * @feature: dspp feature
 */
bool reg_dmav1_dspp_feature_support(int feature);

/**
 * reg_dma_init_sspp_op_v4() - initialize the sspp feature op for sde v4
 * @feature: sspp feature
 * @idx: sspp idx
 */
int reg_dmav1_init_sspp_op_v4(int feature, enum sde_sspp idx);

/**
 * reg_dmav1_setup_dspp_vlutv18() - vlut v18 implementation using reg dma v1.
 * @ctx: dspp ctx info
 * @cfg: pointer to struct sde_hw_cp_cfg
 */
void reg_dmav1_setup_dspp_vlutv18(struct sde_hw_dspp *ctx, void *cfg);

/**
 * reg_dmav1_setup_3d_gamutv4() - gamut v4 implementation using reg dma v1.
 * @ctx: dspp ctx info
 * @cfg: pointer to struct sde_hw_cp_cfg
 */
void reg_dmav1_setup_dspp_3d_gamutv4(struct sde_hw_dspp *ctx, void *cfg);

/**
 * reg_dmav1_setup_dspp_gcv18() - gc v18 implementation using reg dma v1.
 * @ctx: dspp ctx info
 * @cfg: pointer to struct sde_hw_cp_cfg
 */
void reg_dmav1_setup_dspp_gcv18(struct sde_hw_dspp *ctx, void *cfg);

/**
 * reg_dmav1_setup_dspp_igcv31() - igc v31 implementation using reg dma v1.
 * @ctx: dspp ctx info
 * @cfg: pointer to struct sde_hw_cp_cfg
 */
void reg_dmav1_setup_dspp_igcv31(struct sde_hw_dspp *ctx, void *cfg);

/**
 * reg_dmav1_setup_dspp_pccv4() - pcc v4 implementation using reg dma v1.
 * @ctx: dspp ctx info
 * @cfg: pointer to struct sde_hw_cp_cfg
 */
void reg_dmav1_setup_dspp_pccv4(struct sde_hw_dspp *ctx, void *cfg);

/**
 * reg_dmav1_setup_dspp_pa_hsicv18() - pa hsic v18 impl using reg dma v1.
 * @ctx: dspp ctx info
 * @cfg: pointer to struct sde_hw_cp_cfg
 */
void reg_dmav1_setup_dspp_pa_hsicv18(struct sde_hw_dspp *ctx, void *cfg);

/**
 * reg_dmav1_setup_dspp_sixzonev18() - sixzone v18 impl using reg dma v1.
 * @ctx: dspp ctx info
 * @cfg: pointer to struct sde_hw_cp_cfg
 */
void reg_dmav1_setup_dspp_sixzonev18(struct sde_hw_dspp *ctx, void *cfg);

/**
 * reg_dmav1_deinit_dspp_ops() - deinitialize the dspp feature op for sde v4
 *                               which were initialized.
 * @idx: dspp idx
 */
int reg_dmav1_deinit_dspp_ops(enum sde_dspp idx);
#endif /* _SDE_HW_REG_DMA_V1_COLOR_PROC_H */
