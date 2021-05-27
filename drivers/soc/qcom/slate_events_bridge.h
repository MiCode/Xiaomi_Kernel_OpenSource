/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef SLATE_EVENTS_BRIDGE_H
#define SLATE_EVENTS_BRIDGE_H

#include <linux/notifier.h>
#include <linux/soc/qcom/slate_events_bridge_intf.h>

/* APIs for slate_event_bridge_rpmsg */
void seb_notify_glink_channel_state(bool state);
void seb_rx_msg(void *data, int len);
#endif /* SLATE_EVENTS_BRIDGE_H */
