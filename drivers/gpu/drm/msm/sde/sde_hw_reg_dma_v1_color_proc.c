/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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
#include <drm/msm_drm_pp.h>
#include "sde_reg_dma.h"
#include "sde_hw_reg_dma_v1_color_proc.h"
#include "sde_hw_color_proc_common_v4.h"
#include "sde_hw_ctl.h"
#include "sde_hw_sspp.h"
#include "sde_hwio.h"

/* Reserve space of 128 words for LUT dma payload set-up */
#define REG_DMA_HEADERS_BUFFER_SZ (sizeof(u32) * 128)
#define REG_DMA_VIG_SWI_DIFF 0x200
#define REG_DMA_DMA_SWI_DIFF 0x200

#define VLUT_MEM_SIZE ((128 * sizeof(u32)) + REG_DMA_HEADERS_BUFFER_SZ)
#define VLUT_LEN (128 * sizeof(u32))
#define PA_OP_MODE_OFF 0x800
#define PA_LUTV_OPMODE_OFF 0x84c
#define REG_DMA_PA_MODE_HSIC_MASK 0xE1EFFFFF
#define REG_DMA_PA_MODE_SZONE_MASK 0x1FEFFFFF
#define REG_DMA_PA_PWL_HOLD_SZONE_MASK 0x0FFF
#define PA_LUTV_DSPP_CTRL_OFF 0x4c
#define PA_LUTV_DSPP_SWAP_OFF 0x18

#define GAMUT_LUT_MEM_SIZE ((sizeof(struct drm_msm_3d_gamut)) + \
		REG_DMA_HEADERS_BUFFER_SZ)
#define GAMUT_SCALE_OFF_LEN (GAMUT_3D_SCALE_OFF_SZ * sizeof(u32))
#define GAMUT_SCALE_OFF_LEN_12 (GAMUT_3D_SCALEB_OFF_SZ * sizeof(u32))

#define GC_LUT_MEM_SIZE ((sizeof(struct drm_msm_pgc_lut)) + \
		REG_DMA_HEADERS_BUFFER_SZ)

#define IGC_LUT_MEM_SIZE ((sizeof(struct drm_msm_igc_lut)) + \
		REG_DMA_HEADERS_BUFFER_SZ)

#define PCC_LUT_ENTRIES (PCC_NUM_PLANES * PCC_NUM_COEFF)
#define PCC_LEN (PCC_LUT_ENTRIES * sizeof(u32))
#define PCC_MEM_SIZE (PCC_LEN + \
		REG_DMA_HEADERS_BUFFER_SZ)

#define HSIC_MEM_SIZE ((sizeof(struct drm_msm_pa_hsic)) + \
		REG_DMA_HEADERS_BUFFER_SZ)
#define SIXZONE_MEM_SIZE ((sizeof(struct drm_msm_sixzone)) + \
		REG_DMA_HEADERS_BUFFER_SZ)
#define MEMCOLOR_MEM_SIZE ((sizeof(struct drm_msm_memcol)) + \
		REG_DMA_HEADERS_BUFFER_SZ)

#define QSEED3_MEM_SIZE (sizeof(struct sde_hw_scaler3_cfg) + \
			(450 * sizeof(u32)) + \
			REG_DMA_HEADERS_BUFFER_SZ)

#define REG_MASK(n) ((BIT(n)) - 1)
#define REG_MASK_SHIFT(n, shift) ((REG_MASK(n)) << (shift))
#define REG_DMA_VIG_GAMUT_OP_MASK 0x300
#define REG_DMA_VIG_IGC_OP_MASK 0x1001F
#define DMA_DGM_0_OP_MODE_OFF 0x604
#define DMA_DGM_1_OP_MODE_OFF 0x1604
#define REG_DMA_DMA_IGC_OP_MASK 0x10005
#define REG_DMA_DMA_GC_OP_MASK 0x10003
#define DMA_1D_LUT_IGC_LEN 128
#define DMA_1D_LUT_GC_LEN 128
#define DMA_1D_LUT_IGC_DITHER_OFF 0x408
#define VIG_1D_LUT_IGC_LEN 128
#define VIG_IGC_DATA_MASK (BIT(10) - 1)
#define VIG_IGC_DATA_MASK_V6 (BIT(12) - 1)

/* SDE_SCALER_QSEED3 */
#define QSEED3_DE_OFFSET                       0x24
#define QSEED3_COEF_LUT_SWAP_BIT           0
#define QSEED3_COEF_LUT_CTRL_OFF               0x4C

/* SDE_SCALER_QSEED3LITE */
#define QSEED3L_COEF_LUT_SWAP_BIT          0
#define QSEED3L_COEF_LUT_Y_SEP_BIT         4
#define QSEED3L_COEF_LUT_UV_SEP_BIT        5
#define QSEED3L_COEF_LUT_CTRL_OFF              0x4c
#define Y_INDEX                            0
#define UV_INDEX                           1

static struct sde_reg_dma_buffer *dspp_buf[REG_DMA_FEATURES_MAX][DSPP_MAX];
static struct sde_reg_dma_buffer
	*sspp_buf[SDE_SSPP_RECT_MAX][REG_DMA_FEATURES_MAX][SSPP_MAX];

static u32 feature_map[SDE_DSPP_MAX] = {
	[SDE_DSPP_VLUT] = VLUT,
	[SDE_DSPP_GAMUT] = GAMUT,
	[SDE_DSPP_IGC] = IGC,
	[SDE_DSPP_PCC] = PCC,
	[SDE_DSPP_GC] = GC,
	[SDE_DSPP_HSIC] = HSIC,
	/* MEMCOLOR can be mapped to any MEMC_SKIN/SKY/FOLIAGE/PROT*/
	[SDE_DSPP_MEMCOLOR] = MEMC_SKIN,
	[SDE_DSPP_SIXZONE] = SIX_ZONE,
	[SDE_DSPP_DITHER] = REG_DMA_FEATURES_MAX,
	[SDE_DSPP_HIST] = REG_DMA_FEATURES_MAX,
	[SDE_DSPP_AD] = REG_DMA_FEATURES_MAX,
};

static u32 sspp_feature_map[SDE_SSPP_MAX] = {
	[SDE_SSPP_VIG_IGC] = IGC,
	[SDE_SSPP_VIG_GAMUT] = GAMUT,
	[SDE_SSPP_DMA_IGC] = IGC,
	[SDE_SSPP_DMA_GC] = GC,
	[SDE_SSPP_SCALER_QSEED3] = QSEED,
	[SDE_SSPP_SCALER_QSEED3LITE] = REG_DMA_FEATURES_MAX,
};

static u32 feature_reg_dma_sz[SDE_DSPP_MAX] = {
	[SDE_DSPP_VLUT] = VLUT_MEM_SIZE,
	[SDE_DSPP_GAMUT] = GAMUT_LUT_MEM_SIZE,
	[SDE_DSPP_GC] = GC_LUT_MEM_SIZE,
	[SDE_DSPP_IGC] = IGC_LUT_MEM_SIZE,
	[SDE_DSPP_PCC] = PCC_MEM_SIZE,
	[SDE_DSPP_HSIC] = HSIC_MEM_SIZE,
	[SDE_DSPP_SIXZONE] = SIXZONE_MEM_SIZE,
	[SDE_DSPP_MEMCOLOR] = MEMCOLOR_MEM_SIZE,
};

static u32 sspp_feature_reg_dma_sz[SDE_SSPP_MAX] = {
	[SDE_SSPP_VIG_IGC] = IGC_LUT_MEM_SIZE,
	[SDE_SSPP_VIG_GAMUT] = GAMUT_LUT_MEM_SIZE,
	[SDE_SSPP_DMA_IGC] = IGC_LUT_MEM_SIZE,
	[SDE_SSPP_DMA_GC] = GC_LUT_MEM_SIZE,
	[SDE_SSPP_SCALER_QSEED3] = QSEED3_MEM_SIZE,
};

static u32 dspp_mapping[DSPP_MAX] = {
	[DSPP_0] = DSPP0,
	[DSPP_1] = DSPP1,
	[DSPP_2] = DSPP2,
	[DSPP_3] = DSPP3,
};

static u32 sspp_mapping[SSPP_MAX] = {
	[SSPP_VIG0] = VIG0,
	[SSPP_VIG1] = VIG1,
	[SSPP_VIG2] = VIG2,
	[SSPP_VIG3] = VIG3,
	[SSPP_DMA0] = DMA0,
	[SSPP_DMA1] = DMA1,
	[SSPP_DMA2] = DMA2,
	[SSPP_DMA3] = DMA3,
};

#define REG_DMA_INIT_OPS(cfg, block, reg_dma_feature, feature_dma_buf) \
	do { \
		memset(&cfg, 0, sizeof(cfg)); \
		(cfg).blk = block; \
		(cfg).feature = reg_dma_feature; \
		(cfg).dma_buf = feature_dma_buf; \
	} while (0)

#define REG_DMA_SETUP_OPS(cfg, block_off, data_ptr, data_len, op, \
		wrap_sz, wrap_inc, reg_mask) \
	do { \
		(cfg).ops = op; \
		(cfg).blk_offset = block_off; \
		(cfg).data_size = data_len; \
		(cfg).data = data_ptr; \
		(cfg).inc = wrap_inc; \
		(cfg).wrap_size = wrap_sz; \
		(cfg).mask = reg_mask; \
	} while (0)

#define REG_DMA_SETUP_KICKOFF(cfg, hw_ctl, feature_dma_buf, ops, ctl_q, \
		mode) \
	do { \
		memset(&cfg, 0, sizeof(cfg)); \
		(cfg).ctl = hw_ctl; \
		(cfg).dma_buf = feature_dma_buf; \
		(cfg).op = ops; \
		(cfg).queue_select = ctl_q; \
		(cfg).trigger_mode = mode; \
	} while (0)

static int reg_dma_buf_init(struct sde_reg_dma_buffer **buf, u32 sz);
static int reg_dma_dspp_check(struct sde_hw_dspp *ctx, void *cfg,
		enum sde_reg_dma_features feature);
static int reg_dma_sspp_check(struct sde_hw_pipe *ctx, void *cfg,
		enum sde_reg_dma_features feature,
		enum sde_sspp_multirect_index idx);

static int reg_dma_buf_init(struct sde_reg_dma_buffer **buf, u32 size)
{
	struct sde_hw_reg_dma_ops *dma_ops;

	dma_ops = sde_reg_dma_get_ops();
	if (IS_ERR_OR_NULL(dma_ops))
		return -ENOTSUPP;

	if (!buf) {
		DRM_ERROR("invalid buf\n");
		return -EINVAL;
	}

	/* buffer already initialized */
	if (*buf)
		return 0;

	*buf = dma_ops->alloc_reg_dma_buf(size);
	if (IS_ERR_OR_NULL(*buf))
		return -EINVAL;

	return 0;
}

static int reg_dma_dspp_check(struct sde_hw_dspp *ctx, void *cfg,
		enum sde_reg_dma_features feature)
{
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_hw_cp_cfg *hw_cfg = cfg;

	if (!cfg || !ctx) {
		DRM_ERROR("invalid cfg %pK ctx %pK\n", cfg, ctx);
		return -EINVAL;
	}

	dma_ops = sde_reg_dma_get_ops();
	if (IS_ERR_OR_NULL(dma_ops))
		return -EINVAL;

	if (!hw_cfg->ctl || ctx->idx >= DSPP_MAX ||
		feature >= REG_DMA_FEATURES_MAX) {
		DRM_ERROR("invalid ctl %pK dspp idx %d feature %d\n",
			hw_cfg->ctl, ctx->idx, feature);
		return -EINVAL;
	}

	if (!dspp_buf[feature][ctx->idx]) {
		DRM_ERROR("invalid dma_buf\n");
		return -EINVAL;
	}

	return 0;
}

int reg_dmav1_init_dspp_op_v4(int feature, enum sde_dspp idx)
{
	int rc = -ENOTSUPP;
	struct sde_hw_reg_dma_ops *dma_ops;
	bool is_supported = false;
	u32 blk;

	if (feature >= SDE_DSPP_MAX || idx >= DSPP_MAX) {
		DRM_ERROR("invalid feature %x max %x dspp idx %x max %xd\n",
			feature, SDE_DSPP_MAX, idx, DSPP_MAX);
		return rc;
	}

	if (feature_map[feature] >= REG_DMA_FEATURES_MAX) {
		DRM_ERROR("invalid feature map %d for feature %d\n",
			feature_map[feature], feature);
		return -ENOTSUPP;
	}

	dma_ops = sde_reg_dma_get_ops();
	if (IS_ERR_OR_NULL(dma_ops))
		return -ENOTSUPP;

	blk = (feature_map[feature] == IGC) ? DSPP_IGC : dspp_mapping[idx];
	rc = dma_ops->check_support(feature_map[feature], blk, &is_supported);
	if (!rc)
		rc = (is_supported) ? 0 : -ENOTSUPP;

	if (!rc) {
		if (feature == SDE_DSPP_MEMCOLOR) {
			rc = reg_dma_buf_init(
				&dspp_buf[MEMC_SKIN][idx],
				feature_reg_dma_sz[feature]);
			if (rc)
				return rc;
			rc = reg_dma_buf_init(
				&dspp_buf[MEMC_SKY][idx],
				feature_reg_dma_sz[feature]);
			if (rc)
				return rc;
			rc = reg_dma_buf_init(
				&dspp_buf[MEMC_FOLIAGE][idx],
				feature_reg_dma_sz[feature]);
			if (rc)
				return rc;
			rc = reg_dma_buf_init(
				&dspp_buf[MEMC_PROT][idx],
				feature_reg_dma_sz[feature]);
		} else {
			rc = reg_dma_buf_init(
				&dspp_buf[feature_map[feature]][idx],
				feature_reg_dma_sz[feature]);
		}

	}
	return rc;
}

void reg_dmav1_setup_dspp_vlutv18(struct sde_hw_dspp *ctx, void *cfg)
{
	struct drm_msm_pa_vlut *payload = NULL;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct sde_reg_dma_kickoff_cfg kick_off;
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_hw_ctl *ctl = NULL;
	u32 *data = NULL;
	int i, j, rc = 0;

	rc = reg_dma_dspp_check(ctx, cfg, VLUT);
	if (rc)
		return;

	ctl = hw_cfg->ctl;
	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("Disable vlut feature\n");
		SDE_REG_WRITE(&ctx->hw,
		    ctx->cap->sblk->hist.base + PA_LUTV_DSPP_CTRL_OFF, 0);
		goto exit;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_pa_vlut)) {
		DRM_ERROR("invalid size of payload len %d exp %zd\n",
				hw_cfg->len, sizeof(struct drm_msm_pa_vlut));
		return;
	}

	dma_ops = sde_reg_dma_get_ops();
	dma_ops->reset_reg_dma_buf(dspp_buf[VLUT][ctx->idx]);

	REG_DMA_INIT_OPS(dma_write_cfg, dspp_mapping[ctx->idx],
	VLUT, dspp_buf[VLUT][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	data = kzalloc(VLUT_LEN, GFP_KERNEL);
	if (!data)
		return;

	payload = hw_cfg->payload;
	DRM_DEBUG_DRIVER("Enable vlut feature flags %llx\n", payload->flags);
	for (i = 0, j = 0; i < ARRAY_SIZE(payload->val); i += 2, j++)
		data[j] = (payload->val[i] & REG_MASK(10)) |
		((payload->val[i + 1] & REG_MASK(10)) << 16);


	REG_DMA_SETUP_OPS(dma_write_cfg, ctx->cap->sblk->vlut.base, data,
			VLUT_LEN, REG_BLK_WRITE_SINGLE, 0, 0, 0);

	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write pa vlut failed ret %d\n", rc);
		goto exit;
	}

	i = 1;
	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->hist.base + PA_LUTV_DSPP_CTRL_OFF, &i,
		sizeof(i), REG_SINGLE_WRITE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("opmode write single reg failed ret %d\n", rc);
		goto exit;
	}
	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->hist.base + PA_LUTV_DSPP_SWAP_OFF, &i,
		sizeof(i), REG_SINGLE_WRITE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("opmode write single reg failed ret %d\n", rc);
		goto exit;
	}

	REG_DMA_SETUP_KICKOFF(kick_off, hw_cfg->ctl, dspp_buf[VLUT][ctx->idx],
	    REG_DMA_WRITE, DMA_CTL_QUEUE0, WRITE_IMMEDIATE);
	rc = dma_ops->kick_off(&kick_off);
	if (rc) {
		DRM_ERROR("failed to kick off ret %d\n", rc);
		goto exit;
	}

exit:
	kfree(data);
	/* update flush bit */
	if (!rc && ctl && ctl->ops.update_bitmask_dspp_pavlut)
		ctl->ops.update_bitmask_dspp_pavlut(ctl, ctx->idx, true);
}

static int sde_gamut_get_mode_info(u32 pipe, struct drm_msm_3d_gamut *payload,
		u32 *tbl_len, u32 *tbl_off, u32 *opcode, u32 *scale_off)
{
	int rc = 0;

	if (payload->mode > GAMUT_3D_MODE_13) {
		DRM_ERROR("invalid mode %d", payload->mode);
		return -EINVAL;
	}

	switch (payload->mode) {
	case GAMUT_3D_MODE_17:
		*tbl_len = GAMUT_3D_MODE17_TBL_SZ * sizeof(u32) * 2;
		*tbl_off = 0;
		if (pipe == DSPP) {
			*scale_off = GAMUT_SCALEA_OFFSET_OFF;
			*opcode = gamut_mode_17;
		} else {
			*opcode = (*opcode & (BIT(5) - 1)) >> 2;
			if (*opcode == gamut_mode_17b)
				*opcode = gamut_mode_17;
			else
				*opcode = gamut_mode_17b;
			*scale_off = (*opcode == gamut_mode_17) ?
				GAMUT_SCALEA_OFFSET_OFF :
				GAMUT_SCALEB_OFFSET_OFF;
		}
		*opcode <<= 2;
		break;
	case GAMUT_3D_MODE_5:
		*tbl_len = GAMUT_3D_MODE5_TBL_SZ * sizeof(u32) * 2;
		*tbl_off = GAMUT_MODE_5_OFF;
		*scale_off = GAMUT_SCALEB_OFFSET_OFF;
		*opcode = gamut_mode_5 << 2;
		break;
	case GAMUT_3D_MODE_13:
		*tbl_len = GAMUT_3D_MODE13_TBL_SZ * sizeof(u32) * 2;
		*opcode = (*opcode & (BIT(4) - 1)) >> 2;
		if (*opcode == gamut_mode_13a)
			*opcode = gamut_mode_13b;
		else
			*opcode = gamut_mode_13a;
		*tbl_off = (*opcode == gamut_mode_13a) ? 0 :
			GAMUT_MODE_13B_OFF;
		*scale_off = (*opcode == gamut_mode_13a) ?
			GAMUT_SCALEA_OFFSET_OFF : GAMUT_SCALEB_OFFSET_OFF;
		*opcode <<= 2;
		break;
	default:
		rc = -EINVAL;
		break;
	}
	if (payload->flags & GAMUT_3D_MAP_EN)
		*opcode |= GAMUT_MAP_EN;
	*opcode |= GAMUT_EN;

	return rc;
}

static void dspp_3d_gamutv4_off(struct sde_hw_dspp *ctx, void *cfg)
{
	struct sde_reg_dma_kickoff_cfg kick_off;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	u32 op_mode = 0;
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;
	int rc;

	dma_ops = sde_reg_dma_get_ops();
	dma_ops->reset_reg_dma_buf(dspp_buf[GAMUT][ctx->idx]);

	REG_DMA_INIT_OPS(dma_write_cfg, dspp_mapping[ctx->idx], GAMUT,
			dspp_buf[GAMUT][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->gamut.base,
		&op_mode, sizeof(op_mode), REG_SINGLE_WRITE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("opmode write single reg failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_KICKOFF(kick_off, hw_cfg->ctl, dspp_buf[GAMUT][ctx->idx],
			REG_DMA_WRITE, DMA_CTL_QUEUE0, WRITE_IMMEDIATE);
	rc = dma_ops->kick_off(&kick_off);
	if (rc)
		DRM_ERROR("failed to kick off ret %d\n", rc);
}

static void reg_dmav1_setup_dspp_3d_gamutv4_common(struct sde_hw_dspp *ctx,
		void *cfg, u32 scale_tbl_a_len, u32 scale_tbl_b_len)
{
	struct drm_msm_3d_gamut *payload;
	struct sde_reg_dma_kickoff_cfg kick_off;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	u32 op_mode, reg, tbl_len, tbl_off, scale_off, i;
	u32 scale_tbl_len, scale_tbl_off;
	u32 *scale_data;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;
	struct sde_hw_reg_dma_ops *dma_ops;
	int rc;

	rc = reg_dma_dspp_check(ctx, cfg, GAMUT);
	if (rc)
		return;

	op_mode = SDE_REG_READ(&ctx->hw, ctx->cap->sblk->gamut.base);
	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("disable gamut feature\n");
		dspp_3d_gamutv4_off(ctx, cfg);
		return;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_3d_gamut)) {
		DRM_ERROR("invalid size of payload len %d exp %zd\n",
				hw_cfg->len, sizeof(struct drm_msm_3d_gamut));
		return;
	}
	payload = hw_cfg->payload;
	rc = sde_gamut_get_mode_info(DSPP, payload, &tbl_len, &tbl_off,
			&op_mode, &scale_off);
	if (rc) {
		DRM_ERROR("invalid mode info rc %d\n", rc);
		return;
	}

	dma_ops = sde_reg_dma_get_ops();
	dma_ops->reset_reg_dma_buf(dspp_buf[GAMUT][ctx->idx]);

	REG_DMA_INIT_OPS(dma_write_cfg, dspp_mapping[ctx->idx], GAMUT,
			dspp_buf[GAMUT][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}
	for (i = 0; i < GAMUT_3D_TBL_NUM; i++) {
		reg = GAMUT_TABLE0_SEL << i;
		reg |= ((tbl_off) & (BIT(11) - 1));
		REG_DMA_SETUP_OPS(dma_write_cfg,
			ctx->cap->sblk->gamut.base + GAMUT_TABLE_SEL_OFF,
			&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0, 0);
		rc = dma_ops->setup_payload(&dma_write_cfg);
		if (rc) {
			DRM_ERROR("write tbl sel reg failed ret %d\n", rc);
			return;
		}
		REG_DMA_SETUP_OPS(dma_write_cfg,
		    ctx->cap->sblk->gamut.base + GAMUT_LOWER_COLOR_OFF,
		    &payload->col[i][0].c2_c1, tbl_len,
		    REG_BLK_WRITE_MULTIPLE, 2, 0, 0);
		rc = dma_ops->setup_payload(&dma_write_cfg);
		if (rc) {
			DRM_ERROR("write color reg failed ret %d\n", rc);
			return;
		}
	}

	if (op_mode & GAMUT_MAP_EN) {
		if (scale_off == GAMUT_SCALEA_OFFSET_OFF)
			scale_tbl_len = scale_tbl_a_len;
		else
			scale_tbl_len = scale_tbl_b_len;

		for (i = 0; i < GAMUT_3D_SCALE_OFF_TBL_NUM; i++) {
			scale_tbl_off = ctx->cap->sblk->gamut.base + scale_off +
					(i * scale_tbl_len);
			scale_data = &payload->scale_off[i][0];
			REG_DMA_SETUP_OPS(dma_write_cfg, scale_tbl_off,
					scale_data, scale_tbl_len,
					REG_BLK_WRITE_SINGLE, 0, 0, 0);
			rc = dma_ops->setup_payload(&dma_write_cfg);
			if (rc) {
				DRM_ERROR("write scale/off reg failed ret %d\n",
						rc);
				return;
			}
		}
	}

	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->gamut.base,
		&op_mode, sizeof(op_mode), REG_SINGLE_WRITE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("opmode write single reg failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_KICKOFF(kick_off, hw_cfg->ctl, dspp_buf[GAMUT][ctx->idx],
			REG_DMA_WRITE, DMA_CTL_QUEUE0, WRITE_IMMEDIATE);
	rc = dma_ops->kick_off(&kick_off);
	if (rc)
		DRM_ERROR("failed to kick off ret %d\n", rc);
}

void reg_dmav1_setup_dspp_3d_gamutv4(struct sde_hw_dspp *ctx, void *cfg)
{
	reg_dmav1_setup_dspp_3d_gamutv4_common(ctx, cfg, GAMUT_SCALE_OFF_LEN,
		GAMUT_SCALE_OFF_LEN_12);
}

void reg_dmav1_setup_dspp_3d_gamutv41(struct sde_hw_dspp *ctx, void *cfg)
{
	reg_dmav1_setup_dspp_3d_gamutv4_common(ctx, cfg, GAMUT_SCALE_OFF_LEN,
		GAMUT_SCALE_OFF_LEN);
}

void reg_dmav1_setup_dspp_3d_gamutv42(struct sde_hw_dspp *ctx, void *cfg)
{
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct drm_msm_3d_gamut *payload = NULL;
	uint32_t i, j, tmp;
	uint32_t scale_off[GAMUT_3D_SCALE_OFF_TBL_NUM][GAMUT_3D_SCALE_OFF_SZ];
	int rc;

	rc = reg_dma_dspp_check(ctx, cfg, GAMUT);
	if (rc)
		return;
	if (hw_cfg->payload && hw_cfg->len != sizeof(struct drm_msm_3d_gamut)) {
		DRM_ERROR("invalid payload len actual %d expected %zd",
				hw_cfg->len, sizeof(struct drm_msm_3d_gamut));
		return;
	}

	payload = hw_cfg->payload;
	if (payload && (payload->flags & GAMUT_3D_MAP_EN)) {
		for (i = 0; i < GAMUT_3D_SCALE_OFF_TBL_NUM; i++) {
			for (j = 0; j < GAMUT_3D_SCALE_OFF_SZ; j++) {
				scale_off[i][j] = payload->scale_off[i][j];
				tmp = payload->scale_off[i][j] & 0x1ffff000;
				payload->scale_off[i][j] &= 0xfff;
				tmp = tmp << 3;
				payload->scale_off[i][j] =
					tmp | payload->scale_off[i][j];
			}
		}
	}
	reg_dmav1_setup_dspp_3d_gamutv4_common(ctx, cfg, GAMUT_SCALE_OFF_LEN,
		GAMUT_SCALE_OFF_LEN);
	if (payload && (payload->flags & GAMUT_3D_MAP_EN)) {
		for (i = 0; i < GAMUT_3D_SCALE_OFF_TBL_NUM; i++) {
			for (j = 0; j < GAMUT_3D_SCALE_OFF_SZ; j++)
				payload->scale_off[i][j] = scale_off[i][j];
		}
	}
}

void reg_dmav1_setup_dspp_gcv18(struct sde_hw_dspp *ctx, void *cfg)
{
	struct drm_msm_pgc_lut *lut_cfg;
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_reg_dma_kickoff_cfg kick_off;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;
	int rc, i = 0;
	u32 reg;

	rc = reg_dma_dspp_check(ctx, cfg, GC);
	if (rc)
		return;

	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("disable pgc feature\n");
		SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->gc.base, 0);
		return;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_pgc_lut)) {
		DRM_ERROR("invalid size of payload len %d exp %zd\n",
				hw_cfg->len, sizeof(struct drm_msm_pgc_lut));
		return;
	}
	lut_cfg = hw_cfg->payload;

	dma_ops = sde_reg_dma_get_ops();
	dma_ops->reset_reg_dma_buf(dspp_buf[GC][ctx->idx]);

	REG_DMA_INIT_OPS(dma_write_cfg, dspp_mapping[ctx->idx], GC,
			dspp_buf[GC][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	for (i = 0; i < GC_TBL_NUM; i++) {
		reg = 0;
		REG_DMA_SETUP_OPS(dma_write_cfg,
			ctx->cap->sblk->gc.base + GC_C0_INDEX_OFF +
			(i * sizeof(u32) * 2),
			&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0, 0);
		rc = dma_ops->setup_payload(&dma_write_cfg);
		if (rc) {
			DRM_ERROR("index init failed ret %d\n", rc);
			return;
		}

		REG_DMA_SETUP_OPS(dma_write_cfg,
			ctx->cap->sblk->gc.base + GC_C0_OFF +
			(i * sizeof(u32) * 2),
			lut_cfg->c0 + (ARRAY_SIZE(lut_cfg->c0) * i),
			PGC_TBL_LEN * sizeof(u32),
			REG_BLK_WRITE_INC, 0, 0, 0);
		rc = dma_ops->setup_payload(&dma_write_cfg);
		if (rc) {
			DRM_ERROR("lut write failed ret %d\n", rc);
			return;
		}
	}

	reg = BIT(0);
	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->gc.base + GC_LUT_SWAP_OFF,
		&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("setting swap offset failed ret %d\n", rc);
		return;
	}

	reg = GC_EN | ((lut_cfg->flags & PGC_8B_ROUND) ? GC_8B_ROUND_EN : 0);
	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->gc.base,
		&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("enabling gamma correction failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_KICKOFF(kick_off, hw_cfg->ctl, dspp_buf[GC][ctx->idx],
			REG_DMA_WRITE, DMA_CTL_QUEUE0, WRITE_IMMEDIATE);
	rc = dma_ops->kick_off(&kick_off);
	if (rc) {
		DRM_ERROR("failed to kick off ret %d\n", rc);
		return;
	}
}

static void _dspp_igcv31_off(struct sde_hw_dspp *ctx, void *cfg)
{
	struct sde_reg_dma_kickoff_cfg kick_off;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;
	int rc;
	u32 reg;

	dma_ops = sde_reg_dma_get_ops();
	dma_ops->reset_reg_dma_buf(dspp_buf[IGC][ctx->idx]);

	REG_DMA_INIT_OPS(dma_write_cfg, dspp_mapping[ctx->idx], IGC,
		dspp_buf[IGC][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	reg = IGC_DIS;
	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->igc.base + IGC_OPMODE_OFF,
		&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("setting opcode failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_KICKOFF(kick_off, hw_cfg->ctl, dspp_buf[IGC][ctx->idx],
			REG_DMA_WRITE, DMA_CTL_QUEUE0, WRITE_IMMEDIATE);
	rc = dma_ops->kick_off(&kick_off);
	if (rc)
		DRM_ERROR("failed to kick off ret %d\n", rc);
}

void reg_dmav1_setup_dspp_igcv31(struct sde_hw_dspp *ctx, void *cfg)
{
	struct drm_msm_igc_lut *lut_cfg;
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_reg_dma_kickoff_cfg kick_off;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;
	int rc, i = 0, j = 0;
	u32 *addr = NULL;
	u32 offset = 0;
	u32 reg;

	rc = reg_dma_dspp_check(ctx, cfg, IGC);
	if (rc)
		return;

	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("disable igc feature\n");
		_dspp_igcv31_off(ctx, cfg);
		return;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_igc_lut)) {
		DRM_ERROR("invalid size of payload len %d exp %zd\n",
				hw_cfg->len, sizeof(struct drm_msm_igc_lut));
		return;
	}

	lut_cfg = hw_cfg->payload;

	dma_ops = sde_reg_dma_get_ops();
	dma_ops->reset_reg_dma_buf(dspp_buf[IGC][ctx->idx]);

	REG_DMA_INIT_OPS(dma_write_cfg, DSPP_IGC, IGC, dspp_buf[IGC][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	for (i = 0; i < IGC_TBL_NUM; i++) {
		addr = lut_cfg->c0 + (i * ARRAY_SIZE(lut_cfg->c0));
		offset = IGC_C0_OFF + (i * sizeof(u32));

		for (j = 0; j < IGC_TBL_LEN; j++) {
			addr[j] &= IGC_DATA_MASK;
			addr[j] |= IGC_DSPP_SEL_MASK(ctx->idx - 1);
			if (j == 0)
				addr[j] |= IGC_INDEX_UPDATE;
		}

		REG_DMA_SETUP_OPS(dma_write_cfg, offset, addr,
			IGC_TBL_LEN * sizeof(u32),
			REG_BLK_WRITE_INC, 0, 0, 0);
		rc = dma_ops->setup_payload(&dma_write_cfg);
		if (rc) {
			DRM_ERROR("lut write failed ret %d\n", rc);
			return;
		}
	}

	REG_DMA_INIT_OPS(dma_write_cfg, dspp_mapping[ctx->idx], IGC,
		dspp_buf[IGC][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	if (lut_cfg->flags & IGC_DITHER_ENABLE) {
		reg = lut_cfg->strength & IGC_DITHER_DATA_MASK;
		REG_DMA_SETUP_OPS(dma_write_cfg,
			ctx->cap->sblk->igc.base + IGC_DITHER_OFF,
			&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0, 0);
		rc = dma_ops->setup_payload(&dma_write_cfg);
		if (rc) {
			DRM_ERROR("dither strength failed ret %d\n", rc);
			return;
		}
	}

	reg = IGC_EN;
	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->igc.base + IGC_OPMODE_OFF,
		&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("setting opcode failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_KICKOFF(kick_off, hw_cfg->ctl, dspp_buf[IGC][ctx->idx],
			REG_DMA_WRITE, DMA_CTL_QUEUE0, WRITE_IMMEDIATE);
	rc = dma_ops->kick_off(&kick_off);
	if (rc)
		DRM_ERROR("failed to kick off ret %d\n", rc);
}

static void _dspp_pccv4_off(struct sde_hw_dspp *ctx, void *cfg)
{
	struct sde_reg_dma_kickoff_cfg kick_off;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;
	int rc;
	u32 reg;

	dma_ops = sde_reg_dma_get_ops();
	dma_ops->reset_reg_dma_buf(dspp_buf[PCC][ctx->idx]);

	REG_DMA_INIT_OPS(dma_write_cfg, dspp_mapping[ctx->idx], PCC,
		dspp_buf[PCC][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	reg = PCC_DIS;
	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->pcc.base,
		&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("setting opcode failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_KICKOFF(kick_off, hw_cfg->ctl, dspp_buf[PCC][ctx->idx],
			REG_DMA_WRITE, DMA_CTL_QUEUE0, WRITE_IMMEDIATE);
	rc = dma_ops->kick_off(&kick_off);
	if (rc)
		DRM_ERROR("failed to kick off ret %d\n", rc);
}

void reg_dmav1_setup_dspp_pccv4(struct sde_hw_dspp *ctx, void *cfg)
{
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_reg_dma_kickoff_cfg kick_off;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;
	struct drm_msm_pcc *pcc_cfg;
	struct drm_msm_pcc_coeff *coeffs = NULL;
	u32 *data = NULL;
	int rc, i = 0;
	u32 reg = 0;

	rc = reg_dma_dspp_check(ctx, cfg, PCC);
	if (rc)
		return;

	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("disable pcc feature\n");
		_dspp_pccv4_off(ctx, cfg);
		return;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_pcc)) {
		DRM_ERROR("invalid size of payload len %d exp %zd\n",
				hw_cfg->len, sizeof(struct drm_msm_pcc));
		return;
	}

	pcc_cfg = hw_cfg->payload;

	dma_ops = sde_reg_dma_get_ops();
	dma_ops->reset_reg_dma_buf(dspp_buf[PCC][ctx->idx]);

	REG_DMA_INIT_OPS(dma_write_cfg, dspp_mapping[ctx->idx],
		PCC, dspp_buf[PCC][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	data = kzalloc(PCC_LEN, GFP_KERNEL);
	if (!data)
		return;

	for (i = 0; i < PCC_NUM_PLANES; i++) {
		switch (i) {
		case 0:
			coeffs = &pcc_cfg->r;
			data[i + 24] = pcc_cfg->r_rr;
			data[i + 27] = pcc_cfg->r_gg;
			data[i + 30] = pcc_cfg->r_bb;
			break;
		case 1:
			coeffs = &pcc_cfg->g;
			data[i + 24] = pcc_cfg->g_rr;
			data[i + 27] = pcc_cfg->g_gg;
			data[i + 30] = pcc_cfg->g_bb;
			break;
		case 2:
			coeffs = &pcc_cfg->b;
			data[i + 24] = pcc_cfg->b_rr;
			data[i + 27] = pcc_cfg->b_gg;
			data[i + 30] = pcc_cfg->b_bb;
			break;
		default:
			DRM_ERROR("invalid pcc plane: %d\n", i);
			goto exit;
		}

		data[i] = coeffs->c;
		data[i + 3] = coeffs->r;
		data[i + 6] = coeffs->g;
		data[i + 9] = coeffs->b;
		data[i + 12] = coeffs->rg;
		data[i + 15] = coeffs->rb;
		data[i + 18] = coeffs->gb;
		data[i + 21] = coeffs->rgb;
	}

	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->pcc.base + PCC_C_OFF,
		data, PCC_LEN,
		REG_BLK_WRITE_SINGLE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write pcc lut failed ret %d\n", rc);
		goto exit;
	}

	reg = PCC_EN;
	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->pcc.base,
		&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("setting opcode failed ret %d\n", rc);
		goto exit;
	}

	REG_DMA_SETUP_KICKOFF(kick_off, hw_cfg->ctl, dspp_buf[PCC][ctx->idx],
			REG_DMA_WRITE, DMA_CTL_QUEUE0, WRITE_IMMEDIATE);
	rc = dma_ops->kick_off(&kick_off);
	if (rc)
		DRM_ERROR("failed to kick off ret %d\n", rc);

exit:
	kfree(data);
}

void reg_dmav1_setup_dspp_pa_hsicv17(struct sde_hw_dspp *ctx, void *cfg)
{
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_reg_dma_kickoff_cfg kick_off;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;
	struct drm_msm_pa_hsic *hsic_cfg;
	u32 reg = 0, opcode = 0, local_opcode = 0;
	int rc;

	opcode = SDE_REG_READ(&ctx->hw, ctx->cap->sblk->hsic.base);

	rc = reg_dma_dspp_check(ctx, cfg, HSIC);
	if (rc)
		return;

	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("disable pa hsic feature\n");
		opcode &= ~(PA_HUE_EN | PA_SAT_EN | PA_VAL_EN | PA_CONT_EN);
		if (PA_DISABLE_REQUIRED(opcode))
			opcode &= ~PA_EN;
		SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->hsic.base, opcode);
		return;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_pa_hsic)) {
		DRM_ERROR("invalid size of payload len %d exp %zd\n",
				hw_cfg->len, sizeof(struct drm_msm_pa_hsic));
		return;
	}

	hsic_cfg = hw_cfg->payload;

	dma_ops = sde_reg_dma_get_ops();
	dma_ops->reset_reg_dma_buf(dspp_buf[HSIC][ctx->idx]);

	REG_DMA_INIT_OPS(dma_write_cfg, dspp_mapping[ctx->idx],
		HSIC, dspp_buf[HSIC][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	if (hsic_cfg->flags & PA_HSIC_HUE_ENABLE) {
		reg = hsic_cfg->hue & PA_HUE_MASK;
		REG_DMA_SETUP_OPS(dma_write_cfg,
			ctx->cap->sblk->hsic.base + PA_HUE_OFF,
			&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0, 0);
		rc = dma_ops->setup_payload(&dma_write_cfg);
		if (rc) {
			DRM_ERROR("hsic hue write failed ret %d\n", rc);
			return;
		}
		local_opcode |= PA_HUE_EN;
	}

	if (hsic_cfg->flags & PA_HSIC_SAT_ENABLE) {
		reg = hsic_cfg->saturation & PA_SAT_MASK;
		REG_DMA_SETUP_OPS(dma_write_cfg,
			ctx->cap->sblk->hsic.base + PA_SAT_OFF,
			&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0, 0);
		rc = dma_ops->setup_payload(&dma_write_cfg);
		if (rc) {
			DRM_ERROR("hsic saturation write failed ret %d\n", rc);
			return;
		}
		local_opcode |= PA_SAT_EN;
	}

	if (hsic_cfg->flags & PA_HSIC_VAL_ENABLE) {
		reg = hsic_cfg->value & PA_VAL_MASK;
		REG_DMA_SETUP_OPS(dma_write_cfg,
			ctx->cap->sblk->hsic.base + PA_VAL_OFF,
			&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0, 0);
		rc = dma_ops->setup_payload(&dma_write_cfg);
		if (rc) {
			DRM_ERROR("hsic value write failed ret %d\n", rc);
			return;
		}
		local_opcode |= PA_VAL_EN;
	}

	if (hsic_cfg->flags & PA_HSIC_CONT_ENABLE) {
		reg = hsic_cfg->contrast & PA_CONT_MASK;
		REG_DMA_SETUP_OPS(dma_write_cfg,
			ctx->cap->sblk->hsic.base + PA_CONT_OFF,
			&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0, 0);
		rc = dma_ops->setup_payload(&dma_write_cfg);
		if (rc) {
			DRM_ERROR("hsic contrast write failed ret %d\n", rc);
			return;
		}
		local_opcode |= PA_CONT_EN;
	}

	if (local_opcode) {
		local_opcode |= PA_EN;
	} else {
		DRM_ERROR("Invalid hsic config 0x%x\n", local_opcode);
		return;
	}

	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->hsic.base, &local_opcode, sizeof(local_opcode),
		REG_SINGLE_MODIFY, 0, 0, REG_DMA_PA_MODE_HSIC_MASK);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("setting opcode failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_KICKOFF(kick_off, hw_cfg->ctl, dspp_buf[HSIC][ctx->idx],
			REG_DMA_WRITE, DMA_CTL_QUEUE0, WRITE_IMMEDIATE);
	rc = dma_ops->kick_off(&kick_off);
	if (rc)
		DRM_ERROR("failed to kick off ret %d\n", rc);
}

void reg_dmav1_setup_dspp_sixzonev17(struct sde_hw_dspp *ctx, void *cfg)
{
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_reg_dma_kickoff_cfg kick_off;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;
	struct drm_msm_sixzone *sixzone;
	u32 reg = 0, local_hold = 0;
	u32 opcode = 0, local_opcode = 0;
	int rc;

	opcode = SDE_REG_READ(&ctx->hw, ctx->cap->sblk->hsic.base);

	rc = reg_dma_dspp_check(ctx, cfg, SIX_ZONE);
	if (rc)
		return;

	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("disable sixzone feature\n");
		opcode &= ~(PA_SIXZONE_HUE_EN | PA_SIXZONE_SAT_EN |
			PA_SIXZONE_VAL_EN);
		if (PA_DISABLE_REQUIRED(opcode))
			opcode &= ~PA_EN;
		SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->hsic.base, opcode);
		return;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_sixzone)) {
		DRM_ERROR("invalid size of payload len %d exp %zd\n",
			hw_cfg->len, sizeof(struct drm_msm_sixzone));
		return;
	}

	sixzone = hw_cfg->payload;

	dma_ops = sde_reg_dma_get_ops();
	dma_ops->reset_reg_dma_buf(dspp_buf[SIX_ZONE][ctx->idx]);

	REG_DMA_INIT_OPS(dma_write_cfg, dspp_mapping[ctx->idx],
		SIX_ZONE, dspp_buf[SIX_ZONE][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	reg = BIT(26);
	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->sixzone.base,
		&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("setting lut index failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_OPS(dma_write_cfg,
	    (ctx->cap->sblk->sixzone.base + SIXZONE_ADJ_CURVE_P1_OFF),
		&sixzone->curve[0].p1, (SIXZONE_LUT_SIZE * sizeof(u32) * 2),
		REG_BLK_WRITE_MULTIPLE, 2, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write sixzone lut failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->sixzone.base + SIXZONE_THRESHOLDS_OFF,
		&sixzone->threshold, 3 * sizeof(u32),
		REG_BLK_WRITE_SINGLE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write sixzone threshold failed ret %d\n", rc);
		return;
	}

	local_hold = ((sixzone->sat_hold & REG_MASK(2)) << 12);
	local_hold |= ((sixzone->val_hold & REG_MASK(2)) << 14);
	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->hsic.base + PA_PWL_HOLD_OFF, &local_hold,
		sizeof(local_hold), REG_SINGLE_MODIFY, 0, 0,
		REG_DMA_PA_PWL_HOLD_SZONE_MASK);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("setting local_hold failed ret %d\n", rc);
		return;
	}

	if (sixzone->flags & SIXZONE_HUE_ENABLE)
		local_opcode |= PA_SIXZONE_HUE_EN;
	if (sixzone->flags & SIXZONE_SAT_ENABLE)
		local_opcode |= PA_SIXZONE_SAT_EN;
	if (sixzone->flags & SIXZONE_VAL_ENABLE)
		local_opcode |= PA_SIXZONE_VAL_EN;

	if (local_opcode) {
		local_opcode |= PA_EN;
	} else {
		DRM_ERROR("Invalid six zone config 0x%x\n", local_opcode);
		return;
	}
	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->hsic.base, &local_opcode, sizeof(local_opcode),
		REG_SINGLE_MODIFY, 0, 0, REG_DMA_PA_MODE_SZONE_MASK);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("setting local_opcode failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_KICKOFF(kick_off, hw_cfg->ctl,
		dspp_buf[SIX_ZONE][ctx->idx],
		REG_DMA_WRITE, DMA_CTL_QUEUE0, WRITE_IMMEDIATE);
	rc = dma_ops->kick_off(&kick_off);
	if (rc)
		DRM_ERROR("failed to kick off ret %d\n", rc);
}

int reg_dmav1_deinit_dspp_ops(enum sde_dspp idx)
{
	int i;
	struct sde_hw_reg_dma_ops *dma_ops;

	dma_ops = sde_reg_dma_get_ops();
	if (IS_ERR_OR_NULL(dma_ops))
		return -ENOTSUPP;

	if (idx >= DSPP_MAX) {
		DRM_ERROR("invalid dspp idx %x max %xd\n", idx, DSPP_MAX);
		return -EINVAL;
	}

	for (i = 0; i < REG_DMA_FEATURES_MAX; i++) {
		if (!dspp_buf[i][idx])
			continue;
		dma_ops->dealloc_reg_dma(dspp_buf[i][idx]);
		dspp_buf[i][idx] = NULL;
	}
	return 0;
}

static void __setup_dspp_memcol(struct sde_hw_dspp *ctx,
		enum sde_reg_dma_features type,
		struct sde_hw_cp_cfg *hw_cfg)
{
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;
	struct sde_reg_dma_kickoff_cfg kick_off;
	struct drm_msm_memcol *memcolor;
	int rc;
	u32 addr = 0, idx = 0;
	u32 hold = 0, hold_shift = 0, mask = 0xFFFF;
	u32 opcode = 0, opcode_mask = 0xFFFFFFFF;

	switch (type) {
	case MEMC_SKIN:
		idx = 0;
		opcode |= PA_SKIN_EN;
		break;
	case MEMC_SKY:
		idx = 1;
		opcode |= PA_SKY_EN;
		break;
	case MEMC_FOLIAGE:
		idx = 2;
		opcode |= PA_FOL_EN;
		break;
	default:
		DRM_ERROR("Invalid memory color type %d\n", type);
		return;
	}

	dma_ops = sde_reg_dma_get_ops();
	dma_ops->reset_reg_dma_buf(dspp_buf[type][ctx->idx]);

	REG_DMA_INIT_OPS(dma_write_cfg, dspp_mapping[ctx->idx],
		type, dspp_buf[type][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	memcolor = hw_cfg->payload;
	addr = ctx->cap->sblk->memcolor.base + MEMCOL_PWL0_OFF +
		(idx * MEMCOL_SIZE0);
	/* write color_adjust_p0 and color_adjust_p1 */
	REG_DMA_SETUP_OPS(dma_write_cfg, addr, &memcolor->color_adjust_p0,
		sizeof(u32) * 2, REG_BLK_WRITE_SINGLE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("setting color_adjust_p0 failed ret %d\n", rc);
		return;
	}

	/* write hue/sat/val region */
	addr += 8;
	REG_DMA_SETUP_OPS(dma_write_cfg, addr, &memcolor->hue_region,
		sizeof(u32) * 3, REG_BLK_WRITE_SINGLE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("setting color_adjust_p0 failed ret %d\n", rc);
		return;
	}

	addr = ctx->cap->sblk->memcolor.base + MEMCOL_PWL2_OFF +
		(idx * MEMCOL_SIZE1);
	/* write color_adjust_p2 and blend_gain */
	REG_DMA_SETUP_OPS(dma_write_cfg, addr, &memcolor->color_adjust_p2,
		sizeof(u32) * 2, REG_BLK_WRITE_SINGLE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("setting color_adjust_p0 failed ret %d\n", rc);
		return;
	}

	addr = ctx->cap->sblk->hsic.base + PA_PWL_HOLD_OFF;
	hold_shift = idx * MEMCOL_HOLD_SIZE;
	hold = ((memcolor->sat_hold & REG_MASK(2)) << hold_shift);
	hold |= ((memcolor->val_hold & REG_MASK(2)) << (hold_shift + 2));
	mask &= ~REG_MASK_SHIFT(4, hold_shift);
	/* write sat_hold and val_hold in PA_PWL_HOLD */
	REG_DMA_SETUP_OPS(dma_write_cfg, addr, &hold, sizeof(hold),
		REG_SINGLE_MODIFY, 0, 0, mask);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("setting color_adjust_p0 failed ret %d\n", rc);
		return;
	}

	opcode |= PA_EN;
	opcode_mask &= ~(opcode);
	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->hsic.base, &opcode, sizeof(opcode),
		REG_SINGLE_MODIFY, 0, 0, opcode_mask);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("setting opcode failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_KICKOFF(kick_off, hw_cfg->ctl,
		dspp_buf[type][ctx->idx],
		REG_DMA_WRITE, DMA_CTL_QUEUE0, WRITE_IMMEDIATE);
	rc = dma_ops->kick_off(&kick_off);
	if (rc)
		DRM_ERROR("failed to kick off ret %d\n", rc);
}

void reg_dmav1_setup_dspp_memcol_skinv17(struct sde_hw_dspp *ctx, void *cfg)
{
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	u32 opcode = 0;
	int rc;

	if (!ctx || !cfg) {
		DRM_ERROR("invalid param ctx %pK cfg %pK\n", ctx, cfg);
		return;
	}

	rc = reg_dma_dspp_check(ctx, cfg, MEMC_SKIN);
	if (rc)
		return;

	opcode = SDE_REG_READ(&ctx->hw, ctx->cap->sblk->hsic.base);

	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("disable memcolor skin feature\n");
		opcode &= ~(PA_SKIN_EN);
		if (PA_DISABLE_REQUIRED(opcode))
			opcode &= ~PA_EN;
		SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->hsic.base, opcode);
		return;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_memcol)) {
		DRM_ERROR("invalid size of payload len %d exp %zd\n",
			hw_cfg->len, sizeof(struct drm_msm_memcol));
		return;
	}

	__setup_dspp_memcol(ctx, MEMC_SKIN, hw_cfg);
}

void reg_dmav1_setup_dspp_memcol_skyv17(struct sde_hw_dspp *ctx, void *cfg)
{
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	u32 opcode = 0;
	int rc;

	if (!ctx || !cfg) {
		DRM_ERROR("invalid param ctx %pK cfg %pK\n", ctx, cfg);
		return;
	}

	rc = reg_dma_dspp_check(ctx, cfg, MEMC_SKY);
	if (rc)
		return;

	opcode = SDE_REG_READ(&ctx->hw, ctx->cap->sblk->hsic.base);

	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("disable memcolor sky feature\n");
		opcode &= ~(PA_SKY_EN);
		if (PA_DISABLE_REQUIRED(opcode))
			opcode &= ~PA_EN;
		SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->hsic.base, opcode);
		return;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_memcol)) {
		DRM_ERROR("invalid size of payload len %d exp %zd\n",
			hw_cfg->len, sizeof(struct drm_msm_memcol));
		return;
	}

	__setup_dspp_memcol(ctx, MEMC_SKY, hw_cfg);
}

void reg_dmav1_setup_dspp_memcol_folv17(struct sde_hw_dspp *ctx, void *cfg)
{
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	u32 opcode = 0;
	int rc;

	if (!ctx || !cfg) {
		DRM_ERROR("invalid param ctx %pK cfg %pK\n", ctx, cfg);
		return;
	}

	rc = reg_dma_dspp_check(ctx, cfg, MEMC_FOLIAGE);
	if (rc)
		return;

	opcode = SDE_REG_READ(&ctx->hw, ctx->cap->sblk->hsic.base);

	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("disable memcolor foliage feature\n");
		opcode &= ~(PA_FOL_EN);
		if (PA_DISABLE_REQUIRED(opcode))
			opcode &= ~PA_EN;
		SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->hsic.base, opcode);
		return;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_memcol)) {
		DRM_ERROR("invalid size of payload len %d exp %zd\n",
			hw_cfg->len, sizeof(struct drm_msm_memcol));
		return;
	}

	__setup_dspp_memcol(ctx, MEMC_FOLIAGE, hw_cfg);
}

void reg_dmav1_setup_dspp_memcol_protv17(struct sde_hw_dspp *ctx, void *cfg)
{
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;
	struct sde_reg_dma_kickoff_cfg kick_off;
	struct drm_msm_memcol *memcolor;
	int rc;
	u32 opcode = 0, opcode_mask = 0xFFFFFFFF;

	if (!ctx || !cfg) {
		DRM_ERROR("invalid param ctx %pK cfg %pK\n", ctx, cfg);
		return;
	}

	rc = reg_dma_dspp_check(ctx, cfg, MEMC_PROT);
	if (rc)
		return;

	opcode = SDE_REG_READ(&ctx->hw, ctx->cap->sblk->hsic.base);

	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("disable memcolor prot feature\n");
		opcode &= ~(MEMCOL_PROT_MASK);
		if (PA_DISABLE_REQUIRED(opcode))
			opcode &= ~PA_EN;
		SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->hsic.base, opcode);
		return;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_memcol)) {
		DRM_ERROR("invalid size of payload len %d exp %zd\n",
			hw_cfg->len, sizeof(struct drm_msm_memcol));
		return;
	}

	memcolor = hw_cfg->payload;
	opcode = 0;
	if (memcolor->prot_flags) {
		if (memcolor->prot_flags & MEMCOL_PROT_HUE)
			opcode |= MEMCOL_PROT_HUE_EN;
		if (memcolor->prot_flags & MEMCOL_PROT_SAT)
			opcode |= MEMCOL_PROT_SAT_EN;
		if (memcolor->prot_flags & MEMCOL_PROT_VAL)
			opcode |= MEMCOL_PROT_VAL_EN;
		if (memcolor->prot_flags & MEMCOL_PROT_CONT)
			opcode |= MEMCOL_PROT_CONT_EN;
		if (memcolor->prot_flags & MEMCOL_PROT_SIXZONE)
			opcode |= MEMCOL_PROT_SIXZONE_EN;
		if (memcolor->prot_flags & MEMCOL_PROT_BLEND)
			opcode |= MEMCOL_PROT_BLEND_EN;
	}

	if (!opcode) {
		DRM_ERROR("Invalid memcolor prot config 0x%x\n", opcode);
		return;
	}
	opcode |= PA_EN;
	opcode_mask &= ~(MEMCOL_PROT_MASK);

	dma_ops = sde_reg_dma_get_ops();
	dma_ops->reset_reg_dma_buf(dspp_buf[MEMC_PROT][ctx->idx]);

	REG_DMA_INIT_OPS(dma_write_cfg, dspp_mapping[ctx->idx],
		MEMC_PROT, dspp_buf[MEMC_PROT][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->hsic.base, &opcode, sizeof(opcode),
		REG_SINGLE_MODIFY, 0, 0, opcode_mask);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("setting opcode failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_KICKOFF(kick_off, hw_cfg->ctl,
			dspp_buf[MEMC_PROT][ctx->idx], REG_DMA_WRITE,
			DMA_CTL_QUEUE0, WRITE_IMMEDIATE);
	rc = dma_ops->kick_off(&kick_off);
	if (rc)
		DRM_ERROR("failed to kick off ret %d\n", rc);
}

int reg_dmav1_init_sspp_op_v4(int feature, enum sde_sspp idx)
{
	int rc = -ENOTSUPP;
	struct sde_hw_reg_dma_ops *dma_ops;
	bool is_supported = false;
	u32 blk, i = 0;

	if (feature >= SDE_SSPP_MAX || idx >= SSPP_MAX) {
		DRM_ERROR("invalid feature %x max %x sspp idx %x max %xd\n",
			feature, SDE_SSPP_MAX, idx, SSPP_MAX);
		return rc;
	}

	if (sspp_feature_map[feature] >= REG_DMA_FEATURES_MAX) {
		DRM_ERROR("invalid feature map %d for feature %d\n",
			sspp_feature_map[feature], feature);
		return -ENOTSUPP;
	}

	dma_ops = sde_reg_dma_get_ops();
	if (IS_ERR_OR_NULL(dma_ops))
		return -ENOTSUPP;

	blk = sspp_mapping[idx];
	rc = dma_ops->check_support(sspp_feature_map[feature], blk,
				    &is_supported);
	if (!rc)
		rc = (is_supported) ? 0 : -ENOTSUPP;

	if (!rc) {
		for (i = SDE_SSPP_RECT_SOLO; i < SDE_SSPP_RECT_MAX; i++) {
			rc = reg_dma_buf_init(
				&sspp_buf[i][sspp_feature_map[feature]][idx],
				sspp_feature_reg_dma_sz[feature]);
			if (rc) {
				DRM_ERROR("rect %d buf init failed\n", i);
				break;
			}
		}

	}

	return rc;
}

static int reg_dma_sspp_check(struct sde_hw_pipe *ctx, void *cfg,
		enum sde_reg_dma_features feature,
		enum sde_sspp_multirect_index idx)
{
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_hw_cp_cfg *hw_cfg = cfg;

	if (!cfg || !ctx) {
		DRM_ERROR("invalid cfg %pK ctx %pK\n", cfg, ctx);
		return -EINVAL;
	}

	if (idx >= SDE_SSPP_RECT_MAX) {
		DRM_ERROR("invalid multirect idx %d\n", idx);
		return -EINVAL;
	}

	dma_ops = sde_reg_dma_get_ops();
	if (IS_ERR_OR_NULL(dma_ops))
		return -EINVAL;

	if (!hw_cfg->ctl || ctx->idx > SSPP_DMA3 || ctx->idx <= SSPP_NONE ||
		feature >= REG_DMA_FEATURES_MAX) {
		DRM_ERROR("invalid ctl %pK sspp idx %d feature %d\n",
			hw_cfg->ctl, ctx->idx, feature);
		return -EINVAL;
	}

	if (!sspp_buf[idx][feature][ctx->idx]) {
		DRM_ERROR("invalid dma_buf for rect idx %d sspp idx %d\n", idx,
			ctx->idx);
		return -EINVAL;
	}

	return 0;
}

static void vig_gamutv5_off(struct sde_hw_pipe *ctx, void *cfg)
{
	int rc;
	u32 op_mode = 0;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;
	struct sde_reg_dma_kickoff_cfg kick_off;
	u32 gamut_base = ctx->cap->sblk->gamut_blk.base - REG_DMA_VIG_SWI_DIFF;
	enum sde_sspp_multirect_index idx = SDE_SSPP_RECT_0;

	dma_ops = sde_reg_dma_get_ops();
	dma_ops->reset_reg_dma_buf(sspp_buf[idx][GAMUT][ctx->idx]);

	REG_DMA_INIT_OPS(dma_write_cfg, sspp_mapping[ctx->idx], GAMUT,
			sspp_buf[idx][GAMUT][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_OPS(dma_write_cfg, gamut_base,
		&op_mode, sizeof(op_mode), REG_SINGLE_MODIFY, 0, 0,
		REG_DMA_VIG_GAMUT_OP_MASK);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("opmode modify single reg failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_KICKOFF(kick_off, hw_cfg->ctl,
			sspp_buf[idx][GAMUT][ctx->idx], REG_DMA_WRITE,
			DMA_CTL_QUEUE0, WRITE_IMMEDIATE);
	rc = dma_ops->kick_off(&kick_off);
	if (rc)
		DRM_ERROR("failed to kick off ret %d\n", rc);
}

void reg_dmav1_setup_vig_gamutv5(struct sde_hw_pipe *ctx, void *cfg)
{
	int rc;
	u32 i, op_mode, reg, tbl_len, tbl_off, scale_off, scale_tbl_off;
	u32 *scale_data;
	struct drm_msm_3d_gamut *payload;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;
	struct sde_reg_dma_kickoff_cfg kick_off;
	u32 gamut_base = ctx->cap->sblk->gamut_blk.base - REG_DMA_VIG_SWI_DIFF;
	bool use_2nd_memory = false;
	enum sde_sspp_multirect_index idx = SDE_SSPP_RECT_0;

	rc = reg_dma_sspp_check(ctx, cfg, GAMUT, idx);
	if (rc)
		return;

	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("disable gamut feature\n");
		/* v5 and v6 call the same off version */
		vig_gamutv5_off(ctx, cfg);
		return;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_3d_gamut)) {
		DRM_ERROR("invalid size of payload len %d exp %zd\n",
				hw_cfg->len, sizeof(struct drm_msm_3d_gamut));
		return;
	}
	op_mode = SDE_REG_READ(&ctx->hw, ctx->cap->sblk->gamut_blk.base);
	payload = hw_cfg->payload;
	rc = sde_gamut_get_mode_info(SSPP, payload, &tbl_len, &tbl_off,
			&op_mode, &scale_off);
	if (rc) {
		DRM_ERROR("invalid mode info rc %d\n", rc);
		return;
	}

	dma_ops = sde_reg_dma_get_ops();
	dma_ops->reset_reg_dma_buf(sspp_buf[idx][GAMUT][ctx->idx]);

	REG_DMA_INIT_OPS(dma_write_cfg, sspp_mapping[ctx->idx], GAMUT,
			sspp_buf[idx][GAMUT][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}
	if ((op_mode & (BIT(5) - 1)) >> 2 == gamut_mode_17b)
		use_2nd_memory = true;
	for (i = 0; i < GAMUT_3D_TBL_NUM; i++) {
		reg = GAMUT_TABLE0_SEL << i;
		reg |= ((tbl_off) & (BIT(11) - 1));
		/* when bit 11 equals to 1, 2nd memory will be in use */
		if (use_2nd_memory)
			reg |= BIT(11);
		REG_DMA_SETUP_OPS(dma_write_cfg,
			gamut_base + GAMUT_TABLE_SEL_OFF,
			&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0, 0);
		rc = dma_ops->setup_payload(&dma_write_cfg);
		if (rc) {
			DRM_ERROR("write tbl sel reg failed ret %d\n", rc);
			return;
		}
		REG_DMA_SETUP_OPS(dma_write_cfg,
		    gamut_base + GAMUT_LOWER_COLOR_OFF,
		    &payload->col[i][0].c2_c1, tbl_len,
		    REG_BLK_WRITE_MULTIPLE, 2, 0, 0);
		rc = dma_ops->setup_payload(&dma_write_cfg);
		if (rc) {
			DRM_ERROR("write color reg failed ret %d\n", rc);
			return;
		}
	}

	if (op_mode & GAMUT_MAP_EN) {
		for (i = 0; i < GAMUT_3D_SCALE_OFF_TBL_NUM; i++) {
			scale_tbl_off = gamut_base + scale_off +
					(i * GAMUT_SCALE_OFF_LEN);
			scale_data = &payload->scale_off[i][0];
			REG_DMA_SETUP_OPS(dma_write_cfg, scale_tbl_off,
					scale_data, GAMUT_SCALE_OFF_LEN,
					REG_BLK_WRITE_SINGLE, 0, 0, 0);
			rc = dma_ops->setup_payload(&dma_write_cfg);
			if (rc) {
				DRM_ERROR("write scale/off reg failed ret %d\n",
						rc);
				return;
			}
		}
	}

	REG_DMA_SETUP_OPS(dma_write_cfg, gamut_base,
		&op_mode, sizeof(op_mode), REG_SINGLE_MODIFY, 0, 0,
		REG_DMA_VIG_GAMUT_OP_MASK);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("opmode write single reg failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_KICKOFF(kick_off, hw_cfg->ctl,
			sspp_buf[idx][GAMUT][ctx->idx], REG_DMA_WRITE,
			DMA_CTL_QUEUE0, WRITE_IMMEDIATE);
	rc = dma_ops->kick_off(&kick_off);
	if (rc)
		DRM_ERROR("failed to kick off ret %d\n", rc);
}

void reg_dmav1_setup_vig_gamutv6(struct sde_hw_pipe *ctx, void *cfg)
{
	reg_dmav1_setup_vig_gamutv5(ctx, cfg);
}

static void vig_igcv5_off(struct sde_hw_pipe *ctx, void *cfg)
{
	int rc;
	u32 op_mode = 0;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;
	struct sde_reg_dma_kickoff_cfg kick_off;
	u32 igc_base = ctx->cap->sblk->igc_blk[0].base - REG_DMA_VIG_SWI_DIFF;
	enum sde_sspp_multirect_index idx = SDE_SSPP_RECT_0;

	dma_ops = sde_reg_dma_get_ops();
	dma_ops->reset_reg_dma_buf(sspp_buf[idx][IGC][ctx->idx]);

	REG_DMA_INIT_OPS(dma_write_cfg, sspp_mapping[ctx->idx], IGC,
			sspp_buf[idx][IGC][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_OPS(dma_write_cfg, igc_base, &op_mode, sizeof(op_mode),
		REG_SINGLE_MODIFY, 0, 0, REG_DMA_VIG_IGC_OP_MASK);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("opmode modify single reg failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_KICKOFF(kick_off, hw_cfg->ctl,
			sspp_buf[idx][IGC][ctx->idx], REG_DMA_WRITE,
			DMA_CTL_QUEUE0, WRITE_IMMEDIATE);
	rc = dma_ops->kick_off(&kick_off);
	if (rc)
		DRM_ERROR("failed to kick off ret %d\n", rc);
}

static int reg_dmav1_setup_vig_igc_common(struct sde_hw_reg_dma_ops *dma_ops,
				struct sde_reg_dma_setup_ops_cfg *dma_write_cfg,
				struct sde_hw_pipe *ctx,
				struct sde_hw_cp_cfg *hw_cfg, u32 mask,
				struct drm_msm_igc_lut *igc_lut)
{
	int rc = 0;
	u32 i = 0, j = 0, reg = 0, index = 0;
	u32 offset = 0;
	u32 lut_sel = 0, lut_enable = 0;
	u32 *data = NULL, *data_ptr = NULL;
	u32 igc_base = ctx->cap->sblk->igc_blk[0].base - REG_DMA_VIG_SWI_DIFF;

	if (hw_cfg->len != sizeof(struct drm_msm_igc_lut)) {
		DRM_ERROR("invalid size of payload len %d exp %zd\n",
				hw_cfg->len, sizeof(struct drm_msm_igc_lut));
	}

	data = kzalloc(VIG_1D_LUT_IGC_LEN * sizeof(u32), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	reg = SDE_REG_READ(&ctx->hw, ctx->cap->sblk->igc_blk[0].base);
	lut_enable = (reg >> 8) & BIT(0);
	lut_sel = (reg >> 9) & BIT(0);
	/* select LUT table (0 or 1) when 1D LUT is in active mode */
	if (lut_enable)
		lut_sel = (~lut_sel) && BIT(0);

	for (i = 0; i < IGC_TBL_NUM; i++) {
		/* write 0 to the index register */
		index = 0;
		REG_DMA_SETUP_OPS(*dma_write_cfg, igc_base + 0x1B0,
			&index, sizeof(index), REG_SINGLE_WRITE, 0, 0, 0);
		rc = dma_ops->setup_payload(dma_write_cfg);
		if (rc) {
			DRM_ERROR("VIG IGC index write failed ret %d\n", rc);
			goto exit;
		}

		offset = igc_base + 0x1B4 + i * sizeof(u32);
		data_ptr = igc_lut->c0 + (ARRAY_SIZE(igc_lut->c0) * i);
		for (j = 0; j < VIG_1D_LUT_IGC_LEN; j++)
			data[j] = (data_ptr[2 * j] & mask) |
				(data_ptr[2 * j + 1] & mask) << 16;

		REG_DMA_SETUP_OPS(*dma_write_cfg, offset, data,
				VIG_1D_LUT_IGC_LEN * sizeof(u32),
				REG_BLK_WRITE_INC, 0, 0, 0);
		rc = dma_ops->setup_payload(dma_write_cfg);
		if (rc) {
			DRM_ERROR("lut write failed ret %d\n", rc);
			goto exit;
		}
	}

	if (igc_lut->flags & IGC_DITHER_ENABLE) {
		reg = igc_lut->strength & IGC_DITHER_DATA_MASK;
		reg |= BIT(4);
	} else {
		reg = 0;
	}
	REG_DMA_SETUP_OPS(*dma_write_cfg, igc_base + 0x1C0,
			&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0, 0);
	rc = dma_ops->setup_payload(dma_write_cfg);
	if (rc) {
		DRM_ERROR("dither strength failed ret %d\n", rc);
		goto exit;
	}

	reg = BIT(8) | (lut_sel << 9);
	REG_DMA_SETUP_OPS(*dma_write_cfg, igc_base, &reg, sizeof(reg),
		REG_SINGLE_MODIFY, 0, 0, REG_DMA_VIG_IGC_OP_MASK);
	rc = dma_ops->setup_payload(dma_write_cfg);
	if (rc)
		DRM_ERROR("setting opcode failed ret %d\n", rc);
exit:
	kfree(data);
	return rc;
}

void reg_dmav1_setup_vig_igcv5(struct sde_hw_pipe *ctx, void *cfg)
{
	int rc;
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_reg_dma_kickoff_cfg kick_off;
	struct drm_msm_igc_lut *igc_lut;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	enum sde_sspp_multirect_index idx = SDE_SSPP_RECT_0;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;

	rc = reg_dma_sspp_check(ctx, hw_cfg, IGC, idx);
	if (rc)
		return;

	igc_lut = hw_cfg->payload;
	if (!igc_lut) {
		DRM_DEBUG_DRIVER("disable igc feature\n");
		vig_igcv5_off(ctx, hw_cfg);
		return;
	}

	dma_ops = sde_reg_dma_get_ops();
	dma_ops->reset_reg_dma_buf(sspp_buf[idx][IGC][ctx->idx]);

	REG_DMA_INIT_OPS(dma_write_cfg, sspp_mapping[ctx->idx], IGC,
			sspp_buf[idx][IGC][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	rc = reg_dmav1_setup_vig_igc_common(dma_ops, &dma_write_cfg,
			ctx, cfg, VIG_IGC_DATA_MASK, igc_lut);
	if (rc) {
		DRM_ERROR("setup_vig_igc_common failed\n");
		return;
	}

	REG_DMA_SETUP_KICKOFF(kick_off, hw_cfg->ctl,
			sspp_buf[idx][IGC][ctx->idx], REG_DMA_WRITE,
			DMA_CTL_QUEUE0, WRITE_IMMEDIATE);
	rc = dma_ops->kick_off(&kick_off);
	if (rc)
		DRM_ERROR("failed to kick off ret %d\n", rc);
}

void reg_dmav1_setup_vig_igcv6(struct sde_hw_pipe *ctx, void *cfg)
{
	int rc;
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_reg_dma_kickoff_cfg kick_off;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	u32 igc_base = ctx->cap->sblk->igc_blk[0].base - REG_DMA_VIG_SWI_DIFF;
	enum sde_sspp_multirect_index idx = SDE_SSPP_RECT_0;
	struct drm_msm_igc_lut *igc_lut;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;

	rc = reg_dma_sspp_check(ctx, hw_cfg, IGC, idx);
	if (rc)
		return;

	igc_lut = hw_cfg->payload;
	if (!igc_lut) {
		DRM_DEBUG_DRIVER("disable igc feature\n");
		/* Both v5 and v6 call same igcv5_off */
		vig_igcv5_off(ctx, hw_cfg);
		return;
	}

	dma_ops = sde_reg_dma_get_ops();
	dma_ops->reset_reg_dma_buf(sspp_buf[idx][IGC][ctx->idx]);

	REG_DMA_INIT_OPS(dma_write_cfg, sspp_mapping[ctx->idx], IGC,
			sspp_buf[idx][IGC][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	rc = reg_dmav1_setup_vig_igc_common(dma_ops, &dma_write_cfg,
			ctx, cfg, VIG_IGC_DATA_MASK_V6, igc_lut);
	if (rc) {
		DRM_ERROR("setup_vig_igcv6 failed\n");
		return;
	}

	/* Perform LAST_LUT required for v6*/
	REG_DMA_SETUP_OPS(dma_write_cfg, igc_base + 0x1C4, &igc_lut->c0_last,
		sizeof(u32) * 3, REG_BLK_WRITE_SINGLE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("c_last failed ret %d", rc);
		return;
	}

	REG_DMA_SETUP_KICKOFF(kick_off, hw_cfg->ctl,
			sspp_buf[idx][IGC][ctx->idx], REG_DMA_WRITE,
			DMA_CTL_QUEUE0, WRITE_IMMEDIATE);
	rc = dma_ops->kick_off(&kick_off);
	if (rc)
		DRM_ERROR("failed to kick off ret %d\n", rc);
}

static void dma_igcv5_off(struct sde_hw_pipe *ctx, void *cfg,
			enum sde_sspp_multirect_index idx)
{
	int rc;
	u32 op_mode = 0;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;
	struct sde_reg_dma_kickoff_cfg kick_off;
	u32 igc_opmode_off;

	dma_ops = sde_reg_dma_get_ops();
	dma_ops->reset_reg_dma_buf(sspp_buf[idx][IGC][ctx->idx]);

	REG_DMA_INIT_OPS(dma_write_cfg, sspp_mapping[ctx->idx], IGC,
			sspp_buf[idx][IGC][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	if (idx == SDE_SSPP_RECT_SOLO || idx == SDE_SSPP_RECT_0)
		igc_opmode_off = DMA_DGM_0_OP_MODE_OFF;
	else
		igc_opmode_off = DMA_DGM_1_OP_MODE_OFF;

	REG_DMA_SETUP_OPS(dma_write_cfg, igc_opmode_off, &op_mode,
			sizeof(op_mode), REG_SINGLE_MODIFY, 0, 0,
			REG_DMA_DMA_IGC_OP_MASK);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("opmode modify single reg failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_KICKOFF(kick_off, hw_cfg->ctl,
			sspp_buf[idx][IGC][ctx->idx], REG_DMA_WRITE,
			DMA_CTL_QUEUE0, WRITE_IMMEDIATE);
	rc = dma_ops->kick_off(&kick_off);
	if (rc)
		DRM_ERROR("failed to kick off ret %d\n", rc);
}

void reg_dmav1_setup_dma_igcv5(struct sde_hw_pipe *ctx, void *cfg,
			enum sde_sspp_multirect_index idx)
{
	int rc;
	u32 i = 0, reg = 0;
	u32 *data = NULL;
	struct drm_msm_igc_lut *igc_lut;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;
	struct sde_reg_dma_kickoff_cfg kick_off;
	u32 igc_base, igc_dither_off, igc_opmode_off;

	rc = reg_dma_sspp_check(ctx, cfg, IGC, idx);
	if (rc)
		return;

	igc_lut = hw_cfg->payload;
	if (!igc_lut) {
		DRM_DEBUG_DRIVER("disable igc feature\n");
		dma_igcv5_off(ctx, cfg, idx);
		return;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_igc_lut)) {
		DRM_ERROR("invalid size of payload len %d exp %zd\n",
				hw_cfg->len, sizeof(struct drm_msm_igc_lut));
		return;
	}

	dma_ops = sde_reg_dma_get_ops();
	dma_ops->reset_reg_dma_buf(sspp_buf[idx][IGC][ctx->idx]);

	REG_DMA_INIT_OPS(dma_write_cfg, sspp_mapping[ctx->idx], IGC,
			sspp_buf[idx][IGC][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	data = kzalloc(DMA_1D_LUT_IGC_LEN * sizeof(u32), GFP_KERNEL);
	if (!data) {
		DRM_ERROR("failed to allocate memory for igc\n");
		return;
	}

	/* client packs the 1D LUT data in c2 instead of c0 */
	for (i = 0; i < DMA_1D_LUT_IGC_LEN; i++)
		data[i] = (igc_lut->c2[2 * i] & IGC_DATA_MASK) |
			((igc_lut->c2[2 * i + 1] & IGC_DATA_MASK) << 16);

	if (idx == SDE_SSPP_RECT_SOLO || idx == SDE_SSPP_RECT_0) {
		igc_base = ctx->cap->sblk->igc_blk[0].base -
				REG_DMA_DMA_SWI_DIFF;
		igc_dither_off = igc_base + DMA_1D_LUT_IGC_DITHER_OFF;
		igc_opmode_off = DMA_DGM_0_OP_MODE_OFF;
	} else {
		igc_base = ctx->cap->sblk->igc_blk[1].base -
				REG_DMA_DMA_SWI_DIFF;
		igc_dither_off = igc_base + DMA_1D_LUT_IGC_DITHER_OFF;
		igc_opmode_off = DMA_DGM_1_OP_MODE_OFF;
	}

	REG_DMA_SETUP_OPS(dma_write_cfg, igc_base, data,
			DMA_1D_LUT_IGC_LEN * sizeof(u32),
			REG_BLK_WRITE_SINGLE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("lut write failed ret %d\n", rc);
		goto igc_exit;
	}
	if (igc_lut->flags & IGC_DITHER_ENABLE) {
		reg = igc_lut->strength & IGC_DITHER_DATA_MASK;
		reg |= BIT(4);
	} else {
		reg = 0;
	}
	REG_DMA_SETUP_OPS(dma_write_cfg, igc_dither_off, &reg,
			sizeof(reg), REG_SINGLE_WRITE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("failed to set dither strength %d\n", rc);
		goto igc_exit;
	}

	reg = BIT(1);
	REG_DMA_SETUP_OPS(dma_write_cfg, igc_opmode_off, &reg, sizeof(reg),
			REG_SINGLE_MODIFY, 0, 0, REG_DMA_DMA_IGC_OP_MASK);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("setting opcode failed ret %d\n", rc);
		goto igc_exit;
	}

	REG_DMA_SETUP_KICKOFF(kick_off, hw_cfg->ctl,
			sspp_buf[idx][IGC][ctx->idx], REG_DMA_WRITE,
			DMA_CTL_QUEUE0, WRITE_IMMEDIATE);
	rc = dma_ops->kick_off(&kick_off);
	if (rc)
		DRM_ERROR("failed to kick off ret %d\n", rc);
igc_exit:
	kfree(data);
}

static void dma_gcv5_off(struct sde_hw_pipe *ctx, void *cfg,
			enum sde_sspp_multirect_index idx)
{
	int rc;
	u32 op_mode = 0;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;
	struct sde_reg_dma_kickoff_cfg kick_off;
	u32 gc_opmode_off;

	dma_ops = sde_reg_dma_get_ops();
	dma_ops->reset_reg_dma_buf(sspp_buf[idx][GC][ctx->idx]);

	REG_DMA_INIT_OPS(dma_write_cfg, sspp_mapping[ctx->idx], GC,
			sspp_buf[idx][GC][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	if (idx == SDE_SSPP_RECT_SOLO || idx == SDE_SSPP_RECT_0)
		gc_opmode_off = DMA_DGM_0_OP_MODE_OFF;
	else
		gc_opmode_off = DMA_DGM_1_OP_MODE_OFF;

	REG_DMA_SETUP_OPS(dma_write_cfg, gc_opmode_off, &op_mode,
			sizeof(op_mode), REG_SINGLE_MODIFY, 0, 0,
			REG_DMA_DMA_GC_OP_MASK);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("opmode modify single reg failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_KICKOFF(kick_off, hw_cfg->ctl,
			sspp_buf[idx][GC][ctx->idx], REG_DMA_WRITE,
			DMA_CTL_QUEUE0, WRITE_IMMEDIATE);
	rc = dma_ops->kick_off(&kick_off);
	if (rc)
		DRM_ERROR("failed to kick off ret %d\n", rc);
}

void reg_dmav1_setup_dma_gcv5(struct sde_hw_pipe *ctx, void *cfg,
			enum sde_sspp_multirect_index idx)
{
	int rc;
	u32 reg = 0;
	struct drm_msm_pgc_lut *gc_lut;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;
	struct sde_reg_dma_kickoff_cfg kick_off;
	u32 gc_base, gc_opmode_off;

	rc = reg_dma_sspp_check(ctx, cfg, GC, idx);
	if (rc)
		return;

	gc_lut = hw_cfg->payload;
	if (!gc_lut) {
		DRM_DEBUG_DRIVER("disable gc feature\n");
		dma_gcv5_off(ctx, cfg, idx);
		return;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_pgc_lut)) {
		DRM_ERROR("invalid size of payload len %d exp %zd\n",
				hw_cfg->len, sizeof(struct drm_msm_pgc_lut));
		return;
	}

	dma_ops = sde_reg_dma_get_ops();
	dma_ops->reset_reg_dma_buf(sspp_buf[idx][GC][ctx->idx]);

	REG_DMA_INIT_OPS(dma_write_cfg, sspp_mapping[ctx->idx], GC,
			sspp_buf[idx][GC][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	if (idx == SDE_SSPP_RECT_SOLO || idx == SDE_SSPP_RECT_0) {
		gc_base = ctx->cap->sblk->gc_blk[0].base - REG_DMA_DMA_SWI_DIFF;
		gc_opmode_off = DMA_DGM_0_OP_MODE_OFF;
	} else {
		gc_base = ctx->cap->sblk->gc_blk[1].base - REG_DMA_DMA_SWI_DIFF;
		gc_opmode_off = DMA_DGM_1_OP_MODE_OFF;
	}

	/* client packs the 1D LUT data in c2 instead of c0,
	 * and even & odd values are already stacked in register foramt
	 */
	REG_DMA_SETUP_OPS(dma_write_cfg, gc_base, gc_lut->c2,
			DMA_1D_LUT_GC_LEN * sizeof(u32),
			REG_BLK_WRITE_SINGLE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("lut write failed ret %d\n", rc);
		return;
	}
	reg = BIT(2);
	REG_DMA_SETUP_OPS(dma_write_cfg, gc_opmode_off, &reg,
			sizeof(reg), REG_SINGLE_MODIFY, 0, 0,
			REG_DMA_DMA_GC_OP_MASK);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("setting opcode failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_KICKOFF(kick_off, hw_cfg->ctl,
			sspp_buf[idx][GC][ctx->idx], REG_DMA_WRITE,
			DMA_CTL_QUEUE0, WRITE_IMMEDIATE);
	rc = dma_ops->kick_off(&kick_off);
	if (rc)
		DRM_ERROR("failed to kick off ret %d\n", rc);
}

int reg_dmav1_deinit_sspp_ops(enum sde_sspp idx)
{
	u32 i, j;
	struct sde_hw_reg_dma_ops *dma_ops;

	dma_ops = sde_reg_dma_get_ops();
	if (IS_ERR_OR_NULL(dma_ops))
		return -ENOTSUPP;

	if (idx >= SSPP_MAX) {
		DRM_ERROR("invalid sspp idx %x max %x\n", idx, SSPP_MAX);
		return -EINVAL;
	}

	for (i = SDE_SSPP_RECT_SOLO; i < SDE_SSPP_RECT_MAX; i++) {
		for (j = 0; j < REG_DMA_FEATURES_MAX; j++) {
			if (!sspp_buf[i][j][idx])
				continue;
			dma_ops->dealloc_reg_dma(sspp_buf[i][j][idx]);
			sspp_buf[i][j][idx] = NULL;
		}
	}
	return 0;
}

void reg_dmav1_setup_scaler3_lut(struct sde_reg_dma_setup_ops_cfg *buf,
		struct sde_hw_scaler3_cfg *scaler3_cfg, u32 offset)
{
	int i, filter, rc;
	int config_lut = 0x0;
	unsigned long lut_flags;
	u32 lut_addr, lut_offset, lut_len;
	struct sde_hw_reg_dma_ops *dma_ops;
	u32 *lut[QSEED3_FILTERS] = {NULL, NULL, NULL, NULL, NULL};
	static const uint32_t off_tbl[QSEED3_FILTERS][QSEED3_LUT_REGIONS][2] = {
		{{18, 0x000}, {12, 0x120}, {12, 0x1E0}, {8, 0x2A0} },
		{{6, 0x320}, {3, 0x3E0}, {3, 0x440}, {3, 0x4A0} },
		{{6, 0x500}, {3, 0x5c0}, {3, 0x620}, {3, 0x680} },
		{{6, 0x380}, {3, 0x410}, {3, 0x470}, {3, 0x4d0} },
		{{6, 0x560}, {3, 0x5f0}, {3, 0x650}, {3, 0x6b0} },
	};

	dma_ops = sde_reg_dma_get_ops();
	lut_flags = (unsigned long) scaler3_cfg->lut_flag;
	if (test_bit(QSEED3_COEF_LUT_DIR_BIT, &lut_flags) &&
		(scaler3_cfg->dir_len == QSEED3_DIR_LUT_SIZE)) {
		lut[0] = scaler3_cfg->dir_lut;
		config_lut = 1;
	}
	if (test_bit(QSEED3_COEF_LUT_Y_CIR_BIT, &lut_flags) &&
		(scaler3_cfg->y_rgb_cir_lut_idx < QSEED3_CIRCULAR_LUTS) &&
		(scaler3_cfg->cir_len == QSEED3_CIR_LUT_SIZE)) {
		lut[1] = scaler3_cfg->cir_lut +
			scaler3_cfg->y_rgb_cir_lut_idx * QSEED3_LUT_SIZE;
		config_lut = 1;
	}
	if (test_bit(QSEED3_COEF_LUT_UV_CIR_BIT, &lut_flags) &&
		(scaler3_cfg->uv_cir_lut_idx < QSEED3_CIRCULAR_LUTS) &&
		(scaler3_cfg->cir_len == QSEED3_CIR_LUT_SIZE)) {
		lut[2] = scaler3_cfg->cir_lut +
			scaler3_cfg->uv_cir_lut_idx * QSEED3_LUT_SIZE;
		config_lut = 1;
	}
	if (test_bit(QSEED3_COEF_LUT_Y_SEP_BIT, &lut_flags) &&
		(scaler3_cfg->y_rgb_sep_lut_idx < QSEED3_SEPARABLE_LUTS) &&
		(scaler3_cfg->sep_len == QSEED3_SEP_LUT_SIZE)) {
		lut[3] = scaler3_cfg->sep_lut +
			scaler3_cfg->y_rgb_sep_lut_idx * QSEED3_LUT_SIZE;
		config_lut = 1;
	}
	if (test_bit(QSEED3_COEF_LUT_UV_SEP_BIT, &lut_flags) &&
		(scaler3_cfg->uv_sep_lut_idx < QSEED3_SEPARABLE_LUTS) &&
		(scaler3_cfg->sep_len == QSEED3_SEP_LUT_SIZE)) {
		lut[4] = scaler3_cfg->sep_lut +
			scaler3_cfg->uv_sep_lut_idx * QSEED3_LUT_SIZE;
		config_lut = 1;
	}

	for (filter = 0; filter < QSEED3_FILTERS && config_lut; filter++) {
		if (!lut[filter])
			continue;
		lut_offset = 0;
		for (i = 0; i < QSEED3_LUT_REGIONS; i++) {
			lut_addr = QSEED3_COEF_LUT_OFF + offset
				+ off_tbl[filter][i][1];
			lut_len = off_tbl[filter][i][0] << 2;
			REG_DMA_SETUP_OPS(*buf, lut_addr,
				&lut[filter][lut_offset], lut_len * sizeof(u32),
				REG_BLK_WRITE_SINGLE, 0, 0, 0);
			rc = dma_ops->setup_payload(buf);
			if (rc) {
				DRM_ERROR("lut write failed ret %d\n", rc);
				return;
			}
			lut_offset += lut_len;
		}
	}

	if (test_bit(QSEED3_COEF_LUT_SWAP_BIT, &lut_flags)) {
		i = BIT(0);
		REG_DMA_SETUP_OPS(*buf, QSEED3_COEF_LUT_CTRL_OFF + offset, &i,
				sizeof(i), REG_SINGLE_WRITE, 0, 0, 0);
		rc = dma_ops->setup_payload(buf);
		if (rc) {
			DRM_ERROR("lut write failed ret %d\n", rc);
			return;
		}
	}

}

void reg_dmav1_setup_scaler3lite_lut(
		struct sde_reg_dma_setup_ops_cfg *buf,
			struct sde_hw_scaler3_cfg *scaler3_cfg, u32 offset)
{
	int i, filter, rc;
	int config_lut = 0x0;
	unsigned long lut_flags;
	u32 lut_addr, lut_offset;
	struct sde_hw_reg_dma_ops *dma_ops;
	u32 *lut[QSEED3LITE_FILTERS] = {NULL, NULL};
	static const uint32_t off_tbl[QSEED3LITE_FILTERS] = {0x000, 0x200};

	/* destination scaler case */
	if (!scaler3_cfg->sep_lut)
		return;

	dma_ops = sde_reg_dma_get_ops();
	lut_flags = (unsigned long) scaler3_cfg->lut_flag;
	if (test_bit(QSEED3L_COEF_LUT_Y_SEP_BIT, &lut_flags) &&
		(scaler3_cfg->y_rgb_sep_lut_idx < QSEED3L_SEPARABLE_LUTS) &&
		(scaler3_cfg->sep_len == QSEED3L_SEP_LUT_SIZE)) {
		lut[Y_INDEX] = scaler3_cfg->sep_lut +
			scaler3_cfg->y_rgb_sep_lut_idx * QSEED3L_LUT_SIZE;
		config_lut = 1;
	}
	if (test_bit(QSEED3L_COEF_LUT_UV_SEP_BIT, &lut_flags) &&
		(scaler3_cfg->uv_sep_lut_idx < QSEED3L_SEPARABLE_LUTS) &&
		(scaler3_cfg->sep_len == QSEED3L_SEP_LUT_SIZE)) {
		lut[UV_INDEX] = scaler3_cfg->sep_lut +
			scaler3_cfg->uv_sep_lut_idx * QSEED3L_LUT_SIZE;
		config_lut = 1;
	}

	for (filter = 0; filter < QSEED3LITE_FILTERS && config_lut; filter++) {
		if (!lut[filter])
			continue;
		lut_offset = 0;
		lut_addr = QSEED3L_COEF_LUT_OFF + offset
			+ off_tbl[filter];
		REG_DMA_SETUP_OPS(*buf, lut_addr,
				&lut[filter][0], QSEED3L_LUT_SIZE * sizeof(u32),
				REG_BLK_WRITE_SINGLE, 0, 0, 0);
		rc = dma_ops->setup_payload(buf);
		if (rc) {
			DRM_ERROR("lut write failed ret %d\n", rc);
			return;
		}
	}

	if (test_bit(QSEED3L_COEF_LUT_SWAP_BIT, &lut_flags)) {
		i = BIT(0);
		REG_DMA_SETUP_OPS(*buf, QSEED3L_COEF_LUT_CTRL_OFF + offset, &i,
				sizeof(i), REG_SINGLE_WRITE, 0, 0, 0);
		rc = dma_ops->setup_payload(buf);
		if (rc) {
			DRM_ERROR("lut write failed ret %d\n", rc);
			return;
		}
	}
}

static int reg_dmav1_setup_scaler3_de(struct sde_reg_dma_setup_ops_cfg *buf,
	struct sde_hw_scaler3_de_cfg *de_cfg, u32 offset)
{
	u32 de_config[7];
	struct sde_hw_reg_dma_ops *dma_ops;
	int rc;

	dma_ops = sde_reg_dma_get_ops();
	de_config[0] = (de_cfg->sharpen_level1 & 0x1FF) |
		((de_cfg->sharpen_level2 & 0x1FF) << 16);

	de_config[1] = ((de_cfg->limit & 0xF) << 9) |
		((de_cfg->prec_shift & 0x7) << 13) |
		((de_cfg->clip & 0x7) << 16) |
		((de_cfg->blend & 0xF) << 20);

	de_config[2] = (de_cfg->thr_quiet & 0xFF) |
		((de_cfg->thr_dieout & 0x3FF) << 16);

	de_config[3] = (de_cfg->thr_low & 0x3FF) |
		((de_cfg->thr_high & 0x3FF) << 16);

	de_config[4] = (de_cfg->adjust_a[0] & 0x3FF) |
		((de_cfg->adjust_a[1] & 0x3FF) << 10) |
		((de_cfg->adjust_a[2] & 0x3FF) << 20);

	de_config[5] = (de_cfg->adjust_b[0] & 0x3FF) |
		((de_cfg->adjust_b[1] & 0x3FF) << 10) |
		((de_cfg->adjust_b[2] & 0x3FF) << 20);

	de_config[6] = (de_cfg->adjust_c[0] & 0x3FF) |
		((de_cfg->adjust_c[1] & 0x3FF) << 10) |
		((de_cfg->adjust_c[2] & 0x3FF) << 20);

	offset += QSEED3_DE_OFFSET;
	REG_DMA_SETUP_OPS(*buf, offset,
		de_config, sizeof(de_config), REG_BLK_WRITE_SINGLE, 0, 0, 0);
	rc = dma_ops->setup_payload(buf);
	if (rc) {
		DRM_ERROR("de write failed ret %d\n", rc);
		return rc;
	}
	return 0;
}

void reg_dmav1_setup_vig_qseed3(struct sde_hw_pipe *ctx,
	struct sde_hw_pipe_cfg *sspp, struct sde_hw_pixel_ext *pe,
	void *scaler_cfg)
{
	struct sde_hw_scaler3_cfg *scaler3_cfg = scaler_cfg;
	int rc;
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;
	struct sde_reg_dma_kickoff_cfg kick_off;
	struct sde_hw_cp_cfg hw_cfg = {};
	u32 op_mode = 0, offset;
	u32 preload, src_y_rgb, src_uv, dst, dir_weight;
	u32 cache[4];
	enum sde_sspp_multirect_index idx = SDE_SSPP_RECT_0;

	if (!ctx || !pe || !scaler_cfg) {
		DRM_ERROR("invalid params ctx %pK pe %pK scaler_cfg %pK",
			ctx, pe, scaler_cfg);
		return;
	}

	hw_cfg.ctl = ctx->ctl;
	hw_cfg.payload = scaler_cfg;
	hw_cfg.len = sizeof(*scaler3_cfg);
	rc = reg_dma_sspp_check(ctx, &hw_cfg, QSEED, idx);
	if (rc || !sspp) {
		DRM_ERROR("invalid params rc %d sspp %pK\n", rc, sspp);
		return;
	}

	offset = ctx->cap->sblk->scaler_blk.base - REG_DMA_VIG_SWI_DIFF;
	dma_ops = sde_reg_dma_get_ops();
	dma_ops->reset_reg_dma_buf(sspp_buf[idx][QSEED][ctx->idx]);

	REG_DMA_INIT_OPS(dma_write_cfg, sspp_mapping[ctx->idx], QSEED,
	    sspp_buf[idx][QSEED][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	if (!scaler3_cfg->enable)
		goto end;

	op_mode = BIT(0);
	op_mode |= (scaler3_cfg->y_rgb_filter_cfg & 0x3) << 16;

	if (sspp->layout.format && SDE_FORMAT_IS_YUV(sspp->layout.format)) {
		op_mode |= BIT(12);
		op_mode |= (scaler3_cfg->uv_filter_cfg & 0x3) << 24;
	}

	op_mode |= (scaler3_cfg->blend_cfg & 1) << 31;
	op_mode |= (scaler3_cfg->dir_en) ? BIT(4) : 0;
	op_mode |= (scaler3_cfg->dyn_exp_disabled) ? BIT(13) : 0;

	preload =
		((scaler3_cfg->preload_x[0] & 0x7F) << 0) |
		((scaler3_cfg->preload_y[0] & 0x7F) << 8) |
		((scaler3_cfg->preload_x[1] & 0x7F) << 16) |
		((scaler3_cfg->preload_y[1] & 0x7F) << 24);

	src_y_rgb = (scaler3_cfg->src_width[0] & 0xFFFF) |
		((scaler3_cfg->src_height[0] & 0xFFFF) << 16);

	src_uv = (scaler3_cfg->src_width[1] & 0xFFFF) |
		((scaler3_cfg->src_height[1] & 0xFFFF) << 16);

	dst = (scaler3_cfg->dst_width & 0xFFFF) |
		((scaler3_cfg->dst_height & 0xFFFF) << 16);

	if (scaler3_cfg->de.enable) {
		rc = reg_dmav1_setup_scaler3_de(&dma_write_cfg,
			&scaler3_cfg->de, offset);
		if (!rc)
			op_mode |= BIT(8);
	}

	ctx->ops.setup_scaler_lut(&dma_write_cfg, scaler3_cfg, offset);

	cache[0] = scaler3_cfg->init_phase_x[0] & 0x1FFFFF;
	cache[1] = scaler3_cfg->init_phase_y[0] & 0x1FFFFF;
	cache[2] = scaler3_cfg->init_phase_x[1] & 0x1FFFFF;
	cache[3] = scaler3_cfg->init_phase_y[1] & 0x1FFFFF;
	REG_DMA_SETUP_OPS(dma_write_cfg,
		offset + 0x90, cache, sizeof(cache),
		REG_BLK_WRITE_SINGLE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("setting phase failed ret %d\n", rc);
		return;
	}

	cache[0] = scaler3_cfg->phase_step_x[0] & 0xFFFFFF;
	cache[1] = scaler3_cfg->phase_step_y[0] & 0xFFFFFF;
	cache[2] = scaler3_cfg->phase_step_x[1] & 0xFFFFFF;
	cache[3] = scaler3_cfg->phase_step_y[1] & 0xFFFFFF;
	REG_DMA_SETUP_OPS(dma_write_cfg,
		offset + 0x10, cache, sizeof(cache),
		REG_BLK_WRITE_SINGLE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("setting phase failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_OPS(dma_write_cfg,
		offset + 0x20, &preload, sizeof(u32),
		REG_SINGLE_WRITE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("setting preload failed ret %d\n", rc);
		return;
	}

	cache[0] = src_y_rgb;
	cache[1] = src_uv;
	cache[2] = dst;

	REG_DMA_SETUP_OPS(dma_write_cfg,
		offset + 0x40, cache, 3 * sizeof(u32),
		REG_BLK_WRITE_SINGLE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("setting sizes failed ret %d\n", rc);
		return;
	}

	if (is_qseed3_rev_qseed3lite(ctx->catalog)) {
		dir_weight = (scaler3_cfg->dir_weight & 0xFF);

		REG_DMA_SETUP_OPS(dma_write_cfg,
				offset + 0x60, &dir_weight, sizeof(u32),
				REG_SINGLE_WRITE, 0, 0, 0);
		rc = dma_ops->setup_payload(&dma_write_cfg);
		if (rc) {
			DRM_ERROR("lut write failed ret %d\n", rc);
			return;
		}
	}

end:
	if (sspp->layout.format) {
		if (!SDE_FORMAT_IS_DX(sspp->layout.format))
			op_mode |= BIT(14);
		if (sspp->layout.format->alpha_enable) {
			op_mode |= BIT(10);
			op_mode |= (scaler3_cfg->alpha_filter_cfg & 0x3) << 29;
		}
	}

	REG_DMA_SETUP_OPS(dma_write_cfg,
		offset + 0x4,
		&op_mode, sizeof(op_mode), REG_SINGLE_WRITE, 0, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("setting opmode failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_KICKOFF(kick_off, hw_cfg.ctl,
			sspp_buf[idx][QSEED][ctx->idx], REG_DMA_WRITE,
			DMA_CTL_QUEUE0, WRITE_IMMEDIATE);
	rc = dma_ops->kick_off(&kick_off);
	if (rc)
		DRM_ERROR("failed to kick off ret %d\n", rc);

}
