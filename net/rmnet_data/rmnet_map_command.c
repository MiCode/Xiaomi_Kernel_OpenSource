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
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/rmnet_data.h>
#include <net/pkt_sched.h>
#include "rmnet_data_config.h"
#include "rmnet_map.h"
#include "rmnet_data_private.h"

unsigned long int rmnet_map_command_stats[RMNET_MAP_COMMAND_ENUM_LENGTH];
module_param_array(rmnet_map_command_stats, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(rmnet_map_command_stats, "MAP command statistics");

static uint8_t rmnet_map_do_flow_control(struct sk_buff *skb,
					 struct rmnet_phys_ep_conf_s *config,
					 int enable)
{
	return RMNET_MAP_COMMAND_UNSUPPORTED;
}

static void rmnet_map_send_ack(struct sk_buff *skb,
			       unsigned char type)
{
	struct net_device *dev;
	struct rmnet_map_control_command_s *cmd;
	unsigned long flags;
	int xmit_status;

	if (!skb)
		BUG();

	dev = skb->dev;

	cmd = RMNET_MAP_GET_CMD_START(skb);
	cmd->cmd_type = type & 0x03;

	spin_lock_irqsave(&(skb->dev->tx_global_lock), flags);
	xmit_status = skb->dev->netdev_ops->ndo_start_xmit(skb, skb->dev);
	spin_unlock_irqrestore(&(skb->dev->tx_global_lock), flags);
}

rx_handler_result_t rmnet_map_command(struct sk_buff *skb,
				      struct rmnet_phys_ep_conf_s *config)
{
	struct rmnet_map_control_command_s *cmd;
	unsigned char command_name;
	unsigned char rc = 0;

	if (!skb)
		BUG();

	cmd = RMNET_MAP_GET_CMD_START(skb);
	command_name = cmd->command_name;

	if (command_name < RMNET_MAP_COMMAND_ENUM_LENGTH)
		rmnet_map_command_stats[command_name]++;

	switch (command_name) {
	case RMNET_MAP_COMMAND_FLOW_ENABLE:
		rc = rmnet_map_do_flow_control(skb, config, 1);
		break;

	case RMNET_MAP_COMMAND_FLOW_DISABLE:
		rc = rmnet_map_do_flow_control(skb, config, 0);
		break;

	default:
		rmnet_map_command_stats[RMNET_MAP_COMMAND_UNKNOWN]++;
		LOGM("%s(): Uknown MAP command: %d\n", __func__, command_name);
		rc = RMNET_MAP_COMMAND_UNSUPPORTED;
		break;
	}
	rmnet_map_send_ack(skb, rc);
	return 0; /* TODO: handler_consumed */
}
