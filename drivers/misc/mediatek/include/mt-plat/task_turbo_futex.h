/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _TASK_TURBO_FUTEX_H_
#define _TASK_TURBO_FUTEX_H_

#include <turbo_common.h>

inline void futex_plist_add(struct futex_q *q, struct futex_hash_bucket *hb)
{
	struct futex_q *this, *next;
	struct plist_node *current_node = &q->list;
	struct plist_node *this_node;

	if (!sub_feat_enable(SUB_FEAT_LOCK) &&
	    !is_turbo_task(current)) {
		plist_add(&q->list, &hb->chain);
		return;
	}

	plist_for_each_entry_safe(this, next, &hb->chain, list) {
		if ((!this->pi_state || !this->rt_waiter)
		  && !is_turbo_task(this->task)) {
			this_node = &this->list;
			list_add(&current_node->node_list,
				 this_node->node_list.prev);
			return;
		}
	}

	plist_add(&q->list, &hb->chain);
}
#endif
