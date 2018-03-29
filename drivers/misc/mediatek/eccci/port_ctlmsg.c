/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/kthread.h>
#include "ccci_config.h"

#include "ccci_core.h"
#include "ccci_modem.h"
#include "ccci_bm.h"
#include "ccci_platform.h"
#include "port_proxy.h"
#include "port_ctlmsg.h"

#define MAX_QUEUE_LENGTH 16

static int normal_msg_handler(struct ccci_port *port, struct sk_buff *skb)
{
	int ret = 1;
	struct ccci_header *ccci_h = (struct ccci_header *)skb->data;

	if (ccci_h->data[1] == MD_INIT_START_BOOT
	    && ccci_h->reserved == MD_INIT_CHK_ID) {
		port_proxy_md_hs1_msg_notify(port->port_proxy, skb);
	} else if (ccci_h->data[1] == MD_NORMAL_BOOT) {
		port_proxy_md_hs2_msg_notify(port->port_proxy, skb);
	} else {
		ret = 0;
	}

	return ret;
}

static int c2k_msg_handler(struct ccci_port *port, struct sk_buff *skb)
{
	int md_id = port->md_id;
	struct ccci_header *ccci_h = (struct ccci_header *)skb->data;
	struct c2k_ctrl_port_msg *c2k_ctl_msg = NULL;
	int ret = 1;
	struct ccci_port *status_port;

	if (port->md_id != MD_SYS3)
		return 0;
	if (ccci_h->data[1] == C2K_HB_MSG) {
		status_port = port_proxy_get_port_by_channel(port->port_proxy, CCCI_STATUS_RX);
		status_port->skb_handler(status_port, skb);
		CCCI_NORMAL_LOG(md_id, KERN, "heart beat msg received\n");
		return 2;/*status handler freed skb*/
	} else if (ccci_h->data[1] == C2K_STATUS_IND_MSG) {
		c2k_ctl_msg = (struct c2k_ctrl_port_msg *)&ccci_h->reserved;
		CCCI_NORMAL_LOG(md_id, KERN, "c2k status ind 0x%02x\n", c2k_ctl_msg->option);
		if (c2k_ctl_msg->option & 0x80)	/*connect */
			port_proxy_set_dtr_state(port->port_proxy, 1);
		else		/*disconnect */
			port_proxy_set_dtr_state(port->port_proxy, 0);
	} else if (ccci_h->data[1] == C2K_STATUS_QUERY_MSG) {
		c2k_ctl_msg = (struct c2k_ctrl_port_msg *)&ccci_h->reserved;
		CCCI_NORMAL_LOG(md_id, KERN, "c2k status query 0x%02x\n", c2k_ctl_msg->option);
		if (c2k_ctl_msg->option & 0x80)	/*connect */
			port_proxy_set_dtr_state(port->port_proxy, 1);
		else		/*disconnect */
			port_proxy_set_dtr_state(port->port_proxy, 0);
#ifdef FEATURE_SCP_CCCI_SUPPORT
	} else if (ccci_h->data[1] == C2K_CCISM_SHM_INIT_ACK) {
		port_proxy_ccism_shm_init_ack_hdlr(port->port_proxy, 0);
#endif
	} else if (ccci_h->data[1] == C2K_FLOW_CTRL_MSG) {
		CCCI_NORMAL_LOG(md_id, KERN, "flow ctrl msg: queue = 0x%x\n", ccci_h->reserved);
		port_proxy_wake_up_tx_queue(port->port_proxy, ccci_h->reserved);
	} else {
		ret = 0;
	}

	return ret;
}
/*
 * all supported modems should follow these handshake messages as a protocol.
 * but we still can support un-usual modem by providing cutomed kernel_port_ops.
 */
static void control_msg_handler(struct ccci_port *port, struct sk_buff *skb)
{
	int md_id = port->md_id;
	struct ccci_header *ccci_h = (struct ccci_header *)skb->data;
	int ret = 0;

	CCCI_NORMAL_LOG(md_id, KERN, "control message 0x%X,0x%X\n", ccci_h->data[1], ccci_h->reserved);
	ccci_event_log("md%d: control message 0x%X,0x%X\n", md_id, ccci_h->data[1], ccci_h->reserved);
	switch (ccci_h->data[1]) {
	case MD_INIT_START_BOOT: /*MD_INIT_START_BOOT == MD_NORMAL_BOOT == 0*/
		ret = normal_msg_handler(port, skb);
	break;
	case MD_EX:
	case MD_EX_REC_OK:
	case MD_EX_PASS:
	case CCCI_DRV_VER_ERROR:
		ret = mdee_ctlmsg_handler(port, skb);
		break;
	case C2K_HB_MSG:
	case C2K_STATUS_IND_MSG:
	case C2K_STATUS_QUERY_MSG:
	case C2K_CCISM_SHM_INIT_ACK:
	case C2K_FLOW_CTRL_MSG:
	if (port->md_id == MD_SYS3)
		ret = c2k_msg_handler(port, skb);
		break;
	default:
		CCCI_ERROR_LOG(port->md_id, KERN, "receive unknown data from CCCI_CONTROL_RX = %d\n", ccci_h->data[1]);
		break;
	}
	if (ret != 2)
		ccci_free_skb(skb);
}

static int port_ctl_init(struct ccci_port *port)
{
	CCCI_DEBUG_LOG(port->md_id, KERN, "kernel port %s is initializing\n", port->name);
	port->skb_handler = &control_msg_handler;
	port->private_data = kthread_run(port_kthread_handler, port, "%s", port->name);
	port->rx_length_th = MAX_QUEUE_LENGTH;
	port->skb_from_pool = 1;
	return 0;
}

struct ccci_port_ops ctl_port_ops = {
	.init = &port_ctl_init,
	.recv_skb = &port_recv_skb,
};
