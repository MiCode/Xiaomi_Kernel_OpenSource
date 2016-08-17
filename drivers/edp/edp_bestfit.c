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

static struct list_head hilist;
static struct list_head lolist_hash[10];

static void init_hash(void)
{
	int i;
	INIT_LIST_HEAD(&hilist);
	for (i = 0; i < ARRAY_SIZE(lolist_hash); i++)
		INIT_LIST_HEAD(lolist_hash + i);
}

/* Return the minimum state that we must approve */
static unsigned int min_ai(struct edp_client *c)
{
	unsigned int ri = req_index(c);
	if (ri >= c->e0_index)
		return ri;
	return min(cur_index(c), c->e0_index);
}

static struct edp_client *lolist_hi_entry(int hash)
{
	int i;

	for (i = hash; i < ARRAY_SIZE(lolist_hash); i++) {
		if (!list_empty(lolist_hash + i))
			return list_first_entry(lolist_hash + i,
					struct edp_client, glnk);
	}

	return NULL;
}

static struct edp_client *lolist_li_entry(int hash)
{
	int i;

	for (i = hash; i >= 0; i--) {
		if (!list_empty(lolist_hash + i))
			return list_first_entry(lolist_hash + i,
					struct edp_client, glnk);
	}

	return NULL;
}

static struct edp_client *lolist_entry(int hash, bool hifirst)
{
	struct edp_client *c;

	if (hifirst)
		c = lolist_hi_entry(hash) ?: lolist_li_entry(hash - 1);
	else
		c = lolist_li_entry(hash) ?: lolist_hi_entry(hash + 1);

	return c;
}

/*
 * Use hashing to fasten up the lookup for bestfit (we might have to do
 * multiple passes). If a single client can supply the minimum
 * requirement, put them into hilist. Otherwise, compute a simple hash
 * from the ratio of (cur - E0) to the minimum requirement and add to
 * the corresponding lolist queue. Entries are added to the list head
 * so that lower priority clients are throttled first.
 */
static void prep_throttle_hash(struct edp_client *client, unsigned int mn)
{
	struct edp_manager *m = client->manager;
	struct edp_client *c;
	unsigned int i;
	unsigned int more;

	init_hash();

	list_for_each_entry(c, &m->clients, link) {
		if (c == client || cur_level(c) <= e0_level(c))
			continue;

		more = cur_level(c) - e0_level(c);

		if (more + m->remaining < mn) {
			i = more * ARRAY_SIZE(lolist_hash) / mn;
			list_add(&c->glnk, lolist_hash + i);
		} else {
			list_add(&c->glnk, &hilist);
		}
	}
}

/*
 * Find the bestfit point between the requesting client and a potential
 * throttle-victim. Choose the one with lowest remaining current.
 */
static unsigned int bestfit_point(struct edp_client *rc, struct edp_client *c,
		unsigned int mn, unsigned int *opt_bal)
{
	unsigned int ai = cur_index(rc);
	unsigned int ri = req_index(rc);
	unsigned int cl = cur_level(rc);
	unsigned int step;
	unsigned int bal;
	unsigned int i;
	unsigned int j;

	*opt_bal = rc->manager->max;

	for (i = cur_index(c) + 1; i <= c->e0_index && ai > ri; i++) {
		step = rc->manager->remaining + *c->cur - c->states[i];
		if (step < mn)
			continue;

		j = edp_promotion_point(rc, step);
		bal = step - (rc->states[j] - cl);
		if (bal < *opt_bal) {
			*opt_bal = bal;
			c->gwt = i;
			ai = j;
		}
	}

	return ai;
}

static struct edp_client *throttle_bestfit_hi(struct edp_client *rc,
		unsigned int mn, unsigned int *bfi, unsigned int *opt_bal)
{
	struct edp_client *c;
	struct edp_client *opt_c;
	unsigned int bal;
	unsigned int i;

	if (list_empty(&hilist))
		return NULL;

	opt_c = NULL;
	*opt_bal = rc->manager->max;

	list_for_each_entry(c, &hilist, glnk) {
		i = bestfit_point(rc, c, mn, &bal);
		if (bal < *opt_bal) {
			*bfi = i;
			*opt_bal = bal;
			opt_c = c;
		}
	}

	WARN_ON(!opt_c);
	return opt_c;
}

static unsigned int throttle_recover(struct edp_manager *m, unsigned int mn)
{
	struct edp_client *c;
	unsigned int i;
	unsigned int tp;
	unsigned int step;
	unsigned int rsum = m->remaining;

	while (rsum < mn) {
		i = ((mn - rsum) * ARRAY_SIZE(lolist_hash) + mn - 1) / mn;
		c = lolist_entry(i, true);
		if (!c)
			break;

		list_del(&c->glnk);
		step = min(cur_level(c) - e0_level(c), mn - rsum);
		tp = edp_throttling_point(c, step);
		if (tp == cur_index(c))
			continue;

		rsum += cur_level(c) - c->states[tp];
		c->throttle(tp, c->private_data);
		if (c->cur == c->req)
			m->num_denied++;
		c->cur = c->states + tp;
	}

	WARN_ON(rsum < mn);
	return rsum;
}

static void throttle(struct edp_client *client)
{
	struct edp_manager *m = client->manager;
	unsigned int ai;
	unsigned int mn;
	struct edp_client *c;
	unsigned int balance;

	ai = min_ai(client);
	mn = client->states[ai] - cur_level(client);

	if (mn <= m->remaining) {
		ai = edp_promotion_point(client, m->remaining);
		m->remaining -= client->states[ai] - cur_level(client);
		client->cur = client->states + ai;
		return;
	}

	prep_throttle_hash(client, mn);
	c = throttle_bestfit_hi(client, mn, &ai, &balance);

	if (c) {
		c->throttle(c->gwt, c->private_data);
		if (c->cur == c->req)
			m->num_denied++;
		m->remaining = balance;
		c->cur = c->states + c->gwt;
		client->cur = client->states + ai;
		return;
	}

	balance = throttle_recover(m, mn);
	WARN_ON(balance < mn);
	ai = edp_promotion_point(client, balance);
	m->remaining = balance - (client->states[ai] - cur_level(client));
	client->cur = client->states + ai;
}

static void bestfit_update_request(struct edp_client *client,
		const unsigned int *req)
{
	edp_default_update_request(client, req, throttle);
}

static void prep_promotion_hash(struct edp_manager *m)
{
	unsigned int balance = m->remaining;
	struct edp_client *c;
	unsigned int step;
	unsigned int i;

	init_hash();

	list_for_each_entry(c, &m->clients, link) {
		if (req_level(c) <= cur_level(c) || !c->notify_promotion)
			continue;

		step = req_level(c) - cur_level(c);

		/*
		 * Add to the list tail so that higher priority clients
		 * are promoted first
		 */
		if (step < balance) {
			i = step * ARRAY_SIZE(lolist_hash) / balance;
			list_add_tail(&c->glnk, lolist_hash + i);
		} else {
			list_add_tail(&c->glnk, &hilist);
		}
	}
}

static struct edp_client *promotion_bestfit_hi(unsigned int balance)
{
	struct edp_client *c;
	unsigned int i;
	unsigned int step;
	struct edp_client *opt_c = NULL;
	unsigned int opt_bal = balance;

	list_for_each_entry(c, &hilist, glnk) {
		i = edp_promotion_point(c, balance);
		step = c->states[i] - cur_level(c);
		if (balance - step < opt_bal) {
			c->gwt = i;
			opt_c = c;
		}
	}

	return opt_c;
}

static void bestfit_promote(struct edp_manager *mgr)
{
	unsigned int balance = mgr->remaining;
	struct edp_client *c;
	unsigned int i;

	prep_promotion_hash(mgr);
	c = promotion_bestfit_hi(balance);

	if (c) {
		balance -= c->states[c->gwt] - cur_level(c);
		c->cur = c->states + c->gwt;
		if (c->cur == c->req)
			mgr->num_denied--;
		c->notify_promotion(c->gwt, c->private_data);
	}

	while (balance && mgr->num_denied) {
		i = balance * ARRAY_SIZE(lolist_hash) / mgr->remaining;
		if (i)
			i--;
		c = lolist_entry(i, false);
		if (!c)
			break;

		list_del(&c->glnk);
		c->gwt = edp_promotion_point(c, balance);
		if (c->gwt == cur_index(c))
			continue;

		balance -= c->states[c->gwt] - cur_level(c);
		c->cur = c->states + c->gwt;
		if (c->cur == c->req)
			mgr->num_denied--;
		c->notify_promotion(c->gwt, c->private_data);
	}

	mgr->remaining = balance;
}

static struct edp_governor bestfit_governor = {
	.name = "bestfit",
	.owner = THIS_MODULE,
	.update_request = bestfit_update_request,
	.update_loans = edp_default_update_loans,
	.promote = bestfit_promote
};

static int __init bestfit_init(void)
{
	return edp_register_governor(&bestfit_governor);
}
postcore_initcall(bestfit_init);
