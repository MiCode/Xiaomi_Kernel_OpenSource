/* Copyright (c) 2013-2018, The Linux Foundation. All rights reserved.
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

#include <linux/netdevice.h>
#include "rmnet_config.h"
#include "rmnet_map.h"
#include "rmnet_private.h"
#include "rmnet_vnd.h"

#define RMNET_DL_IND_HDR_SIZE (sizeof(struct rmnet_map_dl_ind_hdr) + \
			       sizeof(struct rmnet_map_header) + \
			       sizeof(struct rmnet_map_control_command_header))

#define RMNET_MAP_CMD_SIZE (sizeof(struct rmnet_map_header) + \
			    sizeof(struct rmnet_map_control_command_header))

#define RMNET_DL_IND_TRL_SIZE (sizeof(struct rmnet_map_dl_ind_trl) + \
			       sizeof(struct rmnet_map_header) + \
			       sizeof(struct rmnet_map_control_command_header))

static u8 rmnet_map_do_flow_control(struct sk_buff *skb,
				    struct rmnet_port *port,
				    int enable)
{
	struct rmnet_map_control_command *cmd;
	struct rmnet_endpoint *ep;
	struct net_device *vnd;
	u16 ip_family;
	u16 fc_seq;
	u32 qos_id;
	u8 mux_id;
	int r;

	mux_id = RMNET_MAP_GET_MUX_ID(skb);
	cmd = RMNET_MAP_GET_CMD_START(skb);

	if (mux_id >= RMNET_MAX_LOGICAL_EP) {
		kfree_skb(skb);
		return RX_HANDLER_CONSUMED;
	}

	ep = rmnet_get_endpoint(port, mux_id);
	if (!ep) {
		kfree_skb(skb);
		return RX_HANDLER_CONSUMED;
	}

	vnd = ep->egress_dev;

	ip_family = cmd->flow_control.ip_family;
	fc_seq = ntohs(cmd->flow_control.flow_control_seq_num);
	qos_id = ntohl(cmd->flow_control.qos_id);

	/* Ignore the ip family and pass the sequence number for both v4 and v6
	 * sequence. User space does not support creating dedicated flows for
	 * the 2 protocols
	 */
	r = rmnet_vnd_do_flow_control(vnd, enable);
	if (r) {
		kfree_skb(skb);
		return RMNET_MAP_COMMAND_UNSUPPORTED;
	} else {
		return RMNET_MAP_COMMAND_ACK;
	}
}

static void rmnet_map_send_ack(struct sk_buff *skb,
			       unsigned char type,
			       struct rmnet_port *port)
{
	struct rmnet_map_control_command *cmd;
	struct net_device *dev = skb->dev;

	if (port->data_format & RMNET_FLAGS_INGRESS_MAP_CKSUMV4)
		skb_trim(skb,
			 skb->len - sizeof(struct rmnet_map_dl_csum_trailer));

	skb->protocol = htons(ETH_P_MAP);

	cmd = RMNET_MAP_GET_CMD_START(skb);
	cmd->cmd_type = type & 0x03;

	netif_tx_lock(dev);
	dev->netdev_ops->ndo_start_xmit(skb, dev);
	netif_tx_unlock(dev);
}

static  void rmnet_map_dl_hdr_notify(struct rmnet_port *port,
				     struct rmnet_map_dl_ind_hdr *dlhdr)
{
	struct rmnet_map_dl_ind *tmp;

	list_for_each_entry(tmp, &port->dl_list, list)
		tmp->dl_hdr_handler(dlhdr);
}

static  void rmnet_map_dl_trl_notify(struct rmnet_port *port,
				     struct rmnet_map_dl_ind_trl *dltrl)
{
	struct rmnet_map_dl_ind *tmp;

	list_for_each_entry(tmp, &port->dl_list, list)
		tmp->dl_trl_handler(dltrl);
}

static void rmnet_map_process_flow_start(struct sk_buff *skb,
					 struct rmnet_port *port)
{
	struct rmnet_map_dl_ind_hdr *dlhdr;

	if (skb->len < RMNET_DL_IND_HDR_SIZE)
		return;

	skb_pull(skb, RMNET_MAP_CMD_SIZE);

	dlhdr = (struct rmnet_map_dl_ind_hdr *)skb->data;

	port->stats.dl_hdr_last_seq = dlhdr->le.seq;
	port->stats.dl_hdr_last_bytes = dlhdr->le.bytes;
	port->stats.dl_hdr_last_pkts = dlhdr->le.pkts;
	port->stats.dl_hdr_last_flows = dlhdr->le.flows;
	port->stats.dl_hdr_total_bytes += port->stats.dl_hdr_last_bytes;
	port->stats.dl_hdr_total_pkts += port->stats.dl_hdr_last_pkts;
	port->stats.dl_hdr_count++;

	if (unlikely(!(port->stats.dl_hdr_count)))
		port->stats.dl_hdr_count = 1;

	port->stats.dl_hdr_avg_bytes = port->stats.dl_hdr_total_bytes /
				       port->stats.dl_hdr_count;

	port->stats.dl_hdr_avg_pkts = port->stats.dl_hdr_total_pkts /
				      port->stats.dl_hdr_count;

	rmnet_map_dl_hdr_notify(port, dlhdr);
}

static void rmnet_map_process_flow_end(struct sk_buff *skb,
				       struct rmnet_port *port)
{
	struct rmnet_map_dl_ind_trl *dltrl;

	if (skb->len < RMNET_DL_IND_TRL_SIZE)
		return;

	skb_pull(skb, RMNET_MAP_CMD_SIZE);

	dltrl = (struct rmnet_map_dl_ind_trl *)skb->data;

	port->stats.dl_trl_last_seq = dltrl->seq_le;
	port->stats.dl_trl_count++;

	rmnet_map_dl_trl_notify(port, dltrl);
}

/* Process MAP command frame and send N/ACK message as appropriate. Message cmd
 * name is decoded here and appropriate handler is called.
 */
void rmnet_map_command(struct sk_buff *skb, struct rmnet_port *port)
{
	struct rmnet_map_control_command *cmd;
	unsigned char command_name;
	unsigned char rc = 0;

	cmd = RMNET_MAP_GET_CMD_START(skb);
	command_name = cmd->command_name;

	switch (command_name) {
	case RMNET_MAP_COMMAND_FLOW_ENABLE:
		rc = rmnet_map_do_flow_control(skb, port, 1);
		break;

	case RMNET_MAP_COMMAND_FLOW_DISABLE:
		rc = rmnet_map_do_flow_control(skb, port, 0);
		break;

	default:
		rc = RMNET_MAP_COMMAND_UNSUPPORTED;
		kfree_skb(skb);
		break;
	}
	if (rc == RMNET_MAP_COMMAND_ACK)
		rmnet_map_send_ack(skb, rc, port);
}

int rmnet_map_flow_command(struct sk_buff *skb, struct rmnet_port *port)
{
	struct rmnet_map_control_command *cmd;
	unsigned char command_name;

	cmd = RMNET_MAP_GET_CMD_START(skb);
	command_name = cmd->command_name;

	switch (command_name) {
	case RMNET_MAP_COMMAND_FLOW_START:
		rmnet_map_process_flow_start(skb, port);
		break;

	case RMNET_MAP_COMMAND_FLOW_END:
		rmnet_map_process_flow_end(skb, port);
		break;

	default:
		return 1;
	}

	consume_skb(skb);
	return 0;
}

void rmnet_map_cmd_exit(struct rmnet_port *port)
{
	struct rmnet_map_dl_ind *tmp, *idx;

	list_for_each_entry_safe(tmp, idx, &port->dl_list, list)
		list_del_rcu(&tmp->list);
}

void rmnet_map_cmd_init(struct rmnet_port *port)
{
	INIT_LIST_HEAD(&port->dl_list);
}

int rmnet_map_dl_ind_register(struct rmnet_port *port,
			      struct rmnet_map_dl_ind *dl_ind)
{
	if (!port || !dl_ind || !dl_ind->dl_hdr_handler ||
	    !dl_ind->dl_trl_handler)
		return -EINVAL;

	list_add_rcu(&dl_ind->list, &port->dl_list);

	return 0;
}

int rmnet_map_dl_ind_deregister(struct rmnet_port *port,
				struct rmnet_map_dl_ind *dl_ind)
{
	struct rmnet_map_dl_ind *tmp;

	if (!port || !dl_ind)
		return -EINVAL;

	list_for_each_entry(tmp, &port->dl_list, list) {
		if (tmp == dl_ind) {
			list_del_rcu(&dl_ind->list);
			goto done;
		}
	}

done:
	return 0;
}
