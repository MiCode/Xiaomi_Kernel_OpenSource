// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */
#include <linux/kernel.h>
#include <linux/kthread.h>
#include "ccci_config.h"
#include "ccci_common_config.h"

#include "ccci_core.h"
#include "ccci_fsm.h"
#include "ccci_bm.h"
#include "port_ctlmsg.h"

#define MAX_QUEUE_LENGTH 16
/*
 * all supported modems should follow these
 * handshake messages as a protocol.
 * but we still can support un-usual modem by
 * providing cutomed kernel_port_ops.
 */
static void control_msg_handler(struct port_t *port, struct sk_buff *skb)
{
	int md_id = port->md_id;
	int ret = 0;

	ret = ccci_fsm_recv_control_packet(md_id, skb);
	if (ret)
		CCCI_ERROR_LOG(port->md_id, PORT,
			"%s control msg gotten error: %d\n",
			port->name, ret);
}

static int port_ctl_init(struct port_t *port)
{
	CCCI_DEBUG_LOG(port->md_id, PORT,
		"kernel port %s is initializing\n", port->name);
	port->skb_handler = &control_msg_handler;
	port->private_data = kthread_run(port_kthread_handler,
		port, "%s", port->name);
	port->rx_length_th = MAX_QUEUE_LENGTH;
	port->skb_from_pool = 1;
	return 0;
}

struct port_ops ctl_port_ops = {
	.init = &port_ctl_init,
	.recv_skb = &port_recv_skb,
};
