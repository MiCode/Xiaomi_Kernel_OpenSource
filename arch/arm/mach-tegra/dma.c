/*
 * arch/arm/mach-tegra/dma.c
 *
 * System DMA driver for NVIDIA Tegra SoCs
 *
 * Copyright (c) 2008-2012, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/syscore_ops.h>
#include <mach/dma.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/clk.h>

#include "apbio.h"
#include "clock.h"

#define APB_DMA_GEN				0x000
#define GEN_ENABLE				(1<<31)

#define APB_DMA_CNTRL				0x010

#define APB_DMA_IRQ_MASK			0x01c

#define APB_DMA_IRQ_MASK_SET			0x020

#define APB_DMA_SWID				0x3c
#define SWID_CHAN0				0x1

#define APB_DMA_CHAN_CSR			0x000
#define CSR_ENB					(1<<31)
#define CSR_IE_EOC				(1<<30)
#define CSR_HOLD				(1<<29)
#define CSR_DIR					(1<<28)
#define CSR_ONCE				(1<<27)
#define CSR_FLOW				(1<<21)
#define CSR_REQ_SEL_SHIFT			16
#define CSR_WCOUNT_SHIFT			2
#define CSR_WCOUNT_MASK				0xFFFC

#define APB_DMA_CHAN_STA			0x004
#define STA_BUSY				(1<<31)
#define STA_ISE_EOC				(1<<30)
#define STA_HALT				(1<<29)
#define STA_PING_PONG				(1<<28)
#define STA_COUNT_SHIFT				2
#define STA_COUNT_MASK				0xFFFC

#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
#define APB_DMA_CHAN_CSRE			0x00C
#define CSRE_PAUSE				(1<<31)
#endif

#define APB_DMA_CHAN_AHB_PTR			0x010

#define APB_DMA_CHAN_AHB_SEQ			0x014
#define AHB_SEQ_INTR_ENB			(1<<31)
#define AHB_SEQ_BUS_WIDTH_SHIFT			28
#define AHB_SEQ_BUS_WIDTH_MASK			(0x7<<AHB_SEQ_BUS_WIDTH_SHIFT)
#define AHB_SEQ_BUS_WIDTH_8			(0<<AHB_SEQ_BUS_WIDTH_SHIFT)
#define AHB_SEQ_BUS_WIDTH_16			(1<<AHB_SEQ_BUS_WIDTH_SHIFT)
#define AHB_SEQ_BUS_WIDTH_32			(2<<AHB_SEQ_BUS_WIDTH_SHIFT)
#define AHB_SEQ_BUS_WIDTH_64			(3<<AHB_SEQ_BUS_WIDTH_SHIFT)
#define AHB_SEQ_BUS_WIDTH_128			(4<<AHB_SEQ_BUS_WIDTH_SHIFT)
#define AHB_SEQ_DATA_SWAP			(1<<27)
#define AHB_SEQ_BURST_MASK			(0x7<<24)
#define AHB_SEQ_BURST_1				(4<<24)
#define AHB_SEQ_BURST_4				(5<<24)
#define AHB_SEQ_BURST_8				(6<<24)
#define AHB_SEQ_DBL_BUF				(1<<19)
#define AHB_SEQ_WRAP_SHIFT			16
#define AHB_SEQ_WRAP_MASK			(0x7<<AHB_SEQ_WRAP_SHIFT)

#define APB_DMA_CHAN_APB_PTR			0x018

#define APB_DMA_CHAN_APB_SEQ			0x01c
#define APB_SEQ_BUS_WIDTH_SHIFT			28
#define APB_SEQ_BUS_WIDTH_MASK			(0x7<<APB_SEQ_BUS_WIDTH_SHIFT)
#define APB_SEQ_BUS_WIDTH_8			(0<<APB_SEQ_BUS_WIDTH_SHIFT)
#define APB_SEQ_BUS_WIDTH_16			(1<<APB_SEQ_BUS_WIDTH_SHIFT)
#define APB_SEQ_BUS_WIDTH_32			(2<<APB_SEQ_BUS_WIDTH_SHIFT)
#define APB_SEQ_BUS_WIDTH_64			(3<<APB_SEQ_BUS_WIDTH_SHIFT)
#define APB_SEQ_BUS_WIDTH_128			(4<<APB_SEQ_BUS_WIDTH_SHIFT)
#define APB_SEQ_DATA_SWAP			(1<<27)
#define APB_SEQ_WRAP_SHIFT			16
#define APB_SEQ_WRAP_MASK			(0x7<<APB_SEQ_WRAP_SHIFT)

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
#define TEGRA_SYSTEM_DMA_CH_NR			16
#else
#define TEGRA_SYSTEM_DMA_CH_NR			32
#endif
#define TEGRA_SYSTEM_DMA_AVP_CH_NUM		4
#define TEGRA_SYSTEM_DMA_CH_MIN			0
#define TEGRA_SYSTEM_DMA_CH_MAX	\
	(TEGRA_SYSTEM_DMA_CH_NR - TEGRA_SYSTEM_DMA_AVP_CH_NUM - 1)

/* Maximum dma transfer size */
#define TEGRA_DMA_MAX_TRANSFER_SIZE		0x10000

#ifndef CONFIG_TEGRA_SIMULATION_PLATFORM
static struct clk *dma_clk;
#endif

static const unsigned int ahb_addr_wrap_table[8] = {
	0, 32, 64, 128, 256, 512, 1024, 2048
};

static const unsigned int apb_addr_wrap_table[8] = {
	0, 1, 2, 4, 8, 16, 32, 64
};

static const unsigned int bus_width_table[5] = {
	8, 16, 32, 64, 128
};

static void __iomem *general_dma_addr = IO_ADDRESS(TEGRA_APB_DMA_BASE);
typedef void (*dma_isr_handler)(struct tegra_dma_channel *ch);

#define TEGRA_DMA_NAME_SIZE 16
struct tegra_dma_channel {
	struct list_head	list;
	int			id;
	spinlock_t		lock;
	char			name[TEGRA_DMA_NAME_SIZE];
	char			client_name[TEGRA_DMA_NAME_SIZE];
	void  __iomem		*addr;
	int			mode;
	int			irq;
	dma_callback		callback;
	struct tegra_dma_req	*cb_req;
	dma_isr_handler		isr_handler;
	bool	dma_is_paused;
};

#define  NV_DMA_MAX_CHANNELS  32

static bool tegra_dma_initialized;
static DEFINE_MUTEX(tegra_dma_lock);
static DEFINE_SPINLOCK(enable_lock);

static DECLARE_BITMAP(channel_usage, NV_DMA_MAX_CHANNELS);
static struct tegra_dma_channel dma_channels[NV_DMA_MAX_CHANNELS];

static void tegra_dma_update_hw(struct tegra_dma_channel *ch,
	struct tegra_dma_req *req);
static bool tegra_dma_update_hw_partial(struct tegra_dma_channel *ch,
	struct tegra_dma_req *req);
static void handle_oneshot_dma(struct tegra_dma_channel *ch);
static void handle_continuous_dbl_dma(struct tegra_dma_channel *ch);
static void handle_continuous_sngl_dma(struct tegra_dma_channel *ch);
static void handle_dma_isr_locked(struct tegra_dma_channel *ch);

void tegra_dma_flush(struct tegra_dma_channel *ch)
{
}
EXPORT_SYMBOL(tegra_dma_flush);

static void tegra_dma_stop(struct tegra_dma_channel *ch)
{
	u32 csr;
	u32 status;

	csr = readl(ch->addr + APB_DMA_CHAN_CSR);
	csr &= ~CSR_IE_EOC;
	writel(csr, ch->addr + APB_DMA_CHAN_CSR);

	csr &= ~CSR_ENB;
	writel(csr, ch->addr + APB_DMA_CHAN_CSR);

	status = readl(ch->addr + APB_DMA_CHAN_STA);
	if (status & STA_ISE_EOC)
		writel(status, ch->addr + APB_DMA_CHAN_STA);
}

static void pause_dma(bool wait_for_burst_complete)
{
	spin_lock(&enable_lock);
	writel(0, general_dma_addr + APB_DMA_GEN);
	if (wait_for_burst_complete)
		udelay(20);
}

static void resume_dma(void)
{
	writel(GEN_ENABLE, general_dma_addr + APB_DMA_GEN);
	spin_unlock(&enable_lock);
}
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
static void pause_dma_channel(struct tegra_dma_channel *ch, bool wait_for_burst_complete)
{
	writel(CSRE_PAUSE, ch->addr + APB_DMA_CHAN_CSRE);
	if (wait_for_burst_complete)
		udelay(20);
}
#endif
static void start_head_req(struct tegra_dma_channel *ch)
{
	struct tegra_dma_req *head_req;
	struct tegra_dma_req *next_req;
	if (!list_empty(&ch->list)) {
		head_req = list_entry(ch->list.next, typeof(*head_req), node);
		tegra_dma_update_hw(ch, head_req);

		/* Set next request to idle. */
		if (!list_is_last(&head_req->node, &ch->list)) {
			next_req = list_entry(head_req->node.next,
					typeof(*head_req), node);
			next_req->status = TEGRA_DMA_REQ_PENDING;
		}
	}
}

static void configure_next_req(struct tegra_dma_channel *ch,
	struct tegra_dma_req *hreq)
{
	struct tegra_dma_req *next_req;
	if (!list_is_last(&hreq->node, &ch->list)) {
		next_req = list_entry(hreq->node.next, typeof(*next_req), node);
		tegra_dma_update_hw_partial(ch, next_req);
	}
}

static inline unsigned int get_req_xfer_word_count(
	struct tegra_dma_channel *ch, struct tegra_dma_req *req)
{
	if (ch->mode & TEGRA_DMA_MODE_CONTINUOUS_DOUBLE)
		return req->size >> 3;
	else
		return req->size >> 2;
}

static int get_current_xferred_count(struct tegra_dma_channel *ch,
	struct tegra_dma_req *req, unsigned long status)
{
	int req_transfer_count;
	req_transfer_count = get_req_xfer_word_count(ch, req) << 2;
	return req_transfer_count - ((status & STA_COUNT_MASK) + 4);
}

static void tegra_dma_abort_req(struct tegra_dma_channel *ch,
		struct tegra_dma_req *req, const char *warn_msg)
{
	unsigned long status = readl(ch->addr + APB_DMA_CHAN_STA);

	/*
	 * Check if interrupt is pending.
	 * This api is called from isr and hence need not to call
	 * isr handle again, just update the byte_transferred.
	 */
	if (status & STA_ISE_EOC)
		req->bytes_transferred += get_req_xfer_word_count(ch, req) << 2;
	tegra_dma_stop(ch);

	req->bytes_transferred +=  get_current_xferred_count(ch, req, status);
	req->status = -TEGRA_DMA_REQ_ERROR_STOPPED;
	if (warn_msg)
		WARN(1, KERN_WARNING "%s\n", warn_msg);
	start_head_req(ch);
}

static void handle_continuous_head_request(struct tegra_dma_channel *ch,
		struct tegra_dma_req *last_req)
{
	struct tegra_dma_req *hreq = NULL;

	if (list_empty(&ch->list)) {
		tegra_dma_abort_req(ch, last_req, NULL);
		return;
	}

	/*
	 * Check that head req on list should be in flight.
	 * If it is not in flight then request came late
	 * and so need to abort dma and start next request
	 * immediately.
	 */
	hreq = list_entry(ch->list.next, typeof(*hreq), node);
	if (hreq->status != TEGRA_DMA_REQ_INFLIGHT) {
		tegra_dma_abort_req(ch, last_req, "Req was not queued on time");
		return;
	}

	/* Configure next request in single buffer mode */
	if (ch->mode & TEGRA_DMA_MODE_CONTINUOUS_SINGLE)
		configure_next_req(ch, hreq);
}

static unsigned int get_channel_status(struct tegra_dma_channel *ch,
			struct tegra_dma_req *req, bool is_stop_dma)
{
	unsigned int status;

	if (is_stop_dma) {
		/* STOP the DMA and get the transfer count.
		 * Getting the transfer count is tricky.
		 *  - Globally disable DMA on all channels
		 *  - Read the channel's status register to know the number
		 *    of pending bytes to be transfered.
		 *  - Stop the dma channel
		 *  - Globally re-enable DMA to resume other transfers
		 */
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
		spin_lock(&enable_lock);
		pause_dma_channel(ch, true);
		status = readl(ch->addr + APB_DMA_CHAN_STA);
		tegra_dma_stop(ch);
		spin_unlock(&enable_lock);
#else
		pause_dma(true);
		status = readl(ch->addr + APB_DMA_CHAN_STA);
		tegra_dma_stop(ch);
		resume_dma();
#endif
		if (status & STA_ISE_EOC) {
			pr_err("Got Dma Int here clearing");
			writel(status, ch->addr + APB_DMA_CHAN_STA);
		}
		req->status = TEGRA_DMA_REQ_ERROR_ABORTED;
	} else {
		status = readl(ch->addr + APB_DMA_CHAN_STA);
	}
	return status;
}

/* should be called with the channel lock held */
static unsigned int dma_active_count(struct tegra_dma_channel *ch,
	struct tegra_dma_req *req, unsigned int status)
{
	unsigned int to_transfer;
	unsigned int req_transfer_count;

	unsigned int bytes_transferred;

	to_transfer = ((status & STA_COUNT_MASK) >> STA_COUNT_SHIFT) + 1;
	req_transfer_count = get_req_xfer_word_count(ch, req);
	bytes_transferred = req_transfer_count;

	if (status & STA_BUSY)
		bytes_transferred -= to_transfer;

	/*
	 * In continuous transfer mode, DMA only tracks the count of the
	 * half DMA buffer. So, if the DMA already finished half the DMA
	 * then add the half buffer to the completed count.
	 */
	if (ch->mode & TEGRA_DMA_MODE_CONTINUOUS_DOUBLE)
		if (req->buffer_status == TEGRA_DMA_REQ_BUF_STATUS_HALF_FULL)
			bytes_transferred += req_transfer_count;

	if (status & STA_ISE_EOC)
		bytes_transferred += req_transfer_count;

	bytes_transferred *= 4;

	return bytes_transferred;
}

int tegra_dma_dequeue_req(struct tegra_dma_channel *ch,
	struct tegra_dma_req *_req)
{
	struct tegra_dma_req *req = NULL;
	int found = 0;
	unsigned int status;
	unsigned long irq_flags;
	int stop = 0;

	spin_lock_irqsave(&ch->lock, irq_flags);

	if (list_entry(ch->list.next, struct tegra_dma_req, node) == _req)
		stop = 1;

	list_for_each_entry(req, &ch->list, node) {
		if (req == _req) {
			list_del(&req->node);
			found = 1;
			break;
		}
	}
	if (!found) {
		spin_unlock_irqrestore(&ch->lock, irq_flags);
		return -ENOENT;
	}

	if (!stop)
		goto skip_stop_dma;

	status = get_channel_status(ch, req, true);
	req->bytes_transferred = dma_active_count(ch, req, status);

	if (!list_empty(&ch->list)) {
		/* if the list is not empty, queue the next request */
		struct tegra_dma_req *next_req;
		next_req = list_entry(ch->list.next,
			typeof(*next_req), node);
		tegra_dma_update_hw(ch, next_req);
	}

skip_stop_dma:
	req->status = -TEGRA_DMA_REQ_ERROR_ABORTED;

	spin_unlock_irqrestore(&ch->lock, irq_flags);

	/* Callback should be called without any lock */
	if (req->complete)
		req->complete(req);
	return 0;
}
EXPORT_SYMBOL(tegra_dma_dequeue_req);

int tegra_dma_cancel(struct tegra_dma_channel *ch)
{
	struct tegra_dma_req *hreq = NULL;
	unsigned long status;
	unsigned long irq_flags;
	struct tegra_dma_req *cb_req = NULL;
	dma_callback callback = NULL;
	struct list_head new_list;

	INIT_LIST_HEAD(&new_list);

	if (ch->mode & TEGRA_DMA_SHARED) {
		pr_err("Can not abort requests from shared channel %d\n",
			ch->id);
		return -EPERM;
	}

	spin_lock_irqsave(&ch->lock, irq_flags);

	/* If list is empty, return with error*/
	if (list_empty(&ch->list)) {
		spin_unlock_irqrestore(&ch->lock, irq_flags);
		return 0;
	}

	/* Pause dma before checking the queue status */
	pause_dma(true);
	ch->dma_is_paused = true;
	status = readl(ch->addr + APB_DMA_CHAN_STA);
	if (status & STA_ISE_EOC) {
		handle_dma_isr_locked(ch);
		cb_req = ch->cb_req;
		callback = ch->callback;
		ch->cb_req = NULL;
		ch->callback = NULL;
		/* Read status because it may get changed */
		status = readl(ch->addr + APB_DMA_CHAN_STA);
	}

	/* Abort head requests, stop dma and dequeue all requests */
	if (!list_empty(&ch->list)) {
		tegra_dma_stop(ch);
		hreq = list_entry(ch->list.next, typeof(*hreq), node);
		hreq->bytes_transferred +=
				get_current_xferred_count(ch, hreq, status);

		/* copy the list into new list. */
		list_replace_init(&ch->list, &new_list);
	}

	resume_dma();
	ch->dma_is_paused = false;

	spin_unlock_irqrestore(&ch->lock, irq_flags);

	/* Call callback if it is due from interrupts */
	if (callback)
		callback(cb_req);

	/* Abort all requests on list. */
	while (!list_empty(&new_list)) {
		hreq = list_entry(new_list.next, typeof(*hreq), node);
		hreq->status = -TEGRA_DMA_REQ_ERROR_ABORTED;
		list_del(&hreq->node);
	}

	return 0;
}
EXPORT_SYMBOL(tegra_dma_cancel);

bool tegra_dma_is_empty(struct tegra_dma_channel *ch)
{
	unsigned long irq_flags;
	bool is_empty;

	spin_lock_irqsave(&ch->lock, irq_flags);
	if (list_empty(&ch->list))
		is_empty = true;
	else
		is_empty = false;
	spin_unlock_irqrestore(&ch->lock, irq_flags);
	return is_empty;
}
EXPORT_SYMBOL(tegra_dma_is_empty);

bool tegra_dma_is_req_inflight(struct tegra_dma_channel *ch,
	struct tegra_dma_req *_req)
{
	unsigned long irq_flags;
	struct tegra_dma_req *req;

	spin_lock_irqsave(&ch->lock, irq_flags);
	list_for_each_entry(req, &ch->list, node) {
		if (req == _req) {
			spin_unlock_irqrestore(&ch->lock, irq_flags);
			return true;
		}
	}
	spin_unlock_irqrestore(&ch->lock, irq_flags);
	return false;
}
EXPORT_SYMBOL(tegra_dma_is_req_inflight);

int tegra_dma_get_transfer_count(struct tegra_dma_channel *ch,
			struct tegra_dma_req *req)
{
	unsigned int status;
	unsigned long irq_flags;
	int bytes_transferred = 0;

	if (IS_ERR_OR_NULL(ch))
		BUG();

	spin_lock_irqsave(&ch->lock, irq_flags);

	if (list_entry(ch->list.next, struct tegra_dma_req, node) != req) {
		spin_unlock_irqrestore(&ch->lock, irq_flags);
		pr_debug("The dma request is not the head req\n");
		return req->bytes_transferred;
	}

	if (req->status != TEGRA_DMA_REQ_INFLIGHT) {
		spin_unlock_irqrestore(&ch->lock, irq_flags);
		pr_debug("The dma request is not running\n");
		return req->bytes_transferred;
	}

	status = get_channel_status(ch, req, false);
	bytes_transferred = dma_active_count(ch, req, status);
	spin_unlock_irqrestore(&ch->lock, irq_flags);
	return bytes_transferred;
}
EXPORT_SYMBOL(tegra_dma_get_transfer_count);

int tegra_dma_enqueue_req(struct tegra_dma_channel *ch,
	struct tegra_dma_req *req)
{
	unsigned long irq_flags;
	struct tegra_dma_req *_req;
	int start_dma = 0;
	struct tegra_dma_req *hreq, *hnreq;

	if (req->size > TEGRA_DMA_MAX_TRANSFER_SIZE ||
		req->source_addr & 0x3 || req->dest_addr & 0x3) {
		pr_err("Invalid DMA request for channel %d\n", ch->id);
		return -EINVAL;
	}

	if ((req->size & 0x3) ||
	   ((ch->mode & TEGRA_DMA_MODE_CONTINUOUS_DOUBLE) && (req->size & 0x7)))
	{
		pr_err("Invalid DMA request size 0x%08x for channel %d\n",
				req->size, ch->id);
		return -EINVAL;
	}

	spin_lock_irqsave(&ch->lock, irq_flags);

	list_for_each_entry(_req, &ch->list, node) {
		if (req == _req) {
			spin_unlock_irqrestore(&ch->lock, irq_flags);
			return -EEXIST;
		}
	}

	req->bytes_transferred = 0;
	req->status = TEGRA_DMA_REQ_PENDING;
	/* STATUS_EMPTY just means the DMA hasn't processed the buf yet. */
	req->buffer_status = TEGRA_DMA_REQ_BUF_STATUS_EMPTY;
	if (list_empty(&ch->list))
		start_dma = 1;

	list_add_tail(&req->node, &ch->list);

	if (start_dma) {
		tegra_dma_update_hw(ch, req);
	} else {
		/*
		 * Check to see if this request needs to be configured
		 * immediately in continuous mode.
		 */
		if (ch->mode & TEGRA_DMA_MODE_ONESHOT)
			goto end;

		hreq = list_entry(ch->list.next, typeof(*hreq), node);
		hnreq = list_entry(hreq->node.next, typeof(*hnreq), node);
		if (hnreq != req)
			goto end;

		if ((ch->mode & TEGRA_DMA_MODE_CONTINUOUS_DOUBLE) &&
		    (req->buffer_status != TEGRA_DMA_REQ_BUF_STATUS_HALF_FULL))
			goto end;

		/* Need to configure the new request now */
		tegra_dma_update_hw_partial(ch, req);
	}

end:
	spin_unlock_irqrestore(&ch->lock, irq_flags);
	return 0;
}
EXPORT_SYMBOL(tegra_dma_enqueue_req);

static void tegra_dma_dump_channel_usage(void)
{
	int i;
	pr_info("DMA channel allocation dump:\n");
	for (i = TEGRA_SYSTEM_DMA_CH_MIN; i <= TEGRA_SYSTEM_DMA_CH_MAX; i++) {
		struct tegra_dma_channel *ch = &dma_channels[i];
		pr_warn("dma %d used by %s\n", i, ch->client_name);
	}
	return;
}

struct tegra_dma_channel *tegra_dma_allocate_channel(int mode,
		const char namefmt[], ...)
{
	int channel;
	struct tegra_dma_channel *ch = NULL;
	va_list args;
	dma_isr_handler isr_handler = NULL;

	if (!tegra_dma_initialized)
		return NULL;

	mutex_lock(&tegra_dma_lock);

	/* first channel is the shared channel */
	if (mode & TEGRA_DMA_SHARED) {
		channel = TEGRA_SYSTEM_DMA_CH_MIN;
	} else {
		channel = find_first_zero_bit(channel_usage,
			ARRAY_SIZE(dma_channels));
		if (channel >= ARRAY_SIZE(dma_channels)) {
			tegra_dma_dump_channel_usage();
			goto out;
		}
	}

	if (mode & TEGRA_DMA_MODE_ONESHOT)
		isr_handler = handle_oneshot_dma;
	else if (mode & TEGRA_DMA_MODE_CONTINUOUS_DOUBLE)
		isr_handler = handle_continuous_dbl_dma;
	else if (mode & TEGRA_DMA_MODE_CONTINUOUS_SINGLE)
		isr_handler = handle_continuous_sngl_dma;
	else
		pr_err("Bad channel mode for DMA ISR handler\n");

	if (!isr_handler)
		goto out;

	__set_bit(channel, channel_usage);
	ch = &dma_channels[channel];
	ch->mode = mode;
	ch->isr_handler = isr_handler;
	ch->dma_is_paused = false;
	va_start(args, namefmt);
	vsnprintf(ch->client_name, sizeof(ch->client_name),
		namefmt, args);
	va_end(args);

out:
	mutex_unlock(&tegra_dma_lock);
	return ch;
}
EXPORT_SYMBOL(tegra_dma_allocate_channel);

void tegra_dma_free_channel(struct tegra_dma_channel *ch)
{
	if (ch->mode & TEGRA_DMA_SHARED)
		return;
	tegra_dma_cancel(ch);
	mutex_lock(&tegra_dma_lock);
	__clear_bit(ch->id, channel_usage);
	memset(ch->client_name, 0, sizeof(ch->client_name));
	ch->isr_handler = NULL;
	ch->callback = NULL;
	ch->cb_req = NULL;
	mutex_unlock(&tegra_dma_lock);
}
EXPORT_SYMBOL(tegra_dma_free_channel);

int tegra_dma_get_channel_id(struct tegra_dma_channel *ch)
{
	return ch->id;
}
EXPORT_SYMBOL(tegra_dma_get_channel_id);

static bool tegra_dma_update_hw_partial(struct tegra_dma_channel *ch,
	struct tegra_dma_req *req)
{
	u32 apb_ptr;
	u32 ahb_ptr;
	u32 csr;
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	u32 swid;
#endif
	unsigned long status;
	unsigned int req_transfer_count;
	bool configure = false;

	if (req->to_memory) {
		apb_ptr = req->source_addr;
		ahb_ptr = req->dest_addr;
	} else {
		apb_ptr = req->dest_addr;
		ahb_ptr = req->source_addr;
	}

	/*
	 * The dma controller reloads the new configuration for next transfer
	 * after last burst of current transfer completes.
	 * If there is no IEC status then this make sure that last burst
	 * has not be completed.
	 * If there is already IEC status then interrupt handle need to
	 * load new configuration after aborting current dma.
	 */
	/* Dont pause again if channel has already paused it*/
	if (!ch->dma_is_paused)
		pause_dma(false);
	status  = readl(ch->addr + APB_DMA_CHAN_STA);

	/*
	 * If interrupt is pending then do nothing as the ISR will handle
	 * the programing for new request.
	 */
	if (status & STA_ISE_EOC) {
		pr_warn("%s(): "
			"Skipping new configuration as interrupt is pending\n",
				__func__);
		goto exit_config;
	}

	/* Safe to program new configuration */
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	swid = readl(general_dma_addr + APB_DMA_SWID);
	if (req->use_smmu) {
		swid |= (SWID_CHAN0 << (ch->id));
		writel(swid, general_dma_addr + APB_DMA_SWID);
	}
	else {
		swid &= (~(SWID_CHAN0 << (ch->id)));
		writel(swid, general_dma_addr + APB_DMA_SWID);
	}
#endif
	writel(apb_ptr, ch->addr + APB_DMA_CHAN_APB_PTR);
	writel(ahb_ptr, ch->addr + APB_DMA_CHAN_AHB_PTR);

	req_transfer_count = get_req_xfer_word_count(ch, req);
	csr = readl(ch->addr + APB_DMA_CHAN_CSR);
	csr &= ~CSR_WCOUNT_MASK;
	csr |= (req_transfer_count - 1) << CSR_WCOUNT_SHIFT;
	writel(csr, ch->addr + APB_DMA_CHAN_CSR);
	req->status = TEGRA_DMA_REQ_INFLIGHT;
	configure = true;

exit_config:
	if (!ch->dma_is_paused)
		resume_dma();
	return configure;
}

static void tegra_dma_update_hw(struct tegra_dma_channel *ch,
	struct tegra_dma_req *req)
{
	int ahb_addr_wrap;
	int apb_addr_wrap;
	int ahb_bus_width;
	int apb_bus_width;
	int index;
	unsigned int req_transfer_count;

	u32 ahb_seq;
	u32 apb_seq;
	u32 ahb_ptr;
	u32 apb_ptr;
	u32 csr;
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	u32 swid;
#endif

	csr = CSR_FLOW;
	if (req->complete || req->threshold)
		csr |= CSR_IE_EOC;

	ahb_seq = AHB_SEQ_INTR_ENB;

	switch (req->req_sel) {
	case TEGRA_DMA_REQ_SEL_SL2B1:
	case TEGRA_DMA_REQ_SEL_SL2B2:
	case TEGRA_DMA_REQ_SEL_SL2B3:
	case TEGRA_DMA_REQ_SEL_SL2B4:
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
	case TEGRA_DMA_REQ_SEL_SL2B5:
	case TEGRA_DMA_REQ_SEL_SL2B6:
	case TEGRA_DMA_REQ_SEL_APBIF_CH0:
	case TEGRA_DMA_REQ_SEL_APBIF_CH1:
	case TEGRA_DMA_REQ_SEL_APBIF_CH2:
	case TEGRA_DMA_REQ_SEL_APBIF_CH3:
#if !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	case TEGRA_DMA_REQ_SEL_APBIF_CH4:
	case TEGRA_DMA_REQ_SEL_APBIF_CH5:
	case TEGRA_DMA_REQ_SEL_APBIF_CH6:
	case TEGRA_DMA_REQ_SEL_APBIF_CH7:
	case TEGRA_DMA_REQ_SEL_APBIF_CH8:
	case TEGRA_DMA_REQ_SEL_APBIF_CH9:
#endif
#endif
	case TEGRA_DMA_REQ_SEL_SPI:
		/* dtv interface has fixed burst size of 4 */
		if (req->fixed_burst_size) {
			ahb_seq |= AHB_SEQ_BURST_4;
			break;
		}
		/* For spi/slink the burst size based on transfer size
		 * i.e. if multiple of 32 bytes then busrt is 8
		 * word(8x32bits) else if multiple of 16 bytes then
		 * burst is 4 word(4x32bits) else burst size is 1
		 * word(1x32bits) */
		if (req->size & 0xF)
			ahb_seq |= AHB_SEQ_BURST_1;
		else if ((req->size >> 4) & 0x1)
			ahb_seq |= AHB_SEQ_BURST_4;
		else
			ahb_seq |= AHB_SEQ_BURST_8;
		break;
#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
	case TEGRA_DMA_REQ_SEL_I2S_2:
	case TEGRA_DMA_REQ_SEL_I2S_1:
	case TEGRA_DMA_REQ_SEL_SPD_I:
	case TEGRA_DMA_REQ_SEL_UI_I:
	case TEGRA_DMA_REQ_SEL_I2S2_2:
	case TEGRA_DMA_REQ_SEL_I2S2_1:
		/* For ARCH_2x i2s/spdif burst size is 4 word */
		ahb_seq |= AHB_SEQ_BURST_4;
		break;
#endif

	default:
		ahb_seq |= AHB_SEQ_BURST_1;
		break;
	}

	apb_seq = 0;

	csr |= req->req_sel << CSR_REQ_SEL_SHIFT;

	req_transfer_count = get_req_xfer_word_count(ch, req);

	/* One shot mode is always single buffered.  Continuous mode could
	 * support either.
	 */
	if (ch->mode & TEGRA_DMA_MODE_ONESHOT)
		csr |= CSR_ONCE;

	if (ch->mode & TEGRA_DMA_MODE_CONTINUOUS_DOUBLE)
		ahb_seq |= AHB_SEQ_DBL_BUF;

	csr |= (req_transfer_count - 1) << CSR_WCOUNT_SHIFT;

	if (req->to_memory) {
		apb_ptr = req->source_addr;
		ahb_ptr = req->dest_addr;

		apb_addr_wrap = req->source_wrap;
		ahb_addr_wrap = req->dest_wrap;
		apb_bus_width = req->source_bus_width;
		ahb_bus_width = req->dest_bus_width;

	} else {
		csr |= CSR_DIR;
		apb_ptr = req->dest_addr;
		ahb_ptr = req->source_addr;

		apb_addr_wrap = req->dest_wrap;
		ahb_addr_wrap = req->source_wrap;
		apb_bus_width = req->dest_bus_width;
		ahb_bus_width = req->source_bus_width;
	}

	apb_addr_wrap >>= 2;
	ahb_addr_wrap >>= 2;

	/* set address wrap for APB size */
	index = 0;
	do  {
		if (apb_addr_wrap_table[index] == apb_addr_wrap)
			break;
		index++;
	} while (index < ARRAY_SIZE(apb_addr_wrap_table));
	BUG_ON(index == ARRAY_SIZE(apb_addr_wrap_table));
	apb_seq |= index << APB_SEQ_WRAP_SHIFT;

	/* set address wrap for AHB size */
	index = 0;
	do  {
		if (ahb_addr_wrap_table[index] == ahb_addr_wrap)
			break;
		index++;
	} while (index < ARRAY_SIZE(ahb_addr_wrap_table));
	BUG_ON(index == ARRAY_SIZE(ahb_addr_wrap_table));
	ahb_seq |= index << AHB_SEQ_WRAP_SHIFT;

	for (index = 0; index < ARRAY_SIZE(bus_width_table); index++) {
		if (bus_width_table[index] == ahb_bus_width)
			break;
	}
	BUG_ON(index == ARRAY_SIZE(bus_width_table));
	ahb_seq |= index << AHB_SEQ_BUS_WIDTH_SHIFT;

	for (index = 0; index < ARRAY_SIZE(bus_width_table); index++) {
		if (bus_width_table[index] == apb_bus_width)
			break;
	}
	BUG_ON(index == ARRAY_SIZE(bus_width_table));
	apb_seq |= index << APB_SEQ_BUS_WIDTH_SHIFT;

#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	swid = readl(general_dma_addr + APB_DMA_SWID);
	if (req->use_smmu) {
		swid |= (SWID_CHAN0 << (ch->id));
		writel(swid, general_dma_addr + APB_DMA_SWID);
	}
	else {
		swid &= (~(SWID_CHAN0 << (ch->id)));
		writel(swid, general_dma_addr + APB_DMA_SWID);
	}
#endif

	writel(csr, ch->addr + APB_DMA_CHAN_CSR);
	writel(apb_seq, ch->addr + APB_DMA_CHAN_APB_SEQ);
	writel(apb_ptr, ch->addr + APB_DMA_CHAN_APB_PTR);
	writel(ahb_seq, ch->addr + APB_DMA_CHAN_AHB_SEQ);
	writel(ahb_ptr, ch->addr + APB_DMA_CHAN_AHB_PTR);

	csr |= CSR_ENB;
	writel(csr, ch->addr + APB_DMA_CHAN_CSR);

	req->status = TEGRA_DMA_REQ_INFLIGHT;
}

static void handle_oneshot_dma(struct tegra_dma_channel *ch)
{
	struct tegra_dma_req *req;

	req = list_entry(ch->list.next, typeof(*req), node);
	list_del(&req->node);
	req->bytes_transferred += req->size;
	req->status = TEGRA_DMA_REQ_SUCCESS;

	ch->callback = req->complete;
	ch->cb_req = req;

	start_head_req(ch);
	return;
}

static void handle_continuous_dbl_dma(struct tegra_dma_channel *ch)
{
	struct tegra_dma_req *req;

	req = list_entry(ch->list.next, typeof(*req), node);

	if (req->buffer_status == TEGRA_DMA_REQ_BUF_STATUS_EMPTY) {
		bool is_dma_ping_complete;
		unsigned long status = readl(ch->addr + APB_DMA_CHAN_STA);
		is_dma_ping_complete = (status & STA_PING_PONG) ? true : false;

		/* Ping pong status shows in reverse if it is Memory write */
		if (req->to_memory)
			is_dma_ping_complete = !is_dma_ping_complete;

		/* Out of sync - Release current buffer */
		if (!is_dma_ping_complete) {
			/*
			 * We should not land here if queue mechanism
			 * with system latency are properly configured.
			 */
			req->bytes_transferred += req->size;

			list_del(&req->node);
			ch->callback = req->complete;
			ch->cb_req = req;

			tegra_dma_abort_req(ch, req,
				"Dma becomes out of sync for ping-pong buffer");
			return;
		}

		/*
		 * Configure next request so after full buffer transfer,
		 * it can be start without sw intervention.
		 */
		configure_next_req(ch, req);

		req->buffer_status = TEGRA_DMA_REQ_BUF_STATUS_HALF_FULL;
		req->status = TEGRA_DMA_REQ_SUCCESS;
		req->bytes_transferred += req->size >> 1;

		ch->callback = req->threshold;
		ch->cb_req = req;
		return;
	}

	if (req->buffer_status == TEGRA_DMA_REQ_BUF_STATUS_HALF_FULL) {
		/* Interrupt for full buffer complete */
		req->buffer_status = TEGRA_DMA_REQ_BUF_STATUS_FULL;
		req->bytes_transferred += req->size >> 1;
		req->status = TEGRA_DMA_REQ_SUCCESS;

		list_del(&req->node);
		ch->callback = req->complete;
		ch->cb_req = req;

		handle_continuous_head_request(ch, req);
		return;
	}
	tegra_dma_abort_req(ch, req, "Dma status is not on sync\n");
	/* Dma should be stop much earlier */
	BUG();
	return;
}

static void handle_continuous_sngl_dma(struct tegra_dma_channel *ch)
{
	struct tegra_dma_req *req;

	req = list_entry(ch->list.next, typeof(*req), node);
	if (req->buffer_status == TEGRA_DMA_REQ_BUF_STATUS_FULL) {
		tegra_dma_stop(ch);
		pr_err("%s: DMA complete irq without corresponding req\n",
				__func__);
		WARN_ON(1);
		return;
	}

	/* Handle the case when buffer is completely full */
	req->bytes_transferred += req->size;
	req->buffer_status = TEGRA_DMA_REQ_BUF_STATUS_FULL;
	req->status = TEGRA_DMA_REQ_SUCCESS;

	list_del(&req->node);
	ch->callback = req->complete;
	ch->cb_req = req;

	handle_continuous_head_request(ch, req);
	return;
}

static void handle_dma_isr_locked(struct tegra_dma_channel *ch)
{
	/* There should be proper isr handler */
	BUG_ON(!ch->isr_handler);

	if (list_empty(&ch->list)) {
		tegra_dma_stop(ch);
		pr_err("%s: No requests in the list.\n", __func__);
		WARN_ON(1);
		return;
	}

	ch->isr_handler(ch);
}

#ifndef CONFIG_TEGRA_SIMULATION_PLATFORM
static irqreturn_t dma_isr(int irq, void *data)
{
	struct tegra_dma_channel *ch = data;
	unsigned long irq_flags;
	unsigned long status;
	dma_callback callback = NULL;
	struct tegra_dma_req *cb_req = NULL;

	spin_lock_irqsave(&ch->lock, irq_flags);

	/*
	 * Calbacks should be set and cleared while holding the spinlock,
	 * never left set
	 */
	if (ch->callback || ch->cb_req)
		pr_err("%s():"
			"Channel %d callbacks are not initialized properly\n",
			__func__, ch->id);
	BUG_ON(ch->callback || ch->cb_req);

	status = readl(ch->addr + APB_DMA_CHAN_STA);
	if (status & STA_ISE_EOC) {
		/* Clear dma int status */
		writel(status, ch->addr + APB_DMA_CHAN_STA);
		handle_dma_isr_locked(ch);
		callback = ch->callback;
		cb_req = ch->cb_req;
		ch->callback = NULL;
		ch->cb_req = NULL;
	} else {
		pr_info("Interrupt is already handled %d\n", ch->id);
	}
	spin_unlock_irqrestore(&ch->lock, irq_flags);

	/* Call callback function to notify client if it is there */
	if (callback)
		callback(cb_req);
	return IRQ_HANDLED;
}
#endif

int __init tegra_dma_init(void)
{
#ifdef CONFIG_TEGRA_SIMULATION_PLATFORM
	return -ENODEV;
#else
	int ret = 0;
	int i;
	unsigned int irq;

	bitmap_fill(channel_usage, NV_DMA_MAX_CHANNELS);

	dma_clk = clk_get_sys("tegra-dma", NULL);
	if (IS_ERR_OR_NULL(dma_clk)) {
		pr_err("Unable to get clock for APB DMA\n");
		ret = PTR_ERR(dma_clk);
		goto fail;
	}
	ret = tegra_clk_prepare_enable(dma_clk);
	if (ret != 0) {
		pr_err("Unable to enable clock for APB DMA\n");
		goto fail;
	}

	/*
	 * Resetting all dma channels to make sure all channels are in init
	 * state.
	 */
	tegra_periph_reset_assert(dma_clk);
	udelay(10);
	tegra_periph_reset_deassert(dma_clk);
	udelay(10);

	writel(GEN_ENABLE, general_dma_addr + APB_DMA_GEN);
	writel(0, general_dma_addr + APB_DMA_CNTRL);
	writel(0xFFFFFFFFul >> (31 - TEGRA_SYSTEM_DMA_CH_MAX),
			general_dma_addr + APB_DMA_IRQ_MASK_SET);

	for (i = TEGRA_SYSTEM_DMA_CH_MIN; i <= TEGRA_SYSTEM_DMA_CH_MAX; i++) {
		struct tegra_dma_channel *ch = &dma_channels[i];

		ch->id = i;
		ch->isr_handler = NULL;
		snprintf(ch->name, TEGRA_DMA_NAME_SIZE, "dma_channel_%d", i);

		memset(ch->client_name, 0, sizeof(ch->client_name));

		ch->addr = IO_ADDRESS(TEGRA_APB_DMA_CH0_BASE +
			TEGRA_APB_DMA_CH0_SIZE * i);

		spin_lock_init(&ch->lock);
		INIT_LIST_HEAD(&ch->list);

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
		if (i >= 16)
			irq = INT_APB_DMA_CH16 + i - 16;
		else
#endif
			irq = INT_APB_DMA_CH0 + i;
		ret = request_irq(irq, dma_isr, 0, dma_channels[i].name, ch);
		if (ret) {
			pr_err("Failed to register IRQ %d for DMA %d\n",
				irq, i);
			goto fail;
		}
		ch->irq = irq;

		__clear_bit(i, channel_usage);
	}
	/* mark the shared channel allocated */
	__set_bit(TEGRA_SYSTEM_DMA_CH_MIN, channel_usage);

	tegra_dma_initialized = true;

	return 0;
fail:
	writel(0, general_dma_addr + APB_DMA_GEN);
	for (i = TEGRA_SYSTEM_DMA_CH_MIN; i <= TEGRA_SYSTEM_DMA_CH_MAX; i++) {
		struct tegra_dma_channel *ch = &dma_channels[i];
		if (ch->irq)
			free_irq(ch->irq, ch);
	}
	return ret;
#endif
}
postcore_initcall(tegra_dma_init);

#ifdef CONFIG_PM_SLEEP

static u32 apb_dma[5*TEGRA_SYSTEM_DMA_CH_NR + 3];

static int tegra_dma_suspend(void)
{
	u32 *ctx = apb_dma;
	int i;

	*ctx++ = readl(general_dma_addr + APB_DMA_GEN);
	*ctx++ = readl(general_dma_addr + APB_DMA_CNTRL);
	*ctx++ = readl(general_dma_addr + APB_DMA_IRQ_MASK);

	for (i = 0; i < TEGRA_SYSTEM_DMA_CH_NR; i++) {
		void __iomem *addr = IO_ADDRESS(TEGRA_APB_DMA_CH0_BASE +
				  TEGRA_APB_DMA_CH0_SIZE * i);

		*ctx++ = readl(addr + APB_DMA_CHAN_CSR);
		*ctx++ = readl(addr + APB_DMA_CHAN_AHB_PTR);
		*ctx++ = readl(addr + APB_DMA_CHAN_AHB_SEQ);
		*ctx++ = readl(addr + APB_DMA_CHAN_APB_PTR);
		*ctx++ = readl(addr + APB_DMA_CHAN_APB_SEQ);
	}

	/* Disabling clock of dma. */
	tegra_clk_disable_unprepare(dma_clk);
	return 0;
}

static void tegra_dma_resume(void)
{
	u32 *ctx = apb_dma;
	int i;

	/* Enabling clock of dma. */
	tegra_clk_prepare_enable(dma_clk);

	writel(*ctx++, general_dma_addr + APB_DMA_GEN);
	writel(*ctx++, general_dma_addr + APB_DMA_CNTRL);
	writel(*ctx++, general_dma_addr + APB_DMA_IRQ_MASK);

	for (i = 0; i < TEGRA_SYSTEM_DMA_CH_NR; i++) {
		void __iomem *addr = IO_ADDRESS(TEGRA_APB_DMA_CH0_BASE +
				  TEGRA_APB_DMA_CH0_SIZE * i);

		writel(*ctx++, addr + APB_DMA_CHAN_CSR);
		writel(*ctx++, addr + APB_DMA_CHAN_AHB_PTR);
		writel(*ctx++, addr + APB_DMA_CHAN_AHB_SEQ);
		writel(*ctx++, addr + APB_DMA_CHAN_APB_PTR);
		writel(*ctx++, addr + APB_DMA_CHAN_APB_SEQ);
	}
}

static struct syscore_ops tegra_dma_syscore_ops = {
	.suspend = tegra_dma_suspend,
	.resume = tegra_dma_resume,
};

static int tegra_dma_syscore_init(void)
{
	register_syscore_ops(&tegra_dma_syscore_ops);

	return 0;
}
subsys_initcall(tegra_dma_syscore_init);
#endif

#ifdef CONFIG_DEBUG_FS

#include <linux/debugfs.h>
#include <linux/seq_file.h>

static int dbg_dma_show(struct seq_file *s, void *unused)
{
	int i;

	seq_printf(s, "    APBDMA global register\n");
	seq_printf(s, "DMA_GEN:   0x%08x\n",
			__raw_readl(general_dma_addr + APB_DMA_GEN));
	seq_printf(s, "DMA_CNTRL: 0x%08x\n",
			__raw_readl(general_dma_addr + APB_DMA_CNTRL));
	seq_printf(s, "IRQ_MASK:  0x%08x\n",
			__raw_readl(general_dma_addr + APB_DMA_IRQ_MASK));

	for (i = 0; i < TEGRA_SYSTEM_DMA_CH_NR; i++) {
		void __iomem *addr = IO_ADDRESS(TEGRA_APB_DMA_CH0_BASE +
				  TEGRA_APB_DMA_CH0_SIZE * i);

		seq_printf(s, "    APBDMA channel %02d register\n", i);
		seq_printf(s, "0x00: 0x%08x 0x%08x 0x%08x 0x%08x\n",
					__raw_readl(addr + 0x0),
					__raw_readl(addr + 0x4),
					__raw_readl(addr + 0x8),
					__raw_readl(addr + 0xC));
		seq_printf(s, "0x10: 0x%08x 0x%08x 0x%08x 0x%08x\n",
					__raw_readl(addr + 0x10),
					__raw_readl(addr + 0x14),
					__raw_readl(addr + 0x18),
					__raw_readl(addr + 0x1C));
	}
	seq_printf(s, "\nAPB DMA users\n");
	seq_printf(s, "-------------\n");
	for (i = TEGRA_SYSTEM_DMA_CH_MIN; i <= TEGRA_SYSTEM_DMA_CH_MAX; i++) {
		struct tegra_dma_channel *ch = &dma_channels[i];
		if (strlen(ch->client_name) > 0)
			seq_printf(s, "dma %d -> %s\n", i, ch->client_name);
	}
	return 0;
}

static int dbg_dma_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_dma_show, &inode->i_private);
}

static const struct file_operations debug_fops = {
	.open	   = dbg_dma_open,
	.read	   = seq_read,
	.llseek	 = seq_lseek,
	.release	= single_release,
};

static int __init tegra_dma_debuginit(void)
{
	(void) debugfs_create_file("tegra_dma", S_IRUGO,
					NULL, NULL, &debug_fops);
	return 0;
}
late_initcall(tegra_dma_debuginit);
#endif
