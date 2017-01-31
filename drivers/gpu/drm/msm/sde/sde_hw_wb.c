/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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
#include "sde_hw_wb.h"

static struct sde_wb_cfg *_wb_offset(enum sde_wb wb,
		struct sde_mdss_cfg *m,
		void __iomem *addr,
		struct sde_hw_blk_reg_map *b)
{
	int i;

	for (i = 0; i < m->wb_count; i++) {
		if (wb == m->wb[i].id) {
			b->base_off = addr;
			b->blk_off = m->wb[i].base;
			b->hwversion = m->hwversion;
			return &m->wb[i];
		}
	}
	return ERR_PTR(-EINVAL);
}

static void sde_hw_wb_setup_csc_8bit(struct sde_hw_wb *ctx,
		struct sde_csc_cfg *data)
{
}

static void sde_hw_wb_setup_outaddress(struct sde_hw_wb *ctx,
		struct sde_hw_wb_cfg *data)
{
}

static void sde_hw_wb_setup_format(struct sde_hw_wb *ctx,
		struct sde_hw_wb_cfg *data)
{
}

static void sde_hw_wb_setup_rotator(struct sde_hw_wb *ctx,
		struct sde_hw_wb_cfg *data)
{
}

static void sde_hw_setup_dither(struct sde_hw_wb *ctx,
		struct sde_hw_wb_cfg *data)
{
}

static void sde_hw_wb_setup_cdwn(struct sde_hw_wb *ctx,
		struct sde_hw_wb_cfg *data)
{
}
static void sde_hw_wb_traffic_shaper(struct sde_hw_wb *ctx,
		struct sde_hw_wb_cfg *data)
{
}

static void _setup_wb_ops(struct sde_hw_wb_ops *ops,
	unsigned long features)
{
	if (test_bit(SDE_WB_CSC, &features))
		ops->setup_csc_data = sde_hw_wb_setup_csc_8bit;

	ops->setup_outaddress = sde_hw_wb_setup_outaddress;
	ops->setup_outformat = sde_hw_wb_setup_format;

	if (test_bit(SDE_WB_BLOCK_MODE, &features))
		ops->setup_rotator = sde_hw_wb_setup_rotator;

	if (test_bit(SDE_WB_DITHER, &features))
		ops->setup_dither = sde_hw_setup_dither;

	if (test_bit(SDE_WB_CHROMA_DOWN, &features))
		ops->setup_cdwn = sde_hw_wb_setup_cdwn;

	if (test_bit(SDE_WB_TRAFFIC_SHAPER, &features))
		ops->setup_trafficshaper = sde_hw_wb_traffic_shaper;
}

struct sde_hw_wb *sde_hw_wb_init(enum sde_wb idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m)
{
	struct sde_hw_wb *c;
	struct sde_wb_cfg *cfg;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	cfg = _wb_offset(idx, m, addr, &c->hw);
	if (!cfg) {
		kfree(c);
		return ERR_PTR(-EINVAL);
	}

	/* Assign ops */
	c->idx = idx;
	c->caps = cfg;
	_setup_wb_ops(&c->ops, c->caps->features);

	/*
	 * Perform any default initialization for the chroma down module
	 */

	return c;
}

