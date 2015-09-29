/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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
#include "sde_hw_dspp.h"

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
			b->hwversion = m->hwversion;
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

void sde_dspp_setup_pa(struct sde_hw_dspp *dspp, void *cfg)
{
}

void sde_dspp_setup_pcc(struct sde_hw_dspp *ctx, void *cfg)
{
}

void sde_dspp_setup_sharpening(struct sde_hw_dspp *ctx, void *cfg)
{
}

void sde_dspp_setup_pa_memcolor(struct sde_hw_dspp *ctx, void *cfg)
{
}

void sde_dspp_setup_sixzone(struct sde_hw_dspp *dspp)
{
}

void sde_dspp_setup_danger_safe(struct sde_hw_dspp *ctx, void *cfg)
{
}

void sde_dspp_setup_dither(struct sde_hw_dspp *ctx, void *cfg)
{
}

static void _setup_dspp_ops(struct sde_hw_dspp_ops *ops,
		unsigned long features)
{
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
	_setup_dspp_ops(&c->ops, c->cap->features);

	return c;
}

