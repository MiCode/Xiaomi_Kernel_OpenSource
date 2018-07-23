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

#include "sde_hw_mdss.h"
#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_dsc.h"
#include "sde_hw_pingpong.h"
#include "sde_dbg.h"
#include "sde_kms.h"

#define DSC_COMMON_MODE	                0x000
#define DSC_ENC                         0X004
#define DSC_PICTURE                     0x008
#define DSC_SLICE                       0x00C
#define DSC_CHUNK_SIZE                  0x010
#define DSC_DELAY                       0x014
#define DSC_SCALE_INITIAL               0x018
#define DSC_SCALE_DEC_INTERVAL          0x01C
#define DSC_SCALE_INC_INTERVAL          0x020
#define DSC_FIRST_LINE_BPG_OFFSET       0x024
#define DSC_BPG_OFFSET                  0x028
#define DSC_DSC_OFFSET                  0x02C
#define DSC_FLATNESS                    0x030
#define DSC_RC_MODEL_SIZE               0x034
#define DSC_RC                          0x038
#define DSC_RC_BUF_THRESH               0x03C
#define DSC_RANGE_MIN_QP                0x074
#define DSC_RANGE_MAX_QP                0x0B0
#define DSC_RANGE_BPG_OFFSET            0x0EC

#define DSC_CTL(m)     \
	(((m == DSC_NONE) || (m >= DSC_MAX)) ? 0 : (0x1800 - 0x3FC * (m - 1)))

static void sde_hw_dsc_disable(struct sde_hw_dsc *dsc)
{
	struct sde_hw_blk_reg_map *dsc_c = &dsc->hw;

	SDE_REG_WRITE(dsc_c, DSC_COMMON_MODE, 0);
}

static void sde_hw_dsc_config(struct sde_hw_dsc *hw_dsc,
		struct msm_display_dsc_info *dsc, u32 mode,
		bool ich_reset_override)
{
	u32 data;
	int bpp, lsb;
	u32 initial_lines = dsc->initial_lines;
	bool is_cmd_mode = !(mode & BIT(2));
	struct sde_hw_blk_reg_map *dsc_c = &hw_dsc->hw;

	SDE_REG_WRITE(dsc_c, DSC_COMMON_MODE, mode);

	data = 0;
	if (ich_reset_override)
		data = 3 << 28;

	if (is_cmd_mode)
		initial_lines += 1;

	data |= (initial_lines << 20);
	data |= (dsc->slice_last_group_size << 18);
	/* bpp is 6.4 format, 4 LSBs bits are for fractional part */
	lsb = dsc->bpp % 4;
	bpp = dsc->bpp / 4;
	bpp *= 4;	/* either 8 or 12 */
	bpp <<= 4;
	bpp |= lsb;
	data |= (bpp << 8);
	data |= (dsc->block_pred_enable << 7);
	data |= (dsc->line_buf_depth << 3);
	data |= (dsc->enable_422 << 2);
	data |= (dsc->convert_rgb << 1);
	data |= dsc->input_10_bits;

	SDE_REG_WRITE(dsc_c, DSC_ENC, data);

	data = dsc->pic_width << 16;
	data |= dsc->pic_height;
	SDE_REG_WRITE(dsc_c, DSC_PICTURE, data);

	data = dsc->slice_width << 16;
	data |= dsc->slice_height;
	SDE_REG_WRITE(dsc_c, DSC_SLICE, data);

	data = dsc->chunk_size << 16;
	SDE_REG_WRITE(dsc_c, DSC_CHUNK_SIZE, data);

	data = dsc->initial_dec_delay << 16;
	data |= dsc->initial_xmit_delay;
	SDE_REG_WRITE(dsc_c, DSC_DELAY, data);

	data = dsc->initial_scale_value;
	SDE_REG_WRITE(dsc_c, DSC_SCALE_INITIAL, data);

	data = dsc->scale_decrement_interval;
	SDE_REG_WRITE(dsc_c, DSC_SCALE_DEC_INTERVAL, data);

	data = dsc->scale_increment_interval;
	SDE_REG_WRITE(dsc_c, DSC_SCALE_INC_INTERVAL, data);

	data = dsc->first_line_bpg_offset;
	SDE_REG_WRITE(dsc_c, DSC_FIRST_LINE_BPG_OFFSET, data);

	data = dsc->nfl_bpg_offset << 16;
	data |= dsc->slice_bpg_offset;
	SDE_REG_WRITE(dsc_c, DSC_BPG_OFFSET, data);

	data = dsc->initial_offset << 16;
	data |= dsc->final_offset;
	SDE_REG_WRITE(dsc_c, DSC_DSC_OFFSET, data);

	data = dsc->det_thresh_flatness << 10;
	data |= dsc->max_qp_flatness << 5;
	data |= dsc->min_qp_flatness;
	SDE_REG_WRITE(dsc_c, DSC_FLATNESS, data);

	data = dsc->rc_model_size;
	SDE_REG_WRITE(dsc_c, DSC_RC_MODEL_SIZE, data);

	data = dsc->tgt_offset_lo << 18;
	data |= dsc->tgt_offset_hi << 14;
	data |= dsc->quant_incr_limit1 << 9;
	data |= dsc->quant_incr_limit0 << 4;
	data |= dsc->edge_factor;
	SDE_REG_WRITE(dsc_c, DSC_RC, data);
}

static void sde_hw_dsc_config_thresh(struct sde_hw_dsc *hw_dsc,
		struct msm_display_dsc_info *dsc)
{
	u32 *lp;
	char *cp;
	int i;

	struct sde_hw_blk_reg_map *dsc_c = &hw_dsc->hw;
	u32 off = 0x0;

	lp = dsc->buf_thresh;
	off = DSC_RC_BUF_THRESH;
	for (i = 0; i < 14; i++) {
		SDE_REG_WRITE(dsc_c, off, *lp++);
		off += 4;
	}

	cp = dsc->range_min_qp;
	off = DSC_RANGE_MIN_QP;
	for (i = 0; i < 15; i++) {
		SDE_REG_WRITE(dsc_c, off, *cp++);
		off += 4;
	}

	cp = dsc->range_max_qp;
	off = DSC_RANGE_MAX_QP;
	for (i = 0; i < 15; i++) {
		SDE_REG_WRITE(dsc_c, off, *cp++);
		off += 4;
	}

	cp = dsc->range_bpg_offset;
	off = DSC_RANGE_BPG_OFFSET;
	for (i = 0; i < 15; i++) {
		SDE_REG_WRITE(dsc_c, off, *cp++);
		off += 4;
	}
}

static void sde_hw_dsc_bind_pingpong_blk(
		struct sde_hw_dsc *hw_dsc,
		bool enable,
		const enum sde_pingpong pp)
{
	struct sde_hw_blk_reg_map *c;
	int mux_cfg = 0xF;
	u32 dsc_ctl_offset;

	if (!hw_dsc)
		return;

	c = &hw_dsc->hw;
	dsc_ctl_offset = DSC_CTL(hw_dsc->idx);

	if (enable)
		mux_cfg = (pp - PINGPONG_0) & 0x7;

	if (dsc_ctl_offset)
		SDE_REG_WRITE(c, dsc_ctl_offset, mux_cfg);
}


static struct sde_dsc_cfg *_dsc_offset(enum sde_dsc dsc,
		struct sde_mdss_cfg *m,
		void __iomem *addr,
		struct sde_hw_blk_reg_map *b)
{
	int i;

	for (i = 0; i < m->dsc_count; i++) {
		if (dsc == m->dsc[i].id) {
			b->base_off = addr;
			b->blk_off = m->dsc[i].base;
			b->length = m->dsc[i].len;
			b->hwversion = m->hwversion;
			b->log_mask = SDE_DBG_MASK_DSC;
			return &m->dsc[i];
		}
	}

	return NULL;
}

static void _setup_dsc_ops(struct sde_hw_dsc_ops *ops,
		unsigned long features)
{
	ops->dsc_disable = sde_hw_dsc_disable;
	ops->dsc_config = sde_hw_dsc_config;
	ops->dsc_config_thresh = sde_hw_dsc_config_thresh;
	if (test_bit(SDE_DSC_OUTPUT_CTRL, &features))
		ops->bind_pingpong_blk = sde_hw_dsc_bind_pingpong_blk;
};

static struct sde_hw_blk_ops sde_hw_ops = {
	.start = NULL,
	.stop = NULL,
};

struct sde_hw_dsc *sde_hw_dsc_init(enum sde_dsc idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m)
{
	struct sde_hw_dsc *c;
	struct sde_dsc_cfg *cfg;
	int rc;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	cfg = _dsc_offset(idx, m, addr, &c->hw);
	if (IS_ERR_OR_NULL(cfg)) {
		kfree(c);
		return ERR_PTR(-EINVAL);
	}

	c->idx = idx;
	c->caps = cfg;
	_setup_dsc_ops(&c->ops, c->caps->features);

	rc = sde_hw_blk_init(&c->base, SDE_HW_BLK_DSC, idx, &sde_hw_ops);
	if (rc) {
		SDE_ERROR("failed to init hw blk %d\n", rc);
		goto blk_init_error;
	}

	sde_dbg_reg_register_dump_range(SDE_DBG_NAME, cfg->name, c->hw.blk_off,
		c->hw.blk_off + c->hw.length, c->hw.xin_id);

	return c;

blk_init_error:
	kzfree(c);

	return ERR_PTR(rc);
}

void sde_hw_dsc_destroy(struct sde_hw_dsc *dsc)
{
	if (dsc)
		sde_hw_blk_destroy(&dsc->base);
	kfree(dsc);
}
