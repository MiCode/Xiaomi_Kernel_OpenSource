/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef SLATE_EVENTS_BRIDGE_INTF_H
#define SLATE_EVENTS_BRIDGE_INTF_H

#include <linux/notifier.h>

enum event_group_type {
	SEB_BUTTON,
	SEB_QBG,
	SEB_RSB,
	SEB_TOUCH,
	SEB_MAX
};

/* Use the seb_register_for_slate_event API to register for events for
 * a particular group type.
 * This API will return a handle that can be used to un-reg for events
 * using the seb_unregister_for_slate_event API by passing in that handle
 * as an argument.
 */
void *seb_register_for_slate_event(enum event_group_type event,
						struct notifier_block *nb);

int seb_unregister_for_slate_event(void *seb_handle,
						struct notifier_block *nb);

/* Use the seb_send_event_to_slate API to send an event to Slate.
 * API return success/failure for the send event.
 */
int seb_send_event_to_slate(void *seb_handle, enum event_group_type event,
					void *event_buf, uint32_t buf_size);

#endif /* SLATE_EVENTS_BRIDGE_INTF_H */
