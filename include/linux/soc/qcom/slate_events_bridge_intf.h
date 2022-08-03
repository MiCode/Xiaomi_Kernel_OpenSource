/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef SLATE_EVENTS_BRIDGE_INTF_H
#define SLATE_EVENTS_BRIDGE_INTF_H

#include <linux/notifier.h>

enum event_group_type {
	GMI_SLATE_EVENT_QBG     = 0x01,
	GMI_SLATE_EVENT_RSB     = 0x02,
	GMI_SLATE_EVENT_BUTTON  = 0x03,
	GMI_SLATE_EVENT_TOUCH   = 0x04,
	GMI_SLATE_EVENT_SENSOR  = 0x05,

	GLINK_CHANNEL_STATE_UP   = 0xfd,
	GLINK_CHANNEL_STATE_DOWN = 0xfe,
	GMI_SLATE_EVENT_MAX      = 0xff,
};

//#ifdef CONFIG_MSM_SEB
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
/*#else
static void *seb_register_for_slate_event(enum event_group_type event,
						struct notifier_block *nb)
{
	return ERR_PTR(-ENODEV);
}

static int seb_unregister_for_slate_event(void *seb_handle,
						struct notifier_block *nb)
{
	return -ENODEV;
}

static int seb_send_event_to_slate(void *seb_handle, enum event_group_type event,
					void *event_buf, uint32_t buf_size)
{
	return -ENODEV;
}
#endif*/

#endif /* SLATE_EVENTS_BRIDGE_INTF_H */
