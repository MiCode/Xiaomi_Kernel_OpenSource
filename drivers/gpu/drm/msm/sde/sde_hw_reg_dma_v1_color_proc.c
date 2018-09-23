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
#include <drm/msm_drm_pp.h>
#include "sde_reg_dma.h"
#include "sde_hw_reg_dma_v1_color_proc.h"
#include "sde_hw_color_proc_common_v4.h"

/* Reserve space of 128 words for LUT dma payload set-up */
#define REG_DMA_HEADERS_BUFFER_SZ (sizeof(u32) * 128)

#define VLUT_MEM_SIZE ((128 * sizeof(u32)) + REG_DMA_HEADERS_BUFFER_SZ)
#define VLUT_LEN (128 * sizeof(u32))
#define PA_OP_MODE_OFF 0x800
#define PA_LUTV_OPMODE_OFF 0x84c

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

#define REG_MASK(n) ((BIT(n)) - 1)
#define REG_MASK_SHIFT(n, shift) ((REG_MASK(n)) << (shift))

static struct sde_reg_dma_buffer *dspp_buf[REG_DMA_FEATURES_MAX][DSPP_MAX];

static u32 feature_map[SDE_DSPP_MAX] = {
	[SDE_DSPP_VLUT] = REG_DMA_FEATURES_MAX,
	[SDE_DSPP_GAMUT] = GAMUT,
	[SDE_DSPP_IGC] = IGC,
	[SDE_DSPP_PCC] = PCC,
	[SDE_DSPP_GC] = GC,
	[SDE_DSPP_HSIC] = HSIC,
	[SDE_DSPP_MEMCOLOR] = REG_DMA_FEATURES_MAX,
	[SDE_DSPP_SIXZONE] = SIX_ZONE,
	[SDE_DSPP_DITHER] = REG_DMA_FEATURES_MAX,
	[SDE_DSPP_HIST] = REG_DMA_FEATURES_MAX,
	[SDE_DSPP_AD] = REG_DMA_FEATURES_MAX,
};

static u32 feature_reg_dma_sz[SDE_DSPP_MAX] = {
	[SDE_DSPP_VLUT] = VLUT_MEM_SIZE,
	[SDE_DSPP_GAMUT] = GAMUT_LUT_MEM_SIZE,
	[SDE_DSPP_GC] = GC_LUT_MEM_SIZE,
	[SDE_DSPP_IGC] = IGC_LUT_MEM_SIZE,
	[SDE_DSPP_PCC] = PCC_MEM_SIZE,
	[SDE_DSPP_HSIC] = HSIC_MEM_SIZE,
	[SDE_DSPP_SIXZONE] = SIXZONE_MEM_SIZE,
};

static u32 dspp_mapping[DSPP_MAX] = {
	[DSPP_0] = DSPP0,
	[DSPP_1] = DSPP1,
	[DSPP_2] = DSPP2,
	[DSPP_3] = DSPP3,
};

#define REG_DMA_INIT_OPS(cfg, block, reg_dma_feature, feature_dma_buf) \
	do { \
		memset(&cfg, 0, sizeof(cfg)); \
		(cfg).blk = block; \
		(cfg).feature = reg_dma_feature; \
		(cfg).dma_buf = feature_dma_buf; \
	} while (0)

#define REG_DMA_SETUP_OPS(cfg, block_off, data_ptr, data_len, op, \
		wrap_sz, wrap_inc) \
	do { \
		(cfg).ops = op; \
		(cfg).blk_offset = block_off; \
		(cfg).data_size = data_len; \
		(cfg).data = data_ptr; \
		(cfg).inc = wrap_inc; \
		(cfg).wrap_size = wrap_sz; \
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
static int reg_dma_blk_select(enum sde_reg_dma_features feature,
		enum sde_reg_dma_blk blk, struct sde_reg_dma_buffer *dma_buf);
static int reg_dma_write(enum sde_reg_dma_setup_ops ops, u32 off, u32 data_sz,
			u32 *data, struct sde_reg_dma_buffer *dma_buf,
			enum sde_reg_dma_features feature,
			enum sde_reg_dma_blk blk);
static int reg_dma_kick_off(enum sde_reg_dma_op op, enum sde_reg_dma_queue q,
		enum sde_reg_dma_trigger_mode mode,
		struct sde_reg_dma_buffer *dma_buf, struct sde_hw_ctl *ctl);

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

static int reg_dma_blk_select(enum sde_reg_dma_features feature,
		enum sde_reg_dma_blk blk, struct sde_reg_dma_buffer *dma_buf)
{
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;
	int rc = 0;

	dma_ops = sde_reg_dma_get_ops();
	dma_ops->reset_reg_dma_buf(dma_buf);
	memset(&dma_write_cfg, 0, sizeof(dma_write_cfg));
	dma_write_cfg.blk = blk;
	dma_write_cfg.feature = feature;
	dma_write_cfg.ops = HW_BLK_SELECT;
	dma_write_cfg.dma_buf = dma_buf;

	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc)
		DRM_ERROR("write decode select failed ret %d\n", rc);

	return rc;
}

static int reg_dma_write(enum sde_reg_dma_setup_ops ops, u32 off, u32 data_sz,
			u32 *data, struct sde_reg_dma_buffer *dma_buf,
			enum sde_reg_dma_features feature,
			enum sde_reg_dma_blk blk)
{
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;
	int rc;

	dma_ops = sde_reg_dma_get_ops();
	memset(&dma_write_cfg, 0, sizeof(dma_write_cfg));

	dma_write_cfg.ops = ops;
	dma_write_cfg.blk_offset = off;
	dma_write_cfg.data_size = data_sz;
	dma_write_cfg.data = data;
	dma_write_cfg.dma_buf = dma_buf;
	dma_write_cfg.feature = feature;
	dma_write_cfg.blk = blk;
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc)
		DRM_ERROR("write single reg failed ret %d\n", rc);

	return rc;
}

static int reg_dma_kick_off(enum sde_reg_dma_op op, enum sde_reg_dma_queue q,
		enum sde_reg_dma_trigger_mode mode,
		struct sde_reg_dma_buffer *dma_buf, struct sde_hw_ctl *ctl)
{
	struct sde_reg_dma_kickoff_cfg kick_off;
	struct sde_hw_reg_dma_ops *dma_ops;
	int rc;

	dma_ops = sde_reg_dma_get_ops();
	memset(&kick_off, 0, sizeof(kick_off));
	kick_off.ctl = ctl;
	kick_off.dma_buf = dma_buf;
	kick_off.op = op;
	kick_off.queue_select = q;
	kick_off.trigger_mode = mode;
	rc = dma_ops->kick_off(&kick_off);
	if (rc)
		DRM_ERROR("failed to kick off ret %d\n", rc);

	return rc;
}

bool reg_dmav1_dspp_feature_support(int feature)
{
	struct sde_hw_reg_dma_ops *dma_ops;
	bool is_supported = false;

	if (feature >= SDE_DSPP_MAX) {
		DRM_ERROR("invalid feature %x max %x\n",
			feature, SDE_DSPP_MAX);
		return is_supported;
	}

	if (feature_map[feature] >= REG_DMA_FEATURES_MAX) {
		DRM_ERROR("invalid feature map %d for feature %d\n",
			feature_map[feature], feature);
		return is_supported;
	}

	dma_ops = sde_reg_dma_get_ops();
	if (IS_ERR_OR_NULL(dma_ops))
		return is_supported;

	dma_ops->check_support(feature_map[feature], DSPP0, &is_supported);

	return is_supported;
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
		DRM_WARN("invalid feature map %d for feature %d\n",
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

	if (!rc)
		rc = reg_dma_buf_init(&dspp_buf[feature_map[feature]][idx],
				feature_reg_dma_sz[feature]);

	return rc;
}

int reg_dmav1_init_sspp_op_v4(int feature, enum sde_sspp idx)
{
	return -ENOTSUPP;
}

void reg_dmav1_setup_dspp_vlutv18(struct sde_hw_dspp *ctx, void *cfg)
{
	struct drm_msm_pa_vlut *payload = NULL;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	u32 op_mode;
	u32 *data = NULL;
	int i, j, rc = 0;

	rc = reg_dma_dspp_check(ctx, cfg, VLUT);
	if (rc)
		return;

	op_mode = SDE_REG_READ(&ctx->hw, PA_OP_MODE_OFF);
	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("Disable vlut feature\n");
		SDE_REG_WRITE(&ctx->hw, PA_LUTV_OPMODE_OFF, 0);
		if (PA_DISABLE_REQUIRED(op_mode))
			op_mode &= ~PA_EN;
		SDE_REG_WRITE(&ctx->hw, PA_OP_MODE_OFF, op_mode);
		return;
	}

	rc = reg_dma_blk_select(VLUT, dspp_mapping[ctx->idx],
			dspp_buf[VLUT][ctx->idx]);
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

	rc = reg_dma_write(REG_BLK_WRITE_SINGLE, ctx->cap->sblk->vlut.base,
			VLUT_LEN, data,
			dspp_buf[VLUT][ctx->idx], VLUT,
			dspp_mapping[ctx->idx]);
	if (rc) {
		DRM_ERROR("write single reg failed ret %d\n", rc);
		goto exit;
	}

	rc = reg_dma_kick_off(REG_DMA_WRITE, DMA_CTL_QUEUE0, WRITE_IMMEDIATE,
			dspp_buf[VLUT][ctx->idx], hw_cfg->ctl);
	if (rc) {
		DRM_ERROR("failed to kick off ret %d\n", rc);
		goto exit;
	}
	SDE_REG_WRITE(&ctx->hw, PA_LUTV_OPMODE_OFF, BIT(0));
	SDE_REG_WRITE(&ctx->hw, PA_OP_MODE_OFF, op_mode | BIT(20));

exit:
	kfree(data);
}

static int sde_gamut_get_mode_info(struct drm_msm_3d_gamut *payload,
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
		*scale_off = GAMUT_SCALEA_OFFSET_OFF;
		*opcode = gamut_mode_17 << 2;
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

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->gamut.base,
		&op_mode, sizeof(op_mode), REG_SINGLE_WRITE, 0, 0);
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
	rc = sde_gamut_get_mode_info(payload, &tbl_len, &tbl_off, &op_mode,
			&scale_off);
	if (rc) {
		DRM_ERROR("invalid mode info rc %d\n", rc);
		return;
	}

	dma_ops = sde_reg_dma_get_ops();
	dma_ops->reset_reg_dma_buf(dspp_buf[GAMUT][ctx->idx]);

	REG_DMA_INIT_OPS(dma_write_cfg, dspp_mapping[ctx->idx], GAMUT,
			dspp_buf[GAMUT][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0);
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
			&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0);
		rc = dma_ops->setup_payload(&dma_write_cfg);
		if (rc) {
			DRM_ERROR("write tbl sel reg failed ret %d\n", rc);
			return;
		}
		REG_DMA_SETUP_OPS(dma_write_cfg,
		    ctx->cap->sblk->gamut.base + GAMUT_LOWER_COLOR_OFF,
		    &payload->col[i][0].c2_c1, tbl_len,
		    REG_BLK_WRITE_MULTIPLE, 2, 0);
		rc = dma_ops->setup_payload(&dma_write_cfg);
		if (rc) {
			DRM_ERROR("write color reg failed ret %d\n", rc);
			return;
		}
	}

	if (op_mode & GAMUT_MAP_EN) {
		if (scale_off == GAMUT_SCALEA_OFFSET_OFF)
			scale_tbl_len = GAMUT_SCALE_OFF_LEN;
		else
			scale_tbl_len = GAMUT_SCALE_OFF_LEN_12;

		for (i = 0; i < GAMUT_3D_SCALE_OFF_TBL_NUM; i++) {
			scale_tbl_off = ctx->cap->sblk->gamut.base + scale_off +
					(i * scale_tbl_len);
			scale_data = &payload->scale_off[i][0];
			REG_DMA_SETUP_OPS(dma_write_cfg, scale_tbl_off,
					scale_data, scale_tbl_len,
					REG_BLK_WRITE_SINGLE, 0, 0);
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
		&op_mode, sizeof(op_mode), REG_SINGLE_WRITE, 0, 0);
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

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0);
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
			&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0);
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
			REG_BLK_WRITE_INC, 0, 0);
		rc = dma_ops->setup_payload(&dma_write_cfg);
		if (rc) {
			DRM_ERROR("lut write failed ret %d\n", rc);
			return;
		}
	}

	reg = BIT(0);
	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->gc.base + GC_LUT_SWAP_OFF,
		&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("setting swap offset failed ret %d\n", rc);
		return;
	}

	reg = GC_EN | ((lut_cfg->flags & PGC_8B_ROUND) ? GC_8B_ROUND_EN : 0);
	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->gc.base,
		&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0);
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

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	reg = IGC_DIS;
	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->igc.base + IGC_OPMODE_OFF,
		&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0);
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

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0);
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
			REG_BLK_WRITE_INC, 0, 0);
		rc = dma_ops->setup_payload(&dma_write_cfg);
		if (rc) {
			DRM_ERROR("lut write failed ret %d\n", rc);
			return;
		}
	}

	REG_DMA_INIT_OPS(dma_write_cfg, dspp_mapping[ctx->idx], IGC,
		dspp_buf[IGC][ctx->idx]);

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	if (lut_cfg->flags & IGC_DITHER_ENABLE) {
		reg = lut_cfg->strength & IGC_DITHER_DATA_MASK;
		REG_DMA_SETUP_OPS(dma_write_cfg,
			ctx->cap->sblk->igc.base + IGC_DITHER_OFF,
			&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0);
		rc = dma_ops->setup_payload(&dma_write_cfg);
		if (rc) {
			DRM_ERROR("dither strength failed ret %d\n", rc);
			return;
		}
	}

	reg = IGC_EN;
	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->igc.base + IGC_OPMODE_OFF,
		&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0);
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

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	reg = PCC_DIS;
	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->pcc.base,
		&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0);
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

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0);
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
		REG_BLK_WRITE_SINGLE, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write pcc lut failed ret %d\n", rc);
		goto exit;
	}

	reg = PCC_EN;
	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->pcc.base,
		&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0);
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

void reg_dmav1_setup_dspp_pa_hsicv18(struct sde_hw_dspp *ctx, void *cfg)
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

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	if (hsic_cfg->flags & PA_HSIC_HUE_ENABLE) {
		reg = hsic_cfg->hue & PA_HUE_MASK;
		REG_DMA_SETUP_OPS(dma_write_cfg,
			ctx->cap->sblk->hsic.base + PA_HUE_OFF,
			&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0);
		rc = dma_ops->setup_payload(&dma_write_cfg);
		if (rc) {
			DRM_ERROR("hsic hue write failed ret %d\n", rc);
			return;
		}
		local_opcode |= PA_HUE_EN;
	} else if (opcode & PA_HUE_EN)
		opcode &= ~PA_HUE_EN;

	if (hsic_cfg->flags & PA_HSIC_SAT_ENABLE) {
		reg = hsic_cfg->saturation & PA_SAT_MASK;
		REG_DMA_SETUP_OPS(dma_write_cfg,
			ctx->cap->sblk->hsic.base + PA_SAT_OFF,
			&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0);
		rc = dma_ops->setup_payload(&dma_write_cfg);
		if (rc) {
			DRM_ERROR("hsic saturation write failed ret %d\n", rc);
			return;
		}
		local_opcode |= PA_SAT_EN;
	} else if (opcode & PA_SAT_EN)
		opcode &= ~PA_SAT_EN;

	if (hsic_cfg->flags & PA_HSIC_VAL_ENABLE) {
		reg = hsic_cfg->value & PA_VAL_MASK;
		REG_DMA_SETUP_OPS(dma_write_cfg,
			ctx->cap->sblk->hsic.base + PA_VAL_OFF,
			&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0);
		rc = dma_ops->setup_payload(&dma_write_cfg);
		if (rc) {
			DRM_ERROR("hsic value write failed ret %d\n", rc);
			return;
		}
		local_opcode |= PA_VAL_EN;
	} else if (opcode & PA_VAL_EN)
		opcode &= ~PA_VAL_EN;

	if (hsic_cfg->flags & PA_HSIC_CONT_ENABLE) {
		reg = hsic_cfg->contrast & PA_CONT_MASK;
		REG_DMA_SETUP_OPS(dma_write_cfg,
			ctx->cap->sblk->hsic.base + PA_CONT_OFF,
			&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0);
		rc = dma_ops->setup_payload(&dma_write_cfg);
		if (rc) {
			DRM_ERROR("hsic contrast write failed ret %d\n", rc);
			return;
		}
		local_opcode |= PA_CONT_EN;
	} else if (opcode & PA_CONT_EN)
		opcode &= ~PA_CONT_EN;

	if (local_opcode)
		opcode |= (local_opcode | PA_EN);
	else {
		DRM_ERROR("Invalid hsic config\n");
		return;
	}

	REG_DMA_SETUP_KICKOFF(kick_off, hw_cfg->ctl, dspp_buf[HSIC][ctx->idx],
			REG_DMA_WRITE, DMA_CTL_QUEUE0, WRITE_IMMEDIATE);
	rc = dma_ops->kick_off(&kick_off);
	if (rc)
		DRM_ERROR("failed to kick off ret %d\n", rc);

	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->hsic.base, opcode);
}

void reg_dmav1_setup_dspp_sixzonev18(struct sde_hw_dspp *ctx, void *cfg)
{
	struct sde_hw_reg_dma_ops *dma_ops;
	struct sde_reg_dma_kickoff_cfg kick_off;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct sde_reg_dma_setup_ops_cfg dma_write_cfg;
	struct drm_msm_sixzone *sixzone;
	u32 reg = 0, hold = 0, local_hold = 0;
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

	REG_DMA_SETUP_OPS(dma_write_cfg, 0, NULL, 0, HW_BLK_SELECT, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write decode select failed ret %d\n", rc);
		return;
	}

	reg = BIT(26);
	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->sixzone.base,
		&reg, sizeof(reg), REG_SINGLE_WRITE, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("setting lut index failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_OPS(dma_write_cfg,
	    (ctx->cap->sblk->sixzone.base + SIXZONE_ADJ_CURVE_P1_OFF),
		&sixzone->curve[0].p1, (SIXZONE_LUT_SIZE * sizeof(u32) * 2),
	    REG_BLK_WRITE_MULTIPLE, 2, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write sixzone lut failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_OPS(dma_write_cfg,
		ctx->cap->sblk->sixzone.base + SIXZONE_THRESHOLDS_OFF,
		&sixzone->threshold, 3 * sizeof(u32),
		REG_BLK_WRITE_SINGLE, 0, 0);
	rc = dma_ops->setup_payload(&dma_write_cfg);
	if (rc) {
		DRM_ERROR("write sixzone threshold failed ret %d\n", rc);
		return;
	}

	REG_DMA_SETUP_KICKOFF(kick_off, hw_cfg->ctl,
		dspp_buf[SIX_ZONE][ctx->idx],
		REG_DMA_WRITE, DMA_CTL_QUEUE0, WRITE_IMMEDIATE);
	rc = dma_ops->kick_off(&kick_off);
	if (rc)
		DRM_ERROR("failed to kick off ret %d\n", rc);

	hold = SDE_REG_READ(&ctx->hw,
			(ctx->cap->sblk->hsic.base + PA_PWL_HOLD_OFF));
	local_hold = ((sixzone->sat_hold & REG_MASK(2)) << 12);
	local_hold |= ((sixzone->val_hold & REG_MASK(2)) << 14);
	hold &= ~REG_MASK_SHIFT(4, 12);
	hold |= local_hold;
	SDE_REG_WRITE(&ctx->hw,
			(ctx->cap->sblk->hsic.base + PA_PWL_HOLD_OFF), hold);

	if (sixzone->flags & SIXZONE_HUE_ENABLE)
		local_opcode |= PA_SIXZONE_HUE_EN;
	if (sixzone->flags & SIXZONE_SAT_ENABLE)
		local_opcode |= PA_SIXZONE_SAT_EN;
	if (sixzone->flags & SIXZONE_VAL_ENABLE)
		local_opcode |= PA_SIXZONE_VAL_EN;

	if (local_opcode)
		local_opcode |= PA_EN;

	opcode &= ~REG_MASK_SHIFT(3, 29);
	opcode |= local_opcode;
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->hsic.base, opcode);
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
