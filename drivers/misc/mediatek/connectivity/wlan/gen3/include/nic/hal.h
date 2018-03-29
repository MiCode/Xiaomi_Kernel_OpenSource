/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/
/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/nic/hal.h#1
*/

/*! \file   "hal.h"
    \brief  The declaration of hal functions

    N/A
*/

#ifndef _HAL_H
#define _HAL_H

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

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/* Macros for flag operations for the Adapter structure */
#define HAL_SET_FLAG(_M, _F)             ((_M)->u4HwFlags |= (_F))
#define HAL_CLEAR_FLAG(_M, _F)           ((_M)->u4HwFlags &= ~(_F))
#define HAL_TEST_FLAG(_M, _F)            ((_M)->u4HwFlags & (_F))
#define HAL_TEST_FLAGS(_M, _F)           (((_M)->u4HwFlags & (_F)) == (_F))

#if defined(_HIF_SDIO)
#define HAL_MCR_RD(_prAdapter, _u4Offset, _pu4Value) \
do { \
	HAL_MCR_HIF_RD(_prAdapter, _u4Offset, _pu4Value); \
} while (0)

#define HAL_MCR_HIF_RD(_prAdapter, _u4Offset, _pu4Value) \
do { \
	if (HAL_TEST_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR) == FALSE) { \
		if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
			ASSERT(0); \
		} \
		if (kalDevRegRead(_prAdapter->prGlueInfo, _u4Offset, _pu4Value) == FALSE) {\
			HAL_SET_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR); \
			fgIsBusAccessFailed = TRUE; \
			DBGLOG(HAL, ERROR, "HAL_MCR_RD access fail! 0x%x: 0x%x\n", \
				(UINT_32) (_u4Offset), *((PUINT_32) (_pu4Value))); \
			glGetRstReason(RST_HIF_RD_FAIL); \
			glResetTrigger(_prAdapter); \
		} \
	} else { \
		DBGLOG(HAL, WARN, "ignore HAL_MCR_RD access! 0x%x\n", \
			(UINT_32) (_u4Offset)); \
	} \
} while (0)

#define HAL_MCR_WR(_prAdapter, _u4Offset, _u4Value) \
do { \
	HAL_MCR_HIF_WR(_prAdapter, _u4Offset, _u4Value); \
} while (0)

#define HAL_MCR_HIF_WR(_prAdapter, _u4Offset, _u4Value) \
do { \
	if (HAL_TEST_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR) == FALSE) { \
		if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
			ASSERT(0); \
		} \
		if (kalDevRegWrite(_prAdapter->prGlueInfo, _u4Offset, _u4Value) == FALSE) {\
			HAL_SET_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR); \
			fgIsBusAccessFailed = TRUE; \
			DBGLOG(HAL, ERROR, "HAL_MCR_WR access fail! 0x%x: 0x%x\n", \
				(UINT_32) (_u4Offset), (UINT_32) (_u4Value)); \
			glGetRstReason(RST_HIF_WR_FAIL); \
			glResetTrigger(_prAdapter); \
		} \
	} else { \
		DBGLOG(HAL, WARN, "ignore HAL_MCR_WR access! 0x%x: 0x%x\n", \
			(UINT_32) (_u4Offset), (UINT_32) (_u4Value)); \
	} \
} while (0)

#define HAL_PORT_RD(_prAdapter, _u4Port, _u4Len, _pucBuf, _u4ValidBufSize) \
{ \
	/*fgResult = FALSE; */\
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	if (HAL_TEST_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR) == FALSE) { \
		UINT_32 i = 1; \
		while (kalDevPortRead(_prAdapter->prGlueInfo, _u4Port, _u4Len, _pucBuf, _u4ValidBufSize) == FALSE) {\
			if (i < 5) { \
				i++; \
				continue; \
			} \
			HAL_SET_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR); \
			fgIsBusAccessFailed = TRUE; \
			DBGLOG(HAL, ERROR, "HAL_PORT_RD access fail! 0x%x\n", \
				(UINT_32) (_u4Port)); \
			glGetRstReason(RST_PORT_RD_FAIL); \
			glResetTrigger(_prAdapter); \
			break; \
		} \
	} else { \
		DBGLOG(HAL, WARN, "ignore HAL_PORT_RD access! 0x%x\n", \
			(UINT_32) (_u4Port)); \
	} \
}

#define HAL_PORT_WR(_prAdapter, _u4Port, _u4Len, _pucBuf, _u4ValidBufSize) \
{ \
	/*fgResult = FALSE; */\
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	if (HAL_TEST_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR) == FALSE) { \
		UINT_32 i = 1; \
		while (kalDevPortWrite(_prAdapter->prGlueInfo, _u4Port, _u4Len, _pucBuf, _u4ValidBufSize) == FALSE) {\
			if (i < 5) { \
				i++; \
				continue; \
			} \
			HAL_SET_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR); \
			fgIsBusAccessFailed = TRUE; \
			DBGLOG(HAL, ERROR, "HAL_PORT_WR access fail! 0x%x\n", \
				(UINT_32) (_u4Port)); \
			glGetRstReason(RST_PORT_WR_FAIL); \
			glResetTrigger(_prAdapter); \
			break; \
		} \
	} else { \
		DBGLOG(HAL, WARN, "ignore HAL_PORT_WR access! 0x%x\n", \
			(UINT_32) (_u4Port)); \
	} \
}

#define HAL_BYTE_WR(_prAdapter, _u4Port, _ucBuf) \
{ \
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	if (HAL_TEST_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR) == FALSE) { \
		if (kalDevWriteWithSdioCmd52(_prAdapter->prGlueInfo, _u4Port, _ucBuf) == FALSE) {\
			HAL_SET_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR); \
			fgIsBusAccessFailed = TRUE; \
			DBGLOG(HAL, ERROR, "HAL_BYTE_WR access fail! 0x%x\n", \
				(UINT_32)(_u4Port)); \
		} \
		else { \
			/* Todo:: Nothing*/ \
		} \
	} \
	else { \
		DBGLOG(HAL, WARN, "ignore HAL_BYTE_WR access! 0x%x\n", \
			(UINT_32) (_u4Port)); \
	} \
}

#define HAL_DRIVER_OWN_BY_SDIO_CMD52(_prAdapter, _pfgDriverIsOwnReady) \
{ \
	UINT_8 ucBuf = BIT(1); \
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	if (HAL_TEST_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR) == FALSE) { \
		if (kalDevReadAfterWriteWithSdioCmd52(_prAdapter->prGlueInfo, MCR_WHLPCR_BYTE1, &ucBuf, 1) == FALSE) {\
			HAL_SET_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR); \
			fgIsBusAccessFailed = TRUE; \
			DBGLOG(HAL, ERROR, "kalDevReadAfterWriteWithSdioCmd52 access fail!\n"); \
		} \
		else { \
			*_pfgDriverIsOwnReady = (ucBuf & BIT(0)) ? TRUE : FALSE; \
		} \
	} else { \
		DBGLOG(HAL, WARN, "ignore HAL_DRIVER_OWN_BY_SDIO_CMD52 access!\n"); \
	} \
}

#else /* #if defined(_HIF_SDIO) */
/* Now rx_thread don't access hif register, so if someone call HAL_MCR_RD,
 * no need to use spinlock to protect u4Register and prRegValue. */
#define HAL_MCR_RD(_prAdapter, _u4Offset, _pu4Value) \
do { \
	if ((!_prAdapter->prGlueInfo->hif_thread) || !kalStrnCmp(current->comm, "hif_thread", 10)) { \
		HAL_MCR_HIF_RD(_prAdapter, _u4Offset, _pu4Value); \
	} else { \
		_prAdapter->prGlueInfo->u4Register = _u4Offset; \
		_prAdapter->prGlueInfo->prRegValue = _pu4Value; \
		KAL_WAKE_LOCK_TIMEOUT(_prAdapter, &_prAdapter->prGlueInfo->rTimeoutWakeLock, \
							MSEC_TO_JIFFIES(WAKE_LOCK_THREAD_WAKEUP_TIMEOUT)); \
		set_bit(GLUE_FLAG_HAL_MCR_RD_BIT, &_prAdapter->prGlueInfo->ulFlag); \
		wake_up_interruptible(&_prAdapter->prGlueInfo->waitq_hif); \
		wait_for_completion_interruptible(&_prAdapter->prGlueInfo->rHalRDMCRComp); \
	} \
} while (0)

#define HAL_MCR_HIF_RD(_prAdapter, _u4Offset, _pu4Value) \
do { \
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	kalDevRegRead(_prAdapter->prGlueInfo, _u4Offset, _pu4Value); \
} while (0)

#define HAL_MCR_WR(_prAdapter, _u4Offset, _u4Value) \
do { \
	if ((!_prAdapter->prGlueInfo->hif_thread) || !kalStrnCmp(current->comm, "hif_thread", 10)) { \
		HAL_MCR_HIF_WR(_prAdapter, _u4Offset, _u4Value); \
	} else { \
		_prAdapter->prGlueInfo->u4Register = _u4Offset; \
		_prAdapter->prGlueInfo->u4RegValue = _u4Value; \
		KAL_WAKE_LOCK_TIMEOUT(_prAdapter, &_prAdapter->prGlueInfo->rTimeoutWakeLock, \
							MSEC_TO_JIFFIES(WAKE_LOCK_THREAD_WAKEUP_TIMEOUT)); \
		set_bit(GLUE_FLAG_HAL_MCR_WR_BIT, &_prAdapter->prGlueInfo->ulFlag); \
		wake_up_interruptible(&_prAdapter->prGlueInfo->waitq_hif); \
		wait_for_completion_interruptible(&_prAdapter->prGlueInfo->rHalWRMCRComp); \
	} \
} while (0)

#define HAL_MCR_HIF_WR(_prAdapter, _u4Offset, _u4Value) \
do { \
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	kalDevRegWrite(_prAdapter->prGlueInfo, _u4Offset, _u4Value); \
} while (0)

#define HAL_PORT_RD(_prAdapter, _u4Port, _u4Len, _pucBuf, _u4ValidBufSize) \
do{ \
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	kalDevPortRead(_prAdapter->prGlueInfo, _u4Port, _u4Len, _pucBuf, _u4ValidBufSize); \
}while(0)

#define HAL_PORT_WR(_prAdapter, _u4Port, _u4Len, _pucBuf, _u4ValidBufSize) \
do{ \
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	kalDevPortWrite(_prAdapter->prGlueInfo, _u4Port, _u4Len, _pucBuf, _u4ValidBufSize); \
}while(0)

#endif /* #if defined(_HIF_SDIO) */

#define HAL_READ_RX_PORT(prAdapter, u4PortId, u4Len, pvBuf, _u4ValidBufSize) \
{ \
	ASSERT(u4PortId < 2); \
	HAL_PORT_RD(prAdapter, \
		((u4PortId == 0) ? MCR_WRDR0 : MCR_WRDR1), \
		u4Len, \
		pvBuf, \
		_u4ValidBufSize/*temp!!*//*4Kbyte*/); \
}

#define HAL_WRITE_TX_PORT(_prAdapter, _u4Len, _pucBuf, _u4ValidBufSize) \
{ \
	if ((_u4ValidBufSize - _u4Len) >= sizeof(UINT_32)) { \
		/* fill with single dword of zero as TX-aggregation termination */ \
		*(PUINT_32) (&((_pucBuf)[ALIGN_4(_u4Len)])) = 0; \
	} \
	HAL_PORT_WR(_prAdapter, \
		MCR_WTDR1, \
		_u4Len, \
		_pucBuf, \
		_u4ValidBufSize/*temp!!*//*4KByte*/); \
}

/* The macro to read the given MCR several times to check if the wait
   condition come true. */
#define HAL_MCR_RD_AND_WAIT(_pAdapter, _offset, _pReadValue, _waitCondition, _waitDelay, _waitCount, _status) \
	{ \
		UINT_32 count; \
		(_status) = FALSE; \
		for (count = 0; count < (_waitCount); count++) { \
			HAL_MCR_RD((_pAdapter), (_offset), (_pReadValue)); \
			if ((_waitCondition)) { \
				(_status) = TRUE; \
				break; \
			} \
			kalUdelay((_waitDelay)); \
		} \
	}

/* The macro to write 1 to a R/S bit and read it several times to check if the
   command is done */
#define HAL_MCR_WR_AND_WAIT(_pAdapter, _offset, _writeValue, _busyMask, _waitDelay, _waitCount, _status) \
	{ \
		UINT_32 u4Temp; \
		UINT_32 u4Count = _waitCount; \
		(_status) = FALSE; \
		HAL_MCR_WR((_pAdapter), (_offset), (_writeValue)); \
		do { \
			kalUdelay((_waitDelay)); \
			HAL_MCR_RD((_pAdapter), (_offset), &u4Temp); \
			if (!(u4Temp & (_busyMask))) { \
				(_status) = TRUE; \
				break; \
			} \
			u4Count--; \
		} while (u4Count); \
	}

#define HAL_GET_CHIP_ID_VER(_prAdapter, pu2ChipId, pu2Version) \
{ \
	UINT_32 u4Value; \
	HAL_MCR_RD(_prAdapter, \
		MCR_WCIR, \
		&u4Value); \
	*pu2ChipId = (UINT_16)(u4Value & WCIR_CHIP_ID); \
	*pu2Version = (UINT_16)(u4Value & WCIR_REVISION_ID) >> 16; \
}

#define HAL_WAIT_WIFI_FUNC_READY(_prAdapter) \
{ \
	UINT_32 u4Value; \
	UINT_32 i; \
	for (i = 0; i < 100; i++) { \
		HAL_MCR_RD(_prAdapter, \
			MCR_WCIR, \
			&u4Value); \
		if (u4Value & WCIR_WLAN_READY) { \
			break; \
		} \
		NdisMSleep(10); \
	} \
}

#define HAL_INTR_DISABLE(_prAdapter) \
	HAL_MCR_WR(_prAdapter, \
		MCR_WHLPCR, \
		WHLPCR_INT_EN_CLR)

#define HAL_INTR_ENABLE(_prAdapter) \
	HAL_MCR_WR(_prAdapter, \
		MCR_WHLPCR, \
		WHLPCR_INT_EN_SET)

#define HAL_INTR_ENABLE_AND_LP_OWN_SET(_prAdapter) \
	HAL_MCR_WR(_prAdapter, \
		MCR_WHLPCR, \
		(WHLPCR_INT_EN_SET | WHLPCR_FW_OWN_REQ_SET))

#define HAL_LP_OWN_SET(_prAdapter) \
	HAL_MCR_WR(_prAdapter, \
		MCR_WHLPCR, \
		WHLPCR_FW_OWN_REQ_SET)

#define HAL_LP_OWN_CLR_OK(_prAdapter, _pfgResult) \
{ \
	UINT_32 i; \
	UINT_32 u4RegValue; \
	UINT_32 u4LoopCnt = 2048 / 8; \
	*_pfgResult = TRUE; \
	/* Software get LP ownership */ \
	HAL_MCR_WR(_prAdapter, \
			MCR_WHLPCR, \
			WHLPCR_FW_OWN_REQ_CLR) \
	for (i = 0; i < u4LoopCnt; i++) { \
		HAL_MCR_RD(_prAdapter, MCR_WHLPCR, &u4RegValue); \
		if (u4RegValue & WHLPCR_IS_DRIVER_OWN) { \
			break; \
		} \
		else { \
			kalUdelay(8); \
		} \
	} \
	if (i == u4LoopCnt) { \
		*_pfgResult = FALSE; \
		/*ERRORLOG(("LP cannot be own back (%ld)", u4LoopCnt));*/ \
		/* check the time of LP instructions need to perform from Sleep to On */ \
		/*ASSERT(0); */ \
	} \
}

#define HAL_GET_ABNORMAL_INTERRUPT_REASON_CODE(_prAdapter, pu4AbnormalReason) \
{ \
	HAL_MCR_RD(_prAdapter, \
		MCR_WASR, \
		pu4AbnormalReason); \
}

#define HAL_DISABLE_RX_ENHANCE_MODE(_prAdapter) \
{ \
	UINT_32 u4Value; \
	HAL_MCR_RD(_prAdapter, \
		MCR_WHCR, \
		&u4Value); \
	HAL_MCR_WR(_prAdapter, \
		MCR_WHCR, \
		u4Value & ~WHCR_RX_ENHANCE_MODE_EN); \
}

#define HAL_ENABLE_RX_ENHANCE_MODE(_prAdapter) \
{ \
	UINT_32 u4Value; \
	HAL_MCR_RD(_prAdapter, \
		MCR_WHCR, \
		&u4Value); \
	HAL_MCR_WR(_prAdapter, \
		MCR_WHCR, \
		u4Value | WHCR_RX_ENHANCE_MODE_EN); \
}

#define HAL_CFG_MAX_HIF_RX_LEN_NUM(_prAdapter, _ucNumOfRxLen) \
{ \
	UINT_32 u4Value, ucNum; \
	ucNum = ((_ucNumOfRxLen >= 16) ? 0 : _ucNumOfRxLen); \
	u4Value = 0; \
	HAL_MCR_RD(_prAdapter, \
		MCR_WHCR, \
		&u4Value); \
	u4Value &= ~WHCR_MAX_HIF_RX_LEN_NUM; \
	u4Value |= ((((UINT_32)ucNum) << WHCR_OFFSET_MAX_HIF_RX_LEN_NUM) & WHCR_MAX_HIF_RX_LEN_NUM); \
	HAL_MCR_WR(_prAdapter, \
		MCR_WHCR, \
		u4Value); \
}

#define HAL_SET_INTR_STATUS_READ_CLEAR(prAdapter) \
{ \
	UINT_32 u4Value; \
	HAL_MCR_RD(prAdapter, \
		MCR_WHCR, \
		&u4Value); \
	HAL_MCR_WR(prAdapter, \
		MCR_WHCR, \
		u4Value & ~WHCR_W_INT_CLR_CTRL); \
	prAdapter->prGlueInfo->rHifInfo.fgIntReadClear = TRUE;\
}

#define HAL_SET_INTR_STATUS_WRITE_1_CLEAR(prAdapter) \
{ \
	UINT_32 u4Value; \
	HAL_MCR_RD(prAdapter, \
		MCR_WHCR, \
		&u4Value); \
	HAL_MCR_WR(prAdapter, \
		MCR_WHCR, \
		u4Value | WHCR_W_INT_CLR_CTRL); \
	prAdapter->prGlueInfo->rHifInfo.fgIntReadClear = FALSE;\
}

/* Note: enhance mode structure may also carried inside the buffer,
	 if the length of the buffer is long enough */
#define HAL_READ_INTR_STATUS(prAdapter, length, pvBuf) \
	HAL_PORT_RD(prAdapter, \
		MCR_WHISR, \
		length, \
		pvBuf, \
		length)

#define HAL_READ_TX_RELEASED_COUNT(_prAdapter, au2TxReleaseCount) \
{ \
	PUINT_32 pu4Value = (PUINT_32)au2TxReleaseCount; \
	HAL_MCR_RD(_prAdapter, \
		MCR_WTQCR0, \
		&pu4Value[0]); \
	HAL_MCR_RD(_prAdapter, \
		MCR_WTQCR1, \
		&pu4Value[1]); \
	HAL_MCR_RD(_prAdapter, \
		MCR_WTQCR2, \
		&pu4Value[2]); \
	HAL_MCR_RD(_prAdapter, \
		MCR_WTQCR3, \
		&pu4Value[3]); \
	HAL_MCR_RD(_prAdapter, \
		MCR_WTQCR4, \
		&pu4Value[4]); \
	HAL_MCR_RD(_prAdapter, \
		MCR_WTQCR5, \
		&pu4Value[5]); \
	HAL_MCR_RD(_prAdapter, \
		MCR_WTQCR6, \
		&pu4Value[6]); \
	HAL_MCR_RD(_prAdapter, \
		MCR_WTQCR7, \
		&pu4Value[7]); \
}

#define HAL_READ_RX_LENGTH(prAdapter, pu2Rx0Len, pu2Rx1Len) \
{ \
	UINT_32 u4Value; \
	u4Value = 0; \
	HAL_MCR_RD(prAdapter, \
		MCR_WRPLR, \
		&u4Value); \
	*pu2Rx0Len = (UINT_16)u4Value; \
	*pu2Rx1Len = (UINT_16)(u4Value >> 16); \
}

#define HAL_GET_INTR_STATUS_FROM_ENHANCE_MODE_STRUCT(pvBuf, u2Len, pu4Status) \
{ \
	PUINT_32 pu4Buf = (PUINT_32)pvBuf; \
	*pu4Status = pu4Buf[0]; \
}

#define HAL_GET_TX_STATUS_FROM_ENHANCE_MODE_STRUCT(pvInBuf, pu4BufOut, u4LenBufOut) \
{ \
	PUINT_32 pu4Buf = (PUINT_32)pvInBuf; \
	ASSERT(u4LenBufOut >= 8); \
	pu4BufOut[0] = pu4Buf[1]; \
	pu4BufOut[1] = pu4Buf[2]; \
}

#define HAL_GET_RX_LENGTH_FROM_ENHANCE_MODE_STRUCT(pvInBuf, pu2Rx0Num, au2Rx0Len, pu2Rx1Num, au2Rx1Len) \
{ \
	PUINT_32 pu4Buf = (PUINT_32)pvInBuf; \
	ASSERT((sizeof(au2Rx0Len) / sizeof(UINT_16)) >= 16); \
	ASSERT((sizeof(au2Rx1Len) / sizeof(UINT_16)) >= 16); \
	*pu2Rx0Num = (UINT_16)pu4Buf[3]; \
	*pu2Rx1Num = (UINT_16)(pu4Buf[3] >> 16); \
	kalMemCopy(au2Rx0Len, &pu4Buf[4], 8); \
	kalMemCopy(au2Rx1Len, &pu4Buf[12], 8); \
}

#define HAL_GET_MAILBOX_FROM_ENHANCE_MODE_STRUCT(pvInBuf, pu4Mailbox0, pu4Mailbox1) \
{ \
	PUINT_32 pu4Buf = (PUINT_32)pvInBuf; \
	*pu4Mailbox0 = (UINT_16)pu4Buf[21]; \
	*pu4Mailbox1 = (UINT_16)pu4Buf[22]; \
}

#define HAL_IS_TX_DONE_INTR(u4IntrStatus) \
	((u4IntrStatus & WHISR_TX_DONE_INT) ? TRUE : FALSE)

#define HAL_IS_RX_DONE_INTR(u4IntrStatus) \
	((u4IntrStatus & (WHISR_RX0_DONE_INT | WHISR_RX1_DONE_INT)) ? TRUE : FALSE)

#define HAL_IS_ABNORMAL_INTR(u4IntrStatus) \
	((u4IntrStatus & WHISR_ABNORMAL_INT) ? TRUE : FALSE)

#define HAL_IS_FW_OWNBACK_INTR(u4IntrStatus) \
	((u4IntrStatus & WHISR_FW_OWN_BACK_INT) ? TRUE : FALSE)

#define HAL_PUT_MAILBOX(prAdapter, u4MboxId, u4Data) \
{ \
	ASSERT(u4MboxId < 2); \
	HAL_MCR_WR(prAdapter, \
		((u4MboxId == 0) ? MCR_H2DSM0R : MCR_H2DSM1R), \
		u4Data); \
}

#define HAL_GET_MAILBOX(prAdapter, u4MboxId, pu4Data) \
{ \
	ASSERT(u4MboxId < 2); \
	HAL_MCR_RD(prAdapter, \
		((u4MboxId == 0) ? MCR_D2HRM0R : MCR_D2HRM1R), \
		pu4Data); \
}

#define HAL_SET_MAILBOX_READ_CLEAR(prAdapter, fgEnableReadClear) \
{ \
	UINT_32 u4Value; \
	HAL_MCR_RD(prAdapter, MCR_WHCR, &u4Value);\
	HAL_MCR_WR(prAdapter, MCR_WHCR, \
		    (fgEnableReadClear) ? \
			(u4Value | WHCR_RECV_MAILBOX_RD_CLR_EN) : \
			(u4Value & ~WHCR_RECV_MAILBOX_RD_CLR_EN)); \
	prAdapter->prGlueInfo->rHifInfo.fgMbxReadClear = fgEnableReadClear;\
}

#define HAL_GET_MAILBOX_READ_CLEAR(prAdapter) (prAdapter->prGlueInfo->rHifInfo.fgMbxReadClear)

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* _HAL_H */
