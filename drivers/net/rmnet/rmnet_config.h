/* Copyright (c) 2013-2014, 2016-2017 The Linux Foundation. All rights reserved.
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
#include <linux/time.h>
#include <linux/skbuff.h>

#ifndef _RMNET_CONFIG_H_
#define _RMNET_CONFIG_H_

#define RMNET_MAX_LOGICAL_EP 256

/* struct rmnet_logical_ep_conf_s - Logical end-point configuration
 *
 * @refcount: Reference count for this endpoint. 0 signifies the endpoint is not
 *            configured for use
 * @rmnet_mode: Specifies how the traffic should be finally delivered. Possible
 *            options are available in enum rmnet_config_endpoint_modes_e
 * @mux_id: Virtual channel ID used by MAP protocol
 * @egress_dev: Next device to deliver the packet to. Exact usage of this
 *            parmeter depends on the rmnet_mode
 */
struct rmnet_logical_ep_conf_s {
	u8 refcount;
	u8 rmnet_mode;
	u8 mux_id;
	struct timespec flush_time;
	struct net_device *egress_dev;
};

/* struct rmnet_phys_ep_conf_s - Physical endpoint configuration
 * One instance of this structure is instantiated for each net_device associated
 * with rmnet.
 *
 * @dev: The device which is associated with rmnet. Corresponds to this
 *       specific instance of rmnet_phys_ep_conf_s
 * @local_ep: Default non-muxed endpoint. Used for non-MAP protocols/formats
 * @muxed_ep: All multiplexed logical endpoints associated with this device
 * @ingress_data_format: RMNET_INGRESS_FORMAT_* flags from rmnet.h
 * @egress_data_format: RMNET_EGRESS_FORMAT_* flags from rmnet.h
 *
 * @egress_agg_size: Maximum size (bytes) of data which should be aggregated
 * @egress_agg_count: Maximum count (packets) of data which should be aggregated
 *                  Smaller of the two parameters above are chosen for
 *                  aggregation
 * @tail_spacing: Guaranteed padding (bytes) when de-aggregating ingress frames
 * @agg_time: Wall clock time when aggregated frame was created
 * @agg_last: Last time the aggregation routing was invoked
 */
struct rmnet_phys_ep_conf_s {
	struct net_device *dev;
	struct rmnet_logical_ep_conf_s local_ep;
	struct rmnet_logical_ep_conf_s muxed_ep[RMNET_MAX_LOGICAL_EP];
	u32 ingress_data_format;
	u32 egress_data_format;
};

int rmnet_config_init(void);
void rmnet_config_exit(void);

int rmnet_unassociate_network_device(struct net_device *dev);
int rmnet_set_ingress_data_format(struct net_device *dev,
				  u32 ingress_data_format);
int rmnet_set_egress_data_format(struct net_device *dev,
				 u32 egress_data_format,
				 u16 agg_size,
				 u16 agg_count);
int rmnet_associate_network_device(struct net_device *dev);
int _rmnet_set_logical_endpoint_config(struct net_device *dev,
				       int config_id,
				      struct rmnet_logical_ep_conf_s *epconfig);
int rmnet_set_logical_endpoint_config(struct net_device *dev,
				      int config_id,
				      u8 rmnet_mode,
				      struct net_device *egress_dev);
int _rmnet_unset_logical_endpoint_config(struct net_device *dev,
					 int config_id);
int rmnet_unset_logical_endpoint_config(struct net_device *dev,
					int config_id);
int _rmnet_get_logical_endpoint_config(struct net_device *dev,
				       int config_id,
				      struct rmnet_logical_ep_conf_s *epconfig);
int rmnet_get_logical_endpoint_config(struct net_device *dev,
				      int config_id,
				      u8 *rmnet_mode,
				      u8 *egress_dev_name,
				      size_t egress_dev_name_size);
void rmnet_config_netlink_msg_handler (struct sk_buff *skb);
int rmnet_config_notify_cb(struct notifier_block *nb,
			   unsigned long event, void *data);
int rmnet_create_vnd(int id);
int rmnet_create_vnd_prefix(int id, const char *name);
int rmnet_free_vnd(int id);

#endif /* _RMNET_CONFIG_H_ */
