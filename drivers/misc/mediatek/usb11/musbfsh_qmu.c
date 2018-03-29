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

#ifdef MUSBFSH_QMU_SUPPORT

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/stat.h>

#include "musbfsh_core.h"
#include "musbfsh_host.h"
#include "musbfsh_hsdma.h"
/*#include "mtk_musb.h"*/
#include "musbfsh_qmu.h"
#include "mtk11_qmu.h"

void __iomem *musbfsh_qmu_base;
/* debug variable to check musbfsh_qmu_base issue */
void __iomem *musbfsh_qmu_base_2;

int musbfsh_qmu_init(struct musbfsh *musbfsh)
{
	/* set DMA channel 0 burst mode to boost QMU speed */
	musbfsh_writel(musbfsh->mregs, 0x204, musbfsh_readl(musbfsh->mregs, 0x204) | 0x600);
	musbfsh_writel((musbfsh->mregs + MUSBFSH_QISAR), 0x30, 0);

#ifdef CONFIG_OF
	musbfsh_qmu_base = (void __iomem *)(musbfsh->mregs + MUSBFSH_QMUBASE);
	/* debug variable to check musbfsh_qmu_base issue */
	musbfsh_qmu_base_2 = (void __iomem *)(musbfsh->mregs + MUSBFSH_QMUBASE);
#else
	musbfsh_qmu_base = (void __iomem *)(musbfsh->mregs + MUSBFSH_QMUBASE);
	/* debug variable to check musbfsh_qmu_base issue */
	musbfsh_qmu_base_2 = (void __iomem *)(musbfsh->mregs + MUSBFSH_QMUBASE);
#endif
	mb();

	if (mtk11_qmu_init_gpd_pool(musbfsh->controller)) {
		QMU_ERR("[QMU]mtk11_qmu_init_gpd_pool fail\n");
		return -1;
	}

	return 0;
}

void musbfsh_qmu_exit(struct musbfsh *musbfsh)
{
	mtk11_qmu_destroy_gpd_pool(musbfsh->controller);
}

void musbfsh_disable_q_all(struct musbfsh *musbfsh)
{
	u32 ep_num;

	QMU_WARN("disable_q_all\n");

	for (ep_num = 1; ep_num <= RXQ_NUM; ep_num++) {
		if (mtk11_is_qmu_enabled(ep_num, RXQ))
			mtk11_disable_q(musbfsh, ep_num, 1);
	}
	for (ep_num = 1; ep_num <= TXQ_NUM; ep_num++) {
		if (mtk11_is_qmu_enabled(ep_num, TXQ))
			mtk11_disable_q(musbfsh, ep_num, 0);
	}
}

irqreturn_t musbfsh_q_irq(struct musbfsh *musbfsh)
{
	irqreturn_t retval = IRQ_NONE;
	u32 wQmuVal = musbfsh->int_queue;
	u32 i;

	QMU_INFO("wQmuVal:%d\n", wQmuVal);
	for (i = 1; i <= MAX_QMU_EP; i++) {
		if (wQmuVal & DQMU_M_RX_DONE(i))
			h_mtk11_qmu_done_rx(musbfsh, i);

		if (wQmuVal & DQMU_M_TX_DONE(i))
			h_mtk11_qmu_done_tx(musbfsh, i);
	}
	mtk11_qmu_irq_err(musbfsh, wQmuVal);

	return retval;
}

void musbfsh_flush_qmu(u32 ep_num, u8 isRx)
{
	QMU_DBG("flush %s(%d)\n", isRx ? "RQ" : "TQ", ep_num);
	mtk11_qmu_stop(ep_num, isRx);
	mtk11_qmu_reset_gpd_pool(ep_num, isRx);
}

void musbfsh_restart_qmu(struct musbfsh *musbfsh, u32 ep_num, u8 isRx)
{
	QMU_DBG("restart %s(%d)\n", isRx ? "RQ" : "TQ", ep_num);
	mtk11_flush_ep_csr(musbfsh, ep_num, isRx);
	mtk11_qmu_enable(musbfsh, ep_num, isRx);
}

bool musbfsh_is_qmu_stop(u32 ep_num, u8 isRx)
{
	void __iomem *base = musbfsh_qmu_base;

	/* debug variable to check musbfsh_qmu_base issue */
	if (musbfsh_qmu_base != musbfsh_qmu_base_2) {
		QMU_WARN("musbfsh_qmu_base != musbfsh_qmu_base_2");
		QMU_WARN("musbfsh_qmu_base = %p, musbfsh_qmu_base_2=%p", musbfsh_qmu_base, musbfsh_qmu_base_2);
	}

	if (!isRx) {
		if (MGC_ReadQMU16(base, MGC_O_QMU_TQCSR(ep_num)) & DQMU_QUE_ACTIVE)
			return false;
		else
			return true;
	} else {
		if (MGC_ReadQMU16(base, MGC_O_QMU_RQCSR(ep_num)) & DQMU_QUE_ACTIVE)
			return false;
		else
			return true;
	}
}

void musbfsh_tx_zlp_qmu(struct musbfsh *musbfsh, u32 ep_num)
{
	/* sent ZLP through PIO */
	void __iomem *epio = musbfsh->endpoints[ep_num].regs;
	void __iomem *mbase = musbfsh->mregs;
	int cnt = 50; /* 50*200us, total 10 ms */
	int is_timeout = 1;
	u16 csr;

	QMU_WARN("TX ZLP direct sent\n");
	musbfsh_ep_select(mbase, ep_num);

	/* disable dma for pio */
	csr = musbfsh_readw(epio, MUSBFSH_TXCSR);
	csr &= ~MUSBFSH_TXCSR_DMAENAB;
	musbfsh_writew(epio, MUSBFSH_TXCSR, csr);

	/* TXPKTRDY */
	csr = musbfsh_readw(epio, MUSBFSH_TXCSR);
	csr |= MUSBFSH_TXCSR_TXPKTRDY;
	musbfsh_writew(epio, MUSBFSH_TXCSR, csr);

	/* wait ZLP sent */
	while (cnt--) {
		csr = musbfsh_readw(epio, MUSBFSH_TXCSR);
		if (!(csr & MUSBFSH_TXCSR_TXPKTRDY)) {
			is_timeout = 0;
			break;
		}
		udelay(200);
	}

	/* re-enable dma for qmu */
	csr = musbfsh_readw(epio, MUSBFSH_TXCSR);
	csr |= MUSBFSH_TXCSR_DMAENAB;
	musbfsh_writew(epio, MUSBFSH_TXCSR, csr);

	if (is_timeout)
		QMU_ERR("TX ZLP sent fail???\n");
	QMU_WARN("TX ZLP sent done\n");
}

int mtk11_kick_CmdQ(struct musbfsh *musbfsh, int isRx, struct musbfsh_qh *qh, struct urb *urb)
{
	void __iomem        *mbase = musbfsh->mregs;
	u16 intr_e = 0;
	struct musbfsh_hw_ep	*hw_ep = qh->hw_ep;
	void __iomem		*epio = hw_ep->regs;
	unsigned int offset = 0;
	u8 bIsIoc;
	u8 *pBuffer;
	u32 dwLength;
	u16 i;
	u32 gdp_free_count = 0;

	if (!urb) {
		QMU_WARN("!urb\n");
		return -1; /*KOBE : should we return a value */
	}

	if (!mtk11_is_qmu_enabled(hw_ep->epnum, isRx)) {
		INFO("! mtk_is_qmu_enabled\n");

		musbfsh_ep_select(mbase, hw_ep->epnum);
		mtk11_flush_ep_csr(musbfsh, hw_ep->epnum,  isRx);

		if (isRx) {
			INFO("isRX = 1\n");
			if (qh->type == USB_ENDPOINT_XFER_ISOC) {
				INFO("USB_ENDPOINT_XFER_ISOC\n");
				if (qh->hb_mult == 3)
					musbfsh_writew(epio, MUSBFSH_RXMAXP, qh->maxpacket|0x1000);
				else if (qh->hb_mult == 2)
					musbfsh_writew(epio, MUSBFSH_RXMAXP, qh->maxpacket|0x800);
				else
					musbfsh_writew(epio, MUSBFSH_RXMAXP, qh->maxpacket);
			} else {
				INFO("!! USB_ENDPOINT_XFER_ISOC\n");
				musbfsh_writew(epio, MUSBFSH_RXMAXP, qh->maxpacket);
			}

			musbfsh_writew(epio, MUSBFSH_RXCSR, MUSBFSH_RXCSR_DMAENAB);
			/*CC: speed */
			musbfsh_writeb(epio, MUSBFSH_RXTYPE, qh->type_reg);
			musbfsh_writeb(epio, MUSBFSH_RXINTERVAL, qh->intv_reg);

			if (musbfsh->is_multipoint) {
				INFO("is_multipoint\n");
				musbfsh_write_rxfunaddr(musbfsh->mregs, hw_ep->epnum, qh->addr_reg);
				musbfsh_write_rxhubaddr(musbfsh->mregs, hw_ep->epnum, qh->h_addr_reg);
				musbfsh_write_rxhubport(musbfsh->mregs, hw_ep->epnum, qh->h_port_reg);
			} else {
				INFO("!! is_multipoint\n");
				musbfsh_writeb(musbfsh->mregs, MUSBFSH_FADDR, qh->addr_reg);
			}

			/*turn off intrRx*/
			intr_e = musbfsh_readw(musbfsh->mregs, MUSBFSH_INTRRXE);
			intr_e = intr_e & (~(1<<(hw_ep->epnum)));
			musbfsh_writew(musbfsh->mregs, MUSBFSH_INTRRXE, intr_e);
		} else {
			musbfsh_writew(epio, MUSBFSH_TXMAXP, qh->maxpacket);
			musbfsh_writew(epio, MUSBFSH_TXCSR, MUSBFSH_TXCSR_DMAENAB);
			/*CC: speed?*/
			musbfsh_writeb(epio, MUSBFSH_TXTYPE, qh->type_reg);
			musbfsh_writeb(epio, MUSBFSH_TXINTERVAL, qh->intv_reg);

			if (musbfsh->is_multipoint) {
				INFO("is_multipoint\n");
				musbfsh_write_txfunaddr(mbase, hw_ep->epnum, qh->addr_reg);
				musbfsh_write_txhubaddr(mbase, hw_ep->epnum, qh->h_addr_reg);
				musbfsh_write_txhubport(mbase, hw_ep->epnum, qh->h_port_reg);
				/* FIXME if !epnum, do the same for RX ... */
			} else {
				INFO("!! is_multipoint\n");
				musbfsh_writeb(mbase, MUSBFSH_FADDR, qh->addr_reg);
			}
			/* turn off intrTx , but this will be revert by musbfsh_ep_program*/
			intr_e = musbfsh_readw(musbfsh->mregs, MUSBFSH_INTRTXE);
			intr_e = intr_e & (~(1<<hw_ep->epnum));
			musbfsh_writew(musbfsh->mregs, MUSBFSH_INTRTXE, intr_e);
		}

		INFO("mtk11_qmu_enable\n");
		mtk11_qmu_enable(musbfsh, hw_ep->epnum, isRx);
	}

	gdp_free_count = mtk11_qmu_free_gpd_count(isRx, hw_ep->epnum);
	if (qh->type == USB_ENDPOINT_XFER_ISOC) {
		INFO("USB_ENDPOINT_XFER_ISOC\n");
		pBuffer = (uint8_t *)urb->transfer_dma;

		if (gdp_free_count < urb->number_of_packets) {
			INFO("gdp_free_count:%d, number_of_packets:%d\n", gdp_free_count, urb->number_of_packets);
			BUG();
		}
		for (i = 0; i < urb->number_of_packets; i++) {
			urb->iso_frame_desc[i].status = 0;
			offset = urb->iso_frame_desc[i].offset;
			dwLength = urb->iso_frame_desc[i].length;
			/* If interrupt on complete ? */
			bIsIoc = (i == (urb->number_of_packets-1)) ? 1 : 0;
			INFO("mtk11_qmu_insert_task\n");
			mtk11_qmu_insert_task(hw_ep->epnum, isRx, pBuffer+offset, dwLength, 0, bIsIoc);

			mtk11_qmu_resume(hw_ep->epnum, isRx);
		}

		if (mtk11_host_qmu_max_active_isoc_gpd < mtk11_qmu_used_gpd_count(isRx, hw_ep->epnum))
			mtk11_host_qmu_max_active_isoc_gpd = mtk11_qmu_used_gpd_count(isRx, hw_ep->epnum);

		if (mtk11_host_qmu_max_number_of_pkts < urb->number_of_packets)
			mtk11_host_qmu_max_number_of_pkts = urb->number_of_packets;

		{
			static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 1);
			static int skip_cnt;

			if (__ratelimit(&ratelimit)) {
				INFO("max_isoc gpd:%d, max_pkts:%d, skip_cnt:%d\n",
						mtk11_host_qmu_max_active_isoc_gpd,
						mtk11_host_qmu_max_number_of_pkts,
						skip_cnt);
				skip_cnt = 0;
			} else
				skip_cnt++;
		}
	} else {
		/* Must be the bulk transfer type */
		QMU_WARN("non isoc\n");
		pBuffer = (uint8_t *)urb->transfer_dma;
		if (urb->transfer_buffer_length < QMU_RX_SPLIT_THRE) {
			if (gdp_free_count < 1) {
				INFO("gdp_free_count:%d, number_of_packets:%d\n",
						gdp_free_count, urb->number_of_packets);
				BUG();
			}
			INFO("urb->transfer_buffer_length : %d\n", urb->transfer_buffer_length);

			dwLength = urb->transfer_buffer_length;
			bIsIoc = 1;

			mtk11_qmu_insert_task(hw_ep->epnum, isRx, pBuffer+offset, dwLength, 0, bIsIoc);
			mtk11_qmu_resume(hw_ep->epnum, isRx);
		} else {
			/*reuse isoc urb->unmber_of_packets*/
			urb->number_of_packets =
				((urb->transfer_buffer_length) + QMU_RX_SPLIT_BLOCK_SIZE-1)/(QMU_RX_SPLIT_BLOCK_SIZE);
			if (gdp_free_count < urb->number_of_packets) {
				INFO("gdp_free_count:%d, number_of_packets:%d\n",
						gdp_free_count, urb->number_of_packets);
				BUG();
			}
			for (i = 0; i < urb->number_of_packets; i++) {
				offset = QMU_RX_SPLIT_BLOCK_SIZE*i;
				dwLength = QMU_RX_SPLIT_BLOCK_SIZE;

				/* If interrupt on complete ? */
				bIsIoc = (i == (urb->number_of_packets-1)) ? 1 : 0;
				dwLength = (i == (urb->number_of_packets-1)) ?
					((urb->transfer_buffer_length) % QMU_RX_SPLIT_BLOCK_SIZE) : dwLength;
				if (dwLength == 0)
					dwLength = QMU_RX_SPLIT_BLOCK_SIZE;

				mtk11_qmu_insert_task(hw_ep->epnum, isRx, pBuffer+offset, dwLength, 0, bIsIoc);
				mtk11_qmu_resume(hw_ep->epnum, isRx);
			}
		}
	}
	INFO("\n");
	return 0;
}
#endif
