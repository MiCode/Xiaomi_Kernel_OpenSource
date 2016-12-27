/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

/* Reserve space of 128 words for LUT dma payload set-up */
#define REG_DMA_HEADERS_BUFFER_SZ (sizeof(u32) * 128)

#define VLUT_MEM_SIZE ((128 * sizeof(u32)) + REG_DMA_HEADERS_BUFFER_SZ)
#define VLUT_LEN (128 * sizeof(u32))
#define PA_OP_MODE_OFF 0x800
#define PA_LUTV_OPMODE_OFF 0x84c

#define GAMUT_LUT_MEM_SIZE ((sizeof(struct drm_msm_3d_gamut)) + \
		REG_DMA_HEADERS_BUFFER_SZ)

#define REG_MASK(n) ((BIT(n)) - 1)

static struct sde_reg_dma_buffer *dspp_buf[REG_DMA_FEATURES_MAX][DSPP_MAX];

static u32 feature_map[SDE_DSPP_MAX] = {
	[SDE_DSPP_VLUT] = VLUT,
	[SDE_DSPP_GAMUT] = GAMUT,
	[SDE_DSPP_IGC] = REG_DMA_FEATURES_MAX,
	[SDE_DSPP_PCC] = REG_DMA_FEATURES_MAX,
	[SDE_DSPP_GC] = REG_DMA_FEATURES_MAX,
	[SDE_DSPP_HSIC] = REG_DMA_FEATURES_MAX,
	[SDE_DSPP_MEMCOLOR] = REG_DMA_FEATURES_MAX,
	[SDE_DSPP_SIXZONE] = REG_DMA_FEATURES_MAX,
	[SDE_DSPP_DITHER] = REG_DMA_FEATURES_MAX,
	[SDE_DSPP_HIST] = REG_DMA_FEATURES_MAX,
	[SDE_DSPP_AD] = REG_DMA_FEATURES_MAX,
};

static u32 feature_reg_dma_sz[SDE_DSPP_MAX] = {
	[SDE_DSPP_VLUT] = VLUT_MEM_SIZE,
	[SDE_DSPP_GAMUT] = GAMUT_LUT_MEM_SIZE,
};

static u32 dspp_mapping[DSPP_MAX] = {
	[DSPP_0] = DSPP0,
	[DSPP_1] = DSPP1,
	[DSPP_2] = DSPP2,
	[DSPP_3] = DSPP3,
};

#define REG_DMA_OP_SETUP(cfg, block, reg_dma_feature, op, buf) \
	do { \
		(cfg).blk = block; \
		(cfg).feature = reg_dma_feature; \
		(cfg).ops = op; \
		(cfg).dma_buf = buf; \
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

int reg_dmav1_init_dspp_op_v4(int feature, enum sde_dspp idx)
{
	int rc = -ENOTSUPP;
	struct sde_hw_reg_dma_ops *dma_ops;
	bool is_supported = false;

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

	rc = dma_ops->check_support(feature_map[feature], dspp_mapping[idx],
			&is_supported);
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
		if (op_mode & (~(BIT(20))))
			op_mode = 0;
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
