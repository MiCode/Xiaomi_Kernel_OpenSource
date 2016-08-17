/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/debugfs.h>
#include "edp_internal.h"

struct dentry *edp_debugfs_dir;

/*
 * Reducing the cap is tricky - we might require throttling of other
 * clients (therefore, involving the governor). So we will fool the
 * framework by using a dummy client that has a single E-state (E0)
 * equalling the reduction.
 */
static int reduce_cap(struct edp_manager *m, unsigned int new_max)
{
	int r = 0;
	unsigned int delta = m->max - new_max;
	unsigned int remain;
	struct edp_client c = {
		.name = ".",
		.states = &delta,
		.num_states = 1,
		.e0_index = 0,
		.max_borrowers = 0,
		.priority = EDP_MIN_PRIO
	};

	r = register_client(m, &c);
	if (r)
		return r;

	r = edp_update_client_request_unlocked(&c, 0, NULL);
	if (r)
		return r;

	remain = m->remaining;
	r = unregister_client(&c);
	if (r)
		return r;

	m->remaining = remain;
	m->max = new_max;
	return 0;
}

static int __manager_cap_set(struct edp_manager *m, unsigned int new_max)
{
	if (new_max >= m->max) {
		m->remaining += new_max - m->max;
		m->max = new_max;
		schedule_promotion(m);
		return 0;
	}

	return reduce_cap(m, new_max);
}

static int manager_status_show(struct seq_file *file, void *data)
{
	struct edp_manager *m;
	struct edp_client *c;

	if (!file->private)
		return -ENODEV;

	m = file->private;

	mutex_lock(&edp_lock);

	seq_printf(file, "cap      : %u\n", m->max);
	seq_printf(file, "sum(E0)  : %u\n", e0_current_sum(m));
	seq_printf(file, "remaining: %u\n", m->remaining);

	seq_printf(file, "------------------------------------------\n");
	seq_printf(file, "%-16s %3s %5s %7s %7s\n",
			"client", "pri", "E0", "request", "current");
	seq_printf(file, "------------------------------------------\n");

	list_for_each_entry(c, &m->clients, link)
		seq_printf(file, "%-16s %3d %5u %7u %7u\n", c->name,
				c->priority, e0_level(c), req_level(c),
				cur_level(c));

	mutex_unlock(&edp_lock);
	return 0;
}

static int manager_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, manager_status_show, inode->i_private);
}

static const struct file_operations manager_status_fops = {
	.open = manager_status_open,
	.read = seq_read,
};

static int manager_cap_set(void *data, u64 val)
{
	struct edp_manager *m = data;
	int r;

	mutex_lock(&edp_lock);
	r = __manager_cap_set(m, val);
	mutex_unlock(&edp_lock);
	return r;
}

static int manager_cap_get(void *data, u64 *val)
{
	struct edp_manager *m = data;

	mutex_lock(&edp_lock);
	*val = m->max;
	mutex_unlock(&edp_lock);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(manager_cap_fops, manager_cap_get,
		manager_cap_set, "%lld\n");

void manager_add_dentry(struct edp_manager *m)
{
	struct dentry *d;

	if (!edp_debugfs_dir)
		return;

	d = debugfs_create_dir(m->name, edp_debugfs_dir);
	if (IS_ERR_OR_NULL(d))
		return;

	m->dentry = d;

	d = debugfs_create_file("status", S_IRUGO, m->dentry, m,
			&manager_status_fops);
	WARN_ON(IS_ERR_OR_NULL(d));

	d = debugfs_create_file("cap", S_IRUGO | S_IWUSR, m->dentry, m,
			&manager_cap_fops);
	WARN_ON(IS_ERR_OR_NULL(d));
}

void manager_remove_dentry(struct edp_manager *m)
{
	debugfs_remove_recursive(m->dentry);
	m->dentry = NULL;
}

static int __client_current_set(struct edp_client *c, unsigned int new)
{
	struct edp_manager *m;
	unsigned int nl;
	unsigned int cl;

	if (new >= c->num_states)
		return -EINVAL;

	nl = c->states[new];
	cl = cur_level(c);
	m = c->manager;

	if (nl > cl && nl - cl > m->remaining)
		return -EBUSY;

	c->cur = c->states + new;
	c->req = c->states + new;

	if (nl < cl) {
		m->remaining += cl - nl;
		if (c->throttle)
			c->throttle(new, c->private_data);
		schedule_promotion(m);
	} else if (nl > cl) {
		m->remaining -= nl - cl;
		if (c->notify_promotion)
			c->notify_promotion(new, c->private_data);
	}

	return 0;
}

static int client_current_set(void *data, u64 val)
{
	struct edp_client *c = data;
	int r;

	mutex_lock(&edp_lock);
	r = __client_current_set(c, val);
	mutex_unlock(&edp_lock);
	return r;
}

static int client_current_get(void *data, u64 *val)
{
	struct edp_client *c = data;

	mutex_lock(&edp_lock);
	*val = cur_level(c);
	mutex_unlock(&edp_lock);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(client_current_fops, client_current_get,
		client_current_set, "%lld\n");

void client_add_dentry(struct edp_client *c)
{
	struct dentry *d;

	if (!c->manager->dentry)
		return;

	d = debugfs_create_dir(c->name, c->manager->dentry);
	if (IS_ERR_OR_NULL(d)) {
		WARN_ON(1);
		return;
	}

	c->dentry = d;

	d = debugfs_create_file("current", S_IRUGO | S_IWUSR, c->dentry,
			c, &client_current_fops);
	WARN_ON(IS_ERR_OR_NULL(d));
}

void client_remove_dentry(struct edp_client *c)
{
	debugfs_remove_recursive(c->dentry);
	c->dentry = NULL;
}

static void dbg_update_request(struct edp_client *c, const unsigned int *r) {}
static void dbg_update_loans(struct edp_client *c) {}
static void dbg_promote(struct edp_manager *mgr) {}

static struct edp_governor dbg_governor = {
	.name = "debug",
	.owner = THIS_MODULE,
	.update_request = dbg_update_request,
	.update_loans = dbg_update_loans,
	.promote = dbg_promote
};

static int __init debug_init(void)
{
	struct dentry *d;

	d = debugfs_create_dir("edp", NULL);
	if (IS_ERR_OR_NULL(d)) {
		WARN_ON(1);
		return -EFAULT;
	}

	edp_debugfs_dir = d;
	return edp_register_governor(&dbg_governor);
}
postcore_initcall(debug_init);
