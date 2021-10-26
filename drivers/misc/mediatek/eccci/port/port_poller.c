/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/device.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/sched/clock.h> /* local_clock() */
#include <linux/kthread.h>
#include <linux/rtc.h>
#include <linux/kernel.h>
#include <linux/random.h>
#include <linux/rtc.h>
#include <linux/timer.h>
#include "ccci_config.h"

#include "ccci_core.h"
#include "ccci_bm.h"
#include "ccci_fsm.h"
#include "port_poller.h"

#define MAX_QUEUE_LENGTH 16

static void status_msg_handler(struct port_t *port, struct sk_buff *skb)
{
	int ret = 0;

	ret = ccci_fsm_recv_status_packet(port->md_id, skb);
	if (ret)
		CCCI_ERROR_LOG(port->md_id, PORT,
			"%s status poller gotten error: %d\n", port->name, ret);
}

static int port_poller_init(struct port_t *port)
{
	CCCI_DEBUG_LOG(port->md_id, PORT,
		"kernel port %s is initializing\n", port->name);
	port->rx_length_th = MAX_QUEUE_LENGTH;
	port->skb_from_pool = 1;
	port->skb_handler = &status_msg_handler;
	return 0;
}

struct port_ops poller_port_ops = {
	.init = &port_poller_init,
	.recv_skb = &port_recv_skb,
};

