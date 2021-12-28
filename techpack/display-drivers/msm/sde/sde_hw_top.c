// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
 */

#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_top.h"
#include "sde_dbg.h"
#include "sde_kms.h"

#define SSPP_SPARE                        0x28
#define UBWC_DEC_HW_VERSION               0x058
#define UBWC_STATIC                       0x144
#define UBWC_CTRL_2                       0x150
#define UBWC_PREDICTION_MODE              0x154

#define FLD_SPLIT_DISPLAY_CMD             BIT(1)
#define FLD_SMART_PANEL_FREE_RUN          BIT(2)
#define FLD_INTF_1_SW_TRG_MUX             BIT(4)
#define FLD_INTF_2_SW_TRG_MUX             BIT(8)
#define FLD_TE_LINE_INTER_WATERLEVEL_MASK 0xFFFF

#define MDP_DSPP_DBGBUS_CTRL              0x348
#define MDP_DSPP_DBGBUS_STATUS            0x34C
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
#define MDP_WD_TIMER_1_CTL                0x390
#define MDP_WD_TIMER_1_CTL2               0x394
#define MDP_WD_TIMER_1_LOAD_VALUE         0x398
#define MDP_PERIPH_DBGBUS_CTRL            0x418
#define MDP_WD_TIMER_2_CTL                0x420
#define MDP_WD_TIMER_2_CTL2               0x424
#define MDP_WD_TIMER_2_LOAD_VALUE         0x428
#define MDP_WD_TIMER_3_CTL                0x430
#define MDP_WD_TIMER_3_CTL2               0x434
#define MDP_WD_TIMER_3_LOAD_VALUE         0x438
#define MDP_WD_TIMER_4_CTL                0x440
#define MDP_WD_TIMER_4_CTL2               0x444
#define MDP_WD_TIMER_4_LOAD_VALUE         0x448

#define MDP_PERIPH_TOP0                   0x380
#define MDP_SSPP_TOP2                     0x3A8

#define AUTOREFRESH_TEST_POINT	0x2
#define TEST_MASK(id, tp)	((id << 4) | (tp << 1) | BIT(0))

#define DCE_SEL                           0x450

#define MDP_SID_VIG0			  0x0
#define MDP_SID_VIG1			  0x4
#define MDP_SID_VIG2			  0x8
#define MDP_SID_VIG3			  0xC
#define MDP_SID_DMA0			  0x10
#define MDP_SID_DMA1			  0x14
#define MDP_SID_DMA2			  0x18
#define MDP_SID_DMA3			  0x1C
#define MDP_SID_ROT_RD			  0x20
#define MDP_SID_ROT_WR			  0x24
#define MDP_SID_WB2			  0x28
#define MDP_SID_XIN7			  0x2C

#define ROT_SID_ID_VAL			  0x1c

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

			/* smart panel align mode */
			lower_pipe |= BIT(mdp->caps->smart_panel_align_mode);
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
	wmb(); /* ensure write finished before progressing */

	clk_forced_on = !(reg_val & BIT(bit_off));

	return clk_forced_on;
}

static int sde_hw_get_clk_ctrl_status(struct sde_hw_mdp *mdp,
		enum sde_clk_ctrl_type clk_ctrl, bool *status)
{
	struct sde_hw_blk_reg_map *c;
	u32 reg_off, bit_off;

	if (!mdp)
		return -EINVAL;

	c = &mdp->hw;

	if (clk_ctrl <= SDE_CLK_CTRL_NONE || clk_ctrl >= SDE_CLK_CTRL_MAX ||
			!mdp->caps->clk_status[clk_ctrl].reg_off)
		return -EINVAL;

	reg_off = mdp->caps->clk_status[clk_ctrl].reg_off;
	bit_off = mdp->caps->clk_status[clk_ctrl].bit_off;

	*status = SDE_REG_READ(c, reg_off) & BIT(bit_off);
	return 0;
}

static void _update_vsync_source(struct sde_hw_mdp *mdp,
		struct sde_vsync_source_cfg *cfg)
{
	struct sde_hw_blk_reg_map *c;
	u32 reg, wd_load_value, wd_ctl, wd_ctl2;

	if (!mdp || !cfg)
		return;

	c = &mdp->hw;

	if (cfg->vsync_source >= SDE_VSYNC_SOURCE_WD_TIMER_4 &&
			cfg->vsync_source <= SDE_VSYNC_SOURCE_WD_TIMER_0) {
		switch (cfg->vsync_source) {
		case SDE_VSYNC_SOURCE_WD_TIMER_4:
			wd_load_value = MDP_WD_TIMER_4_LOAD_VALUE;
			wd_ctl = MDP_WD_TIMER_4_CTL;
			wd_ctl2 = MDP_WD_TIMER_4_CTL2;
			break;
		case SDE_VSYNC_SOURCE_WD_TIMER_3:
			wd_load_value = MDP_WD_TIMER_3_LOAD_VALUE;
			wd_ctl = MDP_WD_TIMER_3_CTL;
			wd_ctl2 = MDP_WD_TIMER_3_CTL2;
			break;
		case SDE_VSYNC_SOURCE_WD_TIMER_2:
			wd_load_value = MDP_WD_TIMER_2_LOAD_VALUE;
			wd_ctl = MDP_WD_TIMER_2_CTL;
			wd_ctl2 = MDP_WD_TIMER_2_CTL2;
			break;
		case SDE_VSYNC_SOURCE_WD_TIMER_1:
			wd_load_value = MDP_WD_TIMER_1_LOAD_VALUE;
			wd_ctl = MDP_WD_TIMER_1_CTL;
			wd_ctl2 = MDP_WD_TIMER_1_CTL2;
			break;
		case SDE_VSYNC_SOURCE_WD_TIMER_0:
		default:
			wd_load_value = MDP_WD_TIMER_0_LOAD_VALUE;
			wd_ctl = MDP_WD_TIMER_0_CTL;
			wd_ctl2 = MDP_WD_TIMER_0_CTL2;
			break;
		}

		SDE_REG_WRITE(c, wd_load_value, CALCULATE_WD_LOAD_VALUE(cfg->frame_rate));

		SDE_REG_WRITE(c, wd_ctl, BIT(0)); /* clear timer */
		reg = SDE_REG_READ(c, wd_ctl2);
		reg |= BIT(8); /* enable heartbeat timer */
		reg |= BIT(0); /* enable WD timer */
		SDE_REG_WRITE(c, wd_ctl2, reg);

		/* make sure that timers are enabled/disabled for vsync state */
		wmb();
	}
}

static void sde_hw_setup_vsync_source(struct sde_hw_mdp *mdp,
		struct sde_vsync_source_cfg *cfg)
{
	struct sde_hw_blk_reg_map *c;
	u32 reg, i;
	static const u32 pp_offset[PINGPONG_MAX] = {0xC, 0x8, 0x4, 0x13, 0x18};

	if (!mdp || !cfg || (cfg->pp_count > ARRAY_SIZE(cfg->ppnumber)))
		return;

	c = &mdp->hw;
	reg = SDE_REG_READ(c, MDP_VSYNC_SEL);
	for (i = 0; i < cfg->pp_count; i++) {
		int pp_idx = cfg->ppnumber[i] - PINGPONG_0;

		if (pp_idx >= ARRAY_SIZE(pp_offset))
			continue;

		reg &= ~(0xf << pp_offset[pp_idx]);
		reg |= (cfg->vsync_source & 0xf) << pp_offset[pp_idx];
	}
	SDE_REG_WRITE(c, MDP_VSYNC_SEL, reg);

	_update_vsync_source(mdp, cfg);
}

static void sde_hw_setup_vsync_source_v1(struct sde_hw_mdp *mdp,
		struct sde_vsync_source_cfg *cfg)
{
	_update_vsync_source(mdp, cfg);
}

void sde_hw_reset_ubwc(struct sde_hw_mdp *mdp, struct sde_mdss_cfg *m)
{
	struct sde_hw_blk_reg_map c;
	u32 ubwc_version;

	if (!mdp || !m)
		return;

	/* force blk offset to zero to access beginning of register region */
	c = mdp->hw;
	c.blk_off = 0x0;
	ubwc_version = SDE_REG_READ(&c, UBWC_DEC_HW_VERSION);

	if (IS_UBWC_40_SUPPORTED(ubwc_version)) {
		u32 ver = 2;
		u32 mode = 1;
		u32 reg = (m->mdp[0].ubwc_swizzle & 0x7) |
			((m->mdp[0].ubwc_static & 0x1) << 3) |
			((m->mdp[0].highest_bank_bit & 0x7) << 4) |
			((m->macrotile_mode & 0x1) << 12);

		if (IS_UBWC_30_SUPPORTED(m->ubwc_version)) {
			ver = 1;
			mode = 0;
		}

		SDE_REG_WRITE(&c, UBWC_STATIC, reg);
		SDE_REG_WRITE(&c, UBWC_CTRL_2, ver);
		SDE_REG_WRITE(&c, UBWC_PREDICTION_MODE, mode);
	} else if (IS_UBWC_20_SUPPORTED(ubwc_version)) {
		SDE_REG_WRITE(&c, UBWC_STATIC, m->mdp[0].ubwc_static);
	} else if (IS_UBWC_30_SUPPORTED(ubwc_version)) {
		u32 reg = m->mdp[0].ubwc_static |
			(m->mdp[0].ubwc_swizzle & 0x1) |
			((m->mdp[0].highest_bank_bit & 0x3) << 4) |
			((m->macrotile_mode & 0x1) << 12);

		if (IS_UBWC_30_SUPPORTED(m->ubwc_version))
			reg |= BIT(10);
		if (IS_UBWC_10_SUPPORTED(m->ubwc_version))
			reg |= BIT(8);

		SDE_REG_WRITE(&c, UBWC_STATIC, reg);
	} else {
		SDE_ERROR("Unsupported UBWC version 0x%08x\n", ubwc_version);
	}
}

static void sde_hw_intf_audio_select(struct sde_hw_mdp *mdp)
{
	struct sde_hw_blk_reg_map *c;

	if (!mdp)
		return;

	c = &mdp->hw;

	SDE_REG_WRITE(c, HDMI_DP_CORE_SELECT, 0x1);
}

static void sde_hw_mdp_events(struct sde_hw_mdp *mdp, bool enable)
{
	struct sde_hw_blk_reg_map *c;

	if (!mdp)
		return;

	c = &mdp->hw;

	SDE_REG_WRITE(c, HW_EVENTS_CTL, enable);
}

struct sde_hw_sid *sde_hw_sid_init(void __iomem *addr,
	u32 sid_len, const struct sde_mdss_cfg *m)
{
	struct sde_hw_sid *c;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	c->hw.base_off = addr;
	c->hw.blk_off = 0;
	c->hw.length = sid_len;
	c->hw.hwversion = m->hwversion;
	c->hw.log_mask = SDE_DBG_MASK_SID;

	return c;
}

void sde_hw_set_rotator_sid(struct sde_hw_sid *sid)
{
	if (!sid)
		return;

	SDE_REG_WRITE(&sid->hw, MDP_SID_ROT_RD, ROT_SID_ID_VAL);
	SDE_REG_WRITE(&sid->hw, MDP_SID_ROT_WR, ROT_SID_ID_VAL);
}

void sde_hw_set_sspp_sid(struct sde_hw_sid *sid, u32 pipe, u32 vm)
{
	u32 offset = 0;

	if (!sid)
		return;

	if ((pipe >= SSPP_VIG0) && (pipe <= SSPP_VIG3))
		offset = MDP_SID_VIG0 + ((pipe - SSPP_VIG0) * 4);
	else if ((pipe >= SSPP_DMA0) && (pipe <= SSPP_DMA3))
		offset = MDP_SID_DMA0 + ((pipe - SSPP_DMA0) * 4);
	else
		return;

	SDE_REG_WRITE(&sid->hw, offset, vm << 2);
}

void sde_hw_set_lutdma_sid(struct sde_hw_sid *sid, u32 vm)
{
	if (!sid)
		return;

	SDE_REG_WRITE(&sid->hw, MDP_SID_XIN7, vm << 2);
}

static void sde_hw_program_cwb_ppb_ctrl(struct sde_hw_mdp *mdp,
		bool dual, bool dspp_out)
{
	u32 value = dspp_out ? 0x4 : 0x0;

	SDE_REG_WRITE(&mdp->hw, PPB2_CNTL, value);
	if (dual) {
		value |= 0x1;
		SDE_REG_WRITE(&mdp->hw, PPB3_CNTL, value);
	}
}

static void sde_hw_set_hdr_plus_metadata(struct sde_hw_mdp *mdp,
		u8 *payload, u32 len, u32 stream_id)
{
	u32 i, b;
	u32 length = len - 1;
	u32 d_offset, nb_offset, data = 0;
	const u32 dword_size = sizeof(u32);
	bool is_4k_aligned = mdp->caps->features &
			BIT(SDE_MDP_DHDR_MEMPOOL_4K);

	if (!payload || !len) {
		SDE_ERROR("invalid payload with length: %d\n", len);
		return;
	}

	if (stream_id) {
		if (is_4k_aligned) {
			d_offset = DP_DHDR_MEM_POOL_1_DATA_4K;
			nb_offset = DP_DHDR_MEM_POOL_1_NUM_BYTES_4K;
		} else {
			d_offset = DP_DHDR_MEM_POOL_1_DATA;
			nb_offset = DP_DHDR_MEM_POOL_1_NUM_BYTES;
		}
	} else {
		if (is_4k_aligned) {
			d_offset = DP_DHDR_MEM_POOL_0_DATA_4K;
			nb_offset = DP_DHDR_MEM_POOL_0_NUM_BYTES_4K;
		} else {
			d_offset = DP_DHDR_MEM_POOL_0_DATA;
			nb_offset = DP_DHDR_MEM_POOL_0_NUM_BYTES;
		}
	}

	/* payload[0] is set in VSCEXT header byte 1, skip programming here */
	SDE_REG_WRITE(&mdp->hw, nb_offset, length);
	for (i = 1; i < len; i += dword_size) {
		for (b = 0; (i + b) < len && b < dword_size; b++)
			data |= payload[i + b] << (8 * b);

		SDE_REG_WRITE(&mdp->hw, d_offset, data);
		data = 0;
	}
}

static u32 sde_hw_get_autorefresh_status(struct sde_hw_mdp *mdp, u32 intf_idx)
{
	struct sde_hw_blk_reg_map *c;
	u32 autorefresh_status;
	u32 blk_id = (intf_idx == INTF_2) ? 65 : 64;

	if (!mdp)
		return 0;

	c = &mdp->hw;

	SDE_REG_WRITE(&mdp->hw, MDP_PERIPH_DBGBUS_CTRL,
			TEST_MASK(blk_id, AUTOREFRESH_TEST_POINT));
	SDE_REG_WRITE(&mdp->hw, MDP_DSPP_DBGBUS_CTRL, 0x7001);
	wmb(); /* make sure test bits were written */

	autorefresh_status = SDE_REG_READ(&mdp->hw, MDP_DSPP_DBGBUS_STATUS);
	SDE_REG_WRITE(&mdp->hw, MDP_PERIPH_DBGBUS_CTRL, 0x0);

	return autorefresh_status;
}

static void _setup_mdp_ops(struct sde_hw_mdp_ops *ops,
		unsigned long cap)
{
	ops->setup_split_pipe = sde_hw_setup_split_pipe;
	ops->setup_pp_split = sde_hw_setup_pp_split;
	ops->setup_cdm_output = sde_hw_setup_cdm_output;
	ops->setup_clk_force_ctrl = sde_hw_setup_clk_force_ctrl;
	ops->get_clk_ctrl_status = sde_hw_get_clk_ctrl_status;
	ops->set_cwb_ppb_cntl = sde_hw_program_cwb_ppb_ctrl;
	ops->reset_ubwc = sde_hw_reset_ubwc;
	ops->intf_audio_select = sde_hw_intf_audio_select;
	ops->set_mdp_hw_events = sde_hw_mdp_events;
	if (cap & BIT(SDE_MDP_VSYNC_SEL))
		ops->setup_vsync_source = sde_hw_setup_vsync_source;
	else if (cap & BIT(SDE_MDP_WD_TIMER))
		ops->setup_vsync_source = sde_hw_setup_vsync_source_v1;

	if (cap & BIT(SDE_MDP_DHDR_MEMPOOL_4K) ||
			cap & BIT(SDE_MDP_DHDR_MEMPOOL))
		ops->set_hdr_plus_metadata = sde_hw_set_hdr_plus_metadata;
	ops->get_autorefresh_status = sde_hw_get_autorefresh_status;
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

	sde_dbg_reg_register_dump_range(SDE_DBG_NAME, "mdss_hw", 0,
			m->mdss_hw_block_size, 0);

	if (test_bit(SDE_MDP_PERIPH_TOP_0_REMOVED, &m->mdp[0].features)) {
		char name[SDE_HW_BLK_NAME_LEN];

		snprintf(name, sizeof(name), "%s_1", cfg->name);

		sde_dbg_reg_register_dump_range(SDE_DBG_NAME, cfg->name, mdp->hw.blk_off,
				mdp->hw.blk_off + MDP_PERIPH_TOP0, mdp->hw.xin_id);

		sde_dbg_reg_register_dump_range(SDE_DBG_NAME, name, mdp->hw.blk_off + MDP_SSPP_TOP2,
				mdp->hw.blk_off +  mdp->hw.length, mdp->hw.xin_id);
	} else {
		sde_dbg_reg_register_dump_range(SDE_DBG_NAME, cfg->name,
			mdp->hw.blk_off, mdp->hw.blk_off + mdp->hw.length,
			mdp->hw.xin_id);
	}
	sde_dbg_set_sde_top_offset(mdp->hw.blk_off);

	return mdp;

blk_init_error:
	kfree(mdp);

	return ERR_PTR(rc);
}

void sde_hw_mdp_destroy(struct sde_hw_mdp *mdp)
{
	if (mdp)
		sde_hw_blk_destroy(&mdp->base);
	kfree(mdp);
}

