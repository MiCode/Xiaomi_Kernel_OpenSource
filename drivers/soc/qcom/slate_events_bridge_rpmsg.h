/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */
#ifndef SEB_RPMSG_H
#define SEB_RPMSG_H

#include <linux/rpmsg.h>
#include "slate_events_bridge.h"

struct seb_rpmsg_dev {
	struct rpmsg_endpoint *channel;
	struct device *dev;
	bool chnl_state;
	void *message;
	size_t message_length;
};

int seb_rpmsg_tx_msg(void  *msg, size_t len);

#endif /* SEB_RPMSG_H */
