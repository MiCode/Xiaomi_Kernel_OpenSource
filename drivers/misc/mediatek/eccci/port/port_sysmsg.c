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
#include <linux/kernel.h>
#include <mt-plat/mtk_battery.h>

#include "ccci_config.h"
#include "ccci_core.h"
#include "ccci_bm.h"
#include "port_sysmsg.h"
#include "ccci_swtp.h"
#define MAX_QUEUE_LENGTH 16

struct md_rf_notify_struct {
	unsigned int bit;
	void (*notify_func)(unsigned int para0, unsigned int para1);
	const char *module_name;
};

#define MD_RF_NOTIFY(bit, func, name) \
	__weak void func(unsigned int para0, unsigned int para1) \
	{ \
		pr_debug("[ccci1/SYS]weak %s", __func__); \
	}
#include "mdrf_notify_list.h"

#undef MD_RF_NOTIFY
#define MD_RF_NOTIFY(bit, func, name) \
	{bit, func, name},

static struct md_rf_notify_struct notify_members[] = {
	#include "mdrf_notify_list.h"
};

#define NOTIFY_LIST_ITM_NUM  ARRAY_SIZE(notify_members)

static void sys_msg_MD_RF_Notify(int md_id, unsigned int bit_value,
	unsigned int data_1)
{
	int i;
	unsigned int data_send;

	for (i = 0; i < NOTIFY_LIST_ITM_NUM; i++) {
		data_send = (bit_value&(1<<notify_members[i].bit))
			>> notify_members[i].bit;

		CCCI_NORMAL_LOG(md_id, SYS, "0x121: notify to (%s)\n",
			notify_members[i].module_name);

		notify_members[i].notify_func(data_send, data_1);
	}
}

#ifndef TEST_MESSAGE_FOR_BRINGUP
#define TEST_MESSAGE_FOR_BRINGUP
#endif

#ifdef TEST_MESSAGE_FOR_BRINGUP
static inline int port_sys_notify_md(struct port_t *port, unsigned int msg,
	unsigned int data)
{
	return port_send_msg_to_md(port, msg, data, 1);
}

static inline int port_sys_echo_test(struct port_t *port, int data)
{
	CCCI_DEBUG_LOG(port->md_id, SYS,
		"system message: Enter ccci_sysmsg_echo_test data= %08x",
		data);
	port_sys_notify_md(port, TEST_MSG_ID_AP2MD, data);
	return 0;
}

static inline int port_sys_echo_test_l1core(struct port_t *port, int data)
{
	CCCI_DEBUG_LOG(port->md_id, SYS,
		"system message: Enter ccci_sysmsg_echo_test_l1core data= %08x",
		data);
	port_sys_notify_md(port, TEST_MSG_ID_L1CORE_AP2MD, data);
	return 0;
}
#endif
/* for backward compatibility */
struct ccci_sys_cb_func_info ccci_sys_cb_table_100[MAX_MD_NUM][MAX_KERN_API];
struct ccci_sys_cb_func_info ccci_sys_cb_table_1000[MAX_MD_NUM][MAX_KERN_API];

int register_ccci_sys_call_back(int md_id, unsigned int id,
	ccci_sys_cb_func_t func)
{
	int ret = 0;
	struct ccci_sys_cb_func_info *info_ptr = NULL;

	if (md_id >= MAX_MD_NUM) {
		CCCI_ERROR_LOG(md_id, SYS,
			"register_sys_call_back fail: invalid md id\n");
		return -EINVAL;
	}

	if ((id >= 0x100) && ((id - 0x100) < MAX_KERN_API)) {
		info_ptr = &(ccci_sys_cb_table_100[md_id][id - 0x100]);
	} else if ((id >= 0x1000) && ((id - 0x1000) < MAX_KERN_API)) {
		info_ptr = &(ccci_sys_cb_table_1000[md_id][id - 0x1000]);
	} else {
		CCCI_ERROR_LOG(md_id, SYS,
			"register_sys_call_back fail: invalid func id(0x%x)\n",
			id);
		return -EINVAL;
	}

	if (info_ptr->func == NULL) {
		info_ptr->id = id;
		info_ptr->func = func;
	} else {
		CCCI_ERROR_LOG(md_id, SYS,
			"register_sys_call_back fail: func(0x%x) registered! %ps\n",
			id, info_ptr->func);
	}

	return ret;
}

void exec_ccci_sys_call_back(int md_id, int cb_id, int data)
{
	ccci_sys_cb_func_t func;
	int id;
	struct ccci_sys_cb_func_info *curr_table = NULL;

	if (md_id >= MAX_MD_NUM) {
		CCCI_ERROR_LOG(md_id, SYS,
			"exec_sys_cb fail: invalid md id\n");
		return;
	}

	id = cb_id & 0xFF;
	if (id >= MAX_KERN_API) {
		CCCI_ERROR_LOG(md_id, SYS,
			"exec_sys_cb fail: invalid func id(0x%x)\n", cb_id);
		return;
	}

	if ((cb_id & (0x1000 | 0x100)) == 0x1000) {
		curr_table = ccci_sys_cb_table_1000[md_id];
	} else if ((cb_id & (0x1000 | 0x100)) == 0x100) {
		curr_table = ccci_sys_cb_table_100[md_id];
	} else {
		CCCI_ERROR_LOG(md_id, SYS,
			"exec_sys_cb fail: invalid func id(0x%x)\n", cb_id);
		return;
	}

	func = curr_table[id].func;
	if (func != NULL)
		func(md_id, data);
	else
		CCCI_ERROR_LOG(md_id, SYS,
			"exec_sys_cb fail: func id(0x%x) not register!\n",
			cb_id);
}

signed int __weak battery_get_bat_voltage(void)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}

static int sys_msg_send_battery(struct port_t *port)
{
	int data;

	data = (int)battery_get_bat_voltage();
	CCCI_REPEAT_LOG(port->md_id, SYS, "get bat voltage %d\n", data);
	port_send_msg_to_md(port, MD_GET_BATTERY_INFO, data, 1);
	return 0;
}

static void sys_msg_handler(struct port_t *port, struct sk_buff *skb)
{
	struct ccci_header *ccci_h = (struct ccci_header *)skb->data;
	int md_id = port->md_id;
	unsigned long rem_nsec;
	u64 ts_nsec, ref;

	CCCI_NORMAL_LOG(md_id, SYS, "system message (%x %x %x %x)\n",
		ccci_h->data[0], ccci_h->data[1],
		ccci_h->channel, ccci_h->reserved);

	ts_nsec = sched_clock();
	ref = ts_nsec;
	rem_nsec = do_div(ts_nsec, 1000000000);
	CCCI_HISTORY_LOG(md_id, SYS, "[%5lu.%06lu]sysmsg-%08x %08x %04x\n",
			(unsigned long)ts_nsec, rem_nsec / 1000,
			ccci_h->data[1], ccci_h->reserved, ccci_h->seq_num);

	switch (ccci_h->data[1]) {
	case MD_WDT_MONITOR:
		/* abandoned */
		break;
#ifdef TEST_MESSAGE_FOR_BRINGUP
	case TEST_MSG_ID_MD2AP:
		port_sys_echo_test(port, ccci_h->reserved);
		break;
	case TEST_MSG_ID_L1CORE_MD2AP:
		port_sys_echo_test_l1core(port, ccci_h->reserved);
		break;
#endif
#ifdef CONFIG_MTK_SIM_LOCK_POWER_ON_WRITE_PROTECT
	case SIM_LOCK_RANDOM_PATTERN:
		fsm_monitor_send_message(md_id, CCCI_MD_MSG_RANDOM_PATTERN, 0);
		break;
#endif
#ifdef FEATURE_SCP_CCCI_SUPPORT
	case CCISM_SHM_INIT_ACK:
		/* Fall through */
#endif
	case MD_SIM_TYPE:
		/* Fall through */
	case MD_TX_POWER:
		/* Fall through */
	case MD_RF_TEMPERATURE:
		/* Fall through */
	case MD_RF_TEMPERATURE_3G:
		/* Fall through */
	case MD_SW_MD1_TX_POWER_REQ:
		/* Fall through */
	case MD_DISPLAY_DYNAMIC_MIPI:
		/* Fall through */
	case LWA_CONTROL_MSG:
		exec_ccci_sys_call_back(md_id, ccci_h->data[1],
			ccci_h->reserved);
		break;
	case MD_GET_BATTERY_INFO:
		sys_msg_send_battery(port);
		break;
	case MD_RF_HOPPING_NOTIFY:
		sys_msg_MD_RF_Notify(md_id, ccci_h->reserved, ccci_h->data[0]);
		break;
	};
	ccci_free_skb(skb);

	ts_nsec = sched_clock();
#ifdef __LP64__
	rem_nsec = (unsigned long)((ts_nsec - ref) / 1000);
#else
	ts_nsec = ts_nsec - ref;
	rem_nsec = do_div(ts_nsec, 1000LL);
#endif

	CCCI_HISTORY_LOG(md_id, SYS, "cost: %lu us\n", rem_nsec);
}

static int port_sys_init(struct port_t *port)
{
	CCCI_DEBUG_LOG(port->md_id, SYS,
		"kernel port %s is initializing\n", port->name);

	if (port->md_id == MD_SYS1)
		swtp_init(port->md_id);
	port->skb_handler = &sys_msg_handler;
	port->private_data = kthread_run(port_kthread_handler, port, "%s",
							port->name);
	port->rx_length_th = MAX_QUEUE_LENGTH;
	port->skb_from_pool = 1;
	return 0;
}

struct port_ops sys_port_ops = {
	.init = &port_sys_init,
	.recv_skb = &port_recv_skb,
};
