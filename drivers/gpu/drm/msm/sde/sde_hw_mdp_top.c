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

#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_mdp_top.h"

#define SPLIT_DISPLAY_ENABLE              0x2F4
#define LOWER_PIPE_CTRL                   0x2F8
#define UPPER_PIPE_CTRL                   0x3F0
#define TE_LINE_INTERVAL                  0x3F4

static void sde_hw_setup_split_pipe_control(struct sde_hw_mdp *mdp,
		 struct split_pipe_cfg *cfg)
{
	struct sde_hw_blk_reg_map *c = &mdp->hw;
	u32 upper_pipe;
	u32 lower_pipe;

	if (cfg->en) {
		upper_pipe = BIT(8);
		lower_pipe = BIT(8);

		if (cfg->mode == INTF_MODE_CMD) {
			upper_pipe |= BIT(0);
			lower_pipe |= BIT(0);
		}

		SDE_REG_WRITE(c, LOWER_PIPE_CTRL, lower_pipe);
		SDE_REG_WRITE(c, UPPER_PIPE_CTRL, upper_pipe);
	}

	SDE_REG_WRITE(c, SPLIT_DISPLAY_ENABLE, cfg->en & 0x1);
}

static void _setup_mdp_ops(struct sde_hw_mdp_ops *ops,
		unsigned long cap)
{
	ops->setup_split_pipe = sde_hw_setup_split_pipe_control;
}

static const struct sde_mdp_cfg *_top_offset(enum sde_mdp mdp,
		const struct sde_mdss_cfg *m,
		void __iomem *addr,
		struct sde_hw_blk_reg_map *b)
{
	int i;

	for (i = 0; i < m->mdp_count; i++) {
		if (mdp == m->mdp[i].id) {
			b->base_off = addr;
			b->blk_off = m->mdp[i].base;
			b->hwversion = m->hwversion;
			return &m->mdp[i];
		}
	}

	return ERR_PTR(-EINVAL);
}

struct sde_hw_mdp *sde_hw_mdptop_init(enum sde_mdp idx,
		void __iomem *addr,
		const struct sde_mdss_cfg *m)
{
	static struct sde_hw_mdp *c;
	const struct sde_mdp_cfg *cfg;

	/* mdp top is singleton */
	if (c) {
		pr_err(" %s returning  %pK", __func__, c);
		return c;
	}

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	pr_err(" %s returning  %pK", __func__, c);
	if (!c)
		return ERR_PTR(-ENOMEM);

	cfg = _top_offset(idx, m, addr, &c->hw);
	if (IS_ERR_OR_NULL(cfg)) {
		kfree(c);
		return ERR_PTR(-EINVAL);
	}

	/*
	 * Assign ops
	 */
	c->idx = idx;
	c->cap = cfg;
	_setup_mdp_ops(&c->ops, c->cap->features);

	/*
	 * Perform any default initialization for the intf
	 */
	return c;
}

void sde_hw_mdp_destroy(struct sde_hw_mdp *mdp)
{
}

