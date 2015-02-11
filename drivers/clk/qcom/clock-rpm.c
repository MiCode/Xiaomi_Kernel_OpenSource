/* Copyright (c) 2010-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/clk/msm-clk-provider.h>
#include <soc/qcom/clock-rpm.h>
#include <soc/qcom/msm-clock-controller.h>

#define __clk_rpmrs_set_rate(r, value, ctx) \
	((r)->rpmrs_data->set_rate_fn((r), (value), (ctx)))

#define clk_rpmrs_set_rate_sleep(r, value) \
	    __clk_rpmrs_set_rate((r), (value), (r)->rpmrs_data->ctx_sleep_id)

#define clk_rpmrs_set_rate_active(r, value) \
	   __clk_rpmrs_set_rate((r), (value), (r)->rpmrs_data->ctx_active_id)

static int clk_rpmrs_set_rate_smd(struct rpm_clk *r, uint32_t value,
				uint32_t context)
{
	struct msm_rpm_kvp kvp = {
		.key = r->rpm_key,
		.data = (void *)&value,
		.length = sizeof(value),
	};

	return msm_rpm_send_message(context, r->rpm_res_type, r->rpm_clk_id,
			&kvp, 1);
}

static int clk_rpmrs_handoff_smd(struct rpm_clk *r)
{
	if (!r->branch)
		r->c.rate = INT_MAX;

	return 0;
}

static int clk_rpmrs_is_enabled_smd(struct rpm_clk *r)
{
	return !!r->c.prepare_count;
}

struct clk_rpmrs_data {
	int (*set_rate_fn)(struct rpm_clk *r, uint32_t value, uint32_t context);
	int (*get_rate_fn)(struct rpm_clk *r);
	int (*handoff_fn)(struct rpm_clk *r);
	int (*is_enabled)(struct rpm_clk *r);
	int ctx_active_id;
	int ctx_sleep_id;
};

struct clk_rpmrs_data clk_rpmrs_data_smd = {
	.set_rate_fn = clk_rpmrs_set_rate_smd,
	.handoff_fn = clk_rpmrs_handoff_smd,
	.is_enabled = clk_rpmrs_is_enabled_smd,
	.ctx_active_id = MSM_RPM_CTX_ACTIVE_SET,
	.ctx_sleep_id = MSM_RPM_CTX_SLEEP_SET,
};

static DEFINE_MUTEX(rpm_clock_lock);

static void to_active_sleep_khz(struct rpm_clk *r, unsigned long rate,
			unsigned long *active_khz, unsigned long *sleep_khz)
{
	/* Convert the rate (hz) to khz */
	*active_khz = DIV_ROUND_UP(rate, 1000);

	/*
	 * Active-only clocks don't care what the rate is during sleep. So,
	 * they vote for zero.
	 */
	if (r->active_only)
		*sleep_khz = 0;
	else
		*sleep_khz = *active_khz;
}

static int rpm_clk_prepare(struct clk *clk)
{
	struct rpm_clk *r = to_rpm_clk(clk);
	uint32_t value;
	int rc = 0;
	unsigned long this_khz, this_sleep_khz;
	unsigned long peer_khz = 0, peer_sleep_khz = 0;
	struct rpm_clk *peer = r->peer;

	mutex_lock(&rpm_clock_lock);

	to_active_sleep_khz(r, r->c.rate, &this_khz, &this_sleep_khz);

	/* Don't send requests to the RPM if the rate has not been set. */
	if (this_khz == 0)
		goto out;

	/* Take peer clock's rate into account only if it's enabled. */
	if (peer->enabled)
		to_active_sleep_khz(peer, peer->c.rate,
				&peer_khz, &peer_sleep_khz);

	value = max(this_khz, peer_khz);
	if (r->branch)
		value = !!value;

	rc = clk_rpmrs_set_rate_active(r, value);
	if (rc)
		goto out;

	value = max(this_sleep_khz, peer_sleep_khz);
	if (r->branch)
		value = !!value;

	rc = clk_rpmrs_set_rate_sleep(r, value);
	if (rc) {
		/* Undo the active set vote and restore it to peer_khz */
		value = peer_khz;
		rc = clk_rpmrs_set_rate_active(r, value);
	}

out:
	if (!rc)
		r->enabled = true;

	mutex_unlock(&rpm_clock_lock);

	return rc;
}

static void rpm_clk_unprepare(struct clk *clk)
{
	struct rpm_clk *r = to_rpm_clk(clk);

	mutex_lock(&rpm_clock_lock);

	if (r->c.rate) {
		uint32_t value;
		struct rpm_clk *peer = r->peer;
		unsigned long peer_khz = 0, peer_sleep_khz = 0;
		int rc;

		/* Take peer clock's rate into account only if it's enabled. */
		if (peer->enabled)
			to_active_sleep_khz(peer, peer->c.rate,
				&peer_khz, &peer_sleep_khz);

		value = r->branch ? !!peer_khz : peer_khz;
		rc = clk_rpmrs_set_rate_active(r, value);
		if (rc)
			goto out;

		value = r->branch ? !!peer_sleep_khz : peer_sleep_khz;
		rc = clk_rpmrs_set_rate_sleep(r, value);
	}
	r->enabled = false;
out:
	mutex_unlock(&rpm_clock_lock);

	return;
}

static int rpm_clk_set_rate(struct clk *clk, unsigned long rate)
{
	struct rpm_clk *r = to_rpm_clk(clk);
	unsigned long this_khz, this_sleep_khz;
	int rc = 0;

	mutex_lock(&rpm_clock_lock);

	if (r->enabled) {
		uint32_t value;
		struct rpm_clk *peer = r->peer;
		unsigned long peer_khz = 0, peer_sleep_khz = 0;

		to_active_sleep_khz(r, rate, &this_khz, &this_sleep_khz);

		/* Take peer clock's rate into account only if it's enabled. */
		if (peer->enabled)
			to_active_sleep_khz(peer, peer->c.rate,
					&peer_khz, &peer_sleep_khz);

		value = max(this_khz, peer_khz);
		rc = clk_rpmrs_set_rate_active(r, value);
		if (rc)
			goto out;

		value = max(this_sleep_khz, peer_sleep_khz);
		rc = clk_rpmrs_set_rate_sleep(r, value);
	}

out:
	mutex_unlock(&rpm_clock_lock);

	return rc;
}

static unsigned long rpm_clk_get_rate(struct clk *clk)
{
	struct rpm_clk *r = to_rpm_clk(clk);
	if (r->rpmrs_data->get_rate_fn)
		return r->rpmrs_data->get_rate_fn(r);
	else
		return clk->rate;
}

static int rpm_clk_is_enabled(struct clk *clk)
{
	struct rpm_clk *r = to_rpm_clk(clk);
	return r->rpmrs_data->is_enabled(r);
}

static long rpm_clk_round_rate(struct clk *clk, unsigned long rate)
{
	/* Not supported. */
	return rate;
}

static bool rpm_clk_is_local(struct clk *clk)
{
	return false;
}

static enum handoff rpm_clk_handoff(struct clk *clk)
{
	struct rpm_clk *r = to_rpm_clk(clk);
	int rc;

	/*
	 * Querying an RPM clock's status will return 0 unless the clock's
	 * rate has previously been set through the RPM. When handing off,
	 * assume these clocks are enabled (unless the RPM call fails) so
	 * child clocks of these RPM clocks can still be handed off.
	 */
	rc  = r->rpmrs_data->handoff_fn(r);
	if (rc < 0)
		return HANDOFF_DISABLED_CLK;

	/*
	 * Since RPM handoff code may update the software rate of the clock by
	 * querying the RPM, we need to make sure our request to RPM now
	 * matches the software rate of the clock. When we send the request
	 * to RPM, we also need to update any other state info we would
	 * normally update. So, call the appropriate clock function instead
	 * of directly using the RPM driver APIs.
	 */
	rc = rpm_clk_prepare(clk);
	if (rc < 0)
		return HANDOFF_DISABLED_CLK;

	return HANDOFF_ENABLED_CLK;
}

#define RPM_MISC_CLK_TYPE	0x306b6c63
#define RPM_SCALING_ENABLE_ID	0x2

int enable_rpm_scaling(void)
{
	int rc, value = 0x1;
	static int is_inited;

	struct msm_rpm_kvp kvp = {
		.key = RPM_SMD_KEY_ENABLE,
		.data = (void *)&value,
		.length = sizeof(value),
	};

	if (is_inited)
		return 0;

	rc = msm_rpm_send_message_noirq(MSM_RPM_CTX_SLEEP_SET,
			RPM_MISC_CLK_TYPE, RPM_SCALING_ENABLE_ID, &kvp, 1);
	if (rc < 0) {
		if (rc != -EPROBE_DEFER)
			WARN(1, "RPM clock scaling (sleep set) did not enable!\n");
		return rc;
	}

	rc = msm_rpm_send_message_noirq(MSM_RPM_CTX_ACTIVE_SET,
			RPM_MISC_CLK_TYPE, RPM_SCALING_ENABLE_ID, &kvp, 1);
	if (rc < 0) {
		if (rc != -EPROBE_DEFER)
			WARN(1, "RPM clock scaling (active set) did not enable!\n");
		return rc;
	}

	is_inited++;
	return 0;
}

int vote_bimc(struct rpm_clk *r, uint32_t value)
{
	int rc;

	struct msm_rpm_kvp kvp = {
		.key = r->rpm_key,
		.data = (void *)&value,
		.length = sizeof(value),
	};

	rc = msm_rpm_send_message_noirq(MSM_RPM_CTX_ACTIVE_SET,
			r->rpm_res_type, r->rpmrs_data->ctx_active_id,
			&kvp, 1);
	if (rc < 0) {
		if (rc != -EPROBE_DEFER)
			WARN(1, "BIMC vote not sent!\n");
		return rc;
	}

	return rc;
}

struct clk_ops clk_ops_rpm = {
	.prepare = rpm_clk_prepare,
	.unprepare = rpm_clk_unprepare,
	.set_rate = rpm_clk_set_rate,
	.get_rate = rpm_clk_get_rate,
	.is_enabled = rpm_clk_is_enabled,
	.round_rate = rpm_clk_round_rate,
	.is_local = rpm_clk_is_local,
	.handoff = rpm_clk_handoff,
};

struct clk_ops clk_ops_rpm_branch = {
	.prepare = rpm_clk_prepare,
	.unprepare = rpm_clk_unprepare,
	.is_local = rpm_clk_is_local,
	.handoff = rpm_clk_handoff,
};

static struct rpm_clk *rpm_clk_dt_parser_common(struct device *dev,
						struct device_node *np)
{
	struct rpm_clk *rpm, *peer;
	struct clk *c;
	int rc = 0;
	phandle p;
	const char *str;

	rpm = devm_kzalloc(dev, sizeof(*rpm), GFP_KERNEL);
	if (!rpm) {
		dt_err(np, "memory alloc failure\n");
		return ERR_PTR(-ENOMEM);
	}

	rc = of_property_read_phandle_index(np, "qcom,rpm-peer", 0, &p);
	if (rc) {
		dt_err(np, "missing qcom,rpm-peer dt property\n");
		return ERR_PTR(rc);
	}

	/* Rely on whoever's called last to setup the circular ref */
	c = msmclk_lookup_phandle(dev, p);
	if (!IS_ERR(c)) {
		peer = to_rpm_clk(c);
		peer->peer = rpm;
		rpm->peer = peer;
	}

	rpm->rpmrs_data = &clk_rpmrs_data_smd;
	rpm->active_only = of_device_is_compatible(np, "qcom,rpm-a-clk") ||
			of_device_is_compatible(np, "qcom,rpm-branch-a-clk");

	rc = of_property_read_string(np, "qcom,res-type", &str);
	if (rc) {
		dt_err(np, "missing qcom,res-type dt property\n");
		return ERR_PTR(rc);
	}
	sscanf(str, "%4c", (char *) &rpm->rpm_res_type);

	rc = of_property_read_u32(np, "qcom,res-id", &rpm->rpm_clk_id);
	if (rc) {
		dt_err(np, "missing qcom,res-id dt property\n");
		return ERR_PTR(rc);
	}

	rc = of_property_read_string(np, "qcom,key", &str);
	if (rc) {
		dt_err(np, "missing qcom,key dt property\n");
		return ERR_PTR(rc);
	}
	sscanf(str, "%4c", (char *) &rpm->rpm_key);
	return rpm;
}

static void *rpm_clk_dt_parser(struct device *dev, struct device_node *np)
{
	struct rpm_clk *rpm;
	rpm = rpm_clk_dt_parser_common(dev, np);
	if (IS_ERR(rpm))
		return rpm;

	rpm->c.ops = &clk_ops_rpm;
	return msmclk_generic_clk_init(dev, np, &rpm->c);
}

static void *rpm_branch_clk_dt_parser(struct device *dev,
					struct device_node *np)
{
	struct rpm_clk *rpm;
	u32 rate;
	int rc;
	rpm = rpm_clk_dt_parser_common(dev, np);
	if (IS_ERR(rpm))
		return rpm;

	rpm->c.ops = &clk_ops_rpm_branch;
	rpm->branch = true;

	rc = of_property_read_u32(np, "qcom,rcg-init-rate", &rate);
	if (!rc)
		rpm->c.rate = rate;

	return msmclk_generic_clk_init(dev, np, &rpm->c);
}
MSMCLK_PARSER(rpm_clk_dt_parser, "qcom,rpm-clk", 0);
MSMCLK_PARSER(rpm_clk_dt_parser, "qcom,rpm-a-clk", 1);
MSMCLK_PARSER(rpm_branch_clk_dt_parser, "qcom,rpm-branch-clk", 0);
MSMCLK_PARSER(rpm_branch_clk_dt_parser, "qcom,rpm-branch-a-clk", 1);
