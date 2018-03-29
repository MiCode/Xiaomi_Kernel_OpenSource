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
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/poll.h>
#include <linux/bitops.h>
#include "ccci_config.h"
#include "ccci_core.h"
#include "ccci_bm.h"
#include "port_ipc.h"
#include "ccci_ipc_msg_id.h"
#include "port_proxy.h"
#include "ccci_modem.h"
static struct ipc_task_id_map ipc_msgsvc_maptbl[] = {

#define __IPC_ID_TABLE
#include "ccci_ipc_task_ID.h"
#undef __IPC_ID_TABLE
};

#ifdef FEATURE_CONN_MD_EXP_EN
#include "conn_md_exp.h"	/* this file also include ccci_ipc_task_ID.h, must include it after ipc_msgsvc_maptbl */
#endif

#define MAX_QUEUE_LENGTH 32

#define local_AP_id_2_unify_id(id) local_xx_id_2_unify_id(id, 1)/* not using */
#define local_MD_id_2_unify_id(id) local_xx_id_2_unify_id(id, 0)
#define unify_AP_id_2_local_id(id) unify_xx_id_2_local_id(id, 1)
#define unify_MD_id_2_local_id(id) unify_xx_id_2_local_id(id, 0)/* not using */

static struct ipc_task_id_map *local_xx_id_2_unify_id(u32 local_id, int AP)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ipc_msgsvc_maptbl); i++) {
		if (ipc_msgsvc_maptbl[i].task_id == local_id &&
		    (AP ? (ipc_msgsvc_maptbl[i].extq_id & AP_UNIFY_ID_FLAG) :
		     !(ipc_msgsvc_maptbl[i].extq_id & AP_UNIFY_ID_FLAG)))
			return ipc_msgsvc_maptbl + i;
	}
	return NULL;
}

static struct ipc_task_id_map *unify_xx_id_2_local_id(u32 unify_id, int AP)
{
	int i;

	if (!(AP ? (unify_id & AP_UNIFY_ID_FLAG) : !(unify_id & AP_UNIFY_ID_FLAG)))
		return NULL;

	for (i = 0; i < ARRAY_SIZE(ipc_msgsvc_maptbl); i++) {
		if (ipc_msgsvc_maptbl[i].extq_id == unify_id)
			return ipc_msgsvc_maptbl + i;
	}
	return NULL;
}

static int port_ipc_ack_recv_skb(struct ccci_port *port, struct sk_buff *skb)
{
	struct ccci_ipc_ctrl *ipc_ctrl = (struct ccci_ipc_ctrl *)port->private_data;

	clear_bit(CCCI_TASK_PENDING, &ipc_ctrl->flag);
	wake_up_all(&ipc_ctrl->tx_wq);
	ccci_free_skb(skb);
	wake_lock_timeout(&port->rx_wakelock, HZ / 2);
	return 0;
}

/*
 * CCCI_IPC_TX/RX are treated as char device port, and we assemble CCCI_IPC_TX/RX_ACK as a
 * separate port. some IPC dedicated function are also been put here. that's why some of the function name
 * have "_ack_" in it, and others not.
 * ALL IPC ports share one ACK port.
 */
struct ccci_port_ops ipc_port_ack_ops = {
	.recv_skb = &port_ipc_ack_recv_skb,
};

int port_ipc_recv_match(struct ccci_port *port, struct sk_buff *skb)
{
	struct ccci_header *ccci_h = (struct ccci_header *)skb->data;
	struct ccci_ipc_ctrl *ipc_ctrl = (struct ccci_ipc_ctrl *)port->private_data;
	struct ipc_task_id_map *id_map;

	if (port->rx_ch != CCCI_IPC_RX)
		return 1;

	CCCI_DEBUG_LOG(port->md_id, IPC, "task_id matching: (%x/%x)\n", ipc_ctrl->task_id, ccci_h->reserved);
	id_map = unify_AP_id_2_local_id(ccci_h->reserved);
	if (id_map == NULL)
		return 0;
	if (id_map->task_id == ipc_ctrl->task_id)
		return 1;
	return 0;
}

static int send_new_time_to_md(struct ccci_modem *md, int tz);
volatile int current_time_zone;
int port_ipc_ioctl(struct ccci_port *port, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct sk_buff *skb = NULL;
	unsigned long flags;
	struct ccci_ipc_ctrl *ipc_ctrl = (struct ccci_ipc_ctrl *)port->private_data;

	switch (cmd) {
	case CCCI_IPC_RESET_RECV:
		/* purge the Rx list */
		spin_lock_irqsave(&port->rx_skb_list.lock, flags);
		while ((skb = __skb_dequeue(&port->rx_skb_list)) != NULL)
			ccci_free_skb(skb);
		spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
		break;

	case CCCI_IPC_RESET_SEND:
		clear_bit(CCCI_TASK_PENDING, &ipc_ctrl->flag);
		wake_up(&ipc_ctrl->tx_wq);
		break;

	case CCCI_IPC_WAIT_MD_READY:
		if (ipc_ctrl->md_is_ready == 0) {
			ret = wait_event_interruptible(ipc_ctrl->md_rdy_wq, ipc_ctrl->md_is_ready == 1);
			if (ret == -ERESTARTSYS)
				ret = -EINTR;
		}
		break;

	case CCCI_IPC_UPDATE_TIME:
#ifdef FEATURE_MD_GET_CLIB_TIME
		CCCI_REPEAT_LOG(port->md_id, IPC, "CCCI_IPC_UPDATE_TIME 0x%x\n", (unsigned int)arg);
		current_time_zone = (int)arg;
		ret = send_new_time_to_md(port->modem, (int)arg);
#else
		CCCI_REPEAT_LOG(port->md_id, IPC, "CCCI_IPC_UPDATE_TIME 0x%x(dummy)\n", (unsigned int)arg);
#endif
		break;

	case CCCI_IPC_WAIT_TIME_UPDATE:
		CCCI_DEBUG_LOG(port->md_id, IPC, "CCCI_IPC_WAIT_TIME_UPDATE\n");
		ret = wait_time_update_notify();
		CCCI_DEBUG_LOG(port->md_id, IPC, "CCCI_IPC_WAIT_TIME_UPDATE wakeup\n");
		break;

	case CCCI_IPC_UPDATE_TIMEZONE:
		CCCI_REPEAT_LOG(port->md_id, IPC, "CCCI_IPC_UPDATE_TIMEZONE keep 0x%x\n", (unsigned int)arg);
		current_time_zone = (int)arg;
		break;
	};
	return ret;
}

void port_ipc_md_state_notice(struct ccci_port *port, MD_STATE state)
{
	struct ccci_ipc_ctrl *ipc_ctrl = (struct ccci_ipc_ctrl *)port->private_data;

	switch (state) {
	case READY:
		ipc_ctrl->md_is_ready = 1;
		wake_up_all(&ipc_ctrl->md_rdy_wq);
		break;
	default:
		break;
	};
}

int port_ipc_write_check_id(struct ccci_port *port, struct sk_buff *skb)
{
	struct ccci_ipc_ilm *ilm = (struct ccci_ipc_ilm *)((char *)skb->data + sizeof(struct ccci_header));
	struct ipc_task_id_map *id_map;

	id_map = local_MD_id_2_unify_id(ilm->dest_mod_id);
	if (id_map == NULL) {
		CCCI_ERROR_LOG(port->md_id, IPC, "Invalid Dest MD ID (%d)\n", ilm->dest_mod_id);
		return -CCCI_ERR_IPC_ID_ERROR;
	}
	return id_map->extq_id;
}

unsigned int port_ipc_poll(struct file *fp, struct poll_table_struct *poll)
{
	struct ccci_port *port = fp->private_data;
	struct ccci_ipc_ctrl *ipc_ctrl = (struct ccci_ipc_ctrl *)port->private_data;
	unsigned int mask = 0;

	poll_wait(fp, &ipc_ctrl->tx_wq, poll);
	poll_wait(fp, &port->rx_wq, poll);
	if (!skb_queue_empty(&port->rx_skb_list))
		mask |= POLLIN | POLLRDNORM;
	if (!test_bit(CCCI_TASK_PENDING, &ipc_ctrl->flag))
		mask |= POLLOUT | POLLWRNORM;

	return mask;
}

static struct ccci_port *find_ipc_port_by_task_id(struct port_proxy *proxy_p, int task_id)
{
	return port_proxy_get_port_by_minor(proxy_p, task_id + CCCI_IPC_MINOR_BASE);
}

int port_ipc_init(struct ccci_port *port)
{
	struct ccci_ipc_ctrl *ipc_ctrl = kmalloc(sizeof(struct ccci_ipc_ctrl), GFP_KERNEL);

	port->private_data = ipc_ctrl;
	/*
	 * tricky part, we use pre-defined minor number as task ID, then we modify it into the right number.
	 */
	ipc_ctrl->task_id = port->minor;
	port->minor += CCCI_IPC_MINOR_BASE;
	init_waitqueue_head(&ipc_ctrl->tx_wq);
	init_waitqueue_head(&ipc_ctrl->md_rdy_wq);
	ipc_ctrl->md_is_ready = 0;
	ipc_ctrl->port = port;
	port->skb_from_pool = 1;

	return 0;
}

#ifdef FEATURE_GARBAGE_FILTER_SUPPORT
#define GF_PORT_LIST_MAX 128
struct garbage_filter_item gf_port_list[GF_PORT_LIST_MAX];
	/* frame size be less than 1400, so we made it global variable */
int gf_port_list_reg[GF_PORT_LIST_MAX];
int gf_port_list_unreg[GF_PORT_LIST_MAX];

static int port_ipc_parse_gf_port(GF_IP_TYPE ip_type, GF_PROTOCOL_TYPE prot_type, struct garbage_filter_item *list,
				  int number)
{
	int port_number = -1;
	/*
	*[Porting notes]
	* if really need, please find suitable net interface to get opened port list
	*/
	CCCI_NORMAL_LOG(-1, IPC, "IP:%d Protocol:%d port number:%d\n", ip_type, prot_type, port_number);
	return port_number;
}

int ccci_ipc_set_garbage_filter(struct ccci_modem *md, int reg)
{
	struct garbage_filter_header gf_header;
	int ret, actual_count, count = 0;
	struct skb_buff *skb;
	struct ccci_header *ccci_h;
	struct ccci_ipc_ilm *ilm;
	struct local_para *local_para_ptr;
	struct ccci_port *port;
	int garbage_length;

	memset(gf_port_list, 0, sizeof(gf_port_list));

	port = find_ipc_port_by_task_id(md->port_proxy_obj, AP_IPC_GF);
	if (!port)
		return -EINVAL;
	if (port->modem->md_state != READY)
		return -ENODEV;

	if (reg) {
		ret = port_ipc_parse_gf_port(GF_IPV4, GF_TCP, gf_port_list, GF_PORT_LIST_MAX - count);
		if (ret > 0)
			count += ret;
		ret = port_ipc_parse_gf_port(GF_IPV4, GF_UDP, gf_port_list + count, GF_PORT_LIST_MAX - count);
		if (ret > 0)
			count += ret;
		ret = port_ipc_parse_gf_port(GF_IPV6, GF_TCP, gf_port_list + count, GF_PORT_LIST_MAX - count);
		if (ret > 0)
			count += ret;
		ret = port_ipc_parse_gf_port(GF_IPV6, GF_UDP, gf_port_list + count, GF_PORT_LIST_MAX - count);
		if (ret > 0)
			count += ret;
		CCCI_NORMAL_LOG(md->index, IPC, "register garbage filer port number %d\n", count);
		gf_header.filter_set_id = 0;
		gf_header.filter_cnt = count;
	} else {
		int i;

		for (i = 0; i < GF_PORT_LIST_MAX; i++) {
			if (gf_port_list_unreg[i] != 0)
				count++;
			else
				break;
		}
		gf_header.filter_set_id = 0;
		if (count == 0)
			gf_header.filter_cnt = -1;	/* de-register all */
		else
			gf_header.filter_cnt = count;
		CCCI_NORMAL_LOG(md->index, IPC, "unregister garbage filer port number %d\n", count);
	}
	gf_header.uplink = 0;

	actual_count = sizeof(struct ccci_header) + sizeof(struct ccci_ipc_ilm) + sizeof(struct local_para);
	if (reg)
		actual_count += (sizeof(struct garbage_filter_header) + count * sizeof(struct garbage_filter_item));
	else
		actual_count += (sizeof(struct garbage_filter_header) + count * sizeof(int));
	skb = ccci_alloc_skb(actual_count, 1, 1);
	if (skb) {
		/* ccci header */
		ccci_h = (struct ccci_header *)skb_put(skb, sizeof(struct ccci_header));
		ccci_h->data[0] = 0;
		ccci_h->data[1] = actual_count;
		ccci_h->channel = port->tx_ch;
		ccci_h->reserved = 0;
		/* set ilm */
		ilm = (struct ccci_ipc_ilm *)skb_put(skb, sizeof(struct ccci_ipc_ilm));
		ilm->src_mod_id = AP_MOD_GF;
		ilm->dest_mod_id = MD_MOD_IPCORE;
		ilm->sap_id = 0;
		if (reg)
			ilm->msg_id = IPC_MSG_ID_IPCORE_GF_REG;
		else
			ilm->msg_id = IPC_MSG_ID_IPCORE_GF_UNREG;
		ilm->local_para_ptr = 1;	/* to let MD treat it as != NULL */
		ilm->peer_buff_ptr = 0;
		/* set ilm->local_para_ptr structure */
		local_para_ptr = (struct local_para *)skb_put(skb, sizeof(struct local_para));
		local_para_ptr->ref_count = 0;
		local_para_ptr->_stub = 0;
		if (reg)
			garbage_length = count * sizeof(struct garbage_filter_item);
		else
			garbage_length = count * sizeof(int);
		local_para_ptr->msg_len =
		    garbage_length + sizeof(struct garbage_filter_header) + sizeof(struct local_para);
		/* copy gf header */
		memcpy(skb_put(skb, sizeof(struct garbage_filter_header)),
		       &gf_header, sizeof(struct garbage_filter_header));
		/* copy gf items */
		if (reg)
			memcpy(skb_put(skb, garbage_length), gf_port_list, garbage_length);
		else
			memcpy(skb_put(skb, garbage_length), gf_port_list_unreg, garbage_length);
		CCCI_NORMAL_LOG(md->index, IPC, "garbage filer data length %d/%d\n", garbage_length, actual_count);
		ccci_mem_dump(md->index, skb->data, skb->len);
		/* send packet */
		ret = port_ipc_write_check_id(port, skb);
		if (ret < 0)
			goto err_out;
		else
			ccci_h->reserved = ret;	/* Unity ID */
		ret = ccci_port_send_skb(port, skb);
		if (ret)
			goto err_out;
		else
			return actual_count - sizeof(struct ccci_header);

 err_out:
		ccci_free_skb(skb);
		return ret;
	} else {
		return -CCCI_ERR_ALLOCATE_MEMORY_FAIL;
	}
}
#endif

static int port_ipc_kernel_write(struct ccci_modem *md, ipc_ilm_t *in_ilm)
{
	u32 task_id;
	int count, actual_count, ret;
	struct ccci_port *port;
	struct ccci_header *ccci_h;
	struct ccci_ipc_ilm *ilm;
	struct sk_buff *skb;

	/* src module id check */
	task_id = in_ilm->src_mod_id & (~AP_UNIFY_ID_FLAG);
	port = find_ipc_port_by_task_id(md->port_proxy_obj, task_id);
	if (!port) {
		CCCI_ERROR_LOG(-1, IPC, "invalid task ID %x\n", in_ilm->src_mod_id);
		return -EINVAL;
	}
	if (in_ilm->local_para_ptr == NULL) {
		CCCI_ERROR_LOG(-1, IPC, "invalid ILM local parameter pointer %p for task %d\n", in_ilm, task_id);
		return -EINVAL;
	}

	count = sizeof(struct ccci_ipc_ilm) + in_ilm->local_para_ptr->msg_len;
	if (count > CCCI_MTU) {
		CCCI_ERROR_LOG(port->md_id, IPC, "reject packet(size=%d ), lager than MTU on %s\n", count,
			     port->name);
		return -ENOMEM;
	}
	CCCI_DEBUG_LOG(port->md_id, IPC, "write on %s for %d\n", port->name, in_ilm->local_para_ptr->msg_len);

	actual_count = count + sizeof(struct ccci_header);
	skb = ccci_alloc_skb(actual_count, 1, 1);
	if (skb) {
		/* ccci header */
		ccci_h = (struct ccci_header *)skb_put(skb, sizeof(struct ccci_header));
		ccci_h->data[0] = 0;
		ccci_h->data[1] = actual_count;
		ccci_h->channel = port->tx_ch;
		ccci_h->reserved = 0;
		/* copy ilm */
		ilm = (struct ccci_ipc_ilm *)skb_put(skb, sizeof(struct ccci_ipc_ilm));
		ilm->src_mod_id = in_ilm->src_mod_id;
		ilm->dest_mod_id = in_ilm->dest_mod_id;
		ilm->sap_id = in_ilm->sap_id;
		ilm->msg_id = in_ilm->msg_id;
		ilm->local_para_ptr = 1;	/* to let MD treat it as != NULL */
		ilm->peer_buff_ptr = 0;
		/* copy data */
		count = in_ilm->local_para_ptr->msg_len;
		memcpy(skb_put(skb, count), in_ilm->local_para_ptr, count);
		/* send packet */
		ret = port_ipc_write_check_id(port, skb);
		if (ret < 0)
			goto err_out;
		else
			ccci_h->reserved = ret;	/* Unity ID */
		ret = port_proxy_send_skb_to_md(port->port_proxy, port, skb, 1);
		if (ret)
			goto err_out;
		else
			return actual_count - sizeof(struct ccci_header);

 err_out:
		ccci_free_skb(skb);
		return ret;
	} else {
		return -EBUSY;
	}
}

int ccci_ipc_send_ilm(int md_id, ipc_ilm_t *in_ilm)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	if (!md)
		return -EINVAL;
	return port_ipc_kernel_write(md, in_ilm);
}

#ifdef FEATURE_CONN_MD_EXP_EN
static int ccci_ipc_send_ilm_to_md1(ipc_ilm_t *in_ilm)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(0);

	if (!md)
		return -EINVAL;
	return port_ipc_kernel_write(md, in_ilm);
}
#endif
static int port_ipc_kernel_thread(void *arg)
{
	struct ccci_port *port = arg;
	struct sk_buff *skb;
	struct ccci_header *ccci_h;
	unsigned long flags;
	int ret = 0;
	struct ccci_ipc_ilm *ilm;
	ipc_ilm_t out_ilm;
	struct ipc_task_id_map *id_map;

	CCCI_DEBUG_LOG(port->md_id, IPC, "port %s's thread running\n", port->name);

	while (1) {
retry:
		if (skb_queue_empty(&port->rx_skb_list)) {
			ret = wait_event_interruptible(port->rx_wq, !skb_queue_empty(&port->rx_skb_list));
			if (ret == -ERESTARTSYS)
				continue;	/* FIXME */
		}
		if (kthread_should_stop())
			break;
		CCCI_DEBUG_LOG(port->md_id, IPC, "read on %s\n", port->name);
		/* 1. dequeue */
		spin_lock_irqsave(&port->rx_skb_list.lock, flags);
		skb = __skb_dequeue(&port->rx_skb_list);
		if (port->rx_skb_list.qlen == 0)
			port_proxy_ask_more_req_to_md(port->port_proxy, port);
		spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
		if (skb == NULL)
			goto retry;
		/* 2. process the request */
		/* ccci header */
		ccci_h = (struct ccci_header *)skb->data;
		skb_pull(skb, sizeof(struct ccci_header));
		ilm = (struct ccci_ipc_ilm *)(skb->data);
		/* copy ilm */
		out_ilm.src_mod_id = ilm->src_mod_id;
		out_ilm.dest_mod_id = ccci_h->reserved;
		out_ilm.sap_id = ilm->sap_id;
		out_ilm.msg_id = ilm->msg_id;
		/* data pointer */
		skb_pull(skb, sizeof(struct ccci_ipc_ilm));
		out_ilm.local_para_ptr = (struct local_para *)(skb->data);
		out_ilm.peer_buff_ptr = 0;
		id_map = unify_AP_id_2_local_id(ccci_h->reserved);
		if (id_map != NULL) {
			switch (id_map->task_id) {
			case AP_IPC_WMT:
#ifdef FEATURE_CONN_MD_EXP_EN
				mtk_conn_md_bridge_send_msg(&out_ilm);
#endif
				break;
			case AP_IPC_PKTTRC:
#if defined(CONFIG_MTK_MD_DIRECT_TETHERING_SUPPORT)
				pkt_track_md_msg_hdlr(&out_ilm);
#endif
				break;
			case AP_IPC_USB:
#if defined(CONFIG_MTK_MD_DIRECT_TETHERING_SUPPORT) || defined(CONFIG_MTK_MD_DIRECT_LOGGING_SUPPORT)
				musb_md_msg_hdlr(&out_ilm);
#endif
				break;
			default:
				CCCI_ERROR_LOG(port->md_id, IPC, "recv unknown task ID %d\n", id_map->task_id);
				break;
			}
		} else {
			CCCI_ERROR_LOG(port->md_id, IPC, "recv unknown module ID %d\n", ccci_h->reserved);
		}
		CCCI_DEBUG_LOG(port->md_id, IPC, "read done on %s l=%d\n", port->name,
			     out_ilm.local_para_ptr->msg_len);
		ccci_free_skb(skb);
	}
	return 0;
}

static int port_ipc_kernel_init(struct ccci_port *port)
{
	struct ccci_ipc_ctrl *ipc_ctrl;

	CCCI_DEBUG_LOG(port->md_id, IPC, "IPC kernel port %s is initializing\n", port->name);
	kthread_run(port_ipc_kernel_thread, port, "%s", port->name);
	port->rx_length_th = MAX_QUEUE_LENGTH;

	port_ipc_init(port);
	ipc_ctrl = (struct ccci_ipc_ctrl *)port->private_data;
	if (ipc_ctrl->task_id == AP_IPC_WMT) {
#ifdef FEATURE_CONN_MD_EXP_EN
		CONN_MD_BRIDGE_OPS ccci_ipc_conn_ops = {.rx_cb = ccci_ipc_send_ilm_to_md1};

		mtk_conn_md_bridge_reg(MD_MOD_EL1, &ccci_ipc_conn_ops);
#endif
	}
	return 0;
}

struct ccci_port_ops ipc_kern_port_ops = {
	.init = &port_ipc_kernel_init,
	.recv_skb = &port_recv_skb,
	.recv_match = &port_ipc_recv_match,
	.md_state_notice = &port_ipc_md_state_notice,
};

int send_new_time_to_md(struct ccci_modem *md, int tz)
{
	ipc_ilm_t in_ilm;
	char local_param[sizeof(local_para_struct) + 16];
	unsigned int timeinfo[4];
	struct timeval tv = { 0 };

	do_gettimeofday(&tv);

	timeinfo[0] = tv.tv_sec;
	timeinfo[1] = sizeof(tv.tv_sec) > 4 ? tv.tv_sec >> 32 : 0;
	timeinfo[2] = tz;
	timeinfo[3] = sys_tz.tz_dsttime;

	in_ilm.src_mod_id = AP_MOD_CCCIIPC;
	in_ilm.dest_mod_id = MD_MOD_CCCIIPC;
	in_ilm.sap_id = 0;
	in_ilm.msg_id = IPC_MSG_ID_CCCIIPC_CLIB_TIME_REQ;
	in_ilm.local_para_ptr = (local_para_struct *)&local_param[0];
	/* msg_len not only contain local_para_ptr->data, but also contain 4 Bytes header itself */
	in_ilm.local_para_ptr->msg_len = 20;
	memcpy(in_ilm.local_para_ptr->data, timeinfo, 16);

	CCCI_DEBUG_LOG(-1, IPC, "Update time(R): [sec=0x%lx][timezone=0x%08x][des=0x%08x]\n", tv.tv_sec,
		     sys_tz.tz_minuteswest, sys_tz.tz_dsttime);
	CCCI_DEBUG_LOG(-1, IPC, "Update time(A): [L:0x%08x][H:0x%08x][0x%08x][0x%08x]\n", timeinfo[0], timeinfo[1],
		     timeinfo[2], timeinfo[3]);
	if (port_ipc_kernel_write(md, &in_ilm) < 0) {
		CCCI_NORMAL_LOG(-1, IPC, "Update fail\n");
		return -1;
	}
	CCCI_REPEAT_LOG(-1, IPC, "Update success\n");
	return 0;
}

int ccci_get_emi_info(int md_id, struct ccci_emi_info *emi_info)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	if (!md || !emi_info)
		return -EINVAL;

	emi_info->ap_domain_id = 0;
	emi_info->md_domain_id = 1;
	emi_info->ap_view_bank0_base = md->mem_layout.md_region_phy;
	emi_info->bank0_size = md->mem_layout.md_region_size;
	emi_info->ap_view_bank4_base = md->mem_layout.smem_offset_AP_to_MD + 0x40000000;
	emi_info->bank4_size = md->mem_layout.smem_region_size +
				(md->mem_layout.smem_region_phy - md->mem_layout.smem_offset_AP_to_MD - 0x40000000);
	return 0;
}

