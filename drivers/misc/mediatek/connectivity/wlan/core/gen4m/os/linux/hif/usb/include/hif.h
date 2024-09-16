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
/*! \file   "hif.h"
*    \brief  Functions for the driver to register bus and setup the IRQ
*
*    Functions for the driver to register bus and setup the IRQ
*/


#ifndef _HIF_H
#define _HIF_H

#include "nic_cmd_event.h"
#include "wlan_typedef.h"

enum ENUM_USB_END_POINT {
	USB_DATA_BULK_OUT_EP4 = 4,
	USB_DATA_BULK_OUT_EP5,
	USB_DATA_BULK_OUT_EP6,
	USB_DATA_BULK_OUT_EP7,
	USB_DATA_BULK_OUT_EP8,
	USB_DATA_BULK_OUT_EP9,

	USB_DATA_BULK_IN_EP4 = 4,
	USB_DATA_BULK_IN_EP5,

};

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

#if defined(_HIF_USB)
#define HIF_NAME "USB"
#else
#error "No HIF defined!"
#endif
#define HIF_CR4_FWDL_SECTION_NUM         2
#define HIF_IMG_DL_STATUS_PORT_IDX       0
#define HIF_IST_LOOP_COUNT              (4)
#define HIF_IST_TX_THRESHOLD            (1) /* Min msdu count to trigger Tx during INT polling state */

#define HIF_NUM_OF_QM_RX_PKT_NUM        (512)

#define HIF_TX_BUFF_COUNT_TC0            256
#define HIF_TX_BUFF_COUNT_TC1            256
#define HIF_TX_BUFF_COUNT_TC2            256
#define HIF_TX_BUFF_COUNT_TC3            256
#define HIF_TX_BUFF_COUNT_TC4            256
#define HIF_TX_BUFF_COUNT_TC5            256

#define HIF_TX_RESOURCE_CTRL             0 /* enable/disable TX resource control */
#define HIF_TX_RESOURCE_CTRL_PLE         0 /* enable/disable TX resource control PLE */


#if CFG_USB_TX_AGG
#define HIF_TX_PAGE_SIZE_IN_POWER_OF_2   0
#define HIF_TX_PAGE_SIZE                 1	/* in unit of bytes */
#else
#define HIF_TX_PAGE_SIZE_IN_POWER_OF_2   11
#define HIF_TX_PAGE_SIZE                 2048	/* in unit of bytes */
#endif

#define USB_EVENT_TYPE                  (EVENT_EP_TYPE_UNKONW)

#define USB_CMD_EP_OUT                  (USB_DATA_BULK_OUT_EP8)
#define USB_EVENT_EP_IN                 (0x85)
#define USB_DATA_EP_IN                  (0x84)

#define HIF_TX_INIT_CMD_PORT             USB_CMD_EP_OUT

#ifdef CFG_USB_REQ_TX_DATA_FFA_CNT
#define USB_REQ_TX_DATA_FFA_CNT         (CFG_USB_REQ_TX_DATA_FFA_CNT)	/* platform specific USB_REQ_TX_DATA_FFA_CNT */
#else
#define USB_REQ_TX_DATA_FFA_CNT         (10)
#endif

#ifdef CFG_USB_REQ_TX_DATA_CNT
#define USB_REQ_TX_DATA_CNT             (CFG_USB_REQ_TX_DATA_CNT)	/* platform specific USB_REQ_TX_DATA_CNT */
#else
#if CFG_USB_TX_AGG
#define USB_REQ_TX_DATA_CNT             (2)	/* must be >= 2 */
#else
#define USB_REQ_TX_DATA_CNT             (CFG_TX_MAX_PKT_NUM)
#endif
#endif

#define USB_REQ_TX_CMD_CNT              (CFG_TX_MAX_CMD_PKT_NUM)
#define USB_REQ_RX_EVENT_CNT            (1)
#ifdef CFG_USB_REQ_RX_DATA_CNT
#define USB_REQ_RX_DATA_CNT             (CFG_USB_REQ_RX_DATA_CNT)	/* platform specific USB_REQ_RX_DATA_CNT */
#else
#define USB_REQ_RX_DATA_CNT             (2)
#endif

#define USB_RX_AGGREGTAION_LIMIT        (32)	/* Unit: K-bytes */
#define USB_RX_AGGREGTAION_TIMEOUT      (100)	/* Unit: us */
#define USB_RX_AGGREGTAION_PKT_LIMIT    (30)

#define USB_TX_CMD_BUF_SIZE             (1600)
#if CFG_USB_TX_AGG
#define USB_TX_DATA_BUFF_SIZE           (32*1024)
#else
#define USB_TX_DATA_BUF_SIZE            (NIC_TX_DESC_AND_PADDING_LENGTH + NIC_TX_DESC_HEADER_PADDING_LENGTH + \
					 NIC_TX_MAX_SIZE_PER_FRAME + LEN_USB_UDMA_TX_TERMINATOR)
#endif
#define USB_RX_EVENT_BUF_SIZE           (CFG_RX_MAX_PKT_SIZE + 3 + LEN_USB_RX_PADDING_CSO + 4)
#define USB_RX_DATA_BUF_SIZE            (CFG_RX_MAX_PKT_SIZE + \
					 min(USB_RX_AGGREGTAION_LIMIT * 1024, \
					     (USB_RX_AGGREGTAION_PKT_LIMIT * \
					      (CFG_RX_MAX_PKT_SIZE + 3 + LEN_USB_RX_PADDING_CSO) + 4)))

#define LEN_USB_UDMA_TX_TERMINATOR      (4)	/*HW design spec */
#define LEN_USB_RX_PADDING_CSO          (4)	/*HW design spec */

#define USB_RX_EVENT_RFB_RSV_CNT        (0)
#define USB_RX_DATA_RFB_RSV_CNT         (4)

#define DEVICE_VENDOR_REQUEST_IN        (0xc0)
#define DEVICE_VENDOR_REQUEST_IN_CONNAC2       (0xdF)
#define DEVICE_VENDOR_REQUEST_OUT       (0x40)
#define DEVICE_VENDOR_REQUEST_OUT_CONNAC2       (0x5F)
#define VENDOR_TIMEOUT_MS               (1000)
#define BULK_TIMEOUT_MS                 (1500)
#define INTERRUPT_TIMEOUT_MS            (1000)
#define SW_RFB_RECHECK_MS               (10)
#define SW_RFB_LOG_LIMIT_MS             (5000)

/* Vendor Request */
#define VND_REQ_POWER_ON_WIFI           (0x4)
#define VND_REQ_REG_READ                (0x63)
#define VND_REQ_REG_WRITE               (0x66)
#define VND_REQ_EP5_IN_INFO             (0x67)
#define VND_REQ_FEATURE_SET             (0x91)
#define FEATURE_SET_WVALUE_RESUME       (0x5)
#define FEATURE_SET_WVALUE_SUSPEND      (0x6)
#define VND_REQ_BUF_SIZE                (16)

#define USB_TX_CMD_QUEUE_MASK           (BITS(2, 4))   /* For H2CDMA Tx CMD mapping */

#define USB_DBDC1_TC                    (TC_NUM)/* for DBDC1 */
#define USB_TC_NUM                      (TC_NUM + 1)/* for DBDC1 */
#define USB_TX_EPOUT_NUM                (5)

#define HIF_EXTRA_IO_BUFFER_SIZE        (0)

#define HIF_TX_COALESCING_BUFFER_SIZE   (USB_TX_CMD_BUF_SIZE)
#define HIF_RX_COALESCING_BUFFER_SIZE   (USB_RX_EVENT_BUF_SIZE)

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/* host interface's private data structure, which is attached to os glue
** layer info structure.
 */

enum usb_state {
	USB_STATE_WIFI_OFF, /* Hif power off wifi */
	USB_STATE_LINK_DOWN,
	USB_STATE_PRE_SUSPEND_START,
	USB_STATE_PRE_SUSPEND_DONE,
	USB_STATE_PRE_SUSPEND_FAIL,
	USB_STATE_SUSPEND,
	USB_STATE_PRE_RESUME,
	USB_STATE_LINK_UP,
	USB_STATE_READY
};

enum usb_submit_type {
	SUBMIT_TYPE_TX_CMD,
	SUBMIT_TYPE_TX_DATA,
	SUBMIT_TYPE_RX_EVENT,
	SUBMIT_TYPE_RX_DATA
};

enum EVENT_EP_TYPE {
	EVENT_EP_TYPE_UNKONW,
	EVENT_EP_TYPE_BULK,
	EVENT_EP_TYPE_INTR,
	EVENT_EP_TYPE_DATA_EP
};

enum ENUM_SUSPEND_VERSION {
	SUSPEND_V1 = 1,
	SUSPEND_V2
};

struct BUF_CTRL {
	uint8_t *pucBuf;
	uint32_t u4BufSize;
	uint32_t u4WrIdx;
	uint32_t u4ReadSize;
};

struct GL_HIF_INFO {
	struct usb_interface *intf;
	struct usb_device *udev;

	struct GLUE_INFO *prGlueInfo;
	enum usb_state state;

	spinlock_t rTxDataQLock;
	spinlock_t rTxCmdQLock;
	spinlock_t rRxEventQLock;
	spinlock_t rRxDataQLock;
	spinlock_t rStateLock;

	void *prTxCmdReqHead;
	void *arTxDataFfaReqHead;
	void *arTxDataReqHead[USB_TC_NUM];
	void *prRxEventReqHead;
	void *prRxDataReqHead;
	struct list_head rTxCmdFreeQ;
	spinlock_t rTxCmdFreeQLock;
	struct list_head rTxCmdSendingQ;
	spinlock_t rTxCmdSendingQLock;
	struct list_head rTxDataFfaQ;
#if CFG_USB_TX_AGG
	uint32_t u4AggRsvSize[USB_TC_NUM];
	struct list_head rTxDataFreeQ[USB_TC_NUM];
	struct usb_anchor rTxDataAnchor[USB_TC_NUM];
#else
	struct list_head rTxDataFreeQ;
	struct usb_anchor rTxDataAnchor;
#endif
	/*spinlock_t rTxDataFreeQLock;*/
	struct list_head rRxEventFreeQ;
	/*spinlock_t rRxEventFreeQLock;*/
	struct usb_anchor rRxEventAnchor;
	struct list_head rRxDataFreeQ;
	/*spinlock_t rRxDataFreeQLock;*/
	struct usb_anchor rRxDataAnchor;
	struct list_head rRxEventCompleteQ;
	/*spinlock_t rRxEventCompleteQLock;*/
	struct list_head rRxDataCompleteQ;
	/*spinlock_t rRxDataCompleteQLock;*/
	struct list_head rTxCmdCompleteQ;
	struct list_head rTxDataCompleteQ;

	struct BUF_CTRL rTxCmdBufCtrl[USB_REQ_TX_CMD_CNT];
	struct BUF_CTRL rTxDataFfaBufCtrl[USB_REQ_TX_DATA_FFA_CNT];
#if CFG_USB_TX_AGG
	struct BUF_CTRL rTxDataBufCtrl[USB_TC_NUM][USB_REQ_TX_DATA_CNT];
#else
	struct BUF_CTRL rTxDataBufCtrl[USB_REQ_TX_DATA_CNT];
#endif
	struct BUF_CTRL rRxEventBufCtrl[USB_REQ_RX_EVENT_CNT];
	struct BUF_CTRL rRxDataBufCtrl[USB_REQ_RX_DATA_CNT];

	struct mutex vendor_req_sem;
	void *vendor_req_buf;
	u_int32_t vendor_req_buf_sz;
	u_int8_t fgIntReadClear;
	u_int8_t fgMbxReadClear;
	u_int8_t fgEventEpDetected;
	enum EVENT_EP_TYPE eEventEpType;
};

struct USB_REQ {
	struct list_head list;
	struct urb *prUrb;
	struct BUF_CTRL *prBufCtrl;
	struct GL_HIF_INFO *prHifInfo;
	void *prPriv;
	struct QUE rSendingDataMsduInfoList;
};

struct BUS_INFO {
	const uint32_t u4UdmaWlCfg_0_Addr;
	const uint32_t u4UdmaWlCfg_1_Addr;
	const uint32_t u4UdmaTxQsel;
	const uint32_t u4device_vender_request_in;
	const uint32_t u4device_vender_request_out;
	const uint32_t u4usb_tx_cmd_queue_mask;
	uint32_t u4UdmaWlCfg_0;
	uint32_t u4UdmaTxTimeout; /* UDMA Tx time out limit, unit: us */
	uint32_t u4SuspendVer;
	u_int8_t (*asicUsbSuspend)(
		IN struct ADAPTER *prAdapter,
		IN struct GLUE_INFO *prGlueInfo);
	u_int8_t (*asicUsbResume)(
		IN struct ADAPTER *prAdapter,
		IN struct GLUE_INFO *prGlueInfo);
	uint8_t (*asicUsbEventEpDetected)(IN struct ADAPTER *prAdapter);
	uint16_t (*asicUsbRxByteCount)(IN struct ADAPTER *prAdapter,
		IN struct BUS_INFO *prBusInfo,
		IN uint8_t *pRXD);
	void (*DmaShdlInit)(IN struct ADAPTER *prAdapter);
};

/* USB_REQ_T prPriv field for TxData */
#define FFA_MASK                        BIT(7)           /* Indicate if this UsbReq is from FFA queue. */
#define TC_MASK                         BITS(0, 6)       /* Indicate which TC this UsbReq belongs to */

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

#define USB_TRANS_MSDU_TC(_prMsduInfo) \
	((_prMsduInfo)->ucWmmQueSet ? USB_DBDC1_TC : (_prMsduInfo)->ucTC)

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

uint32_t glRegisterBus(probe_card pfProbe, remove_card pfRemove);

void glUnregisterBus(remove_card pfRemove);

void glSetHifInfo(struct GLUE_INFO *prGlueInfo, unsigned long ulCookie);

void glClearHifInfo(struct GLUE_INFO *prGlueInfo);

u_int8_t glBusInit(void *pvData);

void glBusRelease(void *pData);

int32_t glBusSetIrq(void *pvData, void *pfnIsr, void *pvCookie);

void glBusFreeIrq(void *pvData, void *pvCookie);

void glSetPowerState(IN struct GLUE_INFO *prGlueInfo, IN uint32_t ePowerMode);

void glUdmaTxRxEnable(struct GLUE_INFO *prGlueInfo, u_int8_t enable);

void glUdmaRxAggEnable(struct GLUE_INFO *prGlueInfo, u_int8_t enable);

int32_t mtk_usb_vendor_request(IN struct GLUE_INFO *prGlueInfo,
		IN uint8_t uEndpointAddress, IN uint8_t RequestType,
	    IN uint8_t Request, IN uint16_t Value, IN uint16_t Index,
	    IN void *TransferBuffer, IN uint32_t TransferBufferLength);

void glUsbEnqueueReq(struct GL_HIF_INFO *prHifInfo, struct list_head *prHead, struct USB_REQ *prUsbReq,
		     spinlock_t *prLock, u_int8_t fgHead);
struct USB_REQ *glUsbDequeueReq(struct GL_HIF_INFO *prHifInfo, struct list_head *prHead, spinlock_t *prLock);
u_int8_t glUsbBorrowFfaReq(struct GL_HIF_INFO *prHifInfo, uint8_t ucTc);

void glUsbSetState(IN struct GL_HIF_INFO *prHifInfo, enum usb_state state);

int glUsbSubmitUrb(IN struct GL_HIF_INFO *prHifInfo, struct urb *urb,
			enum usb_submit_type type);

uint32_t halTxUSBSendCmd(IN struct GLUE_INFO *prGlueInfo, IN uint8_t ucTc, IN struct CMD_INFO *prCmdInfo);
void halTxUSBSendCmdComplete(struct urb *urb);
void halTxUSBProcessCmdComplete(IN struct ADAPTER *prAdapter, struct USB_REQ *prUsbReq);

uint32_t halTxUSBSendData(IN struct GLUE_INFO *prGlueInfo, IN struct MSDU_INFO *prMsduInfo);
uint32_t halTxUSBKickData(IN struct GLUE_INFO *prGlueInfo);
void halTxUSBSendDataComplete(struct urb *urb);
void halTxUSBProcessMsduDone(IN struct GLUE_INFO *prGlueInfo, struct USB_REQ *prUsbReq);
void halTxUSBProcessDataComplete(IN struct ADAPTER *prAdapter, struct USB_REQ *prUsbReq);

uint32_t halRxUSBEnqueueRFB(
	IN struct ADAPTER *prAdapter,
	IN uint8_t *pucBuf,
	IN uint32_t u4Length,
	IN uint32_t u4MinRfbCnt,
	IN struct list_head *prCompleteQ);
uint32_t halRxUSBReceiveEvent(IN struct ADAPTER *prAdapter, IN u_int8_t fgFillUrb);
void halRxUSBReceiveEventComplete(struct urb *urb);
uint32_t halRxUSBReceiveData(IN struct ADAPTER *prAdapter);
void halRxUSBReceiveDataComplete(struct urb *urb);
void halRxUSBProcessEventDataComplete(IN struct ADAPTER *prAdapter,
	struct list_head *prCompleteQ, struct list_head *prFreeQ, uint32_t u4MinRfbCnt);

void halUSBPreSuspendCmd(IN struct ADAPTER *prAdapter);
void halUSBPreResumeCmd(IN struct ADAPTER *prAdapter);
void halUSBPreSuspendDone(IN struct ADAPTER *prAdapter, IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);
void halUSBPreSuspendTimeout(IN struct ADAPTER *prAdapter, IN struct CMD_INFO *prCmdInfo);

void glGetDev(void *ctx, struct device **dev);
void glGetHifDev(struct GL_HIF_INFO *prHif, struct device **dev);

void halGetCompleteStatus(IN struct ADAPTER *prAdapter, OUT uint32_t *pu4IntStatus);

uint16_t glGetUsbDeviceVendorId(struct usb_device *dev);
uint16_t glGetUsbDeviceProductId(struct usb_device *dev);

int32_t glGetUsbDeviceManufacturerName(struct usb_device *dev, uint8_t *buffer, uint32_t bufLen);
int32_t glGetUsbDeviceProductName(struct usb_device *dev, uint8_t *buffer, uint32_t bufLen);
int32_t glGetUsbDeviceSerialNumber(struct usb_device *dev, uint8_t *buffer, uint32_t bufLen);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
#endif /* _HIF_H */
