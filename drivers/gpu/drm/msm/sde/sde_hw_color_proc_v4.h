/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */
#ifndef _SDE_HW_COLOR_PROC_V4_H_
#define _SDE_HW_COLOR_PROC_V4_H_

#include "sde_hw_util.h"
#include "sde_hw_catalog.h"
#include "sde_hw_dspp.h"
/**
 * sde_setup_dspp_3d_gamutv4 - Function for 3d gamut v4 version feature
 *                             programming.
 * @ctx: dspp ctx pointer
 * @cfg: pointer to sde_hw_cp_cfg
 */
void sde_setup_dspp_3d_gamutv4(struct sde_hw_dspp *ctx, void *cfg);

/**
 * sde_setup_dspp_3d_gamutv41 - Function for 3d gamut v4_1 version feature
 *                             programming.
 * @ctx: dspp ctx pointer
 * @cfg: pointer to sde_hw_cp_cfg
 */
void sde_setup_dspp_3d_gamutv41(struct sde_hw_dspp *ctx, void *cfg);

/**
 * sde_setup_dspp_igcv3 - Function for igc v3 version feature
 *                             programming.
 * @ctx: dspp ctx pointer
 * @cfg: pointer to sde_hw_cp_cfg
 */
void sde_setup_dspp_igcv3(struct sde_hw_dspp *ctx, void *cfg);

/**
 * sde_setup_dspp_pccv4 - Function for pcc v4 version feature
 *                             programming.
 * @ctx: dspp ctx pointer
 * @cfg: pointer to sde_hw_cp_cfg
 */
void sde_setup_dspp_pccv4(struct sde_hw_dspp *ctx, void *cfg);

#endif /* _SDE_HW_COLOR_PROC_V4_H_ */
