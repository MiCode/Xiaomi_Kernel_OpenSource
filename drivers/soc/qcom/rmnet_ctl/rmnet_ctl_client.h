/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#ifndef _RMNET_CTL_CLIENT_H_
#define _RMNET_CTL_CLIENT_H_

#include <linux/skbuff.h>

struct rmnet_ctl_stats {
	u64 rx_pkts;
	u64 rx_err;
	u64 tx_pkts;
	u64 tx_err;
	u64 tx_complete;
};

struct rmnet_ctl_dev {
	int (*xmit)(struct rmnet_ctl_dev *dev, struct sk_buff *skb);
	struct rmnet_ctl_stats stats;
};

void rmnet_ctl_endpoint_post(const void *data, size_t len);
void rmnet_ctl_endpoint_setdev(const struct rmnet_ctl_dev *dev);

#endif /* _RMNET_CTL_CLIENT_H_ */
