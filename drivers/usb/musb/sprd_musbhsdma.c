// SPDX-License-Identifier: GPL-2.0
/*
 * sprd-musbhsdmac - Unisoc MUSB Specific Glue layer
 *
 * Copyright (c) 2020 Unisoc Inc.
 * http://www.Unisoc.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/usb/ch9.h>
#include "musb_core.h"
#include "musb_host.h"
#include "sprd_musbhsdma.h"

static void sprd_dma_channel_release(struct dma_channel *channel);

static void sprd_dma_controller_stop(struct sprd_musb_dma_controller *controller)
{
	struct musb *musb = controller->private_data;
	struct dma_channel *channel;
	u32 bit;

	if (controller->used_channels != 0) {
		dev_info(musb->controller,
			"Stopping DMA controller while channel active\n");

		/* start form ep1 ~ ep15, channel1 ~ channel30 */
		for (bit = 1; bit <= MUSB_DMA_CHANNELS; bit++) {
			if (controller->used_channels & (1 << bit)) {
				channel = &controller->channel[bit].channel;
				sprd_dma_channel_release(channel);

				if (!controller->used_channels)
					break;
			}
		}
	}
}

/*alloc dma channel number*/

static void sprd_dma_channel_release(struct dma_channel *channel)
{
	struct sprd_musb_dma_channel *musb_channel;
	struct sprd_musb_dma_controller *controller;

	if (!channel)
		return;
	musb_channel = channel->private_data;
	controller = musb_channel->controller;

	channel->actual_len = 0;

	musb_channel->controller->used_channels &=
		~(1 << musb_channel->channel_num);

	if (controller->used_channels == 0)
		wake_up(&controller->wait);

	channel->status = MUSB_DMA_STATUS_UNKNOWN;
}

static struct dma_channel *sprd_dma_channel_allocate(struct dma_controller *c,
						     struct musb_hw_ep *hw_ep,
						     u8 transmit)
{
	struct sprd_musb_dma_controller *controller = container_of(c,
			struct sprd_musb_dma_controller, controller);
	struct sprd_musb_dma_channel *musb_channel = NULL;
	struct dma_channel *channel = NULL;
	u8 bit;
	u16 csr;
	struct musb *musb;
#if IS_ENABLED(CONFIG_USB_SPRD_ADAPTIVE)
	u32 reg;
#endif

	WARN_ON(hw_ep->epnum == 0);
	bit = hw_ep->epnum;
	musb = controller->private_data;

	if (transmit) {
		musb_channel = &(controller->channel[bit]);
		controller->used_channels |= (1 << bit);
		musb_channel->channel_num = bit;
		if (musb->xceiv->otg->state != OTG_STATE_A_HOST) {
			/* CONFIG DMA MODE */
			csr = musb_readw(hw_ep->regs, MUSB_TXCSR);
			csr |= MUSB_TXCSR_DMAMODE | MUSB_TXCSR_DMAENAB |
				MUSB_TXCSR_AUTOSET;
			musb_writew(hw_ep->regs, MUSB_TXCSR, csr);
		}
	} else {
		musb_channel = &(controller->channel[bit + 15]);
		controller->used_channels |= (1 << (bit + 15));
		musb_channel->channel_num = bit + 15;
		if (musb->xceiv->otg->state != OTG_STATE_A_HOST) {
			/* CONFIG DMA MODE */
			csr = musb_readw(hw_ep->regs, MUSB_RXCSR);
			csr |= MUSB_RXCSR_DMAMODE | MUSB_RXCSR_DMAENAB |
				MUSB_RXCSR_AUTOCLEAR;
			musb_writew(hw_ep->regs, MUSB_RXCSR, csr);
		}
	}

	/* Wait 9 more cycles for ensuring DMA can get USB request length */
#if IS_ENABLED(CONFIG_USB_SPRD_ADAPTIVE)
	reg = musb_readl(controller->base, MUSB_DMA_FRAG_WAIT);
	musb_writel(controller->base, MUSB_DMA_FRAG_WAIT, reg | 0x8);
#else
	musb_writel(controller->base, MUSB_DMA_FRAG_WAIT, 0x8);
#endif
	musb_channel->controller = controller;
	musb_channel->ep_num = bit;
	musb_channel->transmit = transmit;
	musb_channel->node_num = 0;
	musb_channel->busy_slot = 0;
	musb_channel->free_slot = 0;
	channel = &(musb_channel->channel);
	channel->private_data = musb_channel;
	channel->status = MUSB_DMA_STATUS_FREE;
	channel->max_len = 0xffff;
	/* Tx => mode 1; Rx => mode 0 */
	channel->desired_mode = transmit;
	channel->actual_len = 0;
	INIT_LIST_HEAD(&musb_channel->req_queued);

	return channel;
}

#if IS_ENABLED(CONFIG_USB_SPRD_LINKFIFO)
static void musb_reg_update(struct musb *musb, unsigned int reg,
		       unsigned int mask, unsigned int val)
{
	void __iomem *mbase = musb->mregs;
	u32 shift = __ffs(mask);
	u32 tmp;

	tmp = musb_readl(mbase, reg);
	tmp &= ~mask;
	tmp |= (val << shift) & mask;
	musb_writel(mbase, reg, tmp);
}

static void musb_linknode_pushlist(struct musb *musb,
		u64 ll_list, u32 is_tx, u32 chan)
{
	void __iomem *mbase = musb->mregs;

	if (is_tx) {
		musb_reg_update(musb, MUSB_DMA_MULT_LL_CTRL,
				BIT_TX_IPA_CHN_MASK, chan);

		musb_writel(mbase, MUSB_DMA_TX_CMD_QUEUE_HIGH,
				((u32)(ll_list >> 32)) & 0xf);
		musb_writel(mbase, MUSB_DMA_TX_CMD_QUEUE_LOW, (u32)ll_list);

		musb_reg_update(musb, MUSB_DMA_MULT_LL_CTRL,
				BIT_TX_CMD_QUEUE_WR, 1);
	} else {
		musb_reg_update(musb, MUSB_DMA_MULT_LL_CTRL,
				BIT_RX_IPA_CHN_MASK, chan);

		musb_writel(mbase, MUSB_DMA_RX_CMD_QUEUE_HIGH,
				((u32)(ll_list >> 32)) & 0xf);
		musb_writel(mbase, MUSB_DMA_RX_CMD_QUEUE_LOW, (u32)ll_list);

		musb_reg_update(musb, MUSB_DMA_MULT_LL_CTRL,
				BIT_RX_CMD_QUEUE_WR, 1);
	}
}

static u64 musb_linknode_poplist(struct musb *musb, int is_tx, u32 chan)
{
	void __iomem *mbase = musb->mregs;
	u64 cmplt_list;
	u32 cmplt_list_low;
	u32 cmplt_list_high;

	if (is_tx) {
		musb_reg_update(musb, MUSB_DMA_MULT_LL_CTRL,
				BIT_TX_IPA_CHN_MASK, chan);
		musb_reg_update(musb, MUSB_DMA_MULT_LL_CTRL,
				BIT_TX_CMPLT_QUEUE_RD, 1);

		cmplt_list_low = musb_readl(mbase, MUSB_DMA_TX_CMPLT_QUEUE_LOW);
		cmplt_list_high = musb_readl(mbase,
					     MUSB_DMA_TX_CMPLT_QUEUE_HIGH);
	} else {
		musb_reg_update(musb, MUSB_DMA_MULT_LL_CTRL,
				BIT_RX_IPA_CHN_MASK, chan);
		musb_reg_update(musb, MUSB_DMA_MULT_LL_CTRL,
				BIT_RX_CMPLT_QUEUE_RD, 1);

		cmplt_list_low = musb_readl(mbase, MUSB_DMA_RX_CMPLT_QUEUE_LOW);
		cmplt_list_high = musb_readl(mbase,
					     MUSB_DMA_RX_CMPLT_QUEUE_HIGH);
	}

	cmplt_list = ((u64)cmplt_list_high << 32) | cmplt_list_low;
	return cmplt_list;
}

void musb_linknode_clear(struct musb *musb, int is_tx, int chan)
{
	struct dma_controller	*c = musb->dma_controller;
	struct sprd_musb_dma_controller *controller = container_of(c,
				struct sprd_musb_dma_controller, controller);
	struct sprd_musb_dma_channel *musb_channel = NULL;

	if (is_tx)
		musb_channel = &(controller->channel[chan]);
	else
		musb_channel = &(controller->channel[chan + 15]);

	musb_channel->used_queue = 0;
	if (is_tx) {
		musb_reg_update(musb, MUSB_DMA_MULT_LL_CTRL,
				BIT_TX_IPA_CHN_MASK, chan);
		musb_reg_update(musb, MUSB_DMA_MULT_LL_Q_CTRL_STATUS,
				BIT_TX_CMD_CLR, 1);
		musb_reg_update(musb, MUSB_DMA_MULT_LL_Q_CTRL_STATUS,
				BIT_TX_CMPLT_CLR, 1);
	} else {
		musb_reg_update(musb, MUSB_DMA_MULT_LL_CTRL,
				BIT_RX_IPA_CHN_MASK, chan);
		musb_reg_update(musb, MUSB_DMA_MULT_LL_Q_CTRL_STATUS,
				BIT_RX_CMD_CLR, 1);
		musb_reg_update(musb, MUSB_DMA_MULT_LL_Q_CTRL_STATUS,
				BIT_RX_CMPLT_CLR, 1);
	}
}
EXPORT_SYMBOL_GPL(musb_linknode_clear);

u32 musb_linknode_full(struct musb *musb, u32 is_tx)
{
	void __iomem *mbase = musb->mregs;
	u32 is_queue_full = 0;
	u32 ctrl_status;

	ctrl_status = musb_readl(mbase, MUSB_DMA_MULT_LL_Q_CTRL_STATUS);
	if (is_tx)
		is_queue_full = (ctrl_status & BIT_TX_CMD_FULL) ? 1 : 0;
	else
		is_queue_full = (ctrl_status & BIT_RX_CMD_FULL) ? 1 : 0;

	if (is_queue_full)
		dev_info(musb->controller, "%s channel is full\n",
		is_tx ? "tx" : "rx");

	return is_queue_full;
}

static void sprd_configure_channel(struct dma_channel *channel,
				   u8 transmit)
{
	struct sprd_musb_dma_channel *musb_channel = channel->private_data;
	struct sprd_musb_dma_controller *controller = musb_channel->controller;
	void __iomem *mbase = controller->base;
	u8 bchannel = musb_channel->channel_num;
	struct musb *musb = controller->private_data;
	u32 csr = 0;
	u32 haddr4;
	struct musb_hw_ep *hw_ep;
	struct musb_qh *qh;
	u32 queue = musb_channel->used_queue;

	hw_ep = &musb->endpoints[musb_channel->ep_num];
	if (musb_channel->transmit)
		qh = hw_ep->out_qh;
	else
		qh = hw_ep->in_qh;

	if (qh && qh->linkfifo) {
		if (transmit) {
			/* enable linklist end interrupt */
			csr = musb_readl(mbase, MUSB_DMA_CHN_INTR(bchannel));
			csr |= CHN_LLIST_INT_EN | CHN_CLEAR_INT_EN;
			musb_writel(mbase, MUSB_DMA_CHN_INTR(bchannel), csr);

			/* set linklist pointer */
			musb_linknode_pushlist(musb,
					       (u64)musb_channel->list_dma_addr[queue],
					       transmit, bchannel);

			/* enable channel and trigger tx dma transfer */
			csr = musb_readl(mbase,
					 MUSB_DMA_CHN_PAUSE(bchannel));
			if (csr & CHN_CLR)
				musb_writel(mbase,
					    MUSB_DMA_CHN_PAUSE(bchannel), 0);
		} else {
			/* enable linklist end and rx last interrupt */
			csr = musb_readl(mbase, MUSB_DMA_CHN_INTR(bchannel));
			csr |= CHN_USBRX_INT_EN | CHN_LLIST_INT_EN | CHN_CLEAR_INT_EN;
			musb_writel(mbase, MUSB_DMA_CHN_INTR(bchannel), csr);

			/* set linklist pointer */
			musb_linknode_pushlist(musb,
					       (u64)musb_channel->list_dma_addr[queue],
					       transmit, bchannel - 15);

			/* enable channel and trigger rx dma transfer */
			csr = musb_readl(mbase,
					 MUSB_DMA_CHN_PAUSE(bchannel));
			if (csr & CHN_CLR)
				musb_writel(mbase,
					    MUSB_DMA_CHN_PAUSE(bchannel), 0);
		}
	} else {
		haddr4 = (u32)((u64)musb_channel->list_dma_addr[queue] >> 32);
		haddr4 <<= 4;
		if (transmit) {
			/* enable linklist end interrupt */
			csr = musb_readl(mbase, MUSB_DMA_CHN_INTR(bchannel));
#if IS_ENABLED(CONFIG_USB_SPRD_ADAPTIVE)
			if (!musb->is_adaptive_out || musb->adaptive_out_configured)
				csr |= CHN_LLIST_INT_EN | CHN_CLEAR_INT_EN;
#else
			csr |= CHN_LLIST_INT_EN | CHN_CLEAR_INT_EN;
#endif
			musb_writel(mbase, MUSB_DMA_CHN_INTR(bchannel), csr);

			/* set linklist pointer */
			musb_writel(mbase, MUSB_DMA_CHN_LLIST_PTR(bchannel),
						(u32)musb_channel->list_dma_addr[queue]);

			musb_writel(mbase, MUSB_DMA_CHN_ADDR_H(bchannel), haddr4);
			/* enable channel and trigger tx dma transfer */
			csr = musb_readl(mbase, MUSB_DMA_CHN_PAUSE(bchannel));
			if (csr & CHN_CLR)
				musb_writel(mbase, MUSB_DMA_CHN_PAUSE(bchannel), 0);
			csr = musb_readl(mbase, MUSB_DMA_CHN_CFG(bchannel));
			csr |= CHN_EN;
			musb_writel(mbase, MUSB_DMA_CHN_CFG(bchannel), csr);
		} else {
			/* enable linklist end and rx last interrupt */
			csr = musb_readl(mbase, MUSB_DMA_CHN_INTR(bchannel));
#if IS_ENABLED(CONFIG_USB_SPRD_ADAPTIVE)
			if (!musb->is_adaptive_in || musb->adaptive_in_configured)
				csr |= CHN_USBRX_INT_EN | CHN_LLIST_INT_EN | CHN_CLEAR_INT_EN;
#else
			csr |= CHN_USBRX_INT_EN | CHN_LLIST_INT_EN | CHN_CLEAR_INT_EN;
#endif
			musb_writel(mbase, MUSB_DMA_CHN_INTR(bchannel), csr);

			/* set linklist pointer */
			musb_writel(mbase, MUSB_DMA_CHN_LLIST_PTR(bchannel),
						(u32)musb_channel->list_dma_addr[queue]);

			musb_writel(mbase, MUSB_DMA_CHN_ADDR_H(bchannel), haddr4);
			/* enable channel and trigger rx dma transfer */
			csr = musb_readl(mbase, MUSB_DMA_CHN_PAUSE(bchannel));
			if (csr & CHN_CLR)
				musb_writel(mbase, MUSB_DMA_CHN_PAUSE(bchannel), 0);
			csr = musb_readl(mbase, MUSB_DMA_CHN_CFG(bchannel));
			csr |= CHN_EN;
			musb_writel(mbase, MUSB_DMA_CHN_CFG(bchannel), csr);
		}
	}
}

static void musb_prepare_node(struct sprd_musb_dma_channel *musb_channel,
			      dma_addr_t dma_addr, u32 len,
			      u8 last, u8 sp, u32 index)
{
	struct linklist_node_s	*dma_linklist =
					musb_channel->dma_linklist[musb_channel->used_queue];
#if IS_ENABLED(CONFIG_USB_SPRD_ADAPTIVE)
	struct sprd_musb_dma_controller *controller = musb_channel->controller;
	struct musb *musb = controller->private_data;

	if ((musb->is_adaptive_in && !musb->adaptive_in_configured) ||
		(musb->is_adaptive_out && !musb->adaptive_out_configured))
		dev_info(musb->controller, "blk_len: %d.\n", len);
#endif
	musb_channel->free_slot++;

	dma_linklist[index].addr = (unsigned int)dma_addr;
	dma_linklist[index].data_addr =
		((u8)((u64)dma_addr >> 32)) & 0xf;
#if IS_ENABLED(CONFIG_USB_SPRD_ADAPTIVE)
	/* adaptive in mode, when need bigger DMA buffer */
	if (musb->is_adaptive_in && !musb->adaptive_in_configured
		&& musb_channel->channel_num == 16)
		dma_linklist[index].blk_len = 1024;
	else
		dma_linklist[index].blk_len = len;
#else
	dma_linklist[index].blk_len = len;
#endif
	dma_linklist[index].frag_len = 32;
	dma_linklist[index].ioc = last;
	dma_linklist[index].sp = sp;
	dma_linklist[index].list_end = last;
	/*
	 *   wmb below is used to make sure linklist CPU
	 *   initialized is really written to DDR before
	 *   USB DMA read the linklist.
	 */
	wmb();
}

static void musb_prepare_sg_lastnode(struct sprd_musb_dma_channel *musb_channel,
				     u32 index)
{
	struct linklist_node_s	*dma_linklist =
					musb_channel->dma_linklist[musb_channel->used_queue];
	dma_linklist[index].ioc = 1;
	dma_linklist[index].list_end = 1;
	/*
	 *   wmb below is used to make sure linklist CPU
	 *   initialized is really written to DDR before
	 *   USB DMA read the linklist.
	 */
	wmb();
}

static void musb_host_listnodes_for_isoc(struct sprd_musb_dma_channel *musb_channel,
					struct musb_qh *qh)
{
	struct urb		*urb = last_urb(qh);
	struct usb_iso_packet_descriptor *d;
	u32 i;

	for (i = 0; i < urb->number_of_packets; i++) {
		d = urb->iso_frame_desc + i;
		musb_prepare_node(musb_channel,
			(urb->transfer_dma + d->offset),
			d->length,
			(i == urb->number_of_packets - 1),
			(d->length < musb_channel->max_packet_sz),
			i);
	}
}
#else
static void sprd_configure_channel(struct dma_channel *channel,
				   u8 transmit)
{
	struct sprd_musb_dma_channel *musb_channel = channel->private_data;
	struct sprd_musb_dma_controller *controller = musb_channel->controller;
	void __iomem *mbase = controller->base;
#if IS_ENABLED(CONFIG_USB_SPRD_ADAPTIVE)
	struct musb *musb = controller->private_data;
#endif
	u8 bchannel = musb_channel->channel_num;
	u32 csr = 0;
	u32 haddr4;

	haddr4 = (u32)((u64)musb_channel->list_dma_addr >> 32);
	haddr4 <<= 4;
	if (transmit) {
		/* enable linklist end interrupt */
		csr = musb_readl(mbase, MUSB_DMA_CHN_INTR(bchannel));
#if IS_ENABLED(CONFIG_USB_SPRD_ADAPTIVE)
		if (!musb->is_adaptive_out || musb->adaptive_out_configured)
			csr |= CHN_LLIST_INT_EN | CHN_CLEAR_INT_EN;
#else
		csr |= CHN_LLIST_INT_EN | CHN_CLEAR_INT_EN;
#endif
		musb_writel(mbase, MUSB_DMA_CHN_INTR(bchannel), csr);

		/* set linklist pointer */
		musb_writel(mbase, MUSB_DMA_CHN_LLIST_PTR(bchannel),
					(u32)musb_channel->list_dma_addr);

		musb_writel(mbase, MUSB_DMA_CHN_ADDR_H(bchannel), haddr4);
		/* enable channel and trigger tx dma transfer */
		csr = musb_readl(mbase, MUSB_DMA_CHN_PAUSE(bchannel));
		if (csr & CHN_CLR)
			musb_writel(mbase, MUSB_DMA_CHN_PAUSE(bchannel), 0);
		csr = musb_readl(mbase, MUSB_DMA_CHN_CFG(bchannel));
		csr |= CHN_EN;
		musb_writel(mbase, MUSB_DMA_CHN_CFG(bchannel), csr);
	} else {
		/* enable linklist end and rx last interrupt */
		csr = musb_readl(mbase, MUSB_DMA_CHN_INTR(bchannel));

#if IS_ENABLED(CONFIG_USB_SPRD_ADAPTIVE)
		if (!musb->is_adaptive_in || musb->adaptive_in_configured)
			csr |= CHN_USBRX_INT_EN | CHN_LLIST_INT_EN | CHN_CLEAR_INT_EN;
#else
		csr |= CHN_USBRX_INT_EN | CHN_LLIST_INT_EN | CHN_CLEAR_INT_EN;
#endif
		musb_writel(mbase, MUSB_DMA_CHN_INTR(bchannel), csr);

		/* set linklist pointer */
		musb_writel(mbase, MUSB_DMA_CHN_LLIST_PTR(bchannel),
					(u32)musb_channel->list_dma_addr);

		musb_writel(mbase, MUSB_DMA_CHN_ADDR_H(bchannel), haddr4);
		/* enable channel and trigger rx dma transfer */
		csr = musb_readl(mbase, MUSB_DMA_CHN_PAUSE(bchannel));
		if (csr & CHN_CLR)
			musb_writel(mbase, MUSB_DMA_CHN_PAUSE(bchannel), 0);
		csr = musb_readl(mbase, MUSB_DMA_CHN_CFG(bchannel));
		csr |= CHN_EN;
		musb_writel(mbase, MUSB_DMA_CHN_CFG(bchannel), csr);
	}
}

static void musb_prepare_node(struct sprd_musb_dma_channel *musb_channel,
			      dma_addr_t dma_addr, u32 len,
			      u8 last, u8 sp, u32 index)
{
#if IS_ENABLED(CONFIG_USB_SPRD_ADAPTIVE)
	struct sprd_musb_dma_controller *controller = musb_channel->controller;
	struct musb *musb = controller->private_data;

	if ((musb->is_adaptive_in && !musb->adaptive_in_configured) ||
		(musb->is_adaptive_out && !musb->adaptive_out_configured))
		dev_info(musb->controller, "blk_len: %d.\n", len);
#endif

	musb_channel->free_slot++;

	musb_channel->dma_linklist[index].addr = (unsigned int)dma_addr;
	musb_channel->dma_linklist[index].data_addr =
		((u8)((u64)dma_addr >> 32)) & 0xf;

#if IS_ENABLED(CONFIG_USB_SPRD_ADAPTIVE)
	/* adaptive in mode, when need bigger DMA buffer */
	if (musb->is_adaptive_in && !musb->adaptive_in_configured
		&& musb_channel->channel_num == 16)
		musb_channel->dma_linklist[index].blk_len = 1024;
	else
		musb_channel->dma_linklist[index].blk_len = len;
#else
	musb_channel->dma_linklist[index].blk_len = len;
#endif
	musb_channel->dma_linklist[index].frag_len = 32;
	musb_channel->dma_linklist[index].ioc = last;
	musb_channel->dma_linklist[index].sp = sp;
	musb_channel->dma_linklist[index].list_end = last;
	/*
	 *   wmb below is used to make sure linklist CPU
	 *   initialized is really written to DDR before
	 *   USB DMA read the linklist.
	 */
	wmb();
}

static void musb_prepare_sg_lastnode(struct sprd_musb_dma_channel *musb_channel,
				     u32 index)
{
	musb_channel->dma_linklist[index].ioc = 1;
	musb_channel->dma_linklist[index].list_end = 1;
	/*
	 *   wmb below is used to make sure linklist CPU
	 *   initialized is really written to DDR before
	 *   USB DMA read the linklist.
	 */
	wmb();
}
#endif

static void musb_prepare_nodes(struct sprd_musb_dma_channel *musb_channel,
			       dma_addr_t dma_addr, u32 length,
			       struct musb_request *musb_req,
			       struct musb_ep *musb_ep,
			       u32 node_num, u32 nodes_left)
{
	int i;
	u8 last_one = 0;
	u32 len;

	for (i = 0; i < node_num; i++) {
		nodes_left--;
		len = 0xfffc;
		if ((node_num - 1) == i) {
			len = length - (node_num
			- 1) * 0xfffc;
			last_one = 1;
		}
		musb_prepare_node(musb_channel,
		dma_addr, len, last_one, 0,
		musb_channel->node_num);
		dma_addr += len;
		musb_channel->node_num++;
	}
	list_del(&musb_req->list);
	list_add_tail(&musb_req->list, &musb_channel->req_queued);
}

static void musb_prepare_first_nodes(struct sprd_musb_dma_channel *musb_channel,
				     dma_addr_t dma_addr,
				     u32 length,
				     dma_addr_t addr_last,
				     u32 node_num, u32 nodes_left)
{
	int i;
	u8 last_one = 0;
	unsigned int len;

	for (i = 0; i < node_num; i++) {
		nodes_left--;
		len = 0xfffc;
		if ((node_num - 1) == i) {
			len = length - (node_num - 1) * 0xfffc;
			if (!addr_last)
				last_one = 1;
		}
		musb_prepare_node(musb_channel,
		dma_addr, len, last_one, 0,
		musb_channel->node_num);
		dma_addr += len;
		musb_channel->node_num++;
	}
}

static void musb_prepare_listnodes(struct sprd_musb_dma_channel *musb_channel,
				   struct musb_ep *musb_ep, bool starting)
{
	struct musb_request *musb_req, *n;
	u8 last_one = 0;
	dma_addr_t addr, addr_cpr, addr_last;
	unsigned int length;
	dma_addr_t dma;
	u32 i, nodes_left, len;
	u8  sp = 0;
	u32 total_sgs = 0, len_sgs;

	/* the first request must not be queued */
	nodes_left = (musb_channel->busy_slot - musb_channel->free_slot)
			& LISTNODE_MASK;

	/*
	 * If busy & slot are equal than it is either full or empty. If we are
	 * starting to process requests then we are empty. Otherwise we are
	 * full and don't do anything
	 */
	if (!nodes_left) {
		if (!starting)
			return;
		nodes_left = LISTNODE_NUM;
		musb_channel->busy_slot = 0;
		musb_channel->free_slot = 0;
		musb_channel->node_num = 0;
	}

	list_for_each_entry_safe(musb_req, n, &musb_ep->req_list, list) {
		if (musb_req->request.num_mapped_sgs > 0) {
			struct usb_request *request = &musb_req->request;
			struct scatterlist *sg = request->sg;
			struct scatterlist *s;

			/*
			 * total req's num_sgs should not exceed LISTNODE_NUM,
			 * or else set last_one flag
			 */
			total_sgs += musb_req->request.num_mapped_sgs;
			if (total_sgs > LISTNODE_NUM &&
				musb_channel->node_num > 1) {
				musb_prepare_sg_lastnode(musb_channel,
					musb_channel->node_num - 1);
				break;
			}

			list_del(&musb_req->list);
			list_add_tail(&musb_req->list,
			&musb_channel->req_queued);

			len_sgs = 0;
			for_each_sg(sg, s, request->num_mapped_sgs, i) {
				length = sg_dma_len(s);
				dma = sg_dma_address(s);
				addr = (dma + length) & ADDR_FLAG;
				addr_cpr = dma & ADDR_FLAG;
				addr_last = (dma + length) & 0xfffffff;
				sp = 0;
				len_sgs += length;

				if (i == (request->num_mapped_sgs - 1) || sg_is_last(s)) {
					if (list_empty(&musb_ep->req_list) ||
						(len_sgs % musb_ep->end_point.maxpacket == 0)) {
						last_one = 1;
						sp = 0;
					} else {
						last_one = 0;
						sp = 1;
					}
				}

				nodes_left--;
				if (!nodes_left) {
					last_one = 1;
					sp = 0;
				}

				if (addr == addr_cpr || !addr_last) {
					musb_prepare_node(musb_channel,
					dma, length, last_one, sp,
					musb_channel->node_num);
					musb_channel->node_num++;
				} else {
					/*
					 *If nodes_left is 0,need config list
					 *node next time,but if request is
					 *empty,it canconfig this time.
					 */
					if (last_one && (!nodes_left))
						break;
					len = (u32)(addr - dma);
					musb_prepare_node(musb_channel,
						dma, len, 0, sp,
						musb_channel->node_num);
					musb_channel->node_num++;
					nodes_left--;

					if (!nodes_left)
						last_one = 1;
					len = length - (addr - dma);
					musb_prepare_node(musb_channel,
						addr, len, last_one, sp,
						musb_channel->node_num);
					musb_channel->node_num++;
				}

				if (last_one)
					break;
			}

			if (last_one)
				break;
		} else {
			nodes_left = LISTNODE_NUM;
			musb_channel->busy_slot = 0;
			musb_channel->free_slot = 0;
			musb_channel->node_num = 0;

			length = musb_req->request.length;
			dma = musb_req->request.dma;

			addr = (dma + length) & ADDR_FLAG;
			addr_cpr = dma & ADDR_FLAG;
			addr_last = (dma + length) & 0xfffffff;

			if (length < 0xffff) {
				if (addr == addr_cpr) {
					nodes_left--;

					list_del(&musb_req->list);
					list_add_tail(&musb_req->list,
					&musb_channel->req_queued);
					last_one = 1;
					if (musb_req->request.zero && length &&
					(length % musb_ep->packet_sz == 0)) {
						musb_prepare_node(
						musb_channel,
						dma, length, 0, sp,
						musb_channel->node_num);
						musb_channel->node_num++;

						musb_prepare_node(
						musb_channel, dma, 0, 1, sp,
						musb_channel->node_num);
						musb_channel->node_num++;
					} else {
						musb_prepare_node(
						musb_channel, dma, length,
						last_one, sp,
						musb_channel->node_num);
						musb_channel->node_num++;
					}
					if (last_one)
						break;
				} else {
					nodes_left--;
					len = (u32)(addr - dma);
					if (!addr_last)
						last_one = 1;
					musb_prepare_node(musb_channel,
					dma, len, last_one, sp,
					musb_channel->node_num);
					musb_channel->node_num++;

					if (addr_last) {
						nodes_left--;
						len = length - (addr - dma);
						last_one = 1;
						musb_prepare_node(musb_channel,
						addr, len, last_one, sp,
						musb_channel->node_num);
						musb_channel->node_num++;
					}
					list_del(&musb_req->list);
					list_add_tail(&musb_req->list,
					&musb_channel->req_queued);
					if (last_one)
						break;
				}
			} else {
				last_one = 1;

				if (addr == addr_cpr) {
					u32 node_num;

					if (length % 0xfffc)
						node_num = length / 0xfffc + 1;
					else
						node_num = length / 0xfffc;

					musb_prepare_nodes(musb_channel,
					dma, length, musb_req, musb_ep,
					node_num, nodes_left);
				} else {
					u32 node_num1, node_num2;

					if ((addr_cpr + 0x10000000
						- dma) % 0xfffc)
						node_num1 = (addr_cpr +
						0x10000000 - dma) / 0xfffc + 1;
					else
						node_num1 = (addr_cpr +
						0x10000000 - dma) / 0xfffc;

					if ((dma + length - addr) % 0xfffc)
						node_num2 = (dma + length
						- addr) / 0xfffc + 1;
					else
						node_num2 = (dma + length
						- addr) / 0xfffc;

					len = addr - dma;
					musb_prepare_first_nodes(musb_channel,
						dma, len, addr_last,
						node_num1, nodes_left);

					if (addr_last) {
						len = dma + length - addr;
						musb_prepare_nodes(musb_channel,
						addr, len, musb_req, musb_ep,
						node_num2, nodes_left);
					} else {
						list_del(&musb_req->list);
						list_add_tail(&musb_req->list,
						&musb_channel->req_queued);
					}
				}
				if (last_one)
					break;
			}
		}
	}
}

static void musb_host_prepare_nodes(struct sprd_musb_dma_channel *musb_channel,
				    dma_addr_t dma_addr, u32 length,
				    u32 node_num, u32 nodes_left)
{
	int i;
	u8 last_one = 0;
	unsigned int len;

	for (i = 0; i < node_num; i++) {
		nodes_left--;
		len = 0xfffc;
		if ((node_num - 1) == i) {
			len = length - (node_num
			- 1) * 0xfffc;
			last_one = 1;
		}
		musb_prepare_node(musb_channel,
		dma_addr, len, last_one, 0,
		musb_channel->node_num);
		dma_addr += len;
		musb_channel->node_num++;
	}
}

static void musb_host_listnodes(struct sprd_musb_dma_channel *musb_channel,
				dma_addr_t dma_addr, u32 len)
{
	dma_addr_t addr, addr_cpr, addr_last;
	u32 i;
	u32 nodes_left;
	u32 length;
	int last_one = 0;

	nodes_left = LISTNODE_NUM;
	addr = (dma_addr + len) & ADDR_FLAG;
	addr_cpr = dma_addr & ADDR_FLAG;

	if (len < 0xffff) {
		if (addr == addr_cpr) {
			nodes_left--;
			musb_prepare_node(musb_channel,
				dma_addr, len, 1, 0,
				musb_channel->node_num);
			musb_channel->node_num++;
		} else {
			nodes_left--;
			length = (u32)(addr - dma_addr);
			addr_last = (dma_addr + len) & 0xfffffff;
			if (!addr_last)
				last_one = 1;
			musb_prepare_node(musb_channel,
				dma_addr, length, last_one, 0,
				musb_channel->node_num);
			musb_channel->node_num++;
			if (addr_last) {
				nodes_left--;
				length = len - (addr - dma_addr);
				musb_prepare_node(musb_channel,
					addr, length, 1, 0,
					musb_channel->node_num);
				musb_channel->node_num++;
			}
		}
	} else {
		if (addr == addr_cpr) {
			u32 node_num;

			if (len % 0xfffc)
				node_num = len / 0xfffc + 1;
			else
				node_num = len / 0xfffc;

			musb_host_prepare_nodes(musb_channel,
				dma_addr, len,
				node_num, nodes_left);
		} else {
			u32 node_num1, node_num2;
			u32 len2;

			addr_last = (dma_addr + len) & 0xfffffff;

			if ((addr_cpr + 0x10000000
				- dma_addr) % 0xfffc)
				node_num1 = (addr_cpr +
				0x10000000 - dma_addr) / 0xfffc + 1;
			else
				node_num1 = (addr_cpr +
				0x10000000 - dma_addr) / 0xfffc;

			if (addr_last) {
				if ((dma_addr + len - addr) % 0xfffc)
					node_num2 = (dma_addr + len
						- addr) / 0xfffc + 1;
				else
					node_num2 = (dma_addr + len
						- addr) / 0xfffc;
			} else
				node_num2 = 0;

			len2 = dma_addr + len - addr;

			for (i = 0; i < node_num1; i++) {
				nodes_left--;
				length = 0xfffc;
				if ((node_num1 - 1) == i) {
					length = addr_cpr +
						0x10000000 - dma_addr;
					if (!addr_last)
						last_one = 1;
				}
				musb_prepare_node(musb_channel,
					dma_addr, length, last_one, 0,
					musb_channel->node_num);
				dma_addr += length;
				musb_channel->node_num++;
			}

			if (addr_last) {
				musb_host_prepare_nodes(musb_channel,
					addr, len2,
					node_num2, nodes_left);
			}
		}
	}
}

static int sprd_dma_channel_program(struct dma_channel *channel,
				    u16 packet_sz, u8 mode,
				    dma_addr_t dma_addr, u32 len)
{
	struct sprd_musb_dma_channel *musb_channel = channel->private_data;
	struct sprd_musb_dma_controller *controller = musb_channel->controller;
	struct musb *musb = controller->private_data;
	struct musb_ep *musb_ep;
	struct musb_hw_ep *hw_ep;
#if IS_ENABLED(CONFIG_USB_SPRD_LINKFIFO)
	struct musb_qh *qh;
#endif

	if (channel->status == MUSB_DMA_STATUS_UNKNOWN ||
		channel->status == MUSB_DMA_STATUS_BUSY)
		return -EINVAL;

	hw_ep = &musb->endpoints[musb_channel->ep_num];
	if (musb_channel->transmit)
		musb_ep = &hw_ep->ep_in;
	else
		musb_ep = &hw_ep->ep_out;
	/*
	 * The DMA engine in RTL1.8 and above cannot handle
	 * DMA addresses that are not aligned to a 4 byte boundary.
	 * It ends up masking the last two bits of the address
	 * programmed in DMA_ADDR.
	 *
	 * Fail such DMA transfers, so that the backup PIO mode
	 * can carry out the transfer
	 */
	if ((musb->hwvers >= MUSB_HWVERS_1800) && (dma_addr % 4))
		return -EINVAL;

#if IS_ENABLED(CONFIG_USB_SPRD_LINKFIFO)
	if (musb_channel->transmit)
		qh = hw_ep->out_qh;
	else
		qh = hw_ep->in_qh;

	if (qh && qh->linkfifo) {
		if (musb_channel->used_queue >= CHN_MAX_QUEUE_SIZE)
			musb_channel->used_queue = 0;
	} else
#endif
	{
		musb_channel->used_queue = 0;
		channel->status = MUSB_DMA_STATUS_BUSY;
	}

	channel->actual_len = 0;
	musb_channel->max_packet_sz = packet_sz;

	/*
	 * DMA transfer length and address has some restriction
	 */

	if (musb->xceiv->otg->state == OTG_STATE_B_PERIPHERAL) {
		void __iomem *epio = hw_ep->regs;
		u16 csr, dma_setting;

		if (!musb_channel->transmit) {
			dma_setting = MUSB_RXCSR_AUTOCLEAR |
				      MUSB_RXCSR_DMAMODE |
				      MUSB_RXCSR_DMAENAB;
			csr = musb_readw(epio, MUSB_RXCSR);
			if ((csr & dma_setting) != dma_setting)
				musb_writew(epio, MUSB_RXCSR, dma_setting | csr);
		} else {
			dma_setting = MUSB_TXCSR_AUTOSET |
				      MUSB_TXCSR_DMAMODE |
				      MUSB_TXCSR_DMAENAB;
			csr = musb_readw(epio, MUSB_TXCSR);
			if ((csr & dma_setting) != dma_setting)
				musb_writew(epio, MUSB_TXCSR, dma_setting);
		}
		musb_prepare_listnodes(musb_channel, musb_ep, true);
	} else {
		musb_channel->busy_slot = 0;
		musb_channel->free_slot = 0;
		musb_channel->node_num = 0;
#if IS_ENABLED(CONFIG_USB_SPRD_LINKFIFO)
		if (qh && qh->linkfifo) {
			musb_linknode_full(musb, musb_channel->transmit);
			musb_host_listnodes_for_isoc(musb_channel, qh);
		} else
#endif
			musb_host_listnodes(musb_channel, dma_addr, len);
	}
	dev_dbg(musb->controller,
		"ep%d-%s  dma_addr 0x%x length %d\n",
		musb_channel->ep_num,
		musb_channel->transmit ? "Tx" : "Rx",
		(uint32_t)dma_addr, len);

	sprd_configure_channel(channel, musb_channel->transmit);

#if IS_ENABLED(CONFIG_USB_SPRD_LINKFIFO)
	if (qh && qh->linkfifo)
		musb_channel->used_queue++;
#endif

	return 1;
}

static inline struct musb_request *channel_get_next_request(struct list_head *list)
{
	if (list_empty(list))
		return NULL;

	return list_first_entry(list, struct musb_request, list);
}

#if IS_ENABLED(CONFIG_USB_MUSB_HOST) || IS_ENABLED(CONFIG_USB_MUSB_DUAL_ROLE)
static void musb_host_channel_abort(struct musb *musb,
				    struct sprd_musb_dma_channel *musb_channel)
{
	struct urb *urb;
	struct musb_qh *qh;
	struct musb_hw_ep *hw_ep = &musb->endpoints[musb_channel->ep_num];
	struct dma_channel *channel = &musb_channel->channel;

	if (musb_channel->transmit)
		qh = hw_ep->out_qh;
	else
		qh = hw_ep->in_qh;

	if (qh) {
		urb = next_urb(qh);
		if (urb)
			urb->status = -ECONNRESET;
		if (list_empty(&qh->hep->urb_list))
			channel->status = MUSB_DMA_STATUS_FREE;
	}
}
#else
static void musb_host_channel_abort(struct musb *musb,
	struct sprd_musb_dma_channel *musb_channel)
{
}
#endif

static int sprd_dma_channel_abort(struct dma_channel *channel)
{
	struct sprd_musb_dma_channel *musb_channel;
	struct sprd_musb_dma_controller *controller;
	void __iomem *mbase;
	struct musb *musb;
	struct musb_request *musb_req;
	struct usb_request *request;
	struct musb_ep *musb_ep;
	u8 bchannel;
	void __iomem *epio;
	u16 csr;
	u32 pause;

	if (!channel)
		return 0;

	musb_channel = channel->private_data;
	if (!musb_channel)
		return 0;

	controller = musb_channel->controller;
	mbase = controller->base;
	musb = controller->private_data;
	bchannel = musb_channel->channel_num;

	if (channel->status == MUSB_DMA_STATUS_BUSY) {
		pause = musb_readl(mbase, MUSB_DMA_CHN_PAUSE(bchannel));
		pause |= CHN_CLR;
		musb_writel(mbase, MUSB_DMA_CHN_PAUSE(bchannel), pause);
		if (musb->g.state != USB_STATE_NOTATTACHED) {
			epio = musb->endpoints[musb_channel->ep_num].regs;

			if (musb_channel->transmit) {
				/*
				 * The programming guide says that we must clear
				 * the DMAENAB bit before the DMAMODE bit...
				 */
				csr = musb_readw(epio, MUSB_TXCSR);
				csr &= ~(MUSB_TXCSR_AUTOSET |
				 MUSB_TXCSR_DMAENAB);
				musb_writew(epio, MUSB_TXCSR, csr);
				csr &= ~MUSB_TXCSR_DMAMODE;
				musb_writew(epio, MUSB_TXCSR, csr);
			} else {
				csr = musb_readw(epio, MUSB_RXCSR);
				csr &= ~(MUSB_RXCSR_AUTOCLEAR |
				 MUSB_RXCSR_DMAENAB |
				 MUSB_RXCSR_DMAMODE);
				musb_writew(epio, MUSB_RXCSR, csr);
			}

			musb_writel(mbase, MUSB_DMA_CHN_LLIST_PTR(bchannel), 0);
			musb_write_dma_addr(mbase, bchannel, 0);
		}
		if (!is_host_active(musb) &&
		    (musb->xceiv->otg->state != OTG_STATE_A_HOST) &&
		    (musb->xceiv->otg->state != OTG_STATE_A_WAIT_BCON)) {
			/*release request*/
			if (musb_channel->transmit)
				musb_ep =
				&musb->endpoints[musb_channel->ep_num].ep_in;
			else
				musb_ep =
				&musb->endpoints[musb_channel->ep_num].ep_out;
			while (!list_empty(&musb_channel->req_queued)) {
				musb_req =
					channel_get_next_request
					(&musb_channel->req_queued);
				request = &musb_req->request;
				if (request->num_mapped_sgs)
					musb_channel->busy_slot +=
						request->num_mapped_sgs;
				else
					musb_channel->busy_slot++;

				musb_g_giveback(musb_ep, request, -ESHUTDOWN);
				channel->status = MUSB_DMA_STATUS_FREE;
			}
		} else {
			musb_host_channel_abort(musb, musb_channel);
		}
	} else if (channel->status == MUSB_DMA_STATUS_CORE_ABORT) {
		pause = musb_readl(mbase, MUSB_DMA_CHN_PAUSE(bchannel));
		pause |= CHN_CLR;
		musb_writel(mbase, MUSB_DMA_CHN_PAUSE(bchannel), pause);
	}

	return 0;
}

static void sprd_musb_dma_completion(struct musb *musb, u8 epnum, u8 transmit)
{
	struct musb_ep *musb_ep;
	struct musb_request *musb_req;
	struct usb_request *request;
	struct dma_channel *channel;
	struct sprd_musb_dma_channel *musb_channel;
	u32 blk_len = 0;

	if (transmit)
		musb_ep = &musb->endpoints[epnum].ep_in;
	else
		musb_ep = &musb->endpoints[epnum].ep_out;

	channel = musb_ep->dma;
	if (!channel)
		return;

	musb_channel = channel->private_data;

	do {
		musb_req = channel_get_next_request(&musb_channel->req_queued);
		if (!musb_req) {
			WARN_ON_ONCE(1);
			break;
		}
		request = &musb_req->request;

		if (!transmit) {
			blk_len = musb_readl(musb->mregs,
				MUSB_DMA_CHN_LEN(epnum + 15));
			blk_len = (blk_len & 0xffff0000) >> 16;
			request->actual = request->length - blk_len;
		} else
			request->actual = request->length;

		if (request->num_mapped_sgs)
			musb_channel->busy_slot += request->num_mapped_sgs;
		else
			musb_channel->busy_slot++;

		musb_g_giveback(musb_ep, request, 0);
	} while (!list_empty(&musb_channel->req_queued));

	if ((musb_ep->dma == NULL) || (channel->status == MUSB_DMA_STATUS_UNKNOWN)) {
		dev_info(musb->controller, "musb_ep->dma is %s, channel->status = %d\n",
			musb_ep->dma ? "not NULL" : "NULL",  channel->status);
			return;
	}

	channel->status = MUSB_DMA_STATUS_FREE;
	musb_req = musb_ep->desc ? next_request(musb_ep) : NULL;
	if (!musb_req) {
		dev_dbg(musb->controller, "%s idle now\n",
			musb_ep->end_point.name);
		return;
	}
	musb_ep_select(musb->mregs, epnum);

	if (channel->status == MUSB_DMA_STATUS_BUSY) {
		dev_info(musb->controller, "dma pending...\n");
		return;
	}

	request = &musb_req->request;
	sprd_dma_channel_program(channel, musb_ep->packet_sz, musb_req->tx,
			request->dma + request->actual,
			request->length - request->actual);
}

#if IS_ENABLED(CONFIG_USB_MUSB_HOST) || IS_ENABLED(CONFIG_USB_MUSB_DUAL_ROLE)
static void sprd_calc_urb_actual_length(struct musb *musb,
	struct urb *urb,
	struct musb_qh *qh,
	struct dma_channel *channel,
	u32 dma_addr,
	u32 blk_len)
{
	u32 i = 0, j = 0;
	u32 node_num = 0;
	u32 unused_length = 0;
	struct sprd_musb_dma_channel *musb_channel = channel->private_data;

#if IS_ENABLED(CONFIG_USB_SPRD_LINKFIFO)
	u32 expect_length = 0;

	if (qh->linkfifo) {
		for (i = 0; i < urb->number_of_packets; i++)
			expect_length += urb->iso_frame_desc[i].length;
		urb->actual_length = expect_length - blk_len;
		return;
	}

	node_num = musb_channel->node_num;
	for (i = 0; i < node_num; i++) {
		if ((musb_channel->dma_linklist[0][i].addr +
			musb_channel->dma_linklist[0][i].blk_len -
			blk_len) == dma_addr)
			break;
	}

	if (i == node_num) {
		dev_err(musb->controller, "cannot find the dma_addr\n");
		urb->actual_length = 0;
		return;
	}

	if (i + 1 == node_num) {
		urb->actual_length += qh->segsize - blk_len;
		return;
	}

	for (j = i + 1; j < node_num; j++)
		unused_length +=
		musb_channel->dma_linklist[0][j].blk_len;
#else
	node_num = musb_channel->node_num;
	for (i = 0; i < node_num; i++) {
		if ((musb_channel->dma_linklist[i].addr +
			musb_channel->dma_linklist[i].blk_len -
			blk_len) == dma_addr)
			break;
	}

	if (i == node_num) {
		dev_err(musb->controller, "cannot find the dma_addr\n");
		urb->actual_length = 0;
		return;
	}

	if (i + 1 == node_num) {
		urb->actual_length += qh->segsize - blk_len;
		return;
	}

	for (j = i + 1; j < node_num; j++)
		unused_length += musb_channel->dma_linklist[j].blk_len;
#endif

	urb->actual_length += qh->segsize - unused_length - blk_len;
}


static void sprd_musb_urb_completion(struct musb *musb, u8 epnum, u8 is_in)
{
	struct musb_hw_ep *hw_ep;
	struct musb_qh *qh;
	struct dma_channel *channel;
	struct urb *urb;
	u32 blk_len = 0;
	u32 dma_addr = 0;

	hw_ep = &musb->endpoints[epnum];
	if (is_in) {
		channel = hw_ep->rx_channel;
		if (!channel)
			return;
		qh = hw_ep->in_qh;
	} else {
		channel = hw_ep->tx_channel;
		if (!channel)
			return;
		qh = hw_ep->out_qh;
	}
	if (!qh)
		return;

	urb = next_urb(qh);
	if (!urb)
		return;

	if (is_in) {
		blk_len = musb_readl(musb->mregs,
			MUSB_DMA_CHN_LEN(epnum + 15));
		dma_addr = musb_readl(musb->mregs,
			MUSB_DMA_CHN_ADDR(epnum + 15));
	} else {
		blk_len = musb_readl(musb->mregs,
			MUSB_DMA_CHN_LEN(epnum));
		dma_addr = musb_readl(musb->mregs,
			MUSB_DMA_CHN_ADDR(epnum));
	}

	blk_len = (blk_len & 0xffff0000) >> 16;
	sprd_calc_urb_actual_length(musb, urb, qh, channel, dma_addr, blk_len);

#if IS_ENABLED(CONFIG_USB_SPRD_LINKFIFO)
	if (qh->linkfifo) {
		channel->actual_len = urb->actual_length;
		musb_linknode_poplist(musb, (!is_in), epnum);
	} else if (usb_pipeisoc(urb->pipe)) {
		struct usb_iso_packet_descriptor *d;

		d = urb->iso_frame_desc + qh->iso_idx;
		channel->actual_len = qh->segsize - blk_len;
		d->status = 0;
		d->actual_length = channel->actual_len;
		if (++qh->iso_idx < urb->number_of_packets) {
			channel->status = MUSB_DMA_STATUS_FREE;
			d++;
			if (is_in)
				musb->hops.rx_dma_program(channel, musb, epnum, qh, urb,
					d->offset, d->length);
			return;
		}
	}
#else
	if (usb_pipeisoc(urb->pipe)) {
		struct usb_iso_packet_descriptor *d;

		d = urb->iso_frame_desc + qh->iso_idx;
		channel->actual_len = qh->segsize - blk_len;
		d->status = 0;
		d->actual_length = channel->actual_len;
		if (++qh->iso_idx < urb->number_of_packets) {
			channel->status = MUSB_DMA_STATUS_FREE;
			d++;
			if (is_in)
				musb->hops.rx_dma_program(channel, musb, epnum, qh, urb,
					d->offset, d->length);
			else
				musb->hops.tx_dma_program(musb->dma_controller, hw_ep,
					qh, urb, d->offset, d->length);
			return;
		}
	}
#endif
	dev_dbg(musb->controller, "%s epnum=%d transmit=%d urb %ps\n",
		__func__, epnum, urb->actual_length, urb);
	channel->status = MUSB_DMA_STATUS_FREE;

	musb->hops.advance_schedule(musb, urb, hw_ep, is_in);
}
#else
static void sprd_musb_urb_completion(struct musb *musb, u8 epnum, u8 is_in)
{
}
#endif

irqreturn_t sprd_dma_interrupt(struct musb *musb, u32 int_hsdma)
{
	void __iomem *mbase = musb->mregs;
	struct musb_ep *musb_ep;
	u8 bchannel = 0, epnum;
	u32 intr, int_dma;
	int i;
	int is_tx;

	int_dma = musb_readl(musb->mregs, MUSB_DMA_INTR_MASK_STATUS);

	/* start form ep1 ~ ep15, channel1 ~ channel30 */
	for (i = 1; i <= MUSB_DMA_CHANNELS; i++) {
		bchannel++;
		if ((int_hsdma & BIT(0)) != BIT(0)) {
			int_hsdma = int_hsdma >> 1;
			continue;
		}
		int_hsdma = int_hsdma >> 1;

		intr = musb_readl(mbase, MUSB_DMA_CHN_INTR(bchannel));
		dev_dbg(musb->controller, "%s is 0x%x, %d , %d\n",
				__func__, intr, bchannel, int_dma);

		if (intr & CHN_START_INT_MASK_STATUS) {
			dev_info(musb->controller, "DMA request is NULL\n");

			/* clear interrupt */
			intr |= CHN_START_INT_CLR;
			musb_writel(mbase, MUSB_DMA_CHN_INTR(bchannel), intr);
		}
		if (intr & CHN_CLEAR_INT_MASK_STATUS) {
			musb_writel(mbase, MUSB_DMA_CHN_PAUSE(bchannel), 0x0);
			musb_writel(mbase, MUSB_DMA_CHN_CFG(bchannel), 0x0);

			if (bchannel > 15) {
				is_tx = 0;
				epnum = bchannel - 15;
				musb_ep = &musb->endpoints[epnum].ep_out;
			} else {
				is_tx = 1;
				epnum = bchannel;
				musb_ep = &musb->endpoints[epnum].ep_in;
			}

			dev_dbg(musb->controller, "dma interrupt clear channel\n");
		}

		if (bchannel > 15) {
			if (intr & CHN_LLIST_INT_MASK_STATUS) {
				/* clear interrupt */
				intr |= CHN_LLIST_INT_CLR | CHN_START_INT_CLR |
					CHN_FRAG_INT_CLR | CHN_BLK_INT_CLR |
					CHN_USBRX_LAST_INT_CLR;
				musb_writel(mbase, MUSB_DMA_CHN_INTR(bchannel),
					intr);
				/* callback to give complete */
				if (musb->xceiv->otg->state == OTG_STATE_B_PERIPHERAL)
					sprd_musb_dma_completion(musb,
								 (bchannel - 15), 0);
				else
					sprd_musb_urb_completion(musb,
					(bchannel - 15), 1);
			}
		} else {
			if (intr & CHN_LLIST_INT_MASK_STATUS) {
				/* clear interrupt */
				intr |= CHN_LLIST_INT_CLR | CHN_START_INT_CLR |
					CHN_FRAG_INT_CLR | CHN_BLK_INT_CLR;
				musb_writel(mbase, MUSB_DMA_CHN_INTR(bchannel),
					intr);
				/* callback to give complete */
				if (musb->xceiv->otg->state == OTG_STATE_B_PERIPHERAL)
					sprd_musb_dma_completion(musb, bchannel, 1);
				else
					sprd_musb_urb_completion(musb, bchannel, 0);
			}
		}
	}

	return IRQ_HANDLED;
}
EXPORT_SYMBOL_GPL(sprd_dma_interrupt);

static void sprd_musb_free_nodes(struct sprd_musb_dma_controller *controller)
{
	struct sprd_musb_dma_channel *chp = NULL;
	struct musb *musb;
	int ch;
#if IS_ENABLED(CONFIG_USB_SPRD_LINKFIFO)
	int i = 0;
#endif

	if (!controller)
		return;

	musb = controller->private_data;

	/* start form ep1 ~ ep15, channel1 ~ channel30 */
	for (ch = 1; ch <= MUSB_DMA_CHANNELS; ch++) {
		chp = &controller->channel[ch];
#if IS_ENABLED(CONFIG_USB_SPRD_LINKFIFO)
		if (chp->dma_linklist[0]) {
			dma_free_coherent(musb->controller,
				sizeof(struct linklist_node_s) * LISTNODE_NUM * CHN_MAX_QUEUE_SIZE,
				chp->dma_linklist[0], chp->list_dma_addr[0]);

			for (i = 0; i < CHN_MAX_QUEUE_SIZE; i++) {
				chp->dma_linklist[i] = NULL;
				chp->list_dma_addr[i] = 0;
			}
		}
#else
		if (chp->dma_linklist) {
			dma_free_coherent(musb->controller,
				sizeof(struct linklist_node_s) * LISTNODE_NUM,
				chp->dma_linklist, chp->list_dma_addr);
			chp->dma_linklist = NULL;
			chp->list_dma_addr = 0;
		}
#endif
	}
}

void sprd_musb_dma_controller_destroy(struct dma_controller *c)
{
	struct sprd_musb_dma_controller *controller = container_of(c,
			struct sprd_musb_dma_controller, controller);

	sprd_dma_controller_stop(controller);
	sprd_musb_free_nodes(controller);
	kfree(controller);
}
EXPORT_SYMBOL_GPL(sprd_musb_dma_controller_destroy);

struct dma_controller *sprd_musb_dma_controller_create(struct musb *musb,
						       void __iomem *base)
{
	struct sprd_musb_dma_controller *controller;
	struct sprd_musb_dma_channel *chp = NULL;
	int	ch;
#if IS_ENABLED(CONFIG_USB_SPRD_LINKFIFO)
	int i = 0;
#endif

	controller = kzalloc(sizeof(*controller), GFP_KERNEL);
	if (!controller)
		return NULL;

	controller->private_data = musb;
	controller->base = base;

	controller->controller.channel_alloc = sprd_dma_channel_allocate;
	controller->controller.channel_release = sprd_dma_channel_release;
	controller->controller.channel_program = sprd_dma_channel_program;
	controller->controller.channel_abort = sprd_dma_channel_abort;
	init_waitqueue_head(&controller->wait);

	/* start form ep1 ~ ep15, channel1 ~ channel30 */
	for (ch = 1; ch <= MUSB_DMA_CHANNELS; ch++) {
		chp = &controller->channel[ch];
#if IS_ENABLED(CONFIG_USB_SPRD_LINKFIFO)
		chp->dma_linklist[0] = dma_alloc_coherent(musb->controller,
			sizeof(struct linklist_node_s) * LISTNODE_NUM * CHN_MAX_QUEUE_SIZE,
			&chp->list_dma_addr[0], GFP_KERNEL);

		if (!chp->dma_linklist[0]) {
			dev_err(musb->controller,
				"failed to allocate dma linklist %d %d\n", ch, i);
			sprd_musb_free_nodes(controller);
			kfree(controller);
			return NULL;
		}

		for (i = 0; i < CHN_MAX_QUEUE_SIZE - 1; i++) {
			chp->dma_linklist[i + 1] = chp->dma_linklist[i] +  LISTNODE_NUM;
			chp->list_dma_addr[i + 1] = chp->list_dma_addr[i] +
				sizeof(struct linklist_node_s) * LISTNODE_NUM;
		}
#else
		chp->dma_linklist = dma_alloc_coherent(musb->controller,
			sizeof(struct linklist_node_s) * LISTNODE_NUM,
			&chp->list_dma_addr, GFP_KERNEL);

		if (!chp->dma_linklist) {
			dev_err(musb->controller,
				 "failed to allocate dma linklist\n");
			sprd_musb_free_nodes(controller);
			kfree(controller);
			return NULL;
		}
#endif
	}

	return &controller->controller;
}
EXPORT_SYMBOL_GPL(sprd_musb_dma_controller_create);
