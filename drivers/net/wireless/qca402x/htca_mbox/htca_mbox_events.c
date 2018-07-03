/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include <linux/completion.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/spinlock_types.h>
#include <linux/wait.h>

#include "../hif_sdio/hif.h"
#include "htca.h"
#include "htca_mbox_internal.h"

/* Host Target Communications Event Management */

/* Protect all event tables -- global as well as per-endpoint. */
static spinlock_t event_lock; /* protects all event tables */

/* Mapping table for global events -- avail/unavail */
static struct htca_event_table_element
	global_event_table[HTCA_EVENT_GLOBAL_COUNT];

struct htca_event_table_element *
htca_event_id_to_event(struct htca_target *target,
		       u8 end_point_id,
		       u8 event_id)
{
	struct htca_event_table_element *ev = NULL;

	/* is ep event */
	if ((event_id >= HTCA_EVENT_EP_START) &&
	    (event_id <= HTCA_EVENT_EP_END)) {
		struct htca_endpoint *end_point;
		int ep_evid;

		ep_evid = event_id - HTCA_EVENT_EP_START;
		end_point = &target->end_point[end_point_id];
		ev = &end_point->endpoint_event_tbl[ep_evid];
	/* is global event */
	} else if ((event_id >= HTCA_EVENT_GLOBAL_START) &&
		   (event_id <= HTCA_EVENT_GLOBAL_END)) {
		int global_evid;

		global_evid = event_id - HTCA_EVENT_GLOBAL_START;
		ev = &global_event_table[global_evid];
	} else {
		WARN_ON(1); /* unknown event */
	}

	return ev;
}

void htca_dispatch_event(struct htca_target *target,
			 u8 end_point_id,
			 u8 event_id,
			 struct htca_event_info *event_info)
{
	struct htca_event_table_element *ev;

	ev = htca_event_id_to_event(target, end_point_id, event_id);
	if (!ev) {
		panic("BUG");
		return;
	}
	if (ev->handler) {
		htca_event_handler handler;
		void *param;
		unsigned long flags;

		spin_lock_irqsave(&event_lock, flags);
		handler = ev->handler;
		param = ev->param;
		spin_unlock_irqrestore(&event_lock, flags);

		handler((void *)target, end_point_id, event_id,
			event_info, param);
	}
}

int htca_add_to_event_table(struct htca_target *target,
			    u8 end_point_id,
			    u8 event_id,
			    htca_event_handler handler, void *param) {
	struct htca_event_table_element *ev;
	unsigned long flags;

	ev = htca_event_id_to_event(target, end_point_id, event_id);
	if (!ev)
		return HTCA_ERROR;

	spin_lock_irqsave(&event_lock, flags);
	ev->handler = handler;
	ev->param = param;
	spin_unlock_irqrestore(&event_lock, flags);

	return HTCA_OK;
}

int htca_remove_from_event_table(struct htca_target *target,
				 u8 end_point_id,
				 u8 event_id) {
	struct htca_event_table_element *ev;
	unsigned long flags;

	ev = htca_event_id_to_event(target, end_point_id, event_id);
	if (!ev)
		return HTCA_ERROR;

	spin_lock_irqsave(&event_lock, flags);
	/* Clear event handler info */
	memset(ev, 0, sizeof(*ev));
	spin_unlock_irqrestore(&event_lock, flags);

	return HTCA_OK;
}

/* Called once during module initialization */
void htca_event_table_init(void)
{
	spin_lock_init(&event_lock);
}
