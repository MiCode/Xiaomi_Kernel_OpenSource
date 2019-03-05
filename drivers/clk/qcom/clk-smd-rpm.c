/*
 * Copyright (c) 2016, Linaro Limited
 * Copyright (c) 2014, 2016-2019, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/soc/qcom/smd-rpm.h>
#include <soc/qcom/rpm-smd.h>
#include <linux/clk.h>

#include <dt-bindings/clock/qcom,rpmcc.h>
#include <dt-bindings/mfd/qcom-rpm.h>

#include "clk-voter.h"
#include "clk-debug.h"

#define QCOM_RPM_KEY_SOFTWARE_ENABLE			0x6e657773
#define QCOM_RPM_KEY_PIN_CTRL_CLK_BUFFER_ENABLE_KEY	0x62636370
#define QCOM_RPM_SMD_KEY_RATE				0x007a484b
#define QCOM_RPM_SMD_KEY_ENABLE				0x62616e45
#define QCOM_RPM_SMD_KEY_STATE				0x54415453
#define QCOM_RPM_SCALING_ENABLE_ID			0x2

#define __DEFINE_CLK_SMD_RPM(_platform, _name, _active, type, r_id, stat_id,  \
			     key)					      \
	static struct clk_smd_rpm _platform##_##_active;		      \
	static unsigned long _name##_##last_active_set_vote;		      \
	static unsigned long _name##_##last_sleep_set_vote;		      \
	static struct clk_smd_rpm _platform##_##_name = {		      \
		.rpm_res_type = (type),					      \
		.rpm_clk_id = (r_id),					      \
		.rpm_status_id = (stat_id),				      \
		.rpm_key = (key),					      \
		.peer = &_platform##_##_active,				      \
		.rate = INT_MAX,					      \
		.last_active_set_vote = &_name##_##last_active_set_vote,      \
		.last_sleep_set_vote = &_name##_##last_sleep_set_vote,	      \
		.hw.init = &(struct clk_init_data){			      \
			.ops = &clk_smd_rpm_ops,			      \
			.name = #_name,					      \
			.flags = CLK_ENABLE_HAND_OFF,			      \
			.parent_names = (const char *[]){ "xo_board" },       \
			.num_parents = 1,				      \
		},							      \
	};								      \
	static struct clk_smd_rpm _platform##_##_active = {		      \
		.rpm_res_type = (type),					      \
		.rpm_clk_id = (r_id),					      \
		.rpm_status_id = (stat_id),				      \
		.active_only = true,					      \
		.rpm_key = (key),					      \
		.peer = &_platform##_##_name,				      \
		.rate = INT_MAX,					      \
		.last_active_set_vote = &_name##_##last_active_set_vote,      \
		.last_sleep_set_vote = &_name##_##last_sleep_set_vote,	      \
		.hw.init = &(struct clk_init_data){			      \
			.ops = &clk_smd_rpm_ops,			      \
			.name = #_active,				      \
			.flags = CLK_ENABLE_HAND_OFF,			      \
			.parent_names = (const char *[]){ "xo_board" },	      \
			.num_parents = 1,				      \
		},							      \
	}

#define __DEFINE_CLK_SMD_RPM_BRANCH(_platform, _name, _active, type, r_id,    \
				    stat_id, r, key)			      \
	static struct clk_smd_rpm _platform##_##_active;		      \
	static unsigned long _name##_##last_active_set_vote;		      \
	static unsigned long _name##_##last_sleep_set_vote;		      \
	static struct clk_smd_rpm _platform##_##_name = {		      \
		.rpm_res_type = (type),					      \
		.rpm_clk_id = (r_id),					      \
		.rpm_status_id = (stat_id),				      \
		.rpm_key = (key),					      \
		.branch = true,						      \
		.peer = &_platform##_##_active,				      \
		.rate = (r),						      \
		.last_active_set_vote = &_name##_##last_active_set_vote,      \
		.last_sleep_set_vote = &_name##_##last_sleep_set_vote,	      \
		.hw.init = &(struct clk_init_data){			      \
			.ops = &clk_smd_rpm_branch_ops,			      \
			.name = #_name,					      \
			.flags = CLK_ENABLE_HAND_OFF,			      \
			.parent_names = (const char *[]){ "xo_board" },	      \
			.num_parents = 1,				      \
		},							      \
	};								      \
	static struct clk_smd_rpm _platform##_##_active = {		      \
		.rpm_res_type = (type),					      \
		.rpm_clk_id = (r_id),					      \
		.rpm_status_id = (stat_id),				      \
		.active_only = true,					      \
		.rpm_key = (key),					      \
		.branch = true,						      \
		.peer = &_platform##_##_name,				      \
		.rate = (r),						      \
		.last_active_set_vote = &_name##_##last_active_set_vote,      \
		.last_sleep_set_vote = &_name##_##last_sleep_set_vote,	      \
		.hw.init = &(struct clk_init_data){			      \
			.ops = &clk_smd_rpm_branch_ops,			      \
			.name = #_active,				      \
			.flags = CLK_ENABLE_HAND_OFF,			      \
			.parent_names = (const char *[]){ "xo_board" },	      \
			.num_parents = 1,				      \
		},							      \
	}

#define DEFINE_CLK_SMD_RPM(_platform, _name, _active, type, r_id)	      \
		__DEFINE_CLK_SMD_RPM(_platform, _name, _active, type, r_id,   \
		0, QCOM_RPM_SMD_KEY_RATE)

#define DEFINE_CLK_SMD_RPM_BRANCH(_platform, _name, _active, type, r_id, r)   \
		__DEFINE_CLK_SMD_RPM_BRANCH(_platform, _name, _active, type,  \
		r_id, 0, r, QCOM_RPM_SMD_KEY_ENABLE)

#define DEFINE_CLK_SMD_RPM_QDSS(_platform, _name, _active, type, r_id)	      \
		__DEFINE_CLK_SMD_RPM(_platform, _name, _active, type, r_id,   \
		0, QCOM_RPM_SMD_KEY_STATE)

#define DEFINE_CLK_SMD_RPM_XO_BUFFER(_platform, _name, _active, r_id)	      \
		__DEFINE_CLK_SMD_RPM_BRANCH(_platform, _name, _active,	      \
		QCOM_SMD_RPM_CLK_BUF_A, r_id, 0, 1000,			      \
		QCOM_RPM_KEY_SOFTWARE_ENABLE)

#define DEFINE_CLK_SMD_RPM_XO_BUFFER_PINCTRL(_platform, _name, _active, r_id) \
		__DEFINE_CLK_SMD_RPM_BRANCH(_platform, _name, _active,	      \
		QCOM_SMD_RPM_CLK_BUF_A, r_id, 0, 1000,			      \
		QCOM_RPM_KEY_PIN_CTRL_CLK_BUFFER_ENABLE_KEY)

#define to_clk_smd_rpm(_hw) container_of(_hw, struct clk_smd_rpm, hw)

struct clk_smd_rpm {
	const int rpm_res_type;
	const int rpm_key;
	const int rpm_clk_id;
	const int rpm_status_id;
	const bool active_only;
	bool enabled;
	bool branch;
	struct clk_smd_rpm *peer;
	struct clk_hw hw;
	unsigned long rate;
	unsigned long *last_active_set_vote;
	unsigned long *last_sleep_set_vote;
};

struct clk_smd_rpm_req {
	__le32 key;
	__le32 nbytes;
	__le32 value;
};

struct rpm_cc {
	struct qcom_rpm *rpm;
	struct clk_onecell_data data;
	struct clk *clks[];
};

struct rpm_smd_clk_desc {
	struct clk_hw **clks;
	size_t num_rpm_clks;
	size_t num_clks;
};

static DEFINE_MUTEX(rpm_smd_clk_lock);

static int clk_smd_rpm_prepare(struct clk_hw *hw);

static int clk_smd_rpm_handoff(struct clk_hw *hw)
{
	return clk_smd_rpm_prepare(hw);
}

static int clk_smd_rpm_set_rate_active(struct clk_smd_rpm *r,
					uint32_t rate)
{
	int ret = 0;
	struct msm_rpm_kvp req = {
		.key = cpu_to_le32(r->rpm_key),
		.data = (void *)&rate,
		.length = sizeof(rate),
	};

	if (*r->last_active_set_vote == rate)
		return ret;

	ret = msm_rpm_send_message(QCOM_SMD_RPM_ACTIVE_STATE, r->rpm_res_type,
			r->rpm_clk_id, &req, 1);
	if (ret)
		return ret;

	*r->last_active_set_vote = rate;

	return ret;
}

static int clk_smd_rpm_set_rate_sleep(struct clk_smd_rpm *r,
					uint32_t rate)
{
	int ret = 0;
	struct msm_rpm_kvp req = {
		.key = cpu_to_le32(r->rpm_key),
		.data = (void *)&rate,
		.length = sizeof(rate),
	};

	if (*r->last_sleep_set_vote == rate)
		return ret;

	ret = msm_rpm_send_message(QCOM_SMD_RPM_SLEEP_STATE, r->rpm_res_type,
			r->rpm_clk_id, &req, 1);
	if (ret)
		return ret;

	*r->last_sleep_set_vote = rate;

	return ret;
}

static void to_active_sleep(struct clk_smd_rpm *r, unsigned long rate,
			    unsigned long *active, unsigned long *sleep)
{
	/* Convert the rate (hz) to khz */
	*active = DIV_ROUND_UP(rate, 1000);

	/*
	 * Active-only clocks don't care what the rate is during sleep. So,
	 * they vote for zero.
	 */
	if (r->active_only)
		*sleep = 0;
	else
		*sleep = *active;
}

static int clk_smd_rpm_prepare(struct clk_hw *hw)
{
	struct clk_smd_rpm *r = to_clk_smd_rpm(hw);
	struct clk_smd_rpm *peer = r->peer;
	unsigned long this_rate = 0, this_sleep_rate = 0;
	unsigned long peer_rate = 0, peer_sleep_rate = 0;
	uint32_t active_rate, sleep_rate;
	int ret = 0;

	mutex_lock(&rpm_smd_clk_lock);

	to_active_sleep(r, r->rate, &this_rate, &this_sleep_rate);

	/* Don't send requests to the RPM if the rate has not been set. */
	if (this_rate == 0)
		goto out;

	/* Take peer clock's rate into account only if it's enabled. */
	if (peer->enabled)
		to_active_sleep(peer, peer->rate,
				&peer_rate, &peer_sleep_rate);

	active_rate = max(this_rate, peer_rate);

	if (r->branch)
		active_rate = !!active_rate;

	ret = clk_smd_rpm_set_rate_active(r, active_rate);
	if (ret)
		goto out;

	sleep_rate = max(this_sleep_rate, peer_sleep_rate);
	if (r->branch)
		sleep_rate = !!sleep_rate;

	ret = clk_smd_rpm_set_rate_sleep(r, sleep_rate);
	if (ret)
		/* Undo the active set vote and restore it */
		ret = clk_smd_rpm_set_rate_active(r, peer_rate);

out:
	if (!ret)
		r->enabled = true;

	mutex_unlock(&rpm_smd_clk_lock);

	return ret;
}

static void clk_smd_rpm_unprepare(struct clk_hw *hw)
{
	struct clk_smd_rpm *r = to_clk_smd_rpm(hw);
	struct clk_smd_rpm *peer = r->peer;
	unsigned long peer_rate = 0, peer_sleep_rate = 0;
	uint32_t active_rate, sleep_rate;
	int ret;

	mutex_lock(&rpm_smd_clk_lock);

	if (!r->rate)
		goto enable;

	/* Take peer clock's rate into account only if it's enabled. */
	if (peer->enabled)
		to_active_sleep(peer, peer->rate, &peer_rate,
				&peer_sleep_rate);

	active_rate = r->branch ? !!peer_rate : peer_rate;
	ret = clk_smd_rpm_set_rate_active(r, active_rate);
	if (ret)
		goto out;

	sleep_rate = r->branch ? !!peer_sleep_rate : peer_sleep_rate;
	ret = clk_smd_rpm_set_rate_sleep(r, sleep_rate);
	if (ret)
		goto out;

enable:
	r->enabled = false;

out:
	mutex_unlock(&rpm_smd_clk_lock);
}

static int clk_smd_rpm_set_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate)
{
	struct clk_smd_rpm *r = to_clk_smd_rpm(hw);
	struct clk_smd_rpm *peer = r->peer;
	uint32_t active_rate, sleep_rate;
	unsigned long this_rate = 0, this_sleep_rate = 0;
	unsigned long peer_rate = 0, peer_sleep_rate = 0;
	int ret = 0;

	mutex_lock(&rpm_smd_clk_lock);

	if (!r->enabled)
		goto out;

	to_active_sleep(r, rate, &this_rate, &this_sleep_rate);

	/* Take peer clock's rate into account only if it's enabled. */
	if (peer->enabled)
		to_active_sleep(peer, peer->rate,
				&peer_rate, &peer_sleep_rate);

	active_rate = max(this_rate, peer_rate);
	ret = clk_smd_rpm_set_rate_active(r, active_rate);
	if (ret)
		goto out;

	sleep_rate = max(this_sleep_rate, peer_sleep_rate);
	ret = clk_smd_rpm_set_rate_sleep(r, sleep_rate);
	if (ret)
		goto out;

	r->rate = rate;

out:
	mutex_unlock(&rpm_smd_clk_lock);

	return ret;
}

static long clk_smd_rpm_round_rate(struct clk_hw *hw, unsigned long rate,
				   unsigned long *parent_rate)
{
	/*
	 * RPM handles rate rounding and we don't have a way to
	 * know what the rate will be, so just return whatever
	 * rate is requested.
	 */
	return rate;
}

static unsigned long clk_smd_rpm_recalc_rate(struct clk_hw *hw,
					     unsigned long parent_rate)
{
	struct clk_smd_rpm *r = to_clk_smd_rpm(hw);

	/*
	 * RPM handles rate rounding and we don't have a way to
	 * know what the rate will be, so just return whatever
	 * rate was set.
	 */
	return r->rate;
}

static int clk_smd_rpm_enable_scaling(void)
{
	int ret = 0;
	uint32_t value = cpu_to_le32(1);
	struct msm_rpm_kvp req = {
		.key = cpu_to_le32(QCOM_RPM_SMD_KEY_ENABLE),
		.data = (void *)&value,
		.length = sizeof(value),
	};

	ret = msm_rpm_send_message(QCOM_SMD_RPM_SLEEP_STATE,
			QCOM_SMD_RPM_MISC_CLK,
			QCOM_RPM_SCALING_ENABLE_ID, &req, 1);
	if (ret) {
		pr_err("RPM clock scaling (sleep set) not enabled!\n");
		return ret;
	}

	ret = msm_rpm_send_message(QCOM_SMD_RPM_ACTIVE_STATE,
			QCOM_SMD_RPM_MISC_CLK,
			QCOM_RPM_SCALING_ENABLE_ID, &req, 1);
	if (ret) {
		pr_err("RPM clock scaling (active set) not enabled!\n");
		return ret;
	}

	pr_debug("%s: RPM clock scaling is enabled\n", __func__);
	return ret;
}

static int clk_vote_bimc(struct clk_hw *hw, uint32_t rate)
{
	int ret = 0;
	struct clk_smd_rpm *r = to_clk_smd_rpm(hw);
	struct msm_rpm_kvp req = {
		.key = r->rpm_key,
		.data = (void *)&rate,
		.length = sizeof(rate),
	};

	ret = msm_rpm_send_message(QCOM_SMD_RPM_ACTIVE_STATE,
		r->rpm_res_type, r->rpm_clk_id, &req, 1);
	if (ret < 0) {
		if (ret != -EPROBE_DEFER)
			WARN(1, "BIMC vote not sent!\n");
		return ret;
	}

	return ret;
}

static int clk_smd_rpm_is_enabled(struct clk_hw *hw)
{
	struct clk_smd_rpm *r = to_clk_smd_rpm(hw);

	return r->enabled;
}

static const struct clk_ops clk_smd_rpm_ops = {
	.prepare	= clk_smd_rpm_prepare,
	.unprepare	= clk_smd_rpm_unprepare,
	.set_rate	= clk_smd_rpm_set_rate,
	.round_rate	= clk_smd_rpm_round_rate,
	.recalc_rate	= clk_smd_rpm_recalc_rate,
	.is_enabled	= clk_smd_rpm_is_enabled,
	.debug_init	= clk_debug_measure_add,
};

static const struct clk_ops clk_smd_rpm_branch_ops = {
	.prepare	= clk_smd_rpm_prepare,
	.unprepare	= clk_smd_rpm_unprepare,
	.round_rate	= clk_smd_rpm_round_rate,
	.recalc_rate	= clk_smd_rpm_recalc_rate,
	.is_enabled	= clk_smd_rpm_is_enabled,
	.debug_init	= clk_debug_measure_add,
};

/* msm8916 */
DEFINE_CLK_SMD_RPM(msm8916, pcnoc_clk, pcnoc_a_clk, QCOM_SMD_RPM_BUS_CLK, 0);
DEFINE_CLK_SMD_RPM(msm8916, snoc_clk, snoc_a_clk, QCOM_SMD_RPM_BUS_CLK, 1);
DEFINE_CLK_SMD_RPM(msm8916, bimc_clk, bimc_a_clk, QCOM_SMD_RPM_MEM_CLK, 0);
DEFINE_CLK_SMD_RPM_QDSS(msm8916, qdss_clk, qdss_a_clk, QCOM_SMD_RPM_MISC_CLK, 1);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msm8916, bb_clk1, bb_clk1_a, 1);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msm8916, bb_clk2, bb_clk2_a, 2);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msm8916, rf_clk1, rf_clk1_a, 4);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msm8916, rf_clk2, rf_clk2_a, 5);
DEFINE_CLK_SMD_RPM_XO_BUFFER_PINCTRL(msm8916, bb_clk1_pin, bb_clk1_a_pin, 1);
DEFINE_CLK_SMD_RPM_XO_BUFFER_PINCTRL(msm8916, bb_clk2_pin, bb_clk2_a_pin, 2);
DEFINE_CLK_SMD_RPM_XO_BUFFER_PINCTRL(msm8916, rf_clk1_pin, rf_clk1_a_pin, 4);
DEFINE_CLK_SMD_RPM_XO_BUFFER_PINCTRL(msm8916, rf_clk2_pin, rf_clk2_a_pin, 5);

static struct clk_hw *msm8916_clks[] = {
	[RPM_SMD_PCNOC_CLK]		= &msm8916_pcnoc_clk.hw,
	[RPM_SMD_PCNOC_A_CLK]		= &msm8916_pcnoc_a_clk.hw,
	[RPM_SMD_SNOC_CLK]		= &msm8916_snoc_clk.hw,
	[RPM_SMD_SNOC_A_CLK]		= &msm8916_snoc_a_clk.hw,
	[RPM_SMD_BIMC_CLK]		= &msm8916_bimc_clk.hw,
	[RPM_SMD_BIMC_A_CLK]		= &msm8916_bimc_a_clk.hw,
	[RPM_SMD_QDSS_CLK]		= &msm8916_qdss_clk.hw,
	[RPM_SMD_QDSS_A_CLK]		= &msm8916_qdss_a_clk.hw,
	[RPM_SMD_BB_CLK1]		= &msm8916_bb_clk1.hw,
	[RPM_SMD_BB_CLK1_A]		= &msm8916_bb_clk1_a.hw,
	[RPM_SMD_BB_CLK2]		= &msm8916_bb_clk2.hw,
	[RPM_SMD_BB_CLK2_A]		= &msm8916_bb_clk2_a.hw,
	[RPM_SMD_RF_CLK1]		= &msm8916_rf_clk1.hw,
	[RPM_SMD_RF_CLK1_A]		= &msm8916_rf_clk1_a.hw,
	[RPM_SMD_RF_CLK2]		= &msm8916_rf_clk2.hw,
	[RPM_SMD_RF_CLK2_A]		= &msm8916_rf_clk2_a.hw,
	[RPM_SMD_BB_CLK1_PIN]		= &msm8916_bb_clk1_pin.hw,
	[RPM_SMD_BB_CLK1_A_PIN]		= &msm8916_bb_clk1_a_pin.hw,
	[RPM_SMD_BB_CLK2_PIN]		= &msm8916_bb_clk2_pin.hw,
	[RPM_SMD_BB_CLK2_A_PIN]		= &msm8916_bb_clk2_a_pin.hw,
	[RPM_SMD_RF_CLK1_PIN]		= &msm8916_rf_clk1_pin.hw,
	[RPM_SMD_RF_CLK1_A_PIN]		= &msm8916_rf_clk1_a_pin.hw,
	[RPM_SMD_RF_CLK2_PIN]		= &msm8916_rf_clk2_pin.hw,
	[RPM_SMD_RF_CLK2_A_PIN]		= &msm8916_rf_clk2_a_pin.hw,
};

static const struct rpm_smd_clk_desc rpm_clk_msm8916 = {
	.clks = msm8916_clks,
	.num_rpm_clks = RPM_SMD_RF_CLK2_A_PIN,
	.num_clks = ARRAY_SIZE(msm8916_clks),
};

/* msm8974 */
DEFINE_CLK_SMD_RPM(msm8974, pnoc_clk, pnoc_a_clk, QCOM_SMD_RPM_BUS_CLK, 0);
DEFINE_CLK_SMD_RPM(msm8974, snoc_clk, snoc_a_clk, QCOM_SMD_RPM_BUS_CLK, 1);
DEFINE_CLK_SMD_RPM(msm8974, cnoc_clk, cnoc_a_clk, QCOM_SMD_RPM_BUS_CLK, 2);
DEFINE_CLK_SMD_RPM(msm8974, mmssnoc_ahb_clk, mmssnoc_ahb_a_clk, QCOM_SMD_RPM_BUS_CLK, 3);
DEFINE_CLK_SMD_RPM(msm8974, bimc_clk, bimc_a_clk, QCOM_SMD_RPM_MEM_CLK, 0);
DEFINE_CLK_SMD_RPM(msm8974, gfx3d_clk_src, gfx3d_a_clk_src, QCOM_SMD_RPM_MEM_CLK, 1);
DEFINE_CLK_SMD_RPM(msm8974, ocmemgx_clk, ocmemgx_a_clk, QCOM_SMD_RPM_MEM_CLK, 2);
DEFINE_CLK_SMD_RPM_QDSS(msm8974, qdss_clk, qdss_a_clk, QCOM_SMD_RPM_MISC_CLK, 1);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msm8974, cxo_d0, cxo_d0_a, 1);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msm8974, cxo_d1, cxo_d1_a, 2);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msm8974, cxo_a0, cxo_a0_a, 4);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msm8974, cxo_a1, cxo_a1_a, 5);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msm8974, cxo_a2, cxo_a2_a, 6);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msm8974, diff_clk, diff_a_clk, 7);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msm8974, div_clk1, div_a_clk1, 11);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msm8974, div_clk2, div_a_clk2, 12);
DEFINE_CLK_SMD_RPM_XO_BUFFER_PINCTRL(msm8974, cxo_d0_pin, cxo_d0_a_pin, 1);
DEFINE_CLK_SMD_RPM_XO_BUFFER_PINCTRL(msm8974, cxo_d1_pin, cxo_d1_a_pin, 2);
DEFINE_CLK_SMD_RPM_XO_BUFFER_PINCTRL(msm8974, cxo_a0_pin, cxo_a0_a_pin, 4);
DEFINE_CLK_SMD_RPM_XO_BUFFER_PINCTRL(msm8974, cxo_a1_pin, cxo_a1_a_pin, 5);
DEFINE_CLK_SMD_RPM_XO_BUFFER_PINCTRL(msm8974, cxo_a2_pin, cxo_a2_a_pin, 6);

static struct clk_hw *msm8974_clks[] = {
	[RPM_SMD_PNOC_CLK]		= &msm8974_pnoc_clk.hw,
	[RPM_SMD_PNOC_A_CLK]		= &msm8974_pnoc_a_clk.hw,
	[RPM_SMD_SNOC_CLK]		= &msm8974_snoc_clk.hw,
	[RPM_SMD_SNOC_A_CLK]		= &msm8974_snoc_a_clk.hw,
	[RPM_SMD_CNOC_CLK]		= &msm8974_cnoc_clk.hw,
	[RPM_SMD_CNOC_A_CLK]		= &msm8974_cnoc_a_clk.hw,
	[RPM_SMD_MMSSNOC_AHB_CLK]	= &msm8974_mmssnoc_ahb_clk.hw,
	[RPM_SMD_MMSSNOC_AHB_A_CLK]	= &msm8974_mmssnoc_ahb_a_clk.hw,
	[RPM_SMD_BIMC_CLK]		= &msm8974_bimc_clk.hw,
	[RPM_SMD_BIMC_A_CLK]		= &msm8974_bimc_a_clk.hw,
	[RPM_SMD_OCMEMGX_CLK]		= &msm8974_ocmemgx_clk.hw,
	[RPM_SMD_OCMEMGX_A_CLK]		= &msm8974_ocmemgx_a_clk.hw,
	[RPM_SMD_QDSS_CLK]		= &msm8974_qdss_clk.hw,
	[RPM_SMD_QDSS_A_CLK]		= &msm8974_qdss_a_clk.hw,
	[RPM_SMD_CXO_D0]		= &msm8974_cxo_d0.hw,
	[RPM_SMD_CXO_D0_A]		= &msm8974_cxo_d0_a.hw,
	[RPM_SMD_CXO_D1]		= &msm8974_cxo_d1.hw,
	[RPM_SMD_CXO_D1_A]		= &msm8974_cxo_d1_a.hw,
	[RPM_SMD_CXO_A0]		= &msm8974_cxo_a0.hw,
	[RPM_SMD_CXO_A0_A]		= &msm8974_cxo_a0_a.hw,
	[RPM_SMD_CXO_A1]		= &msm8974_cxo_a1.hw,
	[RPM_SMD_CXO_A1_A]		= &msm8974_cxo_a1_a.hw,
	[RPM_SMD_CXO_A2]		= &msm8974_cxo_a2.hw,
	[RPM_SMD_CXO_A2_A]		= &msm8974_cxo_a2_a.hw,
	[RPM_SMD_DIFF_CLK]		= &msm8974_diff_clk.hw,
	[RPM_SMD_DIFF_A_CLK]		= &msm8974_diff_a_clk.hw,
	[RPM_SMD_DIV_CLK1]		= &msm8974_div_clk1.hw,
	[RPM_SMD_DIV_A_CLK1]		= &msm8974_div_a_clk1.hw,
	[RPM_SMD_DIV_CLK2]		= &msm8974_div_clk2.hw,
	[RPM_SMD_DIV_A_CLK2]		= &msm8974_div_a_clk2.hw,
	[RPM_SMD_CXO_D0_PIN]		= &msm8974_cxo_d0_pin.hw,
	[RPM_SMD_CXO_D0_A_PIN]		= &msm8974_cxo_d0_a_pin.hw,
	[RPM_SMD_CXO_D1_PIN]		= &msm8974_cxo_d1_pin.hw,
	[RPM_SMD_CXO_D1_A_PIN]		= &msm8974_cxo_d1_a_pin.hw,
	[RPM_SMD_CXO_A0_PIN]		= &msm8974_cxo_a0_pin.hw,
	[RPM_SMD_CXO_A0_A_PIN]		= &msm8974_cxo_a0_a_pin.hw,
	[RPM_SMD_CXO_A1_PIN]		= &msm8974_cxo_a1_pin.hw,
	[RPM_SMD_CXO_A1_A_PIN]		= &msm8974_cxo_a1_a_pin.hw,
	[RPM_SMD_CXO_A2_PIN]		= &msm8974_cxo_a2_pin.hw,
	[RPM_SMD_CXO_A2_A_PIN]		= &msm8974_cxo_a2_a_pin.hw,
};

static const struct rpm_smd_clk_desc rpm_clk_msm8974 = {
	.clks = msm8974_clks,
	.num_rpm_clks = RPM_SMD_CXO_A2_A_PIN,
	.num_clks = ARRAY_SIZE(msm8974_clks),
};

/* QCS405 */
DEFINE_CLK_SMD_RPM_BRANCH(qcs405, cxo, cxo_a, QCOM_SMD_RPM_MISC_CLK, 0,
								19200000);
DEFINE_CLK_SMD_RPM(qcs405, pnoc_clk, pnoc_a_clk, QCOM_SMD_RPM_BUS_CLK, 0);
DEFINE_CLK_SMD_RPM(qcs405, bimc_clk, bimc_a_clk, QCOM_SMD_RPM_MEM_CLK, 0);
DEFINE_CLK_SMD_RPM(qcs405, snoc_clk, snoc_a_clk, QCOM_SMD_RPM_BUS_CLK, 1);
DEFINE_CLK_SMD_RPM_QDSS(qcs405, qdss_clk, qdss_a_clk,
						QCOM_SMD_RPM_MISC_CLK, 1);
DEFINE_CLK_SMD_RPM(qcs405, qpic_clk, qpic_a_clk, QCOM_SMD_RPM_QPIC_CLK, 0);
DEFINE_CLK_SMD_RPM(qcs405, ce1_clk, ce1_a_clk, QCOM_SMD_RPM_CE_CLK, 0);
DEFINE_CLK_SMD_RPM(qcs405, bimc_gpu_clk, bimc_gpu_a_clk,
						QCOM_SMD_RPM_MEM_CLK, 2);
/* SMD_XO_BUFFER */
DEFINE_CLK_SMD_RPM_XO_BUFFER(qcs405, ln_bb_clk, ln_bb_clk_a, 8);
DEFINE_CLK_SMD_RPM_XO_BUFFER(qcs405, rf_clk1, rf_clk1_a, 4);
DEFINE_CLK_SMD_RPM_XO_BUFFER_PINCTRL(qcs405, ln_bb_clk_pin, ln_bb_clk_a_pin, 8);
DEFINE_CLK_SMD_RPM_XO_BUFFER_PINCTRL(qcs405, rf_clk1_pin, rf_clk1_a_pin, 4);

/* Voter clocks */
static DEFINE_CLK_VOTER(pnoc_msmbus_clk, pnoc_clk, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_msmbus_clk, snoc_clk, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_msmbus_clk, bimc_clk, LONG_MAX);

static DEFINE_CLK_VOTER(pnoc_msmbus_a_clk, pnoc_a_clk, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_msmbus_a_clk, snoc_a_clk, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_msmbus_a_clk, bimc_a_clk, LONG_MAX);
static DEFINE_CLK_VOTER(pnoc_keepalive_a_clk, pnoc_a_clk, LONG_MAX);

static DEFINE_CLK_VOTER(pnoc_usb_a_clk, pnoc_a_clk, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_usb_a_clk, snoc_a_clk, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_usb_a_clk, bimc_a_clk, LONG_MAX);

static DEFINE_CLK_VOTER(pnoc_usb_clk, pnoc_clk, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_usb_clk, snoc_clk, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_usb_clk, bimc_clk, LONG_MAX);

static DEFINE_CLK_VOTER(snoc_wcnss_a_clk, snoc_a_clk, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_wcnss_a_clk, bimc_a_clk, LONG_MAX);

static DEFINE_CLK_VOTER(mcd_ce1_clk, ce1_clk, 85710000);
static DEFINE_CLK_VOTER(qcedev_ce1_clk, ce1_clk, 85710000);
static DEFINE_CLK_VOTER(qcrypto_ce1_clk, ce1_clk, 85710000);
static DEFINE_CLK_VOTER(qseecom_ce1_clk, ce1_clk, 85710000);
static DEFINE_CLK_VOTER(scm_ce1_clk, ce1_clk, 85710000);

/* Branch Voter clocks */
static DEFINE_CLK_BRANCH_VOTER(cxo_otg_clk, cxo);
static DEFINE_CLK_BRANCH_VOTER(cxo_lpm_clk, cxo);
static DEFINE_CLK_BRANCH_VOTER(cxo_pil_pronto_clk, cxo);
static DEFINE_CLK_BRANCH_VOTER(cxo_pil_mss_clk, cxo);
static DEFINE_CLK_BRANCH_VOTER(cxo_wlan_clk, cxo);
static DEFINE_CLK_BRANCH_VOTER(cxo_pil_lpass_clk, cxo);
static DEFINE_CLK_BRANCH_VOTER(cxo_pil_cdsp_clk, cxo);

static struct clk_hw *qcs405_clks[] = {
	[RPM_SMD_XO_CLK_SRC]		= &qcs405_cxo.hw,
	[RPM_SMD_XO_A_CLK_SRC]		= &qcs405_cxo_a.hw,
	[RPM_SMD_SNOC_CLK]		= &qcs405_snoc_clk.hw,
	[RPM_SMD_SNOC_A_CLK]		= &qcs405_snoc_a_clk.hw,
	[RPM_SMD_BIMC_CLK]		= &qcs405_bimc_clk.hw,
	[RPM_SMD_BIMC_A_CLK]		= &qcs405_bimc_a_clk.hw,
	[RPM_SMD_QDSS_CLK]		= &qcs405_qdss_clk.hw,
	[RPM_SMD_QDSS_A_CLK]		= &qcs405_qdss_a_clk.hw,
	[RPM_SMD_RF_CLK1]		= &qcs405_rf_clk1.hw,
	[RPM_SMD_RF_CLK1_A]		= &qcs405_rf_clk1_a.hw,
	[RPM_SMD_RF_CLK1_PIN]		= &qcs405_rf_clk1_pin.hw,
	[RPM_SMD_RF_CLK1_A_PIN]		= &qcs405_rf_clk1_a_pin.hw,
	[RPM_SMD_LN_BB_CLK]		= &qcs405_ln_bb_clk.hw,
	[RPM_SMD_LN_BB_CLK_A]		= &qcs405_ln_bb_clk_a.hw,
	[RPM_SMD_LN_BB_CLK_PIN]		= &qcs405_ln_bb_clk_pin.hw,
	[RPM_SMD_LN_BB_CLK_A_PIN]	= &qcs405_ln_bb_clk_a_pin.hw,
	[RPM_SMD_PNOC_CLK]		= &qcs405_pnoc_clk.hw,
	[RPM_SMD_PNOC_A_CLK]		= &qcs405_pnoc_a_clk.hw,
	[RPM_SMD_CE1_CLK]		= &qcs405_ce1_clk.hw,
	[RPM_SMD_CE1_A_CLK]		= &qcs405_ce1_a_clk.hw,
	[RPM_SMD_QPIC_CLK]		= &qcs405_qpic_clk.hw,
	[RPM_SMD_QPIC_A_CLK]		= &qcs405_qpic_a_clk.hw,
	[RPM_SMD_BIMC_GPU_CLK]		= &qcs405_bimc_gpu_clk.hw,
	[RPM_SMD_BIMC_GPU_A_CLK]	= &qcs405_bimc_gpu_a_clk.hw,
	[PNOC_MSMBUS_CLK]		= &pnoc_msmbus_clk.hw,
	[PNOC_MSMBUS_A_CLK]		= &pnoc_msmbus_a_clk.hw,
	[PNOC_KEEPALIVE_A_CLK]		= &pnoc_keepalive_a_clk.hw,
	[SNOC_MSMBUS_CLK]		= &snoc_msmbus_clk.hw,
	[SNOC_MSMBUS_A_CLK]		= &snoc_msmbus_a_clk.hw,
	[BIMC_MSMBUS_CLK]		= &bimc_msmbus_clk.hw,
	[BIMC_MSMBUS_A_CLK]		= &bimc_msmbus_a_clk.hw,
	[PNOC_USB_CLK]			= &pnoc_usb_clk.hw,
	[PNOC_USB_A_CLK]		= &pnoc_usb_a_clk.hw,
	[SNOC_USB_CLK]			= &snoc_usb_clk.hw,
	[SNOC_USB_A_CLK]		= &snoc_usb_a_clk.hw,
	[BIMC_USB_CLK]			= &bimc_usb_clk.hw,
	[BIMC_USB_A_CLK]		= &bimc_usb_a_clk.hw,
	[SNOC_WCNSS_A_CLK]		= &snoc_wcnss_a_clk.hw,
	[BIMC_WCNSS_A_CLK]		= &bimc_wcnss_a_clk.hw,
	[MCD_CE1_CLK]			= &mcd_ce1_clk.hw,
	[QCEDEV_CE1_CLK]		= &qcedev_ce1_clk.hw,
	[QCRYPTO_CE1_CLK]		= &qcrypto_ce1_clk.hw,
	[QSEECOM_CE1_CLK]		= &qseecom_ce1_clk.hw,
	[SCM_CE1_CLK]			= &scm_ce1_clk.hw,
	[CXO_SMD_OTG_CLK]		= &cxo_otg_clk.hw,
	[CXO_SMD_LPM_CLK]		= &cxo_lpm_clk.hw,
	[CXO_SMD_PIL_PRONTO_CLK]	= &cxo_pil_pronto_clk.hw,
	[CXO_SMD_PIL_MSS_CLK]		= &cxo_pil_mss_clk.hw,
	[CXO_SMD_WLAN_CLK]		= &cxo_wlan_clk.hw,
	[CXO_SMD_PIL_LPASS_CLK]		= &cxo_pil_lpass_clk.hw,
	[CXO_SMD_PIL_CDSP_CLK]		= &cxo_pil_cdsp_clk.hw,
};

static const struct rpm_smd_clk_desc rpm_clk_qcs405 = {
	.clks = qcs405_clks,
	.num_rpm_clks = RPM_SMD_LN_BB_CLK_A_PIN,
	.num_clks = ARRAY_SIZE(qcs405_clks),
};

/* Trinket */
DEFINE_CLK_SMD_RPM_BRANCH(trinket, bi_tcxo, bi_tcxo_ao,
					QCOM_SMD_RPM_MISC_CLK, 0, 19200000);
DEFINE_CLK_SMD_RPM(trinket, cnoc_clk, cnoc_a_clk, QCOM_SMD_RPM_BUS_CLK, 1);
DEFINE_CLK_SMD_RPM(trinket, bimc_clk, bimc_a_clk, QCOM_SMD_RPM_MEM_CLK, 0);
DEFINE_CLK_SMD_RPM(trinket, snoc_clk, snoc_a_clk, QCOM_SMD_RPM_BUS_CLK, 2);
DEFINE_CLK_SMD_RPM_BRANCH(trinket, qdss_clk, qdss_a_clk,
					QCOM_SMD_RPM_MISC_CLK, 1, 19200000);
DEFINE_CLK_SMD_RPM(trinket, ce1_clk, ce1_a_clk, QCOM_SMD_RPM_CE_CLK, 0);
DEFINE_CLK_SMD_RPM(trinket, ipa_clk, ipa_a_clk, QCOM_SMD_RPM_IPA_CLK, 0);
DEFINE_CLK_SMD_RPM(trinket, qup_clk, qup_a_clk, QCOM_SMD_RPM_QUP_CLK, 0);
DEFINE_CLK_SMD_RPM(trinket, mmnrt_clk, mmnrt_a_clk, QCOM_SMD_RPM_MMXI_CLK, 0);
DEFINE_CLK_SMD_RPM(trinket, mmrt_clk, mmrt_a_clk, QCOM_SMD_RPM_MMXI_CLK, 1);
DEFINE_CLK_SMD_RPM(trinket, snoc_periph_clk, snoc_periph_a_clk,
						QCOM_SMD_RPM_BUS_CLK, 0);
DEFINE_CLK_SMD_RPM(trinket, snoc_lpass_clk, snoc_lpass_a_clk,
						QCOM_SMD_RPM_BUS_CLK, 5);

/* SMD_XO_BUFFER */
DEFINE_CLK_SMD_RPM_XO_BUFFER(trinket, ln_bb_clk1, ln_bb_clk1_a, 1);
DEFINE_CLK_SMD_RPM_XO_BUFFER(trinket, ln_bb_clk2, ln_bb_clk2_a, 2);
DEFINE_CLK_SMD_RPM_XO_BUFFER(trinket, ln_bb_clk3, ln_bb_clk3_a, 3);
DEFINE_CLK_SMD_RPM_XO_BUFFER(trinket, rf_clk1, rf_clk1_a, 4);
DEFINE_CLK_SMD_RPM_XO_BUFFER(trinket, rf_clk2, rf_clk2_a, 5);

/* Voter clocks */
static DEFINE_CLK_VOTER(cnoc_msmbus_clk, cnoc_clk, LONG_MAX);
static DEFINE_CLK_VOTER(cnoc_msmbus_a_clk, cnoc_a_clk, LONG_MAX);
static DEFINE_CLK_VOTER(cnoc_keepalive_a_clk, cnoc_a_clk, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_keepalive_a_clk, snoc_a_clk, LONG_MAX);
static DEFINE_CLK_VOTER(vfe_mmrt_msmbus_clk, mmrt_clk, LONG_MAX);
static DEFINE_CLK_VOTER(vfe_mmrt_msmbus_a_clk, mmrt_a_clk, LONG_MAX);
static DEFINE_CLK_VOTER(mdp_mmrt_msmbus_clk, mmrt_clk, LONG_MAX);
static DEFINE_CLK_VOTER(mdp_mmrt_msmbus_a_clk, mmrt_a_clk, LONG_MAX);
static DEFINE_CLK_VOTER(cpp_mmnrt_msmbus_clk, mmnrt_clk, LONG_MAX);
static DEFINE_CLK_VOTER(cpp_mmnrt_msmbus_a_clk, mmnrt_a_clk, LONG_MAX);
static DEFINE_CLK_VOTER(jpeg_mmnrt_msmbus_clk, mmnrt_clk, LONG_MAX);
static DEFINE_CLK_VOTER(jpeg_mmnrt_msmbus_a_clk, mmnrt_a_clk, LONG_MAX);
static DEFINE_CLK_VOTER(venus_mmnrt_msmbus_clk, mmnrt_clk, LONG_MAX);
static DEFINE_CLK_VOTER(venus_mmnrt_msmbus_a_clk, mmnrt_a_clk, LONG_MAX);
static DEFINE_CLK_VOTER(arm9_mmnrt_msmbus_clk, mmnrt_clk, LONG_MAX);
static DEFINE_CLK_VOTER(arm9_mmnrt_msmbus_a_clk, mmnrt_a_clk, LONG_MAX);
static DEFINE_CLK_VOTER(qup0_msmbus_snoc_periph_clk, snoc_periph_clk,
								LONG_MAX);
static DEFINE_CLK_VOTER(qup0_msmbus_snoc_periph_a_clk, snoc_periph_a_clk,
								LONG_MAX);
static DEFINE_CLK_VOTER(qup1_msmbus_snoc_periph_clk, snoc_periph_clk,
								LONG_MAX);
static DEFINE_CLK_VOTER(qup1_msmbus_snoc_periph_a_clk, snoc_periph_a_clk,
								LONG_MAX);
static DEFINE_CLK_VOTER(dap_msmbus_snoc_periph_clk, snoc_periph_clk,
								LONG_MAX);
static DEFINE_CLK_VOTER(dap_msmbus_snoc_periph_a_clk, snoc_periph_a_clk,
								LONG_MAX);
static DEFINE_CLK_VOTER(sdc1_msmbus_snoc_periph_clk, snoc_periph_clk,
								LONG_MAX);
static DEFINE_CLK_VOTER(sdc1_msmbus_snoc_periph_a_clk, snoc_periph_a_clk,
								LONG_MAX);
static DEFINE_CLK_VOTER(sdc2_msmbus_snoc_periph_clk, snoc_periph_clk,
								LONG_MAX);
static DEFINE_CLK_VOTER(sdc2_msmbus_snoc_periph_a_clk, snoc_periph_a_clk,
								LONG_MAX);
static DEFINE_CLK_VOTER(crypto_msmbus_snoc_periph_clk, snoc_periph_clk,
								LONG_MAX);
static DEFINE_CLK_VOTER(crypto_msmbus_snoc_periph_a_clk, snoc_periph_a_clk,
								LONG_MAX);
static DEFINE_CLK_VOTER(sdc1_slv_msmbus_snoc_periph_clk, snoc_periph_clk,
								LONG_MAX);
static DEFINE_CLK_VOTER(sdc1_slv_msmbus_snoc_periph_a_clk, snoc_periph_a_clk,
								LONG_MAX);
static DEFINE_CLK_VOTER(sdc2_slv_msmbus_snoc_periph_clk, snoc_periph_clk,
								LONG_MAX);
static DEFINE_CLK_VOTER(sdc2_slv_msmbus_snoc_periph_a_clk, snoc_periph_a_clk,
								LONG_MAX);

/* Branch Voter clocks */
static DEFINE_CLK_BRANCH_VOTER(bi_tcxo_otg_clk, bi_tcxo);
static DEFINE_CLK_BRANCH_VOTER(bi_tcxo_pil_pronto_clk, bi_tcxo);
static DEFINE_CLK_BRANCH_VOTER(bi_tcxo_pil_mss_clk, bi_tcxo);
static DEFINE_CLK_BRANCH_VOTER(bi_tcxo_wlan_clk, bi_tcxo);
static DEFINE_CLK_BRANCH_VOTER(bi_tcxo_pil_lpass_clk, bi_tcxo);
static DEFINE_CLK_BRANCH_VOTER(bi_tcxo_pil_cdsp_clk, bi_tcxo);

static struct clk_hw *trinket_clks[] = {
	[RPM_SMD_XO_CLK_SRC] = &trinket_bi_tcxo.hw,
	[RPM_SMD_XO_A_CLK_SRC] = &trinket_bi_tcxo_ao.hw,
	[RPM_SMD_SNOC_CLK] = &trinket_snoc_clk.hw,
	[RPM_SMD_SNOC_A_CLK] = &trinket_snoc_a_clk.hw,
	[RPM_SMD_BIMC_CLK] = &trinket_bimc_clk.hw,
	[RPM_SMD_BIMC_A_CLK] = &trinket_bimc_a_clk.hw,
	[RPM_SMD_QDSS_CLK] = &trinket_qdss_clk.hw,
	[RPM_SMD_QDSS_A_CLK] = &trinket_qdss_a_clk.hw,
	[RPM_SMD_RF_CLK1] = &trinket_rf_clk1.hw,
	[RPM_SMD_RF_CLK1_A] = &trinket_rf_clk1_a.hw,
	[RPM_SMD_RF_CLK2] = &trinket_rf_clk2.hw,
	[RPM_SMD_RF_CLK2_A] = &trinket_rf_clk2_a.hw,
	[RPM_SMD_LN_BB_CLK1] = &trinket_ln_bb_clk1.hw,
	[RPM_SMD_LN_BB_CLK1_A] = &trinket_ln_bb_clk1_a.hw,
	[RPM_SMD_LN_BB_CLK2] = &trinket_ln_bb_clk2.hw,
	[RPM_SMD_LN_BB_CLK2_A] = &trinket_ln_bb_clk2_a.hw,
	[RPM_SMD_LN_BB_CLK3] = &trinket_ln_bb_clk3.hw,
	[RPM_SMD_LN_BB_CLK3_A] = &trinket_ln_bb_clk3_a.hw,
	[RPM_SMD_CNOC_CLK] = &trinket_cnoc_clk.hw,
	[RPM_SMD_CNOC_A_CLK] = &trinket_cnoc_a_clk.hw,
	[RPM_SMD_CE1_CLK] = &trinket_ce1_clk.hw,
	[RPM_SMD_CE1_A_CLK] = &trinket_ce1_a_clk.hw,
	[CNOC_MSMBUS_CLK] = &cnoc_msmbus_clk.hw,
	[CNOC_MSMBUS_A_CLK] = &cnoc_msmbus_a_clk.hw,
	[SNOC_KEEPALIVE_A_CLK] = &snoc_keepalive_a_clk.hw,
	[CNOC_KEEPALIVE_A_CLK] = &cnoc_keepalive_a_clk.hw,
	[SNOC_MSMBUS_CLK] = &snoc_msmbus_clk.hw,
	[SNOC_MSMBUS_A_CLK] = &snoc_msmbus_a_clk.hw,
	[BIMC_MSMBUS_CLK] = &bimc_msmbus_clk.hw,
	[BIMC_MSMBUS_A_CLK] = &bimc_msmbus_a_clk.hw,
	[CPP_MMNRT_MSMBUS_CLK] = &cpp_mmnrt_msmbus_clk.hw,
	[CPP_MMNRT_MSMBUS_A_CLK] = &cpp_mmnrt_msmbus_a_clk.hw,
	[JPEG_MMNRT_MSMBUS_CLK] = &jpeg_mmnrt_msmbus_clk.hw,
	[JPEG_MMNRT_MSMBUS_A_CLK] = &jpeg_mmnrt_msmbus_a_clk.hw,
	[VENUS_MMNRT_MSMBUS_CLK] = &venus_mmnrt_msmbus_clk.hw,
	[VENUS_MMNRT_MSMBUS_A_CLK] = &venus_mmnrt_msmbus_a_clk.hw,
	[ARM9_MMNRT_MSMBUS_CLK] = &arm9_mmnrt_msmbus_clk.hw,
	[ARM9_MMNRT_MSMBUS_A_CLK] = &arm9_mmnrt_msmbus_a_clk.hw,
	[VFE_MMRT_MSMBUS_CLK] = &vfe_mmrt_msmbus_clk.hw,
	[VFE_MMRT_MSMBUS_A_CLK] = &vfe_mmrt_msmbus_a_clk.hw,
	[MDP_MMRT_MSMBUS_CLK] = &mdp_mmrt_msmbus_clk.hw,
	[MDP_MMRT_MSMBUS_A_CLK] = &mdp_mmrt_msmbus_a_clk.hw,
	[QUP0_MSMBUS_SNOC_PERIPH_CLK] = &qup0_msmbus_snoc_periph_clk.hw,
	[QUP0_MSMBUS_SNOC_PERIPH_A_CLK] = &qup0_msmbus_snoc_periph_a_clk.hw,
	[QUP1_MSMBUS_SNOC_PERIPH_CLK] = &qup1_msmbus_snoc_periph_clk.hw,
	[QUP1_MSMBUS_SNOC_PERIPH_A_CLK] = &qup1_msmbus_snoc_periph_a_clk.hw,
	[DAP_MSMBUS_SNOC_PERIPH_CLK] = &dap_msmbus_snoc_periph_clk.hw,
	[DAP_MSMBUS_SNOC_PERIPH_A_CLK] = &dap_msmbus_snoc_periph_a_clk.hw,
	[SDC1_MSMBUS_SNOC_PERIPH_CLK] = &sdc1_msmbus_snoc_periph_clk.hw,
	[SDC1_MSMBUS_SNOC_PERIPH_A_CLK] = &sdc1_msmbus_snoc_periph_a_clk.hw,
	[SDC2_MSMBUS_SNOC_PERIPH_CLK] = &sdc2_msmbus_snoc_periph_clk.hw,
	[SDC2_MSMBUS_SNOC_PERIPH_A_CLK] = &sdc2_msmbus_snoc_periph_a_clk.hw,
	[CRYPTO_MSMBUS_SNOC_PERIPH_CLK] = &crypto_msmbus_snoc_periph_clk.hw,
	[CRYPTO_MSMBUS_SNOC_PERIPH_A_CLK] =
				&crypto_msmbus_snoc_periph_a_clk.hw,
	[SDC1_SLV_MSMBUS_SNOC_PERIPH_CLK] =
				&sdc1_slv_msmbus_snoc_periph_clk.hw,
	[SDC1_SLV_MSMBUS_SNOC_PERIPH_A_CLK] =
				&sdc1_slv_msmbus_snoc_periph_a_clk.hw,
	[SDC2_SLV_MSMBUS_SNOC_PERIPH_CLK] =
				&sdc2_slv_msmbus_snoc_periph_clk.hw,
	[SDC2_SLV_MSMBUS_SNOC_PERIPH_A_CLK] =
				&sdc2_slv_msmbus_snoc_periph_a_clk.hw,
	[MCD_CE1_CLK] = &mcd_ce1_clk.hw,
	[QCEDEV_CE1_CLK] = &qcedev_ce1_clk.hw,
	[QCRYPTO_CE1_CLK] = &qcrypto_ce1_clk.hw,
	[QSEECOM_CE1_CLK] = &qseecom_ce1_clk.hw,
	[SCM_CE1_CLK] = &scm_ce1_clk.hw,
	[CXO_SMD_OTG_CLK] = &bi_tcxo_otg_clk.hw,
	[CXO_SMD_PIL_PRONTO_CLK] = &bi_tcxo_pil_pronto_clk.hw,
	[CXO_SMD_PIL_MSS_CLK] = &bi_tcxo_pil_mss_clk.hw,
	[CXO_SMD_WLAN_CLK] = &bi_tcxo_wlan_clk.hw,
	[CXO_SMD_PIL_LPASS_CLK] = &bi_tcxo_pil_lpass_clk.hw,
	[CXO_SMD_PIL_CDSP_CLK] = &bi_tcxo_pil_cdsp_clk.hw,
	[RPM_SMD_IPA_CLK] = &trinket_ipa_clk.hw,
	[RPM_SMD_IPA_A_CLK] = &trinket_ipa_a_clk.hw,
	[RPM_SMD_QUP_CLK] = &trinket_qup_clk.hw,
	[RPM_SMD_QUP_A_CLK] = &trinket_qup_a_clk.hw,
	[RPM_SMD_MMRT_CLK] = &trinket_mmrt_clk.hw,
	[RPM_SMD_MMRT_A_CLK] = &trinket_mmrt_a_clk.hw,
	[RPM_SMD_MMNRT_CLK] = &trinket_mmnrt_clk.hw,
	[RPM_SMD_MMNRT_A_CLK] = &trinket_mmnrt_a_clk.hw,
	[RPM_SMD_SNOC_PERIPH_CLK] = &trinket_snoc_periph_clk.hw,
	[RPM_SMD_SNOC_PERIPH_A_CLK] = &trinket_snoc_periph_a_clk.hw,
	[RPM_SMD_SNOC_LPASS_CLK] = &trinket_snoc_lpass_clk.hw,
	[RPM_SMD_SNOC_LPASS_A_CLK] = &trinket_snoc_lpass_a_clk.hw,
};

static const struct rpm_smd_clk_desc rpm_clk_trinket = {
	.clks = trinket_clks,
	.num_rpm_clks = RPM_SMD_LN_BB_CLK3_A,
	.num_clks = ARRAY_SIZE(trinket_clks),
};

static const struct of_device_id rpm_smd_clk_match_table[] = {
	{ .compatible = "qcom,rpmcc-msm8916", .data = &rpm_clk_msm8916 },
	{ .compatible = "qcom,rpmcc-msm8974", .data = &rpm_clk_msm8974 },
	{ .compatible = "qcom,rpmcc-qcs405",  .data = &rpm_clk_qcs405  },
	{ .compatible = "qcom,rpmcc-trinket", .data = &rpm_clk_trinket  },
	{ }
};
MODULE_DEVICE_TABLE(of, rpm_smd_clk_match_table);

static int rpm_smd_clk_probe(struct platform_device *pdev)
{
	struct clk **clks;
	struct clk *clk;
	struct rpm_cc *rcc;
	struct clk_onecell_data *data;
	int ret, is_qcs405, is_trinket;
	size_t num_clks, i;
	struct clk_hw **hw_clks;
	const struct rpm_smd_clk_desc *desc;

	is_qcs405 = of_device_is_compatible(pdev->dev.of_node,
						"qcom,rpmcc-qcs405");

	is_trinket = of_device_is_compatible(pdev->dev.of_node,
						"qcom,rpmcc-trinket");
	if (is_qcs405) {
		ret = clk_vote_bimc(&qcs405_bimc_clk.hw, INT_MAX);
		if (ret < 0)
			return ret;
	} else if (is_trinket) {
		ret = clk_vote_bimc(&trinket_bimc_clk.hw, INT_MAX);
		if (ret < 0)
			return ret;
	}

	desc = of_device_get_match_data(&pdev->dev);
	if (!desc)
		return -EINVAL;

	hw_clks = desc->clks;
	num_clks = desc->num_clks;

	rcc = devm_kzalloc(&pdev->dev, sizeof(*rcc) + sizeof(*clks) * num_clks,
			   GFP_KERNEL);
	if (!rcc)
		return -ENOMEM;

	clks = rcc->clks;
	data = &rcc->data;
	data->clks = clks;
	data->clk_num = num_clks;

	for (i = 0; i <= desc->num_rpm_clks; i++) {
		if (!hw_clks[i]) {
			clks[i] = ERR_PTR(-ENOENT);
			continue;
		}

		ret = clk_smd_rpm_handoff(hw_clks[i]);
		if (ret)
			goto err;
	}

	for (i = (desc->num_rpm_clks + 1); i < num_clks; i++) {
		if (!hw_clks[i]) {
			clks[i] = ERR_PTR(-ENOENT);
			continue;
		}

		ret = voter_clk_handoff(hw_clks[i]);
		if (ret)
			goto err;
	}

	ret = clk_smd_rpm_enable_scaling();
	if (ret)
		goto err;

	for (i = 0; i < num_clks; i++) {
		if (!hw_clks[i]) {
			clks[i] = ERR_PTR(-ENOENT);
			continue;
		}

		clk = devm_clk_register(&pdev->dev, hw_clks[i]);
		if (IS_ERR(clk)) {
			ret = PTR_ERR(clk);
			goto err;
		}

		clks[i] = clk;
	}

	ret = of_clk_add_provider(pdev->dev.of_node, of_clk_src_onecell_get,
				  data);
	if (ret)
		goto err;

	if (is_qcs405) {
		/*
		 * Keep an active vote on CXO in case no other driver
		 * votes for it.
		 */
		clk_prepare_enable(qcs405_cxo_a.hw.clk);

		/* Hold an active set vote for the pnoc_keepalive_a_clk */
		clk_set_rate(pnoc_keepalive_a_clk.hw.clk, 19200000);
		clk_prepare_enable(pnoc_keepalive_a_clk.hw.clk);
	} else if (is_trinket) {
		/*
		 * Keep an active vote on CXO in case no other driver
		 * votes for it.
		 */
		clk_prepare_enable(trinket_bi_tcxo_ao.hw.clk);

		/* Hold an active set vote for the cnoc_keepalive_a_clk */
		clk_set_rate(cnoc_keepalive_a_clk.hw.clk, 19200000);
		clk_prepare_enable(cnoc_keepalive_a_clk.hw.clk);

		/* Hold an active set vote for the snoc_keepalive_a_clk */
		clk_set_rate(snoc_keepalive_a_clk.hw.clk, 19200000);
		clk_prepare_enable(snoc_keepalive_a_clk.hw.clk);
	}

	dev_info(&pdev->dev, "Registered RPM clocks\n");

	return 0;
err:
	dev_err(&pdev->dev, "Error registering SMD clock driver (%d)\n", ret);
	return ret;
}

static int rpm_smd_clk_remove(struct platform_device *pdev)
{
	of_clk_del_provider(pdev->dev.of_node);
	return 0;
}

static struct platform_driver rpm_smd_clk_driver = {
	.driver = {
		.name = "qcom-clk-smd-rpm",
		.of_match_table = rpm_smd_clk_match_table,
	},
	.probe = rpm_smd_clk_probe,
	.remove = rpm_smd_clk_remove,
};

static int __init rpm_smd_clk_init(void)
{
	return platform_driver_register(&rpm_smd_clk_driver);
}
core_initcall(rpm_smd_clk_init);

static void __exit rpm_smd_clk_exit(void)
{
	platform_driver_unregister(&rpm_smd_clk_driver);
}
module_exit(rpm_smd_clk_exit);

MODULE_DESCRIPTION("Qualcomm RPM over SMD Clock Controller Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:qcom-clk-smd-rpm");
