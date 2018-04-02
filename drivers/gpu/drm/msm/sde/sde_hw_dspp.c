/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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
#include "sde_hw_mdss.h"
#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_dspp.h"
#include "sde_hw_color_processing.h"
#include "sde_dbg.h"

static struct sde_dspp_cfg *_dspp_offset(enum sde_dspp dspp,
		struct sde_mdss_cfg *m,
		void __iomem *addr,
		struct sde_hw_blk_reg_map *b)
{
	int i;

	for (i = 0; i < m->dspp_count; i++) {
		if (dspp == m->dspp[i].id) {
			b->base_off = addr;
			b->blk_off = m->dspp[i].base;
			b->length = m->dspp[i].len;
			b->hwversion = m->hwversion;
			b->log_mask = SDE_DBG_MASK_DSPP;
			return &m->dspp[i];
		}
	}

	return ERR_PTR(-EINVAL);
}

void sde_dspp_setup_histogram(struct sde_hw_dspp *ctx, void *cfg)
{
}

void sde_dspp_read_histogram(struct sde_hw_dspp *ctx, void *cfg)
{
}

void sde_dspp_update_igc(struct sde_hw_dspp *ctx, void *cfg)
{
}

void sde_dspp_setup_sharpening(struct sde_hw_dspp *ctx, void *cfg)
{
}

void sde_dspp_setup_danger_safe(struct sde_hw_dspp *ctx, void *cfg)
{
}

void sde_dspp_setup_dither(struct sde_hw_dspp *ctx, void *cfg)
{
}

static void _setup_dspp_ops(struct sde_hw_dspp *c, unsigned long features)
{
	int i = 0;

	for (i = 0; i < SDE_DSPP_MAX; i++) {
		if (!test_bit(i, &features))
			continue;
		switch (i) {
		case SDE_DSPP_PCC:
			if (c->cap->sblk->pcc.version ==
				(SDE_COLOR_PROCESS_VER(0x1, 0x7)))
				c->ops.setup_pcc = sde_setup_dspp_pcc_v1_7;
			break;
		case SDE_DSPP_HSIC:
			if (c->cap->sblk->hsic.version ==
				(SDE_COLOR_PROCESS_VER(0x1, 0x7)))
				c->ops.setup_hue = sde_setup_dspp_pa_hue_v1_7;
			break;
		case SDE_DSPP_VLUT:
			if (c->cap->sblk->vlut.version ==
				(SDE_COLOR_PROCESS_VER(0x1, 0x7))) {
				c->ops.setup_vlut = sde_setup_dspp_pa_vlut_v1_7;
			}
		default:
			break;
		}
	}
}

struct sde_hw_dspp *sde_hw_dspp_init(enum sde_dspp idx,
			void __iomem *addr,
			struct sde_mdss_cfg *m)
{
	struct sde_hw_dspp *c;
	struct sde_dspp_cfg *cfg;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	cfg = _dspp_offset(idx, m, addr, &c->hw);
	if (IS_ERR_OR_NULL(cfg)) {
		kfree(c);
		return ERR_PTR(-EINVAL);
	}

	/* Assign ops */
	c->idx = idx;
	c->cap = cfg;
	_setup_dspp_ops(c, c->cap->features);

	sde_dbg_reg_register_dump_range(SDE_DBG_NAME, cfg->name, c->hw.blk_off,
			c->hw.blk_off + c->hw.length, c->hw.xin_id);

	return c;
}

void sde_hw_dspp_destroy(struct sde_hw_dspp *dspp)
{
	kfree(dspp);
}
