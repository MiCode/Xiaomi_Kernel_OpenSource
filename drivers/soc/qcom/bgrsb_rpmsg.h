/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef BGRSBRPMSG_H
#define BGRSBRPMSG_H

#include <linux/rpmsg.h>
#include "bgrsb.h"

struct bgrsb_rpmsg_dev {
	struct rpmsg_endpoint *channel;
	struct device *dev;
	bool chnl_state;
	void *message;
	size_t message_length;
};

#if IS_ENABLED(CONFIG_MSM_BGRSB_RPMSG)
int bgrsb_rpmsg_tx_msg(void  *msg, size_t len);
#else
static inline int bgrsb_rpmsg_tx_msg(void  *msg, size_t len)
{
	return -EIO;
}
#endif

#endif
