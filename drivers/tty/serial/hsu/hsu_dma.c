/*
 * hsu_dma.c: driver for Intel High Speed UART device
 *
 * (C) Copyright 2014 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/slab.h>
#include <linux/circ_buf.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/dma-mapping.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/pm_runtime.h>
#include <linux/irq.h>
#include <linux/acpi.h>

#include "hsu.h"

/* Designware DMA ops */
static int dw_dma_init(struct uart_hsu_port *up)
{
	struct dw_dma_priv *dw_dma;
	struct hsu_dma_buffer *dbuf;
	dma_cap_mask_t mask;

	dw_dma = kzalloc(sizeof(*dw_dma), GFP_KERNEL);
	if (!dw_dma) {
		pr_warn("DW HSU: Can't alloc memory for dw_dm_priv\n");
		return -1;
	}

	up->dma_priv = dw_dma;

	/* Default slave configuration parameters */
	dw_dma->rxconf.direction	= DMA_DEV_TO_MEM;
	dw_dma->rxconf.src_addr_width	= DMA_SLAVE_BUSWIDTH_1_BYTE;
	dw_dma->rxconf.src_addr		= up->port.mapbase + UART_RX;
	dw_dma->rxconf.src_maxburst	= 8;
	dw_dma->rxconf.dst_maxburst	= 8;

	dw_dma->txconf.direction	= DMA_MEM_TO_DEV;
	dw_dma->txconf.dst_addr_width	= DMA_SLAVE_BUSWIDTH_1_BYTE;
	dw_dma->txconf.dst_addr		= up->port.mapbase + UART_TX;
	dw_dma->txconf.src_maxburst	= 8;
	dw_dma->txconf.dst_maxburst	= 8;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	/* Get a channel for RX */
	dw_dma->rxchan = dma_request_slave_channel_compat(mask,
						       NULL, NULL,
						       up->port.dev, "rx");
	if (!dw_dma->rxchan)
		return -ENODEV;

	dmaengine_slave_config(dw_dma->rxchan, &dw_dma->rxconf);

	/* Get a channel for TX */
	dw_dma->txchan = dma_request_slave_channel_compat(mask,
						       NULL, NULL,
						       up->port.dev, "tx");
	if (!dw_dma->txchan) {
		dma_release_channel(dw_dma->rxchan);
		return -ENODEV;
	}

	dmaengine_slave_config(dw_dma->txchan, &dw_dma->txconf);

	/* RX buffer */
	dbuf = &up->rxbuf;
	if (!dbuf->dma_size)
		dbuf->dma_size = PAGE_SIZE;

	dbuf->buf = dma_alloc_coherent(dw_dma->rxchan->device->dev,
			dbuf->dma_size, &dbuf->dma_addr, GFP_KERNEL);
	if (!dbuf->buf) {
		dma_release_channel(dw_dma->rxchan);
		dma_release_channel(dw_dma->txchan);
		return -ENOMEM;
	}

	/* TX buffer */
	dbuf = &up->txbuf;
	dbuf->dma_addr = dma_map_single(dw_dma->txchan->device->dev,
					up->port.state->xmit.buf,
					UART_XMIT_SIZE,
					DMA_TO_DEVICE);

	dw_dma->up = up;
	up->dma_inited = 1;

	dev_dbg_ratelimited(up->port.dev, "got both dma channels\n");
	return 0;
}

static int dw_dma_suspend(struct uart_hsu_port *up)
{
	struct dw_dma_priv *dw_dma = up->dma_priv;
	struct dma_chan *txchan;
	struct dma_chan *rxchan;

	if (!up->dma_inited)
		return 0;

	txchan = dw_dma->txchan;
	rxchan = dw_dma->rxchan;

	if (test_bit(flag_rx_on, &up->flags) ||
		test_bit(flag_rx_pending, &up->flags)) {
		dev_warn(up->dev, "ignore suspend for rx dma is running\n");
		return -EBUSY;
	}

	dmaengine_pause(dw_dma->txchan);
	dmaengine_pause(dw_dma->rxchan);

	return 0;
}

static int dw_dma_resume(struct uart_hsu_port *up)
{
	struct dw_dma_priv *dw_dma = up->dma_priv;

	if (!up->dma_inited)
		return 0;

	dmaengine_resume(dw_dma->txchan);
	dmaengine_resume(dw_dma->rxchan);

	return 0;
}


static int dw_dma_exit(struct uart_hsu_port *up)
{
	struct dw_dma_priv *dw_dma = up->dma_priv;
	struct hsu_dma_buffer *dbuf;

	if (!dw_dma)
		return -1;

	/* Release RX resources */
	dbuf = &up->rxbuf;
	dmaengine_terminate_all(dw_dma->rxchan);
	dma_free_coherent(dw_dma->rxchan->device->dev, dbuf->dma_size,
				dbuf->buf, dbuf->dma_addr);
	dma_release_channel(dw_dma->rxchan);
	dw_dma->rxchan = NULL;

	/* Release TX resources */
	dbuf = &up->txbuf;
	dmaengine_terminate_all(dw_dma->txchan);
	dma_unmap_single(dw_dma->txchan->device->dev, dbuf->dma_addr,
			 UART_XMIT_SIZE, DMA_TO_DEVICE);
	dma_release_channel(dw_dma->txchan);
	dw_dma->txchan = NULL;

	dev_dbg_ratelimited(up->port.dev, "dma channels released\n");

	up->dma_inited = 0;
	kfree(dw_dma);
	up->dma_priv = NULL;
	return 0;
}

static void dw_dma_tx_done(void *arg)
{
	struct dw_dma_priv *dw_dma = arg;
	struct uart_hsu_port *up = dw_dma->up;
	struct circ_buf *xmit = &up->port.state->xmit;
	struct hsu_dma_buffer *dbuf = &up->txbuf;
	unsigned long flags;

	dma_sync_single_for_cpu(dw_dma->txchan->device->dev, dbuf->dma_addr + xmit->tail,
				dbuf->dma_size, DMA_TO_DEVICE);

	xmit->tail += dbuf->dma_size;
	xmit->tail &= UART_XMIT_SIZE - 1;
	up->port.icount.tx += dbuf->dma_size;

	clear_bit(flag_tx_on, &up->flags);

	if (!uart_circ_empty(xmit) && !uart_tx_stopped(&up->port)) {
		spin_lock_irqsave(&up->port.lock, flags);
		serial_sched_cmd(up, qcmd_start_tx);
		spin_unlock_irqrestore(&up->port.lock, flags);
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&up->port);
}

static void dw_dma_start_tx(struct uart_hsu_port *up)
{
	struct dw_dma_priv *dw_dma = up->dma_priv;
	struct dma_async_tx_descriptor *desc = NULL;
	struct circ_buf *xmit = &up->port.state->xmit;
	struct hsu_dma_buffer *dbuf = &up->txbuf;

	if (uart_circ_empty(xmit) || uart_tx_stopped(&up->port)) {
		if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
			uart_write_wakeup(&up->port);
		return;
	}

	set_bit(flag_tx_on, &up->flags);

	dbuf->dma_size = CIRC_CNT_TO_END(xmit->head, xmit->tail,
						UART_XMIT_SIZE);

	desc = dmaengine_prep_slave_single(dw_dma->txchan,
					   dbuf->dma_addr + xmit->tail,
					   dbuf->dma_size, DMA_MEM_TO_DEV,
					   DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!desc)
		return;

	desc->callback = dw_dma_tx_done;
	desc->callback_param = dw_dma;

	dw_dma->tx_cookie = dmaengine_submit(desc);

	dma_sync_single_for_device(dw_dma->txchan->device->dev, dbuf->dma_addr + xmit->tail,
				dbuf->dma_size, DMA_TO_DEVICE);

	dma_async_issue_pending(dw_dma->txchan);
}

static void dw_dma_stop_tx(struct uart_hsu_port *up)
{
	struct dw_dma_priv *dw_dma = up->dma_priv;

	if (!test_bit(flag_tx_on, &up->flags))
		return;

	dmaengine_terminate_all(dw_dma->txchan);
}

static void dw_dma_rx_done(void *arg)
{
	struct dw_dma_priv *dw_dma = arg;
	struct uart_hsu_port *up = dw_dma->up;
	struct tty_port *tty_port = &up->port.state->port;
	struct hsu_dma_buffer *dbuf = &up->rxbuf;
	struct dma_tx_state	state;
	int count;
	unsigned long flags;

	dma_sync_single_for_cpu(dw_dma->rxchan->device->dev, dbuf->dma_addr,
				dbuf->dma_size, DMA_FROM_DEVICE);

	dmaengine_tx_status(dw_dma->rxchan, dw_dma->rx_cookie, &state);
	dmaengine_terminate_all(dw_dma->rxchan);

	count = dbuf->dma_size - state.residue;

	tty_insert_flip_string(tty_port, dbuf->buf, count);
	up->port.icount.rx += count;

	tty_flip_buffer_push(tty_port);

	clear_bit(flag_rx_on, &up->flags);

	spin_lock_irqsave(&up->port.lock, flags);
	if (test_bit(flag_rx_pending, &up->flags))
		serial_sched_cmd(up, qcmd_start_rx);
	spin_unlock_irqrestore(&up->port.lock, flags);
}


static void dw_dma_start_rx(struct uart_hsu_port *up)
{
	struct dma_async_tx_descriptor *desc = NULL;
	struct dw_dma_priv *dw_dma = up->dma_priv;
	struct hsu_dma_buffer *dbuf = &up->rxbuf;
	struct dma_tx_state state;
	int dma_status;

	if (test_and_set_bit(flag_rx_on, &up->flags)) {
		set_bit(flag_rx_pending, &up->flags);
		return;
	}

	dma_status = dmaengine_tx_status(dw_dma->rxchan,
					dw_dma->rx_cookie, &state);
	if (dma_status)
		return;

	desc = dmaengine_prep_slave_single(dw_dma->rxchan, dbuf->dma_addr,
					   dbuf->dma_size, DMA_DEV_TO_MEM,
					   DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!desc)
		return;

	desc->callback = dw_dma_rx_done;
	desc->callback_param = dw_dma;

	dw_dma->rx_cookie = dmaengine_submit(desc);

	dma_sync_single_for_device(dw_dma->rxchan->device->dev, dbuf->dma_addr,
				   dbuf->dma_size, DMA_FROM_DEVICE);

	dma_async_issue_pending(dw_dma->rxchan);
}

static void dw_dma_stop_rx(struct uart_hsu_port *up)
{
	struct dw_dma_priv *dw_dma = up->dma_priv;
	struct dma_tx_state	state;
	struct tty_port *tty_port = &up->port.state->port;
	struct hsu_dma_buffer *dbuf = &up->rxbuf;
	int count;

	if (!test_bit(flag_rx_on, &up->flags)) {
		clear_bit(flag_rx_pending, &up->flags);
		return;
	}

	dmaengine_pause(dw_dma->rxchan);

	dma_sync_single_for_cpu(dw_dma->rxchan->device->dev, dbuf->dma_addr,
				dbuf->dma_size, DMA_FROM_DEVICE);

	dmaengine_tx_status(dw_dma->rxchan, dw_dma->rx_cookie, &state);
	dmaengine_terminate_all(dw_dma->rxchan);

	count = dbuf->dma_size - state.residue;
	if (!count)
		goto exit;

	tty_insert_flip_string(tty_port, dbuf->buf, count);
	up->port.icount.rx += count;

	tty_flip_buffer_push(tty_port);

exit:
	clear_bit(flag_rx_on, &up->flags);
	clear_bit(flag_rx_pending, &up->flags);
}

struct hsu_dma_ops dw_dma_ops = {
	.init =		dw_dma_init,
	.exit =		dw_dma_exit,
	.suspend =	dw_dma_suspend,
	.resume	=	dw_dma_resume,
	.start_tx =	dw_dma_start_tx,
	.stop_tx =	dw_dma_stop_tx,
	.start_rx =	dw_dma_start_rx,
	.stop_rx =	dw_dma_stop_rx,
};

/* Intel DMA ops */

/* The buffer is already cache coherent */
void hsu_dma_start_rx_chan(struct hsu_dma_chan *rxc,
			struct hsu_dma_buffer *dbuf)
{
	dbuf->ofs = 0;

	chan_writel(rxc, HSU_CH_BSR, HSU_DMA_BSR);
	chan_writel(rxc, HSU_CH_MOTSR, HSU_DMA_MOTSR);

	chan_writel(rxc, HSU_CH_D0SAR, dbuf->dma_addr);
	chan_writel(rxc, HSU_CH_D0TSR, dbuf->dma_size);
	chan_writel(rxc, HSU_CH_DCR, 0x1 | (0x1 << 8)
					 | (0x1 << 16)
					 | (0x1 << 24)	/* timeout, Errata 1 */
					 );
	chan_writel(rxc, HSU_CH_CR, 0x3);
}

static int intel_dma_init(struct uart_hsu_port *up)
{
	struct hsu_dma_buffer *dbuf;
	struct circ_buf *xmit = &up->port.state->xmit;

	clear_bit(flag_tx_on, &up->flags);

	/* 1. Allocate the RX buffer */
	dbuf = &up->rxbuf;
	dbuf->buf = kzalloc(HSU_DMA_BUF_SIZE, GFP_KERNEL);
	if (!dbuf->buf) {
		up->use_dma = 0;
		dev_err(up->dev, "allocate DMA buffer failed!!\n");
		return -ENOMEM;
	}

	dbuf->dma_addr = dma_map_single(up->dev,
			dbuf->buf,
			HSU_DMA_BUF_SIZE,
			DMA_FROM_DEVICE);
	dbuf->dma_size = HSU_DMA_BUF_SIZE;

	/* 2. prepare teh TX buffer */
	dbuf = &up->txbuf;
	dbuf->buf = xmit->buf;
	dbuf->dma_addr = dma_map_single(up->dev,
			dbuf->buf,
			UART_XMIT_SIZE,
			DMA_TO_DEVICE);
	dbuf->dma_size = UART_XMIT_SIZE;
	dbuf->ofs = 0;

	/* This should not be changed all around */
	chan_writel(up->txc, HSU_CH_BSR, HSU_DMA_BSR);
	chan_writel(up->txc, HSU_CH_MOTSR, HSU_DMA_MOTSR);

	/* Start the RX channel right now */
	hsu_dma_start_rx_chan(up->rxc, &up->rxbuf);

	up->dma_inited = 1;
	return 0;
}

static int intel_dma_exit(struct uart_hsu_port *up)
{
	struct hsu_dma_buffer *dbuf;
	struct uart_port *port = &up->port;

	chan_writel(up->txc, HSU_CH_CR, 0x0);
	clear_bit(flag_tx_on, &up->flags);
	chan_writel(up->rxc, HSU_CH_CR, 0x2);

	/* Free and unmap rx dma buffer */
	dbuf = &up->rxbuf;
	dma_unmap_single(port->dev,
			dbuf->dma_addr,
			dbuf->dma_size,
			DMA_FROM_DEVICE);
	kfree(dbuf->buf);

	/* Next unmap tx dma buffer*/
	dbuf = &up->txbuf;
	dma_unmap_single(port->dev,
			dbuf->dma_addr,
			dbuf->dma_size,
			DMA_TO_DEVICE);

	up->dma_inited = 0;
	return 0;
}


static void intel_dma_start_tx(struct uart_hsu_port *up)
{
	struct circ_buf *xmit = &up->port.state->xmit;
	struct hsu_dma_buffer *dbuf = &up->txbuf;
	unsigned long flags;
	int count;

	spin_lock_irqsave(&up->port.lock, flags);
	chan_writel(up->txc, HSU_CH_CR, 0x0);
	while (chan_readl(up->txc, HSU_CH_CR))
		cpu_relax();
	clear_bit(flag_tx_on, &up->flags);
	if (dbuf->ofs) {
		u32 real = chan_readl(up->txc, HSU_CH_D0SAR) - up->tx_addr;

		/* we found in flow control case, TX irq came without sending
		 * all TX buffer
		 */
		if (real < dbuf->ofs)
			dbuf->ofs = real; /* adjust to real chars sent */

		/* Update the circ buf info */
		xmit->tail += dbuf->ofs;
		xmit->tail &= UART_XMIT_SIZE - 1;

		up->port.icount.tx += dbuf->ofs;
		dbuf->ofs = 0;
	}

	if (!uart_circ_empty(xmit) && !uart_tx_stopped(&up->port)) {
		set_bit(flag_tx_on, &up->flags);
		dma_sync_single_for_device(up->port.dev,
					   dbuf->dma_addr,
					   dbuf->dma_size,
					   DMA_TO_DEVICE);

		count = CIRC_CNT_TO_END(xmit->head, xmit->tail, UART_XMIT_SIZE);
		dbuf->ofs = count;

		/* Reprogram the channel */
		up->tx_addr = dbuf->dma_addr + xmit->tail;
		chan_writel(up->txc, HSU_CH_D0SAR, up->tx_addr);
		chan_writel(up->txc, HSU_CH_D0TSR, count);

		/* Reenable the channel */
		chan_writel(up->txc, HSU_CH_DCR, 0x1
						 | (0x1 << 8)
						 | (0x1 << 16));
		chan_writel(up->txc, HSU_CH_CR, 0x1);
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&up->port);

	spin_unlock_irqrestore(&up->port.lock, flags);
	return;
}

static void intel_dma_stop_tx(struct uart_hsu_port *up)
{
	chan_writel(up->txc, HSU_CH_CR, 0x0);
	return;
}

static void intel_dma_start_rx(struct uart_hsu_port *up)
{
	return;
}

static void intel_dma_stop_rx(struct uart_hsu_port *up)
{
	chan_writel(up->rxc, HSU_CH_CR, 0x2);
	return;
}

static void intel_dma_context_op(struct uart_hsu_port *up, int op)
{
	if (op == context_save) {
		up->txc->cr  = chan_readl(up->txc, HSU_CH_CR);
		up->txc->dcr = chan_readl(up->txc, HSU_CH_DCR);
		up->txc->sar = chan_readl(up->txc, HSU_CH_D0SAR);
		up->txc->tsr = chan_readl(up->txc, HSU_CH_D0TSR);

		up->rxc->cr  = chan_readl(up->rxc, HSU_CH_CR);
		up->rxc->dcr = chan_readl(up->rxc, HSU_CH_DCR);
		up->rxc->sar = chan_readl(up->rxc, HSU_CH_D0SAR);
		up->rxc->tsr = chan_readl(up->rxc, HSU_CH_D0TSR);
	} else {
		chan_writel(up->txc, HSU_CH_DCR, up->txc->dcr);
		chan_writel(up->txc, HSU_CH_D0SAR, up->txc->sar);
		chan_writel(up->txc, HSU_CH_D0TSR, up->txc->tsr);
		chan_writel(up->txc, HSU_CH_BSR, HSU_DMA_BSR);
		chan_writel(up->txc, HSU_CH_MOTSR, HSU_DMA_MOTSR);

		chan_writel(up->rxc, HSU_CH_DCR, up->rxc->dcr);
		chan_writel(up->rxc, HSU_CH_D0SAR, up->rxc->sar);
		chan_writel(up->rxc, HSU_CH_D0TSR, up->rxc->tsr);
		chan_writel(up->rxc, HSU_CH_BSR, HSU_DMA_BSR);
		chan_writel(up->rxc, HSU_CH_MOTSR, HSU_DMA_MOTSR);
	}
}


static int intel_dma_resume(struct uart_hsu_port *up)
{
	chan_writel(up->rxc, HSU_CH_CR, up->rxc_chcr_save);
	return 0;
}

static int intel_dma_suspend(struct uart_hsu_port *up)
{
	int loop = 100000;
	struct hsu_dma_chan *chan = up->rxc;

	up->rxc_chcr_save = chan_readl(up->rxc, HSU_CH_CR);

	if (test_bit(flag_startup, &up->flags)
			&& serial_in(up, UART_FOR) & 0x7F) {
		dev_err(up->dev, "ignore suspend for rx fifo\n");
		return -1;
	}

	if (chan_readl(up->txc, HSU_CH_CR)) {
		dev_info(up->dev, "ignore suspend for tx dma\n");
		return -1;
	}

	chan_writel(up->rxc, HSU_CH_CR, 0x2);
	while (--loop) {
		if (chan_readl(up->rxc, HSU_CH_CR) == 0x2)
			break;
		cpu_relax();
	}

	if (!loop) {
		dev_err(up->dev, "Can't stop rx dma\n");
		return -1;
	}

	if (chan_readl(chan, HSU_CH_D0SAR) - up->rxbuf.dma_addr) {
		dev_err(up->dev, "ignore suspend for dma pointer\n");
		return -1;
	}

	return 0;
}

struct hsu_dma_ops intel_hsu_dma_ops = {
	.init =		intel_dma_init,
	.exit =		intel_dma_exit,
	.suspend =	intel_dma_suspend,
	.resume	=	intel_dma_resume,
	.start_tx =	intel_dma_start_tx,
	.stop_tx =	intel_dma_stop_tx,
	.start_rx =	intel_dma_start_rx,
	.stop_rx =	intel_dma_stop_rx,
	.context_op =	intel_dma_context_op,
};
