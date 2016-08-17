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

/*
 * Calculate the approvable E-state for the requesting client.
 * Non-negative E-state requests are always approved.  A (higher)
 * negative E-state request is approved only if lower priority clients
 * can be throttled (at most to E0) in order to recover the necessary
 * power deficit. If this can not be met, a lower E-state is approved
 * (at least E0).
 */
static unsigned int approvable_req(struct edp_client *client)
{
	struct edp_manager *m = client->manager;
	unsigned int old = cur_level(client);
	unsigned int deficit = *client->req - old;
	unsigned int recoverable = m->remaining;
	unsigned int i = req_index(client);
	struct edp_client *p = client;

	if (i >= client->e0_index)
		return i;

	list_for_each_entry_continue(p, &m->clients, link) {
		if (cur_level(p) > e0_level(p)) {
			recoverable += cur_level(p) - e0_level(p);
			if (recoverable >= deficit)
				return i;
		}
	}

	while (i < client->e0_index && recoverable < deficit) {
		i++;
		deficit = client->states[i] - old;
	}

	return i;
}

static void throttle(struct edp_client *client)
{
	unsigned int ar;
	unsigned int deficit;
	struct edp_client *p;
	struct edp_manager *m = client->manager;
	unsigned int pledged = m->remaining;
	unsigned int recovered = m->remaining;

	/* Check if we can satisfy the request as it is */
	ar = approvable_req(client);
	deficit = client->states[ar] - cur_level(client);
	if (m->remaining >= deficit)
		goto ret;

	/*
	 * We do the throttling in two steps: first we will identify and
	 * mark the clients starting from the lower priority ones. We
	 * stop when we find the highest priority client that should be
	 * throttled.
	 */
	list_for_each_entry_reverse(p, &m->clients, link) {
		if (p == client || cur_level(p) <= e0_level(p))
			continue;

		p->gwt = edp_throttling_point(p, deficit - pledged);
		pledged += cur_level(p) - p->states[p->gwt];
		if (pledged >= deficit)
			break;
	}

	/*
	 * By now, we are guaranteed to have at least the adjusted
	 * deficit - may be even more.
	 */
	WARN_ON(pledged < deficit);

	/*
	 * We now do the actual throttling starting from where we stoped
	 * in step 1 and going in the opposite direction. This way we
	 * can avoid situations where clients are throttled needlessly
	 * and promoted back immediately.
	 */
	list_for_each_entry_from(p, &m->clients, link) {
		if (p == client || cur_level(p) <= e0_level(p) ||
				p->gwt == cur_index(p))
			continue;

		p->throttle(p->gwt, p->private_data);
		recovered += cur_level(p) - p->states[p->gwt];
		if (p->cur == p->req)
			m->num_denied++;

		p->cur = p->states + p->gwt;
		if (recovered >= deficit)
			break;
	}

ret:
	client->cur = client->states + ar;
	m->remaining = recovered - deficit;
}

static void prio_update_request(struct edp_client *client,
		const unsigned int *req)
{
	edp_default_update_request(client, req, throttle);
}

static void prio_promote(struct edp_manager *mgr)
{
	struct edp_client *p;
	unsigned int delta;
	unsigned int pp;

	list_for_each_entry(p, &mgr->clients, link) {
		if (req_level(p) <= cur_level(p) || !p->notify_promotion)
			continue;

		delta = req_level(p) - cur_level(p);
		if (delta > mgr->remaining)
			delta = mgr->remaining;

		pp = edp_promotion_point(p, delta);
		if (pp == cur_index(p))
			continue;

		mgr->remaining -= p->states[pp] - cur_level(p);
		p->cur = p->states + pp;

		if (p->cur == p->req)
			mgr->num_denied--;

		p->notify_promotion(pp, p->private_data);
		if (!mgr->remaining || !mgr->num_denied)
			return;
	}
}

static struct edp_governor prio_governor = {
	.name = "priority",
	.owner = THIS_MODULE,
	.update_request = prio_update_request,
	.update_loans = edp_default_update_loans,
	.promote = prio_promote
};

static int __init prio_init(void)
{
	return edp_register_governor(&prio_governor);
}
postcore_initcall(prio_init);
