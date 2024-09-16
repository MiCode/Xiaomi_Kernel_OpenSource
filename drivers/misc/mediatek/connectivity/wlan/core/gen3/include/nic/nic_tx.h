/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/nic/nic_tx.h#1
 */

/*
 * ! \file   nic_tx.h
 *  \brief  Functions that provide TX operation in NIC's point of view.
 *
 *   This file provides TX functions which are responsible for both Hardware and
 *   Software Resource Management and keep their Synchronization.
 */

#ifndef _NIC_TX_H
#define _NIC_TX_H

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

#define MAC_TX_RESERVED_FIELD               0

#define NIC_TX_RESOURCE_POLLING_TIMEOUT     256
#define NIC_TX_RESOURCE_POLLING_DELAY_MSEC  50

#define NIC_TX_CMD_INFO_RESERVED_COUNT      4

/* Maximum buffer count for individual HIF TCQ */
#define NIC_TX_PAGE_COUNT_TC0           (NIC_TX_BUFF_COUNT_TC0 * NIC_TX_MAX_PAGE_PER_FRAME)
#define NIC_TX_PAGE_COUNT_TC1           (NIC_TX_BUFF_COUNT_TC1 * NIC_TX_MAX_PAGE_PER_FRAME)
#define NIC_TX_PAGE_COUNT_TC2           (NIC_TX_BUFF_COUNT_TC2 * NIC_TX_MAX_PAGE_PER_FRAME)
#define NIC_TX_PAGE_COUNT_TC3           (NIC_TX_BUFF_COUNT_TC3 * NIC_TX_MAX_PAGE_PER_FRAME)
#define NIC_TX_PAGE_COUNT_TC4           (NIC_TX_BUFF_COUNT_TC4 * NIC_TX_MAX_PAGE_PER_FRAME)
#define NIC_TX_PAGE_COUNT_TC5           (NIC_TX_BUFF_COUNT_TC5 * NIC_TX_MAX_PAGE_PER_FRAME)

#define NIC_TX_BUFF_COUNT_TC0               1
#define NIC_TX_BUFF_COUNT_TC1               36
#define NIC_TX_BUFF_COUNT_TC2               1
#define NIC_TX_BUFF_COUNT_TC3               1
#define NIC_TX_BUFF_COUNT_TC4               2
#define NIC_TX_BUFF_COUNT_TC5               1

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

#define NIC_TX_INIT_PAGE_COUNT_TC0           (NIC_TX_INIT_BUFF_COUNT_TC0 * NIC_TX_MAX_PAGE_PER_FRAME)
#define NIC_TX_INIT_PAGE_COUNT_TC1           (NIC_TX_INIT_BUFF_COUNT_TC1 * NIC_TX_MAX_PAGE_PER_FRAME)
#define NIC_TX_INIT_PAGE_COUNT_TC2           (NIC_TX_INIT_BUFF_COUNT_TC2 * NIC_TX_MAX_PAGE_PER_FRAME)
#define NIC_TX_INIT_PAGE_COUNT_TC3           (NIC_TX_INIT_BUFF_COUNT_TC3 * NIC_TX_MAX_PAGE_PER_FRAME)
#define NIC_TX_INIT_PAGE_COUNT_TC4           (NIC_TX_INIT_BUFF_COUNT_TC4 * NIC_TX_MAX_PAGE_PER_FRAME)
#define NIC_TX_INIT_PAGE_COUNT_TC5           (NIC_TX_INIT_BUFF_COUNT_TC5 * NIC_TX_MAX_PAGE_PER_FRAME)

#endif

#define NIC_TX_ENABLE_SECOND_HW_QUEUE            0

/* 4 TODO: The following values shall be got from FW by query CMD */
/*------------------------------------------------------------------------*/
/* Resource Management related information                                */
/*------------------------------------------------------------------------*/
#define NIC_TX_PAGE_SIZE_IS_POWER_OF_2          TRUE
#define NIC_TX_PAGE_SIZE_IN_POWER_OF_2          7
#define NIC_TX_PAGE_SIZE                        128	/* in unit of bytes */

/* For development only */
#define NIC_TX_MAX_SIZE_PER_FRAME               1532	/* calculated by MS native 802.11 format */
#define NIC_TX_MAX_PAGE_PER_FRAME \
	((NIC_TX_DESC_AND_PADDING_LENGTH + NIC_TX_DESC_HEADER_PADDING_LENGTH + \
	NIC_TX_MAX_SIZE_PER_FRAME + NIC_TX_PAGE_SIZE - 1) / NIC_TX_PAGE_SIZE)

/*------------------------------------------------------------------------*/
/* Tx descriptor related information                                      */
/*------------------------------------------------------------------------*/

/* Frame Buffer
*  |<--Tx Descriptor-->|<--Tx descriptor padding-->|<--802.3/802.11 Header-->|<--Header padding-->|<--Payload-->|
*/

/* Tx descriptor length by format (TXD.FT) */
#define NIC_TX_DESC_LONG_FORMAT_LENGTH_DW       7	/* in unit of double word */
#define NIC_TX_DESC_LONG_FORMAT_LENGTH          DWORD_TO_BYTE(NIC_TX_DESC_LONG_FORMAT_LENGTH_DW)
#define NIC_TX_DESC_SHORT_FORMAT_LENGTH_DW      2	/* in unit of double word */
#define NIC_TX_DESC_SHORT_FORMAT_LENGTH         DWORD_TO_BYTE(NIC_TX_DESC_SHORT_FORMAT_LENGTH_DW)

/* Tx descriptor padding length (DMA.MICR.TXDSCR_PAD) */
#define NIC_TX_DESC_PADDING_LENGTH_DW           0	/* in unit of double word */
#define NIC_TX_DESC_PADDING_LENGTH              DWORD_TO_BYTE(NIC_TX_DESC_PADDING_LENGTH_DW)

#define NIC_TX_DESC_AND_PADDING_LENGTH          (NIC_TX_DESC_LONG_FORMAT_LENGTH + NIC_TX_DESC_PADDING_LENGTH)

/* Tx header padding (TXD.HeaderPadding)  */
/* Warning!! To use MAC header padding, every Tx packet must be decomposed */
#define NIC_TX_DESC_HEADER_PADDING_LENGTH       0	/* in unit of bytes */

#define NIC_TX_DEFAULT_WLAN_INDEX               31	/* For Tx packets to peer who has no WLAN table index. */

#define NIC_TX_DESC_PID_INVALID                 0
#define NIC_TX_DESC_DRIVER_PID_MIN              1
#define NIC_TX_DESC_DRIVER_PID_MAX              127
#define NIC_TX_DESC_PID_RESERVED_FOR_MGMT       10	/* Reserve PID number for MGMT/Security frames */

#if defined(MT6630) || defined(MT6631)
#define NIC_TX_DATA_DEFAULT_RETRY_COUNT_LIMIT   30
#else
#define NIC_TX_DATA_DEFAULT_RETRY_COUNT_LIMIT   7
#endif
#define NIC_TX_MGMT_DEFAULT_RETRY_COUNT_LIMIT   30

#define NIC_TX_AC_BE_REMAINING_TX_TIME          TX_DESC_TX_TIME_NO_LIMIT	/* in unit of ms */
#define NIC_TX_AC_BK_REMAINING_TX_TIME          TX_DESC_TX_TIME_NO_LIMIT	/* in unit of ms */
#define NIC_TX_AC_VO_REMAINING_TX_TIME          TX_DESC_TX_TIME_NO_LIMIT	/* in unit of ms */
#define NIC_TX_AC_VI_REMAINING_TX_TIME          TX_DESC_TX_TIME_NO_LIMIT	/* in unit of ms */

/*------------------------------------------------------------------------*/
/* Tx descriptor field related information                                                                 */
/*------------------------------------------------------------------------*/
/* DW 0 */
#define TX_DESC_TX_BYTE_COUNT_MASK              BITS(0, 15)
#define TX_DESC_TX_BYTE_COUNT_OFFSET            0

#define TX_DESC_ETHER_TYPE_OFFSET_MASK          BITS(0, 6)
#define TX_DESC_ETHER_TYPE_OFFSET_OFFSET        0
#define TX_DESC_IP_CHKSUM_OFFLOAD               BIT(7)
#define TX_DESC_TCP_UDP_CHKSUM_OFFLOAD          BIT(0)
#define TX_DESC_USB_NEXT_VLD                    BIT(1)
#define TX_DESC_USB_TX_BURST                    BIT(2)
#define TX_DESC_QUEUE_INDEX_MASK                BITS(3, 6)
#define TX_DESC_QUEUE_INDEX_OFFSET              3
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
#define HEADER_FORMAT_802_11_NORMAL_MODE        2	/* 802.11 (normal mode) */
#define HEADER_FORMAT_802_11_ENHANCE_MODE       3	/* 802.11 (Enhancement mode) */
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
#define TX_DESC_HEADER_PADDING_LENGTH_MASK      BITS(0, 1)
#define TX_DESC_HEADER_PADDING_LENGTH_OFFSET    0
#define TX_DESC_HEADER_PADDING_MODE             BIT(2)
#define TX_DESC_NO_ACK                          BIT(3)
#define TX_DESC_TID_MASK                        BITS(4, 6)
#define TX_DESC_TID_OFFSET                      4
#define TX_DESC_TID_NUM                         8
#define TX_DESC_PROTECTED_FRAME                 BIT(7)

#define TX_DESC_PACKET_TYPE_MASK                BITS(0, 1)	/* SW Field */
#define TX_DESC_PACKET_TYPE_OFFSET              0
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
#define TX_DESC_LIFE_TIME_UNIT                  POWER_OF_2(TX_DESC_LIFE_TIME_UNIT_IN_POWER_OF_2)
#define TX_DESC_POWER_OFFSET_MASK               BITS(0, 4)
#define TX_DESC_BA_DISABLE                      BIT(5)
#define TX_DESC_TIMING_MEASUREMENT              BIT(6)
#define TX_DESC_FIXED_RATE                      BIT(7)

/* DW 3 */
#define TX_DESC_SW_RESERVED_MASK                BITS(0, 5)
#define TX_DESC_SW_RESERVED_OFFSET              0
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
#define TX_DESC_BAR_SSN_CONTROL                 BIT(4)
#define TX_DESC_POWER_MANAGEMENT_CONTROL        BIT(5)
#define TX_DESC_PN_PART2                        BITS(0, 15)

			       /* DW 6 *//* FR = 1 */
#define TX_DESC_FIXED_RATE_MODE                 BIT(0)
#define TX_DESC_ANTENNA_INDEX_MASK              BITS(2, 7)
#define TX_DESC_ANTENNA_INDEX_OFFSET            2

#define TX_DESC_BANDWIDTH_MASK                  BITS(0, 2)
#define TX_DESC_SPATIAL_EXTENSION               BIT(3)
#define TX_DESC_ANTENNA_PRIORITY                BITS(4, 6)
#define TX_DESC_DYNAMIC_BANDWIDTH               BIT(7)

#define TX_DESC_EXPLICIT_BEAMFORMING            BIT(0)
#define TX_DESC_IMPLICIT_BEAMFORMING            BIT(1)
#define TX_DESC_FIXDE_RATE_MASK                 BITS(2, 13)
#define TX_DESC_FIXDE_RATE_OFFSET               2
#define TX_DESC_LDPC                            BIT(14)
#define TX_DESC_GUARD_INTERVAL                  BIT(15)

#if CFG_ENABLE_PKT_LIFETIME_PROFILE
#define NIC_TX_TIME_THRESHOLD                       100	/* in unit of ms */
#endif

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
	       /* 3 *//* Session for TX QUEUES */
/* The definition in this ENUM is used to categorize packet's Traffic Class according
 * to the their TID(User Priority).
 * In order to achieve QoS goal, a particular TC should not block the process of
 * another packet with different TC.
 * In current design we will have 5 categories(TCs) of SW resource.
 */
/* HIF Tx interrupt status queue index*/
typedef enum _ENUM_HIF_TX_INDEX_T {
	HIF_TX_AC0_INDEX = 0,	/* HIF TX: AC0 packets */
	HIF_TX_AC1_INDEX,	/* HIF TX: AC1 packets */
	HIF_TX_AC2_INDEX,	/* HIF TX: AC2 packets */
	HIF_TX_AC3_INDEX,	/* HIF TX: AC3 packets */
	HIF_TX_AC4_INDEX,	/* HIF TX: AC4 packets */
	HIF_TX_AC5_INDEX,	/* HIF TX: AC5 packets */
	HIF_TX_AC6_INDEX,	/* HIF TX: AC6 packets */
	HIF_TX_BMC_INDEX,	/* HIF TX: BMC packets */
	HIF_TX_BCN_INDEX,	/* HIF TX: BCN packets */
	HIF_TX_AC10_INDEX,	/* HIF TX: AC10 packets */
	HIF_TX_AC11_INDEX,	/* HIF TX: AC11 packets */
	HIF_TX_AC12_INDEX,	/* HIF TX: AC12 packets */
	HIF_TX_AC13_INDEX,	/* HIF TX: AC13 packets */
	HIF_TX_AC14_INDEX,	/* HIF TX: AC14 packets */
	HIF_TX_FFA_INDEX,	/* HIF TX: free-for-all */
	HIF_TX_CPU_INDEX,	/* HIF TX: CPU */
	HIF_TX_NUM		/* Maximum number of HIF TX port. */
} ENUM_HIF_TX_INDEX_T;

/* LMAC Tx queue index */
typedef enum _ENUM_MAC_TXQ_INDEX_T {
	MAC_TXQ_AC0_INDEX = 0,
	MAC_TXQ_AC1_INDEX,
	MAC_TXQ_AC2_INDEX,
	MAC_TXQ_AC3_INDEX,
	MAC_TXQ_AC4_INDEX,
	MAC_TXQ_AC5_INDEX,
	MAC_TXQ_AC6_INDEX,
	MAC_TXQ_BMC_INDEX,
	MAC_TXQ_BCN_INDEX,
	MAC_TXQ_AC10_INDEX,
	MAC_TXQ_AC11_INDEX,
	MAC_TXQ_AC12_INDEX,
	MAC_TXQ_AC13_INDEX,
	MAC_TXQ_AC14_INDEX,
	MAC_TXQ_NUM
} ENUM_MAC_TXQ_INDEX_T;

/* MCU quque index */
typedef enum _ENUM_MCU_Q_INDEX_T {
	MCU_Q0_INDEX = 0,
	MCU_Q1_INDEX,
	MCU_Q2_INDEX,
	MCU_Q3_INDEX,
	MCU_Q_NUM
} ENUM_MCU_Q_INDEX_T;

/* Tc Resource index */
typedef enum _ENUM_TRAFFIC_CLASS_INDEX_T {
	/*First HW queue */
	TC0_INDEX = 0,		/* HIF TX: AC0 packets */
	TC1_INDEX,		/* HIF TX: AC1 packets */
	TC2_INDEX,		/* HIF TX: AC2 packets */
	TC3_INDEX,		/* HIF TX: AC3 packets */
	TC4_INDEX,		/* HIF TX: CPU packets */
	TC5_INDEX,		/* HIF TX: AC4 packets */

	/* Second HW queue */
#if NIC_TX_ENABLE_SECOND_HW_QUEUE
	TC6_INDEX,		/* HIF TX: AC10 packets */
	TC7_INDEX,		/* HIF TX: AC11 packets */
	TC8_INDEX,		/* HIF TX: AC12 packets */
	TC9_INDEX,		/* HIF TX: AC13 packets */
	TC10_INDEX,		/* HIF TX: AC14 packets */
#endif

	TC_NUM			/* Maximum number of Traffic Classes. */
} ENUM_TRAFFIC_CLASS_INDEX_T;

/* per-Network Tc Resource index */
typedef enum _ENUM_NETWORK_TC_RESOURCE_INDEX_T {
	/* QoS Data frame, WMM AC index */
	NET_TC_WMM_AC_BE_INDEX = 0,
	NET_TC_WMM_AC_BK_INDEX,
	NET_TC_WMM_AC_VI_INDEX,
	NET_TC_WMM_AC_VO_INDEX,
	/* Mgmt frame */
	NET_TC_MGMT_INDEX,
	/* nonQoS / non StaRec frame (BMC/non-associated frame) */
	NET_TC_NON_STAREC_NON_QOS_INDEX,

	NET_TC_NUM
} ENUM_NETWORK_TC_RESOURCE_INDEX_T;

typedef enum _ENUM_TX_STATISTIC_COUNTER_T {
	TX_MPDU_TOTAL_COUNT = 0,
	TX_INACTIVE_BSS_DROP,
	TX_INACTIVE_STA_DROP,
	TX_FORWARD_OVERFLOW_DROP,
	TX_AP_BORADCAST_DROP,
	TX_STATISTIC_COUNTER_NUM
} ENUM_TX_STATISTIC_COUNTER_T;

typedef enum _ENUM_FIX_BW_T {
	FIX_BW_NO_FIXED = 0,
	FIX_BW_20 = 4,
	FIX_BW_40,
	FIX_BW_80,
	FIX_BW_160,
	FIX_BW_NUM
} ENUM_FIX_BW_T;

typedef enum _ENUM_MSDU_OPTION_T {
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
} ENUM_MSDU_OPTION_T;

typedef enum _ENUM_MSDU_CONTROL_FLAG_T {
	MSDU_CONTROL_FLAG_FORCE_TX = BIT(0)
} ENUM_MSDU_CONTROL_FLAG_T;

typedef enum _ENUM_MSDU_RATE_MODE_T {
	MSDU_RATE_MODE_AUTO = 0,
	MSDU_RATE_MODE_MANUAL_DESC,
	/* The following rate mode is not implemented yet */
	/* DON'T use!!! */
	MSDU_RATE_MODE_MANUAL_CR
} ENUM_MSDU_RATE_MODE_T;

typedef struct _TX_TCQ_STATUS_T {
	/* HIF reported page count delta */
	UINT_16 au2TxDonePageCount[TC_NUM];	/* other TC */
	UINT_16 au2PreUsedPageCount[TC_NUM];
	UINT_16 u2AvaliablePageCount;	/* FFA */
	UINT_8 ucNextTcIdx;	/* For round-robin distribute free page count */

	/* distributed page count */
	UINT_16 au2FreePageCount[TC_NUM];
	UINT_16 au2MaxNumOfPage[TC_NUM];

	/* buffer count */
	UINT_16 au2FreeBufferCount[TC_NUM];
	UINT_16 au2MaxNumOfBuffer[TC_NUM];
} TX_TCQ_STATUS_T, *P_TX_TCQ_STATUS_T;

typedef struct _TX_TCQ_ADJUST_T {
	INT_8 acVariation[TC_NUM];
} TX_TCQ_ADJUST_T, *P_TX_TCQ_ADJUST_T;

typedef struct _TX_CTRL_T {
	UINT_32 u4TxCachedSize;
	PUINT_8 pucTxCached;

	UINT_32 u4PageSize;

	UINT_32 u4TotalPageNum;

	UINT_32 u4TotalTxRsvPageNum;

/* Elements below is classified according to TC (Traffic Class) value. */

	TX_TCQ_STATUS_T rTc;

	PUINT_8 pucTxCoalescingBufPtr;

	QUE_T rFreeMsduInfoList;

	/* Management Frame Tracking */
	/* number of management frames to be sent */
	INT_32 i4TxMgmtPendingNum;

	/* to tracking management frames need TX done callback */
	QUE_T rTxMgmtTxingQueue;

#if CFG_HIF_STATISTICS
	UINT_32 u4TotalTxAccessNum;
	UINT_32 u4TotalTxPacketNum;
#endif
	UINT_32 au4Statistics[TX_STATISTIC_COUNTER_NUM];

	/* Number to track forwarding frames */
	INT_32 i4PendingFwdFrameCount;

} TX_CTRL_T, *P_TX_CTRL_T;

typedef enum _ENUM_TX_PACKET_TYPE_T {
	TX_PACKET_TYPE_DATA = 0,
	TX_PACKET_TYPE_MGMT,
	/* TX_PACKET_TYPE_1X, */
	X_PACKET_TYPE_NUM
} ENUM_TX_PACKET_TYPE_T, *P_ENUM_TX_PACKET_TYPE_T;

typedef enum _ENUM_TX_PACKET_SRC_T {
	TX_PACKET_OS,
	TX_PACKET_OS_OID,
	TX_PACKET_FORWARDING,
	TX_PACKET_MGMT,
	TX_PACKET_NUM
} ENUM_TX_PACKET_SRC_T;

/* TX Call Back Function  */
typedef WLAN_STATUS(*PFN_TX_DONE_HANDLER) (IN P_ADAPTER_T prAdapter,
					   IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus);

#if CFG_ENABLE_PKT_LIFETIME_PROFILE
typedef struct _PKT_PROFILE_T {
	BOOLEAN fgIsValid;
#if CFG_PRINT_PKT_LIFETIME_PROFILE
	BOOLEAN fgIsPrinted;
	UINT_16 u2IpSn;
	UINT_16 u2RtpSn;
	UINT_8 ucTcxFreeCount;
#endif
	OS_SYSTIME rHardXmitArrivalTimestamp;
	OS_SYSTIME rEnqueueTimestamp;
	OS_SYSTIME rDequeueTimestamp;
	OS_SYSTIME rHifTxDoneTimestamp;
} PKT_PROFILE_T, *P_PKT_PROFILE_T;
#endif
/* TX transactions could be divided into 4 kinds:
 *
 * 1) 802.1X / Bluetooth-over-Wi-Fi Security Frames
 *    [CMD_INFO_T] - [prPacket] - in skb or NDIS_PACKET form
 *
 * 2) MMPDU
 *    [CMD_INFO_T] - [prPacket] - [MSDU_INFO_T] - [prPacket] - direct buffer for frame body
 *
 * 3) Command Packets
 *    [CMD_INFO_T] - [pucInfoBuffer] - direct buffer for content of command packet
 *
 * 4) Normal data frame
 *    [MSDU_INFO_T] - [prPacket] - in skb or NDIS_PACKET form
 */

/* PS_FORWARDING_TYPE_NON_PS means that the receiving STA is in Active Mode
*   from the perspective of host driver (maybe not synchronized with FW --> SN is needed)
*/

struct _MSDU_INFO_T {
	QUE_ENTRY_T rQueEntry;
	P_NATIVE_PACKET prPacket;	/* Pointer to packet buffer */

	ENUM_TX_PACKET_SRC_T eSrc;	/* specify OS/FORWARD packet */
	UINT_8 ucUserPriority;	/* QoS parameter, convert to TID */

	/* For composing TX descriptor header */
	UINT_8 ucDhcpArpFlag;	/* 1: DHCP|ARP Data */
	UINT_8 ucTC;		/* Traffic Class: 0~4 (HIF TX0), 5 (HIF TX1) */
	UINT_8 ucPacketType;	/* 0: Data, 1: Management Frame */
	UINT_8 ucStaRecIndex;	/* STA_REC index */
	UINT_8 ucBssIndex;	/* BSS_INFO_T index */
	UINT_8 ucWlanIndex;	/* Wlan entry index */

	BOOLEAN fgIs802_1x;	/* TRUE: 802.1x frame */
	BOOLEAN fgIs802_11;	/* TRUE: 802.11 header is present */
	BOOLEAN fgIs802_3;	/* TRUE: 802.3 frame */
	BOOLEAN fgIsVlanExists;	/* TRUE: VLAN tag is exists */
	UINT_8 ucEapolKeyType; /* 1: 1/4; 2: 2/4; 3: 3/4; 4: 4/4 */

	/* BOOLEAN                     fgIsBIP;                *//* Management Frame Protection */
	/* BOOLEAN                     fgIsBasicRate;      *//* Force Basic Rate Transmission */
	/* BOOLEAN                     fgIsMoreData;      *//* More data */
	/* BOOLEAN                     fgIsEOSP;            *//* End of service period */

	/* Special Option */
	UINT_32 u4Option;	/* Special option in bitmask, no ACK, etc... */
	INT_8 cPowerOffset;	/* Per-packet power offset, in 2's complement */
	UINT_16 u2SwSN;		/* SW assigned sequence number */
	UINT_8 ucRetryLimit;	/* The retry limit */
	UINT_32 u4RemainingLifetime;	/* Remaining lifetime, unit:ms */

	/* Control flag */
	UINT_8 ucControlFlag;	/* Control flag in bitmask */

	/* Fixed Rate Option */
	UINT_8 ucRateMode;	/* Rate mode: AUTO, MANUAL_DESC, MANUAL_CR */
	UINT_32 u4FixedRateOption;	/* The rate option, rate code, GI, etc... */

	BOOLEAN fgIsTXDTemplateValid;	/* There is a valid Tx descriptor for this packet */

	/* flattened from PACKET_INFO_T */
	UINT_8 ucMacHeaderLength;	/* MAC header legth */
	UINT_8 ucLlcLength;	/* w/o EtherType */
	UINT_16 u2FrameLength;	/* Total frame length */
	UINT_8 aucEthDestAddr[MAC_ADDR_LEN];	/* Ethernet Destination Address */
	UINT_8 ucPageCount;	/* Required page count for this MSDU */

	/* for TX done tracking */
	UINT_8 ucTxSeqNum;	/* MGMT frame serial number */
	UINT_8 ucPID;		/* PID */
	PFN_TX_DONE_HANDLER pfTxDoneHandler;	/* Tx done handler */

#if CFG_ENABLE_PKT_LIFETIME_PROFILE
	PKT_PROFILE_T rPktProfile;
#endif

	/* To be removed  */
	UINT_8 ucFormatID;	/* 0: MAUI, Linux, Windows NDIS 5.1 */
	/* UINT_16                     u2PalLLH;              *//* PAL Logical Link Header (for BOW network) */
	/* UINT_16                     u2AclSN;                 *//* ACL Sequence Number (for BOW network) */
	UINT_8 ucPsForwardingType;	/* See ENUM_PS_FORWARDING_TYPE_T */
	/* UINT_8                      ucPsSessionID;        *//* PS Session ID specified by the FW for the STA */
	/* TRUE means this is the last packet of the burst for (STA, TID) */
	/* BOOLEAN                     fgIsBurstEnd;         */
#if CFG_M0VE_BA_TO_DRIVER
	UINT_8 ucTID;
#endif

#if CFG_SUPPORT_MULTITHREAD
	/* Compose TxDesc in tx_thread and place here */
	UINT_8 aucTxDescBuffer[NIC_TX_DESC_AND_PADDING_LENGTH];
#endif
	UINT_16 u2CookieLen;
	PUINT_8 pucCookie;
};

/*!A data structure which is identical with HW MAC TX DMA Descriptor */
typedef struct _HW_MAC_TX_DESC_T {
	/* DW 0 */
	UINT_16 u2TxByteCount;
	UINT_8 ucEtherOffset;	/* Ether-Type Offset,  IP checksum offload */
	UINT_8 ucPortIdx_QueueIdx;	/* UDP/TCP checksum offload,  USB NextVLD/TxBURST, Queue index, Port index */
	/* DW 1 */
	UINT_8 ucWlanIdx;
	UINT_8 ucHeaderFormat;	/* Header format, TX descriptor format */
	UINT_8 ucHeaderPadding;	/* Header padding, no ACK, TID, Protect frame */
	UINT_8 ucOwnMAC;

	/* Long Format, the following structure is for long format ONLY */
	/* DW 2 */
	UINT_8 ucType_SubType;	/* Type, Sub-type, NDP, NDPA */
	UINT_8 ucFrag;		/* Sounding, force RTS/CTS, BMC, BIP, Duration, HTC exist, Fragment */
	UINT_8 ucRemainingMaxTxTime;
	UINT_8 ucPowerOffset;	/* Power offset, Disable BA, Timing measurement, Fixed rate */
	/* DW 3 */
	UINT_16 u2TxCountLimit;	/* TX count limit */
	UINT_16 u2SN;		/* SN, HW own, PN valid, SN valid */
	/* DW 4 */
	UINT_32 u4PN1;
	/* DW 5 */
	UINT_8 ucPID;
	UINT_8 ucTxStatus;	/* TXS format, TXS to mcu, TXS to host, DA source, BAR SSN, Power management */
	UINT_16 u2PN2;
	/* DW 6 */
	UINT_8 ucAntID;		/* Fixed rate, Antenna ID */
	UINT_8 ucBandwidth;	/* Bandwidth,  Spatial Extension, Antenna priority, Dynamic bandwidth */
	UINT_16 u2FixedRate;	/* Explicit/implicit beamforming, Fixed rate table, LDPC, GI */
} HW_MAC_TX_DESC_T, *P_HW_MAC_TX_DESC_T, **PP_HW_MAC_TX_DESC_T;

typedef struct _TX_RESOURCE_CONTROL_T {
	/* HW TX queue definition */
	UINT_8 ucDestPortIndex;
	UINT_8 ucDestQueueIndex;
	/* HIF Interrupt status index */
	UINT_8 ucHifTxQIndex;
} TX_RESOURCE_CONTROL_T, *PTX_RESOURCE_CONTROL_T;

typedef struct _TX_TC_TRAFFIC_SETTING_T {
	UINT_8 ucTxDescLength;
	UINT_32 u4RemainingTxTime;
	UINT_8 ucTxCountLimit;
} TX_TC_TRAFFIC_SETTING_T, P_TX_TC_TRAFFIC_SETTING_T;

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

#define TX_INC_CNT(prTxCtrl, eCounter)              \
	{((P_TX_CTRL_T)prTxCtrl)->au4Statistics[eCounter]++; }

#define TX_ADD_CNT(prTxCtrl, eCounter, u8Amount)    \
	{((P_TX_CTRL_T)prTxCtrl)->au4Statistics[eCounter] += (UINT_32)u8Amount; }

#define TX_GET_CNT(prTxCtrl, eCounter)              \
	(((P_TX_CTRL_T)prTxCtrl)->au4Statistics[eCounter])

#define TX_RESET_ALL_CNTS(prTxCtrl)                 \
	{kalMemZero(&prTxCtrl->au4Statistics[0], sizeof(prTxCtrl->au4Statistics)); }
#if CFG_ENABLE_PKT_LIFETIME_PROFILE

#if CFG_PRINT_PKT_LIFETIME_PROFILE
#define PRINT_PKT_PROFILE(_pkt_profile, _note) \
{ \
	if (!(_pkt_profile)->fgIsPrinted) { \
		DBGLOG(TX, TRACE, "X[%lu] E[%lu] D[%lu] HD[%lu] B[%d] RTP[%d] %s\n", \
		(UINT_32)((_pkt_profile)->rHardXmitArrivalTimestamp), \
		(UINT_32)((_pkt_profile)->rEnqueueTimestamp), \
		(UINT_32)((_pkt_profile)->rDequeueTimestamp), \
		(UINT_32)((_pkt_profile)->rHifTxDoneTimestamp), \
		(UINT_8)((_pkt_profile)->ucTcxFreeCount), \
		(UINT_16)((_pkt_profile)->u2RtpSn), \
		(_note))); \
		(_pkt_profile)->fgIsPrinted = TRUE; \
	} \
}
#else
#define PRINT_PKT_PROFILE(_pkt_profile, _note)
#endif

#define CHK_PROFILES_DELTA(_pkt1, _pkt2, _delta) \
	   (CHECK_FOR_TIMEOUT((_pkt1)->rHardXmitArrivalTimestamp, (_pkt2)->rHardXmitArrivalTimestamp, (_delta)) || \
	    CHECK_FOR_TIMEOUT((_pkt1)->rEnqueueTimestamp, (_pkt2)->rEnqueueTimestamp, (_delta)) || \
	    CHECK_FOR_TIMEOUT((_pkt1)->rDequeueTimestamp, (_pkt2)->rDequeueTimestamp, (_delta)) || \
	    CHECK_FOR_TIMEOUT((_pkt1)->rHifTxDoneTimestamp, (_pkt2)->rHifTxDoneTimestamp, (_delta)))

#define CHK_PROFILE_DELTA(_pkt, _delta) \
	   (CHECK_FOR_TIMEOUT((_pkt)->rEnqueueTimestamp, (_pkt)->rHardXmitArrivalTimestamp, (_delta)) || \
	    CHECK_FOR_TIMEOUT((_pkt)->rDequeueTimestamp, (_pkt)->rEnqueueTimestamp, (_delta)) || \
	    CHECK_FOR_TIMEOUT((_pkt)->rHifTxDoneTimestamp, (_pkt)->rDequeueTimestamp, (_delta)))
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

#define HAL_MAC_TX_DESC_SET_DW(_prHwMacTxDesc, _ucOffsetInDw, _ucLengthInDw, _pucValueAddr) \
	kalMemCopy((PUINT_32)(_prHwMacTxDesc) + (_ucOffsetInDw),	\
	(PUINT_8)(_pucValueAddr), DWORD_TO_BYTE(_ucLengthInDw))
#define HAL_MAC_TX_DESC_GET_DW(_prHwMacTxDesc, _ucOffsetInDw, _ucLengthInDw, _pucValueAddr) \
	kalMemCopy((PUINT_8)(_pucValueAddr),		\
	(PUINT_32)(_prHwMacTxDesc) + (_ucOffsetInDw), DWORD_TO_BYTE(_ucLengthInDw))

/* DW 0 */
#define HAL_MAC_TX_DESC_GET_TX_BYTE_COUNT(_prHwMacTxDesc) ((_prHwMacTxDesc)->u2TxByteCount)
#define HAL_MAC_TX_DESC_SET_TX_BYTE_COUNT(_prHwMacTxDesc, _u2TxByteCount) \
	(((_prHwMacTxDesc)->u2TxByteCount) = ((UINT_16)_u2TxByteCount))

#define HAL_MAC_TX_DESC_GET_ETHER_TYPE_OFFSET(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucEtherOffset,		\
	TX_DESC_ETHER_TYPE_OFFSET_MASK, TX_DESC_ETHER_TYPE_OFFSET_OFFSET)
#define HAL_MAC_TX_DESC_SET_ETHER_TYPE_OFFSET(_prHwMacTxDesc, _ucEtherTypeOffset) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucEtherOffset),		\
	((UINT_8)_ucEtherTypeOffset), TX_DESC_ETHER_TYPE_OFFSET_MASK, TX_DESC_ETHER_TYPE_OFFSET_OFFSET)

#define HAL_MAC_TX_DESC_IS_IP_CHKSUM_ENABLED(_prHwMacTxDesc)		\
	(((_prHwMacTxDesc)->ucEtherOffset & TX_DESC_IP_CHKSUM_OFFLOAD)?FALSE:TRUE)
#define HAL_MAC_TX_DESC_SET_IP_CHKSUM(_prHwMacTxDesc)			\
	((_prHwMacTxDesc)->ucEtherOffset |= TX_DESC_IP_CHKSUM_OFFLOAD)
#define HAL_MAC_TX_DESC_UNSET_IP_CHKSUM(_prHwMacTxDesc)			\
	((_prHwMacTxDesc)->ucEtherOffset &= ~TX_DESC_IP_CHKSUM_OFFLOAD)

#define HAL_MAC_TX_DESC_IS_TCP_UDP_CHKSUM_ENABLED(_prHwMacTxDesc)	\
	(((_prHwMacTxDesc)->ucPortIdx_QueueIdx & TX_DESC_TCP_UDP_CHKSUM_OFFLOAD)?FALSE:TRUE)
#define HAL_MAC_TX_DESC_SET_TCP_UDP_CHKSUM(_prHwMacTxDesc)		\
	((_prHwMacTxDesc)->ucPortIdx_QueueIdx |= TX_DESC_TCP_UDP_CHKSUM_OFFLOAD)
#define HAL_MAC_TX_DESC_UNSET_TCP_UDP_CHKSUM(_prHwMacTxDesc)		\
	((_prHwMacTxDesc)->ucPortIdx_QueueIdx &= ~TX_DESC_TCP_UDP_CHKSUM_OFFLOAD)

#define HAL_MAC_TX_DESC_IS_USB_NEXT_VLD_ENABLED(_prHwMacTxDesc)		\
	(((_prHwMacTxDesc)->ucEtherOffset & TX_DESC_USB_NEXT_VLD)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_USB_NEXT_VLD(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucEtherOffset |= TX_DESC_USB_NEXT_VLD)
#define HAL_MAC_TX_DESC_UNSET_USB_NEXT_VLD(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucEtherOffset &= ~TX_DESC_USB_NEXT_VLD)

#define HAL_MAC_TX_DESC_IS_USB_TX_BURST_ENABLED(_prHwMacTxDesc)		\
	(((_prHwMacTxDesc)->ucPortIdx_QueueIdx & TX_DESC_USB_TX_BURST)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_USB_TX_BURST(_prHwMacTxDesc)		\
	((_prHwMacTxDesc)->ucPortIdx_QueueIdx |= TX_DESC_USB_TX_BURST)
#define HAL_MAC_TX_DESC_UNSET_USB_TX_BURST(_prHwMacTxDesc)		\
	((_prHwMacTxDesc)->ucPortIdx_QueueIdx &= ~TX_DESC_USB_TX_BURST)

#define HAL_MAC_TX_DESC_GET_QUEUE_INDEX(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucPortIdx_QueueIdx, TX_DESC_QUEUE_INDEX_MASK, TX_DESC_QUEUE_INDEX_OFFSET)
#define HAL_MAC_TX_DESC_SET_QUEUE_INDEX(_prHwMacTxDesc, _ucQueueIndex) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucPortIdx_QueueIdx),	\
	((UINT_8)_ucQueueIndex), TX_DESC_QUEUE_INDEX_MASK, TX_DESC_QUEUE_INDEX_OFFSET)

#define HAL_MAC_TX_DESC_GET_PORT_INDEX(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucPortIdx_QueueIdx, TX_DESC_PORT_INDEX, TX_DESC_PORT_INDEX_OFFSET)
#define HAL_MAC_TX_DESC_SET_PORT_INDEX(_prHwMacTxDesc, _ucPortIndex) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucPortIdx_QueueIdx),	\
	((UINT_8)_ucPortIndex), TX_DESC_PORT_INDEX, TX_DESC_PORT_INDEX_OFFSET)

/* DW 1 */
#define HAL_MAC_TX_DESC_GET_WLAN_INDEX(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucWlanIdx)
#define HAL_MAC_TX_DESC_SET_WLAN_INDEX(_prHwMacTxDesc, _ucWlanIdx) \
	(((_prHwMacTxDesc)->ucWlanIdx) = (_ucWlanIdx))

#define HAL_MAC_TX_DESC_IS_LONG_FORMAT(_prHwMacTxDesc) (((_prHwMacTxDesc)->ucHeaderFormat & TX_DESC_FORMAT)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_LONG_FORMAT(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucHeaderFormat |= TX_DESC_FORMAT)
#define HAL_MAC_TX_DESC_SET_SHORT_FORMAT(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucHeaderFormat &= ~TX_DESC_FORMAT)

#define HAL_MAC_TX_DESC_GET_HEADER_FORMAT(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucHeaderFormat,	\
	TX_DESC_HEADER_FORMAT_MASK, TX_DESC_HEADER_FORMAT_OFFSET)
#define HAL_MAC_TX_DESC_SET_HEADER_FORMAT(_prHwMacTxDesc, _ucHdrFormat) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucHeaderFormat),	\
	((UINT_8)_ucHdrFormat), TX_DESC_HEADER_FORMAT_MASK, TX_DESC_HEADER_FORMAT_OFFSET)

/* HF = 0x00, 802.11 normal mode */
#define HAL_MAC_TX_DESC_IS_MORE_DATA(_prHwMacTxDesc)		\
	(((_prHwMacTxDesc)->ucHeaderFormat & TX_DESC_NON_802_11_MORE_DATA)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_MORE_DATA(_prHwMacTxDesc)		\
	((_prHwMacTxDesc)->ucHeaderFormat |= TX_DESC_NON_802_11_MORE_DATA)
#define HAL_MAC_TX_DESC_UNSET_MORE_DATA(_prHwMacTxDesc)		\
	((_prHwMacTxDesc)->ucHeaderFormat &= ~TX_DESC_NON_802_11_MORE_DATA)

#define HAL_MAC_TX_DESC_IS_REMOVE_VLAN(_prHwMacTxDesc)		\
	(((_prHwMacTxDesc)->ucHeaderFormat & TX_DESC_NON_802_11_REMOVE_VLAN)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_REMOVE_VLAN(_prHwMacTxDesc)		\
	((_prHwMacTxDesc)->ucHeaderFormat |= TX_DESC_NON_802_11_REMOVE_VLAN)
#define HAL_MAC_TX_DESC_UNSET_REMOVE_VLAN(_prHwMacTxDesc)	\
	((_prHwMacTxDesc)->ucHeaderFormat &= ~TX_DESC_NON_802_11_REMOVE_VLAN)

#define HAL_MAC_TX_DESC_IS_VLAN(_prHwMacTxDesc)		\
	(((_prHwMacTxDesc)->ucHeaderFormat & TX_DESC_NON_802_11_VLAN_FIELD)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_VLAN(_prHwMacTxDesc)	\
	((_prHwMacTxDesc)->ucHeaderFormat |= TX_DESC_NON_802_11_VLAN_FIELD)
#define HAL_MAC_TX_DESC_UNSET_VLAN(_prHwMacTxDesc)	\
	((_prHwMacTxDesc)->ucHeaderFormat &= ~TX_DESC_NON_802_11_VLAN_FIELD)

#define HAL_MAC_TX_DESC_IS_ETHERNET_II(_prHwMacTxDesc)		\
	(((_prHwMacTxDesc)->ucHeaderFormat & TX_DESC_NON_802_11_ETHERNET_II)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_ETHERNET_II(_prHwMacTxDesc)		\
	((_prHwMacTxDesc)->ucHeaderFormat |= TX_DESC_NON_802_11_ETHERNET_II)
#define HAL_MAC_TX_DESC_UNSET_ETHERNET_II(_prHwMacTxDesc)	\
	((_prHwMacTxDesc)->ucHeaderFormat &= ~TX_DESC_NON_802_11_ETHERNET_II)

/* HF = 0x00/0x11, 802.11 normal/enhancement mode */
#define HAL_MAC_TX_DESC_IS_EOSP(_prHwMacTxDesc)	\
	(((_prHwMacTxDesc)->ucHeaderFormat & TX_DESC_NON_802_11_EOSP)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_EOSP(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucHeaderFormat |= TX_DESC_NON_802_11_EOSP)
#define HAL_MAC_TX_DESC_UNSET_EOSP(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucHeaderFormat &= ~TX_DESC_NON_802_11_EOSP)

/* HF = 0x11, 802.11 enhancement mode */
#define HAL_MAC_TX_DESC_IS_AMSDU(_prHwMacTxDesc)	\
	(((_prHwMacTxDesc)->ucHeaderFormat & TX_DESC_ENH_802_11_AMSDU)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_AMSDU(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucHeaderFormat |= TX_DESC_ENH_802_11_AMSDU)
#define HAL_MAC_TX_DESC_UNSET_AMSDU(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucHeaderFormat &= ~TX_DESC_ENH_802_11_AMSDU)

/* HF = 0x10, non-802.11 */
#define HAL_MAC_TX_DESC_GET_802_11_HEADER_LENGTH(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucHeaderFormat,	\
	TX_DESC_NOR_802_11_HEADER_LENGTH_MASK, TX_DESC_NOR_802_11_HEADER_LENGTH_OFFSET)
#define HAL_MAC_TX_DESC_SET_802_11_HEADER_LENGTH(_prHwMacTxDesc, _ucHdrLength) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucHeaderFormat),	\
	((UINT_8)_ucHdrLength), TX_DESC_NOR_802_11_HEADER_LENGTH_MASK, TX_DESC_NOR_802_11_HEADER_LENGTH_OFFSET)

#define HAL_MAC_TX_DESC_GET_HEADER_PADDING(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucHeaderPadding,	\
	TX_DESC_HEADER_PADDING_LENGTH_MASK, TX_DESC_HEADER_PADDING_LENGTH_OFFSET)
#define HAL_MAC_TX_DESC_SET_HEADER_PADDING(_prHwMacTxDesc, _ucHdrPadding) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucHeaderPadding),	\
	((UINT_8)_ucHdrPadding), TX_DESC_HEADER_PADDING_LENGTH_MASK, TX_DESC_HEADER_PADDING_LENGTH_OFFSET)

#define HAL_MAC_TX_DESC_IS_HEADER_PADDING_IN_THE_HEAD(_prHwMacTxDesc)	\
	(((_prHwMacTxDesc)->ucHeaderPadding & TX_DESC_HEADER_PADDING_MODE)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_HEADER_PADDING_IN_THE_HEAD(_prHwMacTxDesc)	\
	((_prHwMacTxDesc)->ucHeaderPadding |= TX_DESC_HEADER_PADDING_MODE)
#define HAL_MAC_TX_DESC_SET_HEADER_PADDING_IN_THE_TAIL(_prHwMacTxDesc)	\
	((_prHwMacTxDesc)->ucHeaderPadding &= ~TX_DESC_HEADER_PADDING_MODE)

#define HAL_MAC_TX_DESC_IS_NO_ACK(_prHwMacTxDesc) (((_prHwMacTxDesc)->ucHeaderPadding & TX_DESC_NO_ACK)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_NO_ACK(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucHeaderPadding |= TX_DESC_NO_ACK)
#define HAL_MAC_TX_DESC_UNSET_NO_ACK(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucHeaderPadding &= ~TX_DESC_NO_ACK)

#define HAL_MAC_TX_DESC_GET_TID(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucHeaderPadding, TX_DESC_TID_MASK, TX_DESC_TID_OFFSET)
#define HAL_MAC_TX_DESC_SET_TID(_prHwMacTxDesc, _ucTID) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucHeaderPadding), ((UINT_8)_ucTID), TX_DESC_TID_MASK, TX_DESC_TID_OFFSET)

#define HAL_MAC_TX_DESC_IS_PROTECTION(_prHwMacTxDesc)	\
	(((_prHwMacTxDesc)->ucHeaderPadding & TX_DESC_PROTECTED_FRAME)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_PROTECTION(_prHwMacTxDesc)	\
	((_prHwMacTxDesc)->ucHeaderPadding |= TX_DESC_PROTECTED_FRAME)
#define HAL_MAC_TX_DESC_UNSET_PROTECTION(_prHwMacTxDesc)	\
	((_prHwMacTxDesc)->ucHeaderPadding &= ~TX_DESC_PROTECTED_FRAME)

#define HAL_MAC_TX_DESC_GET_OWN_MAC_INDEX(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucOwnMAC, TX_DESC_OWN_MAC_MASK, TX_DESC_OWN_MAC_OFFSET)
#define HAL_MAC_TX_DESC_SET_OWN_MAC_INDEX(_prHwMacTxDesc, _ucOwnMacIdx) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucOwnMAC), ((UINT_8)_ucOwnMacIdx),	\
	TX_DESC_OWN_MAC_MASK, TX_DESC_OWN_MAC_OFFSET)

/* DW 2 */
#define HAL_MAC_TX_DESC_GET_SUB_TYPE(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucType_SubType, TX_DESC_SUB_TYPE_MASK, TX_DESC_SUB_TYPE_OFFSET)
#define HAL_MAC_TX_DESC_SET_SUB_TYPE(_prHwMacTxDesc, _ucSubType) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucType_SubType),	\
	((UINT_8)_ucSubType), TX_DESC_SUB_TYPE_MASK, TX_DESC_SUB_TYPE_OFFSET)

#define HAL_MAC_TX_DESC_GET_TYPE(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucType_SubType, TX_DESC_TYPE_MASK, TX_DESC_TYPE_OFFSET)
#define HAL_MAC_TX_DESC_SET_TYPE(_prHwMacTxDesc, _ucType) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucType_SubType), ((UINT_8)_ucType), TX_DESC_TYPE_MASK, TX_DESC_TYPE_OFFSET)

#define HAL_MAC_TX_DESC_IS_NDP(_prHwMacTxDesc) (((_prHwMacTxDesc)->ucType_SubType & TX_DESC_NDP)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_NDP(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucType_SubType |= TX_DESC_NDP)
#define HAL_MAC_TX_DESC_UNSET_NDP(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucType_SubType &= ~TX_DESC_NDP)

#define HAL_MAC_TX_DESC_IS_NDPA(_prHwMacTxDesc) (((_prHwMacTxDesc)->ucType_SubType & TX_DESC_NDPA)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_NDPA(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucType_SubType |= TX_DESC_NDPA)
#define HAL_MAC_TX_DESC_UNSET_NDPA(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucType_SubType &= ~TX_DESC_NDPA)

#define HAL_MAC_TX_DESC_IS_SOUNDING_FRAME(_prHwMacTxDesc) (((_prHwMacTxDesc)->ucFrag & TX_DESC_SOUNDING)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_SOUNDING_FRAME(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucFrag |= TX_DESC_SOUNDING)
#define HAL_MAC_TX_DESC_UNSET_SOUNDING_FRAME(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucFrag &= ~TX_DESC_SOUNDING)

#define HAL_MAC_TX_DESC_IS_FORCE_RTS_CTS_EN(_prHwMacTxDesc)	\
	(((_prHwMacTxDesc)->ucFrag & TX_DESC_FORCE_RTS_CTS)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_FORCE_RTS_CTS(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucFrag |= TX_DESC_FORCE_RTS_CTS)
#define HAL_MAC_TX_DESC_UNSET_FORCE_RTS_CTS(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucFrag &= ~TX_DESC_FORCE_RTS_CTS)

#define HAL_MAC_TX_DESC_IS_BMC(_prHwMacTxDesc) (((_prHwMacTxDesc)->ucFrag & TX_DESC_BROADCAST_MULTICAST)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_BMC(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucFrag |= TX_DESC_BROADCAST_MULTICAST)
#define HAL_MAC_TX_DESC_UNSET_BMC(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucFrag &= ~TX_DESC_BROADCAST_MULTICAST)

#define HAL_MAC_TX_DESC_IS_BIP(_prHwMacTxDesc) (((_prHwMacTxDesc)->ucFrag & TX_DESC_BIP_PROTECTED)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_BIP(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucFrag |= TX_DESC_BIP_PROTECTED)
#define HAL_MAC_TX_DESC_UNSET_BIP(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucFrag &= ~TX_DESC_BIP_PROTECTED)

#define HAL_MAC_TX_DESC_IS_DURATION_CONTROL_BY_SW(_prHwMacTxDesc)	\
	(((_prHwMacTxDesc)->ucFrag & TX_DESC_DURATION_FIELD_CONTROL)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_DURATION_CONTROL_BY_SW(_prHwMacTxDesc)	\
	((_prHwMacTxDesc)->ucFrag |= TX_DESC_DURATION_FIELD_CONTROL)
#define HAL_MAC_TX_DESC_SET_DURATION_CONTROL_BY_HW(_prHwMacTxDesc)	\
	((_prHwMacTxDesc)->ucFrag &= ~TX_DESC_DURATION_FIELD_CONTROL)

#define HAL_MAC_TX_DESC_IS_HTC_EXIST(_prHwMacTxDesc) (((_prHwMacTxDesc)->ucFrag & TX_DESC_HTC_EXISTS)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_HTC_EXIST(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucFrag |= TX_DESC_HTC_EXISTS)
#define HAL_MAC_TX_DESC_UNSET_HTC_EXIST(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucFrag &= ~TX_DESC_HTC_EXISTS)

#define HAL_MAC_TX_DESC_IS_FRAG_PACKET(_prHwMacTxDesc) (((_prHwMacTxDesc)->ucFrag & TX_DESC_FRAGMENT_MASK)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_GET_FRAG_PACKET_POS(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucFrag, TX_DESC_FRAGMENT_MASK, TX_DESC_FRAGMENT_OFFSET)
#define HAL_MAC_TX_DESC_SET_FRAG_PACKET_POS(_prHwMacTxDesc, _ucFragPos) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucFrag),	\
	((UINT_8)_ucFragPos), TX_DESC_FRAGMENT_MASK, TX_DESC_FRAGMENT_OFFSET)

/* For driver */
/* in unit of 32TU */
#define HAL_MAC_TX_DESC_GET_REMAINING_LIFE_TIME(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucRemainingMaxTxTime)
#define HAL_MAC_TX_DESC_SET_REMAINING_LIFE_TIME(_prHwMacTxDesc, _ucLifeTime)	\
	((_prHwMacTxDesc)->ucRemainingMaxTxTime = (_ucLifeTime))
/* in unit of ms (minimal value is about 40ms) */
#define HAL_MAC_TX_DESC_GET_REMAINING_LIFE_TIME_IN_MS(_prHwMacTxDesc) \
	(TU_TO_MSEC(HAL_MAC_TX_DESC_GET_REMAINING_LIFE_TIME(_prHwMacTxDesc) << TX_DESC_LIFE_TIME_UNIT_IN_POWER_OF_2))
#define HAL_MAC_TX_DESC_SET_REMAINING_LIFE_TIME_IN_MS(_prHwMacTxDesc, _u4LifeTimeMs) \
	{ \
		UINT_32 u4LifeTimeInUnit =	\
		((MSEC_TO_USEC(_u4LifeTimeMs) / USEC_PER_TU) >> TX_DESC_LIFE_TIME_UNIT_IN_POWER_OF_2); \
		if (u4LifeTimeInUnit >= BIT(8)) { \
			u4LifeTimeInUnit = BITS(0, 7); \
		} \
		else if ((_u4LifeTimeMs != TX_DESC_TX_TIME_NO_LIMIT)	\
			&& (u4LifeTimeInUnit == TX_DESC_TX_TIME_NO_LIMIT)) { \
			u4LifeTimeInUnit = 1; \
		} \
		HAL_MAC_TX_DESC_SET_REMAINING_LIFE_TIME(_prHwMacTxDesc, (UINT_8)u4LifeTimeInUnit); \
	}

#define HAL_MAC_TX_DESC_GET_POWER_OFFSET(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucPowerOffset, TX_DESC_POWER_OFFSET_MASK, 0)
#define HAL_MAC_TX_DESC_SET_POWER_OFFSET(_prHwMacTxDesc, _ucPowerOffset) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucPowerOffset), ((UINT_8)_ucPowerOffset), TX_DESC_POWER_OFFSET_MASK, 0)

#define HAL_MAC_TX_DESC_IS_BA_DISABLE(_prHwMacTxDesc)	\
	(((_prHwMacTxDesc)->ucPowerOffset & TX_DESC_BA_DISABLE)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_BA_DISABLE(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucPowerOffset |= TX_DESC_BA_DISABLE)
#define HAL_MAC_TX_DESC_SET_BA_ENABLE(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucPowerOffset &= ~TX_DESC_BA_DISABLE)

#define HAL_MAC_TX_DESC_IS_TIMING_MEASUREMENT(_prHwMacTxDesc)	\
	(((_prHwMacTxDesc)->ucPowerOffset & TX_DESC_TIMING_MEASUREMENT)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_TIMING_MEASUREMENT(_prHwMacTxDesc)	\
	((_prHwMacTxDesc)->ucPowerOffset |= TX_DESC_TIMING_MEASUREMENT)
#define HAL_MAC_TX_DESC_UNSET_TIMING_MEASUREMENT(_prHwMacTxDesc)	\
	((_prHwMacTxDesc)->ucPowerOffset &= ~TX_DESC_TIMING_MEASUREMENT)

#define HAL_MAC_TX_DESC_IS_FIXED_RATE_ENABLE(_prHwMacTxDesc)	\
	(((_prHwMacTxDesc)->ucPowerOffset & TX_DESC_FIXED_RATE)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_FIXED_RATE_ENABLE(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucPowerOffset |= TX_DESC_FIXED_RATE)
#define HAL_MAC_TX_DESC_SET_FIXED_RATE_DISABLE(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucPowerOffset &= ~TX_DESC_FIXED_RATE)

/* DW 3 */
#define HAL_MAC_TX_DESC_GET_SW_RESERVED(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->u2TxCountLimit,	\
	TX_DESC_SW_RESERVED_MASK, TX_DESC_SW_RESERVED_OFFSET)
#define HAL_MAC_TX_DESC_SET_SW_RESERVED(_prHwMacTxDesc, _ucSwReserved) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u2TxCountLimit),	\
	((UINT_8)_ucSwReserved), TX_DESC_SW_RESERVED_MASK, TX_DESC_SW_RESERVED_OFFSET)
#define HAL_MAC_TX_DESC_GET_TX_COUNT(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->u2TxCountLimit,	\
	TX_DESC_TX_COUNT_MASK, TX_DESC_TX_COUNT_OFFSET)
#define HAL_MAC_TX_DESC_SET_TX_COUNT(_prHwMacTxDesc, _ucTxCountLimit) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u2TxCountLimit),	\
	((UINT_8)_ucTxCountLimit), TX_DESC_TX_COUNT_MASK, TX_DESC_TX_COUNT_OFFSET)
#define HAL_MAC_TX_DESC_GET_REMAINING_TX_COUNT(_prHwMacTxDesc)	\
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->u2TxCountLimit,	\
	TX_DESC_REMAINING_TX_COUNT_MASK, TX_DESC_REMAINING_TX_COUNT_OFFSET)
#define HAL_MAC_TX_DESC_SET_REMAINING_TX_COUNT(_prHwMacTxDesc, _ucTxCountLimit) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u2TxCountLimit), ((UINT_8)_ucTxCountLimit),	\
	TX_DESC_REMAINING_TX_COUNT_MASK, TX_DESC_REMAINING_TX_COUNT_OFFSET)
#define HAL_MAC_TX_DESC_GET_SEQUENCE_NUMBER(_prHwMacTxDesc) \
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->u2SN, TX_DESC_SEQUENCE_NUMBER, 0)
#define HAL_MAC_TX_DESC_SET_SEQUENCE_NUMBER(_prHwMacTxDesc, _u2SN) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u2SN), ((UINT_16)_u2SN), TX_DESC_SEQUENCE_NUMBER, 0)
#define HAL_MAC_TX_DESC_SET_HW_RESERVED(_prHwMacTxDesc, _ucHwReserved) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u2SN), ((UINT_8)_ucHwReserved),	\
	TX_DESC_HW_RESERVED_MASK, TX_DESC_HW_RESERVED_OFFSET)
#define HAL_MAC_TX_DESC_IS_TXD_SN_VALID(_prHwMacTxDesc) (((_prHwMacTxDesc)->u2SN & TX_DESC_SN_IS_VALID)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_TXD_SN_VALID(_prHwMacTxDesc) ((_prHwMacTxDesc)->u2SN |= TX_DESC_SN_IS_VALID)
#define HAL_MAC_TX_DESC_SET_TXD_SN_INVALID(_prHwMacTxDesc) ((_prHwMacTxDesc)->u2SN &= ~TX_DESC_SN_IS_VALID)

#define HAL_MAC_TX_DESC_IS_TXD_PN_VALID(_prHwMacTxDesc) (((_prHwMacTxDesc)->u2SN & TX_DESC_PN_IS_VALID)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_TXD_PN_VALID(_prHwMacTxDesc) ((_prHwMacTxDesc)->u2SN |= TX_DESC_PN_IS_VALID)
#define HAL_MAC_TX_DESC_SET_TXD_PN_INVALID(_prHwMacTxDesc) ((_prHwMacTxDesc)->u2SN &= ~TX_DESC_PN_IS_VALID)

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
	((UINT_32)_u4PN_0_31) = (_prHwMacTxDesc)->u4PN1; \
	((UINT_16)_u2PN_32_47) = (_prHwMacTxDesc)->u2PN2; \
}
#define HAL_MAC_TX_DESC_SET_PN(_prHwMacTxDesc, _u4PN_0_31, _u2PN_32_47) \
{ \
	(_prHwMacTxDesc)->u4PN1 = ((UINT_32)_u4PN_0_31); \
	(_prHwMacTxDesc)->u2PN2 = ((UINT_16)_u2PN_32_47); \
}

#define HAL_MAC_TX_DESC_ASSIGN_PN_BY_SW(_prHwMacTxDesc, _u4PN_0_31, _u2PN_32_47) \
{ \
	HAL_MAC_TX_DESC_SET_PN(_prHwMacTxDesc, _u4PN_0_31, _u2PN_32_47); \
	HAL_MAC_TX_DESC_SET_TXD_PN_VALID(_prHwMacTxDesc); \
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
	TX_DESC_GET_FIELD((_prHwMacTxDesc)->ucTxStatus, TX_DESC_TX_STATUS_FORMAT, TX_DESC_TX_STATUS_FORMAT_OFFSET)
#define HAL_MAC_TX_DESC_SET_TXS_FORMAT(_prHwMacTxDesc, _ucTXSFormat) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucTxStatus),	\
	((UINT_8)_ucTXSFormat), TX_DESC_TX_STATUS_FORMAT, TX_DESC_TX_STATUS_FORMAT_OFFSET)

#define HAL_MAC_TX_DESC_IS_TXS_TO_MCU(_prHwMacTxDesc)	\
	(((_prHwMacTxDesc)->ucTxStatus & TX_DESC_TX_STATUS_TO_MCU)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_TXS_TO_MCU(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucTxStatus |= TX_DESC_TX_STATUS_TO_MCU)
#define HAL_MAC_TX_DESC_UNSET_TXS_TO_MCU(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucTxStatus &= ~TX_DESC_TX_STATUS_TO_MCU)

#define HAL_MAC_TX_DESC_IS_TXS_TO_HOST(_prHwMacTxDesc)	\
	(((_prHwMacTxDesc)->ucTxStatus & TX_DESC_TX_STATUS_TO_HOST)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_TXS_TO_HOST(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucTxStatus |= TX_DESC_TX_STATUS_TO_HOST)
#define HAL_MAC_TX_DESC_UNSET_TXS_TO_HOST(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucTxStatus &= ~TX_DESC_TX_STATUS_TO_HOST)

#define HAL_MAC_TX_DESC_IS_DA_FROM_WTBL(_prHwMacTxDesc)	\
	(((_prHwMacTxDesc)->ucPowerOffset & TX_DESC_DA_SOURCE)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_DA_FROM_WTBL(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucPowerOffset |= TX_DESC_DA_SOURCE)
#define HAL_MAC_TX_DESC_SET_DA_FROM_MSDU(_prHwMacTxDesc) ((_prHwMacTxDesc)->ucPowerOffset &= ~TX_DESC_DA_SOURCE)

#define HAL_MAC_TX_DESC_IS_SW_BAR_SSN(_prHwMacTxDesc)	\
	(((_prHwMacTxDesc)->ucPowerOffset & TX_DESC_BAR_SSN_CONTROL)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_SW_BAR_SSN(_prHwMacTxDesc)	\
	((_prHwMacTxDesc)->ucPowerOffset |= TX_DESC_BAR_SSN_CONTROL)
#define HAL_MAC_TX_DESC_SET_HW_BAR_SSN(_prHwMacTxDesc)	\
	((_prHwMacTxDesc)->ucPowerOffset &= ~TX_DESC_BAR_SSN_CONTROL)

#define HAL_MAC_TX_DESC_IS_SW_PM_CONTROL(_prHwMacTxDesc)	\
	(((_prHwMacTxDesc)->ucPowerOffset & TX_DESC_POWER_MANAGEMENT_CONTROL)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_SW_PM_CONTROL(_prHwMacTxDesc)	\
	((_prHwMacTxDesc)->ucPowerOffset |= TX_DESC_POWER_MANAGEMENT_CONTROL)
#define HAL_MAC_TX_DESC_SET_HW_PM_CONTROL(_prHwMacTxDesc)	\
	((_prHwMacTxDesc)->ucPowerOffset &= ~TX_DESC_POWER_MANAGEMENT_CONTROL)

/* DW 6 */
#define HAL_MAC_TX_DESC_IS_CR_FIXED_RATE_MODE(_prHwMacTxDesc)	\
	(((_prHwMacTxDesc)->ucAntID & TX_DESC_FIXED_RATE_MODE)?TRUE:FALSE)
#define HAL_MAC_TX_DESC_SET_FIXED_RATE_MODE_TO_DESC(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucAntID &= ~TX_DESC_FIXED_RATE_MODE)
#define HAL_MAC_TX_DESC_SET_FIXED_RATE_MODE_TO_CR(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucAntID |= TX_DESC_FIXED_RATE_MODE)

#define HAL_MAC_TX_DESC_SET_FR_ANTENNA_ID(_prHwMacTxDesc, _ucAntId) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucAntID), ((UINT_8)_ucAntId), TX_DESC_ANTENNA_INDEX_MASK, 2)

#define HAL_MAC_TX_DESC_SET_FR_BW(_prHwMacTxDesc, ucBw) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucBandwidth), ((UINT_8)ucBw), TX_DESC_BANDWIDTH_MASK, 0)

#define HAL_MAC_TX_DESC_SET_FR_SPE_EN(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucBandwidth |= TX_DESC_BANDWIDTH_MASK)

#define HAL_MAC_TX_DESC_SET_FR_ANTENNA_PRIORITY(_prHwMacTxDesc, _ucAntPri) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->ucBandwidth), ((UINT_8)_ucAntPri), TX_DESC_ANTENNA_PRIORITY, 4)

#define HAL_MAC_TX_DESC_SET_FR_DYNAMIC_BW_RTS(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->ucBandwidth |= TX_DESC_DYNAMIC_BANDWIDTH)

#define HAL_MAC_TX_DESC_SET_FR_ETX_BF(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2FixedRate |= TX_DESC_EXPLICIT_BEAMFORMING)

#define HAL_MAC_TX_DESC_SET_FR_ITX_BF(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2FixedRate |= TX_DESC_IMPLICIT_BEAMFORMING)

#define HAL_MAC_TX_DESC_SET_FR_RATE(_prHwMacTxDesc, _u2RatetoFixed) \
	TX_DESC_SET_FIELD(((_prHwMacTxDesc)->u2FixedRate), ((UINT_8)_u2RatetoFixed), TX_DESC_FIXDE_RATE_MASK, 2)

#define HAL_MAC_TX_DESC_SET_FR_LDPC(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2FixedRate |= TX_DESC_LDPC)

#define HAL_MAC_TX_DESC_SET_FR_SHORT_GI(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2FixedRate |= TX_DESC_GUARD_INTERVAL)

#define HAL_MAC_TX_DESC_SET_FR_NORMAL_GI(_prHwMacTxDesc) \
	((_prHwMacTxDesc)->u2FixedRate &= ~TX_DESC_GUARD_INTERVAL)

#define HAL_MAC_TX_DESC_SET_FIX_RATE(_BssInfo) \
	(((_BssInfo->u2HwDefaultFixedRateCode << 2) & TX_DESC_FIXDE_RATE_MASK) << 16)

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
VOID nicTxInitialize(IN P_ADAPTER_T prAdapter);

WLAN_STATUS nicTxAcquireResource(IN P_ADAPTER_T prAdapter, IN UINT_8 ucTC, IN UINT_8 ucPageCount);

WLAN_STATUS nicTxPollingResource(IN P_ADAPTER_T prAdapter, IN UINT_8 ucTC);

BOOLEAN nicTxReleaseResource(IN P_ADAPTER_T prAdapter, IN UINT_16 *au2TxRlsCnt);

BOOLEAN nicTxReleaseTCResource(IN P_ADAPTER_T prAdapter, IN UINT_8 ucTC, IN UINT_16 u2ReleasePageCnt);

VOID nicTxReleaseMsduResource(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfoListHead);

WLAN_STATUS nicTxResetResource(IN P_ADAPTER_T prAdapter);

UINT_16 nicTxGetResource(IN P_ADAPTER_T prAdapter, IN UINT_8 ucTC);

UINT_8 nicTxGetFrameResourceType(IN UINT_8 eFrameType, IN P_MSDU_INFO_T prMsduInfo);

UINT_8 nicTxGetCmdResourceType(IN P_CMD_INFO_T prCmdInfo);

VOID
nicTxFillDesc(IN P_ADAPTER_T prAdapter,
	      IN P_MSDU_INFO_T prMsduInfo, OUT PUINT_8 prTxDescBuffer, OUT PUINT_8 pucTxDescLength);

VOID
nicTxComposeSecurityFrameDesc(IN P_ADAPTER_T prAdapter,
			      IN P_CMD_INFO_T prCmdInfo, OUT PUINT_8 prTxDescBuffer, OUT PUINT_8 pucTxDescLength);

WLAN_STATUS nicTxMsduInfoList(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfoListHead);

#if CFG_SUPPORT_MULTITHREAD
WLAN_STATUS nicTxMsduInfoListMthread(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfoListHead);

UINT_32 nicTxMsduQueueMthread(IN P_ADAPTER_T prAdapter);
#endif

WLAN_STATUS nicTxMsduQueue(IN P_ADAPTER_T prAdapter, UINT_8 ucPortIdx, P_QUE_T prQue);

WLAN_STATUS nicTxCmd(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN UINT_8 ucTC);

VOID nicTxRelease(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgProcTxDoneHandler);

VOID nicProcessTxInterrupt(IN P_ADAPTER_T prAdapter);

VOID nicTxFreeMsduInfoPacket(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfoListHead);

VOID nicTxReturnMsduInfo(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfoListHead);

BOOLEAN nicTxFillMsduInfo(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN P_NATIVE_PACKET prNdisPacket);

WLAN_STATUS nicTxAdjustTcq(IN P_ADAPTER_T prAdapter);

WLAN_STATUS nicTxFlush(IN P_ADAPTER_T prAdapter);

#if CFG_ENABLE_FW_DOWNLOAD
WLAN_STATUS nicTxInitCmd(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo);

WLAN_STATUS nicTxInitResetResource(IN P_ADAPTER_T prAdapter);
#endif

WLAN_STATUS nicTxEnqueueMsdu(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo);

UINT_8 nicTxGetWlanIdx(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIdx, IN UINT_8 ucStaRecIdx);

BOOLEAN nicTxIsMgmtResourceEnough(IN P_ADAPTER_T prAdapter);

UINT_32 nicTxGetFreeCmdCount(IN P_ADAPTER_T prAdapter);

UINT_8 nicTxGetPageCount(IN UINT_32 u4FrameLength, IN BOOLEAN fgIncludeDesc);

UINT_8 nicTxGetCmdPageCount(IN P_CMD_INFO_T prCmdInfo);

WLAN_STATUS nicTxGenerateDescTemplate(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec);

VOID nicTxFreeDescTemplate(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec);

VOID
nicTxSetMngPacket(IN P_ADAPTER_T prAdapter,
		  IN P_MSDU_INFO_T prMsduInfo,
		  IN UINT_8 ucBssIndex,
		  IN UINT_8 ucStaRecIndex,
		  IN UINT_8 ucMacHeaderLength,
		  IN UINT_16 u2FrameLength, IN PFN_TX_DONE_HANDLER pfTxDoneHandler, IN UINT_8 ucRateMode);

VOID
nicTxSetDataPacket(IN P_ADAPTER_T prAdapter,
		   IN P_MSDU_INFO_T prMsduInfo,
		   IN UINT_8 ucBssIndex,
		   IN UINT_8 ucStaRecIndex,
		   IN UINT_8 ucMacHeaderLength,
		   IN UINT_16 u2FrameLength,
		   IN PFN_TX_DONE_HANDLER pfTxDoneHandler,
		   IN UINT_8 ucRateMode,
		   IN ENUM_TX_PACKET_SRC_T eSrc, IN UINT_8 ucTID, IN BOOLEAN fgIs802_11Frame, IN BOOLEAN fgIs1xFrame);

VOID nicTxFillDescByPktOption(IN P_MSDU_INFO_T prMsduInfo, IN P_HW_MAC_TX_DESC_T prTxDesc);

VOID nicTxConfigPktOption(IN P_MSDU_INFO_T prMsduInfo, IN UINT_32 u4OptionMask, IN BOOLEAN fgSetOption);

VOID nicTxFillDescByPktControl(P_MSDU_INFO_T prMsduInfo, P_HW_MAC_TX_DESC_T prTxDesc);

VOID nicTxConfigPktControlFlag(IN P_MSDU_INFO_T prMsduInfo, IN UINT_8 ucControlFlagMask, IN BOOLEAN fgSetFlag);

VOID nicTxSetPktLifeTime(IN P_MSDU_INFO_T prMsduInfo, IN UINT_32 u4TxLifeTimeInMs);

VOID nicTxSetPktRetryLimit(IN P_MSDU_INFO_T prMsduInfo, IN UINT_8 ucRetryLimit);

VOID nicTxSetPktPowerOffset(IN P_MSDU_INFO_T prMsduInfo, IN INT_8 cPowerOffset);

VOID nicTxSetPktSequenceNumber(IN P_MSDU_INFO_T prMsduInfo, IN UINT_16 u2SN);

VOID nicTxSetPktMacTxQue(IN P_MSDU_INFO_T prMsduInfo, IN UINT_8 ucMacTxQue);

VOID
nicTxSetPktFixedRateOptionFull(IN P_MSDU_INFO_T prMsduInfo,
			       IN UINT_16 u2RateCode,
			       IN UINT_8 ucBandwidth,
			       IN BOOLEAN fgShortGI,
			       IN BOOLEAN fgLDPC,
			       IN BOOLEAN fgDynamicBwRts,
			       IN BOOLEAN fgSpatialExt,
			       IN BOOLEAN fgEtxBeamforming,
			       IN BOOLEAN fgItxBeamforming, IN UINT_8 ucAntennaIndex, IN UINT_8 ucAntennaPriority);

VOID
nicTxSetPktFixedRateOption(IN P_MSDU_INFO_T prMsduInfo,
			   IN UINT_16 u2RateCode,
			   IN UINT_8 ucBandwidth, IN BOOLEAN fgShortGI, IN BOOLEAN fgDynamicBwRts);

VOID nicTxSetPktLowestFixedRate(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo);

VOID nicTxSetPktMoreData(IN P_MSDU_INFO_T prCurrentMsduInfo, IN BOOLEAN fgSetMoreDataBit);

VOID nicTxSetPktEOSP(IN P_MSDU_INFO_T prCurrentMsduInfo, IN BOOLEAN fgSetEOSPBit);

UINT_8 nicTxAssignPID(IN P_ADAPTER_T prAdapter, IN UINT_8 ucWlanIndex);

WLAN_STATUS
nicTxDummyTxDone(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus);

VOID nicTxUpdateBssDefaultRate(IN P_BSS_INFO_T prBssInfo);

VOID nicTxUpdateStaRecDefaultRate(IN P_STA_RECORD_T prStaRec);

VOID
nicTxPrintMetRTP(IN P_ADAPTER_T prAdapter,
		 IN P_MSDU_INFO_T prMsduInfo, IN P_NATIVE_PACKET prPacket, IN UINT_32 u4PacketLen, IN BOOLEAN bFreeSkb);

VOID nicTxProcessTxDoneEvent(IN P_ADAPTER_T prAdapter, IN P_WIFI_EVENT_T prEvent);

VOID nicTxChangeDataPortByAc(P_STA_RECORD_T prStaRec, UINT_8 ucAci, BOOLEAN fgToMcu);

VOID nicTxHandleRoamingDone(P_ADAPTER_T prAdapter, P_STA_RECORD_T prOldStaRec, P_STA_RECORD_T prNewStaRec);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* _NIC_TX_H */
