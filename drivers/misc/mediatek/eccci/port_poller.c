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

#include <linux/device.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/rtc.h>
#include <linux/kernel.h>
#include <linux/random.h>
#include <linux/rtc.h>
#include <linux/timer.h>
#include "ccci_config.h"

#include "ccci_core.h"
#include "ccci_bm.h"
#include "port_proxy.h"
#include "port_poller.h"
#define MAX_QUEUE_LENGTH 16
#define POLLING_INTERVAL_TIME 15
#define POLLING_TIMEOUT 15

static void status_msg_handler(struct ccci_port *port, struct sk_buff *skb)
{
	struct port_md_status_poller *status_poller = port->private_data;

	port_proxy_record_rx_sched_time(port->port_proxy, CCCI_STATUS_RX);
	del_timer(&status_poller->md_status_timeout);
	ccci_util_cmpt_mem_dump(port->md_id, CCCI_DUMP_REPEAT, skb->data, skb->len);
	ccci_free_skb(skb);
	if (port_proxy_get_md_state(port->port_proxy) == READY)
		mod_timer(&status_poller->md_status_poller, jiffies + POLLING_INTERVAL_TIME * HZ);
}

static void md_status_poller_func(unsigned long data)
{
	struct ccci_port *port = (struct ccci_port *)data;
	struct port_md_status_poller *status_poller = port->private_data;
	int md_id = port->md_id;
	int ret = 0;

	status_poller->latest_poll_start_time = local_clock();
	mod_timer(&status_poller->md_status_timeout, jiffies + POLLING_TIMEOUT * HZ);
	ret = port_proxy_send_msg_to_md(port->port_proxy, CCCI_STATUS_TX, 0, 0, 0);
	CCCI_REPEAT_LOG(md_id, KERN, "poll modem status %d seq=0x%X\n", ret,
		port_proxy_get_poll_seq_num(port->port_proxy));

	if (ret) {
		CCCI_ERROR_LOG(md_id, KERN, "fail to send modem status polling msg ret=%d\n", ret);
		del_timer(&status_poller->md_status_timeout);
		if ((ret == -EBUSY || (ret == -CCCI_ERR_ALLOCATE_MEMORY_FAIL))
			&& port_proxy_get_md_state(port->port_proxy) == READY) {
			if (status_poller->md_status_poller_flag & MD_STATUS_POLL_BUSY) {
				port_proxy_md_no_repsone_notify(port->port_proxy);
			} else if (ret == -CCCI_ERR_ALLOCATE_MEMORY_FAIL) {
				mod_timer(&status_poller->md_status_poller, jiffies + POLLING_INTERVAL_TIME * HZ);
			} else {
				status_poller->md_status_poller_flag |= MD_STATUS_POLL_BUSY;
				mod_timer(&status_poller->md_status_poller, jiffies + POLLING_INTERVAL_TIME * HZ);
			}
		}
	} else {
		status_poller->md_status_poller_flag &= ~MD_STATUS_POLL_BUSY;
	}
}

static void md_status_timeout_func(unsigned long data)
{
	struct ccci_port *port = (struct ccci_port *)data;
	struct port_md_status_poller *status_poller = port->private_data;
	int md_id = port->md_id;

	if (status_poller->md_status_poller_flag & MD_STATUS_ASSERTED) {
		CCCI_ERROR_LOG(md_id, KERN, "modem status polling timeout, assert fail,flag=0x%x",
			status_poller->md_status_poller_flag);
		port_proxy_md_no_repsone_notify(port->port_proxy);
	} else {
		CCCI_ERROR_LOG(md_id, KERN, "modem status polling timeout, force assert\n");
		status_poller->md_status_poller_flag |= MD_STATUS_ASSERTED;
		mod_timer(&status_poller->md_status_timeout, jiffies + POLLING_TIMEOUT * HZ);
		port_proxy_poll_md_fail_notify(port->port_proxy, status_poller->latest_poll_start_time);
	}
}

static void port_poller_md_state_notice(struct ccci_port *port, MD_STATE state)
{
	struct port_md_status_poller *status_poller = port->private_data;

	/* only for thoes states which are updated by modem driver */
	switch (state) {
	case EXCEPTION:
	case WAITING_TO_STOP:
		del_timer(&status_poller->md_status_poller);
		del_timer(&status_poller->md_status_timeout);
		status_poller->md_status_poller_flag = 0;
		break;
	default:
		break;
	};
}
void port_poller_start(struct ccci_port *port)
{
#ifdef FEATURE_POLL_MD_EN
	struct port_md_status_poller *status_poller = port->private_data;

	if (port_proxy_get_md_state(port->port_proxy) == READY)
		mod_timer(&status_poller->md_status_poller, jiffies + POLLING_INTERVAL_TIME * HZ);
#endif
}
void port_poller_stop(struct ccci_port *port)
{
#ifdef FEATURE_POLL_MD_EN
	struct port_md_status_poller *status_poller = port->private_data;

	del_timer(&status_poller->md_status_poller);
#endif
}

static int port_poller_init(struct ccci_port *port)
{
	struct port_md_status_poller *status_poller;

	CCCI_DEBUG_LOG(port->md_id, KERN, "kernel port %s is initializing\n", port->name);
	/* md1: handle directly in recv_skb
	* md3: handle it in contrl message handler
	*/
	port->rx_length_th = MAX_QUEUE_LENGTH;
	port->skb_from_pool = 1;
	port->skb_handler = &status_msg_handler;
	status_poller = kzalloc(sizeof(struct port_md_status_poller), GFP_KERNEL);
	port->private_data = status_poller;
	init_timer(&status_poller->md_status_poller);
	status_poller->md_status_poller.function = md_status_poller_func;
	status_poller->md_status_poller.data = (unsigned long)port;
	init_timer(&status_poller->md_status_timeout);
	status_poller->md_status_timeout.function = md_status_timeout_func;
	status_poller->md_status_timeout.data = (unsigned long)port;

	return 0;
}

struct ccci_port_ops poller_port_ops = {
	.init = &port_poller_init,
	.recv_skb = &port_recv_skb,
	.md_state_notice = &port_poller_md_state_notice,
};

