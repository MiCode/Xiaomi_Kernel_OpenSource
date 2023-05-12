/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2020 The Linux Foundation. All rights reserved.
 */

#ifndef __THUN_CHARGER_VOTER_H
#define __THUN_CHARGER_VOTER_H

#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/string.h>

#define NUM_MAX_CLIENTS		32

struct client_vote {
	bool	enabled;
	int	value;
};

struct votable {
	const char		*name;
	const char		*override_client;
	struct list_head	list;
	struct client_vote	votes[NUM_MAX_CLIENTS];
	int			num_clients;
	int			type;
	int			effective_client_id;
	int			effective_result;
	int			override_result;
	struct mutex		vote_lock;
	void			*data;
	int			(*callback)(struct votable *votable,
						void *data,
						int effective_result,
						const char *effective_client);
	char			*client_strs[NUM_MAX_CLIENTS];
	bool			voted_on;
	struct dentry		*root;
	struct dentry		*status_ent;
	u32			force_val;
	struct dentry		*force_val_ent;
	bool			force_active;
	struct dentry		*force_active_ent;
};

enum votable_type {
	VOTE_MIN,
	VOTE_MAX,
	VOTE_SET_ANY,
	NUM_VOTABLE_TYPES,
};

bool is_client_vote_enabled(struct votable *votable, const char *client_str);
bool is_client_vote_enabled_locked(struct votable *votable,
							const char *client_str);
bool is_override_vote_enabled(struct votable *votable);
bool is_override_vote_enabled_locked(struct votable *votable);
int get_client_vote(struct votable *votable, const char *client_str);
int get_client_vote_locked(struct votable *votable, const char *client_str);
int get_effective_result(struct votable *votable);
int get_effective_result_locked(struct votable *votable);
const char *get_effective_client(struct votable *votable);
const char *get_effective_client_locked(struct votable *votable);
int vote(struct votable *votable, const char *client_str, bool state, int val);
int vote_override(struct votable *votable, const char *override_client,
		  bool state, int val);
int rerun_election(struct votable *votable);
struct votable *find_votable(const char *name);
struct votable *create_votable(const char *name,
				int votable_type,
				int (*callback)(struct votable *votable,
						void *data,
						int effective_result,
						const char *effective_client),
				void *data);
void destroy_votable(struct votable *votable);
void lock_votable(struct votable *votable);
void unlock_votable(struct votable *votable);

#endif /* __PMIC_VOTER_H */
