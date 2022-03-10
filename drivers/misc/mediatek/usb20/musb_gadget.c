// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>
#include <linux/stat.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/kfifo.h>

#if defined(CONFIG_USBIF_COMPLIANCE)
#include <linux/kthread.h>
#include <linux/sched.h>
#endif

#include <linux/usb/composite.h>

#include <musb_core.h>
#include <mtk_musb.h>

/* GADGET only support all-ep QMU, otherwise downgrade to non-QMU */
#ifdef MUSB_QMU_LIMIT_SUPPORT
#undef CONFIG_MTK_MUSB_QMU_SUPPORT
#endif
#ifdef CONFIG_MTK_MUSB_QMU_SUPPORT
#include "musb_qmu.h"
#endif

#define FIFO_START_ADDR 512

/* #define RX_DMA_MODE1 1 */

/* MUSB PERIPHERAL status 3-mar-2006:
 *
 * - EP0 seems solid.  It passes both USBCV and usbtest control cases.
 *   Minor glitches:
 *
 *     + remote wakeup to Linux hosts work, but saw USBCV failures;
 *       in one test run (operator error?)
 *     + endpoint halt tests -- in both usbtest and usbcv -- seem
 *       to break when dma is enabled ... is something wrongly
 *       clearing SENDSTALL?
 *
 * - Mass storage behaved ok when last tested.  Network traffic patterns
 *   (with lots of short transfers etc) need retesting; they turn up the
 *   worst cases of the DMA, since short packets are typical but are not
 *   required.
 *
 * - TX/IN
 *     + both pio and dma behave in with network and g_zero tests
 *     + no cppi throughput issues other than no-hw-queueing
 *     + failed with FLAT_REG (DaVinci)
 *     + seems to behave with double buffering, PIO -and- CPPI
 *     + with gadgetfs + AIO, requests got lost?
 *
 * - RX/OUT
 *     + both pio and dma behave in with network and g_zero tests
 *     + dma is slow in typical case (short_not_ok is clear)
 *     + double buffering ok with PIO
 *     + double buffering *FAILS* with CPPI, wrong data bytes sometimes
 *     + request lossage observed with gadgetfs
 *
 * - ISO not tested ... might work, but only weakly isochronous
 *
 * - Gadget driver disabling of softconnect during bind() is ignored; so
 *   drivers can't hold off host requests until userspace is ready.
 *   (Workaround:  they can turn it off later.)
 *
 * - PORTABILITY (assumes PIO works):
 *     + DaVinci, basically works with cppi dma
 *     + OMAP 2430, ditto with mentor dma
 *     + TUSB 6010, platform-specific dma in the works
 */

/* ----------------------------------------------------------------------- */

#define is_buffer_mapped(req) (is_dma_capable() && \
					(req->map_state != UN_MAPPED))

/* Maps the buffer to dma  */

static inline void map_dma_buffer(struct musb_request *request,
				  struct musb *musb, struct musb_ep *musb_ep)
{
#ifndef CONFIG_MTK_MUSB_QMU_SUPPORT
	int compatible = true;
	struct dma_controller *dma = musb->dma_controller;
#endif

	unsigned int length;

	if (request->request.length == 0)
		return;

	length = ALIGN(request->request.length, dma_get_cache_alignment());


	request->map_state = UN_MAPPED;
#ifndef CONFIG_MTK_MUSB_QMU_SUPPORT
	if (!is_dma_capable() || !musb_ep->dma)
		return;

	/* Check if DMA engine can handle this request.
	 * DMA code must reject the USB request explicitly.
	 * Default behaviour is to map the request.
	 */
	if (dma->is_compatible)
		compatible = dma->is_compatible(musb_ep->dma,
						musb_ep->packet_sz,
						request->request.buf,
						request->request.length);
	if (!compatible)
		return;
#endif

	if (request->request.dma == DMA_ADDR_INVALID) {
		dma_addr_t dma_addr;
		int ret;

		dma_addr = dma_map_single(musb->controller,
						      request->request.buf,
						      length,
						      request->tx
						      ? DMA_TO_DEVICE
						      : DMA_FROM_DEVICE);
		ret = dma_mapping_error(musb->controller, dma_addr);
		if (ret)
			return;

		request->request.dma = dma_addr;
		request->map_state = MUSB_MAPPED;
	} else {
		dma_sync_single_for_device(musb->controller,
					   request->request.dma,
					   length, request->tx ?
					   DMA_TO_DEVICE : DMA_FROM_DEVICE);
		request->map_state = PRE_MAPPED;
	}
}

/* Unmap the buffer from dma and maps it back to cpu */
static inline void
unmap_dma_buffer(struct musb_request *request, struct musb *musb)
{
	unsigned int length;

	if (request->request.length == 0)
		return;

	length = ALIGN(request->request.length, dma_get_cache_alignment());

	if (!is_buffer_mapped(request))
		return;

	if (request->request.dma == DMA_ADDR_INVALID) {
		DBG(1, "not unmapping a never mapped buffer\n");
		return;
	}
	if (request->map_state == MUSB_MAPPED) {
		dma_unmap_single(musb->controller,
				 request->request.dma,
				 length, request->tx ?
				 DMA_TO_DEVICE : DMA_FROM_DEVICE);
		request->request.dma = DMA_ADDR_INVALID;
	} else {		/* PRE_MAPPED */
		dma_sync_single_for_cpu(musb->controller,
					request->request.dma,
					length, request->tx ?
					DMA_TO_DEVICE : DMA_FROM_DEVICE);
	}
	request->map_state = UN_MAPPED;
}

/*
 * Immediately complete a request.
 *
 * @param request the request to complete
 * @param status the status to complete the request with
 * Context: controller locked, IRQs blocked.
 */
void musb_g_giveback(struct musb_ep *ep,
		     struct usb_request *request,
		     int status) __releases(ep->musb->lock)
		     __acquires(ep->musb->lock)
{
	struct musb_request *req;
	struct musb *musb;
	int busy = ep->busy;

	req = to_musb_request(request);

	list_del(&req->list);
	if (req->request.status == -EINPROGRESS)
		req->request.status = status;
	musb = req->musb;

	ep->busy = 1;
	spin_unlock(&musb->lock);

	if (!request) {
		DBG(0, "%s request already free\n", ep->end_point.name);
		goto lock;
	}

	if (!dma_mapping_error(musb->controller, request->dma) &&
			req->request.length != 0)
		unmap_dma_buffer(req, musb);
	else if (req->epnum != 0)
		DBG(0, "%s dma_mapping_error\n", ep->end_point.name);

	if (request->status == 0)
		DBG(1, "%s done request %p,  %d/%d\n",
		    ep->end_point.name, request,
		    req->request.actual,
		    req->request.length);
	else
		DBG(1, "%s request %p, %d/%d fault %d\n",
		    ep->end_point.name, request,
		    req->request.actual, req->request.length, request->status);
	usb_gadget_giveback_request(&req->ep->end_point, &req->request);
lock:
	spin_lock(&musb->lock);
	ep->busy = busy;
}

/* ----------------------------------------------------------------------- */

/*
 * Abort requests queued to an endpoint using the status. Synchronous.
 * caller locked controller and blocked irqs, and selected this ep.
 */
static void nuke(struct musb_ep *ep, const int status)
{
	/*struct musb           *musb = ep->musb; */
	struct musb_request *req = NULL;
#ifndef CONFIG_MTK_MUSB_QMU_SUPPORT
	void __iomem *epio = ep->musb->endpoints[ep->current_epnum].regs;
#endif


	ep->busy = 1;
#ifdef CONFIG_MTK_MUSB_QMU_SUPPORT
	musb_flush_qmu(ep->hw_ep->epnum, (ep->is_in ? TXQ : RXQ));
#else
	if (is_dma_capable() && ep->dma) {
		struct dma_controller *c = ep->musb->dma_controller;
		int value;

		if (ep->is_in) {
			/*
			 * The programming guide says that we must not clear
			 * the DMAMODE bit before DMAENAB, so we only
			 * clear it in the second write...
			 */
			musb_writew(epio, MUSB_TXCSR,
						MUSB_TXCSR_DMAMODE
						| MUSB_TXCSR_FLUSHFIFO);
			musb_writew(epio, MUSB_TXCSR, 0 | MUSB_TXCSR_FLUSHFIFO);
		} else {
			musb_writew(epio, MUSB_RXCSR, 0 | MUSB_RXCSR_FLUSHFIFO);
			musb_writew(epio, MUSB_RXCSR, 0 | MUSB_RXCSR_FLUSHFIFO);
		}

		value = c->channel_abort(ep->dma);
		DBG(0, "%s: %s: abort DMA --> %d\n"
					, __func__, ep->name, value);
		c->channel_release(ep->dma);
		ep->dma = NULL;
	}
#endif
	while (!list_empty(&ep->req_list)) {
		req = list_first_entry(&ep->req_list,
					struct musb_request, list);
		musb_g_giveback(ep, &req->request, status);
		DBG_LIMIT(5, "call musb_g_giveback on function %s ep is %s\n"
			, __func__,
		    ep->end_point.name);
	}
}

/* ----------------------------------------------------------------------- */

/* Data transfers - pure PIO, pure DMA, or mixed mode */

/*
 * This assumes the separate CPPI engine is responding to DMA requests
 * from the usb core ... sequenced a bit differently from mentor dma.
 */

static inline int max_ep_writesize(struct musb *musb, struct musb_ep *ep)
{
	if (can_bulk_split(musb, ep->type))
		return ep->hw_ep->max_packet_sz_tx;
	else
		return ep->packet_sz;
}


/* Peripheral tx (IN) using Mentor DMA works as follows:
 *	Only mode 0 is used for transfers <= wPktSize,
 *	mode 1 is used for larger transfers,
 *
 *	One of the following happens:
 *	- Host sends IN token which causes an endpoint interrupt
 *		-> TxAvail
 *			-> if DMA is currently busy, exit.
 *			-> if queue is non-empty, txstate().
 *	- Request is queued by the gadget driver.
 *		-> if queue was previously empty, txstate()
 *
 *	txstate()
 *		-> start
 *		  /\	-> setup DMA
 *		  |     (data is transferred to the FIFO, then sent out when
 *		  |	IN token(s) are recd from Host.
 *		  |		-> DMA interrupt on completion
 *		  |		   calls TxAvail.
 *		  |		      -> stop DMA, ~DMAENAB,
 *		  |		      -> set TxPktRdy for last short pkt or zlp
 *		  |		      -> Complete Request
 *		  |		      -> Continue next request (call txstate)
 *		  |___________________________________|
 *
 * Non-Mentor DMA engines can of course work differently, such as by
 * upleveling from irq-per-packet to irq-per-buffer.
 */

/*
 * An endpoint is transmitting data. This can be called either from
 * the IRQ routine or from ep.queue() to kickstart a request on an
 * endpoint.
 *
 * Context: controller locked, IRQs blocked, endpoint selected
 */
static void txstate(struct musb *musb, struct musb_request *req)
{
	u8 epnum = req->epnum;
	struct musb_ep *musb_ep;
	void __iomem *epio = musb->endpoints[epnum].regs;
	struct usb_request *request;
	u16 fifo_count = 0, csr;
	int use_dma = 0;

	musb_ep = req->ep;

	/* Check if EP is disabled */
	if (!musb_ep->desc) {
		DBG(0, "ep:%s disabled - ignore request\n"
				, musb_ep->end_point.name);
		return;
	}

	/* we shouldn't get here while DMA is active ... but we do ... */
	if (dma_channel_status(musb_ep->dma) == MUSB_DMA_STATUS_BUSY) {
		DBG(0, "dma pending...\n");
		return;
	}

	/* read TXCSR before */
	csr = musb_readw(epio, MUSB_TXCSR);

	request = &req->request;
	fifo_count = min(max_ep_writesize(musb, musb_ep)
					, (int)(request->length -
					request->actual));

	if (csr & MUSB_TXCSR_TXPKTRDY) {
		DBG(1, "%s old packet still ready , txcsr %03x\n"
				, musb_ep->end_point.name, csr);
		return;
	}

	if (csr & MUSB_TXCSR_P_SENDSTALL) {
		DBG(0, "%s stalling, txcsr %03x\n"
				, musb_ep->end_point.name, csr);
		return;
	}

	DBG(1, "hw_ep%d, maxpacket %d, fifo count %d, txcsr %03x\n",
	    epnum, musb_ep->packet_sz, fifo_count, csr);

	if (is_buffer_mapped(req)) {
		struct dma_controller *c = musb->dma_controller;
		size_t request_size;

		/* setup DMA, then program endpoint CSR */
		request_size = min_t(size_t, request->length - request->actual,
				     musb_ep->dma->max_len);

		use_dma = (request->dma != DMA_ADDR_INVALID && request_size);

		/* MUSB_TXCSR_P_ISO is still set correctly */

		if (request_size < musb_ep->packet_sz)
			musb_ep->dma->desired_mode = 0;
		else
			musb_ep->dma->desired_mode = 1;

		use_dma = use_dma && c->channel_program(musb_ep->dma,
						musb_ep->packet_sz,
						musb_ep->dma->desired_mode,
						request->dma +
						request->actual,
						request_size);
		if (use_dma) {
			if (musb_ep->dma->desired_mode == 0) {
				/*
				 * We must not clear the DMAMODE bit
				 * before the DMAENAB bit -- and the
				 * latter doesn't always get cleared
				 * before we get here...
				 */
				csr &= ~(MUSB_TXCSR_AUTOSET
						| MUSB_TXCSR_DMAENAB);
				musb_writew(epio, MUSB_TXCSR,
						csr | MUSB_TXCSR_P_WZC_BITS);
				csr &= ~MUSB_TXCSR_DMAMODE;
				csr |= (MUSB_TXCSR_DMAENAB | MUSB_TXCSR_MODE);
				/* against programming guide */
			} else {
				csr |= (MUSB_TXCSR_DMAENAB
					| MUSB_TXCSR_DMAMODE
					| MUSB_TXCSR_MODE);
				if (!musb_ep->hb_mult)
					csr |= MUSB_TXCSR_AUTOSET;
			}
			csr &= ~MUSB_TXCSR_P_UNDERRUN;
			musb_writew(epio, MUSB_TXCSR, csr);
		}
	}


	if (!use_dma) {
		/*
		 * Unmap the dma buffer back to cpu if dma channel
		 * programming fails
		 */
		unmap_dma_buffer(req, musb);

		musb_write_fifo(musb_ep->hw_ep, fifo_count,
				(u8 *) (request->buf + request->actual));
		request->actual += fifo_count;
		csr |= MUSB_TXCSR_TXPKTRDY;
		csr &= ~MUSB_TXCSR_P_UNDERRUN;
		musb_writew(epio, MUSB_TXCSR, csr);
	}

	/* host may already have the data when this message shows... */
	DBG(1, "%s TX/IN %s len %d/%d, txcsr %04x, fifo %d/%d\n",
	    musb_ep->end_point.name, use_dma ? "dma" : "pio",
	    request->actual, request->length,
	    musb_readw(epio, MUSB_TXCSR),
	    fifo_count, musb_readw(epio, MUSB_TXMAXP));

}

/*
 * FIFO state update (e.g. data ready).
 * Called from IRQ,  with controller locked.
 */
void musb_g_tx(struct musb *musb, u8 epnum)
{
	u16 csr;
	struct musb_request *req;
	struct usb_request *request;
	u8 __iomem *mbase = musb->mregs;
	struct musb_ep *musb_ep = &musb->endpoints[epnum].ep_in;
	void __iomem *epio = musb->endpoints[epnum].regs;
	struct dma_channel *dma;

	musb_ep_select(mbase, epnum);
	req = next_request(musb_ep);
	request = &req->request;

	csr = musb_readw(epio, MUSB_TXCSR);
	DBG(1, "<== %s, txcsr %04x\n", musb_ep->end_point.name, csr);

	dma = is_dma_capable() ? musb_ep->dma : NULL;

	/*
	 * REVISIT: for high bandwidth, MUSB_TXCSR_P_INCOMPTX
	 * probably rates reporting as a host error.
	 */
	if (csr & MUSB_TXCSR_P_SENTSTALL) {
		csr |= MUSB_TXCSR_P_WZC_BITS;
		csr &= ~MUSB_TXCSR_P_SENTSTALL;
		musb_writew(epio, MUSB_TXCSR, csr);
		return;
	}

	if (csr & MUSB_TXCSR_P_UNDERRUN) {
		/* We NAKed, no big deal... little reason to care. */
		csr |= MUSB_TXCSR_P_WZC_BITS;
		csr &= ~(MUSB_TXCSR_P_UNDERRUN | MUSB_TXCSR_TXPKTRDY);
		musb_writew(epio, MUSB_TXCSR, csr);
		DBG(1, "underrun on ep%d, req %p\n", epnum, request);
	}

	if (dma_channel_status(dma) == MUSB_DMA_STATUS_BUSY) {
		/*
		 * SHOULD NOT HAPPEN... has with CPPI though, after
		 * changing SENDSTALL (and other cases); harmless?
		 */
		DBG(1, "%s dma still busy?\n", musb_ep->end_point.name);
		return;
	}

	if (request) {
		u8 is_dma = 0;

		if (dma && (csr & MUSB_TXCSR_DMAENAB)) {
			is_dma = 1;
			csr |= MUSB_TXCSR_P_WZC_BITS;
			csr &= ~(MUSB_TXCSR_DMAENAB | MUSB_TXCSR_P_UNDERRUN |
				 MUSB_TXCSR_TXPKTRDY | MUSB_TXCSR_AUTOSET);
			musb_writew(epio, MUSB_TXCSR, csr);
			/* Ensure writebuffer is empty. */
			csr = musb_readw(epio, MUSB_TXCSR);
			request->actual += musb_ep->dma->actual_len;
			DBG(3, "TXCSR%d %04x, DMA off, len %zu, req %p\n",
			    epnum, csr, musb_ep->dma->actual_len, request);
		}

		/*
		 * First, maybe a terminating short packet. Some DMA
		 * engines might handle this by themselves.
		 */
		if ((request->zero && request->length
			&& (request->length % musb_ep->packet_sz == 0)
		     && (request->actual == request->length))
		    || (is_dma && (!dma->desired_mode
		    || (request->actual % musb_ep->packet_sz)))
		    ) {
			/*
			 * On DMA completion, FIFO may not be
			 * available yet...
			 */
			if (csr & MUSB_TXCSR_TXPKTRDY)
				return;

			DBG(4, "sending zero pkt\n");
			musb_writew(epio, MUSB_TXCSR, MUSB_TXCSR_MODE
				    | MUSB_TXCSR_TXPKTRDY
				    | (csr & MUSB_TXCSR_P_ISO));
			request->zero = 0;
			/*
			 * Return from here with the expectation of the endpoint
			 * interrupt for further action.
			 */
			return;
		}

		if (request->actual == request->length) {
#ifdef NEVER
			if (ep_in == &(musb_ep->end_point)) {
				adbCmdLog(request->buf, request->actual,
					musb_ep->is_in, "musb_g_tx");
				/* pr_info("adb: musb_g_tx length = 0x%x,
				 * actual = 0x%x, packet_sz = 0x%x\n",
				 * request->length,
				 * request->actual, musb_ep->packet_sz);
				 */
			}
#endif
			musb_g_giveback(musb_ep, request, 0);
			/*
			 * In the giveback function the MUSB lock is
			 * released and acquired after sometime. During
			 * this time period the INDEX register could get
			 * changed by the gadget_queue function especially
			 * on SMP systems. Reselect the INDEX to be sure
			 * we are reading/modifying the right registers
			 */
			musb_ep_select(mbase, epnum);

			/*
			 * Kickstart next transfer if appropriate;
			 * the packet that just completed might not
			 * be transmitted for hours or days.
			 * REVISIT for double buffering...
			 * FIXME revisit for stalls too...
			 *
			 * If configured as DB, then FIFONOTEMPTY
			 * doesn't mean no space for new packet
			 */

			if (!(musb_read_txfifosz(mbase) & MUSB_FIFOSZ_DPB)) {
				csr = musb_readw(epio, MUSB_TXCSR);
				if (csr & MUSB_TXCSR_FIFONOTEMPTY) {
					if ((csr & MUSB_TXCSR_TXPKTRDY) == 0)
						musb_writew(epio, MUSB_TXCSR
							, MUSB_TXCSR_TXPKTRDY);
					return;
				}
			}

			req = musb_ep->desc ? next_request(musb_ep) : NULL;
			if (!req) {
				DBG(1, "%s idle now\n"
					, musb_ep->end_point.name);
				return;
			}
		}

		txstate(musb, req);
	}
}

/* ------------------------------------------------------------ */


/* Peripheral rx (OUT) using Mentor DMA works as follows:
 *	- Only mode 0 is used.
 *
 *	- Request is queued by the gadget class driver.
 *		-> if queue was previously empty, rxstate()
 *
 *	- Host sends OUT token which causes an endpoint interrupt
 *	  /\      -> RxReady
 *	  |	      -> if request queued, call rxstate
 *	  |		/\	-> setup DMA
 *	  |		|	     -> DMA interrupt on completion
 *	  |		|		-> RxReady
 *	  |		|		      -> stop DMA
 *	  |		|		      -> ack the read
 *	  |		|		      -> if data recd = max expected
 *	  |		|				by the request, or host
 *	  |		|				sent a short packet,
 *	  |		|				complete the request,
 *	  |		|				and start the next one.
 *	  |		|_____________________________________|
 *	  |					 else just wait for the host
 *	  |					    to send the next OUT token.
 *	  |__________________________________________________|
 *
 * Non-Mentor DMA engines can of course work differently.
 */


/*
 * Context: controller locked, IRQs blocked, endpoint selected
 */
static void rxstate(struct musb *musb, struct musb_request *req)
{
	const u8 epnum = req->epnum;
	struct usb_request *request = &req->request;
	struct musb_ep *musb_ep;
	void __iomem *epio = musb->endpoints[epnum].regs;
	unsigned int len = 0;
	u16 fifo_count;
	u16 csr = musb_readw(epio, MUSB_RXCSR);
	struct musb_hw_ep *hw_ep = &musb->endpoints[epnum];
	u8 use_mode_1;

	if (hw_ep->is_shared_fifo)
		musb_ep = &hw_ep->ep_in;
	else
		musb_ep = &hw_ep->ep_out;

	fifo_count = musb_ep->packet_sz;

	/* Check if EP is disabled */
	if (!musb_ep->desc) {
		DBG(0, "ep:%s disabled - ignore request\n"
			, musb_ep->end_point.name);
		return;
	}

	/* We shouldn't get here while DMA is active, but we do... */
	if (is_buffer_mapped(req) &&
		dma_channel_status(musb_ep->dma) ==
		MUSB_DMA_STATUS_BUSY) {
		DBG(0, "DMA pending...\n");
		return;
	}

	if (csr & MUSB_RXCSR_P_SENDSTALL) {
		DBG(0, "%s stalling, RXCSR %04x\n"
			, musb_ep->end_point.name, csr);
		return;
	}

	if (csr & MUSB_RXCSR_RXPKTRDY) {
		fifo_count = musb_readw(epio, MUSB_RXCOUNT);

		DBG(1, "%s epnum %d len %d\n ", __func__, epnum, len);
		/*
		 * Enable Mode 1 on RX transfers only when short_not_ok flag
		 * is set. Currently short_not_ok flag is set only from
		 * file_storage and f_mass_storage drivers
		 */
#ifdef RX_DMA_MODE1
		if (fifo_count == musb_ep->packet_sz)
#else
		if (request->short_not_ok && fifo_count == musb_ep->packet_sz)
#endif
			use_mode_1 = 1;
		else
			use_mode_1 = 0;

		if (request->actual < request->length) {
#ifdef RX_DMA_MODE1
			if (is_buffer_mapped(req) && use_mode_1) {
				struct dma_controller *c;
				struct dma_channel *channel;
				int use_dma = 0;
				int transfer_size;

				c = musb->dma_controller;
				channel = musb_ep->dma;

				/* Experimental: Mode1 works with
				 * mass storage use cases
				 */
				csr |= MUSB_RXCSR_AUTOCLEAR;
				musb_writew(epio, MUSB_RXCSR, csr);
				csr |= MUSB_RXCSR_DMAENAB;
				musb_writew(epio, MUSB_RXCSR, csr);

				musb_writew(epio, MUSB_RXCSR, csr
							| MUSB_RXCSR_DMAMODE);

				transfer_size = min(request->length -
							request->actual,
						    channel->max_len);
				/* Program the transfer length to be
				 * a multiple of packet size because
				 * short packets cant be transferred
				 * over mode1
				 */
				transfer_size = transfer_size -
				    (transfer_size % musb_ep->packet_sz);
				musb_ep->dma->prog_len = transfer_size;

				musb_ep->dma->desired_mode = 1;

				use_dma = c->channel_program(channel,
							musb_ep->packet_sz,
							channel->desired_mode,
							request->dma
							+ request->actual,
							transfer_size);
				if (use_dma)
					return;
			}
#else
			if (is_buffer_mapped(req)) {
				struct dma_controller *c;
				struct dma_channel *channel;
				int use_dma = 0;
				int transfer_size;

				c = musb->dma_controller;
				channel = musb_ep->dma;

				/* We use DMA Req mode 0 in rx_csr,
				 * and DMA controller operates in
				 * mode 0 only. So we do not get
				 * endpoint interrupts due to DMA
				 * completion. We only get interrupts
				 * from DMA controller.
				 * We could operate in DMA mode 1 if
				 * we knew the size of the transfer
				 * in advance. For mass storage class,
				 * request->length = what the host
				 * sends, so that'd work.  But for pretty
				 * much everything else,
				 * request->length is routinely more than
				 * what the host sends. For
				 * most these gadgets, end of is signified
				 * either by a short packet,
				 * or filling the last byte of the buffer.
				 * (Sending extra data in
				 * that last pckate should trigger an
				 * overflow fault.)  But in mode 1,
				 * we don't get DMA completion interrupt
				 * for short packets.
				 *
				 * Theoretically, we could enable DMAReq
				 * irq (MUSB_RXCSR_DMAMODE = 1),
				 * to get endpoint interrupt on every DMA
				 * req, but that didn't seem to work reliably.
				 *
				 * REVISIT an updated g_file_storage
				 * can set req->short_not_ok, which
				 * then becomes usable as a runtime
				 * "use mode 1" hint...
				 */

				/* Experimental: Mode1 works
				 * with mass storage
				 * use cases
				 */
				if (use_mode_1) {
					csr |= MUSB_RXCSR_AUTOCLEAR;
					musb_writew(epio, MUSB_RXCSR, csr);
					csr |= MUSB_RXCSR_DMAENAB;
					musb_writew(epio, MUSB_RXCSR, csr);

					/*
					 * this special sequence
					 * (enabling and then
					 * disabling MUSB_RXCSR_DMAMODE)
					 * is required
					 * to get DMAReq to activate
					 */
					musb_writew(epio, MUSB_RXCSR, csr
							| MUSB_RXCSR_DMAMODE);
					musb_writew(epio, MUSB_RXCSR, csr);

					transfer_size = min_t(unsigned int,
							    request->length -
								request->actual,
							    channel->max_len);
					musb_ep->dma->desired_mode = 1;

				} else {
					/*
					 * Comment out here, cuz we dont have
					 * "hb_mult"
					 * and follow the original setting.
					 * Dont want to
					 * change it.
					 * if (!musb_ep->hb_mult &&
					 * musb_ep->hw_ep->rx_double_buffered)
					 * csr |= MUSB_RXCSR_AUTOCLEAR;
					 */
					csr |= MUSB_RXCSR_DMAENAB;
					musb_writew(epio, MUSB_RXCSR, csr);

					transfer_size = min_t(
							unsigned int,
							request->length -
							request->actual,
							fifo_count);
					musb_ep->dma->desired_mode = 0;
				}

				use_dma =
					c->channel_program(channel,
							musb_ep->packet_sz,
							channel->desired_mode,
							request->dma
							+ request->actual,
							transfer_size);
				if (use_dma)
					return;
			}
#endif
			len = request->length - request->actual;
			DBG(3, "%s OUT/RX pio fifo %d/%d, maxpacket %d\n",
				musb_ep->end_point.name,
				len, fifo_count,
				musb_ep->packet_sz);

			fifo_count =
				min_t(unsigned int, len, fifo_count);

			/*
			 * Unmap the dma buffer back to cpu if dma channel
			 * programming fails. This buffer is mapped if the
			 * channel allocation is successful
			 */
			if (is_buffer_mapped(req)) {
				unmap_dma_buffer(req, musb);

				/*
				 * Clear DMAENAB and AUTOCLEAR for the
				 * PIO mode transfer
				 */
				csr &= ~(MUSB_RXCSR_DMAENAB
						| MUSB_RXCSR_AUTOCLEAR);
				musb_writew(epio, MUSB_RXCSR, csr);
			}

			musb_read_fifo(musb_ep->hw_ep, fifo_count, (u8 *)
				       (request->buf + request->actual));
			request->actual += fifo_count;

			/* REVISIT if we left anything in the fifo, flush
			 * it and report -EOVERFLOW
			 */

			/* ack the read! */
			csr |= MUSB_RXCSR_P_WZC_BITS;
			csr &= ~MUSB_RXCSR_RXPKTRDY;
			musb_writew(epio, MUSB_RXCSR, csr);
		}
	}

	/* reach the end or short packet detected */
	if (request->actual == request->length
		|| fifo_count < musb_ep->packet_sz)
		musb_g_giveback(musb_ep, request, 0);
}

/*
 * Data ready for a request; called from IRQ
 */
void musb_g_rx(struct musb *musb, u8 epnum)
{
	u16 csr;
	struct musb_request *req;
	struct usb_request *request;
	void __iomem *mbase = musb->mregs;
	struct musb_ep *musb_ep;
	void __iomem *epio = musb->endpoints[epnum].regs;
	struct dma_channel *dma;
	struct musb_hw_ep *hw_ep = &musb->endpoints[epnum];
#ifdef RX_DMA_MODE1
	u16 len;
	u32 residue;
	struct dma_controller *c = musb->dma_controller;
	int status;
#endif

	if (hw_ep->is_shared_fifo)
		musb_ep = &hw_ep->ep_in;
	else
		musb_ep = &hw_ep->ep_out;

	musb_ep_select(mbase, epnum);

	req = next_request(musb_ep);


	if (!req) {
#ifdef RX_DMA_MODE1
		musb_ep->rx_pending = 1;
		DBG(2, "Packet received on %s but no request queued\n",
			musb_ep->end_point.name);
#endif
		return;
	}

	request = &req->request;

	csr = musb_readw(epio, MUSB_RXCSR);
	dma = is_dma_capable() ? musb_ep->dma : NULL;

	DBG(1, "<== %s, rxcsr %04x%s %p\n", musb_ep->end_point.name,
	    csr, dma ? " (dma)" : "", request);

	if (csr & MUSB_RXCSR_P_SENTSTALL) {
		csr |= MUSB_RXCSR_P_WZC_BITS;
		csr &= ~MUSB_RXCSR_P_SENTSTALL;
		DBG(0, "%s sendstall on %p\n", musb_ep->name, request);
		musb_writew(epio, MUSB_RXCSR, csr);
		return;
	}

	if (csr & MUSB_RXCSR_P_OVERRUN) {
		/* csr |= MUSB_RXCSR_P_WZC_BITS; */
		csr &= ~MUSB_RXCSR_P_OVERRUN;
		musb_writew(epio, MUSB_RXCSR, csr);

		DBG(0, "%s iso overrun on %p\n", musb_ep->name, request);
		if (request->status == -EINPROGRESS)
			request->status = -EOVERFLOW;
	}
	if (csr & MUSB_RXCSR_INCOMPRX) {
		/* REVISIT not necessarily an error */
		DBG(1, "%s, incomprx\n", musb_ep->end_point.name);
	}

	if (csr & MUSB_RXCSR_FIFOFULL)
		DBG(1, "%s, FIFO full\n", musb_ep->end_point.name);

	if (dma && dma_channel_status(dma) == MUSB_DMA_STATUS_BUSY) {

#ifdef RX_DMA_MODE1
		/* For short_not_ok type transfers and mode0 transfers */
		if (dma->desired_mode == 0 || request->short_not_ok)
			return;

		if (!(csr & MUSB_RXCSR_RXPKTRDY)) {
			DBG(1, "%s, DMA busy and Packet not ready\n",
				musb_ep->end_point.name);
			return;
		}

		/* For Mode1 we get here for the last short packet */
		len = musb_readw(epio, MUSB_RXCOUNT);

		/* We should get here only for a short packet. */
		if (len == musb_ep->packet_sz) {
			DBG(2, "%s, Packet not short RXCOUNT=%d\n",
				musb_ep->end_point.name, len);
			return;
		}

		/* Pause the channel to get the correct transfer residue. */
		status = c->channel_pause(musb_ep->dma);
		residue = c->tx_status(musb_ep->dma);
		status = c->check_residue(musb_ep->dma, residue);

		DBG(2, "len=%d, residue=%d\n", len, residue);

		if (status) {
			/* Something's wrong */
			status = c->channel_resume(musb_ep->dma);
			return;
		}

		/* In cases when we don't know the transfer length the short
		 * packet indicates end of current transfer.
		 */
		status = c->channel_abort(musb_ep->dma);
		/* Update with the actual number of bytes transferred */
		request->actual = musb_ep->dma->prog_len - residue;
		/* Clear DMA bits in the CSR */
		csr &= ~(MUSB_RXCSR_AUTOCLEAR
				| MUSB_RXCSR_DMAENAB
				| MUSB_RXCSR_DMAMODE);
		musb_writew(epio, MUSB_RXCSR, csr);

		/* Proceed to read the short packet */
		rxstate(musb, req);
		/* Don't program next transfer, it will tamper with the DMA
		 * busy condition. Wait for next OUT
		 */
#else
		/* "should not happen"; likely RXPKTRDY pending for DMA */

#endif
		return;
	}

	if (dma && (csr & MUSB_RXCSR_DMAENAB)) {
		csr &= ~(MUSB_RXCSR_AUTOCLEAR
				| MUSB_RXCSR_DMAENAB
				| MUSB_RXCSR_DMAMODE);
		musb_writew(epio, MUSB_RXCSR, MUSB_RXCSR_P_WZC_BITS | csr);

		request->actual += musb_ep->dma->actual_len;
		DBG(1, "RXCSR%d %04x, dma off, %04x, len %zu, req %p ep %d\n",
		    epnum, csr,
		    musb_readw(epio, MUSB_RXCSR),
		    musb_ep->dma->actual_len, request, epnum);
		/* Autoclear doesn't clear RxPktRdy for short packets */
		if ((dma->desired_mode == 0 && !hw_ep->rx_double_buffered)
		    || (dma->actual_len & (musb_ep->packet_sz - 1))) {
			/* ack the read! */
			csr &= ~MUSB_RXCSR_RXPKTRDY;
			musb_writew(epio, MUSB_RXCSR, csr);
		}
#ifdef RX_DMA_MODE1
		/* We get here after DMA completion */
		if ((dma->desired_mode == 1) && (!request->short_not_ok)) {
			/* Incomplete? wait for next OUT packet */
			if (request->actual < request->length) {
				DBG(2, "Wait for next OUT\n");
			} else if (request->actual == request->length) {
				DBG(2, "Transfer over mode1 done\n");
				musb_g_giveback(musb_ep, request, 0);
			} else
				DBG(2, "Transfer length exceeded!!\n");
			return;
		}
#endif

		/* incomplete, and not short? wait for next IN packet */
		if ((request->actual < request->length)
		    && (musb_ep->dma->actual_len == musb_ep->packet_sz)) {
			/* In double buffer case, continue to unload fifo if
			 * there is Rx packet in FIFO.
			 **/
			csr = musb_readw(epio, MUSB_RXCSR);
			if ((csr & MUSB_RXCSR_RXPKTRDY)
				&& hw_ep->rx_double_buffered)
				goto exit;
			return;
		}

		musb_g_giveback(musb_ep, request, 0);
		/*
		 * In the giveback function the MUSB lock is
		 * released and acquired after sometime. During
		 * this time period the INDEX register could get
		 * changed by the gadget_queue function especially
		 * on SMP systems. Reselect the INDEX to be sure
		 * we are reading/modifying the right registers
		 */
		musb_ep_select(mbase, epnum);

		req = next_request(musb_ep);
		if (!req)
			return;
	}
exit:
	/* Analyze request */
	rxstate(musb, req);
}

enum {
	USB_TYPE_UNKNOWN,
	USB_TYPE_ADB,
	USB_TYPE_MTP,
	/* USB_TYPE_PTP, */
	USB_TYPE_RNDIS,
	USB_TYPE_ACM,
};

static struct usb_descriptor_header **
get_function_descriptors(struct usb_function *f,
		     enum usb_device_speed speed)
{
	struct usb_descriptor_header **descriptors;

	switch (speed) {
	case USB_SPEED_SUPER_PLUS:
		descriptors = f->ssp_descriptors;
		if (descriptors)
			break;
	case USB_SPEED_SUPER:
		descriptors = f->ss_descriptors;
		if (descriptors)
			break;
	case USB_SPEED_HIGH:
		descriptors = f->hs_descriptors;
		if (descriptors)
			break;
	default:
		descriptors = f->fs_descriptors;
	}
	return descriptors;
}

static int musb_get_ep_type(struct usb_descriptor_header **f_desc)
{
	struct usb_interface_descriptor *int_desc;
	u8 int_class, int_subclass, int_protocol;

	for (; *f_desc; ++f_desc) {
		if ((*f_desc)->bDescriptorType != USB_DT_INTERFACE)
			continue;
		int_desc = (struct usb_interface_descriptor *)*f_desc;
		int_class = int_desc->bInterfaceClass;
		int_subclass = int_desc->bInterfaceSubClass;
		int_protocol = int_desc->bInterfaceProtocol;

		if (int_class == 0x6 && int_subclass == 0x1
			&& int_protocol == 0x1) {
			return USB_TYPE_MTP;
		} else if (int_class == 0xff && int_subclass == 0x42
			&& int_protocol == 0x1) {
			return USB_TYPE_ADB;
		} else if (int_class == 0x2 && int_subclass == 0x2
			&& int_protocol == 0xff) {
			return USB_TYPE_RNDIS;
		} else if (int_class == 0xe0 && int_subclass == 0x1
			&& int_protocol == 0x3) {
			return USB_TYPE_RNDIS;
		} else if (int_class == 0x2 && int_subclass == 0x2
			&& int_protocol == 0x1) {
			return USB_TYPE_ACM;
		}
	}
	return USB_TYPE_UNKNOWN;
}

/*
 * at the safe mode,
 * ACM IN-BULK-> Double Buffer,
 * OUT-BULK-> Signle Buffer,
 * IN-INT-> Signle Buffer
 * ADB IN-BULK-> Signle Buffer,
 * OUT-BULK-> Signle Buffer
 */

static int is_db_ok(struct musb *musb, struct musb_ep *musb_ep)
{
	struct usb_ep *ep = &musb_ep->end_point;
	struct usb_composite_dev *cdev = (musb->g).ep0->driver_data;
	struct usb_gadget *gadget = &(musb->g);
	struct usb_function *f = NULL;
	struct usb_descriptor_header **f_desc;
	int addr;
	int type = USB_TYPE_UNKNOWN;
	int ret = 1;

	addr = ((ep->address & 0x80) >> 3)
			| (ep->address & 0x0f);
	list_for_each_entry(f, &cdev->config->functions, list) {
		if (test_bit(addr, f->endpoints))
			goto find_f;
	}
	goto done;
find_f:
	f_desc = get_function_descriptors(f, gadget->speed);
	if (f_desc)
		type = musb_get_ep_type(f_desc);
	else
		goto done;

	if (type == USB_TYPE_ACM && !musb_ep->is_in)
		ret = 0;
	else if (type == USB_TYPE_ADB)
		ret = 0;

done:
	return ret;
}

static void fifo_setup(struct musb *musb, struct musb_ep *musb_ep)
{
	void __iomem *mbase = musb->mregs;
	int size = 0;
	u16 maxpacket = musb_ep->fifo_size;
	u16 c_off = musb->fifo_addr >> 3;
	u8 c_size;
	int dbuffer_needed = 0;

	/* expect hw_ep has already been zero-initialized */

	size = ffs(max_t(u16, maxpacket, 8)) - 1;
	maxpacket = 1 << size;

	DBG(0, "musb type=%s\n",
			(musb_ep->type == USB_ENDPOINT_XFER_BULK ? "BULK" :
			(musb_ep->type == USB_ENDPOINT_XFER_INT ? "INT" :
			(musb_ep->type == USB_ENDPOINT_XFER_ISOC ?
			"ISO" : "CONTROL"))));
	c_size = size - 3;

	/* Set double buffer, if the transfer type is bulk or isoc. */
	/* So user need to take care the fifo buffer is enough or not. */
	if (musb_ep->fifo_mode == BUF_DOUBLE
	    && (musb_ep->type == USB_ENDPOINT_XFER_BULK
		|| musb_ep->type == USB_ENDPOINT_XFER_ISOC)) {
			dbuffer_needed = 1;
	}

	if (dbuffer_needed) {
		if ((musb->fifo_addr + (maxpacket << 1)) > (musb->fifo_size)) {
			DBG(0,
				"BUF_DOUBLE USB FIFO is not enough!!! (%d>%d), fifo_addr=%d\n",
			    (musb->fifo_addr + (maxpacket << 1)),
			    (musb->fifo_size),
			    musb->fifo_addr);
			return;
		}

		if (is_saving_mode()) {
			if (is_db_ok(musb, musb_ep)) {
				DBG(0, "Saving mode, but EP%d supports DBBUF\n",
				    musb_ep->current_epnum);
				c_size |= MUSB_FIFOSZ_DPB;
			}
		} else {
			DBG(0, "EP%d supports DBBUF\n",
						musb_ep->current_epnum);
			c_size |= MUSB_FIFOSZ_DPB;
		}
	} else if ((musb->fifo_addr + maxpacket) > (musb->fifo_size)) {
		DBG(0, "BUF_SINGLE USB FIFO is not enough!!! (%d>%d)\n",
		    (musb->fifo_addr + maxpacket), (musb->fifo_size));
		return;
	}

	/* configure the FIFO */
	/* musb_writeb(mbase, MUSB_INDEX, musb_ep->hw_ep->epnum); */
	DBG(0,
		"fifo size is %d after %d, fifo address is %d, epnum %d,hwepnum %d\n",
	    c_size, maxpacket, musb->fifo_addr,
	    musb_ep->current_epnum,
	    musb_ep->hw_ep->epnum);

	if (musb_ep->is_in) {
		musb_write_txfifosz(mbase, c_size);
		musb_write_txfifoadd(mbase, c_off);
	} else {
		musb_write_rxfifosz(mbase, c_size);
		musb_write_rxfifoadd(mbase, c_off);
	}
	musb->fifo_addr += (maxpacket << ((c_size & MUSB_FIFOSZ_DPB) ? 1 : 0));
}

/* ------------------------------------------------------------ */

static int musb_gadget_enable
	(struct usb_ep *ep, const struct usb_endpoint_descriptor *desc)
{
	unsigned long flags;
	struct musb_ep *musb_ep;
	struct musb_hw_ep *hw_ep;
	void __iomem *regs;
	struct musb *musb;
	void __iomem *mbase;
	u8 epnum;
	u16 csr;
	unsigned int tmp;
	int status = -EINVAL;

	if (!ep || !desc)
		return -EINVAL;

	musb_ep = to_musb_ep(ep);
	hw_ep = musb_ep->hw_ep;
	regs = hw_ep->regs;
	musb = musb_ep->musb;
	mbase = musb->mregs;
	epnum = musb_ep->current_epnum;

	spin_lock_irqsave(&musb->lock, flags);

	if (musb_ep->desc) {
		status = -EBUSY;
		goto fail;
	}
	musb_ep->type = usb_endpoint_type(desc);

	/* check direction and (later) maxpacket size against endpoint */
	if (usb_endpoint_num(desc) != epnum)
		goto fail;

	/* REVISIT this rules out high bandwidth periodic transfers */
	tmp = usb_endpoint_maxp_mult(desc) - 1;
	if (tmp) {
		int ok;

		if (usb_endpoint_dir_in(desc))
			ok = musb->hb_iso_tx;
		else
			ok = musb->hb_iso_rx;

		if (!ok) {
			DBG(2, "no support for high bandwidth ISO\n");
			goto fail;
		}
		musb_ep->hb_mult = tmp;
	} else {
		musb_ep->hb_mult = 0;
	}

	musb_ep->packet_sz = usb_endpoint_maxp(desc) & 0x7ff;
	tmp = musb_ep->packet_sz * (musb_ep->hb_mult + 1);

	/* enable the interrupts for the endpoint, set the endpoint
	 * packet size (or fail), set the mode, clear the fifo
	 */
	musb_ep_select(mbase, epnum);
	if (usb_endpoint_dir_in(desc)) {

		if (hw_ep->is_shared_fifo)
			musb_ep->is_in = 1;
		if (!musb_ep->is_in)
			goto fail;

		if (tmp > hw_ep->max_packet_sz_tx) {
			DBG(0, "packet size beyond hardware FIFO size\n");
			goto fail;
		}
#ifndef CONFIG_MTK_MUSB_QMU_SUPPORT
		musb->intrtxe |= (1 << epnum);
		musb_writew(mbase, MUSB_INTRTXE, musb->intrtxe);
#endif

		/* REVISIT if can_bulk_split(), use by updating "tmp";
		 * likewise high bandwidth periodic tx
		 */
		/* Set TXMAXP with the FIFO size of the endpoint
		 * to disable double buffering mode.
		 */
		if (musb->double_buffer_not_ok)
			musb_writew(regs, MUSB_TXMAXP, hw_ep->max_packet_sz_tx);
		else
			musb_writew(regs, MUSB_TXMAXP, musb_ep->packet_sz
				    | (musb_ep->hb_mult << 11));

		csr = MUSB_TXCSR_MODE | MUSB_TXCSR_CLRDATATOG;
		if (musb_readw(regs, MUSB_TXCSR)
		    & MUSB_TXCSR_FIFONOTEMPTY)
			csr |= MUSB_TXCSR_FLUSHFIFO;
		if (musb_ep->type == USB_ENDPOINT_XFER_ISOC)
			csr |= MUSB_TXCSR_P_ISO;

		/* set twice in case of double buffering */
		musb_writew(regs, MUSB_TXCSR, csr);
		/* REVISIT may be inappropriate w/o FIFONOTEMPTY ... */
		musb_writew(regs, MUSB_TXCSR, csr);

	} else {

		if (hw_ep->is_shared_fifo)
			musb_ep->is_in = 0;
		if (musb_ep->is_in)
			goto fail;

		if (tmp > hw_ep->max_packet_sz_rx) {
			DBG(0, "packet size beyond hardware FIFO size\n");
			goto fail;
		}
#ifndef CONFIG_MTK_MUSB_QMU_SUPPORT
		musb->intrrxe |= (1 << epnum);
		musb_writew(mbase, MUSB_INTRRXE, musb->intrrxe);
#endif

		/* REVISIT if can_bulk_combine() use by updating "tmp"
		 * likewise high bandwidth periodic rx
		 */
		/* Set RXMAXP with the FIFO size of the endpoint
		 * to disable double buffering mode.
		 */
		if (musb->double_buffer_not_ok)
			musb_writew(regs, MUSB_RXMAXP, hw_ep->max_packet_sz_tx);
		else
			musb_writew(regs, MUSB_RXMAXP, musb_ep->packet_sz
				    | (musb_ep->hb_mult << 11));

		/* force shared fifo to OUT-only mode */
		if (hw_ep->is_shared_fifo) {
			csr = musb_readw(regs, MUSB_TXCSR);
			csr &= ~(MUSB_TXCSR_MODE | MUSB_TXCSR_TXPKTRDY);
			musb_writew(regs, MUSB_TXCSR, csr);
		}
		/* don't flush fifo when enable, because sometimes usb
		 * will receive packets before ep enabled. when flush fifo
		 * here will lost those packets. We will flush fifo during
		 * disabe ep
		 */
#ifdef NEVER
		csr = MUSB_RXCSR_FLUSHFIFO | MUSB_RXCSR_CLRDATATOG;
		if (musb_ep->type == USB_ENDPOINT_XFER_ISOC)
			csr |= MUSB_RXCSR_P_ISO;
		else if (musb_ep->type == USB_ENDPOINT_XFER_INT)
			csr |= MUSB_RXCSR_DISNYET;

		/* set twice in case of double buffering */
		musb_writew(regs, MUSB_RXCSR, csr);
		musb_writew(regs, MUSB_RXCSR, csr);
#endif
	}

	fifo_setup(musb, musb_ep);

#ifndef CONFIG_MTK_MUSB_QMU_SUPPORT
	/* NOTE:  all the I/O code _should_ work fine without DMA, in case
	 * for some reason you run out of channels here.
	 */
	/* interrupt mode ep don't use dma */
	if (is_dma_capable() && musb->dma_controller
		&& musb_ep->type != USB_ENDPOINT_XFER_INT) {
		struct dma_controller *c = musb->dma_controller;

		musb_ep->dma = c->channel_alloc
			(c, hw_ep, (desc->bEndpointAddress & USB_DIR_IN));
	} else
		musb_ep->dma = NULL;
#endif

	musb_ep->desc = desc;
	musb_ep->busy = 0;
	musb_ep->wedged = 0;
	status = 0;

#ifdef CONFIG_MTK_MUSB_QMU_SUPPORT
	mtk_qmu_enable(musb, epnum, !(musb_ep->is_in));
#endif

	DBG(0, "%s periph: enabled %s for %s %s, %smaxpacket %d\n",
			musb_driver_name, musb_ep->end_point.name, ({
				char *s; switch (musb_ep->type) {
				case USB_ENDPOINT_XFER_BULK:
				s = "bulk"; break; case USB_ENDPOINT_XFER_INT:
				s = "int"; break; default:
				s = "iso"; break; }; s; }
				), musb_ep->is_in ? "IN" : "OUT",
				musb_ep->dma ? "dma, " : ""
				, musb_ep->packet_sz);

	schedule_work(&musb->irq_work);

fail:
	spin_unlock_irqrestore(&musb->lock, flags);
	return status;
}

/*
 * Disable an endpoint flushing all requests queued.
 */
static int musb_gadget_disable(struct usb_ep *ep)
{
	unsigned long flags;
	struct musb *musb;
	u8 epnum;
	struct musb_ep *musb_ep;
	void __iomem *epio;
	int status = 0;
	u16 csr;

	musb_ep = to_musb_ep(ep);
	musb = musb_ep->musb;
	epnum = musb_ep->current_epnum;
	epio = musb->endpoints[epnum].regs;

	spin_lock_irqsave(&musb->lock, flags);
	musb_ep_select(musb->mregs, epnum);

	/* zero the endpoint sizes */
	if (musb_ep->is_in) {
#ifndef CONFIG_MTK_MUSB_QMU_SUPPORT
		musb->intrtxe &= ~(1 << epnum);
		musb_writew(musb->mregs, MUSB_INTRTXE, musb->intrtxe);
#endif
		csr = MUSB_TXCSR_FLUSHFIFO | MUSB_TXCSR_CLRDATATOG;
		/* set twice in case of double buffering */
		musb_writew(epio, MUSB_TXCSR, csr);
		musb_writew(epio, MUSB_TXCSR, csr);

		musb_writew(epio, MUSB_TXMAXP, 0);
	} else {
#ifndef CONFIG_MTK_MUSB_QMU_SUPPORT
		musb->intrrxe &= ~(1 << epnum);
		musb_writew(musb->mregs, MUSB_INTRRXE, musb->intrrxe);
#endif

		/* flush fifo here */
		csr = MUSB_RXCSR_FLUSHFIFO | MUSB_RXCSR_CLRDATATOG;
		/* set twice in case of double buffering */
		musb_writew(epio, MUSB_RXCSR, csr);
		musb_writew(epio, MUSB_RXCSR, csr);

		musb_writew(epio, MUSB_RXMAXP, 0);
	}

	musb_ep->desc = NULL;
	musb_ep->end_point.desc = NULL;

	/* abort all pending DMA and requests */
	nuke(musb_ep, -ESHUTDOWN);

	schedule_work(&musb->irq_work);

	spin_unlock_irqrestore(&(musb->lock), flags);

	DBG(2, "%s\n", musb_ep->end_point.name);

	return status;
}

/*
 * Allocate a request for an endpoint.
 * Reused by ep0 code.
 */
struct usb_request *musb_alloc_request(struct usb_ep *ep, gfp_t gfp_flags)
{
	struct musb_ep *musb_ep = to_musb_ep(ep);
	/* struct musb          *musb = musb_ep->musb; */
	struct musb_request *request = NULL;

	request = kzalloc(sizeof(*request), gfp_flags);
	if (!request)
		return NULL;

	request->request.dma = DMA_ADDR_INVALID;
	request->epnum = musb_ep->current_epnum;
	request->ep = musb_ep;

	return &request->request;
}

/*
 * Free a request
 * Reused by ep0 code.
 */
void musb_free_request(struct usb_ep *ep, struct usb_request *req)
{
	kfree(to_musb_request(req));
}

static LIST_HEAD(buffers);

struct free_record {
	struct list_head list;
	struct device *dev;
	unsigned int bytes;
	dma_addr_t dma;
};

/*
 * Context: controller locked, IRQs blocked.
 */
void musb_ep_restart(struct musb *musb, struct musb_request *req)
{
#ifdef CONFIG_MTK_MUSB_QMU_SUPPORT
	/* limit debug mechanism to avoid too much log */
	static DEFINE_RATELIMIT_STATE(ratelimit, HZ, 10);

	if (__ratelimit(&ratelimit))
		pr_debug("<ratelimit> <== %s request %p len %u on hw_ep%d"
			, req->tx ? "TX/IN" : "RX/OUT"
			, &req->request
			, req->request.length
			, req->epnum);
#else
	DBG(2,
		"<== %s request %p len %u on hw_ep%d\n",
	    req->tx ? "TX/IN" : "RX/OUT", &req->request
	    , req->request.length, req->epnum);
	musb_ep_select(musb->mregs, req->epnum);
	if (req->tx)
		txstate(musb, req);
	else
		rxstate(musb, req);
#endif
}

static int musb_gadget_queue
	(struct usb_ep *ep, struct usb_request *req, gfp_t gfp_flags)
{
	struct musb_ep *musb_ep;
	struct musb_request *request;
	struct musb *musb;
	int status = 0;
	unsigned long lockflags;

	if (!ep || !req)
		return -EINVAL;
	if (!req->buf)
		return -ENODATA;

	musb_ep = to_musb_ep(ep);
	musb = musb_ep->musb;

	request = to_musb_request(req);
	request->musb = musb;

	if (request->ep != musb_ep)
		return -EINVAL;

	DBG(2, "<== to %s request=%p\n", ep->name, req);

	/* request is mine now... */
	request->request.actual = 0;
	request->request.status = -EINPROGRESS;
	request->epnum = musb_ep->current_epnum;
	request->tx = musb_ep->is_in;

	map_dma_buffer(request, musb, musb_ep);

	spin_lock_irqsave(&musb->lock, lockflags);

	/* don't queue if the ep is down */
	if (!musb_ep->desc) {
		DBG(2,
			"req %p queued to %s while ep %s\n",
			req, ep->name, "disabled");
		status = -ESHUTDOWN;
		unmap_dma_buffer(request, musb);
		goto unlock;
	}

	/* add request to the list */
	list_add_tail(&request->list, &musb_ep->req_list);
#ifdef CONFIG_MTK_MUSB_QMU_SUPPORT
	if (request->request.dma != DMA_ADDR_INVALID ||
		request->request.length == 0) {
		/* TX case */
		if (request->tx) {
			/* TX QMU don't have info
			 * for length sent, set this
			 * field in advance
			 */
			request->request.actual = request->request.length;
#ifdef CONFIG_MTK_MUSB_QMU_PURE_ZLP_SUPPORT
			if (request->request.length >= 0) {
#else
			/* only enqueue for length > 0 packet.
			 * Don't send ZLP here for MSC protocol.
			 */
			if (request->request.length > 0) {
#endif
				musb_kick_D_CmdQ(musb, request);

#ifndef CONFIG_MTK_MUSB_QMU_PURE_ZLP_SUPPORT
			/*
			 * for UMS special case
			 */
			} else if (request->request.length == 0) {
				int cnt = 50; /* 50*200us, total 10 ms */
				int is_timeout = 1;

				QMU_WARN("TX ZLP sent case\n");

				/*
				 * wait QMU tx done,
				 * should be enough in UMS
				 * case due to protocol
				 */
				while (cnt--) {
					if (musb_is_qmu_stop(request->epnum,
						request->tx ? 0 : 1)) {
						is_timeout = 0;
						break;
					}
					udelay(200);
				}

				if (!is_timeout) {
					musb_tx_zlp_qmu(musb, request->epnum);
					musb_g_giveback(musb_ep
						, &(request->request), 0);

				} else {
					/* let qmu_done_tx to handle this */
					QMU_WARN
					("TX ZLP sent in qmu_done_tx\n");
					goto unlock;
				}
#endif
			} else {
				QMU_ERR("ERR, TX, request->request.length(%d)\n"
					, request->request.length);
			}
		} else {	/* RX case */
			musb_kick_D_CmdQ(musb, request);
		}
	}
#else
#ifdef RX_DMA_MODE1
	/* it this is the head of the queue, start i/o ... */
	if (!musb_ep->busy && &request->list == musb_ep->req_list.next) {

		/* In case of RX, if there is no packet pending to be read
		 * from fifo then wait for next interrupt
		 */
		if (!request->tx) {
			if (!musb_ep->rx_pending) {
				DBG(2, "No packet pending for %s\n"
						, ep->name);
				goto cleanup;
			} else {
				musb_ep->rx_pending = 0;
				DBG(2, "Read packet from fifo %s\n"
						, ep->name);
			}
		}
		musb_ep_restart(musb, request);
	}
#else
	/* it this is the head of the queue, start i/o ... */
	if (!musb_ep->busy && &request->list == musb_ep->req_list.next)
		musb_ep_restart(musb, request);
#endif
#endif

unlock:
	spin_unlock_irqrestore(&musb->lock, lockflags);
	return status;
}

static int musb_gadget_dequeue(struct usb_ep *ep, struct usb_request *request)
{
	struct musb_ep *musb_ep = to_musb_ep(ep);
	struct musb_request *req = to_musb_request(request);
	struct musb_request *r;
	unsigned long flags;
	int status = 0;
	struct musb *musb = musb_ep->musb;

	if (!ep || !request || to_musb_request(request)->ep != musb_ep)
		return -EINVAL;

	disable_irq_nosync(musb->nIrq);

	spin_lock_irqsave(&musb->lock, flags);

	list_for_each_entry(r, &musb_ep->req_list, list) {
		if (r == req)
			break;
	}
	if (r != req) {
		DBG(2, "request %p not queued to %s\n", request, ep->name);
		status = -EINVAL;
		goto done;
	}

	/* if the hardware doesn't have the request, easy ... */
	if (musb_ep->req_list.next != &req->list || musb_ep->busy)
		musb_g_giveback(musb_ep, request, -ECONNRESET);
#ifdef CONFIG_MTK_MUSB_QMU_SUPPORT
	else {
		QMU_DBG("dequeue req(%p), ep(%d), swep(%d)\n"
			, request, musb_ep->hw_ep->epnum,
			 ep->address);
		musb_flush_qmu(musb_ep->hw_ep->epnum,
				(musb_ep->is_in ? TXQ : RXQ));
		mtk_qmu_enable(musb,
				musb_ep->hw_ep->epnum,
				(musb_ep->is_in ? TXQ : RXQ));
		musb_g_giveback(musb_ep, request, -ECONNRESET);
	}
#else
	/* ... else abort the dma transfer ... */
	else if (is_dma_capable() && musb_ep->dma) {
		struct dma_controller *c = musb->dma_controller;

		musb_ep_select(musb->mregs, musb_ep->current_epnum);
		if (c->channel_abort)
			status = c->channel_abort(musb_ep->dma);
		else
			status = -EBUSY;
		if (status == 0)
			musb_g_giveback(musb_ep, request, -ECONNRESET);
	} else {
		/* NOTE: by sticking to easily tested hardware/driver states,
		 * we leave counting of in-flight packets imprecise.
		 */
		musb_g_giveback(musb_ep, request, -ECONNRESET);
	}
#endif

done:
	spin_unlock_irqrestore(&musb->lock, flags);

	enable_irq(musb->nIrq);
	return status;
}

/*
 * Set or clear the halt bit of an endpoint. A halted enpoint won't tx/rx any
 * data but will queue requests.
 *
 * exported to ep0 code
 */
static int musb_gadget_set_halt(struct usb_ep *ep, int value)
{
	struct musb_ep *musb_ep = to_musb_ep(ep);
	u8 epnum = musb_ep->current_epnum;
	struct musb *musb = musb_ep->musb;
	void __iomem *epio = musb->endpoints[epnum].regs;
	void __iomem *mbase;
	unsigned long flags;
	u16 csr;
	struct musb_request *request;
	int status = 0;

	if (!ep)
		return -EINVAL;
	mbase = musb->mregs;

	spin_lock_irqsave(&musb->lock, flags);

	if (musb_ep->type == USB_ENDPOINT_XFER_ISOC) {
		status = -EINVAL;
		goto done;
	}

	musb_ep_select(mbase, epnum);

	request = next_request(musb_ep);
	if (value) {
		if (request) {
			DBG(0, "request in progress, cannot halt %s\n"
				, ep->name);
			status = -EAGAIN;
			goto done;
		}
		/* Cannot portably stall with non-empty FIFO */
		if (musb_ep->is_in) {
			csr = musb_readw(epio, MUSB_TXCSR);
			if (csr & MUSB_TXCSR_FIFONOTEMPTY) {
				DBG(0, "FIFO busy, cannot halt %s\n"
						, ep->name);
				status = -EAGAIN;
				goto done;
			}
		}
	} else
		musb_ep->wedged = 0;

	/* set/clear the stall and toggle bits */
	DBG(2, "%s: %s stall\n", ep->name, value ? "set" : "clear");
	if (musb_ep->is_in) {
		csr = musb_readw(epio, MUSB_TXCSR);
		csr |= MUSB_TXCSR_P_WZC_BITS | MUSB_TXCSR_CLRDATATOG;
		if (value)
			csr |= MUSB_TXCSR_P_SENDSTALL;
		else
			csr &= ~(MUSB_TXCSR_P_SENDSTALL
					| MUSB_TXCSR_P_SENTSTALL);
		csr &= ~MUSB_TXCSR_TXPKTRDY;
		musb_writew(epio, MUSB_TXCSR, csr);
	} else {
		csr = musb_readw(epio, MUSB_RXCSR);
		csr |= MUSB_RXCSR_P_WZC_BITS
				| MUSB_RXCSR_FLUSHFIFO
				| MUSB_RXCSR_CLRDATATOG;
		if (value)
			csr |= MUSB_RXCSR_P_SENDSTALL;
		else
			csr &= ~(MUSB_RXCSR_P_SENDSTALL
					| MUSB_RXCSR_P_SENTSTALL);
		musb_writew(epio, MUSB_RXCSR, csr);
	}

	/* maybe start the first request in the queue */
	if (!musb_ep->busy && !value && request) {
		DBG(0, "restarting the request\n");
		musb_ep_restart(musb, request);
	}

done:
	spin_unlock_irqrestore(&musb->lock, flags);
	return status;
}

/*
 * Sets the halt feature with the clear requests ignored
 */
static int musb_gadget_set_wedge(struct usb_ep *ep)
{
	struct musb_ep *musb_ep = to_musb_ep(ep);

	if (!ep)
		return -EINVAL;

	musb_ep->wedged = 1;

	return usb_ep_set_halt(ep);
}

static int musb_gadget_fifo_status(struct usb_ep *ep)
{
	struct musb_ep *musb_ep = to_musb_ep(ep);
	void __iomem *epio = musb_ep->hw_ep->regs;
	int retval = -EINVAL;

	if (musb_ep->desc && !musb_ep->is_in) {
		struct musb *musb = musb_ep->musb;
		int epnum = musb_ep->current_epnum;
		void __iomem *mbase = musb->mregs;
		unsigned long flags;

		spin_lock_irqsave(&musb->lock, flags);

		musb_ep_select(mbase, epnum);
		/* FIXME return zero unless RXPKTRDY is set */
		retval = musb_readw(epio, MUSB_RXCOUNT);

		spin_unlock_irqrestore(&musb->lock, flags);
	}
	return retval;
}

static void musb_gadget_fifo_flush(struct usb_ep *ep)
{
	struct musb_ep *musb_ep = to_musb_ep(ep);
	struct musb *musb = musb_ep->musb;
	u8 epnum = musb_ep->current_epnum;
	void __iomem *epio = musb->endpoints[epnum].regs;
	void __iomem *mbase;
	unsigned long flags;
	u16 csr;

	mbase = musb->mregs;

	spin_lock_irqsave(&musb->lock, flags);
	musb_ep_select(mbase, (u8) epnum);

#ifndef CONFIG_MTK_MUSB_QMU_SUPPORT
	/* disable interrupts */
	musb_writew(mbase, MUSB_INTRTXE, musb->intrtxe & ~(1 << epnum));
#endif

	if (musb_ep->is_in) {
#ifdef CONFIG_MTK_MUSB_QMU_SUPPORT
		QMU_WARN("fifo flush(%d), sw(%d)\n", epnum, ep->address);
		musb_flush_qmu(epnum, TXQ);
		musb_restart_qmu(musb, epnum, TXQ);
#endif
		csr = musb_readw(epio, MUSB_TXCSR);
		if (csr & MUSB_TXCSR_FIFONOTEMPTY) {
			csr |= MUSB_TXCSR_FLUSHFIFO | MUSB_TXCSR_P_WZC_BITS;
			/*
			 * Setting both TXPKTRDY and FLUSHFIFO makes controller
			 * to interrupt current FIFO loading, but not flushing
			 * the already loaded ones.
			 */
			csr &= ~MUSB_TXCSR_TXPKTRDY;
			musb_writew(epio, MUSB_TXCSR, csr);
			/* REVISIT may be inappropriate w/o FIFONOTEMPTY ... */
			musb_writew(epio, MUSB_TXCSR, csr);
		}
	} else {
#ifdef CONFIG_MTK_MUSB_QMU_SUPPORT
		QMU_WARN("fifo flush(%d), sw(%d)\n", epnum, ep->address);
		musb_flush_qmu(epnum, RXQ);
		musb_restart_qmu(musb, epnum, RXQ);
#endif
		csr = musb_readw(epio, MUSB_RXCSR);
		csr |= MUSB_RXCSR_FLUSHFIFO | MUSB_RXCSR_P_WZC_BITS;
		musb_writew(epio, MUSB_RXCSR, csr);
		musb_writew(epio, MUSB_RXCSR, csr);
	}

#ifndef CONFIG_MTK_MUSB_QMU_SUPPORT
	/* re-enable interrupt */
	musb_writew(mbase, MUSB_INTRTXE, musb->intrtxe);
#endif
	spin_unlock_irqrestore(&musb->lock, flags);
}

#if defined(CONFIG_MTK_MD_DIRECT_TETHERING_SUPPORT)
static void musb_gadget_suspend_control(struct usb_ep *ep)
{
	struct musb_ep	*musb_ep = to_musb_ep(ep);
	struct musb	*musb = musb_ep->musb;
	u8		epnum = musb_ep->current_epnum;
	unsigned long	flags;

	DBG(2, "%s : %s...", __func__, ep->name);

	spin_lock_irqsave(&musb->lock, flags);

	nuke(musb_ep, -ECONNRESET);

#ifdef CONFIG_MTK_MUSB_QMU_SUPPORT
	if (musb_ep->is_in) { /* TX */
		musb_writeb(musb->mregs, MUSB_QIMSR,
			musb_readb(musb->mregs, MUSB_QIMSR)
			| ((1<<0)<<(epnum)));
	} else {
		musb_writeb(musb->mregs, MUSB_QIMSR,
			musb_readb(musb->mregs, MUSB_QIMSR)
			| ((1<<8)<<(epnum)));
	}
#endif


	spin_unlock_irqrestore(&musb->lock, flags);
}

static void musb_gadget_resume_control(struct usb_ep *ep)
{
	struct musb_ep	*musb_ep = to_musb_ep(ep);
	struct musb	*musb = musb_ep->musb;
	u8		epnum = musb_ep->current_epnum;
	unsigned long	flags;

	int timeout_count = 100; /* 50ms*100 */
	u32 int_md = musb_readb(musb->mregs, MUSB_USB_MDL1INTM);

	DBG(2, "%s : %s...", __func__, ep->name);

	/* check if MD finish deactivate follow */

	while (int_md && (timeout_count != 0)) {
		mdelay(50);
		int_md = musb_readb(musb->mregs, MUSB_USB_MDL1INTM);
		DBG(2, "%s : int_md: %d\n", __func__, int_md);
		timeout_count--;
	}

	DBG(2,
		"%s : timeout_count: %d\n", __func__
		, timeout_count);

	if (int_md)
		DBG(2,
			"ep resume timeout, U3D_LV1IER_MD:%x\n"
			, int_md);

	spin_lock_irqsave(&musb->lock, flags);

	nuke(musb_ep, -ECONNRESET);

#ifdef CONFIG_MTK_MUSB_QMU_SUPPORT
	if (musb_ep->is_in) { /* TX */
		musb_writeb(musb->mregs, MUSB_USBGCSR,
			musb_readb(musb->mregs, MUSB_USBGCSR)
				| ((1<<0)<<(epnum)));
		musb_writeb(musb->mregs, MUSB_QIMCR,
				musb_readb(musb->mregs, MUSB_QIMCR)
				| ((1<<0)<<(epnum)));
		musb_restart_qmu(musb, epnum, 0);
	} else {
		musb_writeb(musb->mregs,
			MUSB_USBGCSR, musb_readb(musb->mregs, MUSB_USBGCSR)
			| ((16<<0)<<(epnum)));
		musb_writeb(musb->mregs,
			MUSB_QIMCR, musb_readb(musb->mregs, MUSB_QIMCR)
			| ((8<<0)<<(epnum)));
		musb_restart_qmu(musb, epnum, 1);
	}
#endif
	spin_unlock_irqrestore(&musb->lock, flags);
}
#endif

static const struct usb_ep_ops musb_ep_ops = {
	.enable = musb_gadget_enable,
	.disable = musb_gadget_disable,
	.alloc_request = musb_alloc_request,
	.free_request = musb_free_request,
	.queue = musb_gadget_queue,
	.dequeue = musb_gadget_dequeue,
	.set_halt = musb_gadget_set_halt,
	.set_wedge = musb_gadget_set_wedge,
	.fifo_status = musb_gadget_fifo_status,
	.fifo_flush = musb_gadget_fifo_flush,
#if defined(CONFIG_MTK_MD_DIRECT_TETHERING_SUPPORT)
	|| defined(CONFIG_MTK_MD_DIRECT_LOGGING_SUPPORT)
	.suspend_control = musb_gadget_suspend_control,
	.resume_control	= musb_gadget_resume_control,
#endif
};

/* ----------------------------------------------------------------------- */

static int musb_gadget_get_frame(struct usb_gadget *gadget)
{
	struct musb *musb = gadget_to_musb(gadget);

	return (int)musb_readw(musb->mregs, MUSB_FRAME);
}

static int musb_gadget_wakeup(struct usb_gadget *gadget)
{
	struct musb *musb = gadget_to_musb(gadget);
	void __iomem *mregs = musb->mregs;
	unsigned long flags;
	int status = -EINVAL;
	u8 power, devctl;
	int retries;

	spin_lock_irqsave(&musb->lock, flags);

	switch (musb->xceiv->otg->state) {
	case OTG_STATE_B_PERIPHERAL:
		/* NOTE:  OTG state machine doesn't include B_SUSPENDED;
		 * that's part of the standard usb 1.1 state machine, and
		 * doesn't affect OTG transitions.
		 */
		if (musb->may_wakeup && musb->is_suspended)
			break;
		goto done;
	case OTG_STATE_B_IDLE:
		/* Start SRP ... OTG not required. */
		devctl = musb_readb(mregs, MUSB_DEVCTL);
		DBG(2, "Sending SRP: devctl: %02x\n", devctl);
		devctl |= MUSB_DEVCTL_SESSION;
		musb_writeb(mregs, MUSB_DEVCTL, devctl);
		devctl = musb_readb(mregs, MUSB_DEVCTL);
		retries = 100;
		while (!(devctl & MUSB_DEVCTL_SESSION)) {
			devctl = musb_readb(mregs, MUSB_DEVCTL);
			if (retries-- < 1)
				break;
		}
		retries = 10000;
		while (devctl & MUSB_DEVCTL_SESSION) {
			devctl = musb_readb(mregs, MUSB_DEVCTL);
			if (retries-- < 1)
				break;
		}

		spin_unlock_irqrestore(&musb->lock, flags);
		otg_start_srp(musb->xceiv->otg);
		spin_lock_irqsave(&musb->lock, flags);

		/* Block idling for at least 1s */
		musb_platform_try_idle
			(musb, jiffies + msecs_to_jiffies(1 * HZ));

		status = 0;
		goto done;
	default:
		DBG(2,
			"Unhandled wake: %s\n",
			otg_state_string(musb->xceiv->otg->state));
		goto done;
	}

	status = 0;

	power = musb_readb(mregs, MUSB_POWER);
	power |= MUSB_POWER_RESUME;
	musb_writeb(mregs, MUSB_POWER, power);
	DBG(2, "issue wakeup\n");

	/* FIXME do this next chunk in a timer callback, no udelay */
	mdelay(2);

	power = musb_readb(mregs, MUSB_POWER);
	power &= ~MUSB_POWER_RESUME;
	musb_writeb(mregs, MUSB_POWER, power);
done:
	spin_unlock_irqrestore(&musb->lock, flags);
	return status;
}

static int musb_gadget_set_self_powered
		(struct usb_gadget *gadget, int is_selfpowered)
{
	struct musb *musb = gadget_to_musb(gadget);

	musb->is_self_powered = !!is_selfpowered;
	return 0;
}

static void musb_pullup(struct musb *musb, int is_on, bool usb_in)
{
	u8 power;

	DBG(0,
		"MUSB: gadget pull up %d start, musb->power:%d\n"
		, is_on, musb->power);
	if (musb->power) {
		power = musb_readb(musb->mregs, MUSB_POWER);
		if (is_on)
			power |= (MUSB_POWER_SOFTCONN | MUSB_POWER_ENSUSPEND);
		else
			power &= ~(MUSB_POWER_SOFTCONN | MUSB_POWER_ENSUSPEND);
		musb_writeb(musb->mregs, MUSB_POWER, power);
	}
	DBG(0, "MUSB: gadget pull up %d end\n", is_on);
}

#ifdef NEVER
static int musb_gadget_vbus_session(struct usb_gadget *gadget, int is_active)
{
	DBG(2, "<= %s =>\n", __func__);

	/*
	 * FIXME iff driver's softconnect flag is set (as it is during probe,
	 * though that can clear it), just musb_pullup().
	 */

	return -EINVAL;
}
#endif

static int musb_gadget_vbus_draw
		(struct usb_gadget *gadget, unsigned int mA)
{
	struct musb *musb = gadget_to_musb(gadget);

	if (!musb->xceiv->set_power)
		return -EOPNOTSUPP;
	return usb_phy_set_power(musb->xceiv, mA);
}

/* default value 0 */
static int usb_rdy;
void set_usb_rdy(void)
{
	DBG(0, "set usb_rdy, wake up bat\n");
	usb_rdy = 1;
}

bool is_usb_rdy(void)
{
	if (usb_rdy)
		return true;
	else
		return false;
}
EXPORT_SYMBOL(is_usb_rdy);

static int musb_gadget_pullup(struct usb_gadget *gadget, int is_on)
{
	struct musb *musb = gadget_to_musb(gadget);
	unsigned long        flags;
	bool usb_in = false;

	DBG(0, "is_on=%d, softconnect=%d ++\n", is_on, musb->softconnect);

	is_on = !!is_on;
	pm_runtime_get_sync(musb->controller);

	/* NOTE: this assumes we are sensing vbus; we'd rather
	 * not pullup unless the B-session is active.
	 */

	spin_lock_irqsave(&musb->lock, flags);

	/* MTK additional */
	DBG(0, "is_on=%d, softconnect=%d ++\n", is_on, musb->softconnect);

	if (is_on != musb->softconnect) {
		musb->softconnect = is_on;
		musb_pullup(musb, is_on, usb_in);
	}

	if (!musb->is_ready && is_on) {
		musb->is_ready = true;
		set_usb_rdy();
		/* direct issue connection work if usb is forced on */
		if (musb_force_on) {
			DBG(0, "mt_usb_connect() on is_ready begin\n");
			mt_usb_connect();
		} else {
			DBG(0, "mt_usb_reconnect() on is_ready begin\n");
			mt_usb_reconnect();
		}
	}

	spin_unlock_irqrestore(&musb->lock, flags);

	pm_runtime_put(musb->controller);

	return 0;
}

static int musb_gadget_start
		(struct usb_gadget *g, struct usb_gadget_driver *driver);
static int musb_gadget_stop(struct usb_gadget *g);

static const struct usb_gadget_ops musb_gadget_operations = {
	.get_frame = musb_gadget_get_frame,
	.wakeup = musb_gadget_wakeup,
	.set_selfpowered = musb_gadget_set_self_powered,
	/* .vbus_session                = musb_gadget_vbus_session, */
	.vbus_draw = musb_gadget_vbus_draw,
	.pullup = musb_gadget_pullup,
	.udc_start = musb_gadget_start,
	.udc_stop = musb_gadget_stop,
};

/* ----------------------------------------------------------------------- */

/* Registration */

/* Only this registration code "knows" the rule (from USB standards)
 * about there being only one external upstream port.  It assumes
 * all peripheral ports are external...
 */
static void init_peripheral_ep
			(struct musb *musb,
			struct musb_ep *ep,
			u8 epnum, int is_in)
{
	struct musb_hw_ep *hw_ep = musb->endpoints + epnum;

	/* memset(ep, 0, sizeof *ep); */

	ep->current_epnum = epnum;
	ep->musb = musb;
	ep->hw_ep = hw_ep;
	ep->is_in = is_in;

	INIT_LIST_HEAD(&ep->req_list);

	sprintf(ep->name, "ep%d%s", epnum,
		(!epnum || hw_ep->is_shared_fifo)
			? "" : (is_in ? "in" : "out"));

	ep->end_point.name = ep->name;
	INIT_LIST_HEAD(&ep->end_point.ep_list);
	if (!epnum) {
		ep->end_point.maxpacket = 64;
		ep->end_point.maxpacket_limit = 64;
		ep->end_point.caps.type_control = true;
		ep->end_point.ops = &musb_g_ep0_ops;
		musb->g.ep0 = &ep->end_point;
	} else {
		if (is_in) {
			ep->end_point.maxpacket = hw_ep->max_packet_sz_tx;
			ep->end_point.maxpacket_limit = hw_ep->max_packet_sz_tx;
		} else {
			ep->end_point.maxpacket = hw_ep->max_packet_sz_rx;
			ep->end_point.maxpacket_limit = hw_ep->max_packet_sz_rx;
		}

		ep->end_point.caps.type_iso = true;
		ep->end_point.caps.type_bulk = true;
		ep->end_point.caps.type_int = true;
		ep->end_point.ops = &musb_ep_ops;
		list_add_tail(&ep->end_point.ep_list, &musb->g.ep_list);
	}

	if (!epnum || hw_ep->is_shared_fifo) {
		ep->end_point.caps.dir_in = true;
		ep->end_point.caps.dir_out = true;
	} else if (is_in)
		ep->end_point.caps.dir_in = true;
	else
		ep->end_point.caps.dir_out = true;
}

/*
 * Initialize the endpoints exposed to peripheral drivers, with backlinks
 * to the rest of the driver state.
 */
static inline void musb_g_init_endpoints(struct musb *musb)
{
	u8 epnum;
	struct musb_hw_ep *hw_ep;
	unsigned int count = 0;

	/* initialize endpoint list just once */
	INIT_LIST_HEAD(&(musb->g.ep_list));

	for (epnum = 0, hw_ep = musb->endpoints;
			epnum < musb->nr_endpoints; epnum++, hw_ep++) {
		if (hw_ep->is_shared_fifo /* || !epnum */) {
			init_peripheral_ep(musb, &hw_ep->ep_in, epnum, 0);
			count++;
		} else {
			if (hw_ep->max_packet_sz_tx) {
				init_peripheral_ep(musb, &hw_ep->ep_in
								, epnum, 1);
				count++;
			}
			if (hw_ep->max_packet_sz_rx) {
				init_peripheral_ep(musb, &hw_ep->ep_out
								, epnum, 0);
				count++;
			}
		}
	}
}

/* called once during driver setup to initialize and link into
 * the driver model; memory is zeroed.
 */
int musb_gadget_setup(struct musb *musb)
{
	int status;

	/* REVISIT minor race:  if (erroneously) setting up two
	 * musb peripherals at the same time, only the bus lock
	 * is probably held.
	 */

	musb->g.ops = &musb_gadget_operations;
	musb->g.max_speed = USB_SPEED_HIGH;
	musb->g.speed = USB_SPEED_UNKNOWN;

	/* this "gadget" abstracts/virtualizes the controller */
	musb->g.name = musb_driver_name;
	musb->g.is_otg = 1;

	musb_g_init_endpoints(musb);

	musb->is_active = 0;
	musb_platform_try_idle(musb, 0);

	/* Fix: gadget device dma ops is null,so add musb controller dma ops */
	/* to gadget device dma ops, otherwise will go do dma dump ops. */
#ifdef CONFIG_XEN
	if (musb->controller->archdata.dev_dma_ops) {
		DBG(0, "musb controller dma ops is non-null\n");
		musb->g.dev.archdata.dev_dma_ops =
			musb->controller->archdata.dev_dma_ops;
	}
#endif

	status = usb_add_gadget_udc(musb->controller, &musb->g);
	if (status)
		goto err;

	return 0;
err:
	musb->g.dev.parent = NULL;
	device_unregister(&musb->g.dev);
	return status;
}

void musb_gadget_cleanup(struct musb *musb)
{
	usb_del_gadget_udc(&musb->g);
}

/*
 * Register the gadget driver. Used by gadget drivers when
 * registering themselves with the controller.
 *
 * -EINVAL something went wrong (not driver)
 * -EBUSY another gadget is already using the controller
 * -ENOMEM no memory to perform the operation
 *
 * @param driver the gadget driver
 * @return <0 if error, 0 if everything is fine
 */
static int musb_gadget_start
			(struct usb_gadget *g, struct usb_gadget_driver *driver)
{
	struct musb *musb = gadget_to_musb(g);
	struct usb_otg *otg = musb->xceiv->otg;
	unsigned long flags;
	int retval = 0;
	enum usb_otg_state state = OTG_STATE_UNDEFINED;
	unsigned int is_active = 0;

	DBG(0, "%s\n", __func__);

	if (driver->max_speed < USB_SPEED_HIGH) {
		retval = -EINVAL;
		goto err;
	}

	pm_runtime_get_sync(musb->controller);

	DBG(2, "registering driver %s\n", driver->function);

	musb->softconnect = 0;
	musb->gadget_driver = driver;

	spin_lock_irqsave(&musb->lock, flags);

	if (is_host_active(musb)) {
		is_active = musb->is_active;
		state = musb->xceiv->otg->state;
	}

	/* MTK hack, leave this to connection work */
	musb->is_active = 0;

	otg_set_peripheral(otg, &musb->g);
	musb->xceiv->otg->state = OTG_STATE_B_IDLE;

	if (is_host_active(musb)) {
		musb->is_active = is_active;
		musb->xceiv->otg->state = state;
	}

	spin_unlock_irqrestore(&musb->lock, flags);

	/* REVISIT:  funcall to other code, which also
	 * handles power budgeting ... this way also
	 * ensures HdrcStart is indirectly called.
	 */
	if ((musb->xceiv->last_event == USB_EVENT_ID)
	    && otg->set_vbus)
		otg_set_vbus(otg, 1);

	if (musb->xceiv->last_event == USB_EVENT_NONE)
		pm_runtime_put(musb->controller);

	return 0;

err:
	return retval;
}

#ifdef CONFIG_USB_G_ANDROID
static void stop_activity(struct musb *musb)
{
	int i;
	struct musb_hw_ep *hw_ep;

	/* don't disconnect if it's not connected */
	if (musb->g.speed != USB_SPEED_UNKNOWN)
		musb->g.speed = USB_SPEED_UNKNOWN;

	/* deactivate the hardware */
	if (musb->softconnect) {
		musb->softconnect = 0;
		musb_pullup(musb, 0, false);
	}
	musb_stop(musb);

	/* killing any outstanding requests will quiesce the driver;
	 * then report disconnect
	 */
	if (musb) {
		for (i = 0, hw_ep = musb->endpoints;
			i < musb->nr_endpoints; i++, hw_ep++) {
			musb_ep_select(musb->mregs, i);
			if (hw_ep->is_shared_fifo /* || !epnum */) {
				nuke(&hw_ep->ep_in, -ESHUTDOWN);
			} else {
				if (hw_ep->max_packet_sz_tx)
					nuke(&hw_ep->ep_in, -ESHUTDOWN);
				if (hw_ep->max_packet_sz_rx)
					nuke(&hw_ep->ep_out, -ESHUTDOWN);
			}
		}
	}
}
#endif

/*
 * Unregister the gadget driver. Used by gadget drivers when
 * unregistering themselves from the controller.
 *
 * @param driver the gadget driver to unregister
 */
static int musb_gadget_stop(struct usb_gadget *g)
{
	struct musb *musb = gadget_to_musb(g);
	unsigned long flags;
	enum usb_otg_state state = OTG_STATE_UNDEFINED;
	unsigned int is_active = 0;

	DBG(0, "%s\n", __func__);

	if (musb->xceiv->last_event == USB_EVENT_NONE)
		pm_runtime_get_sync(musb->controller);

	/*
	 * REVISIT always use otg_set_peripheral() here too;
	 * this needs to shut down the OTG engine.
	 */

	spin_lock_irqsave(&musb->lock, flags);

	musb_hnp_stop(musb);

	(void)musb_gadget_vbus_draw(&musb->g, 0);

	if (is_host_active(musb)) {
		is_active = musb->is_active;
		state = musb->xceiv->otg->state;
	}

	musb->xceiv->otg->state = OTG_STATE_UNDEFINED;
#ifdef CONFIG_USB_G_ANDROID
	stop_activity(musb);
#endif
	otg_set_peripheral(musb->xceiv->otg, NULL);

	musb->is_active = 0;
	musb->gadget_driver = NULL;
	musb_platform_try_idle(musb, 0);

	if (is_host_active(musb)) {
		musb->is_active = is_active;
		musb->xceiv->otg->state = state;
	}

	spin_unlock_irqrestore(&musb->lock, flags);

	/*
	 * FIXME we need to be able to register another
	 * gadget driver here and have everything work;
	 * that currently misbehaves.
	 */

	pm_runtime_put(musb->controller);

	return 0;
}

/* ----------------------------------------------------------------------- */

/* lifecycle operations called through plat_uds.c */

void musb_g_resume(struct musb *musb)
{
	musb->is_suspended = 0;
	switch (musb->xceiv->otg->state) {
	case OTG_STATE_B_IDLE:
		break;
	case OTG_STATE_B_WAIT_ACON:
	case OTG_STATE_B_PERIPHERAL:
		musb->is_active = 1;
		if (musb->gadget_driver && musb->gadget_driver->resume) {
			spin_unlock(&musb->lock);
			musb->gadget_driver->resume(&musb->g);
			spin_lock(&musb->lock);
		}
		break;
	default:
		pr_notice("unhandled RESUME transition (%s)\n"
			, otg_state_string(musb->xceiv->otg->state));
	}
}

/* called when SOF packets stop for 3+ msec */
void musb_g_suspend(struct musb *musb)
{
	u8 devctl;

	devctl = musb_readb(musb->mregs, MUSB_DEVCTL);
	DBG(0, "devctl %02x\n", devctl);

	switch (musb->xceiv->otg->state) {
	case OTG_STATE_B_IDLE:
		if ((devctl & MUSB_DEVCTL_VBUS) == MUSB_DEVCTL_VBUS)
			musb->xceiv->otg->state = OTG_STATE_B_PERIPHERAL;
		break;
	case OTG_STATE_B_PERIPHERAL:
		musb->is_suspended = 1;
		if (musb->gadget_driver && musb->gadget_driver->suspend) {
			spin_unlock(&musb->lock);
			musb->gadget_driver->suspend(&musb->g);
			spin_lock(&musb->lock);
		}
		musb_sync_with_bat(musb, USB_SUSPEND);
		break;
	default:
		/* REVISIT if B_HOST, clear DEVCTL.HOSTREQ;
		 * A_PERIPHERAL may need care too
		 */
		pr_notice("unhandled SUSPEND transition (%s)\n",
			otg_state_string(musb->xceiv->otg->state));
	}
}

/* Called during SRP */
void musb_g_wakeup(struct musb *musb)
{
	musb_gadget_wakeup(&musb->g);
}

#if defined(CONFIG_USBIF_COMPLIANCE)
static unsigned long vbus_polling_timeout;

int polling_vbus_value(void *data)
{
	unsigned int vbus_value;
	bool timeout_flag = false;
	u8 devctl;
	u8 power;
	u8 opstate;

	while (!kthread_should_stop()) {
		timeout_flag = false;
#if defined(CONFIG_USBIF_COMPLIANCE_PMIC)
		polling_vbus = true;
		vbus_value = PMIC_IMM_GetOneChannelValue(AUX_VCDT_AP, 1, 1);
		vbus_value = (((R_CHARGER_1 + R_CHARGER_2) * 100 * vbus_value)
						/ R_CHARGER_2) / 100;
#else
		vbus_value = battery_meter_get_charger_voltage();
#endif
		DBG(0, "musb::Vbus (%d)\n", vbus_value);
		DBG(0,
			"OTG_State: (%s)\n"
			, otg_state_string(mtk_musb->xceiv->state));

		switch (mtk_musb->xceiv->otg->state) {
		case OTG_STATE_B_IDLE:
		case OTG_STATE_B_PERIPHERAL:
			vbus_polling_timeout = jiffies + 5 * HZ;

			while (vbus_value < 3800) {
				DBG(0,
					"%s: not above B-device operating voltage! (%d)\n"
					, __func__,
				    vbus_value);
				if (time_after(jiffies, vbus_polling_timeout)) {
					timeout_flag = true;
					break;
				}
				mdelay(10);
#if defined(CONFIG_USBIF_COMPLIANCE_PMIC)
				vbus_value =
					PMIC_IMM_GetOneChannelValue(AUX_VCDT_AP
							, 1, 1);
				vbus_value =
				    (((R_CHARGER_1 +
				       R_CHARGER_2) * 100 * vbus_value)
				       / R_CHARGER_2) / 100;
#else
				vbus_value =
					battery_meter_get_charger_voltage();
#endif
			}
			DBG(0, "%s: Vbus (%d)\n", __func__, vbus_value);

			if (!timeout_flag) {
				DBG(0,
					"CONNECT USB (B-device Operating Voltage! (%d)\n",
				    vbus_value);
				mt_usb_connect();
			}
			break;

		case OTG_STATE_B_SRP_INIT:
			while (vbus_value > 700) {
				if (time_after(jiffies, vbus_polling_timeout)) {
					timeout_flag = true;
					break;
				}
				mdelay(10);
#if defined(CONFIG_USBIF_COMPLIANCE_PMIC)
				vbus_value =
					PMIC_IMM_GetOneChannelValue(AUX_VCDT_AP
						, 1, 1);
				vbus_value =
				    (((R_CHARGER_1 +
				       R_CHARGER_2) * 100 * vbus_value)
				       / R_CHARGER_2) / 100;
#else
				vbus_value =
					battery_meter_get_charger_voltage();
#endif
			}
			DBG(0, "%s: Vbus (%d)\n", __func__, vbus_value);

			{
				u32 val = 0;

				val = USBPHY_READ32(0x6c);
				val = (val & ~(0xff<<0)) | (0x13<<0);
				USBPHY_WRITE32(0x6c, val);

				val = USBPHY_READ32(0x6c);
				val = (val & ~(0xff<<8)) | (0x3f<<8);
				USBPHY_WRITE32(0x6c, val);
			}

			/* Set Vbus Pulsing Length */
			musb_writeb(mtk_musb->mregs, 0x7B, 1);

			mdelay(1800);
			devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);
			DBG(0, "Sending SRP: devctl: %02x\n", devctl);
			devctl |= MUSB_DEVCTL_SESSION;
			musb_writeb(mtk_musb->mregs, MUSB_DEVCTL, devctl);
			devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);
			DBG(0, "Sending SRP Done: devctl: %02x\n", devctl);

			DBG(0, "before OTG_STATE_B_IDLE\n");
			mtk_musb->xceiv->otg->state = OTG_STATE_B_IDLE;
			DBG(0, "%s - after OTG_STATE_B_IDLE\n", __func__);

			vbus_polling_timeout = jiffies + 5 * HZ;

			while (vbus_value < 4000) {
				DBG(0,
					"musb::not above Session-Valid! (%d)\n",
					vbus_value);
				if (time_after(jiffies, vbus_polling_timeout)) {
					timeout_flag = true;
					break;
				}
				mdelay(20);
#if defined(CONFIG_USBIF_COMPLIANCE_PMIC)
				vbus_value =
					PMIC_IMM_GetOneChannelValue(AUX_VCDT_AP
						, 1, 1);
				vbus_value =
				    (((R_CHARGER_1 +
				       R_CHARGER_2) * 100 * vbus_value)
				       / R_CHARGER_2) / 100;
#else
				vbus_value =
					battery_meter_get_charger_voltage();
#endif
			}
			DBG(0, "musb::Vbus (%d)\n", vbus_value);

			if (!timeout_flag) {
				u32 val = 0;

				val = USBPHY_READ32(0x6c);
				val = (val & ~(0xff<<0)) | (0x2f<<0);
				USBPHY_WRITE32(0x6c, val);


				power = musb_readb(mtk_musb->mregs, MUSB_POWER);
				DBG(0, "Setting SOFT CONNECT: power: %02x\n"
					, power);
				power |= MUSB_POWER_SOFTCONN;
				musb_writeb(mtk_musb->mregs, MUSB_POWER, power);
				power = musb_readb(mtk_musb->mregs, MUSB_POWER);
				DBG(0,
					"Setting SOFT CONNECT Done: power: %02x\n"
					, power);
			} else {
				devctl = musb_readb(mtk_musb->mregs
							, MUSB_DEVCTL);
				opstate = musb_readb(mtk_musb->mregs
							, MUSB_OPSTATE);
				DBG(0,
					"SRP: Polling VBUS TimeOut, DEVCTL: 0x%x, OPSTATE: 0x%x\n"
					, devctl, opstate);
				send_otg_event(OTG_EVENT_NO_RESP_FOR_SRP);
				polling_vbus = false;
				mt_usb_disconnect();
			}
			DBG(0, "%s - Done - %s\n", __func__,
			    otg_state_string(mtk_musb->xceiv->otg->state));

			break;
#ifdef NEVER
		case OTG_STATE_A_WAIT_BCON:
		case OTG_STATE_A_IDLE:
		case OTG_STATE_A_WAIT_VRISE:
		case OTG_STATE_A_WAIT_BCON:
		case OTG_STATE_A_HOST:
		case OTG_STATE_A_SUSPEND:
		case OTG_STATE_A_WAIT_VFALL:
		case OTG_STATE_A_VBUS_ERR:
		case OTG_STATE_A_PERIPHERAL:
			pmic_bvalid_det_int_en(0);
			break;
#endif
		}

		DBG(0, "musb::enable mt_usb_disconnect!\n");
		polling_vbus = false;
		DBG(0, "Re-Schedule vbus_polling_tsk (TASK_INTERRRUPTIBLE)!\n");
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}

	DBG(0, "SET Current State - vbus_polling_tsk (TASK_RUNNING)!\n");
	__set_current_state(TASK_RUNNING);
	return 1;
}
#endif


/* called when VBUS drops below session threshold, and in other cases */
void musb_g_disconnect(struct musb *musb)
{
	void __iomem *mregs = musb->mregs;
	u8 devctl = musb_readb(mregs, MUSB_DEVCTL);

	DBG(2, "devctl %02x\n", devctl);

#if defined(CONFIG_USBIF_COMPLIANCE)
	pr_info("%s: %02x, otg_srp_rqd: 0x%x (%s)\n"
		, __func__, devctl, musb->g.otg_srp_reqd,
		otg_state_string(musb->xceiv->otg->state));
	pr_info("devctl %02x\n", devctl);
	if (musb->g.otg_srp_reqd)
		musb->xceiv->otg->state = OTG_STATE_B_SRP_INIT;
#endif

	/* clear HR */
	musb_writeb(mregs, MUSB_DEVCTL, devctl & MUSB_DEVCTL_SESSION);

	/* don't draw vbus until new b-default session */
	(void)musb_gadget_vbus_draw(&musb->g, 0);

	musb->g.speed = USB_SPEED_UNKNOWN;
	if (musb->gadget_driver && musb->gadget_driver->disconnect) {
		spin_unlock(&musb->lock);
		musb->gadget_driver->disconnect(&musb->g);
		spin_lock(&musb->lock);
	}

	switch (musb->xceiv->otg->state) {
	default:
		DBG(2, "Unhandled disconnect %s, setting a_idle\n",
		    otg_state_string(musb->xceiv->otg->state));
		musb->xceiv->otg->state = OTG_STATE_A_IDLE;
		MUSB_HST_MODE(musb);
		break;
	case OTG_STATE_A_PERIPHERAL:
		musb->xceiv->otg->state = OTG_STATE_A_WAIT_BCON;
		MUSB_HST_MODE(musb);
		break;
	case OTG_STATE_B_WAIT_ACON:
	case OTG_STATE_B_HOST:
	case OTG_STATE_B_PERIPHERAL:
	case OTG_STATE_B_IDLE:
		musb->xceiv->otg->state = OTG_STATE_B_IDLE;
#if defined(CONFIG_USBIF_COMPLIANCE)
		pr_info("%s: %x\n", __func__, musb->g.host_request);
		musb_set_host_request_flag(musb, 0);
#endif
		break;
	case OTG_STATE_B_SRP_INIT:
#if defined(CONFIG_USBIF_COMPLIANCE)
		pr_info("%s: %s\n", __func__,
			otg_state_string(musb->xceiv->otg->state));
		if (musb->g.otg_srp_reqd) {
			pr_info("disconnect, Check otg_srp_reqd: 0x%x,  devctl %02x\n",
				musb->g.otg_srp_reqd, devctl);
			musb->g.otg_srp_reqd = 0;

			/* Add 0.5 seconds to fix TD 5.1-5s. */
			vbus_polling_timeout = jiffies + 5 * HZ;
			wake_up_process(vbus_polling_tsk);
		}
#endif
		break;
	}

	musb->is_active = 0;
}

void musb_g_reset(struct musb *musb)
	__releases(musb->lock) __acquires(musb->lock)
{
	void __iomem *mbase = musb->mregs;
	u8 devctl = musb_readb(mbase, MUSB_DEVCTL);
	u8 power;
	struct musb_ep		*ep;

	DBG(2, "<== %s driver '%s'\n", (devctl & MUSB_DEVCTL_BDEVICE)
	    ? "B-Device" : "A-Device",
	    musb->gadget_driver ? musb->gadget_driver->driver.name : NULL);

	if (musb->test_mode == 0)
		musb_sync_with_bat(musb, USB_UNCONFIGURED);

	/* report disconnect, if we didn't already (flushing EP state) */
	if (musb->g.speed != USB_SPEED_UNKNOWN)
		musb_g_disconnect(musb);

	/* clear HR */
	else if (devctl & MUSB_DEVCTL_HR)
		musb_writeb(mbase, MUSB_DEVCTL, MUSB_DEVCTL_SESSION);

	/* active wake lock */
	if (!musb->usb_lock->active)
		__pm_stay_awake(musb->usb_lock);

	/* re-init interrupt setting */
	musb->intrrxe = 0;
	musb_writew(mbase, MUSB_INTRRXE, musb->intrrxe);
	musb->intrtxe = 0x1;
	musb_writew(mbase, MUSB_INTRTXE, musb->intrtxe);

	musb_writeb(mbase, MUSB_INTRUSBE,
					MUSB_INTR_SUSPEND
					| MUSB_INTR_RESUME
					| MUSB_INTR_RESET
#if defined(CONFIG_USBIF_COMPLIANCE)
			/*
			 * Trying to Fix not CONNECT
			 * in B_WAIT_ACON
			 */

		    | MUSB_INTR_CONNECT
#endif
		    | MUSB_INTR_DISCONNECT);

	/* what speed did we negotiate? */
	power = musb_readb(mbase, MUSB_POWER);
	musb->g.speed = (power & MUSB_POWER_HSMODE)
	    ? USB_SPEED_HIGH : USB_SPEED_FULL;

	/* clear address */
	musb_writeb(musb->mregs, MUSB_FADDR, 0);

	/* reset fifo size */
	musb->fifo_addr = FIFO_START_ADDR;

	/* start in USB_STATE_DEFAULT */
	musb->is_active = 1;
	musb->is_suspended = 0;
	MUSB_DEV_MODE(musb);
	musb->address = 0;
	musb->ep0_state = MUSB_EP0_STAGE_SETUP;

	musb->may_wakeup = 0;
	musb->g.b_hnp_enable = 0;
	musb->g.a_alt_hnp_support = 0;
	musb->g.a_hnp_support = 0;

	ep = &musb->endpoints[0].ep_in;
	if (!list_empty(&ep->req_list)) {
		DBG(0, "%s reinit EP[0] req_list\n", __func__);
		INIT_LIST_HEAD(&ep->req_list);
	}

	/* Normal reset, as B-Device;
	 * or else after HNP, as A-Device
	 */
	if (devctl & MUSB_DEVCTL_BDEVICE) {
		musb->xceiv->otg->state = OTG_STATE_B_PERIPHERAL;
		musb->g.is_a_peripheral = 0;
	} else {
		musb->xceiv->otg->state = OTG_STATE_A_PERIPHERAL;
		musb->g.is_a_peripheral = 1;
	}

	/* start with default limits on VBUS power draw */
	(void)musb_gadget_vbus_draw(&musb->g, 8);
}
