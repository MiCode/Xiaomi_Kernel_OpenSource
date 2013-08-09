/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
 * RMNET Data Virtual Network Device APIs
 *
 */

#include <linux/types.h>

#ifndef _RMNET_DATA_VND_H_
#define _RMNET_DATA_VND_H_

int rmnet_vnd_get_flow_mapping(struct net_device *dev,
			       unsigned int map_flow_id,
			       unsigned int *flow_map);
struct rmnet_logical_ep_conf_s *rmnet_vnd_get_le_config(struct net_device *dev);
int rmnet_vnd_create_dev(int id, struct net_device **new_device);
int rmnet_vnd_rx_fixup(struct sk_buff *skb, struct net_device *dev);
int rmnet_vnd_tx_fixup(struct sk_buff *skb, struct net_device *dev);
int rmnet_vnd_is_vnd(struct net_device *dev);
int rmnet_vnd_init(void);
void rmnet_vnd_exit(void);

#endif /* _RMNET_DATA_VND_H_ */
