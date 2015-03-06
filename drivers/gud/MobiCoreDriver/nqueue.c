/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>

#include "public/mc_linux.h"
#include "public/mc_admin.h"

#include "mci/mcimcp.h"	/* struct mcp_buffer */
#include "mci/mcinq.h"

#include "main.h"
#include "fastcall.h"
#include "debug.h"
#include "logging.h"
#include "mcp.h"
#include "scheduler.h"
#include "nqueue.h"

static void mc_irq_worker(struct work_struct *data);
DECLARE_WORK(irq_work, mc_irq_worker);

/** Retrieves the first element from the queue.
*
* @return 0 if queue is not empty and first notification Queue element
*              was copied.
* @return EAGAIN if the queue is empty or .
* @return -errno if something goes wrong
*/
static inline int nq_pop_notification(struct notification *nf)
{
	struct notification_queue *rx = g_ctx.nq.rx;

	if (unlikely(!nf))
		return -EINVAL;

	if ((rx->hdr.write_cnt - rx->hdr.read_cnt) <= 0)
		return -EAGAIN;

	*nf = rx->notification[rx->hdr.read_cnt & (rx->hdr.queue_size - 1)];
	rx->hdr.read_cnt++;
	return 0;
}

static void mc_irq_worker(struct work_struct *data)
{
	struct notification nf;

	/* Deal with all pending notifications in one go */
	while (!nq_pop_notification(&nf)) {
		if (nf.session_id == SID_MCP) {
			dev_dbg(g_ctx.mcd, "Notify <t-base, payload=%d",
				nf.payload);
			mcp_wake_up(nf.payload);
		} else {
			dev_dbg(g_ctx.mcd, "Notify session %x, payload=%d",
				nf.session_id, nf.payload);
			mc_wakeup_session(nf.session_id, nf.payload);
		}
	}

	/*
	 * Finished processing notifications. It does not matter whether
	 * there actually were any notification or not.  S-SIQs can also
	 * be triggered by an SWd driver which was waiting for a FIQ.
	 * In this case the S-SIQ tells NWd that SWd is no longer idle
	 * an will need scheduling again.
	 */
	mc_dev_schedule();
}

/*
 * This function represents the interrupt function of the mcDrvModule.
 * It signals by incrementing of an event counter and the start of the read
 * waiting queue, the read function a interrupt has occurred.
 */
static irqreturn_t irq_handler(int intr, void *arg)
{
	/* wake up thread to continue handling this interrupt */
	schedule_work(&irq_work);
	mobicore_log_read();
	return IRQ_HANDLED;
}

int nqueue_init(uint16_t txq_length, uint16_t rxq_length)
{
	int ret = 0;
	uint order;
	ulong mci;
	size_t queue_sz;
	struct mcp_buffer *mcp_buffer;

	init_rwsem(&g_ctx.nq.rx_lock);
	g_ctx.mcp = NULL;

	g_ctx.mci_base = NULL;
	if (txq_length == 0 || rxq_length == 0 ||
	    ((txq_length & (txq_length - 1)) != 0) ||
	    ((rxq_length & (rxq_length - 1)) != 0)) {
		MCDRV_DBG_WARN("TX/RX queues lengths must be power of 2");
		return -EINVAL;
	}

	queue_sz = sizeof(struct notification_queue_header) +
	    txq_length * sizeof(struct notification);
	queue_sz += sizeof(struct notification_queue_header) +
	    rxq_length * sizeof(struct notification);
	queue_sz = ALIGN(queue_sz, 4);
	if (queue_sz + sizeof(*mcp_buffer) > (uint16_t)-1) {
		MCDRV_DBG_WARN("queues too large (more than 64k), sorry...");
		return -EINVAL;
	}

	order = get_order(queue_sz + sizeof(*mcp_buffer));
	g_ctx.mci_order = order;

	mci = __get_free_pages(GFP_USER | __GFP_ZERO, order);
	if (mci == 0) {
		MCDRV_DBG_WARN("get_free_pages failed");
		return -ENOMEM;
	}

	g_ctx.nq.tx = (struct notification_queue *)mci;
	g_ctx.nq.tx->hdr.queue_size = txq_length;
	mci += sizeof(struct notification_queue_header) +
	    txq_length * sizeof(struct notification);

	g_ctx.nq.rx = (struct notification_queue *)mci;
	mci += sizeof(struct notification_queue_header) +
	    rxq_length * sizeof(struct notification);
	g_ctx.nq.rx->hdr.queue_size = rxq_length;

	mcp_buffer = (void *)ALIGN(mci, 4);

	g_ctx.mci_base_pa = virt_to_phys(g_ctx.nq.tx);

	/* Call the INIT fastcall to setup MobiCore initialization */
	ret = mc_fc_init(mcp_buffer, g_ctx.mci_base_pa, g_ctx.mci_base,
			 queue_sz);
	if (!ret)
		ret = mcp_init(mcp_buffer);

	if (ret)
		nqueue_cleanup();

	MCDRV_DBG_VERBOSE("done, return %d", ret);

	return ret;
}

void nqueue_cleanup(void)
{
	ulong flags;
	void *mci_base = NULL;
	uint order = 0;

	mcp_cleanup();

	down_write(&g_ctx.nq.rx_lock);
	local_irq_save(flags);

	if (g_ctx.mci_base) {
		mci_base = g_ctx.mci_base;
		order = g_ctx.mci_order;
	}
	g_ctx.mci_base = NULL;
	g_ctx.nq.tx = NULL;
	g_ctx.nq.rx = NULL;

	g_ctx.mci_order = 0;
	g_ctx.mci_base_pa = 0;

	local_irq_restore(flags);
	up_write(&g_ctx.nq.rx_lock);

	if (mci_base)
		free_pages((ulong)mci_base, order);
}

/* Set up S-SIQ interrupt handler */
int irq_handler_init(void)
{
	return request_irq(MC_INTR_SSIQ, irq_handler, IRQF_TRIGGER_RISING,
			   MC_ADMIN_DEVNODE, NULL);
}

void irq_handler_exit(void)
{
	flush_scheduled_work();
	free_irq(MC_INTR_SSIQ, g_ctx.mcd);
}
