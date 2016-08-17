/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/edp.h>
#include "edp_internal.h"

static unsigned int approvable_req(struct edp_client *c, unsigned int net)
{
	unsigned int fair_level;
	unsigned int step;
	unsigned int cl;

	if (req_index(c) >= c->e0_index)
		return req_index(c);

	cl = cur_level(c);
	fair_level = c->manager->max * e0_level(c) / net;
	step = max(fair_level, cl + c->manager->remaining) - cl;
	return edp_promotion_point(c, step);
}

static unsigned int net_e0(struct edp_client *client)
{
	struct edp_client *c;
	unsigned int net = 0;

	list_for_each_entry(c, &client->manager->clients, link)
		net += e0_level(c);

	return net;
}

static struct edp_client *throttle_pledge(struct edp_client *client,
		unsigned int required, unsigned int net,
		unsigned int *pledged)
{
	struct edp_manager *m = client->manager;
	unsigned int deficit = required - m->remaining;
	struct edp_client *c;
	unsigned int step;

	*pledged = m->remaining;

	list_for_each_entry_reverse(c, &m->clients, link) {
		if (c == client || cur_level(c) <= e0_level(c))
			continue;

		step = (deficit * e0_level(c) + net - 1) / net;
		c->gwt = edp_throttling_point(c, step ?: 1);
		*pledged += cur_level(c) - c->states[c->gwt];
		if (*pledged >= required)
			break;
	}

	WARN_ON(*pledged < required);
	return c;
}

static void throttle_recover(struct edp_client *client, struct edp_client *tp,
		unsigned int required)
{
	struct edp_manager *m = client->manager;
	unsigned int recovered = m->remaining;

	list_for_each_entry_from(tp, &m->clients, link) {
		if (tp == client || cur_level(tp) <= e0_level(tp) ||
				tp->gwt == cur_index(tp))
			continue;

		tp->throttle(tp->gwt, tp->private_data);
		recovered += cur_level(tp) - tp->states[tp->gwt];
		if (tp->cur == tp->req)
			m->num_denied++;

		tp->cur = tp->states + tp->gwt;
		if (recovered >= required)
			return;
	}
}

static void throttle(struct edp_client *client)
{
	struct edp_manager *m = client->manager;
	struct edp_client *tp;
	unsigned int ar;
	unsigned int pledged;
	unsigned int required;
	unsigned int net;

	net = net_e0(client);
	if (!net) {
		WARN_ON(1);
		return;
	}

	ar = approvable_req(client, net);
	required = client->states[ar] - cur_level(client);

	if (required <= m->remaining) {
		client->cur = client->states + ar;
		m->remaining -= required;
		return;
	}

	tp = throttle_pledge(client, required, net, &pledged);

	/* E-states are discrete - we may get more than we asked for */
	if (pledged > required && ar != req_index(client)) {
		ar = edp_promotion_point(client, pledged);
		required = client->states[ar] - cur_level(client);
	}

	throttle_recover(client, tp, required);
	client->cur = client->states + ar;
	m->remaining = pledged - required;
}

static void fair_update_request(struct edp_client *client,
		const unsigned int *req)
{
	edp_default_update_request(client, req, throttle);
}

static unsigned int fair_promotion_point(struct edp_client *c,
		unsigned int step, unsigned int max)
{
	unsigned int lim = cur_level(c) + step;
	unsigned int ci = cur_index(c);
	unsigned int i = req_index(c);

	while (i < ci && c->states[i] > lim)
		i++;

	/*
	 * While being throttled, we probably contributed more than our
	 * fare share - so take the ceiling E-state here
	 */
	if (c->states[i] < lim && i > req_index(c)) {
		if (c->states[i - 1] <= cur_level(c) + max)
			i--;
	}

	return i;
}

static unsigned int promotion_pledge(struct edp_manager *m, unsigned int net)
{
	unsigned int budget = m->remaining;
	unsigned int unpledged = m->remaining;
	unsigned int denied = m->num_denied;
	struct edp_client *c;
	unsigned int step;

	list_for_each_entry(c, &m->clients, link) {
		if (req_level(c) <= cur_level(c) || !c->notify_promotion)
			continue;

		step = (e0_level(c) * budget + net - 1) / net;
		step = min(step, unpledged);

		c->gwt = fair_promotion_point(c, step, unpledged);
		unpledged -= c->states[c->gwt] - cur_level(c);
		if (req_index(c) == c->gwt)
			denied--;
		if (!unpledged || !denied)
			break;
	}

	return unpledged;
}

static void fair_promote(struct edp_manager *mgr)
{
	unsigned int net = 0;
	struct edp_client *c;
	unsigned int step;
	unsigned int pp;
	unsigned int unpledged;

	list_for_each_entry(c, &mgr->clients, link) {
		if (req_level(c) > cur_level(c) && c->notify_promotion) {
			net += e0_level(c);
			c->gwt = cur_index(c);
		}
	}

	/* if the net is 0, fall back on priority */
	unpledged = net ? promotion_pledge(mgr, net) : mgr->remaining;

	list_for_each_entry(c, &mgr->clients, link) {
		if (req_level(c) <= cur_level(c) || !c->notify_promotion ||
				c->gwt == cur_index(c))
			continue;

		pp = c->gwt;

		/* make sure that the unpledged current is not  wasted */
		if (unpledged && req_index(c) != pp) {
			step = c->states[pp] - cur_level(c) + unpledged;
			pp = edp_promotion_point(c, step);
			unpledged -= c->states[pp] - c->states[c->gwt];
		}

		mgr->remaining -= c->states[pp] - cur_level(c);
		c->cur = c->states + pp;
		if (c->cur == c->req)
			mgr->num_denied--;

		c->notify_promotion(pp, c->private_data);
		if (!mgr->remaining || !mgr->num_denied)
			return;
	}
}

static struct edp_governor fair_governor = {
	.name = "fair",
	.owner = THIS_MODULE,
	.update_request = fair_update_request,
	.update_loans = edp_default_update_loans,
	.promote = fair_promote
};

static int __init fair_init(void)
{
	return edp_register_governor(&fair_governor);
}
postcore_initcall(fair_init);
