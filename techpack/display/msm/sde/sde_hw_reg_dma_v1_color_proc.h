/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */
#ifndef _SDE_HW_REG_DMA_V1_COLOR_PROC_H
#define _SDE_HW_REG_DMA_V1_COLOR_PROC_H

#include "sde_hw_util.h"
#include "sde_hw_catalog.h"
#include "sde_hw_dspp.h"
#include "sde_hw_sspp.h"

/**
 * reg_dmav1_init_dspp_op_v4() - initialize the dspp feature op for sde v4
 *                               using reg dma v1.
 * @feature: dspp feature
 * idx: dspp idx
 */
int reg_dmav1_init_dspp_op_v4(int feature, enum sde_dspp idx);

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
 * reg_dmav1_setup_3d_gamutv41() - gamut v4_1 implementation using reg dma v1.
 * @ctx: dspp ctx info
 * @cfg: pointer to struct sde_hw_cp_cfg
 */
void reg_dmav1_setup_dspp_3d_gamutv41(struct sde_hw_dspp *ctx, void *cfg);

/**
 * reg_dmav1_setup_3d_gamutv42() - gamut v4_2 implementation using reg dma v1.
 * @ctx: dspp ctx info
 * @cfg: pointer to struct sde_hw_cp_cfg
 */
void reg_dmav1_setup_dspp_3d_gamutv42(struct sde_hw_dspp *ctx, void *cfg);

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
 * reg_dmav1_setup_dspp_pa_hsicv17() - pa hsic v17 impl using reg dma v1.
 * @ctx: dspp ctx info
 * @cfg: pointer to struct sde_hw_cp_cfg
 */
void reg_dmav1_setup_dspp_pa_hsicv17(struct sde_hw_dspp *ctx, void *cfg);

/**
 * reg_dmav1_setup_dspp_sixzonev17() - sixzone v17 impl using reg dma v1.
 * @ctx: dspp ctx info
 * @cfg: pointer to struct sde_hw_cp_cfg
 */
void reg_dmav1_setup_dspp_sixzonev17(struct sde_hw_dspp *ctx, void *cfg);

/**
 * reg_dmav1_setup_dspp_memcol_skinv17() - memcol skin v17 impl using
 * reg dma v1.
 * @ctx: dspp ctx info
 * @cfg: pointer to struct sde_hw_cp_cfg
 */
void reg_dmav1_setup_dspp_memcol_skinv17(struct sde_hw_dspp *ctx, void *cfg);

/**
 * reg_dmav1_setup_dspp_memcol_skyv17() - memcol sky v17 impl using reg dma v1.
 * @ctx: dspp ctx info
 * @cfg: pointer to struct sde_hw_cp_cfg
 */
void reg_dmav1_setup_dspp_memcol_skyv17(struct sde_hw_dspp *ctx, void *cfg);

/**
 * reg_dmav1_setup_dspp_memcol_folv17() - memcol foliage v17 impl using
 * reg dma v1.
 * @ctx: dspp ctx info
 * @cfg: pointer to struct sde_hw_cp_cfg
 */
void reg_dmav1_setup_dspp_memcol_folv17(struct sde_hw_dspp *ctx, void *cfg);

/**
 * reg_dmav1_setup_dspp_memcol_protv17() - memcol prot v17 impl using
 * reg dma v1.
 * @ctx: dspp ctx info
 * @cfg: pointer to struct sde_hw_cp_cfg
 */
void reg_dmav1_setup_dspp_memcol_protv17(struct sde_hw_dspp *ctx, void *cfg);

/**
 * reg_dmav1_deinit_dspp_ops() - deinitialize the dspp feature op for sde v4
 *                               which were initialized.
 * @idx: dspp idx
 */
int reg_dmav1_deinit_dspp_ops(enum sde_dspp idx);

/**
 * reg_dmav1_init_sspp_op_v4() - initialize the sspp feature op for sde v4
 * @feature: sspp feature
 * @idx: sspp idx
 */
int reg_dmav1_init_sspp_op_v4(int feature, enum sde_sspp idx);

/**
 * reg_dmav1_setup_vig_gamutv5() - VIG 3D lut gamut v5 implementation
 *                                 using reg dma v1.
 * @ctx: sspp ctx info
 * @cfg: pointer to struct sde_hw_cp_cfg
 */
void reg_dmav1_setup_vig_gamutv5(struct sde_hw_pipe *ctx, void *cfg);

/**
 * reg_dmav1_setup_vig_gamutv6() - VIG 3D lut gamut v6 implementation
 *                                 using reg dma v1.
 * @ctx: sspp ctx info
 * @cfg: pointer to struct sde_hw_cp_cfg
 */
void reg_dmav1_setup_vig_gamutv6(struct sde_hw_pipe *ctx, void *cfg);

/**
 * reg_dmav1_setup_vig_igcv5() - VIG 1D lut IGC v5 implementation
 *                               using reg dma v1.
 * @ctx: sspp ctx info
 * @cfg: pointer to struct sde_hw_cp_cfg
 */
void reg_dmav1_setup_vig_igcv5(struct sde_hw_pipe *ctx, void *cfg);

/**
 * reg_dmav1_setup_dma_igcv5() - DMA 1D lut IGC v5 implementation
 *                               using reg dma v1.
 * @ctx: sspp ctx info
 * @cfg: pointer to struct sde_hw_cp_cfg
 * @idx: multirect index
 */
void reg_dmav1_setup_dma_igcv5(struct sde_hw_pipe *ctx, void *cfg,
			enum sde_sspp_multirect_index idx);
/**
 * reg_dmav1_setup_vig_igcv6() - VIG ID lut IGC v6 implementation
 *				 using reg dma v1.
 * @ctx: sspp ctx info
 * @cfg: pointer to struct sde_hw_cp_cfg
 */
void reg_dmav1_setup_vig_igcv6(struct sde_hw_pipe *ctx, void *cfg);

/**
 * reg_dmav1_setup_dma_gcv5() - DMA 1D lut GC v5 implementation
 *                              using reg dma v1.
 * @ctx: sspp ctx info
 * @cfg: pointer to struct sde_hw_cp_cfg
 * @idx: multirect index
 */
void reg_dmav1_setup_dma_gcv5(struct sde_hw_pipe *ctx, void *cfg,
			enum sde_sspp_multirect_index idx);

/**
 * reg_dmav1_setup_vig_qseed3 - Qseed3 implementation using reg dma v1.
 * @ctx: sspp ctx info
 * @sspp: pointer to sspp hw config
 * @pe: pointer to pixel extension config
 * @scaler_cfg: pointer to scaler config
 */

void reg_dmav1_setup_vig_qseed3(struct sde_hw_pipe *ctx,
	struct sde_hw_pipe_cfg *sspp, struct sde_hw_pixel_ext *pe,
	void *scaler_cfg);

/**reg_dmav1_setup_scaler3_lut - Qseed3 lut coefficient programming
 * @buf: defines structure for reg dma ops on the reg dma buffer.
 * @scaler3_cfg: QSEEDv3 configuration
 * @offset: Scaler Offest
 */

void reg_dmav1_setup_scaler3_lut(struct sde_reg_dma_setup_ops_cfg *buf,
		struct sde_hw_scaler3_cfg *scaler3_cfg, u32 offset);

/**reg_dmav1_setup_scaler3lite_lut - Qseed3lite lut coefficient programming
 * @buf: defines structure for reg dma ops on the reg dma buffer.
 * @scaler3_cfg: QSEEDv3 configuration
 * @offset: Scaler Offest
 */

void reg_dmav1_setup_scaler3lite_lut(struct sde_reg_dma_setup_ops_cfg *buf,
		struct sde_hw_scaler3_cfg *scaler3_cfg, u32 offset);

/**
 * reg_dmav1_deinit_sspp_ops() - deinitialize the sspp feature op for sde v4
 *                               which were initialized.
 * @idx: sspp idx
 */
int reg_dmav1_deinit_sspp_ops(enum sde_sspp idx);

/**
 * reg_dmav1_init_ltm_op_v6() - initialize the ltm feature op for sde v6
 * @feature: ltm feature
 * @idx: dspp idx
 */
int reg_dmav1_init_ltm_op_v6(int feature, enum sde_dspp idx);

/**
 * reg_dmav1_setup_ltm_initv1() - LTM INIT v1 implementation using reg dma v1.
 * @ctx: dspp ctx info
 * @cfg: pointer to struct sde_hw_cp_cfg
 */
void reg_dmav1_setup_ltm_initv1(struct sde_hw_dspp *ctx, void *cfg);

/**
 * reg_dmav1_setup_ltm_roiv1() - LTM ROI v1 implementation using reg dma v1.
 * @ctx: dspp ctx info
 * @cfg: pointer to struct sde_hw_cp_cfg
 */
void reg_dmav1_setup_ltm_roiv1(struct sde_hw_dspp *ctx, void *cfg);

/**
 * reg_dmav1_setup_ltm_vlutv1() - LTM VLUT v1 implementation using reg dma v1.
 * @ctx: dspp ctx info
 * @cfg: pointer to struct sde_hw_cp_cfg
 */
void reg_dmav1_setup_ltm_vlutv1(struct sde_hw_dspp *ctx, void *cfg);

/**
 * reg_dmav1_deinit_ltm_ops() - deinitialize the ltm feature op for sde v4
 *                               which were initialized.
 * @idx: ltm idx
 */
int reg_dmav1_deinit_ltm_ops(enum sde_dspp idx);


#endif /* _SDE_HW_REG_DMA_V1_COLOR_PROC_H */
