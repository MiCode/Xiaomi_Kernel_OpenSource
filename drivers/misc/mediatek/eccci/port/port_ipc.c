// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/poll.h>
#include <linux/bitops.h>
#include "mt-plat/mtk_ccci_common.h"
#include "ccci_config.h"
#include "ccci_common_config.h"
#include "ccci_core.h"
#include "ccci_bm.h"
#include "port_ipc.h"
#include "port_ipc_internal.h"
#include "ccci_ipc_msg_id.h"
#include "ccci_modem.h"
#include "ccci_fsm.h"
#include "port_t.h"
static struct ipc_task_id_map ipc_msgsvc_maptbl[] = {

#define __IPC_ID_TABLE
#include "ccci_ipc_task_ID.h"
#undef __IPC_ID_TABLE
};

#ifdef CONFIG_MTK_CONN_MD
/* this file also include ccci_ipc_task_ID.h,
 * must include it after ipc_msgsvc_maptbl
 */
#include "conn_md_exp.h"
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

	if (!(AP ? (unify_id & AP_UNIFY_ID_FLAG) :
		!(unify_id & AP_UNIFY_ID_FLAG)))
		return NULL;

	for (i = 0; i < ARRAY_SIZE(ipc_msgsvc_maptbl); i++) {
		if (ipc_msgsvc_maptbl[i].extq_id == unify_id)
			return ipc_msgsvc_maptbl + i;
	}
	return NULL;
}

int port_ipc_recv_match(struct port_t *port, struct sk_buff *skb)
{
	struct ccci_header *ccci_h = (struct ccci_header *)skb->data;
	struct ccci_ipc_ctrl *ipc_ctrl =
		(struct ccci_ipc_ctrl *)port->private_data;
	struct ipc_task_id_map *id_map;

	if (port->rx_ch != CCCI_IPC_RX)
		return 1;

	CCCI_DEBUG_LOG(port->md_id, IPC,
		"task_id matching: (%x/%x)\n",
		ipc_ctrl->task_id, ccci_h->reserved);
	id_map = unify_AP_id_2_local_id(ccci_h->reserved);
	if (id_map == NULL)
		return 0;
	if (id_map->task_id == ipc_ctrl->task_id)
		return 1;
	return 0;
}

static int send_new_time_to_md(int md_id, int tz);
int current_time_zone;

long port_ipc_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	long ret = 0;
	struct port_t *port = file->private_data;
	struct sk_buff *skb = NULL;
	unsigned long flags;
	struct ccci_ipc_ctrl *ipc_ctrl =
		(struct ccci_ipc_ctrl *)port->private_data;

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
			ret = wait_event_interruptible(ipc_ctrl->md_rdy_wq,
				ipc_ctrl->md_is_ready == 1);
			if (ret == -ERESTARTSYS)
				ret = -EINTR;
		}
		break;

	case CCCI_IPC_UPDATE_TIME:
		CCCI_REPEAT_LOG(port->md_id, IPC,
			"CCCI_IPC_UPDATE_TIME 0x%x\n", (unsigned int)arg);
		current_time_zone = (int)arg;
		ret = send_new_time_to_md(port->md_id, (int)arg);
		break;

	case CCCI_IPC_WAIT_TIME_UPDATE:
		CCCI_DEBUG_LOG(port->md_id, IPC,
			"CCCI_IPC_WAIT_TIME_UPDATE\n");
		ret = wait_time_update_notify();
		CCCI_DEBUG_LOG(port->md_id, IPC,
			"CCCI_IPC_WAIT_TIME_UPDATE wakeup\n");
		break;


	case CCCI_IPC_UPDATE_TIMEZONE:
		CCCI_REPEAT_LOG(port->md_id, IPC,
			"CCCI_IPC_UPDATE_TIMEZONE keep 0x%x\n",
			(unsigned int)arg);
		current_time_zone = (int)arg;
		break;
	default:
			ret = -1;
			break;
	};

	if (ret == -1)
		ret = port_dev_ioctl(file, cmd, arg);
	return ret;
}

void port_ipc_md_state_notify(struct port_t *port, unsigned int state)
{
	struct ccci_ipc_ctrl *ipc_ctrl =
		(struct ccci_ipc_ctrl *)port->private_data;

	switch (state) {
	case READY:
		ipc_ctrl->md_is_ready = 1;
		wake_up_all(&ipc_ctrl->md_rdy_wq);
		break;
	default:
		break;
	};
}

int port_ipc_write_check_id(struct port_t *port, struct sk_buff *skb)
{
	struct ccci_ipc_ilm *ilm =
		(struct ccci_ipc_ilm *)((char *)skb->data +
		sizeof(struct ccci_header));
	struct ipc_task_id_map *id_map;

	id_map = local_MD_id_2_unify_id(ilm->dest_mod_id);
	if (id_map == NULL) {
		CCCI_ERROR_LOG(port->md_id, IPC,
		"Invalid Dest MD ID (%d)\n", ilm->dest_mod_id);
		return -CCCI_ERR_IPC_ID_ERROR;
	}
	return id_map->extq_id;
}

unsigned int port_ipc_poll(struct file *fp, struct poll_table_struct *poll)
{
	struct port_t *port = fp->private_data;
	struct ccci_ipc_ctrl *ipc_ctrl =
		(struct ccci_ipc_ctrl *)port->private_data;
	unsigned int mask = 0;

	poll_wait(fp, &ipc_ctrl->tx_wq, poll);
	poll_wait(fp, &port->rx_wq, poll);
	if (!skb_queue_empty(&port->rx_skb_list))
		mask |= POLLIN | POLLRDNORM;
	if (!test_bit(CCCI_TASK_PENDING, &ipc_ctrl->flag))
		mask |= POLLOUT | POLLWRNORM;

	return mask;
}

static struct port_t *find_ipc_port_by_task_id(int md_id, int task_id)
{
	return port_get_by_minor(md_id, task_id + CCCI_IPC_MINOR_BASE);
}


static const struct file_operations ipc_dev_fops = {
	.owner = THIS_MODULE,
	.open = &port_dev_open, /*use default API*/
	.read = &port_dev_read, /*use default API*/
	.write = &port_dev_write, /*use default API*/
	.release = &port_dev_close,/*use default API*/
	.unlocked_ioctl = &port_ipc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = &port_dev_compat_ioctl,
#endif
	.poll = &port_ipc_poll,
};

static int port_ipc_kernel_write(int md_id, struct ipc_ilm *in_ilm)
{
	u32 task_id;
	int count, actual_count, ret;
	struct port_t *port;
	struct ccci_header *ccci_h;
	struct ccci_ipc_ilm *ilm;
	struct sk_buff *skb;

	/* src module id check */
	task_id = in_ilm->src_mod_id & (~AP_UNIFY_ID_FLAG);
	port = find_ipc_port_by_task_id(md_id, task_id);
	if (!port) {
		CCCI_ERROR_LOG(-1, IPC, "invalid task ID %x\n",
		in_ilm->src_mod_id);
		return -EINVAL;
	}
	if (in_ilm->local_para_ptr == NULL) {
		CCCI_ERROR_LOG(-1, IPC,
			"invalid ILM local parameter pointer %p for task %d\n",
			in_ilm, task_id);
		return -EINVAL;
	}

	count = sizeof(struct ccci_ipc_ilm) +
		in_ilm->local_para_ptr->msg_len;
	if (count > CCCI_MTU) {
		CCCI_ERROR_LOG(port->md_id, IPC,
			"reject packet(size=%d ), lager than MTU on %s\n",
			count, port->name);
		return -ENOMEM;
	}
	CCCI_DEBUG_LOG(port->md_id, IPC, "write on %s for %d\n",
		port->name, in_ilm->local_para_ptr->msg_len);

	actual_count = count + sizeof(struct ccci_header);
	skb = ccci_alloc_skb(actual_count, 1, 1);
	if (skb) {
		/* ccci header */
		ccci_h = (struct ccci_header *)skb_put(skb,
			sizeof(struct ccci_header));
		ccci_h->data[0] = 0;
		ccci_h->data[1] = actual_count;
		ccci_h->channel = port->tx_ch;
		ccci_h->reserved = 0;
		/* copy ilm */
		ilm = (struct ccci_ipc_ilm *)skb_put(skb,
			sizeof(struct ccci_ipc_ilm));
		ilm->src_mod_id = in_ilm->src_mod_id;
		ilm->dest_mod_id = in_ilm->dest_mod_id;
		ilm->sap_id = in_ilm->sap_id;
		ilm->msg_id = in_ilm->msg_id;
		/* to let MD treat it as != NULL */
		ilm->local_para_ptr = 1;
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
		ret = port_send_skb_to_md(port, skb, 1);
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

int ccci_ipc_send_ilm(int md_id, struct ipc_ilm *in_ilm)
{
	if (md_id < 0 || md_id >= MAX_MD_NUM)
		return -EINVAL;
	return port_ipc_kernel_write(md_id, in_ilm);
}

#ifdef CONFIG_MTK_CONN_MD
static int ccci_ipc_send_ilm_to_md1(struct ipc_ilm *in_ilm)
{
	return port_ipc_kernel_write(0, in_ilm);
}
#endif
static int port_ipc_kernel_thread(void *arg)
{
	struct port_t *port = arg;
	struct sk_buff *skb;
	struct ccci_header *ccci_h;
	unsigned long flags;
	int ret = 0;
	struct ccci_ipc_ilm *ilm;
	struct ipc_ilm out_ilm;
	struct ipc_task_id_map *id_map;

	CCCI_DEBUG_LOG(port->md_id, IPC,
		"port %s's thread running\n", port->name);

	while (1) {
retry:
		if (skb_queue_empty(&port->rx_skb_list)) {
			ret = wait_event_interruptible(port->rx_wq,
				!skb_queue_empty(&port->rx_skb_list));
			if (ret == -ERESTARTSYS)
				continue;	/* FIXME */
		}
		if (kthread_should_stop())
			break;
		CCCI_DEBUG_LOG(port->md_id, IPC,
			"read on %s\n", port->name);
		/* 1. dequeue */
		spin_lock_irqsave(&port->rx_skb_list.lock, flags);
		skb = __skb_dequeue(&port->rx_skb_list);
		if (port->rx_skb_list.qlen == 0)
			port_ask_more_req_to_md(port);
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
#ifdef CONFIG_MTK_CONN_MD
				mtk_conn_md_bridge_send_msg(&out_ilm);
#endif
				break;
			case AP_IPC_PKTTRC:
#if defined(CONFIG_MTK_MD_DIRECT_TETHERING_SUPPORT)
				pkt_track_md_msg_hdlr(&out_ilm);
#endif
				break;
			case AP_IPC_USB:
#if defined(CONFIG_MTK_MD_DIRECT_TETHERING_SUPPORT) \
	|| defined(CONFIG_MTK_MD_DIRECT_LOGGING_SUPPORT)
				rndis_md_msg_hdlr(&out_ilm);
#endif
				break;
			default:
				CCCI_ERROR_LOG(port->md_id, IPC,
					"recv unknown task ID %d\n",
					id_map->task_id);
				break;
			}
		} else {
			CCCI_ERROR_LOG(port->md_id, IPC,
				"recv unknown module ID %d\n",
				ccci_h->reserved);
		}
		CCCI_DEBUG_LOG(port->md_id, IPC,
			"read done on %s l=%d\n", port->name,
			out_ilm.local_para_ptr->msg_len);
		ccci_free_skb(skb);
	}
	return 0;
}
int port_ipc_init(struct port_t *port)
{
	struct cdev *dev;
	int ret = 0;
	struct ccci_ipc_ctrl *ipc_ctrl =
		kmalloc(sizeof(struct ccci_ipc_ctrl), GFP_KERNEL);

	if (unlikely(!ipc_ctrl)) {
		CCCI_ERROR_LOG(port->md_id, IPC, "alloc ipc_ctrl fail!!\n");
		return -1;
	}

	port->rx_length_th = MAX_QUEUE_LENGTH;
	port->private_data = ipc_ctrl;
	/*
	 * tricky part, we use pre-defined minor number as task ID,
	 * then we modify it into the right number.
	 */
	ipc_ctrl->task_id = port->minor;
	port->minor += CCCI_IPC_MINOR_BASE;
	init_waitqueue_head(&ipc_ctrl->tx_wq);
	init_waitqueue_head(&ipc_ctrl->md_rdy_wq);
	ipc_ctrl->md_is_ready = 0;
	ipc_ctrl->port = port;
	port->skb_from_pool = 1;
	if (port->flags & PORT_F_WITH_CHAR_NODE) {
		dev = kmalloc(sizeof(struct cdev), GFP_KERNEL);
		if (unlikely(!dev)) {
			CCCI_ERROR_LOG(port->md_id, IPC,
				"alloc ipc char dev fail!!\n");
			kfree(ipc_ctrl);
			return -1;
		}
		cdev_init(dev, &ipc_dev_fops);
		dev->owner = THIS_MODULE;
		ret = cdev_add(dev, MKDEV(port->major,
			port->minor_base + port->minor), 1);
		ret = ccci_register_dev_node(port->name, port->major,
			port->minor_base + port->minor);
		port->interception = 0;
		port->flags |= PORT_F_ADJUST_HEADER;
	} else {
		kthread_run(port_ipc_kernel_thread, port, "%s", port->name);
		if (ipc_ctrl->task_id == AP_IPC_WMT) {
#ifdef CONFIG_MTK_CONN_MD
			struct conn_md_bridge_ops ccci_ipc_conn_ops = {
			.rx_cb = ccci_ipc_send_ilm_to_md1};

			mtk_conn_md_bridge_reg(MD_MOD_EL1, &ccci_ipc_conn_ops);
			mtk_conn_md_bridge_reg(MD_MOD_GMMGR,
					&ccci_ipc_conn_ops);
#endif
		}
	}
	return 0;
}

struct port_ops ipc_port_ops = {
	.init = &port_ipc_init,
	.recv_skb = &port_recv_skb,
	.recv_match = &port_ipc_recv_match,
	.md_state_notify = &port_ipc_md_state_notify,
};

int send_new_time_to_md(int md_id, int tz)
{
	struct ipc_ilm in_ilm;
	char local_param[sizeof(struct local_para) + 16];
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
	in_ilm.local_para_ptr = (struct local_para *)&local_param[0];
	/* msg_len not only contain local_para_ptr->data,
	 * but also contain 4 Bytes header itself
	 */
	in_ilm.local_para_ptr->msg_len = 20;
	memcpy(in_ilm.local_para_ptr->data, timeinfo, 16);

	CCCI_DEBUG_LOG(md_id, IPC,
		"Update time(R): [sec=0x%lx][timezone=0x%08x][des=0x%08x]\n",
		tv.tv_sec, sys_tz.tz_minuteswest, sys_tz.tz_dsttime);
	CCCI_DEBUG_LOG(md_id, IPC,
		"Update time(A): [L:0x%08x][H:0x%08x][0x%08x][0x%08x]\n",
		timeinfo[0], timeinfo[1], timeinfo[2], timeinfo[3]);
	if (port_ipc_kernel_write(md_id, &in_ilm) < 0) {
		CCCI_NORMAL_LOG(md_id, IPC, "Update fail\n");
		return -1;
	}
	CCCI_REPEAT_LOG(md_id, IPC, "Update success\n");
	return 0;
}

int ccci_get_emi_info(int md_id, struct ccci_emi_info *emi_info)
{
	struct ccci_mem_layout *mem_layout = NULL;

	if (md_id < 0 || md_id > MAX_MD_NUM || !emi_info)
		return -EINVAL;
	mem_layout = ccci_md_get_mem(md_id);

	emi_info->ap_domain_id = 0;
	emi_info->md_domain_id = 1;
	emi_info->ap_view_bank0_base = mem_layout->md_bank0.base_ap_view_phy;
	emi_info->bank0_size = mem_layout->md_bank0.size;
	emi_info->ap_view_bank4_base =
		mem_layout->md_bank4_noncacheable_total.base_md_view_phy;
	emi_info->bank4_size = mem_layout->md_bank4_noncacheable_total.size;
	return 0;
}

