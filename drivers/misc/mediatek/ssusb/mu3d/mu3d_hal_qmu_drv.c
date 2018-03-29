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


#ifdef USE_SSUSB_QMU
#include <linux/dma-mapping.h>

/* #include "mu3d_hal_osal.h" */
/* #define _MTK_QMU_DRV_EXT_ */
#include "mu3d_hal_qmu_drv.h"
/* #undef _MTK_QMU_DRV_EXT_ */
#include "mu3d_hal_usb_drv.h"
#include "mu3d_hal_hw.h"
#include "ssusb_io.h"


static struct ssusb_gpd_range Rx_gpd_List[15];
static struct ssusb_gpd_range Tx_gpd_List[15];


/**
 * get_bd - get a null gpd
 * @args - arg1: dir, arg2: ep number
 */
static struct ssusb_gpd *get_next_gpd(USB_DIR dir, u32 num)
{
	struct ssusb_gpd *ptr;

	if (dir == USB_RX) {
		ptr = Rx_gpd_List[num].next;

		/* qmu_dbg(K_DEBUG, "[RX]""GPD List[%d]->Next=%p\n", num, Rx_gpd_List[num].next); */

		Rx_gpd_List[num].next = Rx_gpd_List[num].next + 1;

		/* qmu_dbg(K_DEBUG, "[Rx]""GPD List[%d]->Start=%p, Next=%p, End=%p\n", */
		/* num, Rx_gpd_List[num].start, Rx_gpd_List[num].next, Rx_gpd_List[num].end); */

		if (Rx_gpd_List[num].next > Rx_gpd_List[num].end)
			Rx_gpd_List[num].next = Rx_gpd_List[num].start;
	} else {
		ptr = Tx_gpd_List[num].next;

		/* qmu_dbg(K_DEBUG, "[TX]""GPD List[%d]->Next=%p\n", num, Tx_gpd_List[num].next); */

		Tx_gpd_List[num].next = Tx_gpd_List[num].next + 1;

		/* qmu_dbg(K_DEBUG, "[TX]""GPD List[%d]->Start=%p, next=%p, end=%p\n", */
		/* num, Tx_gpd_List[num].start, Tx_gpd_List[num].next, Tx_gpd_List[num].end); */

		if (Tx_gpd_List[num].next > Tx_gpd_List[num].end)
			Tx_gpd_List[num].next = Tx_gpd_List[num].start;
	}
	return ptr;
}

static struct ssusb_gpd *mu3d_get_gpd_from_dma(USB_DIR dir, int num, dma_addr_t gpd_dma_addr)
{
	dma_addr_t dma_base = (dir == USB_RX) ? Rx_gpd_List[num].dma : Tx_gpd_List[num].dma;
	struct ssusb_gpd *gpd_head =
	    (dir == USB_RX) ? Rx_gpd_List[num].start : Tx_gpd_List[num].start;
	unsigned int i = (gpd_dma_addr - dma_base) / sizeof(struct ssusb_gpd);
	/* if equal, overflow infact, should not use, only for compare */
	if (i > MAX_GPD_NUM)
		return NULL;
	return gpd_head + i;

}

static dma_addr_t mu3d_gpd_virt_to_dma(USB_DIR dir, int num, struct ssusb_gpd *ptr)
{
	dma_addr_t dma_base = (dir == USB_RX) ? Rx_gpd_List[num].dma : Tx_gpd_List[num].dma;
	struct ssusb_gpd *gpd_head =
	    (dir == USB_RX) ? Rx_gpd_List[num].start : Tx_gpd_List[num].start;
	unsigned int offset;

	if (!ptr)
		return 0;

	offset = ptr - gpd_head;
	if (offset > MAX_GPD_NUM)
		return 0;

	return dma_base + (offset * sizeof(*ptr));
}

/**
 * init_gpd_list - initialize gpd management list
 * @args - arg1: dir, arg2: ep number, arg3: gpd virtual addr, arg4: gpd ioremap addr, arg5: gpd number
 */
static void init_gpd_list(USB_DIR dir, int num, struct ssusb_gpd *ptr, dma_addr_t io_ptr, u32 size)
{
	if (dir == USB_RX) {
		Rx_gpd_List[num].start = ptr;
		Rx_gpd_List[num].enqueue = ptr;
		Rx_gpd_List[num].dequeue = ptr;
		Rx_gpd_List[num].end = ptr + size - 1;	/* the first overflow one */
		Rx_gpd_List[num].next = ptr + 1;
		qmu_dbg(K_DEBUG, "Rx_gpd_List[%d].start=%p, next=%p, end=%p\n",
			num, Rx_gpd_List[num].start, Rx_gpd_List[num].next, Rx_gpd_List[num].end);
		qmu_dbg(K_DEBUG, "virtual start=%p, end=%p\n", ptr, ptr + size);
		qmu_dbg(K_DEBUG, "dma addr start=%#lx, end=%#lx\n", (unsigned long)io_ptr,
			(unsigned long)(io_ptr + size * sizeof(*ptr)));
		qmu_dbg(K_DEBUG, "dma addr start=%#lx, end=%#lx\n",
			(unsigned long)mu3d_gpd_virt_to_dma(dir, num, ptr),
			(unsigned long)mu3d_gpd_virt_to_dma(dir, num, (ptr + size)));
	} else {
		Tx_gpd_List[num].start = ptr;
		Tx_gpd_List[num].enqueue = ptr;
		Tx_gpd_List[num].dequeue = ptr;
		Tx_gpd_List[num].end = ptr + size - 1;
		Tx_gpd_List[num].next = ptr + 1;
		qmu_dbg(K_DEBUG, "Tx_gpd_List[%d].start=%p, next=%p, end=%p\n",
			num, Tx_gpd_List[num].start, Tx_gpd_List[num].next, Tx_gpd_List[num].end);
		/* qmu_dbg(K_INFO, "Tx_gpd_Offset[%d]=0x%08X\n", num, Tx_gpd_Offset[num]); */
		qmu_dbg(K_DEBUG, "virtual start=%p, end=%p\n", ptr, ptr + size);
		qmu_dbg(K_DEBUG, "dma addr start=%#lx, end=%#lx\n", (unsigned long)io_ptr,
			(unsigned long)(io_ptr + size * sizeof(*ptr)));
		qmu_dbg(K_DEBUG, "dma addr start=%#lx, end=%#lx\n",
			(unsigned long)mu3d_gpd_virt_to_dma(dir, num, ptr),
			(unsigned long)mu3d_gpd_virt_to_dma(dir, num, (ptr + size)));
	}
}

static void reset_gpd_list(USB_DIR dir, int num)
{
	if (dir == USB_RX) {
		Rx_gpd_List[num].enqueue = Rx_gpd_List[num].start;
		Rx_gpd_List[num].dequeue = Rx_gpd_List[num].enqueue;
		Rx_gpd_List[num].next = Rx_gpd_List[num].enqueue + 1;
	} else {
		Tx_gpd_List[num].enqueue = Tx_gpd_List[num].start;
		Tx_gpd_List[num].dequeue = Tx_gpd_List[num].enqueue;
		Tx_gpd_List[num].next = Tx_gpd_List[num].enqueue + 1;
	}

}

/**
 * free_gpd - free gpd management list
 * @args - arg1: dir, arg2: ep number
 */
static void free_gpd(USB_DIR dir, int num)
{
	if (dir == USB_RX)
		memset(Rx_gpd_List[num].start, 0, MAX_GPD_NUM * sizeof(struct ssusb_gpd));
	else
		memset(Tx_gpd_List[num].start, 0, MAX_GPD_NUM * sizeof(struct ssusb_gpd));
}

/**
 * mu3d_hal_alloc_qmu_mem - allocate gpd and bd memory for all ep
 *
 */
void mu3d_hal_alloc_qmu_mem(struct musb *musb)
{
	u32 i, size;
	struct ssusb_gpd *ptr;

	for (i = 1; i <= MAX_QMU_EP; i++) {
		/* Allocate Rx GPD */
		size = sizeof(struct ssusb_gpd) * MAX_GPD_NUM;
		ptr =
		    (struct ssusb_gpd *)dma_alloc_coherent(musb->controller, size,
							   &Rx_gpd_List[i].dma, GFP_KERNEL);
		memset(ptr, 0, size);
		init_gpd_list(USB_RX, i, ptr, Rx_gpd_List[i].dma, MAX_GPD_NUM);

		qmu_dbg(K_DEBUG, "ALLOC RX GPD End [%d] Virtual Mem=%p, DMA addr=%#lx\n", i,
			Rx_gpd_List[i].enqueue, (unsigned long)Rx_gpd_List[i].dma);
		TGPD_CLR_FLAGS_HWO(Rx_gpd_List[i].enqueue);

		/* Allocate Tx GPD */
		size = sizeof(struct ssusb_gpd) * MAX_GPD_NUM;
		ptr =
		    (struct ssusb_gpd *)dma_alloc_coherent(musb->controller, size,
							   &Tx_gpd_List[i].dma, GFP_KERNEL);
		memset(ptr, 0, size);
		init_gpd_list(USB_TX, i, ptr, Tx_gpd_List[i].dma, MAX_GPD_NUM);
		qmu_dbg(K_DEBUG, "ALLOC TX GPD End [%d] Virtual Mem=%p, DMA addr=%#lx\n", i,
			Tx_gpd_List[i].enqueue, (unsigned long)Tx_gpd_List[i].dma);
		TGPD_CLR_FLAGS_HWO(Tx_gpd_List[i].enqueue);
	}
}

void mu3d_hal_free_qmu_mem(struct musb *musb)
{
	u32 i, size;

	for (i = 1; i <= MAX_QMU_EP; i++) {
		size = sizeof(struct ssusb_gpd) * MAX_GPD_NUM;
		if (Rx_gpd_List[i].start) {
			dma_free_coherent(musb->controller, size, Rx_gpd_List[i].start,
					  Rx_gpd_List[i].dma);
			Rx_gpd_List[i].start = NULL;
		}

		if (Tx_gpd_List[i].start) {
			dma_free_coherent(musb->controller, size, Tx_gpd_List[i].start,
					  Tx_gpd_List[i].dma);
			Tx_gpd_List[i].start = NULL;
		}
	}
}

/**
 * mu3d_hal_init_qmu - initialize qmu
 *
 */
void mu3d_hal_init_qmu(struct musb *musb)
{
	void __iomem *mbase = musb->mac_base;
	u32 i;
	/* u32 QCR = 0; */

	/* Initialize QMU Tx/Rx start address. */
	for (i = 1; i <= MAX_QMU_EP; i++) {
		qmu_dbg(K_INFO, "==EP[%d]==Start addr RXQ=%#lx, TXQ=%#lx\n", i,
			(unsigned long)Rx_gpd_List[i].dma, (unsigned long)Tx_gpd_List[i].dma);
		mu3d_writel(mbase, USB_QMU_RQSAR(i), Rx_gpd_List[i].dma);
		mu3d_writel(mbase, USB_QMU_TQSAR(i), Tx_gpd_List[i].dma);

		reset_gpd_list(USB_RX, i);
		reset_gpd_list(USB_TX, i);
	}

	/* Enable QMU interrupt. */
	mu3d_writel(mbase, U3D_QIESR1, TXQ_EMPTY_IESR | TXQ_CSERR_IESR | TXQ_LENERR_IESR |
		    RXQ_EMPTY_IESR | RXQ_CSERR_IESR | RXQ_LENERR_IESR | RXQ_ZLPERR_IESR);
	mu3d_writel(mbase, U3D_EPIESR, EP0ISR);
}

/**
 * mu3d_hal_cal_checksum - calculate check sum
 * @args - arg1: data buffer, arg2: data length
 */

static noinline u8 mu3d_hal_cal_checksum(u8 *data, int len)
{
	u8 *uDataPtr, ckSum;
	int i;

	*(data + 1) = 0x0;
	uDataPtr = data;
	ckSum = 0;

	/* For ALPS01572117, we found calculated QMU check sum is wrong. (Dump memory value directly.) */
	/* After check this function, we did not find any flaw. Still cannot find how to get this wrong value. */
	/* Maybe it is a memory corruption or complier problem. Add "noinline" and "mb();" to prevent this problem. */
	mb();
	for (i = 0; i < len; i++)
		ckSum += *(uDataPtr + i);
	return 0xFF - ckSum;
}

/**
 * mu3d_hal_resume_qmu - resume qmu function
 * @args - arg1: ep number, arg2: dir
 */
void mu3d_hal_resume_qmu(struct musb *musb, int q_num, USB_DIR dir)
{
	void __iomem *mbase = musb->mac_base;

	if (dir == USB_TX) {
		mu3d_writel(mbase, USB_QMU_TQCSR(q_num), QMU_Q_RESUME);
		if (!mu3d_readl(mbase, USB_QMU_TQCSR(q_num))) {
			qmu_dbg(K_WARNIN, "[ERROR]" "%s TQCSR[%d]=%x\n", __func__, q_num,
				mu3d_readl(mbase, USB_QMU_TQCSR(q_num)));
			mu3d_writel(mbase, USB_QMU_TQCSR(q_num), QMU_Q_RESUME);
			qmu_dbg(K_WARNIN, "[ERROR]" "%s TQCSR[%d]=%x\n", __func__, q_num,
				mu3d_readl(mbase, USB_QMU_TQCSR(q_num)));
		}
	} else if (dir == USB_RX) {
		mu3d_writel(mbase, USB_QMU_RQCSR(q_num), QMU_Q_RESUME);
		if (!mu3d_readl(mbase, USB_QMU_RQCSR(q_num))) {
			qmu_dbg(K_WARNIN, "[ERROR]" "%s RQCSR[%d]=%x\n", __func__, q_num,
				mu3d_readl(mbase, USB_QMU_RQCSR(q_num)));
			mu3d_writel(mbase, USB_QMU_RQCSR(q_num), QMU_Q_RESUME);
			qmu_dbg(K_WARNIN, "[ERROR]" "%s RQCSR[%d]=%x\n", __func__, q_num,
				mu3d_readl(mbase, USB_QMU_RQCSR(q_num)));
		}
	} else {
		qmu_dbg(K_ERR, "%s wrong direction!!!\n", __func__);
		BUG_ON(1);
	}
}

/**
 * mu3d_hal_prepare_tx_gpd - prepare tx gpd/bd
 * @args - arg1: gpd address, arg2: data buffer address, arg3: data length, arg4: ep number, arg5: with bd or not, arg6: write hwo bit or not,  arg7: write ioc bit or not
 */
static struct ssusb_gpd *mu3d_hal_prepare_tx_gpd(struct ssusb_gpd *gpd, dma_addr_t pBuf,
						 u32 data_len, u8 ep_num, u8 _is_bdp, u8 isHWO,
						 u8 ioc, u8 bps, u8 zlp)
{
	struct ssusb_gpd *enq;

	/*Set actual data point to "DATA Buffer" */
	TGPD_SET_DATA(gpd, pBuf);
	/*Clear "BDP(Buffer Descriptor Present)" flag */
	TGPD_CLR_FORMAT_BDP(gpd);
	/*
	 * "Data Buffer Length" =
	 * 0        (If data length > GPD buffer length, use BDs),
	 * data_len (If data length < GPD buffer length, only use GPD)
	 */
	TGPD_SET_BUF_LEN(gpd, data_len);

	/*"GPD extension length" = 0. Does not use GPD EXT!! */
	TGPD_SET_EXT_LEN(gpd, 0);

	if (zlp)
		TGPD_SET_FORMAT_ZLP(gpd);
	else
		TGPD_CLR_FORMAT_ZLP(gpd);

	/*Default: bps=false */
	TGPD_CLR_FORMAT_BPS(gpd);

	/*Default: ioc=true */
	TGPD_SET_FORMAT_IOC(gpd);

	/*Get the next GPD */
	Tx_gpd_List[ep_num].enqueue = get_next_gpd(USB_TX, ep_num);
	enq = Tx_gpd_List[ep_num].enqueue;
	qmu_dbg(K_DEBUG, "[TX]" "Tx_gpd_end[%d]=%p\n", ep_num, enq);

	/*Initialize the new GPD */
	memset(enq, 0, sizeof(struct ssusb_gpd));

	/*Clear "HWO(Hardware Own)" flag */
	TGPD_CLR_FLAGS_HWO(enq);

	/*Set "Next GDP pointer" as the next GPD */
	TGPD_SET_NEXT(gpd, mu3d_gpd_virt_to_dma(USB_TX, ep_num, enq));

	/*Default: isHWO=true */
	TGPD_SET_CHKSUM(gpd, CHECKSUM_LENGTH);	/*Set GPD Checksum */
	TGPD_SET_FLAGS_HWO(gpd);	/*Set HWO flag */

	return gpd;
}


/**
 * mu3d_hal_prepare_rx_gpd - prepare rx gpd/bd
 * @args - arg1: gpd address, arg2: data buffer address, arg3: data length, arg4: ep number, arg5: with bd or not, arg6: write hwo bit or not,  arg7: write ioc bit or not
 */
static struct ssusb_gpd *mu3d_hal_prepare_rx_gpd(struct ssusb_gpd *gpd, dma_addr_t pBuf,
						 u32 data_len, u8 ep_num, u8 _is_bdp, u8 isHWO,
						 u8 ioc, u8 bps, u32 cMaxPacketSize)
{
	struct ssusb_gpd *enq;

	qmu_dbg(K_DEBUG, "[RX]" "%s gpd=%p, epnum=%d, len=%d\n", __func__, gpd, ep_num, data_len);

	/*Set actual data point to "DATA Buffer" */
	TGPD_SET_DATA(gpd, pBuf);
	/*Clear "BDP(Buffer Descriptor Present)" flag */
	TGPD_CLR_FORMAT_BDP(gpd);
	/*
	 * Set "Allow Data Buffer Length" =
	 * 0        (If data length > GPD buffer length, use BDs),
	 * data_len (If data length < GPD buffer length, only use GPD)
	 */
	TGPD_SET_DATA_BUF_LEN(gpd, data_len);

	/*Set "Transferred Data Length" = 0 */
	TGPD_SET_BUF_LEN(gpd, 0);

	/*Default: bps=false */
	TGPD_CLR_FORMAT_BPS(gpd);

	/*Default: ioc=true */
	TGPD_SET_FORMAT_IOC(gpd);

	/*Get the next GPD */
	Rx_gpd_List[ep_num].enqueue = get_next_gpd(USB_RX, ep_num);
	enq = Rx_gpd_List[ep_num].enqueue;
	qmu_dbg(K_DEBUG, "[RX]" "Rx_gpd_end[%d]=%p gpd=%p\n", ep_num, enq, gpd);

	/* BUG_ON(!check_next_gpd(gpd, Rx_gpd_end[ep_num])); */

	/*Initialize the new GPD */
	memset(enq, 0, sizeof(struct ssusb_gpd));

	/*Clear "HWO(Hardware Own)" flag */
	TGPD_CLR_FLAGS_HWO(enq);

	/*Set Next GDP pointer to the next GPD */
	TGPD_SET_NEXT(gpd, mu3d_gpd_virt_to_dma(USB_RX, ep_num, enq));

	/*Default: isHWO=true */
	TGPD_SET_CHKSUM(gpd, CHECKSUM_LENGTH);	/*Set GPD Checksum */
	TGPD_SET_FLAGS_HWO(gpd);	/*Set HWO flag */

	return gpd;
}

/**
 * mu3d_hal_insert_transfer_gpd - insert new gpd/bd
 * @args - arg1: ep number, arg2: dir, arg3: data buffer, arg4: data length,  arg5: write hwo bit or not,  arg6: write ioc bit or not
 */
void mu3d_hal_insert_transfer_gpd(int ep_num, USB_DIR dir, dma_addr_t buf,
				  u32 count, u8 isHWO, u8 ioc, u8 bps, u8 zlp, u32 maxp)
{
	struct ssusb_gpd *gpd;

	if (dir == USB_TX) {
		gpd = Tx_gpd_List[ep_num].enqueue;
		mu3d_hal_prepare_tx_gpd(gpd, buf, count, ep_num, IS_BDP, isHWO, ioc, bps, zlp);
	} else if (dir == USB_RX) {
		gpd = Rx_gpd_List[ep_num].enqueue;
		mu3d_hal_prepare_rx_gpd(gpd, buf, count, ep_num, IS_BDP, isHWO, ioc, bps, maxp);
	}
}

/**
 * mu3d_hal_start_qmu - start qmu function (QMU flow : mu3d_hal_init_qmu ->mu3d_hal_start_qmu -> mu3d_hal_insert_transfer_gpd -> mu3d_hal_resume_qmu)
 * @args - arg1: ep number, arg2: dir
 */
void mu3d_hal_start_qmu(struct musb *musb, int q_num, USB_DIR dir)
{
	void __iomem *mbase = musb->mac_base;
	/* u32 QCR; */
	u32 txcsr;
	u32 rxcsr;

	if (dir == USB_TX) {
		txcsr = mu3d_xcsr_readl(mbase, U3D_TX1CSR0, q_num);	/* & 0xFFFEFFFF; */
		mu3d_xcsr_writel(mbase, U3D_TX1CSR0, q_num, txcsr | TX_DMAREQEN);
		mu3d_setmsk(mbase, U3D_QCR0, QMU_TX_CS_EN(q_num));
#if (TXZLP == HW_MODE)
		mu3d_clrmsk(mbase, U3D_QCR1, QMU_TX_ZLP(q_num));
		mu3d_setmsk(mbase, U3D_QCR2, QMU_TX_ZLP(q_num));
#elif (TXZLP == GPD_MODE)
		mu3d_setmsk(mbase, U3D_QCR1, QMU_TX_ZLP(q_num));
#endif
		mu3d_setmsk(mbase, U3D_QEMIESR, QMU_TX_EMPTY(q_num));
		mu3d_writel(mbase, U3D_TQERRIESR0, QMU_TX_LEN_ERR(q_num) | QMU_TX_CS_ERR(q_num));

		qmu_dbg(K_DEBUG, "USB_QMU_TQCSR:0x%08X\n", mu3d_readl(mbase, USB_QMU_TQCSR(q_num)));

		if (mu3d_readl(mbase, USB_QMU_TQCSR(q_num)) & QMU_Q_ACTIVE) {
			qmu_dbg(K_INFO, "Tx %d Active Now!\n", q_num);
			return;
		}

		mu3d_writel(mbase, USB_QMU_TQCSR(q_num), QMU_Q_START);

		qmu_dbg(K_DEBUG, "USB_QMU_TQCSR:0x%08X\n", mu3d_readl(mbase, USB_QMU_TQCSR(q_num)));
	} else if (dir == USB_RX) {
		rxcsr = mu3d_xcsr_readl(mbase, U3D_RX1CSR0, q_num);
		mu3d_xcsr_writel(mbase, U3D_RX1CSR0, q_num, rxcsr | (RX_DMAREQEN));
		mu3d_setmsk(mbase, U3D_QCR0, QMU_RX_CS_EN(q_num));

#ifdef CFG_RX_ZLP_EN
		mu3d_setmsk(mbase, U3D_QCR3, QMU_RX_ZLP(q_num));
#else
		mu3d_clrmsk(mbase, U3D_QCR3, QMU_RX_ZLP(q_num));
#endif

#ifdef CFG_RX_COZ_EN
		mu3d_setmsk(mbase, U3D_QCR3, QMU_RX_COZ(q_num));
#else
		mu3d_clrmsk(mbase, U3D_QCR3, QMU_RX_COZ(q_num));
#endif

		mu3d_setmsk(mbase, U3D_QEMIESR, QMU_RX_EMPTY(q_num));
		mu3d_writel(mbase, U3D_RQERRIESR0, QMU_RX_LEN_ERR(q_num) | QMU_RX_CS_ERR(q_num));
		mu3d_writel(mbase, U3D_RQERRIESR1, QMU_RX_EP_ERR(q_num) | QMU_RX_ZLP_ERR(q_num));

		qmu_dbg(K_DEBUG, "USB_QMU_RQCSR:0x%08X\n", mu3d_readl(mbase, USB_QMU_RQCSR(q_num)));

		if (mu3d_readl(mbase, USB_QMU_RQCSR(q_num)) & QMU_Q_ACTIVE) {
			qmu_dbg(K_INFO, "Rx %d Active Now!\n", q_num);
			return;
		}

		mu3d_writel(mbase, USB_QMU_RQCSR(q_num), QMU_Q_START);

		qmu_dbg(K_DEBUG, "USB_QMU_RQCSR:0x%08X\n", mu3d_readl(mbase, USB_QMU_RQCSR(q_num)));
	}
#if (CHECKSUM_TYPE == CS_16B)
	mu3d_setmsk(mbase, U3D_QCR0, CS16B_EN);
#else
	mu3d_clrmsk(mbase, U3D_QCR0, CS16B_EN);
#endif
}

/**
 * mu3d_hal_stop_qmu - stop qmu function (after qmu stop, fifo should be flushed)
 * @args - arg1: ep number, arg2: dir
 */
void mu3d_hal_stop_qmu(struct musb *musb, int q_num, USB_DIR dir)
{
	void __iomem *mbase = musb->mac_base;

	if (dir == USB_TX) {
		if (!(mu3d_readl(mbase, USB_QMU_TQCSR(q_num)) & (QMU_Q_ACTIVE))) {
			qmu_dbg(K_DEBUG, "Tx%d inActive Now!\n", q_num);
			return;
		}
		mu3d_writel(mbase, USB_QMU_TQCSR(q_num), QMU_Q_STOP);
		while ((mu3d_readl(mbase, USB_QMU_TQCSR(q_num)) & (QMU_Q_ACTIVE)))
			;
		qmu_dbg(K_CRIT, "Tx%d stop Now!\n", q_num);
	} else if (dir == USB_RX) {
		if (!(mu3d_readl(mbase, USB_QMU_RQCSR(q_num)) & QMU_Q_ACTIVE)) {
			qmu_dbg(K_DEBUG, "Rx%d inActive Now!\n", q_num);
			return;
		}
		mu3d_writel(mbase, USB_QMU_RQCSR(q_num), QMU_Q_STOP);
		while ((mu3d_readl(mbase, USB_QMU_RQCSR(q_num)) & (QMU_Q_ACTIVE)))
			;
		qmu_dbg(K_CRIT, "Rx%d stop now!\n", q_num);
	}
}

/**
 * mu3d_hal_send_stall - send stall
 * @args - arg1: ep number, arg2: dir
 */
void mu3d_hal_send_stall(struct musb *musb, int q_num, USB_DIR dir)
{
	void __iomem *mbase = musb->mac_base;
	u32 tmp;

	if (dir == USB_TX) {
		tmp = mu3d_xcsr_readl(mbase, U3D_TX1CSR0, q_num);
		mu3d_xcsr_writel(mbase, U3D_TX1CSR0, q_num, tmp | TX_SENDSTALL);

		while (!(mu3d_xcsr_readl(mbase, U3D_TX1CSR0, q_num) & TX_SENTSTALL))
			;

		tmp = mu3d_xcsr_readl(mbase, U3D_TX1CSR0, q_num);
		mu3d_xcsr_writel(mbase, U3D_TX1CSR0, q_num, tmp | TX_SENTSTALL);
		tmp = mu3d_xcsr_readl(mbase, U3D_TX1CSR0, q_num);
		mu3d_xcsr_writel(mbase, U3D_TX1CSR0, q_num, tmp & (~TX_SENDSTALL));

	} else if (dir == USB_RX) {
		tmp = mu3d_xcsr_readl(mbase, U3D_RX1CSR0, q_num);
		mu3d_xcsr_writel(mbase, U3D_RX1CSR0, q_num, tmp | RX_SENDSTALL);

		while (!(mu3d_xcsr_readl(mbase, U3D_RX1CSR0, q_num) & RX_SENTSTALL))
			;

		tmp = mu3d_xcsr_readl(mbase, U3D_RX1CSR0, q_num);
		mu3d_xcsr_writel(mbase, U3D_RX1CSR0, q_num, tmp | RX_SENTSTALL);
		tmp = mu3d_xcsr_readl(mbase, U3D_RX1CSR0, q_num);
		mu3d_xcsr_writel(mbase, U3D_RX1CSR0, q_num, tmp & (~RX_SENDSTALL));
	}

	qmu_dbg(K_CRIT, "%s %s-EP[%d] sent stall\n", __func__, ((dir == USB_TX) ? "TX" : "RX"),
		q_num);
}

/**
 * mu3d_hal_restart_qmu - clear toggle(or sequence) number and start qmu
 * @args - arg1: ep number, arg2: dir
 */
void mu3d_hal_restart_qmu(struct musb *musb, int q_num, USB_DIR dir)
{
	void __iomem *mbase = musb->mac_base;
	u32 ep_rst;

	qmu_dbg(K_CRIT, "%s : Rest %s-EP[%d]\n", __func__, ((dir == USB_TX) ? "TX" : "RX"), q_num);

	if (dir == USB_TX) {
		ep_rst = BIT16 << q_num;
		mu3d_writel(mbase, U3D_EP_RST, ep_rst);
		mdelay(1);
		mu3d_writel(mbase, U3D_EP_RST, 0);
	} else {
		ep_rst = 1 << q_num;
		mu3d_writel(mbase, U3D_EP_RST, ep_rst);
		mdelay(1);
		mu3d_writel(mbase, U3D_EP_RST, 0);
	}
	mu3d_hal_start_qmu(musb, q_num, dir);
}

/**
 * flush_qmu - stop qmu and align qmu start ptr t0 current ptr
 * @args - arg1: ep number, arg2: dir
 */
void mu3d_hal_flush_qmu(struct musb *musb, int q_num, USB_DIR dir)
{

	qmu_dbg(K_CRIT, "%s flush QMU %s\n", __func__, ((dir == USB_TX) ? "TX" : "RX"));

	/*Stop QMU */
	mu3d_hal_stop_qmu(musb, q_num, dir);
	free_gpd(dir, q_num);
	reset_gpd_list(dir, q_num);
}


/*
 * 1. Find the last gpd HW has executed and update Tx_gpd_last[]
 * 2. Set the flag for txstate to know that TX has been completed
 * caller:qmu_interrupt after getting QMU done interrupt and TX is raised
 *
 * NOTE: request list maybe is already empty as following case:
 * queue_tx --> qmu_interrupt(clear int pending, schedule tasklet)-->
 * queue_tx --> process_tasklet(at the same time, the second one tx over,
 * tasklet process both of them)-->qmu_interrupt for second one.
 * To avoid upper case, put qmu_done_tx in ISR directly to process it.
*/
void qmu_done_tx(struct musb *musb, u8 ep_num, unsigned long flags)
{
	struct ssusb_gpd *gpd = Tx_gpd_List[ep_num].dequeue;
	struct ssusb_gpd *gpd_current = NULL;
	struct musb_ep *musb_ep = &musb->endpoints[ep_num].ep_in;
	void __iomem *mbase = musb->mac_base;
	dma_addr_t gpd_dma = mu3d_readl(mbase, USB_QMU_TQCPR(ep_num));
	struct usb_request *request = NULL;
	struct musb_request *req;

	/* trying to give_back the request to gadget driver. */
	req = next_request(musb_ep);
	if (req)
		request = &req->request;
	else
		return;
	/*Transfer PHY addr got from QMU register to VIR addr */
	gpd_current = mu3d_get_gpd_from_dma(USB_TX, ep_num, gpd_dma);

	/*
	   gpd or Last       gdp_current
	   |                  |
	   |->  GPD1 --> GPD2 --> GPD3 --> GPD4 --> GPD5 -|
	   |----------------------------------------------|
	 */

	qmu_dbg(K_DEBUG, "[TXD]" "%s EP%d, Last=%p, Current=%p, End=%p\n",
		__func__, ep_num, gpd, gpd_current, Tx_gpd_List[ep_num].enqueue);

	/*gpd_current should at least point to the next GPD to the previous last one. */
	if (gpd == gpd_current) {
		qmu_dbg(K_ERR, "[TXD][warn] %s gpd(%p) == gpd_current(%p)\n", __func__, gpd,
			gpd_current);
		return;
	}

	if (TGPD_IS_FLAGS_HWO(gpd)) {
		qmu_dbg(K_DEBUG, "[TXD][ERROR] %s HWO=1, CPR=%x\n", __func__,
			mu3d_readl(mbase, USB_QMU_TQCPR(ep_num)));
		BUG_ON(1);
	}

	while (gpd != gpd_current && !TGPD_IS_FLAGS_HWO(gpd)) {

		if (!TGPD_GET_NEXT(gpd)) {
			qmu_dbg(K_ERR, "[TXD][ERROR]" "Next GPD is null!!\n");
			/* BUG_ON(1); */
			break;
		}

		gpd_dma = (dma_addr_t) TGPD_GET_NEXT(gpd);
		gpd = mu3d_get_gpd_from_dma(USB_TX, ep_num, gpd_dma);

		Tx_gpd_List[ep_num].dequeue = gpd;
		musb_g_giveback(musb_ep, request, 0);
		req = next_request(musb_ep);
		request = &req->request;
	}

	if (gpd != gpd_current && TGPD_IS_FLAGS_HWO(gpd)) {
		qmu_dbg(K_ERR, "[TXD][ERROR]" "EP%d TQCSR=%x, TQSAR=%x, TQCPR=%x\n",
			ep_num, mu3d_readl(mbase, USB_QMU_TQCSR(ep_num)),
			mu3d_readl(mbase, USB_QMU_TQSAR(ep_num)),
			mu3d_readl(mbase, USB_QMU_TQCPR(ep_num)));
	}

	qmu_dbg(K_DEBUG, "[TXD]" "%s EP%d, Last=%p, End=%p, complete\n", __func__,
		ep_num, Tx_gpd_List[ep_num].dequeue, Tx_gpd_List[ep_num].enqueue);

	if (req != NULL) {
		if (request->length == 0) {
			u32 txcsr = 0;

			qmu_dbg(K_DEBUG, "[TXD]" "==Send ZLP== %p\n", req);

			/* NEED to add timeout process */
			while (!(mu3d_xcsr_readl(mbase, U3D_TX1CSR0, req->epnum) & TX_FIFOFULL)) {
				txcsr = mu3d_xcsr_readl(mbase, U3D_TX1CSR0, req->epnum);
				mu3d_xcsr_writel(mbase, U3D_TX1CSR0, req->epnum,
						 txcsr & ~TX_DMAREQEN);

				txcsr = mu3d_xcsr_readl(mbase, U3D_TX1CSR0, req->epnum);
				mu3d_xcsr_writel(mbase, U3D_TX1CSR0, req->epnum,
						 txcsr | TX_TXPKTRDY);
				break;
			}

			qmu_dbg(K_DEBUG,
				"[TXD]" "Giveback ZLP of EP%d, actual:%d, length:%d %p\n",
				req->epnum, request->actual, request->length, request);

			musb_g_giveback(musb_ep, request, 0);
		}
	}
}

/*
   When receiving RXQ done interrupt, qmu_interrupt calls this function.

   1. Traverse GPD/BD data structures to count actual transferred length.
   2. Set the done flag to notify rxstate_qmu() to report status to upper gadget driver.

    ported from proc_qmu_rx() from test driver.

    caller:qmu_interrupt after getting QMU done interrupt and TX is raised

*/
void qmu_done_rx(struct musb *musb, u8 ep_num, unsigned long flags)
{
	struct ssusb_gpd *gpd = Rx_gpd_List[ep_num].dequeue;
	struct ssusb_gpd *gpd_current = NULL;
	struct musb_ep *musb_ep = &musb->endpoints[ep_num].ep_out;
	void __iomem *mbase = musb->mac_base;
	dma_addr_t gpd_dma = (mu3d_readl(mbase, USB_QMU_RQCPR(ep_num)));
	struct usb_request *request = NULL;
	struct musb_request *req;

	/* trying to give_back the request to gadget driver. */
	req = next_request(musb_ep);
	if (req)
		request = &req->request;
	else
		return;

	/*Transfer PHY addr got from QMU register to VIR addr */
	gpd_current = mu3d_get_gpd_from_dma(USB_RX, ep_num, gpd_dma);

	qmu_dbg(K_DEBUG, "[RXD]" "%s EP%d, Last=%p, Current=%p, End=%p\n",
		__func__, ep_num, gpd, gpd_current, Rx_gpd_List[ep_num].enqueue);

	/*gpd_current should at least point to the next GPD to the previous last one. */
	if (gpd == gpd_current) {
		qmu_dbg(K_ERR, "[RXD][ERROR]" "%s gpd(%p) == gpd_current(%p)\n", __func__, gpd,
			gpd_current);

		qmu_dbg(K_ERR, "[RXD][ERROR]" "EP%d RQCSR=%x, RQSAR=%x, RQCPR=%x, RQLDPR=%x\n",
			ep_num, mu3d_readl(mbase, USB_QMU_RQCSR(ep_num)),
			mu3d_readl(mbase, USB_QMU_RQSAR(ep_num)),
			mu3d_readl(mbase, USB_QMU_RQCPR(ep_num)),
			mu3d_readl(mbase, USB_QMU_RQLDPR(ep_num)));
		return;
	}

	if (!gpd || !gpd_current)
		return;

	if (TGPD_IS_FLAGS_HWO(gpd)) {
		qmu_dbg(K_ERR, "[RXD][ERROR]" "HWO=1!!\n");
		BUG_ON(1);
	}

	while (gpd != gpd_current && !TGPD_IS_FLAGS_HWO(gpd)) {
		u32 rcv_len = (u32) TGPD_GET_BUF_LEN(gpd);
		u32 buf_len = (u32) TGPD_GET_DATA_BUF_LEN(gpd);

		if (rcv_len > buf_len)
			qmu_dbg(K_ERR, "[RXD][ERROR]" "%s rcv(%d) > buf(%d) AUK!?\n", __func__,
				rcv_len, buf_len);

		qmu_dbg(K_DEBUG,
			"[RXD]" "gpd=%p ->HWO=%d, Next_GPD=%x, RcvLen=%d, BufLen=%d, pBuf=%#x\n",
			gpd, TGPD_GET_FLAG(gpd), TGPD_GET_NEXT(gpd), rcv_len, buf_len,
			TGPD_GET_DATA(gpd));

		request->actual += rcv_len;

		if (!TGPD_GET_NEXT(gpd) || !TGPD_GET_DATA(gpd)) {
			qmu_dbg(K_ERR, "[RXD][ERROR]" "%s EP%d ,gpd=%p\n", __func__, ep_num, gpd);
			BUG_ON(1);
		}

		gpd_dma = (dma_addr_t) TGPD_GET_NEXT(gpd);
		gpd = mu3d_get_gpd_from_dma(USB_RX, ep_num, gpd_dma);

		if (!gpd) {
			qmu_dbg(K_ERR, "[RXD][ERROR]" "%s EP%d ,gpd=%p\n", __func__, ep_num, gpd);
			BUG_ON(1);
		}

		Rx_gpd_List[ep_num].dequeue = gpd;
		musb_g_giveback(musb_ep, request, 0);
		req = next_request(musb_ep);
		request = &req->request;
	}

	if (gpd != gpd_current && TGPD_IS_FLAGS_HWO(gpd)) {
		qmu_dbg(K_ERR, "[RXD][ERROR]" "gpd=%p\n", gpd);

		qmu_dbg(K_ERR, "[RXD][ERROR]" "EP%d RQCSR=%x, RQSAR=%x, RQCPR=%x, RQLDPR=%x\n",
			ep_num, mu3d_readl(mbase, USB_QMU_RQCSR(ep_num)),
			mu3d_readl(mbase, USB_QMU_RQSAR(ep_num)),
			mu3d_readl(mbase, USB_QMU_RQCPR(ep_num)),
			mu3d_readl(mbase, USB_QMU_RQLDPR(ep_num)));
	}

	qmu_dbg(K_DEBUG, "[RXD]" "%s EP%d, Last=%p, End=%p, complete\n", __func__,
		ep_num, Rx_gpd_List[ep_num].dequeue, Rx_gpd_List[ep_num].enqueue);
}

void qmu_done_tasklet(unsigned long data)
{
	unsigned int qmu_val;
	unsigned int i;
	unsigned long flags;
	struct musb *musb = (struct musb *)data;

	spin_lock_irqsave(&musb->lock, flags);

	qmu_val = musb->qmu_done_intr;

	musb->qmu_done_intr = 0;

	for (i = 1; i <= MAX_QMU_EP; i++) {
		if (qmu_val & QMU_RX_DONE(i))
			qmu_done_rx(musb, i, flags);
		if (qmu_val & QMU_TX_DONE(i))
			qmu_done_tx(musb, i, flags);
	}
	spin_unlock_irqrestore(&musb->lock, flags);
}

void qmu_exception_interrupt(struct musb *musb, u32 wQmuVal)
{
	void __iomem *mbase = musb->mac_base;
	u32 wErrVal;
	int i = (int)wQmuVal;

	if (wQmuVal & RXQ_CSERR_INT)
		qmu_dbg(K_ERR, "==Rx %d checksum error==\n", i);

	if (wQmuVal & RXQ_LENERR_INT)
		qmu_dbg(K_ERR, "==Rx %d length error==\n", i);

	if (wQmuVal & TXQ_CSERR_INT)
		qmu_dbg(K_ERR, "==Tx %d checksum error==\n", i);

	if (wQmuVal & TXQ_LENERR_INT)
		qmu_dbg(K_ERR, "==Tx %d length error==\n", i);

	if ((wQmuVal & RXQ_CSERR_INT) || (wQmuVal & RXQ_LENERR_INT)) {
		wErrVal = mu3d_readl(mbase, U3D_RQERRIR0);
		qmu_dbg(K_DEBUG, "Rx Queue error in QMU mode![0x%x]\r\n", (unsigned int)wErrVal);
		for (i = 1; i <= MAX_QMU_EP; i++) {
			if (wErrVal & QMU_RX_CS_ERR(i))
				qmu_dbg(K_ERR, "Rx %d CS error!\r\n", i);

			if (wErrVal & QMU_RX_LEN_ERR(i))
				qmu_dbg(K_ERR, "RX EP%d Recv Length error\n", i);
		}
		mu3d_writel(mbase, U3D_RQERRIR0, wErrVal);
	}

	if (wQmuVal & RXQ_ZLPERR_INT) {
		wErrVal = mu3d_readl(mbase, U3D_RQERRIR1);
		qmu_dbg(K_DEBUG, "Rx Queue error in QMU mode![0x%x]\r\n", (unsigned int)wErrVal);
		for (i = 1; i <= MAX_QMU_EP; i++) {
			if (wErrVal & QMU_RX_ZLP_ERR(i)) {
				/*FIXME: should _NOT_ got this error. But now just accept. */
				qmu_dbg(K_INFO, "RX EP%d Recv ZLP\n", i);
			}
		}
		mu3d_writel(mbase, U3D_RQERRIR1, wErrVal);
	}

	if ((wQmuVal & TXQ_CSERR_INT) || (wQmuVal & TXQ_LENERR_INT)) {
		wErrVal = mu3d_readl(mbase, U3D_TQERRIR0);
		qmu_dbg(K_DEBUG, "Tx Queue error in QMU mode![0x%x]\r\n", (unsigned int)wErrVal);
		for (i = 1; i <= MAX_QMU_EP; i++) {
			if (wErrVal & QMU_TX_CS_ERR(i))
				qmu_dbg(K_ERR, "Tx %d checksum error!\r\n", i);

			if (wErrVal & QMU_TX_LEN_ERR(i))
				qmu_dbg(K_ERR, "Tx %d buffer length error!\r\n", i);
		}
		mu3d_writel(mbase, U3D_TQERRIR0, wErrVal);
	}

	if ((wQmuVal & RXQ_EMPTY_INT) || (wQmuVal & TXQ_EMPTY_INT)) {
		u32 wEmptyVal = mu3d_readl(mbase, U3D_QEMIR);

		qmu_dbg(K_DEBUG, "%s Empty in QMU mode![0x%x]\r\n",
			(wQmuVal & TXQ_EMPTY_INT) ? "TX" : "RX", wEmptyVal);
		mu3d_writel(mbase, U3D_QEMIR, wEmptyVal);
	}
}

#endif
