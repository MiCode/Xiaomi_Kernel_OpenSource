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

#include "mu3d_hal_osal.h"
#define _MTK_QMU_DRV_EXT_
#include "mu3d_hal_qmu_drv.h"
#undef _MTK_QMU_DRV_EXT_
#include "mu3d_hal_usb_drv.h"
#include "mu3d_hal_hw.h"

/**
 * get_bd - get a null bd
 * @args - arg1: dir, arg2: ep number
 */
struct TBD *get_bd(enum USB_DIR dir, unsigned int num)
{
	struct TBD *ptr;

	if (dir == USB_RX) {
		ptr = (struct TBD *) Rx_bd_List[num].pNext;

		os_printk(K_DEBUG, "%s Rx_bd_List[%d].pNext=%p\n", __func__, num,
			  (Rx_bd_List[num].pNext));

		if ((Rx_bd_List[num].pNext + 1) < Rx_bd_List[num].pEnd)
			Rx_bd_List[num].pNext++;
		else
			Rx_bd_List[num].pNext = Rx_bd_List[num].pStart;

	} else {
		ptr = (struct TBD *) Tx_bd_List[num].pNext;

		os_printk(K_DEBUG, "%s Tx_gpd_List[%d].pNext=%p\n", __func__, num,
			  (Tx_bd_List[num].pNext));

		Tx_bd_List[num].pNext++;
		Tx_bd_List[num].pNext = Tx_bd_List[num].pNext + AT_BD_EXT_LEN;

		if (Tx_bd_List[num].pNext >= Tx_bd_List[num].pEnd)
			Tx_bd_List[num].pNext = Tx_bd_List[num].pStart;
	}
	return ptr;
}

/**
 * get_bd - get a null gpd
 * @args - arg1: dir, arg2: ep number
 */
struct TGPD *get_gpd(enum USB_DIR dir, unsigned int num)
{
	struct TGPD *ptr;

	if (dir == USB_RX) {
		ptr = Rx_gpd_List[num].pNext;

		/* qmu_printk(K_DEBUG, "[RX]""GPD List[%d]->Next=%p\n", num, Rx_gpd_List[num].pNext); */

		Rx_gpd_List[num].pNext =
		    Rx_gpd_List[num].pNext + (AT_GPD_EXT_LEN / sizeof(struct TGPD) + 1);

		/* qmu_printk(K_DEBUG, "[Rx]""GPD List[%d]->Start=%p, Next=%p, End=%p\n", */
		/* num, Rx_gpd_List[num].pStart, Rx_gpd_List[num].pNext, Rx_gpd_List[num].pEnd); */

		if (Rx_gpd_List[num].pNext >= Rx_gpd_List[num].pEnd)
			Rx_gpd_List[num].pNext = Rx_gpd_List[num].pStart;

	} else {
		ptr = Tx_gpd_List[num].pNext;

		/* qmu_printk(K_DEBUG, "[TX]""GPD List[%d]->Next=%p\n", num, Tx_gpd_List[num].pNext); */

		/*
		 * Here is really tricky.
		 * The size of a GPD is 16 bytes. But the cache line size is 64B.
		 * If all GPDs are allocated continiously.
		 * When doing invalidating the cache. The size of 64B from the specified address would flush to
		 * the physical memory. This action may cause that other GPDs corrupted, like HWO=1 when receiving
		 * QMU Done interrupt. Current workaround is that let a GPD as 64 bytes. So the next
		 * GPD is behind 64bytes.
		 */
		Tx_gpd_List[num].pNext =
		    Tx_gpd_List[num].pNext + (AT_GPD_EXT_LEN / sizeof(struct TGPD) + 1);

		/* qmu_printk(K_DEBUG, "[TX]""GPD List[%d]->Start=%p, pNext=%p, pEnd=%p\n", */
		/* num, Tx_gpd_List[num].pStart, Tx_gpd_List[num].pNext, Tx_gpd_List[num].pEnd); */

		if (Tx_gpd_List[num].pNext >= Tx_gpd_List[num].pEnd)
			Tx_gpd_List[num].pNext = Tx_gpd_List[num].pStart;
	}
	return ptr;
}

/**
 * get_bd - align gpd ptr to target ptr
 * @args - arg1: dir, arg2: ep number, arg3: target ptr
 */
void gpd_ptr_align(enum USB_DIR dir, unsigned int num, struct TGPD *ptr)
{
	unsigned int run_next;

	run_next = true;

	/* qmu_printk(K_DEBUG,"%s %d, EP%d, ptr=%p\n", __func__, dir, num, ptr); */

	while (run_next) {
		if (ptr == get_gpd(dir, num))
			run_next = false;
	}
}

/**
 * bd_virt_to_phys - map bd virtual address to physical address
 * @args - arg1: virtual address, arg2: dir, arg3: ep number
 * @return - physical address
 */
dma_addr_t bd_virt_to_phys(void *vaddr, enum USB_DIR dir, unsigned int num)
{
	uintptr_t ptr;

	if (dir == USB_RX)
		ptr = rx_bd_map[num].p_desc_dma;
	else
		ptr = tx_bd_map[num].p_desc_dma;

	os_printk(K_DEBUG, "%s %s[%d]phys=%lx<->virt=%p\n", __func__,
		  ((dir == USB_RX) ? "RX" : "TX"), num, ptr, vaddr);

	return (dma_addr_t) ptr;
}

/**
 * bd_phys_to_virt - map bd physical address to virtual address
 * @args - arg1: physical address, arg2: dir, arg3: ep number
 * @return - virtual address
 */
void *bd_phys_to_virt(void *paddr, enum USB_DIR dir, unsigned int num)
{
	void *ptr;

	os_printk(K_DEBUG, "bd_phys_to_virt paddr=%p, num=%d\n", paddr, num);

	if (dir == USB_RX)
		ptr = rx_bd_map[num].p_desc;
	else
		ptr = tx_bd_map[num].p_desc;


	return ptr;
}

/**
 * mu3d_hal_gpd_virt_to_phys - map gpd virtual address to physical address
 * @args - arg1: virtual address, arg2: dir, arg3: ep number
 * @return - physical address
 */
dma_addr_t mu3d_hal_gpd_virt_to_phys(void *vaddr, enum USB_DIR dir, unsigned int num)
{
	uintptr_t ptr;

	if (dir == USB_RX)
		ptr = rx_gpd_map[num].p_desc_dma + (dma_addr_t) (vaddr - rx_gpd_map[num].p_desc);
	else
		ptr = tx_gpd_map[num].p_desc_dma + (dma_addr_t) (vaddr - tx_gpd_map[num].p_desc);

	os_printk(K_DEBUG, "%s %s[%d]phys=%lx<->virt=%p\n", __func__,
		  ((dir == USB_RX) ? "RX" : "TX"), num, ptr, vaddr);

	return (dma_addr_t) ptr;
}

/**
 * gpd_phys_to_virt - map gpd physical address to virtual address
 * @args - arg1: physical address, arg2: dir, arg3: ep number
 * @return - virtual address
 */
void *gpd_phys_to_virt(void *paddr, enum USB_DIR dir, unsigned int num)
{
	void *ptr;


	if (dir == USB_RX) {
		ptr =
		    (void *)((uintptr_t) rx_gpd_map[num].p_desc +
			     (uintptr_t) (paddr - rx_gpd_map[num].p_desc_dma));
	} else {
		ptr =
		    (void *)((uintptr_t) tx_gpd_map[num].p_desc +
			     (uintptr_t) (paddr - tx_gpd_map[num].p_desc_dma));
	}

	return ptr;
}

/**
 * init_bd_list - initialize bd management list
 * @args - arg1: dir, arg2: ep number, arg3: bd virtual addr, arg4: bd ioremap addr, arg5: bd number
 */
void init_bd_list(enum USB_DIR dir, int num, struct TBD *ptr, dma_addr_t io_ptr, unsigned int size)
{
	if (dir == USB_RX) {
		Rx_bd_List[num].pStart = ptr;
		Rx_bd_List[num].pEnd = (struct TBD *) (ptr + size);
		rx_bd_map[num].p_desc = (void *)ptr;
		rx_bd_map[num].p_desc_dma = io_ptr;
		ptr++;
		Rx_bd_List[num].pNext = ptr;

		os_printk(K_DEBUG, "Rx_bd_List[%d].pStart=%p, pNext=%p, pEnd=%p\n",
			  num, Rx_bd_List[num].pStart, Rx_bd_List[num].pNext, Rx_bd_List[num].pEnd);

		os_printk(K_DEBUG, "rx_bd_map[%d] vir=%p dma=%08llx\n", num,
			  rx_bd_map[num].p_desc, (unsigned long long)rx_bd_map[num].p_desc_dma);

		os_printk(K_DEBUG, "vir=%p dma=%08llx\n", ptr, (unsigned long long)io_ptr);
	} else {
		Tx_bd_List[num].pStart = ptr;
		Tx_bd_List[num].pEnd = (struct TBD *) ((unsigned char *) (ptr + size) + AT_BD_EXT_LEN * size);
		tx_bd_map[num].p_desc = (void *)ptr;
		tx_bd_map[num].p_desc_dma = io_ptr;
		ptr++;
		Tx_bd_List[num].pNext = (struct TBD *) ((unsigned char *) ptr + AT_BD_EXT_LEN);

		os_printk(K_DEBUG, "Tx_bd_List[%d].pStart=%p, pNext=%p, pEnd=%p\n",
			  num, Tx_bd_List[num].pStart, Tx_bd_List[num].pNext, Tx_bd_List[num].pEnd);

		os_printk(K_DEBUG, "tx_bd_map[%d] vir=%p dma=%08llx\n", num,
			  tx_bd_map[num].p_desc, (unsigned long long)tx_bd_map[num].p_desc_dma);

		os_printk(K_DEBUG, "vir=%p, dma=%08llx\n", ptr, (unsigned long long)io_ptr);
	}
}


/**
 * init_gpd_list - initialize gpd management list
 * @args - arg1: dir, arg2: ep number, arg3: gpd virtual addr, arg4: gpd ioremap addr, arg5: gpd number
 */
void init_gpd_list(enum USB_DIR dir, int num, struct TGPD *ptr, dma_addr_t io_ptr, unsigned int size)
{
	if (dir == USB_RX) {
		Rx_gpd_List[num].pStart = ptr;
		Rx_gpd_List[num].pEnd = (struct TGPD *) ((unsigned char *) (ptr + size) + AT_GPD_EXT_LEN * size);
		rx_gpd_map[num].p_desc = (void *)ptr;
		rx_gpd_map[num].p_desc_dma = io_ptr;
		ptr++;
		Rx_gpd_List[num].pNext = (struct TGPD *) ((unsigned char *) ptr + AT_GPD_EXT_LEN);

		qmu_printk(K_INFO, "Rx_gpd_List[%d].pStart=%p, pNext=%p, pEnd=%p\n",
			   num, Rx_gpd_List[num].pStart, Rx_gpd_List[num].pNext,
			   Rx_gpd_List[num].pEnd);

		qmu_printk(K_INFO, "rx_gpd_map[%d] vir=%p dma=%08llx\n", num,
			   rx_gpd_map[num].p_desc, (unsigned long long)rx_gpd_map[num].p_desc_dma);

		qmu_printk(K_INFO, "vir=%p, dma=%08llx\n", ptr, (unsigned long long)io_ptr);
	} else {
		Tx_gpd_List[num].pStart = ptr;
		Tx_gpd_List[num].pEnd = (struct TGPD *) ((unsigned char *) (ptr + size) + AT_GPD_EXT_LEN * size);
		tx_gpd_map[num].p_desc = (void *)ptr;
		tx_gpd_map[num].p_desc_dma = io_ptr;
		ptr++;
		Tx_gpd_List[num].pNext = (struct TGPD *) ((unsigned char *) ptr + AT_GPD_EXT_LEN);

		qmu_printk(K_INFO, "Tx_gpd_List[%d].pStart=%p, pNext=%p, pEnd=%p\n",
			   num, Tx_gpd_List[num].pStart, Tx_gpd_List[num].pNext,
			   Tx_gpd_List[num].pEnd);

		qmu_printk(K_INFO, "tx_gpd_map[%d] vir=%p dma=%08llx\n", num,
			   tx_gpd_map[num].p_desc, (unsigned long long)tx_gpd_map[num].p_desc_dma);

		qmu_printk(K_INFO, "vir=%p, dma=%08llx\n", ptr, (unsigned long long)io_ptr);
	}
}

/**
 * free_gpd - free gpd management list
 * @args - arg1: dir, arg2: ep number
 */
void free_gpd(enum USB_DIR dir, int num)
{
	if (dir == USB_RX) {
		os_memset(Rx_gpd_List[num].pStart, 0,
			  MAX_GPD_NUM * (sizeof(struct TGPD) + AT_GPD_EXT_LEN));
	} else {
		os_memset(Tx_gpd_List[num].pStart, 0,
			  MAX_GPD_NUM * (sizeof(struct TGPD) + AT_GPD_EXT_LEN));
	}
}

/**
 * mu3d_hal_alloc_qmu_mem - allocate gpd and bd memory for all ep
 *
 */
/* USBIF */

static dma_addr_t Tx_gpd_ioptr[15];
static dma_addr_t Rx_gpd_ioptr[15];

void _ex_mu3d_hal_free_qmu_mem(struct device *dev)
{
	unsigned int i;
	unsigned int size = (sizeof(struct TGPD) + AT_GPD_EXT_LEN) * MAX_GPD_NUM;

	qmu_printk(K_INFO, "_ex_mu3d_hal_free_qmu_mem +\n");
	/*TODO:dma_free_coherent() is needed
	 * if _ex_mu3d_hal_alloc_qmu_mem() would be called more than once
	 */
	for (i = 1; i <= MAX_QMU_EP; i++) {
#if 0
		kfree(Rx_gpd_head[i]);
		kfree(Tx_gpd_head[i]);
#else
		dma_free_coherent(dev, size, Rx_gpd_head[i], Rx_gpd_ioptr[i]);
		dma_free_coherent(dev, size, Tx_gpd_head[i], Tx_gpd_ioptr[i]);
#endif
	}
	qmu_printk(K_INFO, "_ex_mu3d_hal_free_qmu_mem -\n");
}


void _ex_mu3d_hal_alloc_qmu_mem(struct device *dev)
{
	unsigned int i, size;
	struct TGPD *ptr;
	dma_addr_t io_ptr;
	dma_addr_t dma_handle;


	/*TODO: dma_pool_alloc() is an alternative choice
	 *  once the memory size is a concern
	 */
	for (i = 1; i <= MAX_QMU_EP; i++) {
		/* Allocate Rx GPD */
		size = (sizeof(struct TGPD) + AT_GPD_EXT_LEN) * MAX_GPD_NUM;
		ptr = (struct TGPD *) dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL);
		memset(ptr, 0, size);
		Rx_gpd_ioptr[i] = io_ptr = dma_handle;

		init_gpd_list(USB_RX, i, ptr, io_ptr, MAX_GPD_NUM);
		Rx_gpd_end[i] = ptr;

		qmu_printk(K_INFO, "ALLOC RX GPD End [%d] Virtual Mem=%p, DMA addr=%08llx\n",
			   i, Rx_gpd_end[i], (unsigned long long)io_ptr);

		TGPD_CLR_FLAGS_HWO(Rx_gpd_end[i]);
		Rx_gpd_head[i] = Rx_gpd_last[i] = Rx_gpd_end[i];

		qmu_printk(K_INFO, "RQSAR[%d]=%08llx\n", i,
			   (unsigned long long)mu3d_hal_gpd_virt_to_phys(Rx_gpd_end[i], USB_RX, i));

		os_printk(K_INFO, "allocate RX GPD successful\n");
		/* Allocate Tx GPD */
		size = (sizeof(struct TGPD) + AT_GPD_EXT_LEN) * MAX_GPD_NUM;
		ptr = (struct TGPD *) dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL);
		memset(ptr, 0, size);
		Tx_gpd_ioptr[i] = io_ptr = dma_handle;

		init_gpd_list(USB_TX, i, ptr, io_ptr, MAX_GPD_NUM);
		Tx_gpd_end[i] = ptr;

		qmu_printk(K_INFO, "ALLOC TX GPD End [%d] Virtual Mem=%p, DMA addr=%08llx\n",
			   i, Tx_gpd_end[i], (unsigned long long)io_ptr);

		TGPD_CLR_FLAGS_HWO(Tx_gpd_end[i]);
		Tx_gpd_head[i] = Tx_gpd_last[i] = Tx_gpd_end[i];

		qmu_printk(K_INFO, "TQSAR[%d]=%08llx\n", i,
			   (unsigned long long)mu3d_hal_gpd_virt_to_phys(Tx_gpd_end[i], USB_TX, i));
	}
}

void mu3d_hal_free_qmu_mem(void)
{
	unsigned int i;

	for (i = 1; i <= MAX_QMU_EP; i++) {
		kfree(Rx_gpd_head[i]);
		kfree(Tx_gpd_head[i]);
		kfree(Rx_bd_List[i].pStart);
		kfree(Tx_bd_List[i].pStart);
	}
}

void mu3d_hal_alloc_qmu_mem(void)
{
	unsigned int i, size;
	struct TGPD *ptr;
	dma_addr_t io_ptr;
	struct TBD *bptr;
	dma_addr_t io_bptr;

	for (i = 1; i <= MAX_QMU_EP; i++) {
		/* Allocate Tx GPD */
		size = sizeof(struct TGPD);
		size *= MAX_GPD_NUM;
		ptr = (struct TGPD *) os_mem_alloc(size);
		os_memset(ptr, 0, size);

		io_ptr = dma_map_single(NULL, ptr, size, DMA_TO_DEVICE);
		init_gpd_list(USB_RX, i, ptr, io_ptr, MAX_GPD_NUM);
		Rx_gpd_end[i] = ptr;

		os_printk(K_DEBUG, "ALLOC RX GPD End [%d] Virtual Mem=%p, DMA addr=%08llx\n",
			  i, Rx_gpd_end[i], (unsigned long long)io_ptr);

		/* os_memset(Rx_gpd_end[i], 0 , sizeof(TGPD)); */
		TGPD_CLR_FLAGS_HWO(Rx_gpd_end[i]);
		Rx_gpd_head[i] = Rx_gpd_last[i] = Rx_gpd_end[i];

		os_printk(K_DEBUG, "RQSAR[%d]=%08llx\n", i,
			  (unsigned long long)mu3d_hal_gpd_virt_to_phys(Rx_gpd_end[i], USB_RX, i));

		/* Allocate Rx GPD */
		size = sizeof(struct TGPD);
		size += AT_GPD_EXT_LEN;
		size *= MAX_GPD_NUM;
		ptr = (struct TGPD *) os_mem_alloc(size);
		os_memset(ptr, 0, size);

		io_ptr = dma_map_single(NULL, ptr, size, DMA_TO_DEVICE);
		init_gpd_list(USB_TX, i, ptr, io_ptr, MAX_GPD_NUM);
		Tx_gpd_end[i] = ptr;

		os_printk(K_DEBUG, "ALLOC TX GPD End [%d] Virtual Mem=%p, DMA addr=%08llx\n",
			  i, Tx_gpd_end[i], (unsigned long long)io_ptr);

		TGPD_CLR_FLAGS_HWO(Tx_gpd_end[i]);
		Tx_gpd_head[i] = Tx_gpd_last[i] = Tx_gpd_end[i];

		os_printk(K_DEBUG, "TQSAR[%d]=%08llx\n", i,
			  (unsigned long long)mu3d_hal_gpd_virt_to_phys(Tx_gpd_end[i], USB_TX, i));

		/* Allocate Tx BD */
		size = (sizeof(struct TBD));
		size *= MAX_BD_NUM;
		bptr = (struct TBD *) os_mem_alloc(size);
		os_memset(bptr, 0, size);
		io_bptr = dma_map_single(NULL, bptr, size, DMA_TO_DEVICE);
		init_bd_list(USB_RX, i, bptr, io_bptr, MAX_BD_NUM);

		/* Allocate Rx BD */
		size = (sizeof(struct TBD));
		size += AT_BD_EXT_LEN;
		size *= MAX_BD_NUM;
		bptr = (struct TBD *) os_mem_alloc(size);
		os_memset(bptr, 0, size);
		io_bptr = dma_map_single(NULL, bptr, size, DMA_TO_DEVICE);
		init_bd_list(USB_TX, i, bptr, io_bptr, MAX_BD_NUM);
	}
}

/**
 * mu3d_hal_init_qmu - initialize qmu
 *
 */
void _ex_mu3d_hal_init_qmu(void)
{
	unsigned int i;
	unsigned int QCR = 0;

	/* Initialize QMU Tx/Rx start address. */
	for (i = 1; i <= MAX_QMU_EP; i++) {
		qmu_printk(K_INFO, "==EP[%d]==Start addr RXQ=0x%08lx, TXQ=0x%08lx\n", i,
			   (uintptr_t) mu3d_hal_gpd_virt_to_phys(Rx_gpd_head[i], USB_RX, i),
			   (uintptr_t) mu3d_hal_gpd_virt_to_phys(Tx_gpd_head[i], USB_TX, i));
		QCR |= QMU_RX_EN(i);
		QCR |= QMU_TX_EN(i);
		os_writel(USB_QMU_RQSAR(i), mu3d_hal_gpd_virt_to_phys(Rx_gpd_head[i], USB_RX, i));
		os_writel(USB_QMU_TQSAR(i), mu3d_hal_gpd_virt_to_phys(Tx_gpd_head[i], USB_TX, i));
		Tx_gpd_end[i] = Tx_gpd_last[i] = Tx_gpd_head[i];
		Rx_gpd_end[i] = Rx_gpd_last[i] = Rx_gpd_head[i];
		gpd_ptr_align(USB_TX, i, Tx_gpd_end[i]);
		gpd_ptr_align(USB_RX, i, Rx_gpd_end[i]);
	}

	/* Enable QMU interrupt. */
	os_writel(U3D_QIESR1, TXQ_EMPTY_IESR | TXQ_CSERR_IESR | TXQ_LENERR_IESR |
		  RXQ_EMPTY_IESR | RXQ_CSERR_IESR | RXQ_LENERR_IESR | RXQ_ZLPERR_IESR);
	os_writel(U3D_EPIESR, EP0ISR);
}


void mu3d_hal_init_qmu(void)
{
	unsigned int i;
	unsigned int QCR = 0;

	/* Initialize QMU Tx/Rx start address. */
	for (i = 1; i <= MAX_QMU_EP; i++) {
		os_printk(K_DEBUG, "==EP[%d]==Start addr RXQ=0x%08lx, TXQ=0x%08lx\n", i,
			  (uintptr_t) mu3d_hal_gpd_virt_to_phys(Rx_gpd_head[i], USB_RX, i),
			  (uintptr_t) mu3d_hal_gpd_virt_to_phys(Tx_gpd_head[i], USB_TX, i));
		QCR |= QMU_RX_EN(i);
		QCR |= QMU_TX_EN(i);
		os_writel(USB_QMU_RQSAR(i), mu3d_hal_gpd_virt_to_phys(Rx_gpd_head[i], USB_RX, i));
		os_writel(USB_QMU_TQSAR(i), mu3d_hal_gpd_virt_to_phys(Tx_gpd_head[i], USB_TX, i));
		Tx_gpd_end[i] = Tx_gpd_last[i] = Tx_gpd_head[i];
		Rx_gpd_end[i] = Rx_gpd_last[i] = Rx_gpd_head[i];
		gpd_ptr_align(USB_TX, i, Tx_gpd_end[i]);
		gpd_ptr_align(USB_RX, i, Rx_gpd_end[i]);
	}
	/* Enable QMU Tx/Rx. */
	os_writel(U3D_QGCSR, QCR);
	os_writel(U3D_QIESR0, QCR);
	/* Enable QMU interrupt. */
	os_writel(U3D_QIESR1,
		  TXQ_EMPTY_IESR | TXQ_CSERR_IESR | TXQ_LENERR_IESR | RXQ_EMPTY_IESR |
		  RXQ_CSERR_IESR | RXQ_LENERR_IESR | RXQ_ZLPERR_IESR);
	os_writel(U3D_EPIESR, EP0ISR);
}

/**
 * mu3d_hal_cal_checksum - calculate check sum
 * @args - arg1: data buffer, arg2: data length
 */
noinline DEV_UINT8 mu3d_hal_cal_checksum(unsigned char *data, int len)
{
	unsigned char *uDataPtr, ckSum;
	int i;

	*(data + 1) = 0x0;
	uDataPtr = data;
	ckSum = 0;
	/* For ALPS01572117, we found calculated QMU check sum is wrong. (Dump memory value directly.) */
	/* After check this function, we did not find any flaw. Still cannot find how to get this wrong value. */
	/* Maybe it is a memory corruption or complier problem. Add "noinline" and "mb();" to prevent this problem. */
	mb(); /* avoid context switch */
	for (i = 0; i < len; i++)
		ckSum += *(uDataPtr + i);

	return 0xFF - ckSum;
}

/**
 * mu3d_hal_resume_qmu - resume qmu function
 * @args - arg1: ep number, arg2: dir
 */
void mu3d_hal_resume_qmu(int q_num, enum USB_DIR dir)
{
	mb(); /* avoid context switch */

	if (dir == USB_TX)
		os_writel(USB_QMU_TQCSR(q_num), QMU_Q_RESUME);
	else if (dir == USB_RX)
		os_writel(USB_QMU_RQCSR(q_num), QMU_Q_RESUME);
	else {
		qmu_printk(K_ERR, "%s wrong direction!!!\n", __func__);
		WARN_ON(1);
	}
}

/**
 * mu3d_hal_prepare_tx_gpd - prepare tx gpd/bd
 * @args - arg1: gpd address, arg2: data buffer address, arg3: data length, arg4: ep number,
 * arg5: with bd or not, arg6: write hwo bit or not,  arg7: write ioc bit or not
 */
struct TGPD *_ex_mu3d_hal_prepare_tx_gpd(struct TGPD *gpd, dma_addr_t pBuf, unsigned int data_len,
				  unsigned char ep_num, unsigned char _is_bdp, unsigned char isHWO,
				  unsigned char ioc, unsigned char bps, unsigned char zlp)
{
	/*Set actual data point to "DATA Buffer" */
	TGPD_SET_DATA(gpd, (unsigned long)pBuf);

#if defined(CONFIG_USB_MU3D_DRV_36BIT)
	TGPD_SET_DATA_TXHI(gpd, (unsigned long) (pBuf >> 32));
#endif

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
	Tx_gpd_end[ep_num] = get_gpd(USB_TX, ep_num);
	qmu_printk(K_DEBUG, "[TX] Tx_gpd_end[%d]=%p\n", ep_num, Tx_gpd_end[ep_num]);

	/*Initialize the new GPD */
	memset(Tx_gpd_end[ep_num], 0, sizeof(struct TGPD) + AT_GPD_EXT_LEN);

	/*Clear "HWO(Hardware Own)" flag */
	TGPD_CLR_FLAGS_HWO(Tx_gpd_end[ep_num]);

	/*Set "Next GDP pointer" as the next GPD */
	TGPD_SET_NEXT(gpd,
		      (unsigned long)mu3d_hal_gpd_virt_to_phys(Tx_gpd_end[ep_num], USB_TX, ep_num));
#if defined(CONFIG_USB_MU3D_DRV_36BIT)
	TGPD_SET_NEXT_TXHI(gpd,
		      (unsigned long) (mu3d_hal_gpd_virt_to_phys(Tx_gpd_end[ep_num], USB_TX, ep_num) >> 32));
#endif
	/*Default: isHWO=true */
	TGPD_SET_CHKSUM(gpd, CHECKSUM_LENGTH);	/*Set GPD Checksum */
	TGPD_SET_FLAGS_HWO(gpd);	/*Set HWO flag */

	return gpd;
}

struct TGPD *mu3d_hal_prepare_tx_gpd(struct TGPD *gpd, dma_addr_t pBuf, unsigned int data_len,
			      unsigned char ep_num, unsigned char _is_bdp, unsigned char isHWO,
			      unsigned char ioc, unsigned char bps, unsigned char zlp)
{
	unsigned int offset;
	int i;
	int bd_num;
	unsigned int length;

	struct TBD *bd_next;
	struct TBD *bd_head;
	struct TBD *bd;
	unsigned char *pBuffer;

	/*If data length is less than the GPD buffer size, just use GPD */
	/* if (data_len <= GPD_BUF_SIZE) { */
	/* _is_bdp = 0; */
	/* } */

	os_printk(K_INFO, "%s gpd=%p, epnum=%d, len=%d, _is_bdp=%d\n", __func__,
		  gpd, ep_num, data_len, _is_bdp);

	if (!_is_bdp) {
		/*Set actual data point to "DATA Buffer" */
		TGPD_SET_DATA(gpd, (unsigned long)pBuf);
		/*Clear "BDP(Buffer Descriptor Present)" flag */
		TGPD_CLR_FORMAT_BDP(gpd);
	} else {
		/*Get the first BD */
		bd_head = (struct TBD *) get_bd(USB_TX, ep_num);
		os_printk(K_INFO, "bd_head=x%p\n", bd_head);

		bd = bd_head;
		os_memset(bd, 0, sizeof(struct TBD));

		/*Date length for transfer */
		length = data_len;

		/*Point of data buffer */
		pBuffer = (unsigned char *) (uintptr_t) (pBuf);

		/*The size of BD buffer */
		offset = BD_BUF_SIZE;

		/*Count how many BD this transfer need. */
		bd_num = (!(length % offset)) ? (length / offset) : ((length / offset) + 1);

		os_printk(K_INFO, "bd_num=%d\n", bd_num);

		/*If the size of BD buffer is bigger than the length of actual transfer, use the actual length */
		if (offset > length)
			offset = length;

		/*Insert data into each BD */
		for (i = 0; i < bd_num; i++) {
			os_printk(K_INFO, "bd[%d]=%p\n", i, bd);
			if (i == (bd_num - 1)) {	/*The last BD */
				TBD_SET_EXT_LEN(bd, 0);	/*"BD Extension Length" = 0. Does not use BD EXT!! */
				TBD_SET_BUF_LEN(bd, length);	/*"Data Buffer Length" = the rest of data length */
				/*Store the data pointer to "Data Buffer" */
				TBD_SET_DATA(bd, (unsigned long)pBuffer);

				TBD_SET_FLAGS_EOL(bd);	/*Set "EOL" */
				TBD_SET_NEXT(bd, 0);	/*Set "Next BD pointer" = 0 */
				TBD_SET_CHKSUM(bd, CHECKSUM_LENGTH);	/*Set "BD Checksum" */

				/*Flush the data of BD struct to device */
				dma_sync_single_for_device(NULL,
							   bd_virt_to_phys(bd, USB_RX, ep_num),
							   sizeof(struct TBD), DMA_BIDIRECTIONAL);

				/*There is no data left to be transferred by GPD */
				/* data_len=length; */
				data_len = 0;

				/*There is no data left to insert BD */
				length = 0;
			} else {
				TBD_SET_EXT_LEN(bd, 0);	/*"BD Extension length" = 0. Does not use BD EXT!! */
				TBD_SET_BUF_LEN(bd, offset);	/*"Data Buffer Length" = the MAX BD transfer size */
				/*Store the data pointer to "Data Buffer" */
				TBD_SET_DATA(bd, (unsigned long)pBuffer);

				TBD_CLR_FLAGS_EOL(bd);	/*Clear "EOL" */
				/*Get the next BD */
				bd_next = (struct TBD *) get_bd(USB_TX, ep_num);
				os_memset(bd_next, 0, sizeof(struct TBD));

				/*Set "Next BD pointer" as the next BD */
				TBD_SET_NEXT(bd,
					     (unsigned long)bd_virt_to_phys(bd_next, USB_TX,
									    ep_num));
				TBD_SET_CHKSUM(bd, CHECKSUM_LENGTH);	/*Set BD Checksum */

				/*Flush the data of BD struct to device */
				dma_sync_single_for_device(NULL,
							   bd_virt_to_phys(bd, USB_RX, ep_num),
							   sizeof(struct TBD), DMA_BIDIRECTIONAL);

				/*Calculate the left data length */
				length -= offset;

				/*Move to pointer of buffer */
				pBuffer += offset;

				/*Move to next BD */
				bd = bd_next;
			}
		}

		/*Set the BD pointer into "BD Pointer" at GPD */
		TGPD_SET_DATA(gpd, (unsigned long)bd_virt_to_phys(bd_head, USB_TX, ep_num));

		/*Set "BDP(Buffer Descriptor Present)" flag */
		TGPD_SET_FORMAT_BDP(gpd);
	}

	os_printk(K_INFO, "%s GPD data_length=%d\n", __func__, data_len);

	/*
	 * "Data Buffer Length" =
	 * 0        (If data length > GPD buffer length, use BDs),
	 * data_len (If data length < GPD buffer length, only use GPD)
	 */
	TGPD_SET_BUF_LEN(gpd, data_len);

	/*"GPD extension length" = 0. Does not use GPD EXT!! */
	TGPD_SET_EXT_LEN(gpd, 0);

	/*Default: zlp=false, except type=ISOC */
	if (zlp)
		TGPD_SET_FORMAT_ZLP(gpd);
	else
		TGPD_CLR_FORMAT_ZLP(gpd);

	/*Default: bps=false */
	if (bps)
		TGPD_SET_FORMAT_BPS(gpd);
	else
		TGPD_CLR_FORMAT_BPS(gpd);

	/*Default: ioc=true */
	if (ioc)
		TGPD_SET_FORMAT_IOC(gpd);
	else
		TGPD_CLR_FORMAT_IOC(gpd);

	/*Get the next GPD */
	Tx_gpd_end[ep_num] = get_gpd(USB_TX, ep_num);
	os_printk(K_INFO, "Tx_gpd_end[%d]=%p\n", ep_num, Tx_gpd_end[ep_num]);

	/*Initialize the new GPD */
	os_memset(Tx_gpd_end[ep_num], 0, sizeof(struct TGPD));

	/*Clear "HWO(Hardware Own)" flag */
	TGPD_CLR_FLAGS_HWO(Tx_gpd_end[ep_num]);

	/*Set "Next GDP pointer" as the next GPD */
	TGPD_SET_NEXT(gpd,
		      (unsigned long)mu3d_hal_gpd_virt_to_phys(Tx_gpd_end[ep_num], USB_TX, ep_num));

	/*Default: isHWO=true */
	if (isHWO) {
		TGPD_SET_CHKSUM(gpd, CHECKSUM_LENGTH);	/*Set GPD Checksum */
		TGPD_SET_FLAGS_HWO(gpd);	/*Set HWO flag */
	} else {
		TGPD_CLR_FLAGS_HWO(gpd);
		TGPD_SET_CHKSUM_HWO(gpd, CHECKSUM_LENGTH);
	}

	/*Flush the data of GPD struct to device */
	dma_sync_single_for_device(NULL, mu3d_hal_gpd_virt_to_phys(gpd, USB_TX, ep_num),
				   sizeof(struct TGPD), DMA_BIDIRECTIONAL);

#if defined(USB_RISC_CACHE_ENABLED)
	os_flushinvalidateDcache();
#endif

	return gpd;
}

static inline int check_next_gpd(struct TGPD *gpd, struct TGPD *next_gpd)
{
	if (((uintptr_t) next_gpd - (uintptr_t) gpd) == 0x40)
		return 1;
	else if (((uintptr_t) gpd - (uintptr_t) next_gpd) == 0x7c0)
		return 1;

	/*UNNECESSARY_ELSE*/
	qmu_printk(K_ERR, "[RX] %p <-> %p\n", gpd, next_gpd);
	return 0;
}

/**
 * mu3d_hal_prepare_rx_gpd - prepare rx gpd/bd
 * @args - arg1: gpd address, arg2: data buffer address, arg3: data length,
 * arg4: ep number, arg5: with bd or not, arg6: write hwo bit or not,  arg7: write ioc bit or not
 */
struct TGPD *_ex_mu3d_hal_prepare_rx_gpd(struct TGPD *gpd, dma_addr_t pBuf, unsigned int data_len,
				  unsigned char ep_num, unsigned char _is_bdp, unsigned char isHWO,
				  unsigned char ioc, unsigned char bps, unsigned int cMaxPacketSize)
{
	/*Set actual data point to "DATA Buffer" */
	TGPD_SET_DATA(gpd, (unsigned long)pBuf);
#if defined(CONFIG_USB_MU3D_DRV_36BIT)
	TGPD_SET_DATA_RXHI(gpd, (unsigned long) (pBuf >> 32));
#endif
	/*Clear "BDP(Buffer Descriptor Present)" flag */
	TGPD_CLR_FORMAT_BDP(gpd);
	/*
	 * Set "Allow Data Buffer Length" =
	 * 0        (If data length > GPD buffer length, use BDs),
	 * data_len (If data length < GPD buffer length, only use GPD)
	 */
	TGPD_SET_DataBUF_LEN(gpd, data_len);

	/*Set "Transferred Data Length" = 0 */
	TGPD_SET_BUF_LEN(gpd, 0);

	/*Default: bps=false */
	TGPD_CLR_FORMAT_BPS(gpd);

	/*Default: ioc=true */
	TGPD_SET_FORMAT_IOC(gpd);

	/*Get the next GPD */
	Rx_gpd_end[ep_num] = get_gpd(USB_RX, ep_num);
	qmu_printk(K_DEBUG, "[RX] Rx_gpd_end[%d]=%p gpd=%p\n", ep_num, Rx_gpd_end[ep_num], gpd);

	/* BUG_ON(!check_next_gpd(gpd, Rx_gpd_end[ep_num])); */

	/*Initialize the new GPD */
	memset(Rx_gpd_end[ep_num], 0, sizeof(struct TGPD) + AT_GPD_EXT_LEN);

	/*Clear "HWO(Hardware Own)" flag */
	TGPD_CLR_FLAGS_HWO(Rx_gpd_end[ep_num]);

	/*Set Next GDP pointer to the next GPD */
	TGPD_SET_NEXT(gpd,
		      (unsigned long)mu3d_hal_gpd_virt_to_phys(Rx_gpd_end[ep_num], USB_RX, ep_num));

#if defined(CONFIG_USB_MU3D_DRV_36BIT)
		TGPD_SET_NEXT_RXHI(gpd,
			(unsigned long) (mu3d_hal_gpd_virt_to_phys(Rx_gpd_end[ep_num], USB_RX, ep_num) >> 32));
#endif

	/*Default: isHWO=true */
	TGPD_SET_CHKSUM(gpd, CHECKSUM_LENGTH);	/*Set GPD Checksum */
	TGPD_SET_FLAGS_HWO(gpd);	/*Set HWO flag */

	/* os_printk(K_DEBUG,"Rx gpd info { HWO %d, Next_GPD %x ,DataBufferLength %d, */
	/*  DataBuffer %x, Recived Len %d, Endpoint %d, TGL %d, ZLP %d}\n", */
	/* (unsigned int)TGPD_GET_FLAG(gpd), (unsigned int)TGPD_GET_NEXT(gpd), */
	/* (unsigned int)TGPD_GET_DataBUF_LEN(gpd), (unsigned int)TGPD_GET_DATA(gpd), */
	/* (unsigned int)TGPD_GET_BUF_LEN(gpd), (unsigned int)TGPD_GET_EPaddr(gpd), */
	/* (unsigned int)TGPD_GET_TGL(gpd), (unsigned int)TGPD_GET_ZLP(gpd)); */

	return gpd;
}

struct TGPD *mu3d_hal_prepare_rx_gpd(struct TGPD *gpd, dma_addr_t pBuf, unsigned int data_len,
			      unsigned char ep_num, unsigned char _is_bdp, unsigned char isHWO,
			      unsigned char ioc, unsigned char bps, unsigned int cMaxPacketSize)
{
	unsigned int offset;
	int i;
	int bd_num;
	unsigned int length;

	struct TBD *bd_next;
	struct TBD *bd_head;
	struct TBD *bd;
	unsigned char *pBuffer;

	/*If data length is less than the GPD buffer size, just use GPD */
	if (data_len < GPD_BUF_SIZE)
		_is_bdp = 0;

	os_printk(K_INFO, "%s gpd=%p, epnum=%d, len=%d, _is_bdp=%d, maxp=%d\n", __func__,
		  gpd, ep_num, data_len, _is_bdp, cMaxPacketSize);

	if (!_is_bdp) {
		/*Set actual data point to "DATA Buffer" */
		TGPD_SET_DATA(gpd, (unsigned long)pBuf);
		/*Clear "BDP(Buffer Descriptor Present)" flag */
		TGPD_CLR_FORMAT_BDP(gpd);
	} else {
		/*Get the first BD */
		bd_head = (struct TBD *) get_bd(USB_RX, ep_num);
		os_printk(K_INFO, "bd_head=x%p\n", bd_head);

		bd = bd_head;
		os_memset(bd, 0, sizeof(struct TBD));

		/*Date length for transfer */
		length = data_len;

		/*Point of data buffer */
		pBuffer = (unsigned char *) (uintptr_t) (pBuf);

		/*The size of BD buffer */
		offset = BD_BUF_SIZE;

		/*Count how many BD this transfer need. */
		bd_num = (!(length % offset)) ? (length / offset) : ((length / offset) + 1);

		os_printk(K_INFO, "%s bd_num=%d\n", __func__, bd_num);

		/*Insert data into each BD */
		for (i = 0; i < bd_num; i++) {
			os_printk(K_INFO, "%s bd[%d]=%p\n", __func__, i, bd);
			if (i == (bd_num - 1)) {
				TBD_SET_BUF_LEN(bd, 0);	/*Set "Transferred Data Length" = 0 */

				/*The last one's data buffer lengnth must be precise, or the GPD will never
				 * done unless ZLP or short packet.
				 */
				/*"Allow Data Buffer Length" = the rest of data length* */
				length =
				    (!(length % cMaxPacketSize)) ? (length) : ((length /
										cMaxPacketSize) +
									       1) * cMaxPacketSize;
				TBD_SET_DataBUF_LEN(bd, length);
				/*Store the data pointer to "Data Buffer" */
				TBD_SET_DATA(bd, (unsigned long)pBuffer);

				TBD_SET_FLAGS_EOL(bd);	/*Set "EOL" */
				TBD_SET_NEXT(bd, 0);	/*Set "Next BD pointer" = 0 */
				TBD_SET_CHKSUM(bd, CHECKSUM_LENGTH);	/*Set "BD Checksum" */

				/*Flush the data of BD struct to device */
				dma_sync_single_for_device(NULL,
							   bd_virt_to_phys(bd, USB_RX, ep_num),
							   sizeof(struct TBD), DMA_BIDIRECTIONAL);

				break;
			}
			/*WARNING:UNNECESSARY_ELSE: else is not generally useful after a break or return*/
			/*else*/
			{
				TBD_SET_BUF_LEN(bd, 0);	/*Set "Transferred Data Length" = 0 */

				/*"Allow Data Buffer Length" = the MAX BD transfer size */
				TBD_SET_DataBUF_LEN(bd, offset);
				/*Store the data pointer to "Data Buffer" */
				TBD_SET_DATA(bd, (unsigned long)pBuffer);

				TBD_CLR_FLAGS_EOL(bd);	/*Clear "EOL" */
				/*Get the next BD */
				bd_next = (struct TBD *) get_bd(USB_RX, ep_num);
				os_memset(bd_next, 0, sizeof(struct TBD));

				/*Set "Next BD pointer" as the next BD */
				TBD_SET_NEXT(bd,
					     (unsigned long)bd_virt_to_phys(bd_next, USB_RX,
									    ep_num));
				TBD_SET_CHKSUM(bd, CHECKSUM_LENGTH);	/*Set BD Checksum */

				/*Flush the data of BD struct to device */
				dma_sync_single_for_device(NULL,
							   bd_virt_to_phys(bd, USB_RX, ep_num),
							   sizeof(struct TBD), DMA_BIDIRECTIONAL);

				/*Calculate the left data length */
				length -= offset;

				/*Move to pointer of buffer */
				pBuffer += offset;

				/*Move to next BD */
				bd = bd_next;
			}
		}

		/*Set the BD pointer into "BD Pointer" at GPD */
		TGPD_SET_DATA(gpd, (unsigned long)bd_virt_to_phys(bd_head, USB_RX, ep_num));

		/*Set "BDP(Buffer Descriptor Present)" flag */
		TGPD_SET_FORMAT_BDP(gpd);
	}

	os_printk(K_INFO, "%s GPD data_length=%d\n", __func__, data_len);

	/*
	 * Set "Allow Data Buffer Length" =
	 * 0        (If data length > GPD buffer length, use BDs),
	 * data_len (If data length < GPD buffer length, only use GPD)
	 */
	TGPD_SET_DataBUF_LEN(gpd, data_len);
	/* TGPD_SET_DataBUF_LEN(gpd, gpd_buf_size); */

	/*Set "Transferred Data Length" = 0 */
	TGPD_SET_BUF_LEN(gpd, 0);

	/*Default: bps=false */
	if (bps)
		TGPD_SET_FORMAT_BPS(gpd);
	else
		TGPD_CLR_FORMAT_BPS(gpd);

	/*Default: ioc=true */
	if (ioc)
		TGPD_SET_FORMAT_IOC(gpd);
	else
		TGPD_CLR_FORMAT_IOC(gpd);

	/*Get the next GPD */
	Rx_gpd_end[ep_num] = get_gpd(USB_RX, ep_num);
	os_printk(K_INFO, "%s Rx_gpd_end[%d]=%p\n", __func__, ep_num, Tx_gpd_end[ep_num]);

	/*Initialize the new GPD */
	os_memset(Rx_gpd_end[ep_num], 0, sizeof(struct TGPD));

	/*Clear "HWO(Hardware Own)" flag */
	TGPD_CLR_FLAGS_HWO(Rx_gpd_end[ep_num]);

	/*Set Next GDP pointer to the next GPD */
	TGPD_SET_NEXT(gpd,
		      (unsigned long)mu3d_hal_gpd_virt_to_phys(Rx_gpd_end[ep_num], USB_RX, ep_num));

	/*Default: isHWO=true */
	if (isHWO) {
		TGPD_SET_CHKSUM(gpd, CHECKSUM_LENGTH);	/*Set GPD Checksum */
		TGPD_SET_FLAGS_HWO(gpd);	/*Set HWO flag */
	} else {
		TGPD_CLR_FLAGS_HWO(gpd);
		TGPD_SET_CHKSUM_HWO(gpd, CHECKSUM_LENGTH);
	}

	/* os_printk(K_DEBUG,"Rx gpd info { HWO %d, Next_GPD %x ,DataBufferLength %d,
	 * DataBuffer %x, Recived Len %d, Endpoint %d, TGL %d, ZLP %d}\n",
	 */
	/* (unsigned int)TGPD_GET_FLAG(gpd), (unsigned int)TGPD_GET_NEXT(gpd), */
	/* (unsigned int)TGPD_GET_DataBUF_LEN(gpd), (unsigned int)TGPD_GET_DATA(gpd), */
	/* (unsigned int)TGPD_GET_BUF_LEN(gpd), (unsigned int)TGPD_GET_EPaddr(gpd), */
	/* (unsigned int)TGPD_GET_TGL(gpd), (unsigned int)TGPD_GET_ZLP(gpd)); */

	/*Flush the data of GPD struct to device */
	dma_sync_single_for_device(NULL, mu3d_hal_gpd_virt_to_phys(gpd, USB_RX, ep_num),
				   sizeof(struct TGPD), DMA_BIDIRECTIONAL);

	return gpd;
}

/*
 * mu3d_hal_insert_transfer_gpd - insert new gpd/bd
 * @args - arg1: ep number, arg2: dir, arg3: data buffer, arg4: data length,
 * arg5: write hwo bit or not,  arg6: write ioc bit or not
 */
void _ex_mu3d_hal_insert_transfer_gpd(int ep_num, enum USB_DIR dir, dma_addr_t buf,
				      unsigned int count, unsigned char isHWO, unsigned char ioc,
				      unsigned char bps, unsigned char zlp, unsigned int maxp)
{
	struct TGPD *gpd;

	if (dir == USB_TX) {
		gpd = Tx_gpd_end[ep_num];
		_ex_mu3d_hal_prepare_tx_gpd(gpd, buf, count, ep_num, IS_BDP, isHWO, ioc, bps, zlp);
	} else if (dir == USB_RX) {
		gpd = Rx_gpd_end[ep_num];
		_ex_mu3d_hal_prepare_rx_gpd(gpd, buf, count, ep_num, IS_BDP, isHWO, ioc, bps, maxp);
	}
}

void mu3d_hal_insert_transfer_gpd(int ep_num, enum USB_DIR dir, dma_addr_t buf,
				  unsigned int count, unsigned char isHWO, unsigned char ioc,
				  unsigned char bps, unsigned char zlp, unsigned int maxp)
{
	struct TGPD *gpd;

	if (dir == USB_TX) {
		gpd = Tx_gpd_end[ep_num];
		/* os_printk(K_INFO,"TX gpd :%x\n", (unsigned int)gpd); */
		mu3d_hal_prepare_tx_gpd(gpd, buf, count, ep_num, IS_BDP, isHWO, ioc, bps, zlp);
	} else if (dir == USB_RX) {
		gpd = Rx_gpd_end[ep_num];
		/* os_printk(K_INFO,"RX gpd :%x\n",(unsigned int)gpd); */
		mu3d_hal_prepare_rx_gpd(gpd, buf, count, ep_num, IS_BDP, isHWO, ioc, bps, maxp);
	}
}

/**
 * mu3d_hal_start_qmu - start qmu function (QMU flow :
 * mu3d_hal_init_qmu ->mu3d_hal_start_qmu -> mu3d_hal_insert_transfer_gpd -> mu3d_hal_resume_qmu)
 * @args - arg1: ep number, arg2: dir
 */
void mu3d_hal_start_qmu(int Q_num, enum USB_DIR dir)
{
	unsigned int QCR;
	unsigned int txcsr;

	if (dir == USB_TX) {
		txcsr = USB_ReadCsr32(U3D_TX1CSR0, Q_num) & 0xFFFEFFFF;
		USB_WriteCsr32(U3D_TX1CSR0, Q_num, txcsr | TX_DMAREQEN);

		QCR = os_readl(U3D_QCR0);
		os_writel(U3D_QCR0, QCR | QMU_TX_CS_EN(Q_num));

#if (TXZLP == HW_MODE)
		QCR = os_readl(U3D_QCR1);
		os_writel(U3D_QCR1, QCR & ~QMU_TX_ZLP(Q_num));
		QCR = os_readl(U3D_QCR2);
		os_writel(U3D_QCR2, QCR | QMU_TX_ZLP(Q_num));
#elif (TXZLP == GPD_MODE)
		QCR = os_readl(U3D_QCR1);
		os_writel(U3D_QCR1, QCR | QMU_TX_ZLP(Q_num));
#endif
		os_writel(U3D_QEMIESR, os_readl(U3D_QEMIESR) | QMU_TX_EMPTY(Q_num));
		os_writel(U3D_TQERRIESR0, QMU_TX_LEN_ERR(Q_num) | QMU_TX_CS_ERR(Q_num));

		qmu_printk(K_DEBUG, "USB_QMU_TQCSR:0x%08X\n", os_readl(USB_QMU_TQCSR(Q_num)));

		if (os_readl(USB_QMU_TQCSR(Q_num)) & QMU_Q_ACTIVE) {
			qmu_printk(K_INFO, "Tx %d Active Now!\n", Q_num);
			return;
		}

		os_writel(USB_QMU_TQCSR(Q_num), QMU_Q_START);

		qmu_printk(K_DEBUG, "USB_QMU_TQCSR:0x%08X\n", os_readl(USB_QMU_TQCSR(Q_num)));
	} else if (dir == USB_RX) {
		USB_WriteCsr32(U3D_RX1CSR0, Q_num,
			       USB_ReadCsr32(U3D_RX1CSR0, Q_num) | (RX_DMAREQEN));

		QCR = os_readl(U3D_QCR0);
		os_writel(U3D_QCR0, QCR | QMU_RX_CS_EN(Q_num));

#ifdef CFG_RX_ZLP_EN
		QCR = os_readl(U3D_QCR3);
		os_writel(U3D_QCR3, QCR | QMU_RX_ZLP(Q_num));
#else
		QCR = os_readl(U3D_QCR3);
		os_writel(U3D_QCR3, QCR & ~(QMU_RX_ZLP(Q_num)));
#endif

#ifdef CFG_RX_COZ_EN
		QCR = os_readl(U3D_QCR3);
		os_writel(U3D_QCR3, QCR | QMU_RX_COZ(Q_num));
#else
		QCR = os_readl(U3D_QCR3);
		os_writel(U3D_QCR3, QCR & ~(QMU_RX_COZ(Q_num)));
#endif

		os_writel(U3D_QEMIESR, os_readl(U3D_QEMIESR) | QMU_RX_EMPTY(Q_num));
		os_writel(U3D_RQERRIESR0, QMU_RX_LEN_ERR(Q_num) | QMU_RX_CS_ERR(Q_num));
		os_writel(U3D_RQERRIESR1, QMU_RX_EP_ERR(Q_num) | QMU_RX_ZLP_ERR(Q_num));

		qmu_printk(K_DEBUG, "USB_QMU_RQCSR:0x%08X\n", os_readl(USB_QMU_RQCSR(Q_num)));

		if (os_readl(USB_QMU_RQCSR(Q_num)) & QMU_Q_ACTIVE) {
			qmu_printk(K_INFO, "Rx %d Active Now!\n", Q_num);
			return;
		}

		os_writel(USB_QMU_RQCSR(Q_num), QMU_Q_START);

		qmu_printk(K_DEBUG, "USB_QMU_RQCSR:0x%08X\n", os_readl(USB_QMU_RQCSR(Q_num)));
	}

#if (CHECKSUM_TYPE == CS_16B)
	os_writel(U3D_QCR0, os_readl(U3D_QCR0) | CS16B_EN);
#else
	os_writel(U3D_QCR0, os_readl(U3D_QCR0) & ~CS16B_EN);
#endif
}

/**
 * mu3d_hal_stop_qmu - stop qmu function (after qmu stop, fifo should be flushed)
 * @args - arg1: ep number, arg2: dir
 */
void mu3d_hal_stop_qmu(int q_num, enum USB_DIR dir)
{
	if (dir == USB_TX) {
		if (!(os_readl(USB_QMU_TQCSR(q_num)) & (QMU_Q_ACTIVE))) {
			qmu_printk(K_DEBUG, "Tx%d inActive Now!\n", q_num);
		} else {
			os_writel(USB_QMU_TQCSR(q_num), QMU_Q_STOP);
			mb(); /* avoid context switch */
			if (wait_for_value_us(USB_QMU_TQCSR(q_num), QMU_Q_ACTIVE, 0, 10, 100) == RET_SUCCESS)
				qmu_printk(K_DEBUG, "Tx%d stop Now! CSR=0x%x\n",
					   q_num, os_readl(USB_QMU_TQCSR(q_num)));
			else {
				qmu_printk(K_CRIT, "Tx%d UNSTOPABLE!! CSR=0x%x\n",
					   q_num, os_readl(USB_QMU_TQCSR(q_num)));
				WARN_ON(1);
			}
		}
#ifdef CONFIG_MTK_MD_DIRECT_TETHERING_SUPPORT
		os_writel(U3D_QCR0, os_readl(U3D_QCR0)&~(QMU_TX_CS_EN(q_num)));
		os_writel(U3D_QEMIESR, os_readl(U3D_QEMIESR)&~(QMU_TX_EMPTY(q_num)));
		os_writel(U3D_TQERRIESR0, os_readl(U3D_TQERRIESR0)
				  &~(QMU_TX_LEN_ERR(q_num))&~(QMU_TX_CS_ERR(q_num)));
		qmu_printk(K_DEBUG, "Stop Tx%d qmu interrupt\n", q_num);
#endif
	} else if (dir == USB_RX) {
		if (!(os_readl(USB_QMU_RQCSR(q_num)) & QMU_Q_ACTIVE)) {
			qmu_printk(K_DEBUG, "Rx%d inActive Now!\n", q_num);
		} else {
			os_writel(USB_QMU_RQCSR(q_num), QMU_Q_STOP);
			mb(); /* avoid context switch */
			if (wait_for_value_us(USB_QMU_RQCSR(q_num), QMU_Q_ACTIVE, 0, 10, 100) == RET_SUCCESS)
				qmu_printk(K_DEBUG, "Rx%d stop Now! CSR=0x%x\n",
					   q_num, os_readl(USB_QMU_RQCSR(q_num)));
			else {
				qmu_printk(K_CRIT, "Rx%d UNSTOPABLE!! CSR=0x%x\n",
					   q_num, os_readl(USB_QMU_RQCSR(q_num)));
				WARN_ON(1);
			}
		}
#ifdef CONFIG_MTK_MD_DIRECT_TETHERING_SUPPORT
		os_writel(U3D_QCR0, os_readl(U3D_QCR0)&~(QMU_RX_CS_EN(q_num)));
		os_writel(U3D_QEMIESR, os_readl(U3D_QEMIESR)&~(QMU_RX_EMPTY(q_num)));
		os_writel(U3D_RQERRIESR0, os_readl(U3D_RQERRIESR0)
				  &~(QMU_RX_CS_ERR(q_num))&~(QMU_RX_LEN_ERR(q_num)));
		os_writel(U3D_RQERRIESR1, os_readl(U3D_RQERRIESR1)
				  &~(QMU_RX_ZLP_ERR(q_num))&~(QMU_RX_EP_ERR(q_num)));
		qmu_printk(K_DEBUG, "Stop Rx%d qmu interrupt\n", q_num);
#endif
	}
}

/**
 * mu3d_hal_send_stall - send stall
 * @args - arg1: ep number, arg2: dir
 */
void mu3d_hal_send_stall(int q_num, enum USB_DIR dir)
{
	if (dir == USB_TX) {
		USB_WriteCsr32(U3D_TX1CSR0, q_num,
			       USB_ReadCsr32(U3D_TX1CSR0, q_num) | TX_SENDSTALL);
		while (!(USB_ReadCsr32(U3D_TX1CSR0, q_num) & TX_SENTSTALL))
			;
		USB_WriteCsr32(U3D_TX1CSR0, q_num,
			       USB_ReadCsr32(U3D_TX1CSR0, q_num) | TX_SENTSTALL);
		USB_WriteCsr32(U3D_TX1CSR0, q_num,
			       USB_ReadCsr32(U3D_TX1CSR0, q_num) & ~TX_SENDSTALL);
	} else if (dir == USB_RX) {
		USB_WriteCsr32(U3D_RX1CSR0, q_num,
			       USB_ReadCsr32(U3D_RX1CSR0, q_num) | RX_SENDSTALL);
		while (!(USB_ReadCsr32(U3D_RX1CSR0, q_num) & RX_SENTSTALL))
			;
		USB_WriteCsr32(U3D_RX1CSR0, q_num,
			       USB_ReadCsr32(U3D_RX1CSR0, q_num) | RX_SENTSTALL);
		USB_WriteCsr32(U3D_RX1CSR0, q_num,
			       USB_ReadCsr32(U3D_RX1CSR0, q_num) & ~RX_SENDSTALL);
	}

	os_printk(K_CRIT, "%s %s-EP[%d] sent stall\n", __func__, ((dir == USB_TX) ? "TX" : "RX"),
		  q_num);
}

/**
 * mu3d_hal_restart_qmu - clear toggle(or sequence) number and start qmu
 * @args - arg1: ep number, arg2: dir
 */
void mu3d_hal_restart_qmu(int q_num, enum USB_DIR dir)
{
	unsigned int ep_rst;

	qmu_printk(K_CRIT, "%s : Reset %s-EP[%d]\n", __func__, ((dir == USB_TX) ? "TX" : "RX"),
		   q_num);

	if (dir == USB_TX) {
		ep_rst = BIT16 << q_num;
		os_writel(U3D_EP_RST, ep_rst);
		os_ms_delay(1);
		os_writel(U3D_EP_RST, 0);
	} else {
		ep_rst = 1 << q_num;
		os_writel(U3D_EP_RST, ep_rst);
		os_ms_delay(1);
		os_writel(U3D_EP_RST, 0);
	}
	mu3d_hal_start_qmu(q_num, dir);
}

/**
 * flush_qmu - stop qmu and align qmu start ptr t0 current ptr
 * @args - arg1: ep number, arg2: dir
 */
void _ex_mu3d_hal_flush_qmu(int Q_num, enum USB_DIR dir)
{
	qmu_printk(K_DEBUG, "%s flush QMU %s-EP[%d]\n", __func__, ((dir == USB_TX) ? "TX" : "RX"),
		   Q_num);

	if (dir == USB_TX) {
		/*Stop QMU */
		mu3d_hal_stop_qmu(Q_num, USB_TX);

		/*Reset the TX GPD list state */
		Tx_gpd_end[Q_num] = Tx_gpd_last[Q_num] = Tx_gpd_head[Q_num];
		gpd_ptr_align(dir, Q_num, Tx_gpd_end[Q_num]);
		free_gpd(dir, Q_num);

		/*FIXME: Do not know why... */
		os_writel(USB_QMU_TQSAR(Q_num),
			  mu3d_hal_gpd_virt_to_phys(Tx_gpd_last[Q_num], USB_TX, Q_num));
		qmu_printk(K_DEBUG, "USB_QMU_TQSAR %x\n", os_readl(USB_QMU_TQSAR(Q_num)));
	} else if (dir == USB_RX) {
		/*Stop QMU */
		mu3d_hal_stop_qmu(Q_num, USB_RX);

		/*Reset the RX GPD list state */
		Rx_gpd_end[Q_num] = Rx_gpd_last[Q_num] = Rx_gpd_head[Q_num];
		gpd_ptr_align(dir, Q_num, Rx_gpd_end[Q_num]);
		free_gpd(dir, Q_num);

		/*FIXME: Do not know why... */
		os_writel(USB_QMU_RQSAR(Q_num),
			  mu3d_hal_gpd_virt_to_phys(Rx_gpd_end[Q_num], USB_RX, Q_num));
		qmu_printk(K_DEBUG, "USB_QMU_RQSAR %x\n", os_readl(USB_QMU_RQSAR(Q_num)));
	}
}

void mu3d_hal_flush_qmu(int Q_num, enum USB_DIR dir)
{
	struct TGPD *gpd_current;

	struct USB_REQ *req = mu3d_hal_get_req(Q_num, dir);

	os_printk(K_INFO, "%s flush QMU %s\n", __func__, ((dir == USB_TX) ? "TX" : "RX"));

	if (dir == USB_TX) {
		/*Stop QMU */
		mu3d_hal_stop_qmu(Q_num, USB_TX);

		/*Get TX Queue Current Pointer Register */
		/* QMU GPD address --> CPU DMA address */
		gpd_current = (struct TGPD *) (uintptr_t) (os_readl(USB_QMU_TQCPR(Q_num)));

		/*If gpd_current = 0, it means QMU has not yet to execute GPD in QMU. */
		if (!gpd_current) {
			/*Get TX Queue Starting Address Register */
			/* QMU GPD address --> CPU DMA address */
			gpd_current = (struct TGPD *) (uintptr_t) (os_readl(USB_QMU_TQSAR(Q_num)));
		}

		/*Switch physical to virtual address */
		os_printk(K_INFO, "gpd_current(P) %p\n", gpd_current);
		gpd_current = gpd_phys_to_virt(gpd_current, USB_TX, Q_num);
		os_printk(K_INFO, "gpd_current(V) %p\n", gpd_current);

		/*Reset the TX GPD list state */
		Tx_gpd_end[Q_num] = Tx_gpd_last[Q_num] = gpd_current;
		gpd_ptr_align(dir, Q_num, Tx_gpd_end[Q_num]);
		free_gpd(dir, Q_num);

		/*FIXME: Do not know why... */
		os_writel(USB_QMU_TQSAR(Q_num),
			  mu3d_hal_gpd_virt_to_phys(Tx_gpd_last[Q_num], USB_TX, Q_num));
		os_printk(K_INFO, "USB_QMU_TQSAR %x\n", os_readl(USB_QMU_TQSAR(Q_num)));
		req->complete = true;
		/* os_printk(K_ERR,"TxQ %d Flush Now!\n", Q_num); */
	} else if (dir == USB_RX) {
		/*Stop QMU */
		mu3d_hal_stop_qmu(Q_num, USB_RX);

		/*Get RX Queue Current Pointer Register */
		/* QMU GPD address --> CPU DMA address */
		gpd_current = (struct TGPD *) (uintptr_t) (os_readl(USB_QMU_RQCPR(Q_num)));
		if (!gpd_current) {
			/*Get RX Queue Starting Address Register */
			/* QMU GPD address --> CPU DMA address */
			gpd_current = (struct TGPD *) (uintptr_t) (os_readl(USB_QMU_RQSAR(Q_num)));
		}

		/*Switch physical to virtual address */
		os_printk(K_INFO, "gpd_current(P) %p\n", gpd_current);
		gpd_current = gpd_phys_to_virt(gpd_current, USB_RX, Q_num);
		os_printk(K_INFO, "gpd_current(V) %p\n", gpd_current);

		/*Reset the RX GPD list state */
		Rx_gpd_end[Q_num] = Rx_gpd_last[Q_num] = gpd_current;
		gpd_ptr_align(dir, Q_num, Rx_gpd_end[Q_num]);
		free_gpd(dir, Q_num);

		/*FIXME: Do not know why... */
		os_writel(USB_QMU_RQSAR(Q_num),
			  mu3d_hal_gpd_virt_to_phys(Rx_gpd_end[Q_num], USB_RX, Q_num));
		os_printk(K_INFO, "USB_QMU_RQSAR %x\n", os_readl(USB_QMU_RQSAR(Q_num)));
		req->complete = true;
		/* os_printk(K_ERR,"RxQ %d Flush Now!\n", Q_num); */
	}
}

#ifdef CONFIG_MTK_MD_DIRECT_TETHERING_SUPPORT
bool _ex_mu3d_hal_qmu_status_done(int Q_num, enum USB_DIR dir)
{
	qmu_printk(K_CRIT, "%s QMU status %s-EP[%d]\n", __func__, ((dir == USB_TX)?"TX":"RX"), Q_num);

	if (dir == USB_TX) {
		if (Tx_gpd_end[Q_num] == Tx_gpd_last[Q_num])
			return true;
	} else if (dir == USB_RX) {
		if (Rx_gpd_end[Q_num] == Rx_gpd_last[Q_num])
			return true;
	}

	return false;
}
#endif

void mu3d_reset_gpd_resource(void)
{
	unsigned int i;

	/* TX reset */
	for (i = 1; i <= MAX_QMU_EP; i++) {

		mu3d_hal_stop_qmu(i, USB_TX);

		/*Reset the TX GPD list state */
		Tx_gpd_end[i] = Tx_gpd_last[i] = Tx_gpd_head[i];
		gpd_ptr_align(USB_TX, i, Tx_gpd_end[i]);
		free_gpd(USB_TX, i);

		/*FIXME: Do not know why... */
		os_writel(USB_QMU_TQSAR(i),
				mu3d_hal_gpd_virt_to_phys(Tx_gpd_last[i], USB_TX, i));
		qmu_printk(K_INFO, "USB_QMU_TQSAR %x\n", os_readl(USB_QMU_TQSAR(i)));
	}

	/* RX reset */
	for (i = 1; i <= MAX_QMU_EP; i++) {
		mu3d_hal_stop_qmu(i, USB_RX);

		/*Reset the RX GPD list state */
		Rx_gpd_end[i] = Rx_gpd_last[i] = Rx_gpd_head[i];
		gpd_ptr_align(USB_RX, i, Rx_gpd_end[i]);
		free_gpd(USB_RX, i);

		/*FIXME: Do not know why... */
		os_writel(USB_QMU_RQSAR(i),
				mu3d_hal_gpd_virt_to_phys(Rx_gpd_end[i], USB_RX, i));
		qmu_printk(K_INFO, "USB_QMU_RQSAR %x\n", os_readl(USB_QMU_RQSAR(i)));
	}
}

#endif
