/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/******************************************************************************
*[File]             pcie.c
*[Version]          v1.0
*[Revision Date]    2010-03-01
*[Author]
*[Description]
*    The program provides PCIE HIF driver
*[Copyright]
*    Copyright (C) 2010 MediaTek Incorporation. All Rights Reserved.
******************************************************************************/


/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#include "gl_os.h"

#include "hif_pci.h"

#include "precomp.h"

#include <linux/mm.h>
#ifndef CONFIG_X86
#include <asm/memory.h>
#endif

#include "mt66xx_reg.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

#define MTK_PCI_VENDOR_ID	0x14C3
#define NIC6632_PCIe_DEVICE_ID	0x6632
#define NIC7668_PCIe_DEVICE_ID	0x7668

static const struct pci_device_id mtk_pci_ids[] = {
	{	PCI_DEVICE(MTK_PCI_VENDOR_ID, NIC6632_PCIe_DEVICE_ID),
		.driver_data = (kernel_ulong_t)&mt66xx_driver_data_mt6632},
	{	PCI_DEVICE(MTK_PCI_VENDOR_ID, NIC7668_PCIe_DEVICE_ID),
		.driver_data = (kernel_ulong_t)&mt66xx_driver_data_mt7668},
	{ /* end: all zeroes */ },
};

MODULE_DEVICE_TABLE(pci, mtk_pci_ids);

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
static probe_card pfWlanProbe;
static remove_card pfWlanRemove;

static struct pci_driver mtk_pci_driver = {
	.name = "wlan",
	.id_table = mtk_pci_ids,
	.probe = NULL,
	.remove = NULL,
};

static BOOLEAN g_fgDriverProbed = FALSE;
/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a PCIE interrupt callback function
*
* \param[in] func  pointer to PCIE handle
*
* \return void
*/
/*----------------------------------------------------------------------------*/
static PUCHAR CSRBaseAddress;

static irqreturn_t mtk_pci_interrupt(int irq, void *dev_instance)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4RegValue;

	prGlueInfo = (P_GLUE_INFO_T) dev_instance;
	if (!prGlueInfo) {
		DBGLOG(HAL, INFO, "No glue info in mtk_pci_interrupt()\n");
		return IRQ_NONE;
	}

	HAL_MCR_RD(prGlueInfo->prAdapter, WPDMA_INT_STA, &u4RegValue);
	if (!u4RegValue)
		return IRQ_HANDLED;

	halDisableInterrupt(prGlueInfo->prAdapter);

	if (prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
		DBGLOG(HAL, INFO, "GLUE_FLAG_HALT skip INT\n");
		return IRQ_NONE;
	}

	DBGLOG(HAL, TRACE, "%s INT[0x%08x]\n", __func__, u4RegValue);

	kalSetIntEvent(prGlueInfo);

	return IRQ_HANDLED;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a PCIE probe function
*
* \param[in] func   pointer to PCIE handle
* \param[in] id     pointer to PCIE device id table
*
* \return void
*/
/*----------------------------------------------------------------------------*/
static int mtk_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int ret = 0;

	ASSERT(pdev);
	ASSERT(id);

	ret = pci_enable_device(pdev);

	if (ret) {
		DBGLOG(INIT, INFO, "pci_enable_device failed!\n");
		goto out;
	}

	DBGLOG(INIT, INFO, "pci_enable_device done!\n");

	if (pfWlanProbe((PVOID) pdev, (PVOID) id->driver_data) != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO, "pfWlanProbe fail!call pfWlanRemove()\n");
		pfWlanRemove();
		ret = -1;
	} else {
		g_fgDriverProbed = TRUE;
	}
out:
	DBGLOG(INIT, INFO, "mtk_pci_probe() done(%d)\n", ret);

	return ret;
}

static void mtk_pci_remove(struct pci_dev *pdev)
{
	ASSERT(pdev);

	if (g_fgDriverProbed)
		pfWlanRemove();
	DBGLOG(INIT, INFO, "pfWlanRemove done\n");

	/* Unmap CSR base address */
	iounmap(CSRBaseAddress);

	/* release memory region */
	release_mem_region(pci_resource_start(pdev, 0), pci_resource_len(pdev, 0));

	pci_disable_device(pdev);
	DBGLOG(INIT, INFO, "mtk_pci_remove() done\n");
}

static int mtk_pci_suspend(struct pci_dev *pdev, pm_message_t state)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T)pci_get_drvdata(pdev);

	if (!prGlueInfo) {
		DBGLOG(HAL, ERROR, "pci_get_drvdata fail!\n");
		return -1;
	}

	wlanSuspendPmHandle(prGlueInfo);

	if (prGlueInfo->prAdapter->fgIsFwOwn == FALSE)
		RECLAIM_POWER_CONTROL_TO_PM(prGlueInfo->prAdapter, FALSE);

	pci_save_state(pdev);
	pci_set_power_state(pdev, pci_choose_state(pdev, state));
	prGlueInfo->prAdapter->fgIsFwOwn = TRUE;

	DBGLOG(HAL, STATE, "mtk_pci_suspend() done!\n");
	return 0;
}

int mtk_pci_resume(struct pci_dev *pdev)
{
	UINT_32 i;
	UINT_32 phy_addr, offset;
	RTMP_TX_RING *tx_ring;
	RTMP_RX_RING *rx_ring;
	P_GL_HIF_INFO_T prHifInfo;
	P_GLUE_INFO_T prGlueInfo = NULL;

	prGlueInfo = (P_GLUE_INFO_T)pci_get_drvdata(pdev);

	if (!prGlueInfo) {
		DBGLOG(HAL, ERROR, "pci_get_drvdata fail!\n");
		return -1;
	}

	prHifInfo = &prGlueInfo->rHifInfo;

	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);

	/* Restore PDMA settings */
	for (i = 0; i < NUM_OF_TX_RING; i++) {
		tx_ring = &prHifInfo->TxRing[i];
		offset = i * MT_RINGREG_DIFF;
		phy_addr = prHifInfo->TxRing[i].Cell[0].AllocPa;
		tx_ring->TxSwUsedIdx = 0;
		tx_ring->u4UsedCnt = 0;
		tx_ring->TxCpuIdx = 0;
		tx_ring->hw_desc_base = MT_TX_RING_BASE + offset;
		tx_ring->hw_cidx_addr = MT_TX_RING_CIDX + offset;
		tx_ring->hw_didx_addr = MT_TX_RING_DIDX + offset;
		tx_ring->hw_cnt_addr = MT_TX_RING_CNT + offset;
		kalDevRegWrite(prGlueInfo, tx_ring->hw_desc_base, phy_addr);
		kalDevRegWrite(prGlueInfo, tx_ring->hw_cidx_addr, tx_ring->TxCpuIdx);
		kalDevRegWrite(prGlueInfo, tx_ring->hw_cnt_addr, TX_RING_SIZE);
	}

	for (i = 0; i < NUM_OF_RX_RING; i++) {
		rx_ring = &prHifInfo->RxRing[i];
		offset = i * MT_RINGREG_DIFF;
		phy_addr = rx_ring->Cell[0].AllocPa;
		rx_ring->RxSwReadIdx = 0;
		rx_ring->RxCpuIdx = rx_ring->u4RingSize - 1;
		rx_ring->hw_desc_base = MT_RX_RING_BASE + offset;
		rx_ring->hw_cidx_addr = MT_RX_RING_CIDX + offset;
		rx_ring->hw_didx_addr = MT_RX_RING_DIDX + offset;
		rx_ring->hw_cnt_addr = MT_RX_RING_CNT + offset;
		kalDevRegWrite(prGlueInfo, rx_ring->hw_desc_base, phy_addr);
		kalDevRegWrite(prGlueInfo, rx_ring->hw_cidx_addr, rx_ring->RxCpuIdx);
		kalDevRegWrite(prGlueInfo, rx_ring->hw_cnt_addr, rx_ring->u4RingSize);
	}

	halWpdmaSetup(prGlueInfo, TRUE);
	halEnableInterrupt(prGlueInfo->prAdapter);
	DBGLOG(HAL, STATE, "PDMA restore done\n");

	if (prGlueInfo->prAdapter->fgIsFwOwn == FALSE) {
		prGlueInfo->prAdapter->fgIsFwOwn = TRUE;
		halSetDriverOwn(prGlueInfo->prAdapter);
	} else {
		ACQUIRE_POWER_CONTROL_FROM_PM(prGlueInfo->prAdapter);
	}

	kalMsleep(5);

	wlanResumePmHandle(prGlueInfo);

	DBGLOG(HAL, STATE, "mtk_pci_resume done\n");

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will register pci bus to the os
*
* \param[in] pfProbe    Function pointer to detect card
* \param[in] pfRemove   Function pointer to remove card
*
* \return The result of registering pci bus
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS glRegisterBus(probe_card pfProbe, remove_card pfRemove)
{
	int ret = 0;

	ASSERT(pfProbe);
	ASSERT(pfRemove);

	/* printk(KERN_INFO "mtk_pci: MediaTek PCIE WLAN driver\n"); */
	/* printk(KERN_INFO "mtk_pci: Copyright MediaTek Inc.\n"); */

	pfWlanProbe = pfProbe;
	pfWlanRemove = pfRemove;

	mtk_pci_driver.probe = mtk_pci_probe;
	mtk_pci_driver.remove = mtk_pci_remove;

	mtk_pci_driver.suspend = mtk_pci_suspend;
	mtk_pci_driver.resume = mtk_pci_resume;

	ret = (pci_register_driver(&mtk_pci_driver) == 0) ? WLAN_STATUS_SUCCESS : WLAN_STATUS_FAILURE;

	return ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will unregister pci bus to the os
*
* \param[in] pfRemove Function pointer to remove card
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID glUnregisterBus(remove_card pfRemove)
{
	if (g_fgDriverProbed) {
		pfRemove();
		g_fgDriverProbed = FALSE;
	}
	pci_unregister_driver(&mtk_pci_driver);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function stores hif related info, which is initialized before.
*
* \param[in] prGlueInfo Pointer to glue info structure
* \param[in] u4Cookie   Pointer to UINT_32 memory base variable for _HIF_HPI
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID glSetHifInfo(P_GLUE_INFO_T prGlueInfo, ULONG ulCookie)
{
	P_GL_HIF_INFO_T prHif = NULL;

	prHif = &prGlueInfo->rHifInfo;

	prHif->pdev = (struct pci_dev *)ulCookie;

	prHif->CSRBaseAddress = CSRBaseAddress;

	pci_set_drvdata(prHif->pdev, prGlueInfo);

	SET_NETDEV_DEV(prGlueInfo->prDevHandler, &prHif->pdev->dev);

	halWpdmaAllocRing(prGlueInfo);

	halWpdmaInitRing(prGlueInfo);

	spin_lock_init(&prHif->rDynMapRegLock);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function clears hif related info.
*
* \param[in] prGlueInfo Pointer to glue info structure
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID glClearHifInfo(P_GLUE_INFO_T prGlueInfo)
{
	halUninitMsduTokenInfo(prGlueInfo->prAdapter);
	halWpdmaFreeRing(prGlueInfo);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Initialize bus operation and hif related information, request resources.
*
* \param[out] pvData    A pointer to HIF-specific data type buffer.
*                       For eHPI, pvData is a pointer to UINT_32 type and stores a
*                       mapped base address.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
BOOL glBusInit(PVOID pvData)
{
	int ret = 0;
	struct pci_dev *pdev = NULL;

	ASSERT(pvData);

	pdev = (struct pci_dev *)pvData;

	ret = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
	if (ret != 0) {
		DBGLOG(INIT, INFO, "set DMA mask failed!errno=%d\n", ret);
		return FALSE;
	}

	ret = pci_request_regions(pdev, pci_name(pdev));
	if (ret != 0) {
		DBGLOG(INIT, INFO, "Request PCI resource failed, errno=%d!\n", ret);
		goto err_out;
	}

	/* map physical address to virtual address for accessing register */
	CSRBaseAddress = (PUCHAR) ioremap(pci_resource_start(pdev, 0), pci_resource_len(pdev, 0));
	DBGLOG(INIT, INFO, "ioremap for device %s, region 0x%lX @ 0x%lX\n",
		pci_name(pdev), (ULONG) pci_resource_len(pdev, 0), (ULONG) pci_resource_start(pdev, 0));
	if (!CSRBaseAddress) {
		DBGLOG(INIT, INFO, "ioremap failed for device %s, region 0x%lX @ 0x%lX\n",
			pci_name(pdev), (ULONG) pci_resource_len(pdev, 0), (ULONG) pci_resource_start(pdev, 0));
		goto err_out_free_res;
	}

	/* Set DMA master */
	pci_set_master(pdev);

	return TRUE;
#if 0
err_out_iounmap:
	iounmap((void *)(CSRBaseAddress));
	release_mem_region(pci_resource_start(pdev, 0), pci_resource_len(pdev, 0));
#endif
err_out_free_res:
	pci_release_regions(pdev);

err_out:
	pci_disable_device(pdev);

	return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Stop bus operation and release resources.
*
* \param[in] pvData A pointer to struct net_device.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID glBusRelease(PVOID pvData)
{
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Setup bus interrupt operation and interrupt handler for os.
*
* \param[in] pvData     A pointer to struct net_device.
* \param[in] pfnIsr     A pointer to interrupt handler function.
* \param[in] pvCookie   Private data for pfnIsr function.
*
* \retval WLAN_STATUS_SUCCESS   if success
*         NEGATIVE_VALUE   if fail
*/
/*----------------------------------------------------------------------------*/
INT_32 glBusSetIrq(PVOID pvData, PVOID pfnIsr, PVOID pvCookie)
{
	int ret = 0;

	struct net_device *prNetDevice = NULL;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_GL_HIF_INFO_T prHifInfo = NULL;
	struct pci_dev *pdev = NULL;

	ASSERT(pvData);
	if (!pvData)
		return -1;

	prNetDevice = (struct net_device *)pvData;
	prGlueInfo = (P_GLUE_INFO_T) pvCookie;
	ASSERT(prGlueInfo);
	if (!prGlueInfo)
		return -1;

	prHifInfo = &prGlueInfo->rHifInfo;
	pdev = prHifInfo->pdev;

#if defined(CONFIG_ARCH_MT7623) || defined(CONFIG_ARCH_MT7621)
	ret = request_irq(pdev->irq, mtk_pci_interrupt, IRQF_SHARED |
		IRQF_TRIGGER_FALLING, prNetDevice->name, prGlueInfo);
#else
	ret = request_irq(pdev->irq, mtk_pci_interrupt, IRQF_SHARED, prNetDevice->name, prGlueInfo);
#endif

	if (ret != 0)
		DBGLOG(INIT, INFO, "glBusSetIrq: request_irq  ERROR(%d)\n", ret);

	return ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Stop bus interrupt operation and disable interrupt handling for os.
*
* \param[in] pvData     A pointer to struct net_device.
* \param[in] pvCookie   Private data for pfnIsr function.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID glBusFreeIrq(PVOID pvData, PVOID pvCookie)
{
	struct net_device *prNetDevice = NULL;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_GL_HIF_INFO_T prHifInfo = NULL;
	struct pci_dev *pdev = NULL;

	ASSERT(pvData);
	if (!pvData) {
		DBGLOG(INIT, INFO, "%s null pvData\n", __func__);
		return;
	}
	prNetDevice = (struct net_device *)pvData;
	prGlueInfo = (P_GLUE_INFO_T) pvCookie;
	ASSERT(prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(INIT, INFO, "%s no glue info\n", __func__);
		return;
	}

	prHifInfo = &prGlueInfo->rHifInfo;
	pdev = prHifInfo->pdev;

	synchronize_irq(pdev->irq);
	free_irq(pdev->irq, prGlueInfo);
}

BOOLEAN glIsReadClearReg(UINT_32 u4Address)
{
	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Read a 32-bit device register
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
* \param[in] u4Register Register offset
* \param[in] pu4Value   Pointer to variable used to store read value
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
BOOL kalDevRegRead(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Register, OUT PUINT_32 pu4Value)
{
	P_GL_HIF_INFO_T prHifInfo = NULL;
	UINT_32 u4BusAddr = u4Register;
	BOOLEAN fgResult = TRUE;

	ASSERT(prGlueInfo);
	ASSERT(pu4Value);

	prHifInfo = &prGlueInfo->rHifInfo;

	if (halChipToStaticMapBusAddr(u4Register, &u4BusAddr)) {
		/* Static mapping */
		RTMP_IO_READ32(prHifInfo, u4BusAddr, pu4Value);
	} else {
		/* Dynamic mapping */
		fgResult = halGetDynamicMapReg(prHifInfo, u4BusAddr, pu4Value);
	}

	if ((u4Register & 0xFFFFF000) != PCIE_HIF_BASE)
		DBGLOG(HAL, INFO, "Get CR[0x%08x/0x%08x] value[0x%08x]\n", u4Register, u4BusAddr, *pu4Value);

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Write a 32-bit device register
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
* \param[in] u4Register Register offset
* \param[in] u4Value    Value to be written
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
BOOL kalDevRegWrite(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Register, IN UINT_32 u4Value)
{
	P_GL_HIF_INFO_T prHifInfo = NULL;
	UINT_32 u4BusAddr = u4Register;
	BOOLEAN fgResult = TRUE;

	ASSERT(prGlueInfo);

	prHifInfo = &prGlueInfo->rHifInfo;

	if (halChipToStaticMapBusAddr(u4Register, &u4BusAddr)) {
		/* Static mapping */
		RTMP_IO_WRITE32(prHifInfo, u4BusAddr, u4Value);
	} else {
		/* Dynamic mapping */
		fgResult = halSetDynamicMapReg(prHifInfo, u4BusAddr, u4Value);
	}

	if ((u4Register & 0xFFFFF000) != PCIE_HIF_BASE)
		DBGLOG(HAL, INFO, "Set CR[0x%08x/0x%08x] value[0x%08x]\n", u4Register, u4BusAddr, u4Value);

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Read device I/O port
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[in] u2Port             I/O port offset
* \param[in] u2Len              Length to be read
* \param[out] pucBuf            Pointer to read buffer
* \param[in] u2ValidOutBufSize  Length of the buffer valid to be accessed
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
BOOL
kalDevPortRead(IN P_GLUE_INFO_T prGlueInfo, IN UINT_16 u2Port, IN UINT_32 u4Len,
	OUT PUINT_8 pucBuf, IN UINT_32 u4ValidOutBufSize)
{
	P_GL_HIF_INFO_T prHifInfo = NULL;
	PUINT_8 pucDst = NULL;
	RXD_STRUCT *pRxD;
	P_RTMP_RX_RING prRxRing;
	spinlock_t *pRxRingLock;
	PVOID pRxPacket = NULL;
	RTMP_DMACB *pRxCell;
	struct pci_dev *pdev = NULL;
	BOOL fgRet = TRUE;
	ULONG flags = 0;
	PRTMP_DMABUF prDmaBuf;

	ASSERT(prGlueInfo);
	prHifInfo = &prGlueInfo->rHifInfo;

	ASSERT(pucBuf);
	pucDst = pucBuf;

	ASSERT(u4Len <= u4ValidOutBufSize);

	pdev = prHifInfo->pdev;

	prRxRing = &prHifInfo->RxRing[u2Port];
	pRxRingLock = &prHifInfo->RxRingLock[u2Port];

	spin_lock_irqsave(pRxRingLock, flags);

	pRxCell = &prRxRing->Cell[prRxRing->RxSwReadIdx];

	/* Point to Rx indexed rx ring descriptor */
	pRxD = (RXD_STRUCT *) pRxCell->AllocVa;

	if (pRxD->DDONE == 0) {
		/* Get how may packets had been received */
		kalDevRegRead(prGlueInfo, prRxRing->hw_didx_addr, &prRxRing->RxDmaIdx);

		DBGLOG(HAL, TRACE, "Rx DMA done P[%u] DMA[%u] SW_RD[%u]\n", u2Port,
			prRxRing->RxDmaIdx, prRxRing->RxSwReadIdx);
		fgRet = FALSE;
		goto done;
	}

	prDmaBuf = &pRxCell->DmaBuf;

	pci_unmap_single(pdev, prDmaBuf->AllocPa, prDmaBuf->AllocSize, PCI_DMA_FROMDEVICE);

	if (pRxD->LS0 == 0 || prRxRing->fgRxSegPkt) {
		/* Rx segmented packet */

		DBGLOG(HAL, WARN, "Skip Rx segmented packet, SDL0[%u] LS0[%u]\n", pRxD->SDL0, pRxD->LS0);
		if (pRxD->LS0 == 1) {
			/* Last segmented packet */
			prRxRing->fgRxSegPkt = FALSE;
		} else {
			/* Segmented packet */
			prRxRing->fgRxSegPkt = TRUE;
		}

		fgRet = FALSE;
		goto skip;
	}

	if (pRxD->SDL0 <= u4Len) {
		pRxPacket = pRxCell->pPacket;
		ASSERT(pRxPacket);
		kalMemCopy(pucDst, ((UCHAR *) ((struct sk_buff *)(pRxPacket))->data), pRxD->SDL0);
	} else
		DBGLOG(HAL, WARN, "Skip Rx packet, SDL0[%u] > SwRfb max len[%u]\n", pRxD->SDL0, u4Len);

skip:
	prDmaBuf->AllocVa = ((struct sk_buff *)pRxCell->pPacket)->data;
	prDmaBuf->AllocPa = pci_map_single(pdev, prDmaBuf->AllocVa, prDmaBuf->AllocSize, PCI_DMA_FROMDEVICE);

	pRxD->SDP0 = prDmaBuf->AllocPa;
	pRxD->SDL0 = prRxRing->u4BufSize;
	pRxD->DDONE = 0;

	prRxRing->RxCpuIdx = prRxRing->RxSwReadIdx;
	kalDevRegWrite(prGlueInfo, prRxRing->hw_cidx_addr, prRxRing->RxCpuIdx);
	INC_RING_INDEX(prRxRing->RxSwReadIdx, prRxRing->u4RingSize);

done:
	spin_unlock_irqrestore(pRxRingLock, flags);

	return fgRet;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Write device I/O port
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[in] u2Port             I/O port offset
* \param[in] u2Len              Length to be write
* \param[in] pucBuf             Pointer to write buffer
* \param[in] u2ValidInBufSize   Length of the buffer valid to be accessed
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
BOOL
kalDevPortWrite(IN P_GLUE_INFO_T prGlueInfo,
		IN UINT_16 u2Port, IN UINT_32 u4Len, IN PUINT_8 pucBuf, IN UINT_32 u4ValidInBufSize)
{
	P_GL_HIF_INFO_T prHifInfo = NULL;
	PUINT_8 pucSrc = NULL;
	UINT_32 u4SrcLen = u4Len;
	ULONG flags = 0;
	UINT_32 SwIdx = 0;
	P_RTMP_TX_RING prTxRing;
	spinlock_t *prTxRingLock;
	TXD_STRUCT *pTxD;
	struct pci_dev *pdev = NULL;

	ASSERT(prGlueInfo);
	prHifInfo = &prGlueInfo->rHifInfo;

	ASSERT(pucBuf);

	ASSERT(u4Len <= u4ValidInBufSize);

	pucSrc = kalMemAlloc(u4SrcLen, PHY_MEM_TYPE);
	ASSERT(pucSrc);

	kalMemCopy(pucSrc, pucBuf, u4SrcLen);

	pdev = prHifInfo->pdev;
	prTxRing = &prHifInfo->TxRing[u2Port];
	prTxRingLock = &prHifInfo->TxRingLock[u2Port];

	spin_lock_irqsave((spinlock_t *)prTxRingLock, flags);

	SwIdx = prTxRing->TxCpuIdx;
	pTxD = (TXD_STRUCT *) prTxRing->Cell[SwIdx].AllocVa;

	prTxRing->Cell[SwIdx].pPacket = NULL;
	prTxRing->Cell[SwIdx].pBuffer = pucSrc;
	prTxRing->Cell[SwIdx].PacketPa = pci_map_single(pdev, pucSrc, u4SrcLen, PCI_DMA_TODEVICE);

	pTxD->LastSec0 = 1;
	pTxD->LastSec1 = 0;
	pTxD->SDLen0 = u4SrcLen;
	pTxD->SDLen1 = 0;
	pTxD->SDPtr0 = prTxRing->Cell[SwIdx].PacketPa;
	pTxD->SDPtr1 = 0;
	pTxD->Burst = 0;
	pTxD->DMADONE = 0;

	/* Increase TX_CTX_IDX, but write to register later. */
	INC_RING_INDEX(prTxRing->TxCpuIdx, TX_RING_SIZE);

	prTxRing->u4UsedCnt++;

	kalDevRegWrite(prGlueInfo, prTxRing->hw_cidx_addr, prTxRing->TxCpuIdx);

	spin_unlock_irqrestore((spinlock_t *)prTxRingLock, flags);

	return TRUE;
}

VOID kalDevReadIntStatus(IN P_ADAPTER_T prAdapter, OUT PUINT_32 pu4IntStatus)
{
	UINT_32 u4RegValue;
	P_GL_HIF_INFO_T prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	*pu4IntStatus = 0;

	HAL_MCR_RD(prAdapter, WPDMA_INT_STA, &u4RegValue);

	if (HAL_IS_RX_DONE_INTR(u4RegValue))
		*pu4IntStatus |= WHISR_RX0_DONE_INT;

	if (HAL_IS_TX_DONE_INTR(u4RegValue))
		*pu4IntStatus |= WHISR_TX_DONE_INT;

	prHifInfo->u4IntStatus = u4RegValue;

	/* clear interrupt */
	HAL_MCR_WR(prAdapter, WPDMA_INT_STA, u4RegValue);

}

BOOL kalDevWriteCmd(IN P_GLUE_INFO_T prGlueInfo, IN P_CMD_INFO_T prCmdInfo, IN UINT_8 ucTC)
{
	P_GL_HIF_INFO_T prHifInfo = NULL;
	ULONG flags = 0;
	UINT_32 SwIdx = 0;
	P_RTMP_TX_RING prTxRing;
	spinlock_t *prTxRingLock;
	TXD_STRUCT *pTxD;
	struct pci_dev *pdev = NULL;
	UINT_16 u2Port = TX_RING_CMD_IDX_2;
	UINT_32 u4TotalLen;
	PUINT_8 pucSrc = NULL;

	ASSERT(prGlueInfo);
	prHifInfo = &prGlueInfo->rHifInfo;

	pdev = prHifInfo->pdev;
	prTxRing = &prHifInfo->TxRing[u2Port];
	prTxRingLock = &prHifInfo->TxRingLock[u2Port];

	u4TotalLen = prCmdInfo->u4TxdLen + prCmdInfo->u4TxpLen;
	pucSrc = kalMemAlloc(u4TotalLen, PHY_MEM_TYPE);
	ASSERT(pucSrc);

	kalMemCopy(pucSrc, prCmdInfo->pucTxd, prCmdInfo->u4TxdLen);
	kalMemCopy(pucSrc + prCmdInfo->u4TxdLen, prCmdInfo->pucTxp, prCmdInfo->u4TxpLen);

	spin_lock_irqsave((spinlock_t *)prTxRingLock, flags);

	SwIdx = prTxRing->TxCpuIdx;
	pTxD = (TXD_STRUCT *) prTxRing->Cell[SwIdx].AllocVa;

	prTxRing->Cell[SwIdx].pPacket = (PVOID)prCmdInfo;
	prTxRing->Cell[SwIdx].pBuffer = pucSrc;
	prTxRing->Cell[SwIdx].PacketPa = pci_map_single(pdev, pucSrc, u4TotalLen, PCI_DMA_TODEVICE);

	pTxD->SDPtr0 = prTxRing->Cell[SwIdx].PacketPa;
	pTxD->SDLen0 = u4TotalLen;
	pTxD->SDPtr1 = 0;
	pTxD->SDLen1 = 0;
	pTxD->LastSec0 = 1;
	pTxD->LastSec1 = 0;
	pTxD->Burst = 0;
	pTxD->DMADONE = 0;

	/* Increase TX_CTX_IDX, but write to register later. */
	INC_RING_INDEX(prTxRing->TxCpuIdx, TX_RING_SIZE);

	prTxRing->u4UsedCnt++;
	kalDevRegWrite(prGlueInfo, prTxRing->hw_cidx_addr, prTxRing->TxCpuIdx);

	spin_unlock_irqrestore((spinlock_t *)prTxRingLock, flags);

	DBGLOG(HAL, TRACE, "%s: CmdInfo[0x%p], TxD[0x%p/%u] TxP[0x%p/%u] CPU idx[%u] Used[%u]\n", __func__,
		prCmdInfo, prCmdInfo->pucTxd, prCmdInfo->u4TxdLen, prCmdInfo->pucTxp, prCmdInfo->u4TxpLen,
		SwIdx, prTxRing->u4UsedCnt);

	return TRUE;
}

BOOL kalDevWriteData(IN P_GLUE_INFO_T prGlueInfo, IN P_MSDU_INFO_T prMsduInfo)
{
	P_GL_HIF_INFO_T prHifInfo = NULL;
	ULONG flags = 0;
	UINT_32 SwIdx = 0;
	P_RTMP_TX_RING prTxRing;
	spinlock_t *prTxRingLock;
	TXD_STRUCT *pTxD;
	struct pci_dev *pdev = NULL;
	UINT_16 u2Port = TX_RING_DATA0_IDX_0;
	UINT_32 u4TotalLen;
	struct sk_buff *skb;
	PUINT_8 pucSrc;
	P_MSDU_TOKEN_ENTRY_T prToken;

	ASSERT(prGlueInfo);
	prHifInfo = &prGlueInfo->rHifInfo;

	pdev = prHifInfo->pdev;
	prTxRing = &prHifInfo->TxRing[u2Port];
	prTxRingLock = &prHifInfo->TxRingLock[u2Port];

	skb = (struct sk_buff *)prMsduInfo->prPacket;
	pucSrc = skb->data;
	u4TotalLen = skb->len;

	/* Acquire MSDU token */
	prToken = halAcquireMsduToken(prGlueInfo->prAdapter);

#if HIF_TX_PREALLOC_DATA_BUFFER
	kalMemCopy(prToken->prPacket, pucSrc, u4TotalLen);
#else
	prToken->prMsduInfo = prMsduInfo;
	prToken->prPacket = pucSrc;
	prToken->u4DmaLength = u4TotalLen;
	prMsduInfo->prToken = prToken;
#endif

	/* Update Tx descriptor */
	halTxUpdateCutThroughDesc(prGlueInfo, prMsduInfo, prToken);

	prToken->rDmaAddr = pci_map_single(pdev, prToken->prPacket, prToken->u4DmaLength, PCI_DMA_TODEVICE);
	if (pci_dma_mapping_error(pdev, prToken->rDmaAddr)) {
		DBGLOG(HAL, ERROR, "pci_map_single() error!\n");
		halReturnMsduToken(prGlueInfo->prAdapter, prToken->u4Token);
		return FALSE;
	}

	SwIdx = prTxRing->TxCpuIdx;
	pTxD = (TXD_STRUCT *) prTxRing->Cell[SwIdx].AllocVa;

	pTxD->SDPtr0 = prToken->rDmaAddr;
	pTxD->SDLen0 = HIF_TX_DESC_PAYLOAD_LENGTH;
	pTxD->SDPtr1 = 0;
	pTxD->SDLen1 = 0;
	pTxD->LastSec0 = 1;
	pTxD->LastSec1 = 0;
	pTxD->Burst = 0;
	pTxD->DMADONE = 0;

	/* Increase TX_CTX_IDX, but write to register later. */
	INC_RING_INDEX(prTxRing->TxCpuIdx, TX_RING_SIZE);

	/* Update HW Tx DMA ring */
	spin_lock_irqsave((spinlock_t *)prTxRingLock, flags);

	prTxRing->u4UsedCnt++;
	kalDevRegWrite(prGlueInfo, prTxRing->hw_cidx_addr, prTxRing->TxCpuIdx);

	spin_unlock_irqrestore((spinlock_t *)prTxRingLock, flags);

	DBGLOG(HAL, TRACE, "Tx Data: Msdu[0x%p], Tok[%u] TokFree[%u] CPU idx[%u] Used[%u] TxDone[%u]\n",
		prMsduInfo, prToken->u4Token, halGetMsduTokenFreeCnt(prGlueInfo->prAdapter),
		SwIdx, prTxRing->u4UsedCnt, (prMsduInfo->pfTxDoneHandler ? TRUE : FALSE));

	DBGLOG_MEM32(HAL, TRACE, pucSrc, HIF_TX_DESC_PAYLOAD_LENGTH);

	nicTxReleaseResource(prGlueInfo->prAdapter, prMsduInfo->ucTC,
		nicTxGetPageCount(prMsduInfo->u2FrameLength, TRUE), TRUE);

#if HIF_TX_PREALLOC_DATA_BUFFER
	if (!prMsduInfo->pfTxDoneHandler) {
		nicTxFreePacket(prGlueInfo->prAdapter, prMsduInfo, FALSE);
		nicTxReturnMsduInfo(prGlueInfo->prAdapter, prMsduInfo);
	}
#endif

	if (wlanGetTxPendingFrameCount(prGlueInfo->prAdapter))
		kalSetEvent(prGlueInfo);

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Kick Tx data to device
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
BOOL kalDevKickData(IN P_GLUE_INFO_T prGlueInfo)
{
	P_GL_HIF_INFO_T prHifInfo = NULL;
	ULONG flags = 0;
	P_RTMP_TX_RING prTxRing;
	spinlock_t *prTxRingLock;
	UINT_16 u2Port = TX_RING_DATA0_IDX_0;

	ASSERT(prGlueInfo);
	prHifInfo = &prGlueInfo->rHifInfo;

	prTxRing = &prHifInfo->TxRing[u2Port];
	prTxRingLock = &prHifInfo->TxRingLock[u2Port];

	spin_lock_irqsave((spinlock_t *)prTxRingLock, flags);

	kalDevRegWrite(prGlueInfo, prTxRing->hw_cidx_addr, prTxRing->TxCpuIdx);

	spin_unlock_irqrestore((spinlock_t *)prTxRingLock, flags);

	return TRUE;
}

BOOL kalDevReadData(IN P_GLUE_INFO_T prGlueInfo, IN UINT_16 u2Port, IN OUT P_SW_RFB_T prSwRfb)
{
	P_GL_HIF_INFO_T prHifInfo = NULL;
	RXD_STRUCT *pRxD;
	P_RTMP_RX_RING prRxRing;
	spinlock_t *pRxRingLock;
	PVOID pRxPacket = NULL;
	RTMP_DMACB *pRxCell;
	struct pci_dev *pdev = NULL;
	BOOL fgRet = TRUE;
	ULONG flags = 0;
	PRTMP_DMABUF prDmaBuf;

	ASSERT(prGlueInfo);
	prHifInfo = &prGlueInfo->rHifInfo;

	pdev = prHifInfo->pdev;

	prRxRing = &prHifInfo->RxRing[u2Port];
	pRxRingLock = &prHifInfo->RxRingLock[u2Port];

	spin_lock_irqsave(pRxRingLock, flags);

	pRxCell = &prRxRing->Cell[prRxRing->RxSwReadIdx];

	/* Point to Rx indexed rx ring descriptor */
	pRxD = (RXD_STRUCT *) pRxCell->AllocVa;

	if (pRxD->DDONE == 0) {
		/* Get how may packets had been received */
		kalDevRegRead(prGlueInfo, prRxRing->hw_didx_addr, &prRxRing->RxDmaIdx);

		DBGLOG(HAL, TRACE, "Rx DMA done P[%u] DMA[%u] SW_RD[%u]\n", u2Port,
			prRxRing->RxDmaIdx, prRxRing->RxSwReadIdx);
		fgRet = FALSE;
		goto done;
	}

	if (pRxD->LS0 == 0 || prRxRing->fgRxSegPkt) {
		/* Rx segmented packet */

		DBGLOG(HAL, WARN, "Skip Rx segmented data packet, SDL0[%u] LS0[%u]\n", pRxD->SDL0, pRxD->LS0);
		if (pRxD->LS0 == 1) {
			/* Last segmented packet */
			prRxRing->fgRxSegPkt = FALSE;
		} else {
			/* Segmented packet */
			prRxRing->fgRxSegPkt = TRUE;
		}

		fgRet = FALSE;
		goto skip;
	}

	pRxPacket = pRxCell->pPacket;
	ASSERT(pRxPacket);

	prDmaBuf = &pRxCell->DmaBuf;

	pRxCell->pPacket = prSwRfb->pvPacket;

	pci_unmap_single(pdev, prDmaBuf->AllocPa, prDmaBuf->AllocSize, PCI_DMA_FROMDEVICE);
	prSwRfb->pvPacket = pRxPacket;
	prSwRfb->pucRecvBuff = ((struct sk_buff *)pRxPacket)->data;
	prSwRfb->prRxStatus = (P_HW_MAC_RX_DESC_T)prSwRfb->pucRecvBuff;

	prDmaBuf->AllocVa = ((struct sk_buff *)pRxCell->pPacket)->data;
	prDmaBuf->AllocPa = pci_map_single(pdev, prDmaBuf->AllocVa, prDmaBuf->AllocSize, PCI_DMA_FROMDEVICE);

	pRxD->SDP0 = prDmaBuf->AllocPa;
skip:
	pRxD->SDL0 = prRxRing->u4BufSize;
	pRxD->DDONE = 0;

	prRxRing->RxCpuIdx = prRxRing->RxSwReadIdx;
	kalDevRegWrite(prGlueInfo, prRxRing->hw_cidx_addr, prRxRing->RxCpuIdx);
	INC_RING_INDEX(prRxRing->RxSwReadIdx, prRxRing->u4RingSize);

done:
	spin_unlock_irqrestore(pRxRingLock, flags);

	return fgRet;
}


VOID kalPciUnmapToDev(IN P_GLUE_INFO_T prGlueInfo, IN dma_addr_t rDmaAddr, IN UINT_32 u4Length)
{
	P_GL_HIF_INFO_T prHifInfo = NULL;
	struct pci_dev *pdev = NULL;

	prHifInfo = &prGlueInfo->rHifInfo;
	pdev = prHifInfo->pdev;
	pci_unmap_single(pdev, rDmaAddr, u4Length, PCI_DMA_TODEVICE);
}

VOID glSetPowerState(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 ePowerMode)
{
}

void glGetDev(PVOID ctx, struct device **dev)
{
	*dev = &((struct pci_dev *)ctx)->dev;
}

void glGetHifDev(P_GL_HIF_INFO_T prHif, struct device **dev)
{
	*dev = &(prHif->pdev->dev);
}
