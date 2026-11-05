/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef SLATECOMRPMSG_H
#define SLATECOMRPMSG_H

#include <linux/rpmsg.h>
#include <linux/soc/qcom/slatecom_interface.h>

#define TIMEOUT_MS 5000
#define	TIMEOUT_MS_GLINK_OPEN	10000

struct slatecom_rpmsg_dev {
	struct rpmsg_endpoint *channel;
	struct device *dev;
	bool chnl_state;
	void *message;
	size_t message_length;
};

struct ctrl_channel_ops {
	void (*glink_channel_state)(bool state);
	void (*rx_msg)(void *data, int len);
};

void slatecom_ctrl_channel_init(void (*fn1)(bool), void (*fn2)(void *, int));

#if IS_ENABLED(CONFIG_MSM_SLATECOM_RPMSG)
int slatecom_rpmsg_tx_msg(void  *msg, size_t len);
#else
static inline int slatecom_rpmsg_tx_msg(void  *msg, size_t len)
{
	return -EIO;
}
#endif

#endif
