/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"VPU, %s: " fmt, __func__

#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/compiler.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/scm.h>

#include "vpu_hfi.h"
#include "vpu_hfi_intf.h"
#include "vpu_debug.h"

u32 vpu_pil_timeout = VPU_PIL_DEFAULT_TIMEOUT_MS;

/*
 * queue state:
 * DISABLED: queue not working, read/write index not valid
 * RESET: queue not in use, but read/write index valid
 * ACTIVE: queue is fully functioning
 */
#define HFI_QUEUE_STATE_DISABLED	0
#define HFI_QUEUE_STATE_RESET		1
#define HFI_QUEUE_STATE_ACTIVE		2

struct vpu_hfi_txq_info {
	/* only threads can Tx, must lock first to Tx */
	struct mutex lock;
	/* which channel this q belongs to */
	u32 cid;
	/* q hdr table address in IPC memory */
	struct hfi_queue_header	*q_hdr;
	 /* 1=active; 0=not active */
	u32 state;
	/* IPC memory for data */
	void *q_data;
	u32 q_data_size;
	u32 q_data_offset; /* offset of queue data */

	/* client data, not interpreted at this layer */
	void	*priv;
};

struct vpu_hfi_device;

/*
 * the message size should be big than the size of an extended
 * msg, which is defined in API header as VPU_MAX_EXT_DATA_SIZE
 */
#define MAX_MSG_SIZE		800
struct vpu_hfi_rxq_info {
	/* must lock first to Rx */
	struct mutex lock;
	/* which channel this q belongs to */
	u32 cid;
	/* q hdr table address in IPC memory */
	void *q_hdr;
	u32 state;
	/* IPC memory for data */
	void *q_data;
	u32 q_data_size;
	u32 q_data_offset; /* offset of queue data */

	/* work item to handle messages */
	struct work_struct rx_work;

	struct vpu_hfi_device *dev;

	u8 msg_buf[MAX_MSG_SIZE];

	/* client data, not interpreted at this layer */
	void *priv;
};

/* if not use interrupt, polling period for Rx service */
#define VPU_POLL_INTERVAL_MS	50

struct vpu_hfi_device {
	/* all IPC queues, lock first to change the qtable or arrays */
	struct hfi_queue_table_header *qtbl;
	struct vpu_hfi_txq_info txqs[VPU_MAX_TXQ_NUM];
	struct vpu_hfi_rxq_info rxqs[VPU_MAX_RXQ_NUM];
	spinlock_t	qlock;

	/* interrupt information */
	u32 irq_no; /* Firmware to APPS IPC irq */
	u32 irq_wd; /* Firmware's watchdog irq */

	/* io space, already mapped */
	void __iomem *reg_base;
	void __iomem *mem_base;
	void __iomem *vbif_base;

	/* subsystem */
	void *vpu_sub_sys;

	/* work queue to handle rx */
	struct workqueue_struct *main_workq;

	struct work_struct watchdog_work;

	u32 watchdog_enabled;
	u32 watchdog_bited;

	/* polling usage */
	struct delayed_work poll_work;
	u32 use_poll;

	/* callbacks */
	hfi_handle_msg   handle_msg;
	hfi_handle_event handle_event;

	struct vpu_platform_resources	*platform_resouce;
};

/* global */
static struct vpu_hfi_device g_hfi_device;

/*
 * write a packet into the IPC memory
 * caller needs to lock the queue
 */
static int raw_write_data(struct vpu_hfi_txq_info *tq,
		u8 *data1, u32 data1_size,
		u8 *data2, u32 data2_size)
{
	struct hfi_queue_header *qhdr;
	u32 new_write_idx;
	u32 empty_space, write_idx, read_idx, q_size;
	u8 *write_ptr;
	int i;

	qhdr = (struct hfi_queue_header *) tq->q_hdr;
	q_size = qhdr->qhdr_q_size;
	read_idx = qhdr->qhdr_read_idx;
	write_idx = qhdr->qhdr_write_idx;

	/* how much space is available */
	empty_space = (read_idx > write_idx) ?
		(read_idx - write_idx) :
		(q_size + read_idx - write_idx);

	if (unlikely(empty_space <= data1_size + data2_size)) {
		/* at least 1b is reserved btw write and read index */
		qhdr->qhdr_tx_req =  1;
		pr_err("Tx-%d: Insufficient size (%d) to write (%d)\n",
			tq->cid, empty_space, data1_size + data2_size);
		return -ENOSPC;
	}

	qhdr->qhdr_tx_req = 0;

	/* write data1 */
	new_write_idx = (write_idx + data1_size);
	write_ptr = (u8 *)((tq->q_data) + write_idx);

	for (i = 0; i < (data1_size / sizeof(u32)); i++)
		pr_debug("Tx%d-pkt: %2d:0x%08x\n",
				tq->cid, i, ((u32 *)(data1))[i]);
	for (i = 0; i < (data2_size / sizeof(u32)); i++)
		pr_debug("Tx%d-pkt: %2d:0x%08x\n", tq->cid,
			data1_size / sizeof(u32) + i, ((u32 *)(data2))[i]);

	if (likely(new_write_idx < q_size)) {
		/* no wrap */
		memcpy(write_ptr, data1, data1_size);

		/* update the write ptr for data2 */
		write_ptr += data1_size;
	} else {
		/* wrap, copy the first part toward end of the queue */
		new_write_idx -= q_size;
		memcpy(write_ptr, data1, (data1_size - new_write_idx));
		/* copy the reminder to begin of the queue */
		memcpy(tq->q_data,
			data1 + ((data1_size - new_write_idx)),
			new_write_idx);

		/* update the write ptr for data2 */
		write_ptr = tq->q_data + new_write_idx;
	}

	if (data2_size > 0) {
		/* write data2 */
		new_write_idx += data2_size;

		if (likely(new_write_idx < q_size)) {
			/* no wrap */
			memcpy(write_ptr, data2, data2_size);
		} else {
			/* wrap, copy the first part toward end of the queue */
			new_write_idx -= q_size;
			memcpy(write_ptr, data2, (data2_size - new_write_idx));
			/* copy the reminder to begin of the queue */
			memcpy(tq->q_data,
				data2 + ((data2_size - new_write_idx)),
				new_write_idx);
		}
	}

	/* make sure data written before index updated */
	mb();
	qhdr->qhdr_write_idx = new_write_idx;

	return 0;
}

/*
 * copy a packet to an IPC queue
 * callers need to lock the queue
 * more: 1 if more data available, 0 otherwise
 * return packet size read, or <0 for error
 */
static int raw_read_packet(struct vpu_hfi_rxq_info *rxq,
		u8 *data, u32 max_size, u32 *more)
{
	struct hfi_queue_header *qhdr;
	struct vpu_hfi_packet *packet;
	u32 new_read_idx;
	u32 filled_space, write_idx, read_idx, q_size, packet_size;
	u8 *read_ptr;
	int i;

	qhdr = (struct hfi_queue_header *) rxq->q_hdr;
	q_size = qhdr->qhdr_q_size;
	read_idx = qhdr->qhdr_read_idx;
	write_idx = qhdr->qhdr_write_idx;

	/* how much data available */
	filled_space = (write_idx >= read_idx) ?
		(write_idx - read_idx) :
		(q_size + write_idx - read_idx);

	/* no data available */
	if (unlikely(filled_space == 0))
		return 0;

	read_ptr = (u8 *)((rxq->q_data) + read_idx);
	packet = (struct vpu_hfi_packet *) read_ptr;

	packet_size = packet->size;

	if (unlikely(packet_size > filled_space)) {
		/* error! non complete packet */
		pr_err("partial packet at rxq %d, expect %d > available %d\n",
			rxq->cid, packet_size, filled_space);
		return -EIO;
	} else if (unlikely(packet_size > max_size)) {
		/* error! the buffer is not big enough */
		pr_err("packet too big at rxq %d, data %d > buffer %d\n",
			rxq->cid, packet_size, max_size);
		return -ENOBUFS;
	} else {
		new_read_idx = read_idx + packet_size;
		if (unlikely(new_read_idx >= q_size)) {
			/* packet wrapped */
			new_read_idx -= q_size;
			memcpy(data, read_ptr, q_size - read_idx);
			memcpy(data + q_size - read_idx, rxq->q_data,
					packet_size + read_idx - q_size);
		} else {
			/* packet doesn't wrap, copy in one shot */
			memcpy(data, read_ptr, packet_size);
		}

		/* print the packet content */
		if (rxq->cid != VPU_LOGGING_CHANNEL_ID)
			for (i = 0; i < (packet_size / sizeof(u32)); i++)
				pr_debug("Rx%d-pkt: %2d:0x%08x\n",
					rxq->cid, i, ((u32 *)(data))[i]);

		 /* make sure all read/write is done before updating read idx */
		mb();
		qhdr->qhdr_read_idx = new_read_idx;

		if (more)
			*more = (packet_size == filled_space) ? 0 : 1;

		return packet_size;
	}
}

static void raw_handle_rx_msgs_q(struct vpu_hfi_rxq_info *rxq)
{
	int rc;
	int more_data;
	struct vpu_hfi_device *hdevice;
	struct hfi_queue_header *qhdr;

	mutex_lock(&rxq->lock);

	qhdr = (struct hfi_queue_header *) rxq->q_hdr;
	hdevice = rxq->dev;

	if (rxq->cid == VPU_LOGGING_CHANNEL_ID) {
		/* for logging wake up user thread */
		if (qhdr->qhdr_write_idx != qhdr->qhdr_read_idx)
			vpu_wakeup_fw_logging_wq();
		goto exit_1;
	}

	do {

		more_data = 0;
		rc = raw_read_packet(rxq, rxq->msg_buf, MAX_MSG_SIZE,
			&more_data);

		/* no data available or error in reading packet */
		if (rc <= 0)
			break;

		/* inform firmware that new space available */
		if ((qhdr->qhdr_tx_req == 1) && (hdevice->vpu_sub_sys))
			raw_hfi_int_fire(hdevice->reg_base);

		if (!more_data)
			qhdr->qhdr_rx_req = 1;

		/* call client's callback */
		if (hdevice->handle_msg)
			hdevice->handle_msg(rxq->cid,
					(struct vpu_hfi_packet *)rxq->msg_buf,
					rxq->priv);

	} while (more_data);

exit_1:
	mutex_unlock(&rxq->lock);
}

static void _vpu_hfi_rx_work_handler(struct work_struct *work)
{
	struct vpu_hfi_rxq_info *rxq;
	rxq = container_of(work, struct vpu_hfi_rxq_info, rx_work);

	raw_handle_rx_msgs_q(rxq);
}

/*
 * initialize the qtb, queues, and data area
 * this function assumes:
 *   the caller has locked
 *   the caller has done sanity check (memory is aligned, enough space, etc)
 */
static int vpu_hfi_queues_init(struct vpu_hfi_device *dev, void *start_addr,
		u32 total_size)
{
	int i;
	unsigned long flags;
	struct hfi_queue_table_header *qtbl;
	struct hfi_queue_header *qhdr;
	void *qmem_addr;
	int qtbl_size, qmem_size_sum;

	pr_debug("Entering function\n");

	qtbl_size = sizeof(struct hfi_queue_table_header) +
			sizeof(struct hfi_queue_header) * VPU_MAX_QUEUE_NUM;

	spin_lock_irqsave(&dev->qlock, flags);

	/* init qtable */
	qtbl = (struct hfi_queue_table_header *)start_addr;
	qtbl->qtbl_version = 1;
	qtbl->qtbl_size = qtbl_size;
	qtbl->qtbl_qhdr0_offset = sizeof(struct hfi_queue_table_header);
	qtbl->qtbl_qhdr_size = sizeof(struct hfi_queue_header);
	qtbl->qtbl_num_q = VPU_MAX_QUEUE_NUM;
	qtbl->qtbl_num_active_q = 0;

	dev->qtbl = qtbl;

	/* q memory */
	qmem_addr = start_addr + qtbl_size;
	qmem_size_sum = 0; /* incrementing sum of queue memory size */

	/* first queue header */
	qhdr = (struct hfi_queue_header *)(qtbl + 1);

	/* assign the TX queue array, qhdr not initialized */
	for (i = 0; i < VPU_MAX_TXQ_NUM; i++) {
		dev->txqs[i].cid = i;
		dev->txqs[i].priv = NULL;
		dev->txqs[i].q_hdr = qhdr + i * 2;
		dev->txqs[i].q_data = qmem_addr + qmem_size_sum;
		dev->txqs[i].q_data_offset = (u32) dev->txqs[i].q_data -
					(u32) dev->mem_base;
		dev->txqs[i].q_data_size = vpu_hfi_q_size(TX_Q_IDX_TO_Q_ID(i));
		dev->txqs[i].state = HFI_QUEUE_STATE_DISABLED;
		vpu_hfi_init_qhdr(dev->txqs[i].q_hdr, true,
				dev->txqs[i].q_data_offset,
				dev->txqs[i].q_data_size);
		qmem_size_sum += vpu_hfi_q_size(TX_Q_IDX_TO_Q_ID(i));

		mutex_init(&dev->txqs[i].lock);
	}

	/* assign the RX queue array, qhdr not initialized */
	qhdr++;
	for (i = 0; i < VPU_MAX_RXQ_NUM; i++) {
		dev->rxqs[i].cid = i;
		dev->rxqs[i].priv = NULL;
		dev->rxqs[i].dev = dev;
		dev->rxqs[i].q_hdr = qhdr + i * 2;
		dev->rxqs[i].q_data = qmem_addr + qmem_size_sum;
		dev->rxqs[i].q_data_offset = (u32) dev->rxqs[i].q_data -
					(u32) dev->mem_base;
		dev->rxqs[i].q_data_size = vpu_hfi_q_size(RX_Q_IDX_TO_Q_ID(i));
		dev->rxqs[i].state = HFI_QUEUE_STATE_DISABLED;
		vpu_hfi_init_qhdr(dev->rxqs[i].q_hdr, false,
				dev->rxqs[i].q_data_offset,
				dev->rxqs[i].q_data_size);
		qmem_size_sum += vpu_hfi_q_size(RX_Q_IDX_TO_Q_ID(i));

		mutex_init(&dev->rxqs[i].lock);
		INIT_WORK(&dev->rxqs[i].rx_work, _vpu_hfi_rx_work_handler);
	}

	spin_unlock_irqrestore(&dev->qlock, flags);

	/* sanity check on the mem size */
	if (unlikely(total_size < qtbl_size + qmem_size_sum))
		return -ENOMEM;

	return 0;
}

static void raw_handle_rx_msgs_poll(struct vpu_hfi_device *hdevice)
{
	int i;
	struct vpu_hfi_rxq_info *rxq;

	if (unlikely(!hdevice->handle_msg)) {
		pr_err("No rx handler callback: %p\n", hdevice);
		return;
	}

	for (i = 0; i < VPU_MAX_RXQ_NUM; i++) {
		int rc;
		int more_data;
		struct hfi_queue_header *qhdr;

		rxq = &hdevice->rxqs[i];

		if (unlikely(rxq->state != HFI_QUEUE_STATE_ACTIVE))
			continue;

		qhdr = (struct hfi_queue_header *) rxq->q_hdr;
		if (rxq->cid == VPU_LOGGING_CHANNEL_ID) {
			/* for logging wake up user thread */
			if (qhdr->qhdr_write_idx != qhdr->qhdr_read_idx)
				vpu_wakeup_fw_logging_wq();
			continue;
		}

		do {
			more_data = 0;
			rc = raw_read_packet(rxq, rxq->msg_buf, MAX_MSG_SIZE,
					&more_data);

			/* no data available or error in reading packet */
			if (rc <= 0)
				break;

			/* inform firmware that new space available */
			if ((qhdr->qhdr_tx_req == 1) && (hdevice->vpu_sub_sys))
				raw_hfi_int_fire(hdevice->reg_base);

			if (!more_data)
				qhdr->qhdr_rx_req = 1;

			/* call client's callback */
			hdevice->handle_msg(rxq->cid,
					(struct vpu_hfi_packet *)rxq->msg_buf,
					rxq->priv);
		} while (more_data);
	}
};

static void vpu_hfi_shutdown(struct vpu_hfi_device *hdevice)
{
	subsystem_put(hdevice->vpu_sub_sys);
	hdevice->vpu_sub_sys = NULL;
}

static int vpu_hfi_boot(struct vpu_hfi_device *hdevice)
{
	int timeout;
	void *subsys;

	/* load firmware, blocking call */
	subsys = subsystem_get("vpu");
	if (unlikely(IS_ERR_OR_NULL(subsys))) {
		pr_err("failed to download firmware\n");
		return (int)subsys;
	}

	/* tell VPU the lower 32 bits of IPC mem phy address */
	raw_hfi_qtbl_paddr_set(hdevice->reg_base,
			(u32)(hdevice->platform_resouce->mem_base_phy));

	/* enable interrupt to VPU */
	raw_hfi_int_enable(hdevice->reg_base);

	/* wait for VPU FW up (poll status register) */
	timeout = vpu_pil_timeout / 20;
	while (!raw_hfi_fw_ready(hdevice->reg_base)) {
		if (timeout-- <= 0) {
			/* FW bootup timed out */
			pr_err("VPU FW bootup timeout\n");
			subsystem_put(subsys);
			return -ETIMEDOUT;
		}
		msleep(20);
	}

	hdevice->vpu_sub_sys = subsys;

	/*
	 * fire one interrupt, in case there might be data in the IPC queue
	 * already
	 */
	raw_hfi_int_fire(hdevice->reg_base);
	return 0;
}

/*
 * to handle a watchdog timeout
 * notifies upper layer
 */
static void _vpu_hfi_watchdog_work_handler(struct work_struct *work)
{
	struct vpu_hfi_device *hdevice;

	hdevice = container_of(work, struct vpu_hfi_device, watchdog_work);

	subsystem_crashed("vpu");

	if (likely(hdevice))
		/* notify via system channel */
		hdevice->handle_event(hdevice->rxqs[VPU_SYSTEM_CHANNEL_ID].cid,
				VPU_LOCAL_EVENT_WD,
				hdevice->rxqs[VPU_SYSTEM_CHANNEL_ID].priv);
}

static void _vpu_hfi_poll_work_handler(struct work_struct *work)
{
	struct vpu_hfi_device *hdevice;

	hdevice = container_of((struct delayed_work *)work,
			struct vpu_hfi_device, poll_work);

	if (!hdevice->use_poll)
		return;

	/* handle any pending messages */
	raw_handle_rx_msgs_poll(hdevice);

	/* queue the same work again */
	queue_delayed_work(hdevice->main_workq, &hdevice->poll_work,
			msecs_to_jiffies(VPU_POLL_INTERVAL_MS));
}

irqreturn_t _vpu_hfi_ipc_isr(int irq, void *dev)
{
	int i;
	struct vpu_hfi_rxq_info *rxq;
	struct vpu_hfi_device *hdevice = dev;

	/* ack the interrupt before handling it */
	raw_hfi_int_ack(hdevice->reg_base);

	/* lock the qtable before checking the queues */
	spin_lock(&hdevice->qlock);

	for (i = 0; i < VPU_MAX_RXQ_NUM; i++) {
		struct hfi_queue_header *qhdr;

		rxq = &hdevice->rxqs[i];
		if (rxq->state != HFI_QUEUE_STATE_ACTIVE)
			continue;

		qhdr = (struct hfi_queue_header *) rxq->q_hdr;
		if (vpu_hfi_q_empty(qhdr))
			continue;

		queue_work(hdevice->main_workq, &rxq->rx_work);
	}

	spin_unlock(&hdevice->qlock);

	return IRQ_HANDLED;
}

irqreturn_t _vpu_hfi_wdog_isr(int irq, void *dev)
{
	struct vpu_hfi_device *hdevice = dev;
	pr_debug("Watchdog Bite ISR\n");

	disable_irq_nosync(irq);
	hdevice->watchdog_bited = 1;
	queue_work(hdevice->main_workq, &hdevice->watchdog_work);

	return IRQ_HANDLED;
}

/*
 * vpu_hfi_init
 * static initialization, called once during boot
 */
int vpu_hfi_init(struct vpu_platform_resources *res)
{
	int rc;
	struct vpu_hfi_device *hdevice = &g_hfi_device;

	hdevice->platform_resouce = res;

	pr_debug("Entering function\n");

	spin_lock_init(&hdevice->qlock);

	/* create the kernel work queue */
	hdevice->main_workq = alloc_workqueue("vpu_rx_workq",
			WQ_NON_REENTRANT | WQ_UNBOUND, 0);

	if (unlikely(!hdevice->main_workq)) {
		pr_err("create rx workq failed\n");
		rc = -ENOMEM;
		goto workq_fail;
	}

	INIT_WORK(&hdevice->watchdog_work, _vpu_hfi_watchdog_work_handler);
	INIT_DELAYED_WORK(&hdevice->poll_work, _vpu_hfi_poll_work_handler);

	/* map the CSR register */
	hdevice->reg_base =
		ioremap_nocache(res->register_base_phy, res->register_size);
	if (unlikely(!hdevice->reg_base)) {
		pr_err("could not map reg addr 0x%x of size 0x%x\n",
			(u32) res->register_base_phy, res->register_size);
		rc = -ENODEV;
		goto error_map_fail1;
	} else {
		pr_debug("CSR mapped from 0x%08x to 0x%p\n",
			(u32) res->register_base_phy, hdevice->reg_base);
	}

	/* map the IPC mem */
	hdevice->mem_base =
		ioremap_nocache(res->mem_base_phy, res->mem_size);
	if (unlikely(!hdevice->mem_base)) {
		pr_err("could not map mem addr 0x%x of size 0x%x\n",
			(u32) res->mem_base_phy, res->mem_size);
		rc = -ENODEV;
		goto error_map_fail2;
	} else {
		pr_debug("MEM mapped from 0x%08x to 0x%p\n",
				(u32) res->mem_base_phy, hdevice->mem_base);
	}

	/* map the VBIF registers */
	if (res->vbif_size > 0) {
		hdevice->vbif_base =
			ioremap_nocache(res->vbif_base_phy, res->vbif_size);
		if (unlikely(!hdevice->vbif_base)) {
			pr_err("could not map vbif addr 0x%x of size 0x%x\n",
				(u32) res->vbif_base_phy, res->vbif_size);
			rc = -ENODEV;
			goto error_map_fail3;
		} else {
			pr_debug("VBIF mapped from 0x%08x to 0x%p\n",
				(u32) res->vbif_base_phy, hdevice->vbif_base);
		}
	}

	/* init the IPC queues */
	rc = vpu_hfi_queues_init(hdevice, hdevice->mem_base, res->mem_size);
	if (unlikely(rc)) {
		pr_err("IPC queue init failed\n");
		goto error_hfiq;
	}

	hdevice->watchdog_enabled = 1;
	return 0;

error_hfiq:
	if (res->vbif_size > 0)
		iounmap(hdevice->vbif_base);
error_map_fail3:
	iounmap(hdevice->mem_base);
error_map_fail2:
	iounmap(hdevice->reg_base);
error_map_fail1:
	destroy_workqueue(hdevice->main_workq);
workq_fail:
	return rc;
}

/*
 * vpu_hfi_deinit
 * cleanup the static initialization, called once during system shutdown
 */
void vpu_hfi_deinit(void)
{
	struct vpu_hfi_device *hdevice = &g_hfi_device;

	iounmap(hdevice->reg_base);
	iounmap(hdevice->mem_base);
	if (hdevice->platform_resouce->vbif_size > 0)
		iounmap(hdevice->vbif_base);
	destroy_workqueue(hdevice->main_workq);
}

static void program_preset_registers(void)
{
	struct vpu_hfi_device *hdevice = &g_hfi_device;
	struct vpu_platform_resources *res = hdevice->platform_resouce;
	struct reg_value_set *vbif_regs = &res->vbif_reg_set;

	if (res->vbif_size > 0 && vbif_regs->count && vbif_regs->table) {
		int i;
		for (i = 0; i < vbif_regs->count; i++) {
			if (vbif_regs->table[i].reg_offset > res->vbif_size) {
				pr_warn("Preset reg offset 0x%08x not mapped\n",
						vbif_regs->table[i].reg_offset);
			} else {
				pr_debug("Writing offset 0x%08x value 0x%08x\n",
					vbif_regs->table[i].reg_offset,
					vbif_regs->table[i].value);
				raw_hfi_reg_write(hdevice->vbif_base +
						vbif_regs->table[i].reg_offset,
						vbif_regs->table[i].value);
			}
		}
	}
}

/*
 *  vpu_hfi_start
 *  runtime initialization to boot VPU
 *  caller expected to do the reference counting
 */
int vpu_hfi_start(hfi_handle_msg msg_handler,
		hfi_handle_event event_handler)
{
	int rc = 0;
	struct vpu_hfi_device *hdevice = &g_hfi_device;
	struct vpu_platform_resources *res = hdevice->platform_resouce;

	hdevice->handle_msg = msg_handler;
	hdevice->handle_event = event_handler;

	/* request interrupts */
	hdevice->irq_wd = res->irq_wd;
	hdevice->irq_no = res->irq;

	hdevice->watchdog_bited = 0;

	program_preset_registers();

	rc = vpu_hfi_boot(hdevice);
	if (rc)
		goto error_boot;

	rc = request_irq(hdevice->irq_wd, _vpu_hfi_wdog_isr,
			IRQF_TRIGGER_HIGH, "vpu", hdevice);
	if (unlikely(rc)) {
		pr_err("request_irq failed (watchdog irq)\n");
		goto error_irq1_fail;
	}

	if (!hdevice->watchdog_enabled)
		disable_irq_nosync(hdevice->irq_wd);

	if (hdevice->irq_no) {
		hdevice->use_poll = 0;
		rc = request_irq(hdevice->irq_no, _vpu_hfi_ipc_isr,
				IRQF_TRIGGER_HIGH, "vpu", hdevice);
		if (unlikely(rc)) {
			pr_err("request_irq failed (IPC irq)\n");
			goto error_irq2_fail;
		}
	} else {
		/* use polling if no IPC interrupt */
		hdevice->use_poll = 1;
		queue_delayed_work(hdevice->main_workq, &hdevice->poll_work,
				msecs_to_jiffies(VPU_POLL_INTERVAL_MS));
	}

	pr_debug("interrupts registered\n");
	return 0;

error_irq2_fail:
	free_irq(hdevice->irq_wd, hdevice);
error_irq1_fail:
	vpu_hfi_shutdown(hdevice);
error_boot:
	return rc;
}

void vpu_hfi_stop(void)
{
	struct vpu_hfi_device *hdevice = &g_hfi_device;

	disable_irq(hdevice->irq_no);
	disable_irq(hdevice->irq_wd);

	if (unlikely(hdevice->use_poll)) {
		hdevice->use_poll = 0;
		flush_delayed_work(&hdevice->poll_work);
	}

	/* let all queued work to finish */
	flush_workqueue(hdevice->main_workq);

	hdevice->handle_msg = NULL;
	hdevice->handle_event = NULL;

	/* free the interrupt */
	free_irq(hdevice->irq_no, hdevice);
	free_irq(hdevice->irq_wd, hdevice);

	if (!hdevice->watchdog_bited) {
		if (!raw_hfi_fw_halted(hdevice->reg_base)) {
			msleep(20);
			if (!raw_hfi_fw_halted(hdevice->reg_base))
				pr_warn("firmware not halted!\n");
		}
	}

	vpu_hfi_shutdown(hdevice);
}

/*
 * reset the read/write index of the Tx/Rx queues
 * also set the queue status to ACTIVE
 */
void vpu_hfi_enable(u32 cid, void *priv)
{
	unsigned long flags;
	struct vpu_hfi_device *dev = &g_hfi_device;

	if (likely(cid < VPU_MAX_TXQ_NUM)) {
		struct vpu_hfi_txq_info *tq;
		bool updated = false;

		tq = &dev->txqs[cid];

		mutex_lock(&tq->lock);
		if (tq->state == HFI_QUEUE_STATE_DISABLED) {
			/* init the queue hdr */
			vpu_hfi_init_qhdr(tq->q_hdr, true,
					tq->q_data_offset, tq->q_data_size);

			/* save the private data */
			tq->priv = priv;

			/* enable the queue, if its size is > 0 */
			if (vpu_hfi_q_size(TX_Q_IDX_TO_Q_ID(cid)) > 0) {
				tq->state = HFI_QUEUE_STATE_ACTIVE;
				vpu_hfi_enable_qhdr(tq->q_hdr);
				updated = true;
			}
		}
		mutex_unlock(&tq->lock);

		/* inc the active q number in qtable */
		if (updated) {
			spin_lock_irqsave(&dev->qlock, flags);
			dev->qtbl->qtbl_num_active_q++;
			spin_unlock_irqrestore(&dev->qlock, flags);
		}
	}

	if (likely(cid < VPU_MAX_RXQ_NUM)) {
		struct vpu_hfi_rxq_info *rq;
		bool updated = false;

		rq = &dev->rxqs[cid];

		mutex_lock(&rq->lock);
		if (rq->state == HFI_QUEUE_STATE_DISABLED) {
			/* init the queue hdr */
			vpu_hfi_init_qhdr(rq->q_hdr, false,
					rq->q_data_offset, rq->q_data_size);

			/* save the private data */
			rq->priv = priv;

			/* enable the queue, if its size is > 0 */
			if (vpu_hfi_q_size(RX_Q_IDX_TO_Q_ID(cid)) > 0) {
				vpu_hfi_enable_qhdr(rq->q_hdr);

				/* ISR will access qhdr once state is active*/
				mb();
				rq->state = HFI_QUEUE_STATE_ACTIVE;

				updated = true;
			}
		}
		mutex_unlock(&rq->lock);

		/* inc the active q number in qtable */
		if (updated) {
			spin_lock_irqsave(&dev->qlock, flags);
			dev->qtbl->qtbl_num_active_q++;
			spin_unlock_irqrestore(&dev->qlock, flags);
		}
	}
}

void vpu_hfi_disable(u32 cid)
{
	unsigned long flags;
	struct vpu_hfi_device *dev = &g_hfi_device;

	if (likely(cid < VPU_MAX_TXQ_NUM)) {
		struct vpu_hfi_txq_info *tq;
		bool updated = false;

		tq = &dev->txqs[cid];

		mutex_lock(&tq->lock);
		if (tq->state == HFI_QUEUE_STATE_ACTIVE) {
			vpu_hfi_disable_qhdr(tq->q_hdr);
			tq->state = HFI_QUEUE_STATE_DISABLED;
			updated = true;
		}
		mutex_unlock(&tq->lock);

		/* decrement the active q number in qtable */
		if (updated) {
			spin_lock_irqsave(&dev->qlock, flags);
			dev->qtbl->qtbl_num_active_q--;
			spin_unlock_irqrestore(&dev->qlock, flags);
		}
	}

	if (likely(cid < VPU_MAX_RXQ_NUM)) {
		struct vpu_hfi_rxq_info *rq;
		bool updated = false;

		rq = &dev->rxqs[cid];

		mutex_lock(&rq->lock);
		if (rq->state == HFI_QUEUE_STATE_ACTIVE) {
			rq->state = HFI_QUEUE_STATE_DISABLED;
			vpu_hfi_disable_qhdr(rq->q_hdr);
			updated = true;
		}
		mutex_unlock(&rq->lock);

		/* decrement the active q number in qtable */
		if (updated) {
			spin_lock_irqsave(&dev->qlock, flags);
			dev->qtbl->qtbl_num_active_q--;
			spin_unlock_irqrestore(&dev->qlock, flags);
		}
	}
}

/* API to send a packet */
int vpu_hfi_write_packet(u32 cid, struct vpu_hfi_packet *packet)
{
	return vpu_hfi_write_packet_extra(cid, packet, NULL, 0);
}

/* API to send a packet, sourced from several buffers */
int vpu_hfi_write_packet_extra(u32 cid, struct vpu_hfi_packet *packet,
		u8 *extra_data, u32 extra_size)
{
	int rc;
	struct vpu_hfi_txq_info *tq;

	if (unlikely(cid >= VPU_MAX_TXQ_NUM))
		return -EINVAL;

	/* get the queue */
	tq = &g_hfi_device.txqs[cid];

	if (unlikely(tq->state == HFI_QUEUE_STATE_DISABLED)) {
		/* error, queue not in working state */
		pr_err("TX queue not inited, cannot send\n");
		rc = -EPERM;
	} else {
		mutex_lock(&tq->lock);
		rc = raw_write_data(tq, (u8 *)packet, packet->size - extra_size,
						extra_data, extra_size);
		mutex_unlock(&tq->lock);
	}

	return rc;
}

int vpu_hfi_write_packet_commit(u32 cid, struct vpu_hfi_packet *packet)
{
	int rc;

	rc = vpu_hfi_write_packet(cid, packet);

	/* generate interrupt no matter if the TX is successful */
	if (g_hfi_device.vpu_sub_sys)
		raw_hfi_int_fire(g_hfi_device.reg_base);

	return rc;
}

int vpu_hfi_write_packet_extra_commit(u32 cid, struct vpu_hfi_packet *packet,
		u8 *extra_data, u32 extra_size)
{
	int rc;

	rc = vpu_hfi_write_packet_extra(cid, packet, extra_data, extra_size);

	/* generate interrupt no matter if the TX is successful */
	if (g_hfi_device.vpu_sub_sys)
		raw_hfi_int_fire(g_hfi_device.reg_base);

	return rc;
}

#define LOG_BUF_SIZE	128

#define add2buf(dest, dest_size, temp, temp_size, __fmt, arg...) \
	do { \
		snprintf(temp, temp_size, __fmt, ## arg); \
		strlcat(dest, temp, dest_size); \
	} while (0)

/* 26 bytes per line -> 364 bytes required */
static void dump_queue_header(struct hfi_queue_header *qhdr,
		char *dest, size_t dest_size)
{
	/* temporary buffer */
	size_t ts = LOG_BUF_SIZE;
	char t[LOG_BUF_SIZE];
	/* destination buffer */
	size_t ds = dest_size;
	char *d = dest;

	add2buf(d, ds, t, ts, "\tstatus       %10d\n", qhdr->qhdr_status);
	add2buf(d, ds, t, ts, "\tstart_addr   0x%08x\n", qhdr->qhdr_start_addr);
	add2buf(d, ds, t, ts, "\tq_size       %10d\n", qhdr->qhdr_q_size);
	add2buf(d, ds, t, ts, "\tread_idx     0x%08x\n", qhdr->qhdr_read_idx);
	add2buf(d, ds, t, ts, "\twrite_idx    0x%08x\n", qhdr->qhdr_write_idx);
}

/*
 * dump the contents of the IPC queue table header into buf
 * returns the number of valid bytes in buf
 * caller needs to init the buffer with a string!!
 */
int vpu_hfi_dump_queue_headers(int idx, char *buf, size_t buf_size)
{
	struct vpu_hfi_device *dev = &g_hfi_device;
	struct hfi_queue_header *qhdr;
	char string[LOG_BUF_SIZE];

	/* TX-i queue */
	qhdr = dev->txqs[idx].q_hdr;
	if (qhdr->qhdr_q_size > 0) {
		add2buf(buf, buf_size, string, LOG_BUF_SIZE,
				"\nTx-%d queue header:\n", idx);
		dump_queue_header(qhdr, buf, buf_size);
	}

	/* RX-i queue */
	qhdr = dev->rxqs[idx].q_hdr;
	if (qhdr->qhdr_q_size > 0) {
		add2buf(buf, buf_size, string, LOG_BUF_SIZE,
				"\nRx-%d queue header:\n", idx);
		dump_queue_header(qhdr, buf, buf_size);
	}

	return strlcat(buf, "\n", buf_size);
}

#ifdef CONFIG_DEBUG_FS

void vpu_hfi_set_pil_timeout(u32 pil_timeout)
{
	vpu_pil_timeout = pil_timeout;
}

/*
 * return packet size read, or < 0 for error
 */
int vpu_hfi_read_log_data(u32 cid, char *buf, int buf_size)
{
	struct vpu_hfi_device *hdevice = &g_hfi_device;
	struct vpu_hfi_rxq_info *rxq = &hdevice->rxqs[cid];
	int more_data;
	int res = 0;
	int rc;
	char *wr_buf = buf;
	int max_size = buf_size;

	if (unlikely(cid != VPU_LOGGING_CHANNEL_ID)) {
		pr_err("must only access the logging queue!\n");
		res = -EACCES;
		goto exit;
	}

	mutex_lock(&rxq->lock);

	do {
		struct hfi_queue_header *qhdr =
				(struct hfi_queue_header *) rxq->q_hdr;

		more_data = 0;
		rc = raw_read_packet(rxq, wr_buf, max_size, &more_data);

		/* no data available or error in reading packet */
		if (rc <= 0)
			break;

		/* inform firmware that new space available */
		if ((qhdr->qhdr_tx_req == 1) && (hdevice->vpu_sub_sys))
			raw_hfi_int_fire(hdevice->reg_base);

		if (!more_data)
			qhdr->qhdr_rx_req = 1;

		res += rc;
		wr_buf += rc;
		max_size -= rc;

		if (!max_size)
			break;

	} while (more_data);

	mutex_unlock(&rxq->lock);

exit:
	pr_debug("return %d\n", res);
	return res;
}

/*
 * dump the contents of the IPC queue table header into buf
 * returns the number of valid bytes in buf
 */
static int dump_qtbl_header(char *buf, size_t buf_size)
{
	struct vpu_hfi_device *dev = &g_hfi_device;
	struct hfi_queue_table_header *qtbl = dev->qtbl;
	/* temporary buffer */
	size_t ts = LOG_BUF_SIZE;
	char t[ts];
	/* destination buffer */
	size_t ds = buf_size;
	char *d = buf;

	strlcat(buf, "\nQueue table header:\n", buf_size);

	add2buf(d, ds, t, ts, "\tversion      %10d\n", qtbl->qtbl_version);
	add2buf(d, ds, t, ts, "\tsize         %10d\n", qtbl->qtbl_size);
	add2buf(d, ds, t, ts, "\tqhdr0_offset 0x%08x\n",
						       qtbl->qtbl_qhdr0_offset);
	add2buf(d, ds, t, ts, "\tqhdr_size    %10d\n", qtbl->qtbl_qhdr_size);
	add2buf(d, ds, t, ts, "\tnum_q        %10d\n", qtbl->qtbl_num_q);
	add2buf(d, ds, t, ts, "\tnum_active_q %10d\n", qtbl->qtbl_num_active_q);
	return strlcat(d, "\n", ds);
}

size_t vpu_hfi_print_queues(char *buf, size_t buf_size)
{
	int i;
	strlcpy(buf, "", buf_size); /* clear buffer */

	/* print queue table header in buffer */
	dump_qtbl_header(buf, buf_size);

	/* print all queue headers in buffer */
	for (i = 0; i < VPU_CHANNEL_ID_MAX; i++)
		vpu_hfi_dump_queue_headers(i, buf, buf_size);

	return strlcat(buf, "", buf_size);
}

struct addr_range {
	u32 start;
	u32 end;
};

static struct addr_range restricted_csr_addrs[] = {
	/* start and end offsets of inaccessible address ranges */
	{ 0x0000, 0x000F },
	{ 0x0018, 0x001B },
	{ 0x0020, 0x0037 },
	{ 0x00C0, 0x00DF },
	{ 0x01A0, 0x0FFF },
};

/* registers which should not be written through debugfs
 * (may interfere with the normal operation of the driver
 * which makes use of these registers)
 */
static u32 no_write_csr_regs[] = {
	VPU_CSR_APPS_SGI_STS,
	VPU_CSR_APPS_SGI_CLR,
	VPU_CSR_FW_SGI_EN_SET,
	VPU_CSR_FW_SGI_EN_CLR,
	VPU_CSR_FW_SGI_FORCELEVEL,
	VPU_CSR_FW_SGI_STS,
	VPU_CSR_FW_SGI_CLR,
	VPU_CSR_FW_SGI_TRIG,
	VPU_CSR_SW_SCRATCH0_STS,
	VPU_CSR_SW_SCRATCH1_QTBL_INFO,
	VPU_CSR_SW_SCRATCH2_QTBL_ADDR,
};

int vpu_hfi_write_csr_reg(u32 off, u32 val)
{
	struct vpu_hfi_device *hdevice = &g_hfi_device;
	u32 p_base = (u32)(hdevice->platform_resouce->register_base_phy);
	void __iomem *write_addr = hdevice->reg_base + off;
	int i;

	if (off > VPU_CSR_LAST_REG) {
		pr_err("attempting to write outside of addr range\n");
		return -EFAULT;
	}

	if (off % 4) {
		pr_err("addr must be 32-bit word-aligned\n");
		return -EFAULT;
	}

	for (i = 0; i < ARRAY_SIZE(no_write_csr_regs); i++) {
		if (off == no_write_csr_regs[i]) {
			pr_err("not allowed write this reg through debugfs\n");
			return -EFAULT;
		}
	}

	for (i = 0; i < ARRAY_SIZE(restricted_csr_addrs); i++) {
		if (off >= restricted_csr_addrs[i].start
			&& off <= restricted_csr_addrs[i].end) {
			pr_err("attempting to write restricted addr range\n");
			return -EFAULT;
		}
	}

	raw_hfi_reg_write(write_addr, val);

	pr_debug("wrote val: 0x%08x at addr: 0x%08x\n", val, p_base + off);
	return 0;
}

int vpu_hfi_dump_csr_regs(char *buf, size_t buf_size)
{
	struct vpu_hfi_device *hdevice = &g_hfi_device;
	u32 p_base = (u32)(hdevice->platform_resouce->register_base_phy);
	u32 off;
	char temp[32];
	int i = 0, skip = 0, temp_size = 32;

	strlcpy(buf, "", buf_size);

	/* read one at a time. Print 4 registers per line */
	for (off = 0; off <= VPU_CSR_LAST_REG; off += sizeof(u32)) {

		if (i >= ARRAY_SIZE(restricted_csr_addrs))
			break;

		if ((off % 0x10) == 0) {
			snprintf(temp, temp_size, "@0x%08x -", off + p_base);
			strlcat(buf, temp, buf_size);
		}

		if (off >= restricted_csr_addrs[i].start &&
			off <= restricted_csr_addrs[i].end) {
			skip = 1;
			snprintf(temp, temp_size, " xxxxxxxxxx");
			strlcat(buf, temp, buf_size);
		} else {
			if (skip) {
				i++;
				skip = 0;
			}

			snprintf(temp, temp_size, " 0x%08x",
				raw_hfi_reg_read(hdevice->reg_base + off));
			strlcat(buf, temp, buf_size);
		}

		if ((off % 0x10) == 0xc) {
			snprintf(temp, temp_size, "\n");
			strlcat(buf, temp, buf_size);
		}
	}

	return strlcat(buf, "\n", buf_size);
}

int vpu_hfi_dump_smem_line(char *buf, size_t size, u32 offset)
{
	struct vpu_hfi_device *hdevice = &g_hfi_device;
	u32 v_base = (u32)hdevice->mem_base;
	u32 p_base = (u32)(hdevice->platform_resouce->mem_base_phy);
	u32 mem_size = hdevice->platform_resouce->mem_size;
	/* temporary buffer */
	int ts = SZ_64;
	char t[SZ_64];
	int i;

	if (unlikely(!hdevice->mem_base)) {
		const char err[] =
			"vpu_hfi_dump_smem, cannot access mem base addr!";
		pr_err("%s\n", err);
		snprintf(buf, sizeof(err), "%s", err);
		return 0;
	}

	/* sanity check on the mem access */
	if (offset >= mem_size)
		return -EACCES;
	if (size < (14 + 4 * 11 + 2))
		/* @0x%08x - 0x%08x 0x%08x 0x%08x 0x%08x\n\0 */
		return -ENOMEM;

	/*
	 * create a line such as @addr - word1 word2 word3 word4
	 * as long as the words are within the share mem area
	 */
	strlcpy(buf, "@", size);
	add2buf(buf, size, t, ts, "0x%08x -", p_base + offset);
	for (i = 0; i < 4; i++) {
		u32 cur_offset = offset + i * sizeof(u32);

		if (cur_offset < mem_size)
			add2buf(buf, size, t, ts, " 0x%08x",
					*((u32 *)(v_base + cur_offset)));
		else
			break;
	}

	return strlcat(buf, "\n", size);
}

void vpu_hfi_set_watchdog(u32 enable)
{
	struct vpu_hfi_device *hdevice = &g_hfi_device;

	if (enable && (!hdevice->watchdog_enabled)) {
		/* enable watchdog */
		hdevice->watchdog_enabled = 1;
		enable_irq(hdevice->irq_wd);
	} else if ((!enable) && hdevice->watchdog_enabled) {
		/* disable watchdog */
		hdevice->watchdog_enabled = 0;
		disable_irq_nosync(hdevice->irq_wd);
	}
}

#endif /* CONFIG_DEBUG_FS */
