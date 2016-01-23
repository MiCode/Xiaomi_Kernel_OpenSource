/* Copyright (c) 2015 The Linux Foundation. All rights reserved.
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

#include <linux/mutex.h>

struct votable;

enum votable_type {
	VOTE_MIN,
	VOTE_MAX,
	VOTE_SET_ANY,
	NUM_VOTABLE_TYPES,
};

int get_client_vote(struct votable *votable, int client_id);
int get_client_vote_locked(struct votable *votable, int client_id);
int get_effective_result(struct votable *votable);
int get_effective_result_locked(struct votable *votable);
int get_effective_client_id(struct votable *votable);
int get_effective_client_id_locked(struct votable *votable);
int vote(struct votable *votable, int client_id, bool state, int val);
struct votable *create_votable(struct device *dev, const char *name,
				int votable_type, int num_clients,
				int default_result,
				int (*callback)(struct device *dev,
						int effective_result,
						int effective_client,
						int last_result,
						int last_client)
					);
void lock_votable(struct votable *votable);
void unlock_votable(struct votable  *votable);
