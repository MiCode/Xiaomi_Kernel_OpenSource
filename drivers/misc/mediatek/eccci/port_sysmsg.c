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
#include <linux/kernel.h>

#include "ccci_config.h"
#include "ccci_core.h"
#include "ccci_bm.h"
#include "port_proxy.h"
#include "port_sysmsg.h"
#define MAX_QUEUE_LENGTH 16

#ifdef TEST_MESSAGE_FOR_BRINGUP
static inline int port_sys_notify_md(struct ccci_port *port, unsigned int msg, unsigned int data)
{
	return port_proxy_send_msg_to_md(port->port_proxy, CCCI_SYSTEM_TX, msg, data, 1);
}

static inline int port_sys_echo_test(struct ccci_port *port, int data)
{
	CCCI_DEBUG_LOG(port->md_id, KERN, "system message: Enter ccci_sysmsg_echo_test data= %08x", data);
	port_sys_notify_md(port, TEST_MSG_ID_AP2MD, data);
	return 0;
}

static inline int port_sys_echo_test_l1core(struct ccci_port *port, int data)
{
	CCCI_DEBUG_LOG(port->md_id, KERN, "system message: Enter ccci_sysmsg_echo_test_l1core data= %08x", data);
	port_sys_notify_md(port, TEST_MSG_ID_L1CORE_AP2MD, data);
	return 0;
}
#endif
/* for backward compatibility */
ccci_sys_cb_func_info_t ccci_sys_cb_table_100[MAX_MD_NUM][MAX_KERN_API];
ccci_sys_cb_func_info_t ccci_sys_cb_table_1000[MAX_MD_NUM][MAX_KERN_API];
int register_ccci_sys_call_back(int md_id, unsigned int id, ccci_sys_cb_func_t func)
{
	int ret = 0;
	ccci_sys_cb_func_info_t *info_ptr;

	if (md_id >= MAX_MD_NUM) {
		CCCI_ERROR_LOG(md_id, KERN, "register_sys_call_back fail: invalid md id\n");
		return -EINVAL;
	}

	if ((id >= 0x100) && ((id - 0x100) < MAX_KERN_API)) {
		info_ptr = &(ccci_sys_cb_table_100[md_id][id - 0x100]);
	} else if ((id >= 0x1000) && ((id - 0x1000) < MAX_KERN_API)) {
		info_ptr = &(ccci_sys_cb_table_1000[md_id][id - 0x1000]);
	} else {
		CCCI_ERROR_LOG(md_id, KERN, "register_sys_call_back fail: invalid func id(0x%x)\n", id);
		return -EINVAL;
	}

	if (info_ptr->func == NULL) {
		info_ptr->id = id;
		info_ptr->func = func;
	} else {
		CCCI_ERROR_LOG(md_id, KERN, "register_sys_call_back fail: func(0x%x) registered!\n", id);
	}

	return ret;
}

void exec_ccci_sys_call_back(int md_id, int cb_id, int data)
{
	ccci_sys_cb_func_t func;
	int id;
	ccci_sys_cb_func_info_t *curr_table;

	if (md_id >= MAX_MD_NUM) {
		CCCI_ERROR_LOG(md_id, KERN, "exec_sys_cb fail: invalid md id\n");
		return;
	}

	id = cb_id & 0xFF;
	if (id >= MAX_KERN_API) {
		CCCI_ERROR_LOG(md_id, KERN, "exec_sys_cb fail: invalid func id(0x%x)\n", cb_id);
		return;
	}

	if ((cb_id & (0x1000 | 0x100)) == 0x1000) {
		curr_table = ccci_sys_cb_table_1000[md_id];
	} else if ((cb_id & (0x1000 | 0x100)) == 0x100) {
		curr_table = ccci_sys_cb_table_100[md_id];
	} else {
		CCCI_ERROR_LOG(md_id, KERN, "exec_sys_cb fail: invalid func id(0x%x)\n", cb_id);
		return;
	}

	func = curr_table[id].func;
	if (func != NULL)
		func(md_id, data);
	else
		CCCI_ERROR_LOG(md_id, KERN, "exec_sys_cb fail: func id(0x%x) not register!\n", cb_id);
}

static void sys_msg_handler(struct ccci_port *port, struct sk_buff *skb)
{
	struct ccci_header *ccci_h = (struct ccci_header *)skb->data;
	int md_id = port->md_id;

	CCCI_NORMAL_LOG(md_id, KERN, "system message (%x %x %x %x)\n", ccci_h->data[0], ccci_h->data[1],
		     ccci_h->channel, ccci_h->reserved);
	switch (ccci_h->data[1]) {
	case MD_GET_BATTERY_INFO:
		port_proxy_send_msg_to_user(port->port_proxy, CCCI_MONITOR_CH, CCCI_MD_MSG_SEND_BATTERY_INFO, 0);
		break;
	case MD_WDT_MONITOR:
		/* abandoned */
		break;
#ifdef CONFIG_MTK_SIM_LOCK_POWER_ON_WRITE_PROTECT
	case MD_RESET_AP:
		port_proxy_send_msg_to_user(port->port_proxy, CCCI_MONITOR_CH, CCCI_MD_MSG_RANDOM_PATTERN, 0);
		break;
#endif
	case MD_SIM_TYPE:
		port_proxy_set_sim_type(port->port_proxy, ccci_h->reserved);
		CCCI_NORMAL_LOG(md_id, KERN, "MD send sys msg sim type(0x%x)\n", ccci_h->reserved);
		break;
#ifdef TEST_MESSAGE_FOR_BRINGUP
	case TEST_MSG_ID_MD2AP:
		port_sys_echo_test(port, ccci_h->reserved);
		break;
	case TEST_MSG_ID_L1CORE_MD2AP:
		port_sys_echo_test_l1core(port, ccci_h->reserved);
		break;
#endif
#ifdef FEATURE_SCP_CCCI_SUPPORT
	case CCISM_SHM_INIT_ACK:
		port_proxy_ccism_shm_init_ack_hdlr(port->port_proxy, 0);
		break;
#endif
	case MD_TX_POWER:
		/* Fall through */
	case MD_RF_TEMPERATURE:
		/* Fall through */
	case MD_RF_TEMPERATURE_3G:
		/* Fall through */
#ifdef FEATURE_MTK_SWITCH_TX_POWER
	case MD_SW_MD1_TX_POWER_REQ:
		/* Fall through */
#endif
		exec_ccci_sys_call_back(md_id, ccci_h->data[1], ccci_h->reserved);
		break;

	};
	ccci_free_skb(skb);
}

static int port_sys_init(struct ccci_port *port)
{
	CCCI_DEBUG_LOG(port->md_id, KERN, "kernel port %s is initializing\n", port->name);
	port->skb_handler = &sys_msg_handler;
	port->private_data = kthread_run(port_kthread_handler, port, "%s", port->name);
	port->rx_length_th = MAX_QUEUE_LENGTH;
	port->skb_from_pool = 1;

	return 0;
}

struct ccci_port_ops sys_port_ops = {
	.init = &port_sys_init,
	.recv_skb = &port_recv_skb,
};
