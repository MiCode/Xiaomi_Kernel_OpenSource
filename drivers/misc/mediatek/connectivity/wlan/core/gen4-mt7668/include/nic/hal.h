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
/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/nic/hal.h#1
*/

/*! \file   "hal.h"
 *   \brief  The declaration of hal functions
 *
 *   N/A
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
	if (HAL_TEST_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR) == FALSE) { \
		if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
			ASSERT(0); \
		} \
		if (_u4Offset > 0xFFFF) { \
			if (kalDevRegRead_mac(_prAdapter->prGlueInfo, _u4Offset, _pu4Value) == FALSE) {\
				HAL_SET_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR); \
				fgIsBusAccessFailed = TRUE; \
				DBGLOG(HAL, ERROR, \
				"HAL_MCR_RD (MAC) access fail! 0x%x: 0x%x\n", \
					(UINT_32) (_u4Offset), *((PUINT_32) (_pu4Value))); \
			} \
		} else { \
			if (kalDevRegRead(_prAdapter->prGlueInfo, _u4Offset, _pu4Value) == FALSE) {\
				HAL_SET_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR); \
				fgIsBusAccessFailed = TRUE; \
				DBGLOG(HAL, ERROR, \
				"HAL_MCR_RD (SDIO) access fail! 0x%x: 0x%x\n", \
					(UINT_32) (_u4Offset), *((PUINT_32) (_pu4Value))); \
			} \
		} \
	} else { \
		DBGLOG(HAL, WARN, "ignore HAL_MCR_RD access! 0x%x\n", \
			(UINT_32) (_u4Offset)); \
	} \
} while (0)

#define HAL_MCR_WR(_prAdapter, _u4Offset, _u4Value) \
do { \
	if (HAL_TEST_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR) == FALSE) { \
		if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
			ASSERT(0); \
		} \
		if (_u4Offset > 0xFFFF) { \
			if (kalDevRegWrite_mac(_prAdapter->prGlueInfo, _u4Offset, _u4Value) == FALSE) {\
				HAL_SET_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR); \
				fgIsBusAccessFailed = TRUE; \
				DBGLOG(HAL, ERROR, \
				"HAL_MCR_WR (MAC) access fail! 0x%x: 0x%x\n", \
					(UINT_32) (_u4Offset), (UINT_32) (_u4Value)); \
			} \
		} else { \
			if (kalDevRegWrite(_prAdapter->prGlueInfo, _u4Offset, _u4Value) == FALSE) {\
				HAL_SET_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR); \
				fgIsBusAccessFailed = TRUE; \
				DBGLOG(HAL, ERROR, \
				"HAL_MCR_WR (SDIO) access fail! 0x%x: 0x%x\n", \
					(UINT_32) (_u4Offset), (UINT_32) (_u4Value)); \
			} \
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
		if (kalDevPortRead(_prAdapter->prGlueInfo, _u4Port, _u4Len, _pucBuf, _u4ValidBufSize) == FALSE) {\
			HAL_SET_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR); \
			fgIsBusAccessFailed = TRUE; \
			DBGLOG(HAL, ERROR, "HAL_PORT_RD access fail! 0x%x\n", \
				(UINT_32) (_u4Port)); \
		} \
		else { \
			/*fgResult = TRUE;*/ } \
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
		if (kalDevPortWrite(_prAdapter->prGlueInfo, _u4Port, _u4Len, _pucBuf, _u4ValidBufSize) == FALSE) {\
			HAL_SET_FLAG(_prAdapter, ADAPTER_FLAG_HW_ERR); \
			fgIsBusAccessFailed = TRUE; \
			DBGLOG(HAL, ERROR, "HAL_PORT_WR access fail! 0x%x\n", \
				(UINT_32) (_u4Port)); \
		} \
		else \
			; /*fgResult = TRUE;*/ \
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
#define HAL_MCR_RD(_prAdapter, _u4Offset, _pu4Value) \
{ \
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	if (kalDevRegRead(_prAdapter->prGlueInfo, _u4Offset, _pu4Value) \
	    == FALSE) { \
		DBGLOG(HAL, ERROR, "[USB] kalDevRegRead fail\n"); \
	} \
}

#define HAL_MCR_WR(_prAdapter, _u4Offset, _u4Value) \
{ \
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	kalDevRegWrite(_prAdapter->prGlueInfo, _u4Offset, _u4Value); \
}

#define HAL_PORT_RD(_prAdapter, _u4Port, _u4Len, _pucBuf, _u4ValidBufSize) \
{ \
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	kalDevPortRead(_prAdapter->prGlueInfo, _u4Port, _u4Len, _pucBuf, _u4ValidBufSize); \
}

#define HAL_PORT_WR(_prAdapter, _u4Port, _u4Len, _pucBuf, _u4ValidBufSize) \
{ \
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	kalDevPortWrite(_prAdapter->prGlueInfo, _u4Port, _u4Len, _pucBuf, _u4ValidBufSize); \
}

#define HAL_BYTE_WR(_prAdapter, _u4Port, _ucBuf) \
{ \
	HAL_MCR_WR(_prAdapter, _u4Port, (UINT_32)_ucBuf); \
}

#endif /* #if defined(_HIF_SDIO) */

#define HAL_WRITE_TX_DATA(_prAdapter, _prMsduInfo) \
{ \
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	kalDevWriteData(_prAdapter->prGlueInfo, _prMsduInfo); \
}

#define HAL_KICK_TX_DATA(_prAdapter) \
{ \
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	kalDevKickData(_prAdapter->prGlueInfo); \
}

#define HAL_WRITE_TX_CMD(_prAdapter, _prCmdInfo, _ucTC) \
{ \
	if (_prAdapter->rAcpiState == ACPI_STATE_D3) { \
		ASSERT(0); \
	} \
	kalDevWriteCmd(_prAdapter->prGlueInfo, _prCmdInfo, _ucTC); \
}

#if defined(_HIF_PCIE)
#define HAL_READ_RX_PORT(prAdapter, u4PortId, u4Len, pvBuf, _u4ValidBufSize) \
{ \
	ASSERT(u4PortId < 2); \
	HAL_PORT_RD(prAdapter, \
		u4PortId, \
		u4Len, \
		pvBuf, \
		_u4ValidBufSize/*temp!!*//*4Kbyte*/) \
}

#define HAL_WRITE_TX_PORT(_prAdapter, _u4PortId, _u4Len, _pucBuf, _u4ValidBufSize) \
{ \
	HAL_PORT_WR(_prAdapter, \
		_u4PortId, \
		_u4Len, \
		_pucBuf, \
		_u4ValidBufSize/*temp!!*//*4KByte*/) \
}

#define HAL_MCR_RD_AND_WAIT(_pAdapter, _offset, _pReadValue, _waitCondition, _waitDelay, _waitCount, _status)

#define HAL_MCR_WR_AND_WAIT(_pAdapter, _offset, _writeValue, _busyMask, _waitDelay, _waitCount, _status)

#define HAL_GET_CHIP_ID_VER(_prAdapter, pu2ChipId, pu2Version) \
{ \
	UINT_32 u4Value; \
	HAL_MCR_RD(_prAdapter, HIF_SYS_REV, &u4Value); \
	*pu2ChipId = ((u4Value & PCIE_HIF_SYS_PROJ) >> 16); \
	*pu2Version = (u4Value & PCIE_HIF_SYS_REV); \
}

#define HAL_WIFI_FUNC_READY_CHECK(_prAdapter, _checkItem, _pfgResult) \
do { \
	struct mt66xx_chip_info *prChipInfo; \
	UINT_32 u4Value = 0; \
	if (!_prAdapter->chip_info) \
		ASSERT(0); \
	*_pfgResult = FALSE; \
	prChipInfo = _prAdapter->chip_info; \
	HAL_MCR_RD(_prAdapter, prChipInfo->sw_sync0, &u4Value); \
	if ((u4Value & (_checkItem << prChipInfo->sw_ready_bit_offset)) \
	     == (_checkItem << prChipInfo->sw_ready_bit_offset)) \
		*_pfgResult = TRUE; \
} while (0)

#define HAL_WIFI_FUNC_OFF_CHECK(_prAdapter, _checkItem, _pfgResult) \
do { \
	struct mt66xx_chip_info *prChipInfo; \
	UINT_32 u4Value; \
	BOOLEAN fgLpOwnResult; \
	if (!_prAdapter->chip_info) \
		ASSERT(0); \
	*_pfgResult = FALSE; \
	prChipInfo = _prAdapter->chip_info; \
	HAL_MCR_RD(_prAdapter, prChipInfo->sw_sync0, &u4Value); \
	if ((u4Value & (_checkItem << prChipInfo->sw_ready_bit_offset)) == 0) { \
		*_pfgResult = TRUE; \
	} else { \
		HAL_LP_OWN_RD(_prAdapter, &fgLpOwnResult); \
		if (fgLpOwnResult == FALSE) { \
			DBGLOG(INIT, INFO, "HAL_LP_OWN_RD %d\n", fgLpOwnResult); \
			HAL_LP_OWN_SET(prAdapter, &fgLpOwnResult); \
			DBGLOG(INIT, INFO, "HAL_LP_OWN_SET %d\n", fgLpOwnResult); \
		} \
	} \
} while (0)

#define HAL_WIFI_FUNC_GET_STATUS(_prAdapter, _u4Result) \
do { \
	struct mt66xx_chip_info *prChipInfo; \
	if (!_prAdapter->chip_info) \
		ASSERT(0); \
	prChipInfo = _prAdapter->chip_info; \
	HAL_MCR_RD(_prAdapter, prChipInfo->sw_sync0, &_u4Result); \
} while (0)

#define HAL_INTR_DISABLE(_prAdapter)

#define HAL_INTR_ENABLE(_prAdapter)

#define HAL_INTR_ENABLE_AND_LP_OWN_SET(_prAdapter)

/* Polling interrupt status bit due to CFG_PCIE_LPCR_HOST status bit not work when chip sleep */
#define HAL_LP_OWN_RD(_prAdapter, _pfgResult) \
{ \
	UINT_32 u4RegValue = 0; \
	*_pfgResult = FALSE; \
	HAL_MCR_RD(prAdapter, WPDMA_INT_STA, &u4RegValue); \
	*_pfgResult = HAL_IS_FW_OWNBACK_INTR(u4RegValue);\
}

#define HAL_LP_OWN_SET(_prAdapter, _pfgResult) \
{ \
	UINT_32 u4RegValue = 0; \
	*_pfgResult = FALSE; \
	HAL_MCR_WR(_prAdapter, CFG_PCIE_LPCR_HOST, PCIE_LPCR_HOST_SET_OWN); \
	HAL_MCR_RD(_prAdapter, CFG_PCIE_LPCR_HOST, &u4RegValue); \
	if (u4RegValue == 0) { \
		*_pfgResult = TRUE; \
	} \
}

#define HAL_LP_OWN_CLR(_prAdapter, _pfgResult) \
{ \
	UINT_32 u4RegValue = 0; \
	*_pfgResult = FALSE; \
	/* Software get LP ownership */ \
	HAL_MCR_WR(_prAdapter, \
			CFG_PCIE_LPCR_HOST, \
			PCIE_LPCR_HOST_CLR_OWN); \
	HAL_MCR_RD(_prAdapter, CFG_PCIE_LPCR_HOST, &u4RegValue); \
	if (u4RegValue == 0) { \
		*_pfgResult = TRUE; \
	} \
}

#define HAL_GET_ABNORMAL_INTERRUPT_REASON_CODE(_prAdapter, pu4AbnormalReason)

#define HAL_DISABLE_RX_ENHANCE_MODE(_prAdapter)

#define HAL_ENABLE_RX_ENHANCE_MODE(_prAdapter)

#define HAL_CFG_MAX_HIF_RX_LEN_NUM(_prAdapter, _ucNumOfRxLen)

#define HAL_SET_INTR_STATUS_READ_CLEAR(prAdapter)

#define HAL_SET_INTR_STATUS_WRITE_1_CLEAR(prAdapter)

#define HAL_READ_INTR_STATUS(prAdapter, length, pvBuf)

#define HAL_READ_TX_RELEASED_COUNT(_prAdapter, au2TxReleaseCount)

#define HAL_READ_RX_LENGTH(prAdapter, pu2Rx0Len, pu2Rx1Len)

#define HAL_GET_INTR_STATUS_FROM_ENHANCE_MODE_STRUCT(pvBuf, u2Len, pu4Status)

#define HAL_GET_TX_STATUS_FROM_ENHANCE_MODE_STRUCT(pvInBuf, pu4BufOut, u4LenBufOut)

#define HAL_GET_RX_LENGTH_FROM_ENHANCE_MODE_STRUCT(pvInBuf, pu2Rx0Num, au2Rx0Len, pu2Rx1Num, au2Rx1Len)

#define HAL_GET_MAILBOX_FROM_ENHANCE_MODE_STRUCT(pvInBuf, pu4Mailbox0, pu4Mailbox1)

#define HAL_IS_TX_DONE_INTR(u4IntrStatus) \
	((u4IntrStatus & (WPDMA_TX_DONE_INT0 | WPDMA_TX_DONE_INT1 | WPDMA_TX_DONE_INT2 | WPDMA_TX_DONE_INT3)) \
	? TRUE : FALSE)

#define HAL_IS_RX_DONE_INTR(u4IntrStatus) \
	((u4IntrStatus & (WPDMA_RX_DONE_INT0 | WPDMA_RX_DONE_INT1)) ? TRUE : FALSE)

#define HAL_IS_ABNORMAL_INTR(u4IntrStatus)

#define HAL_IS_FW_OWNBACK_INTR(u4IntrStatus) \
	((u4IntrStatus & WPDMA_FW_CLR_OWN_INT) ? TRUE : FALSE)

#define HAL_PUT_MAILBOX(prAdapter, u4MboxId, u4Data)

#define HAL_GET_MAILBOX(prAdapter, u4MboxId, pu4Data)

#define HAL_SET_MAILBOX_READ_CLEAR(prAdapter, fgEnableReadClear) \
{ \
	prAdapter->prGlueInfo->rHifInfo.fgMbxReadClear = fgEnableReadClear;\
}

#define HAL_GET_MAILBOX_READ_CLEAR(prAdapter) (prAdapter->prGlueInfo->rHifInfo.fgMbxReadClear)

#define HAL_READ_INT_STATUS(prAdapter, _pu4IntStatus) \
{ \
	kalDevReadIntStatus(prAdapter, _pu4IntStatus);\
}

#define HAL_HIF_INIT(prAdapter)

#define HAL_ENABLE_FWDL(_prAdapter, _fgEnable)

#define HAL_WAKE_UP_WIFI(_prAdapter) \
{ \
	halWakeUpWiFi(_prAdapter);\
}

#define HAL_CLEAR_DUMMY_REQ(_prAdapter) \
{ \
	struct mt66xx_chip_info *prChipInfo; \
	UINT_32 u4Value; \
	if (!_prAdapter->chip_info) {\
		ASSERT(0); \
	} else {\
		prChipInfo = _prAdapter->chip_info; \
		HAL_MCR_RD(_prAdapter, prChipInfo->sw_sync0, &u4Value); \
		u4Value &= ~(WIFI_FUNC_DUMMY_REQ << prChipInfo->sw_ready_bit_offset);\
		HAL_MCR_WR(prAdapter, prChipInfo->sw_sync0, u4Value);\
	} \
}

#define HAL_IS_TX_DIRECT(_prAdapter) FALSE

#define HAL_IS_RX_DIRECT(_prAdapter) FALSE

#endif

#if defined(_HIF_SDIO)
#define HIF_H2D_SW_INT_SHFT                 (16)
#define SDIO_MAILBOX_FUNC_READ_REG_IDX      (BIT(0) << HIF_H2D_SW_INT_SHFT) /* bit16 */
#define SDIO_MAILBOX_FUNC_WRITE_REG_IDX     (BIT(1) << HIF_H2D_SW_INT_SHFT) /* bit17 */
#define SDIO_MAILBOX_FUNC_CHECKSUN16_IDX    (BIT(2) << HIF_H2D_SW_INT_SHFT) /* bit18 */

#define HAL_READ_RX_PORT(prAdapter, u4PortId, u4Len, pvBuf, _u4ValidBufSize) \
{ \
	ASSERT(u4PortId < 2); \
	HAL_PORT_RD(prAdapter, \
		((u4PortId == 0) ? MCR_WRDR0 : MCR_WRDR1), \
		u4Len, \
		pvBuf, \
		_u4ValidBufSize/*temp!!*//*4Kbyte*/) \
}

#define HAL_WRITE_TX_PORT(_prAdapter, _u4PortId, _u4Len, _pucBuf, _u4ValidBufSize) \
{ \
	if ((_u4ValidBufSize - _u4Len) >= sizeof(UINT_32)) { \
		/* fill with single dword of zero as TX-aggregation termination */ \
		*(PUINT_32) (&((_pucBuf)[ALIGN_4(_u4Len)])) = 0; \
	} \
	HAL_PORT_WR(_prAdapter, \
		MCR_WTDR1, \
		_u4Len, \
		_pucBuf, \
		_u4ValidBufSize/*temp!!*//*4KByte*/) \
}

/* The macro to read the given MCR several times to check if the wait
 *  condition come true.
 */
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
 *  command is done
 */
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

#define HAL_WIFI_FUNC_READY_CHECK(_prAdapter, _checkItem, _pfgResult) \
do { \
	UINT_32 u4Value = 0; \
	*_pfgResult = FALSE; \
	HAL_MCR_RD(_prAdapter, \
		MCR_WCIR, \
		&u4Value); \
	if (u4Value & WCIR_WLAN_READY) { \
		*_pfgResult = TRUE; \
	} \
} while (0)

#define HAL_WIFI_FUNC_OFF_CHECK(_prAdapter, _checkItem, _pfgResult) \
do { \
	UINT_32 u4Value = 0; \
	*_pfgResult = FALSE; \
	HAL_MCR_RD(_prAdapter, MCR_WCIR, &u4Value); \
	if ((u4Value & WCIR_WLAN_READY) == 0) { \
		*_pfgResult = TRUE; \
	} \
	halPrintMailbox(prAdapter);\
	halPollDbgCr(_prAdapter, LP_DBGCR_POLL_ROUND); \
} while (0)

#define HAL_WIFI_FUNC_GET_STATUS(_prAdapter, _u4Result) \
	halGetMailbox(_prAdapter, 0, &_u4Result)

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

#define HAL_LP_OWN_RD(_prAdapter, _pfgResult) \
{ \
	UINT_32 u4RegValue = 0; \
	*_pfgResult = FALSE; \
	HAL_MCR_RD(_prAdapter, MCR_WHLPCR, &u4RegValue); \
	if (u4RegValue & WHLPCR_IS_DRIVER_OWN) { \
		*_pfgResult = TRUE; \
	} \
}

#define HAL_LP_OWN_SET(_prAdapter, _pfgResult) \
{ \
	UINT_32 u4RegValue = 0; \
	*_pfgResult = FALSE; \
	HAL_MCR_WR(_prAdapter, \
		MCR_WHLPCR, \
		WHLPCR_FW_OWN_REQ_SET); \
	HAL_MCR_RD(_prAdapter, MCR_WHLPCR, &u4RegValue); \
	if (u4RegValue & WHLPCR_IS_DRIVER_OWN) { \
		*_pfgResult = TRUE; \
	} \
}

#define HAL_LP_OWN_CLR(_prAdapter, _pfgResult) \
{ \
	UINT_32 u4RegValue = 0; \
	*_pfgResult = FALSE; \
	/* Software get LP ownership */ \
	HAL_MCR_WR(_prAdapter, \
			MCR_WHLPCR, \
			WHLPCR_FW_OWN_REQ_CLR); \
	HAL_MCR_RD(_prAdapter, MCR_WHLPCR, &u4RegValue); \
	if (u4RegValue & WHLPCR_IS_DRIVER_OWN) { \
		*_pfgResult = TRUE; \
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
*	 if the length of the buffer is long enough
*/
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

#define HAL_READ_INT_STATUS(prAdapter, _pu4IntStatus) \
{ \
	kalDevReadIntStatus(prAdapter, _pu4IntStatus);\
}

#define HAL_HIF_INIT(_prAdapter) \
{ \
	halDevInit(_prAdapter);\
}

#define HAL_ENABLE_FWDL(_prAdapter, _fgEnable)

#define HAL_WAKE_UP_WIFI(_prAdapter) \
{ \
	halWakeUpWiFi(_prAdapter);\
}

#define HAL_IS_TX_DIRECT(_prAdapter) FALSE

#define HAL_IS_RX_DIRECT(_prAdapter) FALSE

#endif

#if defined(_HIF_USB)
#define HAL_GET_ABNORMAL_INTERRUPT_REASON_CODE(_prAdapter, pu4AbnormalReason)

#define HAL_DISABLE_RX_ENHANCE_MODE(_prAdapter)

#define HAL_ENABLE_RX_ENHANCE_MODE(_prAdapter)

#define HAL_CFG_MAX_HIF_RX_LEN_NUM(_prAdapter, _ucNumOfRxLen)

#define HAL_SET_INTR_STATUS_READ_CLEAR(prAdapter)

#define HAL_SET_INTR_STATUS_WRITE_1_CLEAR(prAdapter)

#define HAL_READ_INTR_STATUS(prAdapter, length, pvBuf)

#define HAL_READ_TX_RELEASED_COUNT(_prAdapter, au2TxReleaseCount)

#define HAL_READ_RX_LENGTH(prAdapter, pu2Rx0Len, pu2Rx1Len)

#define HAL_GET_INTR_STATUS_FROM_ENHANCE_MODE_STRUCT(pvBuf, u2Len, pu4Status)

#define HAL_GET_TX_STATUS_FROM_ENHANCE_MODE_STRUCT(pvInBuf, pu4BufOut, u4LenBufOut)

#define HAL_GET_RX_LENGTH_FROM_ENHANCE_MODE_STRUCT(pvInBuf, pu2Rx0Num, au2Rx0Len, pu2Rx1Num, au2Rx1Len)

#define HAL_GET_MAILBOX_FROM_ENHANCE_MODE_STRUCT(pvInBuf, pu4Mailbox0, pu4Mailbox1)

#define HAL_READ_TX_RELEASED_COUNT(_prAdapter, au2TxReleaseCount)

#define HAL_IS_TX_DONE_INTR(u4IntrStatus) FALSE

#define HAL_IS_RX_DONE_INTR(u4IntrStatus)

#define HAL_IS_ABNORMAL_INTR(u4IntrStatus)

#define HAL_IS_FW_OWNBACK_INTR(u4IntrStatus)

#define HAL_PUT_MAILBOX(prAdapter, u4MboxId, u4Data)

#define HAL_GET_MAILBOX(prAdapter, u4MboxId, pu4Data) TRUE

#define HAL_SET_MAILBOX_READ_CLEAR(prAdapter, fgEnableReadClear)

#define HAL_GET_MAILBOX_READ_CLEAR(prAdapter) TRUE

#define HAL_WIFI_FUNC_POWER_ON(_prAdapter) \
	mtk_usb_vendor_request(_prAdapter->prGlueInfo, 0, DEVICE_VENDOR_REQUEST_OUT, \
			       VND_REQ_POWER_ON_WIFI, 0, 1, NULL, 0)

#define HAL_WIFI_FUNC_CHIP_RESET(_prAdapter) \
	mtk_usb_vendor_request(_prAdapter->prGlueInfo, 0, DEVICE_VENDOR_REQUEST_OUT, \
			       VND_REQ_POWER_ON_WIFI, 1, 1, NULL, 0)

#define HAL_WIFI_FUNC_READY_CHECK(_prAdapter, _checkItem, _pfgResult) \
do { \
	struct mt66xx_chip_info *prChipInfo; \
	UINT_32 u4Value = 0; \
	if (!_prAdapter->chip_info) \
		ASSERT(0); \
	*_pfgResult = FALSE; \
	prChipInfo = _prAdapter->chip_info; \
	HAL_MCR_RD(_prAdapter, prChipInfo->sw_sync0, &u4Value); \
	if ((u4Value & (_checkItem << prChipInfo->sw_ready_bit_offset)) \
	     == (_checkItem << prChipInfo->sw_ready_bit_offset)) \
		*_pfgResult = TRUE; \
} while (0)

#define HAL_WIFI_FUNC_OFF_CHECK(_prAdapter, _checkItem, _pfgResult) \
do { \
	struct mt66xx_chip_info *prChipInfo; \
	UINT_32 u4Value = 0; \
	if (!_prAdapter->chip_info) \
		ASSERT(0); \
	*_pfgResult = FALSE; \
	prChipInfo = _prAdapter->chip_info; \
	HAL_MCR_RD(_prAdapter, prChipInfo->sw_sync0, &u4Value); \
	if ((u4Value & (_checkItem << prChipInfo->sw_ready_bit_offset)) == 0) \
		*_pfgResult = TRUE; \
} while (0)

#define HAL_WIFI_FUNC_GET_STATUS(_prAdapter, _u4Result) \
do { \
	struct mt66xx_chip_info *prChipInfo; \
	if (!_prAdapter->chip_info) \
		ASSERT(0); \
	prChipInfo = _prAdapter->chip_info; \
	HAL_MCR_RD(_prAdapter, prChipInfo->sw_sync0, &_u4Result); \
} while (0)

#define HAL_INTR_DISABLE(_prAdapter)

#define HAL_INTR_ENABLE(_prAdapter)

#define HAL_INTR_ENABLE_AND_LP_OWN_SET(_prAdapter)

#define HAL_READ_INT_STATUS(prAdapter, _pu4IntStatus) \
{ \
	halGetCompleteStatus(prAdapter, _pu4IntStatus); \
}

#define HAL_HIF_INIT(_prAdapter) \
{ \
	halDevInit(_prAdapter);\
}

#define HAL_ENABLE_FWDL(_prAdapter, _fgEnable) \
{ \
	halEnableFWDownload(_prAdapter, _fgEnable);\
}

#define HAL_WAKE_UP_WIFI(_prAdapter) \
{ \
	halWakeUpWiFi(_prAdapter);\
}

#define HAL_LP_OWN_RD(_prAdapter, _pfgResult)

#define HAL_LP_OWN_SET(_prAdapter, _pfgResult)

#define HAL_LP_OWN_CLR(_prAdapter, _pfgResult)

#define HAL_WRITE_TX_PORT(_prAdapter, _u4PortId, _u4Len, _pucBuf, _u4ValidBufSize) \
{ \
	HAL_PORT_WR(_prAdapter, \
		_u4PortId, \
		_u4Len, \
		_pucBuf, \
		_u4ValidBufSize/*temp!!*//*4KByte*/) \
}

#define HAL_IS_TX_DIRECT(_prAdapter) \
	((CFG_TX_DIRECT_USB) ? TRUE : FALSE)

#define HAL_IS_RX_DIRECT(_prAdapter) \
	((CFG_RX_DIRECT_USB) ? TRUE : FALSE)

#endif

#define INVALID_VERSION 0xFFFF /* used by HW/FW version */
/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

BOOL halVerifyChipID(IN P_ADAPTER_T prAdapter);
UINT_32 halGetChipHwVer(IN P_ADAPTER_T prAdapter);
UINT_32 halGetChipSwVer(IN P_ADAPTER_T prAdapter);

WLAN_STATUS halRxWaitResponse(IN P_ADAPTER_T prAdapter, IN UINT_8 ucPortIdx, OUT PUINT_8 pucRspBuffer,
	IN UINT_32 u4MaxRespBufferLen, OUT PUINT_32 pu4Length);

VOID halEnableInterrupt(IN P_ADAPTER_T prAdapter);
VOID halDisableInterrupt(IN P_ADAPTER_T prAdapter);

BOOLEAN halSetDriverOwn(IN P_ADAPTER_T prAdapter);
VOID halSetFWOwn(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgEnableGlobalInt);

VOID halDevInit(IN P_ADAPTER_T prAdapter);
VOID halEnableFWDownload(IN P_ADAPTER_T prAdapter, IN BOOL fgEnable);
VOID halWakeUpWiFi(IN P_ADAPTER_T prAdapter);
VOID halTxCancelSendingCmd(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo);
VOID halTxCancelAllSending(IN P_ADAPTER_T prAdapter);
BOOLEAN halTxIsDataBufEnough(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo);
VOID halProcessTxInterrupt(IN P_ADAPTER_T prAdapter);
WLAN_STATUS halTxPollingResource(IN P_ADAPTER_T prAdapter, IN UINT_8 ucTC);
VOID halSerHifReset(IN P_ADAPTER_T prAdapter);

VOID halProcessRxInterrupt(IN P_ADAPTER_T prAdapter);
VOID halProcessSoftwareInterrupt(IN P_ADAPTER_T prAdapter);
/* Hif power off wifi */
WLAN_STATUS halHifPowerOffWifi(IN P_ADAPTER_T prAdapter);


VOID halHifSwInfoInit(IN P_ADAPTER_T prAdapter);
VOID halRxProcessMsduReport(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb);
UINT_32 halTxGetPageCount(IN UINT_32 u4FrameLength, IN BOOLEAN fgIncludeDesc);
UINT_32 halDumpHifStatus(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucBuf, IN UINT_32 u4Max);
BOOLEAN halIsPendingRx(IN P_ADAPTER_T prAdapter);
UINT_32 halGetValidCoalescingBufSize(IN P_ADAPTER_T prAdapter);
WLAN_STATUS halAllocateIOBuffer(IN P_ADAPTER_T prAdapter);
WLAN_STATUS halReleaseIOBuffer(IN P_ADAPTER_T prAdapter);
VOID halDeAggRxPktWorker(struct work_struct *work);
VOID halRxTasklet(unsigned long data);
VOID halTxCompleteTasklet(unsigned long data);
VOID halPrintHifDbgInfo(IN P_ADAPTER_T prAdapter);
BOOLEAN halIsTxResourceControlEn(IN P_ADAPTER_T prAdapter);
VOID halTxResourceResetHwTQCounter(IN P_ADAPTER_T prAdapter);

VOID halWpdmaSetup(P_GLUE_INFO_T prGlueInfo, BOOLEAN enable);
#endif /* _HAL_H */
