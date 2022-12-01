/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef SLATECOMRPMSG_H
#define SLATECOMRPMSG_H

#include <linux/rpmsg.h>
#include "slatecom_interface.h"

#define TIMEOUT_MS 5000

struct slatecom_rpmsg_dev {
	struct rpmsg_endpoint *channel;
	struct device *dev;
	bool chnl_state;
	void *message;
	size_t message_length;
};

void slatecom_intf_notify_glink_channel_state(bool state);
void slatecom_rx_msg(void *data, int len);

struct ctrl_channel_ops {
	void (*glink_channel_state)(bool state);
	void (*rx_msg)(void *data, int len);
};

struct ctrl_channel_ops ctrl_ops;

#if IS_ENABLED(CONFIG_MSM_SLATECOM_RPMSG)
int slatecom_rpmsg_tx_msg(void  *msg, size_t len);
#else
static inline int slatecom_rpmsg_tx_msg(void  *msg, size_t len)
{
	return -EIO;
}
#endif

#endif
