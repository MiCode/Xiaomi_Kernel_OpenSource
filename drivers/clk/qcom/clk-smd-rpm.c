/*
 * Copyright (c) 2016, Linaro Limited
 * Copyright (c) 2014, 2016, The Linux Foundation. All rights reserved.
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

static int clk_smd_rpm_handoff(struct clk_hw *hw)
{
	int ret = 0;
	uint32_t value = cpu_to_le32(INT_MAX);
	struct clk_smd_rpm *r = to_clk_smd_rpm(hw);
	struct msm_rpm_kvp req = {
		.key = cpu_to_le32(r->rpm_key),
		.data = (void *)&value,
		.length = sizeof(value),
	};

	ret = msm_rpm_send_message(QCOM_SMD_RPM_ACTIVE_STATE, r->rpm_res_type,
			r->rpm_clk_id, &req, 1);
	if (ret)
		return ret;

	ret = msm_rpm_send_message(QCOM_SMD_RPM_SLEEP_STATE, r->rpm_res_type,
			r->rpm_clk_id, &req, 1);
	if (ret)
		return ret;

	return ret;
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
	*active = rate;

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

	/* Don't send requests to the RPM if the rate has not been set. */
	if (!r->rate)
		goto out;

	to_active_sleep(r, r->rate, &this_rate, &this_sleep_rate);

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
		goto out;

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

	ret = msm_rpm_send_message_noirq(QCOM_SMD_RPM_SLEEP_STATE,
			QCOM_SMD_RPM_MISC_CLK,
			QCOM_RPM_SCALING_ENABLE_ID, &req, 1);
	if (ret) {
		pr_err("RPM clock scaling (sleep set) not enabled!\n");
		return ret;
	}

	ret = msm_rpm_send_message_noirq(QCOM_SMD_RPM_ACTIVE_STATE,
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

	ret = msm_rpm_send_message_noirq(QCOM_SMD_RPM_ACTIVE_STATE,
		r->rpm_res_type, r->rpm_clk_id, &req, 1);
	if (ret < 0) {
		if (ret != -EPROBE_DEFER)
			WARN(1, "BIMC vote not sent!\n");
		return ret;
	}

	return ret;
}

static const struct clk_ops clk_smd_rpm_ops = {
	.prepare	= clk_smd_rpm_prepare,
	.unprepare	= clk_smd_rpm_unprepare,
	.set_rate	= clk_smd_rpm_set_rate,
	.round_rate	= clk_smd_rpm_round_rate,
	.recalc_rate	= clk_smd_rpm_recalc_rate,
};

static const struct clk_ops clk_smd_rpm_branch_ops = {
	.prepare	= clk_smd_rpm_prepare,
	.unprepare	= clk_smd_rpm_unprepare,
	.round_rate	= clk_smd_rpm_round_rate,
	.recalc_rate	= clk_smd_rpm_recalc_rate,
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
	[RPM_PCNOC_CLK]		= &msm8916_pcnoc_clk.hw,
	[RPM_PCNOC_A_CLK]	= &msm8916_pcnoc_a_clk.hw,
	[RPM_SNOC_CLK]		= &msm8916_snoc_clk.hw,
	[RPM_SNOC_A_CLK]	= &msm8916_snoc_a_clk.hw,
	[RPM_BIMC_CLK]		= &msm8916_bimc_clk.hw,
	[RPM_BIMC_A_CLK]	= &msm8916_bimc_a_clk.hw,
	[RPM_QDSS_CLK]		= &msm8916_qdss_clk.hw,
	[RPM_QDSS_A_CLK]	= &msm8916_qdss_a_clk.hw,
	[RPM_BB_CLK1]		= &msm8916_bb_clk1.hw,
	[RPM_BB_CLK1_A]		= &msm8916_bb_clk1_a.hw,
	[RPM_BB_CLK2]		= &msm8916_bb_clk2.hw,
	[RPM_BB_CLK2_A]		= &msm8916_bb_clk2_a.hw,
	[RPM_RF_CLK1]		= &msm8916_rf_clk1.hw,
	[RPM_RF_CLK1_A]		= &msm8916_rf_clk1_a.hw,
	[RPM_RF_CLK2]		= &msm8916_rf_clk2.hw,
	[RPM_RF_CLK2_A]		= &msm8916_rf_clk2_a.hw,
	[RPM_BB_CLK1_PIN]	= &msm8916_bb_clk1_pin.hw,
	[RPM_BB_CLK1_A_PIN]	= &msm8916_bb_clk1_a_pin.hw,
	[RPM_BB_CLK2_PIN]	= &msm8916_bb_clk2_pin.hw,
	[RPM_BB_CLK2_A_PIN]	= &msm8916_bb_clk2_a_pin.hw,
	[RPM_RF_CLK1_PIN]	= &msm8916_rf_clk1_pin.hw,
	[RPM_RF_CLK1_A_PIN]	= &msm8916_rf_clk1_a_pin.hw,
	[RPM_RF_CLK2_PIN]	= &msm8916_rf_clk2_pin.hw,
	[RPM_RF_CLK2_A_PIN]	= &msm8916_rf_clk2_a_pin.hw,
};

static const struct rpm_smd_clk_desc rpm_clk_msm8916 = {
	.clks = msm8916_clks,
	.num_rpm_clks = RPM_RF_CLK2_A_PIN,
	.num_clks = ARRAY_SIZE(msm8916_clks),
};

/* msm8996 */
DEFINE_CLK_SMD_RPM(msm8996, pnoc_clk, pnoc_a_clk, QCOM_SMD_RPM_BUS_CLK, 0);
DEFINE_CLK_SMD_RPM(msm8996, snoc_clk, snoc_a_clk, QCOM_SMD_RPM_BUS_CLK, 1);
DEFINE_CLK_SMD_RPM(msm8996, cnoc_clk, cnoc_a_clk, QCOM_SMD_RPM_BUS_CLK, 2);
DEFINE_CLK_SMD_RPM(msm8996, bimc_clk, bimc_a_clk, QCOM_SMD_RPM_MEM_CLK, 0);
DEFINE_CLK_SMD_RPM(msm8996, mmssnoc_axi_rpm_clk, mmssnoc_axi_rpm_a_clk,
		   QCOM_SMD_RPM_MMAXI_CLK, 0);
DEFINE_CLK_SMD_RPM(msm8996, ipa_clk, ipa_a_clk, QCOM_SMD_RPM_IPA_CLK, 0);
DEFINE_CLK_SMD_RPM(msm8996, ce1_clk, ce1_a_clk, QCOM_SMD_RPM_CE_CLK, 0);
DEFINE_CLK_SMD_RPM_BRANCH(msm8996, cxo, cxo_a, QCOM_SMD_RPM_MISC_CLK, 0, 19200000);
DEFINE_CLK_SMD_RPM_BRANCH(msm8996, aggre1_noc_clk, aggre1_noc_a_clk,
			  QCOM_SMD_RPM_AGGR_CLK, 0, 1000);
DEFINE_CLK_SMD_RPM_BRANCH(msm8996, aggre2_noc_clk, aggre2_noc_a_clk,
			  QCOM_SMD_RPM_AGGR_CLK, 1, 1000);
DEFINE_CLK_SMD_RPM_QDSS(msm8996, qdss_clk, qdss_a_clk, QCOM_SMD_RPM_MISC_CLK, 1);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msm8996, bb_clk1, bb_clk1_a, 1);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msm8996, bb_clk2, bb_clk2_a, 2);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msm8996, rf_clk1, rf_clk1_a, 4);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msm8996, rf_clk2, rf_clk2_a, 5);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msm8996, ln_bb_clk, ln_bb_a_clk, 8);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msm8996, div_clk1, div_clk1_ao, 0xb);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msm8996, div_clk2, div_clk2_ao, 0xc);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msm8996, div_clk3, div_clk3_ao, 0xc);
DEFINE_CLK_SMD_RPM_XO_BUFFER_PINCTRL(msm8996, bb_clk1_pin, bb_clk1_a_pin, 1);
DEFINE_CLK_SMD_RPM_XO_BUFFER_PINCTRL(msm8996, bb_clk2_pin, bb_clk2_a_pin, 2);
DEFINE_CLK_SMD_RPM_XO_BUFFER_PINCTRL(msm8996, rf_clk1_pin, rf_clk1_a_pin, 4);
DEFINE_CLK_SMD_RPM_XO_BUFFER_PINCTRL(msm8996, rf_clk2_pin, rf_clk2_a_pin, 5);

static struct clk_hw *msm8996_clks[] = {
	[RPM_XO_CLK_SRC]	= &msm8996_cxo.hw,
	[RPM_XO_A_CLK_SRC]	= &msm8996_cxo_a.hw,
	[RPM_PCNOC_CLK]		= &msm8996_pnoc_clk.hw,
	[RPM_PCNOC_A_CLK]	= &msm8996_pnoc_a_clk.hw,
	[RPM_SNOC_CLK]		= &msm8996_snoc_clk.hw,
	[RPM_SNOC_A_CLK]	= &msm8996_snoc_a_clk.hw,
	[RPM_BIMC_CLK]		= &msm8996_bimc_clk.hw,
	[RPM_BIMC_A_CLK]	= &msm8996_bimc_a_clk.hw,
	[RPM_QDSS_CLK]		= &msm8996_qdss_clk.hw,
	[RPM_QDSS_A_CLK]	= &msm8996_qdss_a_clk.hw,
	[RPM_BB_CLK1_PIN]	= &msm8996_bb_clk1_pin.hw,
	[RPM_BB_CLK1_A_PIN]	= &msm8996_bb_clk1_a_pin.hw,
	[RPM_BB_CLK2_PIN]	= &msm8996_bb_clk2_pin.hw,
	[RPM_BB_CLK2_A_PIN]	= &msm8996_bb_clk2_a_pin.hw,
	[RPM_RF_CLK1_PIN]	= &msm8996_rf_clk1_pin.hw,
	[RPM_RF_CLK1_A_PIN]	= &msm8996_rf_clk1_a_pin.hw,
	[RPM_RF_CLK2_PIN]	= &msm8996_rf_clk2_pin.hw,
	[RPM_RF_CLK2_A_PIN]	= &msm8996_rf_clk2_a_pin.hw,
	[RPM_AGGR1_NOC_CLK]	= &msm8996_aggre1_noc_clk.hw,
	[RPM_AGGR1_NOC_A_CLK]	= &msm8996_aggre1_noc_a_clk.hw,
	[RPM_AGGR2_NOC_CLK]	= &msm8996_aggre2_noc_clk.hw,
	[RPM_AGGR2_NOC_A_CLK]	= &msm8996_aggre2_noc_a_clk.hw,
	[RPM_CNOC_CLK]		= &msm8996_cnoc_clk.hw,
	[RPM_CNOC_A_CLK]	= &msm8996_cnoc_a_clk.hw,
	[RPM_MMAXI_CLK]		= &msm8996_mmssnoc_axi_rpm_clk.hw,
	[RPM_MMAXI_A_CLK]	= &msm8996_mmssnoc_axi_rpm_a_clk.hw,
	[RPM_IPA_CLK]		= &msm8996_ipa_clk.hw,
	[RPM_IPA_A_CLK]		= &msm8996_ipa_a_clk.hw,
	[RPM_CE1_CLK]		= &msm8996_ce1_clk.hw,
	[RPM_CE1_A_CLK]		= &msm8996_ce1_a_clk.hw,
	[RPM_DIV_CLK1]		= &msm8996_div_clk1.hw,
	[RPM_DIV_CLK1_AO]	= &msm8996_div_clk1_ao.hw,
	[RPM_DIV_CLK2]		= &msm8996_div_clk2.hw,
	[RPM_DIV_CLK2_AO]	= &msm8996_div_clk2_ao.hw,
	[RPM_DIV_CLK3]		= &msm8996_div_clk3.hw,
	[RPM_DIV_CLK3_AO]	= &msm8996_div_clk3_ao.hw,
	[RPM_LN_BB_CLK]		= &msm8996_ln_bb_clk.hw,
	[RPM_LN_BB_A_CLK]	= &msm8996_ln_bb_a_clk.hw,
};

static const struct rpm_smd_clk_desc rpm_clk_msm8996 = {
	.clks = msm8996_clks,
	.num_rpm_clks = RPM_LN_BB_A_CLK,
	.num_clks = ARRAY_SIZE(msm8996_clks),
};

/* msmfalcon */
DEFINE_CLK_SMD_RPM_BRANCH(msmfalcon, cxo, cxo_a, QCOM_SMD_RPM_MISC_CLK, 0,
								19200000);
DEFINE_CLK_SMD_RPM(msmfalcon, snoc_clk, snoc_a_clk, QCOM_SMD_RPM_BUS_CLK, 1);
DEFINE_CLK_SMD_RPM(msmfalcon, cnoc_clk, cnoc_a_clk, QCOM_SMD_RPM_BUS_CLK, 2);
DEFINE_CLK_SMD_RPM(msmfalcon, cnoc_periph_clk, cnoc_periph_a_clk,
						QCOM_SMD_RPM_BUS_CLK, 0);
DEFINE_CLK_SMD_RPM(msmfalcon, bimc_clk, bimc_a_clk, QCOM_SMD_RPM_MEM_CLK, 0);
DEFINE_CLK_SMD_RPM(msmfalcon, mmssnoc_axi_clk, mmssnoc_axi_a_clk,
						   QCOM_SMD_RPM_MMAXI_CLK, 0);
DEFINE_CLK_SMD_RPM(msmfalcon, ipa_clk, ipa_a_clk, QCOM_SMD_RPM_IPA_CLK, 0);
DEFINE_CLK_SMD_RPM(msmfalcon, ce1_clk, ce1_a_clk, QCOM_SMD_RPM_CE_CLK, 0);
DEFINE_CLK_SMD_RPM(msmfalcon, aggre2_noc_clk, aggre2_noc_a_clk,
						QCOM_SMD_RPM_AGGR_CLK, 2);
DEFINE_CLK_SMD_RPM_QDSS(msmfalcon, qdss_clk, qdss_a_clk,
						QCOM_SMD_RPM_MISC_CLK, 1);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msmfalcon, rf_clk2, rf_clk2_ao, 5);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msmfalcon, div_clk1, div_clk1_ao, 0xb);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msmfalcon, ln_bb_clk1, ln_bb_clk1_ao, 0x1);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msmfalcon, ln_bb_clk2, ln_bb_clk2_ao, 0x2);
DEFINE_CLK_SMD_RPM_XO_BUFFER(msmfalcon, ln_bb_clk3, ln_bb_clk3_ao, 0x3);

DEFINE_CLK_SMD_RPM_XO_BUFFER_PINCTRL(msmfalcon, rf_clk2_pin, rf_clk2_a_pin, 5);
DEFINE_CLK_SMD_RPM_XO_BUFFER_PINCTRL(msmfalcon, ln_bb_clk1_pin,
							ln_bb_clk1_pin_ao, 0x1);
DEFINE_CLK_SMD_RPM_XO_BUFFER_PINCTRL(msmfalcon, ln_bb_clk2_pin,
							ln_bb_clk2_pin_ao, 0x2);
DEFINE_CLK_SMD_RPM_XO_BUFFER_PINCTRL(msmfalcon, ln_bb_clk3_pin,
							ln_bb_clk3_pin_ao, 0x3);
/* Voter clocks */
static DEFINE_CLK_VOTER(bimc_msmbus_clk, bimc_clk, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_msmbus_a_clk, bimc_a_clk, LONG_MAX);
static DEFINE_CLK_VOTER(cnoc_msmbus_clk, cnoc_clk, LONG_MAX);
static DEFINE_CLK_VOTER(cnoc_msmbus_a_clk, cnoc_a_clk, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_msmbus_clk, snoc_clk, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_msmbus_a_clk, snoc_a_clk, LONG_MAX);
static DEFINE_CLK_VOTER(cnoc_periph_keepalive_a_clk, cnoc_periph_a_clk,
						LONG_MAX);
static DEFINE_CLK_VOTER(mcd_ce1_clk, ce1_clk, 85710000);
static DEFINE_CLK_VOTER(qcedev_ce1_clk, ce1_clk, 85710000);
static DEFINE_CLK_VOTER(qcrypto_ce1_clk, ce1_clk, 85710000);
static DEFINE_CLK_VOTER(qseecom_ce1_clk, ce1_clk, 85710000);
static DEFINE_CLK_VOTER(scm_ce1_clk, ce1_clk, 85710000);

static DEFINE_CLK_BRANCH_VOTER(cxo_dwc3_clk, cxo);
static DEFINE_CLK_BRANCH_VOTER(cxo_lpm_clk, cxo);
static DEFINE_CLK_BRANCH_VOTER(cxo_otg_clk, cxo);
static DEFINE_CLK_BRANCH_VOTER(cxo_pil_lpass_clk, cxo);
static DEFINE_CLK_BRANCH_VOTER(cxo_pil_cdsp_clk, cxo);

static struct clk_hw *msmfalcon_clks[] = {
	[RPM_XO_CLK_SRC]	= &msmfalcon_cxo.hw,
	[RPM_XO_A_CLK_SRC]	= &msmfalcon_cxo_a.hw,
	[RPM_SNOC_CLK]		= &msmfalcon_snoc_clk.hw,
	[RPM_SNOC_A_CLK]	= &msmfalcon_snoc_a_clk.hw,
	[RPM_BIMC_CLK]		= &msmfalcon_bimc_clk.hw,
	[RPM_BIMC_A_CLK]	= &msmfalcon_bimc_a_clk.hw,
	[RPM_QDSS_CLK]		= &msmfalcon_qdss_clk.hw,
	[RPM_QDSS_A_CLK]	= &msmfalcon_qdss_a_clk.hw,
	[RPM_RF_CLK2_PIN]	= &msmfalcon_rf_clk2_pin.hw,
	[RPM_RF_CLK2_A_PIN]	= &msmfalcon_rf_clk2_a_pin.hw,
	[RPM_AGGR2_NOC_CLK]	= &msmfalcon_aggre2_noc_clk.hw,
	[RPM_AGGR2_NOC_A_CLK]	= &msmfalcon_aggre2_noc_a_clk.hw,
	[RPM_CNOC_CLK]		= &msmfalcon_cnoc_clk.hw,
	[RPM_CNOC_A_CLK]	= &msmfalcon_cnoc_a_clk.hw,
	[RPM_IPA_CLK]		= &msmfalcon_ipa_clk.hw,
	[RPM_IPA_A_CLK]		= &msmfalcon_ipa_a_clk.hw,
	[RPM_CE1_CLK]		= &msmfalcon_ce1_clk.hw,
	[RPM_CE1_A_CLK]		= &msmfalcon_ce1_a_clk.hw,
	[RPM_DIV_CLK1]		= &msmfalcon_div_clk1.hw,
	[RPM_DIV_CLK1_AO]	= &msmfalcon_div_clk1_ao.hw,
	[RPM_LN_BB_CLK1]	= &msmfalcon_ln_bb_clk1.hw,
	[RPM_LN_BB_CLK1]	= &msmfalcon_ln_bb_clk1_ao.hw,
	[RPM_LN_BB_CLK1_PIN]	= &msmfalcon_ln_bb_clk1_pin.hw,
	[RPM_LN_BB_CLK1_PIN_AO]	= &msmfalcon_ln_bb_clk1_pin_ao.hw,
	[RPM_LN_BB_CLK2]	= &msmfalcon_ln_bb_clk2.hw,
	[RPM_LN_BB_CLK2_AO]	= &msmfalcon_ln_bb_clk2_ao.hw,
	[RPM_LN_BB_CLK2_PIN]	= &msmfalcon_ln_bb_clk2_pin.hw,
	[RPM_LN_BB_CLK2_PIN_AO] = &msmfalcon_ln_bb_clk2_pin_ao.hw,
	[RPM_LN_BB_CLK3]	= &msmfalcon_ln_bb_clk3.hw,
	[RPM_LN_BB_CLK3_AO]	= &msmfalcon_ln_bb_clk3_ao.hw,
	[RPM_LN_BB_CLK3_PIN]	= &msmfalcon_ln_bb_clk3_pin.hw,
	[RPM_LN_BB_CLK3_PIN_AO] = &msmfalcon_ln_bb_clk3_pin_ao.hw,
	[RPM_CNOC_PERIPH_CLK]	= &msmfalcon_cnoc_periph_clk.hw,
	[RPM_CNOC_PERIPH_A_CLK] = &msmfalcon_cnoc_periph_a_clk.hw,
	[MMSSNOC_AXI_CLK]	= &msmfalcon_mmssnoc_axi_clk.hw,
	[MMSSNOC_AXI_A_CLK]	= &msmfalcon_mmssnoc_axi_a_clk.hw,

	/* Voter Clocks */
	[BIMC_MSMBUS_CLK]	= &bimc_msmbus_clk.hw,
	[BIMC_MSMBUS_A_CLK]	= &bimc_msmbus_a_clk.hw,
	[CNOC_MSMBUS_CLK]	= &cnoc_msmbus_clk.hw,
	[CNOC_MSMBUS_A_CLK]	= &cnoc_msmbus_a_clk.hw,
	[MCD_CE1_CLK]		= &mcd_ce1_clk.hw,
	[QCEDEV_CE1_CLK]	= &qcedev_ce1_clk.hw,
	[QCRYPTO_CE1_CLK]	= &qcrypto_ce1_clk.hw,
	[QSEECOM_CE1_CLK]	= &qseecom_ce1_clk.hw,
	[SCM_CE1_CLK]		= &scm_ce1_clk.hw,
	[SNOC_MSMBUS_CLK]	= &snoc_msmbus_clk.hw,
	[SNOC_MSMBUS_A_CLK]	= &snoc_msmbus_a_clk.hw,
	[CXO_DWC3_CLK]		= &cxo_dwc3_clk.hw,
	[CXO_LPM_CLK]		= &cxo_lpm_clk.hw,
	[CXO_OTG_CLK]		= &cxo_otg_clk.hw,
	[CXO_PIL_LPASS_CLK]	= &cxo_pil_lpass_clk.hw,
	[CXO_PIL_CDSP_CLK]	= &cxo_pil_cdsp_clk.hw,
	[CNOC_PERIPH_KEEPALIVE_A_CLK] = &cnoc_periph_keepalive_a_clk.hw,
};

static const struct rpm_smd_clk_desc rpm_clk_msmfalcon = {
	.clks = msmfalcon_clks,
	.num_rpm_clks = RPM_CNOC_PERIPH_A_CLK,
	.num_clks = ARRAY_SIZE(msmfalcon_clks),
};

static const struct of_device_id rpm_smd_clk_match_table[] = {
	{ .compatible = "qcom,rpmcc-msm8916", .data = &rpm_clk_msm8916},
	{ .compatible = "qcom,rpmcc-msm8996", .data = &rpm_clk_msm8996},
	{ .compatible = "qcom,rpmcc-msmfalcon", .data = &rpm_clk_msmfalcon},
	{ }
};
MODULE_DEVICE_TABLE(of, rpm_smd_clk_match_table);

static int rpm_smd_clk_probe(struct platform_device *pdev)
{
	struct clk **clks;
	struct clk *clk;
	struct rpm_cc *rcc;
	struct clk_onecell_data *data;
	int ret, is_8996 = 0, is_falcon = 0;
	size_t num_clks, i;
	struct clk_hw **hw_clks;
	const struct rpm_smd_clk_desc *desc;

	is_8996 = of_device_is_compatible(pdev->dev.of_node,
						"qcom,rpmcc-msm8996");
	is_falcon = of_device_is_compatible(pdev->dev.of_node,
						"qcom,rpmcc-msmfalcon");
	if (is_8996) {
		ret = clk_vote_bimc(&msm8996_bimc_clk.hw, INT_MAX);
		if (ret < 0)
			return ret;
	} else if (is_falcon) {
		ret = clk_vote_bimc(&msmfalcon_bimc_clk.hw, INT_MAX);
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

	/* Keep an active vote on CXO in case no other driver votes for it */
	if (is_8996)
		clk_prepare_enable(msm8996_cxo_a.hw.clk);
	else if (is_falcon) {
		clk_prepare_enable(msmfalcon_cxo_a.hw.clk);

		/* Hold an active set vote for the cnoc_periph resource */
		clk_set_rate(cnoc_periph_keepalive_a_clk.hw.clk, 19200000);
		clk_prepare_enable(cnoc_periph_keepalive_a_clk.hw.clk);
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
