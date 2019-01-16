#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/poll.h>
#include <asm/bitops.h>
#include <mach/ccci_config.h>
#include "ccci_core.h"
#include "ccci_bm.h"
#include "port_ipc.h"

static struct ccci_ipc_ctrl *ipc_task_ctrl[MAX_NUM_IPC_TASKS];
static struct ipc_task_id_map ipc_msgsvc_maptbl[] =
{
#define __IPC_ID_TABLE
#include "ccci_ipc_task_ID.h"
#undef __IPC_ID_TABLE
};

#ifdef FEATURE_CONN_MD_EXP_EN
#include "conn_md_exp.h" // this file also include ccci_ipc_task_ID.h, must include it after ipc_msgsvc_maptbl
#endif

#define MAX_QUEUE_LENGTH 32

#define local_AP_id_2_unify_id(id) local_xx_id_2_unify_id(id,1) // not using
#define local_MD_id_2_unify_id(id) local_xx_id_2_unify_id(id,0)
#define unify_AP_id_2_local_id(id) unify_xx_id_2_local_id(id,1)
#define unify_MD_id_2_local_id(id) unify_xx_id_2_local_id(id,0) // not using

static struct ipc_task_id_map *local_xx_id_2_unify_id(u32 local_id,int AP)
{
	int i;
	for (i=0; i<ARRAY_SIZE(ipc_msgsvc_maptbl); i++) {
		if(ipc_msgsvc_maptbl[i].task_id==local_id &&
			 (AP?(ipc_msgsvc_maptbl[i].extq_id&AP_UNIFY_ID_FLAG):!(ipc_msgsvc_maptbl[i].extq_id&AP_UNIFY_ID_FLAG)))
			return  ipc_msgsvc_maptbl+i;
	}	
	return NULL;
}

static struct ipc_task_id_map *unify_xx_id_2_local_id(u32 unify_id,int AP)
{
	int i;
	if (!(AP?(unify_id&AP_UNIFY_ID_FLAG):!(unify_id&AP_UNIFY_ID_FLAG)))
		return NULL;
	
	for (i=0; i<ARRAY_SIZE(ipc_msgsvc_maptbl); i++) {
		if(ipc_msgsvc_maptbl[i].extq_id==unify_id)
			return 	ipc_msgsvc_maptbl+i;
	}
	return NULL;
}

static int port_ipc_ack_init(struct ccci_port *port)
{
	return 0;
}

static int port_ipc_ack_recv_req(struct ccci_port *port, struct ccci_request *req)
{
	struct ccci_header *ccci_h = (struct ccci_header *)req->skb->data;
	struct ccci_ipc_ctrl *ipc_ctrl = ipc_task_ctrl[ccci_h->reserved]; // find port via task ID
	list_del(&req->entry); // dequeue from queue's list
	clear_bit(CCCI_TASK_PENDING, &ipc_ctrl->flag);
	wake_up_all(&ipc_ctrl->tx_wq);
	req->policy = RECYCLE;
	ccci_free_req(req);
	wake_lock_timeout(&port->rx_wakelock, HZ/2);
	return 0;
}

/*
 * CCCI_IPC_TX/RX are treated as char device port, and we assemble CCCI_IPC_TX/RX_ACK as a
 * seperate port. some IPC dedicated function are also been put here. that's why some of the function name
 * have "_ack_" in it, and others not.
 * ALL IPC ports share one ACK port.
 */
struct ccci_port_ops ipc_port_ack_ops = {
	.init = &port_ipc_ack_init,
	.recv_request = &port_ipc_ack_recv_req,
};

int port_ipc_req_match(struct ccci_port *port, struct ccci_request *req)
{
	struct ccci_header *ccci_h = (struct ccci_header *)req->skb->data;
	struct ccci_ipc_ctrl *ipc_ctrl = (struct ccci_ipc_ctrl *)port->private_data;
    struct ipc_task_id_map *id_map;

	if(port->rx_ch != CCCI_IPC_RX)
		return 1;
	
	CCCI_DBG_MSG(port->modem->index, IPC, "task_id matching: (%x/%x)\n", ipc_ctrl->task_id, ccci_h->reserved);
	if ((id_map=unify_AP_id_2_local_id(ccci_h->reserved)) == NULL) {
		return 0;
   	} else {
		if(id_map->task_id == ipc_ctrl->task_id) {
			return 1;
		}
   	}
	return 0;
}

int port_ipc_tx_wait(struct ccci_port *port)
{
	struct ccci_ipc_ctrl *ipc_ctrl = (struct ccci_ipc_ctrl *)port->private_data;
	int ret;
	
	ret = wait_event_interruptible(ipc_ctrl->tx_wq, !test_and_set_bit(CCCI_TASK_PENDING, &ipc_ctrl->flag));
	if(ret == -ERESTARTSYS) {
		return -EINTR;
	}
	return 0;
}

int port_ipc_rx_ack(struct ccci_port *port)
{
	struct ccci_ipc_ctrl *ipc_ctrl = (struct ccci_ipc_ctrl *)port->private_data;
	return ccci_send_msg_to_md(port->modem, CCCI_IPC_RX_ACK, IPC_MSGSVC_RVC_DONE, ipc_ctrl->task_id, 1);
}

int port_ipc_ioctl(struct ccci_port *port, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct ccci_request *req = NULL;
	struct ccci_request *reqn;
	unsigned long flags;
	struct ccci_ipc_ctrl *ipc_ctrl = (struct ccci_ipc_ctrl *)port->private_data;
	
	switch(cmd) {
	case CCCI_IPC_RESET_RECV:
		// purge the Rx list
		spin_lock_irqsave(&port->rx_req_lock, flags);
		list_for_each_entry_safe(req, reqn, &port->rx_req_list, entry) {
			list_del(&req->entry);
			port->rx_length++;
			req->policy = RECYCLE;
			ccci_free_req(req);
		}
		INIT_LIST_HEAD(&port->rx_req_list);
		spin_unlock_irqrestore(&port->rx_req_lock, flags);
		break;

	case CCCI_IPC_RESET_SEND:
		clear_bit(CCCI_TASK_PENDING, &ipc_ctrl->flag);
		wake_up(&ipc_ctrl->tx_wq);
		break;

	case CCCI_IPC_WAIT_MD_READY:
		if (ipc_ctrl->md_is_ready == 0)
		{
			interruptible_sleep_on(&ipc_ctrl->md_rdy_wq);
			if (signal_pending(current)) {
				CCCI_INF_MSG(port->modem->index, IPC, "Got signal @ WAIT_MD_READY\n");
				ret = -EINTR;
			}
		}
		break;
	};
	return ret;
}

void port_ipc_md_state_notice(struct ccci_port *port, MD_STATE state)
{
	struct ccci_ipc_ctrl *ipc_ctrl = (struct ccci_ipc_ctrl *)port->private_data;
	
	switch(state) {
	case READY:
		ipc_ctrl->md_is_ready=1;
		wake_up_all(&ipc_ctrl->md_rdy_wq);
		break;
	default:
		break;
	};
}

int port_ipc_write_check_id(struct ccci_port *port, struct ccci_request *req)
{
	struct ccci_ipc_ilm *ilm = (struct ccci_ipc_ilm *)((char*)req->skb->data + sizeof(struct ccci_header));
    struct ipc_task_id_map *id_map;
	
	if((id_map=local_MD_id_2_unify_id(ilm->dest_mod_id)) == NULL) {
		CCCI_ERR_MSG(port->modem->index, IPC, "Invalid Dest MD ID (%d)\n", ilm->dest_mod_id);
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
	if(!list_empty(&port->rx_req_list))
		mask |= POLLIN|POLLRDNORM;
	if (!test_bit(CCCI_TASK_PENDING, &ipc_ctrl->flag)) 
		mask |= POLLOUT|POLLWRNORM;
	
	return mask;
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
	ipc_task_ctrl[ipc_ctrl->task_id] = ipc_ctrl;
	init_waitqueue_head(&ipc_ctrl->tx_wq);
	init_waitqueue_head(&ipc_ctrl->md_rdy_wq);
	ipc_ctrl->md_is_ready = 0;
	ipc_ctrl->port = port;

	return 0;
}

static int port_ipc_kernel_write(ipc_ilm_t *in_ilm)
{
	u32 task_id;
	int count, actual_count, ret;
	struct ccci_port *port;
	struct ccci_header *ccci_h;
	struct ccci_ipc_ilm *ilm;
	struct ccci_request *req;

	// src module id check
    task_id = in_ilm->src_mod_id & (~AP_UNIFY_ID_FLAG);
	if(task_id >= ARRAY_SIZE(ipc_task_ctrl)) {
		CCCI_ERR_MSG(-1, IPC, "invalid task ID %x\n", in_ilm->src_mod_id);
		return -1;
	}
    if(in_ilm->local_para_ptr == NULL){
		CCCI_ERR_MSG(-1, IPC, "invalid ILM local parameter pointer %p for task %d\n", in_ilm, task_id);
        return -2;
    }
	
	port = ipc_task_ctrl[task_id]->port;
	if(port->modem->md_state!=READY)
		return -ENODEV;
	
	count = sizeof(struct ccci_ipc_ilm) + in_ilm->local_para_ptr->msg_len;
	if(count > CCCI_MTU) {
		CCCI_ERR_MSG(port->modem->index, IPC, "reject packet(size=%d ), lager than MTU on %s\n", count, port->name);
		return -ENOMEM;
	}
	CCCI_DBG_MSG(port->modem->index, IPC, "write on %s for %d\n", port->name, in_ilm->local_para_ptr->msg_len);
	
	actual_count = count + sizeof(struct ccci_header);
	req = ccci_alloc_req(OUT, actual_count, 1, 1);
	if(req) {
		req->policy = RECYCLE;
		// ccci header
		ccci_h = (struct ccci_header *)skb_put(req->skb, sizeof(struct ccci_header));
		ccci_h->data[0] = 0;
		ccci_h->data[1] = actual_count;
		ccci_h->channel = port->tx_ch;
		ccci_h->reserved = 0;
		// copy ilm
		ilm = (struct ccci_ipc_ilm *)skb_put(req->skb, sizeof(struct ccci_ipc_ilm));
		ilm->src_mod_id = in_ilm->src_mod_id;
		ilm->dest_mod_id = in_ilm->dest_mod_id;
		ilm->sap_id = in_ilm->sap_id;
		ilm->msg_id = in_ilm->msg_id;
		ilm->local_para_ptr = 1; // to let MD treat it as != NULL
		ilm->peer_buff_ptr = 0;
		// copy data
		count = in_ilm->local_para_ptr->msg_len;
		memcpy(skb_put(req->skb, count), in_ilm->local_para_ptr, count);
		if((ret=port_ipc_write_check_id(port, req)) < 0)
			goto err_out;
		else
			ccci_h->reserved = ret; // Unity ID
		ret = ccci_port_send_request(port, req);
		if(ret)
			goto err_out;
		else
			return (actual_count-sizeof(struct ccci_header));
		
err_out:
		ccci_free_req(req);
		return ret;
	} else {
		return -EBUSY;
	}
}

static int port_ipc_kernel_recv_req(struct ccci_port *port, struct ccci_request *req)
{
	unsigned long flags;
	
	spin_lock_irqsave(&port->rx_req_lock, flags);
	CCCI_DBG_MSG(port->modem->index, IPC, "recv on %s, len=%d\n", port->name, port->rx_length);
	if(port->rx_length < port->rx_length_th) {
		port->flags &= ~PORT_F_RX_FULLED;
		port->rx_length++;
		list_del(&req->entry); // dequeue from queue's list
		list_add_tail(&req->entry, &port->rx_req_list);
		spin_unlock_irqrestore(&port->rx_req_lock, flags);
		wake_lock_timeout(&port->rx_wakelock, HZ);
		wake_up_all(&port->rx_wq);
		return 0;
	} else {
		port->flags |= PORT_F_RX_FULLED;
		spin_unlock_irqrestore(&port->rx_req_lock, flags);
		if(port->flags & PORT_F_ALLOW_DROP/* || !(port->flags&PORT_F_RX_EXCLUSIVE)*/) {
			CCCI_INF_MSG(port->modem->index, IPC, "port %s Rx full, drop packet\n", port->name);
			goto drop;
		} else {
			return -CCCI_ERR_PORT_RX_FULL;
		}
	}

drop:
	// drop this packet
	CCCI_INF_MSG(port->modem->index, IPC, "drop on %s, len=%d\n", port->name, port->rx_length);
	list_del(&req->entry); 
	req->policy = RECYCLE;
	ccci_free_req(req);
	return -CCCI_ERR_DROP_PACKET;
}

static int port_ipc_kernel_thread(void *arg)
{
	struct ccci_port *port = arg;
	struct ccci_request *req;
	struct ccci_header *ccci_h;
	unsigned long flags;
	int ret;
	struct ccci_ipc_ilm *ilm;
	ipc_ilm_t out_ilm;

	CCCI_DBG_MSG(port->modem->index, IPC, "port %s's thread runnning\n", port->name);

	while(1) {
		if(list_empty(&port->rx_req_list)) {
			ret = wait_event_interruptible(port->rx_wq, !list_empty(&port->rx_req_list));	
			if(ret == -ERESTARTSYS) {
				continue; // FIXME
			}
		}
		if (kthread_should_stop()){
				break ;
		}
		CCCI_DBG_MSG(port->modem->index, IPC, "read on %s\n", port->name);
		// 1. dequeue
		spin_lock_irqsave(&port->rx_req_lock, flags);
		req = list_first_entry(&port->rx_req_list, struct ccci_request, entry);
		list_del(&req->entry);
		if(--(port->rx_length) == 0)
			ccci_port_ask_more_request(port);
		spin_unlock_irqrestore(&port->rx_req_lock, flags);
		// 2. process the request
		// ccci header
		ccci_h = (struct ccci_header *)req->skb->data;
		skb_pull(req->skb, sizeof(struct ccci_header));
		ilm = (struct ccci_ipc_ilm*)(req->skb->data);
		// copy ilm
		out_ilm.src_mod_id = ilm->src_mod_id;
		out_ilm.dest_mod_id = ccci_h->reserved;
		out_ilm.sap_id = ilm->sap_id;
		out_ilm.msg_id = ilm->msg_id;
		// data pointer
		skb_pull(req->skb, sizeof(struct ccci_ipc_ilm));
		out_ilm.local_para_ptr = (struct local_para *)(req->skb->data);
		out_ilm.peer_buff_ptr = 0;
		#ifdef FEATURE_CONN_MD_EXP_EN
		mtk_conn_md_bridge_send_msg(&out_ilm);
		#endif
		port->rx_length--;
		CCCI_DBG_MSG(port->modem->index, IPC, "read done on %s l=%d\n", port->name, out_ilm.local_para_ptr->msg_len);
		req->policy = RECYCLE;
		ccci_free_req(req);
	}
	return 0;
}

static int port_ipc_kernel_init(struct ccci_port *port)
{
	struct ccci_ipc_ctrl *ipc_ctrl;
	
	CCCI_DBG_MSG(port->modem->index, IPC, "IPC kernel port %s is initializing\n", port->name);
	port->private_data = kthread_run(port_ipc_kernel_thread, port, "%s", port->name);
	port->rx_length_th = MAX_QUEUE_LENGTH;
	
	port_ipc_init(port);
	ipc_ctrl = (struct ccci_ipc_ctrl *)port->private_data;
	if(ipc_ctrl->task_id == AP_IPC_WMT) {
		#ifdef FEATURE_CONN_MD_EXP_EN
		CONN_MD_BRIDGE_OPS ccci_ipc_conn_ops = {.rx_cb = port_ipc_kernel_write};
		mtk_conn_md_bridge_reg(MD_MOD_EL1, &ccci_ipc_conn_ops);
		#endif
	}
	return 0;
}

struct ccci_port_ops ipc_kern_port_ops = {
	.init = &port_ipc_kernel_init,
	.recv_request = &port_ipc_kernel_recv_req,
	.req_match = &port_ipc_req_match,
	.md_state_notice = &port_ipc_md_state_notice,
};
