/*
 * MUSB OTG driver peripheral support
 *
 * Copyright 2005 Mentor Graphics Corporation
 * Copyright (C) 2005-2006 by Texas Instruments
 * Copyright (C) 2006-2007 Nokia Corporation
 * Copyright (C) 2009 MontaVista Software, Inc. <source@mvista.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
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

#include <linux/usb/composite.h>

#include <linux/musb/musb_core.h>

#define FIFO_START_ADDR 512

//#define RX_DMA_MODE1 1

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

//__ADB_DEBUG__ start
extern struct usb_ep	*ep_in;
extern struct usb_ep	*ep_out;
extern int bitdebug_enabled;
extern unsigned bitdebug_writeCnt;
extern unsigned bitdebug_readCnt;

struct amessage {
    unsigned command;       /* command identifier constant      */
    unsigned arg0;          /* first argument                   */
    unsigned arg1;          /* second argument                  */
    unsigned data_length;   /* length of payload (0 is allowed) */
    unsigned data_check;    /* checksum of data payload         */
    unsigned magic;         /* command ^ 0xffffffff             */
};

struct debuginfo {
	unsigned headtoken;
	unsigned command;       /* command identifier constant      */
	unsigned msg_check;
	unsigned data_check;
	unsigned count;
	unsigned dummy;
	unsigned tailtoken;
};
typedef struct amessage amessage;
typedef struct debuginfo debuginfo;


#define A_SYNC 0x434e5953
#define A_CNXN 0x4e584e43
#define A_OPEN 0x4e45504f
#define A_OKAY 0x59414b4f
#define A_CLSE 0x45534c43
#define A_WRTE 0x45545257
#define A_AUTH 0x48545541
#define A_DBUG 0x41424a42
#define DBGHEADTOKEN 0x13579bdf
#define DBGTAILTOKEN 0xdca86420

struct adbDbg_t{
	u32 cmdChkSum;
	u32 dataSize;
	u32 dataChkSum;
};

typedef struct adbDbg_t adbDbg_t;

adbDbg_t adbDbg[2] = {{0},{0}};
adbDbg_t adbDbgTest;
spinlock_t debugLock;

struct kfifo fifo;
#define FIFO_SIZE	32

static u32 adbDoChkSum(u32 length, u8 *buf)
{
	u32 i;
	u32 chkSum = 0;
	for (i=0;i<length;i++)
		chkSum ^= buf[i];
	return chkSum;
}

static void adbCmdLog(u8 *buf, u32 length, u8 is_in, char *func)
{
	amessage *msg = (amessage*) buf;
	int status = -1;


	if(bitdebug_enabled == 1)
	{
		if(msg != NULL)
		{
			if(sizeof(amessage) == length)
			{
				switch(msg->command)
				{
					case A_SYNC:
					case A_CNXN:
					case A_OPEN:
					case A_OKAY:
					case A_CLSE:
					case A_WRTE:
					case A_AUTH:
						status = 0;
						break;
					case A_DBUG:
						printk(KERN_INFO "adb: %s ERROR A_DBUG should not be tranfsered \n", func);
						break;
					default:
						status = 1;
						break;
				}
			}
			else
			{
				status = 1;
				//printk(KERN_INFO "adb: %s A_DATA, data length = 0x%x \n", func, length);
			}
		}
		else
			printk(KERN_INFO "adb: %s ERROR: amessage = NULL \n", func);

		if (0 == status)
		{
			adbDbg[is_in].dataSize  = msg->data_length;
			adbDbg[is_in].cmdChkSum = adbDoChkSum(length, buf);

			if(0 == adbDbg[is_in].dataSize)
				adbDbg[is_in].dataChkSum = 0;

			printk(KERN_INFO "adb: %s cmd = 0x%x, pack length = 0x%x, checksum = 0x%x \n", func, msg->command, msg->data_length, adbDbg[is_in].cmdChkSum);
		}

		if (1 == status)
		{
			if(adbDbg[is_in].dataSize != length)
			{
				if (0 != length)
				  printk(KERN_INFO "adb: %s ERROR: data size not match, adbDbg.dataSize = 0x%x, actual = 0x%x \n", func, adbDbg[is_in].dataSize, length);
			}
			else
			{
				adbDbg[is_in].dataChkSum = adbDoChkSum(length, buf);
				printk(KERN_INFO "adb: %s data length = 0x%x, checksum = 0x%x \n", func, length, adbDbg[is_in].dataChkSum);
			}
		}
	}
}

static int adbDbgInfoCheck(u8 *buf, u32 length, u8 is_in, char *func)
{
	debuginfo *dbg = (debuginfo*) buf;
	int status = -1;

	if(dbg != NULL)
	{
		if(sizeof(debuginfo) == length)
		{
			switch(dbg->command)
			{
				case A_DBUG:
					//printk(KERN_INFO "adb: %s dbg->headtoken = 0x%x, dbg->tailtoken = 0x%x, is_in = %d \n", func, dbg->headtoken , dbg->tailtoken, is_in);
					//printk(KERN_INFO "adb: %s dbg->msg_check = 0x%x, dbg->data_check = 0x%x, is_in = %d \n", func, dbg->msg_check , dbg->data_check, is_in);
					if(dbg->command == A_DBUG && dbg->headtoken == DBGHEADTOKEN && dbg->tailtoken == DBGTAILTOKEN)
					{
						status = 0;
						if (adbDbg[is_in].cmdChkSum != dbg->msg_check)
							printk(KERN_INFO "adb: %s ERROR: cmdChkSum = 0x%x, msg_check = 0x%x, is_in = %d \n", func, adbDbg[is_in].cmdChkSum, dbg->msg_check, is_in);
						//else
						//	printk(KERN_INFO "adb: %s cmdChkSum match, count = %d \n", func, bitdebug_writeCnt);


						if (adbDbg[is_in].dataChkSum != dbg->data_check)
							printk(KERN_INFO "adb: %s ERROR: dataChkSum = 0x%x, data_check = 0x%x, is_in = %d \n", func, adbDbg[is_in].dataChkSum, dbg->data_check, is_in);
						//else
						//	printk(KERN_INFO "adb: %s dataChkSum match, count = %d \n", func, bitdebug_writeCnt);

						adbDbg[is_in].cmdChkSum = 0;
						adbDbg[is_in].dataChkSum = 0;

						if (bitdebug_writeCnt != dbg->count)
							printk(KERN_INFO "adb: %s ERROR: miss count = %d, dbg->count = %d \n", func, bitdebug_writeCnt, dbg->count);

						bitdebug_writeCnt++;
					}
					else
						printk(KERN_INFO "adb: %s ERROR: not A_DBUG, data length = 0x%x, is_in = %d \n", func, length, is_in);
					break;
			}
		}
	}
	else
		printk(KERN_INFO "adb: %s ERROR: debuginfo = NULL, is_in = %d \n", func, is_in);

	return status;
}

static int adbDebugInfoWrite(struct musb_ep *ep, struct usb_request *req)
{
	static u32  isDebugCmd = 0;
	static bool isDataCmd = false;
	static bool isNormalCmd = false;
	static bool packLength = 0;
	unsigned long flags;

	if (!((1 == ep->is_in) && (ep_in == &(ep->end_point))))
		return -1;

	spin_lock_irqsave(&debugLock, flags);
	if(req->length == sizeof(amessage)){
		amessage *msg = (amessage*) req->buf;
		if(msg != NULL){
			switch(msg->command){
				case A_SYNC:
				case A_CNXN:
				case A_OPEN:
				case A_OKAY:
				case A_CLSE:
				case A_WRTE:
				case A_AUTH:
					isNormalCmd = true;
					packLength = msg->data_length;
					printk(KERN_INFO "adb: adb_complete_in msg (0x%x) (0x%x) (0x%x), isNormalCmd = %d \n", msg->command, msg->data_length, msg->data_check, isNormalCmd);
					break;
				default:
					isDataCmd = true;
					printk(KERN_INFO "adb: adb_complete_in msg A_DATA, isDataCmd = %d \n", isDataCmd);
					break;
			}
		}
	}
	else
		if(req->length == sizeof(debuginfo)){
			debuginfo *dbg = (debuginfo*) req->buf;
			if(dbg != NULL && dbg->command == A_DBUG && dbg->headtoken == DBGHEADTOKEN && dbg->tailtoken == DBGTAILTOKEN){
				isDebugCmd++;
				printk(KERN_INFO "adb: adb_complete_in A_DBUG (0x%x) (0x%x) (0x%x), isDebugCmd = %d \n", dbg->command, dbg->msg_check, dbg->data_check, isDebugCmd);
				//if (false == isDataCmd)
				//  printk(KERN_INFO "adb_complete_in dbg WARNING, Data is not ready \n");
				kfifo_in(&fifo, dbg, sizeof(debuginfo));
				//printk(KERN_INFO "adb_complete_in A_DBUG a \n");
			}
		else
		{
			isDataCmd = true;
			printk(KERN_INFO "adb: adb_complete_in msg A_DATA, isDataCmd = %d \n", isDataCmd);
		}
	}
	else
	{
		isDataCmd = true;
		printk(KERN_INFO "adb: adb_complete_in msg A_DATA, isDataCmd = %d \n", isDataCmd);
	}

	if ((isNormalCmd && isDataCmd && isDebugCmd) || (isNormalCmd && (0 == packLength) && isDebugCmd))
	{
		debuginfo tmp;
		unsigned int ret;
		ret = kfifo_out(&fifo, &tmp, sizeof(debuginfo));
		//printk(KERN_INFO "adb_complete_in kfifo_out = %d \n", isDebugCmd);
		if(-1 < adbDbgInfoCheck((u8 *)&tmp, sizeof(debuginfo), 1, "adb_complete_in"))
		{
			isDebugCmd--;
			isDataCmd  = false;
			isNormalCmd = false;
			packLength = 0;
			//printk(KERN_INFO "adb_complete_in Clear  isDebugCmd = 0x%x, isDataCmd = %d, isNormalCmd = %d \n", isDebugCmd, isDataCmd, isNormalCmd);
		}
		else
			printk(KERN_INFO "adb: adb_complete_in ERROR adbDbgInfoCheck = %d, headtoken = 0x%x, command = 0x%x, msg_check = 0x%x, data_check = 0x%x \n",
			isDebugCmd, tmp.headtoken, tmp.command, tmp.msg_check, tmp.data_check);
  }

	spin_unlock_irqrestore(&debugLock, flags);
	return 0;
}

static int adbDegInfoHandle(struct usb_ep *ep, struct usb_request *req, char *func)
{
	struct musb_ep		*musb_ep;
	struct musb_request	*request;
	int status = -1;

	if(bitdebug_enabled == 1){
		musb_ep = to_musb_ep(ep);
		request = to_musb_request(req);

		if((musb_ep->is_in == 0) && (ep_out == &(musb_ep->end_point)))
		{
			if(req->length == sizeof(debuginfo))
			{
				debuginfo *dbg = (debuginfo*) req->buf;
				if(dbg != NULL && dbg->command == A_DBUG){
					dbg->msg_check = adbDbg[musb_ep->is_in].cmdChkSum;
					dbg->data_check = adbDbg[musb_ep->is_in].dataChkSum;
					dbg->count = bitdebug_readCnt++;
					//printk(KERN_INFO "adb: %s dbg (0x%x) (0x%x) (0x%x) \n", func, dbg->command, dbg->msg_check, dbg->data_check);
					request->request.complete(&request->ep->end_point, &request->request);
					return -EINPROGRESS;
				}
			}
		}

		if((musb_ep->is_in == 1) && (ep_in == &(musb_ep->end_point)))
		{
#if 0
	    if(req->length == sizeof(amessage))
	    {
	  		amessage *msg = (amessage*) req->buf;
	  		if(msg != NULL){
	  			switch(msg->command){
					case A_SYNC:
					case A_CNXN:
					case A_OPEN:
					case A_OKAY:
					case A_CLSE:
					case A_WRTE:
					case A_AUTH:
	  					printk(KERN_INFO "adb: %s msg (0x%x) (0x%x) (0x%x) (0x%x) (0x%x) (0x%x) \n", func, msg->command, msg->arg0, msg->arg1,
							msg->data_length, msg->data_check, msg->magic);
	  					break;
					default:
						printk(KERN_INFO "adb: %s msg A_DATA or A_DBUG \n", func);
						break;
				}
			}
		}
#endif
			if(req->length == sizeof(debuginfo))
			{
				debuginfo *dbg = (debuginfo*) req->buf;
				if(dbg != NULL && dbg->command == A_DBUG){
					//printk(KERN_INFO "adb: %s dbg (0x%x) (0x%x) (0x%x) \n", func, dbg->command, dbg->msg_check, dbg->data_check);
					adbDebugInfoWrite(musb_ep, req);
					request->request.complete(&request->ep->end_point, &request->request);
					return 0;
				}
			}
		}
	}

	return status;
}

//__ADB_DEBUG__ end

#define is_buffer_mapped(req) (is_dma_capable() && \
					(req->map_state != UN_MAPPED))

/* Maps the buffer to dma  */

static inline void map_dma_buffer(struct musb_request *request,
			struct musb *musb, struct musb_ep *musb_ep)
{
	int compatible = true;
	struct dma_controller *dma = musb->dma_controller;

	unsigned length;
	length = ALIGN(request->request.length ,dma_get_cache_alignment());


	request->map_state = UN_MAPPED;

	if (!is_dma_capable() || !musb_ep->dma)
		return;

	/* Check if DMA engine can handle this request.
	 * DMA code must reject the USB request explicitly.
	 * Default behaviour is to map the request.
	 */
	if (dma->is_compatible)
		compatible = dma->is_compatible(musb_ep->dma,
				musb_ep->packet_sz, request->request.buf,
				request->request.length);
	if (!compatible)
		return;

	if (request->request.dma == DMA_ADDR_INVALID) {
		request->request.dma = dma_map_single(
				musb->controller,
				request->request.buf,
				length,
				request->tx
					? DMA_TO_DEVICE
					: DMA_FROM_DEVICE);
		request->map_state = MUSB_MAPPED;
	} else {
		dma_sync_single_for_device(musb->controller,
			request->request.dma,
			length,
			request->tx
				? DMA_TO_DEVICE
				: DMA_FROM_DEVICE);
		request->map_state = PRE_MAPPED;
	}
}

/* Unmap the buffer from dma and maps it back to cpu */
static inline void unmap_dma_buffer(struct musb_request *request,
				struct musb *musb)
{
	unsigned length;
	length = ALIGN(request->request.length ,dma_get_cache_alignment());

	if (!is_buffer_mapped(request))
		return;

	if (request->request.dma == DMA_ADDR_INVALID) {
		DBG(1, "not unmapping a never mapped buffer\n");
		return;
	}
	if (request->map_state == MUSB_MAPPED) {
		dma_unmap_single(musb->controller,
			request->request.dma,
			length,
			request->tx
				? DMA_TO_DEVICE
				: DMA_FROM_DEVICE);
		request->request.dma = DMA_ADDR_INVALID;
	} else { /* PRE_MAPPED */
		dma_sync_single_for_cpu(musb->controller,
			request->request.dma,
			length,
			request->tx
				? DMA_TO_DEVICE
				: DMA_FROM_DEVICE);
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
void musb_g_giveback(
	struct musb_ep		*ep,
	struct usb_request	*request,
	int			status)
__releases(ep->musb->lock)
__acquires(ep->musb->lock)
{
	struct musb_request	*req;
	struct musb		*musb;
	int			busy = ep->busy;

	req = to_musb_request(request);

	list_del(&req->list);
	if (req->request.status == -EINPROGRESS)
		req->request.status = status;
	musb = req->musb;

	ep->busy = 1;
	spin_unlock(&musb->lock);
	unmap_dma_buffer(req, musb);
	if (request->status == 0)
		DBG(1, "%s done request %p,  %d/%d\n",
				ep->end_point.name, request,
				req->request.actual, req->request.length);
	else
		DBG(1, "%s request %p, %d/%d fault %d\n",
				ep->end_point.name, request,
				req->request.actual, req->request.length,
				request->status);
	//__ADB_DEBUG__ start
	if(bitdebug_enabled == 1){
		adbDebugInfoWrite(ep, request);
	}
	//__ADB_DEBUG__ end
	req->request.complete(&req->ep->end_point, &req->request);
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
	/*struct musb		*musb = ep->musb;*/
	struct musb_request	*req = NULL;
	void __iomem *epio = ep->musb->endpoints[ep->current_epnum].regs;

	ep->busy = 1;

	if (is_dma_capable() && ep->dma) {
		struct dma_controller	*c = ep->musb->dma_controller;
		int value;

		if (ep->is_in) {
			/*
			 * The programming guide says that we must not clear
			 * the DMAMODE bit before DMAENAB, so we only
			 * clear it in the second write...
			 */
			musb_writew(epio, MUSB_TXCSR,
				    MUSB_TXCSR_DMAMODE | MUSB_TXCSR_FLUSHFIFO);
			musb_writew(epio, MUSB_TXCSR,
					0 | MUSB_TXCSR_FLUSHFIFO);
		} else {
			musb_writew(epio, MUSB_RXCSR,
					0 | MUSB_RXCSR_FLUSHFIFO);
			musb_writew(epio, MUSB_RXCSR,
					0 | MUSB_RXCSR_FLUSHFIFO);
		}

		value = c->channel_abort(ep->dma);
		DBG(0, "%s: %s: abort DMA --> %d\n", __func__, ep->name, value);
		c->channel_release(ep->dma);
		ep->dma = NULL;
	}

	while (!list_empty(&ep->req_list)) {
		req = list_first_entry(&ep->req_list, struct musb_request, list);
		musb_g_giveback(ep, &req->request, status);
		DBG(0,"call musb_g_giveback on function %s ep is %s\n", __func__,ep->end_point.name);
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
	Only mode 0 is used for transfers <= wPktSize,
	mode 1 is used for larger transfers,

	One of the following happens:
	- Host sends IN token which causes an endpoint interrupt
		-> TxAvail
			-> if DMA is currently busy, exit.
			-> if queue is non-empty, txstate().

	- Request is queued by the gadget driver.
		-> if queue was previously empty, txstate()

	txstate()
		-> start
		  /\	-> setup DMA
		  |     (data is transferred to the FIFO, then sent out when
		  |	IN token(s) are recd from Host.
		  |		-> DMA interrupt on completion
		  |		   calls TxAvail.
		  |		      -> stop DMA, ~DMAENAB,
		  |		      -> set TxPktRdy for last short pkt or zlp
		  |		      -> Complete Request
		  |		      -> Continue next request (call txstate)
		  |___________________________________|

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
	u8			epnum = req->epnum;
	struct musb_ep		*musb_ep;
	void __iomem		*epio = musb->endpoints[epnum].regs;
	struct usb_request	*request;
	u16			fifo_count = 0, csr;
	int			use_dma = 0;

	musb_ep = req->ep;

	/* Check if EP is disabled */
	if (!musb_ep->desc) {
		DBG(0, "ep:%s disabled - ignore request\n",
			musb_ep->end_point.name);
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
	fifo_count = min(max_ep_writesize(musb, musb_ep),
			(int)(request->length - request->actual));

	if (csr & MUSB_TXCSR_TXPKTRDY) {
		DBG(1, "%s old packet still ready , txcsr %03x\n",
				musb_ep->end_point.name, csr);
		return;
	}

	if (csr & MUSB_TXCSR_P_SENDSTALL) {
		DBG(0, "%s stalling, txcsr %03x\n",
				musb_ep->end_point.name, csr);
		return;
	}

	DBG(1, "hw_ep%d, maxpacket %d, fifo count %d, txcsr %03x\n",
			epnum, musb_ep->packet_sz, fifo_count,
			csr);

	USB_LOGGER(TXSTATE, TXSTATE, epnum, musb_ep->packet_sz, fifo_count, csr);

	if (is_buffer_mapped(req)) {
		struct dma_controller	*c = musb->dma_controller;
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

		use_dma = use_dma && c->channel_program(
				musb_ep->dma, musb_ep->packet_sz,
				musb_ep->dma->desired_mode,
				request->dma + request->actual, request_size);
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
				musb_writew(epio, MUSB_TXCSR, csr
					| MUSB_TXCSR_P_WZC_BITS);
					csr &= ~MUSB_TXCSR_DMAMODE;
					csr |= (MUSB_TXCSR_DMAENAB |
							MUSB_TXCSR_MODE);
				/* against programming guide */
			} else {
					csr |= (MUSB_TXCSR_DMAENAB
						| MUSB_TXCSR_DMAMODE
							| MUSB_TXCSR_MODE);
				if (!musb_ep->hb_mult)
					csr |= MUSB_TXCSR_AUTOSET;
			}
			csr &= ~MUSB_TXCSR_P_UNDERRUN;

			//__ADB_DEBUG__ start
			if(bitdebug_enabled == 1){
				if(ep_in == &(musb_ep->end_point)){
					adbCmdLog(request->buf, request->length, musb_ep->is_in, "txstate");
					//printk(KERN_INFO "adb: musb_g_tx length = 0x%x, actual = 0x%x, packet_sz = 0x%x \n", request->length, request->actual, musb_ep->packet_sz);
				}
			}
			//__ADB_DEBUG__ end
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
			fifo_count,
			musb_readw(epio, MUSB_TXMAXP));

	USB_LOGGER( TXSTATE_END, TXSTATE, musb_ep->end_point.name, use_dma ? "dma" : "pio",
			request->actual, request->length, musb_readw(epio, MUSB_TXCSR), fifo_count,
			musb_readw(epio, MUSB_TXMAXP));
}

/*
 * FIFO state update (e.g. data ready).
 * Called from IRQ,  with controller locked.
 */
void musb_g_tx(struct musb *musb, u8 epnum)
{
	u16			csr;
	struct musb_request	*req;
	struct usb_request	*request;
	u8 __iomem		*mbase = musb->mregs;
	struct musb_ep		*musb_ep = &musb->endpoints[epnum].ep_in;
	void __iomem		*epio = musb->endpoints[epnum].regs;
	struct dma_channel	*dma;

	musb_ep_select(mbase, epnum);
	req = next_request(musb_ep);
	request = &req->request;

	csr = musb_readw(epio, MUSB_TXCSR);
	DBG(1, "<== %s, txcsr %04x\n", musb_ep->end_point.name, csr);


	USB_LOGGER(MUSB_G_TX, MUSB_G_TX, musb_ep->end_point.name, csr);

	//__ADB_DEBUG__ start
	if(bitdebug_enabled == 1){
		//if(ep_in == &(musb_ep->end_point)){
		//	printk(KERN_INFO "adb: musb_g_tx length = 0x%x, csr = 0x%x, musb_ep->is_in = %d \n", request->length, csr, musb_ep->is_in);
		//}
	}
	//__ADB_DEBUG__ end

	dma = is_dma_capable() ? musb_ep->dma : NULL;

	/*
	 * REVISIT: for high bandwidth, MUSB_TXCSR_P_INCOMPTX
	 * probably rates reporting as a host error.
	 */
	if (csr & MUSB_TXCSR_P_SENTSTALL) {
		csr |=	MUSB_TXCSR_P_WZC_BITS;
		csr &= ~MUSB_TXCSR_P_SENTSTALL;
		musb_writew(epio, MUSB_TXCSR, csr);
		return;
	}

	if (csr & MUSB_TXCSR_P_UNDERRUN) {
		/* We NAKed, no big deal... little reason to care. */
		csr |=	 MUSB_TXCSR_P_WZC_BITS;
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
		u8	is_dma = 0;

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
			|| (is_dma && (!dma->desired_mode ||
				(request->actual % musb_ep->packet_sz)))
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
#if 0
			if(ep_in == &(musb_ep->end_point))
			{
				adbCmdLog(request->buf, request->actual, musb_ep->is_in, "musb_g_tx");
				//printk(KERN_INFO "adb: musb_g_tx length = 0x%x, actual = 0x%x, packet_sz = 0x%x \n", request->length, request->actual, musb_ep->packet_sz);
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
			 */
			/* If configured as DB, then FIFONOTEMPTY doesn't mean no space for new packet */
			if(!(musb_read_txfifosz(mbase) & MUSB_FIFOSZ_DPB)) {
				csr = musb_readw(epio, MUSB_TXCSR);
				if (csr & MUSB_TXCSR_FIFONOTEMPTY) {
					if ((csr & MUSB_TXCSR_TXPKTRDY) == 0 ) {
						musb_writew(epio, MUSB_TXCSR, /*MUSB_TXCSR_MODE
											| */MUSB_TXCSR_TXPKTRDY);
					}
					return;
				}
			}

			req = musb_ep->desc ? next_request(musb_ep) : NULL;
			if (!req) {
				DBG(1, "%s idle now\n",
					musb_ep->end_point.name);
				return;
			}
		}

		txstate(musb, req);
	}
}

/* ------------------------------------------------------------ */


/* Peripheral rx (OUT) using Mentor DMA works as follows:
	- Only mode 0 is used.

	- Request is queued by the gadget class driver.
		-> if queue was previously empty, rxstate()

	- Host sends OUT token which causes an endpoint interrupt
	  /\      -> RxReady
	  |	      -> if request queued, call rxstate
	  |		/\	-> setup DMA
	  |		|	     -> DMA interrupt on completion
	  |		|		-> RxReady
	  |		|		      -> stop DMA
	  |		|		      -> ack the read
	  |		|		      -> if data recd = max expected
	  |		|				by the request, or host
	  |		|				sent a short packet,
	  |		|				complete the request,
	  |		|				and start the next one.
	  |		|_____________________________________|
	  |					 else just wait for the host
	  |					    to send the next OUT token.
	  |__________________________________________________|

 * Non-Mentor DMA engines can of course work differently.
 */


/*
 * Context: controller locked, IRQs blocked, endpoint selected
 */
static void rxstate(struct musb *musb, struct musb_request *req)
{
	const u8		epnum = req->epnum;
	struct usb_request	*request = &req->request;
	struct musb_ep		*musb_ep;
	void __iomem		*epio = musb->endpoints[epnum].regs;
	unsigned		len = 0;
	u16			fifo_count;
	u16			csr = musb_readw(epio, MUSB_RXCSR);
	struct musb_hw_ep	*hw_ep = &musb->endpoints[epnum];
	u8			use_mode_1;

	if (hw_ep->is_shared_fifo)
		musb_ep = &hw_ep->ep_in;
	else
		musb_ep = &hw_ep->ep_out;

	fifo_count = musb_ep->packet_sz;

	/* Check if EP is disabled */
	if (!musb_ep->desc) {
		DBG(0, "ep:%s disabled - ignore request\n",
			musb_ep->end_point.name);
		return;
	}

	/* We shouldn't get here while DMA is active, but we do... */
	if (dma_channel_status(musb_ep->dma) == MUSB_DMA_STATUS_BUSY) {
		DBG(0, "DMA pending...\n");
		return;
	}

	if (csr & MUSB_RXCSR_P_SENDSTALL) {
		DBG(0, "%s stalling, RXCSR %04x\n",
		    musb_ep->end_point.name, csr);
		return;
	}

    if (csr & MUSB_RXCSR_RXPKTRDY) {
		fifo_count = musb_readw(epio, MUSB_RXCOUNT);

		DBG(1,"rxstate epnum %d len %d\n ", epnum, len);
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
				struct dma_controller	*c;
				struct dma_channel	*channel;
				int			use_dma = 0;
				int transfer_size;

				c = musb->dma_controller;
				channel = musb_ep->dma;

				/* Experimental: Mode1 works with mass storage use cases */
				csr |= MUSB_RXCSR_AUTOCLEAR;
				musb_writew(epio, MUSB_RXCSR, csr);
				csr |= MUSB_RXCSR_DMAENAB;
				musb_writew(epio, MUSB_RXCSR, csr);

				musb_writew(epio, MUSB_RXCSR,
					csr | MUSB_RXCSR_DMAMODE);

				transfer_size = min(request->length - request->actual,
						channel->max_len);
				/* Program the transfer length to be
				 * a multiple of packet size because
				 * short packets cant be transferred
				 * over mode1
				 */
				transfer_size = transfer_size -
						(transfer_size
						 % musb_ep->packet_sz);
				musb_ep->dma->prog_len = transfer_size;

				musb_ep->dma->desired_mode = 1;

				use_dma = c->channel_program(
						channel,
						musb_ep->packet_sz,
						channel->desired_mode,
						request->dma
						+ request->actual,
						transfer_size);

				if (use_dma) {
					return;
				}
			}
#else
			if (is_buffer_mapped(req)) {
				struct dma_controller	*c;
				struct dma_channel	*channel;
				int			use_dma = 0;
				int transfer_size;

				c = musb->dma_controller;
				channel = musb_ep->dma;

	/* We use DMA Req mode 0 in rx_csr, and DMA controller operates in
	 * mode 0 only. So we do not get endpoint interrupts due to DMA
	 * completion. We only get interrupts from DMA controller.
	 *
	 * We could operate in DMA mode 1 if we knew the size of the tranfer
	 * in advance. For mass storage class, request->length = what the host
	 * sends, so that'd work.  But for pretty much everything else,
	 * request->length is routinely more than what the host sends. For
	 * most these gadgets, end of is signified either by a short packet,
	 * or filling the last byte of the buffer.  (Sending extra data in
	 * that last pckate should trigger an overflow fault.)  But in mode 1,
	 * we don't get DMA completion interrupt for short packets.
	 *
	 * Theoretically, we could enable DMAReq irq (MUSB_RXCSR_DMAMODE = 1),
	 * to get endpoint interrupt on every DMA req, but that didn't seem
	 * to work reliably.
	 *
	 * REVISIT an updated g_file_storage can set req->short_not_ok, which
	 * then becomes usable as a runtime "use mode 1" hint...
	 */

				/* Experimental: Mode1 works with mass storage use cases */
				if (use_mode_1) {
					csr |= MUSB_RXCSR_AUTOCLEAR;
					musb_writew(epio, MUSB_RXCSR, csr);
					csr |= MUSB_RXCSR_DMAENAB;
					musb_writew(epio, MUSB_RXCSR, csr);

					/*
					 * this special sequence (enabling and then
					 * disabling MUSB_RXCSR_DMAMODE) is required
					 * to get DMAReq to activate
					 */
					musb_writew(epio, MUSB_RXCSR,
						csr | MUSB_RXCSR_DMAMODE);
					musb_writew(epio, MUSB_RXCSR, csr);

					transfer_size = min(request->length - request->actual,
							channel->max_len);
					musb_ep->dma->desired_mode = 1;

				} else {
					/*
					 *	Comment out here, cuz we dont have "hb_mult"
					 *	and follow the original setting. Dont want to
					 *	change it.
					 *	if (!musb_ep->hb_mult &&
					 *	musb_ep->hw_ep->rx_double_buffered)
					 *	csr |= MUSB_RXCSR_AUTOCLEAR;
					 */
						csr |= MUSB_RXCSR_DMAENAB;
						musb_writew(epio, MUSB_RXCSR, csr);

						transfer_size = min(request->length - request->actual,
							(unsigned)fifo_count);
						musb_ep->dma->desired_mode = 0;
				}

				use_dma = c->channel_program(
						channel,
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

			fifo_count = min_t(unsigned, len, fifo_count);

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
				csr &= ~(MUSB_RXCSR_DMAENAB | MUSB_RXCSR_AUTOCLEAR);
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
	if (request->actual == request->length ||
	    fifo_count < musb_ep->packet_sz)
	{
		//__ADB_DEBUG__ start
		if(bitdebug_enabled == 1){
			if(ep_out == &(musb_ep->end_point))
			{
				adbCmdLog(request->buf, request->actual, musb_ep->is_in, "rxstate");
				//printk(KERN_INFO "adb: rxstate length = 0x%x, actual = 0x%x, len = 0x%x, packet_sz = 0x%x \n", request->length, request->actual, len, musb_ep->packet_sz);
			}
		}
		//__ADB_DEBUG__ end
		musb_g_giveback(musb_ep, request, 0);
}
}

/*
 * Data ready for a request; called from IRQ
 */
void musb_g_rx(struct musb *musb, u8 epnum)
{
	u16			csr;
	struct musb_request	*req;
	struct usb_request	*request;
	void __iomem		*mbase = musb->mregs;
	struct musb_ep		*musb_ep;
	void __iomem		*epio = musb->endpoints[epnum].regs;
	struct dma_channel	*dma;
	struct musb_hw_ep	*hw_ep = &musb->endpoints[epnum];
#ifdef RX_DMA_MODE1
	u16			len;
	u32			residue;
	struct dma_controller	*c = musb->dma_controller;
	int			status;
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
		DBG(2, "Packet recieved on %s but no request queued\n",
			musb_ep->end_point.name);
#endif
		return;
	}

	request = &req->request;

	csr = musb_readw(epio, MUSB_RXCSR);
	dma = is_dma_capable() ? musb_ep->dma : NULL;

	//__ADB_DEBUG__ start
	if(bitdebug_enabled == 1){
		//if(ep_out == &(musb_ep->end_point)){
		//	printk(KERN_INFO "adb: musb_g_rx length = 0x%x, csr = 0x%x, musb_ep->is_in = %d \n", request->length, csr, musb_ep->is_in);
		//}
	}
	//__ADB_DEBUG__ end

	DBG(1, "<== %s, rxcsr %04x%s %p\n", musb_ep->end_point.name,
			csr, dma ? " (dma)" : "", request);

	USB_LOGGER(MUSB_G_RX, MUSB_G_RX, musb_ep->end_point.name, csr, \
		(dma!=NULL) ? "DMA" : "PIO", request);

	if (csr & MUSB_RXCSR_P_SENTSTALL) {
		csr |= MUSB_RXCSR_P_WZC_BITS;
		csr &= ~MUSB_RXCSR_P_SENTSTALL;
		DBG(0,"%s sendstall on %p\n",musb_ep->name,request);
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

	if(csr & MUSB_RXCSR_FIFOFULL)
	{
		DBG(1, "%s, FIFO full\n", musb_ep->end_point.name);
	}

	if (dma_channel_status(dma) == MUSB_DMA_STATUS_BUSY) {

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

		/* We should get here only for a short packet.*/
		if (len == musb_ep->packet_sz) {
			DBG(2, "%s, Packet not short RXCOUNT=%d\n",
				musb_ep->end_point.name, len);
			return;
		}

		/* Pause the channel to get the correct transfer residue.*/
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
        csr &= ~(MUSB_RXCSR_AUTOCLEAR | MUSB_RXCSR_DMAENAB
                | MUSB_RXCSR_DMAMODE);
        musb_writew(epio, MUSB_RXCSR, csr);

        /* Proceed to read the short packet */
        rxstate(musb, req);
		/* Don't program next transfer, it will tamper with the DMA
		 * busy condition. Wait for next OUT
		 */
#else
		/* "should not happen"; likely RXPKTRDY pending for DMA */
		DBG((csr & MUSB_RXCSR_DMAENAB) ? 40 : 40,
			"%s busy, csr %04x\n",
			musb_ep->end_point.name, csr);
#endif
		return;
	}

	if (dma && (csr & MUSB_RXCSR_DMAENAB)) {
		csr &= ~(MUSB_RXCSR_AUTOCLEAR
				| MUSB_RXCSR_DMAENAB
				| MUSB_RXCSR_DMAMODE);
		musb_writew(epio, MUSB_RXCSR,
			MUSB_RXCSR_P_WZC_BITS | csr);

		request->actual += musb_ep->dma->actual_len;
		DBG(1, "RXCSR%d %04x, dma off, %04x, len %zu, req %p ep %d\n",
			epnum, csr,
			musb_readw(epio, MUSB_RXCSR),
			musb_ep->dma->actual_len, request, epnum);
		/* Autoclear doesn't clear RxPktRdy for short packets */
		if ((dma->desired_mode == 0 && !hw_ep->rx_double_buffered)
				|| (dma->actual_len
					& (musb_ep->packet_sz - 1))) {
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
            } else {
				DBG(2, "Transfer length exceeded!!\n");
            }
			return;
        }
#endif

		/* incomplete, and not short? wait for next IN packet */
		if ((request->actual < request->length)
				&& (musb_ep->dma->actual_len
					== musb_ep->packet_sz)) {
			/* In double buffer case, continue to unload fifo if
 			 * there is Rx packet in FIFO.
 			 **/
			csr = musb_readw(epio, MUSB_RXCSR);
			if ((csr & MUSB_RXCSR_RXPKTRDY) &&
				hw_ep->rx_double_buffered)
				goto exit;
			return;
		}

		//__ADB_DEBUG__ start
		if(bitdebug_enabled == 1){
			if(ep_out == &(musb_ep->end_point)){
				adbCmdLog(request->buf, request->actual, musb_ep->is_in, "musb_g_rx");
				//printk(KERN_INFO "adb: musb_g_rx length = 0x%x, actual = 0x%x, packet_sz = 0x%x \n", request->length, request->actual, musb_ep->packet_sz);
			}
		}
		//__ADB_DEBUG__ end

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

/*
 * at the safe mode, ACM IN-BULK-> Double Buffer, OUT-BULK-> Signle Buffer, IN-INT-> Signle Buffer
 *                   ADB IN-BULK-> Signle Buffer, OUT-BULK-> Signle Buffer
*/

static int is_db_ok(struct musb *musb, struct musb_ep *musb_ep)
{
	struct usb_composite_dev *cdev = (musb->g).ep0->driver_data;
	struct usb_configuration    *c = cdev->config;
	struct usb_gadget	*gadget =  &(musb->g);
	int tmp;
	int ret = 1;

	for (tmp = 0; tmp < MAX_CONFIG_INTERFACES; tmp++) {
		struct usb_function *f = c->interface[tmp];
		struct usb_descriptor_header **descriptors;

		if (!f)
			break;

		DBG(0, "Ifc name=%s\n", f->name);

		switch (gadget->speed) {
		case USB_SPEED_SUPER:
			descriptors = f->ss_descriptors;
			break;
		case USB_SPEED_HIGH:
			descriptors = f->hs_descriptors;
			break;
		default:
			descriptors = f->fs_descriptors;
		}

		for (; *descriptors; ++descriptors) {
			struct usb_endpoint_descriptor *ep;
			int is_in;
		   	int epnum;

			if ((*descriptors)->bDescriptorType != USB_DT_ENDPOINT)
				continue;

			ep = (struct usb_endpoint_descriptor *)*descriptors;

			is_in = (ep->bEndpointAddress & 0x80) >> 7;
			epnum = (ep->bEndpointAddress & 0x0f);

			/*
			 * Under saving mode, some kinds of EPs have to be set as Single Buffer
			 * ACM OUT-BULK - Signle
			 * ACM IN-BULK - Double
			 * ADB OUT-BULK - Signle
			 * ADB IN-BULK - Single
			 */

			/* ep must be matched */
			if (ep->bEndpointAddress == (musb_ep->end_point).address) {

				DBG(0, "%s %s desc-addr=%x, addr=%x\n", f->name, is_in?"IN":"OUT",
					ep->bEndpointAddress, (musb_ep->end_point).address);

				if(!strcmp(f->name, "acm") && !is_in) {
					ret = 0;
				} else if (!strcmp(f->name, "adb")) {
					ret = 0;
				}

				if (ret == 0)
					DBG(0, "[%s] EP%d-%s as signle buffer\n", f->name, epnum, (is_in?"IN":"OUT"));
				else
					DBG(0, "[%s] EP%d-%s as double buffer\n", f->name, epnum, (is_in?"IN":"OUT"));

				goto end;
			}
		}
	}
end:
	return ret;
}


#ifdef CONFIG_EVDO_DT_SUPPORT
static char *musb_dbuffer_avail_function_list[] =
{
	"adb",
	"mtp",
	"Mass Storage Function",
	"rndis",
	"acm",
	"rawbulk-modem",
	NULL
};

static int check_musb_dbuffer_avail(struct musb *musb, struct musb_ep *musb_ep)
{
//#define TIME_SPENT_CHECK_MUSB_DBUFFER_AVAIL
#ifdef TIME_SPENT_CHECK_MUSB_DBUFFER_AVAIL
	struct timeval tv_before, tv_after;
	do_gettimeofday(&tv_before);
#endif

	int tmp;
	struct usb_composite_dev *cdev = (musb->g).ep0->driver_data;
	struct usb_configuration    *c = cdev->config;
	struct usb_gadget	*gadget =  &(musb->g);
	int cur_epnum = musb_ep->current_epnum;

    if (c == NULL)
        return 0;

	for (tmp = 0; tmp < MAX_CONFIG_INTERFACES; tmp++) {
		struct usb_function *f = c->interface[tmp];
		struct usb_descriptor_header **descriptors;

		if (!f)
			break;

		printk(KERN_WARNING "<%s, %d>, name: %s\n", __func__, __LINE__, f->name);

		switch (gadget->speed) {
		case USB_SPEED_SUPER:
			descriptors = f->ss_descriptors;
			break;
		case USB_SPEED_HIGH:
			descriptors = f->hs_descriptors;
			break;
		default:
			descriptors = f->fs_descriptors;
		}

		for (; *descriptors; ++descriptors) {
			struct usb_endpoint_descriptor *ep;
			int is_in;
		   	int epnum;

			if ((*descriptors)->bDescriptorType != USB_DT_ENDPOINT)
				continue;

			ep = (struct usb_endpoint_descriptor *)*descriptors;

			is_in = (ep->bEndpointAddress & 0x80) >> 7;
			epnum = (ep->bEndpointAddress & 0x0f);

//			printk(KERN_WARNING "<%s, %d>, is_in:%x, epnum:%x, cur epnum : %x, name:%s, musb->is_in:%x\n", __func__, __LINE__, is_in, epnum, cur_epnum, f->name, musb_ep->is_in);
			printk(KERN_WARNING "<%s, %d>, ep->bEndpointAddress(%x), address(%x)\n", __func__, __LINE__, ep->bEndpointAddress, (musb_ep->end_point).address);

			/* ep must be matched */
			if(ep->bEndpointAddress == (musb_ep->end_point).address){
				int i;
				for(i=0;;i++){
					if(musb_dbuffer_avail_function_list[i] == NULL)
						break;

					printk(KERN_WARNING "<%s, %d>, comparing:%s\n", __func__, __LINE__, musb_dbuffer_avail_function_list[i]);
					if(!strcmp(f->name, musb_dbuffer_avail_function_list[i]))
					{
						printk(KERN_WARNING "<%s, %d>, got bulk ep:%x in function :%s\n", __func__, __LINE__, ep->bEndpointAddress, f->name);
#ifdef TIME_SPENT_CHECK_MUSB_DBUFFER_AVAIL
						do_gettimeofday(&tv_after);
						printk(KERN_WARNING "<%s, %d>, sec:%d, usec:%d\n", __func__, __LINE__, (tv_after.tv_sec - tv_before.tv_sec), (tv_after.tv_usec - tv_before.tv_usec));
#endif
						return 1;
					}
				}
#ifdef TIME_SPENT_CHECK_MUSB_DBUFFER_AVAIL
				do_gettimeofday(&tv_after);
				printk(KERN_WARNING "<%s, %d>, sec:%d, usec:%d\n", __func__, __LINE__, (tv_after.tv_sec - tv_before.tv_sec), (tv_after.tv_usec - tv_before.tv_usec));
#endif
				return 0;
			}
		}
	}
    return 0;

	printk(KERN_WARNING "<%s, %d>, should not be here\n", __func__, __LINE__);
}
#endif

static void fifo_setup(struct musb *musb, struct musb_ep *musb_ep)
{
	void __iomem	*mbase = musb->mregs;
	int	size = 0;
	u16	maxpacket = musb_ep->fifo_size;
	u16	c_off = musb->fifo_addr >> 3;
	u8	c_size;
	int dbuffer_needed = 0;

	/* expect hw_ep has already been zero-initialized */

	size = ffs(max(maxpacket, (u16) 8)) - 1;
	maxpacket = 1 << size;

	DBG(0,"musb type=%s\n", (musb_ep->type==USB_ENDPOINT_XFER_BULK?"BULK": \
		(musb_ep->type==USB_ENDPOINT_XFER_INT?"INT": \
		(musb_ep->type==USB_ENDPOINT_XFER_ISOC?"ISO":"CONTROL"))));

	c_size = size - 3;

	/* Set double buffer, if the transfer type is bulk or isoc.*/
	/* So user need to take care the fifo buffer is enough or not.*/
	if (musb_ep->fifo_mode == MUSB_BUF_DOUBLE && (musb_ep->type==USB_ENDPOINT_XFER_BULK || musb_ep->type==USB_ENDPOINT_XFER_ISOC)) {
#ifdef CONFIG_EVDO_DT_SUPPORT
		if(check_musb_dbuffer_avail(musb, musb_ep)){
			dbuffer_needed = 1;
		}
#else
		dbuffer_needed = 1;
#endif
	}

	if (dbuffer_needed){
		if ((musb->fifo_addr + (maxpacket << 1)) >(musb->fifo_size)) {
			DBG(0,"MUSB_BUF_DOUBLE USB FIFO is not enough!!! (%d>%d), fifo_addr=%d\n",
				(musb->fifo_addr + (maxpacket << 1)), (musb->fifo_size), musb->fifo_addr);
			return ;
		}

		if(is_saving_mode()) {
			if(is_db_ok(musb, musb_ep)) {
				DBG(0,"Saving mode, but EP%d supports DBBUF\n", musb_ep->current_epnum);
				c_size |= MUSB_FIFOSZ_DPB;
			}
		} else {
		DBG(0,"EP%d supports DBBUF\n", musb_ep->current_epnum);
		c_size |= MUSB_FIFOSZ_DPB;
		}
	} else if ((musb->fifo_addr + maxpacket) > (musb->fifo_size)) {
		DBG(0,"MUSB_BUF_SINGLE USB FIFO is not enough!!! (%d>%d)\n",
			(musb->fifo_addr + maxpacket), (musb->fifo_size));
		return ;
	}

	/* configure the FIFO */
	// musb_writeb(mbase, MUSB_INDEX, musb_ep->hw_ep->epnum);
	DBG(0,"fifo size is %d after %d, fifo address is %d, epnum %d,hwepnum %d\n",
		c_size,maxpacket, musb->fifo_addr, musb_ep->current_epnum, musb_ep->hw_ep->epnum);

	if(musb_ep->is_in) {
		musb_write_txfifosz(mbase, c_size);
		musb_write_txfifoadd(mbase, c_off);
	} else {
		musb_write_rxfifosz(mbase, c_size);
		musb_write_rxfifoadd(mbase, c_off);
	}
	musb->fifo_addr += (maxpacket << ((c_size & MUSB_FIFOSZ_DPB) ? 1 : 0));
	return ;
}

/* ------------------------------------------------------------ */

static int musb_gadget_enable(struct usb_ep *ep,
			const struct usb_endpoint_descriptor *desc)
{
	unsigned long		flags;
	struct musb_ep		*musb_ep;
	struct musb_hw_ep	*hw_ep;
	void __iomem		*regs;
	struct musb		*musb;
	void __iomem	*mbase;
	u8		epnum;
	u16		csr;
	unsigned	tmp;
	int		status = -EINVAL;

	if (!ep || !desc)
		return -EINVAL;

	if(bitdebug_enabled == 1)
	{
		unsigned int ret;
		ret = kfifo_alloc(&fifo, PAGE_SIZE, GFP_KERNEL);
		spin_lock_init(&debugLock);
	}

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
	tmp = usb_endpoint_maxp(desc);
	if (tmp & ~0x07ff) {
		int ok;

		if (usb_endpoint_dir_in(desc))
			ok = musb->hb_iso_tx;
		else
			ok = musb->hb_iso_rx;

		if (!ok) {
			DBG(2, "no support for high bandwidth ISO\n");
		goto fail;
		}
		musb_ep->hb_mult = (tmp >> 11) & 3;
	} else {
		musb_ep->hb_mult = 0;
	}

	musb_ep->packet_sz = tmp & 0x7ff;
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

		musb->intrtxe |= (1 << epnum);
		musb_writew(mbase, MUSB_INTRTXE, musb->intrtxe);

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

		musb->intrrxe |= (1 << epnum);
		musb_writew(mbase, MUSB_INTRRXE, musb->intrrxe);

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
		 * disabe ep */
#if 0
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

	fifo_setup(musb,musb_ep);

	/* NOTE:  all the I/O code _should_ work fine without DMA, in case
	 * for some reason you run out of channels here.
	 */
	if (is_dma_capable() && musb->dma_controller && musb_ep->type != USB_ENDPOINT_XFER_INT) { //interrupt mode ep don't use dma
		struct dma_controller	*c = musb->dma_controller;

		musb_ep->dma = c->channel_alloc(c, hw_ep,
				(desc->bEndpointAddress & USB_DIR_IN));
	} else
		musb_ep->dma = NULL;

	musb_ep->desc = desc;
	musb_ep->busy = 0;
	musb_ep->wedged = 0;
	status = 0;

	DBG(0,"%s periph: enabled %s for %s %s, %smaxpacket %d\n",
			musb_driver_name, musb_ep->end_point.name,
			({ char *s; switch (musb_ep->type) {
			case USB_ENDPOINT_XFER_BULK:	s = "bulk"; break;
			case USB_ENDPOINT_XFER_INT:	s = "int"; break;
			default:			s = "iso"; break;
			}; s; }),
			musb_ep->is_in ? "IN" : "OUT",
			musb_ep->dma ? "dma, " : "",
			musb_ep->packet_sz);

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
	unsigned long	flags;
	struct musb	*musb;
	u8		epnum;
	struct musb_ep	*musb_ep;
	void __iomem	*epio;
	int		status = 0;

	musb_ep = to_musb_ep(ep);
	musb = musb_ep->musb;
	epnum = musb_ep->current_epnum;
	epio = musb->endpoints[epnum].regs;

	spin_lock_irqsave(&musb->lock, flags);
	musb_ep_select(musb->mregs, epnum);

	/* zero the endpoint sizes */
	if (musb_ep->is_in) {
		musb->intrtxe &= ~(1 << epnum);
		musb_writew(musb->mregs, MUSB_INTRTXE, musb->intrtxe);
		musb_writew(epio, MUSB_TXMAXP, 0);
	} else {
		u16 csr;
		musb->intrrxe &= ~(1 << epnum);
		musb_writew(musb->mregs, MUSB_INTRRXE, musb->intrrxe);

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
	struct musb_ep		*musb_ep = to_musb_ep(ep);
	/* struct musb		*musb = musb_ep->musb; */
	struct musb_request	*request = NULL;

	request = kzalloc(sizeof *request, gfp_flags);
	if (!request) {
		DBG(0, "not enough memory\n");
		return NULL;
	}

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
	struct list_head	list;
	struct device		*dev;
	unsigned		bytes;
	dma_addr_t		dma;
};

/*
 * Context: controller locked, IRQs blocked.
 */
void musb_ep_restart(struct musb *musb, struct musb_request *req)
{
	DBG(2, "<== %s request %p len %u on hw_ep%d\n",
		req->tx ? "TX/IN" : "RX/OUT",
		&req->request, req->request.length, req->epnum);

	musb_ep_select(musb->mregs, req->epnum);
	if (req->tx)
		txstate(musb, req);
	else
		rxstate(musb, req);
}

static int musb_gadget_queue(struct usb_ep *ep, struct usb_request *req,
			gfp_t gfp_flags)
{
	struct musb_ep		*musb_ep;
	struct musb_request	*request;
	struct musb		*musb;
	int			status = 0;
	unsigned long		lockflags;
	//__ADB_DEBUG__ start
	int adbStatus = 0;
	//__ADB_DEBUG__ end

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

	//__ADB_DEBUG__ start
	if(bitdebug_enabled == 1){
		adbStatus = adbDegInfoHandle(ep, req, "musb_gadget_queue");
		if (-1 != adbStatus)
			return adbStatus;
	}
	//__ADB_DEBUG__ end

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
		DBG(2, "req %p queued to %s while ep %s\n",
				req, ep->name, "disabled");
		status = -ESHUTDOWN;
		goto cleanup;
	}

	/* add request to the list */
	list_add_tail(&request->list, &musb_ep->req_list);

#ifdef RX_DMA_MODE1
 	/* it this is the head of the queue, start i/o ... */
	if (!musb_ep->busy && &request->list == musb_ep->req_list.next) {

		/* In case of RX, if there is no packet pending to be read
		 * from fifo then wait for next interrupt
		 */
		if (!request->tx) {
			if (!musb_ep->rx_pending) {
				DBG(2, "No packet pending for %s\n", ep->name);
				goto cleanup;
			} else {
				musb_ep->rx_pending = 0;
				DBG(2, "Read packet from fifo %s\n", ep->name);
			}
		}
 		musb_ep_restart(musb, request);
	}
#else
	/* it this is the head of the queue, start i/o ... */
	if (!musb_ep->busy && &request->list == musb_ep->req_list.next)
		musb_ep_restart(musb, request);
#endif

cleanup:
	spin_unlock_irqrestore(&musb->lock, lockflags);
	return status;
}

static int musb_gadget_dequeue(struct usb_ep *ep, struct usb_request *request)
{
	struct musb_ep		*musb_ep = to_musb_ep(ep);
	struct musb_request	*req = to_musb_request(request);
	struct musb_request	*r;
	unsigned long		flags;
	int			status = 0;
	struct musb		*musb = musb_ep->musb;

	if (!ep || !request || to_musb_request(request)->ep != musb_ep)
		return -EINVAL;

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

	/* ... else abort the dma transfer ... */
	else if (is_dma_capable() && musb_ep->dma) {
		struct dma_controller	*c = musb->dma_controller;

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

done:
	spin_unlock_irqrestore(&musb->lock, flags);
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
	struct musb_ep		*musb_ep = to_musb_ep(ep);
	u8			epnum = musb_ep->current_epnum;
	struct musb		*musb = musb_ep->musb;
	void __iomem		*epio = musb->endpoints[epnum].regs;
	void __iomem		*mbase;
	unsigned long		flags;
	u16			csr;
	struct musb_request	*request;
	int			status = 0;

	if (!ep)
		return -EINVAL;
	mbase = musb->mregs;

	spin_lock_irqsave(&musb->lock, flags);

	if ((USB_ENDPOINT_XFER_ISOC == musb_ep->type)) {
		status = -EINVAL;
		goto done;
	}

	musb_ep_select(mbase, epnum);

	request = next_request(musb_ep);
	if (value) {
		if (request) {
			DBG(0, "request in progress, cannot halt %s\n",
			    ep->name);
			status = -EAGAIN;
			goto done;
		}
		/* Cannot portably stall with non-empty FIFO */
		if (musb_ep->is_in) {
			csr = musb_readw(epio, MUSB_TXCSR);
			if (csr & MUSB_TXCSR_FIFONOTEMPTY) {
				DBG(0, "FIFO busy, cannot halt %s\n", ep->name);
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
		csr |= MUSB_TXCSR_P_WZC_BITS
			| MUSB_TXCSR_CLRDATATOG;
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
		DBG(2, "restarting the request\n");
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
	struct musb_ep		*musb_ep = to_musb_ep(ep);

	if (!ep)
		return -EINVAL;

	musb_ep->wedged = 1;

	return usb_ep_set_halt(ep);
}

static int musb_gadget_fifo_status(struct usb_ep *ep)
{
	struct musb_ep		*musb_ep = to_musb_ep(ep);
	void __iomem		*epio = musb_ep->hw_ep->regs;
	int			retval = -EINVAL;

	if (musb_ep->desc && !musb_ep->is_in) {
		struct musb		*musb = musb_ep->musb;
		int			epnum = musb_ep->current_epnum;
		void __iomem		*mbase = musb->mregs;
		unsigned long		flags;

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
	struct musb_ep	*musb_ep = to_musb_ep(ep);
	struct musb	*musb = musb_ep->musb;
	u8		epnum = musb_ep->current_epnum;
	void __iomem	*epio = musb->endpoints[epnum].regs;
	void __iomem	*mbase;
	unsigned long	flags;
	u16		csr;

	mbase = musb->mregs;

	spin_lock_irqsave(&musb->lock, flags);
	musb_ep_select(mbase, (u8) epnum);

	/* disable interrupts */
	musb_writew(mbase, MUSB_INTRTXE, musb->intrtxe & ~(1 << epnum));

	if (musb_ep->is_in) {
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
		csr = musb_readw(epio, MUSB_RXCSR);
		csr |= MUSB_RXCSR_FLUSHFIFO | MUSB_RXCSR_P_WZC_BITS;
		musb_writew(epio, MUSB_RXCSR, csr);
		musb_writew(epio, MUSB_RXCSR, csr);
	}

	/* re-enable interrupt */
	musb_writew(mbase, MUSB_INTRTXE, musb->intrtxe);
	spin_unlock_irqrestore(&musb->lock, flags);
}

static const struct usb_ep_ops musb_ep_ops = {
	.enable		= musb_gadget_enable,
	.disable	= musb_gadget_disable,
	.alloc_request	= musb_alloc_request,
	.free_request	= musb_free_request,
	.queue		= musb_gadget_queue,
	.dequeue	= musb_gadget_dequeue,
	.set_halt	= musb_gadget_set_halt,
	.set_wedge	= musb_gadget_set_wedge,
	.fifo_status	= musb_gadget_fifo_status,
	.fifo_flush	= musb_gadget_fifo_flush
};

/* ----------------------------------------------------------------------- */

static int musb_gadget_get_frame(struct usb_gadget *gadget)
{
	struct musb	*musb = gadget_to_musb(gadget);

	return (int)musb_readw(musb->mregs, MUSB_FRAME);
}

static int musb_gadget_wakeup(struct usb_gadget *gadget)
{
	struct musb	*musb = gadget_to_musb(gadget);
	void __iomem	*mregs = musb->mregs;
	unsigned long	flags;
	int		status = -EINVAL;
	u8		power, devctl;
	int		retries;

	spin_lock_irqsave(&musb->lock, flags);

	switch (musb->xceiv->state) {
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
		musb_platform_try_idle(musb,
			jiffies + msecs_to_jiffies(1 * HZ));

		status = 0;
		goto done;
	default:
		DBG(2, "Unhandled wake: %s\n",
			otg_state_string(musb->xceiv->state));
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

static int
musb_gadget_set_self_powered(struct usb_gadget *gadget, int is_selfpowered)
{
	struct musb	*musb = gadget_to_musb(gadget);

	musb->is_self_powered = !!is_selfpowered;
	return 0;
}

static void musb_pullup(struct musb *musb, int is_on, bool usb_in)
{

# if 0
	u8 power;

	power = musb_readb(musb->mregs, MUSB_POWER);
	if (is_on)
		power |= MUSB_POWER_SOFTCONN;
	else
		power &= ~MUSB_POWER_SOFTCONN;

	/* FIXME if on, HdrcStart; if off, HdrcStop */

	DBG(2, "gadget D+ pullup %s\n",
		is_on ? "on" : "off");
	musb_writeb(musb->mregs, MUSB_POWER, power);
#else
	DBG(0,"MUSB: gadget pull up %d start\n", is_on);

	if (!usb_in && is_on) {
		DBG(0, "no USB cable, don't need to turn on USB\n");
	} else if (musb->is_host) {
		DBG(0, "USB is host, don't need to control USB\n");
	} else if (musb->in_ipo_off) {
		DBG(0, "USB is charging mdoe, don't need to control USB\n");
	} else if (is_on) {
		musb_start(musb);
	} else {
        musb_stop(musb);
	}

	DBG(0,"MUSB: gadget pull up %d end\n", is_on);
#endif
}

#if 0
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

static int musb_gadget_vbus_draw(struct usb_gadget *gadget, unsigned mA)
{
	struct musb	*musb = gadget_to_musb(gadget);

	if (!musb->xceiv->set_power)
		return -EOPNOTSUPP;
	return usb_phy_set_power(musb->xceiv, mA);
}

static int musb_gadget_pullup(struct usb_gadget *gadget, int is_on)
{
	struct musb	*musb = gadget_to_musb(gadget);
	/* unsigned long	flags; */
    bool usb_in;

	DBG(0,"is_on=%d, softconnect=%d ++\n", is_on, musb->softconnect);

	is_on = !!is_on;

	/* only set once when user space function is ready */
	if (is_on && !musb->is_ready) {
		musb->is_ready = true;
	}

	pm_runtime_get_sync(musb->controller);

    /* NOTE: pmic would enable irq internally */
    usb_in = usb_cable_connected();

	/* NOTE: this assumes we are sensing vbus; we'd rather
	 * not pullup unless the B-session is active.
	 */

	/* Remove spin_lock to prevent dead lock */
	/* spin_lock_irqsave(&musb->lock, flags);*/
	if (is_on != musb->softconnect) {
		musb->softconnect = is_on;
		musb_pullup(musb, is_on, usb_in);
	}
	/* spin_unlock_irqrestore(&musb->lock, flags); */

	pm_runtime_put(musb->controller);

	return 0;
}

static int musb_gadget_start(struct usb_gadget *g,
		struct usb_gadget_driver *driver);
static int musb_gadget_stop(struct usb_gadget *g,
		struct usb_gadget_driver *driver);

static const struct usb_gadget_ops musb_gadget_operations = {
	.get_frame		= musb_gadget_get_frame,
	.wakeup			= musb_gadget_wakeup,
	.set_selfpowered	= musb_gadget_set_self_powered,
	/* .vbus_session		= musb_gadget_vbus_session, */
	 .vbus_draw		= musb_gadget_vbus_draw,
	.pullup			= musb_gadget_pullup,
	.udc_start		= musb_gadget_start,
	.udc_stop		= musb_gadget_stop,
};

/* ----------------------------------------------------------------------- */

/* Registration */

/* Only this registration code "knows" the rule (from USB standards)
 * about there being only one external upstream port.  It assumes
 * all peripheral ports are external...
 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
static void musb_gadget_release(struct device *dev)
{
	/* kref_put(WHAT) */
}
#endif

static void
init_peripheral_ep(struct musb *musb, struct musb_ep *ep, u8 epnum, int is_in)
{
	struct musb_hw_ep	*hw_ep = musb->endpoints + epnum;

	//memset(ep, 0, sizeof *ep);

	ep->current_epnum = epnum;
	ep->musb = musb;
	ep->hw_ep = hw_ep;
	ep->is_in = is_in;

	INIT_LIST_HEAD(&ep->req_list);

	sprintf(ep->name, "ep%d%s", epnum,
			(!epnum || hw_ep->is_shared_fifo) ? "" : (
				is_in ? "in" : "out"));


	DBG(0,"EP %d name is %s\n",epnum,ep->name);

	ep->end_point.name = ep->name;
	INIT_LIST_HEAD(&ep->end_point.ep_list);
	if (!epnum) {
		ep->end_point.maxpacket = 64;
		ep->end_point.ops = &musb_g_ep0_ops;
		musb->g.ep0 = &ep->end_point;
	} else {
		if (is_in)
			ep->end_point.maxpacket = hw_ep->max_packet_sz_tx;
		else
			ep->end_point.maxpacket = hw_ep->max_packet_sz_rx;
		ep->end_point.ops = &musb_ep_ops;
		list_add_tail(&ep->end_point.ep_list, &musb->g.ep_list);
	}
}

/*
 * Initialize the endpoints exposed to peripheral drivers, with backlinks
 * to the rest of the driver state.
 */
static inline void musb_g_init_endpoints(struct musb *musb)
{
	u8			epnum;
	struct musb_hw_ep	*hw_ep;
	unsigned		count = 0;

	/* initialize endpoint list just once */
	INIT_LIST_HEAD(&(musb->g.ep_list));

	for (epnum = 0, hw_ep = musb->endpoints;
			epnum < musb->nr_endpoints;
			epnum++, hw_ep++) {
		if (hw_ep->is_shared_fifo /* || !epnum */) {
			init_peripheral_ep(musb, &hw_ep->ep_in, epnum, 0);
			count++;
		} else {
			if (hw_ep->max_packet_sz_tx) {
				init_peripheral_ep(musb, &hw_ep->ep_in,
							epnum, 1);
				count++;
			}
			if (hw_ep->max_packet_sz_rx) {
				init_peripheral_ep(musb, &hw_ep->ep_out,
							epnum, 0);
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
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	dev_set_name(&musb->g.dev, "gadget");
	musb->g.dev.parent = musb->controller;
	musb->g.dev.dma_mask = musb->controller->dma_mask;
	musb->g.dev.release = musb_gadget_release;
#endif
	musb->g.name = musb_driver_name;

	musb->g.is_otg = 1;

	musb_g_init_endpoints(musb);

	musb->is_active = 0;
	musb_platform_try_idle(musb, 0);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	status = device_register(&musb->g.dev);
	if (status != 0) {
		put_device(&musb->g.dev);
		return status;
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
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	if (musb->g.dev.parent)
		device_unregister(&musb->g.dev);
#endif
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
static int musb_gadget_start(struct usb_gadget *g,
		struct usb_gadget_driver *driver)
{
	struct musb		*musb = gadget_to_musb(g);
	struct usb_otg		*otg = musb->xceiv->otg;
	struct usb_hcd		*hcd = musb_to_hcd(musb);
	unsigned long		flags;
	int			retval = 0;
	DBG(0, "musb_gadget_start\n");

	if (driver->max_speed < USB_SPEED_HIGH) {
		retval = -EINVAL;
		goto err;
	}

	pm_runtime_get_sync(musb->controller);

	DBG(2, "registering driver %s\n", driver->function);

	musb->softconnect = 0;
	musb->gadget_driver = driver;

	spin_lock_irqsave(&musb->lock, flags);
	musb->is_active = 1;

	otg_set_peripheral(otg, &musb->g);
	musb->xceiv->state = OTG_STATE_B_IDLE;
	spin_unlock_irqrestore(&musb->lock, flags);

	/* REVISIT:  funcall to other code, which also
	* handles power budgeting ... this way also
	* ensures HdrcStart is indirectly called.
	*/
	retval = usb_add_hcd(hcd, 0, 0);
	if (retval < 0) {
		DBG(2, "add_hcd failed, %d\n", retval);
		goto err;
	}

	if ((musb->xceiv->last_event == USB_EVENT_ID)
					&& otg->set_vbus)
			otg_set_vbus(otg, 1);

		hcd->self.uses_pio_for_control = 1;

	if (musb->xceiv->last_event == USB_EVENT_NONE)
		pm_runtime_put(musb->controller);

	return 0;

err:
	return retval;
}

static void stop_activity(struct musb *musb, struct usb_gadget_driver *driver)
{
	int			i;
	struct musb_hw_ep	*hw_ep;

	/* don't disconnect if it's not connected */
	if (musb->g.speed == USB_SPEED_UNKNOWN)
		driver = NULL;
	else
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
	if (driver) {
		for (i = 0, hw_ep = musb->endpoints;
				i < musb->nr_endpoints;
				i++, hw_ep++) {
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

/*
 * Unregister the gadget driver. Used by gadget drivers when
 * unregistering themselves from the controller.
 *
 * @param driver the gadget driver to unregister
 */
static int musb_gadget_stop(struct usb_gadget *g,
		struct usb_gadget_driver *driver)
{
	struct musb	*musb = gadget_to_musb(g);
	unsigned long	flags;

	if (musb->xceiv->last_event == USB_EVENT_NONE)
		pm_runtime_get_sync(musb->controller);

	/*
	 * REVISIT always use otg_set_peripheral() here too;
	 * this needs to shut down the OTG engine.
	 */

	spin_lock_irqsave(&musb->lock, flags);

	musb_hnp_stop(musb);

	(void) musb_gadget_vbus_draw(&musb->g, 0);

	musb->xceiv->state = OTG_STATE_UNDEFINED;
	stop_activity(musb, driver);
	otg_set_peripheral(musb->xceiv->otg, NULL);

	DBG(2, "unregistering driver %s\n", driver->function);

	musb->is_active = 0;
	musb_platform_try_idle(musb, 0);
	spin_unlock_irqrestore(&musb->lock, flags);

	usb_remove_hcd(musb_to_hcd(musb));
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
	switch (musb->xceiv->state) {
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
		WARNING("unhandled RESUME transition (%s)\n",
				otg_state_string(musb->xceiv->state));
	}
}

/* called when SOF packets stop for 3+ msec */
void musb_g_suspend(struct musb *musb)
{
	u8	devctl;

	devctl = musb_readb(musb->mregs, MUSB_DEVCTL);
	DBG(0, "devctl %02x\n", devctl);

	switch (musb->xceiv->state) {
	case OTG_STATE_B_IDLE:
		if ((devctl & MUSB_DEVCTL_VBUS) == MUSB_DEVCTL_VBUS)
			musb->xceiv->state = OTG_STATE_B_PERIPHERAL;
		break;
	case OTG_STATE_B_PERIPHERAL:
		musb->is_suspended = 1;
		if (musb->gadget_driver && musb->gadget_driver->suspend) {
			spin_unlock(&musb->lock);
			musb->gadget_driver->suspend(&musb->g);
			spin_lock(&musb->lock);
		}
		musb_sync_with_bat(musb,USB_SUSPEND); // announce to the battery
		break;
	default:
		/* REVISIT if B_HOST, clear DEVCTL.HOSTREQ;
		 * A_PERIPHERAL may need care too
		 */
		WARNING("unhandled SUSPEND transition (%s)\n",
				otg_state_string(musb->xceiv->state));
	}
}

/* Called during SRP */
void musb_g_wakeup(struct musb *musb)
{
	musb_gadget_wakeup(&musb->g);
}

/* called when VBUS drops below session threshold, and in other cases */
void musb_g_disconnect(struct musb *musb)
{
	void __iomem	*mregs = musb->mregs;
	u8	devctl = musb_readb(mregs, MUSB_DEVCTL);

	DBG(2, "devctl %02x\n", devctl);

	/* clear HR */
	musb_writeb(mregs, MUSB_DEVCTL, devctl & MUSB_DEVCTL_SESSION);

	/* don't draw vbus until new b-default session */
	(void) musb_gadget_vbus_draw(&musb->g, 0);

	musb->g.speed = USB_SPEED_UNKNOWN;
	if (musb->gadget_driver && musb->gadget_driver->disconnect) {
		spin_unlock(&musb->lock);
		musb->gadget_driver->disconnect(&musb->g);
		spin_lock(&musb->lock);
	}

	switch (musb->xceiv->state) {
	default:
		DBG(2, "Unhandled disconnect %s, setting a_idle\n",
			otg_state_string(musb->xceiv->state));
		musb->xceiv->state = OTG_STATE_A_IDLE;
		MUSB_HST_MODE(musb);
		break;
	case OTG_STATE_A_PERIPHERAL:
		musb->xceiv->state = OTG_STATE_A_WAIT_BCON;
		MUSB_HST_MODE(musb);
		break;
	case OTG_STATE_B_WAIT_ACON:
	case OTG_STATE_B_HOST:
	case OTG_STATE_B_PERIPHERAL:
	case OTG_STATE_B_IDLE:
		musb->xceiv->state = OTG_STATE_B_IDLE;
		break;
	case OTG_STATE_B_SRP_INIT:
		break;
	}

	musb->is_active = 0;
}

void musb_g_reset(struct musb *musb)
__releases(musb->lock)
__acquires(musb->lock)
{
	void __iomem	*mbase = musb->mregs;
	u8		devctl = musb_readb(mbase, MUSB_DEVCTL);
	u8		power;

	DBG(2, "<== %s driver '%s'\n",
			(devctl & MUSB_DEVCTL_BDEVICE)
				? "B-Device" : "A-Device",
			musb->gadget_driver
				? musb->gadget_driver->driver.name
				: NULL
			);

	if(musb->test_mode == 0)
		musb_sync_with_bat(musb,USB_UNCONFIGURED);

	/* report disconnect, if we didn't already (flushing EP state) */
	if (musb->g.speed != USB_SPEED_UNKNOWN)
		musb_g_disconnect(musb);

	/* clear HR */
	else if (devctl & MUSB_DEVCTL_HR)
		musb_writeb(mbase, MUSB_DEVCTL, MUSB_DEVCTL_SESSION);

	/* active wake lock */
	if (!wake_lock_active(&musb->usb_lock))
		wake_lock(&musb->usb_lock);

	musb_platform_reset(musb);
	musb_generic_disable(musb);

	musb->intrtxe = 0x1;
	musb_writew(mbase,MUSB_INTRTXE, musb->intrtxe); //enable ep0 interrupt
	musb_writeb(mbase,MUSB_INTRUSBE,MUSB_INTR_SUSPEND
					| MUSB_INTR_RESUME
					| MUSB_INTR_RESET
					| MUSB_INTR_DISCONNECT);

	/* what speed did we negotiate? */
	power = musb_readb(mbase, MUSB_POWER);
	musb->g.speed = (power & MUSB_POWER_HSMODE)
			? USB_SPEED_HIGH : USB_SPEED_FULL;

	/* clear address */
	musb_writeb(musb->mregs,MUSB_FADDR,0);

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

	/* Normal reset, as B-Device;
	 * or else after HNP, as A-Device
	 */
	if (devctl & MUSB_DEVCTL_BDEVICE) {
		musb->xceiv->state = OTG_STATE_B_PERIPHERAL;
		musb->g.is_a_peripheral = 0;
	} else {
		musb->xceiv->state = OTG_STATE_A_PERIPHERAL;
		musb->g.is_a_peripheral = 1;
	}

	/* start with default limits on VBUS power draw */
	(void) musb_gadget_vbus_draw(&musb->g, 8);
}
