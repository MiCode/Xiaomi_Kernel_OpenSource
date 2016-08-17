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
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/edp.h>
#include "edp_internal.h"

DEFINE_MUTEX(edp_lock);
static LIST_HEAD(edp_managers);
LIST_HEAD(edp_governors);

static struct edp_manager *find_manager(const char *name)
{
	struct edp_manager *mgr;

	if (!name)
		return NULL;

	list_for_each_entry(mgr, &edp_managers, link)
		if (!strcmp(mgr->name, name))
			return mgr;

	return NULL;
}

static void promote(struct work_struct *work)
{
	unsigned int prev_denied;
	struct edp_manager *m = container_of(work, struct edp_manager, work);

	mutex_lock(&edp_lock);

	if (m->num_denied && m->remaining) {
		prev_denied = m->num_denied;
		m->gov->promote(m);
		if (prev_denied != m->num_denied)
			sysfs_notify(m->kobj, NULL, "denied");
	}

	mutex_unlock(&edp_lock);
}

void schedule_promotion(struct edp_manager *mgr)
{
	if (mgr->remaining && mgr->num_denied && mgr->gov->promote)
		schedule_work(&mgr->work);
}

int edp_register_manager(struct edp_manager *mgr)
{
	int r = -EEXIST;

	if (!mgr)
		return -EINVAL;
	if (!mgr->max)
		return -EINVAL;

	mutex_lock(&edp_lock);
	if (!find_manager(mgr->name)) {
		list_add_tail(&mgr->link, &edp_managers);
		mgr->registered = true;
		mgr->remaining = mgr->max;
		mgr->gov = NULL;
		mgr->gov_data = NULL;
		INIT_LIST_HEAD(&mgr->clients);
		INIT_WORK(&mgr->work, promote);
		mgr->kobj = NULL;
		edp_manager_add_kobject(mgr);
		manager_add_dentry(mgr);
		r = 0;
	}
	mutex_unlock(&edp_lock);

	return r;
}
EXPORT_SYMBOL(edp_register_manager);

int edp_set_governor_unlocked(struct edp_manager *mgr,
		struct edp_governor *gov)
{
	int r = 0;

	if (mgr ? !mgr->registered : 1)
		return -EINVAL;

	if (mgr->gov) {
		cancel_work_sync(&mgr->work);
		if (mgr->gov->stop)
			mgr->gov->stop(mgr);
		mgr->gov->refcnt--;
		module_put(mgr->gov->owner);
		mgr->gov = NULL;
	}

	if (gov) {
		if (!gov->refcnt)
			return -EINVAL;
		if (!try_module_get(gov->owner))
			return -EINVAL;
		if (gov->start)
			r = gov->start(mgr);
		if (r) {
			module_put(gov->owner);
			WARN_ON(1);
			return r;
		}

		gov->refcnt++;
		mgr->gov = gov;
	}

	return 0;
}

int edp_unregister_manager(struct edp_manager *mgr)
{
	int r = 0;

	if (!mgr)
		return -EINVAL;

	mutex_lock(&edp_lock);
	if (!mgr->registered) {
		r = -ENODEV;
	} else if (!list_empty(&mgr->clients)) {
		r = -EBUSY;
	} else {
		manager_remove_dentry(mgr);
		edp_manager_remove_kobject(mgr);
		edp_set_governor_unlocked(mgr, NULL);
		list_del(&mgr->link);
		mgr->registered = false;
	}
	mutex_unlock(&edp_lock);

	return r;
}
EXPORT_SYMBOL(edp_unregister_manager);

struct edp_manager *edp_get_manager(const char *name)
{
	struct edp_manager *mgr;

	mutex_lock(&edp_lock);
	mgr = find_manager(name);
	mutex_unlock(&edp_lock);

	return mgr;
}
EXPORT_SYMBOL(edp_get_manager);

static struct edp_client *find_client(struct edp_manager *mgr,
		const char *name)
{
	struct edp_client *p;

	if (!name)
		return NULL;

	list_for_each_entry(p, &mgr->clients, link)
		if (!strcmp(p->name, name))
			return p;

	return NULL;
}

unsigned int e0_current_sum(struct edp_manager *mgr)
{
	struct edp_client *p;
	unsigned int sum = 0;

	list_for_each_entry(p, &mgr->clients, link)
		sum += p->states[p->e0_index];

	return sum;
}

static bool states_ok(struct edp_client *client)
{
	int i;

	if (!client->states || !client->num_states ||
			client->e0_index >= client->num_states)
		return false;

	/* state array should be sorted in descending order */
	for (i = 1; i < client->num_states; i++)
		if (client->states[i] > client->states[i - 1])
			return false;

	return client->states[0] ? true : false;
}

/* Keep the list sorted on priority */
static void add_client(struct edp_client *new, struct list_head *head)
{
	struct edp_client *p;

	list_for_each_entry(p, head, link) {
		if (p->priority > new->priority) {
			list_add_tail(&new->link, &p->link);
			return;
		}
	}

	list_add_tail(&new->link, &p->link);
}

int register_client(struct edp_manager *mgr, struct edp_client *client)
{
	if (!mgr || !client)
		return -EINVAL;

	if (!mgr->registered)
		return -ENODEV;

	if (client->manager || find_client(mgr, client->name))
		return -EEXIST;

	if (!states_ok(client) || client->priority > EDP_MIN_PRIO ||
			client->priority < EDP_MAX_PRIO ||
			(client->e0_index && !client->throttle))
		return -EINVAL;

	/* make sure that we can satisfy E0 for all registered clients */
	if (e0_current_sum(mgr) + client->states[client->e0_index] > mgr->max)
		return -E2BIG;

	add_client(client, &mgr->clients);
	client->manager = mgr;
	client->req = NULL;
	client->cur = NULL;
	INIT_LIST_HEAD(&client->borrowers);
	client->num_borrowers = 0;
	client->num_loans = 0;
	client->ithreshold = client->states[0];
	client->kobj = NULL;
	edp_client_add_kobject(client);
	client_add_dentry(client);

	return 0;
}

int edp_register_client(struct edp_manager *mgr, struct edp_client *client)
{
	int r;

	mutex_lock(&edp_lock);
	r = register_client(mgr, client);
	mutex_unlock(&edp_lock);

	return r;
}
EXPORT_SYMBOL(edp_register_client);

static void update_loans(struct edp_client *client)
{
	struct edp_governor *gov;
	gov = client->manager ? client->manager->gov : NULL;
	if (gov && client->cur && !list_empty(&client->borrowers)) {
		if (gov->update_loans && *client->cur > client->ithreshold)
			gov->update_loans(client);
	}
}

/* generic default implementation */
void edp_default_update_request(struct edp_client *client,
		const unsigned int *req,
		void (*throttle)(struct edp_client *))
{
	struct edp_manager *m = client->manager;
	unsigned int old = cur_level(client);
	unsigned int new = req ? *req : 0;
	bool was_denied = client->cur != client->req;

	client->req = req;

	if (new < old) {
		client->cur = req;
		m->remaining += old - new;
	} else if (new - old <= m->remaining) {
		client->cur = req;
		m->remaining -= new - old;
	} else {
		throttle(client);
	}

	if (was_denied && client->cur == client->req)
		m->num_denied--;
	else if (!was_denied && client->cur != client->req)
		m->num_denied++;
}

/* generic default implementation */
void edp_default_update_loans(struct edp_client *lender)
{
	unsigned int size = *lender->cur - lender->ithreshold;
	struct loan_client *p;

	list_for_each_entry(p, &lender->borrowers, link) {
		if (size != p->size) {
			p->size = p->client->notify_loan_update(
				size, lender, p->client->private_data);
			WARN_ON(p->size > size);
		}

		size -= min(p->size, size);
		if (!size)
			return;
	}
}

unsigned int edp_throttling_point(struct edp_client *c, unsigned int deficit)
{
	unsigned int lim;
	unsigned int i;

	if (cur_level(c) - e0_level(c) <= deficit)
		return c->e0_index;

	lim = cur_level(c) - deficit;
	i = cur_index(c);
	while (i < c->e0_index && c->states[i] > lim)
		i++;

	return i;
}

unsigned int edp_promotion_point(struct edp_client *c, unsigned int step)
{
	unsigned int limit = cur_level(c) + step;
	unsigned int ci = cur_index(c);
	unsigned int i = req_index(c);

	while (i < ci && c->states[i] > limit)
		i++;

	WARN_ON(i >= c->num_states);
	return i;
}

static int mod_request(struct edp_client *client, const unsigned int *req)
{
	struct edp_manager *m = client->manager;
	unsigned int prev_remain = m->remaining;
	unsigned int prev_denied = m->num_denied;

	if (!m->gov)
		return -ENODEV;

	m->gov->update_request(client, req);
	update_loans(client);

	/* Do not block calling clients for promotions */
	if (m->remaining > prev_remain)
		schedule_promotion(m);

	if (m->num_denied != prev_denied)
		sysfs_notify(m->kobj, NULL, "denied");

	return 0;
}

static void del_borrower(struct edp_client *lender, struct loan_client *pcl)
{
	pcl->client->notify_loan_close(lender, pcl->client->private_data);
	lender->num_borrowers--;
	pcl->client->num_loans--;
	list_del(&pcl->link);
	kfree(pcl);
}

static void close_all_loans(struct edp_client *client)
{
	struct loan_client *p;

	while (!list_empty(&client->borrowers)) {
		p = list_first_entry(&client->borrowers, struct loan_client,
				link);
		del_borrower(client, p);
	}
}

static inline bool registered_client(struct edp_client *client)
{
	return client ? client->manager : false;
}

int unregister_client(struct edp_client *client)
{
	if (!registered_client(client))
		return -EINVAL;

	if (client->num_loans)
		return -EBUSY;

	client_remove_dentry(client);
	edp_client_remove_kobject(client);
	close_all_loans(client);
	mod_request(client, NULL);
	list_del(&client->link);
	client->manager = NULL;

	return 0;
}

int edp_unregister_client(struct edp_client *client)
{
	int r;

	mutex_lock(&edp_lock);
	r = unregister_client(client);
	mutex_unlock(&edp_lock);

	return r;
}
EXPORT_SYMBOL(edp_unregister_client);

int edp_update_client_request_unlocked(struct edp_client *client,
		unsigned int req, int *approved)
{
	int r;

	if (!registered_client(client))
		return -EINVAL;

	if (req >= client->num_states)
		return -EINVAL;

	r = mod_request(client, client->states + req);
	if (!r && approved)
		*approved = client->cur - client->states;

	return r;
}

int edp_update_client_request(struct edp_client *client, unsigned int req,
		unsigned int *approved)
{
	int r;

	mutex_lock(&edp_lock);
	r = edp_update_client_request_unlocked(client, req, approved);
	mutex_unlock(&edp_lock);

	return r;
}
EXPORT_SYMBOL(edp_update_client_request);

static struct edp_client *get_client(const char *name)
{
	struct edp_client *client;
	struct edp_manager *mgr;

	if (!name)
		return NULL;

	list_for_each_entry(mgr, &edp_managers, link) {
		client = find_client(mgr, name);
		if (client)
			return client;
	}

	return NULL;
}

struct edp_client *edp_get_client(const char *name)
{
	struct edp_client *client;

	mutex_lock(&edp_lock);
	client = get_client(name);
	mutex_unlock(&edp_lock);

	return client;
}
EXPORT_SYMBOL(edp_get_client);

static struct loan_client *find_borrower(struct edp_client *lender,
		struct edp_client *borrower)
{
	struct loan_client *p;

	list_for_each_entry(p, &lender->borrowers, link)
		if (p->client == borrower)
			return p;
	return NULL;
}

/* Keep the list sorted on priority */
static void add_borrower(struct loan_client *new, struct list_head *head)
{
	struct loan_client *p;

	list_for_each_entry(p, head, link) {
		if (p->client->priority > new->client->priority) {
			list_add_tail(&new->link, &p->link);
			return;
		}
	}

	list_add_tail(&new->link, &p->link);
}

static int register_loan(struct edp_client *lender, struct edp_client *borrower)
{
	struct loan_client *p;

	if (!registered_client(lender) || !registered_client(borrower))
		return -EINVAL;

	if (lender->manager != borrower->manager ||
			!borrower->notify_loan_update ||
			!borrower->notify_loan_close)
		return -EINVAL;

	if (find_borrower(lender, borrower))
		return -EEXIST;

	if (lender->num_borrowers >= lender->max_borrowers)
		return -EBUSY;

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	p->client = borrower;
	lender->num_borrowers++;
	borrower->num_loans++;
	add_borrower(p, &lender->borrowers);

	update_loans(lender);
	return 0;
}

int edp_register_loan(struct edp_client *lender, struct edp_client *borrower)
{
	int r;

	mutex_lock(&edp_lock);
	r = register_loan(lender, borrower);
	mutex_unlock(&edp_lock);

	return r;
}
EXPORT_SYMBOL(edp_register_loan);

static int unregister_loan(struct edp_client *lender,
		struct edp_client *borrower)
{
	struct loan_client *p;

	if (!registered_client(lender) || !registered_client(borrower))
		return -EINVAL;

	p = find_borrower(lender, borrower);
	if (!p)
		return -EINVAL;

	del_borrower(lender, p);
	update_loans(lender);
	return 0;
}

int edp_unregister_loan(struct edp_client *lender, struct edp_client *borrower)
{
	int r;

	mutex_lock(&edp_lock);
	r = unregister_loan(lender, borrower);
	mutex_unlock(&edp_lock);

	return r;
}
EXPORT_SYMBOL(edp_unregister_loan);

int edp_update_loan_threshold_unlocked(struct edp_client *client,
		unsigned int threshold)
{
	if (!registered_client(client))
		return -EINVAL;

	client->ithreshold = threshold;
	update_loans(client);
	return 0;
}

int edp_update_loan_threshold(struct edp_client *client, unsigned int threshold)
{
	int r;

	mutex_lock(&edp_lock);
	r = edp_update_loan_threshold_unlocked(client, threshold);
	mutex_unlock(&edp_lock);

	return r;
}
EXPORT_SYMBOL(edp_update_loan_threshold);

struct edp_governor *edp_find_governor_unlocked(const char *s)
{
	struct edp_governor *g;

	list_for_each_entry(g, &edp_governors, link)
		if (!strnicmp(s, g->name, EDP_NAME_LEN))
			return g;

	return NULL;
}

int edp_register_governor(struct edp_governor *gov)
{
	int r = 0;

	if (!gov)
		return -EINVAL;

	if (!gov->update_request)
		return -EINVAL;

	mutex_lock(&edp_lock);
	if (edp_find_governor_unlocked(gov->name)) {
		r = -EEXIST;
	} else {
		gov->refcnt = 1;
		list_add(&gov->link, &edp_governors);
	}
	mutex_unlock(&edp_lock);

	return r;
}
EXPORT_SYMBOL(edp_register_governor);

int edp_unregister_governor(struct edp_governor *gov)
{
	int r = 0;

	mutex_lock(&edp_lock);
	if (!gov) {
		r = -EINVAL;
	} else if (gov->refcnt != 1) {
		r = gov->refcnt > 1 ? -EBUSY : -ENODEV;
	} else {
		list_del(&gov->link);
		gov->refcnt = 0;
	}
	mutex_unlock(&edp_lock);

	return r;
}
EXPORT_SYMBOL(edp_unregister_governor);

struct edp_governor *edp_get_governor(const char *name)
{
	struct edp_governor *g;

	mutex_lock(&edp_lock);
	g = edp_find_governor_unlocked(name);
	mutex_unlock(&edp_lock);

	return g;
}
EXPORT_SYMBOL(edp_get_governor);

int edp_set_governor(struct edp_manager *mgr, struct edp_governor *gov)
{
	int r;

	mutex_lock(&edp_lock);
	r = edp_set_governor_unlocked(mgr, gov);
	mutex_unlock(&edp_lock);

	return r;
}
EXPORT_SYMBOL(edp_set_governor);
