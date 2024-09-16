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
*[File]             usb.c
*[Version]          v1.0
*[Revision Date]    2010-03-01
*[Author]
*[Description]
*    The program provides USB HIF driver
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

#include "precomp.h"

#include <linux/usb.h>
#include <linux/mutex.h>

#include <linux/mm.h>
#ifndef CONFIG_X86
#include <asm/memory.h>
#endif

#include "mt66xx_reg.h"
#include "cust_usb_id.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

#define HIF_USB_ERR_TITLE_STR           "["CHIP_NAME"] USB Access Error!"
#define HIF_USB_ERR_DESC_STR            "**USB Access Error**\n"

#define HIF_USB_ACCESS_RETRY_LIMIT      1

#define MT_MAC_BASE                     0x2

#define MTK_USB_PORT_MASK               0x0F
#define MTK_USB_BULK_IN_MIN_EP          4
#define MTK_USB_BULK_IN_MAX_EP          5
#define MTK_USB_BULK_OUT_MIN_EP         4
#define MTK_USB_BULK_OUT_MAX_EP         9

static const struct usb_device_id mtk_usb_ids[] = {
	/* {USB_DEVICE(0x0E8D,0x6632), .driver_info = MT_MAC_BASE}, */
	{	USB_DEVICE_AND_INTERFACE_INFO(0x0E8D, 0x6632, 0xff, 0xff, 0xff),
		.driver_info = (kernel_ulong_t)&mt66xx_driver_data_mt6632},
	{	USB_DEVICE_AND_INTERFACE_INFO(0x0E8D, 0x7666, 0xff, 0xff, 0xff),
		.driver_info = (kernel_ulong_t)&mt66xx_driver_data_mt6632},
	{	USB_DEVICE_AND_INTERFACE_INFO(0x0E8D, 0x7668, 0xff, 0xff, 0xff),
		.driver_info = (kernel_ulong_t)&mt66xx_driver_data_mt7668},
	/* If customer usb id is presented, add to the table. */
	CUST_USB_ID_TABLES
	{ /* end: all zeroes */ },
};

MODULE_DEVICE_TABLE(usb, mtk_usb_ids);

#if CFG_USB_TX_AGG
/* TODO */
const UINT_32 USB_TX_DATA_BUF_SIZE[TC_NUM] = { NIC_TX_PAGE_COUNT_TC0 / (USB_REQ_TX_DATA_CNT - 1),
	NIC_TX_PAGE_COUNT_TC1 / (USB_REQ_TX_DATA_CNT - 1),
	NIC_TX_PAGE_COUNT_TC2 / (USB_REQ_TX_DATA_CNT - 1),
	NIC_TX_PAGE_COUNT_TC3 / (USB_REQ_TX_DATA_CNT - 1),
	NIC_TX_PAGE_COUNT_TC4 / (USB_REQ_TX_DATA_CNT - 1),
	NIC_TX_PAGE_COUNT_TC5 / (USB_REQ_TX_DATA_CNT - 1),
};
#endif

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

static BOOLEAN g_fgDriverProbed = FALSE;

static struct usb_driver mtk_usb_driver = {
	.name = "wlan",		/* "MTK USB WLAN Driver" */
	.id_table = mtk_usb_ids,
	.probe = NULL,
	.disconnect = NULL,
	.suspend = NULL,
	.resume = NULL,
	.reset_resume = NULL,
	.supports_autosuspend = 0,
};

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static int mtk_usb_probe(struct usb_interface *intf, const struct usb_device_id *id);
static void mtk_usb_disconnect(struct usb_interface *intf);
static int mtk_usb_suspend(struct usb_interface *intf, pm_message_t message);
static int mtk_usb_resume(struct usb_interface *intf);
static int mtk_usb_reset_resume(struct usb_interface *intf);
static int mtk_usb_bulk_in_msg(IN P_GL_HIF_INFO_T prHifInfo, IN UINT_32 len, OUT UCHAR * buffer, int InEp);
static int mtk_usb_intr_in_msg(IN P_GL_HIF_INFO_T prHifInfo, IN UINT_32 len, OUT UCHAR * buffer, int InEp);
static int mtk_usb_bulk_out_msg(IN P_GL_HIF_INFO_T prHifInfo, IN UINT_32 len, IN UCHAR * buffer, int OutEp);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a USB probe function
*
* \param[in] intf   USB interface
* \param[in] id     USB device id
*
* \return void
*/
/*----------------------------------------------------------------------------*/
static int mtk_usb_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	int ret = 0;
	struct usb_device *dev;

	DBGLOG(HAL, EVENT, "mtk_usb_probe()\n");

	ASSERT(intf);
	ASSERT(id);

	dev = interface_to_usbdev(intf);
	dev = usb_get_dev(dev);

	/* Prevent un-expected usb operation  */
	if (g_fgDriverProbed) {
		DBGLOG(HAL, ERROR, "wlan_probe(): Device already probed!!\n");
		return -EBUSY;
	}

	DBGLOG(HAL, EVENT, "wlan_probe()\n");
	if (pfWlanProbe((PVOID) intf, (PVOID) id->driver_info) != WLAN_STATUS_SUCCESS) {
		/* printk(KERN_WARNING DRV_NAME"pfWlanProbe fail!call pfWlanRemove()\n"); */
		pfWlanRemove();
		DBGLOG(HAL, ERROR, "wlan_probe() failed\n");
		ret = -1;
	} else {
		g_fgDriverProbed = TRUE;
	}

	return ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a USB remove function
*
* \param[in] intf   USB interface
*
* \return void
*/
/*----------------------------------------------------------------------------*/
static void mtk_usb_disconnect(struct usb_interface *intf)
{
	P_GLUE_INFO_T prGlueInfo;

	DBGLOG(HAL, STATE, "mtk_usb_disconnect()\n");

	ASSERT(intf);
	prGlueInfo  = (P_GLUE_INFO_T)usb_get_intfdata(intf);
	if (prGlueInfo) {
		glUsbSetState(&prGlueInfo->rHifInfo, USB_STATE_LINK_DOWN);
		usb_set_intfdata(intf, NULL);
	} else
		DBGLOG(HAL, ERROR, "prGlueInfo is NULL!!\n");

	glUsbWlanRemove();

	usb_put_dev(interface_to_usbdev(intf));

	DBGLOG(HAL, STATE, "mtk_usb_disconnect() done\n");
}

static int mtk_usb_suspend(struct usb_interface *intf, pm_message_t message)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T)usb_get_intfdata(intf);
	ADAPTER_T *prAdapter;
	P_AIS_FSM_INFO_T prAisFsmInfo = NULL;
	UINT_8 count = 1;
	UINT_8 ret = 0;

	DBGLOG(HAL, STATE, "mtk_usb_suspend()\n");

	/* clear ongoing scan */
	prAdapter = prGlueInfo->prAdapter;
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	while (prAisFsmInfo->eCurrentState != AIS_STATE_NORMAL_TR
		&& prAisFsmInfo->eCurrentState != AIS_STATE_IDLE) {
		DBGLOG(REQ, STATE, "AIS(%d) await count(%d)!\n",
			prAisFsmInfo->eCurrentState, count);

		if (++count > 20) {
			DBGLOG(REQ, WARN, "AIS await idle timeout!!!\n");
			break;
		}

		if (prAisFsmInfo->eCurrentState == AIS_STATE_ONLINE_SCAN
			|| prAisFsmInfo->eCurrentState == AIS_STATE_LOOKING_FOR
			|| prAisFsmInfo->eCurrentState == AIS_STATE_SCAN) {
			DBGLOG(REQ, STATE, "AIS is scanning, do abort\n");
			aisFsmStateAbort_SCAN(prAdapter);
			kalMsleep(200);
			continue;
		}
		kalMsleep(25);
	}
	count = 0;

	wlanSuspendPmHandle(prGlueInfo);

	halUSBPreSuspendCmd(prGlueInfo->prAdapter);

	while (prGlueInfo->rHifInfo.state != USB_STATE_PRE_SUSPEND_DONE) {
		if (count > 25) {
			DBGLOG(HAL, ERROR,
			"pre_suspend timeout, return -ETIME in the end\n");
			ret = -ETIME;
			break;
		}
		msleep(20);
		count++;
	}

	glUsbSetState(&prGlueInfo->rHifInfo, USB_STATE_SUSPEND);
	halDisableInterrupt(prGlueInfo->prAdapter);
	halTxCancelAllSending(prGlueInfo->prAdapter);

	DBGLOG(HAL, STATE, "mtk_usb_suspend() done!\n");

	if (ret && PMSG_IS_AUTO(message))
		mtk_usb_resume(intf);

	return ret;
}

static int mtk_usb_resume(struct usb_interface *intf)
{
	int ret = 0;
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T)usb_get_intfdata(intf);

	DBGLOG(HAL, STATE, "mtk_usb_resume()\n");
	if (!prGlueInfo) {
		DBGLOG(HAL, ERROR, "prGlueInfo is NULL!\n");
		return -EFAULT;
	}

	/* NOTE: USB bus may not really do suspend and resume*/
	ret = usb_control_msg(prGlueInfo->rHifInfo.udev,
			      usb_sndctrlpipe(prGlueInfo->rHifInfo.udev, 0), VND_REQ_FEATURE_SET,
			      DEVICE_VENDOR_REQUEST_OUT, FEATURE_SET_WVALUE_RESUME, 0, NULL, 0,
			      VENDOR_TIMEOUT_MS);
	if (ret)
		DBGLOG(HAL, ERROR, "VendorRequest FeatureSetResume ERROR: %x\n", (unsigned int)ret);

	glUsbSetState(&prGlueInfo->rHifInfo, USB_STATE_PRE_RESUME);
	/* To trigger CR4 path */
	wlanSendDummyCmd(prGlueInfo->prAdapter, FALSE);

	glUsbSetState(&prGlueInfo->rHifInfo, USB_STATE_LINK_UP);
	halEnableInterrupt(prGlueInfo->prAdapter);

	wlanResumePmHandle(prGlueInfo);

	DBGLOG(HAL, STATE, "mtk_usb_resume() done!\n");

	/* TODO */
	return 0;
}

static int mtk_usb_reset_resume(struct usb_interface *intf)
{
	DBGLOG(HAL, STATE, "mtk_usb_reset_resume()\n");

	mtk_usb_resume(intf);

	DBGLOG(HAL, STATE, "mtk_usb_reset_resume done!()\n");

	/* TODO */
	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief USB EP0 vendor request
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
* \param[in] uEndpointAddress
* \param[in] RequestType
* \param[in] Request
* \param[in] Value
* \param[in] Index
* \param[in] TransferBuffer
* \param[in] TransferBufferLength
*
* \retval 0          if success
*         non-zero   if fail, the return value of usb_control_msg()
*/
/*----------------------------------------------------------------------------*/
int mtk_usb_vendor_request(IN P_GLUE_INFO_T prGlueInfo, IN UCHAR uEndpointAddress, IN UCHAR RequestType,
			    IN UCHAR Request, IN UINT_16 Value, IN UINT_16 Index, IN PVOID TransferBuffer,
			    IN UINT_32 TransferBufferLength)
{
	P_GL_HIF_INFO_T prHifInfo = &prGlueInfo->rHifInfo;
	void *xfer_buf;
	/* refer to RTUSB_VendorRequest */
	int ret = 0;

	/* TODO: semaphore */

	if (in_interrupt()) {
		DBGLOG(REQ, ERROR, "BUG: mtk_usb_vendor_request is called from invalid context\n");
		return -EFAULT;
	}

	if (unlikely(TransferBufferLength > prHifInfo->vendor_req_buf_sz)) {
		DBGLOG(REQ, ERROR, "len %u exceeds limit %zu\n", TransferBufferLength,
			prHifInfo->vendor_req_buf_sz);
		return -E2BIG;
	}

	if (unlikely(TransferBuffer && !prHifInfo->vendor_req_buf)) {
		DBGLOG(REQ, ERROR, "NULL vendor_req_buf\n");
		return -EFAULT;
	}

	/* use heap instead of old stack memory */
	xfer_buf = (TransferBuffer) ? prHifInfo->vendor_req_buf : NULL;

	mutex_lock(&prHifInfo->vendor_req_sem);

	if (RequestType == DEVICE_VENDOR_REQUEST_OUT) {
		if (xfer_buf)
			memcpy(xfer_buf, TransferBuffer, TransferBufferLength);
		ret = usb_control_msg(prHifInfo->udev,
				      usb_sndctrlpipe(prHifInfo->udev, uEndpointAddress),
				      Request, RequestType, Value, Index,
				      xfer_buf, TransferBufferLength,
				      VENDOR_TIMEOUT_MS);
	} else if (RequestType == DEVICE_VENDOR_REQUEST_IN) {
		ret = usb_control_msg(prHifInfo->udev,
				      usb_rcvctrlpipe(prHifInfo->udev, uEndpointAddress),
				      Request, RequestType, Value, Index,
				      xfer_buf, TransferBufferLength,
				      VENDOR_TIMEOUT_MS);
		if (xfer_buf && (ret > 0))
			memcpy(TransferBuffer, xfer_buf, TransferBufferLength);
	}
	mutex_unlock(&prHifInfo->vendor_req_sem);

	return (ret == TransferBufferLength) ? 0 : ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief USB Bulk IN msg
*
* \param[in] prHifInfo  Pointer to the GL_HIF_INFO_T structure
* \param[in] len
* \param[in] buffer
* \param[in] InEp
*
* \retval
*/
/*----------------------------------------------------------------------------*/
static int mtk_usb_bulk_in_msg(IN P_GL_HIF_INFO_T prHifInfo, IN UINT_32 len, OUT UCHAR *buffer, int InEp)
{
	int ret = 0;
	UINT_32 count;

	if (in_interrupt()) {
		DBGLOG(REQ, ERROR, "BUG: mtk_usb_bulk_in_msg is called from invalid context\n");
		return FALSE;
	}

	mutex_lock(&prHifInfo->vendor_req_sem);

	/* do a blocking bulk read to get data from the device */
	ret = usb_bulk_msg(prHifInfo->udev,
			   usb_rcvbulkpipe(prHifInfo->udev, InEp), buffer, len, &count, BULK_TIMEOUT_MS);

	mutex_unlock(&prHifInfo->vendor_req_sem);

	if (ret >= 0) {
#if 0 /* maximize buff len for usb in */
		if (count != len) {
			DBGLOG(HAL, WARN, "usb_bulk_msg(IN=%d) Warning. Data is not completed. (receive %d/%u)\n",
			       InEp, count, len);
		}
#endif
		return count;
	}

	DBGLOG(HAL, ERROR, "usb_bulk_msg(IN=%d) Fail. Error code = %d.\n", InEp, ret);
	return ret;
}

static int mtk_usb_intr_in_msg(IN P_GL_HIF_INFO_T prHifInfo, IN UINT_32 len, OUT UCHAR *buffer, int InEp)
{
	int ret = 0;
	UINT_32 count;

	if (in_interrupt()) {
		DBGLOG(REQ, ERROR, "BUG: mtk_usb_intr_in_msg is called from invalid context\n");
		return FALSE;
	}

	mutex_lock(&prHifInfo->vendor_req_sem);

	/* do a blocking interrupt read to get data from the device */
	ret = usb_interrupt_msg(prHifInfo->udev,
			   usb_rcvintpipe(prHifInfo->udev, InEp), buffer, len, &count, INTERRUPT_TIMEOUT_MS);

	mutex_unlock(&prHifInfo->vendor_req_sem);

	if (ret >= 0) {
#if 0 /* maximize buff len for usb in */
		if (count != len) {
			DBGLOG(HAL, WARN, "usb_interrupt_msg(IN=%d) Warning. Data is not completed. (receive %d/%u)\n",
			       InEp, count, len);
		}
#endif
		return count;
	}

	DBGLOG(HAL, ERROR, "usb_interrupt_msg(IN=%d) Fail. Error code = %d.\n", InEp, ret);
	return ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief USB Bulk OUT msg
*
* \param[in] prHifInfo  Pointer to the GL_HIF_INFO_T structure
* \param[in] len
* \param[in] buffer
* \param[in] OutEp
*
* \retval
*/
/*----------------------------------------------------------------------------*/
static int mtk_usb_bulk_out_msg(IN P_GL_HIF_INFO_T prHifInfo, IN UINT_32 len, IN UCHAR *buffer, int OutEp)
{
	int ret = 0;
	UINT_32 count;

	if (in_interrupt()) {
		DBGLOG(REQ, ERROR, "BUG: mtk_usb_bulk_out_msg is called from invalid context\n");
		return FALSE;
	}

	mutex_lock(&prHifInfo->vendor_req_sem);

	/* do a blocking bulk read to get data from the device */
	ret = usb_bulk_msg(prHifInfo->udev,
			   usb_sndbulkpipe(prHifInfo->udev, OutEp), buffer, len, &count, BULK_TIMEOUT_MS);

	mutex_unlock(&prHifInfo->vendor_req_sem);

	if (ret >= 0) {
#if 0
		if (count != len) {
			DBGLOG(HAL, ERROR, "usb_bulk_msg(OUT=%d) Warning. Data is not completed. (send %d/%u)\n", OutEp,
			       count, len);
		}
#endif
		return count;
	}

	DBGLOG(HAL, ERROR, "usb_bulk_msg(OUT=%d) Fail. Error code = %d.\n", OutEp, ret);
	return ret;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is wlan remove wrapper for different caller
*
* \param[in] (none)
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID glUsbWlanRemove(VOID)
{
	if (g_fgDriverProbed) {
		g_fgDriverProbed = FALSE;
		pfWlanRemove();
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will register USB bus to the os
*
* \param[in] pfProbe    Function pointer to detect card
* \param[in] pfRemove   Function pointer to remove card
*
* \return The result of registering USB bus
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS glRegisterBus(probe_card pfProbe, remove_card pfRemove)
{
	int ret = 0;

	ASSERT(pfProbe);
	ASSERT(pfRemove);

	pfWlanProbe = pfProbe;
	pfWlanRemove = pfRemove;

	mtk_usb_driver.probe = mtk_usb_probe;
	mtk_usb_driver.disconnect = mtk_usb_disconnect;
	mtk_usb_driver.suspend = mtk_usb_suspend;
	mtk_usb_driver.resume = mtk_usb_resume;
	mtk_usb_driver.reset_resume = mtk_usb_reset_resume;
	mtk_usb_driver.supports_autosuspend = 1;

	ret = (usb_register(&mtk_usb_driver) == 0) ? WLAN_STATUS_SUCCESS : WLAN_STATUS_FAILURE;

	return ret;
}				/* end of glRegisterBus() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will unregister USB bus to the os
*
* \param[in] pfRemove   Function pointer to remove card
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID glUnregisterBus(remove_card pfRemove)
{
	glUsbWlanRemove();
	usb_deregister(&mtk_usb_driver);
}				/* end of glUnregisterBus() */

VOID glUdmaTxRxEnable(P_GLUE_INFO_T prGlueInfo, BOOLEAN enable)
{
	UINT_32 u4Value = 0;

	kalDevRegRead(prGlueInfo, UDMA_WLCFG_0, &u4Value);

	/* enable UDMA TX & RX */
	if (enable)
		u4Value |= UDMA_WLCFG_0_TX_EN(1) | UDMA_WLCFG_0_RX_EN(1) | UDMA_WLCFG_0_RX_MPSZ_PAD0(1);
	else
		u4Value &= ~(UDMA_WLCFG_0_TX_EN(1) | UDMA_WLCFG_0_RX_EN(1));

	kalDevRegWrite(prGlueInfo, UDMA_WLCFG_0, u4Value);
}

VOID glUdmaRxAggEnable(P_GLUE_INFO_T prGlueInfo, BOOLEAN enable)
{
	UINT_32 u4Value = 0;

	if (enable) {
		kalDevRegRead(prGlueInfo, UDMA_WLCFG_0, &u4Value);
		/* enable UDMA TX & RX */
		u4Value &= ~(UDMA_WLCFG_0_RX_AGG_EN_MASK |
		    UDMA_WLCFG_0_RX_AGG_LMT_MASK |
		    UDMA_WLCFG_0_RX_AGG_TO_MASK);
		u4Value |= UDMA_WLCFG_0_RX_AGG_EN(1) |
		    UDMA_WLCFG_0_RX_AGG_LMT(USB_RX_AGGREGTAION_LIMIT) |
		    UDMA_WLCFG_0_RX_AGG_TO(USB_RX_AGGREGTAION_TIMEOUT);
		kalDevRegWrite(prGlueInfo, UDMA_WLCFG_0, u4Value);

		kalDevRegRead(prGlueInfo, UDMA_WLCFG_1, &u4Value);
		u4Value &= ~UDMA_WLCFG_1_RX_AGG_PKT_LMT_MASK;
		u4Value |= UDMA_WLCFG_1_RX_AGG_PKT_LMT(USB_RX_AGGREGTAION_PKT_LIMIT);
		kalDevRegWrite(prGlueInfo, UDMA_WLCFG_1, u4Value);
	} else {
		kalDevRegRead(prGlueInfo, UDMA_WLCFG_0, &u4Value);
		u4Value &= ~UDMA_WLCFG_0_RX_AGG_EN(1);
		kalDevRegWrite(prGlueInfo, UDMA_WLCFG_0, u4Value);
	}
}

PVOID glUsbInitQ(P_GL_HIF_INFO_T prHifInfo, struct list_head *prHead, UINT_32 u4Cnt)
{
	UINT_32 i;
	P_USB_REQ_T prUsbReqs, prUsbReq;

	INIT_LIST_HEAD(prHead);

	prUsbReqs = kcalloc(u4Cnt, sizeof(struct _USB_REQ_T), GFP_ATOMIC);
	if (!prUsbReqs) {
		DBGLOG(HAL, ERROR, "kcalloc fail!\n");
		return NULL;
	}
	prUsbReq = prUsbReqs;

	for (i = 0; i < u4Cnt; ++i) {
		prUsbReq->prHifInfo = prHifInfo;
		prUsbReq->prUrb = usb_alloc_urb(0, GFP_ATOMIC);

		if (prUsbReq->prUrb == NULL)
			DBGLOG(HAL, ERROR, "usb_alloc_urb() reports error\n");

		prUsbReq->prBufCtrl = NULL;

		INIT_LIST_HEAD(&prUsbReq->list);
		list_add_tail(&prUsbReq->list, prHead);

		prUsbReq++;
	}

	return (PVOID) prUsbReqs;
}

void glUsbUnInitQ(struct list_head *prHead)
{
	P_USB_REQ_T prUsbReq, prUsbReqNext;

	list_for_each_entry_safe(prUsbReq, prUsbReqNext, prHead, list) {
		usb_free_urb(prUsbReq->prUrb);
		list_del_init(&prUsbReq->list);
	}
}

VOID glUsbEnqueueReq(P_GL_HIF_INFO_T prHifInfo, struct list_head *prHead, P_USB_REQ_T prUsbReq, spinlock_t *prLock,
		     BOOLEAN fgHead)
{
	unsigned long flags;
	P_USB_REQ_T prUsbReqCur, prUsbReqTmp;

	spin_lock_irqsave(prLock, flags);

	/* check for repeated prUsbReq */
	if (&prHifInfo->rRxDataFreeQ == prHead) {
		list_for_each_entry_safe(prUsbReqCur,
			prUsbReqTmp, prHead, list) {
			if (&prUsbReq->list == &prUsbReqCur->list) {
				DBGLOG(HAL, ERROR,
					"error: try to insert existed prUsbReq!!\n");
				spin_unlock_irqrestore(prLock, flags);
				return;
			}
		}
	}

	if (fgHead)
		list_add(&prUsbReq->list, prHead);
	else
		list_add_tail(&prUsbReq->list, prHead);
	spin_unlock_irqrestore(prLock, flags);
}

P_USB_REQ_T glUsbDequeueReq(P_GL_HIF_INFO_T prHifInfo, struct list_head *prHead, spinlock_t *prLock)
{
	P_USB_REQ_T prUsbReq;
	unsigned long flags;

	spin_lock_irqsave(prLock, flags);
	if (list_empty(prHead) || (prHead->next == NULL)) {
		spin_unlock_irqrestore(prLock, flags);
		return NULL;
	}
	prUsbReq = list_entry(prHead->next, struct _USB_REQ_T, list);

	/* prHead->next->next == NULL, error happen */
	if ((&prHifInfo->rRxDataFreeQ == prHead) &&
		(prHead->next->next == NULL)) {
		/* Fatal error happen, ring list interrupt */
		DBGLOG(HAL, ERROR,
			"Fatal error: rRxDataFreeQ ring list break!!\n");
		/* Try to resume*/
		INIT_LIST_HEAD(prHead);
		INIT_LIST_HEAD(&prUsbReq->list);
		list_add_tail(&prUsbReq->list, prHead);
		spin_unlock_irqrestore(prLock, flags);
		return NULL;
	}

	list_del_init(prHead->next);
	spin_unlock_irqrestore(prLock, flags);

	return prUsbReq;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function borrow UsbReq from Tx data FFA queue to the spcified TC Tx data free queue
*
* \param[in] prHifInfo  Pointer to the GL_HIF_INFO_T structure
* \param[in] ucTc       Specify TC index
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
BOOLEAN glUsbBorrowFfaReq(P_GL_HIF_INFO_T prHifInfo, UINT_8 ucTc)
{
	P_USB_REQ_T prUsbReq;

	if (list_empty(&prHifInfo->rTxDataFfaQ))
		return FALSE;
	prUsbReq = list_entry(prHifInfo->rTxDataFfaQ.next, struct _USB_REQ_T, list);
	list_del_init(prHifInfo->rTxDataFfaQ.next);

	*((PUINT_8)&prUsbReq->prPriv) = FFA_MASK | ucTc;
	list_add_tail(&prUsbReq->list, &prHifInfo->rTxDataFreeQ[ucTc]);

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function set USB state
*
* \param[in] prHifInfo  Pointer to the GL_HIF_INFO_T structure
* \param[in] state      Specify TC index
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
VOID glUsbSetState(P_GL_HIF_INFO_T prHifInfo, enum usb_state state)
{
	unsigned long flags;

	spin_lock_irqsave(&prHifInfo->rStateLock, flags);
	prHifInfo->state = state;
	spin_unlock_irqrestore(&prHifInfo->rStateLock, flags);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a wrapper of submit urb to ensure driver can transmit
*        WiFi packet when WiFi path of device is allowed.
*
* \param[in] prHifInfo  Pointer to the GL_HIF_INFO_T structure
* \param[in] type       Specify submit type
*
* \retval 0		Successful submissions.
* \retval negative	Error number.
*/
/*----------------------------------------------------------------------------*/
INT_32 glUsbSubmitUrb(P_GL_HIF_INFO_T prHifInfo, struct urb *urb, enum usb_submit_type type)
{
	unsigned long flags;
	int ret = 0;

	if (type == SUBMIT_TYPE_RX_EVENT || type == SUBMIT_TYPE_RX_DATA)
		return usb_submit_urb(urb, GFP_ATOMIC);

	spin_lock_irqsave(&prHifInfo->rStateLock, flags);
	if (type == SUBMIT_TYPE_TX_CMD) {
		if (!(prHifInfo->state == USB_STATE_LINK_UP || prHifInfo->state == USB_STATE_PRE_RESUME)) {
			spin_unlock_irqrestore(&prHifInfo->rStateLock, flags);
			DBGLOG(HAL, INFO, "WiFi path is not allowed to transmit CMD packet. (%d)\n", prHifInfo->state);
			return -ESHUTDOWN;
		}
	} else if (type == SUBMIT_TYPE_TX_DATA) {
		if (prHifInfo->state != USB_STATE_LINK_UP) {
			spin_unlock_irqrestore(&prHifInfo->rStateLock, flags);
			DBGLOG(HAL, INFO, "WiFi path is not allowed to transmit DATA packet. (%d)\n", prHifInfo->state);
			return -ESHUTDOWN;
		}
	}
	ret = usb_submit_urb(urb, GFP_ATOMIC);
	spin_unlock_irqrestore(&prHifInfo->rStateLock, flags);

	return ret;
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
	struct usb_host_interface *alts;
	struct usb_host_endpoint *ep;
	struct usb_endpoint_descriptor *ep_desc;
	P_GL_HIF_INFO_T prHifInfo = &prGlueInfo->rHifInfo;
	P_USB_REQ_T prUsbReq, prUsbReqNext;
	UINT_32 i;
#if CFG_USB_TX_AGG
	UINT_8 ucTc;
#endif

	prHifInfo->eEventEpType = USB_EVENT_TYPE;
	prHifInfo->fgEventEpDetected = FALSE;

	prHifInfo->intf = (struct usb_interface *)ulCookie;
	prHifInfo->udev = interface_to_usbdev(prHifInfo->intf);

	alts = prHifInfo->intf->cur_altsetting;
	DBGLOG(HAL, STATE, "USB Device speed: %x [%u]\n",
		prHifInfo->udev->speed, alts->endpoint[0].desc.wMaxPacketSize);

	if (prHifInfo->eEventEpType == EVENT_EP_TYPE_UNKONW) {
		for (i = 0; i < alts->desc.bNumEndpoints; ++i) {
			ep = &alts->endpoint[i];
			if (ep->desc.bEndpointAddress == USB_EVENT_EP_IN) {
				ep_desc = &alts->endpoint[i].desc;
				switch (ep_desc->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) {
				case USB_ENDPOINT_XFER_INT:
					prHifInfo->eEventEpType = EVENT_EP_TYPE_INTR;
					break;
				case USB_ENDPOINT_XFER_BULK:
				default:
					prHifInfo->eEventEpType = EVENT_EP_TYPE_BULK;
					break;
				}
			}
		}
	}
	ASSERT(prHifInfo->eEventEpType != EVENT_EP_TYPE_UNKONW);
	DBGLOG(HAL, INFO, "Event EP Type: %x\n", prHifInfo->eEventEpType);

	prHifInfo->prGlueInfo = prGlueInfo;
	usb_set_intfdata(prHifInfo->intf, prGlueInfo);

	SET_NETDEV_DEV(prGlueInfo->prDevHandler, &prHifInfo->udev->dev);

	spin_lock_init(&prHifInfo->rTxCmdQLock);
	spin_lock_init(&prHifInfo->rTxDataQLock);
	spin_lock_init(&prHifInfo->rRxEventQLock);
	spin_lock_init(&prHifInfo->rRxDataQLock);
	spin_lock_init(&prHifInfo->rStateLock);

	mutex_init(&prHifInfo->vendor_req_sem);
	prHifInfo->vendor_req_buf = kzalloc(VND_REQ_BUF_SIZE, GFP_KERNEL);
	if (!prHifInfo->vendor_req_buf) {
		DBGLOG(HAL, ERROR, "kzalloc vendor_req_buf %zu error\n",
			VND_REQ_BUF_SIZE);
		goto error;
	}
	prHifInfo->vendor_req_buf_sz = VND_REQ_BUF_SIZE;

#if CFG_USB_TX_AGG
	for (ucTc = 0; ucTc < USB_TC_NUM; ++ucTc) {
		prHifInfo->u4AggRsvSize[ucTc] = 0;
		init_usb_anchor(&prHifInfo->rTxDataAnchor[ucTc]);
	}
#else
	init_usb_anchor(&prHifInfo->rTxDataAnchor);
#endif
	init_usb_anchor(&prHifInfo->rRxDataAnchor);
	init_usb_anchor(&prHifInfo->rRxEventAnchor);

	/* TX CMD */
	prHifInfo->prTxCmdReqHead = glUsbInitQ(prHifInfo, &prHifInfo->rTxCmdFreeQ, USB_REQ_TX_CMD_CNT);
	prUsbReq = list_entry(prHifInfo->rTxCmdFreeQ.next, struct _USB_REQ_T, list);
	i = 0;
	list_for_each_entry_safe(prUsbReq, prUsbReqNext, &prHifInfo->rTxCmdFreeQ, list) {
		prUsbReq->prBufCtrl = &prHifInfo->rTxCmdBufCtrl[i];
#if CFG_USB_CONSISTENT_DMA
		prUsbReq->prBufCtrl->pucBuf = usb_alloc_coherent(prHifInfo->udev, USB_TX_CMD_BUF_SIZE, GFP_ATOMIC,
								 &prUsbReq->prUrb->transfer_dma);
#else
#ifdef CFG_PREALLOC_MEMORY
		prUsbReq->prBufCtrl->pucBuf = preallocGetMem(MEM_ID_TX_CMD);
#else
		prUsbReq->prBufCtrl->pucBuf = kmalloc(USB_TX_CMD_BUF_SIZE, GFP_ATOMIC);
#endif
#endif
		if (prUsbReq->prBufCtrl->pucBuf == NULL) {
			DBGLOG(HAL, ERROR, "kmalloc() reports error\n");
			goto error;
		}
		prUsbReq->prBufCtrl->u4BufSize = USB_TX_CMD_BUF_SIZE;
		++i;
	}

	glUsbInitQ(prHifInfo, &prHifInfo->rTxCmdSendingQ, 0);

	/* TX Data FFA */
	prHifInfo->arTxDataFfaReqHead = glUsbInitQ(prHifInfo,
							&prHifInfo->rTxDataFfaQ, USB_REQ_TX_DATA_FFA_CNT);
	i = 0;
	list_for_each_entry_safe(prUsbReq, prUsbReqNext, &prHifInfo->rTxDataFfaQ, list) {
		QUEUE_INITIALIZE(&prUsbReq->rSendingDataMsduInfoList);
		*((PUINT_8)&prUsbReq->prPriv) = FFA_MASK;
		prUsbReq->prBufCtrl = &prHifInfo->rTxDataFfaBufCtrl[i];
#if CFG_USB_CONSISTENT_DMA
		prUsbReq->prBufCtrl->pucBuf =
		    usb_alloc_coherent(prHifInfo->udev, USB_TX_DATA_BUFF_SIZE, GFP_ATOMIC,
				       &prUsbReq->prUrb->transfer_dma);
#else
#ifdef CFG_PREALLOC_MEMORY
		prUsbReq->prBufCtrl->pucBuf = preallocGetMem(MEM_ID_TX_DATA_FFA);
#else
		prUsbReq->prBufCtrl->pucBuf = kmalloc(USB_TX_DATA_BUFF_SIZE, GFP_ATOMIC);
#endif
#endif
		if (prUsbReq->prBufCtrl->pucBuf == NULL) {
			DBGLOG(HAL, ERROR, "kmalloc() reports error\n");
			goto error;
		}
		prUsbReq->prBufCtrl->u4BufSize = USB_TX_DATA_BUFF_SIZE;
		prUsbReq->prBufCtrl->u4WrIdx = 0;
		++i;
	}

	/* TX Data */
#if CFG_USB_TX_AGG
	for (ucTc = 0; ucTc < USB_TC_NUM; ++ucTc) {
		/* Only for TC0 ~ TC3 and DBDC1_TC */
		if (ucTc >= TC4_INDEX && ucTc < USB_DBDC1_TC)
			continue;
		prHifInfo->arTxDataReqHead[ucTc] = glUsbInitQ(prHifInfo,
								&prHifInfo->rTxDataFreeQ[ucTc], USB_REQ_TX_DATA_CNT);
		i = 0;
		list_for_each_entry_safe(prUsbReq, prUsbReqNext, &prHifInfo->rTxDataFreeQ[ucTc], list) {
			QUEUE_INITIALIZE(&prUsbReq->rSendingDataMsduInfoList);
			/* TODO: every endpoint should has an unique and only TC */
			*((PUINT_8)&prUsbReq->prPriv) = ucTc;
			prUsbReq->prBufCtrl = &prHifInfo->rTxDataBufCtrl[ucTc][i];
#if CFG_USB_CONSISTENT_DMA
			prUsbReq->prBufCtrl->pucBuf =
			    usb_alloc_coherent(prHifInfo->udev, USB_TX_DATA_BUFF_SIZE, GFP_ATOMIC,
					       &prUsbReq->prUrb->transfer_dma);
#else
#ifdef CFG_PREALLOC_MEMORY
			prUsbReq->prBufCtrl->pucBuf = preallocGetMem(MEM_ID_TX_DATA);
#else
			prUsbReq->prBufCtrl->pucBuf = kmalloc(USB_TX_DATA_BUFF_SIZE, GFP_ATOMIC);
#endif
#endif
			if (prUsbReq->prBufCtrl->pucBuf == NULL) {
				DBGLOG(HAL, ERROR, "kmalloc() reports error\n");
				goto error;
			}
			prUsbReq->prBufCtrl->u4BufSize = USB_TX_DATA_BUFF_SIZE;
			prUsbReq->prBufCtrl->u4WrIdx = 0;
			++i;
		}

		DBGLOG(INIT, INFO, "USB Tx URB INIT Tc[%u] cnt[%u] len[%u]\n", ucTc, i,
		       prHifInfo->rTxDataBufCtrl[ucTc][0].u4BufSize);
	}
#else
	glUsbInitQ(prHifInfo, &prHifInfo->rTxDataFreeQ, USB_REQ_TX_DATA_CNT);
	prUsbReq = list_entry(prHifInfo->rTxDataFreeQ.next, struct _USB_REQ_T, list);
	i = 0;
	list_for_each_entry_safe(prUsbReq, prUsbReqNext, &prHifInfo->rTxDataFreeQ, list) {
		QUEUE_INITIALIZE(&prUsbReq->rSendingDataMsduInfoList);
		prUsbReq->prBufCtrl = &prHifInfo->rTxDataBufCtrl[i];
#if CFG_USB_CONSISTENT_DMA
		prUsbReq->prBufCtrl->pucBuf =
		    usb_alloc_coherent(prHifInfo->udev, USB_TX_DATA_BUF_SIZE, GFP_ATOMIC,
				       &prUsbReq->prUrb->transfer_dma);
#else
#ifdef CFG_PREALLOC_MEMORY
		prUsbReq->prBufCtrl->pucBuf = preallocGetMem(MEM_ID_TX_DATA);
#else
		prUsbReq->prBufCtrl->pucBuf = kmalloc(USB_TX_DATA_BUF_SIZE, GFP_ATOMIC);
#endif
#endif
		if (prUsbReq->prBufCtrl->pucBuf == NULL) {
			DBGLOG(HAL, ERROR, "kmalloc() reports error\n");
			goto error;
		}
		prUsbReq->prBufCtrl->u4BufSize = USB_TX_DATA_BUF_SIZE;
		++i;
	}
#endif

	glUsbInitQ(prHifInfo, &prHifInfo->rTxCmdCompleteQ, 0);
	glUsbInitQ(prHifInfo, &prHifInfo->rTxDataCompleteQ, 0);

	/* RX EVENT */
	prHifInfo->prRxEventReqHead = glUsbInitQ(prHifInfo, &prHifInfo->rRxEventFreeQ, USB_REQ_RX_EVENT_CNT);
	i = 0;
	list_for_each_entry_safe(prUsbReq, prUsbReqNext, &prHifInfo->rRxEventFreeQ, list) {
		prUsbReq->prBufCtrl = &prHifInfo->rRxEventBufCtrl[i];
#ifdef CFG_PREALLOC_MEMORY
		prUsbReq->prBufCtrl->pucBuf = preallocGetMem(MEM_ID_RX_EVENT);
#else
		prUsbReq->prBufCtrl->pucBuf = kmalloc(USB_RX_EVENT_BUF_SIZE, GFP_ATOMIC);
#endif
		if (prUsbReq->prBufCtrl->pucBuf == NULL) {
			DBGLOG(HAL, ERROR, "kmalloc() reports error\n");
			goto error;
		}
		prUsbReq->prBufCtrl->u4BufSize = USB_RX_EVENT_BUF_SIZE;
		prUsbReq->prBufCtrl->u4ReadSize = 0;
		++i;
	}

	/* RX Data */
	prHifInfo->prRxDataReqHead = glUsbInitQ(prHifInfo, &prHifInfo->rRxDataFreeQ, USB_REQ_RX_DATA_CNT);
	i = 0;
	list_for_each_entry_safe(prUsbReq, prUsbReqNext, &prHifInfo->rRxDataFreeQ, list) {
		prUsbReq->prBufCtrl = &prHifInfo->rRxDataBufCtrl[i];
#ifdef CFG_PREALLOC_MEMORY
		prUsbReq->prBufCtrl->pucBuf = preallocGetMem(MEM_ID_RX_DATA);
#else
		prUsbReq->prBufCtrl->pucBuf = kmalloc(USB_RX_DATA_BUF_SIZE, GFP_ATOMIC);
#endif
		if (prUsbReq->prBufCtrl->pucBuf == NULL) {
			DBGLOG(HAL, ERROR, "kmalloc() reports error\n");
			goto error;
		}
		prUsbReq->prBufCtrl->u4BufSize = USB_RX_DATA_BUF_SIZE;
		prUsbReq->prBufCtrl->u4ReadSize = 0;
		++i;
	}

	glUsbInitQ(prHifInfo, &prHifInfo->rRxEventCompleteQ, 0);
	glUsbInitQ(prHifInfo, &prHifInfo->rRxDataCompleteQ, 0);

	glUsbSetState(prHifInfo, USB_STATE_LINK_UP);

	return;

error:
	/* TODO */
	;
}				/* end of glSetHifInfo() */

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
	/* P_GL_HIF_INFO_T prHifInfo = NULL; */
	/* ASSERT(prGlueInfo); */
	/* prHifInfo = &prGlueInfo->rHifInfo; */
#if CFG_USB_TX_AGG
	UINT_8 ucTc;
#endif
	P_USB_REQ_T prUsbReq, prUsbReqNext;
	P_GL_HIF_INFO_T prHifInfo = &prGlueInfo->rHifInfo;

#if CFG_USB_TX_AGG
	for (ucTc = 0; ucTc < USB_TC_NUM; ++ucTc) {
		if (ucTc >= TC4_INDEX && ucTc < USB_DBDC1_TC)
			continue;
		list_for_each_entry_safe(prUsbReq, prUsbReqNext, &prHifInfo->rTxDataFreeQ[ucTc], list) {
#if CFG_USB_CONSISTENT_DMA
			usb_free_coherent(prHifInfo->udev, USB_TX_DATA_BUFF_SIZE,
				prUsbReq->prBufCtrl->pucBuf, prUsbReq->prUrb->transfer_dma);
#else
#ifndef CFG_PREALLOC_MEMORY
			kfree(prUsbReq->prBufCtrl->pucBuf);
#endif
#endif
			usb_free_urb(prUsbReq->prUrb);
		}
	}
#else
	list_for_each_entry_safe(prUsbReq, prUsbReqNext, &prHifInfo->rTxDataFreeQ, list) {
#if CFG_USB_CONSISTENT_DMA
		usb_free_coherent(prHifInfo->udev, USB_TX_DATA_BUFF_SIZE,
			prUsbReq->prBufCtrl->pucBuf, prUsbReq->prUrb->transfer_dma);
#else
#ifndef CFG_PREALLOC_MEMORY
		kfree(prUsbReq->prBufCtrl->pucBuf);
#endif
#endif
		usb_free_urb(prUsbReq->prUrb);
	}
#endif

	list_for_each_entry_safe(prUsbReq, prUsbReqNext, &prHifInfo->rTxDataFfaQ, list) {
#if CFG_USB_CONSISTENT_DMA
		usb_free_coherent(prHifInfo->udev, USB_TX_DATA_BUFF_SIZE,
			prUsbReq->prBufCtrl->pucBuf, prUsbReq->prUrb->transfer_dma);
#else
#ifndef CFG_PREALLOC_MEMORY
		kfree(prUsbReq->prBufCtrl->pucBuf);
#endif
#endif
		usb_free_urb(prUsbReq->prUrb);
	}

	list_for_each_entry_safe(prUsbReq, prUsbReqNext, &prHifInfo->rTxCmdFreeQ, list) {
#if CFG_USB_CONSISTENT_DMA
		usb_free_coherent(prHifInfo->udev, USB_TX_CMD_BUF_SIZE,
			prUsbReq->prBufCtrl->pucBuf, prUsbReq->prUrb->transfer_dma);
#else
#ifndef CFG_PREALLOC_MEMORY
		kfree(prUsbReq->prBufCtrl->pucBuf);
#endif
#endif
		usb_free_urb(prUsbReq->prUrb);
	}

	list_for_each_entry_safe(prUsbReq, prUsbReqNext, &prHifInfo->rTxCmdCompleteQ, list) {
#if CFG_USB_CONSISTENT_DMA
		usb_free_coherent(prHifInfo->udev, USB_TX_CMD_BUF_SIZE,
			prUsbReq->prBufCtrl->pucBuf, prUsbReq->prUrb->transfer_dma);
#else
#ifndef CFG_PREALLOC_MEMORY
		kfree(prUsbReq->prBufCtrl->pucBuf);
#endif
#endif
		usb_free_urb(prUsbReq->prUrb);
	}

	list_for_each_entry_safe(prUsbReq, prUsbReqNext, &prHifInfo->rTxDataCompleteQ, list) {
#if CFG_USB_CONSISTENT_DMA
		usb_free_coherent(prHifInfo->udev, USB_TX_CMD_BUF_SIZE,
			prUsbReq->prBufCtrl->pucBuf, prUsbReq->prUrb->transfer_dma);
#else
#ifndef CFG_PREALLOC_MEMORY
		kfree(prUsbReq->prBufCtrl->pucBuf);
#endif
#endif
		usb_free_urb(prUsbReq->prUrb);
	}

	list_for_each_entry_safe(prUsbReq, prUsbReqNext, &prHifInfo->rRxDataFreeQ, list) {
#ifndef CFG_PREALLOC_MEMORY
		kfree(prUsbReq->prBufCtrl->pucBuf);
#endif
		usb_free_urb(prUsbReq->prUrb);
	}

	list_for_each_entry_safe(prUsbReq, prUsbReqNext, &prHifInfo->rRxEventFreeQ, list) {
#ifndef CFG_PREALLOC_MEMORY
		kfree(prUsbReq->prBufCtrl->pucBuf);
#endif
		usb_free_urb(prUsbReq->prUrb);
	}

	list_for_each_entry_safe(prUsbReq, prUsbReqNext, &prHifInfo->rRxDataCompleteQ, list) {
#ifndef CFG_PREALLOC_MEMORY
		kfree(prUsbReq->prBufCtrl->pucBuf);
#endif
		usb_free_urb(prUsbReq->prUrb);
	}

	list_for_each_entry_safe(prUsbReq, prUsbReqNext, &prHifInfo->rRxEventCompleteQ, list) {
#ifndef CFG_PREALLOC_MEMORY
		kfree(prUsbReq->prBufCtrl->pucBuf);
#endif
		usb_free_urb(prUsbReq->prUrb);
	}
	kfree(prHifInfo->prTxCmdReqHead);
	prHifInfo->prTxCmdReqHead = NULL;


	kfree(prHifInfo->arTxDataFfaReqHead);
	prHifInfo->arTxDataFfaReqHead = NULL;

	for (ucTc = 0; ucTc < USB_TC_NUM; ++ucTc) {
		kfree(prHifInfo->arTxDataReqHead[ucTc]);
		prHifInfo->arTxDataReqHead[ucTc] = NULL;
	}

	kfree(prHifInfo->prRxEventReqHead);
	prHifInfo->prRxEventReqHead = NULL;

	kfree(prHifInfo->prRxDataReqHead);
	prHifInfo->prRxDataReqHead = NULL;

	mutex_destroy(&prHifInfo->vendor_req_sem);
	kfree(prHifInfo->vendor_req_buf);
	prHifInfo->vendor_req_buf = NULL;

	prHifInfo->vendor_req_buf_sz = 0;
} /* end of glClearHifInfo() */

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
	return TRUE;
}				/* end of glBusInit() */

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
}				/* end of glBusRelease() */

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

	ASSERT(pvData);
	if (!pvData)
		return -1;

	prNetDevice = (struct net_device *)pvData;
	prGlueInfo = (P_GLUE_INFO_T) pvCookie;
	ASSERT(prGlueInfo);
	if (!prGlueInfo)
		return -1;

	prHifInfo = &prGlueInfo->rHifInfo;

	return ret;
}				/* end of glBusSetIrq() */

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

	ASSERT(pvData);
	if (!pvData) {
		/* printk(KERN_INFO DRV_NAME"%s null pvData\n", __FUNCTION__); */
		return;
	}
	prNetDevice = (struct net_device *)pvData;
	prGlueInfo = (P_GLUE_INFO_T) pvCookie;
	ASSERT(prGlueInfo);
	if (!prGlueInfo) {
		/* printk(KERN_INFO DRV_NAME"%s no glue info\n", __FUNCTION__); */
		return;
	}

	prHifInfo = &prGlueInfo->rHifInfo;
}				/* end of glBusreeIrq() */

BOOLEAN glIsReadClearReg(UINT_32 u4Address)
{
	switch (u4Address) {
	case MCR_WHISR:
	case MCR_WASR:
	case MCR_D2HRM0R:
	case MCR_D2HRM1R:
	case MCR_WTQCR0:
	case MCR_WTQCR1:
	case MCR_WTQCR2:
	case MCR_WTQCR3:
	case MCR_WTQCR4:
	case MCR_WTQCR5:
	case MCR_WTQCR6:
	case MCR_WTQCR7:
		return TRUE;

	default:
		return FALSE;
	}
}

UINT_16 glGetUsbDeviceVendorId(struct usb_device *dev)
{
	return dev->descriptor.idVendor;
}				/* end of glGetUsbDeviceVendorId() */

UINT_16 glGetUsbDeviceProductId(struct usb_device *dev)
{
	return dev->descriptor.idProduct;
}				/* end of glGetUsbDeviceProductId() */

INT_32 glGetUsbDeviceManufacturerName(struct usb_device *dev, UCHAR *buffer, UINT_32 bufLen)
{
	return usb_string(dev, dev->descriptor.iManufacturer, buffer, bufLen);
}				/* end of glGetUsbDeviceManufacturerName() */

INT_32 glGetUsbDeviceProductName(struct usb_device *dev, UCHAR *buffer, UINT_32 bufLen)
{
	return usb_string(dev, dev->descriptor.iProduct, buffer, bufLen);
}				/* end of glGetUsbDeviceManufacturerName() */

INT_32 glGetUsbDeviceSerialNumber(struct usb_device *dev, UCHAR *buffer, UINT_32 bufLen)
{
	return usb_string(dev, dev->descriptor.iSerialNumber, buffer, bufLen);
}				/* end of glGetUsbDeviceSerialNumber() */


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
	int ret = 0;
	UINT_8 ucRetryCount = 0;

	ASSERT(prGlueInfo);
	ASSERT(pu4Value);

	*pu4Value = 0xFFFFFFFF;

	do {
		ret = mtk_usb_vendor_request(prGlueInfo, 0, DEVICE_VENDOR_REQUEST_IN, VND_REQ_REG_READ,
				       (u4Register & 0xffff0000) >> 16, (u4Register & 0x0000ffff), pu4Value,
				       sizeof(*pu4Value));

		if (ret || ucRetryCount)
			DBGLOG(HAL, ERROR, "usb_control_msg() status: %d retry: %u\n",
				ret, ucRetryCount);


		ucRetryCount++;
		if (ucRetryCount > HIF_USB_ACCESS_RETRY_LIMIT)
			break;
	} while (ret);

	if (ret) {
		kalSendAeeWarning(HIF_USB_ERR_TITLE_STR,
				  HIF_USB_ERR_DESC_STR "USB() reports error: %x retry: %u", ret, ucRetryCount);
		DBGLOG(HAL, ERROR, "usb_readl() reports error: %x retry: %u\n", ret, ucRetryCount);
	} else {
		DBGLOG(HAL, INFO, "Get CR[0x%08x] value[0x%08x]\n", u4Register, *pu4Value);
	}

	return (ret) ? FALSE : TRUE;
}				/* end of kalDevRegRead() */

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
	int ret = 0;
	UINT_8 ucRetryCount = 0;

	ASSERT(prGlueInfo);

	do {
		ret = mtk_usb_vendor_request(prGlueInfo, 0, DEVICE_VENDOR_REQUEST_OUT, VND_REQ_REG_WRITE,
				       (u4Register & 0xffff0000) >> 16, (u4Register & 0x0000ffff), &u4Value,
				       sizeof(u4Value));

		if (ret || ucRetryCount)
			DBGLOG(HAL, ERROR, "usb_control_msg() status: %d retry: %u\n", ret, ucRetryCount);

		ucRetryCount++;
		if (ucRetryCount > HIF_USB_ACCESS_RETRY_LIMIT)
			break;

	} while (ret);

	if (ret) {
		kalSendAeeWarning(HIF_USB_ERR_TITLE_STR,
				  HIF_USB_ERR_DESC_STR "usb_writel() reports error: %x retry: %u", ret, ucRetryCount);
		DBGLOG(HAL, ERROR, "usb_writel() reports error: %x retry: %u\n", ret, ucRetryCount);
	} else {
		DBGLOG(HAL, INFO, "Set CR[0x%08x] value[0x%08x]\n", u4Register, u4Value);
	}

	return (ret) ? FALSE : TRUE;
}				/* end of kalDevRegWrite() */

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
kalDevPortRead(IN P_GLUE_INFO_T prGlueInfo,
	       IN UINT_16 u2Port, IN UINT_32 u4Len, OUT PUINT_8 pucBuf, IN UINT_32 u4ValidOutBufSize)
{
	P_GL_HIF_INFO_T prHifInfo = NULL;
	PUINT_8 pucDst = NULL;
	/* int count = u4Len; */
	int ret = 0;
	/* int bNum = 0; */

#if DBG
	DBGLOG(HAL, INFO, "++kalDevPortRead++ buf:0x%p, port:0x%x, length:%d\n", pucBuf, u2Port, u4Len);
#endif

	ASSERT(prGlueInfo);
	prHifInfo = &prGlueInfo->rHifInfo;

	ASSERT(pucBuf);
	pucDst = pucBuf;

	ASSERT(u4Len <= u4ValidOutBufSize);

	u2Port &= MTK_USB_PORT_MASK;
	if (prGlueInfo->rHifInfo.eEventEpType == EVENT_EP_TYPE_INTR &&
		u2Port == (USB_EVENT_EP_IN & USB_ENDPOINT_NUMBER_MASK)) {
		/* maximize buff len for usb in */
		ret = mtk_usb_intr_in_msg(&prGlueInfo->rHifInfo, u4ValidOutBufSize, pucDst, u2Port);
		if (ret != u4Len) {
			DBGLOG(HAL, WARN, "usb_interrupt_msg(IN=%d) Warning. Data is not completed. (receive %d/%u)\n",
			       u2Port, ret, u4Len);
		}
		ret = ret >= 0 ? 0 : ret;
	} else if (u2Port >= MTK_USB_BULK_IN_MIN_EP && u2Port <= MTK_USB_BULK_IN_MAX_EP) {
		/* maximize buff len for usb in */
		ret = mtk_usb_bulk_in_msg(&prGlueInfo->rHifInfo, u4ValidOutBufSize, pucDst, u2Port);
		if (ret != u4Len) {
			DBGLOG(HAL, WARN, "usb_bulk_msg(IN=%d) Warning. Data is not completed. (receive %d/%u)\n",
			       u2Port, ret, u4Len);
		}
		ret = ret >= 0 ? 0 : ret;
	} else {
		DBGLOG(HAL, ERROR, "kalDevPortRead reports error: invalid port %x\n", u2Port);
		ret = -EINVAL;
	}

	if (ret) {
		kalSendAeeWarning(HIF_USB_ERR_TITLE_STR, HIF_USB_ERR_DESC_STR "usb_readsb() reports error: %x", ret);
		DBGLOG(HAL, ERROR, "usb_readsb() reports error: %x\n", ret);
	}

	return (ret) ? FALSE : TRUE;
}				/* end of kalDevPortRead() */

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
	/* int count = u4Len; */
	int ret = 0;
	/* int bNum = 0; */

#if DBG
	DBGLOG(HAL, INFO, "++kalDevPortWrite++ buf:0x%p, port:0x%x, length:%d\n", pucBuf, u2Port, u4Len);
#endif

	ASSERT(prGlueInfo);
	prHifInfo = &prGlueInfo->rHifInfo;

	ASSERT(pucBuf);
	pucSrc = pucBuf;

	ASSERT((u4Len + LEN_USB_UDMA_TX_TERMINATOR) <= u4ValidInBufSize);

	kalMemZero(pucSrc + u4Len, LEN_USB_UDMA_TX_TERMINATOR);
	u4Len += LEN_USB_UDMA_TX_TERMINATOR;

	u2Port &= MTK_USB_PORT_MASK;
	if (u2Port >= MTK_USB_BULK_OUT_MIN_EP && u2Port <= MTK_USB_BULK_OUT_MAX_EP) {
		ret = mtk_usb_bulk_out_msg(&prGlueInfo->rHifInfo, u4Len, pucSrc, 8);
		if (ret != u4Len) {
			DBGLOG(HAL, WARN, "usb_bulk_msg(OUT=%d) Warning. Data is not completed. (receive %d/%u)\n",
			       u2Port, ret, u4Len);
		}
		ret = ret >= 0 ? 0 : ret;
	} else {
		DBGLOG(HAL, ERROR, "kalDevPortWrite reports error: invalid port %x\n", u2Port);
		ret = -EINVAL;
	}

	if (ret) {
		kalSendAeeWarning(HIF_USB_ERR_TITLE_STR, HIF_USB_ERR_DESC_STR "usb_writesb() reports error: %x", ret);
		DBGLOG(HAL, ERROR, "usb_writesb() reports error: %x\n", ret);
	}

	return (ret) ? FALSE : TRUE;
}				/* end of kalDevPortWrite() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Set power state
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[in] ePowerMode
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID glSetPowerState(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 ePowerMode)
{
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Write data to device
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[in] prMsduInfo         msdu info
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
BOOL kalDevWriteData(IN P_GLUE_INFO_T prGlueInfo, IN P_MSDU_INFO_T prMsduInfo)
{
	halTxUSBSendData(prGlueInfo, prMsduInfo);
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
#if 0
	halTxUSBKickData(prGlueInfo);
#endif
	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Write command to device
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[in] u4Addr             I/O port offset
* \param[in] ucData             Single byte of data to be written
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
BOOL kalDevWriteCmd(IN P_GLUE_INFO_T prGlueInfo, IN P_CMD_INFO_T prCmdInfo, IN UINT_8 ucTC)
{
	halTxUSBSendCmd(prGlueInfo, ucTC, prCmdInfo);
	return TRUE;
}

void glGetDev(PVOID ctx, struct device **dev)
{
	struct usb_interface *prUsbIntf = (struct usb_interface *) ctx;
	struct usb_device *prUsbDev = interface_to_usbdev(prUsbIntf);

	*dev = &prUsbDev->dev;
}

void glGetHifDev(P_GL_HIF_INFO_T prHif, struct device **dev)
{
	*dev = &(prHif->udev->dev);
}
