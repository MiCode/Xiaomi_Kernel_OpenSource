/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_UT_EVENT_H
#define __MTK_CAM_UT_EVENT_H

#include <linux/list.h>
#include <linux/slab.h>

struct ut_event {
	int	mask;
};

struct ut_event_source;
struct ut_event_listener {
	void (*on_notify)(struct ut_event_listener *listener,
			  struct ut_event_source *src,
			  struct ut_event event);
};

struct ut_event_linstener_entry {
	struct ut_event_listener *listener;
	struct ut_event	filter;
	struct list_head list;
};

struct ut_event_source {
	struct list_head listeners;
};

static inline void init_event_source(struct ut_event_source *src)
{
	WARN_ON(!src);

	INIT_LIST_HEAD(&src->listeners);
};

static inline void add_listener(struct ut_event_source *src,
				struct ut_event_listener *listener,
				struct ut_event event)
{
	struct ut_event_linstener_entry *entry;

	WARN_ON(!src || !listener);

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	WARN_ON(!entry);

	entry->listener = listener;
	entry->filter = event;

	list_add_tail(&entry->list, &src->listeners);
}

static inline void remove_listener(struct ut_event_source *src,
				   struct ut_event_listener *listener)
{
	struct ut_event_linstener_entry *entry;

	WARN_ON(!src || !listener);

	list_for_each_entry(entry, &src->listeners, list) {
		if (entry->listener == listener) {
			list_del(&entry->list);
			kfree(entry);
			break;
		}
	}
}

static inline void send_event(struct ut_event_source *src,
			      struct ut_event event)
{
	struct ut_event_linstener_entry *entry;
	struct ut_event masked_event;

	WARN_ON(!src);

	list_for_each_entry(entry, &src->listeners, list) {

		WARN_ON(!entry->listener);
		masked_event.mask = entry->filter.mask & event.mask;

		//pr_info("entry with mask 0x%x, f_notify %x\n",
		//	entry->filter.mask, entry->on_notify);
		if (masked_event.mask && entry->listener->on_notify)
			entry->listener->on_notify(entry->listener, src,
						   masked_event);
	}
}

/* events */

#define EVENT_SOF		BIT(0)
#define EVENT_CQ_DONE		BIT(1)
#define EVENT_SW_P1_DONE	BIT(2)
#define EVENT_CQ_MAIN_TRIG_DLY	BIT(3)
#define EVENT_SV_SOF		BIT(4)

#endif /* __MTK_CAM_UT_EVENT_H */
