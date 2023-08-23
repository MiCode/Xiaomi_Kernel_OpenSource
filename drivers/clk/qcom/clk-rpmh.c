/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "clk: %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>
#include <soc/qcom/cmd-db.h>
#include <soc/qcom/rpmh.h>
#include <dt-bindings/clock/qcom,rpmh.h>

#include "common.h"
#include "clk-debug.h"
#include "clk-regmap.h"

#define CLK_RPMH_ARC_EN_OFFSET 0
#define CLK_RPMH_VRM_EN_OFFSET 4
#define CLK_RPMH_VRM_OFF_VAL 0
#define CLK_RPMH_VRM_ON_VAL 1
#define CLK_RPMH_APPS_RSC_AO_STATE_MASK (BIT(RPMH_WAKE_ONLY_STATE) | \
					 BIT(RPMH_ACTIVE_ONLY_STATE))
#define CLK_RPMH_APPS_RSC_STATE_MASK (BIT(RPMH_WAKE_ONLY_STATE) | \
				      BIT(RPMH_ACTIVE_ONLY_STATE) | \
				      BIT(RPMH_SLEEP_STATE))

#define BCM_TCS_CMD_COMMIT_MASK		0x40000000
#define BCM_TCS_CMD_VALID_SHIFT		29
#define BCM_TCS_CMD_VOTE_MASK		0x3fff
#define BCM_TCS_CMD_VOTE_SHIFT		0

#define BCM_TCS_CMD(valid, vote)				\
	(BCM_TCS_CMD_COMMIT_MASK |				\
	((valid) << BCM_TCS_CMD_VALID_SHIFT) |			\
	((vote & BCM_TCS_CMD_VOTE_MASK)				\
	<< BCM_TCS_CMD_VOTE_SHIFT))

/**
 * struct bcm_db - Auxiliary data pertaining to each Bus Clock Manager(BCM)
 * @unit: divisor used to convert Hz value to an RPMh msg
 * @width: multiplier used to convert Hz value to an RPMh msg
 * @vcd: virtual clock domain that this bcm belongs to
 * @reserved: reserved to pad the struct
 */
struct bcm_db {
	__le32 unit;
	__le16 width;
	u8 vcd;
	u8 reserved;
};

struct clk_rpmh {
	const char *res_name;
	u32 res_addr;
	u32 res_en_offset;
	u32 res_on_val;
	u32 res_off_val;
	u32 state;
	u32 aggr_state;
	u32 last_sent_aggr_state;
	u32 valid_state_mask;
	u32 unit;
	struct rsc_type *rsc;
	unsigned long rate;
	struct clk_rpmh *peer;
	struct clk_hw hw;
};

struct rsc_type {
	struct rpmh_client *rpmh_client;
	const char *mbox_name;
	const bool use_awake_state;
};

struct rpmh_cc {
	struct clk_onecell_data data;
	struct clk *clks[];
};

struct clk_rpmh_desc {
	struct clk_hw **clks;
	size_t num_clks;
};

static DEFINE_MUTEX(rpmh_clk_lock);

#define __DEFINE_CLK_RPMH(_platform, _name, _name_active, _res_name, \
			  _res_en_offset, _res_on, _res_off, _rsc_id, _rate, \
			  _state_mask, _state_on_mask)			      \
	static struct clk_rpmh _platform##_##_name_active;		      \
	static struct clk_rpmh _platform##_##_name = {			      \
		.res_name = _res_name,					      \
		.res_en_offset = _res_en_offset,			      \
		.res_on_val = _res_on,					      \
		.res_off_val = _res_off,				      \
		.rsc = _rsc_id,						      \
		.rate = _rate,						      \
		.peer = &_platform##_##_name_active,			      \
		.valid_state_mask = _state_mask,			      \
		.hw.init = &(struct clk_init_data){			      \
			.ops = &clk_rpmh_ops,			              \
			.name = #_name,					      \
		},							      \
	};								      \
	static struct clk_rpmh _platform##_##_name_active = {		      \
		.res_name = _res_name,					      \
		.res_en_offset = _res_en_offset,			      \
		.res_on_val = _res_on,					      \
		.res_off_val = _res_off,				      \
		.rsc = _rsc_id,						      \
		.rate = _rate,						      \
		.peer = &_platform##_##_name,				      \
		.valid_state_mask = _state_on_mask,			      \
		.hw.init = &(struct clk_init_data){			      \
			.ops = &clk_rpmh_ops,				      \
			.name = #_name_active,				      \
		},							      \
	}

#define DEFINE_CLK_RPMH_ARC(_platform, _name, _name_active, _res_name, \
			    _res_on, _res_off, _rsc_id, _rate, _state_mask, \
			    _state_on_mask)				\
	__DEFINE_CLK_RPMH(_platform, _name, _name_active, _res_name,	\
			  CLK_RPMH_ARC_EN_OFFSET, _res_on, _res_off, _rsc_id, \
			  _rate, _state_mask, _state_on_mask)

#define DEFINE_CLK_RPMH_VRM(_platform, _name, _name_active, _res_name,	\
			    _rsc_id, _rate, _state_mask, _state_on_mask) \
	__DEFINE_CLK_RPMH(_platform, _name, _name_active, _res_name,	\
			  CLK_RPMH_VRM_EN_OFFSET, CLK_RPMH_VRM_ON_VAL,	\
			  CLK_RPMH_VRM_OFF_VAL, _rsc_id, _rate, _state_mask, \
			  _state_on_mask)

#define DEFINE_CLK_RPMH_BCM(_platform, _name, _res_name, _rsc_id)	\
	static struct clk_rpmh _platform##_##_name = {			\
		.res_name = _res_name,					\
		.valid_state_mask = BIT(RPMH_ACTIVE_ONLY_STATE),	\
		.rsc = _rsc_id,						\
		.hw.init = &(struct clk_init_data){			\
			.ops = &clk_rpmh_bcm_ops,			\
			.name = #_name,					\
		},							\
	}

#define DEFINE_RSC_TYPE(name, mbox_id, awake_state)	\
	static struct rsc_type name = {			\
		.rpmh_client = NULL,			\
		.mbox_name = mbox_id,			\
		.use_awake_state = awake_state,		\
	}

static inline struct clk_rpmh *to_clk_rpmh(struct clk_hw *_hw)
{
	return container_of(_hw, struct clk_rpmh, hw);
}

static inline bool has_state_changed(struct clk_rpmh *c, u32 state)
{
	return ((c->last_sent_aggr_state & BIT(state))
		!= (c->aggr_state & BIT(state)));
}

static int clk_rpmh_send_aggregate_command(struct clk_rpmh *c)
{
	struct tcs_cmd cmd = { 0 };
	int ret = 0;

	cmd.addr = c->res_addr + c->res_en_offset;

	if (has_state_changed(c, RPMH_SLEEP_STATE)) {
		cmd.data = (c->aggr_state >> RPMH_SLEEP_STATE) & 1
			? c->res_on_val : c->res_off_val;
		ret = rpmh_write_async(c->rsc->rpmh_client, RPMH_SLEEP_STATE,
				       &cmd, 1);
		if (ret) {
			pr_err("rpmh_write_async(%s, state=%d) failed (%d)\n",
			       c->res_name, RPMH_SLEEP_STATE, ret);
			return ret;
		}
	}

	if (has_state_changed(c, RPMH_WAKE_ONLY_STATE)) {
		cmd.data = (c->aggr_state >> RPMH_WAKE_ONLY_STATE) & 1
			? c->res_on_val : c->res_off_val;
		ret = rpmh_write_async(c->rsc->rpmh_client,
				       RPMH_WAKE_ONLY_STATE, &cmd, 1);
		if (ret) {
			pr_err("rpmh_write_async(%s, state=%d) failed (%d)\n",
			       c->res_name, RPMH_WAKE_ONLY_STATE, ret);
			return ret;
		}
	}

	if (has_state_changed(c, RPMH_ACTIVE_ONLY_STATE)) {
		cmd.data = (c->aggr_state >> RPMH_ACTIVE_ONLY_STATE) & 1
			? c->res_on_val : c->res_off_val;
		ret = rpmh_write(c->rsc->rpmh_client, RPMH_ACTIVE_ONLY_STATE,
				 &cmd, 1);
		if (ret) {
			pr_err("rpmh_write(%s, state=%d) failed (%d)\n",
			       c->res_name, RPMH_ACTIVE_ONLY_STATE, ret);
			return ret;
		}
	}

	if (has_state_changed(c, RPMH_AWAKE_STATE)) {
		cmd.data = (c->aggr_state >> RPMH_AWAKE_STATE) & 1
			? c->res_on_val : c->res_off_val;
		ret = rpmh_write(c->rsc->rpmh_client, RPMH_AWAKE_STATE,
				 &cmd, 1);
		if (ret) {
			pr_err("rpmh_write(%s, state=%d) failed (%d)\n",
			       c->res_name, RPMH_AWAKE_STATE, ret);
			return ret;
		}
	}

	c->last_sent_aggr_state = c->aggr_state;
	c->peer->last_sent_aggr_state =  c->last_sent_aggr_state;

	return 0;
}

static void clk_rpmh_aggregate_state(struct clk_rpmh *c, bool enable)
{
	/* Update state and aggregate state values based on enable value. */
	c->state = enable ? c->valid_state_mask : 0;
	c->aggr_state = c->state | c->peer->state;
	c->peer->aggr_state = c->aggr_state;
}

static int clk_rpmh_prepare(struct clk_hw *hw)
{
	struct clk_rpmh *c = to_clk_rpmh(hw);
	int ret = 0;

	mutex_lock(&rpmh_clk_lock);

	if (c->state)
		goto out;

	clk_rpmh_aggregate_state(c, true);

	ret = clk_rpmh_send_aggregate_command(c);

	if (ret)
		c->state = 0;

out:
	mutex_unlock(&rpmh_clk_lock);
	return ret;
};

static void clk_rpmh_unprepare(struct clk_hw *hw)
{
	struct clk_rpmh *c = to_clk_rpmh(hw);
	int ret = 0;

	mutex_lock(&rpmh_clk_lock);

	if (!c->state)
		goto out;

	clk_rpmh_aggregate_state(c, false);

	ret = clk_rpmh_send_aggregate_command(c);

	if (ret) {
		c->state = c->valid_state_mask;
		WARN(1, "clk: %s failed to disable\n", c->res_name);
	}

out:
	mutex_unlock(&rpmh_clk_lock);
	return;
};

static unsigned long clk_rpmh_recalc_rate(struct clk_hw *hw,
					  unsigned long parent_rate)
{
	struct clk_rpmh *r = to_clk_rpmh(hw);

	/*
	 * RPMh clocks have a fixed rate. Return static rate set
	 * at init time.
	 */
	return r->rate;
}

static const struct clk_ops clk_rpmh_ops = {
	.prepare	= clk_rpmh_prepare,
	.unprepare	= clk_rpmh_unprepare,
	.recalc_rate	= clk_rpmh_recalc_rate,
};

static int clk_rpmh_bcm_send_cmd(struct clk_rpmh *c, bool enable)
{
	struct tcs_cmd cmd = { 0 };
	u32 cmd_state;
	int ret;

	mutex_lock(&rpmh_clk_lock);

	cmd_state = 0;
	if (enable) {
		cmd_state = 1;
		if (c->aggr_state)
			cmd_state = c->aggr_state;
	}

	cmd_state = min_t(u32, cmd_state, BCM_TCS_CMD_VOTE_MASK);

	if (c->last_sent_aggr_state == cmd_state) {
		mutex_unlock(&rpmh_clk_lock);
		return 0;
	}

	cmd.addr = c->res_addr;
	cmd.data = BCM_TCS_CMD(enable, cmd_state);

	ret = rpmh_write_async(c->rsc->rpmh_client, RPMH_ACTIVE_ONLY_STATE,
				&cmd, 1);
	if (ret) {
		pr_err("set active state of %s failed: (%d)\n",
			c->res_name, ret);
		mutex_unlock(&rpmh_clk_lock);
		return ret;
	}

	c->last_sent_aggr_state = cmd_state;

	mutex_unlock(&rpmh_clk_lock);

	return 0;
}

static int clk_rpmh_bcm_prepare(struct clk_hw *hw)
{
	struct clk_rpmh *c = to_clk_rpmh(hw);

	return clk_rpmh_bcm_send_cmd(c, true);
};

static void clk_rpmh_bcm_unprepare(struct clk_hw *hw)
{
	struct clk_rpmh *c = to_clk_rpmh(hw);

	clk_rpmh_bcm_send_cmd(c, false);
};

static int clk_rpmh_bcm_set_rate(struct clk_hw *hw, unsigned long rate,
				 unsigned long parent_rate)
{
	struct clk_rpmh *c = to_clk_rpmh(hw);

	c->aggr_state = rate / c->unit;
	/*
	 * Since any non-zero value sent to hw would result in enabling the
	 * clock, only send the value if the clock has already been prepared.
	 */
	if (clk_hw_is_prepared(hw))
		clk_rpmh_bcm_send_cmd(c, true);

	return 0;
};

static long clk_rpmh_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *parent_rate)
{
	return rate;
}

static unsigned long clk_rpmh_bcm_recalc_rate(struct clk_hw *hw,
					unsigned long prate)
{
	struct clk_rpmh *c = to_clk_rpmh(hw);

	return c->aggr_state * c->unit;
}

static const struct clk_ops clk_rpmh_bcm_ops = {
	.prepare	= clk_rpmh_bcm_prepare,
	.unprepare	= clk_rpmh_bcm_unprepare,
	.set_rate	= clk_rpmh_bcm_set_rate,
	.round_rate	= clk_rpmh_round_rate,
	.recalc_rate	= clk_rpmh_bcm_recalc_rate,
	.debug_init	= clk_debug_measure_add,
};

/* Use awake state instead of active-only on RSCs that do not have an AMC. */
DEFINE_RSC_TYPE(apps_rsc, "apps", false);
DEFINE_RSC_TYPE(disp_rsc, "display", true);

/* Resource name must match resource id present in cmd-db. */
DEFINE_CLK_RPMH_ARC(sm8150, bi_tcxo, bi_tcxo_ao, "xo.lvl", 0x3, 0x0,
		    &apps_rsc, 19200000, CLK_RPMH_APPS_RSC_STATE_MASK,
		    CLK_RPMH_APPS_RSC_AO_STATE_MASK);
DEFINE_CLK_RPMH_VRM(sm8150, ln_bb_clk2, ln_bb_clk2_ao, "lnbclka2", &apps_rsc,
		    19200000, CLK_RPMH_APPS_RSC_STATE_MASK,
		    CLK_RPMH_APPS_RSC_AO_STATE_MASK);
DEFINE_CLK_RPMH_VRM(sm8150, ln_bb_clk3, ln_bb_clk3_ao, "lnbclka3", &apps_rsc,
		    19200000, CLK_RPMH_APPS_RSC_STATE_MASK,
		    CLK_RPMH_APPS_RSC_AO_STATE_MASK);
DEFINE_CLK_RPMH_VRM(sm8150, rf_clk1, rf_clk1_ao, "rfclka1", &apps_rsc,
		    38400000, CLK_RPMH_APPS_RSC_STATE_MASK,
		    CLK_RPMH_APPS_RSC_AO_STATE_MASK);
DEFINE_CLK_RPMH_VRM(sm8150, rf_clk2, rf_clk2_ao, "rfclka2", &apps_rsc,
		    38400000, CLK_RPMH_APPS_RSC_STATE_MASK,
		    CLK_RPMH_APPS_RSC_AO_STATE_MASK);
DEFINE_CLK_RPMH_VRM(sm8150, rf_clk3, rf_clk3_ao, "rfclka3", &apps_rsc,
		    38400000, CLK_RPMH_APPS_RSC_STATE_MASK,
		    CLK_RPMH_APPS_RSC_AO_STATE_MASK);
DEFINE_CLK_RPMH_VRM(sdmshrike, rf_clk1, rf_clk1_ao, "rfclkd1", &apps_rsc,
		    38400000, CLK_RPMH_APPS_RSC_STATE_MASK,
		    CLK_RPMH_APPS_RSC_AO_STATE_MASK);
DEFINE_CLK_RPMH_VRM(sdmshrike, rf_clk2, rf_clk2_ao, "rfclkd2", &apps_rsc,
		    38400000, CLK_RPMH_APPS_RSC_STATE_MASK,
		    CLK_RPMH_APPS_RSC_AO_STATE_MASK);
DEFINE_CLK_RPMH_VRM(sdmshrike, rf_clk3, rf_clk3_ao, "rfclkd3", &apps_rsc,
		    38400000, CLK_RPMH_APPS_RSC_STATE_MASK,
		    CLK_RPMH_APPS_RSC_AO_STATE_MASK);
DEFINE_CLK_RPMH_VRM(sdmshrike, rf_clk4, rf_clk4_ao, "rfclkd4", &apps_rsc,
		    38400000, CLK_RPMH_APPS_RSC_STATE_MASK,
		    CLK_RPMH_APPS_RSC_AO_STATE_MASK);
DEFINE_CLK_RPMH_BCM(sdxprairie, qpic_clk, "QP0", &apps_rsc);

static struct clk_hw *sm8150_rpmh_clocks[] = {
	[RPMH_CXO_CLK]		= &sm8150_bi_tcxo.hw,
	[RPMH_CXO_CLK_A]	= &sm8150_bi_tcxo_ao.hw,
	[RPMH_LN_BB_CLK2]	= &sm8150_ln_bb_clk2.hw,
	[RPMH_LN_BB_CLK2_A]	= &sm8150_ln_bb_clk2_ao.hw,
	[RPMH_LN_BB_CLK3]	= &sm8150_ln_bb_clk3.hw,
	[RPMH_LN_BB_CLK3_A]	= &sm8150_ln_bb_clk3_ao.hw,
	[RPMH_RF_CLK1]		= &sm8150_rf_clk1.hw,
	[RPMH_RF_CLK1_A]	= &sm8150_rf_clk1_ao.hw,
	[RPMH_RF_CLK2]		= &sm8150_rf_clk2.hw,
	[RPMH_RF_CLK2_A]	= &sm8150_rf_clk2_ao.hw,
	[RPMH_RF_CLK3]		= &sm8150_rf_clk3.hw,
	[RPMH_RF_CLK3_A]	= &sm8150_rf_clk3_ao.hw,
};

static const struct clk_rpmh_desc clk_rpmh_sm8150 = {
	.clks = sm8150_rpmh_clocks,
	.num_clks = ARRAY_SIZE(sm8150_rpmh_clocks),
};

static struct clk_hw *sm6150_rpmh_clocks[] = {
	[RPMH_CXO_CLK]          = &sm8150_bi_tcxo.hw,
	[RPMH_CXO_CLK_A]        = &sm8150_bi_tcxo_ao.hw,
	[RPMH_LN_BB_CLK2]       = &sm8150_ln_bb_clk2.hw,
	[RPMH_LN_BB_CLK2_A]     = &sm8150_ln_bb_clk2_ao.hw,
	[RPMH_LN_BB_CLK3]       = &sm8150_ln_bb_clk3.hw,
	[RPMH_LN_BB_CLK3_A]     = &sm8150_ln_bb_clk3_ao.hw,
	[RPMH_RF_CLK1]          = &sm8150_rf_clk1.hw,
	[RPMH_RF_CLK1_A]        = &sm8150_rf_clk1_ao.hw,
	[RPMH_RF_CLK2]          = &sm8150_rf_clk2.hw,
	[RPMH_RF_CLK2_A]        = &sm8150_rf_clk2_ao.hw,
};

static const struct clk_rpmh_desc clk_rpmh_sm6150 = {
	.clks = sm6150_rpmh_clocks,
	.num_clks = ARRAY_SIZE(sm6150_rpmh_clocks),
};

static struct clk_hw *sdmshrike_rpmh_clocks[] = {
	[RPMH_CXO_CLK]		= &sm8150_bi_tcxo.hw,
	[RPMH_CXO_CLK_A]	= &sm8150_bi_tcxo_ao.hw,
	[RPMH_LN_BB_CLK2]	= &sm8150_ln_bb_clk2.hw,
	[RPMH_LN_BB_CLK2_A]	= &sm8150_ln_bb_clk2_ao.hw,
	[RPMH_LN_BB_CLK3]	= &sm8150_ln_bb_clk3.hw,
	[RPMH_LN_BB_CLK3_A]	= &sm8150_ln_bb_clk3_ao.hw,
	[RPMH_RF_CLK1]		= &sdmshrike_rf_clk1.hw,
	[RPMH_RF_CLK1_A]	= &sdmshrike_rf_clk1_ao.hw,
	[RPMH_RF_CLK2]		= &sdmshrike_rf_clk2.hw,
	[RPMH_RF_CLK2_A]	= &sdmshrike_rf_clk2_ao.hw,
	[RPMH_RF_CLK3]		= &sdmshrike_rf_clk3.hw,
	[RPMH_RF_CLK3_A]	= &sdmshrike_rf_clk3_ao.hw,
	[RPMH_RF_CLK4]		= &sdmshrike_rf_clk4.hw,
	[RPMH_RF_CLK4_A]	= &sdmshrike_rf_clk4_ao.hw,
};

static const struct clk_rpmh_desc clk_rpmh_sdmshrike = {
	.clks = sdmshrike_rpmh_clocks,
	.num_clks = ARRAY_SIZE(sdmshrike_rpmh_clocks),
};

static struct clk_hw *sdxprairie_rpmh_clocks[] = {
	[RPMH_CXO_CLK]          = &sm8150_bi_tcxo.hw,
	[RPMH_CXO_CLK_A]        = &sm8150_bi_tcxo_ao.hw,
	[RPMH_RF_CLK1]          = &sdmshrike_rf_clk1.hw,
	[RPMH_RF_CLK1_A]        = &sdmshrike_rf_clk1_ao.hw,
	[RPMH_RF_CLK2]          = &sdmshrike_rf_clk2.hw,
	[RPMH_RF_CLK2_A]        = &sdmshrike_rf_clk2_ao.hw,
	[RPMH_QPIC_CLK]		= &sdxprairie_qpic_clk.hw,
};

static const struct clk_rpmh_desc clk_rpmh_sdxprairie = {
	.clks = sdxprairie_rpmh_clocks,
	.num_clks = ARRAY_SIZE(sdxprairie_rpmh_clocks),
};

static const struct of_device_id clk_rpmh_match_table[] = {
	{ .compatible = "qcom,rpmh-clk-sm8150", .data = &clk_rpmh_sm8150},
	{ .compatible = "qcom,rpmh-clk-sdmshrike", .data = &clk_rpmh_sdmshrike},
	{ .compatible = "qcom,rpmh-clk-sm6150", .data = &clk_rpmh_sm6150},
	{ .compatible = "qcom,rpmh-clk-sdmmagpie", .data = &clk_rpmh_sm6150},
	{ .compatible = "qcom,rpmh-clk-sdxprairie",
						.data = &clk_rpmh_sdxprairie},
	{ .compatible = "qcom,rpmh-clk-atoll", .data = &clk_rpmh_sm6150},
	{ }
};
MODULE_DEVICE_TABLE(of, clk_rpmh_match_table);

static int clk_rpmh_probe(struct platform_device *pdev)
{
	struct clk **clks;
	struct clk *clk;
	struct rpmh_cc *rcc;
	struct clk_onecell_data *data;
	int ret;
	size_t num_clks, i;
	struct clk_hw **hw_clks;
	struct clk_rpmh *rpmh_clk;
	const struct clk_rpmh_desc *desc;
	struct property *prop;
	const char *mbox_name;
	size_t aux_data_len;
	struct bcm_db db = {0};

	desc = of_device_get_match_data(&pdev->dev);
	if (!desc) {
		ret = -EINVAL;
		goto err;
	}

	ret = cmd_db_ready();
	if (ret) {
		if (ret != -EPROBE_DEFER) {
			dev_err(&pdev->dev, "Command DB not available (%d)\n",
				ret);
			goto err;
		}
		return ret;
	}

	of_property_for_each_string(pdev->dev.of_node, "mbox-names", prop,
				    mbox_name) {
		if (!strcmp(apps_rsc.mbox_name, mbox_name)) {
			apps_rsc.rpmh_client = rpmh_get_byname(pdev, mbox_name);
			if (IS_ERR(apps_rsc.rpmh_client)) {
				ret = PTR_ERR(apps_rsc.rpmh_client);
				if (ret != -EPROBE_DEFER) {
					dev_err(&pdev->dev,
						"failed to request RPMh client for %s (%d)\n",
						mbox_name, ret);
					goto err;
				}
				return ret;
			}
		}

		if (!strcmp(disp_rsc.mbox_name, mbox_name)) {
			disp_rsc.rpmh_client = rpmh_get_byname(pdev, mbox_name);
			if (IS_ERR(disp_rsc.rpmh_client)) {
				ret = PTR_ERR(disp_rsc.rpmh_client);
				if (ret != -EPROBE_DEFER) {
					dev_err(&pdev->dev,
						"failed to request RPMh client for %s (%d)\n",
						mbox_name, ret);
					goto err2;
				}
				return ret;
			}
		}
	}

	if (!apps_rsc.rpmh_client) {
		dev_err(&pdev->dev, "%s mbox is missing\n", apps_rsc.mbox_name);
		ret = -EINVAL;
		goto err2;
	}

	hw_clks = desc->clks;
	num_clks = desc->num_clks;

	rcc = devm_kzalloc(&pdev->dev, sizeof(*rcc) + sizeof(*clks) * num_clks,
			   GFP_KERNEL);
	if (!rcc) {
		ret = -ENOMEM;
		goto err2;
	}

	clks = rcc->clks;
	data = &rcc->data;
	data->clks = clks;
	data->clk_num = num_clks;

	for (i = 0; i < num_clks; i++) {
		if (!hw_clks[i]) {
			clks[i] = ERR_PTR(-ENOENT);
			continue;
		}

		rpmh_clk = to_clk_rpmh(hw_clks[i]);
		rpmh_clk->res_addr = cmd_db_get_addr(rpmh_clk->res_name);
		if (!rpmh_clk->res_addr) {
			dev_err(&pdev->dev, "missing RPMh resource address for %s\n",
				rpmh_clk->res_name);
			ret = -ENODEV;
			goto err2;
		}

		rpmh_clk->unit = 1000ULL;
		aux_data_len = cmd_db_get_aux_data_len(rpmh_clk->res_name);
		if (aux_data_len) {
			cmd_db_get_aux_data(rpmh_clk->res_name, (u8 *)&db,
				sizeof(struct bcm_db));
			if (db.unit)
				rpmh_clk->unit *= le32_to_cpu(db.unit);
		}

		clk = devm_clk_register(&pdev->dev, hw_clks[i]);
		if (IS_ERR(clk)) {
			ret = PTR_ERR(clk);
			goto err2;
		}

		clks[i] = clk;
	}

	ret = of_clk_add_provider(pdev->dev.of_node, of_clk_src_onecell_get,
				  data);
	if (ret)
		goto err2;

	dev_info(&pdev->dev, "Registered RPMh clocks\n");
	return ret;

err2:
	rpmh_release(apps_rsc.rpmh_client);
	if (disp_rsc.rpmh_client)
		rpmh_release(disp_rsc.rpmh_client);
err:
	dev_err(&pdev->dev, "Error registering RPMh Clock driver (%d)\n", ret);
	return ret;
}

static struct platform_driver clk_rpmh_driver = {
	.probe		= clk_rpmh_probe,
	.driver		= {
		.name	= "clk-rpmh",
		.of_match_table = clk_rpmh_match_table,
	},
};

static int __init clk_rpmh_init(void)
{
	return platform_driver_register(&clk_rpmh_driver);
}
subsys_initcall(clk_rpmh_init);

static void __exit clk_rpmh_exit(void)
{
	platform_driver_unregister(&clk_rpmh_driver);
}
module_exit(clk_rpmh_exit);

MODULE_DESCRIPTION("QTI RPMh Clock Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:clk-rpmh");
