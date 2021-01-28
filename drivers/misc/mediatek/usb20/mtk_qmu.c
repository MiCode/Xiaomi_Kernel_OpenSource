// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/list.h>
#include <linux/module.h>
#include "musb_qmu.h"
#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
#endif

#ifdef CONFIG_MTK_UAC_POWER_SAVING
#define USB_AUDIO_DATA_OUT 0
static struct TGPD *Tx_gpd_head_dram;
static u64 Tx_gpd_Offset_dram;
#endif

static struct TGPD *Rx_gpd_head[MAX_QMU_EP + 1];
static struct TGPD *Tx_gpd_head[MAX_QMU_EP + 1];
static struct TGPD *Rx_gpd_end[MAX_QMU_EP + 1];
static struct TGPD *Tx_gpd_end[MAX_QMU_EP + 1];
static struct TGPD *Rx_gpd_last[MAX_QMU_EP + 1];
static struct TGPD *Tx_gpd_last[MAX_QMU_EP + 1];
static struct _GPD_RANGE Rx_gpd_List[MAX_QMU_EP + 1];
static struct _GPD_RANGE Tx_gpd_List[MAX_QMU_EP + 1];
static u64 Rx_gpd_Offset[MAX_QMU_EP + 1];
static u64 Tx_gpd_Offset[MAX_QMU_EP + 1];
static u32 Rx_gpd_free_count[MAX_QMU_EP + 1];
static u32 Tx_gpd_free_count[MAX_QMU_EP + 1];
static u32 Rx_gpd_max_count[MAX_QMU_EP + 1];
static u32 Tx_gpd_max_count[MAX_QMU_EP + 1];
static bool Tx_enable[MAX_QMU_EP + 1];
static bool Rx_enable[MAX_QMU_EP + 1];


u32 qmu_used_gpd_count(u8 isRx, u32 num)
{
	if (isRx)
		return (Rx_gpd_max_count[num] - 1) - Rx_gpd_free_count[num];
	else
		return (Tx_gpd_max_count[num] - 1) - Tx_gpd_free_count[num];
}

u32 qmu_free_gpd_count(u8 isRx, u32 num)
{
	if (isRx)
		return Rx_gpd_free_count[num];
	else
		return Tx_gpd_free_count[num];
}

u8 PDU_calcCksum(u8 *data, int len)
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

static struct TGPD *get_gpd(u8 isRx, u32 num)
{
	struct TGPD *ptr;

	if (isRx) {
		ptr = Rx_gpd_List[num].pNext;
		Rx_gpd_List[num].pNext =
			(struct TGPD *) ((u8 *) (Rx_gpd_List[num].pNext) +
				GPD_LEN_ALIGNED);

		if (Rx_gpd_List[num].pNext >= Rx_gpd_List[num].pEnd)
			Rx_gpd_List[num].pNext = Rx_gpd_List[num].pStart;
		Rx_gpd_free_count[num]--;
	} else {
		ptr = Tx_gpd_List[num].pNext;
		Tx_gpd_List[num].pNext =
			(struct TGPD *) ((u8 *) (Tx_gpd_List[num].pNext) +
			GPD_LEN_ALIGNED);

		if (Tx_gpd_List[num].pNext >= Tx_gpd_List[num].pEnd)
			Tx_gpd_List[num].pNext = Tx_gpd_List[num].pStart;
		Tx_gpd_free_count[num]--;
	}
	return ptr;
}

static void gpd_ptr_align(u8 isRx, u32 num, struct TGPD *ptr)
{
	if (isRx)
		Rx_gpd_List[num].pNext =
			(struct TGPD *) ((u8 *) (ptr) + GPD_LEN_ALIGNED);
	else
		Tx_gpd_List[num].pNext =
			(struct TGPD *) ((u8 *) (ptr) + GPD_LEN_ALIGNED);
}

static dma_addr_t gpd_virt_to_phys(void *vaddr, u8 isRx, u32 num)
{
	dma_addr_t paddr;

	if (isRx)
		paddr = (dma_addr_t) ((u64) (uintptr_t)vaddr -
				Rx_gpd_Offset[num]);
	else
		paddr = (dma_addr_t) ((u64) (uintptr_t)vaddr -
				Tx_gpd_Offset[num]);

	QMU_INFO("%s[%d]phys=%p<->virt=%p\n",
		 ((isRx == RXQ) ? "RQ" : "TQ"), num,
		 (void *)(uintptr_t)paddr, vaddr);

	return paddr;
}

static void *gpd_phys_to_virt(dma_addr_t paddr, u8 isRx, u32 num)
{
	void *vaddr;


	if (isRx)
		vaddr = (void *)(uintptr_t)((u64) paddr + Rx_gpd_Offset[num]);
	else
		vaddr = (void *)(uintptr_t)((u64) paddr + Tx_gpd_Offset[num]);
	QMU_INFO("%s[%d]phys=%p<->virt=%p\n",
		 ((isRx == RXQ) ? "RQ" : "TQ"),
		 num, (void *)(uintptr_t)paddr, vaddr);

	return vaddr;
}

static void init_gpd_list(u8 isRx,
	int num, struct TGPD *ptr, struct TGPD *io_ptr, u32 size)
{
	if (isRx) {
		Rx_gpd_List[num].pStart = ptr;
		Rx_gpd_List[num].pEnd =
			(struct TGPD *) ((u8 *) (ptr + size) +
			(GPD_EXT_LEN * size));
		Rx_gpd_Offset[num] =
			(u64) (uintptr_t)ptr - (u64) (uintptr_t)io_ptr;
		ptr++;
		Rx_gpd_List[num].pNext =
			(struct TGPD *) ((u8 *) ptr + GPD_EXT_LEN);

		QMU_INFO("Rx_gpd_List[%d].pStart=%p, pNext=%p, pEnd=%p\n",
			 num, Rx_gpd_List[num].pStart, Rx_gpd_List[num].pNext,
			 Rx_gpd_List[num].pEnd);
		QMU_INFO(
			"Rx_gpd_Offset[%d]=%p\n"
			, num, (void *)(uintptr_t)Rx_gpd_Offset[num]);
	} else {
		Tx_gpd_List[num].pStart = ptr;
		Tx_gpd_List[num].pEnd =
				(struct TGPD *) ((u8 *) (ptr + size) +
				(GPD_EXT_LEN * size));
		Tx_gpd_Offset[num] =
			(u64) (uintptr_t)ptr - (u64) (uintptr_t)io_ptr;
		ptr++;
		Tx_gpd_List[num].pNext =
			(struct TGPD *) ((u8 *) ptr + GPD_EXT_LEN);

		QMU_INFO("Tx_gpd_List[%d].pStart=%p, pNext=%p, pEnd=%p\n",
			 num, Tx_gpd_List[num].pStart, Tx_gpd_List[num].pNext,
			 Tx_gpd_List[num].pEnd);
		QMU_INFO("Tx_gpd_Offset[%d]=%p\n"
			, num, (void *)(uintptr_t)Tx_gpd_Offset[num]);
	}
}

int qmu_init_gpd_pool(struct device *dev)
{
	u32 i, size;
	struct TGPD *ptr, *io_ptr;
	dma_addr_t dma_handle;
	u32 gpd_sz;
	unsigned long addr;
	u64 coherent_dma_mask;

	/* make sure GPD address no longer than 32-bit */
	coherent_dma_mask = dev->coherent_dma_mask;
	dev->coherent_dma_mask = DMA_BIT_MASK(32);
	QMU_DBG("save coherent<%llx>, force to 32-bit\n",
			coherent_dma_mask);

	if (!mtk_qmu_max_gpd_num)
		mtk_qmu_max_gpd_num = DFT_MAX_GPD_NUM;

#ifdef MUSB_QMU_LIMIT_SUPPORT
	isoc_ep_end_idx = MAX_QMU_EP;
#endif

	for (i = 1; i <= isoc_ep_end_idx; i++) {
		if (isoc_ep_gpd_count > mtk_qmu_max_gpd_num)
			Rx_gpd_max_count[i] =
				Tx_gpd_max_count[i] = isoc_ep_gpd_count;
		else
			Rx_gpd_max_count[i] =
				Tx_gpd_max_count[i] = mtk_qmu_max_gpd_num;
	}

	for (i = isoc_ep_end_idx + 1 ; i <= MAX_QMU_EP; i++)
		Rx_gpd_max_count[i] = Tx_gpd_max_count[i] = mtk_qmu_max_gpd_num;

	gpd_sz = (u32) (u64) sizeof(struct TGPD);
	pr_notice("sizeof(struct TGPD):%d\n", gpd_sz);
	if (gpd_sz != GPD_SZ)
		QMU_ERR("ERR!!!, GPD SIZE != %d\n", GPD_SZ);

	for (i = 1; i <= RXQ_NUM; i++) {

		/* Allocate Rx GPD */
		size = GPD_LEN_ALIGNED * Rx_gpd_max_count[i];
		ptr = (struct TGPD *)
				dma_alloc_coherent(dev,
					size, &dma_handle, GFP_KERNEL);
		if (!ptr)
			return -ENOMEM;

		/* covert to physical address  for check only */
		addr = virt_to_phys(ptr);

		memset_io(ptr, 0, size);
		io_ptr = (struct TGPD *)(uintptr_t)(dma_handle);

		init_gpd_list(RXQ, i, ptr, io_ptr, Rx_gpd_max_count[i]);
		Rx_gpd_end[i] = Rx_gpd_last[i] = Rx_gpd_head[i] = ptr;
		/* one must be for tail */
		Rx_gpd_free_count[i] = Rx_gpd_max_count[i] - 1;
		TGPD_CLR_FLAGS_HWO(Rx_gpd_end[i]);
		gpd_ptr_align(RXQ, i, Rx_gpd_end[i]);
		QMU_DBG(
				"RX GPD HEAD[%d], VIRT<%p>, DMA<%p>, PHY<%p>, RQSAR<%p>\n"
				, i
				, Rx_gpd_head[i], io_ptr, (void *)addr,
				(void *)(uintptr_t)
				gpd_virt_to_phys(Rx_gpd_end[i], RXQ, i));
	}

	for (i = 1; i <= TXQ_NUM; i++) {

		/* Allocate Tx GPD */
		size = GPD_LEN_ALIGNED * Tx_gpd_max_count[i];
		ptr = (struct TGPD *)
			dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL);
		if (!ptr)
			return -ENOMEM;

		/* covert to physical address  for check only */
		addr = virt_to_phys(ptr);

		memset_io(ptr, 0, size);
		io_ptr = (struct TGPD *)(uintptr_t)(dma_handle);

		init_gpd_list(TXQ, i, ptr, io_ptr, Tx_gpd_max_count[i]);
		Tx_gpd_end[i] = Tx_gpd_last[i] = Tx_gpd_head[i] = ptr;
		/* one must be for tail */
		Tx_gpd_free_count[i] = Tx_gpd_max_count[i] - 1;
		TGPD_CLR_FLAGS_HWO(Tx_gpd_end[i]);
		gpd_ptr_align(TXQ, i, Tx_gpd_end[i]);
		QMU_DBG(
				"TX GPD HEAD[%d], VIRT<%p>, DMA<%p>, PHY<%p>, TQSAR<%p>\n",
				i,
				Tx_gpd_head[i], io_ptr, (void *)addr,
				(void *)(uintptr_t)gpd_virt_to_phys
				(Tx_gpd_end[i], TXQ, i));
	}

#ifdef CONFIG_MTK_UAC_POWER_SAVING
	Tx_gpd_head_dram = Tx_gpd_head[ISOC_EP_START_IDX];
	Tx_gpd_Offset_dram = Tx_gpd_Offset[ISOC_EP_START_IDX];
#endif

	dev->coherent_dma_mask = coherent_dma_mask;
	QMU_DBG("restore coherent from 32-bit to <%llx>\n",
			coherent_dma_mask);
	return 0;
}

#ifdef CONFIG_MTK_UAC_POWER_SAVING
void *mtk_usb_alloc_sram(int id, size_t size, dma_addr_t *dma)
{
	void *sram_virt_addr = NULL;

	if (!use_mtk_audio || !usb_on_sram)
		return NULL;

	if (id == USB_AUDIO_DATA_OUT) {
		mtk_audio_request_sram(dma, (unsigned char **)&sram_virt_addr,
				size, &audio_on_sram);

		if (sram_virt_addr)
			audio_on_sram = 1;
		else {
			DBG(0, "NO MEMORY!!!\n");
			audio_on_sram = 0;
		}
	}

	return sram_virt_addr;
}

void mtk_usb_free_sram(int id)
{
	if (!use_mtk_audio)
		return;

	if (id == USB_AUDIO_DATA_OUT) {
		mtk_audio_free_sram(&audio_on_sram);
		audio_on_sram = 0;
	}
}

int gpd_switch_to_sram(struct device *dev)
{
	u32 size;
	struct TGPD *ptr = NULL, *io_ptr;
	int index = ISOC_EP_START_IDX;
	dma_addr_t dma_handle;

	size = GPD_LEN_ALIGNED * Tx_gpd_max_count[index];

	if (use_mtk_audio)
		mtk_audio_request_sram(&dma_handle,
				(unsigned char **)&ptr, size, &usb_on_sram);
	else
		ptr = (struct TGPD *)
			dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL);

	if (!ptr) {
		DBG(0, "NO MEMORY!!!\n");
		return -ENOMEM;
	}

	memset_io(ptr, 0, size);

	/* setup Tx_gpd_Offset & Tx_gpd_List */
	io_ptr = (struct TGPD *)(uintptr_t)(dma_handle);
	init_gpd_list(TXQ, index, ptr, io_ptr, Tx_gpd_max_count[index]);

	Tx_gpd_end[index] = Tx_gpd_last[index] = Tx_gpd_head[index] = ptr;
	/* one must be for tail */
	Tx_gpd_free_count[index] = Tx_gpd_max_count[index] - 1;
	TGPD_CLR_FLAGS_HWO(Tx_gpd_end[index]);
	gpd_ptr_align(TXQ, index, Tx_gpd_end[index]);

	DBG(0, "head<%p>, offset<%p>\n",
			Tx_gpd_head[index],
			(void *)(uintptr_t)Tx_gpd_Offset[index]);

	return 0;
}

void gpd_switch_to_dram(struct device *dev)
{
	u32 size;
	struct TGPD *ptr, *io_ptr;
	int index = ISOC_EP_START_IDX;

	size = GPD_LEN_ALIGNED * Tx_gpd_max_count[index];

	if (use_mtk_audio)
		mtk_audio_free_sram(&usb_on_sram);
	else
		dma_free_coherent(dev, size, Tx_gpd_head[index],
				gpd_virt_to_phys(Tx_gpd_head[index],
				TXQ, index));

	ptr = Tx_gpd_head_dram;
	memset_io(ptr, 0, size);

	/* setup Tx_gpd_Offset & Tx_gpd_List, careful about type casting */
	io_ptr = (struct TGPD *)(uintptr_t)((u64)(uintptr_t)Tx_gpd_head_dram
				- Tx_gpd_Offset_dram);
	init_gpd_list(TXQ, index, ptr, io_ptr, Tx_gpd_max_count[index]);

	if (Tx_gpd_Offset[index] != Tx_gpd_Offset_dram) {
		static char string[64];

		sprintf(string, "offset<%p, %p>\n",
				(void *)(uintptr_t)Tx_gpd_Offset[index],
				(void *)(uintptr_t)Tx_gpd_Offset_dram);
		QMU_ERR("%s\n", string);
#ifdef CONFIG_MTK_AEE_FEATURE
		aee_kernel_warning(string, string);
#endif
	}

	Tx_gpd_end[index] = Tx_gpd_last[index] = Tx_gpd_head[index] = ptr;
	/* one must be for tail */
	Tx_gpd_free_count[index] = Tx_gpd_max_count[index] - 1;
	TGPD_CLR_FLAGS_HWO(Tx_gpd_end[index]);
	gpd_ptr_align(TXQ, index, Tx_gpd_end[index]);

	DBG(0, "head<%p>, offset<%p>\n"
		, Tx_gpd_head[index], (void *)(uintptr_t)Tx_gpd_Offset[index]);
}
#endif

void qmu_reset_gpd_pool(u32 ep_num, u8 isRx)
{
	u32 size;


	/* SW reset */
	if (isRx) {
		size = GPD_LEN_ALIGNED * Rx_gpd_max_count[ep_num];
		memset_io(Rx_gpd_head[ep_num], 0, size);
		Rx_gpd_end[ep_num] = Rx_gpd_last[ep_num] = Rx_gpd_head[ep_num];
		/* one must be for tail */
		Rx_gpd_free_count[ep_num] = Rx_gpd_max_count[ep_num] - 1;
		TGPD_CLR_FLAGS_HWO(Rx_gpd_end[ep_num]);
		gpd_ptr_align(isRx, ep_num, Rx_gpd_end[ep_num]);

	} else {
		size = GPD_LEN_ALIGNED * Tx_gpd_max_count[ep_num];
		memset_io(Tx_gpd_head[ep_num], 0, size);
		Tx_gpd_end[ep_num] = Tx_gpd_last[ep_num] = Tx_gpd_head[ep_num];
		 /* one must be for tail */
		Tx_gpd_free_count[ep_num] = Tx_gpd_max_count[ep_num] - 1;
		TGPD_CLR_FLAGS_HWO(Tx_gpd_end[ep_num]);
		gpd_ptr_align(isRx, ep_num, Tx_gpd_end[ep_num]);
	}
}

void qmu_destroy_gpd_pool(struct device *dev)
{

	int i;

	for (i = 1; i <= RXQ_NUM; i++) {
		dma_free_coherent(dev, GPD_LEN_ALIGNED * Rx_gpd_max_count[i],
			Rx_gpd_head[i],
			gpd_virt_to_phys(Rx_gpd_head[i],
			RXQ, i));
	}

	for (i = 1; i <= TXQ_NUM; i++) {
		dma_free_coherent(dev, GPD_LEN_ALIGNED * Tx_gpd_max_count[i],
			Tx_gpd_head[i],
			gpd_virt_to_phys(Tx_gpd_head[i], TXQ, i));
	}
}

static void prepare_rx_gpd(dma_addr_t pBuf, u32 data_len, u8 ep_num, u8 isioc)
{
	struct TGPD *gpd;

	/* get gpd from tail */
	gpd = Rx_gpd_end[ep_num];

	TGPD_SET_DATA(gpd, (uintptr_t)pBuf);

#ifdef CONFIG_MTK_MUSB_DRV_36BIT
	TGPD_SET_DATA_RXHI(gpd, (u8)(pBuf >> 32));
#endif

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
	QMU_INFO("[RX]Rx_gpd_end[%d]=%p gpd=%p\n",
		ep_num, Rx_gpd_end[ep_num], gpd);
	memset_io(Rx_gpd_end[ep_num], 0, GPD_LEN_ALIGNED);
	TGPD_CLR_FLAGS_HWO(Rx_gpd_end[ep_num]);

	/* make sure struct ready before set to next */
	mb();
	TGPD_SET_NEXT(gpd, (uintptr_t)gpd_virt_to_phys(Rx_gpd_end[ep_num]
		, RXQ, ep_num));

#ifdef CONFIG_MTK_MUSB_DRV_36BIT
	TGPD_SET_NEXT_RXHI(gpd,
		(u8)(gpd_virt_to_phys(Rx_gpd_end[ep_num], RXQ, ep_num) >> 32));
#endif

	TGPD_SET_CHKSUM_HWO(gpd, 16);

	/* make sure struct ready before HWO */
	mb();
	TGPD_SET_FLAGS_HWO(gpd);
}

static void prepare_tx_gpd(dma_addr_t pBuf,
	u32 data_len, u8 ep_num, u8 zlp, u8 isioc)
{
	struct TGPD *gpd;

	/* get gpd from tail */
	gpd = Tx_gpd_end[ep_num];

	TGPD_SET_DATA(gpd, (uintptr_t)pBuf);

#ifdef CONFIG_MTK_MUSB_DRV_36BIT
	TGPD_SET_DATA_TXHI(gpd, (u8)(pBuf >> 32));
#endif

	TGPD_CLR_FORMAT_BDP(gpd);

	TGPD_SET_BUF_LEN(gpd, data_len);
	TGPD_SET_EXT_LEN(gpd, 0);

#ifdef CONFIG_MTK_MUSB_QMU_PURE_ZLP_SUPPORT
	if (zlp | (data_len == 0))
#else
	if (zlp)
#endif
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
	QMU_INFO("[TX]Tx_gpd_end[%d]=%p gpd=%p\n"
		, ep_num, Tx_gpd_end[ep_num], gpd);
	memset_io(Tx_gpd_end[ep_num], 0, GPD_LEN_ALIGNED);
	TGPD_CLR_FLAGS_HWO(Tx_gpd_end[ep_num]);


	/* make sure struct ready before set to next */
	mb();
	TGPD_SET_NEXT(gpd,
		(uintptr_t)gpd_virt_to_phys(Tx_gpd_end[ep_num], TXQ, ep_num));

#ifdef CONFIG_MTK_MUSB_DRV_36BIT
	TGPD_SET_NEXT_TXHI(gpd,
		(u8)(gpd_virt_to_phys(Tx_gpd_end[ep_num], TXQ, ep_num) >> 32));
#endif

	TGPD_SET_CHKSUM_HWO(gpd, 16);

	/* make sure struct ready before HWO */
	mb();
	TGPD_SET_FLAGS_HWO(gpd);

}

void mtk_qmu_resume(u8 ep_num, u8 isRx)
{
	void __iomem *base = qmu_base;

	if (!isRx) {
		MGC_WriteQMU32(base, MGC_O_QMU_TQCSR(ep_num), DQMU_QUE_RESUME);
		if (!MGC_ReadQMU32(base, MGC_O_QMU_TQCSR(ep_num))) {
			QMU_ERR("TQCSR[%d]=%x\n", ep_num,
				MGC_ReadQMU32(base, MGC_O_QMU_TQCSR(ep_num)));
			MGC_WriteQMU32(base
				, MGC_O_QMU_TQCSR(ep_num), DQMU_QUE_RESUME);
			QMU_ERR("TQCSR[%d]=%x\n", ep_num,
				MGC_ReadQMU32(base, MGC_O_QMU_TQCSR(ep_num)));
		}
	} else {
		MGC_WriteQMU32(base, MGC_O_QMU_RQCSR(ep_num), DQMU_QUE_RESUME);
		if (!MGC_ReadQMU32(base, MGC_O_QMU_RQCSR(ep_num))) {
			QMU_ERR("RQCSR[%d]=%x\n", ep_num,
				MGC_ReadQMU32(base, MGC_O_QMU_RQCSR(ep_num)));
			MGC_WriteQMU32(base,
					MGC_O_QMU_RQCSR(ep_num)
					, DQMU_QUE_RESUME);
			QMU_ERR("RQCSR[%d]=%x\n", ep_num,
				MGC_ReadQMU32(base, MGC_O_QMU_RQCSR(ep_num)));
		}
	}
}

bool mtk_is_qmu_enabled(u8 ep_num, u8 isRx)
{
	void __iomem *base = qmu_base;

	if (isRx) {
		if (MGC_ReadQUCS32(base, MGC_O_QUCS_USBGCSR)
			& (USB_QMU_Rx_EN(ep_num)))
			return true;
	} else {
		if (MGC_ReadQUCS32(base, MGC_O_QUCS_USBGCSR)
			& (USB_QMU_Tx_EN(ep_num)))
			return true;
	}
	return false;
}

void mtk_qmu_enable(struct musb *musb, u8 ep_num, u8 isRx)
{
	struct musb_ep *musb_ep;
	u32 QCR;
	void __iomem *base = qmu_base;
	void __iomem *mbase = musb->mregs;
	void __iomem *epio;
	u16 csr = 0;
	u16 intr_e = 0;

	epio = musb->endpoints[ep_num].regs;
	musb_ep_select(mbase, ep_num);

	if (isRx) {
		QMU_WARN("enable RQ(%d)\n", ep_num);

		Rx_enable[ep_num] = true;

		/* enable dma */
		csr |= MUSB_RXCSR_DMAENAB;

		/* check ISOC */
		if (!musb->is_host) {
			musb_ep = &musb->endpoints[ep_num].ep_out;
			if (musb_ep->type == USB_ENDPOINT_XFER_ISOC)
				csr |= MUSB_RXCSR_P_ISO;
		}
		musb_writew(epio, MUSB_RXCSR, csr);

		/* turn off intrRx */
		intr_e = musb_readw(mbase, MUSB_INTRRXE);
		intr_e = intr_e & (~(1 << (ep_num)));
		musb_writew(mbase, MUSB_INTRRXE, intr_e);

		/* set 1st gpd and enable */
		MGC_WriteQMU32(base, MGC_O_QMU_RQSAR(ep_num),
			       gpd_virt_to_phys(Rx_gpd_end[ep_num]
					, RXQ, ep_num));
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
				DQMU_M_RX_DONE(ep_num)
				| DQMU_M_RQ_EMPTY
				| DQMU_M_RXQ_ERR
				| DQMU_M_RXEP_ERR);


#ifdef CFG_EMPTY_CHECK
		MGC_WriteQIRQ32(base,
			MGC_O_QIRQ_REPEMPMCR, DQMU_M_RX_EMPTY(ep_num));
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


		MGC_WriteQIRQ32(base,
			MGC_O_QIRQ_REPEIMCR, DQMU_M_RX_EP_ERR(ep_num));

		/* make sure HW setting done before start QMU */
		mb();
		/* qmu start */
		MGC_WriteQMU32(base, MGC_O_QMU_RQCSR(ep_num), DQMU_QUE_START);

	} else {
		QMU_WARN("enable TQ(%d)\n", ep_num);

		Tx_enable[ep_num] = true;

		/* enable dma */
		csr |= MUSB_TXCSR_DMAENAB;

		/* check ISOC */
		if (!musb->is_host) {
			musb_ep = &musb->endpoints[ep_num].ep_in;
			if (musb_ep->type == USB_ENDPOINT_XFER_ISOC)
				csr |= MUSB_TXCSR_P_ISO;
		}

		musb_writew(epio, MUSB_TXCSR, csr);

		/* turn off intrTx */
		intr_e = musb_readw(mbase, MUSB_INTRTXE);
		intr_e = intr_e & (~(1 << ep_num));
		musb_writew(mbase, MUSB_INTRTXE, intr_e);

		/* set 1st gpd and enable */
		MGC_WriteQMU32(base, MGC_O_QMU_TQSAR(ep_num),
			       gpd_virt_to_phys(Tx_gpd_end[ep_num],
			       TXQ, ep_num));
		MGC_WriteQUCS32(base, MGC_O_QUCS_USBGCSR,
				MGC_ReadQUCS32(base, MGC_O_QUCS_USBGCSR)
				| (USB_QMU_Tx_EN(ep_num)));

#ifdef CFG_CS_CHECK
		QCR = MGC_ReadQMU32(base, MGC_O_QMU_QCR0);
		MGC_WriteQMU32(base,
				MGC_O_QMU_QCR0, QCR
				| DQMU_TQCS_EN(ep_num));
#endif

#if (TXZLP == HW_MODE)
		QCR = MGC_ReadQMU32(base, MGC_O_QMU_QCR2);
		MGC_WriteQMU32(base, MGC_O_QMU_QCR2, QCR | DQMU_TX_ZLP(ep_num));
#elif (TXZLP == GPD_MODE)
		QCR = MGC_ReadQMU32(base, MGC_O_QMU_QCR2);
		MGC_WriteQMU32(base,
				MGC_O_QMU_QCR2,
				QCR | DQMU_TQ_GDP_ZLP(ep_num));
#endif

#ifdef CONFIG_MTK_MUSB_QMU_PURE_ZLP_SUPPORT
		QCR = musb_readb(mbase, MUSB_GPZCR);
		musb_writeb(mbase, MUSB_GPZCR, QCR | (1 << (ep_num-1)));
#endif

		MGC_WriteQIRQ32(base, MGC_O_QIRQ_QIMCR,
				DQMU_M_TX_DONE(ep_num)
				| DQMU_M_TQ_EMPTY
				| DQMU_M_TXQ_ERR
				| DQMU_M_TXEP_ERR);

#ifdef CFG_EMPTY_CHECK
		MGC_WriteQIRQ32(base,
			MGC_O_QIRQ_TEPEMPMCR, DQMU_M_TX_EMPTY(ep_num));
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

		/* make sure HW setting done before start QMU */
		mb();
		/* qmu start */
		MGC_WriteQMU32(base, MGC_O_QMU_TQCSR(ep_num), DQMU_QUE_START);
	}
}

void mtk_qmu_stop(u8 ep_num, u8 isRx)
{
	void __iomem *base = qmu_base;

	if (!isRx) {
		if (MGC_ReadQMU16(base,
				MGC_O_QMU_TQCSR(ep_num)) & DQMU_QUE_ACTIVE) {
			MGC_WriteQMU32(base,
				MGC_O_QMU_TQCSR(ep_num),
				DQMU_QUE_STOP);
			QMU_INFO("Stop TQ %d\n", ep_num);
		} else {
			QMU_INFO("TQ %d already inactive\n", ep_num);
		}
	} else {
		if (MGC_ReadQMU16(base
				, MGC_O_QMU_RQCSR(ep_num)) & DQMU_QUE_ACTIVE) {
			MGC_WriteQMU32(base
				, MGC_O_QMU_RQCSR(ep_num)
				, DQMU_QUE_STOP);
			QMU_INFO("Stop RQ %d\n", ep_num);
		} else {
			QMU_INFO("RQ %d already inactive\n", ep_num);
		}
	}
}

static void mtk_qmu_disable(u8 ep_num, u8 isRx)
{
	u32 QCR;
	void __iomem *base = qmu_base;
	bool state_change = false;

	if (isRx && Rx_enable[ep_num]) {
		Rx_enable[ep_num] = false;
		state_change = true;
	} else if (!isRx && Tx_enable[ep_num]) {
		Tx_enable[ep_num] = false;
		state_change = true;
	}

	if (state_change)
		QMU_WARN("disable %s(%d)\n", isRx ? "RQ" : "TQ", ep_num);

	mtk_qmu_stop(ep_num, isRx);
	if (isRx) {
		/* / clear Queue start address */
		MGC_WriteQMU32(base, MGC_O_QMU_RQSAR(ep_num), 0);

		/* KOBE, in MT6735,
		 * different EP QMU
		 * EN is separated in MGC_O_QUCS_USBGCSR ??
		 */
		MGC_WriteQUCS32(base, MGC_O_QUCS_USBGCSR,
				MGC_ReadQUCS32(base,
					MGC_O_QUCS_USBGCSR) &
					(~(USB_QMU_Rx_EN(ep_num))));

		QCR = MGC_ReadQMU32(base, MGC_O_QMU_QCR0);
		MGC_WriteQMU32(base,
				MGC_O_QMU_QCR0,
				QCR & (~(DQMU_RQCS_EN(ep_num))));
		QCR = MGC_ReadQMU32(base, MGC_O_QMU_QCR3);
		MGC_WriteQMU32(base,
				MGC_O_QMU_QCR3, QCR & (~(DQMU_RX_ZLP(ep_num))));

		MGC_WriteQIRQ32(base, MGC_O_QIRQ_QIMSR, DQMU_M_RX_DONE(ep_num));
		MGC_WriteQIRQ32(base,
				MGC_O_QIRQ_REPEMPMSR, DQMU_M_RX_EMPTY(ep_num));
		MGC_WriteQIRQ32(base, MGC_O_QIRQ_RQEIMSR,
				DQMU_M_RX_LEN_ERR(ep_num)
				| DQMU_M_RX_GPDCS_ERR(ep_num)
				| DQMU_M_RX_ZLP_ERR(ep_num));
		MGC_WriteQIRQ32(base,
				MGC_O_QIRQ_REPEIMSR,
				DQMU_M_RX_EP_ERR(ep_num));
	} else {
		/* / clear Queue start address */
		MGC_WriteQMU32(base, MGC_O_QMU_TQSAR(ep_num), 0);

		/* KOBE, in MT6735,
		 * different EP QMU EN is
		 * separated in MGC_O_QUCS_USBGCSR ??
		 */
		MGC_WriteQUCS32(base, MGC_O_QUCS_USBGCSR,
				MGC_ReadQUCS32(base,
					       MGC_O_QUCS_USBGCSR)
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
				DQMU_M_TX_LEN_ERR(ep_num)
				| DQMU_M_TX_GPDCS_ERR(ep_num)
				| DQMU_M_TX_BDCS_ERR(ep_num));
		MGC_WriteQIRQ32(base,
				MGC_O_QIRQ_TEPEIMSR,
				DQMU_M_TX_EP_ERR(ep_num));
	}
}

void mtk_qmu_insert_task(u8 ep_num, u8 isRx
	, dma_addr_t buf, u32 length, u8 zlp, u8 isioc)
{
	QMU_INFO(
		"%s ep_num: %d, isRx: %d, buf: %p, length: %d zlp: %d isioc: %d\n",
			__func__, ep_num, isRx, (void *)(uintptr_t)buf,
			length, zlp, isioc);
	if (isRx) /* rx don't care zlp input */
		prepare_rx_gpd(buf, length, ep_num, isioc);
	else
		prepare_tx_gpd(buf, length, ep_num, zlp, isioc);
}

void qmu_done_rx(struct musb *musb, u8 ep_num)
{
	void __iomem *base = qmu_base;

	struct TGPD *gpd = Rx_gpd_last[ep_num];
	struct TGPD *gpd_current = (struct TGPD *) (uintptr_t)
			MGC_ReadQMU32(base, MGC_O_QMU_RQCPR(ep_num));
	struct musb_ep *musb_ep = &musb->endpoints[ep_num].ep_out;
	struct usb_request *request = NULL;
	struct musb_request *req;

	/* trying to give_back the request to gadget driver. */
	req = next_request(musb_ep);
	if (!req) {
		QMU_ERR(
			"[RXD]%s Cannot get next request of %d, but QMU has done.\n"
			, __func__, ep_num);
		return;
	}
	request = &req->request;

	/*Transfer PHY addr got from QMU register to VIR addr */
	gpd_current = (struct TGPD *)
		gpd_phys_to_virt((dma_addr_t)(uintptr_t)
						gpd_current, RXQ, ep_num);

	QMU_INFO("[RXD]%s EP%d, Last=%p, Current=%p, End=%p\n",
		 __func__, ep_num, gpd, gpd_current, Rx_gpd_end[ep_num]);

	/* gpd_current should at least
	 * point to the next GPD to
	 * the previous last one
	 */
	if (gpd == gpd_current) {

		QMU_ERR(
			"[RXD][ERROR] gpd(%p) == gpd_current(%p)\n"
			"[RXD][ERROR]EP%d RQCSR=%x, RQSAR=%x, RQCPR=%x, RQLDPR=%x\n"
			"[RXD][ERROR]QCR0=%x, QCR2=%x, QCR3=%x, QGCSR=%x\n"
			"[RXD][ERROR]HWO=%d, Next_GPD=%p ,DataBufLen=%d, DataBuf=%p, RecvLen=%d, Endpoint=%d\n",
			gpd, gpd_current,
			ep_num,
			MGC_ReadQMU32(base, MGC_O_QMU_RQCSR(ep_num)),
			MGC_ReadQMU32(base, MGC_O_QMU_RQSAR(ep_num)),
			MGC_ReadQMU32(base, MGC_O_QMU_RQCPR(ep_num)),
			MGC_ReadQMU32(base, MGC_O_QMU_RQLDPR(ep_num)),
			MGC_ReadQMU32(base, MGC_O_QMU_QCR0),
			MGC_ReadQMU32(base, MGC_O_QMU_QCR2),
			MGC_ReadQMU32(base, MGC_O_QMU_QCR3),
			MGC_ReadQUCS32(base, MGC_O_QUCS_USBGCSR),
				(u32) TGPD_GET_FLAG(gpd),
				TGPD_GET_NEXT_RX(gpd),
				(u32) TGPD_GET_DataBUF_LEN(gpd),
				TGPD_GET_DATA_RX(gpd),
				(u32) TGPD_GET_BUF_LEN(gpd),
				(u32) TGPD_GET_EPaddr(gpd));
		return;
	}

	if (!gpd || !gpd_current) {

		QMU_ERR(
			"[RXD][ERROR] EP%d, gpd=%p, gpd_current=%p, ishwo=%d, rx_gpd_last=%p,	RQCPR=0x%x\n",
		     ep_num, gpd, gpd_current,
		     ((gpd == NULL) ? 999 : TGPD_IS_FLAGS_HWO(gpd)),
		     Rx_gpd_last[ep_num],
		     MGC_ReadQMU32(base, MGC_O_QMU_RQCPR(ep_num)));
		return;
	}

	if (TGPD_IS_FLAGS_HWO(gpd)) {
		QMU_ERR("[RXD][ERROR]HWO=1!!\n");
		/* BUG_ON(1); */
		return;
	}

	/* NORMAL EXEC FLOW */
	while (gpd != gpd_current && !TGPD_IS_FLAGS_HWO(gpd)) {
		u32 rcv_len = (u32) TGPD_GET_BUF_LEN(gpd);
		u32 buf_len = (u32) TGPD_GET_DataBUF_LEN(gpd);

		if (rcv_len > buf_len)
			QMU_ERR(
				"[RXD][ERROR] rcv(%d) > buf(%d) AUK!?\n"
				, rcv_len, buf_len);

		QMU_INFO(
			"[RXD]gpd=%p ->HWO=%d, Next_GPD=%p, RcvLen=%d, BufLen=%d, pBuf=%p\n"
			, gpd, TGPD_GET_FLAG(gpd)
			, TGPD_GET_NEXT_RX(gpd)
			, rcv_len, buf_len,
			 TGPD_GET_DATA_RX(gpd));

		request->actual += rcv_len;

		if (!TGPD_GET_NEXT_RX(gpd) || !TGPD_GET_DATA_RX(gpd)) {
			QMU_ERR("[RXD][ERROR] EP%d ,gpd=%p\n", ep_num, gpd);
			/* BUG_ON(1); */
			return;
		}

		gpd = TGPD_GET_NEXT_RX(gpd);

		gpd = gpd_phys_to_virt((dma_addr_t)(uintptr_t)gpd, RXQ, ep_num);

		if (!gpd) {
			QMU_ERR("[RXD][ERROR] !gpd, EP%d ,gpd=%p\n"
						, ep_num, gpd);
			/* BUG_ON(1); */
			return;
		}

		Rx_gpd_last[ep_num] = gpd;
		Rx_gpd_free_count[ep_num]++;
		musb_g_giveback(musb_ep, request, 0);
		req = next_request(musb_ep);
		request = &req->request;
	}

	/* QMU should keep take HWO gpd , so there is error */
	if (gpd != gpd_current && TGPD_IS_FLAGS_HWO(gpd)) {
		QMU_ERR("[RXD][ERROR]gpd=%p\n"
				"[RXD][ERROR]EP%d RQCSR=%x, RQSAR=%x, RQCPR=%x, RQLDPR=%x\n"
				"[RXD][ERROR]QCR0=%x, QCR2=%x, QCR3=%x, QGCSR=%x\n"
				"[RXD][ERROR]HWO=%d, Next_GPD=%p ,DataBufLen=%d, DataBuf=%p, RecvLen=%d, Endpoint=%d\n"
				, gpd, ep_num,
			MGC_ReadQMU32(base, MGC_O_QMU_RQCSR(ep_num)),
			MGC_ReadQMU32(base, MGC_O_QMU_RQSAR(ep_num)),
			MGC_ReadQMU32(base, MGC_O_QMU_RQCPR(ep_num)),
				MGC_ReadQMU32(base, MGC_O_QMU_RQLDPR(ep_num)),
			MGC_ReadQMU32(base, MGC_O_QMU_QCR0),
			MGC_ReadQMU32(base, MGC_O_QMU_QCR2),
			MGC_ReadQMU32(base, MGC_O_QMU_QCR3),
				MGC_ReadQUCS32(base, MGC_O_QUCS_USBGCSR),
				(u32) TGPD_GET_FLAG(gpd),
				TGPD_GET_NEXT_RX(gpd),
				(u32) TGPD_GET_DataBUF_LEN(gpd),
				TGPD_GET_DATA_RX(gpd),
				(u32) TGPD_GET_BUF_LEN(gpd),
				(u32) TGPD_GET_EPaddr(gpd));

	}

	QMU_INFO("[RXD]%s EP%d, Last=%p, End=%p, complete\n", __func__,
		 ep_num, Rx_gpd_last[ep_num], Rx_gpd_end[ep_num]);
}

void qmu_done_tx(struct musb *musb, u8 ep_num)
{
	void __iomem *base = qmu_base;
	struct TGPD *gpd = Tx_gpd_last[ep_num];
	struct TGPD *gpd_current =
		(struct TGPD *) (uintptr_t)
		MGC_ReadQMU32(base, MGC_O_QMU_TQCPR(ep_num));
	struct musb_ep *musb_ep = &musb->endpoints[ep_num].ep_in;
	struct usb_request *request = NULL;
	struct musb_request *req = NULL;

	/* Transfer PHY addr got
	 * from QMU register to VIR addr
	 */
	gpd_current = gpd_phys_to_virt
			((dma_addr_t)(uintptr_t)gpd_current, TXQ, ep_num);

	/*
	 *   gpd or Last       gpd_current
	 *   |                  |
	 *   |->  GPD1 --> GPD2 --> GPD3 --> GPD4 --> GPD5 -|
	 *   |----------------------------------------------|
	 */

	QMU_INFO("[TXD]%s EP%d, Last=%p, Current=%p, End=%p\n",
		 __func__, ep_num, gpd, gpd_current, Tx_gpd_end[ep_num]);

	/* gpd_current should at least
	 * point to the next GPD to the
	 * previous last one.
	 */
	if (gpd == gpd_current) {
		QMU_INFO("[TXD] gpd(%p) == gpd_current(%p)\n"
			, gpd, gpd_current);
		return;
	}

	if (TGPD_IS_FLAGS_HWO(gpd)) {
		QMU_ERR("[TXD] HWO=1, CPR=%x\n",
			MGC_ReadQMU32(base, MGC_O_QMU_TQCPR(ep_num)));
		/* BUG_ON(1); */
		return;
	}

	/* NORMAL EXEC FLOW */
	while (gpd != gpd_current && !TGPD_IS_FLAGS_HWO(gpd)) {

		QMU_INFO(
			"[TXD]gpd=%p ->HWO=%d, BPD=%d, Next_GPD=%p, DataBuffer=%p, BufferLen=%d request=%p\n",
			 gpd, (u32) TGPD_GET_FLAG(gpd),
			 (u32) TGPD_GET_FORMAT(gpd),
			 TGPD_GET_NEXT_TX(gpd), TGPD_GET_DATA_TX(gpd),
			 (u32) TGPD_GET_BUF_LEN(gpd), req);

		if (!TGPD_GET_NEXT_TX(gpd)) {
			QMU_ERR("[TXD][ERROR]Next GPD is null!!\n");
			/* BUG_ON(1); */
			return;
		}

		gpd = TGPD_GET_NEXT_TX(gpd);

		gpd = gpd_phys_to_virt((dma_addr_t)(uintptr_t)gpd, TXQ, ep_num);

		/* trying to give_back the request to gadget driver. */
		req = next_request(musb_ep);
		if (!req) {
			QMU_ERR(
				"[TXD]%s Cannot get next request of %d, but QMU has done.\n",
					__func__, ep_num);
			return;
		}
		request = &req->request;

		Tx_gpd_last[ep_num] = gpd;
		Tx_gpd_free_count[ep_num]++;
		musb_g_giveback(musb_ep, request, 0);
		req = next_request(musb_ep);
		if (req != NULL)
			request = &req->request;
	}

	if (gpd != gpd_current && TGPD_IS_FLAGS_HWO(gpd)) {

		QMU_ERR("[TXD][ERROR]EP%d TQCSR=%x, TQSAR=%x, TQCPR=%x\n"
				"[RXD][ERROR]QCR0=%x, QCR2=%x, QCR3=%x, QGCSR=%x\n"
				"[TXD][ERROR]HWO=%d, BPD=%d, Next_GPD=%p, DataBuffer=%p, BufferLen=%d, Endpoint=%d\n",
			ep_num,
			MGC_ReadQMU32(base, MGC_O_QMU_TQCSR(ep_num)),
			MGC_ReadQMU32(base, MGC_O_QMU_TQSAR(ep_num)),
			MGC_ReadQMU32(base, MGC_O_QMU_TQCPR(ep_num)),
			MGC_ReadQMU32(base, MGC_O_QMU_QCR0),
			MGC_ReadQMU32(base, MGC_O_QMU_QCR2),
			MGC_ReadQMU32(base, MGC_O_QMU_QCR3),
			MGC_ReadQUCS32(base, MGC_O_QUCS_USBGCSR),
			(u32) TGPD_GET_FLAG(gpd), (u32) TGPD_GET_FORMAT(gpd),
			TGPD_GET_NEXT_TX(gpd), TGPD_GET_DATA_TX(gpd),
			(u32) TGPD_GET_BUF_LEN(gpd),
			(u32) TGPD_GET_EPaddr(gpd));
	}

	QMU_INFO("[TXD]%s EP%d, Last=%p, End=%p, complete\n", __func__,
		 ep_num, Tx_gpd_last[ep_num], Tx_gpd_end[ep_num]);


#ifndef CONFIG_MTK_MUSB_QMU_PURE_ZLP_SUPPORT
	/* special case handle for zero request , only solve 1 zlp case */
	if (req != NULL) {
		if (request->length == 0) {

			QMU_WARN("[TXD]==Send ZLP== %p\n", req);
			musb_tx_zlp_qmu(musb, req->epnum);

			QMU_WARN(
				"[TXD]Giveback ZLP of EP%d, actual:%d, length:%d %p\n"
				, req->epnum,
				request->actual,
				request->length, request);
			musb_g_giveback(musb_ep, request, 0);
		}
	}
#endif
}

void flush_ep_csr(struct musb *musb, u8 ep_num, u8 isRx)
{
	void __iomem *mbase = musb->mregs;
	struct musb_hw_ep *hw_ep = musb->endpoints + ep_num;
	void __iomem *epio = hw_ep->regs;
	u16 csr, wCsr;

	if (epio == NULL)
		QMU_ERR("epio == NULL\n");

	if (isRx) {
		csr = musb_readw(epio, MUSB_RXCSR);
		csr |= MUSB_RXCSR_FLUSHFIFO | MUSB_RXCSR_RXPKTRDY;
		if (musb->is_host)
			csr &= ~MUSB_RXCSR_H_REQPKT;

		/* write 2x to allow double buffering */
		/* CC: see if some check is necessary */
		musb_writew(epio, MUSB_RXCSR, csr);
		musb_writew(epio, MUSB_RXCSR, csr | MUSB_RXCSR_CLRDATATOG);
	} else {
		csr = musb_readw(epio, MUSB_TXCSR);

		/* force flush without checking MUSB_TXCSR_TXPKTRDY */
		wCsr = csr | MUSB_TXCSR_FLUSHFIFO | MUSB_TXCSR_TXPKTRDY;
		musb_writew(epio, MUSB_TXCSR, wCsr);

		csr |= MUSB_TXCSR_FLUSHFIFO & ~MUSB_TXCSR_TXPKTRDY;
		musb_writew(epio, MUSB_TXCSR, csr);
		musb_writew(epio, MUSB_TXCSR, csr | MUSB_TXCSR_CLRDATATOG);
		/* CC: why is this special? */
		musb_writew(mbase, MUSB_INTRTX, 1 << ep_num);
	}
}

void mtk_disable_q(struct musb *musb, u8 ep_num, u8 isRx)
{
	void __iomem *mbase = musb->mregs;
	struct musb_hw_ep *hw_ep = musb->endpoints + ep_num;
	void __iomem *epio = hw_ep->regs;
	u16 csr;

	mtk_qmu_disable(ep_num, isRx);
	qmu_reset_gpd_pool(ep_num, isRx);

	musb_ep_select(mbase, ep_num);
	if (isRx) {
		csr = musb_readw(epio, MUSB_RXCSR);
		csr &= ~MUSB_RXCSR_DMAENAB;
		musb_writew(epio, MUSB_RXCSR, csr);
		flush_ep_csr(musb, ep_num, isRx);
	} else {
		csr = musb_readw(epio, MUSB_TXCSR);
		csr &= ~MUSB_TXCSR_DMAENAB;
		musb_writew(epio, MUSB_TXCSR, csr);
		flush_ep_csr(musb, ep_num, isRx);
	}
}

void h_qmu_done_rx(struct musb *musb, u8 ep_num)
{
	void __iomem *base = qmu_base;

	struct TGPD *gpd = Rx_gpd_last[ep_num];
	struct TGPD *gpd_current =
		(struct TGPD *)(uintptr_t)MGC_ReadQMU32
				(base, MGC_O_QMU_RQCPR(ep_num));
	struct musb_hw_ep	*hw_ep = musb->endpoints + ep_num;
	struct musb_qh	*qh = hw_ep->in_qh;
	struct urb	*urb = NULL;
	bool done = true;

	if (unlikely(!qh)) {
		DBG(0, "hw_ep:%d, QH NULL\n", ep_num);
		return;
	}

	urb = next_urb(qh);
	if (unlikely(!urb)) {
		DBG(0, "hw_ep:%d, !URB\n", ep_num);
		musb_advance_schedule
			(musb, (struct urb *)QH_FREE_RESCUE_INTERRUPT
			, hw_ep, USB_DIR_IN);
		return;
	}

	/*Transfer PHY addr got from QMU register to VIR addr*/
	gpd_current =
		(struct TGPD *)gpd_phys_to_virt
		((dma_addr_t)(uintptr_t)gpd_current, RXQ, ep_num);

	QMU_INFO("[RXD]%s EP%d, Last=%p, Current=%p, End=%p\n",
				__func__, ep_num,
				gpd, gpd_current,
				Rx_gpd_end[ep_num]);

	/* gpd_current should at least
	 * point to the next GPD to
	 * the previous last one
	 */
	if (gpd == gpd_current) {
		QMU_ERR("[RXD][ERROR] gpd(%p) == gpd_current(%p)\n"
				"[RXD][ERROR]EP%d RQCSR=%x, RQSAR=%x, RQCPR=%x, RQLDPR=%x\n"
				"[RXD][ERROR]QCR0=%x, QCR2=%x, QCR3=%x, QGCSR=%x\n"
				"[RX]HWO=%d, Next_GPD=%p ,BufLen=%d, Buf=%p, RLen=%d, EP=%d\n",
				gpd, gpd_current, ep_num,
				MGC_ReadQMU32(base, MGC_O_QMU_RQCSR(ep_num)),
				MGC_ReadQMU32(base, MGC_O_QMU_RQSAR(ep_num)),
				MGC_ReadQMU32(base, MGC_O_QMU_RQCPR(ep_num)),
				MGC_ReadQMU32(base, MGC_O_QMU_RQLDPR(ep_num)),
				MGC_ReadQMU32(base, MGC_O_QMU_QCR0),
				MGC_ReadQMU32(base, MGC_O_QMU_QCR2),
				MGC_ReadQMU32(base, MGC_O_QMU_QCR3),
				MGC_ReadQUCS32(base, MGC_O_QUCS_USBGCSR),
				(u32)TGPD_GET_FLAG(gpd), TGPD_GET_NEXT_RX(gpd),
				(u32)TGPD_GET_DataBUF_LEN(gpd),
				TGPD_GET_DATA_RX(gpd),
				(u32)TGPD_GET_BUF_LEN(gpd),
				(u32)TGPD_GET_EPaddr(gpd));
		return;
	}

	if (!gpd || !gpd_current) {
		QMU_ERR(
				"[RXD][ERROR] EP%d, gpd=%p, gpd_current=%p, ishwo=%d, rx_gpd_last=%p, RQCPR=0x%x\n"
				, ep_num, gpd, gpd_current,
				((gpd == NULL) ? 999 : TGPD_IS_FLAGS_HWO(gpd)),
				Rx_gpd_last[ep_num],
				MGC_ReadQMU32(base, MGC_O_QMU_RQCPR(ep_num)));
		return;
	}

	if (TGPD_IS_FLAGS_HWO(gpd)) {
		QMU_ERR("[RXD][ERROR]HWO=1!!\n");
		return;
	}

	/* NORMAL EXEC FLOW */
	while (gpd != gpd_current && !TGPD_IS_FLAGS_HWO(gpd)) {
		u32 rcv_len = (u32)TGPD_GET_BUF_LEN(gpd);

		urb = next_urb(qh);
		if (!urb) {
			DBG(4, "extra RX%d ready\n", ep_num);
			return;
		}

		if (!TGPD_GET_NEXT_RX(gpd) || !TGPD_GET_DATA_RX(gpd)) {
			QMU_ERR("[RXD][ERROR] EP%d ,gpd=%p\n", ep_num, gpd);
			return;
		}
		if (usb_pipebulk(urb->pipe)
				&& urb->transfer_buffer_length >=
					QMU_RX_SPLIT_THRE
				&& usb_pipein(urb->pipe)) {
			urb->actual_length += TGPD_GET_BUF_LEN(gpd);
			qh->offset += TGPD_GET_BUF_LEN(gpd);
			qh->iso_idx++;
			done = (qh->iso_idx ==
					urb->number_of_packets) ? true : false;
		} else if (usb_pipeisoc(urb->pipe)) {
			struct usb_iso_packet_descriptor	*d;

			d = urb->iso_frame_desc + qh->iso_idx;
			d->actual_length = rcv_len;
			d->status = 0;
			urb->actual_length += rcv_len;
			qh->offset += TGPD_GET_BUF_LEN(gpd);
			qh->iso_idx++;
			done =
				(qh->iso_idx == urb->number_of_packets)
								? true : false;
		} else {
			urb->actual_length = TGPD_GET_BUF_LEN(gpd);
			qh->offset = TGPD_GET_BUF_LEN(gpd);
			done = true;
		}

		gpd = TGPD_GET_NEXT_RX(gpd);

		gpd = gpd_phys_to_virt((dma_addr_t)(uintptr_t)gpd, RXQ, ep_num);
		DBG(4, "gpd = %p ep_num = %d\n", gpd, ep_num);
		if (!gpd) {
			pr_notice("[RXD][ERROR]%s EP%d ,gpd=%p\n"
					, __func__, ep_num, gpd);
			return;
		}
		DBG(4, "gpd = %p ep_num = %d\n", gpd, ep_num);
		Rx_gpd_last[ep_num] = gpd;
		Rx_gpd_free_count[ep_num]++;
		DBG(4, "gpd = %p ep_num = %d\n"
				"hw_ep = %p\n",
				gpd, ep_num, hw_ep);

		if (done) {
			if (musb_ep_get_qh(hw_ep, USB_DIR_IN))
				qh->iso_idx = 0;

			musb_advance_schedule(musb, urb, hw_ep, USB_DIR_IN);

			if (!hw_ep->in_qh) {
				DBG(0,
					"hw_ep:%d, QH NULL after advance_schedule\n"
					, ep_num);
				return;
			}
		}
	}
	/* QMU should keep take HWO gpd , so there is error*/
	if (gpd != gpd_current && TGPD_IS_FLAGS_HWO(gpd)) {
		QMU_ERR("[RXD][ERROR]gpd=%p\n"
				"[RXD][ERROR]EP%d RQCSR=%x, RQSAR=%x, RQCPR=%x, RQLDPR=%x\n"
				"[RXD][ERROR]QCR0=%x, QCR2=%x, QCR3=%x, QGCSR=%x\n"
				"[RX]HWO=%d, Next_GPD=%p ,BufLen=%d, Buf=%p, RLen=%d, EP=%d\n"
				, gpd, ep_num,
				MGC_ReadQMU32(base, MGC_O_QMU_RQCSR(ep_num)),
				MGC_ReadQMU32(base, MGC_O_QMU_RQSAR(ep_num)),
				MGC_ReadQMU32(base, MGC_O_QMU_RQCPR(ep_num)),
				MGC_ReadQMU32(base, MGC_O_QMU_RQLDPR(ep_num)),
				MGC_ReadQMU32(base, MGC_O_QMU_QCR0),
				MGC_ReadQMU32(base, MGC_O_QMU_QCR2),
				MGC_ReadQMU32(base, MGC_O_QMU_QCR3),
				MGC_ReadQUCS32(base, MGC_O_QUCS_USBGCSR),
				(u32)TGPD_GET_FLAG(gpd),
				TGPD_GET_NEXT_RX(gpd),
				(u32)TGPD_GET_DataBUF_LEN(gpd),
				TGPD_GET_DATA_RX(gpd),
				(u32)TGPD_GET_BUF_LEN(gpd),
				(u32)TGPD_GET_EPaddr(gpd));
	}

	QMU_INFO("[RXD]%s EP%d, Last=%p, End=%p, complete\n"
		, __func__,	ep_num, Rx_gpd_last[ep_num]
		, Rx_gpd_end[ep_num]);
}

void h_qmu_done_tx(struct musb *musb, u8 ep_num)
{
	void __iomem *base = qmu_base;
	struct TGPD *gpd = Tx_gpd_last[ep_num];
	struct TGPD *gpd_current =
		(struct TGPD *)(uintptr_t)MGC_ReadQMU32
				(base, MGC_O_QMU_TQCPR(ep_num));
	struct musb_hw_ep	*hw_ep = musb->endpoints + ep_num;
	struct musb_qh	*qh = hw_ep->out_qh;
	struct urb	*urb = NULL;
	bool done = true;

	if (unlikely(!qh)) {
		DBG(0, "hw_ep:%d, QH NULL\n", ep_num);
		return;
	}

	urb = next_urb(qh);
	if (unlikely(!urb)) {
		DBG(0, "hw_ep:%d, !URB\n", ep_num);
		musb_advance_schedule(musb,
			(struct urb *)QH_FREE_RESCUE_INTERRUPT,
							hw_ep, USB_DIR_OUT);
		return;
	}

	/*Transfer PHY addr got from QMU register to VIR addr*/
	gpd_current =
		gpd_phys_to_virt((dma_addr_t)(uintptr_t)gpd_current,
						TXQ, ep_num);

	QMU_INFO("[TXD]%s EP%d, Last=%p, Current=%p, End=%p\n",
				__func__,
				ep_num, gpd, gpd_current,
				Tx_gpd_end[ep_num]);

	/* gpd_current should at
	 * least point to the next
	 * GPD to the previous last one.
	 */
	if (gpd == gpd_current)
		return;

	if (TGPD_IS_FLAGS_HWO(gpd)) {
		QMU_ERR("[TXD] HWO=1, CPR=%x\n",
			MGC_ReadQMU32(base, MGC_O_QMU_TQCPR(ep_num)));
		return;
	}

	/* NORMAL EXEC FLOW */
	while (gpd != gpd_current && !TGPD_IS_FLAGS_HWO(gpd)) {
		QMU_INFO(
			"[TXD]gpd=%p ->HWO=%d, BPD=%d, Next_GPD=%p, DataBuffer=%p, BufferLen=%d\n"
			, gpd, (u32)TGPD_GET_FLAG(gpd)
			, (u32)TGPD_GET_FORMAT(gpd),
			TGPD_GET_NEXT_TX(gpd),
			TGPD_GET_DATA_TX(gpd),
			(u32)TGPD_GET_BUF_LEN(gpd));

		if (!TGPD_GET_NEXT_TX(gpd)) {
			QMU_ERR("[TXD][ERROR]Next GPD is null!!\n");
			break;
		}

		urb = next_urb(qh);
		if (!urb) {
			QMU_ERR("extra TX%d ready\n", ep_num);
			return;
		}

		if (!TGPD_GET_NEXT_TX(gpd) || !TGPD_GET_DATA_TX(gpd)) {
			QMU_ERR("[RXD][ERROR] EP%d ,gpd=%p\n", ep_num, gpd);
			return;
		}

		if (usb_pipebulk(urb->pipe)
				&& urb->transfer_buffer_length >=
					QMU_RX_SPLIT_THRE
				&& usb_pipeout(urb->pipe)) {
			QMU_WARN("bulk???\n");
			urb->actual_length += TGPD_GET_BUF_LEN(gpd);
			qh->offset += TGPD_GET_BUF_LEN(gpd);
			qh->iso_idx++;
			done = (qh->iso_idx == urb->number_of_packets)
					? true : false;
		} else if (usb_pipeisoc(urb->pipe)) {
			struct usb_iso_packet_descriptor	*d;

			d = urb->iso_frame_desc + qh->iso_idx;
			d->actual_length = TGPD_GET_BUF_LEN(gpd);
			d->status = 0;
			urb->actual_length += TGPD_GET_BUF_LEN(gpd);
			qh->offset += TGPD_GET_BUF_LEN(gpd);
			qh->iso_idx++;
			done = (qh->iso_idx == urb->number_of_packets)
					? true : false;
		} else {
			QMU_WARN("others use qmu???\n");
			urb->actual_length = TGPD_GET_BUF_LEN(gpd);
			qh->offset = TGPD_GET_BUF_LEN(gpd);
			done = true;
		}
		gpd = TGPD_GET_NEXT_TX(gpd);
		gpd = gpd_phys_to_virt((dma_addr_t)(uintptr_t)gpd, TXQ, ep_num);
		Tx_gpd_last[ep_num] = gpd;
		Tx_gpd_free_count[ep_num]++;

		if (done) {
			if (musb_ep_get_qh(hw_ep, USB_DIR_OUT))
				qh->iso_idx = 0;

			musb_advance_schedule(musb, urb, hw_ep, USB_DIR_OUT);

			if (!hw_ep->out_qh) {
				DBG(0,
					"hw_ep:%d, QH NULL after advance_schedule\n"
						, ep_num);
				return;
			}
		}
	}
}

#define MUSB_HOST_QMU_AEE_STR_SZ 64
void mtk_qmu_host_rx_err(struct musb *musb, u8 epnum)
{
	struct urb *urb;
	u16 rx_csr, val;
	struct musb_hw_ep *hw_ep = musb->endpoints + epnum;
	void __iomem *epio = hw_ep->regs;
	struct musb_qh *qh = hw_ep->in_qh;
	bool done = false;
	u32 status = 0;
	void __iomem *mbase = musb->mregs;

	musb_ep_select(mbase, epnum);
	rx_csr = musb_readw(epio, MUSB_RXCSR);
	val = rx_csr;

	if (!qh) {
		DBG(0, "!QH for ep %d\n", epnum);
		goto finished;
	}

	urb = next_urb(qh);
	status = 0;

	if (unlikely(!urb)) {
		/* REVISIT -- THIS SHOULD NEVER HAPPEN ... but, at least
		 * usbtest #11 (unlinks) triggers it regularly, sometimes
		 * with fifo full.  (Only with DMA??)
		 */
		DBG(0, "BOGUS RX%d ready, csr %04x, count %d\n"
			, epnum, val,
		    musb_readw(epio, MUSB_RXCOUNT));
		musb_h_flush_rxfifo(hw_ep, 0);
		goto finished;
	}

	DBG(0, "<== hw %d rxcsr %04x, urb actual %d\n",
	    epnum, rx_csr, urb->actual_length);

	/*
	 * check for errors, concurrent stall
	 * & unlink is not really handled yet!
	 */
	if (rx_csr & MUSB_RXCSR_H_RXSTALL) {
		DBG(0, "RX end %d STALL\n", epnum);

		/* handle stall in MAC */
		rx_csr &= ~MUSB_RXCSR_H_RXSTALL;
		musb_writew(epio, MUSB_RXCSR, rx_csr);

		/* stall; record URB status */
		status = -EPIPE;

	} else if (rx_csr & MUSB_RXCSR_H_ERROR) {
		DBG(0, "end %d RX proto error,rxtoggle=0x%x\n", epnum,
		    musb_readl(mbase, MUSB_RXTOG));

		status = -EPROTO;
		musb_writeb(epio, MUSB_RXINTERVAL, 0);

	} else if (rx_csr & MUSB_RXCSR_DATAERROR) {

		DBG(0, "RX end %d ISO data error\n", epnum);
	} else if (rx_csr & MUSB_RXCSR_INCOMPRX) {
		DBG(0, "end %d high bandwidth incomplete ISO packet RX\n"
			, epnum);
		status = -EPROTO;
	}

	/* faults abort the transfer */
	if (status) {
		musb_h_flush_rxfifo(hw_ep, 0);
		musb_writeb(epio, MUSB_RXINTERVAL, 0);
		done = true;
	}

	if (done)
		DBG(0,
			"FIXME!!!, to be implemented, related HW/SW abort procedure\n");

finished:
	{
		/* must use static string for AEE usage */
		static char string[MUSB_HOST_QMU_AEE_STR_SZ];

		sprintf(string, "USB20_HOST, RXQ<%d> ERR, CSR:%x", epnum, val);
		QMU_ERR("%s\n", string);
#ifdef CONFIG_MEDIATEK_SOLUTION
		{
			u16 skip_val;

			skip_val = val &
				(MUSB_RXCSR_INCOMPRX
				 |MUSB_RXCSR_DATAERROR
				 |MUSB_RXCSR_PID_ERR);

			/* filter specific value to prevent false alarm */
			switch (val) {
			case 0x2020:
			case 0x2003:
				val = 0;
				QMU_ERR("force val to 0 for bypass AEE\n");
				break;
			default:
				break;
			}

#ifdef CONFIG_MTK_AEE_FEATURE
			if (val && !skip_val)
				aee_kernel_warning(string, string);
#endif
		}
#endif
	}
}

void mtk_qmu_host_tx_err(struct musb *musb, u8 epnum)
{
	struct urb *urb;
	u16 tx_csr, val;
	struct musb_hw_ep *hw_ep = musb->endpoints + epnum;
	void __iomem *epio = hw_ep->regs;
	struct musb_qh *qh = hw_ep->out_qh;
	bool done = false;
	u32 status = 0;
	void __iomem *mbase = musb->mregs;

	musb_ep_select(mbase, epnum);
	tx_csr = musb_readw(epio, MUSB_TXCSR);
	val = tx_csr;

	if (!qh) {
		DBG(0, "!QH for ep %d\n", epnum);
		goto finished;
	}

	urb = next_urb(qh);
	/* with CPPI, DMA sometimes triggers "extra" irqs */
	if (!urb) {
		DBG(0, "extra TX%d ready, csr %04x\n", epnum, tx_csr);
		goto finished;
	}

	DBG(0, "OUT/TX%d end, csr %04x\n", epnum, tx_csr);

	/* check for errors */
	if (tx_csr & MUSB_TXCSR_H_RXSTALL) {
		/* dma was disabled, fifo flushed */
		DBG(0, "TX end %d stall\n", epnum);

		/* stall; record URB status */
		status = -EPIPE;

	} else if (tx_csr & MUSB_TXCSR_H_ERROR) {
		/* (NON-ISO) dma was disabled, fifo flushed */
		DBG(0, "TX 3strikes on ep=%d\n", epnum);

		status = -ETIMEDOUT;
	} else if (tx_csr & MUSB_TXCSR_H_NAKTIMEOUT) {
		DBG(0, "TX end=%d device not responding\n", epnum);

		/* NOTE:  this code path would be a good place to PAUSE a
		 * transfer, if there's some other (nonperiodic) tx urb
		 * that could use this fifo.  (dma complicates it...)
		 * That's already done for bulk RX transfers.
		 *
		 * if (bulk && qh->ring.next != &musb->out_bulk), then
		 * we have a candidate... NAKing is *NOT* an error
		 */
		musb_ep_select(mbase, epnum);
		musb_writew(epio,
			MUSB_TXCSR, MUSB_TXCSR_H_WZC_BITS
			| MUSB_TXCSR_TXPKTRDY);
		return;
	}

/* done: */
	if (status) {
		tx_csr &= ~(MUSB_TXCSR_AUTOSET
			    | MUSB_TXCSR_DMAENAB
			    | MUSB_TXCSR_H_ERROR
			    | MUSB_TXCSR_H_RXSTALL
			    | MUSB_TXCSR_H_NAKTIMEOUT);

		musb_ep_select(mbase, epnum);
		musb_writew(epio, MUSB_TXCSR, tx_csr);
		/* REVISIT may need to clear FLUSHFIFO ... */
		musb_writew(epio, MUSB_TXCSR, tx_csr);
		musb_writeb(epio, MUSB_TXINTERVAL, 0);

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

	if (done)
		DBG(0,
			"FIXME!!!, to be implemented, related HW/SW abort procedure\n");

finished:
	{
		/* must use static string for AEE usage */
		static char string[MUSB_HOST_QMU_AEE_STR_SZ];

		sprintf(string, "USB20_HOST, TXQ<%d> ERR, CSR:%x", epnum, val);
		QMU_ERR("%s\n", string);
#ifdef CONFIG_MTK_AEE_FEATURE
		aee_kernel_warning(string, string);
#endif
	}

}

static void flush_urb_status(struct musb_qh *qh, struct urb *urb)
{
	qh->iso_idx = 0;
	qh->offset = 0;
	urb->actual_length = 0;
	urb->status = -EINPROGRESS;
	if (qh->type == USB_ENDPOINT_XFER_ISOC) {
		struct usb_iso_packet_descriptor *d;
		int index;

		for (index = 0; index < urb->number_of_packets; index++) {
			d = urb->iso_frame_desc + qh->iso_idx;
			d->actual_length = 0;
			d->status = -EXDEV;
		}
	}
}

static void mtk_qmu_host_err(struct musb *musb, u8 ep_num, u8 isRx)
{
	struct urb *urb;
	struct musb_hw_ep *hw_ep = musb->endpoints + ep_num;
	struct musb_qh			*qh;
	struct usb_host_endpoint	*hep;

	if (!mtk_host_qmu_force_isoc_restart)
		goto normal_handle;

	if (isRx)
		qh = hw_ep->in_qh;
	else
		qh = hw_ep->out_qh;

	hep = qh->hep;
	/* same action as musb_flush_qmu */
	mtk_qmu_stop(ep_num, isRx);
	qmu_reset_gpd_pool(ep_num, isRx);

	urb = next_urb(qh);
	if (unlikely(!urb)) {
		QMU_WARN("No URB.\n");
		return;
	}

	flush_ep_csr(musb, ep_num, isRx);
	if (usb_pipeisoc(urb->pipe)) {
		mtk_qmu_enable(musb, ep_num, isRx);
		list_for_each_entry(urb, &hep->urb_list, urb_list) {
			QMU_WARN("%s qh:0x%p flush and kick urb:0x%p\n"
				, __func__, qh, urb);
			flush_urb_status(qh, urb);
			mtk_kick_CmdQ(musb, isRx, qh, urb);
		}
		return;
	}

normal_handle:
	if (isRx)
		mtk_qmu_host_rx_err(musb, ep_num);
	else
		mtk_qmu_host_tx_err(musb, ep_num);
}
void mtk_err_recover(struct musb *musb, u8 ep_num, u8 isRx, bool is_len_err)
{
	struct musb_ep *musb_ep;
	struct musb_request *request;

	if (musb->is_host) {
		mtk_qmu_host_err(musb, ep_num, isRx);
		return;
	}

	/* same action as musb_flush_qmu */
	mtk_qmu_stop(ep_num, isRx);
	qmu_reset_gpd_pool(ep_num, isRx);

	/* same action as musb_restart_qmu */
	flush_ep_csr(musb, ep_num, isRx);
	mtk_qmu_enable(musb, ep_num, isRx);

	if (isRx)
		musb_ep = &musb->endpoints[ep_num].ep_out;
	else
		musb_ep = &musb->endpoints[ep_num].ep_in;

	/* requeue all req , basically the same as musb_kick_D_CmdQ */
	list_for_each_entry(request, &musb_ep->req_list, list) {
		QMU_ERR("request 0x%p length(%d) len_err(%d)\n"
			, request, request->request.length,
			is_len_err);

		if (request->request.dma != DMA_ADDR_INVALID) {
			if (request->tx) {
				QMU_ERR(
				"[TX] gpd=%p, epnum=%d,len=%d zero=%d\n"
					, Tx_gpd_end[ep_num]
					, ep_num
					, request->request.length
					, request->request.zero);
				request->request.actual =
					request->request.length;
#ifdef CONFIG_MTK_MUSB_QMU_PURE_ZLP_SUPPORT
				if (request->request.length >= 0) {
#else
				if (request->request.length > 0) {
#endif
					mtk_qmu_insert_task(request->epnum,
						isRx,
						request->request.dma,
						request->request.length,
						((request->request.zero
							== 1) ? 1 : 0), 1);

#ifndef CONFIG_MTK_MUSB_QMU_PURE_ZLP_SUPPORT
				} else if (request->request.length == 0) {
					/* this case may be a problem */
					QMU_ERR(
					"[TX]Send ZLP cases,may be a problem!!!\n");
					musb_tx_zlp_qmu(musb, request->epnum);
					musb_g_giveback(musb_ep
						, &(request->request), 0);
#endif
				} else {
					QMU_ERR(
						"ERR, TX, request->request.length(%d)\n"
						, request->request.length);
				}
			} else {
				QMU_ERR("[RX] gpd=%p, epnum=%d, len=%d\n",
					Rx_gpd_end[ep_num], ep_num,
					request->request.length);
					mtk_qmu_insert_task(request->epnum,
					    isRx,
					    request->request.dma,
					    request->request.length,
				    ((request->request.zero == 1) ? 1 : 0), 1);
			}
		}
	}
	pr_notice("RESUME QMU\n");
	/* RESUME QMU */
	mtk_qmu_resume(ep_num, isRx);
}

void mtk_qmu_irq_err(struct musb *musb, u32 qisar)
{
	u8 i;
	u32 wQmuVal;
	u32 wRetVal;
	void __iomem *base = qmu_base;
	u8 rx_err_ep_num = 0; /*RX & TX would be occur the same time*/
	u8 tx_err_ep_num = 0;
	bool is_len_err = false;

	wQmuVal = qisar;

	/* RXQ ERROR */
	if (wQmuVal & DQMU_M_RXQ_ERR) {
		wRetVal =
		    MGC_ReadQIRQ32(base,
			   MGC_O_QIRQ_RQEIR)
			   & (~(MGC_ReadQIRQ32(base, MGC_O_QIRQ_RQEIMR)));
		QMU_ERR("RQ error in QMU mode![0x%x]\n", wRetVal);

		for (i = 1; i <= RXQ_NUM; i++) {
			if (wRetVal & DQMU_M_RX_GPDCS_ERR(i)) {
				QMU_ERR("RQ %d GPD checksum error!\n", i);
				rx_err_ep_num = i;
			}
			if (wRetVal & DQMU_M_RX_LEN_ERR(i)) {
				QMU_ERR("RQ %d receive length error!\n", i);
				rx_err_ep_num = i;
				is_len_err = true;
			}
			if (wRetVal & DQMU_M_RX_ZLP_ERR(i))
				QMU_ERR("RQ %d receive an zlp packet!\n", i);
		}
		MGC_WriteQIRQ32(base, MGC_O_QIRQ_RQEIR, wRetVal);
	}

	/* TXQ ERROR */
	if (wQmuVal & DQMU_M_TXQ_ERR) {
		wRetVal =
		    MGC_ReadQIRQ32(base,
				   MGC_O_QIRQ_TQEIR)
			   & (~(MGC_ReadQIRQ32(base, MGC_O_QIRQ_TQEIMR)));
		QMU_ERR("TQ error in QMU mode![0x%x]\n", wRetVal);

		for (i = 1; i <= RXQ_NUM; i++) {
			if (wRetVal & DQMU_M_TX_BDCS_ERR(i)) {
				QMU_ERR("TQ %d BD checksum error!\n", i);
				tx_err_ep_num = i;
			}
			if (wRetVal & DQMU_M_TX_GPDCS_ERR(i)) {
				QMU_ERR("TQ %d GPD checksum error!\n", i);
				tx_err_ep_num = i;
			}
			if (wRetVal & DQMU_M_TX_LEN_ERR(i)) {
				QMU_ERR("TQ %d buffer length error!\n", i);
				tx_err_ep_num = i;
				is_len_err = true;
			}
		}
		MGC_WriteQIRQ32(base, MGC_O_QIRQ_TQEIR, wRetVal);
	}

	/* RX EP ERROR */
	if (wQmuVal & DQMU_M_RXEP_ERR) {
		wRetVal =
		    MGC_ReadQIRQ32(base,
				   MGC_O_QIRQ_REPEIR) &
		    (~(MGC_ReadQIRQ32(base, MGC_O_QIRQ_REPEIMR)));
		QMU_ERR("Rx endpoint error in QMU mode![0x%x]\n", wRetVal);

		for (i = 1; i <= RXQ_NUM; i++) {
			if (wRetVal & DQMU_M_RX_EP_ERR(i)) {
				QMU_ERR("RX EP %d ERR\n", i);
				rx_err_ep_num = i;
			}
		}

		MGC_WriteQIRQ32(base, MGC_O_QIRQ_REPEIR, wRetVal);
	}

	/* TX EP ERROR */
	if (wQmuVal & DQMU_M_TXEP_ERR) {
		wRetVal =
		    MGC_ReadQIRQ32(base,
				   MGC_O_QIRQ_TEPEIR) &
		    (~(MGC_ReadQIRQ32(base, MGC_O_QIRQ_TEPEIMR)));
		QMU_ERR("Tx endpoint error in QMU mode![0x%x]\n", wRetVal);

		for (i = 1; i <= TXQ_NUM; i++) {
			if (wRetVal & DQMU_M_TX_EP_ERR(i)) {
				QMU_ERR("TX EP %d ERR\n", i);
				tx_err_ep_num = i;
			}
		}

		MGC_WriteQIRQ32(base, MGC_O_QIRQ_TEPEIR, wRetVal);
	}

	/* RXQ EMPTY */
	if (wQmuVal & DQMU_M_RQ_EMPTY) {
		wRetVal = MGC_ReadQIRQ32(base, MGC_O_QIRQ_REPEMPR)
		    & (~(MGC_ReadQIRQ32(base, MGC_O_QIRQ_REPEMPMR)));
		QMU_ERR("RQ Empty in QMU mode![0x%x]\n", wRetVal);

		for (i = 1; i <= RXQ_NUM; i++) {
			if (wRetVal & DQMU_M_RX_EMPTY(i))
				QMU_ERR("RQ %d Empty!\n", i);
		}

		MGC_WriteQIRQ32(base, MGC_O_QIRQ_REPEMPR, wRetVal);
	}

	/* TXQ EMPTY */
	if (wQmuVal & DQMU_M_TQ_EMPTY) {
		wRetVal = MGC_ReadQIRQ32(base, MGC_O_QIRQ_TEPEMPR)
		    & (~(MGC_ReadQIRQ32(base, MGC_O_QIRQ_TEPEMPMR)));
		QMU_ERR("TQ Empty in QMU mode![0x%x]\n", wRetVal);

		for (i = 1; i <= TXQ_NUM; i++) {
			if (wRetVal & DQMU_M_TX_EMPTY(i))
				QMU_ERR("TQ %d Empty!\n", i);
		}

		MGC_WriteQIRQ32(base, MGC_O_QIRQ_TEPEMPR, wRetVal);
	}

	/* QMU ERR RECOVER , only servie one ep error ? */
	if (rx_err_ep_num)
		mtk_err_recover(musb, rx_err_ep_num, 1, is_len_err);

	if (tx_err_ep_num)
		mtk_err_recover(musb, tx_err_ep_num, 0, is_len_err);
}
