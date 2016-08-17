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
#include <linux/slab.h>
#include "edp_internal.h"

/*
 * This file implements the (1) LRR: least recently requested (2) MRR:
 * most recently requested and (3) RR: round robin governors.
 *
 * Since they are all based on some timestamps, we use a simple list
 * (the 'temporal list') for ordering the clients according to the main
 * selection criteria. This list is maintained in such a way that
 * throttle-victims appear at the tail and promotions are done from the
 * head.
 */

static void throttle(struct edp_client *client);

/*  Temporal list is manager specific */
static int temporal_start(struct edp_manager *m)
{
	struct list_head *head;
	struct edp_client *c;

	head = kzalloc(sizeof(*head), GFP_KERNEL);
	if (!head)
		return -ENOMEM;

	INIT_LIST_HEAD(head);
	m->gov_data = head;

	list_for_each_entry(c, &m->clients, link) {
		if (req_index(c) < c->e0_index)
			list_add(&c->glnk, head);
	}

	return 0;
}

static void temporal_stop(struct edp_manager *m)
{
	kfree(m->gov_data);
	m->gov_data = NULL;
}

/*
 * We need to remember only those clients that can either be throttled
 * or promoted - this way, we have a smaller list.
 */
static void lrr_update_request(struct edp_client *client,
		const unsigned int *req)
{
	struct list_head *head;

	if (req_index(client) < client->e0_index)
		list_del(&client->glnk);

	edp_default_update_request(client, req, throttle);

	if (req_index(client) < client->e0_index) {
		head = client->manager->gov_data;
		list_add(&client->glnk, head);
	}
}

/*
 * We need to remember only those clients that can either be throttled
 * or promoted - this way, we have a smaller list.
 */
static void mrr_update_request(struct edp_client *client,
		const unsigned int *req)
{
	struct list_head *head;

	if (req_index(client) < client->e0_index)
		list_del(&client->glnk);

	edp_default_update_request(client, req, throttle);

	if (req_index(client) < client->e0_index) {
		head = client->manager->gov_data;
		list_add_tail(&client->glnk, head);
	}
}

static void rr_update_request(struct edp_client *client,
		const unsigned int *req)
{
	struct list_head *head;

	/* new entry */
	if (!client->req) {
		head = client->manager->gov_data;
		list_add(&client->glnk, head);
	}

	edp_default_update_request(client, req, throttle);
}

static void temporal_promote(struct edp_manager *m)
{
	struct list_head *head = m->gov_data;
	struct edp_client *c;
	unsigned int i;

	list_for_each_entry(c, head, glnk) {
		if (req_level(c) <= cur_level(c) || !c->notify_promotion)
			continue;

		i = edp_promotion_point(c, m->remaining);
		if (i == cur_index(c))
			continue;

		m->remaining -= c->states[i] - cur_level(c);
		c->cur = c->states + i;
		if (c->cur == c->req)
			m->num_denied--;

		c->notify_promotion(i, c->private_data);
		if (!m->remaining || !m->num_denied)
			return;
	}
}

#define DEFINE_TEMPORAL_GOV(_gov, _name, _func)	\
	struct edp_governor _gov = {	\
		.name = _name,	\
		.owner = THIS_MODULE,	\
		.start = temporal_start,	\
		.stop = temporal_stop,	\
		.update_request = _func,	\
		.update_loans = edp_default_update_loans,	\
		.promote = temporal_promote	\
	};

static DEFINE_TEMPORAL_GOV(lrr_governor, "least_recent", lrr_update_request);
static DEFINE_TEMPORAL_GOV(mrr_governor, "most_recent", mrr_update_request);
static DEFINE_TEMPORAL_GOV(rr_governor, "round_robin", rr_update_request);

static void throttle(struct edp_client *client)
{
	struct edp_manager *m = client->manager;
	unsigned int required = req_level(client) - cur_level(client);
	struct list_head *head = m->gov_data;
	struct edp_client *n;
	struct edp_client *c;
	unsigned int bal;

	bal = m->remaining;
	n = NULL;

	list_for_each_entry_reverse(c, head, glnk) {
		if (cur_level(c) > e0_level(c) && c != client) {
			c->gwt = edp_throttling_point(c, required - bal);
			bal += cur_level(c) - c->states[c->gwt];
			n = c;
			if (bal >= required)
				break;
		}
	}

	c = n;
	bal = m->remaining;
	if (!c)
		goto finish;

	/* use the safe version as we might be re-arraging the list */
	list_for_each_entry_safe_from(c, n, head, glnk) {
		if (cur_level(c) <= e0_level(c) || c == client ||
				c->gwt == cur_index(c))
			continue;

		c->throttle(c->gwt, c->private_data);
		bal += cur_level(c) - c->states[c->gwt];
		if (c->cur == c->req)
			m->num_denied++;
		c->cur = c->states + c->gwt;

		/* for RR, move this client to the head */
		if (m->gov == &rr_governor)
			list_move(&c->glnk, head);
		if (bal >= required)
			break;
	}

finish:
	m->remaining = bal + cur_level(client);
	client->cur = client->states + edp_promotion_point(client, bal);
	m->remaining -= cur_level(client);
}

static int __init temporal_init(void)
{
	int ret = 0;
	int r;

	r = edp_register_governor(&lrr_governor);
	if (r) {
		pr_err("least_recent governor registration failed: %d\n", r);
		ret = r;
	}

	r = edp_register_governor(&mrr_governor);
	if (r) {
		pr_err("most_recent governor registration failed: %d\n", r);
		ret = r;
	}

	r = edp_register_governor(&rr_governor);
	if (r) {
		pr_err("round_robin governor registration failed: %d\n", r);
		ret = r;
	}

	return ret;
}
postcore_initcall(temporal_init);
