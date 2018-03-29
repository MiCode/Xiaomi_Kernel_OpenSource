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
#include <linux/kthread.h>
#include "ccci_config.h"
#include "ccci_core.h"
#include "ccci_bm.h"
#include "ccci_support.h"
#include "ccci_platform.h"
#include "port_proxy.h"
#ifdef FEATURE_GET_MD_BAT_VOL	/* must be after ccci_config.h */
#include <mt-plat/battery_common.h>
#else
#define BAT_Get_Battery_Voltage(polling_mode)    ({ 0; })
#endif
#ifdef CONFIG_MTK_SIM_LOCK_POWER_ON_WRITE_PROTECT
#include <mt-plat/env.h>
#endif
#include <mt-plat/mt_boot_common.h>

#define TAG     "port"
#define CCCI_DEV_NAME "ccci"

/*******************************************************************************************/
/*REGION: port class default method implementation*/
/*******************************************************************************************/
/* structure initialize */
static inline void port_struct_init(struct ccci_port *port, struct port_proxy *port_p)
{
	INIT_LIST_HEAD(&port->entry);
	skb_queue_head_init(&port->rx_skb_list);
	init_waitqueue_head(&port->rx_wq);
	port->tx_busy_count = 0;
	port->rx_busy_count = 0;
	atomic_set(&port->usage_cnt, 0);
	port->port_proxy = port_p;
	port->modem = port_p->md_obj;
	port->md_id = port_p->md_id;

	wake_lock_init(&port->rx_wakelock, WAKE_LOCK_SUSPEND, port->name);
}

static void port_dump_string(struct ccci_port *port, int dir, void *msg_buf, int len)
{
#define DUMP_BUF_SIZE 32
	unsigned char *char_ptr = (unsigned char *)msg_buf;
	char buf[DUMP_BUF_SIZE];
	int i, j;
	u64 ts_nsec;
	unsigned long rem_nsec;
	char *replace_str;

	for (i = 0, j = 0; i < len && i < DUMP_BUF_SIZE && j + 4 < DUMP_BUF_SIZE; i++) {
		if (((char_ptr[i] >= 32) && (char_ptr[i] <= 126))) {
			buf[j] = char_ptr[i];
			j += 1;
		} else if (char_ptr[i] == '\r' ||
			char_ptr[i] == '\n' ||
			char_ptr[i] == '\t') {
			switch (char_ptr[i]) {
			case '\r':
				replace_str = "\\r";
				break;
			case '\n':
				replace_str = "\\n";
				break;
			case '\t':
				replace_str = "\\t";
				break;
			default:
				replace_str = "";
				break;
			}
			snprintf(buf+j, DUMP_BUF_SIZE - j, "%s", replace_str);
			j += 2;
		} else {
			snprintf(buf+j, DUMP_BUF_SIZE - j, "[%02X]", char_ptr[i]);
			j += 4;
		}
	}
	buf[j] = '\0';
	ts_nsec = local_clock();
	rem_nsec = do_div(ts_nsec, 1000000000);
	if (dir == 0)
		CCCI_HISTORY_LOG(port->md_id, TAG, "[%5lu.%06lu]C:%d,%d(%d,%d,%d) %s: %d<%s\n",
			(unsigned long)ts_nsec, rem_nsec / 1000, port->flags, port->rx_ch,
			port->rx_skb_list.qlen, port->rx_pkg_cnt, port->rx_drop_cnt, "R", len, buf);
	else
		CCCI_HISTORY_LOG(port->md_id, TAG, "[%5lu.%06lu]C:%d,%d(%d) %s: %d>%s\n",
			(unsigned long)ts_nsec, rem_nsec / 1000, port->flags, port->tx_ch,
			port->tx_pkg_cnt, "W", len, buf);
}
static void port_dump_raw_data(struct ccci_port *port, int dir, void *msg_buf, int len)
{
#define DUMP_RAW_DATA_SIZE 16
	unsigned int *curr_p = (unsigned int *)msg_buf;
	unsigned char *curr_ch_p;
	int _16_fix_num = len / 16;
	int tail_num = len % 16;
	char buf[16];
	int i, j;
	int dump_size;
	u64 ts_nsec;
	unsigned long rem_nsec;

	dump_size = len > DUMP_RAW_DATA_SIZE ? DUMP_RAW_DATA_SIZE : len;
	_16_fix_num = dump_size / 16;
	tail_num = dump_size % 16;

	if (curr_p == NULL) {
		CCCI_HISTORY_LOG(port->md_id, TAG, "start_addr <NULL>\n");
		return;
	}
	if (len == 0) {
		CCCI_HISTORY_LOG(port->md_id, TAG, "len [0]\n");
		return;
	}
	ts_nsec = local_clock();
	rem_nsec = do_div(ts_nsec, 1000000000);

	if (dir == 0)
		CCCI_HISTORY_LOG(port->md_id, TAG, "[%5lu.%06lu]C:%d,%d(%d,%d,%d) %s: %d<",
			(unsigned long)ts_nsec, rem_nsec / 1000, port->flags, port->rx_ch,
			port->rx_skb_list.qlen, port->rx_pkg_cnt, port->rx_drop_cnt, "R", len);
	else
		CCCI_HISTORY_LOG(port->md_id, TAG, "[%5lu.%06lu]C:%d,%d(%d) %s: %d>",
			(unsigned long)ts_nsec, rem_nsec / 1000, port->flags, port->tx_ch,
			port->tx_pkg_cnt, "W", len);
	/* Fix section */
	for (i = 0; i < _16_fix_num; i++) {
		CCCI_HISTORY_LOG(port->md_id, TAG, "%03X: %08X %08X %08X %08X\n",
		       i * 16, *curr_p, *(curr_p + 1), *(curr_p + 2), *(curr_p + 3));
		curr_p += 4;
	}

	/* Tail section */
	if (tail_num > 0) {
		curr_ch_p = (unsigned char *)curr_p;
		for (j = 0; j < tail_num; j++) {
			buf[j] = *curr_ch_p;
			curr_ch_p++;
		}
		for (; j < 16; j++)
			buf[j] = 0;
		curr_p = (unsigned int *)buf;
		CCCI_HISTORY_LOG(port->md_id, TAG, "%03X: %08X %08X %08X %08X\n",
		       i * 16, *curr_p, *(curr_p + 1), *(curr_p + 2), *(curr_p + 3));
	}
}

void port_ch_dump(struct ccci_port *port, int dir, void *msg_buf, int len)
{
	if (port->flags & PORT_F_DUMP_RAW_DATA)
		port_dump_raw_data(port, dir, msg_buf, len);
	else
		port_dump_string(port, dir, msg_buf, len);
}


static inline int port_adjust_skb(struct ccci_port *port, struct sk_buff *skb)
{
	struct ccci_header *ccci_h = NULL;

	ccci_h = (struct ccci_header *)skb->data;
	if (port->flags & PORT_F_USER_HEADER) { /* header provide by user */
		/* CCCI_MON_CH should fall in here, as header must be send to md_init */
		if (ccci_h->data[0] == CCCI_MAGIC_NUM) {
			if (ccci_h->channel == CCCI_MONITOR_CH)
				*(((u32 *) ccci_h) + 2) = CCCI_MONITOR_CH_ID;
			if (unlikely(skb->len > sizeof(struct ccci_header))) {
				CCCI_ERROR_LOG(port->md_id, TAG, "recv unexpected data for %s, skb->len=%d\n",
					port->name, skb->len);
				skb_trim(skb, sizeof(struct ccci_header));
			}
		}
	} else {
		/* remove CCCI header */
		skb_pull(skb, sizeof(struct ccci_header));
	}

	return 0;
}

int port_recv_skb(struct ccci_port *port, struct sk_buff *skb)
{
	unsigned long flags;
	struct ccci_header *ccci_h = (struct ccci_header *)skb->data;

	spin_lock_irqsave(&port->rx_skb_list.lock, flags);
	CCCI_DEBUG_LOG(port->md_id, TAG, "recv on %s, len=%d\n", port->name, port->rx_skb_list.qlen);
	if (port->rx_skb_list.qlen < port->rx_length_th) {
		port->flags &= ~PORT_F_RX_FULLED;
		if (port->flags & PORT_F_ADJUST_HEADER)
			port_adjust_skb(port, skb);
		if (ccci_h->channel == CCCI_STATUS_RX)
			port->skb_handler(port, skb);
		else
			__skb_queue_tail(&port->rx_skb_list, skb);
		port->rx_pkg_cnt++;
		spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
		wake_lock_timeout(&port->rx_wakelock, HZ);
		wake_up_all(&port->rx_wq);

		return 0;
	}
	port->flags |= PORT_F_RX_FULLED;
	spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
	if (port->flags & PORT_F_ALLOW_DROP) {
		CCCI_NORMAL_LOG(port->md_id, TAG, "port %s Rx full, drop packet\n", port->name);
		goto drop;
	} else
		return -CCCI_ERR_PORT_RX_FULL;

 drop:
	/* only return drop and caller do drop */
	CCCI_NORMAL_LOG(port->md_id, TAG, "drop on %s, len=%d\n", port->name, port->rx_skb_list.qlen);
	port->rx_drop_cnt++;
	return -CCCI_ERR_DROP_PACKET;
}

int port_kthread_handler(void *arg)
{
	struct ccci_port *port = arg;
	/* struct sched_param param = { .sched_priority = 1 }; */
	struct sk_buff *skb;
	unsigned long flags;
	int ret = 0;
	int md_id = port->md_id;

	CCCI_DEBUG_LOG(md_id, TAG, "port %s's thread running\n", port->name);

	while (1) {
		if (skb_queue_empty(&port->rx_skb_list)) {
			ret = wait_event_interruptible(port->rx_wq, !skb_queue_empty(&port->rx_skb_list));
			if (ret == -ERESTARTSYS)
				continue;	/* FIXME */
		}
		if (kthread_should_stop())
			break;
		CCCI_DEBUG_LOG(md_id, TAG, "read on %s\n", port->name);
		port_proxy_record_rx_sched_time(port->port_proxy, port->rx_ch);
		/* 1. dequeue */
		spin_lock_irqsave(&port->rx_skb_list.lock, flags);
		skb = __skb_dequeue(&port->rx_skb_list);
		if (port->rx_skb_list.qlen == 0)
			port_proxy_ask_more_req_to_md(port->port_proxy, port);
		spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
		/* 2. process port skb */
		if (port->skb_handler)
			port->skb_handler(port, skb);
	}
	return 0;
}

/*******************************************************************************************/
/*REGION: port_proxy class method implementation*/
/*******************************************************************************************/
static inline void port_proxy_set_critical_user(struct port_proxy *proxy_p, int user_id, int enabled)
{
	if (enabled)
		proxy_p->critical_user_active |= (1 << user_id);
	else
		proxy_p->critical_user_active &= ~(1 << user_id);
}

int port_proxy_user_register(struct port_proxy *proxy_p, struct ccci_port *port)
{
	int rx_ch = port->rx_ch;

	if (rx_ch == CCCI_FS_RX)
		port_proxy_set_critical_user(proxy_p, CRIT_USR_FS, 1);
	if (rx_ch == CCCI_UART2_RX)
		port_proxy_set_critical_user(proxy_p, CRIT_USR_MUXD, 1);
	if (rx_ch == CCCI_MD_LOG_RX)
		port_proxy_set_critical_user(proxy_p, CRIT_USR_MDLOG, 1);
	if (rx_ch == CCCI_UART1_RX)
		port_proxy_set_critical_user(proxy_p, CRIT_USR_META, 1);
	return 0;
}

int port_proxy_check_critical_user(struct port_proxy *proxy_p)
{
	int ret = 1;
	int md_id = proxy_p->md_id;

	if (port_proxy_get_critical_user(proxy_p, CRIT_USR_MUXD) == 0) {
		if (is_meta_mode() || is_advanced_meta_mode()) {
			if (port_proxy_get_critical_user(proxy_p, CRIT_USR_META) == 0) {
				CCCI_NORMAL_LOG(md_id, TAG, "ready to reset MD in META mode\n");
				ret = 0;
				goto __EXIT_FUN__;
			}
			/* this should never happen */
			CCCI_ERROR_LOG(md_id, TAG, "DHL ctrl is still open in META mode\n");
		} else {
			if (port_proxy_get_critical_user(proxy_p, CRIT_USR_MDLOG) == 0 &&
				port_proxy_get_critical_user(proxy_p, CRIT_USR_MDLOG_CTRL) == 0) {
				CCCI_NORMAL_LOG(md_id, TAG, "ready to reset MD in normal mode\n");
				ret = 0;
				goto __EXIT_FUN__;
			}
		}
	}
__EXIT_FUN__:
	return ret;
}

int port_proxy_user_unregister(struct port_proxy *proxy_p, struct ccci_port *port)
{
	int rx_ch = port->rx_ch;
	int md_id = proxy_p->md_id;

	if (rx_ch == CCCI_FS_RX)
		port_proxy_set_critical_user(proxy_p, CRIT_USR_FS, 0);
	if (rx_ch == CCCI_UART2_RX)
		port_proxy_set_critical_user(proxy_p, CRIT_USR_MUXD, 0);
	if (rx_ch == CCCI_MD_LOG_RX)
		port_proxy_set_critical_user(proxy_p, CRIT_USR_MDLOG, 0);
	if (rx_ch == CCCI_UART1_RX)
		port_proxy_set_critical_user(proxy_p, CRIT_USR_META, 0);

	CCCI_NORMAL_LOG(md_id, TAG, "critical user check: 0x%x\n", proxy_p->critical_user_active);
	ccci_event_log("md%d: critical user check: 0x%x\n", md_id, proxy_p->critical_user_active);
	return 0;
}
static inline void port_proxy_channel_mapping(struct port_proxy *proxy_p)
{
	int i;
	struct ccci_port *port = NULL;
	/* setup mapping */
	for (i = 0; i < ARRAY_SIZE(proxy_p->rx_ch_ports); i++)
		INIT_LIST_HEAD(&proxy_p->rx_ch_ports[i]);	/* clear original list */
	for (i = 0; i < proxy_p->port_number; i++)
		list_add_tail(&proxy_p->ports[i].entry, &proxy_p->rx_ch_ports[proxy_p->ports[i].rx_ch]);
	for (i = 0; i < ARRAY_SIZE(proxy_p->rx_ch_ports); i++) {
		if (!list_empty(&proxy_p->rx_ch_ports[i])) {
			list_for_each_entry(port, &proxy_p->rx_ch_ports[i], entry) {
				CCCI_DEBUG_LOG(proxy_p->md_id, TAG, "CH%d ports:%s(%d/%d)\n",
					i, port->name, port->rx_ch, port->tx_ch);
			}
		}
	}
}

struct ccci_port *port_proxy_get_port(struct port_proxy *proxy_p, int minor, CCCI_CH ch)
{
	int i;
	struct ccci_port *port;

	if (proxy_p == NULL)
		return NULL;
	for (i = 0; i < proxy_p->port_number; i++) {
		port = proxy_p->ports + i;
		if (minor >= 0 && port->minor == minor)
			return port;
		if (ch != CCCI_INVALID_CH_ID && (port->rx_ch == ch || port->tx_ch == ch))
			return port;
	}
	return NULL;
}

struct ccci_port *port_proxy_get_port_by_node(int major, int minor)
{
	struct port_proxy *proxy_p = ccci_md_get_port_proxy_by_major(major);

	return port_proxy_get_port_by_minor(proxy_p, minor);
}

static inline int port_proxy_get_port_queue_no(struct port_proxy *proxy_p, DIRECTION dir,
			struct ccci_port *port, int is_ack)
{
	int md_state = ccci_md_get_state(proxy_p->md_obj);

	if (dir == OUT) {
		if (is_ack == 1)
			return (md_state == EXCEPTION ? port->txq_exp_index : (port->txq_exp_index&0x0F));
		return (md_state == EXCEPTION ? port->txq_exp_index : port->txq_index);
	} else if (dir == IN)
		return (md_state == EXCEPTION ? port->rxq_exp_index : port->rxq_index);
	else
		return -1;
}


/*
 * kernel inject CCCI message to modem.
 */
int port_proxy_send_msg_to_md(struct port_proxy *proxy_p, CCCI_CH ch, u32 msg, u32 resv, int blocking)
{
	struct ccci_port *port = NULL;
	struct sk_buff *skb = NULL;
	struct ccci_header *ccci_h;
	int ret = 0;
	int md_state;
	int qno = -1;

	md_state = ccci_md_get_state(proxy_p->md_obj);
	if (md_state != BOOT_WAITING_FOR_HS1 && md_state != BOOT_WAITING_FOR_HS2
		&& md_state != READY && md_state != EXCEPTION)
		return -CCCI_ERR_MD_NOT_READY;
	if (ch == CCCI_SYSTEM_TX && md_state != READY)
		return -CCCI_ERR_MD_NOT_READY;
	if ((msg == CCISM_SHM_INIT || msg == CCISM_SHM_INIT_DONE ||
		msg == C2K_CCISM_SHM_INIT || msg == C2K_CCISM_SHM_INIT_DONE) &&
		md_state != READY) {
		return -CCCI_ERR_MD_NOT_READY;
	}
	if (ch == CCCI_SYSTEM_TX)
		port = proxy_p->sys_port;
	else if (ch == CCCI_CONTROL_TX)
		port = proxy_p->ctl_port;
	else
		port = port_proxy_get_port_by_channel(proxy_p, ch);
	if (port) {
		skb = ccci_alloc_skb(sizeof(struct ccci_header), port->skb_from_pool, blocking);
		if (skb) {
			ccci_h = (struct ccci_header *)skb_put(skb, sizeof(struct ccci_header));
			ccci_h->data[0] = CCCI_MAGIC_NUM;
			ccci_h->data[1] = msg;
			ccci_h->channel = ch;
			ccci_h->reserved = resv;
			qno = port_proxy_get_port_queue_no(proxy_p, OUT, port, -1);
			ret = ccci_md_send_skb(proxy_p->md_obj, qno, skb, port->skb_from_pool, blocking);
			if (ret)
				ccci_free_skb(skb);
			return ret;
		} else {
			return -CCCI_ERR_ALLOCATE_MEMORY_FAIL;
		}
	}
	return -CCCI_ERR_INVALID_LOGIC_CHANNEL_ID;
}

/*
 * if recv_request returns 0 or -CCCI_ERR_DROP_PACKET, then it's port's duty to free the request, and caller should
 * NOT reference the request any more. but if it returns other error, caller should be responsible to free the request.
 */
int port_proxy_recv_skb(struct port_proxy *proxy_p, struct sk_buff *skb)
{
	struct ccci_header *ccci_h;
	struct ccci_port *port = NULL;
	struct list_head *port_list = NULL;
	int ret = -CCCI_ERR_CHANNEL_NUM_MIS_MATCH;
	char matched = 0;
	int md_state = ccci_md_get_state(proxy_p->md_obj);

	if (likely(skb)) {
		ccci_h = (struct ccci_header *)skb->data;
	} else {
		ret = -CCCI_ERR_INVALID_PARAM;
		goto err_exit;
	}
	if (unlikely(ccci_h->channel >= CCCI_MAX_CH_NUM)) {
		ret = -CCCI_ERR_CHANNEL_NUM_MIS_MATCH;
		goto err_exit;
	}
	if (unlikely((md_state == GATED || md_state == INVALID) &&
		     ccci_h->channel != CCCI_MONITOR_CH)) {
		ret = -CCCI_ERR_HIF_NOT_POWER_ON;
		goto err_exit;
	}

	port_list = &proxy_p->rx_ch_ports[ccci_h->channel];
	list_for_each_entry(port, port_list, entry) {
		/*
		 * multi-cast is not supported, because one port may freed or modified this request
		 * before another port can process it. but we still can use req->state to achive some
		 * kind of multi-cast if needed.
		 */
		matched =
		    (port->ops->recv_match == NULL) ?
			(ccci_h->channel == port->rx_ch) : port->ops->recv_match(port, skb);
		if (matched) {
			if (likely(skb && port->ops->recv_skb)) {
				ret = port->ops->recv_skb(port, skb);
				if (ret == 0 && port->rx_ch == CCCI_FS_RX && md_state == BOOT_WAITING_FOR_HS2)
					ccci_fsm_append_event(proxy_p->md_obj, CCCI_EVENT_FS_IN, NULL, 0);
			} else {
				CCCI_ERROR_LOG(proxy_p->md_id, TAG, "port->ops->recv_request is null\n");
				ret = -CCCI_ERR_CHANNEL_NUM_MIS_MATCH;
				goto err_exit;
			}
			if (ret == -CCCI_ERR_PORT_RX_FULL)
				port->rx_busy_count++;
			break;
		}
	}

 err_exit:
	if (ret < 0 && ret != -CCCI_ERR_PORT_RX_FULL) {
		/* CCCI_ERROR_LOG(md->index, CORE, "drop on channel %d\n", ccci_h->channel); */ /* Fix me, mask temp */
		ccci_free_skb(skb);
		ret = -CCCI_ERR_DROP_PACKET;
	}

	return ret;
}
void port_proxy_md_status_notice(struct port_proxy *proxy_p, DIRECTION dir,
						int filter_ch_no, int filter_queue_idx, MD_STATE state)
{
	int i, match = 0;
	struct ccci_port *port;

	for (i = 0; i < proxy_p->port_number; i++) {
		port = proxy_p->ports + i;
		if (filter_ch_no > 0) {
			if (dir == OUT)
				match = filter_ch_no == port->tx_ch;
			else
				match = filter_ch_no == port->rx_ch;
			if (match == 0)
				continue;
		} else if (filter_queue_idx > 0) {
			if (ccci_md_get_state(proxy_p->md_obj) == EXCEPTION) {
				if (dir == OUT)
					match =	filter_queue_idx == port->txq_exp_index;
				else
					match = filter_queue_idx == port->rxq_exp_index;
			} else {
				/* consider network data/ack queue design */
				if (dir == OUT)
					match = filter_queue_idx == port->txq_index
						|| filter_queue_idx == (port->txq_exp_index & 0x0F);
				else
					match = filter_queue_idx == port->rxq_index;
			}
			if (match == 0)
				continue;
			state = (dir<<31) | (filter_queue_idx<<16) | state;
		}
		if ((state == GATED) && (port->flags & PORT_F_CH_TRAFFIC)) {
			port->rx_pkg_cnt = 0;
			port->rx_drop_cnt = 0;
			port->tx_pkg_cnt = 0;
		}
		if (port->ops->md_state_notice)
			port->ops->md_state_notice(port, state);
	}
}

void port_proxy_wake_up_tx_queue(struct port_proxy *proxy_p, unsigned char qno)
{
	ccci_md_start_queue(proxy_p->md_obj, qno, OUT);
}

/*
 * kernel inject message to user space daemon, this function may sleep
 */
int port_proxy_send_msg_to_user(struct port_proxy *proxy_p, CCCI_CH ch, CCCI_MD_MSG msg, u32 resv)
{
	struct sk_buff *skb = NULL;
	struct ccci_header *ccci_h;
	int ret = 0, count = 0;

	if (unlikely(ch != CCCI_MONITOR_CH)) {
		CCCI_ERROR_LOG(proxy_p->md_id, TAG, "invalid channel %x for sending virtual msg\n", ch);
		return -CCCI_ERR_INVALID_LOGIC_CHANNEL_ID;
	}
	if (unlikely(in_interrupt())) {
		CCCI_ERROR_LOG(proxy_p->md_id, TAG, "sending virtual msg from IRQ context %ps\n",
				 __builtin_return_address(0));
		return -CCCI_ERR_ASSERT_ERR;
	}

	skb = ccci_alloc_skb(sizeof(struct ccci_header), 1, 1);
	/* request will be recycled in char device's read function */
	ccci_h = (struct ccci_header *)skb_put(skb, sizeof(struct ccci_header));
	ccci_h->data[0] = CCCI_MAGIC_NUM;
	ccci_h->data[1] = msg;
	ccci_h->channel = ch;
#ifdef FEATURE_SEQ_CHECK_EN
	ccci_h->assert_bit = 0;
#endif
	ccci_h->reserved = resv;
retry:
	ret = port_proxy_recv_skb(proxy_p, skb);
	if (ret >= 0 || ret == -CCCI_ERR_DROP_PACKET)
		return ret;
	msleep(100);
	if (count++ < 20) {
		goto retry;
	} else {
		CCCI_ERROR_LOG(proxy_p->md_id, TAG, "fail to send virtual msg %x for %ps\n", msg,
				 __builtin_return_address(0));
		ccci_free_skb(skb);
	}
	return ret;

}

int port_proxy_send_skb_to_md(struct port_proxy *proxy_p, struct ccci_port *port, struct sk_buff *skb, int blocking)
{
	int tx_qno = 0;
	int ret = 0;
	int md_state = ccci_md_get_state(proxy_p->md_obj);

	if ((md_state == BOOT_WAITING_FOR_HS1 || md_state == BOOT_WAITING_FOR_HS2)
		&& port->tx_ch != CCCI_FS_TX && port->tx_ch != CCCI_RPC_TX) {
		CCCI_NORMAL_LOG(port->md_id, TAG, "port %s ch%d write fail when md_state=%d\n", port->name,
			     port->tx_ch, port->modem->md_state);
		return -ENODEV;
	}
	if (md_state == EXCEPTION
		&& port->tx_ch != CCCI_MD_LOG_TX
		&& port->tx_ch != CCCI_UART1_TX
	    && port->tx_ch != CCCI_FS_TX)
		return -ETXTBSY;
	if (md_state == GATED
			|| md_state == WAITING_TO_STOP
			|| md_state == INVALID)
		return -ENODEV;

	tx_qno = port_proxy_get_port_queue_no(proxy_p, OUT, port, -1);
	ret = ccci_md_send_skb(proxy_p->md_obj, tx_qno, skb, port->skb_from_pool, blocking);
	if (ret == 0) {
		port->tx_pkg_cnt++;
		/*Check FS still under working, no need time out, so resched bootup timer*/
		if (port->tx_ch == CCCI_FS_TX && md_state == BOOT_WAITING_FOR_HS2)
			ccci_fsm_append_event(proxy_p->md_obj, CCCI_EVENT_FS_OUT, NULL, 0);
	}
	return ret;
}

int port_proxy_napi_poll(struct port_proxy *proxy_p, struct ccci_port *port,
				struct napi_struct *napi, int weight)
{
	int rx_qno = 0;

	if (ccci_md_get_state(proxy_p->md_obj) != READY)
		return -ENODEV;

	rx_qno = port_proxy_get_port_queue_no(proxy_p, IN, port, -1);
	return ccci_md_napi_poll(proxy_p->md_obj, rx_qno, napi, weight);
}

int port_proxy_net_send_skb_to_md(struct port_proxy *proxy_p, struct ccci_port *port,
			int is_ack, struct sk_buff *skb)
{
	int tx_qno = 0;

	if (ccci_md_get_state(proxy_p->md_obj) != READY)
		return -ENODEV;
	tx_qno = port_proxy_get_port_queue_no(proxy_p, OUT, port, is_ack);
	return ccci_md_send_skb(proxy_p->md_obj, tx_qno, skb, port->skb_from_pool, 0);
}

int port_proxy_send_ccb_tx_notify_to_md(struct port_proxy *proxy_p, int core_id)
{
	return ccci_md_send_ccb_tx_notify(proxy_p->md_obj, core_id);
}

int port_proxy_ask_more_req_to_md(struct port_proxy *proxy_p, struct ccci_port *port)
{
	int ret = -1;
	int rx_qno = port_proxy_get_port_queue_no(port->port_proxy, IN, port, -1);

	if (port->flags & PORT_F_RX_FULLED)
		ret = ccci_md_ask_more_request(port->modem, rx_qno);
	return ret;
}
int port_proxy_write_room_to_md(struct port_proxy *proxy_p, struct ccci_port *port)
{
	int ret = -1;
	int tx_qno = port_proxy_get_port_queue_no(port->port_proxy, OUT, port, -1);

	if (port->flags & PORT_F_RX_FULLED)
		ret = ccci_md_write_room(port->modem, tx_qno);
	return ret;
}


void port_proxy_dump_status(struct port_proxy *proxy_p)
{
	struct ccci_port *port;
	unsigned long long port_full = 0;	/* hardcode, port number should not be larger than 64 */
	unsigned int i;

	for (i = 0; i < proxy_p->port_number; i++) {
		port = proxy_p->ports + i;
		if (port->flags & PORT_F_RX_FULLED)
			port_full |= (1 << i);
		if (port->tx_busy_count != 0 || port->rx_busy_count != 0) {
			CCCI_REPEAT_LOG(proxy_p->md_id, TAG, "port %s busy count %d/%d\n", port->name,
				     port->tx_busy_count, port->rx_busy_count);
			port->tx_busy_count = 0;
			port->rx_busy_count = 0;
		}
		if (port->ops->dump_info)
			port->ops->dump_info(port, 0);
	}
	if (port_full)
		CCCI_ERROR_LOG(proxy_p->md_id, TAG, "port_full status=%llx\n", port_full);
}

static inline int port_proxy_register_char_dev(struct port_proxy *proxy_p)
{
	int ret = 0;
	dev_t dev = 0;

	if (proxy_p->major) {
		dev = MKDEV(proxy_p->major, proxy_p->minor_base);
		ret = register_chrdev_region(dev, 120, CCCI_DEV_NAME);
	} else {
		ret = alloc_chrdev_region(&dev, proxy_p->minor_base, 120, CCCI_DEV_NAME);
		if (ret)
			CCCI_ERROR_LOG(proxy_p->md_id, CHAR, "alloc_chrdev_region fail,ret=%d\n", ret);
		proxy_p->major = MAJOR(dev);
	}
	return ret;
}

static inline void port_proxy_init_all_ports(struct port_proxy *proxy_p)
{
	int i;
	int md_id;
	struct ccci_port *port;

	md_id = proxy_p->md_id;
	for (i = 0; i < ARRAY_SIZE(proxy_p->rx_ch_ports); i++)
		INIT_LIST_HEAD(&proxy_p->rx_ch_ports[i]);

	/* init port */
	for (i = 0; i < proxy_p->port_number; i++) {
		port = proxy_p->ports + i;
		port_struct_init(port, proxy_p);
		if (port->tx_ch == CCCI_SYSTEM_TX)
			proxy_p->sys_port = port;
		if (port->tx_ch == CCCI_CONTROL_TX)
			proxy_p->ctl_port = port;
		port->major = proxy_p->major;
		port->minor_base = proxy_p->minor_base;
		if (port->ops->init)
			port->ops->init(port);
		if ((port->flags & PORT_F_RX_EXCLUSIVE) && (proxy_p->md_capability & MODEM_CAP_NAPI) &&
		    ((1 << port->rxq_index) & proxy_p->napi_queue_mask) && port->rxq_index != 0xFF &&
		    proxy_p->napi_port[port->rxq_index] == NULL) {
			proxy_p->napi_port[port->rxq_index] = port;
			CCCI_DEBUG_LOG(md_id, TAG, "queue%d add NAPI port %s\n", port->rxq_index, port->name);
		}
		/*
		 * be careful, port->rxq_index may be 0xFF (1<<port->rxq_index may fly away)
		 * one queue should have only one NAPI port, firt come first serve. you can also use
		 * PORT_F_RX_EXCLUSIVE bit in ccci_port[] to control the setting.
		 */
	}
	port_proxy_channel_mapping(proxy_p);
}

struct port_proxy *port_proxy_alloc(int md_id, int md_capability, int napi_queue_mask, void *md)
{
	int ret = 0;
	struct port_proxy *proxy_p;
	struct ccci_smem_layout *smem_layout;

	/* Allocate port_proxy obj and set all member zero */
	proxy_p = kzalloc(sizeof(struct port_proxy), GFP_KERNEL);
	if (proxy_p == NULL) {
		CCCI_ERROR_LOG(-1, TAG, "%s:alloc port_proxy fail\n", __func__);
		return NULL;
	}
	proxy_p->md_id = md_id;
	proxy_p->md_capability = md_capability;
	proxy_p->md_obj = md;
	proxy_p->napi_queue_mask = napi_queue_mask;
	proxy_p->sim_type = 0xEEEEEEEE;	/* sim_type(MCC/MNC) sent by MD wouldn't be 0xEEEEEEEE */
	snprintf(proxy_p->wakelock_name, sizeof(proxy_p->wakelock_name), "md%d_wakelock", md_id + 1);
	wake_lock_init(&proxy_p->wakelock, WAKE_LOCK_SUSPEND, proxy_p->wakelock_name);

	/* config port smem layout*/
	smem_layout = ccci_md_get_smem(md);
	port_smem_cfg(md_id, smem_layout);

	ret = port_proxy_register_char_dev(proxy_p);
	if (ret)
		goto EXIT_FUN;
	proxy_p->port_number = get_md_port_cfg(proxy_p->md_id, &proxy_p->ports);
	if (proxy_p->port_number > 0 && proxy_p->ports)
		port_proxy_init_all_ports(proxy_p);
	else
		ret = -1;

EXIT_FUN:
	if (ret) {
		kfree(proxy_p);
		proxy_p = NULL;
		CCCI_ERROR_LOG(-1, TAG, "%s:get md port config fail,ret=%d\n", __func__, ret);
	}
	return proxy_p;
};
void port_proxy_free(struct port_proxy *proxy_p)
{
	kfree(proxy_p);
}

#ifdef CONFIG_MTK_ECCCI_C2K
int modem_dtr_set(int on, int low_latency)
{
	struct port_proxy *proxy_p;
	struct c2k_ctrl_port_msg c2k_ctl_msg;
	int ret = 0;

	/* only md3 can usb bypass */
	proxy_p = port_proxy_get_by_md_id(MD_SYS3);

	c2k_ctl_msg.chan_num = DATA_PPP_CH_C2K;
	c2k_ctl_msg.id_hi = (C2K_STATUS_IND_MSG & 0xFF00) >> 8;
	c2k_ctl_msg.id_low = C2K_STATUS_IND_MSG & 0xFF;
	c2k_ctl_msg.option = 0;
	if (on)
		c2k_ctl_msg.option |= 0x04;
	else
		c2k_ctl_msg.option &= 0xFB;

	CCCI_NORMAL_LOG(proxy_p->md_id, KERN, "usb bypass dtr set(%d)(0x%x)\n", on, (u32) (*((u32 *)&c2k_ctl_msg)));
	port_proxy_send_msg_to_md(proxy_p, CCCI_CONTROL_TX, C2K_STATUS_IND_MSG, (u32) (*((u32 *)&c2k_ctl_msg)), 1);

	return ret;
}

int modem_dcd_state(void)
{
	struct port_proxy *proxy_p;
	struct c2k_ctrl_port_msg c2k_ctl_msg;
	int dcd_state = 0;
	int ret = 0;

	/* only md3 can usb bypass */
	proxy_p = port_proxy_get_by_md_id(MD_SYS3);

	c2k_ctl_msg.chan_num = DATA_PPP_CH_C2K;
	c2k_ctl_msg.id_hi = (C2K_STATUS_QUERY_MSG & 0xFF00) >> 8;
	c2k_ctl_msg.id_low = C2K_STATUS_QUERY_MSG & 0xFF;
	c2k_ctl_msg.option = 0;

	CCCI_NORMAL_LOG(proxy_p->md_id, KERN, "usb bypass query state(0x%x)\n", (u32) (*((u32 *)&c2k_ctl_msg)));
	ret = port_proxy_send_msg_to_md(proxy_p, CCCI_CONTROL_TX, C2K_STATUS_QUERY_MSG,
				(u32) (*((u32 *)&c2k_ctl_msg)), 1);
	if (ret == -CCCI_ERR_MD_NOT_READY)
		dcd_state = 0;
	else {
		msleep(20);
		dcd_state = proxy_p->dtr_state;
	}
	return dcd_state;
}

int ccci_c2k_rawbulk_intercept(int ch_id, unsigned int interception)
{
	int ret = 0;
	struct port_proxy *proxy_p;
	struct ccci_port *port = NULL;
	struct list_head *port_list = NULL;
	char matched = 0;
	int ch_id_rx = 0;

	/* USB bypass's channel id offset, please refer to viatel_rawbulk.h */
	if (ch_id >= FS_CH_C2K)
		ch_id += 2;
	else
		ch_id += 1;

	/*only data and log channel are legal*/
	if (ch_id == DATA_PPP_CH_C2K) {
		ch_id = CCCI_C2K_PPP_DATA;
		ch_id_rx = CCCI_C2K_PPP_DATA;
	} else if (ch_id == MDLOG_CH_C2K) {
		ch_id = CCCI_MD_LOG_TX;
		ch_id_rx = CCCI_MD_LOG_RX;
	} else {
		ret = -ENODEV;
		CCCI_ERROR_LOG(MD_SYS3, CHAR, "Err: wrong ch_id(%d) from usb bypass\n", ch_id);
		return ret;
	}

	/* only md3 can usb bypass */
	proxy_p = port_proxy_get_by_md_id(MD_SYS3);

	/*use rx channel to find port*/
	port_list = &proxy_p->rx_ch_ports[ch_id_rx];
	list_for_each_entry(port, port_list, entry) {
		matched = (ch_id == port->tx_ch);
		if (matched) {
			port->interception = !!interception;
			if (port->interception)
				atomic_inc(&port->usage_cnt);
			else
				atomic_dec(&port->usage_cnt);
			if (ch_id == CCCI_C2K_PPP_DATA)
				ccci_md_set_usb_data_bypass(proxy_p->md_obj, !!interception);
			ret = 0;
			CCCI_NORMAL_LOG(proxy_p->md_id, CHAR, "port(%s) ch(%d) interception(%d) set\n",
				port->name, ch_id, interception);
		}
	}
	if (!matched) {
		ret = -ENODEV;
		CCCI_ERROR_LOG(proxy_p->md_id, CHAR, "Err: no port found when setting interception(%d,%d)\n",
			ch_id, interception);
	}

	return ret;
}


int ccci_c2k_buffer_push(int ch_id, void *buf, int count)
{
	int ret = 0;
	struct port_proxy *proxy_p;
	struct ccci_port *port = NULL;
	struct list_head *port_list = NULL;
	struct sk_buff *skb = NULL;
	struct ccci_header *ccci_h = NULL;
	char matched = 0;
	size_t actual_count = 0;
	int ch_id_rx = 0;
	unsigned char blk1 = 0;	/* usb will call this routine in ISR, so we cannot schedule */
	unsigned char blk2 = 0;	/* non-blocking for all request from USB */

	/* USB bypass's channel id offset, please refer to viatel_rawbulk.h */
	if (ch_id >= FS_CH_C2K)
		ch_id += 2;
	else
		ch_id += 1;

	/* only data and log channel are legal */
	if (ch_id == DATA_PPP_CH_C2K) {
		ch_id = CCCI_C2K_PPP_DATA;
		ch_id_rx = CCCI_C2K_PPP_DATA;
	} else if (ch_id == MDLOG_CH_C2K) {
		ch_id = CCCI_MD_LOG_TX;
		ch_id_rx = CCCI_MD_LOG_RX;
	} else {
		ret = -ENODEV;
		CCCI_ERROR_LOG(MD_SYS3, CHAR, "Err: wrong ch_id(%d) from usb bypass\n", ch_id);
		return ret;
	}

	/* only md3 can usb bypass */
	proxy_p = port_proxy_get_by_md_id(MD_SYS3);

	CCCI_NORMAL_LOG(MD_SYS3, CHAR, "data from usb bypass (ch%d)(%d)\n", ch_id, count);

	actual_count = count > CCCI_MTU ? CCCI_MTU : count;

	port_list = &proxy_p->rx_ch_ports[ch_id_rx];
	list_for_each_entry(port, port_list, entry) {
		matched = (ch_id == port->tx_ch);
		if (matched) {
			skb = ccci_alloc_skb(actual_count, port->skb_from_pool, blk1);
			if (skb) {
				ccci_h = (struct ccci_header *)skb_put(skb, sizeof(struct ccci_header));
				ccci_h->data[0] = 0;
				ccci_h->data[1] = actual_count + sizeof(struct ccci_header);
				ccci_h->channel = port->tx_ch;
				ccci_h->reserved = 0;

				memcpy(skb_put(skb, actual_count), buf, actual_count);

				/*
				 * for md3, ccci_h->channel will probably change after call send_skb,
				 * because md3's channel mapping.
				 * do NOT reference request after called this,
				 * modem may have freed it, unless you get -EBUSY
				 */
				ret = port_proxy_send_skb_to_md(proxy_p, port, skb, blk2);

				if (ret) {
					if (ret == -EBUSY && !blk2)
						ret = -EAGAIN;
					goto push_err_out;
				}
				return actual_count;
push_err_out:
				ccci_free_skb(skb);
				return ret;
			}
			/* consider this case as non-blocking */
			return -ENOMEM;
		}
	}
	return -ENODEV;
}

#endif

void port_proxy_set_md_dsp_protection(struct port_proxy *proxy_p, int is_loaded)
{
	ccci_md_set_dsp_protection(proxy_p->md_obj, is_loaded);
}

int port_proxy_get_md_state(struct port_proxy *proxy_p)
{
	return ccci_md_get_state(proxy_p->md_obj);
}

char *port_proxy_get_md_img_post_fix(struct port_proxy *proxy_p)
{
	return ccci_md_get_post_fix(proxy_p->md_obj);
}

struct port_proxy *port_proxy_get_by_md_id(int md_id)
{
	return ccci_md_get_port_proxy_by_id(md_id);
}

int port_proxy_stop_md(struct port_proxy *proxy_p, unsigned int stop_type)
{
	int ret = 0;

	proxy_p->sim_type = 0xEEEEEEEE; /* reset sim_type(MCC/MNC) to 0xEEEEEEEE */
	ret = ccci_fsm_append_command(proxy_p->md_obj, CCCI_COMMAND_STOP, CCCI_CMD_FLAG_WAIT_FOR_COMPLETE |
		(stop_type == MD_FLIGHT_MODE_ENTER ? CCCI_CMD_FLAG_FLIGHT_MODE : 0));
	return ret;
}

int port_proxy_start_md(struct port_proxy *proxy_p)
{
	int ret = 0;

	proxy_p->mdlog_dump_done = 0;
	ret = ccci_fsm_append_command(proxy_p->md_obj, CCCI_COMMAND_START, CCCI_CMD_FLAG_WAIT_FOR_COMPLETE);
	return ret;
}

static int port_proxy_send_runtime_data_to_md(struct port_proxy *proxy_p)
{
	int qno = port_proxy_get_port_queue_no(proxy_p, OUT, proxy_p->ctl_port, -1);

	return ccci_md_send_runtime_data(proxy_p->md_obj, CCCI_CONTROL_TX, qno, proxy_p->ctl_port->skb_from_pool);
}

static void port_proxy_set_traffic_flag(struct port_proxy *proxy_p, unsigned int dump_flag)
{
	int idx;
	struct ccci_port *port;

	proxy_p->traffic_dump_flag = dump_flag;
	CCCI_NORMAL_LOG(proxy_p->md_id, TAG,
			 "%s: 0x%x\n", __func__, proxy_p->traffic_dump_flag);
	for (idx = 0; idx < proxy_p->port_number; idx++) {
		port = proxy_p->ports + idx;
		/*clear traffic & dump flag*/
		port->flags &= ~(PORT_F_CH_TRAFFIC | PORT_F_DUMP_RAW_DATA);

		/*set RILD related port*/
		if (proxy_p->traffic_dump_flag & (1 << PORT_DBG_DUMP_RILD)) {
			if (port->rx_ch == CCCI_UART2_RX ||
				port->rx_ch == CCCI_C2K_AT ||
				port->rx_ch == CCCI_C2K_AT2 ||
				port->rx_ch == CCCI_C2K_AT3 ||
				port->rx_ch == CCCI_C2K_AT4 ||
				port->rx_ch == CCCI_C2K_AT5 ||
				port->rx_ch == CCCI_C2K_AT6 ||
				port->rx_ch == CCCI_C2K_AT7 ||
				port->rx_ch == CCCI_C2K_AT8)
				port->flags |= PORT_F_CH_TRAFFIC;
		}
		/*set AUDIO related port*/
		if (proxy_p->traffic_dump_flag & (1 << PORT_DBG_DUMP_AUDIO)) {
			if (port->rx_ch == CCCI_PCM_RX)
				port->flags |= (PORT_F_CH_TRAFFIC | PORT_F_DUMP_RAW_DATA);
		}
		/*set IMS related port*/
		if (proxy_p->traffic_dump_flag & (1 << PORT_DBG_DUMP_IMS)) {
			if (port->rx_ch == CCCI_IMSV_DL ||
				port->rx_ch == CCCI_IMSC_DL ||
				port->rx_ch == CCCI_IMSA_DL ||
				port->rx_ch == CCCI_IMSA_DL ||
				port->rx_ch == CCCI_IMSEM_DL)
				port->flags |= (PORT_F_CH_TRAFFIC | PORT_F_DUMP_RAW_DATA);
		}
	}
}

long port_proxy_user_ioctl(struct port_proxy *proxy_p, int ch, unsigned int cmd, unsigned long arg)
{
	long state_for_user, ret = 0;
	struct ccci_setting *ccci_setting;
	unsigned int sim_mode, sim_switch_type, enable_sim_type, sim_id, bat_info;
	unsigned int traffic_control = 0;
	unsigned int sim_slot_cfg[4];
	struct siginfo sig_info;
	unsigned int sig_pid;
	unsigned int md_boot_data[16] = { 0 };
	int md_type = 0;
	long other_md_state_for_user = 0;
	int md_id = proxy_p->md_id;
	char md_protol[] = "DHL";
	unsigned int sig = 0;
	unsigned int pid = 0;
#ifdef CONFIG_MTK_SIM_LOCK_POWER_ON_WRITE_PROTECT
	unsigned int val;
	char magic_pattern[64];
#endif

	switch (cmd) {
	case CCCI_IOC_GET_MD_PROTOCOL_TYPE:
		CCCI_ERROR_LOG(md_id, CHAR, "Call CCCI_IOC_GET_MD_PROTOCOL_TYPE!\n");
		if (copy_to_user((void __user *)arg, md_protol, sizeof(md_protol))) {
			CCCI_ERROR_LOG(md_id, CHAR, "copy_to_user MD_PROTOCOL failed !!\n");
			return -EFAULT;
		}
		break;
	case CCCI_IOC_GET_MD_STATE:
		state_for_user = ccci_md_get_state_for_user(proxy_p->md_obj);
		if (state_for_user >= 0) {
			ret = put_user((unsigned int)state_for_user, (unsigned int __user *)arg);
		} else {
			CCCI_ERROR_LOG(md_id, CHAR, "Get MD state fail: %ld\n", state_for_user);
			ret = state_for_user;
		}
		break;
	case CCCI_IOC_GET_OTHER_MD_STATE:
		CCCI_NOTICE_LOG(md_id, CHAR, "Get other md state ioctl(%d) called by %s\n", ch, current->comm);
		if (md_id == MD_SYS1)
			other_md_state_for_user = exec_ccci_kern_func_by_md_id(MD_SYS3, ID_GET_MD_STATE, NULL, 0);
		else if (md_id == MD_SYS3)
			other_md_state_for_user = exec_ccci_kern_func_by_md_id(MD_SYS1, ID_GET_MD_STATE, NULL, 0);

		if (other_md_state_for_user == CCCI_ERR_MD_INDEX_NOT_FOUND)
			other_md_state_for_user = MD_STATE_INVALID;

		if (other_md_state_for_user >= 0) {
			CCCI_NORMAL_LOG(md_id, CHAR, "Other MD state %ld\n", other_md_state_for_user);
			ret = put_user((unsigned int)other_md_state_for_user, (unsigned int __user *)arg);
		} else {
			CCCI_ERROR_LOG(md_id, CHAR, "Get Other MD state fail: %ld\n", other_md_state_for_user);
			ret = other_md_state_for_user;
		}
		break;
	case CCCI_IOC_MD_RESET:
		CCCI_NORMAL_LOG(md_id, CHAR, "MD reset ioctl called by (%d)%s\n", ch, current->comm);
		ccci_event_log("md%d: MD reset ioctl called by (%d)%s\n", md_id, ch, current->comm);
		ret = port_proxy_send_msg_to_user(proxy_p, CCCI_MONITOR_CH, CCCI_MD_MSG_RESET_REQUEST, 0);
#ifdef CONFIG_MTK_ECCCI_C2K
		if (md_id == MD_SYS1)
			exec_ccci_kern_func_by_md_id(MD_SYS3, ID_RESET_MD, NULL, 0);
		else if (md_id == MD_SYS3)
			exec_ccci_kern_func_by_md_id(MD_SYS1, ID_RESET_MD, NULL, 0);
#else
#if defined(CONFIG_MTK_MD3_SUPPORT) && (CONFIG_MTK_MD3_SUPPORT > 0)
		if (md_id == MD_SYS1 && ccci_get_opt_val("opt_c2k_lte_mode") == 1)
			c2k_reset_modem();
#endif
#endif
		break;
	case CCCI_IOC_FORCE_MD_ASSERT:
		CCCI_NORMAL_LOG(md_id, CHAR, "Force MD assert ioctl called by (%d)%s\n", ch, current->comm);
		ccci_event_log("md%d: Force MD assert ioctl called by (%d)%s\n", md_id, ch, current->comm);
		ret = ccci_md_force_assert(proxy_p->md_obj, MD_FORCE_ASSERT_BY_USER_TRIGGER, NULL, 0);
		break;
	case CCCI_IOC_SEND_RUN_TIME_DATA:
		if (ch == CCCI_MONITOR_CH) {
			ret = port_proxy_send_runtime_data_to_md(proxy_p);
		} else {
			CCCI_NORMAL_LOG(md_id, CHAR, "Set runtime by invalid user(%u) called by %s\n", ch,
				     current->comm);
			ret = -1;
		}
		break;
	case CCCI_IOC_GET_MD_INFO:
		md_type = ccci_md_get_img_version(proxy_p->md_obj);
		ret = put_user((unsigned int)md_type, (unsigned int __user *)arg);
		break;
	case CCCI_IOC_GET_MD_EX_TYPE:
		md_type = ccci_md_get_ex_type(proxy_p->md_obj);
		ret = put_user((unsigned int)md_type, (unsigned int __user *)arg);
		CCCI_NORMAL_LOG(md_id, CHAR, "get modem exception type=%d ret=%ld\n", md_type, ret);
		break;
	case CCCI_IOC_SET_BOOT_DATA:
			CCCI_NORMAL_LOG(md_id, CHAR, "set MD boot env data called by %s\n",
					 current->comm);
			ccci_event_log("md%d: set MD boot env data called by %s\n", md_id,
					 current->comm);
			if (copy_from_user
				(&md_boot_data, (void __user *)arg, sizeof(md_boot_data))) {
				CCCI_NORMAL_LOG(md_id, CHAR,
					 "CCCI_IOC_SET_BOOT_DATA: copy_from_user fail!\n");
				ret = -EFAULT;
			} else {
				if (md_boot_data[MD_CFG_DUMP_FLAG] != MD_DBG_DUMP_INVALID &&
					(md_boot_data[MD_CFG_DUMP_FLAG] & MD_DBG_DUMP_PORT)) {
					/*port traffic use 0x6000_000x as port dump flag*/
					port_proxy_set_traffic_flag(proxy_p, md_boot_data[MD_CFG_DUMP_FLAG]);
					md_boot_data[MD_CFG_DUMP_FLAG] =  MD_DBG_DUMP_INVALID;
				}
				ret = ccci_md_set_boot_data(proxy_p->md_obj, md_boot_data, ARRAY_SIZE(md_boot_data));
				if (ret < 0) {
					CCCI_NORMAL_LOG(md_id, CHAR,
					"ccci_set_md_boot_data return fail!\n");
					ret = -EFAULT;
				}
			}
			break;
	case CCCI_IOC_SEND_STOP_MD_REQUEST:
		CCCI_NORMAL_LOG(md_id, CHAR, "stop MD request ioctl called by %s\n", current->comm);
		ccci_event_log("md%d: stop MD request ioctl called by %s\n", md_id, current->comm);
		ret = port_proxy_send_msg_to_user(proxy_p, CCCI_MONITOR_CH, CCCI_MD_MSG_FORCE_STOP_REQUEST, 0);
#ifdef CONFIG_MTK_ECCCI_C2K
		if (md_id == MD_SYS1)
			exec_ccci_kern_func_by_md_id(MD_SYS3, ID_STOP_MD, NULL, 0);
		else if (md_id == MD_SYS3)
			exec_ccci_kern_func_by_md_id(MD_SYS1, ID_STOP_MD, NULL, 0);
#else
#if defined(CONFIG_MTK_MD3_SUPPORT) && (CONFIG_MTK_MD3_SUPPORT > 0)
		if (md_id == MD_SYS1 && ccci_get_opt_val("opt_c2k_lte_mode") == 1)
			c2k_reset_modem();
#endif
#endif
		break;
	case CCCI_IOC_SEND_START_MD_REQUEST:
		CCCI_NORMAL_LOG(md_id, CHAR, "start MD request ioctl called by %s\n", current->comm);
		ccci_event_log("md%d: start MD request ioctl called by %s\n", md_id, current->comm);
		ret = port_proxy_send_msg_to_user(proxy_p, CCCI_MONITOR_CH, CCCI_MD_MSG_FORCE_START_REQUEST, 0);
#ifdef CONFIG_MTK_ECCCI_C2K
		if (md_id == MD_SYS1)
			exec_ccci_kern_func_by_md_id(MD_SYS3, ID_START_MD, NULL, 0);
		else if (md_id == MD_SYS3)
			exec_ccci_kern_func_by_md_id(MD_SYS1, ID_START_MD, NULL, 0);
#endif
		break;
	case CCCI_IOC_DO_START_MD:
		CCCI_NORMAL_LOG(md_id, CHAR, "start MD ioctl called by %s\n", current->comm);
		ccci_event_log("md%d: start MD ioctl called by %s\n", md_id, current->comm);
		ret = port_proxy_start_md(proxy_p);
		break;
	case CCCI_IOC_DO_STOP_MD:
		if (copy_from_user(&sim_mode, (void __user *)arg, sizeof(unsigned int))) {
			CCCI_BOOTUP_LOG(md_id, CHAR, "CCCI_IOC_DO_STOP_MD: copy_from_user fail!\n");
			ret = -EFAULT;
		} else {
			CCCI_NORMAL_LOG(md_id, CHAR, "stop MD ioctl called by %s %d\n", current->comm, sim_mode);
			ccci_event_log("md%d: stop MD ioctl called by %s %d\n", md_id, current->comm, sim_mode);
			ret = port_proxy_stop_md(proxy_p, sim_mode ? MD_FLIGHT_MODE_ENTER : MD_FLIGHT_MODE_NONE);
		}
		break;
	case CCCI_IOC_ENTER_DEEP_FLIGHT:
		CCCI_NOTICE_LOG(md_id, CHAR, "enter MD flight mode ioctl called by %s\n", current->comm);
		ccci_event_log("md%d: enter MD flight mode ioctl called by %s\n", md_id, current->comm);
		ret = port_proxy_send_msg_to_user(proxy_p, CCCI_MONITOR_CH, CCCI_MD_MSG_FLIGHT_STOP_REQUEST, 0);
		break;
	case CCCI_IOC_LEAVE_DEEP_FLIGHT:
		CCCI_NOTICE_LOG(md_id, CHAR, "leave MD flight mode ioctl called by %s\n", current->comm);
		ccci_event_log("md%d: leave MD flight mode ioctl called by %s\n", md_id, current->comm);
		port_proxy_start_wake_lock(proxy_p, 10);
		ret = port_proxy_send_msg_to_user(proxy_p, CCCI_MONITOR_CH, CCCI_MD_MSG_FLIGHT_START_REQUEST, 0);
		break;
	case CCCI_IOC_SIM_SWITCH:
		if (copy_from_user(&sim_mode, (void __user *)arg, sizeof(unsigned int))) {
			CCCI_BOOTUP_LOG(md_id, CHAR, "IOC_SIM_SWITCH: copy_from_user fail!\n");
			ret = -EFAULT;
		} else {
			switch_sim_mode(md_id, (char *)&sim_mode, sizeof(sim_mode));
			CCCI_BOOTUP_LOG(md_id, CHAR, "IOC_SIM_SWITCH(%x): %ld\n", sim_mode, ret);
		}
		break;
	case CCCI_IOC_SIM_SWITCH_TYPE:
		sim_switch_type = get_sim_switch_type();
		CCCI_BOOTUP_LOG(md_id, KERN, "CCCI_IOC_SIM_SWITCH_TYPE:sim type(0x%x)\n", sim_switch_type);
		ret = put_user(sim_switch_type, (unsigned int __user *)arg);
		break;
	case CCCI_IOC_GET_SIM_TYPE:
		if (proxy_p->sim_type == 0xEEEEEEEE)
			CCCI_BOOTUP_LOG(md_id, KERN, "md has not send sim type yet(0x%x)", proxy_p->sim_type);
		else
			CCCI_BOOTUP_LOG(md_id, KERN, "md has send sim type(0x%x)", proxy_p->sim_type);
		ret = put_user(proxy_p->sim_type, (unsigned int __user *)arg);
		break;
	case CCCI_IOC_ENABLE_GET_SIM_TYPE:
		if (copy_from_user(&enable_sim_type, (void __user *)arg, sizeof(unsigned int))) {
			CCCI_NORMAL_LOG(md_id, CHAR, "CCCI_IOC_ENABLE_GET_SIM_TYPE: copy_from_user fail!\n");
			ret = -EFAULT;
		} else {
			CCCI_NORMAL_LOG(md_id, KERN, "CCCI_IOC_ENABLE_GET_SIM_TYPE: sim type(0x%x)",
								enable_sim_type);
			ret = port_proxy_send_msg_to_md(proxy_p, CCCI_SYSTEM_TX, MD_SIM_TYPE, enable_sim_type, 1);
		}
		break;
	case CCCI_IOC_SEND_BATTERY_INFO:
		bat_info = (unsigned int)BAT_Get_Battery_Voltage(0);
		CCCI_NORMAL_LOG(md_id, CHAR, "get bat voltage %d\n", bat_info);
		ret = port_proxy_send_msg_to_md(proxy_p, CCCI_SYSTEM_TX, MD_GET_BATTERY_INFO, bat_info, 1);
		break;
	case CCCI_IOC_RELOAD_MD_TYPE:
		md_type = 0;
		if (copy_from_user(&md_type, (void __user *)arg, sizeof(unsigned int))) {
			CCCI_NORMAL_LOG(md_id, CHAR, "IOC_RELOAD_MD_TYPE: copy_from_user fail!\n");
			ret = -EFAULT;
		} else {
			CCCI_NORMAL_LOG(md_id, CHAR, "IOC_RELOAD_MD_TYPE: storing md type(%d)!\n", md_type);
			ccci_event_log("md%d: IOC_RELOAD_MD_TYPE: storing md type(%d)!\n", md_id, md_type);
			ccci_md_set_reload_type(proxy_p->md_obj, md_type);
		}
		break;
	case CCCI_IOC_SET_MD_IMG_EXIST:
		if (copy_from_user
		    (&proxy_p->md_img_exist, (void __user *)arg, sizeof(proxy_p->md_img_exist))) {
			CCCI_BOOTUP_LOG(md_id, CHAR,
				     "CCCI_IOC_SET_MD_IMG_EXIST: copy_from_user fail!\n");
			ret = -EFAULT;
		}
		proxy_p->md_img_type_is_set = 1;
		CCCI_BOOTUP_LOG(md_id, CHAR,
			"CCCI_IOC_SET_MD_IMG_EXIST: set done!\n");
		break;

	case CCCI_IOC_GET_MD_IMG_EXIST:
		md_type = get_md_type_from_lk(md_id); /* For LK load modem use */
		if (md_type) {
			memset(&proxy_p->md_img_exist, 0, sizeof(proxy_p->md_img_exist));
			proxy_p->md_img_exist[0] = md_type;
			CCCI_BOOTUP_LOG(md_id, CHAR, "lk md_type: %d, image num:1\n", md_type);
		} else {
			CCCI_BOOTUP_LOG(md_id, CHAR,
				"CCCI_IOC_GET_MD_IMG_EXIST: waiting set\n");
			while (proxy_p->md_img_type_is_set == 0)
				msleep(200);
		}
		CCCI_BOOTUP_LOG(md_id, CHAR,
			"CCCI_IOC_GET_MD_IMG_EXIST: waiting set done!\n");
		if (copy_to_user((void __user *)arg, &proxy_p->md_img_exist, sizeof(proxy_p->md_img_exist))) {
			CCCI_BOOTUP_LOG(md_id, CHAR,
				     "CCCI_IOC_GET_MD_IMG_EXIST: copy_to_user fail!\n");
			ret = -EFAULT;
		}
		break;
	case CCCI_IOC_GET_MD_TYPE:
		md_type = ccci_md_get_load_type(proxy_p->md_obj);
		ret = put_user((unsigned int)md_type, (unsigned int __user *)arg);
		break;
	case CCCI_IOC_STORE_MD_TYPE:
		if (copy_from_user(&md_type, (void __user *)arg, sizeof(unsigned int))) {
			CCCI_BOOTUP_LOG(md_id, CHAR, "store md type fail: copy_from_user fail!\n");
			ret = -EFAULT;
			break;
		}
		ret = ccci_md_store_load_type(proxy_p->md_obj, md_type);
		/* Notify md_init daemon to store md type in nvram */
		if (ret == 0)
			port_proxy_send_msg_to_user(proxy_p, CCCI_MONITOR_CH, CCCI_MD_MSG_STORE_NVRAM_MD_TYPE, 0);
		break;
	case CCCI_IOC_GET_MD_TYPE_SAVING:
		md_type = ccci_md_get_load_saving_type(proxy_p->md_obj);
		ret = put_user(md_type, (unsigned int __user *)arg);
		break;
	case CCCI_IOC_GET_RAT_STR:
		ret = ccci_get_rat_str_from_drv(proxy_p->md_id, (char *)md_boot_data, sizeof(md_boot_data));
		if (ret < 0)
			CCCI_NORMAL_LOG(md_id, CHAR, "get md rat sting fail: gen str fail! %d\n", (int)ret);
		else {
			if (copy_to_user((void __user *)arg, (char *)md_boot_data,
						strlen((char *)md_boot_data) + 1)) {
				CCCI_NORMAL_LOG(md_id, CHAR, "get md rat sting fail: copy_from_user fail!\n");
				ret = -EFAULT;
			}
		}
		break;
	case CCCI_IOC_SET_RAT_STR:
		if (copy_from_user((char *)md_boot_data, (void __user *)arg, sizeof(md_boot_data))) {
			CCCI_NORMAL_LOG(md_id, CHAR, "set rat string fail: copy_from_user fail!\n");
			ret = -EFAULT;
			break;
		}
		ccci_set_rat_str_to_drv(proxy_p->md_id, (char *)md_boot_data);
		break;

	case CCCI_IOC_GET_EXT_MD_POST_FIX:
		if (copy_to_user((void __user *)arg, ccci_md_get_post_fix(proxy_p->md_obj), IMG_POSTFIX_LEN)) {
			CCCI_BOOTUP_LOG(md_id, CHAR, "CCCI_IOC_GET_EXT_MD_POST_FIX: copy_to_user fail\n");
			ret = -EFAULT;
		}
		break;
	case CCCI_IOC_SEND_ICUSB_NOTIFY:
		if (copy_from_user(&sim_id, (void __user *)arg, sizeof(unsigned int))) {
			CCCI_NORMAL_LOG(md_id, CHAR, "CCCI_IOC_SEND_ICUSB_NOTIFY: copy_from_user fail!\n");
			ret = -EFAULT;
		} else {
			ret = port_proxy_send_msg_to_md(proxy_p, CCCI_SYSTEM_TX, MD_ICUSB_NOTIFY, sim_id, 1);
		}
		break;
	case CCCI_IOC_DL_TRAFFIC_CONTROL:
		if (copy_from_user(&traffic_control, (void __user *)arg, sizeof(unsigned int)))
			CCCI_NORMAL_LOG(md_id, CHAR, "CCCI_IOC_DL_TRAFFIC_CONTROL: copy_from_user fail\n");
		if (traffic_control == 1)
			;/* turn off downlink queue */
		else if (traffic_control == 0)
			;/* turn on donwlink queue */
		else
		;
		ret = 0;
		break;
	case CCCI_IOC_UPDATE_SIM_SLOT_CFG:
		if (copy_from_user(&sim_slot_cfg, (void __user *)arg, sizeof(sim_slot_cfg))) {
			CCCI_NORMAL_LOG(md_id, CHAR, "CCCI_IOC_UPDATE_SIM_SLOT_CFG: copy_from_user fail!\n");
			ret = -EFAULT;
		} else {
			int need_update;

			sim_switch_type = get_sim_switch_type();
			CCCI_NORMAL_LOG(md_id, CHAR, "CCCI_IOC_UPDATE_SIM_SLOT_CFG get s0:%d s1:%d s2:%d s3:%d\n",
				     sim_slot_cfg[0], sim_slot_cfg[1], sim_slot_cfg[2], sim_slot_cfg[3]);
			ccci_setting = ccci_get_common_setting(md_id);
			need_update = sim_slot_cfg[0];
			ccci_setting->sim_mode = sim_slot_cfg[1];
			ccci_setting->slot1_mode = sim_slot_cfg[2];
			ccci_setting->slot2_mode = sim_slot_cfg[3];
			sim_mode = ((sim_switch_type << 16) | ccci_setting->sim_mode);
			switch_sim_mode(md_id, (char *)&sim_mode, sizeof(sim_mode));
			port_proxy_send_msg_to_user(proxy_p, CCCI_MONITOR_CH, CCCI_MD_MSG_CFG_UPDATE, need_update);
			ret = 0;
		}
		break;
	case CCCI_IOC_STORE_SIM_MODE:
		if (copy_from_user(&sim_mode, (void __user *)arg, sizeof(unsigned int))) {
			CCCI_NORMAL_LOG(md_id, CHAR, "store sim mode fail: copy_from_user fail!\n");
			ret = -EFAULT;
			break;
		}
		CCCI_NORMAL_LOG(md_id, CHAR, "store sim mode(%x) in kernel space!\n", sim_mode);
		exec_ccci_kern_func_by_md_id(0, ID_STORE_SIM_SWITCH_MODE, (char *)&sim_mode,
					     sizeof(unsigned int));
		break;
	case CCCI_IOC_GET_SIM_MODE:
		CCCI_NORMAL_LOG(md_id, CHAR, "get sim mode ioctl called by %s\n", current->comm);
		exec_ccci_kern_func_by_md_id(0, ID_GET_SIM_SWITCH_MODE, (char *)&sim_mode, sizeof(unsigned int));
		ret = put_user(sim_mode, (unsigned int __user *)arg);
		break;
	case CCCI_IOC_GET_CFG_SETTING:
		ccci_setting = ccci_get_common_setting(md_id);
		if (copy_to_user((void __user *)arg, ccci_setting, sizeof(struct ccci_setting))) {
			CCCI_NORMAL_LOG(md_id, CHAR, "CCCI_IOC_GET_CFG_SETTING: copy_to_user fail\n");
			ret = -EFAULT;
		}
		break;

#ifdef CONFIG_MTK_SIM_LOCK_POWER_ON_WRITE_PROTECT
	case CCCI_IOC_RESET_AP:
		if (copy_from_user(&val, (void __user *)arg, sizeof(unsigned int)))
			CCCI_ERR_MSG(md_id, KERN, "get SML value failed.\n");

		CCCI_INF_MSG(md_id, CHAR, "get val=%x from userspace.\n", val);

		snprintf(magic_pattern, 64, "%x", val);
		set_env("sml_sync", magic_pattern);
		break;
#endif
	case CCCI_IOC_SEND_SIGNAL_TO_USER:
		if (copy_from_user(&sig_pid, (void __user *)arg, sizeof(unsigned int))) {
			CCCI_NORMAL_LOG(md_id, CHAR, "signal to rild fail: copy_from_user fail!\n");
			ret = -EFAULT;
			break;
		}
		sig = (sig_pid >> 16) & 0xFFFF;
		pid = sig_pid & 0xFFFF;
		sig_info.si_signo = sig;
		sig_info.si_code = SI_KERNEL;
		sig_info.si_pid = current->pid;
		sig_info.si_uid = __kuid_val(current->cred->uid);
		ret = kill_proc_info(SIGUSR2, &sig_info, pid);
		CCCI_NORMAL_LOG(md_id, CHAR, "send signal %d to rild %d ret=%ld\n", sig, pid, ret);
		break;
	case CCCI_IOC_RESET_MD1_MD3_PCCIF:
		CCCI_NORMAL_LOG(md_id, CHAR, "reset md pccif ioctl called by %s\n", current->comm);
		ccci_md_reset_pccif(proxy_p->md_obj);
		break;
	case CCCI_IOC_SET_EFUN:
		if (copy_from_user(&sim_mode, (void __user *)arg, sizeof(unsigned int))) {
			CCCI_ERR_MSG(md_id, CHAR, "set efun fail: copy_from_user fail!\n");
			ret = -EFAULT;
			break;
		}
		CCCI_NORMAL_LOG(md_id, CHAR, "efun set to %d\n", sim_mode);
		if (sim_mode == 0)
			ccci_md_soft_stop(proxy_p->md_obj, sim_mode);
		else if (sim_mode != 0)
			ccci_md_soft_start(proxy_p->md_obj, sim_mode);
		break;
	case CCCI_IOC_MDLOG_DUMP_DONE:
		CCCI_NORMAL_LOG(md_id, CHAR, "mdlog dump done ioctl called by %s\n", current->comm);
		proxy_p->mdlog_dump_done = 1;
		break;
	case CCCI_IOC_SET_MD_BOOT_MODE:
		if (copy_from_user(&sim_mode, (void __user *)arg, sizeof(unsigned int))) {
			CCCI_NORMAL_LOG(md_id, CHAR, "set MD boot mode fail: copy_from_user fail!\n");
			ret = -EFAULT;
		} else {
			CCCI_NORMAL_LOG(md_id, CHAR, "set MD boot mode to %d\n", sim_mode);
			exec_ccci_kern_func_by_md_id(md_id, ID_UPDATE_MD_BOOT_MODE,
						(char *)&sim_mode, sizeof(sim_mode));
#ifdef CONFIG_MTK_ECCCI_C2K
			if (md_id == MD_SYS1)
				exec_ccci_kern_func_by_md_id(MD_SYS3, ID_UPDATE_MD_BOOT_MODE,
							(char *)&sim_mode, sizeof(sim_mode));
			else if (md_id == MD_SYS3)
				exec_ccci_kern_func_by_md_id(MD_SYS1, ID_UPDATE_MD_BOOT_MODE,
							(char *)&sim_mode, sizeof(sim_mode));
#endif
		}
		break;
	case CCCI_IOC_GET_MD_BOOT_MODE:
		ret = put_user((unsigned int)ccci_md_get_boot_mode(proxy_p->md_obj), (unsigned int __user *)arg);
		break;
	case CCCI_IOC_GET_AT_CH_NUM:
		{
			unsigned int at_ch_num = 4; /*default value*/
			struct ccci_runtime_feature *rt_feature = NULL;

			rt_feature = ccci_md_get_rt_feature_by_id(proxy_p->md_obj, AT_CHANNEL_NUM, 1);
			if (rt_feature)
				ret = ccci_md_parse_rt_feature(proxy_p->md_obj,
							rt_feature, &at_ch_num, sizeof(at_ch_num));
			else
				CCCI_ERROR_LOG(md_id, CHAR, "get AT_CHANNEL_NUM fail\n");

			CCCI_NORMAL_LOG(md_id, CHAR, "get at_ch_num = %u\n", at_ch_num);
			ret = put_user(at_ch_num, (unsigned int __user *)arg);
			break;
		}
	default:
		ret = -ENOTTY;
		break;
	}
	return ret;
}

#ifdef FEATURE_SCP_CCCI_SUPPORT
int port_proxy_ccism_shm_init_ack_hdlr(struct port_proxy *proxy_p, unsigned int data)
{
	struct ccci_smem_layout *smem_layout = ccci_md_get_smem(proxy_p->md_obj);

	memset_io(smem_layout->ccci_ccism_smem_base_vir, 0, smem_layout->ccci_ccism_smem_size);
	ccci_md_scp_ipi_send(proxy_p->md_obj, CCCI_OP_SHM_INIT, &smem_layout->ccci_ccism_smem_base_phy);
	return 0;
}
#endif

static int port_proxy_get_no_response_assert_type(struct port_proxy *proxy_p, u64 latest_poll_start_time)
{
	u64 latest_isr_time = ccci_md_get_latest_isr_time(proxy_p->md_obj);
	u64 latest_poll_isr_time = ccci_md_get_latest_poll_isr_time(proxy_p->md_obj);
	u64 latest_q0_rx_time = ccci_md_get_latest_q0_rx_time(proxy_p->md_obj);
	u64 latest_rx_thread_time = proxy_p->latest_rx_thread_time;
	int md_id = proxy_p->md_id;
	unsigned long rem_nsec0, rem_nsec1, rem_nsec2, rem_nsec3, rem_nsec4;

	rem_nsec0 = (latest_poll_start_time == 0 ? 0 : do_div(latest_poll_start_time, 1000000000));
	rem_nsec1 = (latest_isr_time == 0 ? 0 : do_div(latest_isr_time, 1000000000));
	rem_nsec2 = (latest_poll_isr_time == 0 ? 0 : do_div(latest_poll_isr_time, 1000000000));
	rem_nsec3 = (latest_q0_rx_time == 0 ? 0 : do_div(latest_q0_rx_time, 1000000000));
	rem_nsec4 = (latest_rx_thread_time == 0 ? 0 : do_div(latest_rx_thread_time, 1000000000));

	CCCI_ERROR_LOG(md_id, KERN,
	"polling: start=%lu.%06lu, isr=%lu.%06lu,q0_isr=%lu.%06lu, q0_rx=%lu.%06lu,rx_thread=%lu.%06lu\n",
			(unsigned long)latest_poll_start_time, rem_nsec0 / 1000,
			(unsigned long)latest_isr_time, rem_nsec1 / 1000,
			(unsigned long)latest_poll_isr_time, rem_nsec2 / 1000,
			(unsigned long)latest_q0_rx_time, rem_nsec3 / 1000,
			(unsigned long)latest_rx_thread_time, rem_nsec4 / 1000);
	/*Check whether ap received polling queue irq, after polling start*/
	if (latest_poll_start_time > latest_poll_isr_time) {
		if (latest_poll_start_time < latest_isr_time)
			CCCI_ERROR_LOG(md_id, KERN,
			"After polling start, have isr but no polling isr, maybe md no response\n");
		else {
			CCCI_ERROR_LOG(md_id, KERN,
			"After polling start, no any irq, check ap irq status and md side send or no\n");
		}
		return MD_FORCE_ASSERT_BY_MD_NO_RESPONSE;
	}
	/*AP received polling queue irq, need check q0 work normal or no polling response package*/
	if (latest_poll_isr_time > latest_q0_rx_time) {
		CCCI_ERROR_LOG(md_id, KERN, "AP rx polling queue no work after isr, rx queue maybe blocked\n");
		return MD_FORCE_ASSERT_BY_AP_Q0_BLOCKED;
	}

	if (latest_q0_rx_time > latest_rx_thread_time) {
		CCCI_ERROR_LOG(md_id, KERN, "AP rx kthread no work after q0_rx, kthread maybe blocked\n");
		return MD_FORCE_ASSERT_BY_AP_Q0_BLOCKED;
	}

	CCCI_ERROR_LOG(md_id, KERN,
	"AP polling isr & rx queue & kthread normally after polling start, MD may not response\n");
	return MD_FORCE_ASSERT_BY_MD_NO_RESPONSE;
}

void port_proxy_md_hs1_msg_notify(struct port_proxy *proxy_p, struct sk_buff *skb)
{
	int md_id = proxy_p->md_id;

	if (ccci_md_get_state(proxy_p->md_obj) != BOOT_WAITING_FOR_HS1) {
		CCCI_ERROR_LOG(md_id, KERN, "gotten invalid hs1 message at stage%d\n",
			ccci_md_get_state(proxy_p->md_obj));
		return;
	}
	ccci_md_broadcast_state(proxy_p->md_obj, BOOT_WAITING_FOR_HS2);

	if (skb->len == sizeof(struct md_query_ap_feature) + sizeof(struct ccci_header))
		ccci_md_prepare_runtime_data(proxy_p->md_obj, skb);
	else if (skb->len == sizeof(struct ccci_header))
		CCCI_NORMAL_LOG(md_id, KERN, "get old handshake message\n");
	else
		CCCI_ERROR_LOG(md_id, KERN, "get invalid MD_QUERY_MSG, skb->len =%u\n", skb->len);

#ifdef SET_EMI_STEP_BY_STAGE
	ccci_set_mem_access_protection_second_stage(proxy_p->md_obj);
#endif
	ccci_md_dump_info(proxy_p->md_obj, DUMP_MD_BOOTUP_STATUS, NULL, 0);
	ccci_fsm_append_event(proxy_p->md_obj, CCCI_EVENT_HS1, NULL, 0);
	port_proxy_send_runtime_data_to_md(proxy_p);
}

void port_proxy_md_hs2_msg_notify(struct port_proxy *proxy_p, struct sk_buff *skb)
{
	int md_id = proxy_p->md_id;

	if (ccci_md_get_state(proxy_p->md_obj) != BOOT_WAITING_FOR_HS2) {
		CCCI_ERROR_LOG(md_id, KERN, "getton invalid hs2 message at stage %d\n",
			ccci_md_get_state(proxy_p->md_obj));
		return;
	}
	/* service of uplayer sync with modem need maybe 10s */
	port_proxy_start_wake_lock(proxy_p, 10);
	/* update this first, otherwise send message on HS2 may fail */
	ccci_md_broadcast_state(proxy_p->md_obj, READY);
	ccci_fsm_append_event(proxy_p->md_obj, CCCI_EVENT_HS2, NULL, 0);
}

void port_proxy_md_no_repsone_notify(struct port_proxy *proxy_p)
{
	ccci_md_dump_info(proxy_p->md_obj, DUMP_FLAG_QUEUE_0, NULL, 0);
	ccci_md_exception_notify(proxy_p->md_obj, MD_NO_RESPONSE);
}

void port_proxy_poll_md_fail_notify(struct port_proxy *proxy_p, u64 latest_poll_start_time)
{
	int assert_md_type = 0;

	assert_md_type = port_proxy_get_no_response_assert_type(proxy_p, latest_poll_start_time);
	if (assert_md_type == MD_FORCE_ASSERT_BY_MD_NO_RESPONSE)
		ccci_md_dump_info(proxy_p->md_obj, DUMP_FLAG_IRQ_STATUS, NULL, 0);
	ccci_md_dump_info(proxy_p->md_obj, DUMP_FLAG_QUEUE_0, NULL, 0);
	ccci_md_force_assert(proxy_p->md_obj, assert_md_type, NULL, 0);
}

unsigned int port_proxy_get_poll_seq_num(struct port_proxy *proxy_p)
{
	return ccci_md_get_seq_num(proxy_p->md_obj, OUT, CCCI_STATUS_TX);
}
void *port_proxy_get_mdee(struct port_proxy *proxy_p)
{
	return ccci_md_get_mdee(proxy_p->md_obj);
}

unsigned long long *port_proxy_get_md_net_rx_profile(struct port_proxy *proxy_p)
{
	return ccci_md_get_net_rx_profile(proxy_p->md_obj);
}

