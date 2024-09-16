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

/*! \file   nic_connac2x_rx.h
*    \brief  Functions that provide TX operation in NIC's point of view.
*
*    This file provides TX functions which are responsible for both Hardware and
*    Software Resource Management and keep their Synchronization.
*
*/


#ifndef _NIC_CONNAC2X_RX_H
#define _NIC_CONNAC2X_RX_H

#if (CFG_SUPPORT_CONNAC2X == 1)
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
#define CONNAC2X_RX_STATUS_PKT_TYPE_SW_BITMAP 0x380F
#define CONNAC2X_RX_STATUS_PKT_TYPE_SW_EVENT  0x3800
#define CONNAC2X_RX_STATUS_PKT_TYPE_SW_FRAME  0x3801

/*------------------------------------------------------------------------------
 * Bit fields for HW_MAC_RX_DESC_T
 *------------------------------------------------------------------------------
*/

/*! MAC CONNAC2X RX DMA Descriptor */
/* DW 0*/
#define CONNAC2X_RX_STATUS_RX_BYTE_COUNT_MASK    BITS(0, 15)
#define CONNAC2X_RX_STATUS_RX_BYTE_COUNT_OFFSET  0
#define CONNAC2X_RX_STATUS_ETH_TYPE_MASK         BITS(16, 22)
#define CONNAC2X_RX_STATUS_ETH_TYPE_OFFSET       16
#define CONNAC2X_RX_STATUS_IP_CHKSUM             BIT(23)
#define CONNAC2X_RX_STATUS_UDP_TCP_CHKSUM        BIT(24)
#define CONNAC2X_RX_STATUS_DW0_HW_INFO_MASK      BITS(25, 26)
#define CONNAC2X_RX_STATUS_DW0_HW_INFO_OFFSET    25
#define CONNAC2X_RX_STATUS_PKT_TYPE_MASK         BITS(27, 31)
#define CONNAC2X_RX_STATUS_PKT_TYPE_OFFSET       27

/* DW 1 */
#define CONNAC2X_RX_STATUS_WLAN_INDEX_MASK       BITS(0, 9)
#define CONNAC2X_RX_STATUS_WLAN_INDEX_OFFSET     0
#define CONNAC2X_RX_STATUS_GROUP_VLD_MASK        BITS(11, 15)
#define CONNAC2X_RX_STATUS_GROUP_VLD_OFFSET      11
#define CONNAC2X_RX_STATUS_SEC_MASK              BITS(16, 20)
#define CONNAC2X_RX_STATUS_SEC_OFFSET            16
#define CONNAC2X_RX_STATUS_KEYID_MASK            BITS(21, 22)
#define CONNAC2X_RX_STATUS_KEYID_OFFSET          21
#define CONNAC2X_RX_STATUS_FLAG_CIPHER_MISMATCH  BIT(23)
#define CONNAC2X_RX_STATUS_FLAG_CIPHER_LENGTH_MISMATCH     BIT(24)
#define CONNAC2X_RX_STATUS_FLAG_ICV_ERROR        BIT(25)
#define CONNAC2X_RX_STATUS_FLAG_TKIPMIC_ERROR    BIT(26)
#define CONNAC2X_RX_STATUS_FLAG_FCS_ERROR        BIT(27)
#define CONNAC2X_RX_STATUS_DW1_HW_INFO_MASK     BITS(29, 31)
#define CONNAC2X_RX_STATUS_DW1_HW_INFO_OFFSET   29
#define CONNAC2X_RX_STATUS_DW1_BAND_MASK        BIT(28)
#define CONNAC2X_RX_STATUS_DW1_BAND_OFFSET      28


#define CONNAC2X_RX_STATUS_FLAG_ERROR_MASK  (CONNAC2X_RX_STATUS_FLAG_FCS_ERROR\
	| CONNAC2X_RX_STATUS_FLAG_ICV_ERROR \
	| CONNAC2X_RX_STATUS_FLAG_CIPHER_LENGTH_MISMATCH)/* No TKIP MIC error */

/* DW 2 */
#define CONNAC2X_RX_STATUS_BSSID_MASK            BITS(0, 5)
#define CONNAC2X_RX_STATUS_BSSID_OFFSET          0
#define CONNAC2X_RX_STATUS_FLAG_BF_CQI           BIT(7)
#define CONNAC2X_RX_STATUS_HEADER_LEN_MASK       BITS(8, 12)
#define CONNAC2X_RX_STATUS_HEADER_LEN_OFFSET     8
#define CONNAC2X_RX_STATUS_FLAG_HEADER_TRAN      BIT(13)
#define CONNAC2X_RX_STATUS_HEADER_OFFSET_MASK    BITS(14, 15)
#define CONNAC2X_RX_STATUS_HEADER_OFFSET_OFFSET  14
#define CONNAC2X_RX_STATUS_TID_MASK              BITS(16, 19)
#define CONNAC2X_RX_STATUS_TID_OFFSET            16
#define CONNAC2X_RX_STATUS_FLAG_SW_BIT           BIT(22)
#define CONNAC2X_RX_STATUS_FLAG_DE_AMSDU_FAIL    BIT(23)
#define CONNAC2X_RX_STATUS_FLAG_EXCEED_LEN       BIT(24)
#define CONNAC2X_RX_STATUS_FLAG_TRANS_FAIL       BIT(25)
#define CONNAC2X_RX_STATUS_FLAG_INTF             BIT(26)
#define CONNAC2X_RX_STATUS_FLAG_FRAG             BIT(27)
#define CONNAC2X_RX_STATUS_FLAG_NULL             BIT(28)
#define CONNAC2X_RX_STATUS_FLAG_NDATA            BIT(29)
#define CONNAC2X_RX_STATUS_FLAG_NAMP             BIT(30)
#define CONNAC2X_RX_STATUS_FLAG_BF_RPT           BIT(31)

/* DW 3 */
#define CONNAC2X_RX_STATUS_RXV_SEQ_NO_MASK       BITS(0, 7)
#define CONNAC2X_RX_STATUS_RXV_SEQ_NO_OFFSET     0
#define CONNAC2X_RX_STATUS_CH_FREQ_MASK          BITS(8, 15)
#define CONNAC2X_RX_STATUS_CH_FREQ_OFFSET        8
#define CONNAC2X_RX_STATUS_A1_TYPE_MASK          BITS(16, 17)
#define CONNAC2X_RX_STATUS_A1_TYPE_OFFSET        16
#define CONNAC2X_RX_STATUS_UC2ME                 BIT(16)
#define CONNAC2X_RX_STATUS_MC_FRAME              BIT(17)
#define CONNAC2X_RX_STATUS_BC_FRAME              BITS(16, 17)

#define CONNAC2X_RX_STATUS_FLAG_HTC              BIT(18)
#define CONNAC2X_RX_STATUS_FLAG_TCL              BIT(19)
#define CONNAC2X_RX_STATUS_FLAG_BBM              BIT(20)
#define CONNAC2X_RX_STATUS_FLAG_BU               BIT(21)
#define CONNAC2X_RX_STATUS_DW3_HW_INFO_MASK      BITS(22, 31)
#define CONNAC2X_RX_STATUS_DW3_HW_INFO_OFFSET    22

/* DW 4 */
#define CONNAC2X_RX_STATUS_PF_MASK               BITS(0, 1)
#define CONNAC2X_RX_STATUS_PF_OFFSET             0
#define CONNAC2X_RX_STATUS_FLAG_DP               BIT(9)
#define CONNAC2X_RX_STATUS_FLAG_CLS              BIT(10)
#define CONNAC2X_RX_STATUS_OFLD_MASK             BITS(11, 12)
#define CONNAC2X_RX_STATUS_OFLD_OFFSET           11
#define CONNAC2X_RX_STATUS_FLAG_MGC              BIT(13)
#define CONNAC2X_RX_STATUS_WOL_MASK              BITS(14, 18)
#define CONNAC2X_RX_STATUS_WOL_OFFSET            14
#define CONNAC2X_RX_STATUS_CLS_BITMAP_MASK       BITS(19, 28)
#define CONNAC2X_RX_STATUS_CLS_BITMAP_OFFSET     19
#define CONNAC2X_RX_STATUS_FLAG_PF_MODE          BIT(29)
#define CONNAC2X_RX_STATUS_PF_STS_MASK           BITS(30, 31)
#define CONNAC2X_RX_STATUS_PF_STS_OFFSET         30

/* DW 5 */
#define CONNAC2X_RX_STATUS_DW5_CLS_BITMAP_MASK      BITS(0, 9)
#define CONNAC2X_RX_STATUS_DW5_CLS_BITMAP_OFFSET    0
#define CONNAC2X_RX_STATUS_MAC_MASK             BIT(31)
#define CONNAC2X_RX_STATUS_MAC_OFFSET           31


/*
 *   GROUP_VLD: RFB Group valid indicators
 *   Bit[0] indicates GROUP1 (DW10~DW13)
 *   Bit[1] indicates GROUP2 (DW14~DW15)
 *   Bit[2] indicates GROUP3 (DW16~DW17)
 *   Bit[3] indicates GROUP4 (DW6~DW9)
 *   Bit[4] indicates GROUP5 (DW18~DW33)
 */
#define CONNAC2X_RX_STATUS_GROUP1_VALID    BIT(0)
#define CONNAC2X_RX_STATUS_GROUP2_VALID    BIT(1)
#define CONNAC2X_RX_STATUS_GROUP3_VALID    BIT(2)
#define CONNAC2X_RX_STATUS_GROUP4_VALID    BIT(3)
#define CONNAC2X_RX_STATUS_GROUP5_VALID    BIT(4)

#define CONNAC2X_RX_STATUS_FIXED_LEN       24

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*! A data structure which is identical with MAC RX DMA Descriptor */
struct HW_MAC_CONNAC2X_RX_DESC {
	uint32_t u4DW0;
	uint32_t u4DW1;
	uint32_t u4DW2;
	uint32_t u4DW3;
	uint32_t u4DW4;
	uint32_t u4DW5;
};

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

#define RX_DESC_GET_FIELD(_rHwMacTxDescField, _mask, _offset) \
	(((_rHwMacTxDescField) & (_mask)) >> (_offset))

/*------------------------------------------------------------------------------
 * MACRO for HW_MAC_RX_DESC_T
 *------------------------------------------------------------------------------
*/

#define HAL_MAC_CONNAC2X_RX_STATUS_GET_RX_BYTE_CNT(_prHwMacRxDesc) \
	RX_DESC_GET_FIELD((_prHwMacRxDesc)->u4DW0, \
		CONNAC2X_RX_STATUS_RX_BYTE_COUNT_MASK, \
		CONNAC2X_RX_STATUS_RX_BYTE_COUNT_OFFSET)

#define HAL_MAC_CONNAC2X_RX_STATUS_GET_ETH_TYPE_OFFSET(_prHwMacRxDesc)	\
RX_DESC_GET_FIELD((_prHwMacRxDesc)->u4DW0, \
CONNAC2X_RX_STATUS_ETH_TYPE_MASK, CONNAC2X_RX_STATUS_ETH_TYPE_OFFSET)

#define HAL_MAC_CONNAC2X_RX_STATUS_GET_PKT_TYPE(_prHwMacRxDesc) \
RX_DESC_GET_FIELD((_prHwMacRxDesc)->u4DW0, \
CONNAC2X_RX_STATUS_PKT_TYPE_MASK, CONNAC2X_RX_STATUS_PKT_TYPE_OFFSET)

#define HAL_MAC_CONNAC2X_RX_STATUS_GET_WLAN_IDX(_prHwMacRxDesc) \
RX_DESC_GET_FIELD((_prHwMacRxDesc)->u4DW1, \
CONNAC2X_RX_STATUS_WLAN_INDEX_MASK, CONNAC2X_RX_STATUS_WLAN_INDEX_OFFSET)

#define HAL_MAC_CONNAC2X_RX_STATUS_GET_GROUP_VLD(_prHwMacRxDesc) \
RX_DESC_GET_FIELD((_prHwMacRxDesc)->u4DW1, \
CONNAC2X_RX_STATUS_GROUP_VLD_MASK, CONNAC2X_RX_STATUS_GROUP_VLD_OFFSET)

#define HAL_MAC_CONNAC2X_RX_STATUS_GET_SEC_MODE(_prHwMacRxDesc) \
RX_DESC_GET_FIELD((_prHwMacRxDesc)->u4DW1, \
CONNAC2X_RX_STATUS_SEC_MASK, CONNAC2X_RX_STATUS_SEC_OFFSET)

#define HAL_MAC_CONNAC2X_RX_STATUS_GET_KID(_prHwMacRxDesc) \
RX_DESC_GET_FIELD((_prHwMacRxDesc)->u4DW1, \
CONNAC2X_RX_STATUS_KEYID_MASK, CONNAC2X_RX_STATUS_KEYID_OFFSET)

#define HAL_MAC_CONNAC2X_RX_STATUS_IS_CIPHER_MISMATCH(_prHwMacRxDesc)	\
(((_prHwMacRxDesc)->u4DW1 & CONNAC2X_RX_STATUS_FLAG_CIPHER_MISMATCH) \
	? TRUE : FALSE)
#define HAL_MAC_CONNAC2X_RX_STATUS_IS_CLM_ERROR(_prHwMacRxDesc)	\
(((_prHwMacRxDesc)->u4DW1 & CONNAC2X_RX_STATUS_FLAG_CIPHER_LENGTH_MISMATCH) \
	? TRUE : FALSE)
#define HAL_MAC_CONNAC2X_RX_STATUS_IS_ICV_ERROR(_prHwMacRxDesc) \
(((_prHwMacRxDesc)->u4DW1 & CONNAC2X_RX_STATUS_FLAG_ICV_ERROR)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_RX_STATUS_IS_TKIP_MIC_ERROR(_prHwMacRxDesc) \
(((_prHwMacRxDesc)->u4DW1 & CONNAC2X_RX_STATUS_FLAG_TKIPMIC_ERROR)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_RX_STATUS_IS_FCS_ERROR(_prHwMacRxDesc) \
(((_prHwMacRxDesc)->u4DW1 & CONNAC2X_RX_STATUS_FLAG_FCS_ERROR)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_RX_STATUS_IS_ERROR(_prHwMacRxDesc) \
(((_prHwMacRxDesc)->u4DW1 & CONNAC2X_RX_STATUS_FLAG_ERROR_MASK)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_RX_STATUS_GET_HEADER_OFFSET(_prHwMacRxDesc) \
((RX_DESC_GET_FIELD((_prHwMacRxDesc)->u4DW2, \
CONNAC2X_RX_STATUS_HEADER_OFFSET_MASK, \
CONNAC2X_RX_STATUS_HEADER_OFFSET_OFFSET)) << 1)

#define HAL_MAC_CONNAC2X_RX_STATUS_GET_HEADER_LEN(_prHwMacRxDesc)	\
(RX_DESC_GET_FIELD((_prHwMacRxDesc)->u4DW2, \
CONNAC2X_RX_STATUS_HEADER_LEN_MASK, CONNAC2X_RX_STATUS_HEADER_LEN_OFFSET) << 1)

#define HAL_MAC_CONNAC2X_RX_STATUS_IS_HEADER_TRAN(_prHwMacRxDesc)	\
(((_prHwMacRxDesc)->u4DW2 & CONNAC2X_RX_STATUS_FLAG_HEADER_TRAN)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_RX_STATUS_IS_DAF(_prHwMacRxDesc) \
(((_prHwMacRxDesc)->u4DW2 & CONNAC2X_RX_STATUS_FLAG_DE_AMSDU_FAIL)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_RX_STATUS_IS_FRAG(_prHwMacRxDesc) \
(((_prHwMacRxDesc)->u4DW2 & CONNAC2X_RX_STATUS_FLAG_FRAG)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_RX_STATUS_IS_NDATA(_prHwMacRxDesc) \
(((_prHwMacRxDesc)->u4DW2 & CONNAC2X_RX_STATUS_FLAG_NDATA)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_RX_STATUS_IS_NAMP(_prHwMacRxDesc) \
(((_prHwMacRxDesc)->u4DW2 & CONNAC2X_RX_STATUS_FLAG_NAMP)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_RX_STATUS_IS_SW_DEFINE_RX_CLASSERR(_prHwMacRxDesc) \
(((_prHwMacRxDesc)->u4DW2 & CONNAC2X_RX_STATUS_FLAG_SW_BIT)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_RX_STATUS_IS_LLC_MIS(_prHwMacRxDesc) \
(((_prHwMacRxDesc)->u4DW2 & CONNAC2X_RX_STATUS_FLAG_TRANS_FAIL)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_RX_STATUS_GET_TID(_prHwMacRxDesc) \
RX_DESC_GET_FIELD((_prHwMacRxDesc)->u4DW2, \
CONNAC2X_RX_STATUS_TID_MASK, CONNAC2X_RX_STATUS_TID_OFFSET)

#define HAL_MAC_CONNAC2X_RX_STATUS_GET_RXV_SEQ_NO(_prHwMacRxDesc) \
RX_DESC_GET_FIELD((_prHwMacRxDesc)->u4DW3, \
CONNAC2X_RX_STATUS_RXV_SEQ_NO_MASK, CONNAC2X_RX_STATUS_RXV_SEQ_NO_OFFSET)

#define HAL_MAC_CONNAC2X_RX_STATUS_IS_UC2ME(_prHwMacRxDesc) \
(((_prHwMacRxDesc)->u4DW3 & CONNAC2X_RX_STATUS_UC2ME)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_RX_STATUS_IS_MC(_prHwMacRxDesc) \
(((_prHwMacRxDesc)->u4DW3 & CONNAC2X_RX_STATUS_MC_FRAME)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_RX_STATUS_IS_BC(_prHwMacRxDesc) \
(((_prHwMacRxDesc)->u4DW3 & CONNAC2X_RX_STATUS_BC_FRAME) \
	== CONNAC2X_RX_STATUS_BC_FRAME?TRUE:FALSE)

#define HAL_RX_CONNAC2X_STATUS_GET_PAYLOAD_FORMAT(_prHwMacRxDesc) \
RX_DESC_GET_FIELD((_prHwMacRxDesc)->u4DW4, \
CONNAC2X_RX_STATUS_PF_MASK, CONNAC2X_RX_STATUS_PF_OFFSET)

#define HAL_MAC_CONNAC2X_RX_STATUS_GET_RF_BAND(_prHwMacRxDesc) \
(((RX_DESC_GET_FIELD((_prHwMacRxDesc)->u4DW3, \
CONNAC2X_RX_STATUS_CH_FREQ_MASK, CONNAC2X_RX_STATUS_CH_FREQ_OFFSET)) \
<= HW_CHNL_NUM_MAX_2G4) ? BAND_2G4 : BAND_5G)

#define HAL_MAC_CONNAC2X_RX_STATUS_GET_CHNL_NUM(_prHwMacRxDesc) \
(RX_DESC_GET_FIELD((_prHwMacRxDesc)->u4DW3, \
CONNAC2X_RX_STATUS_CH_FREQ_MASK, CONNAC2X_RX_STATUS_CH_FREQ_OFFSET))

#define HAL_MAC_CONNAC2X_RX_STATUS_GET_TCL(_prHwMacRxDesc)	\
(((_prHwMacRxDesc)->u4DW3 & CONNAC2X_RX_STATUS_FLAG_TCL)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_RX_STATUS_GET_PAYLOAD_FORMAT(_prHwMacRxDesc) \
RX_DESC_GET_FIELD((_prHwMacRxDesc)->u4DW4, \
CONNAC2X_RX_STATUS_PF_MASK, CONNAC2X_RX_STATUS_PF_OFFSET)

#define HAL_MAC_CONNAC2X_RX_STATUS_GET_OFLD(_prHwMacRxDesc)	\
(((_prHwMacRxDesc)->u4DW4 & CONNAC2X_RX_STATUS_OFLD_MASK) >> \
CONNAC2X_RX_STATUS_OFLD_OFFSET)

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/


/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* CFG_SUPPORT_CONNAC2X == 1 */
#endif /* _NIC_CONNAC2X_RX_H */

