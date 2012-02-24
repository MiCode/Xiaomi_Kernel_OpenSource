/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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

#include <linux/err.h>
#include <mach/clk-provider.h>

#include "rpm_resources.h"
#include "clock-rpm.h"

#define __clk_rpmrs_set_rate(r, value, ctx, noirq) \
	((r)->rpmrs_data->set_rate_fn((r), (value), (ctx), (noirq)))

#define clk_rpmrs_set_rate_sleep(r, value) \
	    __clk_rpmrs_set_rate((r), (value), (r)->rpmrs_data->ctx_sleep_id, 0)

#define clk_rpmrs_set_rate_sleep_noirq(r, value) \
	    __clk_rpmrs_set_rate((r), (value), (r)->rpmrs_data->ctx_sleep_id, 1)

#define clk_rpmrs_set_rate_active(r, value) \
	   __clk_rpmrs_set_rate((r), (value), (r)->rpmrs_data->ctx_active_id, 0)

#define clk_rpmrs_set_rate_active_noirq(r, value) \
	   __clk_rpmrs_set_rate((r), (value), (r)->rpmrs_data->ctx_active_id, 1)

static int clk_rpmrs_set_rate(struct rpm_clk *r, uint32_t value,
			   uint32_t context, int noirq)
{
	struct msm_rpm_iv_pair iv = {
		.id = r->rpm_clk_id,
		.value = value,
	};
	if (noirq)
		return msm_rpmrs_set_noirq(context, &iv, 1);
	else
		return msm_rpmrs_set(context, &iv, 1);
}

static int clk_rpmrs_get_rate(struct rpm_clk *r)
{
	int rc;
	struct msm_rpm_iv_pair iv = { .id = r->rpm_status_id, };
	rc = msm_rpm_get_status(&iv, 1);
	return (rc < 0) ? rc : iv.value * r->factor;
}

static int clk_rpmrs_handoff(struct rpm_clk *r)
{
	struct msm_rpm_iv_pair iv = { .id = r->rpm_status_id, };
	int rc = msm_rpm_get_status(&iv, 1);

	if (rc < 0)
		return rc;

	if (!r->branch) {
		r->last_set_khz = iv.value;
		if (!r->active_only)
			r->last_set_sleep_khz = iv.value;
		r->c.rate = iv.value * r->factor;
	}

	return 0;
}

static int clk_rpmrs_set_rate_smd(struct rpm_clk *r, uint32_t value,
				uint32_t context, int noirq)
{
	struct msm_rpm_kvp kvp = {
		.key = r->rpm_key,
		.data = (void *)&value,
		.length = sizeof(value),
	};

	if (noirq)
		return msm_rpm_send_message_noirq(context,
				r->rpm_res_type, r->rpm_clk_id, &kvp, 1);
	else
		return msm_rpm_send_message(context, r->rpm_res_type,
						r->rpm_clk_id, &kvp, 1);
}

static int clk_rpmrs_handoff_smd(struct rpm_clk *r)
{
	return 0;
}

struct clk_rpmrs_data {
	int (*set_rate_fn)(struct rpm_clk *r, uint32_t value,
				uint32_t context, int noirq);
	int (*get_rate_fn)(struct rpm_clk *r);
	int (*handoff_fn)(struct rpm_clk *r);
	int ctx_active_id;
	int ctx_sleep_id;
};

struct clk_rpmrs_data clk_rpmrs_data = {
	.set_rate_fn = clk_rpmrs_set_rate,
	.get_rate_fn = clk_rpmrs_get_rate,
	.handoff_fn = clk_rpmrs_handoff,
	.ctx_active_id = MSM_RPM_CTX_SET_0,
	.ctx_sleep_id = MSM_RPM_CTX_SET_SLEEP,
};

struct clk_rpmrs_data clk_rpmrs_data_smd = {
	.set_rate_fn = clk_rpmrs_set_rate_smd,
	.handoff_fn = clk_rpmrs_handoff_smd,
	.ctx_active_id = MSM_RPM_CTX_ACTIVE_SET,
	.ctx_sleep_id = MSM_RPM_CTX_SLEEP_SET,
};

static DEFINE_SPINLOCK(rpm_clock_lock);

static int rpm_clk_enable(struct clk *clk)
{
	unsigned long flags;
	struct rpm_clk *r = to_rpm_clk(clk);
	uint32_t value;
	int rc = 0;
	unsigned long this_khz, this_sleep_khz;
	unsigned long peer_khz = 0, peer_sleep_khz = 0;
	struct rpm_clk *peer = r->peer;

	spin_lock_irqsave(&rpm_clock_lock, flags);

	this_khz = r->last_set_khz;
	/* Don't send requests to the RPM if the rate has not been set. */
	if (this_khz == 0)
		goto out;

	this_sleep_khz = r->last_set_sleep_khz;

	/* Take peer clock's rate into account only if it's enabled. */
	if (peer->enabled) {
		peer_khz = peer->last_set_khz;
		peer_sleep_khz = peer->last_set_sleep_khz;
	}

	value = max(this_khz, peer_khz);
	if (r->branch)
		value = !!value;

	rc = clk_rpmrs_set_rate_active_noirq(r, value);
	if (rc)
		goto out;

	value = max(this_sleep_khz, peer_sleep_khz);
	if (r->branch)
		value = !!value;

	rc = clk_rpmrs_set_rate_sleep_noirq(r, value);
	if (rc) {
		/* Undo the active set vote and restore it to peer_khz */
		value = peer_khz;
		rc = clk_rpmrs_set_rate_active_noirq(r, value);
	}

out:
	if (!rc)
		r->enabled = true;

	spin_unlock_irqrestore(&rpm_clock_lock, flags);

	return rc;
}

static void rpm_clk_disable(struct clk *clk)
{
	unsigned long flags;
	struct rpm_clk *r = to_rpm_clk(clk);

	spin_lock_irqsave(&rpm_clock_lock, flags);

	if (r->last_set_khz) {
		uint32_t value;
		struct rpm_clk *peer = r->peer;
		unsigned long peer_khz = 0, peer_sleep_khz = 0;
		int rc;

		/* Take peer clock's rate into account only if it's enabled. */
		if (peer->enabled) {
			peer_khz = peer->last_set_khz;
			peer_sleep_khz = peer->last_set_sleep_khz;
		}

		value = r->branch ? !!peer_khz : peer_khz;
		rc = clk_rpmrs_set_rate_active_noirq(r, value);
		if (rc)
			goto out;

		value = r->branch ? !!peer_sleep_khz : peer_sleep_khz;
		rc = clk_rpmrs_set_rate_sleep_noirq(r, value);
	}
	r->enabled = false;
out:
	spin_unlock_irqrestore(&rpm_clock_lock, flags);

	return;
}

static int rpm_clk_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned long flags;
	struct rpm_clk *r = to_rpm_clk(clk);
	unsigned long this_khz, this_sleep_khz;
	int rc = 0;

	this_khz = DIV_ROUND_UP(rate, r->factor);

	spin_lock_irqsave(&rpm_clock_lock, flags);

	/* Active-only clocks don't care what the rate is during sleep. So,
	 * they vote for zero. */
	if (r->active_only)
		this_sleep_khz = 0;
	else
		this_sleep_khz = this_khz;

	if (r->enabled) {
		uint32_t value;
		struct rpm_clk *peer = r->peer;
		unsigned long peer_khz = 0, peer_sleep_khz = 0;

		/* Take peer clock's rate into account only if it's enabled. */
		if (peer->enabled) {
			peer_khz = peer->last_set_khz;
			peer_sleep_khz = peer->last_set_sleep_khz;
		}

		value = max(this_khz, peer_khz);
		rc = clk_rpmrs_set_rate_active_noirq(r, value);
		if (rc)
			goto out;

		value = max(this_sleep_khz, peer_sleep_khz);
		rc = clk_rpmrs_set_rate_sleep_noirq(r, value);
	}
	if (!rc) {
		r->last_set_khz = this_khz;
		r->last_set_sleep_khz = this_sleep_khz;
	}

out:
	spin_unlock_irqrestore(&rpm_clock_lock, flags);

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
	return !!(rpm_clk_get_rate(clk));
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

	return HANDOFF_ENABLED_CLK;
}

struct clk_ops clk_ops_rpm = {
	.enable = rpm_clk_enable,
	.disable = rpm_clk_disable,
	.set_rate = rpm_clk_set_rate,
	.get_rate = rpm_clk_get_rate,
	.is_enabled = rpm_clk_is_enabled,
	.round_rate = rpm_clk_round_rate,
	.is_local = rpm_clk_is_local,
	.handoff = rpm_clk_handoff,
};

struct clk_ops clk_ops_rpm_branch = {
	.enable = rpm_clk_enable,
	.disable = rpm_clk_disable,
	.is_local = rpm_clk_is_local,
	.handoff = rpm_clk_handoff,
};
