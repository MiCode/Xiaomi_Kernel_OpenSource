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
#include "sde_hw_top.h"
#include "sde_dbg.h"
#include "sde_kms.h"

#define SSPP_SPARE                        0x28
#define UBWC_STATIC                       0x144

#define FLD_SPLIT_DISPLAY_CMD             BIT(1)
#define FLD_SMART_PANEL_FREE_RUN          BIT(2)
#define FLD_INTF_1_SW_TRG_MUX             BIT(4)
#define FLD_INTF_2_SW_TRG_MUX             BIT(8)
#define FLD_TE_LINE_INTER_WATERLEVEL_MASK 0xFFFF

#define DANGER_STATUS                     0x360
#define SAFE_STATUS                       0x364

#define TE_LINE_INTERVAL                  0x3F4

#define TRAFFIC_SHAPER_EN                 BIT(31)
#define TRAFFIC_SHAPER_RD_CLIENT(num)     (0x030 + (num * 4))
#define TRAFFIC_SHAPER_WR_CLIENT(num)     (0x060 + (num * 4))
#define TRAFFIC_SHAPER_FIXPOINT_FACTOR    4

#define MDP_WD_TIMER_0_CTL                0x380
#define MDP_WD_TIMER_0_CTL2               0x384
#define MDP_WD_TIMER_0_LOAD_VALUE         0x388

#define MDP_TICK_COUNT                    16
#define XO_CLK_RATE                       19200
#define MS_TICKS_IN_SEC                   1000

#define CALCULATE_WD_LOAD_VALUE(fps) \
	((uint32_t)((MS_TICKS_IN_SEC * XO_CLK_RATE)/(MDP_TICK_COUNT * fps)))

#define DCE_SEL                           0x450

static void sde_hw_setup_split_pipe(struct sde_hw_mdp *mdp,
		struct split_pipe_cfg *cfg)
{
	struct sde_hw_blk_reg_map *c;
	u32 upper_pipe = 0;
	u32 lower_pipe = 0;

	if (!mdp || !cfg)
		return;

	c = &mdp->hw;

	if (cfg->en) {
		if (cfg->mode == INTF_MODE_CMD) {
			lower_pipe = FLD_SPLIT_DISPLAY_CMD;
			/* interface controlling sw trigger */
			if (cfg->intf == INTF_2)
				lower_pipe |= FLD_INTF_1_SW_TRG_MUX;
			else
				lower_pipe |= FLD_INTF_2_SW_TRG_MUX;

			/* free run */
			if (cfg->pp_split_slave != INTF_MAX)
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

	SDE_REG_WRITE(c, SSPP_SPARE, cfg->split_flush_en ? 0x1 : 0x0);
	SDE_REG_WRITE(c, SPLIT_DISPLAY_LOWER_PIPE_CTRL, lower_pipe);
	SDE_REG_WRITE(c, SPLIT_DISPLAY_UPPER_PIPE_CTRL, upper_pipe);
	SDE_REG_WRITE(c, SPLIT_DISPLAY_EN, cfg->en & 0x1);
}

static void sde_hw_setup_pp_split(struct sde_hw_mdp *mdp,
		struct split_pipe_cfg *cfg)
{
	u32 ppb_config = 0x0;
	u32 ppb_control = 0x0;

	if (!mdp || !cfg)
		return;

	if (cfg->en && cfg->pp_split_slave != INTF_MAX) {
		ppb_config |= (cfg->pp_split_slave - INTF_0 + 1) << 20;
		ppb_config |= BIT(16); /* split enable */
		ppb_control = BIT(5); /* horz split*/
	}
	if (cfg->pp_split_index) {
		SDE_REG_WRITE(&mdp->hw, PPB0_CONFIG, 0x0);
		SDE_REG_WRITE(&mdp->hw, PPB0_CNTL, 0x0);
		SDE_REG_WRITE(&mdp->hw, PPB1_CONFIG, ppb_config);
		SDE_REG_WRITE(&mdp->hw, PPB1_CNTL, ppb_control);
	} else {
		SDE_REG_WRITE(&mdp->hw, PPB0_CONFIG, ppb_config);
		SDE_REG_WRITE(&mdp->hw, PPB0_CNTL, ppb_control);
		SDE_REG_WRITE(&mdp->hw, PPB1_CONFIG, 0x0);
		SDE_REG_WRITE(&mdp->hw, PPB1_CNTL, 0x0);
	}
}

static void sde_hw_setup_cdm_output(struct sde_hw_mdp *mdp,
		struct cdm_output_cfg *cfg)
{
	struct sde_hw_blk_reg_map *c;
	u32 out_ctl = 0;

	if (!mdp || !cfg)
		return;

	c = &mdp->hw;

	if (cfg->wb_en)
		out_ctl |= BIT(24);
	else if (cfg->intf_en)
		out_ctl |= BIT(19);

	SDE_REG_WRITE(c, MDP_OUT_CTL_0, out_ctl);
}

static bool sde_hw_setup_clk_force_ctrl(struct sde_hw_mdp *mdp,
		enum sde_clk_ctrl_type clk_ctrl, bool enable)
{
	struct sde_hw_blk_reg_map *c;
	u32 reg_off, bit_off;
	u32 reg_val, new_val;
	bool clk_forced_on;

	if (!mdp)
		return false;

	c = &mdp->hw;

	if (clk_ctrl <= SDE_CLK_CTRL_NONE || clk_ctrl >= SDE_CLK_CTRL_MAX)
		return false;

	reg_off = mdp->caps->clk_ctrls[clk_ctrl].reg_off;
	bit_off = mdp->caps->clk_ctrls[clk_ctrl].bit_off;

	reg_val = SDE_REG_READ(c, reg_off);

	if (enable)
		new_val = reg_val | BIT(bit_off);
	else
		new_val = reg_val & ~BIT(bit_off);

	SDE_REG_WRITE(c, reg_off, new_val);

	clk_forced_on = !(reg_val & BIT(bit_off));

	return clk_forced_on;
}


static void sde_hw_get_danger_status(struct sde_hw_mdp *mdp,
		struct sde_danger_safe_status *status)
{
	struct sde_hw_blk_reg_map *c;
	u32 value;

	if (!mdp || !status)
		return;

	c = &mdp->hw;

	value = SDE_REG_READ(c, DANGER_STATUS);
	status->mdp = (value >> 0) & 0x3;
	status->sspp[SSPP_VIG0] = (value >> 4) & 0x3;
	status->sspp[SSPP_VIG1] = (value >> 6) & 0x3;
	status->sspp[SSPP_VIG2] = (value >> 8) & 0x3;
	status->sspp[SSPP_VIG3] = (value >> 10) & 0x3;
	status->sspp[SSPP_RGB0] = (value >> 12) & 0x3;
	status->sspp[SSPP_RGB1] = (value >> 14) & 0x3;
	status->sspp[SSPP_RGB2] = (value >> 16) & 0x3;
	status->sspp[SSPP_RGB3] = (value >> 18) & 0x3;
	status->sspp[SSPP_DMA0] = (value >> 20) & 0x3;
	status->sspp[SSPP_DMA1] = (value >> 22) & 0x3;
	status->sspp[SSPP_DMA2] = (value >> 28) & 0x3;
	status->sspp[SSPP_DMA3] = (value >> 30) & 0x3;
	status->sspp[SSPP_CURSOR0] = (value >> 24) & 0x3;
	status->sspp[SSPP_CURSOR1] = (value >> 26) & 0x3;
	status->wb[WB_0] = 0;
	status->wb[WB_1] = 0;
	status->wb[WB_2] = (value >> 2) & 0x3;
	status->wb[WB_3] = 0;
}

static void sde_hw_setup_vsync_sel(struct sde_hw_mdp *mdp,
		struct sde_watchdog_te_status *cfg, bool watchdog_te)
{
	struct sde_hw_blk_reg_map *c = &mdp->hw;
	u32 reg = 0;
	int i = 0;
	u32 pp_offset[] = {0xC, 0x8, 0x4, 0x13};

	if (!mdp)
		return;

	reg = SDE_REG_READ(c, MDP_VSYNC_SEL);
	for (i = 0; i < cfg->pp_count; i++) {
		int pp_idx = cfg->ppnumber[i] - PINGPONG_0;

		if (watchdog_te)
			reg |= 0xF << pp_offset[pp_idx];
		else
			reg &= ~(0xF << pp_offset[pp_idx]);
	}

	SDE_REG_WRITE(c, MDP_VSYNC_SEL, reg);

	if (watchdog_te) {
		SDE_REG_WRITE(c, MDP_WD_TIMER_0_LOAD_VALUE,
				CALCULATE_WD_LOAD_VALUE(cfg->frame_rate));

		SDE_REG_WRITE(c, MDP_WD_TIMER_0_CTL, BIT(0)); /* clear timer */
		reg = SDE_REG_READ(c, MDP_WD_TIMER_0_CTL2);
		reg |= BIT(8);		/* enable heartbeat timer */
		reg |= BIT(0);		/* enable WD timer */
		SDE_REG_WRITE(c, MDP_WD_TIMER_0_CTL2, reg);
	}
}

static void sde_hw_get_safe_status(struct sde_hw_mdp *mdp,
		struct sde_danger_safe_status *status)
{
	struct sde_hw_blk_reg_map *c;
	u32 value;

	if (!mdp || !status)
		return;

	c = &mdp->hw;

	value = SDE_REG_READ(c, SAFE_STATUS);
	status->mdp = (value >> 0) & 0x1;
	status->sspp[SSPP_VIG0] = (value >> 4) & 0x1;
	status->sspp[SSPP_VIG1] = (value >> 6) & 0x1;
	status->sspp[SSPP_VIG2] = (value >> 8) & 0x1;
	status->sspp[SSPP_VIG3] = (value >> 10) & 0x1;
	status->sspp[SSPP_RGB0] = (value >> 12) & 0x1;
	status->sspp[SSPP_RGB1] = (value >> 14) & 0x1;
	status->sspp[SSPP_RGB2] = (value >> 16) & 0x1;
	status->sspp[SSPP_RGB3] = (value >> 18) & 0x1;
	status->sspp[SSPP_DMA0] = (value >> 20) & 0x1;
	status->sspp[SSPP_DMA1] = (value >> 22) & 0x1;
	status->sspp[SSPP_DMA2] = (value >> 28) & 0x1;
	status->sspp[SSPP_DMA3] = (value >> 30) & 0x1;
	status->sspp[SSPP_CURSOR0] = (value >> 24) & 0x1;
	status->sspp[SSPP_CURSOR1] = (value >> 26) & 0x1;
	status->wb[WB_0] = 0;
	status->wb[WB_1] = 0;
	status->wb[WB_2] = (value >> 2) & 0x1;
	status->wb[WB_3] = 0;
}

static void sde_hw_setup_dce(struct sde_hw_mdp *mdp, u32 dce_sel)
{
	struct sde_hw_blk_reg_map *c;

	if (!mdp)
		return;

	c = &mdp->hw;

	SDE_REG_WRITE(c, DCE_SEL, dce_sel);
}

void sde_hw_reset_ubwc(struct sde_hw_mdp *mdp, struct sde_mdss_cfg *m)
{
	struct sde_hw_blk_reg_map c;

	if (!mdp || !m)
		return;

	if (!IS_UBWC_20_SUPPORTED(m->ubwc_version))
		return;

	/* force blk offset to zero to access beginning of register region */
	c = mdp->hw;
	c.blk_off = 0x0;
	SDE_REG_WRITE(&c, UBWC_STATIC, m->mdp[0].ubwc_static);
}

static void _setup_mdp_ops(struct sde_hw_mdp_ops *ops,
		unsigned long cap)
{
	ops->setup_split_pipe = sde_hw_setup_split_pipe;
	ops->setup_pp_split = sde_hw_setup_pp_split;
	ops->setup_cdm_output = sde_hw_setup_cdm_output;
	ops->setup_clk_force_ctrl = sde_hw_setup_clk_force_ctrl;
	ops->get_danger_status = sde_hw_get_danger_status;
	ops->setup_vsync_sel = sde_hw_setup_vsync_sel;
	ops->get_safe_status = sde_hw_get_safe_status;
	ops->setup_dce = sde_hw_setup_dce;
	ops->reset_ubwc = sde_hw_reset_ubwc;
}

static const struct sde_mdp_cfg *_top_offset(enum sde_mdp mdp,
		const struct sde_mdss_cfg *m,
		void __iomem *addr,
		struct sde_hw_blk_reg_map *b)
{
	int i;

	if (!m || !addr || !b)
		return ERR_PTR(-EINVAL);

	for (i = 0; i < m->mdp_count; i++) {
		if (mdp == m->mdp[i].id) {
			b->base_off = addr;
			b->blk_off = m->mdp[i].base;
			b->length = m->mdp[i].len;
			b->hwversion = m->hwversion;
			b->log_mask = SDE_DBG_MASK_TOP;
			return &m->mdp[i];
		}
	}

	return ERR_PTR(-EINVAL);
}

static struct sde_hw_blk_ops sde_hw_ops = {
	.start = NULL,
	.stop = NULL,
};

struct sde_hw_mdp *sde_hw_mdptop_init(enum sde_mdp idx,
		void __iomem *addr,
		const struct sde_mdss_cfg *m)
{
	struct sde_hw_mdp *mdp;
	const struct sde_mdp_cfg *cfg;
	int rc;

	if (!addr || !m)
		return ERR_PTR(-EINVAL);

	mdp = kzalloc(sizeof(*mdp), GFP_KERNEL);
	if (!mdp)
		return ERR_PTR(-ENOMEM);

	cfg = _top_offset(idx, m, addr, &mdp->hw);
	if (IS_ERR_OR_NULL(cfg)) {
		kfree(mdp);
		return ERR_PTR(-EINVAL);
	}

	/*
	 * Assign ops
	 */
	mdp->idx = idx;
	mdp->caps = cfg;
	_setup_mdp_ops(&mdp->ops, mdp->caps->features);

	rc = sde_hw_blk_init(&mdp->base, SDE_HW_BLK_TOP, idx, &sde_hw_ops);
	if (rc) {
		SDE_ERROR("failed to init hw blk %d\n", rc);
		goto blk_init_error;
	}

	sde_dbg_reg_register_dump_range(SDE_DBG_NAME, cfg->name,
			mdp->hw.blk_off, mdp->hw.blk_off + mdp->hw.length,
			mdp->hw.xin_id);
	sde_dbg_set_sde_top_offset(mdp->hw.blk_off);

	return mdp;

blk_init_error:
	kzfree(mdp);

	return ERR_PTR(rc);
}

void sde_hw_mdp_destroy(struct sde_hw_mdp *mdp)
{
	if (mdp)
		sde_hw_blk_destroy(&mdp->base);
	kfree(mdp);
}

