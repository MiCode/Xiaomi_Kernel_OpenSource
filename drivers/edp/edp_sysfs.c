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
#include <linux/slab.h>
#include "edp_internal.h"

static struct kobject edp_kobj;

struct manager_entry {
	struct edp_manager *manager;
	struct kobject kobj;
};

struct manager_attr {
	struct attribute attr;
	ssize_t (*show)(struct edp_manager *, char *);
	ssize_t (*store)(struct edp_manager *, const char *, size_t);
};

static ssize_t cap_show(struct edp_manager *m, char *s)
{
	return scnprintf(s, PAGE_SIZE, "%u\n", m->max);
}

static ssize_t remaining_show(struct edp_manager *m, char *s)
{
	return scnprintf(s, PAGE_SIZE, "%u\n", m->remaining);
}

static ssize_t denied_show(struct edp_manager *m, char *s)
{
	return scnprintf(s, PAGE_SIZE, "%u\n", m->num_denied);
}

static ssize_t manager_governor_show(struct edp_manager *m, char *s)
{
	return scnprintf(s, PAGE_SIZE, "%s\n", m->gov ? m->gov->name : "");
}

static ssize_t manager_governor_store(struct edp_manager *m, const char *s,
		size_t count)
{
	char name[EDP_NAME_LEN];
	struct edp_governor *gov;

	if (!count || count >= sizeof(name))
		return -EINVAL;

	memcpy(name, s, count);
	name[count] = 0;
	strim(name);
	gov = edp_find_governor_unlocked(name);
	if (!gov)
		return -EINVAL;

	return edp_set_governor_unlocked(m, gov) ?: count;
}

struct manager_attr attr_cap = __ATTR_RO(cap);
struct manager_attr attr_remaining = __ATTR_RO(remaining);
struct manager_attr attr_denied = __ATTR_RO(denied);
struct manager_attr attr_mgr_gov = __ATTR(governor, 0644,
		manager_governor_show, manager_governor_store);

static struct attribute *manager_attrs[] = {
	&attr_cap.attr,
	&attr_remaining.attr,
	&attr_denied.attr,
	&attr_mgr_gov.attr,
	NULL
};

static struct edp_manager *to_manager(struct kobject *kobj)
{
	struct manager_entry *me = container_of(kobj, struct manager_entry,
			kobj);
	return me ? me->manager : NULL;
}

static ssize_t manager_state_show(struct kobject *kobj,
		struct attribute *attr,	char *buf)
{
	ssize_t r;
	struct edp_manager *m;
	struct manager_attr *mattr;

	mutex_lock(&edp_lock);
	m = to_manager(kobj);
	mattr = container_of(attr, struct manager_attr, attr);
	r = m && mattr ? mattr->show(m, buf) : -EINVAL;
	mutex_unlock(&edp_lock);

	return r;
}

static ssize_t manager_state_store(struct kobject *kobj,
		struct attribute *attr,	const char *buf, size_t count)
{
	ssize_t r;
	struct edp_manager *m;
	struct manager_attr *mattr;

	mutex_lock(&edp_lock);
	m = to_manager(kobj);
	mattr = container_of(attr, struct manager_attr, attr);
	r = m && mattr ? mattr->store(m, buf, count) : -EINVAL;
	mutex_unlock(&edp_lock);

	return r;
}

static const struct sysfs_ops manager_sysfs_ops = {
	.show = manager_state_show,
	.store = manager_state_store
};

static struct kobj_type ktype_manager = {
	.sysfs_ops = &manager_sysfs_ops,
	.default_attrs = manager_attrs
};

void edp_manager_add_kobject(struct edp_manager *mgr)
{
	struct manager_entry *me;

	me = kzalloc(sizeof(*me), GFP_KERNEL);
	if (!me) {
		pr_err("%s: failed to alloc sysfs manager entry\n",
				mgr->name);
		return;
	}

	if (kobject_init_and_add(&me->kobj, &ktype_manager, &edp_kobj,
				mgr->name)) {
		pr_err("%s: failed to init & add sysfs manager entry\n",
				mgr->name);
		kfree(me);
		return;
	}

	me->manager = mgr;
	mgr->kobj = &me->kobj;
	kobject_uevent(&me->kobj, KOBJ_ADD);
	return;
}

void edp_manager_remove_kobject(struct edp_manager *mgr)
{
	struct manager_entry *me;

	if (!mgr->kobj)
		return;

	me = container_of(mgr->kobj, struct manager_entry, kobj);
	mgr->kobj = NULL;
	kobject_put(&me->kobj);
	kfree(me);
}

struct client_entry {
	struct edp_client *client;
	struct kobject kobj;
};

static ssize_t states_show(struct edp_client *c,
		struct edp_client_attribute *attr, char *s)
{
	unsigned int i;
	int cnt = 0;
	const int sz = sizeof(*c->states) * 3 + 2;

	for (i = 0; i < c->num_states && (cnt + sz) < PAGE_SIZE; i++)
		cnt += sprintf(s + cnt, "%s%u", i ? " " : "", c->states[i]);

	cnt += sprintf(s + cnt, "\n");
	return cnt;
}

static ssize_t num_states_show(struct edp_client *c,
		struct edp_client_attribute *attr, char *s)
{
	return scnprintf(s, PAGE_SIZE, "%u\n", c->num_states);
}

static ssize_t e0_show(struct edp_client *c,
		struct edp_client_attribute *attr, char *s)
{
	return scnprintf(s, PAGE_SIZE, "%u\n", c->states[c->e0_index]);
}

static ssize_t max_borrowers_show(struct edp_client *c,
		struct edp_client_attribute *attr, char *s)
{
	return scnprintf(s, PAGE_SIZE, "%u\n", c->max_borrowers);
}

static ssize_t priority_show(struct edp_client *c,
		struct edp_client_attribute *attr, char *s)
{
	return scnprintf(s, PAGE_SIZE, "%d\n", c->priority);
}

static ssize_t request_show(struct edp_client *c,
		struct edp_client_attribute *attr, char *s)
{
	return scnprintf(s, PAGE_SIZE, "%u\n", req_level(c));
}

/* Allow only updates that are guaranteed to succeed */
static ssize_t request_store(struct edp_client *c,
		struct edp_client_attribute *attr, const char *s, size_t count)
{
	unsigned int id;
	int r;

	if (sscanf(s, "%u", &id) != 1)
		return -EINVAL;

	mutex_lock(&edp_lock);

	if (id >= c->num_states) {
		r = -EINVAL;
		goto out;
	}

	if (id < c->e0_index && id < req_index(c)) {
		r = -EPERM;
		goto out;
	}

	r = edp_update_client_request_unlocked(c, id, NULL);

out:
	mutex_unlock(&edp_lock);
	return r ?: count;
}

static ssize_t current_show(struct edp_client *c,
		struct edp_client_attribute *attr, char *s)
{
	return scnprintf(s, PAGE_SIZE, "%u\n", cur_level(c));
}

static ssize_t threshold_show(struct edp_client *c,
		struct edp_client_attribute *attr, char *s)
{
	return scnprintf(s, PAGE_SIZE, "%u\n", c->ithreshold);
}

static ssize_t threshold_store(struct edp_client *c,
		struct edp_client_attribute *attr, const char *s, size_t count)
{
	unsigned int tv;
	int r;

	if (sscanf(s, "%u", &tv) != 1)
		return -EINVAL;

	r = edp_update_loan_threshold(c, tv);
	return r ?: count;
}

static ssize_t borrowers_show(struct edp_client *c,
		struct edp_client_attribute *attr, char *s)
{
	return scnprintf(s, PAGE_SIZE, "%u\n", c->num_borrowers);
}

static ssize_t loans_show(struct edp_client *c,
		struct edp_client_attribute *attr, char *s)
{
	return scnprintf(s, PAGE_SIZE, "%u\n", c->num_loans);
}

static ssize_t notify_show(struct edp_client *c,
		struct edp_client_attribute *attr, char *s)
{
	return scnprintf(s, PAGE_SIZE, "%u\n", c->notify_ui);
}

static ssize_t notify_store(struct edp_client *c,
		struct edp_client_attribute *attr, const char *s, size_t count)
{
	unsigned int f;

	if (sscanf(s, "%u", &f) != 1)
		return -EINVAL;
	c->notify_ui = f;
	return count;
}

struct edp_client_attribute attr_states = __ATTR_RO(states);
struct edp_client_attribute attr_num_states = __ATTR_RO(num_states);
struct edp_client_attribute attr_e0 = __ATTR_RO(e0);
struct edp_client_attribute attr_max_borrowers = __ATTR_RO(max_borrowers);
struct edp_client_attribute attr_priority = __ATTR_RO(priority);
struct edp_client_attribute attr_request = __ATTR(request, 0644, request_show,
		request_store);
struct edp_client_attribute attr_threshold = __ATTR(threshold, 0644,
		threshold_show, threshold_store);
struct edp_client_attribute attr_borrowers = __ATTR_RO(borrowers);
struct edp_client_attribute attr_loans = __ATTR_RO(loans);
struct edp_client_attribute attr_current = {
	.attr = { .name = "current", .mode = 0444 },
	.show = current_show
};
struct edp_client_attribute attr_notify = __ATTR(notify, 0644, notify_show,
		notify_store);

static struct attribute *client_attrs[] = {
	&attr_states.attr,
	&attr_num_states.attr,
	&attr_e0.attr,
	&attr_max_borrowers.attr,
	&attr_priority.attr,
	&attr_request.attr,
	&attr_current.attr,
	&attr_threshold.attr,
	&attr_borrowers.attr,
	&attr_loans.attr,
	&attr_notify.attr,
	NULL
};

static struct edp_client *to_client(struct kobject *kobj)
{
	struct client_entry *ce = container_of(kobj, struct client_entry,
			kobj);
	return ce ? ce->client : NULL;
}

static ssize_t client_state_show(struct kobject *kobj,
		struct attribute *attr,	char *buf)
{
	ssize_t r = -EINVAL;
	struct edp_client *c;
	struct edp_client_attribute *cattr;

	c = to_client(kobj);
	cattr = container_of(attr, struct edp_client_attribute, attr);
	if (c && cattr) {
		if (cattr->show)
			r = cattr->show(c, cattr, buf);
	}

	return r;
}

static ssize_t client_state_store(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t count)
{
	ssize_t r = -EINVAL;
	struct edp_client *c;
	struct edp_client_attribute *cattr;

	c = to_client(kobj);
	cattr = container_of(attr, struct edp_client_attribute, attr);
	if (c && cattr) {
		if (cattr->store)
			r = cattr->store(c, cattr, buf, count);
	}

	return r;
}

static const struct sysfs_ops client_sysfs_ops = {
	.show = client_state_show,
	.store = client_state_store
};

static struct kobj_type ktype_client = {
	.sysfs_ops = &client_sysfs_ops,
	.default_attrs = client_attrs
};

static void create_driver_attrs(struct edp_client *c)
{
	struct edp_client_attribute *p;
	int r;

	if (!c->attrs)
		return;

	for (p = c->attrs; attr_name(*p); p++) {
		r = sysfs_create_file(c->kobj, &p->attr);
		WARN_ON(r);
	}
}

static void remove_driver_attrs(struct edp_client *c)
{
	struct edp_client_attribute *p;

	if (!c->attrs)
		return;

	for (p = c->attrs; attr_name(*p); p++)
		sysfs_remove_file(c->kobj, &p->attr);
}

void edp_client_add_kobject(struct edp_client *client)
{
	struct client_entry *ce;
	struct kobject *parent = client->manager->kobj;

	if (!parent)
		return;

	ce = kzalloc(sizeof(*ce), GFP_KERNEL);
	if (!ce) {
		pr_err("%s: failed to alloc sysfs client entry\n",
				client->name);
		return;
	}

	if (kobject_init_and_add(&ce->kobj, &ktype_client, parent,
				client->name)) {
		pr_err("%s: failed to init & add sysfs client entry\n",
				client->name);
		kfree(ce);
		return;
	}

	ce->client = client;
	client->kobj = &ce->kobj;
	create_driver_attrs(client);
	kobject_uevent(&ce->kobj, KOBJ_ADD);
}

void edp_client_remove_kobject(struct edp_client *client)
{
	struct client_entry *ce;

	if (!client->kobj)
		return;

	ce = container_of(client->kobj, struct client_entry, kobj);

	remove_driver_attrs(client);
	client->kobj = NULL;
	kobject_put(&ce->kobj);
	kfree(ce);
}

static ssize_t governors_show(struct kobject *kobj, struct attribute *attr,
		char *s)
{
	struct edp_governor *g;
	int cnt = 0;

	mutex_lock(&edp_lock);

	list_for_each_entry(g, &edp_governors, link) {
		if (cnt + EDP_NAME_LEN + 2 >= PAGE_SIZE)
			break;
		cnt += sprintf(s + cnt, "%s%s", cnt ? " " : "", g->name);
	}

	cnt += sprintf(s + cnt, "\n");

	mutex_unlock(&edp_lock);

	return cnt;
}

static const struct sysfs_ops edp_sysfs_ops = {
	.show = governors_show
};

static struct attribute attr_governors = {
	.name = "governors",
	.mode = 0444
};

static struct attribute *edp_attrs[] = {
	&attr_governors,
	NULL
};

static struct kobj_type ktype_edp = {
	.sysfs_ops = &edp_sysfs_ops,
	.default_attrs = edp_attrs
};

static int __init edp_sysfs_init(void)
{
	struct kobject *parent = NULL;

#ifdef CONFIG_PM
	parent = power_kobj;
#endif

	return kobject_init_and_add(&edp_kobj, &ktype_edp, parent, "edp");
}
postcore_initcall(edp_sysfs_init);
