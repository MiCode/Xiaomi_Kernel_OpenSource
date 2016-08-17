/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
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

static inline unsigned int cur_overage(struct edp_client *c)
{
	unsigned int cl = cur_level(c);
	unsigned int el = e0_level(c);
	return cl > el ? cl - el : 0;
}

static inline unsigned int req_overage(struct edp_client *c)
{
	unsigned int rl = req_level(c);
	unsigned int el = e0_level(c);
	return rl > el ? rl - el : 0;
}

/*
 * Find the maximum that we can allocate for this client. Since we are
 * using a propotional allocation, ensure that the allowed budget is
 * fare to other clients. Note that the maximum E-state level is used as
 * reference for normalizing client requests (E0 can not be used since
 * it could be zer0 for some clients)
 */
static unsigned int approvable_req(struct edp_client *c,
		unsigned int net_overage, unsigned int net_max)
{
	unsigned int tot_overage;
	unsigned int tot_max;
	unsigned int fair_level;
	unsigned int step;

	if (req_index(c) >= c->e0_index)
		return req_index(c);

	tot_overage  = net_overage + cur_overage(c) + c->manager->remaining;
	tot_max = net_max + c->states[0];
	fair_level = tot_overage * c->states[0] / tot_max + e0_level(c);
	step = max(fair_level, cur_level(c) + c->manager->remaining) -
			cur_level(c);

	return edp_promotion_point(c, step);
}

static void find_net(struct edp_client *client, unsigned int *net_overage,
		unsigned int *net_max)
{
	struct edp_client *c;
	struct edp_manager *m = client->manager;

	*net_overage = 0;
	*net_max = 0;

	list_for_each_entry(c, &m->clients, link) {
		if (c != client && cur_level(c) > e0_level(c)) {
			*net_overage += cur_overage(c);
			*net_max += c->states[0];
		}
	}
}

static struct edp_client *throttle_pledge(struct edp_client *client,
		unsigned int required, unsigned int net_overage,
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

		step = (deficit * cur_overage(c) +
				net_overage - 1) / net_overage;
		c->gwt = edp_throttling_point(c, step);
		*pledged += cur_level(c) - c->states[c->gwt];
		if (*pledged >= required)
			return c;
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
	unsigned int net_overage;
	unsigned int net_max;

	find_net(client, &net_overage, &net_max);
	ar = approvable_req(client, net_overage, net_max);
	required = client->states[ar] - cur_level(client);

	if (required <= m->remaining) {
		client->cur = client->states + ar;
		m->remaining -= required;
		return;
	}

	tp = throttle_pledge(client, required, net_overage, &pledged);

	/* E-states are discrete - we may get more than we asked for */
	if (pledged > required && ar != req_index(client)) {
		ar = edp_promotion_point(client, pledged);
		required = client->states[ar] - cur_level(client);
	}

	throttle_recover(client, tp, required);
	client->cur = client->states + ar;
	m->remaining = pledged - required;
}

static void overage_update_request(struct edp_client *client,
		const unsigned int *req)
{
	edp_default_update_request(client, req, throttle);
}

static unsigned int overage_promotion_point(struct edp_client *c,
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

static void overage_promote(struct edp_manager *mgr)
{
	unsigned int budget = mgr->remaining;
	unsigned int net_overage = 0;
	struct edp_client *c;
	unsigned int step;
	unsigned int pp;

	list_for_each_entry(c, &mgr->clients, link) {
		if (req_level(c) > cur_level(c) && c->notify_promotion)
			net_overage += req_overage(c);
	}

	/* Guarding against division-by-zero */
	if (!net_overage) {
		WARN_ON(1);
		return;
	}

	list_for_each_entry(c, &mgr->clients, link) {
		if (req_level(c) <= cur_level(c) || !c->notify_promotion)
			continue;

		step = (req_overage(c) * budget +
				net_overage - 1) / net_overage;
		if (step > mgr->remaining)
			step = mgr->remaining;

		pp = overage_promotion_point(c, step, mgr->remaining);
		if (pp == cur_index(c))
			continue;

		mgr->remaining -= c->states[pp] - cur_level(c);
		c->cur = c->states + pp;

		if (c->cur == c->req)
			mgr->num_denied--;
		c->notify_promotion(pp, c->private_data);
		if (!mgr->remaining || !mgr->num_denied)
			return;
	}
}

static struct edp_governor overage_governor = {
	.name = "overage",
	.owner = THIS_MODULE,
	.update_request = overage_update_request,
	.update_loans = edp_default_update_loans,
	.promote = overage_promote
};

static int __init overage_init(void)
{
	return edp_register_governor(&overage_governor);
}
postcore_initcall(overage_init);
