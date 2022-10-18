// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/iopoll.h>

#include "sde_hw_mdss.h"
#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_pingpong.h"
#include "sde_dbg.h"
#include "sde_kms.h"

#define PP_TEAR_CHECK_EN                0x000
#define PP_SYNC_CONFIG_VSYNC            0x004
#define PP_SYNC_CONFIG_HEIGHT           0x008
#define PP_SYNC_WRCOUNT                 0x00C
#define PP_VSYNC_INIT_VAL               0x010
#define PP_INT_COUNT_VAL                0x014
#define PP_SYNC_THRESH                  0x018
#define PP_START_POS                    0x01C
#define PP_RD_PTR_IRQ                   0x020
#define PP_WR_PTR_IRQ                   0x024
#define PP_OUT_LINE_COUNT               0x028
#define PP_LINE_COUNT                   0x02C
#define PP_AUTOREFRESH_CONFIG           0x030

#define PP_FBC_MODE                     0x034
#define PP_FBC_BUDGET_CTL               0x038
#define PP_FBC_LOSSY_MODE               0x03C
#define PP_DSC_MODE                     0x0a0
#define PP_DCE_DATA_IN_SWAP             0x0ac
#define PP_DCE_DATA_OUT_SWAP            0x0c8

#define DITHER_VER_MAJOR_1 1
/* supports LUMA Dither */
#define DITHER_VER_MAJOR_2 2

#define MERGE_3D_MODE 0x004
#define MERGE_3D_MUX  0x000

static struct sde_merge_3d_cfg *_merge_3d_offset(enum sde_merge_3d idx,
		struct sde_mdss_cfg *m,
		void __iomem *addr,
		struct sde_hw_blk_reg_map *b)
{
	int i;

	for (i = 0; i < m->merge_3d_count; i++) {
		if (idx == m->merge_3d[i].id) {
			b->base_off = addr;
			b->blk_off = m->merge_3d[i].base;
			b->length = m->merge_3d[i].len;
			b->hwversion = m->hwversion;
			b->log_mask = SDE_DBG_MASK_PINGPONG;
			return &m->merge_3d[i];
		}
	}

	return ERR_PTR(-EINVAL);
}

static void _sde_hw_merge_3d_setup_blend_mode(struct sde_hw_merge_3d *ctx,
			enum sde_3d_blend_mode cfg)
{
	struct sde_hw_blk_reg_map *c;
	u32 mode = 0;

	if (!ctx)
		return;

	c = &ctx->hw;
	if (cfg) {
		mode = BIT(0);
		mode |= (cfg - 0x1) << 1;
	}

	SDE_REG_WRITE(c, MERGE_3D_MODE, mode);
}

static void sde_hw_merge_3d_reset_blend_mode(struct sde_hw_merge_3d *ctx)
{
	struct sde_hw_blk_reg_map *c;

	if (!ctx)
		return;

	c = &ctx->hw;
	SDE_REG_WRITE(c, MERGE_3D_MODE, 0x0);
	SDE_REG_WRITE(c, MERGE_3D_MUX, 0x0);
}

static void _setup_merge_3d_ops(struct sde_hw_merge_3d_ops *ops,
	const struct sde_merge_3d_cfg *hw_cap)
{
	ops->setup_blend_mode = _sde_hw_merge_3d_setup_blend_mode;
	ops->reset_blend_mode = sde_hw_merge_3d_reset_blend_mode;
}

static struct sde_hw_merge_3d *_sde_pp_merge_3d_init(enum sde_merge_3d idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m)
{
	struct sde_hw_merge_3d *c;
	struct sde_merge_3d_cfg *cfg;
	static u32 merge3d_init_mask;

	if (idx < MERGE_3D_0)
		return NULL;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	cfg = _merge_3d_offset(idx, m, addr, &c->hw);
	if (IS_ERR_OR_NULL(cfg)) {
		pr_err("invalid merge_3d cfg%d\n", idx);
		kfree(c);
		return ERR_PTR(-EINVAL);
	}

	c->idx = idx;
	c->caps = cfg;
	_setup_merge_3d_ops(&c->ops, c->caps);

	if (!(merge3d_init_mask & BIT(idx))) {
		sde_dbg_reg_register_dump_range(SDE_DBG_NAME, cfg->name,
				c->hw.blk_off, c->hw.blk_off + c->hw.length,
				c->hw.xin_id);
		merge3d_init_mask |= BIT(idx);
	}

	return c;
}

static struct sde_pingpong_cfg *_pingpong_offset(enum sde_pingpong pp,
		struct sde_mdss_cfg *m,
		void __iomem *addr,
		struct sde_hw_blk_reg_map *b)
{
	int i;

	for (i = 0; i < m->pingpong_count; i++) {
		if (pp == m->pingpong[i].id) {
			b->base_off = addr;
			b->blk_off = m->pingpong[i].base;
			b->length = m->pingpong[i].len;
			b->hwversion = m->hwversion;
			b->log_mask = SDE_DBG_MASK_PINGPONG;
			return &m->pingpong[i];
		}
	}

	return ERR_PTR(-EINVAL);
}

static int sde_hw_pp_setup_te_config(struct sde_hw_pingpong *pp,
		struct sde_hw_tear_check *te)
{
	struct sde_hw_blk_reg_map *c;
	int cfg;

	if (!pp || !te)
		return -EINVAL;
	c = &pp->hw;

	cfg = BIT(19); /*VSYNC_COUNTER_EN */
	if (te->hw_vsync_mode)
		cfg |= BIT(20);

	cfg |= te->vsync_count;

	SDE_REG_WRITE(c, PP_SYNC_CONFIG_VSYNC, cfg);
	SDE_REG_WRITE(c, PP_SYNC_CONFIG_HEIGHT, te->sync_cfg_height);
	SDE_REG_WRITE(c, PP_VSYNC_INIT_VAL, te->vsync_init_val);
	SDE_REG_WRITE(c, PP_RD_PTR_IRQ, te->rd_ptr_irq);
	SDE_REG_WRITE(c, PP_WR_PTR_IRQ, te->wr_ptr_irq);
	SDE_REG_WRITE(c, PP_START_POS, te->start_pos);
	SDE_REG_WRITE(c, PP_SYNC_THRESH,
			((te->sync_threshold_continue << 16) |
			 te->sync_threshold_start));
	SDE_REG_WRITE(c, PP_SYNC_WRCOUNT,
			(te->start_pos + te->sync_threshold_start + 1));

	return 0;
}

static void sde_hw_pp_update_te(struct sde_hw_pingpong *pp,
		struct sde_hw_tear_check *te)
{
	struct sde_hw_blk_reg_map *c;
	int cfg;

	if (!pp || !te)
		return;
	c = &pp->hw;

	cfg = SDE_REG_READ(c, PP_SYNC_THRESH);
	cfg &= ~0xFFFF;
	cfg |= te->sync_threshold_start;
	SDE_REG_WRITE(c, PP_SYNC_THRESH, cfg);
}

static int sde_hw_pp_setup_autorefresh_config(struct sde_hw_pingpong *pp,
		struct sde_hw_autorefresh *cfg)
{
	struct sde_hw_blk_reg_map *c;
	u32 refresh_cfg;

	if (!pp || !cfg)
		return -EINVAL;
	c = &pp->hw;

	if (cfg->enable)
		refresh_cfg = BIT(31) | cfg->frame_count;
	else
		refresh_cfg = 0;

	SDE_REG_WRITE(c, PP_AUTOREFRESH_CONFIG, refresh_cfg);
	SDE_EVT32(pp->idx - PINGPONG_0, refresh_cfg);

	return 0;
}

static int sde_hw_pp_get_autorefresh_config(struct sde_hw_pingpong *pp,
		struct sde_hw_autorefresh *cfg)
{
	struct sde_hw_blk_reg_map *c;
	u32 val;

	if (!pp || !cfg)
		return -EINVAL;

	c = &pp->hw;
	val = SDE_REG_READ(c, PP_AUTOREFRESH_CONFIG);
	cfg->enable = (val & BIT(31)) >> 31;
	cfg->frame_count = val & 0xffff;

	return 0;
}

static int sde_hw_pp_poll_timeout_wr_ptr(struct sde_hw_pingpong *pp,
		u32 timeout_us)
{
	struct sde_hw_blk_reg_map *c;
	u32 val;
	int rc;

	if (!pp)
		return -EINVAL;

	c = &pp->hw;
	rc = readl_poll_timeout(c->base_off + c->blk_off + PP_LINE_COUNT,
			val, (val & 0xffff) >= 1, 10, timeout_us);

	return rc;
}

static void sde_hw_pp_dsc_enable(struct sde_hw_pingpong *pp)
{
	struct sde_hw_blk_reg_map *c;

	if (!pp)
		return;
	c = &pp->hw;

	SDE_REG_WRITE(c, PP_DSC_MODE, 1);
}

static void sde_hw_pp_dsc_disable(struct sde_hw_pingpong *pp)
{
	struct sde_hw_blk_reg_map *c;
	u32 data;

	if (!pp)
		return;
	c = &pp->hw;

	data = SDE_REG_READ(c, PP_DCE_DATA_OUT_SWAP);
	data &= ~BIT(18); /* disable endian flip */
	SDE_REG_WRITE(c, PP_DCE_DATA_OUT_SWAP, data);

	SDE_REG_WRITE(c, PP_DSC_MODE, 0);
}

static int sde_hw_pp_setup_dsc(struct sde_hw_pingpong *pp)
{
	struct sde_hw_blk_reg_map *c;
	int data;

	if (!pp)
		return -EINVAL;
	c = &pp->hw;

	data = SDE_REG_READ(c, PP_DCE_DATA_OUT_SWAP);
	data |= BIT(18); /* endian flip */
	SDE_REG_WRITE(c, PP_DCE_DATA_OUT_SWAP, data);
	return 0;
}

static int sde_hw_pp_setup_dither(struct sde_hw_pingpong *pp,
					void *cfg, size_t len)
{
	struct sde_hw_blk_reg_map *c;
	struct drm_msm_dither *dither = (struct drm_msm_dither *)cfg;
	u32 base = 0, offset = 0, data = 0, i = 0;

	if (!pp)
		return -EINVAL;

	c = &pp->hw;
	base = pp->caps->sblk->dither.base;
	if (!dither) {
		/* dither property disable case */
		SDE_REG_WRITE(c, base, 0);
		return 0;
	}

	if (len != sizeof(struct drm_msm_dither)) {
		DRM_ERROR("input len %zu, expected len %zu\n", len,
			sizeof(struct drm_msm_dither));
		return -EINVAL;
	}

	if (dither->c0_bitdepth >= DITHER_DEPTH_MAP_INDEX ||
		dither->c1_bitdepth >= DITHER_DEPTH_MAP_INDEX ||
		dither->c2_bitdepth >= DITHER_DEPTH_MAP_INDEX ||
		dither->c3_bitdepth >= DITHER_DEPTH_MAP_INDEX)
		return -EINVAL;

	offset += 4;
	data = dither_depth_map[dither->c0_bitdepth] & REG_MASK(2);
	data |= (dither_depth_map[dither->c1_bitdepth] & REG_MASK(2)) << 2;
	data |= (dither_depth_map[dither->c2_bitdepth] & REG_MASK(2)) << 4;
	data |= (dither_depth_map[dither->c3_bitdepth] & REG_MASK(2)) << 6;
	data |= (dither->temporal_en) ? (1 << 8) : 0;
	SDE_REG_WRITE(c, base + offset, data);

	for (i = 0; i < DITHER_MATRIX_SZ - 3; i += 4) {
		offset += 4;
		data = (dither->matrix[i] & REG_MASK(4)) |
			((dither->matrix[i + 1] & REG_MASK(4)) << 4) |
			((dither->matrix[i + 2] & REG_MASK(4)) << 8) |
			((dither->matrix[i + 3] & REG_MASK(4)) << 12);
		SDE_REG_WRITE(c, base + offset, data);
	}

	if (test_bit(SDE_PINGPONG_DITHER_LUMA, &pp->caps->features)
				&& (dither->flags & DITHER_LUMA_MODE))
		SDE_REG_WRITE(c, base, 0x11);
	else
		SDE_REG_WRITE(c, base, 1);

	return 0;
}

static int sde_hw_pp_enable_te(struct sde_hw_pingpong *pp, bool enable)
{
	struct sde_hw_blk_reg_map *c;

	if (!pp)
		return -EINVAL;
	c = &pp->hw;

	SDE_REG_WRITE(c, PP_TEAR_CHECK_EN, enable);
	return 0;
}

static int sde_hw_pp_connect_external_te(struct sde_hw_pingpong *pp,
		bool enable_external_te)
{
	struct sde_hw_blk_reg_map *c = &pp->hw;
	u32 cfg;
	int orig;

	if (!pp)
		return -EINVAL;

	c = &pp->hw;
	cfg = SDE_REG_READ(c, PP_SYNC_CONFIG_VSYNC);
	orig = (bool)(cfg & BIT(20));
	if (enable_external_te)
		cfg |= BIT(20);
	else
		cfg &= ~BIT(20);
	SDE_REG_WRITE(c, PP_SYNC_CONFIG_VSYNC, cfg);
	SDE_EVT32(pp->idx - PINGPONG_0, cfg);

	return orig;
}

static int sde_hw_pp_get_vsync_info(struct sde_hw_pingpong *pp,
		struct sde_hw_pp_vsync_info *info)
{
	struct sde_hw_blk_reg_map *c;
	u32 val;

	if (!pp || !info)
		return -EINVAL;
	c = &pp->hw;

	val = SDE_REG_READ(c, PP_VSYNC_INIT_VAL);
	info->rd_ptr_init_val = val & 0xffff;

	val = SDE_REG_READ(c, PP_INT_COUNT_VAL);
	info->rd_ptr_frame_count = (val & 0xffff0000) >> 16;
	info->rd_ptr_line_count = val & 0xffff;

	val = SDE_REG_READ(c, PP_LINE_COUNT);
	info->wr_ptr_line_count = val & 0xffff;

	return 0;
}

static u32 sde_hw_pp_get_line_count(struct sde_hw_pingpong *pp)
{
	struct sde_hw_blk_reg_map *c = &pp->hw;
	u32 height, init;
	u32 line = 0xFFFF;

	if (!pp)
		return 0;
	c = &pp->hw;

	init = SDE_REG_READ(c, PP_VSYNC_INIT_VAL) & 0xFFFF;
	height = SDE_REG_READ(c, PP_SYNC_CONFIG_HEIGHT) & 0xFFFF;

	if (height < init)
		goto line_count_exit;

	line = SDE_REG_READ(c, PP_INT_COUNT_VAL) & 0xFFFF;

	if (line < init)
		line += (0xFFFF - init);
	else
		line -= init;

line_count_exit:
	return line;
}

static void sde_hw_pp_setup_3d_merge_mode(struct sde_hw_pingpong *pp,
					enum sde_3d_blend_mode cfg)
{
	if (pp->merge_3d && pp->merge_3d->ops.setup_blend_mode)
		pp->merge_3d->ops.setup_blend_mode(pp->merge_3d, cfg);
}

static void sde_hw_pp_reset_3d_merge_mode(struct sde_hw_pingpong *pp)
{
	if (pp->merge_3d && pp->merge_3d->ops.reset_blend_mode)
		pp->merge_3d->ops.reset_blend_mode(pp->merge_3d);
}

static unsigned long sde_hw_pp_get_caps(struct sde_hw_pingpong *pp)
{
	return !pp ? 0 : pp->caps->features;
}

static void _setup_pingpong_ops(struct sde_hw_pingpong_ops *ops,
	const struct sde_pingpong_cfg *hw_cap)
{
	u32 version = 0;

	ops->get_hw_caps = sde_hw_pp_get_caps;
	if (hw_cap->features & BIT(SDE_PINGPONG_TE)) {
		ops->setup_tearcheck = sde_hw_pp_setup_te_config;
		ops->enable_tearcheck = sde_hw_pp_enable_te;
		ops->update_tearcheck = sde_hw_pp_update_te;
		ops->connect_external_te = sde_hw_pp_connect_external_te;
		ops->get_vsync_info = sde_hw_pp_get_vsync_info;
		ops->setup_autorefresh = sde_hw_pp_setup_autorefresh_config;
		ops->get_autorefresh = sde_hw_pp_get_autorefresh_config;
		ops->poll_timeout_wr_ptr = sde_hw_pp_poll_timeout_wr_ptr;
		ops->get_line_count = sde_hw_pp_get_line_count;
	}
	if (hw_cap->features & BIT(SDE_PINGPONG_DSC)) {
		ops->setup_dsc = sde_hw_pp_setup_dsc;
		ops->enable_dsc = sde_hw_pp_dsc_enable;
		ops->disable_dsc = sde_hw_pp_dsc_disable;
	}

	version = SDE_COLOR_PROCESS_MAJOR(hw_cap->sblk->dither.version);
	switch (version) {
	case DITHER_VER_MAJOR_1:
	case DITHER_VER_MAJOR_2:
		ops->setup_dither = sde_hw_pp_setup_dither;
		break;
	default:
		ops->setup_dither = NULL;
		break;
	}
	if (test_bit(SDE_PINGPONG_MERGE_3D, &hw_cap->features)) {
		ops->setup_3d_mode = sde_hw_pp_setup_3d_merge_mode;
		ops->reset_3d_mode = sde_hw_pp_reset_3d_merge_mode;
	}
};

static struct sde_hw_blk_ops sde_hw_ops = {
	.start = NULL,
	.stop = NULL,
};

struct sde_hw_pingpong *sde_hw_pingpong_init(enum sde_pingpong idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m)
{
	struct sde_hw_pingpong *c;
	struct sde_pingpong_cfg *cfg;
	int rc;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	cfg = _pingpong_offset(idx, m, addr, &c->hw);
	if (IS_ERR_OR_NULL(cfg)) {
		kfree(c);
		return ERR_PTR(-EINVAL);
	}

	c->idx = idx;
	c->caps = cfg;
	c->dcwb_idx = cfg->dcwb_id;
	if (test_bit(SDE_PINGPONG_MERGE_3D, &cfg->features)) {
		c->merge_3d = _sde_pp_merge_3d_init(cfg->merge_3d_id, addr, m);
			if (IS_ERR(c->merge_3d)) {
				SDE_ERROR("invalid merge_3d block %d\n", idx);
				return ERR_PTR(-ENOMEM);
			}
	}

	_setup_pingpong_ops(&c->ops, c->caps);

	rc = sde_hw_blk_init(&c->base, SDE_HW_BLK_PINGPONG, idx, &sde_hw_ops);
	if (rc) {
		SDE_ERROR("failed to init hw blk %d\n", rc);
		goto blk_init_error;
	}

	sde_dbg_reg_register_dump_range(SDE_DBG_NAME, cfg->name, c->hw.blk_off,
			c->hw.blk_off + c->hw.length, c->hw.xin_id);

	if (cfg->sblk->dither.base && cfg->sblk->dither.len) {
		sde_dbg_reg_register_dump_range(SDE_DBG_NAME,
			cfg->sblk->dither.name,
			c->hw.blk_off + cfg->sblk->dither.base,
			c->hw.blk_off + cfg->sblk->dither.base +
			cfg->sblk->dither.len,
			c->hw.xin_id);
	}

	return c;

blk_init_error:
	kfree(c);

	return ERR_PTR(rc);
}

void sde_hw_pingpong_destroy(struct sde_hw_pingpong *pp)
{
	if (pp) {
		sde_hw_blk_destroy(&pp->base);
		kfree(pp->merge_3d);
		kfree(pp);
	}
}
