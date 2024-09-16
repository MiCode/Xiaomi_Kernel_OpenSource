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
*[File]             hif_api.c
*[Version]          v1.0
*[Revision Date]    2015-09-08
*[Author]
*[Description]
*    The program provides USB HIF APIs
*[Copyright]
*    Copyright (C) 2015 MediaTek Incorporation. All Rights Reserved.
******************************************************************************/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "precomp.h"

#include <linux/usb.h>
#include <linux/mutex.h>

#include <linux/mm.h>
#ifndef CONFIG_X86
#include <asm/memory.h>
#endif

#include "mt66xx_reg.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

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
static const UINT_16 arTcToUSBEP[USB_TC_NUM] = {
	USB_DATA_BULK_OUT_EP4,
	USB_DATA_BULK_OUT_EP5,
	USB_DATA_BULK_OUT_EP6,
	USB_DATA_BULK_OUT_EP7,
	USB_DATA_BULK_OUT_EP8,
	USB_DATA_BULK_OUT_EP4,
	USB_DATA_BULK_OUT_EP9,

	/* Second HW queue */
#if NIC_TX_ENABLE_SECOND_HW_QUEUE
	USB_DATA_BULK_OUT_EP9,
	USB_DATA_BULK_OUT_EP9,
	USB_DATA_BULK_OUT_EP9,
	USB_DATA_BULK_OUT_EP9,
#endif
};

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
* @brief Verify the CHIP ID
*
* @param prAdapter      a pointer to adapter private data structure.
*
*
* @retval TRUE          CHIP ID is the same as the setting compiled
* @retval FALSE         CHIP ID is different from the setting compiled
*/
/*----------------------------------------------------------------------------*/
BOOL halVerifyChipID(IN P_ADAPTER_T prAdapter)
{
	UINT_32 u4CIR = 0;
	struct mt66xx_chip_info *prChipInfo;

	ASSERT(prAdapter);

	if (prAdapter->fgIsReadRevID)
		return TRUE;

	prChipInfo = prAdapter->chip_info;

	HAL_MCR_RD(prAdapter, TOP_HCR, &u4CIR);
	DBGLOG(INIT, TRACE, "Chip ID: 0x%x\n", u4CIR);

	if (u4CIR != prChipInfo->chip_id)
		return FALSE;

	HAL_MCR_RD(prAdapter, TOP_HVR, &u4CIR);
	DBGLOG(INIT, TRACE, "Revision ID: 0x%x\n", u4CIR);

	prAdapter->ucRevID = (UINT_8) (u4CIR & 0xF);
	prAdapter->fgIsReadRevID = TRUE;
	return TRUE;
}

WLAN_STATUS
halRxWaitResponse(IN P_ADAPTER_T prAdapter, IN UINT_8 ucPortIdx, OUT PUINT_8 pucRspBuffer,
		  IN UINT_32 u4MaxRespBufferLen, OUT PUINT_32 pu4Length)
{
	P_GL_HIF_INFO_T prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;
	P_RX_CTRL_T prRxCtrl;
	BOOL ret = FALSE;

	DEBUGFUNC("halRxWaitResponse");

	ASSERT(prAdapter);
	ASSERT(pucRspBuffer);

	prRxCtrl = &prAdapter->rRxCtrl;

	if (prHifInfo->fgEventEpDetected == FALSE) {
		/* NOTE: This is temporary compatiable code with old/new CR4 FW to detect which EVENT endpoint that
		 *       CR4 FW is using. If the new EP4IN-using CR4 FW works without any issue for a while,
		 *       this code block will be removed.
		 */
		if (prAdapter->fgIsCr4FwDownloaded) {
			ucPortIdx = USB_DATA_EP_IN;
			ret = kalDevPortRead(prAdapter->prGlueInfo, ucPortIdx,
				ALIGN_4(u4MaxRespBufferLen) + LEN_USB_RX_PADDING_CSO,
				prRxCtrl->pucRxCoalescingBufPtr, HIF_RX_COALESCING_BUFFER_SIZE);

			if (ret == TRUE) {
				prHifInfo->eEventEpType = EVENT_EP_TYPE_DATA_EP;
			} else {
				ucPortIdx = USB_EVENT_EP_IN;
				ret = kalDevPortRead(prAdapter->prGlueInfo, ucPortIdx,
					ALIGN_4(u4MaxRespBufferLen) + LEN_USB_RX_PADDING_CSO,
					prRxCtrl->pucRxCoalescingBufPtr, HIF_RX_COALESCING_BUFFER_SIZE);
			}
			prHifInfo->fgEventEpDetected = TRUE;

			kalMemCopy(pucRspBuffer, prRxCtrl->pucRxCoalescingBufPtr, u4MaxRespBufferLen);
			*pu4Length = u4MaxRespBufferLen;

			if (ret == FALSE)
				u4Status = WLAN_STATUS_FAILURE;
			return u4Status;
		}

		ucPortIdx = USB_EVENT_EP_IN;
	} else {
		if (prHifInfo->eEventEpType == EVENT_EP_TYPE_DATA_EP)
			if (prAdapter->fgIsCr4FwDownloaded)
				ucPortIdx = USB_DATA_EP_IN;
			else
				ucPortIdx = USB_EVENT_EP_IN;
		else
			ucPortIdx = USB_EVENT_EP_IN;

	}
	ret = kalDevPortRead(prAdapter->prGlueInfo, ucPortIdx,
		ALIGN_4(u4MaxRespBufferLen) + LEN_USB_RX_PADDING_CSO,
		prRxCtrl->pucRxCoalescingBufPtr, HIF_RX_COALESCING_BUFFER_SIZE);

	kalMemCopy(pucRspBuffer, prRxCtrl->pucRxCoalescingBufPtr, u4MaxRespBufferLen);
	*pu4Length = u4MaxRespBufferLen;

	if (ret == FALSE)
		u4Status = WLAN_STATUS_FAILURE;

	return u4Status;
}

WLAN_STATUS halTxUSBSendCmd(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucTc, IN P_CMD_INFO_T prCmdInfo)
{
	P_GL_HIF_INFO_T prHifInfo = &prGlueInfo->rHifInfo;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;
	P_USB_REQ_T prUsbReq;
	P_BUF_CTRL_T prBufCtrl;
	UINT_16 u2OverallBufferLength = 0;
	unsigned long flags;
	P_HW_MAC_TX_DESC_T prTxDesc;
	UINT_8 ucQueIdx;

	prUsbReq = glUsbDequeueReq(prHifInfo, &prHifInfo->rTxCmdFreeQ, &prHifInfo->rTxCmdQLock);
	if (prUsbReq == NULL)
		return WLAN_STATUS_RESOURCES;

	prBufCtrl = prUsbReq->prBufCtrl;

	if ((TFCB_FRAME_PAD_TO_DW(prCmdInfo->u4TxdLen + prCmdInfo->u4TxpLen) + LEN_USB_UDMA_TX_TERMINATOR) >
	    prBufCtrl->u4BufSize) {
		DBGLOG(HAL, ERROR, "Command TX buffer underflow!\n");
		glUsbEnqueueReq(prHifInfo, &prHifInfo->rTxCmdFreeQ, prUsbReq,
				&prHifInfo->rTxCmdQLock, FALSE);
		return WLAN_STATUS_RESOURCES;
	}

	DBGLOG(HAL, INFO, "TX URB[0x%p]\n", prUsbReq->prUrb);

	if (prCmdInfo->u4TxdLen) {
		memcpy((prBufCtrl->pucBuf + u2OverallBufferLength), prCmdInfo->pucTxd, prCmdInfo->u4TxdLen);
		u2OverallBufferLength += prCmdInfo->u4TxdLen;
	}

	if (prCmdInfo->u4TxpLen) {
		memcpy((prBufCtrl->pucBuf + u2OverallBufferLength), prCmdInfo->pucTxp, prCmdInfo->u4TxpLen);
		u2OverallBufferLength += prCmdInfo->u4TxpLen;
	}

	prTxDesc = (P_HW_MAC_TX_DESC_T)prBufCtrl->pucBuf;
	ucQueIdx = HAL_MAC_TX_DESC_GET_QUEUE_INDEX(prTxDesc);
	/* For H2CDMA Tx CMD mapping */
	/* Mapping port1 queue0~3 to queue28~31, and CR4 will unmask this */
	HAL_MAC_TX_DESC_SET_QUEUE_INDEX(prTxDesc, (ucQueIdx | USB_TX_CMD_QUEUE_MASK));

	/* DBGLOG_MEM32(SW4, INFO, prBufCtrl->pucBuf, 32); */

	memset(prBufCtrl->pucBuf + u2OverallBufferLength, 0,
	       ((TFCB_FRAME_PAD_TO_DW(u2OverallBufferLength) - u2OverallBufferLength) + LEN_USB_UDMA_TX_TERMINATOR));
	prBufCtrl->u4WrIdx = TFCB_FRAME_PAD_TO_DW(u2OverallBufferLength) + LEN_USB_UDMA_TX_TERMINATOR;

	prUsbReq->prPriv = (PVOID) prCmdInfo;
	usb_fill_bulk_urb(prUsbReq->prUrb,
			  prHifInfo->udev,
			  usb_sndbulkpipe(prHifInfo->udev, arTcToUSBEP[ucTc]),
			  (void *)prUsbReq->prBufCtrl->pucBuf,
			  prBufCtrl->u4WrIdx, halTxUSBSendCmdComplete, (void *)prUsbReq);

#if CFG_USB_CONSISTENT_DMA
	prUsbReq->prUrb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
#endif
	spin_lock_irqsave(&prHifInfo->rTxCmdQLock, flags);
	u4Status = glUsbSubmitUrb(prHifInfo, prUsbReq->prUrb, SUBMIT_TYPE_TX_CMD);
	if (u4Status) {
		DBGLOG(HAL, ERROR, "glUsbSubmitUrb() reports error (%d)\n", u4Status);
		list_add_tail(&prUsbReq->list, &prHifInfo->rTxCmdFreeQ);
		spin_unlock_irqrestore(&prHifInfo->rTxCmdQLock, flags);
		return WLAN_STATUS_FAILURE;
	}
	list_add_tail(&prUsbReq->list, &prHifInfo->rTxCmdSendingQ);
	spin_unlock_irqrestore(&prHifInfo->rTxCmdQLock, flags);

	if (wlanIsChipRstRecEnabled(prGlueInfo->prAdapter)
			&& wlanIsChipNoAck(prGlueInfo->prAdapter)) {
		wlanChipRstPreAct(prGlueInfo->prAdapter);
#if CFG_CHIP_RESET_SUPPORT
		glResetTrigger(prGlueInfo->prAdapter);
#else
		DBGLOG(HAL, ERROR, "usb trigger whole reset\n");
		HAL_WIFI_FUNC_CHIP_RESET(prGlueInfo->prAdapter);
#endif
	}
	return u4Status;
}

VOID halTxUSBSendCmdComplete(struct urb *urb)
{
	P_USB_REQ_T prUsbReq = urb->context;
	P_GL_HIF_INFO_T prHifInfo = prUsbReq->prHifInfo;
	P_GLUE_INFO_T prGlueInfo = prHifInfo->prGlueInfo;
	unsigned long flags;

#if CFG_USB_TX_HANDLE_IN_HIF_THREAD
	spin_lock_irqsave(&prHifInfo->rTxCmdQLock, flags);
	list_del_init(&prUsbReq->list);
	list_add_tail(&prUsbReq->list, &prHifInfo->rTxCmdCompleteQ);
	spin_unlock_irqrestore(&prHifInfo->rTxCmdQLock, flags);

	kalSetIntEvent(prGlueInfo);
#else
	spin_lock_irqsave(&prHifInfo->rTxCmdQLock, flags);
	list_del_init(&prUsbReq->list);
	spin_unlock_irqrestore(&prHifInfo->rTxCmdQLock, flags);

	halTxUSBProcessCmdComplete(prGlueInfo->prAdapter, prUsbReq);
#endif
}

VOID halTxUSBProcessCmdComplete(IN P_ADAPTER_T prAdapter, P_USB_REQ_T prUsbReq)
{
	struct urb *urb = prUsbReq->prUrb;
	UINT_32 u4SentDataSize;
	P_GL_HIF_INFO_T prHifInfo = prUsbReq->prHifInfo;
	static UINT_32 uUsbCmdFailCounter;

	if (urb->status != 0) {
		DBGLOG(TX, ERROR, "[%s] send CMD fail (status = %d)\n", __func__, urb->status);
		/* TODO: handle error */
		if ((urb->status != -ENOENT) &&
			(urb->status != -ECONNRESET) &&
			(urb->status != -ESHUTDOWN)) {
			uUsbCmdFailCounter++;
			if (uUsbCmdFailCounter > 3) {
				prAdapter->fgIsChipNoAck = TRUE;
				uUsbCmdFailCounter = 0;
			}
		}
	} else {
		uUsbCmdFailCounter = 0;
	}
	DBGLOG(HAL, INFO, "TX CMD DONE: URB[0x%p]\n", urb);

	glUsbEnqueueReq(prHifInfo, &prHifInfo->rTxCmdFreeQ, prUsbReq, &prHifInfo->rTxCmdQLock, FALSE);

	u4SentDataSize = urb->actual_length - LEN_USB_UDMA_TX_TERMINATOR;
	nicTxReleaseResource(prAdapter, TC4_INDEX, nicTxGetPageCount(u4SentDataSize, TRUE), TRUE);
}

VOID halTxCancelSendingCmd(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo)
{
	P_USB_REQ_T prUsbReq, prNext;
	unsigned long flags;
	struct urb *urb = NULL;
	P_GL_HIF_INFO_T prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	spin_lock_irqsave(&prHifInfo->rTxCmdQLock, flags);
	list_for_each_entry_safe(prUsbReq, prNext, &prHifInfo->rTxCmdSendingQ, list) {
		if (prUsbReq->prPriv == (PVOID) prCmdInfo) {
			list_del_init(&prUsbReq->list);
			urb = prUsbReq->prUrb;
			break;
		}
	}
	spin_unlock_irqrestore(&prHifInfo->rTxCmdQLock, flags);

	if (urb) {
		prCmdInfo->pfHifTxCmdDoneCb = NULL;
		usb_kill_urb(urb);
	}
}

VOID halTxCancelAllSending(IN P_ADAPTER_T prAdapter)
{
	P_GLUE_INFO_T prGlueInfo;
	P_USB_REQ_T prUsbReq, prUsbReqNext;
	P_GL_HIF_INFO_T prHifInfo;
#if CFG_USB_TX_AGG
	UINT_8 ucTc;
#endif

	ASSERT(prAdapter);
	prGlueInfo = prAdapter->prGlueInfo;
	prHifInfo = &prGlueInfo->rHifInfo;

	list_for_each_entry_safe(prUsbReq, prUsbReqNext, &prHifInfo->rTxCmdSendingQ, list) {
		usb_kill_urb(prUsbReq->prUrb);
	}

#if CFG_USB_TX_AGG
	for (ucTc = 0; ucTc < USB_TC_NUM; ++ucTc)
		usb_kill_anchored_urbs(&prHifInfo->rTxDataAnchor[ucTc]);
#else
	usb_kill_anchored_urbs(&prHifInfo->rTxDataAnchor);
#endif
}

#if CFG_USB_TX_AGG
WLAN_STATUS halTxUSBSendAggData(IN P_GL_HIF_INFO_T prHifInfo, IN UINT_8 ucTc, IN P_USB_REQ_T prUsbReq)
{
	P_GLUE_INFO_T prGlueInfo = prHifInfo->prGlueInfo;
	P_BUF_CTRL_T prBufCtrl = prUsbReq->prBufCtrl;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;

	memset(prBufCtrl->pucBuf + prBufCtrl->u4WrIdx, 0, LEN_USB_UDMA_TX_TERMINATOR);
	prBufCtrl->u4WrIdx += LEN_USB_UDMA_TX_TERMINATOR;

	list_del_init(&prUsbReq->list);

	usb_fill_bulk_urb(prUsbReq->prUrb,
			  prHifInfo->udev,
			  usb_sndbulkpipe(prHifInfo->udev, arTcToUSBEP[ucTc]),
			  (void *)prBufCtrl->pucBuf, prBufCtrl->u4WrIdx, halTxUSBSendDataComplete, (void *)prUsbReq);
#if CFG_USB_CONSISTENT_DMA
	prUsbReq->prUrb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
#endif

	usb_anchor_urb(prUsbReq->prUrb, &prHifInfo->rTxDataAnchor[ucTc]);
	u4Status = glUsbSubmitUrb(prHifInfo, prUsbReq->prUrb, SUBMIT_TYPE_TX_DATA);
	if (u4Status) {
		DBGLOG(HAL, ERROR, "glUsbSubmitUrb() reports error (%d)\n", u4Status);
		halTxUSBProcessMsduDone(prHifInfo->prGlueInfo, prUsbReq);
		prBufCtrl->u4WrIdx = 0;
		usb_unanchor_urb(prUsbReq->prUrb);
		list_add_tail(&prUsbReq->list, &prHifInfo->rTxDataCompleteQ);
#if CFG_USB_TX_HANDLE_IN_HIF_THREAD
		kalSetIntEvent(prGlueInfo);
#else
		/*tasklet_hi_schedule(&prGlueInfo->rTxCompleteTask);*/
		tasklet_schedule(&prGlueInfo->rTxCompleteTask);
#endif
		return WLAN_STATUS_FAILURE;
	}

	return u4Status;
}
#endif

WLAN_STATUS halTxUSBSendData(IN P_GLUE_INFO_T prGlueInfo, IN P_MSDU_INFO_T prMsduInfo)
{
	P_GL_HIF_INFO_T prHifInfo = &prGlueInfo->rHifInfo;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;
	P_USB_REQ_T prUsbReq;
	P_BUF_CTRL_T prBufCtrl;
	UINT_32 u4PaddingLength;
	struct sk_buff *skb;
	UINT_8 ucTc;
	UINT_8 *pucBuf;
	UINT_32 u4Length;
#if CFG_USB_TX_AGG
	unsigned long flags;
#endif

	skb = (struct sk_buff *)prMsduInfo->prPacket;
	pucBuf = skb->data;
	u4Length = skb->len;
	ucTc = USB_TRANS_MSDU_TC(prMsduInfo);
#if CFG_USB_TX_AGG
	spin_lock_irqsave(&prHifInfo->rTxDataQLock, flags);

	if (list_empty(&prHifInfo->rTxDataFreeQ[ucTc])) {
		if (glUsbBorrowFfaReq(prHifInfo, ucTc) == FALSE) {
			spin_unlock_irqrestore(&prHifInfo->rTxDataQLock, flags);
			DBGLOG(HAL, ERROR, "run out of rTxDataFreeQ #1!!\n");
			wlanProcessQueuedMsduInfo(prGlueInfo->prAdapter, prMsduInfo);
			return WLAN_STATUS_RESOURCES;
		}
	}
	prUsbReq = list_entry(prHifInfo->rTxDataFreeQ[ucTc].next, struct _USB_REQ_T, list);
	prBufCtrl = prUsbReq->prBufCtrl;

	if (prHifInfo->u4AggRsvSize[ucTc] < ALIGN_4(u4Length))
		DBGLOG(HAL, ERROR, "u4AggRsvSize[%hhu] count FAIL (%u, %u)\n",
		       ucTc, prHifInfo->u4AggRsvSize[ucTc], u4Length);
	prHifInfo->u4AggRsvSize[ucTc] -= ALIGN_4(u4Length);

	if (prBufCtrl->u4WrIdx + ALIGN_4(u4Length) + LEN_USB_UDMA_TX_TERMINATOR > prBufCtrl->u4BufSize) {
		halTxUSBSendAggData(prHifInfo, ucTc, prUsbReq);

		if (list_empty(&prHifInfo->rTxDataFreeQ[ucTc])) {
			if (glUsbBorrowFfaReq(prHifInfo, ucTc) == FALSE) {
				spin_unlock_irqrestore(&prHifInfo->rTxDataQLock, flags);
				DBGLOG(HAL, ERROR, "run out of rTxDataFreeQ #2!!\n");
				wlanProcessQueuedMsduInfo(prGlueInfo->prAdapter, prMsduInfo);
				return WLAN_STATUS_FAILURE;
			}
		}

		prUsbReq = list_entry(prHifInfo->rTxDataFreeQ[ucTc].next, struct _USB_REQ_T, list);
		prBufCtrl = prUsbReq->prBufCtrl;
	}

	memcpy(prBufCtrl->pucBuf + prBufCtrl->u4WrIdx, pucBuf, u4Length);
	prBufCtrl->u4WrIdx += u4Length;

	u4PaddingLength = (ALIGN_4(u4Length) - u4Length);
	if (u4PaddingLength) {
		memset(prBufCtrl->pucBuf + prBufCtrl->u4WrIdx, 0, u4PaddingLength);
		prBufCtrl->u4WrIdx += u4PaddingLength;
	}

	if (!prMsduInfo->pfTxDoneHandler)
		QUEUE_INSERT_TAIL(&prUsbReq->rSendingDataMsduInfoList, (P_QUE_ENTRY_T) prMsduInfo);

	if (usb_anchor_empty(&prHifInfo->rTxDataAnchor[ucTc]))
		halTxUSBSendAggData(prHifInfo, ucTc, prUsbReq);

	spin_unlock_irqrestore(&prHifInfo->rTxDataQLock, flags);
#else
	prUsbReq = glUsbDequeueReq(prHifInfo, &prHifInfo->rTxDataFreeQ, &prHifInfo->rTxDataQLock);
	if (prUsbReq == NULL) {
		DBGLOG(HAL, ERROR, "run out of rTxDataFreeQ!!\n");
		wlanProcessQueuedMsduInfo(prGlueInfo->prAdapter, prMsduInfo);
		return WLAN_STATUS_RESOURCES;
	}

	prBufCtrl = prUsbReq->prBufCtrl;
	prBufCtrl->u4WrIdx = 0;

	memcpy(prBufCtrl->pucBuf, pucBuf, u4Length);
	prBufCtrl->u4WrIdx += u4Length;

	u4PaddingLength = (ALIGN_4(u4Length) - u4Length);
	if (u4PaddingLength) {
		memset(prBufCtrl->pucBuf + prBufCtrl->u4WrIdx, 0, u4PaddingLength);
		prBufCtrl->u4WrIdx += u4PaddingLength;
	}

	memset(prBufCtrl->pucBuf + prBufCtrl->u4WrIdx, 0, LEN_USB_UDMA_TX_TERMINATOR);
	prBufCtrl->u4WrIdx += LEN_USB_UDMA_TX_TERMINATOR;

	if (!prMsduInfo->pfTxDoneHandler)
		QUEUE_INSERT_TAIL(&prUsbReq->rSendingDataMsduInfoList, (P_QUE_ENTRY_T) prMsduInfo);

	*((PUINT_8)&prUsbReq->prPriv) = ucTc;
	usb_fill_bulk_urb(prUsbReq->prUrb,
			  prHifInfo->udev,
			  usb_sndbulkpipe(prHifInfo->udev, arTcToUSBEP[ucTc]),
			  (void *)prUsbReq->prBufCtrl->pucBuf,
			  prBufCtrl->u4WrIdx, halTxUSBSendDataComplete, (void *)prUsbReq);
#if CFG_USB_CONSISTENT_DMA
	prUsbReq->prUrb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
#endif

	usb_anchor_urb(prUsbReq->prUrb, &prHifInfo->rTxDataAnchor);
	u4Status = glUsbSubmitUrb(prHifInfo, prUsbReq->prUrb, SUBMIT_TYPE_TX_DATA);
	if (u4Status) {
		DBGLOG(HAL, ERROR, "glUsbSubmitUrb() reports error (%d)\n", u4Status);
		halTxUSBProcessMsduDone(prHifInfo->prGlueInfo, prUsbReq);
		prBufCtrl->u4WrIdx = 0;
		usb_unanchor_urb(prUsbReq->prUrb);
		glUsbEnqueueReq(prHifInfo, &prHifInfo->rTxDataFreeQ, prUsbReq, &prHifInfo->rTxDataQLock, FALSE);
		return WLAN_STATUS_FAILURE;
	}
#endif

	if (wlanIsChipRstRecEnabled(prGlueInfo->prAdapter)
			&& wlanIsChipNoAck(prGlueInfo->prAdapter)) {
		wlanChipRstPreAct(prGlueInfo->prAdapter);
#if CFG_CHIP_RESET_SUPPORT
		glResetTrigger(prGlueInfo->prAdapter);
#else
		DBGLOG(HAL, ERROR, "usb trigger whole reset\n");
		HAL_WIFI_FUNC_CHIP_RESET(prGlueInfo->prAdapter);
#endif
	}
	return u4Status;
}

WLAN_STATUS halTxUSBKickData(IN P_GLUE_INFO_T prGlueInfo)
{
#if CFG_USB_TX_AGG
	P_GL_HIF_INFO_T prHifInfo = &prGlueInfo->rHifInfo;
	P_USB_REQ_T prUsbReq;
	P_BUF_CTRL_T prBufCtrl;
	UINT_8 ucTc;
	unsigned long flags;

	spin_lock_irqsave(&prHifInfo->rTxDataQLock, flags);

	for (ucTc = TC0_INDEX; ucTc < USB_TC_NUM; ucTc++) {
		if (list_empty(&prHifInfo->rTxDataFreeQ[ucTc]))
			continue;

		prUsbReq = list_entry(prHifInfo->rTxDataFreeQ[ucTc].next, struct _USB_REQ_T, list);
		prBufCtrl = prUsbReq->prBufCtrl;

		if (prBufCtrl->u4WrIdx)
			halTxUSBSendAggData(prHifInfo, ucTc, prUsbReq);
	}

	spin_unlock_irqrestore(&prHifInfo->rTxDataQLock, flags);
#endif

	return WLAN_STATUS_SUCCESS;
}

VOID halTxUSBSendDataComplete(struct urb *urb)
{
	P_USB_REQ_T prUsbReq = urb->context;
	P_GL_HIF_INFO_T prHifInfo = prUsbReq->prHifInfo;
	P_GLUE_INFO_T prGlueInfo = prHifInfo->prGlueInfo;

	glUsbEnqueueReq(prHifInfo, &prHifInfo->rTxDataCompleteQ, prUsbReq, &prHifInfo->rTxDataQLock, FALSE);

#if CFG_USB_TX_HANDLE_IN_HIF_THREAD
	kalSetIntEvent(prGlueInfo);
#else
	/*tasklet_hi_schedule(&prGlueInfo->rTxCompleteTask);*/
	tasklet_schedule(&prGlueInfo->rTxCompleteTask);
#endif
}

VOID halTxUSBProcessMsduDone(IN P_GLUE_INFO_T prGlueInfo, P_USB_REQ_T prUsbReq)
{
	UINT_8 ucTc;
	QUE_T rFreeQueue;
	P_QUE_T prFreeQueue;
	struct urb *urb = prUsbReq->prUrb;
	UINT_32 u4SentDataSize;

	if (g_u4HaltFlag) {
		DBGLOG(TX, WARN, "wlan is halt\n");
		return;
	}

	ucTc = *((PUINT_8)&prUsbReq->prPriv) & TC_MASK;

	prFreeQueue = &rFreeQueue;
	QUEUE_INITIALIZE(prFreeQueue);
	QUEUE_MOVE_ALL((prFreeQueue), (&(prUsbReq->rSendingDataMsduInfoList)));
	if (g_pfTxDataDoneCb)
		g_pfTxDataDoneCb(prGlueInfo, prFreeQueue);

	u4SentDataSize = urb->actual_length - LEN_USB_UDMA_TX_TERMINATOR;
	nicTxReleaseResource(prGlueInfo->prAdapter, ucTc, nicTxGetPageCount(u4SentDataSize, TRUE), TRUE);
}

VOID halTxUSBProcessDataComplete(IN P_ADAPTER_T prAdapter, P_USB_REQ_T prUsbReq)
{
	UINT_8 ucTc;
	BOOLEAN fgFfa;
	struct urb *urb = prUsbReq->prUrb;
	P_GL_HIF_INFO_T prHifInfo = prUsbReq->prHifInfo;
#if CFG_USB_TX_AGG
	P_BUF_CTRL_T prBufCtrl = prUsbReq->prBufCtrl;
#endif
	unsigned long flags;

	ucTc = *((PUINT_8)&prUsbReq->prPriv) & TC_MASK;
	fgFfa =  *((PUINT_8)&prUsbReq->prPriv) & FFA_MASK;

	if (urb->status != 0) {
		DBGLOG(TX, ERROR, "[%s] send DATA fail (status = %d)\n", __func__, urb->status);
		/* TODO: handle error */
		if ((prAdapter->prGlueInfo->
			rHifInfo.state == USB_STATE_WIFI_OFF)
			|| (prAdapter->rTxCtrl.pucTxCached == NULL))
			return;
	}

	halTxUSBProcessMsduDone(prAdapter->prGlueInfo, prUsbReq);

	spin_lock_irqsave(&prHifInfo->rTxDataQLock, flags);
#if CFG_USB_TX_AGG
	prBufCtrl->u4WrIdx = 0;

	if ((fgFfa == FALSE) || list_empty(&prHifInfo->rTxDataFreeQ[ucTc]))
		list_add_tail(&prUsbReq->list, &prHifInfo->rTxDataFreeQ[ucTc]);
	else
		list_add_tail(&prUsbReq->list, &prHifInfo->rTxDataFfaQ);

	if (usb_anchor_empty(&prHifInfo->rTxDataAnchor[ucTc])) {
		prUsbReq = list_entry(prHifInfo->rTxDataFreeQ[ucTc].next, struct _USB_REQ_T, list);
		prBufCtrl = prUsbReq->prBufCtrl;

		if (prBufCtrl->u4WrIdx != 0)
			halTxUSBSendAggData(prHifInfo, ucTc, prUsbReq);	/* TODO */
	}
#else
	list_add_tail(&prUsbReq->list, &prHifInfo->rTxDataFreeQ);
#endif
	spin_unlock_irqrestore(&prHifInfo->rTxDataQLock, flags);

	if (!HAL_IS_TX_DIRECT(prAdapter)) {
		if (kalGetTxPendingCmdCount(prAdapter->prGlueInfo) > 0 || wlanGetTxPendingFrameCount(prAdapter) > 0)
			kalSetEvent(prAdapter->prGlueInfo);
		kalSetTxEvent2Hif(prAdapter->prGlueInfo);
	}
}

UINT_32 halRxUSBEnqueueRFB(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucBuf, IN UINT_32 u4Length,
	IN UINT_32 u4MinRfbCnt)
{
	P_GLUE_INFO_T prGlueInfo = prAdapter->prGlueInfo;
	P_RX_CTRL_T prRxCtrl = &prAdapter->rRxCtrl;
	P_SW_RFB_T prSwRfb = (P_SW_RFB_T) NULL;
	P_HW_MAC_RX_DESC_T prRxStatus;
	UINT_32 u4RemainCount;
	UINT_16 u2RxByteCount;
	UINT_8 *pucRxFrame;
	UINT_32 u4EnqCnt = 0;

	KAL_SPIN_LOCK_DECLARATION();

	pucRxFrame = pucBuf;
	u4RemainCount = u4Length;
	while (u4RemainCount > 4) {
		u2RxByteCount = HAL_RX_STATUS_GET_RX_BYTE_CNT((P_HW_MAC_RX_DESC_T) pucRxFrame);
		u2RxByteCount = ALIGN_4(u2RxByteCount) + LEN_USB_RX_PADDING_CSO;

		if (u2RxByteCount <= CFG_RX_MAX_PKT_SIZE) {
			prSwRfb = NULL;
			KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
			if (prRxCtrl->rFreeSwRfbList.u4NumElem > u4MinRfbCnt)
				QUEUE_REMOVE_HEAD(&prRxCtrl->rFreeSwRfbList, prSwRfb, P_SW_RFB_T);
			KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

			if (!prSwRfb)
				return u4Length - u4RemainCount;

			kalMemCopy(prSwRfb->pucRecvBuff, pucRxFrame, u2RxByteCount);

			prRxStatus = prSwRfb->prRxStatus;
			ASSERT(prRxStatus);

			prSwRfb->ucPacketType = (UINT_8) HAL_RX_STATUS_GET_PKT_TYPE(prRxStatus);
			/* DBGLOG(RX, TRACE, ("ucPacketType = %d\n", prSwRfb->ucPacketType)); */
#if DBG
			DBGLOG(RX, TRACE, "Rx status flag = %x wlan index = %d SecMode = %d\n",
			       prRxStatus->u2StatusFlag, prRxStatus->ucWlanIdx, HAL_RX_STATUS_GET_SEC_MODE(prRxStatus));
#endif
			if (HAL_IS_RX_DIRECT(prAdapter)) {
				switch (prSwRfb->ucPacketType) {
				case RX_PKT_TYPE_RX_DATA:
#if CFG_SUPPORT_SNIFFER
					if (prGlueInfo->fgIsEnableMon) {
						nicRxProcessMonitorPacket(prAdapter, prSwRfb);
						break;
					}
#endif
					nicRxProcessDataPacket(prAdapter, prSwRfb);
					break;
				default:
					KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
					QUEUE_INSERT_TAIL(&prRxCtrl->rReceivedRfbList, &prSwRfb->rQueEntry);
					KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
					u4EnqCnt++;
					break;
				}
			} else {
				KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
				QUEUE_INSERT_TAIL(&prRxCtrl->rReceivedRfbList, &prSwRfb->rQueEntry);
				KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
				u4EnqCnt++;
			}
			RX_INC_CNT(prRxCtrl, RX_MPDU_TOTAL_COUNT);
		} else {
			DBGLOG(RX, WARN, "Rx byte count:%u exceeds SW_RFB max length:%u\n!",
				u2RxByteCount, CFG_RX_MAX_PKT_SIZE);
			DBGLOG_MEM32(RX, WARN, pucRxFrame, sizeof(HW_MAC_RX_DESC_T));
			break;
		}

		u4RemainCount -= u2RxByteCount;
		pucRxFrame += u2RxByteCount;
	}

	if (u4EnqCnt) {
		set_bit(GLUE_FLAG_RX_BIT, &(prGlueInfo->ulFlag));
		wake_up_interruptible(&(prGlueInfo->waitq));
	}

	return u4Length;
}

WLAN_STATUS halRxUSBReceiveEvent(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgFillUrb)
{
	P_GLUE_INFO_T prGlueInfo = prAdapter->prGlueInfo;
	P_GL_HIF_INFO_T prHifInfo = &prGlueInfo->rHifInfo;
	P_USB_REQ_T prUsbReq;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;

	while (1) {
		prUsbReq = glUsbDequeueReq(prHifInfo, &prHifInfo->rRxEventFreeQ, &prHifInfo->rRxEventQLock);
		if (prUsbReq == NULL)
			return WLAN_STATUS_RESOURCES;

		usb_anchor_urb(prUsbReq->prUrb, &prHifInfo->rRxEventAnchor);

		prUsbReq->prBufCtrl->u4ReadSize = 0;
		if (prHifInfo->eEventEpType == EVENT_EP_TYPE_INTR && fgFillUrb) {
			usb_fill_int_urb(prUsbReq->prUrb,
					  prHifInfo->udev,
					  usb_rcvintpipe(prHifInfo->udev, USB_EVENT_EP_IN),
					  (void *)prUsbReq->prBufCtrl->pucBuf, prUsbReq->prBufCtrl->u4BufSize,
					  halRxUSBReceiveEventComplete, (void *)prUsbReq, 1);
		} else if (prHifInfo->eEventEpType == EVENT_EP_TYPE_BULK) {
			usb_fill_bulk_urb(prUsbReq->prUrb,
					  prHifInfo->udev,
					  usb_rcvbulkpipe(prHifInfo->udev, USB_EVENT_EP_IN),
					  (void *)prUsbReq->prBufCtrl->pucBuf, prUsbReq->prBufCtrl->u4BufSize,
					  halRxUSBReceiveEventComplete, (void *)prUsbReq);
		}
		u4Status = glUsbSubmitUrb(prHifInfo, prUsbReq->prUrb, SUBMIT_TYPE_RX_EVENT);
		if (u4Status) {
			DBGLOG(HAL, ERROR, "glUsbSubmitUrb() reports error (%d)\n", u4Status);
			usb_unanchor_urb(prUsbReq->prUrb);
			glUsbEnqueueReq(prHifInfo, &prHifInfo->rRxEventFreeQ, prUsbReq,
					&prHifInfo->rRxEventQLock, FALSE);
			break;
		}
	}

	return u4Status;
}

VOID halRxUSBReceiveEventComplete(struct urb *urb)
{
	P_USB_REQ_T prUsbReq = urb->context;
	P_GL_HIF_INFO_T prHifInfo = prUsbReq->prHifInfo;
	P_GLUE_INFO_T prGlueInfo = prHifInfo->prGlueInfo;

	if (prHifInfo->state != USB_STATE_LINK_UP) {
		glUsbEnqueueReq(prHifInfo, &prHifInfo->rRxEventFreeQ, prUsbReq, &prHifInfo->rRxEventQLock, FALSE);
		return;
	}

	/* Hif power off wifi, drop rx packets and continue polling RX packets until RX path empty */
	if (prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
		glUsbEnqueueReq(prHifInfo, &prHifInfo->rRxEventFreeQ, prUsbReq, &prHifInfo->rRxEventQLock, FALSE);
		halRxUSBReceiveEvent(prGlueInfo->prAdapter, FALSE);
		return;
	}

	if (urb->status == -ESHUTDOWN || urb->status == -ENOENT) {
		glUsbEnqueueReq(prHifInfo, &prHifInfo->rRxEventFreeQ, prUsbReq, &prHifInfo->rRxEventQLock, FALSE);
		DBGLOG(RX, ERROR, "USB device shutdown skip Rx [%s]\n", __func__);
		return;
	}

#if CFG_USB_RX_HANDLE_IN_HIF_THREAD
	DBGLOG(RX, TRACE, "[%s] Rx URB[0x%p] Len[%u] Sts[%u]\n", __func__, urb, urb->actual_length, urb->status);

	glUsbEnqueueReq(prHifInfo, &prHifInfo->rRxEventCompleteQ, prUsbReq, &prHifInfo->rRxEventQLock, FALSE);

	kalSetIntEvent(prGlueInfo);
#else
	if (urb->status == 0) {
		glUsbEnqueueReq(prHifInfo, &prHifInfo->rRxEventCompleteQ, prUsbReq, &prHifInfo->rRxEventQLock, FALSE);

		/*tasklet_hi_schedule(&prGlueInfo->rRxTask);*/
		tasklet_schedule(&prGlueInfo->rRxTask);
	} else {
		DBGLOG(RX, ERROR, "[%s] receive EVENT fail (status = %d)\n", __func__, urb->status);
		glUsbEnqueueReq(prHifInfo, &prHifInfo->rRxEventFreeQ, prUsbReq, &prHifInfo->rRxEventQLock, FALSE);

		halRxUSBReceiveEvent(prGlueInfo->prAdapter, FALSE);
	}
#endif
}

WLAN_STATUS halRxUSBReceiveData(IN P_ADAPTER_T prAdapter)
{
	P_GLUE_INFO_T prGlueInfo = prAdapter->prGlueInfo;
	P_GL_HIF_INFO_T prHifInfo = &prGlueInfo->rHifInfo;
	P_USB_REQ_T prUsbReq;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;

	while (1) {
		prUsbReq = glUsbDequeueReq(prHifInfo, &prHifInfo->rRxDataFreeQ, &prHifInfo->rRxDataQLock);
		if (prUsbReq == NULL)
			return WLAN_STATUS_RESOURCES;

		usb_anchor_urb(prUsbReq->prUrb, &prHifInfo->rRxDataAnchor);

		prUsbReq->prBufCtrl->u4ReadSize = 0;
		usb_fill_bulk_urb(prUsbReq->prUrb,
				  prHifInfo->udev,
				  usb_rcvbulkpipe(prHifInfo->udev, USB_DATA_EP_IN),
				  (void *)prUsbReq->prBufCtrl->pucBuf,
				  prUsbReq->prBufCtrl->u4BufSize, halRxUSBReceiveDataComplete, (void *)prUsbReq);
		u4Status = glUsbSubmitUrb(prHifInfo, prUsbReq->prUrb, SUBMIT_TYPE_RX_DATA);
		if (u4Status) {
			DBGLOG(HAL, ERROR, "glUsbSubmitUrb() reports error (%d)\n", u4Status);
			usb_unanchor_urb(prUsbReq->prUrb);
			glUsbEnqueueReq(prHifInfo, &prHifInfo->rRxDataFreeQ, prUsbReq, &prHifInfo->rRxDataQLock, FALSE);
			break;
		}
	}

	return u4Status;
}

VOID halRxUSBReceiveDataComplete(struct urb *urb)
{
	P_USB_REQ_T prUsbReq = urb->context;
	P_GL_HIF_INFO_T prHifInfo = prUsbReq->prHifInfo;
	P_GLUE_INFO_T prGlueInfo = prHifInfo->prGlueInfo;
	static UINT_32 uUsbCmdFailCounter;

	if (prHifInfo->state != USB_STATE_LINK_UP) {
		glUsbEnqueueReq(prHifInfo, &prHifInfo->rRxDataFreeQ, prUsbReq, &prHifInfo->rRxDataQLock, FALSE);
		return;
	}

	/* Hif power off wifi, drop rx packets and continue polling RX packets until RX path empty */
	if (prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
		glUsbEnqueueReq(prHifInfo, &prHifInfo->rRxDataFreeQ, prUsbReq, &prHifInfo->rRxDataQLock, FALSE);
		halRxUSBReceiveData(prGlueInfo->prAdapter);
		return;
	}

	if (urb->status == -ESHUTDOWN || urb->status == -ENOENT) {
		glUsbEnqueueReq(prHifInfo, &prHifInfo->rRxDataFreeQ, prUsbReq, &prHifInfo->rRxDataQLock, FALSE);
		DBGLOG(RX, ERROR, "USB device shutdown skip Rx [%s]\n", __func__);
		return;
	}

#if CFG_USB_RX_HANDLE_IN_HIF_THREAD
	glUsbEnqueueReq(prHifInfo, &prHifInfo->rRxDataCompleteQ, prUsbReq, FALSE);

	kalSetIntEvent(prGlueInfo);
#else
	if (urb->status == 0) {
		glUsbEnqueueReq(prHifInfo, &prHifInfo->rRxDataCompleteQ, prUsbReq, &prHifInfo->rRxDataQLock, FALSE);

		/*tasklet_hi_schedule(&prGlueInfo->rRxTask);*/
		tasklet_schedule(&prGlueInfo->rRxTask);
		uUsbCmdFailCounter = 0;
	} else {
		LIMITED_DBGLOG(RX, ERROR,
				"[%s] receive DATA fail (status = %d)\n",
				__func__, urb->status);

		if ((urb->status == -EOVERFLOW) || (urb->status == -EPROTO))
			uUsbCmdFailCounter++;

		if (uUsbCmdFailCounter > 3) {
			prGlueInfo->prAdapter->fgIsChipNoAck = TRUE;
			if (uUsbCmdFailCounter > 10)
				return;
		}

		glUsbEnqueueReq(prHifInfo, &prHifInfo->rRxDataFreeQ,
			prUsbReq, &prHifInfo->rRxDataQLock, FALSE);
		halRxUSBReceiveData(prGlueInfo->prAdapter);
	}
#endif
}

VOID halRxUSBProcessEventDataComplete(IN P_ADAPTER_T prAdapter,
	struct list_head *prCompleteQ, struct list_head *prFreeQ, UINT_32 u4MinRfbCnt)
{
	P_USB_REQ_T prUsbReq;
	struct urb *prUrb;
	P_BUF_CTRL_T prBufCtrl;
	P_GLUE_INFO_T prGlueInfo = prAdapter->prGlueInfo;
	P_GL_HIF_INFO_T prHifInfo = &prGlueInfo->rHifInfo;
	PUINT_8 pucBufAddr;
	UINT_32 u4BufLen;
	static BOOL s_fgOutOfSwRfb = FALSE;
	static UINT_32 s_u4OutOfSwRfbPrintLimit;
	static UINT_32 s_u4OutOfSwRfbBeginTime;

	/* Process complete event/data */
	prUsbReq = glUsbDequeueReq(prHifInfo, prCompleteQ, &prHifInfo->rRxEventQLock);
	while (prUsbReq) {
		prUrb = prUsbReq->prUrb;
		prBufCtrl = prUsbReq->prBufCtrl;

		DBGLOG(RX, LOUD, "[%s] Rx URB[0x%p] Len[%u] Sts[%u]\n", __func__,
			prUrb, prUrb->actual_length, prUrb->status);

		if (prUrb->status != 0) {
			DBGLOG(RX, ERROR, "[%s] receive EVENT/DATA fail (status = %d)\n", __func__, prUrb->status);

			glUsbEnqueueReq(prHifInfo, prFreeQ, prUsbReq, &prHifInfo->rRxEventQLock, FALSE);
			prUsbReq = glUsbDequeueReq(prHifInfo, prCompleteQ, &prHifInfo->rRxEventQLock);
			continue;
		}

		pucBufAddr = prBufCtrl->pucBuf + prBufCtrl->u4ReadSize;
		u4BufLen = prUrb->actual_length - prBufCtrl->u4ReadSize;

		prBufCtrl->u4ReadSize += halRxUSBEnqueueRFB(prAdapter, pucBufAddr, u4BufLen, u4MinRfbCnt);

		if (unlikely(prUrb->actual_length - prBufCtrl->u4ReadSize > 4)) {
			if (s_fgOutOfSwRfb == FALSE) {
				if ((long)jiffies - (long)s_u4OutOfSwRfbPrintLimit > 0) {
					DBGLOG(RX, WARN, "Out of SwRfb!\n");
					s_u4OutOfSwRfbPrintLimit = jiffies + MSEC_TO_JIFFIES(SW_RFB_LOG_LIMIT_MS);
				}
				s_u4OutOfSwRfbBeginTime = jiffies;
				s_fgOutOfSwRfb = TRUE;
			}

			/* If the out-of-SW-RFB situation continues more than SW_RFB_BLOCKING_LIMIT_MS millie seconds,
			 * we discard the remaining Rx packets so as to break the possible dead lock
			 */
			if (jiffies_to_msecs((long)jiffies - (long)s_u4OutOfSwRfbBeginTime) >
			    SW_RFB_BLOCKING_LIMIT_MS) {
				DBGLOG(RX, WARN, "Discard Rx packets (%u bytes)!\n",
				       prUrb->actual_length - prBufCtrl->u4ReadSize);
				glUsbEnqueueReq(prHifInfo, prFreeQ, prUsbReq, &prHifInfo->rRxEventQLock, FALSE);
				s_fgOutOfSwRfb = FALSE;
				break;
			}

			glUsbEnqueueReq(prHifInfo, prCompleteQ, prUsbReq, &prHifInfo->rRxEventQLock, TRUE);

			set_bit(GLUE_FLAG_RX_BIT, &prGlueInfo->ulFlag);
			wake_up_interruptible(&prGlueInfo->waitq);

			schedule_delayed_work(&prGlueInfo->rRxPktDeAggWork, MSEC_TO_JIFFIES(SW_RFB_RECHECK_MS));
			break;
		}

		if (unlikely(s_fgOutOfSwRfb == TRUE))
			s_fgOutOfSwRfb = FALSE;

		glUsbEnqueueReq(prHifInfo, prFreeQ, prUsbReq, &prHifInfo->rRxEventQLock, FALSE);
		prUsbReq = glUsbDequeueReq(prHifInfo, prCompleteQ, &prHifInfo->rRxEventQLock);
	}
}

/*----------------------------------------------------------------------------*/
/*!
* @brief enable global interrupt
*
* @param prAdapter pointer to the Adapter handler
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID halEnableInterrupt(IN P_ADAPTER_T prAdapter)
{
	P_GLUE_INFO_T prGlueInfo;
	P_GL_HIF_INFO_T prHifInfo;

	ASSERT(prAdapter);

	prGlueInfo = prAdapter->prGlueInfo;
	prHifInfo = &prGlueInfo->rHifInfo;

	halRxUSBReceiveData(prAdapter);
	if (prHifInfo->eEventEpType != EVENT_EP_TYPE_DATA_EP)
		halRxUSBReceiveEvent(prAdapter, TRUE);

	glUdmaRxAggEnable(prGlueInfo, TRUE);
} /* end of halEnableInterrupt() */

/*----------------------------------------------------------------------------*/
/*!
* @brief disable global interrupt
*
* @param prAdapter pointer to the Adapter handler
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID halDisableInterrupt(IN P_ADAPTER_T prAdapter)
{
	P_GLUE_INFO_T prGlueInfo;
	P_GL_HIF_INFO_T prHifInfo;

	ASSERT(prAdapter);
	prGlueInfo = prAdapter->prGlueInfo;
	prHifInfo = &prGlueInfo->rHifInfo;

	usb_kill_anchored_urbs(&prHifInfo->rRxDataAnchor);
	usb_kill_anchored_urbs(&prHifInfo->rRxEventAnchor);

	glUdmaRxAggEnable(prGlueInfo, FALSE);
	prAdapter->fgIsIntEnable = FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to process the POWER OFF procedure.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
BOOLEAN halSetDriverOwn(IN P_ADAPTER_T prAdapter)
{
	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to process the POWER ON procedure.
*
* \param[in] pvAdapter Pointer to the Adapter structure.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID halSetFWOwn(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgEnableGlobalInt)
{
}

VOID halWakeUpWiFi(IN P_ADAPTER_T prAdapter)
{
	BOOLEAN fgResult;
	UINT_8 ucCount = 0;

#if CFG_SUPPORT_PMIC_SPI_CLOCK_SWITCH
	UINT_32 u4Value = 0;
	/*E1 PMIC clock workaround*/
	HAL_MCR_RD(prAdapter, TOP_CKGEN2_CR_PMIC_CK_MANUAL, &u4Value);

	if ((TOP_CKGEN2_CR_PMIC_CK_MANUAL_MASK & u4Value) == 0)
		HAL_MCR_WR(prAdapter, TOP_CKGEN2_CR_PMIC_CK_MANUAL, (TOP_CKGEN2_CR_PMIC_CK_MANUAL_MASK|u4Value));
	HAL_MCR_RD(prAdapter, TOP_CKGEN2_CR_PMIC_CK_MANUAL, &u4Value);
	DBGLOG(INIT, INFO, "PMIC SPI clock switch = %s\n",
		(TOP_CKGEN2_CR_PMIC_CK_MANUAL_MASK&u4Value) ? "SUCCESS" : "FAIL");
#endif

	DBGLOG(INIT, INFO, "Power on Wi-Fi....\n");

	HAL_WIFI_FUNC_READY_CHECK(prAdapter, WIFI_FUNC_INIT_DONE, &fgResult);

	while (!fgResult) {
		HAL_WIFI_FUNC_POWER_ON(prAdapter);
		kalMdelay(50);
		HAL_WIFI_FUNC_READY_CHECK(prAdapter, WIFI_FUNC_INIT_DONE, &fgResult);

		ucCount++;

		if (ucCount >= 5) {
			DBGLOG(INIT, WARN, "Power on failed!!!\n");
			break;
		}
	}

	if (prAdapter->prGlueInfo->rHifInfo.state == USB_STATE_WIFI_OFF)
		glUsbSetState(&prAdapter->prGlueInfo->rHifInfo,
			  USB_STATE_LINK_UP);

	prAdapter->fgIsFwOwn = FALSE;
}

VOID halEnableFWDownload(IN P_ADAPTER_T prAdapter, IN BOOL fgEnable)
{
#if (CFG_UMAC_GENERATION >= 0x20)

	UINT_32 u4Value = 0;

	ASSERT(prAdapter);

	{
		HAL_MCR_RD(prAdapter, UDMA_TX_QSEL, &u4Value);

		if (fgEnable)
			u4Value |= FW_DL_EN;
		else
			u4Value &= ~FW_DL_EN;

		HAL_MCR_WR(prAdapter, UDMA_TX_QSEL, u4Value);
	}
#endif
}

VOID halDevInit(IN P_ADAPTER_T prAdapter)
{
	P_GLUE_INFO_T prGlueInfo;

	ASSERT(prAdapter);
	prGlueInfo = prAdapter->prGlueInfo;

	glUdmaRxAggEnable(prGlueInfo, FALSE);
	glUdmaTxRxEnable(prGlueInfo, TRUE);
}

BOOLEAN halTxIsDataBufEnough(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo)
{
	P_GLUE_INFO_T prGlueInfo = prAdapter->prGlueInfo;
	P_GL_HIF_INFO_T prHifInfo = &prGlueInfo->rHifInfo;
#if CFG_USB_TX_AGG
	P_USB_REQ_T prUsbReq;
	P_BUF_CTRL_T prBufCtrl;
#endif
	UINT_8 ucTc;
	struct sk_buff *skb;
	UINT_32 u4Length;

	unsigned long flags;

	skb = (struct sk_buff *)prMsduInfo->prPacket;
	u4Length = skb->len;
	ucTc = USB_TRANS_MSDU_TC(prMsduInfo);

	spin_lock_irqsave(&prHifInfo->rTxDataQLock, flags);

#if CFG_USB_TX_AGG
	if (list_empty(&prHifInfo->rTxDataFreeQ[ucTc])) {
		if (glUsbBorrowFfaReq(prHifInfo, ucTc) == FALSE) {
			spin_unlock_irqrestore(&prHifInfo->rTxDataQLock, flags);
			return FALSE;
		}
	}

	prUsbReq = list_entry(prHifInfo->rTxDataFreeQ[ucTc].next, struct _USB_REQ_T, list);
	prBufCtrl = prUsbReq->prBufCtrl;

	if (prHifInfo->rTxDataFreeQ[ucTc].next->next == &prHifInfo->rTxDataFreeQ[ucTc]) {
		/* length of rTxDataFreeQ equals 1 */
		if (prBufCtrl->u4WrIdx + ALIGN_4(u4Length) >
		    prBufCtrl->u4BufSize - prHifInfo->u4AggRsvSize[ucTc] - LEN_USB_UDMA_TX_TERMINATOR) {
			/* Buffer is not enough */
			if (glUsbBorrowFfaReq(prHifInfo, ucTc) == FALSE) {
				spin_unlock_irqrestore(&prHifInfo->rTxDataQLock, flags);
				return FALSE;
			}
		}
	}
	prHifInfo->u4AggRsvSize[ucTc] += ALIGN_4(u4Length);
#else
	if (list_empty(&prHifInfo->rTxDataFreeQ)) {
		spin_unlock_irqrestore(&prHifInfo->rTxDataQLock, flags);

		return FALSE;
	}
#endif

	spin_unlock_irqrestore(&prHifInfo->rTxDataQLock, flags);
	return TRUE;
}

VOID halProcessTxInterrupt(IN P_ADAPTER_T prAdapter)
{
#if CFG_USB_TX_HANDLE_IN_HIF_THREAD
	P_USB_REQ_T prUsbReq;
	P_GL_HIF_INFO_T prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	/* Process complete Tx cmd */
	prUsbReq = glUsbDequeueReq(prHifInfo, &prHifInfo->rTxCmdCompleteQ, &prHifInfo->rTxCmdQLock);
	while (prUsbReq) {
		halTxUSBProcessCmdComplete(prAdapter, prUsbReq);
		prUsbReq = glUsbDequeueReq(prHifInfo, &prHifInfo->rTxCmdCompleteQ, &prHifInfo->rTxCmdQLock);
	}

	/* Process complete Tx data */
	prUsbReq = glUsbDequeueReq(prHifInfo, &prHifInfo->rTxDataCompleteQ, &prHifInfo->rTxDataQLock);
	while (prUsbReq) {
		halTxUSBProcessDataComplete(prAdapter, prUsbReq);
		prUsbReq = glUsbDequeueReq(prHifInfo, &prHifInfo->rTxDataCompleteQ, &prHifInfo->rTxDataQLock);
	}
#endif
}

VOID halHifSwInfoInit(IN P_ADAPTER_T prAdapter)
{

}

VOID halRxProcessMsduReport(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb)
{

}

UINT_32 halTxGetPageCount(IN UINT_32 u4FrameLength, IN BOOLEAN fgIncludeDesc)
{
#if CFG_USB_TX_AGG
	UINT_32 u4RequiredBufferSize;
	UINT_32 u4PageCount;

	/* Frame Buffer
	 *  |<--Tx Descriptor-->|<--Tx descriptor padding-->|
	 *  <--802.3/802.11 Header-->|<--Header padding-->|<--Payload-->|
	 */

	if (fgIncludeDesc)
		u4RequiredBufferSize = u4FrameLength;
	else
		u4RequiredBufferSize = NIC_TX_HEAD_ROOM + u4FrameLength;

	u4RequiredBufferSize = ALIGN_4(u4RequiredBufferSize);

	if (NIC_TX_PAGE_SIZE_IS_POWER_OF_2)
		u4PageCount = (u4RequiredBufferSize + (NIC_TX_PAGE_SIZE - 1)) >> NIC_TX_PAGE_SIZE_IN_POWER_OF_2;
	else
		u4PageCount = (u4RequiredBufferSize + (NIC_TX_PAGE_SIZE - 1)) / NIC_TX_PAGE_SIZE;

	return u4PageCount;
#else
	return 1;
#endif
}

WLAN_STATUS halTxPollingResource(IN P_ADAPTER_T prAdapter, IN UINT_8 ucTC)
{
	return WLAN_STATUS_SUCCESS;
}

VOID halSerHifReset(IN P_ADAPTER_T prAdapter)
{
}

VOID halProcessRxInterrupt(IN P_ADAPTER_T prAdapter)
{
	P_GL_HIF_INFO_T prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	if (g_u4HaltFlag) {
		DBGLOG(RX, WARN, "wlan is halt\n");
		return;
	}

	/* Process complete data */
	halRxUSBProcessEventDataComplete(prAdapter, &prHifInfo->rRxDataCompleteQ,
		&prHifInfo->rRxDataFreeQ, USB_RX_DATA_RFB_RSV_CNT);
	halRxUSBReceiveData(prAdapter);

	if (prHifInfo->eEventEpType != EVENT_EP_TYPE_DATA_EP) {
		/* Process complete event */
		halRxUSBProcessEventDataComplete(prAdapter, &prHifInfo->rRxEventCompleteQ,
			&prHifInfo->rRxEventFreeQ, USB_RX_EVENT_RFB_RSV_CNT);
		halRxUSBReceiveEvent(prAdapter, FALSE);
	}
}

UINT_32 halDumpHifStatus(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucBuf, IN UINT_32 u4Max)
{
	UINT_32 u4CpuIdx, u4DmaIdx, u4Int, u4GloCfg, u4Reg;
	UINT_32 u4Len = 0;
	P_GLUE_INFO_T prGlueInfo = prAdapter->prGlueInfo;
	UCHAR pBuffer[512] = {0};

	HAL_MCR_RD(prAdapter, 0x820b0118, &u4CpuIdx);
	HAL_MCR_RD(prAdapter, 0x820b011c, &u4DmaIdx);
	HAL_MCR_RD(prAdapter, 0x820b0220, &u4Int);
	HAL_MCR_RD(prAdapter, 0x820b0204, &u4GloCfg);

	LOGBUF(pucBuf, u4Max, u4Len, "\n");
	LOGBUF(pucBuf, u4Max, u4Len, "PDMA1R1 CPU[%u] DMA[%u] INT[0x%08x] CFG[0x%08x]\n", u4CpuIdx,
		u4DmaIdx, u4Int, u4GloCfg);

	HAL_MCR_RD(prAdapter, UDMA_WLCFG_0, &u4Reg);

	LOGBUF(pucBuf, u4Max, u4Len, "UDMA WLCFG[0x%08x]\n", u4Reg);

	LOGBUF(pucBuf, u4Max, u4Len, "\n");
	LOGBUF(pucBuf, u4Max, u4Len, "VenderID: %04x\n",
		glGetUsbDeviceVendorId(prGlueInfo->rHifInfo.udev));
	LOGBUF(pucBuf, u4Max, u4Len, "ProductID: %04x\n",
		glGetUsbDeviceProductId(prGlueInfo->rHifInfo.udev));

	glGetUsbDeviceManufacturerName(prGlueInfo->rHifInfo.udev, pBuffer,
		sizeof(pBuffer));
	LOGBUF(pucBuf, u4Max, u4Len, "Manufacturer: %s\n",
		pBuffer);

	glGetUsbDeviceProductName(prGlueInfo->rHifInfo.udev, pBuffer,
		sizeof(pBuffer));
	LOGBUF(pucBuf, u4Max, u4Len, "Product: %s\n", pBuffer);

	glGetUsbDeviceSerialNumber(prGlueInfo->rHifInfo.udev, pBuffer,
		sizeof(pBuffer));
	LOGBUF(pucBuf, u4Max, u4Len, "SerialNumber: %s\n",
		pBuffer);

	return u4Len;
}

VOID halGetCompleteStatus(IN P_ADAPTER_T prAdapter, OUT PUINT_32 pu4IntStatus)
{
#if CFG_USB_RX_HANDLE_IN_HIF_THREAD || CFG_USB_TX_HANDLE_IN_HIF_THREAD
	P_GL_HIF_INFO_T prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
#endif

	*pu4IntStatus = 0;

#if CFG_USB_RX_HANDLE_IN_HIF_THREAD
	if (!list_empty(&prHifInfo->rRxDataCompleteQ) || !list_empty(&prHifInfo->rRxEventCompleteQ))
		*pu4IntStatus |= WHISR_RX0_DONE_INT;
#endif

#if CFG_USB_TX_HANDLE_IN_HIF_THREAD
	if (!list_empty(&prHifInfo->rTxDataCompleteQ) || !list_empty(&prHifInfo->rTxCmdCompleteQ))
		*pu4IntStatus |= WHISR_TX_DONE_INT;
#endif
}

BOOLEAN halIsPendingRx(IN P_ADAPTER_T prAdapter)
{
#if CFG_USB_RX_HANDLE_IN_HIF_THREAD
	P_GL_HIF_INFO_T prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	if (!list_empty(&prHifInfo->rRxDataCompleteQ) || !list_empty(&prHifInfo->rRxEventCompleteQ))
		return TRUE;
	else
		return FALSE;
#else
	return FALSE;
#endif
}

VOID halUSBPreSuspendCmd(IN P_ADAPTER_T prAdapter)
{
	CMD_HIF_CTRL_T rCmdHifCtrl;
	WLAN_STATUS rStatus;

	rCmdHifCtrl.ucHifType = ENUM_HIF_TYPE_USB;
	rCmdHifCtrl.ucHifDirection = ENUM_HIF_TX;
	rCmdHifCtrl.ucHifStop = 1;

	rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
				      CMD_ID_HIF_CTRL,	/* ucCID */
				      TRUE,	/* fgSetQuery */
				      FALSE,	/* fgNeedResp */
				      FALSE,	/* fgIsOid */
				      NULL,	/* pfCmdDoneHandler */
				      NULL,	/* pfCmdTimeoutHandler */
				      sizeof(CMD_HIF_CTRL_T),	/* u4SetQueryInfoLen */
				      (PUINT_8)&rCmdHifCtrl,	/* pucInfoBuffer */
				      NULL,	/* pvSetQueryBuffer */
				      0	/* u4SetQueryBufferLen */
				     );

	ASSERT(rStatus == WLAN_STATUS_PENDING);
}

VOID halUSBPreSuspendDone(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	unsigned long flags;
	P_GL_HIF_INFO_T prHifInfo;

	ASSERT(prAdapter);
	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	spin_lock_irqsave(&prHifInfo->rStateLock, flags);

	if (prHifInfo->state == USB_STATE_LINK_UP)
		prHifInfo->state = USB_STATE_PRE_SUSPEND_DONE;
	else
		DBGLOG(HAL, ERROR, "Previous USB state (%d)!\n", prHifInfo->state);

	spin_unlock_irqrestore(&prHifInfo->rStateLock, flags);
}

VOID halUSBPreSuspendTimeout(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo)
{
	unsigned long flags;
	P_GL_HIF_INFO_T prHifInfo;

	ASSERT(prAdapter);
	prHifInfo = &prAdapter->prGlueInfo->rHifInfo;

	spin_lock_irqsave(&prHifInfo->rStateLock, flags);

	if (prHifInfo->state == USB_STATE_LINK_UP)
		prHifInfo->state = USB_STATE_PRE_SUSPEND_FAIL;
	else
		DBGLOG(HAL, ERROR, "Previous USB state (%d)!\n", prHifInfo->state);

	spin_unlock_irqrestore(&prHifInfo->rStateLock, flags);
}

UINT_32 halGetValidCoalescingBufSize(IN P_ADAPTER_T prAdapter)
{
	UINT_32 u4BufSize;

	if (HIF_TX_COALESCING_BUFFER_SIZE > HIF_RX_COALESCING_BUFFER_SIZE)
		u4BufSize = HIF_TX_COALESCING_BUFFER_SIZE;
	else
		u4BufSize = HIF_RX_COALESCING_BUFFER_SIZE;

	return u4BufSize;
}

WLAN_STATUS halAllocateIOBuffer(IN P_ADAPTER_T prAdapter)
{
	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS halReleaseIOBuffer(IN P_ADAPTER_T prAdapter)
{
	return WLAN_STATUS_SUCCESS;
}

VOID halProcessSoftwareInterrupt(IN P_ADAPTER_T prAdapter)
{

}

VOID halDeAggRxPktWorker(struct work_struct *work)
{
	P_GLUE_INFO_T prGlueInfo = ENTRY_OF(work, GLUE_INFO_T, rRxPktDeAggWork);

	/*tasklet_hi_schedule(&prGlueInfo->rRxTask);*/
	tasklet_schedule(&prGlueInfo->rRxTask);
}

VOID halRxTasklet(unsigned long data)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T)data;

	halProcessRxInterrupt(prGlueInfo->prAdapter);
}

VOID halTxCompleteTasklet(unsigned long data)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T)data;
	P_GL_HIF_INFO_T prHifInfo = &prGlueInfo->rHifInfo;
	P_USB_REQ_T prUsbReq;

	/* Process complete Tx data */
	prUsbReq = glUsbDequeueReq(prHifInfo, &prHifInfo->rTxDataCompleteQ, &prHifInfo->rTxDataQLock);
	while (prUsbReq) {
		halTxUSBProcessDataComplete(prGlueInfo->prAdapter, prUsbReq);
		prUsbReq = glUsbDequeueReq(prHifInfo, &prHifInfo->rTxDataCompleteQ, &prHifInfo->rTxDataQLock);
	}
}

/* Hif power off wifi */
WLAN_STATUS halHifPowerOffWifi(IN P_ADAPTER_T prAdapter)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;

	DBGLOG(INIT, INFO, "Power off Wi-Fi!\n");

	/* Power off Wi-Fi */
	rStatus = wlanSendNicPowerCtrlCmd(prAdapter, TRUE);

	rStatus = wlanCheckWifiFunc(prAdapter, FALSE);

	glUsbSetState(&prAdapter->prGlueInfo->rHifInfo, USB_STATE_WIFI_OFF);

	nicDisableInterrupt(prAdapter);

	wlanClearPendingInterrupt(prAdapter);

	halTxCancelAllSending(prAdapter);

	return rStatus;
}

VOID halPrintHifDbgInfo(IN P_ADAPTER_T prAdapter)
{

}

BOOLEAN halIsTxResourceControlEn(IN P_ADAPTER_T prAdapter)
{
	return FALSE;
}

VOID halTxResourceResetHwTQCounter(IN P_ADAPTER_T prAdapter)
{
}

