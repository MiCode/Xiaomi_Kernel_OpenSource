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

#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_top.h"

#define SSPP_SPARE                        0x24
#define SPLIT_DISPLAY_ENABLE              0x2F4

#define LOWER_PIPE_CTRL                   0x2F8
#define FLD_SPLIT_DISPLAY_CMD             BIT(1)
#define FLD_SMART_PANEL_FREE_RUN          BIT(2)
#define FLD_INTF_1_SW_TRG_MUX             BIT(4)
#define FLD_INTF_2_SW_TRG_MUX             BIT(8)
#define FLD_TE_LINE_INTER_WATERLEVEL_MASK 0xFFFF

#define UPPER_PIPE_CTRL                   0x3F0
#define TE_LINE_INTERVAL                  0x3F4

#define TRAFFIC_SHAPER_EN                 BIT(31)
#define TRAFFIC_SHAPER_RD_CLIENT(num)     (0x030 + (num * 4))
#define TRAFFIC_SHAPER_WR_CLIENT(num)     (0x060 + (num * 4))
#define TRAFFIC_SHAPER_FIXPOINT_FACTOR    4

static void sde_hw_setup_split_pipe_control(struct sde_hw_mdp *mdp,
		struct split_pipe_cfg *cfg)
{
	struct sde_hw_blk_reg_map *c = &mdp->hw;
	u32 upper_pipe = 0;
	u32 lower_pipe = 0;

	/* The SPLIT registers are only for DSI interfaces */
	if ((cfg->intf != INTF_1) && (cfg->intf != INTF_2))
		return;

	if (cfg->en) {
		if (cfg->mode == INTF_MODE_CMD) {
			lower_pipe = FLD_SPLIT_DISPLAY_CMD;
			/* interface controlling sw trigger */
			if (cfg->intf == INTF_2)
				lower_pipe |= FLD_INTF_1_SW_TRG_MUX;
			else
				lower_pipe |= FLD_INTF_2_SW_TRG_MUX;

			/* free run */
			if (cfg->pp_split)
				lower_pipe = FLD_SMART_PANEL_FREE_RUN;

			upper_pipe = lower_pipe;
		} else {
			if (cfg->intf == INTF_2) {
				lower_pipe = FLD_INTF_1_SW_TRG_MUX;
				upper_pipe = FLD_INTF_2_SW_TRG_MUX;
			} else {
				lower_pipe = FLD_INTF_2_SW_TRG_MUX;
				upper_pipe = FLD_INTF_1_SW_TRG_MUX;
			}
		}
	}

	SDE_REG_WRITE(c, SSPP_SPARE, (cfg->split_flush_en) ? 0x1 : 0x0);
	SDE_REG_WRITE(c, LOWER_PIPE_CTRL, lower_pipe);
	SDE_REG_WRITE(c, UPPER_PIPE_CTRL, upper_pipe);
	SDE_REG_WRITE(c, SPLIT_DISPLAY_ENABLE, cfg->en & 0x1);
}

static void sde_hw_setup_cdm_output(struct sde_hw_mdp *mdp,
		struct cdm_output_cfg *cfg)
{
	struct sde_hw_blk_reg_map *c = &mdp->hw;
	u32 out_ctl = 0;

	if (cfg->wb_en)
		out_ctl |= BIT(24);
	else if (cfg->intf_en)
		out_ctl |= BIT(19);

	SDE_REG_WRITE(c, MDP_OUT_CTL_0, out_ctl);
}

static void sde_hw_setup_traffic_shaper(struct sde_hw_mdp *mdp,
		struct traffic_shaper_cfg *cfg)
{
	struct sde_hw_blk_reg_map *c = &mdp->hw;
	u32 ts_control = 0;
	u32 offset;
	u64 bpc;

	if (cfg->rd_client)
		offset = TRAFFIC_SHAPER_RD_CLIENT(cfg->client_id);
	else
		offset = TRAFFIC_SHAPER_WR_CLIENT(cfg->client_id);

	if (cfg->en) {
		bpc = cfg->bpc_numer;
		do_div(bpc, (cfg->bpc_denom >>
					TRAFFIC_SHAPER_FIXPOINT_FACTOR));
		ts_control = lower_32_bits(bpc) + 1;
		ts_control |= TRAFFIC_SHAPER_EN;
	}

	SDE_REG_WRITE(c, offset, ts_control);
}

static void _setup_mdp_ops(struct sde_hw_mdp_ops *ops,
		unsigned long cap)
{
	ops->setup_split_pipe = sde_hw_setup_split_pipe_control;
	ops->setup_cdm_output = sde_hw_setup_cdm_output;
	ops->setup_traffic_shaper = sde_hw_setup_traffic_shaper;
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
			b->log_mask = SDE_DBG_MASK_TOP;
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
	if (c)
		return c;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
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
	kfree(mdp);
}

