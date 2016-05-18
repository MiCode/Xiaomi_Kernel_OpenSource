/* Copyright (c) 2015-2016 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/bitops.h>
#include <linux/printk.h>
#include <linux/device.h>

#include "pmic-voter.h"

#define NUM_MAX_CLIENTS	8

static DEFINE_SPINLOCK(votable_list_slock);
static LIST_HEAD(votable_list);

static struct dentry *debug_root;

struct client_vote {
	bool	enabled;
	int	value;
};

struct votable {
	const char		*name;
	struct list_head	list;
	struct client_vote	votes[NUM_MAX_CLIENTS];
	struct device		*dev;
	int			num_clients;
	int			type;
	int			effective_client_id;
	int			effective_result;
	int			default_result;
	struct mutex		vote_lock;
	int			(*callback)(struct device *dev,
						int effective_result,
						const char *effective_client);
	char			*client_strs[NUM_MAX_CLIENTS];
	struct dentry		*ent;
};

static void vote_set_any(struct votable *votable, int client_id,
				int *eff_res, int *eff_id)
{
	int i;

	for (i = 0; i < votable->num_clients && votable->client_strs[i]; i++)
		*eff_res |= votable->votes[i].enabled;

	*eff_id = client_id;
}

static void vote_min(struct votable *votable, int client_id,
				int *eff_res, int *eff_id)
{
	int i;

	*eff_res = INT_MAX;
	*eff_id = -EINVAL;
	for (i = 0; i < votable->num_clients && votable->client_strs[i]; i++) {
		if (votable->votes[i].enabled
			&& *eff_res > votable->votes[i].value) {
			*eff_res = votable->votes[i].value;
			*eff_id = i;
		}
	}
}

static void vote_max(struct votable *votable, int client_id,
				int *eff_res, int *eff_id)
{
	int i;

	*eff_res = INT_MIN;
	*eff_id = -EINVAL;
	for (i = 0; i < votable->num_clients && votable->client_strs[i]; i++) {
		if (votable->votes[i].enabled &&
				*eff_res < votable->votes[i].value) {
			*eff_res = votable->votes[i].value;
			*eff_id = i;
		}
	}
}

static int get_client_id(struct votable *votable, const char *client_str)
{
	int i;

	for (i = 0; i < votable->num_clients; i++) {
		if (votable->client_strs[i]
		 && (strcmp(votable->client_strs[i], client_str) == 0))
			return i;
	}

	/* new client */
	for (i = 0; i < votable->num_clients; i++) {
		if (!votable->client_strs[i]) {
			votable->client_strs[i]
				= devm_kstrdup(votable->dev,
						client_str, GFP_KERNEL);
			if (!votable->client_strs[i])
				return -ENOMEM;
			return i;
		}
	}
	return -EINVAL;
}

static char *get_client_str(struct votable *votable, int client_id)
{
	if (client_id == -EINVAL)
		return NULL;

	return votable->client_strs[client_id];
}

void lock_votable(struct votable *votable)
{
	mutex_lock(&votable->vote_lock);
}

void unlock_votable(struct votable *votable)
{
	mutex_unlock(&votable->vote_lock);
}

int get_client_vote_locked(struct votable *votable, const char *client_str)
{
	int client_id = get_client_id(votable, client_str);

	if (client_id < 0)
		return votable->default_result;

	if ((votable->type != VOTE_SET_ANY)
		&& !votable->votes[client_id].enabled)
		return votable->default_result;

	return votable->votes[client_id].value;
}

int get_client_vote(struct votable *votable, const char *client_str)
{
	int value;

	lock_votable(votable);
	value = get_client_vote_locked(votable, client_str);
	unlock_votable(votable);
	return value;
}

int get_effective_result_locked(struct votable *votable)
{
	if (votable->effective_result < 0)
		return votable->default_result;

	return votable->effective_result;
}

int get_effective_result(struct votable *votable)
{
	int value;

	lock_votable(votable);
	value = get_effective_result_locked(votable);
	unlock_votable(votable);
	return value;
}

const char *get_effective_client_locked(struct votable *votable)
{
	return get_client_str(votable, votable->effective_client_id);
}

const char *get_effective_client(struct votable *votable)
{
	const char *client_str;

	lock_votable(votable);
	client_str = get_effective_client_locked(votable);
	unlock_votable(votable);
	return client_str;
}

int vote(struct votable *votable, const char *client_str, bool enabled, int val)
{
	int effective_id = -EINVAL;
	int effective_result;
	int client_id;
	int rc = 0;

	lock_votable(votable);

	client_id = get_client_id(votable, client_str);
	if (client_id < 0) {
		rc = client_id;
		goto out;
	}

	if (votable->votes[client_id].enabled == enabled &&
				votable->votes[client_id].value == val) {
		pr_debug("%s: %s,%d same vote %s of %d\n",
				votable->name,
				client_str, client_id,
				enabled ? "on" : "off",
				val);
		goto out;
	}

	votable->votes[client_id].enabled = enabled;
	votable->votes[client_id].value = val;

	pr_debug("%s: %s,%d voting %s of %d\n",
		votable->name,
		client_str, client_id, enabled ? "on" : "off", val);
	switch (votable->type) {
	case VOTE_MIN:
		vote_min(votable, client_id, &effective_result, &effective_id);
		break;
	case VOTE_MAX:
		vote_max(votable, client_id, &effective_result, &effective_id);
		break;
	case VOTE_SET_ANY:
		vote_set_any(votable, client_id,
				&effective_result, &effective_id);
		break;
	}

	/*
	 * If the votable does not have any votes it will maintain the last
	 * known effective_result and effective_client_id
	 */
	if (effective_id < 0) {
		pr_debug("%s: no votes; skipping callback\n", votable->name);
		goto out;
	}

	if (effective_result != votable->effective_result) {
		votable->effective_client_id = effective_id;
		votable->effective_result = effective_result;
		pr_debug("%s: effective vote is now %d voted by %s,%d\n",
			votable->name, effective_result,
			get_client_str(votable, effective_id),
			effective_id);
		if (votable->callback)
			rc = votable->callback(votable->dev, effective_result,
					get_client_str(votable, effective_id));
	}

out:
	unlock_votable(votable);
	return rc;
}

int rerun_election(struct votable *votable)
{
	int rc = 0;

	lock_votable(votable);
	if (votable->callback)
		rc = votable->callback(votable->dev,
			votable->effective_result,
			get_client_str(votable, votable->effective_client_id));
	unlock_votable(votable);
	return rc;
}

struct votable *find_votable(const char *name)
{
	unsigned long flags;
	struct votable *v;
	bool found = false;

	spin_lock_irqsave(&votable_list_slock, flags);
	if (list_empty(&votable_list))
		goto out;

	list_for_each_entry(v, &votable_list, list) {
		if (strcmp(v->name, name) == 0) {
			found = true;
			break;
		}
	}
out:
	spin_unlock_irqrestore(&votable_list_slock, flags);

	if (found)
		return v;
	else
		return NULL;
}

static int show_votable_clients(struct seq_file *m, void *data)
{
	struct votable *votable = m->private;
	int i;
	char *type_str = "Unkonwn";

	lock_votable(votable);

	seq_puts(m, "Clients:\n");
	for (i = 0; i < votable->num_clients; i++) {
		if (votable->client_strs[i]) {
			seq_printf(m, "%-15s:\t\ten=%d\t\tv=%d\n",
					votable->client_strs[i],
					votable->votes[i].enabled,
					votable->votes[i].value);
		}
	}

	switch (votable->type) {
	case VOTE_MIN:
		type_str = "Min";
		break;
	case VOTE_MAX:
		type_str = "Max";
		break;
	case VOTE_SET_ANY:
		type_str = "Set_any";
		break;
	}

	seq_printf(m, "Type: %s\n", type_str);
	seq_puts(m, "Effective:\n");
	seq_printf(m, "%-15s:\t\tv=%d\n",
			get_effective_client_locked(votable),
			get_effective_result_locked(votable));
	unlock_votable(votable);

	return 0;
}

static int votable_debugfs_open(struct inode *inode, struct file *file)
{
	struct votable *votable = inode->i_private;

	return single_open(file, show_votable_clients, votable);
}

static const struct file_operations votable_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= votable_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

struct votable *create_votable(struct device *dev, const char *name,
					int votable_type,
					int default_result,
					int (*callback)(struct device *dev,
						int effective_result,
						const char *effective_client)
					)
{
	struct votable *votable;
	unsigned long flags;

	votable = find_votable(name);
	if (votable)
		return ERR_PTR(-EEXIST);

	if (debug_root == NULL) {
		debug_root = debugfs_create_dir("pmic-votable", NULL);
		if (!debug_root) {
			dev_err(dev, "Couldn't create debug dir\n");
			return ERR_PTR(-ENOMEM);
		}
	}

	if (votable_type >= NUM_VOTABLE_TYPES) {
		dev_err(dev, "Invalid votable_type specified for voter\n");
		return ERR_PTR(-EINVAL);
	}

	votable = devm_kzalloc(dev, sizeof(struct votable), GFP_KERNEL);
	if (!votable)
		return ERR_PTR(-ENOMEM);

	votable->dev = dev;
	votable->name = name;
	votable->num_clients = NUM_MAX_CLIENTS;
	votable->callback = callback;
	votable->type = votable_type;
	votable->default_result = default_result;
	mutex_init(&votable->vote_lock);

	/*
	 * Because effective_result and client states are invalid
	 * before the first vote, initialize them to -EINVAL
	 */
	votable->effective_result = -EINVAL;
	votable->effective_client_id = -EINVAL;

	spin_lock_irqsave(&votable_list_slock, flags);
	list_add(&votable->list, &votable_list);
	spin_unlock_irqrestore(&votable_list_slock, flags);

	votable->ent = debugfs_create_file(name, S_IFREG | 0444,
				  debug_root, votable,
				  &votable_debugfs_ops);
	if (!votable->ent) {
		dev_err(dev, "Couldn't create %s debug file\n", name);
		devm_kfree(dev, votable);
		return ERR_PTR(-EEXIST);
	}

	return votable;
}

void destroy_votable(struct device *dev, struct votable *votable)
{
	unsigned long flags;

	/* only disengage from list, mem will be freed with dev release */
	spin_lock_irqsave(&votable_list_slock, flags);
	list_del(&votable->list);
	spin_unlock_irqrestore(&votable_list_slock, flags);

	debugfs_remove(votable->ent);
}
