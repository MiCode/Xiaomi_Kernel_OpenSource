/*
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
 * RMNET Data configuration engine
 *
 */

#include <linux/types.h>
#include <linux/spinlock.h>

#ifndef _RMNET_DATA_CONFIG_H_
#define _RMNET_DATA_CONFIG_H_

#define	RMNET_DATA_MAX_LOGICAL_EP	32

struct rmnet_logical_ep_conf_s {
	uint8_t refcount;
	uint8_t rmnet_mode;
	uint8_t mux_id;
	struct net_device *egress_dev;
};

struct rmnet_phys_ep_conf_s {
	struct net_device *dev;
	struct rmnet_logical_ep_conf_s local_ep;
	struct rmnet_logical_ep_conf_s muxed_ep[RMNET_DATA_MAX_LOGICAL_EP];
	uint32_t	ingress_data_format;
	uint32_t	egress_data_format;

	/* MAP specific */
	uint16_t egress_agg_size;
	uint16_t egress_agg_count;
	spinlock_t agg_lock;
	struct sk_buff *agg_skb;
	uint8_t agg_state;
	uint8_t agg_count;
	uint8_t tail_spacing;
};

int rmnet_config_init(void);
void rmnet_config_exit(void);

int rmnet_unassociate_network_device(struct net_device *dev);
int rmnet_set_ingress_data_format(struct net_device *dev,
				  uint32_t ingress_data_format,
				  uint8_t  tail_spacing);
int rmnet_set_egress_data_format(struct net_device *dev,
				 uint32_t egress_data_format,
				 uint16_t agg_size,
				 uint16_t agg_count);
int rmnet_associate_network_device(struct net_device *dev);
int _rmnet_set_logical_endpoint_config(struct net_device *dev,
				       int config_id,
				      struct rmnet_logical_ep_conf_s *epconfig);
int rmnet_set_logical_endpoint_config(struct net_device *dev,
				      int config_id,
				      uint8_t rmnet_mode,
				      struct net_device *egress_dev);
int _rmnet_unset_logical_endpoint_config(struct net_device *dev,
					 int config_id);
int rmnet_unset_logical_endpoint_config(struct net_device *dev,
					int config_id);
void rmnet_config_netlink_msg_handler (struct sk_buff *skb);
int rmnet_config_notify_cb(struct notifier_block *nb,
				  unsigned long event, void *data);
int rmnet_create_vnd(int id);
int rmnet_create_vnd_prefix(int id, const char *name);
int rmnet_free_vnd(int id);

#endif /* _RMNET_DATA_CONFIG_H_ */
