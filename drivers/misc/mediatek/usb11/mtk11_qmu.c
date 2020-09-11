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

#ifdef CONFIG_MTK_MUSBFSH_QMU_SUPPORT
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/list.h>
#include "musbfsh_qmu.h"
#include "mtk11_qmu.h"
#include "musbfsh_host.h"

static struct _TGPD *Rx_gpd_head[MAX_QMU_EP + 1];
static struct _TGPD *Tx_gpd_head[MAX_QMU_EP + 1];
static struct _TGPD *Rx_gpd_end[MAX_QMU_EP + 1];
static struct _TGPD *Tx_gpd_end[MAX_QMU_EP + 1];
static struct _TGPD *Rx_gpd_last[MAX_QMU_EP + 1];
static struct _TGPD *Tx_gpd_last[MAX_QMU_EP + 1];
static struct _GPD_RANGE Rx_gpd_List[MAX_QMU_EP + 1];
static struct _GPD_RANGE Tx_gpd_List[MAX_QMU_EP + 1];
static u64 Rx_gpd_Offset[MAX_QMU_EP + 1];
static u64 Tx_gpd_Offset[MAX_QMU_EP + 1];
static u32 Rx_gpd_free_count[MAX_QMU_EP + 1];
static u32 Tx_gpd_free_count[MAX_QMU_EP + 1];
static u32 Rx_gpd_max_count[MAX_QMU_EP + 1];
static u32 Tx_gpd_max_count[MAX_QMU_EP + 1];


u32 mtk11_qmu_used_gpd_count(u8 isRx, u32 num)
{
	if (isRx)
		return (Rx_gpd_max_count[num] - 1) - Rx_gpd_free_count[num];
	else
		return (Tx_gpd_max_count[num] - 1) - Tx_gpd_free_count[num];
}

u32 mtk11_qmu_free_gpd_count(u8 isRx, u32 num)
{
	if (isRx)
		return Rx_gpd_free_count[num];
	else
		return Tx_gpd_free_count[num];
}

u8 mtk11_PDU_calcCksum(u8 *data, int len)
{
	u8 *uDataPtr, ckSum;
	int i;

	*(data + 1) = 0x0;
	uDataPtr = data;
	ckSum = 0;
	for (i = 0; i < len; i++)
		ckSum += *(uDataPtr + i);

	return 0xFF - ckSum;
}

static struct _TGPD *get_gpd(u8 isRx, u32 num)
{
	struct _TGPD *ptr;

	if (isRx) {
		ptr = Rx_gpd_List[num].pNext;
		Rx_gpd_List[num].pNext =
			(struct _TGPD *)((u8 *)(Rx_gpd_List[num].pNext)
			+  GPD_LEN_ALIGNED);

		if (Rx_gpd_List[num].pNext >= Rx_gpd_List[num].pEnd)
			Rx_gpd_List[num].pNext = Rx_gpd_List[num].pStart;
		Rx_gpd_free_count[num]--;
	} else {
		ptr = Tx_gpd_List[num].pNext;
		Tx_gpd_List[num].pNext =
			(struct _TGPD *)((u8 *)(Tx_gpd_List[num].pNext)
			+ GPD_LEN_ALIGNED);

		if (Tx_gpd_List[num].pNext >= Tx_gpd_List[num].pEnd)
			Tx_gpd_List[num].pNext = Tx_gpd_List[num].pStart;
		Tx_gpd_free_count[num]--;
	}
	return ptr;
}

static void gpd_ptr_align(u8 isRx, u32 num, struct _TGPD *ptr)
{
	if (isRx)
		Rx_gpd_List[num].pNext =
			(struct _TGPD *)((u8 *)(ptr) + GPD_LEN_ALIGNED);
	else
		Tx_gpd_List[num].pNext =
			(struct _TGPD *)((u8 *)(ptr) + GPD_LEN_ALIGNED);
}

static dma_addr_t gpd_virt_to_phys(void *vaddr, u8 isRx, u32 num)
{
	dma_addr_t paddr;
	u64 off;

	if (isRx)
		off = Rx_gpd_Offset[num];
	else
		off = Tx_gpd_Offset[num];

	paddr = (dma_addr_t)((u64)(unsigned long)vaddr - off);

	QMU_PR_INFO("%s[%d]phys=%p<->virt=%p\n",
		 ((isRx == RXQ) ? "RQ" : "TQ"), num, (void *)paddr, vaddr);

	return paddr;
}

static void *gpd_phys_to_virt(dma_addr_t paddr, u8 isRx, u32 num)
{
	void *vaddr;
	u64 off;

	if (isRx)
		off = Rx_gpd_Offset[num];
	else
		off = Tx_gpd_Offset[num];
	vaddr = (void *)(unsigned long)((u64)paddr + off);
	QMU_PR_INFO("%s[%d]phys=%p<->virt=%p\n",
		 ((isRx == RXQ) ? "RQ" : "TQ"), num, (void *)paddr, vaddr);

	return vaddr;
}

static void init_gpd_list(u8 isRx, int num,
	struct _TGPD *ptr, struct _TGPD *io_ptr, u32 size)
{
	if (isRx) {
		Rx_gpd_List[num].pStart = ptr;
		Rx_gpd_List[num].pEnd =
			(struct _TGPD *)((u8 *)(ptr + size)
			+ (GPD_EXT_LEN * size));
		Rx_gpd_Offset[num] =
			(u64)(unsigned long)ptr - (u64)(unsigned long)io_ptr;
		ptr++;
		Rx_gpd_List[num].pNext = (struct _TGPD *)((u8 *) ptr
			+ GPD_EXT_LEN);

		QMU_PR_INFO("Rx_gpd_List[%d].pStart=%p, pNext=%p, pEnd=%p\n",
			 num, Rx_gpd_List[num].pStart, Rx_gpd_List[num].pNext,
			 Rx_gpd_List[num].pEnd);
		QMU_PR_INFO("Rx_gpd_Offset[%d]=%p\n", num,
			(void *)(unsigned long)Rx_gpd_Offset[num]);
	} else {
		Tx_gpd_List[num].pStart = ptr;
		Tx_gpd_List[num].pEnd =
			(struct _TGPD *)((u8 *)(ptr + size)
			+ (GPD_EXT_LEN * size));
		Tx_gpd_Offset[num] =
			(u64)(unsigned long)ptr - (u64)(unsigned long)io_ptr;
		ptr++;
		Tx_gpd_List[num].pNext =
			(struct _TGPD *)((u8 *) ptr + GPD_EXT_LEN);

		QMU_PR_INFO("Tx_gpd_List[%d].pStart=%p, pNext=%p, pEnd=%p\n",
			 num, Tx_gpd_List[num].pStart, Tx_gpd_List[num].pNext,
			 Tx_gpd_List[num].pEnd);
		QMU_PR_INFO("Tx_gpd_Offset[%d]=%p\n", num,
			(void *)(unsigned long)Tx_gpd_Offset[num]);
	}
}

int mtk11_qmu_init_gpd_pool(struct device *dev)
{
	u32 i, size;
	struct _TGPD *ptr, *io_ptr;
	dma_addr_t dma_handle;
	u32 gpd_sz;

#ifdef MUSBFSH_QMU_LIMIT_SUPPORT
	for (i = 1; i <= MAX_QMU_EP; i++)
		Rx_gpd_max_count[i] =
			Tx_gpd_max_count[i] = mtk11_isoc_ep_gpd_count;
#else
	if (!mtk11_qmu_max_gpd_num)
		mtk11_qmu_max_gpd_num = DFT_MAX_GPD_NUM;

	for (i = 1; i < mtk11_isoc_ep_start_idx; i++)
		Rx_gpd_max_count[i] =
			Tx_gpd_max_count[i] = mtk11_qmu_max_gpd_num;

	for (i = mtk11_isoc_ep_start_idx; i <= MAX_QMU_EP; i++) {
		if (mtk11_isoc_ep_gpd_count > mtk11_qmu_max_gpd_num)
			Rx_gpd_max_count[i] =
				Tx_gpd_max_count[i] = mtk11_isoc_ep_gpd_count;
		else
			Rx_gpd_max_count[i] =
				Tx_gpd_max_count[i] = mtk11_qmu_max_gpd_num;
	}
#endif
	gpd_sz = (u32) (u64) sizeof(struct _TGPD);
	QMU_PR_INFO("sizeof(TGPD):%d\n", gpd_sz);
	if (gpd_sz != GPD_SZ)
		QMU_PR_WARN("ERR!!!, GPD SIZE != %d\n", GPD_SZ);

	for (i = 1; i <= RXQ_NUM; i++) {
		/* Allocate Rx GPD */
		size = GPD_LEN_ALIGNED * Rx_gpd_max_count[i];
		ptr = (struct _TGPD *)dma_alloc_coherent(dev, size,
				&dma_handle, GFP_KERNEL);
		if (!ptr)
			return -ENOMEM;
		memset(ptr, 0, size);
		io_ptr = (struct _TGPD *) (dma_handle);

		init_gpd_list(RXQ, i, ptr, io_ptr, Rx_gpd_max_count[i]);
		Rx_gpd_head[i] = ptr;
		QMU_PR_INFO("ALLOC RX GPD Head [%d] Virtual Mem=%p\n",
			i, Rx_gpd_head[i]);
		QMU_PR_INFO("ALLOC RX GPD Head [%d] DMA addr=%p\n",
			i, io_ptr);
		Rx_gpd_end[i] = Rx_gpd_last[i] = Rx_gpd_head[i];
		/* one must be for tail */
		Rx_gpd_free_count[i] = Rx_gpd_max_count[i] - 1;
		TGPD_CLR_FLAGS_HWO(Rx_gpd_end[i]);
		gpd_ptr_align(RXQ, i, Rx_gpd_end[i]);
		QMU_PR_INFO("RQSAR[%d]=%p\n", i,
			(void *)gpd_virt_to_phys(Rx_gpd_end[i], RXQ, i));
	}

	for (i = 1; i <= TXQ_NUM; i++) {
		/* Allocate Tx GPD */
		size = GPD_LEN_ALIGNED * Tx_gpd_max_count[i];
		ptr = (struct _TGPD *)dma_alloc_coherent(dev, size,
				&dma_handle, GFP_KERNEL);
		if (!ptr)
			return -ENOMEM;
		memset(ptr, 0, size);
		io_ptr = (struct _TGPD *) (dma_handle);

		init_gpd_list(TXQ, i, ptr, io_ptr, Tx_gpd_max_count[i]);
		Tx_gpd_head[i] = ptr;
		QMU_PR_INFO("ALLOC TX GPD Head [%d] Virtual Mem=%p\n",
			i, Tx_gpd_head[i]);
		QMU_PR_INFO("ALLOC TX GPD Head [%d] Virtual DMA addr=%p\n",
			i, io_ptr);
		Tx_gpd_end[i] = Tx_gpd_last[i] = Tx_gpd_head[i];
		/* one must be for tail */
		Tx_gpd_free_count[i] = Tx_gpd_max_count[i] - 1;
		TGPD_CLR_FLAGS_HWO(Tx_gpd_end[i]);
		gpd_ptr_align(TXQ, i, Tx_gpd_end[i]);
		QMU_PR_INFO("TQSAR[%d]=%p\n", i,
			(void *)gpd_virt_to_phys(Tx_gpd_end[i], TXQ, i));
	}

	return 0;
}

void mtk11_qmu_reset_gpd_pool(u32 ep_num, u8 isRx)
{
	u32 size;

	/* SW reset */
	if (isRx) {
		size = GPD_LEN_ALIGNED * Rx_gpd_max_count[ep_num];
		memset(Rx_gpd_head[ep_num], 0, size);
		Rx_gpd_end[ep_num] = Rx_gpd_last[ep_num] = Rx_gpd_head[ep_num];
		/* one must be for tail */
		Rx_gpd_free_count[ep_num] = Rx_gpd_max_count[ep_num] - 1;
		TGPD_CLR_FLAGS_HWO(Rx_gpd_end[ep_num]);
		gpd_ptr_align(isRx, ep_num, Rx_gpd_end[ep_num]);

	} else {
		size = GPD_LEN_ALIGNED * Tx_gpd_max_count[ep_num];
		memset(Tx_gpd_head[ep_num], 0, size);
		Tx_gpd_end[ep_num] = Tx_gpd_last[ep_num] = Tx_gpd_head[ep_num];
		/* one must be for tail */
		Tx_gpd_free_count[ep_num] = Tx_gpd_max_count[ep_num] - 1;
		TGPD_CLR_FLAGS_HWO(Tx_gpd_end[ep_num]);
		gpd_ptr_align(isRx, ep_num, Tx_gpd_end[ep_num]);
	}
}

void mtk11_qmu_destroy_gpd_pool(struct device *dev)
{
	int i;

	for (i = 1; i <= RXQ_NUM; i++) {
		dma_free_coherent(dev,
			GPD_LEN_ALIGNED * Rx_gpd_max_count[i],
			Rx_gpd_head[i],
			gpd_virt_to_phys(Rx_gpd_head[i], RXQ, i));
	}

	for (i = 1; i <= TXQ_NUM; i++) {
		dma_free_coherent(dev,
			GPD_LEN_ALIGNED * Tx_gpd_max_count[i],
			Tx_gpd_head[i],
			gpd_virt_to_phys(Tx_gpd_head[i], TXQ, i));
	}
}

static void prepare_rx_gpd(u8 *pBuf, u32 data_len, u8 ep_num, u8 isioc)
{
	struct _TGPD *gpd;

	/* get gpd from tail */
	gpd = Rx_gpd_end[ep_num];

	TGPD_SET_DATA(gpd, pBuf);
	TGPD_CLR_FORMAT_BDP(gpd);

	TGPD_SET_DataBUF_LEN(gpd, data_len);
	TGPD_SET_BUF_LEN(gpd, 0);

/* TGPD_CLR_FORMAT_BPS(gpd); */

	if (isioc)
		TGPD_SET_IOC(gpd);
	else
		TGPD_CLR_IOC(gpd);

	/* update gpd tail */
	Rx_gpd_end[ep_num] = get_gpd(RXQ, ep_num);
	QMU_PR_INFO("[RX]Rx_gpd_end[%d]=%p gpd=%p\n",
		ep_num, Rx_gpd_end[ep_num], gpd);
	memset(Rx_gpd_end[ep_num], 0, GPD_LEN_ALIGNED);
	TGPD_CLR_FLAGS_HWO(Rx_gpd_end[ep_num]);

	/* make sure struct ready before set to next */
	mb();
	TGPD_SET_NEXT(gpd, gpd_virt_to_phys(Rx_gpd_end[ep_num], RXQ, ep_num));

	TGPD_SET_CHKSUM_HWO(gpd, 16);

	/* make sure struct ready before HWO */
	mb();
	TGPD_SET_FLAGS_HWO(gpd);
}

static void prepare_tx_gpd(u8 *pBuf, u32 data_len, u8 ep_num, u8 zlp, u8 isioc)
{
	struct _TGPD *gpd;

	/* get gpd from tail */
	gpd = Tx_gpd_end[ep_num];

	TGPD_SET_DATA(gpd, pBuf);
	TGPD_CLR_FORMAT_BDP(gpd);

	TGPD_SET_BUF_LEN(gpd, data_len);
	TGPD_SET_EXT_LEN(gpd, 0);

	if (zlp)
		TGPD_SET_FORMAT_ZLP(gpd);
	else
		TGPD_CLR_FORMAT_ZLP(gpd);

	/* TGPD_CLR_FORMAT_BPS(gpd); */

	if (isioc)
		TGPD_SET_IOC(gpd);
	else
		TGPD_CLR_IOC(gpd);


	/* update gpd tail */
	Tx_gpd_end[ep_num] = get_gpd(TXQ, ep_num);
	QMU_PR_INFO("[TX]Tx_gpd_end[%d]=%p gpd=%p\n",
		ep_num, Tx_gpd_end[ep_num], gpd);
	memset(Tx_gpd_end[ep_num], 0, GPD_LEN_ALIGNED);
	TGPD_CLR_FLAGS_HWO(Tx_gpd_end[ep_num]);


	/* make sure struct ready before set to next */
	mb();
	TGPD_SET_NEXT(gpd, gpd_virt_to_phys(Tx_gpd_end[ep_num], TXQ, ep_num));

	TGPD_SET_CHKSUM_HWO(gpd, 16);

	/* make sure struct ready before HWO */
	mb();
	TGPD_SET_FLAGS_HWO(gpd);

}

bool mtk11_is_qmu_enabled(u8 ep_num, u8 isRx)
{
	void __iomem *base = musbfsh_qmu_base;

	if (isRx) {
		if (MGC_ReadQUCS32(base, MGC_O_QUCS_USBGCSR) &
			(USB_QMU_Rx_EN(ep_num)))
			return true;
	} else {
		if (MGC_ReadQUCS32(base, MGC_O_QUCS_USBGCSR) &
			(USB_QMU_Tx_EN(ep_num)))
			return true;
	}
	return false;
}

void mtk11_qmu_enable(struct musbfsh *musbfsh, u8 ep_num, u8 isRx)
{
	struct musbfsh_hw_ep *hw_ep;
	u32 QCR;
	void __iomem *base = musbfsh_qmu_base;
	void __iomem *mbase = musbfsh->mregs;
	void __iomem *epio;
	u16 csr = 0;
	u16 intr_e = 0;

	epio = musbfsh->endpoints[ep_num].regs;
	hw_ep = &musbfsh->endpoints[ep_num];
	musbfsh_ep_select(mbase, ep_num);

	if (isRx) {
		QMU_PR_WARN("enable RQ(%d)\n", ep_num);

		/* enable dma */
		csr |= MUSBFSH_RXCSR_DMAENAB;

		/* check ISOC */
		if (hw_ep->type == USB_ENDPOINT_XFER_ISOC)
			csr |= MUSBFSH_RXCSR_P_ISO;
		musbfsh_writew(epio, MUSBFSH_RXCSR, csr);

		/* turn off intrRx */
		intr_e = musbfsh_readw(mbase, MUSBFSH_INTRRXE);
		intr_e = intr_e & (~(1 << (ep_num)));
		musbfsh_writew(mbase, MUSBFSH_INTRRXE, intr_e);

		/* set 1st gpd and enable */
		MGC_WriteQMU32(base, MGC_O_QMU_RQSAR(ep_num),
			gpd_virt_to_phys(Rx_gpd_end[ep_num], RXQ, ep_num));
		MGC_WriteQUCS32(base, MGC_O_QUCS_USBGCSR,
			MGC_ReadQUCS32(base,
				MGC_O_QUCS_USBGCSR) | (USB_QMU_Rx_EN(ep_num)));

#ifdef CFG_CS_CHECK
		QCR = MGC_ReadQMU32(base, MGC_O_QMU_QCR0);
		MGC_WriteQMU32(base, MGC_O_QMU_QCR0,
			QCR | DQMU_RQCS_EN(ep_num));
#endif

#ifdef CFG_RX_ZLP_EN
		QCR = MGC_ReadQMU32(base, MGC_O_QMU_QCR3);
		MGC_WriteQMU32(base, MGC_O_QMU_QCR3, QCR | DQMU_RX_ZLP(ep_num));
#endif

#ifdef CFG_RX_COZ_EN
		QCR = MGC_ReadQMU32(base, MGC_O_QMU_QCR3);
		MGC_WriteQMU32(base, MGC_O_QMU_QCR3, QCR | DQMU_RX_COZ(ep_num));
#endif

		MGC_WriteQIRQ32(base, MGC_O_QIRQ_QIMCR,
				DQMU_M_RX_DONE(ep_num) |
				DQMU_M_RQ_EMPTY | DQMU_M_RXQ_ERR |
				DQMU_M_RXEP_ERR);


#ifdef CFG_EMPTY_CHECK
		MGC_WriteQIRQ32(base, MGC_O_QIRQ_REPEMPMCR,
		DQMU_M_RX_EMPTY(ep_num));
#else
		MGC_WriteQIRQ32(base, MGC_O_QIRQ_QIMSR, DQMU_M_RQ_EMPTY);
#endif

		QCR = DQMU_M_RX_LEN_ERR(ep_num);
#ifdef CFG_CS_CHECK
		QCR |= DQMU_M_RX_GPDCS_ERR(ep_num);
#endif

#ifdef CFG_RX_ZLP_EN
		QCR |= DQMU_M_RX_ZLP_ERR(ep_num);
#endif
		MGC_WriteQIRQ32(base, MGC_O_QIRQ_RQEIMCR, QCR);


		MGC_WriteQIRQ32(base, MGC_O_QIRQ_REPEIMCR,
			DQMU_M_RX_EP_ERR(ep_num));

		mb(); /* */
		/* qmu start */
		MGC_WriteQMU32(base, MGC_O_QMU_RQCSR(ep_num), DQMU_QUE_START);

	} else {
		QMU_PR_WARN("enable TQ(%d)\n", ep_num);

		/* enable dma */
		csr |= MUSBFSH_TXCSR_DMAENAB;

		/* check ISOC */
		if (hw_ep->type == USB_ENDPOINT_XFER_ISOC)
			csr |= MUSBFSH_TXCSR_P_ISO;
		musbfsh_writew(epio, MUSBFSH_TXCSR, csr);

		/* turn off intrTx */
		intr_e = musbfsh_readw(mbase, MUSBFSH_INTRTXE);
		intr_e = intr_e & (~(1 << ep_num));
		musbfsh_writew(mbase, MUSBFSH_INTRTXE, intr_e);

		/* set 1st gpd and enable */
		MGC_WriteQMU32(base, MGC_O_QMU_TQSAR(ep_num),
	       gpd_virt_to_phys(Tx_gpd_end[ep_num], TXQ, ep_num));
		MGC_WriteQUCS32(base, MGC_O_QUCS_USBGCSR,
			MGC_ReadQUCS32(base,
				MGC_O_QUCS_USBGCSR) | (USB_QMU_Tx_EN(ep_num)));

#ifdef CFG_CS_CHECK
		QCR = MGC_ReadQMU32(base, MGC_O_QMU_QCR0);
		MGC_WriteQMU32(base, MGC_O_QMU_QCR0,
			QCR | DQMU_TQCS_EN(ep_num));
#endif

#if (TXZLP == HW_MODE)
		QCR = MGC_ReadQMU32(base, MGC_O_QMU_QCR2);
		MGC_WriteQMU32(base, MGC_O_QMU_QCR2, QCR | DQMU_TX_ZLP(ep_num));
#elif (TXZLP == GPD_MODE)
		QCR = MGC_ReadQMU32(base, MGC_O_QMU_QCR2);
		MGC_WriteQMU32(base, MGC_O_QMU_QCR2, QCR |
			DQMU_TX_MULTIPLE(ep_num));
#endif

		MGC_WriteQIRQ32(base, MGC_O_QIRQ_QIMCR,
			DQMU_M_TX_DONE(ep_num) |
			DQMU_M_TQ_EMPTY | DQMU_M_TXQ_ERR |
			DQMU_M_TXEP_ERR);

#ifdef CFG_EMPTY_CHECK
		MGC_WriteQIRQ32(base, MGC_O_QIRQ_TEPEMPMCR,
			DQMU_M_TX_EMPTY(ep_num));
#else
		MGC_WriteQIRQ32(base, MGC_O_QIRQ_QIMSR, DQMU_M_TQ_EMPTY);
#endif

		QCR = DQMU_M_TX_LEN_ERR(ep_num);
#ifdef CFG_CS_CHECK
		QCR |= DQMU_M_TX_GPDCS_ERR(ep_num) | DQMU_M_TX_BDCS_ERR(ep_num);
#endif
		MGC_WriteQIRQ32(base, MGC_O_QIRQ_TQEIMCR, QCR);

		MGC_WriteQIRQ32(base,
			MGC_O_QIRQ_TEPEIMCR, DQMU_M_TX_EP_ERR(ep_num));

		mb(); /* */
		/* qmu start */
		MGC_WriteQMU32(base, MGC_O_QMU_TQCSR(ep_num), DQMU_QUE_START);
	}
}

void mtk11_qmu_stop(u8 ep_num, u8 isRx)
{
	void __iomem *base = musbfsh_qmu_base;

	if (!isRx) {
		if (MGC_ReadQMU16(base, MGC_O_QMU_TQCSR(ep_num)) &
			DQMU_QUE_ACTIVE) {
			MGC_WriteQMU32(base, MGC_O_QMU_TQCSR(ep_num),
				DQMU_QUE_STOP);
			QMU_PR_WARN("Stop TQ %d\n", ep_num);
		} else {
			QMU_PR_WARN("TQ %d already inactive\n", ep_num);
		}
	} else {
		if (MGC_ReadQMU16(base, MGC_O_QMU_RQCSR(ep_num)) &
			DQMU_QUE_ACTIVE) {
			MGC_WriteQMU32(base, MGC_O_QMU_RQCSR(ep_num),
				DQMU_QUE_STOP);
			QMU_PR_WARN("Stop RQ %d\n", ep_num);
		} else {
			QMU_PR_WARN("RQ %d already inactive\n", ep_num);
		}
	}
}

static void mtk11_qmu_disable(u8 ep_num, u8 isRx)
{
	u32 QCR;
	void __iomem *base = musbfsh_qmu_base;

	QMU_PR_WARN("disable %s(%d)\n", isRx ? "RQ" : "TQ", ep_num);

	mtk11_qmu_stop(ep_num, isRx);
	if (isRx) {
		/* / clear Queue start address */
		MGC_WriteQMU32(base, MGC_O_QMU_RQSAR(ep_num), 0);

		MGC_WriteQUCS32(base, MGC_O_QUCS_USBGCSR,
			MGC_ReadQUCS32(base, MGC_O_QUCS_USBGCSR)
			& (~(USB_QMU_Rx_EN(ep_num))));

		QCR = MGC_ReadQMU32(base, MGC_O_QMU_QCR0);
		MGC_WriteQMU32(base, MGC_O_QMU_QCR0,
			QCR & (~(DQMU_RQCS_EN(ep_num))));
		QCR = MGC_ReadQMU32(base, MGC_O_QMU_QCR3);
		MGC_WriteQMU32(base, MGC_O_QMU_QCR3,
			QCR & (~(DQMU_RX_ZLP(ep_num))));

		MGC_WriteQIRQ32(base, MGC_O_QIRQ_QIMSR, DQMU_M_RX_DONE(ep_num));
		MGC_WriteQIRQ32(base, MGC_O_QIRQ_REPEMPMSR,
			DQMU_M_RX_EMPTY(ep_num));
		MGC_WriteQIRQ32(base, MGC_O_QIRQ_RQEIMSR,
			DQMU_M_RX_LEN_ERR(ep_num) |
			DQMU_M_RX_GPDCS_ERR(ep_num) |
			DQMU_M_RX_ZLP_ERR(ep_num));
		MGC_WriteQIRQ32(base, MGC_O_QIRQ_REPEIMSR,
			DQMU_M_RX_EP_ERR(ep_num));
	} else {
		/* / clear Queue start address */
		MGC_WriteQMU32(base, MGC_O_QMU_TQSAR(ep_num), 0);

		MGC_WriteQUCS32(base, MGC_O_QUCS_USBGCSR,
			MGC_ReadQUCS32(base, MGC_O_QUCS_USBGCSR)
			& (~(USB_QMU_Tx_EN(ep_num))));

		QCR = MGC_ReadQMU32(base, MGC_O_QMU_QCR0);
		MGC_WriteQMU32(base, MGC_O_QMU_QCR0,
			QCR & (~(DQMU_TQCS_EN(ep_num))));
		QCR = MGC_ReadQMU32(base, MGC_O_QMU_QCR2);
		MGC_WriteQMU32(base, MGC_O_QMU_QCR2,
			QCR & (~(DQMU_TX_ZLP(ep_num))));

		MGC_WriteQIRQ32(base, MGC_O_QIRQ_QIMSR, DQMU_M_TX_DONE(ep_num));
		MGC_WriteQIRQ32(base, MGC_O_QIRQ_TEPEMPMSR,
			DQMU_M_TX_EMPTY(ep_num));
		MGC_WriteQIRQ32(base, MGC_O_QIRQ_TQEIMSR,
			DQMU_M_TX_LEN_ERR(ep_num) |
			DQMU_M_TX_GPDCS_ERR(ep_num) |
			DQMU_M_TX_BDCS_ERR(ep_num));
		MGC_WriteQIRQ32(base, MGC_O_QIRQ_TEPEIMSR,
			DQMU_M_TX_EP_ERR(ep_num));
	}
}

void mtk11_qmu_insert_task(u8 ep_num, u8 isRx, u8 *buf,
	u32 length, u8 zlp, u8 isioc)
{
	if (isRx) /* rx don't care zlp input */
		prepare_rx_gpd(buf, length, ep_num, isioc);
	else
		prepare_tx_gpd(buf, length, ep_num, zlp, isioc);
}

void mtk11_qmu_resume(u8 ep_num, u8 isRx)
{
	void __iomem *base = musbfsh_qmu_base;

	if (!isRx) {
		MGC_WriteQMU32(base, MGC_O_QMU_TQCSR(ep_num), DQMU_QUE_RESUME);
		if (!MGC_ReadQMU32(base, MGC_O_QMU_TQCSR(ep_num))) {
			QMU_PR_WARN("TQCSR[%d]=%x\n", ep_num,
				MGC_ReadQMU32(base, MGC_O_QMU_TQCSR(ep_num)));
			MGC_WriteQMU32(base, MGC_O_QMU_TQCSR(ep_num),
				DQMU_QUE_RESUME);
			QMU_PR_WARN("TQCSR[%d]=%x\n", ep_num,
				MGC_ReadQMU32(base, MGC_O_QMU_TQCSR(ep_num)));
		}
	} else {
		MGC_WriteQMU32(base, MGC_O_QMU_RQCSR(ep_num), DQMU_QUE_RESUME);
		if (!MGC_ReadQMU32(base, MGC_O_QMU_RQCSR(ep_num))) {
			QMU_PR_WARN("RQCSR[%d]=%x\n", ep_num,
				MGC_ReadQMU32(base, MGC_O_QMU_RQCSR(ep_num)));
			MGC_WriteQMU32(base, MGC_O_QMU_RQCSR(ep_num),
				DQMU_QUE_RESUME);
			QMU_PR_WARN("RQCSR[%d]=%x\n", ep_num,
				MGC_ReadQMU32(base, MGC_O_QMU_RQCSR(ep_num)));
		}
	}
}

void mtk11_flush_ep_csr(struct musbfsh *musbfsh, u8 ep_num, u8 isRx)
{
	void __iomem *mbase = musbfsh->mregs;
	struct musbfsh_hw_ep *hw_ep = musbfsh->endpoints + ep_num;
	void __iomem *epio = hw_ep->regs;
	u16 csr, wCsr;

	if (epio == NULL)
		QMU_PR_WARN("epio == NULL\n");

	if (isRx) {
		csr = musbfsh_readw(epio, MUSBFSH_RXCSR);
		csr |= MUSBFSH_RXCSR_FLUSHFIFO | MUSBFSH_RXCSR_RXPKTRDY;
		csr &= ~MUSBFSH_RXCSR_H_REQPKT;

		/* write 2x to allow double buffering */
		/* CC: see if some check is necessary */
		musbfsh_writew(epio, MUSBFSH_RXCSR, csr);
		/*musbfsh_writew(epio, MUSBFSH_RXCSR,
		 *csr | MUSBFSH_RXCSR_CLRDATATOG);
		 */
	} else {
		csr = musbfsh_readw(epio, MUSBFSH_TXCSR);
		if (csr & MUSBFSH_TXCSR_TXPKTRDY) {
			wCsr = csr | MUSBFSH_TXCSR_FLUSHFIFO |
				MUSBFSH_TXCSR_TXPKTRDY;
			musbfsh_writew(epio, MUSBFSH_TXCSR, wCsr);
		}

		csr |= MUSBFSH_TXCSR_FLUSHFIFO & ~MUSBFSH_TXCSR_TXPKTRDY;
		musbfsh_writew(epio, MUSBFSH_TXCSR, csr);
		/*musbfsh_writew(epio, MUSBFSH_TXCSR,
		 * csr | MUSBFSH_TXCSR_CLRDATATOG);
		 */
		/* CC: why is this special? */
		musbfsh_writew(mbase, MUSBFSH_INTRTX, 1 << ep_num);
	}
}

void mtk11_disable_q(struct musbfsh *musbfsh, u8 ep_num, u8 isRx)
{
	void __iomem *mbase = musbfsh->mregs;
	struct musbfsh_hw_ep *hw_ep = musbfsh->endpoints + ep_num;
	void __iomem *epio = hw_ep->regs;
	u16 csr;

	mtk11_qmu_disable(ep_num, isRx);
	mtk11_qmu_reset_gpd_pool(ep_num, isRx);

	musbfsh_ep_select(mbase, ep_num);
	if (isRx) {
		csr = musbfsh_readw(epio, MUSBFSH_RXCSR);
		csr &= ~MUSBFSH_RXCSR_DMAENAB;
		musbfsh_writew(epio, MUSBFSH_RXCSR, csr);
		mtk11_flush_ep_csr(musbfsh, ep_num, isRx);
	} else {
		csr = musbfsh_readw(epio, MUSBFSH_TXCSR);
		csr &= ~MUSBFSH_TXCSR_DMAENAB;
		musbfsh_writew(epio, MUSBFSH_TXCSR, csr);
		mtk11_flush_ep_csr(musbfsh, ep_num, isRx);
	}
}

void mtk11_qmu_host_iso_rx_err_info(struct musbfsh *musbfsh, u8 epnum)
{
	u16 rx_csr;
	struct musbfsh_hw_ep *hw_ep = musbfsh->endpoints + epnum;
	void __iomem *epio = hw_ep->regs;
	void __iomem *mbase = musbfsh->mregs;

	musbfsh_ep_select(mbase, epnum);
	rx_csr = musbfsh_readw(epio, MUSBFSH_RXCSR);

	WARNING("<== hw %d rxcsr %04x\n", epnum, rx_csr);

	/* check for errors, concurrent stall & unlink
	 * is not really handled yet!
	 */
	if (rx_csr & MUSBFSH_RXCSR_H_RXSTALL) {
		WARNING("RX end %d STALL\n", epnum);
	} else if (rx_csr & MUSBFSH_RXCSR_H_ERROR) {
		WARNING("end %d RX proto error,rxtoggle=0x%x\n", epnum,
		    musbfsh_readl(mbase, MUSBFSH_RXTOG));
	} else if (rx_csr & MUSBFSH_RXCSR_DATAERROR) {
		WARNING("RX end %d ISO data error\n", epnum);
	} else if (rx_csr & MUSBFSH_RXCSR_INCOMPRX) {
		WARNING("end %d incomplete ISO packet RX\n", epnum);
	}
}

void mtk11_qmu_host_iso_tx_err_info(struct musbfsh *musbfsh, u8 epnum)
{
	u16 tx_csr;
	struct musbfsh_hw_ep *hw_ep = musbfsh->endpoints + epnum;
	void __iomem *epio = hw_ep->regs;
	void __iomem *mbase = musbfsh->mregs;

	musbfsh_ep_select(mbase, epnum);
	tx_csr = musbfsh_readw(epio, MUSBFSH_TXCSR);
	WARNING("OUT/TX%d end, csr %04x\n", epnum, tx_csr);
	/* check for errors */
	if (tx_csr & MUSBFSH_TXCSR_H_RXSTALL) {
		/* dma was disabled, fifo flushed */
		WARNING("TX end %d stall\n", epnum);
	} else if (tx_csr & MUSBFSH_TXCSR_H_ERROR) {
		/* (NON-ISO) dma was disabled, fifo flushed */
		WARNING("TX 3strikes on ep=%d\n", epnum);
	} else if (tx_csr & MUSBFSH_TXCSR_H_NAKTIMEOUT) {
		WARNING("TX end=%d device not responding\n", epnum);
	}
}

void mtk11_qmu_host_rx_err(struct musbfsh *musbfsh, u8 epnum)
{
	struct urb *urb;
	u16 rx_csr, val;
	struct musbfsh_hw_ep *hw_ep = musbfsh->endpoints + epnum;
	void __iomem *epio = hw_ep->regs;
	struct musbfsh_qh *qh = hw_ep->in_qh;
	bool done = false;
	u32 status = 0;
	void __iomem *mbase = musbfsh->mregs;

	musbfsh_ep_select(mbase, epnum);
	rx_csr = musbfsh_readw(epio, MUSBFSH_RXCSR);
	val = rx_csr;

	if (!qh) {
		WARNING("!QH for ep %d\n", epnum);
		goto finished;
	}

	urb = next_urb(qh);
	status = 0;

	if (unlikely(!urb)) {
		/* REVISIT -- THIS SHOULD NEVER HAPPEN ... but, at least
		 * usbtest #11 (unlinks) triggers it regularly, sometimes
		 * with fifo full.  (Only with DMA??)
		 */
		WARNING("BOGUS RX%d ready, csr %04x, count %d\n", epnum, val,
		    musbfsh_readw(epio, MUSBFSH_RXCOUNT));
		musbfsh_h_flush_rxfifo(hw_ep, 0);
		goto finished;
	}

	WARNING("<== hw %d rxcsr %04x, urb actual %d\n",
	    epnum, rx_csr, urb->actual_length);

	/* check for errors, concurrent stall & unlink
	 *is not really handled yet!
	 */
	if (rx_csr & MUSBFSH_RXCSR_H_RXSTALL) {
		WARNING("RX end %d STALL\n", epnum);

		/* handle stall in MAC */
		rx_csr &= ~MUSBFSH_RXCSR_H_RXSTALL;
		musbfsh_writew(epio, MUSBFSH_RXCSR, rx_csr);

		/* stall; record URB status */
		status = -EPIPE;

	} else if (rx_csr & MUSBFSH_RXCSR_H_ERROR) {
		WARNING("end %d RX proto error,rxtoggle=0x%x\n", epnum,
		    musbfsh_readl(mbase, MUSBFSH_RXTOG));

		status = -EPROTO;
		musbfsh_writeb(epio, MUSBFSH_RXINTERVAL, 0);

	} else if (rx_csr & MUSBFSH_RXCSR_DATAERROR) {

		WARNING("RX end %d ISO data error\n", epnum);
	} else if (rx_csr & MUSBFSH_RXCSR_INCOMPRX) {
		WARNING("end %d incomplete ISO packet RX\n", epnum);
		status = -EPROTO;
	}

	/* faults abort the transfer */
	if (status) {
		musbfsh_h_flush_rxfifo(hw_ep, 0);
		musbfsh_writeb(epio, MUSBFSH_RXINTERVAL, 0);
		done = true;
	}

	if (done) {
		if (urb->status == -EINPROGRESS)
			urb->status = status;
		musbfsh_advance_schedule(musbfsh, urb, hw_ep, USB_DIR_IN);
	}

finished:
	{
		/* must use static string for AEE usage */
		static char string[100];

		sprintf(string, "USB11_HOST, RXQ<%d> ERR, CSR:%x", epnum, val);
		QMU_PR_WARN("%s\n", string);
	}
}

void mtk11_qmu_host_tx_err(struct musbfsh *musbfsh, u8 epnum)
{
	struct urb *urb;
	u16 tx_csr, val;
	struct musbfsh_hw_ep *hw_ep = musbfsh->endpoints + epnum;
	void __iomem *epio = hw_ep->regs;
	struct musbfsh_qh *qh = hw_ep->out_qh;
	bool done = false;
	u32 status = 0;
	void __iomem *mbase = musbfsh->mregs;

	musbfsh_ep_select(mbase, epnum);
	tx_csr = musbfsh_readw(epio, MUSBFSH_TXCSR);
	val = tx_csr;

	if (!qh) {
		WARNING("!QH for ep %d\n", epnum);
		goto finished;
	}

	urb = next_urb(qh);
	/* with CPPI, DMA sometimes triggers "extra" irqs */
	if (!urb) {
		WARNING("extra TX%d ready, csr %04x\n", epnum, tx_csr);
		goto finished;
	}

	WARNING("OUT/TX%d end, csr %04x\n", epnum, tx_csr);

	/* check for errors */
	if (tx_csr & MUSBFSH_TXCSR_H_RXSTALL) {
		/* dma was disabled, fifo flushed */
		WARNING("TX end %d stall\n", epnum);

		/* stall; record URB status */
		status = -EPIPE;

	} else if (tx_csr & MUSBFSH_TXCSR_H_ERROR) {
		/* (NON-ISO) dma was disabled, fifo flushed */
		WARNING("TX 3strikes on ep=%d\n", epnum);

		status = -ETIMEDOUT;
	} else if (tx_csr & MUSBFSH_TXCSR_H_NAKTIMEOUT) {
		WARNING("TX end=%d device not responding\n", epnum);

		/* NOTE:  this code path would be a good place to PAUSE a
		 * transfer, if there's some other (nonperiodic) tx urb
		 * that could use this fifo.  (dma complicates it...)
		 * That's already done for bulk RX transfers.
		 *
		 * if (bulk && qh->ring.next != &musb->out_bulk), then
		 * we have a candidate... NAKing is *NOT* an error
		 */
		musbfsh_ep_select(mbase, epnum);
		musbfsh_writew(epio, MUSBFSH_TXCSR,
			MUSBFSH_TXCSR_H_WZC_BITS | MUSBFSH_TXCSR_TXPKTRDY);
		return;
	}

/* done: */
	if (status) {
		tx_csr &= ~(MUSBFSH_TXCSR_AUTOSET |
				MUSBFSH_TXCSR_DMAENAB |
				MUSBFSH_TXCSR_H_ERROR |
				MUSBFSH_TXCSR_H_RXSTALL |
				MUSBFSH_TXCSR_H_NAKTIMEOUT);

		musbfsh_ep_select(mbase, epnum);
		musbfsh_writew(epio, MUSBFSH_TXCSR, tx_csr);
		/* REVISIT may need to clear FLUSHFIFO ... */
		musbfsh_writew(epio, MUSBFSH_TXCSR, tx_csr);
		musbfsh_writeb(epio, MUSBFSH_TXINTERVAL, 0);

		done = true;
	}

	/* urb->status != -EINPROGRESS means request has been faulted,
	 * so we must abort this transfer after cleanup
	 */
	if (urb->status != -EINPROGRESS) {
		done = true;
		if (status == 0)
			status = urb->status;
	}

	if (done) {
		/* set status */
		urb->status = status;
		urb->actual_length = qh->offset;
		musbfsh_advance_schedule(musbfsh, urb, hw_ep, USB_DIR_OUT);
	}

finished:
	{
		/* must use static string for AEE usage */
		static char string[100];

		sprintf(string, "USB11_HOST, TXQ<%d> ERR, CSR:%x", epnum, val);
		QMU_PR_WARN("%s\n", string);
#ifdef CONFIG_MTK_BOUNCING_CHECK
		aee_kernel_warning(string, string);
#endif
	}

}

static void mtk11_flush_urb_status(struct musbfsh_qh	*qh, struct urb *urb)
{
	urb->actual_length = 0;
	urb->status = -EINPROGRESS;
	if (qh->type == USB_ENDPOINT_XFER_ISOC) {
		struct usb_iso_packet_descriptor	*d;
		int index;

		for (index = 0; index < urb->number_of_packets; index++) {
			d = urb->iso_frame_desc + qh->iso_idx;
			d->actual_length = 0;
			d->status = -EXDEV;
		}
	}
}

void mtk11_qmu_err_recover(struct musbfsh *musbfsh,
		u8 ep_num, u8 isRx, bool is_len_err)
{
	struct urb *urb;
	struct musbfsh_hw_ep *hw_ep = musbfsh->endpoints + ep_num;
	struct musbfsh_qh			*qh;
	struct usb_host_endpoint	*hep;

	if (isRx)
		qh = hw_ep->in_qh;
	else
		qh = hw_ep->out_qh;

	hep = qh->hep;
	/* same action as musb_flush_qmu */
	mtk11_qmu_stop(ep_num, isRx);
	mtk11_qmu_reset_gpd_pool(ep_num, isRx);

	urb = next_urb(qh);
	if (unlikely(!urb)) {
		pr_warn("No URB.\n");
		return;
	}

	if (usb_pipeisoc(urb->pipe)) {
		if (isRx)
			mtk11_qmu_host_iso_rx_err_info(musbfsh, ep_num);
		else
			mtk11_qmu_host_iso_tx_err_info(musbfsh, ep_num);
		mtk11_flush_ep_csr(musbfsh, ep_num, isRx);
		mtk11_qmu_enable(musbfsh, ep_num, isRx);
		list_for_each_entry(urb, &hep->urb_list, urb_list) {
			QMU_PR_WARN("%s qh:0x%p flush and kick urb:0x%p\n",
				__func__, qh, urb);
			mtk11_flush_urb_status(qh, urb);
			mtk11_kick_CmdQ(musbfsh, isRx, qh, urb);
		}
	} else {
		mtk11_flush_ep_csr(musbfsh, ep_num, isRx);
		if (isRx)
			mtk11_qmu_host_rx_err(musbfsh, ep_num);
		else
			mtk11_qmu_host_tx_err(musbfsh, ep_num);
	}
}

void mtk11_qmu_irq_err(struct musbfsh *musbfsh, u32 qisar)
{
	u8 i;
	u32 wQmuVal;
	u32 wRetVal;
	void __iomem *base = musbfsh_qmu_base;
	u8 err_ep_num = 0;
	bool is_len_err = false;
	u8 isRx;

	wQmuVal = qisar;

	/* RXQ ERROR */
	if (wQmuVal & DQMU_M_RXQ_ERR) {
		wRetVal = MGC_ReadQIRQ32(base, MGC_O_QIRQ_RQEIR) &
				(~(MGC_ReadQIRQ32(base, MGC_O_QIRQ_RQEIMR)));
		QMU_PR_WARN("RQ error in QMU mode![0x%x]\n", wRetVal);

		isRx = RXQ;
		for (i = 1; i <= RXQ_NUM; i++) {
			if (wRetVal & DQMU_M_RX_GPDCS_ERR(i)) {
				QMU_PR_WARN("RQ %d GPD checksum error!\n", i);
				err_ep_num = i;
			}
			if (wRetVal & DQMU_M_RX_LEN_ERR(i)) {
				QMU_PR_WARN("RQ %d receive length error!\n", i);
				err_ep_num = i;
				is_len_err = true;
			}
			if (wRetVal & DQMU_M_RX_ZLP_ERR(i))
				QMU_PR_WARN("RQ %d receive an zlp packet!\n", i);
		}
		MGC_WriteQIRQ32(base, MGC_O_QIRQ_RQEIR, wRetVal);
	}

	/* TXQ ERROR */
	if (wQmuVal & DQMU_M_TXQ_ERR) {
		isRx = TXQ;
		wRetVal = MGC_ReadQIRQ32(base, MGC_O_QIRQ_TQEIR) &
				(~(MGC_ReadQIRQ32(base, MGC_O_QIRQ_TQEIMR)));
		QMU_PR_WARN("TQ error in QMU mode![0x%x]\n", wRetVal);

		for (i = 1; i <= RXQ_NUM; i++) {
			if (wRetVal & DQMU_M_TX_BDCS_ERR(i)) {
				QMU_PR_WARN("TQ %d BD checksum error!\n", i);
				err_ep_num = i;
			}
			if (wRetVal & DQMU_M_TX_GPDCS_ERR(i)) {
				QMU_PR_WARN("TQ %d GPD checksum error!\n", i);
				err_ep_num = i;
			}
			if (wRetVal & DQMU_M_TX_LEN_ERR(i)) {
				QMU_PR_WARN("TQ %d buffer length error!\n", i);
				err_ep_num = i;
				is_len_err = true;
			}
		}
		MGC_WriteQIRQ32(base, MGC_O_QIRQ_TQEIR, wRetVal);
	}

	/* RX EP ERROR */
	if (wQmuVal & DQMU_M_RXEP_ERR) {
		isRx = RXQ;
		wRetVal =
		    MGC_ReadQIRQ32(base,
				   MGC_O_QIRQ_REPEIR) &
		    (~(MGC_ReadQIRQ32(base, MGC_O_QIRQ_REPEIMR)));
		QMU_PR_WARN("Rx endpoint error in QMU mode![0x%x]\n", wRetVal);

		for (i = 1; i <= RXQ_NUM; i++) {
			if (wRetVal & DQMU_M_RX_EP_ERR(i)) {
				QMU_PR_WARN("RX EP %d ERR\n", i);
				err_ep_num = i;
			}
		}

		MGC_WriteQIRQ32(base, MGC_O_QIRQ_REPEIR, wRetVal);
	}

	/* TX EP ERROR */
	if (wQmuVal & DQMU_M_TXEP_ERR) {
		isRx = TXQ;
		wRetVal =
		    MGC_ReadQIRQ32(base,
				   MGC_O_QIRQ_TEPEIR) &
		    (~(MGC_ReadQIRQ32(base, MGC_O_QIRQ_TEPEIMR)));
		QMU_PR_WARN("Tx endpoint error in QMU mode![0x%x]\n", wRetVal);

		for (i = 1; i <= TXQ_NUM; i++) {
			if (wRetVal & DQMU_M_TX_EP_ERR(i)) {
				QMU_PR_WARN("TX EP %d ERR\n", i);
				err_ep_num = i;
			}
		}

		MGC_WriteQIRQ32(base, MGC_O_QIRQ_TEPEIR, wRetVal);
	}

	/* RXQ EMPTY */
	if (wQmuVal & DQMU_M_RQ_EMPTY) {
		wRetVal = MGC_ReadQIRQ32(base, MGC_O_QIRQ_REPEMPR)
		    & (~(MGC_ReadQIRQ32(base, MGC_O_QIRQ_REPEMPMR)));
		QMU_PR_WARN("RQ Empty in QMU mode![0x%x]\n", wRetVal);

		for (i = 1; i <= RXQ_NUM; i++) {
			if (wRetVal & DQMU_M_RX_EMPTY(i))
				QMU_PR_WARN("RQ %d Empty!\n", i);
		}

		MGC_WriteQIRQ32(base, MGC_O_QIRQ_REPEMPR, wRetVal);
	}

	/* TXQ EMPTY */
	if (wQmuVal & DQMU_M_TQ_EMPTY) {
		wRetVal = MGC_ReadQIRQ32(base, MGC_O_QIRQ_TEPEMPR)
		    & (~(MGC_ReadQIRQ32(base, MGC_O_QIRQ_TEPEMPMR)));
		QMU_PR_WARN("TQ Empty in QMU mode![0x%x]\n", wRetVal);

		for (i = 1; i <= TXQ_NUM; i++) {
			if (wRetVal & DQMU_M_TX_EMPTY(i))
				QMU_PR_WARN("TQ %d Empty!\n", i);
		}

		MGC_WriteQIRQ32(base, MGC_O_QIRQ_TEPEMPR, wRetVal);
	}

	/* QMU ERR RECOVER , only servie one ep error ? */
	if (err_ep_num)
		mtk11_qmu_err_recover(musbfsh, err_ep_num, isRx, is_len_err);
}

void h_mtk11_qmu_done_rx(struct musbfsh *musbfsh, u8 ep_num)
{
	void __iomem *base = musbfsh_qmu_base;

	struct _TGPD *gpd = Rx_gpd_last[ep_num];
	struct _TGPD *gpd_current;
	struct musbfsh_hw_ep	*hw_ep = musbfsh->endpoints + ep_num;
	struct musbfsh_qh	*qh = hw_ep->in_qh;
	struct urb	*urb = NULL;
	bool done = true;

	if (unlikely(!qh)) {
		WARNING("hw_ep:%d, QH NULL\n", ep_num);
		return;
	}

	urb = next_urb(qh);
	if (unlikely(!urb)) {
		WARNING("hw_ep:%d, !URB\n", ep_num);
		return;
	}
	INFO("\n");

	gpd_current = (struct _TGPD *)(unsigned long)MGC_ReadQMU32(base,
					MGC_O_QMU_RQCPR(ep_num));

	/*Transfer PHY addr got from QMU register to VIR addr*/
	gpd_current =
		(struct _TGPD *)gpd_phys_to_virt((dma_addr_t)gpd_current,
		RXQ, ep_num);
	INFO("\n");

	QMU_PR_INFO("[RXD]%s EP%d, Last=%p, Current=%p, End=%p\n",
		__func__, ep_num, gpd, gpd_current, Rx_gpd_end[ep_num]);

	/* gpd_current should at least point to the next GPD
	 *to the previous last one
	 */
	if (gpd == gpd_current) {
		QMU_PR_WARN("[RXD][ERROR] gpd(%p) == gpd_current(%p)\n",
					gpd, gpd_current);

		QMU_PR_WARN("[RXD][ERROR]EP%d CSR=%x SAR=%x CPR=%x LDPR=%x\n",
			ep_num,
			MGC_ReadQMU32(base, MGC_O_QMU_RQCSR(ep_num)),
			MGC_ReadQMU32(base, MGC_O_QMU_RQSAR(ep_num)),
			MGC_ReadQMU32(base, MGC_O_QMU_RQCPR(ep_num)),
			MGC_ReadQMU32(base, MGC_O_QMU_RQLDPR(ep_num)));

		QMU_PR_WARN("[RXD][ERROR]QCR0=%x, QCR2=%x, QCR3=%x, QGCSR=%x\n",
			MGC_ReadQMU32(base, MGC_O_QMU_QCR0),
			MGC_ReadQMU32(base, MGC_O_QMU_QCR2),
			MGC_ReadQMU32(base, MGC_O_QMU_QCR3),
			MGC_ReadQUCS32(base, MGC_O_QUCS_USBGCSR));

		QMU_PR_WARN("[RX]HWO=%d N GPD=%p BLen=%d Buf=%p RLen=%d EP=%d\n",
			(u32)TGPD_GET_FLAG(gpd), TGPD_GET_NEXT(gpd),
			(u32)TGPD_GET_DataBUF_LEN(gpd),
			TGPD_GET_DATA(gpd),
			(u32)TGPD_GET_BUF_LEN(gpd),
			(u32)TGPD_GET_EPaddr(gpd));

		return;
	}

	if (!gpd || !gpd_current) {
		QMU_PR_WARN("[RXD][ERR]EP%d gpd%p cur%p is%d last%p CPR0x%x\n",
				ep_num, gpd, gpd_current,
				((gpd == NULL) ? 999 : TGPD_IS_FLAGS_HWO(gpd)),
				Rx_gpd_last[ep_num],
				MGC_ReadQMU32(base, MGC_O_QMU_RQCPR(ep_num)));
		return;
	}

	if (TGPD_IS_FLAGS_HWO(gpd)) {
		QMU_PR_WARN("[RXD][ERROR]HWO=1!!\n");
		musbfsh_bug();
	}

	/* NORMAL EXEC FLOW */
	while (gpd != gpd_current && !TGPD_IS_FLAGS_HWO(gpd)) {
		u32 rcv_len = (u32)TGPD_GET_BUF_LEN(gpd);

		urb = next_urb(qh);
		if (!urb) {
			INFO("extra RX%d ready\n", ep_num);
			mtk11_qmu_stop(ep_num, USB_DIR_IN);
			return;
		}

		if (!TGPD_GET_NEXT(gpd) || !TGPD_GET_DATA(gpd)) {
			QMU_PR_WARN("[RXD][ERROR] EP%d ,gpd=%p\n", ep_num, gpd);
			musbfsh_bug();
		}
		if (usb_pipebulk(urb->pipe) &&
			urb->transfer_buffer_length >= QMU_RX_SPLIT_THRE &&
			usb_pipein(urb->pipe)) {
			urb->actual_length += TGPD_GET_BUF_LEN(gpd);
			qh->offset += TGPD_GET_BUF_LEN(gpd);
			qh->iso_idx++;
			done = (qh->iso_idx == urb->number_of_packets) ?
				true : false;
		} else if (usb_pipeisoc(urb->pipe)) {
			struct usb_iso_packet_descriptor	*d;

			d = urb->iso_frame_desc + qh->iso_idx;
			d->actual_length = rcv_len;
			d->status = 0;
			urb->actual_length += rcv_len;
			qh->offset += TGPD_GET_BUF_LEN(gpd);
			qh->iso_idx++;
			done = (qh->iso_idx == urb->number_of_packets) ?
				true : false;
		} else {
			urb->actual_length = TGPD_GET_BUF_LEN(gpd);
			qh->offset = TGPD_GET_BUF_LEN(gpd);
			done = true;
		}

		gpd = TGPD_GET_NEXT(gpd);

		gpd = gpd_phys_to_virt((dma_addr_t)gpd, RXQ, ep_num);
		INFO("gpd = %p ep_num = %d\n", gpd, ep_num);
		if (!gpd) {
			QMU_PR_WARN("[RXD][ERROR]%s EP%d ,gpd=%p\n",
				__func__, ep_num, gpd);
			musbfsh_bug();
		}
		INFO("gpd = %p ep_num = %d\n", gpd, ep_num);
		Rx_gpd_last[ep_num] = gpd;
		Rx_gpd_free_count[ep_num]++;
		INFO("gpd = %p ep_num = %d\n", gpd, ep_num);
		INFO("hw_ep = %p\n", hw_ep);



		INFO("\n");
		if (done) {
			if (musbfsh_ep_get_qh(hw_ep, USB_DIR_IN))
				qh->iso_idx = 0;

			musbfsh_advance_schedule(musbfsh, urb,
				hw_ep, USB_DIR_IN);

			if (!hw_ep->in_qh) {
				WARNING("hw_ep:%d, QH NULL\n", ep_num);
				return;
			}
		}
	}
	/* QMU should keep take HWO gpd , so there is error*/
	if (gpd != gpd_current && TGPD_IS_FLAGS_HWO(gpd)) {
		QMU_PR_WARN("[RXD][ERROR]gpd=%p\n", gpd);

		QMU_PR_WARN("[RXD][ERROR]EP%d CSR=%x SAR=%x CPR=%x LDPR=%x\n",
				ep_num,
				MGC_ReadQMU32(base, MGC_O_QMU_RQCSR(ep_num)),
				MGC_ReadQMU32(base, MGC_O_QMU_RQSAR(ep_num)),
				MGC_ReadQMU32(base, MGC_O_QMU_RQCPR(ep_num)),
				MGC_ReadQMU32(base, MGC_O_QMU_RQLDPR(ep_num)));

		QMU_PR_WARN("[RXD][ERROR]QCR0=%x, QCR2=%x, QCR3=%x, QGCSR=%x\n",
				MGC_ReadQMU32(base, MGC_O_QMU_QCR0),
				MGC_ReadQMU32(base, MGC_O_QMU_QCR2),
				MGC_ReadQMU32(base, MGC_O_QMU_QCR3),
				MGC_ReadQUCS32(base, MGC_O_QUCS_USBGCSR));

		QMU_PR_WARN("[RX]HWO=%d N GPD=%p BufLen=%d Buf=%p\n",
			(u32)TGPD_GET_FLAG(gpd), TGPD_GET_NEXT(gpd),
			(u32)TGPD_GET_DataBUF_LEN(gpd), TGPD_GET_DATA(gpd));
		QMU_PR_WARN("[RX]RLen=%d EP=%d\n",
			(u32)TGPD_GET_BUF_LEN(gpd), (u32)TGPD_GET_EPaddr(gpd));
	}

	QMU_PR_INFO("[RXD]%s EP%d, Last=%p, End=%p, complete\n", __func__,
		ep_num, Rx_gpd_last[ep_num], Rx_gpd_end[ep_num]);
	INFO("\n");
}

void h_mtk11_qmu_done_tx(struct musbfsh *musbfsh, u8 ep_num)
{
	void __iomem *base = musbfsh_qmu_base;
	struct _TGPD *gpd = Tx_gpd_last[ep_num];
	struct _TGPD *gpd_current;
	struct musbfsh_hw_ep	*hw_ep = musbfsh->endpoints + ep_num;
	struct musbfsh_qh	*qh = hw_ep->out_qh;
	struct urb	*urb = NULL;
	bool done = true;

	if (unlikely(!qh)) {
		WARNING("hw_ep:%d, QH NULL\n", ep_num);
		return;
	}

	urb = next_urb(qh);
	if (unlikely(!urb)) {
		WARNING("hw_ep:%d, !URB\n", ep_num);
		return;
	}

	gpd_current = (struct _TGPD *)(unsigned long)MGC_ReadQMU32(base,
						MGC_O_QMU_TQCPR(ep_num));

	/*Transfer PHY addr got from QMU register to VIR addr*/
	gpd_current = gpd_phys_to_virt((dma_addr_t)gpd_current, TXQ, ep_num);

	QMU_PR_INFO("[TXD]%s EP%d, Last=%p, Current=%p, End=%p\n",
		__func__, ep_num, gpd, gpd_current, Tx_gpd_end[ep_num]);

	/*gpd_current should at least point to the next GPD
	 *to the previous last one.
	 */
	if (gpd == gpd_current)
		return;

	if (TGPD_IS_FLAGS_HWO(gpd)) {
		QMU_PR_WARN("[TXD] HWO=1, CPR=%x\n",
			MGC_ReadQMU32(base, MGC_O_QMU_TQCPR(ep_num)));
		musbfsh_bug();
	}

	/* NORMAL EXEC FLOW */
	while (gpd != gpd_current && !TGPD_IS_FLAGS_HWO(gpd)) {
		QMU_PR_INFO("[TXD]gpd=%p HWO=%d BPD=%d N GPD=%p\n",
			gpd, (u32)TGPD_GET_FLAG(gpd), (u32)TGPD_GET_FORMAT(gpd),
			TGPD_GET_NEXT(gpd));
		QMU_PR_INFO("[TXD]Buf=%p Len=%d\n",
			TGPD_GET_DATA(gpd), (u32)TGPD_GET_BUF_LEN(gpd));

		if (!TGPD_GET_NEXT(gpd)) {
			QMU_PR_WARN("[TXD][ERROR]Next GPD is null!!\n");
			break;
		}

		urb = next_urb(qh);
		if (!urb) {
			QMU_PR_WARN("extra TX%d ready\n", ep_num);
			mtk11_qmu_stop(ep_num, USB_DIR_OUT);
			return;
		}

		if (!TGPD_GET_NEXT(gpd) || !TGPD_GET_DATA(gpd)) {
			QMU_PR_WARN("[RXD][ERROR] EP%d ,gpd=%p\n", ep_num, gpd);
			musbfsh_bug();
		}

		if (usb_pipebulk(urb->pipe)
			&& urb->transfer_buffer_length >= QMU_RX_SPLIT_THRE
			&& usb_pipeout(urb->pipe)) {
			QMU_PR_WARN("bulk???\n");
			urb->actual_length += TGPD_GET_BUF_LEN(gpd);
			qh->offset += TGPD_GET_BUF_LEN(gpd);
			qh->iso_idx++;
			done = (qh->iso_idx == urb->number_of_packets) ?
				true : false;
		} else if (usb_pipeisoc(urb->pipe)) {
			struct usb_iso_packet_descriptor	*d;

			d = urb->iso_frame_desc + qh->iso_idx;
			d->actual_length = TGPD_GET_BUF_LEN(gpd);
			d->status = 0;
			urb->actual_length += TGPD_GET_BUF_LEN(gpd);
			qh->offset += TGPD_GET_BUF_LEN(gpd);
			qh->iso_idx++;
			done = (qh->iso_idx == urb->number_of_packets) ?
				true : false;
		} else {
			QMU_PR_WARN("others use qmu???\n");
			urb->actual_length = TGPD_GET_BUF_LEN(gpd);
			qh->offset = TGPD_GET_BUF_LEN(gpd);
			done = true;
		}
		gpd = TGPD_GET_NEXT(gpd);
		gpd = gpd_phys_to_virt((dma_addr_t)gpd, TXQ, ep_num);
		Tx_gpd_last[ep_num] = gpd;
		Tx_gpd_free_count[ep_num]++;

		if (done) {
			if (musbfsh_ep_get_qh(hw_ep, USB_DIR_OUT))
				qh->iso_idx = 0;

			musbfsh_advance_schedule(musbfsh, urb,
				hw_ep, USB_DIR_OUT);

			if (!hw_ep->out_qh) {
				WARNING("hw_ep:%d,QH NULL\n", ep_num);
				return;
			}
		}
	}
}
#endif
