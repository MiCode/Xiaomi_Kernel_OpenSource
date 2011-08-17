/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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
#include <mach/clk.h>

#include "rpm_resources.h"
#include "clock.h"
#include "clock-rpm.h"

static DEFINE_SPINLOCK(rpm_clock_lock);

static int rpm_clk_enable(struct clk *clk)
{
	unsigned long flags;
	struct msm_rpm_iv_pair iv;
	int rc = 0;
	struct rpm_clk *r = to_rpm_clk(clk);
	unsigned this_khz, this_sleep_khz;
	unsigned peer_khz = 0, peer_sleep_khz = 0;
	struct rpm_clk *peer = r->peer;

	spin_lock_irqsave(&rpm_clock_lock, flags);

	this_khz = r->last_set_khz;
	/* Don't send requests to the RPM if the rate has not been set. */
	if (this_khz == 0)
		goto out;

	this_sleep_khz = r->last_set_sleep_khz;

	iv.id = r->rpm_clk_id;

	/* Take peer clock's rate into account only if it's enabled. */
	if (peer->enabled) {
		peer_khz = peer->last_set_khz;
		peer_sleep_khz = peer->last_set_sleep_khz;
	}

	iv.value = max(this_khz, peer_khz);
	rc = msm_rpmrs_set_noirq(MSM_RPM_CTX_SET_0, &iv, 1);
	if (rc)
		goto out;

	iv.value = max(this_sleep_khz, peer_sleep_khz);
	rc = msm_rpmrs_set_noirq(MSM_RPM_CTX_SET_SLEEP, &iv, 1);
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
		struct msm_rpm_iv_pair iv;
		struct rpm_clk *peer = r->peer;
		unsigned peer_khz = 0, peer_sleep_khz = 0;
		int rc;

		iv.id = r->rpm_clk_id;

		/* Take peer clock's rate into account only if it's enabled. */
		if (peer->enabled) {
			peer_khz = peer->last_set_khz;
			peer_sleep_khz = peer->last_set_sleep_khz;
		}

		iv.value = peer_khz;
		rc = msm_rpmrs_set_noirq(MSM_RPM_CTX_SET_0, &iv, 1);
		if (rc)
			goto out;

		iv.value = peer_sleep_khz;
		rc = msm_rpmrs_set_noirq(MSM_RPM_CTX_SET_SLEEP, &iv, 1);
	}
	r->enabled = false;
out:
	spin_unlock_irqrestore(&rpm_clock_lock, flags);

	return;
}

static void rpm_clk_auto_off(struct clk *clk)
{
	/* Not supported */
}

static int rpm_clk_set_min_rate(struct clk *clk, unsigned rate)
{
	unsigned long flags;
	struct rpm_clk *r = to_rpm_clk(clk);
	unsigned this_khz, this_sleep_khz;
	int rc = 0;

	this_khz = DIV_ROUND_UP(rate, 1000);

	spin_lock_irqsave(&rpm_clock_lock, flags);

	/* Ignore duplicate requests. */
	if (r->last_set_khz == this_khz)
		goto out;

	/* Active-only clocks don't care what the rate is during sleep. So,
	 * they vote for zero. */
	if (r->active_only)
		this_sleep_khz = 0;
	else
		this_sleep_khz = this_khz;

	if (r->enabled) {
		struct msm_rpm_iv_pair iv;
		struct rpm_clk *peer = r->peer;
		unsigned peer_khz = 0, peer_sleep_khz = 0;

		iv.id = r->rpm_clk_id;

		/* Take peer clock's rate into account only if it's enabled. */
		if (peer->enabled) {
			peer_khz = peer->last_set_khz;
			peer_sleep_khz = peer->last_set_sleep_khz;
		}

		iv.value = max(this_khz, peer_khz);
		rc = msm_rpmrs_set_noirq(MSM_RPM_CTX_SET_0, &iv, 1);
		if (rc)
			goto out;

		iv.value = max(this_sleep_khz, peer_sleep_khz);
		rc = msm_rpmrs_set_noirq(MSM_RPM_CTX_SET_SLEEP, &iv, 1);
	}
	if (!rc) {
		r->last_set_khz = this_khz;
		r->last_set_sleep_khz = this_sleep_khz;
	}

out:
	spin_unlock_irqrestore(&rpm_clock_lock, flags);

	return rc;
}

static unsigned rpm_clk_get_rate(struct clk *clk)
{
	struct rpm_clk *r = to_rpm_clk(clk);
	struct msm_rpm_iv_pair iv = { r->rpm_status_id };
	int rc;

	rc  = msm_rpm_get_status(&iv, 1);
	if (rc < 0)
		return rc;
	return iv.value * 1000;
}

static int rpm_clk_is_enabled(struct clk *clk)
{
	return !!(rpm_clk_get_rate(clk));
}

static long rpm_clk_round_rate(struct clk *clk, unsigned rate)
{
	/* Not supported. */
	return rate;
}

static bool rpm_clk_is_local(struct clk *clk)
{
	return false;
}

struct clk_ops clk_ops_rpm = {
	.enable = rpm_clk_enable,
	.disable = rpm_clk_disable,
	.auto_off = rpm_clk_auto_off,
	.set_min_rate = rpm_clk_set_min_rate,
	.get_rate = rpm_clk_get_rate,
	.is_enabled = rpm_clk_is_enabled,
	.round_rate = rpm_clk_round_rate,
	.is_local = rpm_clk_is_local,
};
