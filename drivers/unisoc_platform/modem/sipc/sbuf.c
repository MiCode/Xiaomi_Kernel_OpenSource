// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-sbuf: " fmt

#include <asm/pgtable.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/sipc.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <uapi/linux/sched/types.h>

#include "sbuf.h"
#include "sipc_priv.h"

#define VOLA_SBUF_SMEM volatile struct sbuf_smem_header
#define VOLA_SBUF_RING volatile struct sbuf_ring_header

struct name_node {
	struct	list_head list;
	char	comm[TASK_COMM_LEN];
	pid_t	pid;
	u8	latest;
};

union sbuf_buf {
	void *buf;
	void __user *ubuf;
};

enum task_type {
	TASK_RXWAIT = 0,
	TASK_TXWAIT,
	TASK_SELECT
};

#if defined(CONFIG_DEBUG_FS)
struct sbuf_device {
	int			major;
	int			minor;
	struct cdev		cdev;
	struct device		*sys_dev;	/* Device object in sysfs */
};

static struct class *sbuf_class;
static struct sbuf_device *sbuf_dev;
#endif

static struct sbuf_mgr *sbufs[SIPC_ID_NR][SMSG_VALID_CH_NR];

static bool sbuf_is_task_pointer(const void *ptr)
{
	struct task_struct *task;
	struct thread_info *thread_info;

	task = (struct task_struct *)ptr;
	if (IS_ERR_OR_NULL(task) || !virt_addr_valid(task))
		return false;

#ifndef CONFIG_THREAD_INFO_IN_TASK
	/* in this case thread_info is in the same addres with stack thread_union*/
	if (IS_ERR_OR_NULL(task->stack) || !virt_addr_valid(task->stack))
		return false;
#endif

	thread_info = task_thread_info(task);
	if (IS_ERR_OR_NULL(thread_info) || !virt_addr_valid(thread_info))
		return false;

	return true;
}

static struct task_struct *sbuf_wait_get_task(wait_queue_entry_t *pos, u32 *b_select)
{
	struct task_struct *task;
	struct poll_wqueues *table;

	if (!pos->private)
		return NULL;

	/* if the private is put into wait list by sbuf_read, the struct of
	 * pos->private is struct task_struct
	 * if the private is put into list by sbuf_poll, the struct of
	 * pos->private is struct poll_wqueues
	 */

	/* firstly, try struct poll_wqueues */
	table = (struct poll_wqueues *)pos->private;
	task = table->polling_task;
	if (sbuf_is_task_pointer(task)) {
		*b_select = 1;
		return task;
	}

	/* firstly, try convert it with the struct task_struct */
	task = (struct task_struct *)pos->private;

	if (sbuf_is_task_pointer(task)) {
		*b_select = 0;
		return task;
	}

	return NULL;
}

#if defined(CONFIG_DEBUG_FS)
static void sbuf_record_rdwt_owner(struct sbuf_ring *ring, int b_rx)
{
	int b_add;
	int cnt = 0;
	struct name_node *pos = NULL;
	struct name_node *temp = NULL;
	struct list_head *owner_list;
	unsigned long flags;

	b_add = 1;
	owner_list = b_rx ? (&ring->rx_list) : (&ring->tx_list);

	spin_lock_irqsave(&ring->rxwait.lock, flags);
	list_for_each_entry(pos, owner_list, list) {
		cnt++;
		if (pos->pid == current->pid) {
			b_add = 0;
			pos->latest = 1;
			continue;
		}
		if (pos->latest)
			pos->latest = 0;
	}
	spin_unlock_irqrestore(&ring->rxwait.lock, flags);

	if (b_add) {
		/* delete head next */
		if (cnt == MAX_RECORD_CNT) {
			temp = list_first_entry(owner_list,
				struct name_node, list);
			list_del(&temp->list);
			kfree(temp);
		}

		pos = kzalloc(sizeof(*pos), GFP_KERNEL);
		if (pos) {
			memcpy(pos->comm, current->comm, TASK_COMM_LEN);
			pos->pid = current->pid;
			pos->latest = 1;
			spin_lock_irqsave(&ring->rxwait.lock, flags);
			list_add_tail(&pos->list, owner_list);
			spin_unlock_irqrestore(&ring->rxwait.lock, flags);
		}
	}
}

static void sbuf_destroy_rdwt_owner(struct sbuf_ring *ring)
{
	struct name_node *pos, *temp;
	unsigned long flags;

	spin_lock_irqsave(&ring->rxwait.lock, flags);
	/* free task node */
	list_for_each_entry_safe(pos,
				 temp,
				 &ring->rx_list,
				 list) {
		list_del(&pos->list);
		kfree(pos);
	}

	list_for_each_entry_safe(pos,
				 temp,
				 &ring->tx_list,
				 list) {
		list_del(&pos->list);
		kfree(pos);
	}
	spin_unlock_irqrestore(&ring->rxwait.lock, flags);
}
#endif

static void sbuf_skip_old_data(struct sbuf_mgr *sbuf)
{
	struct sbuf_ring *ring = NULL;
	struct sbuf_ring_header_op *hd_op = NULL;
	u32 i;
	unsigned long flags;

	ring = &sbuf->rings[0];
	/* must reques resource before read or write share memory */
	if (sipc_smem_request_resource(ring->rx_pms, sbuf->dst, -1) < 0)
		return;

	for (i = 0; i < sbuf->ringnr; i++) {
		ring = &sbuf->rings[i];
		hd_op = &ring->header_op;

		/* clean sbuf tx ring , sbuf tx ring and channel 6-6 no need to clear */
		if (sbuf->dst == SIPC_ID_PM_SYS && sbuf->channel != SMSG_CH_TTY) {
			mutex_lock(&ring->txlock);
			*(hd_op->tx_wt_p) = *(hd_op->tx_rd_p);
			mutex_unlock(&ring->txlock);
		}
		/* clean sbuf rx ring */
		mutex_lock(&ring->rxlock);
		*(hd_op->rx_rd_p) = *(hd_op->rx_wt_p);
		mutex_unlock(&ring->rxlock);
		/* restore write mask. */
		spin_lock_irqsave(&ring->poll_lock, flags);
		ring->poll_mask = POLLOUT | POLLWRNORM;
		spin_unlock_irqrestore(&ring->poll_lock, flags);

	}
	ring = &sbuf->rings[0];
	/* release resource */
	sipc_smem_release_resource(ring->rx_pms, sbuf->dst);
}

static void sbuf_pms_init(struct sbuf_ring *ring,
			  uint8_t dst, uint8_t ch, int index)
{
	ring->need_wake_lock = true;
	sprintf(ring->tx_pms_name, "sbuf-%d-%d-%d-tx", dst, ch, index);
	ring->tx_pms = sprd_pms_create(dst, ring->tx_pms_name, false);
	if (!ring->tx_pms)
		pr_warn("create pms %s failed!\n", ring->tx_pms_name);

	sprintf(ring->rx_pms_name, "sbuf-%d-%d-%d-rx", dst, ch, index);
	ring->rx_pms = sprd_pms_create(dst, ring->rx_pms_name, false);
	if (!ring->rx_pms)
		pr_warn("create pms %s failed!\n", ring->rx_pms_name);
}

static void sbuf_comm_init(struct sbuf_mgr *sbuf)
{
	u32 bufnum = sbuf->ringnr;
	int i;
	struct sbuf_ring *ring;

	for (i = 0; i < bufnum; i++) {
		ring = &sbuf->rings[i];
		init_waitqueue_head(&ring->txwait);
		init_waitqueue_head(&ring->rxwait);
#if defined(CONFIG_DEBUG_FS)
		INIT_LIST_HEAD(&ring->tx_list);
		INIT_LIST_HEAD(&ring->rx_list);
#endif
		mutex_init(&ring->txlock);
		mutex_init(&ring->rxlock);
		spin_lock_init(&ring->poll_lock);

		/* init, set write mask. */
		ring->poll_mask = POLLOUT | POLLWRNORM;
		sbuf_pms_init(ring, sbuf->dst, sbuf->channel, i);
	}
}

static int sbuf_host_init(struct smsg_ipc *sipc, struct sbuf_mgr *sbuf,
	u32 bufnum, u32 txbufsize, u32 rxbufsize)
{
	VOLA_SBUF_SMEM *smem;
	VOLA_SBUF_RING *ringhd;
	struct sbuf_ring_header_op *hd_op;
	int hsize, i, rval;
	phys_addr_t offset = 0;
	u8 dst = sbuf->dst;
	struct sbuf_ring *ring;

	sbuf->ringnr = bufnum;

	/* allocate smem */
	hsize = sizeof(struct sbuf_smem_header) +
		sizeof(struct sbuf_ring_header) * bufnum;
	sbuf->smem_size = hsize + (txbufsize + rxbufsize) * bufnum;
	sbuf->smem_addr = smem_alloc_ex(dst, sbuf->smem, sbuf->smem_size);
	if (!sbuf->smem_addr) {
		pr_err("%s: channel %d-%d, Failed to allocate smem for sbuf\n",
			__func__, sbuf->dst, sbuf->channel);
		return -ENOMEM;
	}
	sbuf->dst_smem_addr = sbuf->smem_addr -
		sipc->smem_ptr[sbuf->smem].smem_base +
		sipc->smem_ptr[sbuf->smem].dst_smem_base;

	pr_debug("channel %d-%d, smem_addr=0x%x, smem_size=0x%x, dst_smem_addr=0x%x\n",
		 sbuf->dst,
		 sbuf->channel,
		 sbuf->smem_addr,
		 sbuf->smem_size,
		 sbuf->dst_smem_addr);

#ifdef CONFIG_PHYS_ADDR_T_64BIT
	offset = sipc->high_offset;
	offset = offset << 32;
#endif

	pr_info("channel %d-%d, offset = 0x%llx!\n",
		sbuf->dst, sbuf->channel, offset);
	sbuf->smem_virt = shmem_ram_vmap_nocache_ex(dst,
						sbuf->smem,
						sbuf->smem_addr + offset,
						sbuf->smem_size);
	if (!sbuf->smem_virt) {
		smem_free_ex(dst, sbuf->smem, sbuf->smem_addr, sbuf->smem_size);
		return -EFAULT;
	}

	/* allocate rings description */
	sbuf->rings = kcalloc(bufnum, sizeof(struct sbuf_ring), GFP_KERNEL);
	if (!sbuf->rings) {
		smem_free_ex(dst, sbuf->smem, sbuf->smem_addr, sbuf->smem_size);
		shmem_ram_unmap_ex(dst, sbuf->smem, sbuf->smem_virt);
		return -ENOMEM;
	}

	/* must request resource before read or write share memory */
	rval = sipc_smem_request_resource(sipc->sipc_pms, sipc->dst, -1);
	if (rval < 0) {
		smem_free(dst, sbuf->smem_addr, sbuf->smem_size);
		shmem_ram_unmap(dst, sbuf->smem_virt);
		kfree(sbuf->rings);
		return rval;
	}

	/* initialize all ring bufs */
	smem = (VOLA_SBUF_SMEM *)sbuf->smem_virt;
	smem->ringnr = bufnum;
	for (i = 0; i < bufnum; i++) {
		ringhd = (VOLA_SBUF_RING *)&smem->headers[i];
		ringhd->txbuf_addr = sbuf->dst_smem_addr + hsize +
			(txbufsize + rxbufsize) * i;
		ringhd->txbuf_size = txbufsize;
		ringhd->txbuf_rdptr = 0;
		ringhd->txbuf_wrptr = 0;
		ringhd->rxbuf_addr = smem->headers[i].txbuf_addr + txbufsize;
		ringhd->rxbuf_size = rxbufsize;
		ringhd->rxbuf_rdptr = 0;
		ringhd->rxbuf_wrptr = 0;

		ring = &sbuf->rings[i];
		ring->header = ringhd;
		ring->txbuf_virt = sbuf->smem_virt + hsize +
			(txbufsize + rxbufsize) * i;
		ring->rxbuf_virt = ring->txbuf_virt + txbufsize;
		/* init header op */
		hd_op = &ring->header_op;
		hd_op->rx_rd_p = &ringhd->rxbuf_rdptr;
		hd_op->rx_wt_p = &ringhd->rxbuf_wrptr;
		hd_op->rx_size = ringhd->rxbuf_size;
		hd_op->tx_rd_p = &ringhd->txbuf_rdptr;
		hd_op->tx_wt_p = &ringhd->txbuf_wrptr;
		hd_op->tx_size = ringhd->txbuf_size;
	}

	/* release resource */
	sipc_smem_release_resource(sipc->sipc_pms, sipc->dst);

	sbuf_comm_init(sbuf);

	return 0;
}

static int sbuf_client_init(struct smsg_ipc *sipc, struct sbuf_mgr *sbuf)
{
	VOLA_SBUF_SMEM *smem;
	VOLA_SBUF_RING *ringhd;
	struct sbuf_ring_header_op *hd_op;
	struct sbuf_ring *ring;
	int hsize, i, rval;
	u32 txbufsize, rxbufsize;
	phys_addr_t offset = 0;
	u32 bufnum;
	u8 dst = sbuf->dst;

#ifdef CONFIG_PHYS_ADDR_T_64BIT
	offset = sipc->high_offset;
	offset = offset << 32;
	pr_info("channel %d-%d, offset = 0x%llx!\n",
		sbuf->dst, sbuf->channel, offset);
#endif

	/* get bufnum and bufsize */
	hsize = sizeof(struct sbuf_smem_header) +
		sizeof(struct sbuf_ring_header) * 1;
	sbuf->smem_virt = shmem_ram_vmap_nocache_ex(dst,
						 sbuf->smem,
						 sbuf->smem_addr + offset,
						 hsize);
	if (!sbuf->smem_virt)
		return -EFAULT;

	smem = (VOLA_SBUF_SMEM *)sbuf->smem_virt;
	sbuf->ringnr = smem->ringnr;
	bufnum = sbuf->ringnr;
	ringhd = (VOLA_SBUF_RING *)&smem->headers[0];
	txbufsize = ringhd->rxbuf_size;
	rxbufsize = ringhd->txbuf_size;
	hsize = sizeof(struct sbuf_smem_header) +
	sizeof(struct sbuf_ring_header) * bufnum;
	sbuf->smem_size = hsize + (txbufsize + rxbufsize) * bufnum;
	pr_debug("channel %d-%d, txbufsize = 0x%x, rxbufsize = 0x%x!\n",
		 sbuf->dst, sbuf->channel, txbufsize, rxbufsize);
	pr_debug("channel %d-%d, smem_size = 0x%x, ringnr = %d!\n",
		 sbuf->dst, sbuf->channel, sbuf->smem_size, bufnum);
	shmem_ram_unmap_ex(dst, sbuf->smem, sbuf->smem_virt);

	/* alloc debug smem */
	sbuf->smem_addr_debug = smem_alloc_ex(dst, sbuf->smem, sbuf->smem_size);
	if (!sbuf->smem_addr_debug)
		return -ENOMEM;

	/* get smem virtual address */
	sbuf->smem_virt = shmem_ram_vmap_nocache_ex(dst,
						sbuf->smem,
						sbuf->smem_addr + offset,
						sbuf->smem_size);
	if (!sbuf->smem_virt) {
		pr_err("%s: channel %d-%d,Failed to map smem for sbuf\n",
			__func__, sbuf->dst, sbuf->channel);
		smem_free_ex(dst, sbuf->smem,
			     sbuf->smem_addr_debug, sbuf->smem_size);
		return -EFAULT;
	}

	/* allocate rings description */
	sbuf->rings = kcalloc(bufnum, sizeof(struct sbuf_ring), GFP_KERNEL);
	if (!sbuf->rings) {
		smem_free_ex(dst, sbuf->smem,
			     sbuf->smem_addr_debug, sbuf->smem_size);
		shmem_ram_unmap_ex(dst, sbuf->smem, sbuf->smem_virt);
		return -ENOMEM;
	}
	pr_info("channel %d-%d, ringns = 0x%p!\n",
		 sbuf->dst, sbuf->channel, sbuf->rings);

	/* must request resource before read or write share memory */
	rval = sipc_smem_request_resource(sipc->sipc_pms, sipc->dst, -1);
	if (rval < 0) {
		smem_free(dst, sbuf->smem_addr, sbuf->smem_size);
		shmem_ram_unmap(dst, sbuf->smem_virt);
		kfree(sbuf->rings);
		return rval;
	}

	/* initialize all ring bufs */
	smem = (VOLA_SBUF_SMEM *)sbuf->smem_virt;
	for (i = 0; i < bufnum; i++) {
		ringhd = (VOLA_SBUF_RING *)&smem->headers[i];
		ring = &sbuf->rings[i];
		ring->header = ringhd;
		/* host txbuf_addr */
		ring->rxbuf_virt = sbuf->smem_virt + hsize +
			(txbufsize + rxbufsize) * i;
		/* host rxbuf_addr */
		ring->txbuf_virt = ring->rxbuf_virt + rxbufsize;
		/* init header op , client mode, rx <==> tx */
		hd_op = &ring->header_op;
		hd_op->rx_rd_p = &ringhd->txbuf_rdptr;
		hd_op->rx_wt_p = &ringhd->txbuf_wrptr;
		hd_op->rx_size = ringhd->txbuf_size;
		hd_op->tx_rd_p = &ringhd->rxbuf_rdptr;
		hd_op->tx_wt_p = &ringhd->rxbuf_wrptr;
		hd_op->tx_size = ringhd->rxbuf_size;
	}

	/* release resource */
	sipc_smem_release_resource(sipc->sipc_pms, sipc->dst);

	sbuf_comm_init(sbuf);

	return 0;
}

static int sbuf_thread(void *data)
{
	struct sbuf_mgr *sbuf = data;
	struct sbuf_ring *ring;
	struct smsg mcmd, mrecv;
	int rval, bufid;
	struct smsg_ipc *sipc;
	unsigned long flags;

	/* since the channel open may hang, we call it in the sbuf thread */
	rval = smsg_ch_open(sbuf->dst, sbuf->channel, -1);
	if (rval != 0) {
		pr_err("Failed to open channel %d\n", sbuf->channel);
		/* assign NULL to thread poniter as failed to open channel */
		sbuf->thread = NULL;
		return rval;
	}

	/* if client, send SMSG_CMD_SBUF_INIT, wait sbuf SMSG_DONE_SBUF_INIT */
	sipc = smsg_ipcs[sbuf->dst];
	if (sipc->client) {
		smsg_set(&mcmd, sbuf->channel, SMSG_TYPE_CMD,
			 SMSG_CMD_SBUF_INIT, 0);
		smsg_send(sbuf->dst, &mcmd, -1);
		do {
			smsg_set(&mrecv, sbuf->channel, 0, 0, 0);
			rval = smsg_recv(sbuf->dst, &mrecv, -1);
			if (rval != 0) {
				sbuf->thread = NULL;
				return rval;
			}
		} while (mrecv.type != SMSG_TYPE_DONE ||
			mrecv.flag != SMSG_DONE_SBUF_INIT);
		sbuf->smem_addr = mrecv.value;
		pr_info("channel %d-%d, done_sbuf_init, address = 0x%x!\n",
			sbuf->dst, sbuf->channel, sbuf->smem_addr);
		if (sbuf_client_init(sipc, sbuf)) {
			sbuf->thread = NULL;
			return 0;
		}
		sbuf->state = SBUF_STATE_READY;
	}

	/* sbuf init done, handle the ring rx events */
	while (!kthread_should_stop()) {
		/* monitor sbuf rdptr/wrptr update smsg */
		smsg_set(&mrecv, sbuf->channel, 0, 0, 0);
		rval = smsg_recv(sbuf->dst, &mrecv, -1);
		if (rval == -EIO) {
			/* channel state is free */
			msleep(20);
			continue;
		}

		pr_debug("sbuf thread recv msg: dst=%d, channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
			 sbuf->dst,
			 sbuf->channel,
			 mrecv.type,
			 mrecv.flag,
			 mrecv.value);

		switch (mrecv.type) {
		case SMSG_TYPE_OPEN:
			pr_info("channel %d-%d, state=%d, recv open msg!\n",
				sbuf->dst,
				sbuf->channel, sbuf->state);
			if (sipc->client)
				break;

			/* if channel state is already reay, reopen it
			 * (such as modem reset), we must skip the old
			 * buf data , than give open ack and reset state
			 * to idle
			 */
			if (sbuf->state == SBUF_STATE_READY) {
				sbuf_skip_old_data(sbuf);
				sbuf->state = SBUF_STATE_IDLE;
			}
			/* handle channel open */
			smsg_open_ack(sbuf->dst, sbuf->channel);
			break;
		case SMSG_TYPE_CLOSE:
			/* handle channel close */
			sbuf_skip_old_data(sbuf);
			smsg_close_ack(sbuf->dst, sbuf->channel);
			sbuf->state = SBUF_STATE_IDLE;
			break;
		case SMSG_TYPE_CMD:
			pr_info("channel %d-%d state = %d, recv cmd msg, flag = %d!\n",
				sbuf->dst, sbuf->channel,
				sbuf->state, mrecv.flag);
			if (sipc->client)
				break;

			/* respond cmd done for sbuf init only state is idle */
			if (sbuf->state == SBUF_STATE_IDLE &&
			    mrecv.flag == SMSG_CMD_SBUF_INIT) {
				smsg_set(&mcmd,
					 sbuf->channel,
					 SMSG_TYPE_DONE,
					 SMSG_DONE_SBUF_INIT,
					 sbuf->dst_smem_addr);
				smsg_send(sbuf->dst, &mcmd, -1);
				sbuf->state = SBUF_STATE_READY;
				for (bufid = 0; bufid < sbuf->ringnr; bufid++) {
					ring = &sbuf->rings[bufid];
					if (ring->handler)
						ring->handler(SBUF_NOTIFY_READY,
								ring->data);
				}
			}
			break;
		case SMSG_TYPE_EVENT:
			bufid = mrecv.value;
			WARN_ON(bufid >= sbuf->ringnr);
			ring = &sbuf->rings[bufid];
			switch (mrecv.flag) {
			case SMSG_EVENT_SBUF_RDPTR:
				if (ring->need_wake_lock)
					sprd_pms_request_wakelock_period(ring->tx_pms, 500);
				/* set write mask. */
				spin_lock_irqsave(&ring->poll_lock, flags);
				ring->poll_mask |= POLLOUT | POLLWRNORM;
				spin_unlock_irqrestore(&ring->poll_lock, flags);
				wake_up_interruptible_all(&ring->txwait);
				if (ring->handler)
					ring->handler(SBUF_NOTIFY_WRITE,
						      ring->data);
				break;
			case SMSG_EVENT_SBUF_WRPTR:
				/* set read mask. */
				spin_lock_irqsave(&ring->poll_lock, flags);
				ring->poll_mask |= POLLIN | POLLRDNORM;
				spin_unlock_irqrestore(&ring->poll_lock, flags);

				if (ring->need_wake_lock)
					sprd_pms_request_wakelock_period(ring->rx_pms, 500);
				wake_up_interruptible_all(&ring->rxwait);
				if (ring->handler)
					ring->handler(SBUF_NOTIFY_READ,
						      ring->data);
				break;
			default:
				rval = 1;
				break;
			}
			break;
		default:
			rval = 1;
			break;
		};

		if (rval) {
			pr_info("non-handled sbuf msg: %d-%d, %d, %d, %d\n",
				sbuf->dst,
				sbuf->channel,
				mrecv.type,
				mrecv.flag,
				mrecv.value);
			rval = 0;
		}
		/* unlock sipc channel wake lock */
		smsg_ch_wake_unlock(sbuf->dst, sbuf->channel);
	}

	return 0;
}


int sbuf_create_ex(u8 dst, u8 channel, u16 smem,
		   u32 bufnum, u32 txbufsize, u32 rxbufsize)
{
	struct sbuf_mgr *sbuf;
	u8 ch_index;
	int ret;
	struct smsg_ipc *sipc = NULL;
	struct sched_param param = {.sched_priority = 89};

	sipc = smsg_ipcs[dst];
	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return -EINVAL;
	}

	pr_debug("dst=%d, chanel=%d, bufnum=%d, txbufsize=0x%x, rxbufsize=0x%x\n",
		 dst,
		 channel,
		 bufnum,
		 txbufsize,
		 rxbufsize);

	if (dst >= SIPC_ID_NR  || !sipc) {
		pr_err("dst = %d is invalid\n", dst);
		return -EINVAL;
	}

	sbuf = kzalloc(sizeof(*sbuf), GFP_KERNEL);
	if (!sbuf)
		return -ENOMEM;

	sbuf->state = SBUF_STATE_IDLE;
	sbuf->dst = dst;
	sbuf->channel = channel;
	sbuf->smem = smem;
	if (!sipc->client) {
		ret = sbuf_host_init(sipc, sbuf, bufnum, txbufsize, rxbufsize);
		if (ret) {
			kfree(sbuf);
			return ret;
		}
	}

	sbuf->thread = kthread_create(sbuf_thread, sbuf,
			"sbuf-%d-%d", dst, channel);
	if (IS_ERR(sbuf->thread)) {
		pr_err("Failed to create kthread: sbuf-%d-%d\n", dst, channel);
		if (!sipc->client) {
			kfree(sbuf->rings);
			shmem_ram_unmap_ex(dst, sbuf->smem, sbuf->smem_virt);
			smem_free_ex(dst, sbuf->smem,
				     sbuf->smem_addr, sbuf->smem_size);
		}
		ret = PTR_ERR(sbuf->thread);
		kfree(sbuf);
		return ret;
	}

	sbufs[dst][ch_index] = sbuf;

	/* set the thread as a real time thread, and its priority is 10 */
	sched_setscheduler(sbuf->thread, SCHED_FIFO, &param);
	wake_up_process(sbuf->thread);

	return 0;
}
EXPORT_SYMBOL_GPL(sbuf_create_ex);

void sbuf_set_no_need_wake_lock(u8 dst, u8 channel, u32 bufnum)
{
	u8 ch_index;
	struct sbuf_mgr *sbuf;
	struct sbuf_ring *ring = NULL;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return;
	}

	sbuf = sbufs[dst][ch_index];
	if (!sbuf || sbuf->ringnr <= bufnum)
		return;

	ring = &sbuf->rings[bufnum];
	ring->need_wake_lock = 0;
}
EXPORT_SYMBOL_GPL(sbuf_set_no_need_wake_lock);

void sbuf_destroy(u8 dst, u8 channel)
{
	struct sbuf_mgr *sbuf;
	int i;
	u8 ch_index;
	struct smsg_ipc *sipc;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return;
	}

	sbuf = sbufs[dst][ch_index];
	if (!sbuf)
		return;

	sbuf->state = SBUF_STATE_IDLE;
	smsg_ch_close(dst, channel, -1);

	/* stop sbuf thread if it's created successfully and still alive */
	if (!IS_ERR_OR_NULL(sbuf->thread))
		kthread_stop(sbuf->thread);

	if (sbuf->rings) {
		for (i = 0; i < sbuf->ringnr; i++) {
			wake_up_interruptible_all(&sbuf->rings[i].txwait);
			wake_up_interruptible_all(&sbuf->rings[i].rxwait);
#if defined(CONFIG_DEBUG_FS)
			sbuf_destroy_rdwt_owner(&sbuf->rings[i]);
#endif
			sprd_pms_destroy(sbuf->rings[i].tx_pms);
			sprd_pms_destroy(sbuf->rings[i].rx_pms);
		}
		kfree(sbuf->rings);
	}

	if (sbuf->smem_virt)
		shmem_ram_unmap_ex(dst, sbuf->smem, sbuf->smem_virt);

	sipc = smsg_ipcs[dst];
	if (sipc->client)
		smem_free_ex(dst, sbuf->smem,
			     sbuf->smem_addr_debug, sbuf->smem_size);
	else
		smem_free_ex(dst, sbuf->smem, sbuf->smem_addr, sbuf->smem_size);

	kfree(sbuf);

	sbufs[dst][ch_index] = NULL;
}
EXPORT_SYMBOL_GPL(sbuf_destroy);

int sbuf_write(u8 dst, u8 channel, u32 bufid,
	       void *buf, u32 len, int timeout)
{
	struct sbuf_mgr *sbuf;
	struct sbuf_ring *ring = NULL;
	struct sbuf_ring_header_op *hd_op;
	struct smsg mevt;
	void *txpos;
	int rval, left, tail, txsize;
	u8 ch_index;
	union sbuf_buf u_buf;
	bool no_data;
	unsigned long flags;

	u_buf.buf = buf;
	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return -EINVAL;
	}

	sbuf = sbufs[dst][ch_index];
	if (!sbuf)
		return -ENODEV;
	ring = &sbuf->rings[bufid];
	hd_op = &ring->header_op;
	if (sbuf->state != SBUF_STATE_READY) {
		pr_info("sbuf-%d-%d not ready to write!\n",
			dst, channel);
		return -ENODEV;
	}

	pr_debug("dst=%d, channel=%d, bufid=%d, len=%d, timeout=%d\n",
		 dst,
		 channel,
		 bufid,
		 len,
		 timeout);
	pr_debug("channel=%d, wrptr=%d, rdptr=%d\n",
		 channel,
		 *(hd_op->tx_wt_p),
		 *(hd_op->tx_rd_p));

	rval = 0;
	left = len;

	if (timeout) {
		mutex_lock(&ring->txlock);
	} else {
		if (!mutex_trylock(&ring->txlock)) {
			pr_debug("sbuf_read busy, dst=%d, channel=%d, bufid=%d\n",
				 dst, channel, bufid);
			return -EBUSY;
		}
	}

#if defined(CONFIG_DEBUG_FS)
	sbuf_record_rdwt_owner(ring, 0);
#endif

	/* must request resource before read or write share memory */
	rval = sipc_smem_request_resource(ring->tx_pms, sbuf->dst, -1);
	if (rval < 0) {
		mutex_unlock(&ring->txlock);
		return rval;
	}

	pr_debug("%s: channel=%d, wrptr=%d, rdptr=%d\n",
		 __func__,
		 channel,
		 *(hd_op->tx_wt_p),
		 *(hd_op->tx_rd_p));
	no_data = ((int)(*(hd_op->tx_wt_p) - *(hd_op->tx_rd_p)) >=
					hd_op->tx_size);
	/* update write mask */
	spin_lock_irqsave(&ring->poll_lock, flags);
	if (no_data)
		ring->poll_mask &= ~(POLLOUT | POLLWRNORM);
	else
		ring->poll_mask |= POLLOUT | POLLWRNORM;
	spin_unlock_irqrestore(&ring->poll_lock, flags);

	/* release resource */
	sipc_smem_release_resource(ring->tx_pms, sbuf->dst);

	if (timeout == 0) {
		/* no wait */
		if ((int)(*(hd_op->tx_wt_p) - *(hd_op->tx_rd_p)) >=
				hd_op->tx_size) {
			pr_info("%d-%d ring %d txbuf is full!\n",
				dst, channel, bufid);
			rval = -EBUSY;
		}
	} else if (timeout < 0) {
		mutex_unlock(&ring->txlock);
		/* wait forever */
		rval = wait_event_interruptible(
			ring->txwait,
			(int)(*(hd_op->tx_wt_p) - *(hd_op->tx_rd_p)) <
			hd_op->tx_size ||
			sbuf->state == SBUF_STATE_IDLE);
		mutex_lock(&ring->txlock);
		if (rval < 0)
			pr_debug("wait interrupted!\n");

		if (sbuf->state == SBUF_STATE_IDLE) {
			pr_err("sbuf state is idle!\n");
			rval = -EIO;
		}
	} else {
		mutex_unlock(&ring->txlock);
		/* wait timeout */
		rval = wait_event_interruptible_timeout(
			ring->txwait,
			(int)(*(hd_op->tx_wt_p) - *(hd_op->tx_rd_p)) <
			hd_op->tx_size ||
			sbuf->state == SBUF_STATE_IDLE,
			timeout);
		mutex_lock(&ring->txlock);
		if (rval < 0) {
			pr_debug("wait interrupted!\n");
		} else if (rval == 0) {
			pr_info("wait timeout!\n");
			rval = -ETIME;
		}

		if (sbuf->state == SBUF_STATE_IDLE) {
			pr_err("sbuf state is idle!\n");
			rval = -EIO;
		}
	}

	if (rval < 0) {
		mutex_unlock(&ring->txlock);
		return rval;
	}

	/* must request resource before read or write share memory */
	rval = sipc_smem_request_resource(ring->tx_pms, sbuf->dst, -1);
	if (rval < 0) {
		mutex_unlock(&ring->txlock);
		return rval;
	}

	while (left && (int)(*(hd_op->tx_wt_p) - *(hd_op->tx_rd_p)) <
	       hd_op->tx_size && sbuf->state == SBUF_STATE_READY) {
		/* calc txpos & txsize */
		txpos = ring->txbuf_virt +
			*(hd_op->tx_wt_p) % hd_op->tx_size;
		txsize = hd_op->tx_size -
			(int)(*(hd_op->tx_wt_p) - *(hd_op->tx_rd_p));
		txsize = min(txsize, left);

		tail = txpos + txsize - (ring->txbuf_virt + hd_op->tx_size);
		if (tail > 0) {
			/* ring buffer is rounded */
			if (!access_ok(u_buf.buf, txsize)) {
				unalign_memcpy(txpos, u_buf.buf, txsize - tail);
				unalign_memcpy(ring->txbuf_virt,
					       u_buf.buf + txsize - tail, tail);
			} else {
				if (unalign_copy_from_user(
					txpos,
					u_buf.ubuf,
					txsize - tail) ||
				   unalign_copy_from_user(
					ring->txbuf_virt,
					u_buf.ubuf + txsize - tail,
					tail)) {
					pr_err("failed to copy from user!\n");
					rval = -EFAULT;
					break;
				}
			}
		} else {
			if (!access_ok(u_buf.buf, txsize)) {
				unalign_memcpy(txpos, u_buf.buf, txsize);
			} else {
				/* handle the user space address */
				if (unalign_copy_from_user(
						txpos,
						u_buf.ubuf,
						txsize)) {
					pr_err("failed to copy from user!\n");
					rval = -EFAULT;
					break;
				}
			}
		}

		pr_debug("channel=%d, txpos=%p, txsize=%d\n",
			 channel, txpos, txsize);

		/* update tx wrptr */
		*(hd_op->tx_wt_p) = *(hd_op->tx_wt_p) + txsize;
		/* tx ringbuf is empty, so need to notify peer side */
		if (*(hd_op->tx_wt_p) - *(hd_op->tx_rd_p) == txsize) {
			smsg_set(&mevt, channel,
				 SMSG_TYPE_EVENT,
				 SMSG_EVENT_SBUF_WRPTR,
				 bufid);
			smsg_send(dst, &mevt, -1);
		}

		left -= txsize;
		u_buf.buf += txsize;
	}

	/* update write mask */
	spin_lock_irqsave(&ring->poll_lock, flags);
	if ((int)(*(hd_op->tx_wt_p) - *(hd_op->tx_rd_p)) >=
	    hd_op->tx_size)
		ring->poll_mask &= ~(POLLOUT | POLLWRNORM);
	else
		ring->poll_mask |= POLLOUT | POLLWRNORM;
	spin_unlock_irqrestore(&ring->poll_lock, flags);

	/* release resource */
	sipc_smem_release_resource(ring->tx_pms, sbuf->dst);
	if (ring->need_wake_lock)
		sprd_pms_release_wakelock_later(ring->tx_pms, 20);

	mutex_unlock(&ring->txlock);

	pr_debug("done, channel=%d, len=%d\n",
		 channel, len - left);

	if (len == left)
		return rval;
	else
		return (len - left);
}
EXPORT_SYMBOL_GPL(sbuf_write);

int sbuf_read(u8 dst, u8 channel, u32 bufid,
	      void *buf, u32 len, int timeout)
{
	struct sbuf_mgr *sbuf;
	struct sbuf_ring *ring = NULL;
	struct sbuf_ring_header_op *hd_op;
	struct smsg mevt;
	void *rxpos;
	int rval, left, tail, rxsize;
	u8 ch_index;
	union sbuf_buf u_buf;
	bool no_data;
	unsigned long flags;

	u_buf.buf = buf;
	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return -EINVAL;
	}
	sbuf = sbufs[dst][ch_index];
	if (!sbuf)
		return -ENODEV;
	ring = &sbuf->rings[bufid];
	hd_op = &ring->header_op;

	if (sbuf->state != SBUF_STATE_READY) {
		pr_debug("sbuf-%d-%d not ready to read!\n", dst, channel);
		return -ENODEV;
	}

	pr_debug("dst=%d, channel=%d, bufid=%d, len=%d, timeout=%d\n",
		 dst, channel, bufid, len, timeout);
	pr_debug("channel=%d, wrptr=%d, rdptr=%d\n",
		 channel,
		 *(hd_op->rx_wt_p),
		 *(hd_op->rx_rd_p));

	rval = 0;
	left = len;

	if (timeout) {
		mutex_lock(&ring->rxlock);
	} else {
		if (!mutex_trylock(&ring->rxlock)) {
			pr_debug("busy!,dst=%d, channel=%d, bufid=%d\n",
				 dst, channel, bufid);
			return -EBUSY;
		}
	}

#if defined(CONFIG_DEBUG_FS)
	sbuf_record_rdwt_owner(ring, 1);
#endif

	/* must request resource before read or write share memory */
	rval = sipc_smem_request_resource(ring->rx_pms, sbuf->dst, -1);
	if (rval < 0) {
		mutex_unlock(&ring->rxlock);
		return rval;
	}

	pr_debug("%s: channel=%d, wrptr=%d, rdptr=%d\n",
				__func__, channel,
				*(hd_op->rx_wt_p), *(hd_op->rx_rd_p));
	no_data = (*(hd_op->rx_wt_p) == *(hd_op->rx_rd_p));
	/* update read mask */
	spin_lock_irqsave(&ring->poll_lock, flags);
	if (no_data)
		ring->poll_mask &= ~(POLLIN | POLLRDNORM);
	else
		ring->poll_mask |= POLLIN | POLLRDNORM;
	spin_unlock_irqrestore(&ring->poll_lock, flags);

	/* release resource */
	sipc_smem_release_resource(ring->rx_pms, sbuf->dst);

	if (*(hd_op->rx_wt_p) == *(hd_op->rx_rd_p)) {
		if (timeout == 0) {
			/* no wait */
			pr_debug("%d-%d ring %d rxbuf is empty!\n",
				 dst, channel, bufid);
			rval = -ENODATA;
		} else if (timeout < 0) {
			mutex_unlock(&ring->rxlock);
			/* wait forever */
			rval = wait_event_interruptible(
				ring->rxwait,
				*(hd_op->rx_wt_p) != *(hd_op->rx_rd_p) ||
				sbuf->state == SBUF_STATE_IDLE);
			mutex_lock(&ring->rxlock);
			if (rval < 0)
				pr_debug("wait interrupted!\n");

			if (sbuf->state == SBUF_STATE_IDLE) {
				pr_err("sbuf state is idle!\n");
				rval = -EIO;
			}
		} else {
			mutex_unlock(&ring->rxlock);
			/* wait timeout */
			rval = wait_event_interruptible_timeout(
				ring->rxwait,
				*(hd_op->rx_wt_p) != *(hd_op->rx_rd_p) ||
				sbuf->state == SBUF_STATE_IDLE, timeout);
			mutex_lock(&ring->rxlock);
			if (rval < 0) {
				pr_debug("wait interrupted!\n");
			} else if (rval == 0) {
				pr_info("wait timeout!\n");
				rval = -ETIME;
			}

			if (sbuf->state == SBUF_STATE_IDLE) {
				pr_err("state is idle!\n");
				rval = -EIO;
			}
		}
	}

	if (rval < 0) {
		mutex_unlock(&ring->rxlock);
		return rval;
	}

	/* must request resource before read or write share memory */
	rval = sipc_smem_request_resource(ring->rx_pms, sbuf->dst, -1);
	if (rval < 0) {
		mutex_unlock(&ring->rxlock);
		return rval;
	}

	while (left &&
	       (*(hd_op->rx_wt_p) != *(hd_op->rx_rd_p)) &&
	       sbuf->state == SBUF_STATE_READY) {
		/* calc rxpos & rxsize */
		rxpos = ring->rxbuf_virt +
			*(hd_op->rx_rd_p) % hd_op->rx_size;
		rxsize = (int)(*(hd_op->rx_wt_p) - *(hd_op->rx_rd_p));
		/* check overrun */
		if (rxsize > hd_op->rx_size)
			pr_err("bufid = %d, channel= %d rxsize=0x%x, rdptr=%d, wrptr=%d",
			       bufid,
			       channel,
			       rxsize,
			       *(hd_op->rx_wt_p),
			       *(hd_op->rx_rd_p));

		rxsize = min(rxsize, left);

		pr_debug("channel=%d, buf=%p, rxpos=%p, rxsize=%d\n",
			 channel, u_buf.buf, rxpos, rxsize);

		tail = rxpos + rxsize - (ring->rxbuf_virt + hd_op->rx_size);

		if (tail > 0) {
			/* ring buffer is rounded */
			if (!access_ok(u_buf.buf, rxsize)) {
				unalign_memcpy(u_buf.buf, rxpos, rxsize - tail);
				unalign_memcpy(u_buf.buf + rxsize - tail,
					       ring->rxbuf_virt, tail);
			} else {
				/* handle the user space address */
				if (unalign_copy_to_user(u_buf.ubuf,
							 rxpos,
							 rxsize - tail) ||
				    unalign_copy_to_user(u_buf.ubuf
							 + rxsize - tail,
							 ring->rxbuf_virt,
							 tail)) {
					pr_err("failed to copy to user!\n");
					rval = -EFAULT;
					break;
				}
			}
		} else {
			if (!access_ok(u_buf.buf, rxsize)) {
				unalign_memcpy(u_buf.buf, rxpos, rxsize);
			} else {
				/* handle the user space address */
				if (unalign_copy_to_user(u_buf.ubuf,
							 rxpos, rxsize)) {
					pr_err("failed to copy to user!\n");
					rval = -EFAULT;
					break;
				}
			}
		}

		/* update rx rdptr */
		*(hd_op->rx_rd_p) = *(hd_op->rx_rd_p) + rxsize;
		/* rx ringbuf is full ,so need to notify peer side */
		if (*(hd_op->rx_wt_p) - *(hd_op->rx_rd_p) ==
		    hd_op->rx_size - rxsize) {
			smsg_set(&mevt, channel,
				 SMSG_TYPE_EVENT,
				 SMSG_EVENT_SBUF_RDPTR,
				 bufid);
			smsg_send(dst, &mevt, -1);
		}

		left -= rxsize;
		u_buf.buf += rxsize;
	}

	/* update read mask */
	spin_lock_irqsave(&ring->poll_lock, flags);
	if (*(hd_op->rx_wt_p) == *(hd_op->rx_rd_p))
		ring->poll_mask &= ~(POLLIN | POLLRDNORM);
	else
		ring->poll_mask |= POLLIN | POLLRDNORM;
	spin_unlock_irqrestore(&ring->poll_lock, flags);

	/* release resource */
	sipc_smem_release_resource(ring->rx_pms, sbuf->dst);
	if (ring->need_wake_lock)
		sprd_pms_release_wakelock_later(ring->rx_pms, 20);

	mutex_unlock(&ring->rxlock);

	pr_debug("done, channel=%d, len=%d", channel, len - left);

	if (len == left)
		return rval;
	else
		return (len - left);
}
EXPORT_SYMBOL_GPL(sbuf_read);

int sbuf_poll_wait(u8 dst, u8 channel, u32 bufid,
		   struct file *filp, poll_table *wait)
{
	struct sbuf_mgr *sbuf;
	struct sbuf_ring *ring = NULL;
	struct sbuf_ring_header_op *hd_op;
	unsigned int mask = 0;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return mask;
	}
	sbuf = sbufs[dst][ch_index];
	if (!sbuf)
		return mask;
	ring = &sbuf->rings[bufid];
	hd_op = &ring->header_op;
	if (sbuf->state != SBUF_STATE_READY) {
		pr_err("sbuf-%d-%d not ready to poll !\n", dst, channel);
		return mask;
	}

	poll_wait(filp, &ring->txwait, wait);
	poll_wait(filp, &ring->rxwait, wait);

	if (*(hd_op->rx_wt_p) != *(hd_op->rx_rd_p))
		mask |= POLLIN | POLLRDNORM;

	if (*(hd_op->tx_wt_p) - *(hd_op->tx_rd_p) < hd_op->tx_size)
		mask |= POLLOUT | POLLWRNORM;

	return mask;
}
EXPORT_SYMBOL_GPL(sbuf_poll_wait);

int sbuf_status(u8 dst, u8 channel)
{
	struct sbuf_mgr *sbuf;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return -EINVAL;
	}
	sbuf = sbufs[dst][ch_index];

	if (!sbuf)
		return -ENODEV;
	if (sbuf->state != SBUF_STATE_READY)
		return -ENODEV;

	return 0;
}
EXPORT_SYMBOL_GPL(sbuf_status);

int sbuf_register_notifier(u8 dst, u8 channel, u32 bufid,
			   void (*handler)(int event, void *data), void *data)
{
	struct sbuf_mgr *sbuf;
	struct sbuf_ring *ring = NULL;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return -EINVAL;
	}
	sbuf = sbufs[dst][ch_index];
	if (!sbuf)
		return -ENODEV;
	ring = &sbuf->rings[bufid];
	ring->handler = handler;
	ring->data = data;

	if (sbuf->state == SBUF_STATE_READY)
		handler(SBUF_NOTIFY_READ, data);

	return 0;
}
EXPORT_SYMBOL_GPL(sbuf_register_notifier);

struct sbuf_mgr *sbuf_register_notifier_ex(u8 dst, u8 channel, u32 mark,
					   void (*handler)(int event, u32 bufid,
							   void *data),
					   void *data)
{
	struct sbuf_mgr *sbuf;
	u8 ch_index;
	u32 i;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return NULL;
	}
	sbuf = sbufs[dst][ch_index];
	if (!sbuf)
		return NULL;

	sbuf->ch_mark = mark;
	sbuf->handler = handler;
	sbuf->data = data;

	if (sbuf->state == SBUF_STATE_READY) {
		for (i = 0; i < sbuf->ringnr; i++) {
			if (sbuf->ch_mark & BIT(i))
				handler(SBUF_NOTIFY_READ, i, data);
		}
	}

	return sbuf;
}
EXPORT_SYMBOL_GPL(sbuf_register_notifier_ex);

int sbuf_unregister_notifier(u8 dst, u8 channel, u32 bufid)
{
	struct sbuf_mgr *sbuf;
	struct sbuf_ring *ring = NULL;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("channel %d invalid!\n", channel);
		return -EINVAL;
	}
	sbuf = sbufs[dst][ch_index];
	if (!sbuf)
		return -ENODEV;
	ring = &sbuf->rings[bufid];
	ring->handler = NULL;
	ring->data = NULL;

	return 0;
}
EXPORT_SYMBOL(sbuf_unregister_notifier);

void sbuf_get_status(u8 dst, char *status_info, int size)
{
	struct sbuf_mgr *sbuf = NULL;
	struct sbuf_ring *ring = NULL;
	struct sbuf_ring_header_op *hd_op;
	wait_queue_entry_t *pos;
	struct task_struct *task;
	unsigned long flags;
	int i, n, len, cnt;
	u32 b_select;
	char *phead;
#if defined(CONFIG_DEBUG_FS)
	struct name_node *node = NULL;
#endif

	if (!status_info || size < 0 || dst >= SIPC_ID_NR)
		return;
	len = strlen(status_info);

	for (i = 0;  i < SMSG_VALID_CH_NR; i++) {
		sbuf = sbufs[dst][i];
		if (!sbuf)
			continue;
		ring = &sbuf->rings[0];
		/* must request resource before read or write share memory */
		if (sipc_smem_request_resource(ring->rx_pms, dst, 1000) < 0)
			continue;

		for (n = 0;  n < sbuf->ringnr && len < size; n++) {
			ring = &sbuf->rings[n];
			hd_op = &ring->header_op;

			if ((*(hd_op->rx_wt_p) - *(hd_op->rx_rd_p))
					< hd_op->rx_size)
				continue;

			snprintf(status_info + len,
				 size - len,
				 "ch-%d-ring-%d is full.\n",
				 sbuf->channel,
				 n);
			len = strlen(status_info);

			/*  show all rxwait task */
			spin_lock_irqsave(&ring->rxwait.lock, flags);
			cnt = 0;
			list_for_each_entry(pos,
					    &ring->rxwait.head,
					    entry) {
				task = sbuf_wait_get_task(pos, &b_select);
				if (!task)
					continue;

				if (b_select)
					phead = "rxwait task";
				else
					phead = "select task";

				snprintf(
					 status_info + len,
					 size - len,
					 "%s %d: %s, state=0x%x, pid=%d.\n",
					 phead,
					 cnt, task->comm,
					 task->__state, task->pid);
				cnt++;
				len = strlen(status_info);
			}
			spin_unlock_irqrestore(&ring->rxwait.lock, flags);

			/* only show the latest ever read task */
#if defined(CONFIG_DEBUG_FS)
			spin_lock_irqsave(&ring->rxwait.lock, flags);
			list_for_each_entry(node, &ring->rx_list, list) {
				if (node->latest) {
					snprintf(
						 status_info + len,
						 size - len,
						 "read task: %s, pid = %d.\n",
						 node->comm,
						 node->pid);
					break;
				}
			}
			spin_unlock_irqrestore(&ring->rxwait.lock, flags);
#endif
		}
		ring = &sbuf->rings[0];
		/* release resource */
		sipc_smem_release_resource(ring->rx_pms, sbuf->dst);
	}
}
EXPORT_SYMBOL_GPL(sbuf_get_status);

#if defined(CONFIG_DEBUG_FS)
static void sbuf_debug_task_show(struct seq_file *m,
				 struct sbuf_mgr *sbuf, int task_type)
{
	int n, cnt;
	u32 b_select;
	unsigned long flags;
	struct sbuf_ring *ring = NULL;
	wait_queue_head_t *phead;
	char *buf;
	wait_queue_entry_t *pos;
	struct task_struct *task;

	for (n = 0;  n < sbuf->ringnr;	n++) {
		ring = &sbuf->rings[n];
		cnt = 0;

		if (task_type == TASK_RXWAIT) {
			phead = &ring->rxwait;
			buf = "rxwait task";
		} else if (task_type == TASK_TXWAIT) {
			phead = &ring->txwait;
			buf = "txwait task";
		} else {
			phead = &ring->rxwait;
			buf = "select task";
		}

		spin_lock_irqsave(&phead->lock, flags);
		list_for_each_entry(pos, &phead->head, entry) {
			task = sbuf_wait_get_task(pos, &b_select);
			if (!task)
				continue;

			if (b_select && (task_type != TASK_SELECT))
				continue;

			seq_printf(m, "  ring[%2d]: %s %d ",
				   n,
				   buf,
				   cnt);
			seq_printf(m, ": %s, state = 0x%x, pid = %d\n",
				   task->comm,
				   task->__state,
				   task->pid);
			cnt++;
		}
		spin_unlock_irqrestore(&phead->lock, flags);
	}
}

#if defined(CONFIG_DEBUG_FS)
static void sbuf_debug_list_show(struct seq_file *m,
				 struct sbuf_mgr *sbuf, int b_rx)
{
	int n, cnt;
	struct sbuf_ring *ring = NULL;
	struct list_head *plist;
	char *buf;
	struct name_node *node = NULL;
	unsigned long flags;

	/* list all  sbuf task list */
	for (n = 0;  n < sbuf->ringnr;	n++) {
		ring = &sbuf->rings[n];
		cnt = 0;

		if (b_rx) {
			plist = &ring->rx_list;
			buf = "read task";
		} else {
			plist = &ring->tx_list;
			buf = "write task";
		}

		spin_lock_irqsave(&ring->rxwait.lock, flags);
		list_for_each_entry(node, plist, list) {
			seq_printf(m, "  ring[%2d]: %s %d : %s, pid = %d, latest = %d\n",
				   n,
				   buf,
				   cnt,
				   node->comm,
				   node->pid,
				   node->latest);
			cnt++;
		}
		spin_unlock_irqrestore(&ring->rxwait.lock, flags);
	}
}
#endif

static int sbuf_debug_show(struct seq_file *m, void *private)
{
	struct sbuf_mgr *sbuf = NULL;
	struct sbuf_ring *ring = NULL;
	struct sbuf_ring_header_op *hd_op;
	int i, j, n, cnt;
	struct smsg_ipc *sipc = NULL;

	for (i = 0; i < SIPC_ID_NR; i++) {
		sipc = smsg_ipcs[i];
		if (!sipc)
			continue;

		/* must request resource before read or write share memory */
		if (sipc_smem_request_resource(sipc->sipc_pms,
					       sipc->dst, 1000) < 0)
			continue;

		sipc_debug_putline(m, '*', 120);
		seq_printf(m, "dst: 0x%0x, sipc: %s:\n", i, sipc->name);
		sipc_debug_putline(m, '*', 120);

		for (j = 0;  j < SMSG_VALID_CH_NR; j++) {
			sbuf = sbufs[i][j];
			if (!sbuf)
				continue;
			/* list a sbuf channel */
			sipc_debug_putline(m, '-', 100);
			seq_printf(m, "sbuf_%d_%03d, state: %d, ",
				   sbuf->dst,
				   sbuf->channel,
				   sbuf->state);
			seq_printf(m, "virt: 0x%lx, phy: 0x%0x, map: 0x%x",
				   (unsigned long)sbuf->smem_virt,
				   sbuf->smem_addr,
				   sbuf->dst_smem_addr);
			seq_printf(m, " size: 0x%0x, ringnr: %d\n",
				   sbuf->smem_size,
				   sbuf->ringnr);
			sipc_debug_putline(m, '-', 100);

			/* list all  sbuf ring info list in a chanel */
			sipc_debug_putline(m, '-', 80);
			seq_puts(m, "  1. all sbuf ring info list:\n");
			for (n = 0;  n < sbuf->ringnr;  n++) {
				ring = &sbuf->rings[n];
				hd_op = &ring->header_op;
				if (*(hd_op->tx_wt_p) == 0 &&
				    *(hd_op->rx_wt_p) == 0)
					continue;

				seq_printf(m, "  rx ring[%2d]: addr: 0x%0x, ",
					   n, hd_op->rx_size);
				seq_printf(m, "rp: 0x%0x, wp: 0x%0x, size: 0x%0x\n",
					   *(hd_op->rx_rd_p),
					   *(hd_op->rx_wt_p),
					   hd_op->rx_size);

				seq_printf(m, "  tx ring[%2d]: addr: 0x%0x, ",
					   n, hd_op->tx_size);
				seq_printf(m, "rp: 0x%0x, wp: 0x%0x, size: 0x%0x\n",
					   *(hd_op->tx_rd_p),
					   *(hd_op->tx_wt_p),
					   hd_op->tx_size);
			}

			/* list all sbuf rxwait/txwait in a chanel */;
			sipc_debug_putline(m, '-', 80);
			seq_puts(m, "  2. all waittask list:\n");
			sbuf_debug_task_show(m, sbuf, TASK_RXWAIT);
			sbuf_debug_task_show(m, sbuf, TASK_TXWAIT);
			sbuf_debug_task_show(m, sbuf, TASK_SELECT);

#ifdef CONFIG_DEBUG_FS
			/* list all sbuf ever read task list in a chanel */;
			sipc_debug_putline(m, '-', 80);
			seq_puts(m, "  3. all ever rdwt list:\n");
			sbuf_debug_list_show(m, sbuf, 1);
			sbuf_debug_list_show(m, sbuf, 0);
#endif

			/* list all  rx full ring list in a chanel */
			cnt = 0;
			for (n = 0;  n < sbuf->ringnr;	n++) {
				ring = &sbuf->rings[n];
				hd_op = &ring->header_op;
				if ((*(hd_op->rx_wt_p) - *(hd_op->rx_rd_p))
						== hd_op->rx_size) {
					if (cnt == 0) {
						sipc_debug_putline(m, '-', 80);
						seq_puts(m, "  x. all rx full ring list:\n");
					}
					cnt++;
					seq_printf(m, "  ring[%2d]\n", n);
				}
			}
		}
		/* release resource */
		sipc_smem_release_resource(sipc->sipc_pms, sipc->dst);
	}

	return 0;
}

static int sbuf_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, sbuf_debug_show, inode->i_private);
}

static const struct file_operations sbuf_debug_fops = {
	.open = sbuf_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int sbuf_init_debug(void)
{
	int rval;
	dev_t dev_no;

	sbuf_dev = kzalloc(sizeof(struct sbuf_device), GFP_KERNEL);
	if (!sbuf_dev)
		return -ENOMEM;
	rval = alloc_chrdev_region(&dev_no, 0, 1, "sbuf_debug");
	if (rval) {
		pr_err("Failed to alloc chrdev region\n");
		goto free_sbuf_dev;
	}
	sbuf_dev->major = MAJOR(dev_no);
	sbuf_dev->minor = MINOR(dev_no);
	cdev_init(&sbuf_dev->cdev, &sbuf_debug_fops);
	rval = cdev_add(&sbuf_dev->cdev, dev_no, 1);
	if (rval) {
		pr_err("Failed to add sbuf cdev\n");
		goto free_devno;
	}
	sbuf_class = class_create(THIS_MODULE, "sbuf");
	if (IS_ERR(sbuf_class)) {
		rval = PTR_ERR(sbuf_class);
		pr_err("Failed to create sbuf class\n");
		goto free_cdev;
	}
	sbuf_dev->sys_dev = device_create(sbuf_class, NULL,
				       dev_no,
				       sbuf_dev, "sipc_sbuf");

	return 0;

free_cdev:
	cdev_del(&sbuf_dev->cdev);

free_devno:
	unregister_chrdev_region(dev_no, 1);

free_sbuf_dev:
	kfree(sbuf_dev);
	sbuf_dev = NULL;
	return rval;
}
EXPORT_SYMBOL_GPL(sbuf_init_debug);

void sbuf_destroy_debug(void)
{
	if (sbuf_dev) {
		dev_t dev_no = MKDEV(sbuf_dev->major, sbuf_dev->minor);

		if (sbuf_dev->sys_dev) {
			device_destroy(sbuf_class, dev_no);
			sbuf_dev->sys_dev = NULL;
		}

		if (!IS_ERR(sbuf_class))
			class_destroy(sbuf_class);

		unregister_chrdev_region(dev_no, 1);
		cdev_del(&sbuf_dev->cdev);
		kfree(sbuf_dev);
		sbuf_dev = NULL;
	}
}
EXPORT_SYMBOL_GPL(sbuf_destroy_debug);
#endif /* CONFIG_DEBUG_FS */

MODULE_AUTHOR("Chen Gaopeng");
MODULE_DESCRIPTION("SIPC/SBUF driver");
MODULE_LICENSE("GPL v2");
