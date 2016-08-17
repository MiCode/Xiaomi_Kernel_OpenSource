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

#ifndef _EDP_INTERNAL_H
#define _EDP_INTERNAL_H

#include <linux/kernel.h>
#include <linux/edp.h>

struct loan_client {
	struct list_head link;
	struct edp_client *client;
	unsigned int size;
};

static inline unsigned int cur_level(struct edp_client *c)
{
	return c->cur ? *c->cur : 0;
}

static inline unsigned int req_level(struct edp_client *c)
{
	return c->req ? *c->req : 0;
}

static inline unsigned int e0_level(struct edp_client *c)
{
	return c->states[c->e0_index];
}

static inline unsigned int cur_index(struct edp_client *c)
{
	return c->cur ? c->cur - c->states : c->num_states;
}

static inline unsigned int req_index(struct edp_client *c)
{
	return c->req ? c->req - c->states : c->num_states;
}

extern struct mutex edp_lock;
extern struct list_head edp_governors;

int register_client(struct edp_manager *m, struct edp_client *c);
int unregister_client(struct edp_client *c);

int edp_update_client_request_unlocked(struct edp_client *client,
		unsigned int req, int *approved);
int edp_update_loan_threshold_unlocked(struct edp_client *client,
		unsigned int threshold);
struct edp_governor *edp_find_governor_unlocked(const char *s);
int edp_set_governor_unlocked(struct edp_manager *mgr,
		struct edp_governor *gov);

void edp_manager_add_kobject(struct edp_manager *mgr);
void edp_manager_remove_kobject(struct edp_manager *mgr);
void edp_client_add_kobject(struct edp_client *client);
void edp_client_remove_kobject(struct edp_client *client);
void edp_default_update_request(struct edp_client *client,
		const unsigned int *req,
		void (*throttle)(struct edp_client *));
void edp_default_update_loans(struct edp_client *lender);
unsigned int edp_throttling_point(struct edp_client *c, unsigned int deficit);
unsigned int edp_promotion_point(struct edp_client *c, unsigned int step);

void manager_add_dentry(struct edp_manager *m);
void manager_remove_dentry(struct edp_manager *m);
void client_add_dentry(struct edp_client *c);
void client_remove_dentry(struct edp_client *c);
void schedule_promotion(struct edp_manager *m);
unsigned int e0_current_sum(struct edp_manager *mgr);

#endif
