/*
 * MUSB OTG driver - support for Mentor's DMA controller
 *
 * Copyright 2005 Mentor Graphics Corporation
 * Copyright (C) 2005-2007 by Texas Instruments
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
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include "musbfsh_core.h"
#include "musbfsh_host.h"
#include "musbfsh_dma.h"
#include "musbfsh_hsdma.h"
#include "usb.h"			

#ifdef CONFIG_MTK_ICUSB_SUPPORT
extern struct my_attr skip_mac_init_attr;
#endif	

static int dma_controller_start(struct dma_controller *c)
{
	INFO("++\n");
	/* nothing to do */
	return 0;
}

static void dma_channel_release(struct dma_channel *channel);

static int dma_controller_stop(struct dma_controller *c)
{
	struct musbfsh_dma_controller *controller = container_of(c,
			struct musbfsh_dma_controller, controller);
	struct musbfsh *musbfsh = controller->private_data;
	struct dma_channel *channel;
	u8 bit;
	
	INFO("++\n");
	if (controller->used_channels != 0) {
		dev_err(musbfsh->controller,
			"Stopping DMA controller while channel active\n");

		for (bit = 0; bit < MUSBFSH_HSDMA_CHANNELS; bit++) {
			if (controller->used_channels & (1 << bit)) {
				channel = &controller->channel[bit].channel;
				dma_channel_release(channel);

				if (!controller->used_channels)
					break;
			}
		}
	}

	return 0;
}

static struct dma_channel *dma_channel_allocate(struct dma_controller *c,
				struct musbfsh_hw_ep *hw_ep, u8 transmit)
{
	struct musbfsh_dma_controller *controller = container_of(c,
			struct musbfsh_dma_controller, controller);
	struct musbfsh_dma_channel *musbfsh_channel = NULL;
	struct dma_channel *channel = NULL;
	u8 bit;
	
	INFO("epnum=%d\n", hw_ep->epnum);
	for (bit = 0; bit < MUSBFSH_HSDMA_CHANNELS; bit++) {
		if (!(controller->used_channels & (1 << bit))) {
			controller->used_channels |= (1 << bit);
			musbfsh_channel = &(controller->channel[bit]);
			musbfsh_channel->controller = controller;
			musbfsh_channel->idx = bit;
			musbfsh_channel->epnum = hw_ep->epnum;
			musbfsh_channel->transmit = transmit;
			channel = &(musbfsh_channel->channel);
			channel->private_data = musbfsh_channel;
			channel->status = MUSBFSH_DMA_STATUS_FREE;
			channel->max_len = 0x10000;
			/* Tx => mode 1; Rx => mode 0 */
			channel->desired_mode = transmit;
			//channel->desired_mode = 0; //wz:set Tx and Rx to mode 0
			channel->actual_len = 0;
			break;
		}
	}
	if(musbfsh_channel)
		INFO("idx=%d\n", musbfsh_channel->idx);
	return channel;
}

static void dma_channel_release(struct dma_channel *channel)
{
	struct musbfsh_dma_channel *musbfsh_channel = channel->private_data;
	
	INFO("idx=%d\n", musbfsh_channel->idx);
	channel->actual_len = 0;
	musbfsh_channel->start_addr = 0;
	musbfsh_channel->len = 0;

	musbfsh_channel->controller->used_channels &=
		~(1 << musbfsh_channel->idx);

	channel->status = MUSBFSH_DMA_STATUS_UNKNOWN;
}

static void configure_channel(struct dma_channel *channel,
				u16 packet_sz, u8 mode,
				dma_addr_t dma_addr, u32 len)
{
	struct musbfsh_dma_channel *musbfsh_channel = channel->private_data;
	struct musbfsh_dma_controller *controller = musbfsh_channel->controller;
	//struct musbfs *musb = controller->private_data;
	void __iomem *mbase = controller->base;
	u8 bchannel = musbfsh_channel->idx;
	u16 csr = 0;
	
	INFO("idx=%d\n", musbfsh_channel->idx);
	INFO("%p, pkt_sz %d, addr 0x%x, len %d, mode %d\n",
			channel, packet_sz, (unsigned int)dma_addr, len, mode);

	if (mode) { //mode 1,multi-packet
		csr |= 1 << MUSBFSH_HSDMA_MODE1_SHIFT;
		BUG_ON(len < packet_sz);
	}
	csr |= MUSBFSH_HSDMA_BURSTMODE_INCR16
				<< MUSBFSH_HSDMA_BURSTMODE_SHIFT;

	csr |= (musbfsh_channel->epnum << MUSBFSH_HSDMA_ENDPOINT_SHIFT)
		| (1 << MUSBFSH_HSDMA_ENABLE_SHIFT)
		| (1 << MUSBFSH_HSDMA_IRQENABLE_SHIFT)
		| (musbfsh_channel->transmit
				? (1 << MUSBFSH_HSDMA_TRANSMIT_SHIFT)
				: 0);

	/* address/count */
	musbfsh_write_hsdma_addr(mbase, bchannel, dma_addr);
	musbfsh_write_hsdma_count(mbase, bchannel, len);

	/* control (this should start things) */
	musbfsh_writew(mbase,
		MUSBFSH_HSDMA_CHANNEL_OFFSET(bchannel, MUSBFSH_HSDMA_CONTROL),
		csr);
}

static int dma_channel_program(struct dma_channel *channel,
				u16 packet_sz, u8 mode,
				dma_addr_t dma_addr, u32 len)
{
	struct musbfsh_dma_channel *musbfsh_channel = channel->private_data;
	//struct musbfsh_dma_controller *controller = musbfsh_channel->controller;
	//struct musfsh *musbfsh = controller->private_data;
	
	INFO("ep%d-%s pkt_sz %d, dma_addr 0x%x length %d, mode %d\n",
		musbfsh_channel->epnum,
		musbfsh_channel->transmit ? "Tx" : "Rx",
		packet_sz, (unsigned int)dma_addr, len, mode);

	BUG_ON(channel->status == MUSBFSH_DMA_STATUS_UNKNOWN ||
		channel->status == MUSBFSH_DMA_STATUS_BUSY);

	channel->actual_len = 0;
	musbfsh_channel->start_addr = dma_addr;
	musbfsh_channel->len = len;
	musbfsh_channel->max_packet_sz = packet_sz;
	channel->status = MUSBFSH_DMA_STATUS_BUSY;

	configure_channel(channel, packet_sz, mode, dma_addr, len);

	return true;
}

static int dma_channel_abort(struct dma_channel *channel)
{
	struct musbfsh_dma_channel *musbfsh_channel = channel->private_data;
	void __iomem *mbase = musbfsh_channel->controller->base;

	u8 bchannel = musbfsh_channel->idx;
	int offset;
	u16 csr;
	
	INFO("dma_channel_abort++,idx=%d\r\n", musbfsh_channel->idx);

	if (channel->status == MUSBFSH_DMA_STATUS_BUSY) {
		if (musbfsh_channel->transmit) {
			offset = MUSBFSH_EP_OFFSET(musbfsh_channel->epnum,
						MUSBFSH_TXCSR);

			/*
			 * The programming guide says that we must clear
			 * the DMAENA bit before the DMAMODE bit...
			 */
			csr = musbfsh_readw(mbase, offset);
			csr &= ~(MUSBFSH_TXCSR_AUTOSET | MUSBFSH_TXCSR_DMAENAB);
			musbfsh_writew(mbase, offset, csr);
			csr &= ~MUSBFSH_TXCSR_DMAMODE;
			musbfsh_writew(mbase, offset, csr);
		} else {
			offset = MUSBFSH_EP_OFFSET(musbfsh_channel->epnum,
						MUSBFSH_RXCSR);

			csr = musbfsh_readw(mbase, offset);
			csr &= ~(MUSBFSH_RXCSR_AUTOCLEAR |
				 MUSBFSH_RXCSR_DMAENAB |
				 MUSBFSH_RXCSR_DMAMODE);
			musbfsh_writew(mbase, offset, csr);
		}

		musbfsh_writew(mbase,
			MUSBFSH_HSDMA_CHANNEL_OFFSET(bchannel, MUSBFSH_HSDMA_CONTROL),
			0);
		musbfsh_write_hsdma_addr(mbase, bchannel, 0);
		musbfsh_write_hsdma_count(mbase, bchannel, 0);
		channel->status = MUSBFSH_DMA_STATUS_FREE;
	}

	return 0;
}

irqreturn_t musbfsh_dma_controller_irq(int irq, void *private_data)
{
	struct musbfsh_dma_controller *controller = private_data;
	struct musbfsh *musbfsh = controller->private_data;
	struct musbfsh_dma_channel *musbfsh_channel;
	struct dma_channel *channel;

	void __iomem *mbase = controller->base;

	irqreturn_t retval = IRQ_NONE;

	// unsigned long flags;

	u8 bchannel;
	u8 int_hsdma;

	u32 addr, count;
	u16 csr;
	
	INFO("++\n");
	// spin_lock_irqsave(&musbfsh->lock, flags); // removed due to now this function is called inside generic_interrupt

	int_hsdma = musbfsh->int_dma;

	if (!int_hsdma) {//should not to run here!
		WARNING("spurious DMA irq\n");

		for (bchannel = 0; bchannel < MUSBFSH_HSDMA_CHANNELS; bchannel++) {
			musbfsh_channel = (struct musbfsh_dma_channel *)
					&(controller->channel[bchannel]);
			channel = &musbfsh_channel->channel;
			if (channel->status == MUSBFSH_DMA_STATUS_BUSY) {
				count = musbfsh_read_hsdma_count(mbase, bchannel);

				if (count == 0)//all of the data have been transferred, should notify the CPU to process. 
					int_hsdma |= (1 << bchannel);
			}
		}

		INFO("int_hsdma = 0x%x\n", int_hsdma);

		if (!int_hsdma)
			goto done;
	}

	for (bchannel = 0; bchannel < MUSBFSH_HSDMA_CHANNELS; bchannel++) {
		if (int_hsdma & (1 << bchannel)) {
			musbfsh_channel = (struct musbfsh_dma_channel *)
					&(controller->channel[bchannel]);
			channel = &musbfsh_channel->channel;

			csr = musbfsh_readw(mbase,
					MUSBFSH_HSDMA_CHANNEL_OFFSET(bchannel,
							MUSBFSH_HSDMA_CONTROL));

			if (csr & (1 << MUSBFSH_HSDMA_BUSERROR_SHIFT)) {
				musbfsh_channel->channel.status =
					MUSBFSH_DMA_STATUS_BUS_ABORT;
			} else {
				u8 devctl;

				addr = musbfsh_read_hsdma_addr(mbase,
						bchannel);//the register of address will increase with the data transfer.
				channel->actual_len = addr
					- musbfsh_channel->start_addr;

				INFO("ch %p, 0x%x -> 0x%x (%zu / %d) %s\n",
					channel, musbfsh_channel->start_addr,
					addr, channel->actual_len,
					musbfsh_channel->len,
					(channel->actual_len
						< musbfsh_channel->len) ?
					"=> reconfig 0" : "=> complete");

				devctl = musbfsh_readb(mbase, MUSBFSH_DEVCTL);

				channel->status = MUSBFSH_DMA_STATUS_FREE;

				/* completed */
				if ((devctl & MUSBFSH_DEVCTL_HM)
					&& (musbfsh_channel->transmit)//Tx
					&& ((channel->desired_mode == 0)
					    || (channel->actual_len &//indicate it is a short packet
					    (musbfsh_channel->max_packet_sz - 1)))
				    ) {
					u8  epnum  = musbfsh_channel->epnum;
					int offset = MUSBFSH_EP_OFFSET(epnum,
								    MUSBFSH_TXCSR);
					u16 txcsr;

					/*
					 * The programming guide says that we
					 * must clear DMAENAB before DMAMODE.
					 */
					musbfsh_ep_select(mbase, epnum);
					txcsr = musbfsh_readw(mbase, offset);
					txcsr &= ~(MUSBFSH_TXCSR_DMAENAB
							| MUSBFSH_TXCSR_AUTOSET);
					musbfsh_writew(mbase, offset, txcsr);
					/* Send out the packet */
					txcsr &= ~MUSBFSH_TXCSR_DMAMODE;
					txcsr |=  MUSBFSH_TXCSR_TXPKTRDY;//the packet has been in the fifo,only need to set TxPktRdy
					musbfsh_writew(mbase, offset, txcsr);
				}
					musbfsh_dma_completion(musbfsh, musbfsh_channel->epnum,
						    musbfsh_channel->transmit);
				}
			}
		}

	retval = IRQ_HANDLED;
done:
	// spin_unlock_irqrestore(&musbfsh->lock, flags);
	return retval;
}

void musbfsh_dma_controller_destroy(struct dma_controller *c)
{
	struct musbfsh_dma_controller *controller = container_of(c,
			struct musbfsh_dma_controller, controller);
	
	INFO("++\n");
	if (!controller)
		return;

	if (controller->irq)
		free_irq(controller->irq, c);

	kfree(controller);
}

struct dma_controller *__init
musbfsh_dma_controller_create(struct musbfsh *musbfsh, void __iomem *base)
{
	struct musbfsh_dma_controller *controller;
	
	INFO("++\n");

	controller = kzalloc(sizeof(*controller), GFP_KERNEL);
	if (!controller)
		return NULL;

	controller->channel_count = MUSBFSH_HSDMA_CHANNELS;
	controller->private_data = musbfsh;
	controller->base = base;

	controller->controller.start = dma_controller_start;
	controller->controller.stop = dma_controller_stop;
	controller->controller.channel_alloc = dma_channel_allocate;
	controller->controller.channel_release = dma_channel_release;
	controller->controller.channel_program = dma_channel_program;
	controller->controller.channel_abort = dma_channel_abort;

	controller->irq = 0;
	musbfsh->musbfsh_dma_controller = controller;
	//enable DMA interrupt for all channels

#ifdef CONFIG_MTK_ICUSB_SUPPORT
	if(skip_mac_init_attr.value)
	{
		MYDBG("");
	}
	else
	{
		musbfsh_writeb(base,MUSBFSH_HSDMA_DMA_INTR_UNMASK_SET,0xff);
	}
#else
	musbfsh_writeb(base,MUSBFSH_HSDMA_DMA_INTR_UNMASK_SET,0xff);
#endif

	return &controller->controller;
}

