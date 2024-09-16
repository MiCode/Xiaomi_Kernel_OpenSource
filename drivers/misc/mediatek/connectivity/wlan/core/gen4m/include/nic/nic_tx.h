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
 ** Id: //Department/DaVinci/BRANCHES/
 *			MT6620_WIFI_DRIVER_V2_3/include/nic/nic_tx.h#1
 */

/*! \file   nic_tx.h
 *    \brief  Functions that provide TX operation in NIC's point of view.
 *
 *    This file provides TX functions which are responsible for both Hardware
 *    and Software Resource Management and keep their Synchronization.
 *
 */


#ifndef _NIC_TX_H
#define _NIC_TX_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

#define UNIFIED_MAC_TX_FORMAT               1

#define MAC_TX_RESERVED_FIELD               0

#define NIC_TX_RESOURCE_POLLING_TIMEOUT     256
#define NIC_TX_RESOURCE_POLLING_DELAY_MSEC  5

#define NIC_TX_CMD_INFO_RESERVED_COUNT      4

/* Maximum buffer count for individual HIF TCQ */
#define NIC_TX_PAGE_COUNT_TC0 \
	(NIC_TX_BUFF_COUNT_TC0 * nicTxGetMaxPageCntPerFrame(prAdapter))
#define NIC_TX_PAGE_COUNT_TC1 \
	(NIC_TX_BUFF_COUNT_TC1 * nicTxGetMaxPageCntPerFrame(prAdapter))
#define NIC_TX_PAGE_COUNT_TC2 \
	(NIC_TX_BUFF_COUNT_TC2 * nicTxGetMaxPageCntPerFrame(prAdapter))
#define NIC_TX_PAGE_COUNT_TC3 \
	(NIC_TX_BUFF_COUNT_TC3 * nicTxGetMaxPageCntPerFrame(prAdapter))
#define NIC_TX_PAGE_COUNT_TC4 \
	(NIC_TX_BUFF_COUNT_TC4 * nicTxGetMaxPageCntPerFrame(prAdapter))
#define NIC_TX_PAGE_COUNT_TC5 \
	(NIC_TX_BUFF_COUNT_TC5 * nicTxGetMaxPageCntPerFrame(prAdapter))

#define NIC_TX_BUFF_COUNT_TC0           HIF_TX_BUFF_COUNT_TC0
#define NIC_TX_BUFF_COUNT_TC1           HIF_TX_BUFF_COUNT_TC1
#define NIC_TX_BUFF_COUNT_TC2           HIF_TX_BUFF_COUNT_TC2
#define NIC_TX_BUFF_COUNT_TC3           HIF_TX_BUFF_COUNT_TC3
#define NIC_TX_BUFF_COUNT_TC4           HIF_TX_BUFF_COUNT_TC4
#define NIC_TX_BUFF_COUNT_TC5           HIF_TX_BUFF_COUNT_TC5

#define NIC_TX_RESOURCE_CTRL \
	HIF_TX_RESOURCE_CTRL /* to enable/disable TX resource control */
#define NIC_TX_RESOURCE_CTRL_PLE \
	HIF_TX_RESOURCE_CTRL_PLE /* to enable/disable TX resource control */


#if CFG_ENABLE_FW_DOWNLOAD

#define NIC_TX_INIT_BUFF_COUNT_TC0               8
#define NIC_TX_INIT_BUFF_COUNT_TC1               0
#define NIC_TX_INIT_BUFF_COUNT_TC2               0
#define NIC_TX_INIT_BUFF_COUNT_TC3               0
#define NIC_TX_INIT_BUFF_COUNT_TC4               8
#define NIC_TX_INIT_BUFF_COUNT_TC5               0

#define NIC_TX_INIT_BUFF_SUM			(NIC_TX_INIT_BUFF_COUNT_TC0 + \
						NIC_TX_INIT_BUFF_COUNT_TC1 + \
						NIC_TX_INIT_BUFF_COUNT_TC2 + \
						NIC_TX_INIT_BUFF_COUNT_TC3 + \
						NIC_TX_INIT_BUFF_COUNT_TC4 + \
						NIC_TX_INIT_BUFF_COUNT_TC5)

#define NIC_TX_INIT_PAGE_COUNT_TC0 \
	(NIC_TX_INIT_BUFF_COUNT_TC0 * nicTxGetMaxPageCntPerFrame(prAdapter))
#define NIC_TX_INIT_PAGE_COUNT_TC1 \
	(NIC_TX_INIT_BUFF_COUNT_TC1 * nicTxGetMaxPageCntPerFrame(prAdapter))
#define NIC_TX_INIT_PAGE_COUNT_TC2 \
	(NIC_TX_INIT_BUFF_COUNT_TC2 * nicTxGetMaxPageCntPerFrame(prAdapter))
#define NIC_TX_INIT_PAGE_COUNT_TC3 \
	(NIC_TX_INIT_BUFF_COUNT_TC3 * nicTxGetMaxPageCntPerFrame(prAdapter))
#define NIC_TX_INIT_PAGE_COUNT_TC4 \
	(NIC_TX_INIT_BUFF_COUNT_TC4 * nicTxGetMaxPageCntPerFrame(prAdapter))
#define NIC_TX_INIT_PAGE_COUNT_TC5 \
	(NIC_TX_INIT_BUFF_COUNT_TC5 * nicTxGetMaxPageCntPerFrame(prAdapter))

#endif

/* 4 TODO: The following values shall be got from FW by query CMD */
/*------------------------------------------------------------------------*/
/* Resource Management related information                                */
/*------------------------------------------------------------------------*/
#define NIC_TX_PAGE_SIZE_IS_POWER_OF_2          TRUE
#define NIC_TX_PAGE_SIZE_IN_POWER_OF_2          HIF_TX_PAGE_SIZE_IN_POWER_OF_2
#define NIC_TX_PAGE_SIZE                        HIF_TX_PAGE_SIZE

/* For development only */
/* calculated by MS native 802.11 format */
#define NIC_TX_MAX_SIZE_PER_FRAME		1532
#define NIC_TX_MAX_PAGE_PER_FRAME \
	((NIC_TX_DESC_AND_PADDING_LENGTH + NIC_TX_DESC_HEADER_PADDING_LENGTH + \
	NIC_TX_MAX_SIZE_PER_FRAME + NIC_TX_PAGE_SIZE - 1) / NIC_TX_PAGE_SIZE)

#define NIX_TX_PLE_PAGE_CNT_PER_FRAME		1
#define NIC_TX_LEN_ADDING_LENGTH		8 /*0x8206_C000[15:8] x 4.*/

/*------------------------------------------------------------------------*/
/* Tx descriptor related information                                      */
/*------------------------------------------------------------------------*/

/* Frame Buffer
 *  |<--Tx Descriptor-->|<--Tx descriptor padding-->|
 *  <--802.3/802.11 Header-->|<--Header padding-->|<--Payload-->|
 */

/* Tx descriptor length by format (TXD.FT) */
/* in unit of double word */
#define NIC_TX_DESC_LONG_FORMAT_LENGTH_DW		8
#define NIC_TX_DESC_LONG_FORMAT_LENGTH \
	DWORD_TO_BYTE(NIC_TX_DESC_LONG_FORMAT_LENGTH_DW)
/* in unit of double word */
#define NIC_TX_DESC_SHORT_FORMAT_LENGTH_DW	3
#define NIC_TX_DESC_SHORT_FORMAT_LENGTH \
	DWORD_TO_BYTE(NIC_TX_DESC_SHORT_FORMAT_LENGTH_DW)

/* Tx descriptor padding length (DMA.MICR.TXDSCR_PAD) */
#define NIC_TX_DESC_PADDING_LENGTH_DW       0	/* in unit of double word */
#define NIC_TX_DESC_PADDING_LENGTH \
	DWORD_TO_BYTE(NIC_TX_DESC_PADDING_LENGTH_DW)

#define NIC_TX_DESC_AND_PADDING_LENGTH \
	(NIC_TX_DESC_LONG_FORMAT_LENGTH + NIC_TX_DESC_PADDING_LENGTH)

/* Tx header padding (TXD.HeaderPadding)  */
/* Warning!! To use MAC header padding, every Tx packet must be decomposed */
#define NIC_TX_DESC_HEADER_PADDING_LENGTH       0	/* in unit of bytes */

#define NIC_TX_DESC_PID_RESERVED                0
#define NIC_TX_DESC_DRIVER_PID_MIN              1
#define NIC_TX_DESC_DRIVER_PID_MAX              127

#define NIC_TX_DATA_DEFAULT_RETRY_COUNT_LIMIT   30
#define NIC_TX_MGMT_DEFAULT_RETRY_COUNT_LIMIT   30

/* in unit of ms */
#define NIC_TX_AC_BE_REMAINING_TX_TIME	TX_DESC_TX_TIME_NO_LIMIT
#define NIC_TX_AC_BK_REMAINING_TX_TIME	TX_DESC_TX_TIME_NO_LIMIT
#define NIC_TX_AC_VO_REMAINING_TX_TIME	TX_DESC_TX_TIME_NO_LIMIT
#define NIC_TX_AC_VI_REMAINING_TX_TIME	TX_DESC_TX_TIME_NO_LIMIT
#define NIC_TX_MGMT_REMAINING_TX_TIME		2000

#define NIC_TX_CRITICAL_DATA_TID                7
/*802.1d Voice Traffic,use AC_VO */
#define NIC_TX_PRIORITY_DATA_TID                6

/*Customization: sk_buff mark for special packet that need raise priority */
#define NIC_TX_SKB_PRIORITY_MARK1	0x5a /* customer special value*/
#define NIC_TX_SKB_PRIORITY_MARK_BIT	31 /*Mediatek define, 0x80000000*/
#define NIC_TX_SKB_DUP_DETECT_MARK_BIT	30 /*Mediatek define, 0x40000000*/

#define HW_MAC_TX_DESC_APPEND_T_LENGTH          44
#define NIC_TX_HEAD_ROOM \
	(NIC_TX_DESC_LONG_FORMAT_LENGTH + NIC_TX_DESC_PADDING_LENGTH \
	+ HW_MAC_TX_DESC_APPEND_T_LENGTH)

#define NIC_MSDU_REPORT_DUMP_TIMEOUT		5	/* sec */

/*------------------------------------------------------------------------*/
/* Tx status related information                                          */
/*------------------------------------------------------------------------*/

/* Tx status header & content length */
#define NIC_TX_STATUS_HEADER_LENGTH_DW \
	1       /* in unit of double word */
#define NIC_TX_STATUS_HEADER_LENGTH	\
	DWORD_TO_BYTE(NIC_TX_STATUS_HEADER_LENGTH_DW)
#define NIC_TX_STATUS_LENGTH_DW \
	7       /* in unit of double word */
#define NIC_TX_STATUS_LENGTH \
	DWORD_TO_BYTE(NIC_TX_STATUS_LENGTH_DW)

/*------------------------------------------------------------------------*/
/* Tx descriptor field related information                                */
/*------------------------------------------------------------------------*/
/* DW 0 */
#define TX_DESC_TX_BYTE_COUNT_MASK              BITS(0, 15)
#define TX_DESC_TX_BYTE_COUNT_OFFSET            0

#define TX_DESC_ETHER_TYPE_OFFSET_MASK          BITS(0, 6)
#define TX_DESC_ETHER_TYPE_OFFSET_OFFSET        0
#define TX_DESC_IP_CHKSUM_OFFLOAD               BIT(7)
#define TX_DESC_TCP_UDP_CHKSUM_OFFLOAD          BIT(0)
#define TX_DESC_QUEUE_INDEX_MASK                BITS(2, 6)
#define TX_DESC_QUEUE_INDEX_OFFSET              2
#define TX_DESC_PORT_INDEX                      BIT(7)
#define TX_DESC_PORT_INDEX_OFFSET               7

#define PORT_INDEX_LMAC                         0
#define PORT_INDEX_MCU                          1

/* DW 1 */
#define TX_DESC_WLAN_INDEX_MASK                 BITS(0, 7)
#define TX_DESC_WLAN_INDEX_OFFSET               0
#define TX_DESC_HEADER_FORMAT_MASK              BITS(5, 6)
#define TX_DESC_HEADER_FORMAT_OFFSET            5

#define HEADER_FORMAT_NON_802_11                0	/* Non-802.11 */
#define HEADER_FORMAT_COMMAND                   1	/* Command */
#define HEADER_FORMAT_802_11_NORMAL_MODE \
	2	/* 802.11 (normal mode) */
#define HEADER_FORMAT_802_11_ENHANCE_MODE \
	3	/* 802.11 (Enhancement mode) */
#define HEADER_FORMAT_802_11_MASK               BIT(1)

#define TX_DESC_NON_802_11_MORE_DATA            BIT(0)
#define TX_DESC_NON_802_11_EOSP                 BIT(1)
#define TX_DESC_NON_802_11_REMOVE_VLAN          BIT(2)
#define TX_DESC_NON_802_11_VLAN_FIELD           BIT(3)
#define TX_DESC_NON_802_11_ETHERNET_II          BIT(4)
#define TX_DESC_NOR_802_11_HEADER_LENGTH_MASK   BITS(0, 4)
#define TX_DESC_NOR_802_11_HEADER_LENGTH_OFFSET 0
#define TX_DESC_ENH_802_11_EOSP                 BIT(1)
#define TX_DESC_ENH_802_11_AMSDU                BIT(2)

#define TX_DESC_FORMAT                          BIT(7)
#define TX_DESC_SHORT_FORMAT                    0
#define TX_DESC_LONG_FORMAT                     1

#define TX_DESC_TXD_LENGTH_MASK                 BIT(0)
#define TX_DESC_TXD_LENGTH_OFFSET               0

#define TX_DESC_HEADER_PADDING_LENGTH_MASK      BIT(1)
#define TX_DESC_HEADER_PADDING_LENGTH_OFFSET    1
#define TX_DESC_HEADER_PADDING_MODE             BIT(2)

#define TX_DESC_TXD_EXTEND_LENGTH_MASK          BIT(3)
#define TX_DESC_TXD_EXTEND_LENGTH_OFFSET        3

#define TX_DESC_TXD_UTXB_AMSDU_MASK             BIT(4)
#define TX_DESC_TXD_UTXB_AMSDU_OFFSET           4

#define TX_DESC_TID_MASK                        BITS(5, 7)
#define TX_DESC_TID_OFFSET                      5
#define TX_DESC_TID_NUM                         8

#define TX_DESC_PACKET_FORMAT_MASK              BITS(0, 1) /* SW Field */
#define TX_DESC_PACKET_FORMAT_OFFSET            0
#define TX_DESC_OWN_MAC_MASK                    BITS(2, 7)
#define TX_DESC_OWN_MAC_OFFSET                  2

/* DW 2 */
#define TX_DESC_SUB_TYPE_MASK                   BITS(0, 3)
#define TX_DESC_SUB_TYPE_OFFSET                 0
#define TX_DESC_TYPE_MASK                       BITS(4, 5)
#define TX_DESC_TYPE_OFFSET                     4
#define TX_DESC_NDP                             BIT(6)
#define TX_DESC_NDPA                            BIT(7)

#define TX_DESC_SOUNDING                        BIT(0)
#define TX_DESC_FORCE_RTS_CTS                   BIT(1)
#define TX_DESC_BROADCAST_MULTICAST             BIT(2)
#define TX_DESC_BIP_PROTECTED                   BIT(3)
#define TX_DESC_DURATION_FIELD_CONTROL          BIT(4)
#define TX_DESC_HTC_EXISTS                      BIT(5)
#define TX_DESC_FRAGMENT_MASK                   BITS(6, 7)
#define TX_DESC_FRAGMENT_OFFSET                 6
#define FRAGMENT_FISRT_PACKET                   1
#define FRAGMENT_MIDDLE_PACKET                  2
#define FRAGMENT_LAST_PACKET                    3

#define TX_DESC_REMAINING_MAX_TX_TIME           BITS(0, 7)
#define TX_DESC_TX_TIME_NO_LIMIT                0
/* Unit of life time calculation of Tx descriptor */
#define TX_DESC_LIFE_TIME_UNIT_IN_POWER_OF_2    5
#define TX_DESC_LIFE_TIME_UNIT \
	POWER_OF_2(TX_DESC_LIFE_TIME_UNIT_IN_POWER_OF_2)
#define TX_DESC_POWER_OFFSET_MASK               BITS(0, 4)
#define TX_DESC_BA_DISABLE                      BIT(5)
#define TX_DESC_TIMING_MEASUREMENT              BIT(6)
#define TX_DESC_FIXED_RATE                      BIT(7)

/* DW 3 */
#define TX_DESC_NO_ACK                          BIT(0)
#define TX_DESC_PROTECTED_FRAME                 BIT(1)
#define TX_DESC_EXTEND_MORE_DATA                BIT(2)
#define TX_DESC_EXTEND_EOSP                     BIT(3)

#define TX_DESC_SW_RESERVED_MASK                BITS(4, 5)
#define TX_DESC_SW_RESERVED_OFFSET              4

#define TX_DESC_TX_COUNT_MASK                   BITS(6, 10)
#define TX_DESC_TX_COUNT_OFFSET                 6
#define TX_DESC_TX_COUNT_NO_ATTEMPT             0
#define TX_DESC_TX_COUNT_NO_LIMIT               31
#define TX_DESC_REMAINING_TX_COUNT_MASK         BITS(11, 15)
#define TX_DESC_REMAINING_TX_COUNT_OFFSET       11
#define TX_DESC_SEQUENCE_NUMBER                 BITS(0, 11)
#define TX_DESC_HW_RESERVED_MASK                BITS(12, 13)
#define TX_DESC_HW_RESERVED_OFFSET              12
#define TX_DESC_PN_IS_VALID                     BIT(14)
#define TX_DESC_SN_IS_VALID                     BIT(15)

/* DW 4 */
#define TX_DESC_PN_PART1                        BITS(0, 31)

/* DW 5 */
#define TX_DESC_PACKET_ID                       BIT(0, 7)
#define TX_DESC_TX_STATUS_FORMAT                BIT(0)
#define TX_DESC_TX_STATUS_FORMAT_OFFSET         0
#define TX_DESC_TX_STATUS_TO_MCU                BIT(1)
#define TX_DESC_TX_STATUS_TO_HOST               BIT(2)
#define TX_DESC_DA_SOURCE                       BIT(3)
#define TX_DESC_POWER_MANAGEMENT_CONTROL        BIT(5)
#define TX_DESC_PN_PART2                        BITS(0, 15)

/* DW 6 *//* FR = 1 */
#define TX_DESC_BANDWIDTH_MASK                  BITS(0, 2)
#define TX_DESC_BANDWIDTH_OFFSET                0
#define TX_DESC_DYNAMIC_BANDWIDTH               BIT(3)
#define TX_DESC_ANTENNA_INDEX_MASK              BITS(4, 15)
#define TX_DESC_ANTENNA_INDEX_OFFSET            4

#define TX_DESC_FIXDE_RATE_MASK                 BITS(0, 11)
#define TX_DESC_FIXDE_RATE_OFFSET               0
#define TX_DESC_TX_RATE                         BITS(0, 5)
#define TX_DESC_TX_RATE_OFFSET                  0
#define TX_DESC_TX_MODE                         BITS(6, 8)
#define TX_DESC_TX_MODE_OFFSET                  6
#define TX_DESC_NSTS_MASK                       BITS(9, 10)
#define TX_DESC_NSTS_OFFSET                     9
#define TX_DESC_STBC                            BIT(11)
#define TX_DESC_BF                              BIT(12)
#define TX_DESC_LDPC                            BIT(13)
#define TX_DESC_GUARD_INTERVAL                  BIT(14)
#define TX_DESC_FIXED_RATE_MODE                 BIT(15)

/* DW 7 */
#define TX_DESC_SPE_EXT_IDX_SEL_MASK            BIT(10)
#define TX_DESC_SPE_EXT_IDX_SEL_OFFSET          10
#define TX_DESC_SPE_EXT_IDX_MASK                BITS(11, 15)
#define TX_DESC_SPE_EXT_IDX_OFFSET              11
#define TX_DESC_PSE_FID_MASK                    BITS(0, 13)
#define TX_DESC_PSE_FID_OFFSET                  0
#define TX_DESC_HW_AMSDU                        BIT(14)
#define TX_DESC_HIF_ERR                         BIT(15)

#if CFG_ENABLE_PKT_LIFETIME_PROFILE
#define NIC_TX_TIME_THRESHOLD                       100	/* in unit of ms */
#endif

#define NIC_TX_INIT_CMD_PORT                    HIF_TX_INIT_CMD_PORT

#define NIC_TX_REMAINING_LIFE_TIME              2000	/* in unit of ms */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
/* 3 *//* Session for TX QUEUES */
/* The definition in this ENUM is used to categorize packet's Traffic
 * Class according to the their TID(User Priority).
 * In order to achieve QoS goal, a particular TC should not block the process of
 * another packet with different TC.
 * In current design we will have 5 categories(TCs) of SW resource.
 */
/* TXD_PKT_FORMAT options*/
enum ENUM_TXD_PKT_FORMAT_OPTION {
	TXD_PKT_FORMAT_TXD = 0,	/* TXD only */
	TXD_PKT_FORMAT_TXD_PAYLOAD, /* TXD and paload */
	TXD_PKT_FORMAT_COMMAND,	/* Command */
	TXD_PKT_FORMAT_FWDL,	/* Firmware download */
	TXD_PKT_FORMAT_NUM,
};

/* HIF Tx interrupt status queue index*/
enum ENUM_HIF_TX_INDEX {
	HIF_TX_AC0_INDEX = 0,
	HIF_TX_AC1_INDEX,
	HIF_TX_AC2_INDEX,
	HIF_TX_AC3_INDEX,

	HIF_TX_AC10_INDEX,
	HIF_TX_AC11_INDEX,
	HIF_TX_AC12_INDEX,
	HIF_TX_AC13_INDEX,

	HIF_TX_AC20_INDEX,
	HIF_TX_AC21_INDEX,
	HIF_TX_AC22_INDEX,
	HIF_TX_AC23_INDEX,

	HIF_TX_RSV0_INDEX,
	HIF_TX_RSV1_INDEX,
	HIF_TX_FFA_INDEX,
	HIF_TX_CPU_INDEX,

	HIF_TX_NUM
};

/* LMAC Tx queue index */
enum ENUM_MAC_TXQ_INDEX {
	MAC_TXQ_AC0_INDEX = 0,
	MAC_TXQ_AC1_INDEX,
	MAC_TXQ_AC2_INDEX,
	MAC_TXQ_AC3_INDEX,

	MAC_TXQ_AC10_INDEX,
	MAC_TXQ_AC11_INDEX,
	MAC_TXQ_AC12_INDEX,
	MAC_TXQ_AC13_INDEX,

	MAC_TXQ_AC20_INDEX,
	MAC_TXQ_AC21_INDEX,
	MAC_TXQ_AC22_INDEX,
	MAC_TXQ_AC23_INDEX,

	MAC_TXQ_AC30_INDEX,
	MAC_TXQ_AC31_INDEX,
	MAC_TXQ_AC32_INDEX,
	MAC_TXQ_AC33_INDEX,

	MAC_TXQ_ALTX_0_INDEX,
	MAC_TXQ_BMC_0_INDEX,
	MAC_TXQ_BCN_0_INDEX,
	MAC_TXQ_PSMP_0_INDEX,

	MAC_TXQ_ALTX_1_INDEX,
	MAC_TXQ_BMC_1_INDEX,
	MAC_TXQ_BCN_1_INDEX,
	MAC_TXQ_PSMP_1_INDEX,

	MAC_TXQ_NAF_INDEX,
	MAC_TXQ_NBCN_INDEX,

	MAC_TXQ_NUM
};

/* MCU quque index */
enum ENUM_MCU_Q_INDEX {
	MCU_Q0_INDEX = 0,
	MCU_Q1_INDEX,
	MCU_Q2_INDEX,
	MCU_Q3_INDEX,
	MCU_Q_NUM
};

#define TX_PORT_NUM (TC_NUM)

#define BMC_TC_INDEX TC1_INDEX

/* per-Network Tc Resource index */
enum ENUM_NETWORK_TC_RESOURCE_INDEX {
	/* QoS Data frame, WMM AC index */
	NET_TC_WMM_AC_BE_INDEX = 0,
	NET_TC_WMM_AC_BK_INDEX,
	NET_TC_WMM_AC_VI_INDEX,
	NET_TC_WMM_AC_VO_INDEX,
	/* Mgmt frame */
	NET_TC_MGMT_INDEX,
	/* nonQoS / non StaRec frame (BMC/non-associated frame) */
	NET_TC_BMC_INDEX,

	NET_TC_NUM
};

enum ENUM_TX_STATISTIC_COUNTER {
	TX_MPDU_TOTAL_COUNT = 0,
	TX_INACTIVE_BSS_DROP,
	TX_INACTIVE_STA_DROP,
	TX_FORWARD_OVERFLOW_DROP,
	TX_AP_BORADCAST_DROP,
	TX_STATISTIC_COUNTER_NUM
};

enum ENUM_FIX_BW {
	FIX_BW_NO_FIXED = 0,
	FIX_BW_20 = 4,
	FIX_BW_40,
	FIX_BW_80,
	FIX_BW_160,
	FIX_BW_NUM
};

enum ENUM_MSDU_OPTION {
	MSDU_OPT_NO_ACK = BIT(0),
	MSDU_OPT_NO_AGGREGATE = BIT(1),
	MSDU_OPT_TIMING_MEASURE = BIT(2),
	MSDU_OPT_RCPI_NOISE_STATUS = BIT(3),

	/* Option by Frame Format */
	/* Non-80211 */
	MSDU_OPT_MORE_DATA = BIT(4),
	MSDU_OPT_REMOVE_VLAN = BIT(5),	/* Remove VLAN tag if exists */

	/* 80211-enhanced */
	MSDU_OPT_AMSDU = BIT(6),

	/* 80211-enhanced & Non-80211 */
	MSDU_OPT_EOSP = BIT(7),

	/* Beamform */
	MSDU_OPT_NDP = BIT(8),
	MSDU_OPT_NDPA = BIT(9),
	MSDU_OPT_SOUNDING = BIT(10),

	/* Protection */
	MSDU_OPT_FORCE_RTS = BIT(11),

	/* Security */
	MSDU_OPT_BIP = BIT(12),
	MSDU_OPT_PROTECTED_FRAME = BIT(13),

	/* SW Field */
	MSDU_OPT_SW_DURATION = BIT(14),
	MSDU_OPT_SW_PS_BIT = BIT(15),
	MSDU_OPT_SW_HTC = BIT(16),
	MSDU_OPT_SW_BAR_SN = BIT(17),

	/* Manual Mode */
	MSDU_OPT_MANUAL_FIRST_BIT = BIT(18),

	MSDU_OPT_MANUAL_LIFE_TIME = MSDU_OPT_MANUAL_FIRST_BIT,
	MSDU_OPT_MANUAL_RETRY_LIMIT = BIT(19),
	MSDU_OPT_MANUAL_POWER_OFFSET = BIT(20),
	MSDU_OPT_MANUAL_TX_QUE = BIT(21),
	MSDU_OPT_MANUAL_SN = BIT(22),

	MSDU_OPT_MANUAL_LAST_BIT = MSDU_OPT_MANUAL_SN
};

enum ENUM_MSDU_CONTROL_FLAG {
	MSDU_CONTROL_FLAG_FORCE_TX = BIT(0)
};

enum ENUM_MSDU_RATE_MODE {
	MSDU_RATE_MODE_AUTO = 0,
	MSDU_RATE_MODE_MANUAL_DESC,
	/* The following rate mode is not implemented yet */
	/* DON'T use!!! */
	MSDU_RATE_MODE_MANUAL_CR,
	MSDU_RATE_MODE_LOWEST_RATE
};

enum ENUM_DATA_RATE_MODE {
	DATA_RATE_MODE_AUTO = 0,
	DATA_RATE_MODE_MANUAL,
	DATA_RATE_MODE_BSS_LOWEST
};

struct TX_TCQ_STATUS {
	/* HIF reported page count delta */
	uint32_t au4TxDonePageCount[TC_NUM];	/* other TC */
	uint32_t au4PreUsedPageCount[TC_NUM];
	uint32_t u4AvaliablePageCount;	/* FFA */
	uint8_t ucNextTcIdx;	/* For round-robin distribute free page count */

	/* distributed page count */
	uint32_t au4FreePageCount[TC_NUM];
	uint32_t au4MaxNumOfPage[TC_NUM];

	/* buffer count */
	uint32_t au4FreeBufferCount[TC_NUM];
	uint32_t au4MaxNumOfBuffer[TC_NUM];

	/*
	 * PLE part
	 */

	u_int8_t fgNeedPleCtrl;

	/* HIF reported page count delta */
	uint32_t au4TxDonePageCount_PLE[TC_NUM];	/* other TC */
	uint32_t au4PreUsedPageCoun_PLE[TC_NUM];
	uint32_t u4AvaliablePageCount_PLE;	/* FFA */

	/* distributed page count */
	uint32_t au4FreePageCount_PLE[TC_NUM];
	uint32_t au4MaxNumOfPage_PLE[TC_NUM];

	/* buffer count */
	uint32_t au4FreeBufferCount_PLE[TC_NUM];
	uint32_t au4MaxNumOfBuffer_PLE[TC_NUM];
};

struct TX_TCQ_ADJUST {
	int32_t ai4Variation[TC_NUM];
};

struct TX_CTRL {
	uint32_t u4TxCachedSize;
	uint8_t *pucTxCached;

	uint32_t u4PageSize;

	uint32_t u4TotalPageNum;

	uint32_t u4TotalPageNumPle;
	uint32_t u4TotalTxRsvPageNum;

/* Elements below is classified according to TC (Traffic Class) value. */

	struct TX_TCQ_STATUS rTc;

	uint8_t *pucTxCoalescingBufPtr;

	uint32_t u4WrIdx;

	struct QUE rFreeMsduInfoList;

	/* Management Frame Tracking */
	/* number of management frames to be sent */
	int32_t i4TxMgmtPendingNum;

	/* to tracking management frames need TX done callback */
	struct QUE rTxMgmtTxingQueue;

#if CFG_HIF_STATISTICS
	uint32_t u4TotalTxAccessNum;
	uint32_t u4TotalTxPacketNum;
#endif
	uint32_t au4Statistics[TX_STATISTIC_COUNTER_NUM];

	/* Number to track forwarding frames */
	int32_t i4PendingFwdFrameCount;
	/* Number to track forwarding frames for WMM resource control */
	int32_t i4PendingFwdFrameWMMCount[TC_NUM];

	/* enable/disable TX resource control */
	u_int8_t fgIsTxResourceCtrl;
	/* page counts for a wifi frame */
	uint32_t u4MaxPageCntPerFrame;

	/* Store SysTime of Last TxDone successfully */
	uint32_t u4LastTxTime[MAX_BSSID_NUM];
};

enum ENUM_TX_PACKET_TYPE {
	TX_PACKET_TYPE_DATA = 0,
	TX_PACKET_TYPE_MGMT,
	/* TX_PACKET_TYPE_1X, */
	X_PACKET_TYPE_NUM
};

enum ENUM_TX_PACKET_SRC {
	TX_PACKET_OS,
	TX_PACKET_OS_OID,
	TX_PACKET_FORWARDING,
	TX_PACKET_MGMT,
	TX_PACKET_NUM
};

/* TX Call Back Function  */
typedef uint32_t(*PFN_TX_DONE_HANDLER) (IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo,
	IN enum ENUM_TX_RESULT_CODE rTxDoneStatus);

typedef void(*PFN_HIF_TX_MSDU_DONE_CB) (IN struct ADAPTER
	*prAdapter, IN struct MSDU_INFO *prMsduInfo);

#if CFG_ENABLE_PKT_LIFETIME_PROFILE
struct PKT_PROFILE {
	u_int8_t fgIsValid;
#if CFG_PRINT_PKT_LIFETIME_PROFILE
	u_int8_t fgIsPrinted;
	uint16_t u2IpSn;
	uint16_t u2RtpSn;
	uint8_t ucTcxFreeCount;
#endif
	OS_SYSTIME rHardXmitArrivalTimestamp;
	OS_SYSTIME rEnqueueTimestamp;
	OS_SYSTIME rDequeueTimestamp;
	OS_SYSTIME rHifTxDoneTimestamp;
};
#endif

enum ENUM_EAPOL_KEY_TYPE_T {
	EAPOL_KEY_NOT_KEY = 0,
	EAPOL_KEY_1_OF_4,
	EAPOL_KEY_2_OF_4,
	EAPOL_KEY_3_OF_4,
	EAPOL_KEY_4_OF_4,
	EAPOL_KEY_NUM
};

/* TX transactions could be divided into 4 kinds:
 *
 * 1) 802.1X / Bluetooth-over-Wi-Fi Security Frames
 *    [CMD_INFO_T] - [prPacket] - in skb or NDIS_PACKET form
 *
 * 2) MMPDU
 *    [CMD_INFO_T] - [prPacket] - [MSDU_INFO_T] - [prPacket] - direct
 *    buffer for frame body
 *
 * 3) Command Packets
 *    [CMD_INFO_T] - [pucInfoBuffer] - direct buffer for content
 *    of command packet
 *
 * 4) Normal data frame
 *    [MSDU_INFO_T] - [prPacket] - in skb or NDIS_PACKET form
 */

/* PS_FORWARDING_TYPE_NON_PS means that the receiving STA is in Active Mode
 * from the perspective of host driver
 * (maybe not synchronized with FW --> SN is needed)
 */

struct MSDU_INFO {
	struct QUE_ENTRY rQueEntry;
	void *prPacket;	/* Pointer to packet buffer */

	enum ENUM_TX_PACKET_SRC eSrc;	/* specify OS/FORWARD packet */
	uint8_t ucUserPriority;	/* QoS parameter, convert to TID */

	/* For composing TX descriptor header */
	uint8_t ucTC;		/* Traffic Class: 0~4 (HIF TX0), 5 (HIF TX1) */
	uint8_t ucPacketType;	/* 0: Data, 1: Management Frame */
	uint8_t ucStaRecIndex;	/* STA_REC index */
	uint8_t ucBssIndex;	/* BSS_INFO_T index */
	uint8_t ucWlanIndex;	/* Wlan entry index */
	uint8_t ucPacketFormat;  /* TXD.DW1[25:24] Packet Format */

	u_int8_t fgIs802_1x;	/* TRUE: 802.1x frame */
	/* TRUE: 802.1x frame - Non-Protected */
	u_int8_t fgIs802_1x_NonProtected;
	u_int8_t fgIs802_11;	/* TRUE: 802.11 header is present */
	u_int8_t fgIs802_3;	/* TRUE: 802.3 frame */
	u_int8_t fgIsVlanExists;	/* TRUE: VLAN tag is exists */

	/* Special Option */
	uint32_t u4Option;	/* Special option in bitmask, no ACK, etc... */
	int8_t cPowerOffset;	/* Per-packet power offset, in 2's complement */
	uint16_t u2SwSN;		/* SW assigned sequence number */
	uint8_t ucRetryLimit;	/* The retry limit */
	uint32_t u4RemainingLifetime;	/* Remaining lifetime, unit:ms */

	/* Control flag */
	uint8_t ucControlFlag;	/* Control flag in bitmask */

	/* Fixed Rate Option */
	uint8_t ucRateMode;	/* Rate mode: AUTO, MANUAL_DESC, MANUAL_CR */
	/* The rate option, rate code, GI, etc... */
	uint32_t u4FixedRateOption;

	/* There is a valid Tx descriptor for this packet */
	u_int8_t fgIsTXDTemplateValid;

	/* flattened from PACKET_INFO_T */
	uint8_t ucMacHeaderLength;	/* MAC header legth */
	uint8_t ucLlcLength;	/* w/o EtherType */
	uint16_t u2FrameLength;	/* Total frame length */
	/* Ethernet Destination Address */
	uint8_t aucEthDestAddr[MAC_ADDR_LEN];
	uint32_t u4PageCount;	/* Required page count for this MSDU */

	/* for TX done tracking */
	uint8_t ucTxSeqNum;	/* MGMT frame serial number */
	uint8_t ucPID;		/* PID */
	uint8_t ucWmmQueSet;	/* WMM Set */
	PFN_TX_DONE_HANDLER pfTxDoneHandler;	/* Tx done handler */
	PFN_HIF_TX_MSDU_DONE_CB pfHifTxMsduDoneCb;
	uint32_t u4TxDoneTag;	/* Tag for data frame Tx done log */
	uint8_t ucPktType;

#if CFG_ENABLE_PKT_LIFETIME_PROFILE
	struct PKT_PROFILE rPktProfile;
#endif

	/* To be removed  */
	uint8_t ucFormatID; /* 0: MAUI, Linux, Windows NDIS 5.1 */
	/* UINT_16 u2PalLLH; */ /* PAL Logical Link Header (for BOW network) */
	/* UINT_16 u2AclSN; */ /* ACL Sequence Number (for BOW network) */
	uint8_t ucPsForwardingType; /* See ENUM_PS_FORWARDING_TYPE_T */
	/* PS Session ID specified by the FW for the STA */
	/* UINT_8 ucPsSessionID; */
	/* TRUE means this is the last packet of the burst for (STA, TID) */
	/* BOOLEAN fgIsBurstEnd; */
#if CFG_M0VE_BA_TO_DRIVER
	uint8_t ucTID;
#endif

#if CFG_SUPPORT_MULTITHREAD
	/* Compose TxDesc in main_thread and place here */
	uint8_t aucTxDescBuffer[NIC_TX_DESC_AND_PADDING_LENGTH];
#endif

#if defined(_HIF_PCIE) || defined(_HIF_AXI)
	struct MSDU_TOKEN_ENTRY *prToken;
	struct TX_DATA_REQ rTxReq;
#endif
	enum ENUM_EAPOL_KEY_TYPE_T eEapolKeyType;
#if (CFG_SUPPORT_DMASHDL_SYSDVT)
	uint8_t ucTarQueue;
#endif
	uint8_t fgMgmtUseDataQ;
};

#define HIF_PKT_FLAGS_CT_INFO_APPLY_TXD            BIT(0)
#define HIF_PKT_FLAGS_COPY_HOST_TXD_ALL		BIT(1)
#define HIF_PKT_FLAGS_CT_INFO_MGN_FRAME            BIT(2)
#define HIF_PKT_FLAGS_CT_INFO_NONE_CIPHER_FRAME    BIT(3)
#define HIF_PKT_FLAGS_CT_INFO_HSR2_TX              BIT(4)

#define MAX_BUF_NUM_PER_PKT	6

#define NUM_OF_MSDU_ID_IN_TXD   4
#define TXD_MAX_BUF_NUM         4
#define TXD_MSDU_ID_VLD         BIT(15)     /* MSDU valid */
#define TXD_LEN_AL              BIT(15)     /* A-MSDU last */
#define TXD_LEN_ML              BIT(14)     /* MSDU last */
#define TXD_LEN_ML_V2           BIT(15)     /* MSDU last */
#define TXD_LEN_MASK_V2         BITS(0, 11)
#define TXD_ADDR2_MASK          BITS(12, 14)
#define TXD_ADDR2_OFFSET        20


struct TXD_PTR_LEN {
	uint32_t u4Ptr0;
	uint16_t u2Len0;         /* Bit15: AL, Bit14: ML */
	uint16_t u2Len1;         /* Bit15: AL, Bit14: ML */
	uint32_t u4Ptr1;
};

union HW_MAC_TX_DESC_APPEND {
	struct {
		uint16_t u2PktFlags;
		uint16_t u2MsduToken;
		uint8_t ucBssIndex;
		uint8_t ucWtblIndex;
		uint8_t aucReserved[1];
		uint8_t ucBufNum;
		uint32_t au4BufPtr[MAX_BUF_NUM_PER_PKT];
		uint16_t au2BufLen[MAX_BUF_NUM_PER_PKT];
	} CR4_APPEND;

	struct {
		/* Bit15 indicate valid */
		uint16_t au2MsduId[NUM_OF_MSDU_ID_IN_TXD];
		struct TXD_PTR_LEN arPtrLen[TXD_MAX_BUF_NUM / 2];
	} CONNAC_APPEND;
};

/*!A data structure which is identical with HW MAC TX DMA Descriptor */
struct HW_MAC_TX_DESC {
	/* DW 0 */
	uint16_t u2TxByteCount;
	uint8_t ucEtherOffset;	/* Ether-Type Offset,  IP checksum offload */
	/* UDP/TCP checksum offload,
	 * USB NextVLD/TxBURST, Queue index, Port index
	 */
	uint8_t ucPortIdx_QueueIdx;
	/* DW 1 */
	uint8_t ucWlanIdx;
	/* Header format, TX descriptor format */
	uint8_t ucHeaderFormat;
	/* Header padding, no ACK, TID, Protect frame */
	uint8_t ucHeaderPadding;
	uint8_t ucOwnMAC;

	/* Long Format, the following structure is for long format ONLY */
	/* DW 2 */
	uint8_t ucType_SubType;	/* Type, Sub-type, NDP, NDPA */
	/* Sounding, force RTS/CTS, BMC, BIP, Duration, HTC exist, Fragment */
	uint8_t ucFrag;
	uint8_t ucRemainingMaxTxTime;
	/* Power offset, Disable BA, Timing measurement, Fixed rate */
	uint8_t ucPowerOffset;
	/* DW 3 */
	uint16_t u2TxCountLimit;	/* TX count limit */
	uint16_t u2SN;		/* SN, HW own, PN valid, SN valid */
	/* DW 4 */
	uint32_t u4PN1;
	/* DW 5 */
	uint8_t ucPID;
	/* TXS format, TXS to mcu,
	 * TXS to host, DA source, BAR SSN, Power management
	 */
	uint8_t ucTxStatus;
	uint16_t u2PN2;
	/* DW 6 */
	uint16_t u2AntID;	/* Fixed rate, Antenna ID */
	/* Explicit/implicit beamforming, Fixed rate table, LDPC, GI */
	uint16_t u2FixedRate;
	/* DW 7 */
	uint16_t u2SwTxTime;	/* Sw Tx time[9:0], SPE_IDX[15:11] */
	uint16_t u2PseFid;	/* indicate frame ID in PSE for this TXD */
};

struct TX_RESOURCE_CONTROL {
	/* HW TX queue definition */
	uint8_t ucDestPortIndex;
	uint8_t ucDestQueueIndex;
	/* HIF Interrupt status index */
	uint8_t ucHifTxQIndex;
};

struct TX_TC_TRAFFIC_SETTING {
	uint32_t u4TxDescLength;
	uint32_t u4RemainingTxTime;
	uint8_t ucTxCountLimit;
};

typedef void (*PFN_TX_DATA_DONE_CB) (IN struct GLUE_INFO *prGlueInfo,
	IN struct QUE *prQue);

struct tx_resource_info {
	/* PSE */
	/* the total usable resource for MCU port */
	uint32_t u4CmdTotalResource;
	/* the unit of a MCU resource */
	uint32_t u4CmdResourceUnit;
	/* the total usable resource for LMAC port */
	uint32_t u4DataTotalResource;
	/* the unit of a LMAC resource */
	uint32_t u4DataResourceUnit;

	/* PLE */
	/* the total usable resource for MCU port */
	uint32_t u4CmdTotalResourcePle;
	/* the unit of a MCU resource */
	uint32_t u4CmdResourceUnitPle;
	/* the total usable resource for LMAC port */
	uint32_t u4DataTotalResourcePle;
	/* the unit of a LMAC resource */
	uint32_t u4DataResourceUnitPle;

	/* Packet Processor 0x8206C000[15:8]
	 * 4. Extra PSE resource is needed for HW.
	 */
	uint8_t  ucPpTxAddCnt;/* in unit of byte */

	/* update resource callback */
	void (*txResourceInit)(IN struct ADAPTER *prAdapter);
};

struct TX_DESC_OPS_T {
	void (*fillNicAppend)(IN struct ADAPTER *prAdapter,
		IN struct MSDU_INFO *prMsduInfo,
		OUT uint8_t *prTxDescBuffer);
	void (*fillHifAppend)(IN struct ADAPTER *prAdapter,
		IN struct MSDU_INFO *prMsduInfo, IN uint16_t u4MsduId,
		IN dma_addr_t rDmaAddr, IN uint32_t u4Idx, IN u_int8_t fgIsLast,
		OUT uint8_t *pucBuffer);
	void (*fillTxByteCount)(IN struct ADAPTER *prAdapter,
		IN struct MSDU_INFO *prMsduInfo,
		void *prTxDesc);

	/* TXD Handle APIs */
	uint8_t (*nic_txd_long_format_op)(
		void *prTxDesc,
		uint8_t fgSet);
	uint8_t (*nic_txd_tid_op)(
		void *prTxDesc,
		uint8_t ucTid,
		uint8_t fgSet);
	uint8_t (*nic_txd_queue_idx_op)(
		void *prTxDesc,
		uint8_t ucQueIdx,
		uint8_t fgSet);
#if (CFG_TCP_IP_CHKSUM_OFFLOAD == 1)
	void (*nic_txd_chksum_op)(
		void *prTxDesc,
		uint8_t ucChksumFlag);
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD == 1 */
	void (*nic_txd_header_format_op)(
		void *prTxDesc,
		struct MSDU_INFO *prMsduInfo);
	void (*nic_txd_fill_by_pkt_option)(
		struct MSDU_INFO *prMsduInfo,
		void *prTxD);
	void (*nic_txd_compose)(
		struct ADAPTER *prAdapter,
		struct MSDU_INFO *prMsduInfo,
		u_int32_t u4TxDescLength,
		u_int8_t fgIsTemplate,
		u_int8_t *prTxDescBuffer);
	void (*nic_txd_compose_security_frame)(
		struct ADAPTER *prAdapter,
		struct CMD_INFO *prCmdInfo,
		uint8_t *prTxDescBuffer,
		uint8_t *pucTxDescLength);
	void (*nic_txd_set_pkt_fixed_rate_option_full)(
		struct MSDU_INFO *prMsduInfo,
		uint16_t u2RateCode,
		uint8_t ucBandwidth,
		u_int8_t fgShortGI,
		u_int8_t fgLDPC,
		u_int8_t fgDynamicBwRts, u_int8_t fgBeamforming,
		uint8_t ucAntennaIndex);
	void (*nic_txd_set_pkt_fixed_rate_option)(
		struct MSDU_INFO *prMsduInfo,
		uint16_t u2RateCode,
		uint8_t ucBandwidth,
		u_int8_t fgShortGI,
		u_int8_t fgDynamicBwRts);
	void (*nic_txd_set_hw_amsdu_template)(
		struct ADAPTER *prAdapter,
		struct STA_RECORD *prStaRec,
		uint8_t ucTid,
		u_int8_t fgSet);
	void (*nic_txd_change_data_port_by_ac)(
		struct STA_RECORD *prStaRec,
		uint8_t ucAci,
		u_int8_t fgToMcu);
};

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

extern PFN_TX_DATA_DONE_CB g_pfTxDataDoneCb;

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

#define TX_INC_CNT(prTxCtrl, eCounter)              \
	{((struct TX_CTRL *)prTxCtrl)->au4Statistics[eCounter]++; }

#define TX_ADD_CNT(prTxCtrl, eCounter, u8Amount)    \
	{((struct TX_CTRL *)prTxCtrl)->au4Statistics[eCounter] += \
	(uint32_t)u8Amount; }

#define TX_GET_CNT(prTxCtrl, eCounter)              \
	(((struct TX_CTRL *)prTxCtrl)->au4Statistics[eCounter])

#define TX_RESET_ALL_CNTS(prTxCtrl)                 \
	{kalMemZero(&prTxCtrl->au4Statistics[0], \
	sizeof(prTxCtrl->au4Statistics)); }
#if CFG_ENABLE_PKT_LIFETIME_PROFILE

#if CFG_PRINT_PKT_LIFETIME_PROFILE
#define PRINT_PKT_PROFILE(_pkt_profile, _note) \
do { \
	if (!(_pkt_profile)->fgIsPrinted) { \
		DBGLOG(TX, TRACE, \
		"X[%lu] E[%lu] D[%lu] HD[%lu] B[%d] RTP[%d] %s\n", \
		(uint32_t)((_pkt_profile)->rHardXmitArrivalTimestamp), \
		(uint32_t)((_pkt_profile)->rEnqueueTimestamp), \
		(uint32_t)((_pkt_profile)->rDequeueTimestamp), \
		(uint32_t)((_pkt_profile)->rHifTxDoneTimestamp), \
		(uint8_t)((_pkt_profile)->ucTcxFreeCount), \
		(uint16_t)((_pkt_profile)->u2RtpSn), \
		(_note))); \
		(_pkt_profile)->fgIsPrinted = TRUE; \
	} \
} while (0)
#else
#define PRINT_PKT_PROFILE(_pkt_profile, _note)
#endif

#define CHK_PROFILES_DELTA(_pkt1, _pkt2, _delta) \
	(CHECK_FOR_TIMEOUT((_pkt1)->rHardXmitArrivalTimestamp, \
		(_pkt2)->rHardXmitArrivalTimestamp, (_delta)) || \
	CHECK_FOR_TIMEOUT((_pkt1)->rEnqueueTimestamp, \
		(_pkt2)->rEnqueueTimestamp, (_delta)) || \
	CHECK_FOR_TIMEOUT((_pkt1)->rDequeueTimestamp, \
		(_pkt2)->rDequeueTimestamp, (_delta)) || \
	CHECK_FOR_TIMEOUT((_pkt1)->rHifTxDoneTimestamp, \
		(_pkt2)->rHifTxDoneTimestamp, (_delta)))

#define CHK_PROFILE_DELTA(_pkt, _delta) \
	(CHECK_FOR_TIMEOUT((_pkt)->rEnqueueTimestamp, \
		(_pkt)->rHardXmitArrivalTimestamp, (_delta)) || \
	CHECK_FOR_TIMEOUT((_pkt)->rDequeueTimestamp, \
		(_pkt)->rEnqueueTimestamp, (_delta)) || \
	CHECK_FOR_TIMEOUT((_pkt)->rHifTxDoneTimestamp, \
		(_pkt)->rDequeueTimestamp, (_delta)))
#endif

/*------------------------------------------------------------------------------
 * MACRO for MSDU_INFO
 *------------------------------------------------------------------------------
 */
#define TX_SET_MMPDU            nicTxSetMngPacket
#define TX_SET_DATA_PACKET      nicTxSetDataPacket

/*------------------------------------------------------------------------------
 * MACRO for HW_MAC_TX_DESC_T
 *------------------------------------------------------------------------------
 */
#define TX_DESC_GET_FIELD(_rHwMacTxDescField, _mask, _offset) \
	(((_rHwMacTxDescField) & (_mask)) >> (_offset))
#define TX_DESC_SET_FIELD(_rHwMacTxDescField, _value, _mask, _offset)	\
{ \
	(_rHwMacTxDescField) &= ~(_mask); \
	(_rHwMacTxDescField) |= (((_value) << (_offset)) & (_mask)); \
}

#define HAL_MAC_TX_DESC_SET_DW(_prHwMacTxDesc, _ucOffsetInDw, \
	 _ucLengthInDw, _pucValueAddr) \
	kalMemCopy((uint32_t *)(_prHwMacTxDesc) + (_ucOffsetInDw),	\
	(uint8_t *)(_pucValueAddr), DWORD_TO_BYTE(_ucLengthInDw))
#define HAL_MAC_TX_DESC_GET_DW(_prHwMacTxDesc, _ucOffsetInDw, \
	_ucLengthInDw, _pucValueAddr) \
	kalMemCopy((uint8_t *)(_pucValueAddr),		\
	(uint32_t *)(_prHwMacTxDesc) + (_ucOffsetInDw), \
	DWORD_TO_BYTE(_ucLengthInDw))

/* DW 0 */
#define HAL_MAC_TX_DESC_GET_TX_BYTE_COUNT(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2TxByteCount)
#define HAL_MAC_TX_DESC_SET_TX_BYTE_COUNT(_prHwMacTxDesc, _u2TxByteCount) \
	(((_prHwMacTxDesc)->u2TxByteCount) = ((uint16_t)_u2TxByteCount))

#define HAL_MAC_TX_DESC_GET_ETHER_TYPE_OFFSET(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucEtherOffset, \
	TX_DESC_ETHER_TYPE_OFFSET_MASK, \
	TX_DESC_ETHER_TYPE_OFFSET_OFFSET)
#define HAL_MAC_TX_DESC_SET_ETHER_TYPE_OFFSET(_prHwMacTxDesc, \
	_ucEtherTypeOffset) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucEtherOffset), \
	((uint8_t)_ucEtherTypeOffset), \
	TX_DESC_ETHER_TYPE_OFFSET_MASK, TX_DESC_ETHER_TYPE_OFFSET_OFFSET)

#define HAL_MAC_TX_DESC_IS_IP_CHKSUM_ENABLED(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucEtherOffset & TX_DESC_IP_CHKSUM_OFFLOAD) \
	? FALSE : TRUE)
#define HAL_MAC_TX_DESC_SET_IP_CHKSUM(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucEtherOffset |= TX_DESC_IP_CHKSUM_OFFLOAD)
#define HAL_MAC_TX_DESC_UNSET_IP_CHKSUM(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucEtherOffset &= ~TX_DESC_IP_CHKSUM_OFFLOAD)

#define HAL_MAC_TX_DESC_IS_TCP_UDP_CHKSUM_ENABLED(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucPortIdx_QueueIdx & \
	TX_DESC_TCP_UDP_CHKSUM_OFFLOAD) ? FALSE : TRUE)
#define HAL_MAC_TX_DESC_SET_TCP_UDP_CHKSUM(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucPortIdx_QueueIdx |= TX_DESC_TCP_UDP_CHKSUM_OFFLOAD)
#define HAL_MAC_TX_DESC_UNSET_TCP_UDP_CHKSUM(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucPortIdx_QueueIdx &= \
	~TX_DESC_TCP_UDP_CHKSUM_OFFLOAD)

#if 0 /* USB HIF doesn't use this field. */
#define HAL_MAC_TX_DESC_IS_USB_NEXT_VLD_ENABLED(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucPortIdx_QueueIdx & TX_DESC_USB_NEXT_VLD) \
	? TRUE : FALSE)
#define HAL_MAC_TX_DESC_SET_USB_NEXT_VLD(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucPortIdx_QueueIdx |= TX_DESC_USB_NEXT_VLD)
#define HAL_MAC_TX_DESC_UNSET_USB_NEXT_VLD(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucPortIdx_QueueIdx &= ~TX_DESC_USB_NEXT_VLD)
#endif /* if 0 */

#define HAL_MAC_TX_DESC_GET_QUEUE_INDEX(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucPortIdx_QueueIdx, \
	TX_DESC_QUEUE_INDEX_MASK, TX_DESC_QUEUE_INDEX_OFFSET)
#define HAL_MAC_TX_DESC_SET_QUEUE_INDEX(_prHwMacTxDesc, _ucQueueIndex) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucPortIdx_QueueIdx), \
	((uint8_t)_ucQueueIndex), \
	TX_DESC_QUEUE_INDEX_MASK, TX_DESC_QUEUE_INDEX_OFFSET)

#define HAL_MAC_TX_DESC_GET_PORT_INDEX(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucPortIdx_QueueIdx, \
	TX_DESC_PORT_INDEX, TX_DESC_PORT_INDEX_OFFSET)
#define HAL_MAC_TX_DESC_SET_PORT_INDEX(_prHwMacTxDesc, _ucPortIndex) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucPortIdx_QueueIdx), \
	((uint8_t)_ucPortIndex), \
	TX_DESC_PORT_INDEX, TX_DESC_PORT_INDEX_OFFSET)

/* DW 1 */
#define HAL_MAC_TX_DESC_GET_WLAN_INDEX(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucWlanIdx)
#define HAL_MAC_TX_DESC_SET_WLAN_INDEX(_prHwMacTxDesc, _ucWlanIdx) \
	(((_prHwMacTxDesc)->ucWlanIdx) = (_ucWlanIdx))

#define HAL_MAC_TX_DESC_IS_LONG_FORMAT(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucHeaderFormat & TX_DESC_FORMAT)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_LONG_FORMAT(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucHeaderFormat |= TX_DESC_FORMAT)
#define HAL_MAC_TX_DESC_SET_SHORT_FORMAT(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucHeaderFormat &= ~TX_DESC_FORMAT)

#define HAL_MAC_TX_DESC_GET_HEADER_FORMAT(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucHeaderFormat, \
	TX_DESC_HEADER_FORMAT_MASK, TX_DESC_HEADER_FORMAT_OFFSET)
#define HAL_MAC_TX_DESC_SET_HEADER_FORMAT(_prHwMacTxDesc, _ucHdrFormat) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucHeaderFormat), \
	((uint8_t)_ucHdrFormat), TX_DESC_HEADER_FORMAT_MASK, \
	TX_DESC_HEADER_FORMAT_OFFSET)

/* HF = 0x00, 802.11 normal mode */
#define HAL_MAC_TX_DESC_IS_MORE_DATA(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucHeaderFormat & TX_DESC_NON_802_11_MORE_DATA) \
	? TRUE : FALSE)
#define HAL_MAC_TX_DESC_SET_MORE_DATA(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucHeaderFormat |= TX_DESC_NON_802_11_MORE_DATA)
#define HAL_MAC_TX_DESC_UNSET_MORE_DATA(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucHeaderFormat &= ~TX_DESC_NON_802_11_MORE_DATA)

#define HAL_MAC_TX_DESC_IS_REMOVE_VLAN(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucHeaderFormat & TX_DESC_NON_802_11_REMOVE_VLAN) \
	? TRUE : FALSE)
#define HAL_MAC_TX_DESC_SET_REMOVE_VLAN(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucHeaderFormat |= TX_DESC_NON_802_11_REMOVE_VLAN)
#define HAL_MAC_TX_DESC_UNSET_REMOVE_VLAN(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucHeaderFormat &= ~TX_DESC_NON_802_11_REMOVE_VLAN)

#define HAL_MAC_TX_DESC_IS_VLAN(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucHeaderFormat & TX_DESC_NON_802_11_VLAN_FIELD) \
	? TRUE : FALSE)
#define HAL_MAC_TX_DESC_SET_VLAN(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucHeaderFormat |= TX_DESC_NON_802_11_VLAN_FIELD)
#define HAL_MAC_TX_DESC_UNSET_VLAN(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucHeaderFormat &= ~TX_DESC_NON_802_11_VLAN_FIELD)

#define HAL_MAC_TX_DESC_IS_ETHERNET_II(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucHeaderFormat & TX_DESC_NON_802_11_ETHERNET_II) \
	? TRUE : FALSE)
#define HAL_MAC_TX_DESC_SET_ETHERNET_II(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucHeaderFormat |= TX_DESC_NON_802_11_ETHERNET_II)
#define HAL_MAC_TX_DESC_UNSET_ETHERNET_II(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucHeaderFormat &= ~TX_DESC_NON_802_11_ETHERNET_II)

/* HF = 0x00/0x11, 802.11 normal/enhancement mode */
#define HAL_MAC_TX_DESC_IS_EOSP(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucHeaderFormat & TX_DESC_NON_802_11_EOSP) \
	? TRUE : FALSE)
#define HAL_MAC_TX_DESC_SET_EOSP(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucHeaderFormat |= TX_DESC_NON_802_11_EOSP)
#define HAL_MAC_TX_DESC_UNSET_EOSP(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucHeaderFormat &= ~TX_DESC_NON_802_11_EOSP)

/* HF = 0x11, 802.11 enhancement mode */
#define HAL_MAC_TX_DESC_IS_AMSDU(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucHeaderFormat & TX_DESC_ENH_802_11_AMSDU) \
	? TRUE : FALSE)
#define HAL_MAC_TX_DESC_SET_AMSDU(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucHeaderFormat |= TX_DESC_ENH_802_11_AMSDU)
#define HAL_MAC_TX_DESC_UNSET_AMSDU(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucHeaderFormat &= ~TX_DESC_ENH_802_11_AMSDU)

/* HF = 0x10, non-802.11 */
#define HAL_MAC_TX_DESC_GET_802_11_HEADER_LENGTH(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucHeaderFormat, \
	TX_DESC_NOR_802_11_HEADER_LENGTH_MASK, \
	TX_DESC_NOR_802_11_HEADER_LENGTH_OFFSET)
#define HAL_MAC_TX_DESC_SET_802_11_HEADER_LENGTH(_prHwMacTxDesc, \
	 _ucHdrLength) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucHeaderFormat), \
	((uint8_t)_ucHdrLength), \
	TX_DESC_NOR_802_11_HEADER_LENGTH_MASK, \
	TX_DESC_NOR_802_11_HEADER_LENGTH_OFFSET)

#define HAL_MAC_TX_DESC_GET_TXD_LENGTH(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucHeaderPadding, \
	TX_DESC_TXD_LENGTH_MASK, TX_DESC_TXD_LENGTH_OFFSET)
#define HAL_MAC_TX_DESC_SET_TXD_LENGTH(_prHwMacTxDesc, _ucHdrPadding) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucHeaderPadding), \
	((uint8_t)_ucHdrPadding), \
	TX_DESC_TXD_LENGTH_MASK, TX_DESC_TXD_LENGTH_OFFSET)

#define HAL_MAC_TX_DESC_GET_TXD_EXTEND_LENGTH(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucHeaderPadding, \
	TX_DESC_TXD_EXTEND_LENGTH_MASK, \
	TX_DESC_TXD_EXTEND_LENGTH_OFFSET)
#define HAL_MAC_TX_DESC_SET_TXD_EXTEND_LENGTH(_prHwMacTxDesc, _ucHdrPadding) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucHeaderPadding), \
	((uint8_t)_ucHdrPadding), \
	TX_DESC_TXD_EXTEND_LENGTH_MASK, TX_DESC_TXD_EXTEND_LENGTH_OFFSET)

#define HAL_MAC_TX_DESC_GET_HEADER_PADDING(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucHeaderPadding, \
	TX_DESC_HEADER_PADDING_LENGTH_MASK, \
	TX_DESC_HEADER_PADDING_LENGTH_OFFSET)
#define HAL_MAC_TX_DESC_SET_HEADER_PADDING(_prHwMacTxDesc, _ucHdrPadding) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucHeaderPadding), \
	((uint8_t)_ucHdrPadding), \
	TX_DESC_HEADER_PADDING_LENGTH_MASK, \
	TX_DESC_HEADER_PADDING_LENGTH_OFFSET)

#define HAL_MAC_TX_DESC_GET_UTXB_AMSDU(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucHeaderPadding, \
	TX_DESC_TXD_UTXB_AMSDU_MASK, TX_DESC_TXD_UTXB_AMSDU_OFFSET)
#define HAL_MAC_TX_DESC_SET_UTXB_AMSDU(_prHwMacTxDesc, _ucHdrPadding) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucHeaderPadding), \
	((uint8_t)_ucHdrPadding), \
	TX_DESC_TXD_UTXB_AMSDU_MASK, TX_DESC_TXD_UTXB_AMSDU_OFFSET)

#define HAL_MAC_TX_DESC_IS_HEADER_PADDING_IN_THE_HEAD(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucHeaderPadding & TX_DESC_HEADER_PADDING_MODE) \
	? TRUE : FALSE)
#define HAL_MAC_TX_DESC_SET_HEADER_PADDING_IN_THE_HEAD(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucHeaderPadding |= TX_DESC_HEADER_PADDING_MODE)
#define HAL_MAC_TX_DESC_SET_HEADER_PADDING_IN_THE_TAIL(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucHeaderPadding &= ~TX_DESC_HEADER_PADDING_MODE)

#define HAL_MAC_TX_DESC_GET_TID(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucHeaderPadding, \
	TX_DESC_TID_MASK, TX_DESC_TID_OFFSET)
#define HAL_MAC_TX_DESC_SET_TID(_prHwMacTxDesc, _ucTID) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucHeaderPadding), \
	((uint8_t)_ucTID), TX_DESC_TID_MASK, TX_DESC_TID_OFFSET)

#define HAL_MAC_TX_DESC_GET_PKT_FORMAT(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucOwnMAC, \
	TX_DESC_PACKET_FORMAT_MASK, \
	TX_DESC_PACKET_FORMAT_OFFSET)
#define HAL_MAC_TX_DESC_SET_PKT_FORMAT(_prHwMacTxDesc, _ucPktFormat) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucOwnMAC), \
	((uint8_t)_ucPktFormat), \
	TX_DESC_PACKET_FORMAT_MASK, TX_DESC_PACKET_FORMAT_OFFSET)

#define HAL_MAC_TX_DESC_GET_OWN_MAC_INDEX(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucOwnMAC, \
	TX_DESC_OWN_MAC_MASK, TX_DESC_OWN_MAC_OFFSET)
#define HAL_MAC_TX_DESC_SET_OWN_MAC_INDEX(_prHwMacTxDesc, _ucOwnMacIdx) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucOwnMAC), \
	((uint8_t)_ucOwnMacIdx), \
	TX_DESC_OWN_MAC_MASK, TX_DESC_OWN_MAC_OFFSET)

/* DW 2 */
#define HAL_MAC_TX_DESC_GET_SUB_TYPE(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucType_SubType, \
	TX_DESC_SUB_TYPE_MASK, TX_DESC_SUB_TYPE_OFFSET)
#define HAL_MAC_TX_DESC_SET_SUB_TYPE(_prHwMacTxDesc, _ucSubType) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucType_SubType), \
	((uint8_t)_ucSubType), \
	TX_DESC_SUB_TYPE_MASK, TX_DESC_SUB_TYPE_OFFSET)

#define HAL_MAC_TX_DESC_GET_TYPE(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucType_SubType, \
	TX_DESC_TYPE_MASK, TX_DESC_TYPE_OFFSET)
#define HAL_MAC_TX_DESC_SET_TYPE(_prHwMacTxDesc, _ucType) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucType_SubType), \
	((uint8_t)_ucType), TX_DESC_TYPE_MASK, TX_DESC_TYPE_OFFSET)

#define HAL_MAC_TX_DESC_IS_NDP(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucType_SubType & TX_DESC_NDP)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_NDP(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucType_SubType |= TX_DESC_NDP)
#define HAL_MAC_TX_DESC_UNSET_NDP(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucType_SubType &= ~TX_DESC_NDP)

#define HAL_MAC_TX_DESC_IS_NDPA(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucType_SubType & TX_DESC_NDPA)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_NDPA(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucType_SubType |= TX_DESC_NDPA)
#define HAL_MAC_TX_DESC_UNSET_NDPA(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucType_SubType &= ~TX_DESC_NDPA)

#define HAL_MAC_TX_DESC_IS_SOUNDING_FRAME(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucFrag & TX_DESC_SOUNDING)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_SOUNDING_FRAME(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucFrag |= TX_DESC_SOUNDING)
#define HAL_MAC_TX_DESC_UNSET_SOUNDING_FRAME(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucFrag &= ~TX_DESC_SOUNDING)

#define HAL_MAC_TX_DESC_IS_FORCE_RTS_CTS_EN(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucFrag & TX_DESC_FORCE_RTS_CTS)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_FORCE_RTS_CTS(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucFrag |= TX_DESC_FORCE_RTS_CTS)
#define HAL_MAC_TX_DESC_UNSET_FORCE_RTS_CTS(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucFrag &= ~TX_DESC_FORCE_RTS_CTS)

#define HAL_MAC_TX_DESC_IS_BMC(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucFrag & TX_DESC_BROADCAST_MULTICAST)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_BMC(_prHwMacTxDesc) \
((_prHwMacTxDesc)->ucFrag |= TX_DESC_BROADCAST_MULTICAST)
#define HAL_MAC_TX_DESC_UNSET_BMC(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucFrag &= ~TX_DESC_BROADCAST_MULTICAST)

#define HAL_MAC_TX_DESC_IS_BIP(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucFrag & TX_DESC_BIP_PROTECTED)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_BIP(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucFrag |= TX_DESC_BIP_PROTECTED)
#define HAL_MAC_TX_DESC_UNSET_BIP(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucFrag &= ~TX_DESC_BIP_PROTECTED)

#define HAL_MAC_TX_DESC_IS_DURATION_CONTROL_BY_SW(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucFrag & TX_DESC_DURATION_FIELD_CONTROL)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_DURATION_CONTROL_BY_SW(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucFrag |= TX_DESC_DURATION_FIELD_CONTROL)
#define HAL_MAC_TX_DESC_SET_DURATION_CONTROL_BY_HW(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucFrag &= ~TX_DESC_DURATION_FIELD_CONTROL)

#define HAL_MAC_TX_DESC_IS_HTC_EXIST(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucFrag & TX_DESC_HTC_EXISTS)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_HTC_EXIST(_prHwMacTxDesc) \
((_prHwMacTxDesc)->ucFrag |= TX_DESC_HTC_EXISTS)
#define HAL_MAC_TX_DESC_UNSET_HTC_EXIST(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucFrag &= ~TX_DESC_HTC_EXISTS)

#define HAL_MAC_TX_DESC_IS_FRAG_PACKET(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucFrag & TX_DESC_FRAGMENT_MASK)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_GET_FRAG_PACKET_POS(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucFrag, \
	TX_DESC_FRAGMENT_MASK, TX_DESC_FRAGMENT_OFFSET)
#define HAL_MAC_TX_DESC_SET_FRAG_PACKET_POS(_prHwMacTxDesc, _ucFragPos) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucFrag), ((uint8_t)_ucFragPos), \
			  TX_DESC_FRAGMENT_MASK, TX_DESC_FRAGMENT_OFFSET)

/* For driver */
/* in unit of 32TU */
#define HAL_MAC_TX_DESC_GET_REMAINING_LIFE_TIME(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucRemainingMaxTxTime)
#define HAL_MAC_TX_DESC_SET_REMAINING_LIFE_TIME(_prHwMacTxDesc, _ucLifeTime) \
	((_prHwMacTxDesc)->ucRemainingMaxTxTime = (_ucLifeTime))
/* in unit of ms (minimal value is about 40ms) */
#define HAL_MAC_TX_DESC_GET_REMAINING_LIFE_TIME_IN_MS(_prHwMacTxDesc) \
	(TU_TO_MSEC(HAL_MAC_TX_DESC_GET_REMAINING_LIFE_TIME(_prHwMacTxDesc) \
	<< TX_DESC_LIFE_TIME_UNIT_IN_POWER_OF_2))
#define HAL_MAC_TX_DESC_SET_REMAINING_LIFE_TIME_IN_MS(_prHwMacTxDesc, \
	_u4LifeTimeMs) \
do { \
	uint32_t u4LifeTimeInUnit = \
		((MSEC_TO_USEC(_u4LifeTimeMs) / USEC_PER_TU) \
				    >> TX_DESC_LIFE_TIME_UNIT_IN_POWER_OF_2); \
	if (u4LifeTimeInUnit >= BIT(8)) \
		u4LifeTimeInUnit = BITS(0, 7); \
	else if ((_u4LifeTimeMs != TX_DESC_TX_TIME_NO_LIMIT) && \
		(u4LifeTimeInUnit == TX_DESC_TX_TIME_NO_LIMIT)) \
		u4LifeTimeInUnit = 1; \
	HAL_MAC_TX_DESC_SET_REMAINING_LIFE_TIME(_prHwMacTxDesc, \
	(uint8_t)u4LifeTimeInUnit); \
} while (0)

#define HAL_MAC_TX_DESC_GET_POWER_OFFSET(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucPowerOffset, \
	TX_DESC_POWER_OFFSET_MASK, 0)
#define HAL_MAC_TX_DESC_SET_POWER_OFFSET(_prHwMacTxDesc, _ucPowerOffset) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucPowerOffset), \
	((uint8_t)_ucPowerOffset), TX_DESC_POWER_OFFSET_MASK, 0)

#define HAL_MAC_TX_DESC_IS_BA_DISABLE(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucPowerOffset & TX_DESC_BA_DISABLE)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_BA_DISABLE(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucPowerOffset |= TX_DESC_BA_DISABLE)
#define HAL_MAC_TX_DESC_SET_BA_ENABLE(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucPowerOffset &= ~TX_DESC_BA_DISABLE)

#define HAL_MAC_TX_DESC_IS_TIMING_MEASUREMENT(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucPowerOffset & TX_DESC_TIMING_MEASUREMENT) \
	? TRUE : FALSE)
#define HAL_MAC_TX_DESC_SET_TIMING_MEASUREMENT(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucPowerOffset |= TX_DESC_TIMING_MEASUREMENT)
#define HAL_MAC_TX_DESC_UNSET_TIMING_MEASUREMENT(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucPowerOffset &= ~TX_DESC_TIMING_MEASUREMENT)

#define HAL_MAC_TX_DESC_IS_FIXED_RATE_ENABLE(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucPowerOffset & TX_DESC_FIXED_RATE)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_FIXED_RATE_ENABLE(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucPowerOffset |= TX_DESC_FIXED_RATE)
#define HAL_MAC_TX_DESC_SET_FIXED_RATE_DISABLE(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucPowerOffset &= ~TX_DESC_FIXED_RATE)

/* DW 3 */
#define HAL_MAC_TX_DESC_IS_NO_ACK(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->u2TxCountLimit & TX_DESC_NO_ACK)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_NO_ACK(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2TxCountLimit |= TX_DESC_NO_ACK)
#define HAL_MAC_TX_DESC_UNSET_NO_ACK(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2TxCountLimit &= ~TX_DESC_NO_ACK)

#define HAL_MAC_TX_DESC_IS_PROTECTION(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->u2TxCountLimit & TX_DESC_PROTECTED_FRAME) \
	? TRUE : FALSE)
#define HAL_MAC_TX_DESC_SET_PROTECTION(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2TxCountLimit |= TX_DESC_PROTECTED_FRAME)
#define HAL_MAC_TX_DESC_UNSET_PROTECTION(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2TxCountLimit &= ~TX_DESC_PROTECTED_FRAME)

#define HAL_MAC_TX_DESC_IS_EXTEND_MORE_DATA(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->u2TxCountLimit & TX_DESC_EXTEND_MORE_DATA) \
	? TRUE : FALSE)
#define HAL_MAC_TX_DESC_SET_EXTEND_MORE_DATA(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2TxCountLimit |= TX_DESC_EXTEND_MORE_DATA)
#define HAL_MAC_TX_DESC_UNSET_EXTEND_MORE_DATA(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2TxCountLimit &= ~TX_DESC_EXTEND_MORE_DATA)

#define HAL_MAC_TX_DESC_IS_EXTEND_EOSP(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->u2TxCountLimit & TX_DESC_EXTEND_EOSP)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_EXTEND_EOSP(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2TxCountLimit |= TX_DESC_EXTEND_EOSP)
#define HAL_MAC_TX_DESC_UNSET_EXTEND_EOSP(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2TxCountLimit &= ~TX_DESC_EXTEND_EOSP)

#define HAL_MAC_TX_DESC_GET_SW_RESERVED(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->u2TxCountLimit, \
	TX_DESC_SW_RESERVED_MASK, TX_DESC_SW_RESERVED_OFFSET)
#define HAL_MAC_TX_DESC_SET_SW_RESERVED(_prHwMacTxDesc, _ucSwReserved) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u2TxCountLimit), \
	((uint8_t)_ucSwReserved), \
	TX_DESC_SW_RESERVED_MASK, TX_DESC_SW_RESERVED_OFFSET)
#define HAL_MAC_TX_DESC_GET_TX_COUNT(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->u2TxCountLimit, \
	TX_DESC_TX_COUNT_MASK, TX_DESC_TX_COUNT_OFFSET)
#define HAL_MAC_TX_DESC_SET_TX_COUNT(_prHwMacTxDesc, _ucTxCountLimit) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u2TxCountLimit), \
	((uint8_t)_ucTxCountLimit), \
	TX_DESC_TX_COUNT_MASK, TX_DESC_TX_COUNT_OFFSET)
#define HAL_MAC_TX_DESC_GET_REMAINING_TX_COUNT(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->u2TxCountLimit, \
	TX_DESC_REMAINING_TX_COUNT_MASK, \
	TX_DESC_REMAINING_TX_COUNT_OFFSET)
#define HAL_MAC_TX_DESC_SET_REMAINING_TX_COUNT(_prHwMacTxDesc, \
	_ucTxCountLimit) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u2TxCountLimit), \
	((uint8_t)_ucTxCountLimit), \
	TX_DESC_REMAINING_TX_COUNT_MASK, \
	TX_DESC_REMAINING_TX_COUNT_OFFSET)
#define HAL_MAC_TX_DESC_GET_SEQUENCE_NUMBER(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->u2SN, TX_DESC_SEQUENCE_NUMBER, 0)
#define HAL_MAC_TX_DESC_SET_SEQUENCE_NUMBER(_prHwMacTxDesc, _u2SN) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u2SN), ((uint16_t)_u2SN), \
	TX_DESC_SEQUENCE_NUMBER, 0)
#define HAL_MAC_TX_DESC_SET_HW_RESERVED(_prHwMacTxDesc, _ucHwReserved) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u2SN), ((uint8_t)_ucHwReserved), \
			  TX_DESC_HW_RESERVED_MASK, TX_DESC_HW_RESERVED_OFFSET)
#define HAL_MAC_TX_DESC_IS_TXD_SN_VALID(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->u2SN & TX_DESC_SN_IS_VALID)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_TXD_SN_VALID(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2SN |= TX_DESC_SN_IS_VALID)
#define HAL_MAC_TX_DESC_SET_TXD_SN_INVALID(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2SN &= ~TX_DESC_SN_IS_VALID)

#define HAL_MAC_TX_DESC_IS_TXD_PN_VALID(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->u2SN & TX_DESC_PN_IS_VALID)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_TXD_PN_VALID(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2SN |= TX_DESC_PN_IS_VALID)
#define HAL_MAC_TX_DESC_SET_TXD_PN_INVALID(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2SN &= ~TX_DESC_PN_IS_VALID)

#define HAL_MAC_TX_DESC_ASSIGN_SN_BY_SW(_prHwMacTxDesc, _u2SN) \
{ \
	HAL_MAC_TX_DESC_SET_SEQUENCE_NUMBER(_prHwMacTxDesc, _u2SN); \
	HAL_MAC_TX_DESC_SET_TXD_SN_VALID(_prHwMacTxDesc); \
}
#define HAL_MAC_TX_DESC_ASSIGN_SN_BY_HW(_prHwMacTxDesc) \
{ \
	HAL_MAC_TX_DESC_SET_SEQUENCE_NUMBER(_prHwMacTxDesc, 0); \
	HAL_MAC_TX_DESC_SET_TXD_SN_INVALID(_prHwMacTxDesc); \
}

/* DW 4 */
#define HAL_MAC_TX_DESC_GET_PN(_prHwMacTxDesc, _u4PN_0_31, _u2PN_32_47) \
{ \
	((uint32_t)_u4PN_0_31) = (_prHwMacTxDesc)->u4PN1; \
	((uint16_t)_u2PN_32_47) = (_prHwMacTxDesc)->u2PN2; \
}
#define HAL_MAC_TX_DESC_SET_PN(_prHwMacTxDesc, _u4PN_0_31, _u2PN_32_47) \
{ \
	(_prHwMacTxDesc)->u4PN1 = ((uint32_t)_u4PN_0_31); \
	(_prHwMacTxDesc)->u2PN2 = ((uint16_t)_u2PN_32_47); \
}

#define HAL_MAC_TX_DESC_ASSIGN_PN_BY_SW(_prTxDesc, _u4PN_0_31, _u2PN_32_47) \
{ \
	HAL_MAC_TX_DESC_SET_PN(_prTxDesc, _u4PN_0_31, _u2PN_32_47); \
	HAL_MAC_TX_DESC_SET_TXD_PN_VALID(_prTxDesc); \
}
#define HAL_MAC_TX_DESC_ASSIGN_PSN_BY_HW(_prHwMacTxDesc) \
{ \
	HAL_MAC_TX_DESC_SET_PN(_prHwMacTxDesc, 0, 0); \
	HAL_MAC_TX_DESC_SET_TXD_PN_INVALID(_prHwMacTxDesc); \
}

/* DW 5 */
#define HAL_MAC_TX_DESC_GET_PID(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucPID)
#define HAL_MAC_TX_DESC_SET_PID(_prHwMacTxDesc, _ucPID) \
	(((_prHwMacTxDesc)->ucPID) = (_ucPID))

#define HAL_MAC_TX_DESC_GET_TXS_FORMAT(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucTxStatus, \
	TX_DESC_TX_STATUS_FORMAT, TX_DESC_TX_STATUS_FORMAT_OFFSET)
#define HAL_MAC_TX_DESC_SET_TXS_FORMAT(_prHwMacTxDesc, _ucTXSFormat) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucTxStatus), \
	((uint8_t)_ucTXSFormat), \
	TX_DESC_TX_STATUS_FORMAT, TX_DESC_TX_STATUS_FORMAT_OFFSET)

#define HAL_MAC_TX_DESC_IS_TXS_TO_MCU(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucTxStatus & TX_DESC_TX_STATUS_TO_MCU)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_TXS_TO_MCU(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucTxStatus |= TX_DESC_TX_STATUS_TO_MCU)
#define HAL_MAC_TX_DESC_UNSET_TXS_TO_MCU(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucTxStatus &= ~TX_DESC_TX_STATUS_TO_MCU)

#define HAL_MAC_TX_DESC_IS_TXS_TO_HOST(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucTxStatus & TX_DESC_TX_STATUS_TO_HOST)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_TXS_TO_HOST(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucTxStatus |= TX_DESC_TX_STATUS_TO_HOST)
#define HAL_MAC_TX_DESC_UNSET_TXS_TO_HOST(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucTxStatus &= ~TX_DESC_TX_STATUS_TO_HOST)

#define HAL_MAC_TX_DESC_IS_DA_FROM_WTBL(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucPowerOffset & TX_DESC_DA_SOURCE)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_DA_FROM_WTBL(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucPowerOffset |= TX_DESC_DA_SOURCE)
#define HAL_MAC_TX_DESC_SET_DA_FROM_MSDU(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucPowerOffset &= ~TX_DESC_DA_SOURCE)

#define HAL_MAC_TX_DESC_IS_SW_PM_CONTROL(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->ucPowerOffset & TX_DESC_POWER_MANAGEMENT_CONTROL) \
	? TRUE : FALSE)
#define HAL_MAC_TX_DESC_SET_SW_PM_CONTROL(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucPowerOffset |= TX_DESC_POWER_MANAGEMENT_CONTROL)
#define HAL_MAC_TX_DESC_SET_HW_PM_CONTROL(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucPowerOffset &= ~TX_DESC_POWER_MANAGEMENT_CONTROL)

/* DW 6 */
#define HAL_MAC_TX_DESC_SET_FR_BW(_prHwMacTxDesc, ucBw) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u2AntID), \
	((uint8_t)ucBw), TX_DESC_BANDWIDTH_MASK, TX_DESC_BANDWIDTH_OFFSET)

#define HAL_MAC_TX_DESC_SET_FR_DYNAMIC_BW_RTS(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2AntID |= TX_DESC_DYNAMIC_BANDWIDTH)

#define HAL_MAC_TX_DESC_SET_FR_ANTENNA_ID(_prHwMacTxDesc, _ucAntId) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u2AntID), \
	((uint8_t)_ucAntId), \
	TX_DESC_ANTENNA_INDEX_MASK, TX_DESC_ANTENNA_INDEX_OFFSET)

#define HAL_MAC_TX_DESC_SET_FR_RATE(_prHwMacTxDesc, _u2RatetoFixed) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u2FixedRate), \
	((uint8_t)_u2RatetoFixed), \
	TX_DESC_FIXDE_RATE_MASK, TX_DESC_FIXDE_RATE_OFFSET)

#define HAL_MAC_TX_DESC_SET_FR_BF(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2FixedRate |= TX_DESC_BF)

#define HAL_MAC_TX_DESC_SET_FR_LDPC(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2FixedRate |= TX_DESC_LDPC)

#define HAL_MAC_TX_DESC_SET_FR_SHORT_GI(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2FixedRate |= TX_DESC_GUARD_INTERVAL)

#define HAL_MAC_TX_DESC_SET_FR_NORMAL_GI(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2FixedRate &= ~TX_DESC_GUARD_INTERVAL)

#define HAL_MAC_TX_DESC_IS_CR_FIXED_RATE_MODE(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->u2FixedRate & TX_DESC_FIXED_RATE_MODE)?TRUE:FALSE)

#define HAL_MAC_TX_DESC_SET_FIXED_RATE_MODE_TO_DESC(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2FixedRate &= ~TX_DESC_FIXED_RATE_MODE)
#define HAL_MAC_TX_DESC_SET_FIXED_RATE_MODE_TO_CR(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2FixedRate |= TX_DESC_FIXED_RATE_MODE)

/* DW 7 */
#define HAL_MAC_TX_DESC_SET_SPE_IDX_SEL(_prHwMacTxDesc, _ucSpeIdxSel) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u2SwTxTime), \
	((uint16_t)_ucSpeIdxSel), \
	TX_DESC_SPE_EXT_IDX_SEL_MASK, TX_DESC_SPE_EXT_IDX_SEL_OFFSET)
#define HAL_MAC_TX_DESC_SET_SPE_IDX(_prHwMacTxDesc, _ucSpeIdx) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u2SwTxTime), \
	((uint16_t)_ucSpeIdx), \
	TX_DESC_SPE_EXT_IDX_MASK, TX_DESC_SPE_EXT_IDX_OFFSET)

#define HAL_MAC_TX_DESC_IS_HW_AMSDU(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->u2PseFid & TX_DESC_HW_AMSDU)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_HW_AMSDU(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2PseFid |= TX_DESC_HW_AMSDU)
#define HAL_MAC_TX_DESC_UNSET_HW_AMSDU(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2PseFid &= ~TX_DESC_HW_AMSDU)

#define HAL_MAC_TX_DESC_IS_HIF_ERR(_prHwMacTxDesc) \
	(((_prHwMacTxDesc)->u2PseFid & TX_DESC_HIF_ERR)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_HIF_ERR(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2PseFid |= TX_DESC_HIF_ERR)
#define HAL_MAC_TX_DESC_UNSET_HIF_ERR(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2PseFid &= ~TX_DESC_HIF_ERR)


#define nicTxReleaseResource_PSE(prAdapter, ucTc, u4PageCount, fgReqLock) \
	nicTxReleaseResource(prAdapter, ucTc, u4PageCount, fgReqLock, FALSE)

#define nicTxReleaseResource_PLE(prAdapter, ucTc, u4PageCount, fgReqLock) \
	nicTxReleaseResource(prAdapter, ucTc, u4PageCount, fgReqLock, TRUE)

#if (CFG_SUPPORT_802_11AX == 1)
#define NIC_TX_PPDU_ENABLE(__pAd) \
	HAL_MCR_WR( \
		__pAd, \
		__pAd->chip_info->arb_ac_mode_addr, \
		0x0)

#define NIC_TX_PPDU_DISABLE(__pAd) \
	HAL_MCR_WR( \
		__pAd, \
		__pAd->chip_info->arb_ac_mode_addr, \
		0xFFFFFFFF)
#endif /* CFG_SUPPORT_802_11AX == 1 */

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
void nicTxInitialize(IN struct ADAPTER *prAdapter);

uint32_t nicTxAcquireResource(IN struct ADAPTER *prAdapter,
	IN uint8_t ucTC, IN uint32_t u4PageCount,
	IN u_int8_t fgReqLock);

uint32_t nicTxPollingResource(IN struct ADAPTER *prAdapter,
	IN uint8_t ucTC);

u_int8_t nicTxReleaseResource(IN struct ADAPTER *prAdapter,
	IN uint8_t ucTc, IN uint32_t u4PageCount,
	IN u_int8_t fgReqLock, IN u_int8_t fgPLE);

void nicTxReleaseMsduResource(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfoListHead);

uint32_t nicTxResetResource(IN struct ADAPTER *prAdapter);

#if defined(_HIF_SDIO)
uint32_t nicTxGetAdjustableResourceCnt(IN struct ADAPTER *prAdapter);
#endif

uint16_t nicTxGetResource(IN struct ADAPTER *prAdapter,
	IN uint8_t ucTC);

uint8_t nicTxGetFrameResourceType(IN uint8_t eFrameType,
	IN struct MSDU_INFO *prMsduInfo);

uint8_t nicTxGetCmdResourceType(IN struct CMD_INFO *prCmdInfo);

u_int8_t nicTxSanityCheckResource(IN struct ADAPTER *prAdapter);

void nicTxFillDesc(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo, OUT uint8_t *prTxDescBuffer,
	OUT uint32_t *pu4TxDescLength);

void nicTxFillDataDesc(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo);

void nicTxComposeSecurityFrameDesc(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo,
	OUT uint8_t *prTxDescBuffer, OUT uint8_t *pucTxDescLength);

uint32_t nicTxMsduInfoList(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfoListHead);

uint8_t nicTxGetTxQByTc(IN struct ADAPTER *prAdapter, IN uint8_t ucTc);
uint8_t nicTxGetTxDestPortIdxByTc(IN uint8_t ucTc);
uint8_t nicTxGetTxDestQIdxByTc(IN uint8_t ucTc);
uint32_t nicTxGetRemainingTxTimeByTc(IN uint8_t ucTc);
uint8_t nicTxGetTxCountLimitByTc(IN uint8_t ucTc);
#if CFG_SUPPORT_MULTITHREAD
uint32_t nicTxMsduInfoListMthread(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfoListHead);

uint32_t nicTxMsduQueueMthread(IN struct ADAPTER *prAdapter);

void nicTxMsduQueueByPrio(struct ADAPTER *prAdapter);
void nicTxMsduQueueByRR(struct ADAPTER *prAdapter);

uint32_t nicTxGetMsduPendingCnt(IN struct ADAPTER *prAdapter);
#endif

uint32_t nicTxMsduQueue(IN struct ADAPTER *prAdapter,
	uint8_t ucPortIdx, struct QUE *prQue);

uint32_t nicTxCmd(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint8_t ucTC);

void nicTxRelease(IN struct ADAPTER *prAdapter,
	IN u_int8_t fgProcTxDoneHandler);

void nicProcessTxInterrupt(IN struct ADAPTER *prAdapter);

void nicTxFreeMsduInfoPacket(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfoListHead);

void nicTxReturnMsduInfo(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfoListHead);

u_int8_t nicTxFillMsduInfo(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo, IN void *prNdisPacket);

uint32_t nicTxAdjustTcq(IN struct ADAPTER *prAdapter);

uint32_t nicTxFlush(IN struct ADAPTER *prAdapter);

#if CFG_ENABLE_FW_DOWNLOAD
uint32_t nicTxInitCmd(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo, IN uint16_t u2Port);

uint32_t nicTxInitResetResource(IN struct ADAPTER *prAdapter);
#endif

u_int8_t nicTxProcessCmdDataPacket(IN struct ADAPTER *prAdapter,
			       IN struct MSDU_INFO *prMsduInfo);

uint32_t nicTxEnqueueMsdu(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo);

uint8_t nicTxGetWlanIdx(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIdx, IN uint8_t ucStaRecIdx);

u_int8_t nicTxIsMgmtResourceEnough(IN struct ADAPTER *prAdapter);

uint32_t nicTxGetFreeCmdCount(IN struct ADAPTER *prAdapter);

uint32_t nicTxGetPageCount(IN struct ADAPTER *prAdapter,
	IN uint32_t u4FrameLength, IN u_int8_t fgIncludeDesc);

uint32_t nicTxGetCmdPageCount(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo);

uint32_t nicTxGenerateDescTemplate(IN struct ADAPTER *prAdapter,
	IN struct STA_RECORD *prStaRec);

void nicTxFreeDescTemplate(IN struct ADAPTER *prAdapter,
	IN struct STA_RECORD *prStaRec);

void nicTxSetHwAmsduDescTemplate(IN struct ADAPTER *prAdapter,
	IN struct STA_RECORD *prStaRec,
	IN uint8_t ucTid, IN u_int8_t fgSet);

void nicTxFreePacket(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo, IN u_int8_t fgDrop);

void nicTxSetMngPacket(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo,
	IN uint8_t ucBssIndex,
	IN uint8_t ucStaRecIndex,
	IN uint8_t ucMacHeaderLength,
	IN uint16_t u2FrameLength,
	IN PFN_TX_DONE_HANDLER pfTxDoneHandler,
	IN uint8_t ucRateMode);

void nicTxSetDataPacket(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo,
	IN uint8_t ucBssIndex,
	IN uint8_t ucStaRecIndex,
	IN uint8_t ucMacHeaderLength,
	IN uint16_t u2FrameLength,
	IN PFN_TX_DONE_HANDLER pfTxDoneHandler,
	IN uint8_t ucRateMode,
	IN enum ENUM_TX_PACKET_SRC eSrc, IN uint8_t ucTID,
	IN u_int8_t fgIs802_11Frame, IN u_int8_t fgIs1xFrame);

void nicTxFillDescByPktOption(
	IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo,
	IN void *prTxDesc);

void nicTxConfigPktOption(IN struct MSDU_INFO *prMsduInfo,
	IN uint32_t u4OptionMask, IN u_int8_t fgSetOption);

void nicTxFillDescByPktControl(struct MSDU_INFO *prMsduInfo,
	void *prTxDesc);

void nicTxConfigPktControlFlag(IN struct MSDU_INFO *prMsduInfo,
	IN uint8_t ucControlFlagMask, IN u_int8_t fgSetFlag);

void nicTxSetPktLifeTime(IN struct MSDU_INFO *prMsduInfo,
	IN uint32_t u4TxLifeTimeInMs);

void nicTxSetPktRetryLimit(IN struct MSDU_INFO *prMsduInfo,
	IN uint8_t ucRetryLimit);

void nicTxSetPktPowerOffset(IN struct MSDU_INFO *prMsduInfo,
	IN int8_t cPowerOffset);

void nicTxSetPktSequenceNumber(IN struct MSDU_INFO *prMsduInfo,
	IN uint16_t u2SN);

void nicTxSetPktMacTxQue(IN struct MSDU_INFO *prMsduInfo,
	IN uint8_t ucMacTxQue);

void nicTxSetPktFixedRateOptionFull(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo,
	uint16_t u2RateCode,
	uint8_t ucBandwidth,
	u_int8_t fgShortGI,
	u_int8_t fgLDPC,
	u_int8_t fgDynamicBwRts,
	u_int8_t fgBeamforming,
	uint8_t ucAntennaIndex);

void nicTxSetPktFixedRateOption(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo,
	uint16_t u2RateCode,
	uint8_t ucBandwidth,
	u_int8_t fgShortGI,
	u_int8_t fgDynamicBwRts);

void nicTxSetPktLowestFixedRate(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo);

void nicTxSetPktMoreData(IN struct MSDU_INFO *prCurrentMsduInfo,
	IN u_int8_t fgSetMoreDataBit);

void nicTxSetPktEOSP(IN struct MSDU_INFO *prCurrentMsduInfo,
	IN u_int8_t fgSetEOSPBit);

uint8_t nicTxAssignPID(IN struct ADAPTER *prAdapter,
	IN uint8_t ucWlanIndex);

uint32_t
nicTxDummyTxDone(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo,
	IN enum ENUM_TX_RESULT_CODE rTxDoneStatus);

void nicTxUpdateBssDefaultRate(IN struct BSS_INFO *prBssInfo);

void nicTxUpdateStaRecDefaultRate(IN struct ADAPTER *prAdapter,
	IN struct STA_RECORD *prStaRec);

void nicTxPrintMetRTP(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo, IN void *prPacket,
	IN uint32_t u4PacketLen, IN u_int8_t bFreeSkb);

void nicTxProcessTxDoneEvent(IN struct ADAPTER *prAdapter,
	IN struct WIFI_EVENT *prEvent);

void nicTxChangeDataPortByAc(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	uint8_t ucAci,
			     u_int8_t fgToMcu);

void nicTxHandleRoamingDone(struct ADAPTER *prAdapter,
			    struct STA_RECORD *prOldStaRec,
			    struct STA_RECORD *prNewStaRec);

void nicTxMsduDoneCb(IN struct GLUE_INFO *prGlueInfo, IN struct QUE *prQue);

void nicTxCancelSendingCmd(IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo);
uint32_t nicTxGetMaxPageCntPerFrame(IN struct ADAPTER *prAdapter);

/* TX Direct functions : BEGIN */
void nicTxDirectStartCheckQTimer(IN struct ADAPTER *prAdapter);
void nicTxDirectClearSkbQ(IN struct ADAPTER *prAdapter);
void nicTxDirectClearHifQ(IN struct ADAPTER *prAdapter);
void nicTxDirectClearStaPsQ(IN struct ADAPTER *prAdapter,
	uint8_t ucStaRecIndex);
void nicTxDirectClearBssAbsentQ(IN struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);
void nicTxDirectClearAllStaPsQ(IN struct ADAPTER *prAdapter);

#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
void nicTxDirectTimerCheckSkbQ(struct timer_list *timer);
void nicTxDirectTimerCheckHifQ(struct timer_list *timer);
#else
void nicTxDirectTimerCheckSkbQ(unsigned long data);
void nicTxDirectTimerCheckHifQ(unsigned long data);
#endif

uint32_t nicTxDirectStartXmit(struct sk_buff *prSkb,
	struct GLUE_INFO *prGlueInfo);
/* TX Direct functions : END */

uint32_t nicTxResourceGetPleFreeCount(IN struct ADAPTER *prAdapter,
	IN uint8_t ucTC);
u_int8_t nicTxResourceIsPleCtrlNeeded(IN struct ADAPTER *prAdapter,
	IN uint8_t ucTC);
void nicTxResourceUpdate_v1(IN struct ADAPTER *prAdapter);

int32_t nicTxGetVectorInfo(IN char *pcCommand, IN int i4TotalLen,
			IN struct TX_VECTOR_BBP_LATCH *prTxV);

void nicHifTxMsduDoneCb(IN struct ADAPTER *prAdapter,
		IN struct MSDU_INFO *prMsduInfo);

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

#endif /* _NIC_TX_H */
