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

/*****************************************************************************
 *
 *Filename:
 *---------
 *ccci_ipc.c
 *
 *
 *Author:
 *-------
 *
 ****************************************************************************/
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <ccci.h>

#define local_AP_id_2_unify_id(id) local_xx_id_2_unify_id(id, 1)
#define local_MD_id_2_unify_id(id) local_xx_id_2_unify_id(id, 0)
#define unify_AP_id_2_local_id(id)   unify_xx_id_2_local_id(id, 1)
#define unify_MD_id_2_local_id(id)   unify_xx_id_2_local_id(id, 0)

struct ipc_ctl_block_t {
	int m_md_id;
	spinlock_t ccci_ipc_wr_lock;
	spinlock_t ccci_ipc_rd_lock;
	CCCI_IPC_MEM *ipc_mem;
	unsigned int ccci_ipc_smem_base_phy;
	int ccci_ipc_smem_size;
	unsigned int ccci_ipc_wr_buffer_phy;
	unsigned int ccci_ipc_rd_buffer_phy;
	struct cdev ccci_ipc_cdev;
	wait_queue_head_t poll_md_queue_head;
	int md_is_ready;
	IPC_TASK ipc_task[MAX_NUM_IPC_TASKS];
	struct MD_CALL_BACK_QUEUE md_status_update_call_back;
	int major;
	int start_minor;
};

static struct ipc_ctl_block_t *ipc_ctl_block[MAX_MD_NUM];

static void release_recv_item(CCCI_RECV_ITEM *item);
static void ipc_call_back_func(struct MD_CALL_BACK_QUEUE *, unsigned long);

static void ipc_smem_init(CCCI_IPC_MEM *ipc_mem)
{
	int i;

	ipc_mem->buffer.buff_wr.size = CCCI_IPC_BUFFER_SIZE;
	ipc_mem->buffer.buff_wr.rx_offset = 0;
	ipc_mem->buffer.buff_wr.tx_offset = 0;
	ipc_mem->buffer.buff_rd.size = CCCI_IPC_BUFFER_SIZE;
	ipc_mem->buffer.buff_rd.rx_offset = 0;
	ipc_mem->buffer.buff_rd.tx_offset = 0;

	for (i = 0; i < MAX_NUM_IPC_TASKS; i++) {
		(ipc_mem->ilm + i)->src_mod_id = -1UL;
		(ipc_mem->ilm + i)->dest_mod_id = -1UL;
		(ipc_mem->ilm + i)->sap_id = -1UL;
		(ipc_mem->ilm + i)->msg_id = -1UL;
		(ipc_mem->ilm + i)->local_para_ptr = NULL;
		(ipc_mem->ilm + i)->local_para_ptr = NULL;
	}
}

int ccci_ipc_ipo_h_restore(int md_id)
{
	struct ipc_ctl_block_t *ctl_b;

	ctl_b = ipc_ctl_block[md_id];
	ipc_smem_init(ctl_b->ipc_mem);
	return 0;
}

static void ipc_call_back_func(struct MD_CALL_BACK_QUEUE *queue,
			       unsigned long data)
{
	IPC_TASK *tsk;
	int i;
	CCCI_RECV_ITEM *item, *n;
	struct ipc_ctl_block_t *ctl_b =
	    container_of(queue, struct ipc_ctl_block_t,
			 md_status_update_call_back);
	unsigned long flags;

	switch (data) {
	case CCCI_MD_EXCEPTION:
		ctl_b->md_is_ready = 0;
		CCCI_DBG_MSG(ctl_b->m_md_id, "ipc",
			     "MD exception call chain !\n");
		break;

	case CCCI_MD_RESET:
		/*if (ctl_b->md_is_ready) */
		{
			ctl_b->md_is_ready = 0;
			CCCI_DBG_MSG(ctl_b->m_md_id, "ipc",
				     "MD reset call chain !\n");
			for (i = 0; i < MAX_NUM_IPC_TASKS; i++) {
				tsk = ctl_b->ipc_task + i;
				spin_lock_irqsave(&tsk->lock, flags);
				list_for_each_entry_safe(item, n,
							 &tsk->recv_list,
							 list) {
					release_recv_item(item);
				}
				spin_unlock_irqrestore(&tsk->lock, flags);
				/* __wake_up(&tsk->write_wait_queue, TASK_NORMAL, 0, (void*)POLLERR); */
				/* __wake_up(&tsk->read_wait_queue, TASK_NORMAL, 0, (void*)POLLERR); */
			}
			spin_lock_irqsave(&ctl_b->ccci_ipc_wr_lock, flags);
			ctl_b->ipc_mem->buffer.buff_wr.tx_offset = 0;
			ctl_b->ipc_mem->buffer.buff_wr.rx_offset = 0;
			spin_unlock_irqrestore(&ctl_b->ccci_ipc_wr_lock, flags);

			spin_lock_irqsave(&ctl_b->ccci_ipc_rd_lock, flags);
			ctl_b->ipc_mem->buffer.buff_rd.tx_offset = 0;
			ctl_b->ipc_mem->buffer.buff_rd.rx_offset = 0;
			spin_unlock_irqrestore(&ctl_b->ccci_ipc_rd_lock, flags);

		}
		break;

	case CCCI_MD_BOOTUP:
		ctl_b->md_is_ready = 1;
		wake_up_all(&ctl_b->poll_md_queue_head);
		CCCI_IPC_MSG(ctl_b->m_md_id, "MD boot up successfully.\n");
		break;

	}

}

static IPC_MSGSVC_TASKMAP_T ipc_msgsvc_maptbl[] = {
#define __IPC_ID_TABLE
#include "ccci_ipc_task_ID.h"
#undef __IPC_ID_TABLE
};

void find_task_to_clear(IPC_TASK task_table[], unsigned int to_id)
{
	IPC_TASK *task = NULL;
	int i, tmp;
	struct ipc_ctl_block_t *ctl_b =
	    (struct ipc_ctl_block_t
	     *)(container_of(task_table, struct ipc_ctl_block_t, ipc_task[0]));

	for (i = 0; i < MAX_NUM_IPC_TASKS; i++) {
		if (task_table[i].to_id == to_id) {
			CCCI_DBG_MSG(ctl_b->m_md_id, "ipc",
				     "%s: task->to_id(%d:%d)\n", __func__,
				     i, task_table[i].to_id);

			if (task == NULL) {
				task = ctl_b->ipc_task + i;
				tmp = i;
				continue;
			}
			if (time_after(task->w_jiffies, task_table[i].w_jiffies)) {
				task = task_table + i;
				CCCI_DBG_MSG(ctl_b->m_md_id, "ipc",
					     "%s: select task->to_id(%d:%d)\n",
					     __func__, i, task_table[i].to_id);
			} else if (task->w_jiffies == task_table[i].w_jiffies) {
				CCCI_DBG_MSG(ctl_b->m_md_id, "ipc",
					     "[Error]Wrong time stamp(%ld, %ld), select task->to_id(%d:%d)\n",
					     task->w_jiffies,
					     task_table[i].w_jiffies, tmp,
					     task->to_id);
			}
		}
	}

	if (task == NULL) {
		CCCI_MSG_INF(ctl_b->m_md_id, "ipc",
			     "Wrong MD ID(%d) to clear for next recv.\n",
			     to_id);
		return;
	}
	CCCI_IPC_MSG(ctl_b->m_md_id, "wake up task:%d\n",
		     task - ctl_b->ipc_task);
	clear_bit(CCCI_TASK_PENDING, &task->flag);
	wake_up_poll(&task->write_wait_queue, POLLOUT);
}

static IPC_MSGSVC_TASKMAP_T *local_xx_id_2_unify_id(uint32 local_id, int AP)
{
	int i;

	for (i = 0;
	     i < sizeof(ipc_msgsvc_maptbl) / sizeof(ipc_msgsvc_maptbl[0]);
	     i++) {
		if (ipc_msgsvc_maptbl[i].task_id == local_id
		    && (AP ? (ipc_msgsvc_maptbl[i].extq_id & AP_UNIFY_ID_FLAG) :
			!(ipc_msgsvc_maptbl[i].extq_id & AP_UNIFY_ID_FLAG)))

			return ipc_msgsvc_maptbl + i;

	}
	return NULL;
}

static IPC_MSGSVC_TASKMAP_T *unify_xx_id_2_local_id(uint32 unify_id, int AP)
{
	int i;

	if (!
	    (AP ? (unify_id & AP_UNIFY_ID_FLAG) :
	     !(unify_id & AP_UNIFY_ID_FLAG)))
		return NULL;

	for (i = 0;
	     i < sizeof(ipc_msgsvc_maptbl) / sizeof(ipc_msgsvc_maptbl[0]);
	     i++) {
		if (ipc_msgsvc_maptbl[i].extq_id == unify_id)
			return ipc_msgsvc_maptbl + i;

	}
	return NULL;
}

static int ccci_ipc_write_stream(int md_id, int channel, int addr, int len,
				 uint32 reserved)
{
	struct ccci_msg_t msg;

	msg.addr = addr;
	msg.len = len;
	msg.channel = channel;
	msg.reserved = reserved;
	CCCI_IPC_MSG(md_id, "write to task:%d addr:%#x len:%d.\n", reserved,
		     addr, len);
	return ccci_message_send(md_id, &msg, 1);
}

static int ccci_ipc_ack(int md_id, int channel, int id, uint32 reserved)
{
	struct ccci_msg_t msg;

	msg.magic = 0xFFFFFFFF;
	msg.id = id;
	msg.channel = channel;
	msg.reserved = reserved;
	return ccci_message_send(md_id, &msg, 1);
}

static void release_recv_item(CCCI_RECV_ITEM *item)
{
	if (item) {
		if (!list_empty(&item->list))
			list_del_init(&item->list);
		kfree(item->data);
		kfree(item);
	}
}

void *read_from_ring_buffer(int md_id, ipc_ilm_t *ilm, BUFF *buff_rd,
			    int *len)
{
	int size;
	int write;
	int read;
	int data_size;
	uint8 *data;
	void *ret = NULL;
	int over = 0;
	int copy = 0;
	int real_size = 0;
	unsigned long flag;
	struct ipc_ctl_block_t *ctl_b = ipc_ctl_block[md_id];

	spin_lock_irqsave(&ctl_b->ccci_ipc_rd_lock, flag);
	size = buff_rd->size;
	write = buff_rd->tx_offset;
	read = buff_rd->rx_offset;
	data_size =
	    (write - read) >= 0 ? (write - read) : (size - (read - write));

	if (data_size == 0)
		CCCI_IPC_MSG(md_id, "data_size=0, read(%d)", read);
	else if (data_size < 0) {
		CCCI_MSG_INF(md_id, "ipc", "[Error]wrong data_size: %d",
			     data_size);
		return NULL;
	}

	CCCI_IPC_MSG(md_id, "tx_offset=%d, rx_offset=%d\n", write, read);

	data = kmalloc(data_size + sizeof(ipc_ilm_t), GFP_ATOMIC);
	if (data == NULL) {
		CCCI_MSG_INF(md_id, "ipc", "kmalloc for read ilm fail!\n");
		ret = NULL;
		goto out;
	}

	*((ipc_ilm_t *) data) = *ilm;
	ilm = (ipc_ilm_t *) data;
	data += sizeof(ipc_ilm_t);

	if (write < read)
		over = size - read;

	if (over) {
		if (data_size < over)
			over = data_size;
		memcpy(data, buff_rd->buffer + read, over);
		copy += over;
		read = (read + over) & (size - 1);
	}

	if (copy < data_size)
		memcpy(data + copy, buff_rd->buffer + read, data_size - copy);
	real_size +=
	    (ilm->local_para_ptr) ? ((local_para_struct *) data)->msg_len : 0;
	data += real_size;
	real_size +=
	    (ilm->peer_buff_ptr) ? ((peer_buff_struct *) data)->pdu_len : 0;

	buff_rd->rx_offset += real_size;
	buff_rd->rx_offset &= size - 1;
	ret = ilm;
	*len = real_size + sizeof(ipc_ilm_t);

	if (real_size > data_size)
		CCCI_MSG_INF(md_id, "ipc",
			     "[Error]wrong real_size(%d)>data_size(%d)",
			     real_size, data_size);

 out:
	spin_unlock_irqrestore(&ctl_b->ccci_ipc_rd_lock, flag);

	CCCI_IPC_MSG(md_id, "recv real_size=%08x data_size=%08x\n", real_size,
		     data_size);

	return ret;
}

static void recv_item(int md_id, unsigned int addr, unsigned int len,
		      IPC_TASK *task, BUFF *buff_rd)
{
	struct ipc_ctl_block_t *ctl_b = ipc_ctl_block[md_id];
	ipc_ilm_t *ilm =
	    (ipc_ilm_t *) ((uint32) ctl_b->ipc_mem +
			   (addr - ctl_b->ccci_ipc_smem_base_phy +
			    get_md2_ap_phy_addr_fixed()));
	CCCI_RECV_ITEM *item;
	unsigned long flags;

	if (len != sizeof(ipc_ilm_t))
		CCCI_MSG_INF(md_id, "ipc",
			     "[Error]Wrong msg len: sizeof(ipc_ilm_t)=%d,len=%d\n",
			     sizeof(ipc_ilm_t), len);

	CCCI_IPC_MSG(md_id,
		     "Recv item Physical_Addr:%x Virtual_Addr:%p Len:%d.\n",
		     addr, ilm, len);

	if (addr >
	    ctl_b->ccci_ipc_smem_base_phy - get_md2_ap_phy_addr_fixed() +
	    offset_of(CCCI_IPC_MEM,
		      ilm_md) + sizeof(ipc_ilm_t) * MAX_NUM_IPC_TASKS_MD) {
		CCCI_MSG_INF(md_id, "ipc",
			     "[Error]Wrong physical address(%x)\n", addr);
		return;
	}

	item = kmalloc(sizeof(CCCI_RECV_ITEM), GFP_ATOMIC);
	if (item == NULL) {
		CCCI_MSG_INF(md_id, "ipc", "kmalloc for recv_item fail!\n");
		goto out;
	}

	if (ilm->local_para_ptr) {
		if ((uint32) ilm->local_para_ptr <
		    (uint32) ctl_b->ccci_ipc_rd_buffer_phy
		    || (uint32) ilm->local_para_ptr >=
		    (uint32) ctl_b->ccci_ipc_rd_buffer_phy +
		    CCCI_IPC_BUFFER_SIZE)
			CCCI_MSG_INF(md_id, "ipc",
				     "[Error]wrong ilm->local_para_ptr address(%p)",
				     ilm->local_para_ptr);
	}

	if (ilm->peer_buff_ptr) {
		if ((uint32) ilm->peer_buff_ptr <
		    (uint32) ctl_b->ccci_ipc_rd_buffer_phy
		    || (uint32) ilm->peer_buff_ptr >=
		    (uint32) ctl_b->ccci_ipc_rd_buffer_phy +
		    CCCI_IPC_BUFFER_SIZE)
			CCCI_MSG_INF(md_id, "ipc",
				     "[Error]wrong ilm->peer_buff_ptr address(%p)",
				     ilm->peer_buff_ptr);
	}
	CCCI_IPC_MSG(md_id,
		     "recv ilm->local_para_ptr(%p), ilm->peer_buff_ptr(%p)\n",
		     ilm->local_para_ptr, ilm->peer_buff_ptr);

	INIT_LIST_HEAD(&item->list);
	item->data =
	    (uint8 *) read_from_ring_buffer(md_id, ilm, buff_rd, &item->len);
	if (item->data == NULL) {
		CCCI_MSG_INF(md_id, "ipc", "read ipc rx data fail\n");
		goto out1;
	}

	spin_lock_irqsave(&task->lock, flags);
	list_add_tail(&item->list, &task->recv_list);
	spin_unlock_irqrestore(&task->lock, flags);

	kill_fasync(&task->fasync, SIGIO, POLL_IN);
	wake_up_poll(&task->read_wait_queue, POLLIN);
	goto out;

 out1:
	kfree(item);
 out:
	return;
}

static int write_to_ring_buffer(int md_id, uint8 *data, int count,
				IPC_TASK *task, BUFF *ipc_buffer)
{
	int ret = 0;
	int free;
	int write, read, over, copy;
	int size;
	int write_begin;
	unsigned long flags;
	ipc_ilm_t *ilm = task->ilm_p;
	local_para_struct *local_para =
	    ilm->local_para_ptr ? (local_para_struct *) data : NULL;
	peer_buff_struct *peer_buff =
	    ilm->peer_buff_ptr ? (peer_buff_struct *) ((uint32) data +
						       (local_para ?
							local_para->msg_len :
							0)) : NULL;
	struct ipc_ctl_block_t *ctl_b = ipc_ctl_block[md_id];

	CCCI_IPC_MSG(md_id,
		     "local_para_struct addr=%p peer_buff_struct addr=%p\n",
		     local_para, peer_buff);
	if ((local_para ? local_para->msg_len : 0) +
	    (peer_buff ? peer_buff->pdu_len : 0) != count) {
		CCCI_MSG_INF(md_id, "ipc",
			     "[Error]Count is not equal(%x != %x ) !\n",
			     (local_para ? local_para->msg_len : 0) +
			     (peer_buff ? peer_buff->pdu_len : 0), count);
		return -EINVAL;
	}

	if ((local_para ? local_para->ref_count != 1 : 0)
	    || (peer_buff ? peer_buff->ref_count != 1 : 0)) {
		CCCI_MSG_INF(md_id, "ipc", "[Error]ref count !=1 .\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&ctl_b->ccci_ipc_wr_lock, flags);

	write_begin = write = ipc_buffer->tx_offset;
	read = ipc_buffer->rx_offset;
	size = ipc_buffer->size;
	copy = 0;

	if (read < write) {
		free = size - (write - read);
		over = size - write;
	} else if (read == write) {
		free = size - 1;
		over = size - write;
	} else {
		free = read - write - 1;
		over = 0;
	}

	if (count > free) {
		CCCI_MSG_INF(md_id, "ipc",
			     "[Error]memory isn't enough, data_len(%d)>free_len(%d, %d, %d)\n",
			     count, free, write, read);
		ret = -E2BIG;
		goto out;
	}

	if (over) {
		if (count < over)
			over = count;
		memcpy(ipc_buffer->buffer + write, data, over);
		copy += over;
		write = (write + over) & (size - 1);
		data += copy;
	}

	if (copy < count)
		memcpy(ipc_buffer->buffer + write, data, count - copy);

	mb(); /* wait mem upated*/

	ipc_buffer->tx_offset += count;
	ipc_buffer->tx_offset &= size - 1;
	ret = count;
	ilm->local_para_ptr =
	    local_para ? (local_para_struct *) (ctl_b->ccci_ipc_wr_buffer_phy +
						write_begin) : NULL;
	ilm->peer_buff_ptr =
	    peer_buff ? (peer_buff_struct *) (ctl_b->ccci_ipc_wr_buffer_phy +
					      ((write_begin +
						(local_para ?
						 local_para->msg_len : 0)) &
					       (size - 1))) : NULL;

 out:
	spin_unlock_irqrestore(&ctl_b->ccci_ipc_wr_lock, flags);
	return ret;
}

static void ccci_ipc_callback(void *private)
{
	IPC_TASK *task;
	IPC_MSGSVC_TASKMAP_T *id_map;
	struct logic_channel_info_t *ch_info =
	    (struct logic_channel_info_t *)private;
	struct ccci_msg_t msg;
	struct ipc_ctl_block_t *ctl_b =
	    (struct ipc_ctl_block_t *)ch_info->m_owner;
	int md_id = ctl_b->m_md_id;

	while (get_logic_ch_data(ch_info, &msg)) {
		if (msg.channel == CCCI_IPC_RX_ACK
		    || msg.channel == CCCI_IPC_TX) {
			CCCI_MSG_INF(md_id, "ipc",
				     "[Error]invalid ipc rx channel(%d)!\n",
				     msg.channel);
		}

		if (msg.channel == CCCI_IPC_RX) {
			CCCI_IPC_MSG(md_id, "CCCI_IPC_RX:Unify AP id(%x)\n",
				     msg.reserved);
			id_map = unify_AP_id_2_local_id(msg.reserved);
			if (id_map == NULL) {
				CCCI_MSG_INF(md_id, "ipc",
					     "[Error]Wrong Unify AP id(%x)@RX\n",
					     msg.reserved);
				return;
			}

			task =
			    ((struct ipc_ctl_block_t *)(ch_info->m_owner))->
			    ipc_task + id_map->task_id;
			recv_item(md_id, msg.addr, msg.len, task,
				  &ctl_b->ipc_mem->buffer.buff_rd);
			ccci_ipc_ack(md_id, CCCI_IPC_RX_ACK,
				     IPC_MSGSVC_RVC_DONE, msg.reserved);
		}

		if (msg.channel == CCCI_IPC_TX_ACK) {
			CCCI_IPC_MSG(md_id,
				     "CCCI_IPC_TX_ACK: Unify MD ID(%x)\n",
				     msg.reserved);
			id_map = unify_MD_id_2_local_id(msg.reserved);
			if (id_map == NULL) {
				CCCI_MSG_INF(md_id, "ipc",
					     "[Error]Wrong AP Unify id (%d)@Tx ack.\n",
					     msg.reserved);
				return;
			}

			find_task_to_clear(ctl_b->ipc_task, id_map->task_id);

			if (msg.id != IPC_MSGSVC_RVC_DONE)
				CCCI_MSG_INF(md_id, "ipc",
					     "[Error]Not write mailbox id: %d\n",
					     msg.id);
		}
	}
}

static void ipc_task_init(int md_id, IPC_TASK *task, ipc_ilm_t *ilm)
{
	struct ipc_ctl_block_t *ctl_b = ipc_ctl_block[md_id];

	spin_lock_init(&task->lock);
	task->flag = 0;
	task->user = (atomic_t) ATOMIC_INIT(0);
	task->w_jiffies = -1UL;
	task->fasync = NULL;
	task->ilm_p = ilm;
	task->time_out = -1;
	task->ilm_phy_addr =
	    ctl_b->ccci_ipc_smem_base_phy - get_md2_ap_phy_addr_fixed()
	    + offset_of(CCCI_IPC_MEM,
			ilm) + (uint32) ilm - (uint32) (ctl_b->ipc_mem->ilm);
	task->to_id = -1;

	init_waitqueue_head(&task->read_wait_queue);
	init_waitqueue_head(&task->write_wait_queue);
	INIT_LIST_HEAD(&task->recv_list);
	task->owner = ipc_ctl_block[md_id];
}

static int ccci_ipc_open(struct inode *inode, struct file *file)
{
	int md_id;
	int major;
	int index;
	struct ipc_ctl_block_t *ctl_b;

	major = imajor(inode);
	md_id = get_md_id_by_dev_major(major);
	if (md_id < 0) {
		CCCI_MSG("IPC open fail: invalid major id:%d\n", major);
		return -1;
	}

	ctl_b = ipc_ctl_block[md_id];
	index = iminor(inode) - ctl_b->start_minor;
	if (index >= MAX_NUM_IPC_TASKS) {
		CCCI_MSG_INF(md_id, "ipc", "[Error]Wrong minor num %d.\n",
			     index);
		return -EINVAL;
	}
	CCCI_DBG_MSG(md_id, "ipc", "%s: register task%d\n", __func__, index);
	nonseekable_open(inode, file);
	file->private_data = ctl_b->ipc_task + index;
	atomic_inc(&((ctl_b->ipc_task + index)->user));
	return 0;

}

static ssize_t ccci_ipc_read(struct file *file, char *buf, size_t count,
			     loff_t *ppos)
{
	int ret = 0;
	IPC_TASK *task = file->private_data;
	CCCI_RECV_ITEM *recv_data;
	struct ipc_ctl_block_t *ctl_b;
	unsigned long flags;

	ctl_b = (struct ipc_ctl_block_t *)task->owner;

 retry:
	spin_lock_irqsave(&task->lock, flags);
	if (ctl_b->md_is_ready == 0) {
		ret = -EIO;
		goto out_unlock;
	}
	if (list_empty(&task->recv_list)) {
		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto out_unlock;
		}
		spin_unlock_irqrestore(&task->lock, flags);
		ret = wait_event_interruptible(task->read_wait_queue, !list_empty(&task->recv_list));
		if (ret == -ERESTARTSYS) {
			CCCI_IPC_MSG(ctl_b->m_md_id,
				     "Interrupt read sys_call : task:%s pid:%d tgid:%d SIGPEND:%#llx. GROUP_SIGPEND:%#llx .\n",
				     current->comm, current->pid, current->tgid,
				     *(unsigned long long *)current->pending.
				     signal.sig,
				     *(unsigned long long *)current->signal->
				     shared_pending.signal.sig);
			ret = -EINTR;
			goto out;
		}
		goto retry;
	}
	recv_data = container_of(task->recv_list.next, CCCI_RECV_ITEM, list);

	if (recv_data->len > count) {
		CCCI_MSG_INF(ctl_b->m_md_id, "ipc",
			     "[Error]Recv buff is too small(count=%d data_len=%d)!\n",
			     count, recv_data->len);
		ret = -E2BIG;
		goto out_unlock;
	}
	list_del_init(&recv_data->list);
	spin_unlock_irqrestore(&task->lock, flags);

	if (copy_to_user(buf, recv_data->data, recv_data->len)) {
		ret = -EFAULT;
		release_recv_item(recv_data);
		goto out;
	}
	ret = recv_data->len;
	release_recv_item(recv_data);
	goto out;

 out_unlock:
	spin_unlock_irqrestore(&task->lock, flags);

 out:
	return ret;
}

static ssize_t ccci_ipc_write(struct file *file, const char __user *buf,
			      size_t count, loff_t *ppos)
{
	int ret = 0;
	IPC_TASK *task = file->private_data;
	struct ipc_ctl_block_t *ctl_b;
	IPC_MSGSVC_TASKMAP_T *id_map;
	ipc_ilm_t *ilm = NULL;
	int md_id;

	ctl_b = (struct ipc_ctl_block_t *)task->owner;
	md_id = ctl_b->m_md_id;

	if (count < sizeof(ipc_ilm_t)) {
		CCCI_MSG_INF(md_id, "ipc",
			     "%s: [Error]Write len(%d) < ipc_ilm_t\n",
			     __func__, count);
		ret = -EINVAL;
		goto out;
	}

	ilm = kmalloc(count, GFP_KERNEL);
	if (ilm == NULL) {
		CCCI_MSG_INF(md_id, "ipc", "%s: kmalloc fail!\n", __func__);
		ret = -ENOMEM;
		goto out;
	}
	if (copy_from_user(ilm, buf, count)) {
		CCCI_MSG_INF(md_id, "ipc", "%s: copy_from_user fail!\n",
			     __func__);
		ret = -EFAULT;
		goto out_free;
	}
	id_map = local_MD_id_2_unify_id(ilm->dest_mod_id);
	if (id_map == NULL) {
		CCCI_MSG_INF(md_id, "ipc",
			     "%s: [Error]Invalid Dest MD id (%d)\n",
			     __func__, ilm->dest_mod_id);
		ret = -EINVAL;
		goto out_free;
	}

	if (test_and_set_bit(CCCI_TASK_PENDING, &task->flag)) {
		CCCI_IPC_MSG(md_id, "write is busy. Task ID=%d.\n",
			     task - ctl_b->ipc_task);
		if (file->f_flags & O_NONBLOCK) {
			ret = -EBUSY;
			goto out_free;
		} else
		    if (wait_event_interruptible_exclusive
			(task->write_wait_queue,
			 !test_and_set_bit(CCCI_TASK_PENDING, &task->flag)
			 || ctl_b->md_is_ready == 0) == -ERESTARTSYS) {
			ret = -EINTR;
			goto out_free;
		}
	}

	spin_lock_irq(&task->lock);
	if (ctl_b->md_is_ready == 0) {
		ret = -EIO;
		spin_unlock_irq(&task->lock);
		goto out_free;
	}
	spin_unlock_irq(&task->lock);
	task->w_jiffies = get_jiffies_64();
	*task->ilm_p = *ilm;
	task->to_id = ilm->dest_mod_id;
	task->ilm_p->src_mod_id = task - ctl_b->ipc_task;

	CCCI_DBG_MSG(md_id, "ipc", "%s: src=%d, dst=%d, data_len=%d\n",
		     __func__, task->ilm_p->src_mod_id, task->to_id, count);

	if (count > sizeof(ipc_ilm_t)) {
		if (write_to_ring_buffer
		    (md_id, (uint8 *) (ilm + 1), count - sizeof(ipc_ilm_t),
		     task,
		     &ctl_b->ipc_mem->buffer.buff_wr) !=
		    count - sizeof(ipc_ilm_t)) {
			CCCI_MSG_INF(md_id, "ipc",
				     "[Error]write_to_ring_buffer fail!\n");
			clear_bit(CCCI_TASK_PENDING, &task->flag);
			ret = -EAGAIN;
			goto out_free;
		}
	}

	ret =
	    ccci_ipc_write_stream(md_id, CCCI_IPC_TX, task->ilm_phy_addr,
				  sizeof(ipc_ilm_t), id_map->extq_id);
	if (ret != sizeof(struct ccci_msg_t)) {
		CCCI_MSG_INF(md_id, "ipc",
			     "%s: ccci_ipc_write_stream fail: %d\n",
			     __func__, ret);
		clear_bit(CCCI_TASK_PENDING, &task->flag);
		ret = -EAGAIN;
		goto out_free;
	}

 out_free:
	kfree(ilm);

 out:
	return ret == sizeof(struct ccci_msg_t) ? count : ret;
}

static long ccci_ipc_ioctl(struct file *file, unsigned int cmd,
			   unsigned long arg)
{
	IPC_TASK *task = file->private_data;
	CCCI_RECV_ITEM *item, *n;
	unsigned long flags;
	long ret = 0;
	struct ipc_ctl_block_t *ctl_b;

	ctl_b = (struct ipc_ctl_block_t *)task->owner;

	switch (cmd) {
	case CCCI_IPC_RESET_RECV:
		spin_lock_irqsave(&task->lock, flags);
		list_for_each_entry_safe(item, n, &task->recv_list, list) {
			release_recv_item(item);
		}
		spin_unlock_irqrestore(&task->lock, flags);
		ret = 0;
		break;

	case CCCI_IPC_RESET_SEND:
		clear_bit(CCCI_TASK_PENDING, &task->flag);
		wake_up(&task->write_wait_queue);
		break;

	case CCCI_IPC_WAIT_MD_READY:
		if (ctl_b->md_is_ready == 0) {
			ret = wait_event_interruptible(ctl_b->poll_md_queue_head, !ctl_b->md_is_ready);
			if (ret == -ERESTARTSYS) {
				CCCI_MSG_INF(ctl_b->m_md_id, "ipc",
					     "Got signal @ WAIT_MD_READY\n");
				ret = -EINTR;
			}
		}
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ccci_ipc_release(struct inode *inode, struct file *file)
{
	CCCI_RECV_ITEM *item, *n;
	IPC_TASK *task = file->private_data;
	unsigned long flags;

	if (atomic_dec_and_test(&task->user)) {
		spin_lock_irqsave(&task->lock, flags);
		list_for_each_entry_safe(item, n, &task->recv_list, list) {
			release_recv_item(item);
		}
		spin_unlock_irqrestore(&task->lock, flags);
	}
	clear_bit(CCCI_TASK_PENDING, &task->flag);
	CCCI_DBG_MSG(0, "ipc", "%s\n", __func__);

	return 0;
}

static int ccci_ipc_fasync(int fd, struct file *file, int on)
{
	IPC_TASK *task = file->private_data;

	return fasync_helper(fd, file, on, &task->fasync);
}

static uint32 ccci_ipc_poll(struct file *file, poll_table *wait)
{
	IPC_TASK *task = file->private_data;
	int ret = 0;
	struct ipc_ctl_block_t *ctl_b;

	ctl_b = (struct ipc_ctl_block_t *)task->owner;
	poll_wait(file, &task->read_wait_queue, wait);
	poll_wait(file, &task->write_wait_queue, wait);
	spin_lock_irq(&task->lock);

	if (ctl_b->md_is_ready == 0) {
		/*ret |= POLLERR;  */
		goto out;
	}

	if (!list_empty(&task->recv_list))
		ret |= POLLIN | POLLRDNORM;

	if (!test_bit(CCCI_TASK_PENDING, &task->flag))
		ret |= POLLOUT | POLLWRNORM;

 out:
	spin_unlock_irq(&task->lock);
	return ret;
}

static const struct file_operations ccci_ipc_fops = {
	.owner = THIS_MODULE,
	.open = ccci_ipc_open,
	.read = ccci_ipc_read,
	.write = ccci_ipc_write,
	.release = ccci_ipc_release,
	.unlocked_ioctl = ccci_ipc_ioctl,
	.fasync = ccci_ipc_fasync,
	.poll = ccci_ipc_poll,
};

int __init ccci_ipc_init(int md_id)
{
	int ret = 0;
	int i = 0;
	int major, minor;
	char buf[16];
	struct ipc_ctl_block_t *ctl_b;

	ret = get_dev_id_by_md_id(md_id, "ipc", &major, &minor);
	if (ret < 0) {
		CCCI_MSG("ccci_ipc_init: get md device number failed(%d)\n",
			 ret);
		return ret;
	}
	/*Allocate ipc ctrl struct memory */
	ctl_b = kmalloc(sizeof(struct ipc_ctl_block_t), GFP_KERNEL);
	if (ctl_b == NULL)
		return -CCCI_ERR_GET_MEM_FAIL;
	memset(ctl_b, 0, sizeof(struct ipc_ctl_block_t));

	ipc_ctl_block[md_id] = ctl_b;
	spin_lock_init(&ctl_b->ccci_ipc_wr_lock);
	spin_lock_init(&ctl_b->ccci_ipc_rd_lock);
	init_waitqueue_head(&ctl_b->poll_md_queue_head);
	ctl_b->md_status_update_call_back.call = ipc_call_back_func;
	ctl_b->md_status_update_call_back.next = NULL, ctl_b->m_md_id = md_id;
	ctl_b->major = major;
	ctl_b->start_minor = minor;
	ccci_ipc_base_req(md_id, (int *)(&ctl_b->ipc_mem),
			  &ctl_b->ccci_ipc_smem_base_phy,
			  &ctl_b->ccci_ipc_smem_size);

	ctl_b->ccci_ipc_wr_buffer_phy =
	    ctl_b->ccci_ipc_smem_base_phy - get_md2_ap_phy_addr_fixed()
	    + offset_of(CCCI_IPC_MEM, buffer.buff_wr.buffer);
	ctl_b->ccci_ipc_rd_buffer_phy =
	    ctl_b->ccci_ipc_smem_base_phy - get_md2_ap_phy_addr_fixed()
	    + offset_of(CCCI_IPC_MEM, buffer.buff_rd.buffer);
	/*CCCI_MSG_INF(md_id, "ipc", "ccci_ipc_wr_buffer_phy: %#x, ccci_ipc_buffer_phy_rd: %#x.\n", */
	/*ctl_b->ccci_ipc_wr_buffer_phy, ctl_b->ccci_ipc_rd_buffer_phy); */

	ipc_smem_init(ctl_b->ipc_mem);

	for (i = 0; i < MAX_NUM_IPC_TASKS; i++) {
		ipc_task_init(md_id, ctl_b->ipc_task + i,
			      ctl_b->ipc_mem->ilm + i);
	}

	snprintf(buf, 16, "CCCI_IPC_DEV%d", md_id);
	if (register_chrdev_region(MKDEV(major, minor), MAX_NUM_IPC_TASKS, buf)
	    != 0) {
		CCCI_MSG_INF(md_id, "ipc", "Regsiter CCCI_IPC_DEV failed!\n");
		ret = -1;
		goto _IPC_MAPPING_FAIL;
	}

	cdev_init(&ctl_b->ccci_ipc_cdev, &ccci_ipc_fops);
	ctl_b->ccci_ipc_cdev.owner = THIS_MODULE;
	ret =
	    cdev_add(&ctl_b->ccci_ipc_cdev, MKDEV(major, minor),
		     MAX_NUM_IPC_TASKS);
	if (ret < 0) {
		CCCI_MSG_INF(md_id, "ipc", "cdev_add failed!\n");
		goto _CHR_DEV_ADD_FAIL;
	}

	if (register_to_logic_ch(md_id, CCCI_IPC_RX, ccci_ipc_callback, ctl_b)
	    || register_to_logic_ch(md_id, CCCI_IPC_TX_ACK, ccci_ipc_callback,
				    ctl_b)) {
		CCCI_MSG_INF(md_id, "ipc", "ccci_ipc_register failed!\n");
		ret = -1;
		goto _REG_LOGIC_CH_FAIL;
	}

	md_register_call_chain(md_id, &ctl_b->md_status_update_call_back);
	goto out;

 _REG_LOGIC_CH_FAIL:
	un_register_to_logic_ch(md_id, CCCI_IPC_RX);
	un_register_to_logic_ch(md_id, CCCI_IPC_TX_ACK);

 _CHR_DEV_ADD_FAIL:
	cdev_del(&ctl_b->ccci_ipc_cdev);

 _IPC_MAPPING_FAIL:
	kfree(ctl_b);
	ipc_ctl_block[md_id] = NULL;

 out:
	return ret;
}

void __exit ccci_ipc_exit(int md_id)
{
	int i;
	struct ipc_ctl_block_t *ctl_b = ipc_ctl_block[md_id];

	if (ctl_b == NULL)
		return;

	for (i = 0; i < MAX_NUM_IPC_TASKS; i++) {
		if (atomic_read(&ctl_b->ipc_task[i].user)) {
			CCCI_MSG_INF(md_id, "ipc",
				     "BUG for taskID %d module exit count.\n",
				     i);
		}
	}

	cdev_del(&ctl_b->ccci_ipc_cdev);
	unregister_chrdev_region(MKDEV(ctl_b->major, ctl_b->start_minor),
				 MAX_NUM_IPC_TASKS);
	md_unregister_call_chain(md_id, &ctl_b->md_status_update_call_back);
	un_register_to_logic_ch(md_id, CCCI_IPC_RX);
	un_register_to_logic_ch(md_id, CCCI_IPC_TX_ACK);

	kfree(ctl_b);
	ipc_ctl_block[md_id] = NULL;
}
