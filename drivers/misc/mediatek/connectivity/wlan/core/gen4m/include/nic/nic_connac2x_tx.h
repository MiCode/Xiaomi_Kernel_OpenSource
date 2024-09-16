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
/*! \file   nic_connac2x_tx.h
*    \brief  Functions that provide TX operation in NIC's point of view.
*
*    This file provides TX functions which are responsible for both Hardware and
*    Software Resource Management and keep their Synchronization.
*
*/


#ifndef _NIC_CONNAC2X_TX_H
#define _NIC_CONNAC2X_TX_H

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
#define NORMAL_GI 0
#define SHORT_GI  1
#define CONNAC2X_TX_DESC_LIFE_TIME_UNIT_IN_POWER_OF_2    6

/*------------------------------------------------------------------------*/
/* Tx descriptor field related information                                */
/*------------------------------------------------------------------------*/
/* DW 0 */
#define CONNAC2X_TX_DESC_TX_BYTE_COUNT_MASK              BITS(0, 15)
#define CONNAC2X_TX_DESC_TX_BYTE_COUNT_OFFSET            0

#define CONNAC2X_TX_DESC_ETHER_TYPE_OFFSET_MASK          BITS(16, 22)
#define CONNAC2X_TX_DESC_ETHER_TYPE_OFFSET_OFFSET        16

#define CONNAC2X_TX_DESC_PACKET_FORMAT_MASK              BITS(23, 24)
#define CONNAC2X_TX_DESC_PACKET_FORMAT_OFFSET            23

#define CONNAC2X_TX_DESC_QUEUE_INDEX_MASK                BITS(25, 31)
#define CONNAC2X_TX_DESC_QUEUE_INDEX_OFFSET              25

/* DW 1 */
#define CONNAC2X_TX_DESC_WLAN_INDEX_MASK                 BITS(0, 9)
#define CONNAC2X_TX_DESC_WLAN_INDEX_OFFSET               0
#define CONNAC2X_TX_DESC_VTA_MASK                       BIT(10)
#define CONNAC2X_TX_DESC_VTA_OFFSET                     10


#define CONNAC2X_TX_DESC_HEADER_FORMAT_MASK              BITS(16, 17)
#define CONNAC2X_TX_DESC_HEADER_FORMAT_OFFSET            16

#define CONNAC2X_TX_DESC_NON_802_11_MORE_DATA            BIT(11)
#define CONNAC2X_TX_DESC_NON_802_11_EOSP                 BIT(12)
#define CONNAC2X_TX_DESC_NON_802_11_REMOVE_VLAN          BIT(13)
#define CONNAC2X_TX_DESC_NON_802_11_VLAN_FIELD           BIT(14)
#define CONNAC2X_TX_DESC_NON_802_11_ETHERNET_II          BIT(15)
#define CONNAC2X_TX_DESC_NOR_802_11_HEADER_LENGTH_MASK   BITS(11, 15)
#define CONNAC2X_TX_DESC_NOR_802_11_HEADER_LENGTH_OFFSET 11
#define CONNAC2X_TX_DESC_ENH_802_11_EOSP                 BIT(12)
#define CONNAC2X_TX_DESC_ENH_802_11_AMSDU                BIT(13)

#define CONNAC2X_TX_DESC_HEADER_PADDING_LENGTH_MASK      BIT(18)
#define CONNAC2X_TX_DESC_HEADER_PADDING_LENGTH_OFFSET    18
#define CONNAC2X_TX_DESC_HEADER_PADDING_MODE             BIT(19)

#define CONNAC2X_TX_DESC_TID_MASK                        BITS(20, 22)
#define CONNAC2X_TX_DESC_TID_OFFSET                      20
#define CONNAC2X_TX_DESC_TID_NUM                         8

#define CONNAC2X_TX_DESC_TXD_UTXB_AMSDU_MASK             BIT(23)
#define CONNAC2X_TX_DESC_TXD_UTXB_AMSDU_OFFSET           23

#define CONNAC2X_TX_DESC_OWN_MAC_MASK                    BITS(24, 29)
#define CONNAC2X_TX_DESC_OWN_MAC_OFFSET                  24

 #define CONNAC2X_TX_DESC_TGID_MASK                       BIT(30)
#define CONNAC2X_TX_DESC_TGID_OFFSET                     30
#define CONNAC2X_TX_DESC_FORMAT                          BIT(31)

/* DW 2 */
#define CONNAC2X_TX_DESC_SUB_TYPE_MASK                   BITS(0, 3)
#define CONNAC2X_TX_DESC_SUB_TYPE_OFFSET                 0
#define CONNAC2X_TX_DESC_TYPE_MASK                       BITS(4, 5)
#define CONNAC2X_TX_DESC_TYPE_OFFSET                     4
#define CONNAC2X_TX_DESC_NDP                             BIT(6)
#define CONNAC2X_TX_DESC_NDPA                            BIT(7)

#define CONNAC2X_TX_DESC_SOUNDING                        BIT(8)
#define CONNAC2X_TX_DESC_FORCE_RTS_CTS                   BIT(9)
#define CONNAC2X_TX_DESC_BROADCAST_MULTICAST             BIT(10)
#define CONNAC2X_TX_DESC_BIP_PROTECTED                   BIT(11)
#define CONNAC2X_TX_DESC_DURATION_FIELD_CONTROL          BIT(12)
#define CONNAC2X_TX_DESC_HTC_EXISTS                      BIT(13)
#define CONNAC2X_TX_DESC_FRAGMENT_MASK                   BITS(14, 15)
#define CONNAC2X_TX_DESC_FRAGMENT_OFFSET                 14

#define CONNAC2X_TX_DESC_REMAINING_MAX_TX_TIME_MASK      BITS(16, 23)
#define CONNAC2X_TX_DESC_REMAINING_MAX_TX_TIME_OFFSET    16

#define CONNAC2X_TX_DESC_POWER_OFFSET_MASK               BITS(24, 29)
#define CONNAC2X_TX_DESC_POWER_OFFSET_OFFSET             24
#define CONNAC2X_TX_DESC_FIXED_RATE_MODE                 BIT(30)
#define CONNAC2X_TX_DESC_FIXED_RATE                      BIT(31)

/* DW 3 */
#define CONNAC2X_TX_DESC_NO_ACK                          BIT(0)
#define CONNAC2X_TX_DESC_PROTECTED_FRAME                 BIT(1)
#define CONNAC2X_TX_DESC_EXTEND_MORE_DATA                BIT(2)
#define CONNAC2X_TX_DESC_EXTEND_EOSP                     BIT(3)
#define CONNAC2X_TX_DESC_DA_SOURCE                       BIT(4)
#define CONNAC2X_TX_DESC_TIMING_MEASUREMENT              BIT(5)
#define CONNAC2X_TX_DESC_TX_COUNT_MASK                   BITS(6, 10)
#define CONNAC2X_TX_DESC_TX_COUNT_OFFSET                 6
#define CONNAC2X_TX_DESC_REMAINING_TX_COUNT_MASK         BITS(11, 15)
#define CONNAC2X_TX_DESC_REMAINING_TX_COUNT_OFFSET       11
#define CONNAC2X_TX_DESC_SEQUENCE_NUMBER_MASK            BITS(16, 27)
#define CONNAC2X_TX_DESC_SEQUENCE_NUMBER_MASK_OFFSET     16
#define CONNAC2X_TX_DESC_BA_DISABLE                      BIT(28)
#define CONNAC2X_TX_DESC_POWER_MANAGEMENT_CONTROL        BIT(29)
#define CONNAC2X_TX_DESC_PN_IS_VALID                     BIT(30)
#define CONNAC2X_TX_DESC_SN_IS_VALID                     BIT(31)

/* DW 4 */
#define CONNAC2X_TX_DESC_PN_PART1                        BITS(0, 31)

/* DW 5 */
#define CONNAC2X_TX_DESC_PACKET_ID_MASK                  BITS(0, 7)
#define CONNAC2X_TX_DESC_PACKET_ID_OFFSET                0
#define CONNAC2X_TX_DESC_TX_STATUS_FORMAT                BIT(8)
#define CONNAC2X_TX_DESC_TX_STATUS_FORMAT_OFFSET         8
#define CONNAC2X_TX_DESC_TX_STATUS_TO_MCU                BIT(9)
#define CONNAC2X_TX_DESC_TX_STATUS_TO_HOST               BIT(10)
#define CONNAC2X_TX_DESC_ADD_BA                          BIT(14)
#define CONNAC2X_TX_DESC_ADD_BA_OFFSET                   14
#define CONNAC2X_TX_DESC_MD_MASK                         BIT(15)
#define CONNAC2X_TX_DESC_MD_OFFSET                       15
#define CONNAC2X_TX_DESC_PN_PART2_MASK                   BITS(16, 31)
#define CONNAC2X_TX_DESC_PN_PART2__OFFSET                16

/* DW 6 *//* FR = 1 */
#define CONNAC2X_TX_DESC_BANDWIDTH_MASK                  BITS(0, 2)
#define CONNAC2X_TX_DESC_BANDWIDTH_OFFSET                0
#define CONNAC2X_TX_DESC_DYNAMIC_BANDWIDTH               BIT(3)
 #define CONNAC2X_TX_DESC_ANTENNA_INDEX_MASK              BITS(4, 7)
#define CONNAC2X_TX_DESC_ANTENNA_INDEX_OFFSET            4
#define CONNAC2X_TX_DESC_SPE_IDX_SEL                     BIT(10)
#define CONNAC2X_TX_DESC_SPE_IDX_SEL_OFFSET              10
/* in DW5 for AXE : LDPC, HE_LTF, GI_TYPE*/
#define CONNAC2X_TX_DESC_LDPC                            BIT(11)
#define CONNAC2X_TX_DESC_LDPC_OFFSET                     11
#define CONNAC2X_TX_DESC_HE_LTF_MASK                     BITS(12, 13)
#define CONNAC2X_TX_DESC_HE_LTF_OFFSET                   12
#define CONNAC2X_TX_DESC_GI_TYPE                         BITS(14, 15)
#define CONNAC2X_TX_DESC_GI_TYPE_OFFSET                  14
#define CONNAC2X_TX_DESC_FIXDE_RATE_MASK                 BITS(16, 29)
#define CONNAC2X_TX_DESC_FIXDE_RATE_OFFSET               16
#define CONNAC2X_TX_DESC_TXE_BF                          BIT(30)
#define CONNAC2X_TX_DESC_TXI_BF                          BIT(31)

/* DW 7 */
#define CONNAC2X_TX_DESC_TXD_ARRIVAL_TIME_MASK           BITS(0, 9)

#define CONNAC2X_TX_DESC_HW_AMSDU                        BIT(10)
#define CONNAC2X_TX_DESC_SPE_EXT_IDX_MASK                BITS(11, 15)
#define CONNAC2X_TX_DESC_SPE_EXT_IDX_OFFSET              11
/* For cut-through only */
#define CONNAC2X_TX_DESC7_SUB_TYPE_MASK                  BITS(16, 19)
#define CONNAC2X_TX_DESC7_SUB_TYPE_OFFSET                16
/* For cut-through only */
#define CONNAC2X_TX_DESC7_TYPE_MASK                      BITS(20, 21)
#define CONNAC2X_TX_DESC7_TYPE_OFFSET                    20

#define CONNAC2X_TX_DESC_IP_CHKSUM_OFFLOAD               BIT(28)
#define CONNAC2X_TX_DESC_TCP_UDP_CHKSUM_OFFLOAD          BIT(29)
#define CONNAC2X_TX_DESC_TXD_LENGTH_MASK                 BITS(30, 31)
#define CONNAC2X_TX_DESC_TXD_LENGTH_OFFSET               30

/* For Debug Information Use */
#define CONNAC2X_TX_DESC_PSE_FID_MASK                    BITS(16, 27)
#define CONNAC2X_TX_DESC_PSE_FID_OFFSET                  16
#define CONNAC2X_TX_DESC_CTXD_CNT_MASK                   BITS(23, 25)
#define CONNAC2X_TX_DESC_CTXD_CNT_OFFSET                 23
#define CONNAC2X_TX_DESC_CTXD                            BIT(26)

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

struct HW_MAC_CONNAC2X_TX_DESC {
	uint32_t u4DW0;
	uint32_t u4DW1;
	uint32_t u4DW2;
	uint32_t u4DW3;
	uint32_t u4PN1;
	uint16_t u2DW5_0;
	uint16_t u2PN2;
	uint32_t u4DW6;
	uint32_t u4DW7;
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

/*------------------------------------------------------------------------------
 * MACRO for HW_MAC_TX_DESC_T
 *------------------------------------------------------------------------------
 */

#define HAL_MAC_CONNAC2X_TXD_GET_TX_BYTE_COUNT(_prHwMacTxDesc) \
TX_DESC_GET_FIELD((_prHwMacTxDesc)->u4DW0, \
CONNAC2X_TX_DESC_TX_BYTE_COUNT_MASK, CONNAC2X_TX_DESC_TX_BYTE_COUNT_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_TX_BYTE_COUNT(_prHwMacTxD, _u2TxByteCount)\
TX_DESC_SET_FIELD(((_prHwMacTxD)->u4DW0), ((uint16_t)_u2TxByteCount), \
CONNAC2X_TX_DESC_TX_BYTE_COUNT_MASK, CONNAC2X_TX_DESC_TX_BYTE_COUNT_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_GET_ETHER_TYPE_OFFSET(_prHwMacTxDesc) \
TX_DESC_GET_FIELD((_prHwMacTxDesc)->u4DW0, \
CONNAC2X_TX_DESC_ETHER_TYPE_OFFSET_MASK, \
CONNAC2X_TX_DESC_ETHER_TYPE_OFFSET_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_ETHER_TYPE_OFFSET(_prHwMacTxD, _ucEthTypOff)\
TX_DESC_SET_FIELD(((_prHwMacTxD)->u4DW0), ((uint8_t)_ucEthTypOff), \
CONNAC2X_TX_DESC_ETHER_TYPE_OFFSET_MASK, \
CONNAC2X_TX_DESC_ETHER_TYPE_OFFSET_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_IS_IP_CHKSUM_ENABLED(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW7 & CONNAC2X_TX_DESC_IP_CHKSUM_OFFLOAD)?FALSE:TRUE)

#define HAL_MAC_CONNAC2X_TXD_SET_IP_CHKSUM(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW7 |= CONNAC2X_TX_DESC_IP_CHKSUM_OFFLOAD)

#define HAL_MAC_CONNAC2X_TXD_UNSET_IP_CHKSUM(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW7 &= ~CONNAC2X_TX_DESC_IP_CHKSUM_OFFLOAD)

#define HAL_MAC_CONNAC2X_TXD_IS_TCP_UDP_CHKSUM_ENABLED(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW7 & CONNAC2X_TX_DESC_TCP_UDP_CHKSUM_OFFLOAD)?FALSE:TRUE)

#define HAL_MAC_CONNAC2X_TXD_SET_TCP_UDP_CHKSUM(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW7 |= CONNAC2X_TX_DESC_TCP_UDP_CHKSUM_OFFLOAD)

#define HAL_MAC_CONNAC2X_TXD_UNSET_TCP_UDP_CHKSUM(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW7 &= ~CONNAC2X_TX_DESC_TCP_UDP_CHKSUM_OFFLOAD)

#define HAL_MAC_CONNAC2X_TXD_GET_QUEUE_INDEX(_prHwMacTxDesc) \
TX_DESC_GET_FIELD((_prHwMacTxDesc)->u4DW0,\
CONNAC2X_TX_DESC_QUEUE_INDEX_MASK, CONNAC2X_TX_DESC_QUEUE_INDEX_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_QUEUE_INDEX(_prHwMacTxDesc, _ucQueueIndex) \
TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u4DW0), ((uint8_t)_ucQueueIndex), \
CONNAC2X_TX_DESC_QUEUE_INDEX_MASK, CONNAC2X_TX_DESC_QUEUE_INDEX_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_GET_WLAN_INDEX(_prHwMacTxDesc) \
TX_DESC_GET_FIELD((_prHwMacTxDesc)->u4DW1,\
CONNAC2X_TX_DESC_WLAN_INDEX_MASK, CONNAC2X_TX_DESC_WLAN_INDEX_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_WLAN_INDEX(_prHwMacTxDesc, _ucWlanIdx) \
TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u4DW1), ((uint8_t)_ucWlanIdx), \
CONNAC2X_TX_DESC_WLAN_INDEX_MASK, CONNAC2X_TX_DESC_WLAN_INDEX_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_IS_LONG_FORMAT(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW1 & CONNAC2X_TX_DESC_FORMAT)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_TXD_SET_LONG_FORMAT(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW1 |= CONNAC2X_TX_DESC_FORMAT)

#define HAL_MAC_CONNAC2X_TXD_SET_SHORT_FORMAT(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW1 &= ~CONNAC2X_TX_DESC_FORMAT)

#define HAL_MAC_CONNAC2X_TXD_GET_HEADER_FORMAT(_prHwMacTxDesc) \
TX_DESC_GET_FIELD((_prHwMacTxDesc)->u4DW1, \
CONNAC2X_TX_DESC_HEADER_FORMAT_MASK, CONNAC2X_TX_DESC_HEADER_FORMAT_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_HEADER_FORMAT(_prHwMacTxDesc, _ucHdrFormat) \
TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u4DW1), \
((uint8_t)_ucHdrFormat), CONNAC2X_TX_DESC_HEADER_FORMAT_MASK,\
CONNAC2X_TX_DESC_HEADER_FORMAT_OFFSET)

/* HF = 0x00, 802.11 normal mode */
#define HAL_MAC_CONNAC2X_TXD_IS_MORE_DATA(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW1 & CONNAC2X_TX_DESC_NON_802_11_MORE_DATA)?TRUE:FALSE)
#define HAL_MAC_CONNAC2X_TXD_SET_MORE_DATA(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW1 |= CONNAC2X_TX_DESC_NON_802_11_MORE_DATA)
#define HAL_MAC_CONNAC2X_TXD_UNSET_MORE_DATA(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW1 &= ~CONNAC2X_TX_DESC_NON_802_11_MORE_DATA)

#define HAL_MAC_CONNAC2X_TXD_IS_REMOVE_VLAN(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW1 & CONNAC2X_TX_DESC_NON_802_11_REMOVE_VLAN)?TRUE:FALSE)
#define HAL_MAC_CONNAC2X_TXD_SET_REMOVE_VLAN(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW1 |= CONNAC2X_TX_DESC_NON_802_11_REMOVE_VLAN)
#define HAL_MAC_CONNAC2X_TXD_UNSET_REMOVE_VLAN(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW1 &= ~CONNAC2X_TX_DESC_NON_802_11_REMOVE_VLAN)

#define HAL_MAC_CONNAC2X_TXD_IS_VLAN(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW1 & CONNAC2X_TX_DESC_NON_802_11_VLAN_FIELD)?TRUE:FALSE)
#define HAL_MAC_CONNAC2X_TXD_SET_VLAN(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW1 |= CONNAC2X_TX_DESC_NON_802_11_VLAN_FIELD)
#define HAL_MAC_CONNAC2X_TXD_UNSET_VLAN(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW1 &= ~CONNAC2X_TX_DESC_NON_802_11_VLAN_FIELD)

#define HAL_MAC_CONNAC2X_TXD_IS_ETHERNET_II(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW1 & CONNAC2X_TX_DESC_NON_802_11_ETHERNET_II)?TRUE:FALSE)
#define HAL_MAC_CONNAC2X_TXD_SET_ETHERNET_II(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW1 |= CONNAC2X_TX_DESC_NON_802_11_ETHERNET_II)
#define HAL_MAC_CONNAC2X_TXD_UNSET_ETHERNET_II(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW1 &= ~CONNAC2X_TX_DESC_NON_802_11_ETHERNET_II)

/* HF = 0x00/0x11, 802.11 normal/enhancement mode */
#define HAL_MAC_CONNAC2X_TXD_IS_EOSP(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW1 & CONNAC2X_TX_DESC_NON_802_11_EOSP)?TRUE:FALSE)
#define HAL_MAC_CONNAC2X_TXD_SET_EOSP(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW1 |= CONNAC2X_TX_DESC_NON_802_11_EOSP)
#define HAL_MAC_CONNAC2X_TXD_UNSET_EOSP(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW1 &= ~CONNAC2X_TX_DESC_NON_802_11_EOSP)

/* HF = 0x11, 802.11 enhancement mode */
#define HAL_MAC_CONNAC2X_TXD_IS_AMSDU(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW1 & CONNAC2X_TX_DESC_ENH_802_11_AMSDU)?TRUE:FALSE)
#define HAL_MAC_CONNAC2X_TXD_SET_AMSDU(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW1 |= CONNAC2X_TX_DESC_ENH_802_11_AMSDU)
#define HAL_MAC_CONNAC2X_TXD_UNSET_AMSDU(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW1 &= ~CONNAC2X_TX_DESC_ENH_802_11_AMSDU)

/* HF = 0x10, non-802.11 */
#define HAL_MAC_CONNAC2X_TXD_GET_802_11_HEADER_LENGTH(_prHwMacTxDesc) \
TX_DESC_GET_FIELD((_prHwMacTxDesc)->u4DW1, \
CONNAC2X_TX_DESC_NOR_802_11_HEADER_LENGTH_MASK, \
CONNAC2X_TX_DESC_NOR_802_11_HEADER_LENGTH_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_802_11_HEADER_LENGTH(_prHwMacTxD, _ucHdrLen)\
TX_DESC_SET_FIELD(((_prHwMacTxD)->u4DW1), ((uint8_t)_ucHdrLen), \
CONNAC2X_TX_DESC_NOR_802_11_HEADER_LENGTH_MASK, \
CONNAC2X_TX_DESC_NOR_802_11_HEADER_LENGTH_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_GET_TXD_LENGTH(_prHwMacTxDesc) \
TX_DESC_GET_FIELD((_prHwMacTxDesc)->u4DW7, \
CONNAC2X_TX_DESC_TXD_LENGTH_MASK, CONNAC2X_TX_DESC_TXD_LENGTH_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_TXD_LENGTH(_prHwMacTxDesc, _ucHdrPadding) \
TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u4DW7), ((uint8_t)_ucHdrPadding), \
CONNAC2X_TX_DESC_TXD_LENGTH_MASK, CONNAC2X_TX_DESC_TXD_LENGTH_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_GET_HEADER_PADDING(_prHwMacTxDesc) \
TX_DESC_GET_FIELD((_prHwMacTxDesc)->u4DW1, \
CONNAC2X_TX_DESC_HEADER_PADDING_LENGTH_MASK, \
CONNAC2X_TX_DESC_HEADER_PADDING_LENGTH_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_HEADER_PADDING(_prHwMacTxD, _ucHdrPadding) \
TX_DESC_SET_FIELD(((_prHwMacTxD)->u4DW1), ((uint8_t)_ucHdrPadding), \
CONNAC2X_TX_DESC_HEADER_PADDING_LENGTH_MASK, \
CONNAC2X_TX_DESC_HEADER_PADDING_LENGTH_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_GET_UTXB_AMSDU(_prHwMacTxDesc) \
TX_DESC_GET_FIELD((_prHwMacTxDesc)->u4DW1, \
CONNAC2X_TX_DESC_TXD_UTXB_AMSDU_MASK, CONNAC2X_TX_DESC_TXD_UTXB_AMSDU_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_UTXB_AMSDU(_prHwMacTxDesc, _ucHdrPadding) \
TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u4DW1), ((uint8_t)_ucHdrPadding), \
CONNAC2X_TX_DESC_TXD_UTXB_AMSDU_MASK, CONNAC2X_TX_DESC_TXD_UTXB_AMSDU_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_IS_HEADER_PADDING_IN_THE_HEAD(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW1 & CONNAC2X_TX_DESC_HEADER_PADDING_MODE)?TRUE:FALSE)
#define HAL_MAC_CONNAC2X_TXD_SET_HEADER_PADDING_IN_THE_HEAD(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW1 |= CONNAC2X_TX_DESC_HEADER_PADDING_MODE)
#define HAL_MAC_CONNAC2X_TXD_SET_HEADER_PADDING_IN_THE_TAIL(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW1 &= ~CONNAC2X_TX_DESC_HEADER_PADDING_MODE)

#define HAL_MAC_CONNAC2X_TXD_GET_TID(_prHwMacTxDesc) \
TX_DESC_GET_FIELD((_prHwMacTxDesc)->u4DW1, CONNAC2X_TX_DESC_TID_MASK, \
CONNAC2X_TX_DESC_TID_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_TID(_prHwMacTxDesc, _ucTID) \
TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u4DW1), \
((uint8_t)_ucTID), CONNAC2X_TX_DESC_TID_MASK, CONNAC2X_TX_DESC_TID_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_GET_PKT_FORMAT(_prHwMacTxDesc) \
TX_DESC_GET_FIELD((_prHwMacTxDesc)->u4DW0, \
CONNAC2X_TX_DESC_PACKET_FORMAT_MASK, CONNAC2X_TX_DESC_PACKET_FORMAT_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_PKT_FORMAT(_prHwMacTxDesc, _ucPktFormat) \
TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u4DW0), ((uint8_t)_ucPktFormat), \
CONNAC2X_TX_DESC_PACKET_FORMAT_MASK, CONNAC2X_TX_DESC_PACKET_FORMAT_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_GET_OWN_MAC_INDEX(_prHwMacTxDesc) \
TX_DESC_GET_FIELD((_prHwMacTxDesc)->u4DW1, \
CONNAC2X_TX_DESC_OWN_MAC_MASK, CONNAC2X_TX_DESC_OWN_MAC_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_OWN_MAC_INDEX(_prHwMacTxDesc, _ucOwnMacIdx) \
TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u4DW1), ((uint8_t)_ucOwnMacIdx), \
CONNAC2X_TX_DESC_OWN_MAC_MASK, CONNAC2X_TX_DESC_OWN_MAC_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_GET_SUB_TYPE(_prHwMacTxDesc) \
TX_DESC_GET_FIELD((_prHwMacTxDesc)->u4DW2, \
CONNAC2X_TX_DESC_SUB_TYPE_MASK, CONNAC2X_TX_DESC_SUB_TYPE_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_SUB_TYPE(_prHwMacTxDesc, _ucSubType) \
TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u4DW2), ((uint8_t)_ucSubType), \
CONNAC2X_TX_DESC_SUB_TYPE_MASK, CONNAC2X_TX_DESC_SUB_TYPE_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_GET_TYPE(_prHwMacTxDesc) \
TX_DESC_GET_FIELD((_prHwMacTxDesc)->u4DW2, CONNAC2X_TX_DESC_TYPE_MASK, \
CONNAC2X_TX_DESC_TYPE_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_TYPE(_prHwMacTxDesc, _ucType) \
TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u4DW2), ((uint8_t)_ucType), \
CONNAC2X_TX_DESC_TYPE_MASK, CONNAC2X_TX_DESC_TYPE_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_IS_NDP(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW2 & CONNAC2X_TX_DESC_NDP)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_TXD_SET_NDP(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW2 |= CONNAC2X_TX_DESC_NDP)

#define HAL_MAC_CONNAC2X_TXD_UNSET_NDP(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW2 &= ~CONNAC2X_TX_DESC_NDP)

#define HAL_MAC_CONNAC2X_TXD_IS_NDPA(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW2 & CONNAC2X_TX_DESC_NDPA)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_TXD_SET_NDPA(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW2 |= CONNAC2X_TX_DESC_NDPA)

#define HAL_MAC_CONNAC2X_TXD_UNSET_NDPA(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW2 &= ~CONNAC2X_TX_DESC_NDPA)

#define HAL_MAC_CONNAC2X_TXD_IS_SOUNDING_FRAME(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW2 & CONNAC2X_TX_DESC_SOUNDING)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_TXD_SET_SOUNDING_FRAME(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW2 |= CONNAC2X_TX_DESC_SOUNDING)

#define HAL_MAC_CONNAC2X_TXD_UNSET_SOUNDING_FRAME(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW2 &= ~CONNAC2X_TX_DESC_SOUNDING)

#define HAL_MAC_CONNAC2X_TXD_IS_FORCE_RTS_CTS_EN(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW2 & CONNAC2X_TX_DESC_FORCE_RTS_CTS)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_TXD_SET_FORCE_RTS_CTS(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW2 |= CONNAC2X_TX_DESC_FORCE_RTS_CTS)

#define HAL_MAC_CONNAC2X_TXD_UNSET_FORCE_RTS_CTS(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW2 &= ~CONNAC2X_TX_DESC_FORCE_RTS_CTS)

#define HAL_MAC_CONNAC2X_TXD_IS_BMC(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW2 & CONNAC2X_TX_DESC_BROADCAST_MULTICAST)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_TXD_SET_BMC(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW2 |= CONNAC2X_TX_DESC_BROADCAST_MULTICAST)

#define HAL_MAC_CONNAC2X_TXD_UNSET_BMC(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW2 &= ~CONNAC2X_TX_DESC_BROADCAST_MULTICAST)

#define HAL_MAC_CONNAC2X_TXD_IS_BIP(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW2 & CONNAC2X_TX_DESC_BIP_PROTECTED)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_TXD_SET_BIP(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW2 |= CONNAC2X_TX_DESC_BIP_PROTECTED)

#define HAL_MAC_CONNAC2X_TXD_UNSET_BIP(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW2 &= ~CONNAC2X_TX_DESC_BIP_PROTECTED)

#define HAL_MAC_CONNAC2X_TXD_IS_DURATION_CONTROL_BY_SW(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW2 & CONNAC2X_TX_DESC_DURATION_FIELD_CONTROL)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_TXD_SET_DURATION_CONTROL_BY_SW(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW2 |= CONNAC2X_TX_DESC_DURATION_FIELD_CONTROL)

#define HAL_MAC_CONNAC2X_TXD_SET_DURATION_CONTROL_BY_HW(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW2 &= ~CONNAC2X_TX_DESC_DURATION_FIELD_CONTROL)

#define HAL_MAC_CONNAC2X_TXD_IS_HTC_EXIST(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW2 & CONNAC2X_TX_DESC_HTC_EXISTS)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_TXD_SET_HTC_EXIST(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW2 |= CONNAC2X_TX_DESC_HTC_EXISTS)

#define HAL_MAC_CONNAC2X_TXD_UNSET_HTC_EXIST(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW2 &= ~CONNAC2X_TX_DESC_HTC_EXISTS)

#define HAL_MAC_CONNAC2X_TXD_IS_FRAG_PACKET(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW2 & CONNAC2X_TX_DESC_FRAGMENT_MASK)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_TXD_GET_FRAG_PACKET_POS(_prHwMacTxDesc) \
TX_DESC_GET_FIELD((_prHwMacTxDesc)->u4DW2, CONNAC2X_TX_DESC_FRAGMENT_MASK, \
CONNAC2X_TX_DESC_FRAGMENT_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_FRAG_PACKET_POS(_prHwMacTxDesc, _ucFragPos)\
TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u4DW2), ((uint8_t)_ucFragPos), \
CONNAC2X_TX_DESC_FRAGMENT_MASK, CONNAC2X_TX_DESC_FRAGMENT_OFFSET)

/* For driver */
/* in unit of 32TU */
#define HAL_MAC_CONNAC2X_TXD_GET_REMAINING_LIFE_TIME(_prHwMacTxDesc) \
TX_DESC_GET_FIELD((_prHwMacTxDesc)->u4DW2, \
CONNAC2X_TX_DESC_REMAINING_MAX_TX_TIME_MASK, \
CONNAC2X_TX_DESC_REMAINING_MAX_TX_TIME_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_REMAINING_LIFE_TIME(_prHwMacTxD, _LifeTime) \
TX_DESC_SET_FIELD(((_prHwMacTxD)->u4DW2), ((uint8_t)_LifeTime), \
CONNAC2X_TX_DESC_REMAINING_MAX_TX_TIME_MASK, \
CONNAC2X_TX_DESC_REMAINING_MAX_TX_TIME_OFFSET)

/* in unit of ms (minimal value is about 40ms) */
#define HAL_MAC_CONNAC2X_TXD_GET_REMAINING_LIFE_TIME_IN_MS(_prHwMacTxDesc) \
(TU_TO_MSEC(HAL_MAC_CONNAC2X_TXD_GET_REMAINING_LIFE_TIME(_prHwMacTxDesc)\
<< CONNAC2X_TX_DESC_LIFE_TIME_UNIT_IN_POWER_OF_2))


/*
*    Remaining Life Time/ Max TX Time: This field indicates the remaining life
*                                   time in unit of 64TU used for this packet.
*    8'h0: No life time for current packet, HW should NOT change this field
*    8'hN: N is not 0, HW will calculation a Max. TX time and replace remaining
*          TX time with it. The Max. value is 127.
*    The MSB bit is reserved for HW and SW should set to 0
*    Due to HW design issue, the remaining life time sometimes may have 32TU
*          more than configured.
*    PP will replace this field with "Max. TX Time".
*           (SUM of (Remaining Life Time + Internal Free-run Timer)).
*           If the SUM is 0, PP will add 1 to it.
*/
#define HAL_MAC_CONNAC2X_TXD_SET_REMAINING_LIFE_TIME_IN_MS(_prHwTxD, _LifeMs)\
do { \
uint32_t u4LifeTimeInUnit = ((MSEC_TO_USEC(_LifeMs) / USEC_PER_TU) \
			    >> CONNAC2X_TX_DESC_LIFE_TIME_UNIT_IN_POWER_OF_2); \
if (u4LifeTimeInUnit >= BIT(7)) \
	u4LifeTimeInUnit = BITS(0, 6); \
else if ((u4LifeTimeInUnit != TX_DESC_TX_TIME_NO_LIMIT) \
	&& (u4LifeTimeInUnit == TX_DESC_TX_TIME_NO_LIMIT)) \
	u4LifeTimeInUnit = 1; \
HAL_MAC_CONNAC2X_TXD_SET_REMAINING_LIFE_TIME(_prHwTxD, \
	(uint8_t)u4LifeTimeInUnit); \
} while (0)

#define HAL_MAC_CONNAC2X_TXD_GET_POWER_OFFSET(_prHwMacTxDesc) \
TX_DESC_GET_FIELD((_prHwMacTxDesc)->u4DW2, \
CONNAC2X_TX_DESC_POWER_OFFSET_MASK, CONNAC2X_TX_DESC_POWER_OFFSET_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_POWER_OFFSET(_prHwTxD, _ucPowerOffset) \
TX_DESC_SET_FIELD(((_prHwTxD)->u4DW2), ((uint8_t)_ucPowerOffset), \
CONNAC2X_TX_DESC_POWER_OFFSET_MASK, CONNAC2X_TX_DESC_POWER_OFFSET_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_IS_CR_FIXED_RATE_MODE(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW2 & CONNAC2X_TX_DESC_FIXED_RATE_MODE)?TRUE:FALSE)
#define HAL_MAC_CONNAC2X_TXD_SET_FIXED_RATE_MODE_TO_DESC(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW2 &= ~CONNAC2X_TX_DESC_FIXED_RATE_MODE)
#define HAL_MAC_CONNAC2X_TXD_SET_FIXED_RATE_MODE_TO_CR(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW2 |= CONNAC2X_TX_DESC_FIXED_RATE_MODE)

#define HAL_MAC_CONNAC2X_TXD_IS_FIXED_RATE_ENABLE(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW2 & CONNAC2X_TX_DESC_FIXED_RATE)?TRUE:FALSE)
#define HAL_MAC_CONNAC2X_TXD_SET_FIXED_RATE_ENABLE(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW2 |= CONNAC2X_TX_DESC_FIXED_RATE)
#define HAL_MAC_CONNAC2X_TXD_SET_FIXED_RATE_DISABLE(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW2 &= ~CONNAC2X_TX_DESC_FIXED_RATE)

#define HAL_MAC_CONNAC2X_TXD_IS_BA_DISABLE(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW3 & CONNAC2X_TX_DESC_BA_DISABLE)?TRUE:FALSE)

#define HAL_MAC_CONNAC2X_TXD_SET_BA_DISABLE(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW3 |= CONNAC2X_TX_DESC_BA_DISABLE)
#define HAL_MAC_CONNAC2X_TXD_SET_BA_ENABLE(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW3 &= ~CONNAC2X_TX_DESC_BA_DISABLE)

#define HAL_MAC_CONNAC2X_TXD_IS_TIMING_MEASUREMENT(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW3 & CONNAC2X_TX_DESC_TIMING_MEASUREMENT)?TRUE:FALSE)
#define HAL_MAC_CONNAC2X_TXD_SET_TIMING_MEASUREMENT(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW3 |= CONNAC2X_TX_DESC_TIMING_MEASUREMENT)
#define HAL_MAC_CONNAC2X_TXD_UNSET_TIMING_MEASUREMENT(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW3 &= ~CONNAC2X_TX_DESC_TIMING_MEASUREMENT)

#define HAL_MAC_CONNAC2X_TXD_IS_NO_ACK(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW3 & CONNAC2X_TX_DESC_NO_ACK)?TRUE:FALSE)
#define HAL_MAC_CONNAC2X_TXD_SET_NO_ACK(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW3 |= CONNAC2X_TX_DESC_NO_ACK)
#define HAL_MAC_CONNAC2X_TXD_UNSET_NO_ACK(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW3 &= ~CONNAC2X_TX_DESC_NO_ACK)

#define HAL_MAC_CONNAC2X_TXD_IS_PROTECTION(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW3 & CONNAC2X_TX_DESC_PROTECTED_FRAME)?TRUE:FALSE)
#define HAL_MAC_CONNAC2X_TXD_SET_PROTECTION(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW3 |= CONNAC2X_TX_DESC_PROTECTED_FRAME)
#define HAL_MAC_CONNAC2X_TXD_UNSET_PROTECTION(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW3 &= ~CONNAC2X_TX_DESC_PROTECTED_FRAME)

#define HAL_MAC_CONNAC2X_TXD_IS_EXTEND_MORE_DATA(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW3 & CONNAC2X_TX_DESC_EXTEND_MORE_DATA)?TRUE:FALSE)
#define HAL_MAC_CONNAC2X_TXD_SET_EXTEND_MORE_DATA(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW3 |= CONNAC2X_TX_DESC_EXTEND_MORE_DATA)
#define HAL_MAC_CONNAC2X_TXD_UNSET_EXTEND_MORE_DATA(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW3 &= ~CONNAC2X_TX_DESC_EXTEND_MORE_DATA)

#define HAL_MAC_CONNAC2X_TXD_IS_EXTEND_EOSP(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW3 & CONNAC2X_TX_DESC_EXTEND_EOSP)?TRUE:FALSE)
#define HAL_MAC_CONNAC2X_TXD_SET_EXTEND_EOSP(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW3 |= CONNAC2X_TX_DESC_EXTEND_EOSP)
#define HAL_MAC_CONNAC2X_TXD_UNSET_EXTEND_EOSP(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW3 &= ~CONNAC2X_TX_DESC_EXTEND_EOSP)

#define HAL_MAC_CONNAC2X_TXD_GET_TX_COUNT(_prHwMacTxDesc) \
TX_DESC_GET_FIELD((_prHwMacTxDesc)->u4DW3, \
CONNAC2X_TX_DESC_TX_COUNT_MASK, CONNAC2X_TX_DESC_TX_COUNT_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_TX_COUNT(_prHwMacTxDesc, _ucTxCountLimit) \
TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u4DW3), ((uint8_t)_ucTxCountLimit), \
CONNAC2X_TX_DESC_TX_COUNT_MASK, CONNAC2X_TX_DESC_TX_COUNT_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_GET_REMAINING_TX_COUNT(_prHwMacTxDesc) \
TX_DESC_GET_FIELD((_prHwMacTxDesc)->u4DW3, \
CONNAC2X_TX_DESC_REMAINING_TX_COUNT_MASK, \
CONNAC2X_TX_DESC_REMAINING_TX_COUNT_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_REMAINING_TX_COUNT(_prHwTxD, _ucTxCntLimit) \
TX_DESC_SET_FIELD(((_prHwTxD)->u4DW3), ((uint8_t)_ucTxCntLimit), \
CONNAC2X_TX_DESC_REMAINING_TX_COUNT_MASK, \
CONNAC2X_TX_DESC_REMAINING_TX_COUNT_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_GET_SEQUENCE_NUMBER(_prHwMacTxDesc) \
TX_DESC_GET_FIELD((_prHwMacTxDesc)->u4DW3, \
CONNAC2X_TX_DESC_SEQUENCE_NUMBER_MASK, \
CONNAC2X_TX_DESC_SEQUENCE_NUMBER_MASK_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_SEQUENCE_NUMBER(_prHwMacTxDesc, _u2SN) \
TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u4DW3), \
((uint16_t)_u2SN), CONNAC2X_TX_DESC_SEQUENCE_NUMBER_MASK, \
CONNAC2X_TX_DESC_SEQUENCE_NUMBER_MASK_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_IS_TXD_SN_VALID(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW3 & CONNAC2X_TX_DESC_SN_IS_VALID)?TRUE:FALSE)
#define HAL_MAC_CONNAC2X_TXD_SET_TXD_SN_VALID(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW3 |= CONNAC2X_TX_DESC_SN_IS_VALID)
#define HAL_MAC_CONNAC2X_TXD_SET_TXD_SN_INVALID(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW3 &= ~CONNAC2X_TX_DESC_SN_IS_VALID)

#define HAL_MAC_CONNAC2X_TXD_IS_TXD_PN_VALID(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW3 & CONNAC2X_TX_DESC_PN_IS_VALID)?TRUE:FALSE)
#define HAL_MAC_CONNAC2X_TXD_SET_TXD_PN_VALID(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW3 |= CONNAC2X_TX_DESC_PN_IS_VALID)
#define HAL_MAC_CONNAC2X_TXD_SET_TXD_PN_INVALID(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW3 &= ~CONNAC2X_TX_DESC_PN_IS_VALID)

#define HAL_MAC_CONNAC2X_TXD_ASSIGN_SN_BY_SW(_prHwMacTxDesc, _u2SN) \
{ \
	HAL_MAC_CONNAC2X_TXD_SET_SEQUENCE_NUMBER(_prHwMacTxDesc, _u2SN); \
	HAL_MAC_CONNAC2X_TXD_SET_TXD_SN_VALID(_prHwMacTxDesc); \
}
#define HAL_MAC_CONNAC2X_TXD_ASSIGN_SN_BY_HW(_prHwMacTxDesc) \
{ \
	HAL_MAC_CONNAC2X_TXD_SET_SEQUENCE_NUMBER(_prHwMacTxDesc, 0); \
	HAL_MAC_CONNAC2X_TXD_SET_TXD_SN_INVALID(_prHwMacTxDesc); \
}

#if 0
#define HAL_MAC_FALCON_TX_DESC_GET_PN(_prHwMacTxDesc, _u4PN_0_31, _u2PN_32_47) \
{ \
	((UINT_32)_u4PN_0_31) = (_prHwMacTxDesc)->u4PN1; \
	((UINT_16)_u2PN_32_47) = (_prHwMacTxDesc)->u2PN2; \
}
#define HAL_MAC_FALCON_TX_DESC_SET_PN(_prHwMacTxDesc, _u4PN_0_31, _u2PN_32_47) \
{ \
	(_prHwMacTxDesc)->u4PN1 = ((UINT_32)_u4PN_0_31); \
	(_prHwMacTxDesc)->u2PN2 = ((UINT_16)_u2PN_32_47); \
}
#endif

#define HAL_MAC_CONNAC2X_TXD_ASSIGN_PN_BY_SW(_prHwTxD, _u4PN0_31, _u2PN32_47)\
{ \
	HAL_MAC_TX_DESC_SET_PN(_prHwTxD, _u4PN0_31, _u2PN32_47); \
	HAL_MAC_CONNAC2X_TXD_SET_TXD_PN_VALID(_prHwTxD); \
}
#define HAL_MAC_CONNAC2X_TXD_ASSIGN_PSN_BY_HW(_prHwMacTxDesc) \
{ \
	HAL_MAC_TX_DESC_SET_PN(_prHwMacTxDesc, 0, 0); \
	HAL_MAC_CONNAC2X_TXD_SET_TXD_PN_INVALID(_prHwMacTxDesc); \
}

#define HAL_MAC_CONNAC2X_TXD_GET_PID(_prHwMacTxDesc) \
TX_DESC_GET_FIELD((_prHwMacTxDesc)->u2DW5_0, \
CONNAC2X_TX_DESC_PACKET_ID_MASK, CONNAC2X_TX_DESC_PACKET_ID_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_PID(_prHwMacTxDesc, _ucPID) \
TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u2DW5_0), ((uint8_t)_ucPID), \
CONNAC2X_TX_DESC_PACKET_ID_MASK, CONNAC2X_TX_DESC_PACKET_ID_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_GET_TXS_FORMAT(_prHwMacTxDesc) \
TX_DESC_GET_FIELD((_prHwMacTxDesc)->u2DW5_0, \
CONNAC2X_TX_DESC_TX_STATUS_FORMAT, CONNAC2X_TX_DESC_TX_STATUS_FORMAT_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_TXS_FORMAT(_prHwMacTxDesc, _ucTXSFormat) \
TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u2DW5_0), ((uint8_t)_ucTXSFormat), \
CONNAC2X_TX_DESC_TX_STATUS_FORMAT, CONNAC2X_TX_DESC_TX_STATUS_FORMAT_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_IS_TXS_TO_MCU(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u2DW5_0 & CONNAC2X_TX_DESC_TX_STATUS_TO_MCU)?TRUE:FALSE)
#define HAL_MAC_CONNAC2X_TXD_SET_TXS_TO_MCU(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u2DW5_0 |= CONNAC2X_TX_DESC_TX_STATUS_TO_MCU)
#define HAL_MAC_CONNAC2X_TXD_UNSET_TXS_TO_MCU(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u2DW5_0 &= ~CONNAC2X_TX_DESC_TX_STATUS_TO_MCU)

#define HAL_MAC_CONNAC2X_TXD_IS_TXS_TO_HOST(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u2DW5_0 & CONNAC2X_TX_DESC_TX_STATUS_TO_HOST)?TRUE:FALSE)
#define HAL_MAC_CONNAC2X_TXD_SET_TXS_TO_HOST(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u2DW5_0 |= CONNAC2X_TX_DESC_TX_STATUS_TO_HOST)
#define HAL_MAC_CONNAC2X_TXD_UNSET_TXS_TO_HOST(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u2DW5_0 &= ~CONNAC2X_TX_DESC_TX_STATUS_TO_HOST)

#define HAL_MAC_CONNAC2X_TXD_IS_DA_FROM_WTBL(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u2DW5_0 & TX_DESC_DA_SOURCE)?TRUE:FALSE)
#define HAL_MAC_CONNAC2X_TXD_SET_DA_FROM_WTBL(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u2DW5_0 |= TX_DESC_DA_SOURCE)
#define HAL_MAC_CONNAC2X_TXD_SET_DA_FROM_MSDU(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u2DW5_0 &= ~TX_DESC_DA_SOURCE)

#define HAL_MAC_CONNAC2X_TXD_IS_SW_PM_CONTROL(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW3 & CONNAC2X_TX_DESC_POWER_MANAGEMENT_CONTROL) ?\
	TRUE : FALSE)

#define HAL_MAC_CONNAC2X_TXD_SET_SW_PM_CONTROL(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW3 |= CONNAC2X_TX_DESC_POWER_MANAGEMENT_CONTROL)

#define HAL_MAC_CONNAC2X_TXD_SET_HW_PM_CONTROL(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW3 &= ~CONNAC2X_TX_DESC_POWER_MANAGEMENT_CONTROL)

#define HAL_MAC_CONNAC2X_TXD_SET_FR_BW(_prHwMacTxDesc, ucBw) \
TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u4DW6), ((uint8_t)ucBw), \
CONNAC2X_TX_DESC_BANDWIDTH_MASK, CONNAC2X_TX_DESC_BANDWIDTH_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_FR_DYNAMIC_BW_RTS(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW6 |= CONNAC2X_TX_DESC_DYNAMIC_BANDWIDTH)

#define HAL_MAC_CONNAC2X_TXD_SET_FR_ANTENNA_ID(_prHwMacTxDesc, _ucAntId) \
TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u4DW6), ((uint8_t)_ucAntId), \
CONNAC2X_TX_DESC_ANTENNA_INDEX_MASK, CONNAC2X_TX_DESC_ANTENNA_INDEX_OFFSET)
#define HAL_MAC_CONNAC2X_TXD_SET_SPE_IDX_SEL(_prHwMacTxDesc, _ucSpeIdxSel) \
TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u4DW6), ((uint8_t)_ucSpeIdxSel), \
CONNAC2X_TX_DESC_SPE_IDX_SEL, CONNAC2X_TX_DESC_SPE_IDX_SEL_OFFSET)
#define HAL_MAC_CONNAC2X_TXD_SET_LDPC(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW6 |= CONNAC2X_TX_DESC_LDPC)

#define HAL_MAC_CONNAC2X_TXD_SET_HE_LTF(_prHwMacTxDesc, _ucHeLtf) \
TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u4DW6), ((uint8_t)_ucHeLtf), \
CONNAC2X_TX_DESC_HE_LTF_MASK, CONNAC2X_TX_DESC_HE_LTF_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_GET_HE_LTF(_prHwMacTxDesc) \
TX_DESC_GET_FIELD(((_prHwMacTxDesc)->u4DW6), \
CONNAC2X_TX_DESC_HE_LTF_MASK, CONNAC2X_TX_DESC_HE_LTF_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_GI_TYPE(_prHwMacTxDesc, _ucGIType) \
TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u4DW6), ((uint8_t)_ucGIType), \
CONNAC2X_TX_DESC_GI_TYPE, CONNAC2X_TX_DESC_GI_TYPE_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_GET_GI_TYPE(_prHwMacTxDesc) \
TX_DESC_GET_FIELD(((_prHwMacTxDesc)->u4DW6), \
CONNAC2X_TX_DESC_GI_TYPE, CONNAC2X_TX_DESC_GI_TYPE_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_FR_RATE(_prHwMacTxDesc, _u2RatetoFixed) \
TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u4DW6), ((uint8_t)_u2RatetoFixed), \
CONNAC2X_TX_DESC_FIXDE_RATE_MASK, CONNAC2X_TX_DESC_FIXDE_RATE_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_SET_SPE_IDX(_prHwMacTxDesc, _ucSpeIdx) \
TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u4DW7), ((uint16_t)_ucSpeIdx), \
CONNAC2X_TX_DESC_SPE_EXT_IDX_MASK, CONNAC2X_TX_DESC_SPE_EXT_IDX_OFFSET)

#define HAL_MAC_CONNAC2X_TXD_IS_HW_AMSDU(_prHwMacTxDesc) \
(((_prHwMacTxDesc)->u4DW7 & CONNAC2X_TX_DESC_HW_AMSDU)?TRUE:FALSE)
#define HAL_MAC_CONNAC2X_TXD_SET_HW_AMSDU(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW7 |= CONNAC2X_TX_DESC_HW_AMSDU)
#define HAL_MAC_CONNAC2X_TXD_UNSET_HW_AMSDU(_prHwMacTxDesc) \
((_prHwMacTxDesc)->u4DW7 &= ~CONNAC2X_TX_DESC_HW_AMSDU)

#define HAL_MAC_CONNAC2X_TXD7_GET_SUB_TYPE(_prHwMacTxDesc) \
TX_DESC_GET_FIELD((_prHwMacTxDesc)->u4DW7, \
CONNAC2X_TX_DESC7_SUB_TYPE_MASK, CONNAC2X_TX_DESC7_SUB_TYPE_OFFSET)

#define HAL_MAC_CONNAC2X_TXD7_SET_SUB_TYPE(_prHwMacTxDesc, _ucSubType) \
TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u4DW7), ((uint8_t)_ucSubType), \
CONNAC2X_TX_DESC7_SUB_TYPE_MASK, CONNAC2X_TX_DESC7_SUB_TYPE_OFFSET)

#define HAL_MAC_CONNAC2X_TXD7_GET_TYPE(_prHwMacTxDesc) \
TX_DESC_GET_FIELD((_prHwMacTxDesc)->u4DW7, \
CONNAC2X_TX_DESC7_TYPE_MASK, CONNAC2X_TX_DESC7_TYPE_OFFSET)

#define HAL_MAC_CONNAC2X_TXD7_SET_TYPE(_prHwMacTxDesc, _ucType) \
TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u4DW7), ((uint8_t)_ucType), \
CONNAC2X_TX_DESC7_TYPE_MASK, CONNAC2X_TX_DESC7_TYPE_OFFSET)

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/


/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* CFG_SUPPORT_CONNAC2X == 1 */
#endif /* _NIC_CONNAC2X_TX_H */

