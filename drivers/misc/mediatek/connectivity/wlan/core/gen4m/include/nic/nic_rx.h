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
/*! \file   "nic_rx.h"
 *    \brief  The declaration of the nic rx functions
 *
 */


#ifndef _NIC_RX_H
#define _NIC_RX_H

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

#define UNIFIED_MAC_RX_FORMAT               1

#define MAX_SEQ_NO                  4095
#define MAX_SEQ_NO_COUNT            4096
#define HALF_SEQ_NO_CNOUT           2048

#define HALF_SEQ_NO_COUNT           2048
#define QUARTER_SEQ_NO_COUNT        1024

#define MT6620_FIXED_WIN_SIZE         64
#define CFG_RX_MAX_BA_ENTRY            4
#define CFG_RX_MAX_BA_TID_NUM          8

#define RX_STATUS_FLAG_MORE_PACKET    BIT(30)
#define RX_STATUS_CHKSUM_MASK         BITS(0, 10)

#define RX_RFB_LEN_FIELD_LEN        4
#define RX_HEADER_OFFSET            2

#define RX_RETURN_INDICATED_RFB_TIMEOUT_SEC     3

#define RX_PROCESS_TIMEOUT           1000

#if defined(_HIF_SDIO) && defined(WINDOWS_DDK)
/*! On XP, maximum Tx+Rx Statue <= 64-4(HISR)*/
#define SDIO_MAXIMUM_RX_LEN_NUM              0	/*!< 0~15 (0: un-limited) */
#else
#define SDIO_MAXIMUM_RX_LEN_NUM              0	/*!< 0~15 (0: un-limited) */
#endif

/* RXM Definitions */
/* The payload format of a RX packet */
#define RX_PAYLOAD_FORMAT_MSDU                  0
#define RX_PAYLOAD_FORMAT_FIRST_SUB_AMSDU       3
#define RX_PAYLOAD_FORMAT_MIDDLE_SUB_AMSDU      2
#define RX_PAYLOAD_FORMAT_LAST_SUB_AMSDU        1

/* HAL RX from hal_hw_def_rom.h */
/*------------------------------------------------------------------------------
 * Cipher define
 *------------------------------------------------------------------------------
 */
#define CIPHER_SUITE_NONE               0
#define CIPHER_SUITE_WEP40              1
#define CIPHER_SUITE_TKIP               2
#define CIPHER_SUITE_TKIP_WO_MIC        3
#define CIPHER_SUITE_CCMP               4
#define CIPHER_SUITE_WEP104             5
#define CIPHER_SUITE_BIP                6
#define CIPHER_SUITE_WEP128             7
#define CIPHER_SUITE_WPI                8
#define CIPHER_SUITE_CCMP_W_CCX         9
#define CIPHER_SUITE_GCMP               10

/*------------------------------------------------------------------------------
 * Bit fields for HW_MAC_RX_DESC_T
 *------------------------------------------------------------------------------
 */

/*! MAC RX DMA Descriptor */
/* DW 0*/
/* Word 0 */
#define RX_STATUS_RX_BYTE_COUNT_MASK    BITS(0, 16)
/* Word 1 */
#define RX_STATUS_ETH_TYPE_OFFSET_MASK  BITS(0, 6)
#define RX_STATUS_ETH_TYPE_OFFSET       0
#define RX_STATUS_IP_CHKSUM             BIT(7)
#define RX_STATUS_UDP_TCP_CHKSUM        BIT(8)
#define RX_STATUS_GROUP_VLD_MASK        BITS(9, 12)
#define RX_STATUS_GROUP_VLD_OFFSET      9
#define RX_STATUS_PKT_TYPE_MASK         BITS(13, 15)
#define RX_STATUS_PKT_TYPE_OFFSET       13

/* DW 1 */
/* Byte 0 */
#define RX_STATUS_HTC                   BIT(0)
#define RX_STATUS_UC2ME                 BIT(1)
#define RX_STATUS_MC_FRAME              BIT(2)
#define RX_STATUS_BC_FRAME              BIT(3)
#define RX_STATUS_BCN_WITH_BMC          BIT(4)
#define RX_STATUS_BCN_WITH_UC           BIT(5)
#define RX_STATUS_KEYID_MASK            BITS(6, 7)
#define RX_STATUS_KEYID_OFFSET          6

/* Byte 1 */
#define RX_STATUS_CHAN_FREQ_MASK        BITS(0, 7)
/* Byte 2 */
#define RX_STATUS_HEADER_LEN_MASK       BITS(0, 5)
#define RX_STATUS_HEADER_OFFSET         BIT(6)
#define RX_STATUS_HEADER_TRAN           BIT(7)
/* Byte 3 */
#define RX_STATUS_PAYLOAD_FORMAT_MASK   BITS(0, 1)
#define RX_STATUS_PAYLOAD_FORMAT_OFFSET 0
#define RX_STATUS_BSSID_MASK            BITS(2, 7)
#define RX_STATUS_BSSID_OFFSET          2

/* DW 2 */
/* Byte 1 */
#define RX_STATUS_TID_MASK              BITS(0, 3)
#define RX_STATUS_SEC_MASK              BITS(4, 7)
#define RX_STATUS_SEC_OFFSET            4
/* Byte 2-3 */
#define RX_STATUS_SW_BIT                BIT(0)
#define RX_STATUS_FLAG_FCS_ERROR        BIT(1)
#define RX_STATUS_FLAG_CIPHER_MISMATCH  BIT(2)
#define RX_STATUS_FLAG_CIPHER_LENGTH_MISMATCH     BIT(3)
#define RX_STATUS_FLAG_ICV_ERROR        BIT(4)
#define RX_STATUS_FLAG_TKIPMIC_ERROR    BIT(5)
#define RX_STATUS_FLAG_LEN_MISMATCH     BIT(6)
#define RX_STATUS_FLAG_DE_AMSDU_FAIL    BIT(7)
#define RX_STATUS_FLAG_EXCEED_LEN       BIT(8)
#define RX_STATUS_LLC_MIS               BIT(9)
#define RX_STATUS_UDF_VLT               BIT(10)
#define RX_STATUS_FRAG                  BIT(11)
#define RX_STATUS_NULL                  BIT(12)
#define RX_STATUS_DATA                  BIT(13)
#define RX_STATUS_AMPDU_SUB_FRAME       BIT(14)
#define RX_STATUS_AMPDU_FORMAT          BIT(15)
#define PAYLOAD_FORMAT_IS_MSDU_FRAME    0
#define RX_STATUS_FLAG_ERROR_MASK \
	(RX_STATUS_FLAG_FCS_ERROR | RX_STATUS_FLAG_ICV_ERROR | \
	RX_STATUS_FLAG_CIPHER_LENGTH_MISMATCH)	/* No TKIP MIC error */

/* DW 3 */
#define RX_STATUS_RXV_SEQ_NO_MASK       BITS(0, 7)
#define RX_STATUS_TCL                   BIT(8)
#define RX_STATUS_CLS                   BIT(11)
#define RX_STATUS_OFLD_MASK             BITS(12, 13)
#define RX_STATUS_OFLD_OFFSET           12
#define RX_STATUS_EAPOL_PACKET          BIT(12)
#define RX_STATUS_ARP_NS_PACKET         BIT(13)
#define RX_STATUS_TDLS_PACKET           BITS(12, 13)
#define RX_STATUS_MGC                   BIT(14)
#define RX_STATUS_WOL_MASK              BITS(15, 19)
#define RX_STATUS_WOL_OFFSET            15
#define RX_STATUS_CLS_BITMAP_MASK       BITS(20, 29)
#define RX_STATUS_CLS_BITMAP_OFFSET     20
#define RX_STATUS_PF_MODE_BLACK_LIST    BIT(30)
#define RX_STATUS_PF_STS_CHECKED        BIT(31)

/* DW 12 */
#define RX_STATUS_FRAG_NUM_MASK         BITS(0, 3)
#define RX_STATUS_SEQ_NUM_MASK          BITS(4, 15)
#define RX_STATUS_SEQ_NUM_OFFSET        4

#define RX_STATUS_GROUP1_VALID    BIT(0)
#define RX_STATUS_GROUP2_VALID    BIT(1)
#define RX_STATUS_GROUP3_VALID    BIT(2)
#define RX_STATUS_GROUP4_VALID    BIT(3)

#define RX_STATUS_FIXED_LEN       16

#define RX_STATUS_CHAN_FREQ_MASK_FOR_BY_PASS_MPDE      BITS(0, 7)
#define RX_STATUS_FLAG_FCS_ERROR_FOR_BY_PASS_MODE      BIT(16)

/* Timing Measurement Report */
/* DW0 Word 1 */
#define RX_TMR_TOA_VALID          BIT(11)
#define RX_TMR_TOD_VALID          BIT(10)
#define RX_TMR_TYPE_MASK          BITS(8, 9)
#define RX_TMR_TYPE_OFFSET        8
#define RX_TMR_SUBTYPE_MASK       BITS(4, 7)
#define RX_TMR_SUBTYPE_OFFSET     4

/* DW0 Byte 1*/
#define RX_TMR_TM_FAILED          BIT(2)
#define RX_TMR_NOISY_CHAN         BIT(1)
#define RX_TMR_RESPONDER          BIT(0)

/* TBD */
#define DMA_OWN_TO_HW          BIT(0)
#define DMA_OWN_TO_FW_PENDING  BIT(1)
#define STATUS_IS_OWN_TO_FW(flag) \
	(((flag) & DMA_OWN_TO_HW) ? FALSE : TRUE)
#define STATUS_IS_FW_PENDING(flag) \
	(((flag) & DMA_OWN_TO_FW_PENDING) ? TRUE : FALSE)

/* DW 2 */
#define RX_STATUS_PACKET_LENGTH_MASK    BITS(0, 16)

#define RX_STATUS_HEADER_TRAN_MASK          BIT(7)
#define RX_STATUS_HEADER_TRAN_OFFSET        7
#define RX_STATUS_HEADER_TRAN_BSS0_MASK     BIT(6)
#define RX_STATUS_HEADER_TRAN_BSS0_OFFSET   6
#define RX_STATUS_HEADER_TRAN_BSS1_MASK     BIT(7)
#define RX_STATUS_HEADER_TRAN_BSS1_OFFSET   7

/* DW 4 */
#define RX_STATUS_MATCH_PACKET        BIT(4)

#define RX_STATUS_HEADER_OFFSET_MASK  0xC0
#define RX_STATUS_HEADER_OFFSET_OFFSET  6

/*------------------------------------------------------------------------------
 * Bit fields for HW_RX_VECTOR_DESC_T
 *------------------------------------------------------------------------------
 */
/* DW 2 */
#define RX_VECTOR_FOR_BA_ACK        BIT(7)

/*! HIF RX DMA Descriptor */
/* DW 2 */
#define HIF_RX_DESC_BUFFER_LEN                  BITS(0, 15)
#define HIF_RX_DESC_ETHER_TYPE_OFFSET_MASK      BITS(16, 23)
#define HIF_RX_DESC_ETHER_TYPE_OFFSET_OFFSET    16
#define HIF_RX_DESC_IP_CHKSUM_CHECK             BIT(24)
#define HIF_RX_DESC_TCP_UDP_CHKSUM_CHECK        BIT(25)

#define HIF_RX_DATA_QUEUE       0
#define HIF_RX_EVENT_QUEUE      1

/*------------------------------------------------------------------------------
 * Bit fields for PHY Vector
 *------------------------------------------------------------------------------
 */

/* RX Vector, 1st Cycle */
#define RX_VT_RX_RATE_AC_MASK      BITS(0, 3)
#define RX_VT_RX_RATE_MASK         BITS(0, 6)
#define RX_VT_RX_RATE_OFFSET       0
#define RX_VT_STBC_MASK            BITS(7, 8)
#define RX_VT_STBC_OFFSET          7
#define RX_VT_LDPC                 BIT(9)
#define RX_VT_NESS_MASK            BITS(10, 11)
#define RX_VT_NESS_OFFSET          10
#define RX_VT_RX_MODE_MASK         BITS(12, 14)
#define RX_VT_RX_MODE_OFFSET       12
#define RX_VT_RX_MODE_VHT          BIT(14)
#define RX_VT_FR_MODE_MASK         BITS(15, 16)
#define RX_VT_FR_MODE_OFFSET       15
#define RX_VT_TXOP_PS_NOT_ALLOWED  BIT(17)
#define RX_VT_AGGREGATION          BIT(18)
#define RX_VT_SHORT_GI             BIT(19)
#define RX_VT_SMOOTH               BIT(20)
#define RX_VT_NO_SOUNDING          BIT(21)
#define RX_VT_SOUNDING             BIT(21)
#if 0
/* VHT_SIG_A2[B1], not defined in MT6632 */
#define RX_VT_SHORT_GI_NSYM	   BIT(22)
/* VHT_SIG_A2[B2:B3], not defined in MT6632 */
#define RX_VT_CODING_MASK	   BITS(23, 24)
#define RX_VT_CODING_OFFSET	   23
#endif
#define RX_VT_NUM_RX_MASK          BITS(22, 23)
#define RX_VT_NUM_RX_OFFSET        22
#define RX_VT_LDPC_EXTRA_OFDM_SYM  BIT(24) /* VHT_SIG_A2[B3] */
#define RX_VT_SU_VHT_MU1_3_CODING  BITS(25, 28) /* VHT_SIG_A2[B4:B7] */
#define RX_VT_SU_VHT_MU1_3_CODING_OFFSET 25
#define RX_VT_BEAMFORMED	   BIT(29) /* VHT_SIG_A2[B8] */
#define RX_VT_ACID_DET_LOW         BIT(30)
#define RX_VT_ACID_DET_HIGH        BIT(31)

#define RX_VT_RX_RATE_1M      0x0
#define RX_VT_RX_RATE_2M      0x1
#define RX_VT_RX_RATE_5M      0x2
#define RX_VT_RX_RATE_11M     0x3
#define RX_VT_RX_RATE_6M      0xB
#define RX_VT_RX_RATE_9M      0xF
#define RX_VT_RX_RATE_12M     0xA
#define RX_VT_RX_RATE_18M     0xE
#define RX_VT_RX_RATE_24M     0x9
#define RX_VT_RX_RATE_36M     0xD
#define RX_VT_RX_RATE_48M     0x8
#define RX_VT_RX_RATE_54M     0xC

#define RX_VT_RX_RATE_MCS0    0
#define RX_VT_RX_RATE_MCS1    1
#define RX_VT_RX_RATE_MCS2    2
#define RX_VT_RX_RATE_MCS3    3
#define RX_VT_RX_RATE_MCS4    4
#define RX_VT_RX_RATE_MCS5    5
#define RX_VT_RX_RATE_MCS6    6
#define RX_VT_RX_RATE_MCS7    7
#define RX_VT_RX_RATE_MCS32   32

#define RX_VT_LEGACY_CCK      0
#define RX_VT_LEGACY_OFDM     1
#define RX_VT_MIXED_MODE      2
#define RX_VT_GREEN_MODE      3
#define RX_VT_VHT_MODE        4

#define RX_VT_LG20_HT20       0
#define RX_VT_DL40_HT40       1
#define RX_VT_U20             2
#define RX_VT_L20             3

#define RX_VT_FR_MODE_20      0
#define RX_VT_FR_MODE_40      1
#define RX_VT_FR_MODE_80      2
#define RX_VT_FR_MODE_160     3 /*BW160 or BW80+80*/

#define RX_VT_CCK_SHORT_PREAMBLE   BIT(2)

/* RX Vector, 2nd Cycle */
#define RX_VT_RX_LEN_HT_MASK       BITS(0, 15)
#define RX_VT_RX_LEN_LEACY_MASK    BITS(0, 11)
#define RX_VT_RX_LEN_VHT_MASK      BITS(0, 20)
#define RX_VT_GROUP_ID_MASK        BITS(21, 26)
#define RX_VT_GROUP_ID_OFFSET      21
#define RX_VT_GROUPID_0_MASK	   BITS(21, 22) /* VHT_SIG_A1[B4:B5] */
#define RX_VT_GROUPID_0_OFFSET	   21
#define RX_VT_GROUPID_1_MASK       BITS(23, 26) /* VHT_SIG_A1[B6:B9] */
#define RX_VT_GROUPID_1_OFFSET     23

#define RX_VT_NSTS_MASK            BITS(27, 29)
#define RX_VT_NSTS_OFFSET          27
#define RX_VT_RX_INDICATOR         BIT(30)
#define RX_VT_SEL_ANT              BIT(31) /* Not use in MT7615 and MT6632 */

/* RX Vector, 3rd Cycle */
#define RX_VT_PART_AID_MASK        BITS(3, 11)
#define RX_VT_PART_AID_OFFSET      3
#define RX_VT_AID_0_MASK           BITS(3, 6) /* VHT_SIG_A1[B13:B16] */
#define RX_VT_AID_0_OFFSET         3
#define RX_VT_AID_1_MASK           BITS(7, 11) /* VHT_SIG_A1[B17:B21] */
#define RX_VT_AID_1_OFFSET         7


#define RX_VT_NSTS_PART_AID_MASK     BITS(0, 11)
#define RX_VT_NSTS_PART_AID_OFFSET   0
#define RX_VT_POP_EVER_TRIG          BIT(12)
#define RX_VT_FAGC_LNA_RX_MASK       BITS(13, 15)
#define RX_VT_FAGC_LNA_RX_OFFSET     13
#define RX_VT_IB_RSSI_MASK           BITS(16, 23)
#define RX_VT_IB_RSSI_OFFSET         16
#define RX_VT_WB_RSSI_MASK           BITS(24, 31)
#define RX_VT_WB_RSSI_OFFSET         24

/* RX Vector, 4th Cycle */
#define RX_VT_RCPI0_MASK             BITS(0, 7)
#define RX_VT_RCPI0_OFFSET           0
#define RX_VT_RCPI1_MASK             BITS(8, 15)
#define RX_VT_RCPI1_OFFSET           8
#define RX_VT_RCPI2_MASK             BITS(16, 23)
#define RX_VT_RCPI2_OFFSET           16
#define RX_VT_RCPI3_MASK             BITS(24, 31)
#define RX_VT_RCPI3_OFFSET           24

/* RX Vector, 5th Cycle */
#define RX_VT_FAGC_LNA_GAIN_MASK     BITS(0, 2)
#define RX_VT_FAGC_LNA_GAIN_OFFSET   0
#define RX_VT_FAGC_LPF_GAIN_MASK     BITS(3, 6)
#define RX_VT_FAGC_LPF_GAIN_OFFSET   3
#define RX_VT_OFDM_FOE_MASK          BITS(7, 18)
#define RX_VT_OFDM_FOE_OFFSET        7
#define RX_VT_LTF_PROC_TIME_MASK     BITS(19, 25)
#define RX_VT_LTF_PROC_TIME_OFFSET   19
#define RX_VT_LTF_SNR_MASK           BITS(26, 31)
#define RX_VT_LTF_SNR_OFFSET         26

/*RX Vector, 6th Cycle*/
#define RX_VT_NF0_MASK               BITS(0, 7)
#define RX_VT_NF0_OFFSET             0
#define RX_VT_NF1_MASK               BITS(8, 15)
#define RX_VT_NF1_OFFSET             8
#define RX_VT_NF2_MASK               BITS(16, 23)
#define RX_VT_NF2_OFFSET             16
#define RX_VT_NF3_MASK               BITS(24, 31)
#define RX_VT_NF3_OFFSET             24

/* RX Vector Group 2, the 1st cycle */
#define RX_VT_PRIM_ITFR_ENV           BIT(0)
#define RX_VT_SEC_ITFR_ENV            BIT(1)
#define RX_VT_SEC40_ITFR_ENV          BIT(2)
#define RX_VT_SEC80_ITFR_ENV          BIT(3)
#define RX_VT_OFDM_LQ_BPSK_MASK       BITS(4, 10)
#define RX_VT_OFDM_LQ_BPSK_OFFSET     4
#define RX_VT_OFDM_CAPACITY_LQ_MASK   BITS(11, 17)
#define RX_VT_OFDM_CAPACITY_LQ_OFFSET 11
#define RX_VT_CCK_LQ_MASK             BITS(4, 13)
#define RX_VT_CCK_LQ_OFFSET           4

/* RX Vector Group 2, the 2nd cycle */
#define RX_VT_DYNA_BW_IN_NON_HT_DYNA   BIT(19)
#define RX_VT_CH_BW_IN_NON_HT_MASK     BITS(20, 21)
#define RX_VT_CH_BW_IN_NON_HT_OFFSET   20

#define RX_VT_CH_BW_IN_NON_HT_CBW40    BIT(20)
#define RX_VT_CH_BW_IN_NON_HT_CBW80    BIT(21)
#define RX_VT_CH_BW_IN_NON_HT_CBW160   BITS(20, 21)

/* RX Data Type */
#define RX_DATA_TYPE_RX_VECTOR  0
#define RX_DATA_TYPE_RX_DATA    1
#define RX_DATA_TYPE_RX_EVM     2
#define RX_DATA_TYPE_RX_AMBI    3
#define RX_DATA_TYPE_RX_BT      4

/*------------------------------------------------------------------------------
 * Radiotap define
 *------------------------------------------------------------------------------
 */

/*Radiotap VHT*/
#define RADIOTAP_VHT_ALL_KNOWN			BITS(0, 8)
#define RADIOTAP_VHT_STBC_KNOWN			BIT(0)
#define RADIOTAP_VHT_TXOP_PS_NOT_ALLOWED_KNOWN	BIT(1)
#define RADIOTAP_VHT_GI_KNOWN			BIT(2)
#define RADIOTAP_VHT_SHORT_GI_NSYM_KNOWN	BIT(3)
#define RADIOTAP_VHT_LDPC_EXTRA_OFDM_SYM_KNOWN	BIT(4)
#define RADIOTAP_VHT_BEAMFORMED_KNOWN		BIT(5)
#define RADIOTAP_VHT_BAND_WIDTH_KNOWN		BIT(6)
#define RADIOTAP_VHT_BAND_GROUP_ID_KNOWN	BIT(7)
#define RADIOTAP_VHT_BAND_PARTIAL_AID_KNOWN	BIT(8)

#if CFG_MTK_MCIF_WIFI_SUPPORT
#define NIC_RX_ROOM_SIZE (32 + 32)
#endif

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
enum ENUM_RX_STATISTIC_COUNTER {
	RX_MPDU_TOTAL_COUNT = 0,
	RX_SIZE_ERR_DROP_COUNT,

	RX_DATA_INDICATION_COUNT,
	RX_DATA_RETURNED_COUNT,
	RX_DATA_RETAINED_COUNT,

	RX_DATA_REORDER_TOTAL_COUNT,
	RX_DATA_REORDER_MISS_COUNT,
	RX_DATA_REORDER_WITHIN_COUNT,
	RX_DATA_REORDER_AHEAD_COUNT,
	RX_DATA_REORDER_BEHIND_COUNT,

	RX_DATA_MSDU_IN_AMSDU_COUNT,
	RX_DATA_AMSDU_MISS_COUNT,
	RX_DATA_AMSDU_COUNT,

	RX_DROP_TOTAL_COUNT,

	RX_NO_STA_DROP_COUNT,
	RX_INACTIVE_BSS_DROP_COUNT,
	RX_HS20_DROP_COUNT,
	RX_LESS_SW_RFB_DROP_COUNT,
	RX_DUPICATE_DROP_COUNT,
	RX_MIC_ERROR_DROP_COUNT,
	RX_BAR_DROP_COUNT,
	RX_NO_INTEREST_DROP_COUNT,
	RX_TYPE_ERR_DROP_COUNT,
	RX_CLASS_ERR_DROP_COUNT,
	RX_DST_NULL_DROP_COUNT,

#if CFG_TCP_IP_CHKSUM_OFFLOAD || CFG_TCP_IP_CHKSUM_OFFLOAD_NDIS_60
	RX_CSUM_TCP_FAILED_COUNT,
	RX_CSUM_UDP_FAILED_COUNT,
	RX_CSUM_IP_FAILED_COUNT,
	RX_CSUM_TCP_SUCCESS_COUNT,
	RX_CSUM_UDP_SUCCESS_COUNT,
	RX_CSUM_IP_SUCCESS_COUNT,
	RX_CSUM_UNKNOWN_L4_PKT_COUNT,
	RX_CSUM_UNKNOWN_L3_PKT_COUNT,
	RX_IP_V6_PKT_CCOUNT,
#endif
	RX_STATISTIC_COUNTER_NUM
};

enum ENUM_RX_PKT_DESTINATION {
	/* to OS */
	RX_PKT_DESTINATION_HOST,
	/* to TX queue for forward, AP mode */
	RX_PKT_DESTINATION_FORWARD,
	/* to both TX and OS, AP mode broadcast packet */
	RX_PKT_DESTINATION_HOST_WITH_FORWARD,
	/* packet to be freed */
	RX_PKT_DESTINATION_NULL,
	RX_PKT_DESTINATION_NUM
};

/* Used for MAC RX */
enum ENUM_MAC_RX_PKT_TYPE {
	RX_PKT_TYPE_TX_STATUS = 0,
	RX_PKT_TYPE_RX_VECTOR,
	RX_PKT_TYPE_RX_DATA,
	RX_PKT_TYPE_DUP_RFB,
	RX_PKT_TYPE_TM_REPORT,
	RX_PKT_TYPE_MSDU_REPORT = 6,
	RX_PKT_TYPE_SW_DEFINED = 7
};

enum ENUM_MAC_RX_GROUP_VLD {
	RX_GROUP_VLD_1 = 0,
	RX_GROUP_VLD_2,
	RX_GROUP_VLD_3,
	RX_GROUP_VLD_4,
#if (CFG_SUPPORT_CONNAC2X == 1)
	RX_GROUP_VLD_5,
#endif /* CFG_SUPPORT_CONNAC2X == 1 */
	RX_GROUP_VLD_NUM
};

enum ENUM_MAC_GI_INFO {
	MAC_GI_NORMAL = 0,
	MAC_GI_SHORT
};

enum ENUM_RXPI_MODE {
	RCPI_MODE_WF0 = 0,
	RCPI_MODE_WF1,
	RCPI_MODE_WF2,
	RCPI_MODE_WF3,
	RCPI_MODE_AVG,
	RCPI_MODE_MAX,
	RCPI_MODE_MIN,
	RCPI_MODE_NUM
};

#define RXM_RXD_PKT_TYPE_SW_BITMAP 0xE00F
#define RXM_RXD_PKT_TYPE_SW_EVENT  0xE000
#define RXM_RXD_PKT_TYPE_SW_FRAME  0xE001

/* AMPDU data frame with no errors including
 * FC/FM/I/T/LM/DAF/EL/LLC-MIS/ UDFVLT and Class 3 error
 */
#define RXS_DW2_AMPDU_nERR_BITMAP  0xFFFB /* ignore CM bit (2) 0xFFFF */
#define RXS_DW2_AMPDU_nERR_VALUE   0x0000
/* no error including FC/CLM/I/T/LM/DAF/EL/HTF */
#define RXS_DW2_RX_nERR_BITMAP     0x03FA /* ignore CM bit (2) 0x03FE */
#define RXS_DW2_RX_nERR_VALUE      0x0000
/* Non-Data frames */
#define RXS_DW2_RX_nDATA_BITMAP    0x3000
#define RXS_DW2_RX_nDATA_VALUE     0x2000
/* Claas Error */
#define RXS_DW2_RX_CLASSERR_BITMAP 0x0001
#define RXS_DW2_RX_CLASSERR_VALUE  0x0001
/* Fragmentation */
#define RXS_DW2_RX_FRAG_BITMAP     0x3800
#define RXS_DW2_RX_FRAG_VALUE      0x0800


/* TX Free Done Event Definition
 * Format version of this tx done event.
 *	0: MT7615
 *	1: MT7622, CONNAC (X18/P18/MT7663)
 *	2: MT7619_AXE, MT7915 E1, Petrus
  *	3: MT7915 E2, Buzzard
 */
enum {
	TFD_EVT_VER_0,
	TFD_EVT_VER_1,
	TFD_EVT_VER_2,
	TFD_EVT_VER_3,
};

#define RX_TFD_EVT_V3_PAIR_SHIFT 31
#define RX_TFD_EVT_V3_PAIR_MASK BIT(31)
#define RX_TFD_EVT_V3_QID_SHIFT 24
#define RX_TFD_EVT_V3_QID_MASK BITS(24, 30)
#define RX_TFD_EVT_V3_WLAN_ID_SHIFT 14
#define RX_TFD_EVT_V3_WLAN_ID_MASK BITS(14, 23)
#define RX_TFD_EVT_V3_MSDU_ID_SHIFT 16
#define RX_TFD_EVT_V3_MSDU_ID_MASK BITS(16, 30)

#define RX_TFD_EVT_V3_GET_PAIR_BIT(_rField) \
	(((_rField) & (RX_TFD_EVT_V3_PAIR_MASK)) >> (RX_TFD_EVT_V3_PAIR_SHIFT))
#define RX_TFD_EVT_V3_GET_MSDU_ID(_rField) \
	(((_rField) & (RX_TFD_EVT_V3_MSDU_ID_MASK)) \
		>> (RX_TFD_EVT_V3_MSDU_ID_SHIFT))


/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*! A data structure which is identical with MAC RX DMA Descriptor */
struct HW_MAC_RX_DESC {
	uint16_t u2RxByteCount;	/* DW 0 */
	uint16_t u2PktTYpe;
	uint8_t ucMatchPacket;	/* DW 1 */
	uint8_t ucChanFreq;
	uint8_t ucHeaderLen;
	uint8_t ucBssid;
	uint8_t ucWlanIdx;	/* DW 2 */
	uint8_t ucTidSecMode;
	uint16_t u2StatusFlag;
	uint32_t u4PatternFilterInfo;	/* DW 3 */
	uint32_t u4PatternFilterInfo2;  /* DW 4 */
};

struct HW_MAC_RX_STS_GROUP_1 {
	uint8_t aucPN[16];
};

struct HW_MAC_RX_STS_GROUP_2 {
	uint32_t u4Timestamp;	/* DW 12 */
	uint32_t u4CRC;		/* DW 13 */
};

struct HW_MAC_RX_STS_GROUP_4 {
	/* For HDR_TRAN */
	uint16_t u2FrameCtl;	/* DW 4 */
	uint8_t aucTA[6];	/* DW 4~5 */
	uint16_t u2SeqFrag;	/* DW 6 */
	uint16_t u2Qos;		/* DW 6 */
	uint32_t u4HTC;		/* DW 7 */
};

struct HW_MAC_RX_STS_GROUP_3 {
	/*!  RX Vector Info */
	uint32_t u4RxVector[6];	/* DW 14~19 */
};

#if (CFG_SUPPORT_CONNAC2X == 1)
struct HW_MAC_RX_STS_GROUP_3_V2 {
	/*  PRXVector Info */
	uint32_t u4RxVector[2];	/* FALCON: DW 16~17 */
};

struct HW_MAC_RX_STS_GROUP_5 {
	/*  CRXVector Info */
	/* FALCON: DW 18~33 for harrier E1,  DW 18~35 for harrier E2
	 * Other project: give group5_size in chip info,
	 * e.g Soc3_0.c
,
	 * or modify prChipInfo->group5_size when doing wlanCheckAsicCap,
	 * e.g. Harrier E1
	 */
	uint32_t u4RxVector[18];
};

struct HW_MAC_RX_STS_HARRIER_E1_GROUP_5 {
	/*  CRXVector Info */
	/* FALCON: DW 18~33 for harrier E1,  DW 18~35 for harrier E2 */
	uint32_t u4RxVector[16];
};


#define CONNAC2X_RSSI_MASK	BITS(0, 31)
#define CONNAC2X_SEL_ANT	BITS(28, 30)
#endif /* CFG_SUPPORT_CONNAC2X == 1 */

struct HW_MAC_RX_TMRI_PKT_FORMAT {
	uint8_t ucPID;
	uint8_t ucStatus;
	uint16_t u2PktTYpe;
	uint32_t u4Reserved[2];
	uint32_t u4ToA;
	uint32_t u4ToD;
};

struct HW_MAC_RX_TMRR_PKT_FORMAT {
	uint8_t ucVtSeq;
	uint8_t ucStatus;
	uint16_t u2PktTYpe;
	uint8_t aucTALow[2];
	uint16_t u2SnField;
	uint8_t aucTAHigh[4];
	uint32_t u4ToA;
	uint32_t u4ToD;
};

/*! A data structure which is identical with MAC RX Vector DMA Descriptor */
struct HW_RX_VECTOR_DESC {
	uint8_t aucTA[6];	/* DW 0~1 */
	uint8_t ucRxVtSeqNo;
	/*!  RX Vector Info */
	uint32_t u4RxVector[9];	/* DW 2~10 */

};

union HW_MAC_MSDU_TOKEN_T {
	struct {
		uint16_t         u2MsduID[2];
	} rFormatV0;
	struct {
		uint16_t         u2MsduID;
		uint16_t         u2WlanID:10;
		uint16_t         u2Rsv:1;
		uint16_t         u2QueIdx:5;
	} rFormatV1;
	struct {
		uint32_t         u2MsduID:15;
		uint32_t         u2WlanID:10;
		uint32_t         u2QueIdx:7;
	} rFormatV2;
	union {
		struct {
			uint32_t         u4TxCnt:13;
			uint32_t         u4Stat:2;
			uint32_t         u4H:1;
			uint32_t         u4MsduID:15;
			uint32_t         u4Pair:1;
		} rP0;
		struct {
			uint32_t         u4BW:2;
			uint32_t         u4Rate:4;
			uint32_t         u4GI:2;
			uint32_t         u4TxMode:2;
			uint32_t         u4Nsts:3;
			uint32_t         u4DCM:1;
			uint32_t         u4WlanID:10;
			uint32_t         u4QID:7;
			uint32_t         u4Pair:1;
		} rP1;
	} rFormatV3;
};

struct HW_MAC_MSDU_REPORT {
	/* DW 0 */
	union {
		struct {
			uint16_t         u2BufByteCount;
			uint16_t         u2MsduCount:7;
			uint16_t	 u2TxDoneInfo:9;
		} field;

		struct {
			uint16_t         u2BufByteCount;
			uint16_t         u2MsduCount:10;
			uint16_t	 u2Rsv:1;
			uint16_t	 u2PktType:5;
		} field_v3;

		uint32_t word;
	} DW0;

	/* DW 1 */
	union {
		struct {
	uint32_t         u4TxdCount:8;
	uint32_t         u4Rsv1:8;
	uint32_t         u4Ver:3;
	uint32_t         u4Rsv2:13;
		} field;

		uint32_t word;
	} DW1;

	/* DW 2 */
	/* MSDU token array */
	union HW_MAC_MSDU_TOKEN_T au4MsduToken[0];
};

struct SW_RFB {
	struct QUE_ENTRY rQueEntry;
	void *pvPacket;		/*!< ptr to rx Packet Descriptor */
	uint8_t *pucRecvBuff;	/*!< ptr to receive data buffer */

	/* add fot mt6630 */
	uint8_t ucGroupVLD;
	uint16_t u2RxStatusOffst;
	void *prRxStatus;
	struct HW_MAC_RX_STS_GROUP_1 *prRxStatusGroup1;
	struct HW_MAC_RX_STS_GROUP_2 *prRxStatusGroup2;
	void *prRxStatusGroup3;
	struct HW_MAC_RX_STS_GROUP_4 *prRxStatusGroup4;
#if (CFG_SUPPORT_CONNAC2X == 1)
	struct HW_MAC_RX_STS_GROUP_5 *prRxStatusGroup5;
#endif /* CFG_SUPPORT_CONNAC2X == 1 */

	/* rx data information */
	void *pvHeader;
	uint16_t u2RxByteCount;
	uint16_t u2PacketLen;
	uint16_t u2HeaderLen;

	uint8_t *pucPayload;
	uint16_t u2PayloadLength;

	struct STA_RECORD *prStaRec;

	uint8_t ucPacketType;
	uint8_t ucPayloadFormat;
	uint8_t ucSecMode;
	uint8_t ucOFLD;
	uint8_t ucKeyID;
	uint8_t ucChanFreq;
	uint8_t ucRxvSeqNo;
	uint8_t ucChnlNum;

	/* rx sta record */
	uint8_t ucWlanIdx;
	uint8_t ucStaRecIdx;

	u_int8_t fgReorderBuffer;
	u_int8_t fgDataFrame;
	u_int8_t fgFragFrame;
	u_int8_t fgHdrTran;
	u_int8_t fgIcvErr;
	u_int8_t fgIsBC;
	u_int8_t fgIsMC;
	u_int8_t fgIsCipherMS;
	u_int8_t fgIsCipherLenMS;
	u_int8_t fgIsFrag;
	u_int8_t fgIsFCS;
	u_int8_t fgIsAmpdu;
	/* duplicate detection */
	uint16_t u2FrameCtrl;
	uint16_t u2SequenceControl;
	uint16_t u2SSN;
	uint8_t ucTid;

#if CFG_TCP_IP_CHKSUM_OFFLOAD
	uint32_t u4TcpUdpIpCksStatus;
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

	enum ENUM_CSUM_RESULT aeCSUM[CSUM_TYPE_NUM];
	enum ENUM_RX_PKT_DESTINATION eDst;
	/* only valid when eDst == FORWARD */
	enum ENUM_TRAFFIC_CLASS_INDEX eTC;
#if 0
	/* RX reorder for one MSDU in AMSDU issue */
	/*QUE_T rAmsduQue;*/
#endif
	uint64_t rIntTime;
};

#if CFG_TCP_IP_CHKSUM_OFFLOAD
struct RX_CSO_REPORT_T {
	uint32_t u4IpV4CksStatus:1;
	uint32_t u4Reserved1:1;
	uint32_t u4TcpCksStatus:1;
	uint32_t u4UdpCksStatus:1;
	uint32_t u4IpV4CksType:1;
	uint32_t u4IpV6CksType:1;
	uint32_t u4TcpCksType:1;
	uint32_t u4UdpCksType:1;
	uint32_t u4IpDataLenMismatch:1;
	uint32_t u4IpFragPkg:1;
	uint32_t u4UnknowNextHeader:1;
	uint32_t u4Reserved2:21;
};
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

/*! RX configuration type structure */
struct RX_CTRL {
	uint32_t u4RxCachedSize;
	uint8_t *pucRxCached;
	struct QUE rFreeSwRfbList;
	struct QUE rReceivedRfbList;
	struct QUE rIndicatedRfbList;

#if CFG_SDIO_RX_AGG
	uint8_t *pucRxCoalescingBufPtr;
#endif

	void *apvIndPacket[CFG_RX_MAX_PKT_NUM];
	void *apvRetainedPacket[CFG_RX_MAX_PKT_NUM];

	uint8_t ucNumIndPacket;
	uint8_t ucNumRetainedPacket;
	/*!< RX Counters */
	uint64_t au8Statistics[RX_STATISTIC_COUNTER_NUM];

#if CFG_HIF_STATISTICS
	uint32_t u4TotalRxAccessNum;
	uint32_t u4TotalRxPacketNum;
#endif

#if CFG_HIF_RX_STARVATION_WARNING
	uint32_t u4QueuedCnt;
	uint32_t u4DequeuedCnt;
#endif

#if CFG_RX_PKTS_DUMP
	uint32_t u4RxPktsDumpTypeMask;
#endif

#if CFG_SUPPORT_SNIFFER
	uint32_t u4AmpduRefNum;
#endif

	/* Store SysTime of Last Rx */
	uint32_t u4LastRxTime[MAX_BSSID_NUM];
};

struct RX_MAILBOX {
	uint32_t u4RxMailbox[2];	/* for Device-to-Host Mailbox */
};

typedef uint32_t(*PROCESS_RX_MGT_FUNCTION) (struct ADAPTER *, struct SW_RFB *);

typedef void(*PROCESS_RX_EVENT_FUNCTION) (struct ADAPTER *,
	struct WIFI_EVENT *);

struct RX_EVENT_HANDLER {
	enum ENUM_EVENT_ID eEID;
	PROCESS_RX_EVENT_FUNCTION pfnHandler;
};

struct EMU_MAC_RATE_INFO {
	uint8_t ucPhyRateCode;
	uint32_t u4PhyRate[4][2];
};

struct RX_DESC_OPS_T {
	uint16_t (*nic_rxd_get_rx_byte_count)(
		void *prRxStatus);
	uint8_t (*nic_rxd_get_pkt_type)(
		void *prRxStatus);
	uint8_t (*nic_rxd_get_wlan_idx)(
		void *prRxStatus);
	uint8_t (*nic_rxd_get_sec_mode)(
		void *prRxStatus);
	uint8_t (*nic_rxd_get_sw_class_error_bit)(
		void *prRxStatus);
	uint8_t (*nic_rxd_get_ch_num)(
		void *prRxStatus);
	uint8_t (*nic_rxd_get_rf_band)(
		void *prRxStatus);
	uint8_t (*nic_rxd_get_tcl)(
		void *prRxStatus);
	uint8_t (*nic_rxd_get_ofld)(
		void *prRxStatus);
	void (*nic_rxd_fill_rfb)(
		struct ADAPTER *prAdapter,
		struct SW_RFB *prSwRfb);
	u_int8_t (*nic_rxd_sanity_check)(
		struct ADAPTER *prAdapter,
		struct SW_RFB *prSwRfb);
#if CFG_SUPPORT_WAKEUP_REASON_DEBUG
	void (*nic_rxd_check_wakeup_reason)(
		struct ADAPTER *prAdapter,
		struct SW_RFB *prSwRfb);
#endif /* CFG_SUPPORT_WAKEUP_REASON_DEBUG */
};

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */
#define NIC_RX_GET_U2_SW_PKT_TYPE(__rx_status) \
	(uint16_t)((*(uint32_t *)(__rx_status) & (BITS(16, 31))) >> (16))

#define RATE_INFO(_RateCode, _Bw20, _Bw20SGI, _Bw40, _BW40SGI, \
	_Bw80, _Bw80SGI, _Bw160, _Bw160SGI) \
	{ \
	.ucPhyRateCode                                    = (_RateCode), \
	.u4PhyRate[RX_VT_FR_MODE_20][MAC_GI_NORMAL]       = (_Bw20), \
	.u4PhyRate[RX_VT_FR_MODE_20][MAC_GI_SHORT]        = (_Bw20SGI), \
	.u4PhyRate[RX_VT_FR_MODE_40][MAC_GI_NORMAL]       = (_Bw40), \
	.u4PhyRate[RX_VT_FR_MODE_40][MAC_GI_SHORT]        = (_BW40SGI), \
	.u4PhyRate[RX_VT_FR_MODE_80][MAC_GI_NORMAL]       = (_Bw80), \
	.u4PhyRate[RX_VT_FR_MODE_80][MAC_GI_SHORT]        = (_Bw80SGI), \
	.u4PhyRate[RX_VT_FR_MODE_160][MAC_GI_NORMAL]      = (_Bw160), \
	.u4PhyRate[RX_VT_FR_MODE_160][MAC_GI_SHORT]       = (_Bw160SGI), \
	}

#define RX_INC_CNT(prRxCtrl, eCounter)              \
	{((struct RX_CTRL *)prRxCtrl)->au8Statistics[eCounter]++; }

#define RX_ADD_CNT(prRxCtrl, eCounter, u8Amount)    \
	{((struct RX_CTRL *)prRxCtrl)->au8Statistics[eCounter] += \
	(uint64_t)u8Amount; }

#define RX_GET_CNT(prRxCtrl, eCounter)              \
	(((struct RX_CTRL *)prRxCtrl)->au8Statistics[eCounter])

#define RX_RESET_ALL_CNTS(prRxCtrl)                 \
	{kalMemZero(&prRxCtrl->au8Statistics[0], \
	sizeof(prRxCtrl->au8Statistics)); }

#define RX_STATUS_TEST_MORE_FLAG(flag)	\
	((u_int8_t)((flag & RX_STATUS_FLAG_MORE_PACKET) ? TRUE : FALSE))

#define RX_STATUS_GET(__RxDescOps, __out_buf, __fn_name, __rx_status) { \
	if (__RxDescOps->nic_rxd_##__fn_name) \
		__out_buf = __RxDescOps->nic_rxd_##__fn_name(__rx_status); \
	else {\
		__out_buf = 0; \
		DBGLOG(RX, ERROR, "%s:: no hook api??\n", \
			__func__); \
	} \
} \
/*------------------------------------------------------------------------------
 * MACRO for HW_MAC_RX_DESC_T
 *------------------------------------------------------------------------------
 */
/* DW 0 */
#define HAL_RX_STATUS_GET_RX_BYTE_CNT(_prHwMacRxDesc) \
	((_prHwMacRxDesc)->u2RxByteCount)
#define HAL_RX_STATUS_GET_ETH_TYPE_OFFSET(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->u2PktTYpe & RX_STATUS_ETH_TYPE_OFFSET_MASK) >> \
	RX_STATUS_ETH_TYPE_OFFSET)
#define HAL_RX_STATUS_GET_GROUP_VLD(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->u2PktTYpe & RX_STATUS_GROUP_VLD_MASK) >> \
	RX_STATUS_GROUP_VLD_OFFSET)
#define HAL_RX_STATUS_GET_PKT_TYPE(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->u2PktTYpe & RX_STATUS_PKT_TYPE_MASK) \
	>> RX_STATUS_PKT_TYPE_OFFSET)

/* DW 1 */
#define HAL_RX_STATUS_IS_HTC_EXIST(_prHwMacRxDesc) \
	(((_prHwMacRxDesc)->ucMatchPacket & RX_STATUS_HTC)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_UC2ME(_prHwMacRxDesc) \
	(((_prHwMacRxDesc)->ucMatchPacket & RX_STATUS_UC2ME) \
	? TRUE : FALSE)
#define HAL_RX_STATUS_IS_MC(_prHwMacRxDesc) \
	(((_prHwMacRxDesc)->ucMatchPacket & RX_STATUS_MC_FRAME)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_BC(_prHwMacRxDesc) \
	(((_prHwMacRxDesc)->ucMatchPacket & RX_STATUS_BC_FRAME)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_BCN_WITH_BMC(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->ucMatchPacket & RX_STATUS_BCN_WITH_BMC)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_BCN_WITH_UC(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->ucMatchPacket & RX_STATUS_BCN_WITH_UC)?TRUE:FALSE)
#define HAL_RX_STATUS_GET_KEY_ID(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->ucMatchPacket & RX_STATUS_KEYID_MASK) >> \
	RX_STATUS_KEYID_OFFSET)
#define HAL_RX_STATUS_GET_CHAN_FREQ(_prHwMacRxDesc)	\
	((_prHwMacRxDesc)->ucChanFreq)
#define HAL_RX_STATUS_GET_HEADER_LEN(_prHwMacRxDesc) \
	((_prHwMacRxDesc)->ucHeaderLen & RX_STATUS_HEADER_LEN_MASK)
#define HAL_RX_STATUS_IS_HEADER_OFFSET(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->ucHeaderLen & RX_STATUS_HEADER_OFFSET)?TRUE:FALSE)
#define HAL_RX_STATUS_GET_HEADER_OFFSET(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->ucHeaderLen & RX_STATUS_HEADER_OFFSET) ? 2 : 0)
#define HAL_RX_STATUS_IS_HEADER_TRAN(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->ucHeaderLen & RX_STATUS_HEADER_TRAN)?TRUE:FALSE)
#define HAL_RX_STATUS_GET_HEADER_TRAN(_prHwMacRxDesc) \
	HAL_RX_STATUS_IS_HEADER_TRAN(_prHwMacRxDesc)
#define HAL_RX_STATUS_GET_PAYLOAD_FORMAT(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->ucBssid & RX_STATUS_PAYLOAD_FORMAT_MASK) >> \
	RX_STATUS_PAYLOAD_FORMAT_OFFSET)
#define HAL_RX_STATUS_GET_BSSID(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->ucBssid & RX_STATUS_BSSID_MASK) >> \
	RX_STATUS_BSSID_OFFSET)

/* DW 2 */
#define HAL_RX_STATUS_GET_WLAN_IDX(_prHwMacRxDesc) \
	((_prHwMacRxDesc)->ucWlanIdx)
#define HAL_RX_STATUS_GET_TID(_prHwMacRxDesc) \
	(((_prHwMacRxDesc)->ucTidSecMode & RX_STATUS_TID_MASK))
#define HAL_RX_STATUS_GET_SEC_MODE(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->ucTidSecMode & RX_STATUS_SEC_MASK) >> \
	RX_STATUS_SEC_OFFSET)
#define HAL_RX_STATUS_GET_SW_BIT(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_SW_BIT)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_FCS_ERROR(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_FLAG_FCS_ERROR)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_CIPHER_MISMATCH(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_FLAG_CIPHER_MISMATCH) \
	? TRUE : FALSE)
#define HAL_RX_STATUS_IS_CLM_ERROR(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->u2StatusFlag & \
	RX_STATUS_FLAG_CIPHER_LENGTH_MISMATCH) \
	? TRUE : FALSE)
#define HAL_RX_STATUS_IS_ICV_ERROR(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_FLAG_ICV_ERROR)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_TKIP_MIC_ERROR(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_FLAG_TKIPMIC_ERROR) > 0 \
	? TRUE : FALSE)
#define HAL_RX_STATUS_IS_ERROR(_prHwMacRxDesc)		\
	(((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_FLAG_ERROR_MASK) \
	? TRUE : FALSE)
#define HAL_RX_STATUS_IS_LEN_MISMATCH(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_FLAG_LEN_MISMATCH) \
	? TRUE : FALSE)
#define HAL_RX_STATUS_IS_DE_AMSDU_FAIL(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_FLAG_DE_AMSDU_FAIL) \
	? TRUE : FALSE)
#define HAL_RX_STATUS_IS_EXCEED_LEN(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_FLAG_EXCEED_LEN) \
	? TRUE : FALSE)
#define HAL_RX_STATUS_IS_LLC_MIS(_prHwMacRxDesc) \
	(((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_LLC_MIS)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_UDF_VLT(_prHwMacRxDesc) \
	(((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_UDF_VLT)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_FRAG(_prHwMacRxDesc) \
	(((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_FRAG)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_NULL(_prHwMacRxDesc) \
	(((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_NULL)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_DATA(_prHwMacRxDesc) \
	(((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_DATA)?FALSE:TRUE)
#define HAL_RX_STATUS_IS_AMPDU_SUB_FRAME(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_AMPDU_SUB_FRAME) \
	? FALSE : TRUE)
#define HAL_RX_STATUS_IS_AMPDU_FORMAT(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->u2StatusFlag & RX_STATUS_AMPDU_FORMAT)?FALSE:TRUE)

/* DW 3 */
#define HAL_RX_STATUS_IS_RV_VALID(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->u4PatternFilterInfo & RX_STATUS_RXV_SEQ_NO_MASK) \
	? TRUE : FALSE)
#define HAL_RX_STATUS_GET_RXV_SEQ_NO(_prHwMacRxDesc)	\
	((_prHwMacRxDesc)->u4PatternFilterInfo & RX_STATUS_RXV_SEQ_NO_MASK)
#define HAL_RX_STATUS_GET_TCL(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->u4PatternFilterInfo & RX_STATUS_TCL)?TRUE:FALSE)
#define HAL_RX_STATUS_IS_CLS(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->u4PatternFilterInfo & RX_STATUS_CLS)?TRUE:FALSE)
#define HAL_RX_STATUS_GET_OFLD(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->u4PatternFilterInfo & RX_STATUS_OFLD_MASK) >> \
	RX_STATUS_OFLD_OFFSET)
#define HAL_RX_STATUS_IS_MGC(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->u4PatternFilterInfo & RX_STATUS_MGC)?TRUE:FALSE)
#define HAL_RX_STATUS_GET_WOL(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->u4PatternFilterInfo & RX_STATUS_WOL_MASK) >> \
	RX_STATUS_WOL_OFFSET)
#define HAL_RX_STATUS_GET_CLS_BITMAP(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->u4PatternFilterInfo & RX_STATUS_CLS_BITMAP_MASK) \
	>> RX_STATUS_CLS_BITMAP_OFFSET)
#define HAL_RX_STATUS_IS_PF_BLACK_LIST(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->u4PatternFilterInfo \
	& RX_STATUS_PF_MODE_BLACK_LIST) ? TRUE : FALSE)
#define HAL_RX_STATUS_IS_PF_CHECKED(_prHwMacRxDesc)	\
	(((_prHwMacRxDesc)->u4PatternFilterInfo & RX_STATUS_PF_STS_CHECKED) \
	? TRUE : FALSE)

/* DW 4~7 */
#define HAL_RX_STATUS_GET_FRAME_CTL_FIELD(_prHwMacRxStsGroup4) \
	((_prHwMacRxStsGroup4)->u2FrameCtl)
#define HAL_RX_STATUS_GET_TA(_prHwMacRxStsGroup4, pucTA)   \
{\
	kalMemCopy(pucTA, &(_prHwMacRxStsGroup4)->aucTA[0], 6); \
}
#define HAL_RX_STATUS_GET_SEQ_FRAG_NUM(_prHwMacRxStsGroup4) \
	((_prHwMacRxStsGroup4)->u2SeqFrag)
#define HAL_RX_STATUS_GET_QOS_CTL_FIELD(_prHwMacRxStsGroup4) \
	((_prHwMacRxStsGroup4)->u2Qos)

#define HAL_RX_STATUS_GET_SEQFrag_NUM(_prHwMacRxStsGroup4) \
	((_prHwMacRxStsGroup4)->u2SeqFrag)
#define HAL_RX_STATUS_GET_HTC(_prHwMacRxStsGroup4) \
	((_prHwMacRxStsGroup4)->u4HTC)

/* DW 8~11 */
#define HAL_RX_STATUS_GET_RSC(_prHwMacRxStsGroup1, pucRSC)   \
{\
	kalMemCopy(pucRSC, &(_prHwMacRxStsGroup1)->aucPN[0], 6); \
}

#define HAL_RX_STATUS_GET_PN(_prHwMacRxStsGroup1, pucPN)   \
{\
	kalMemCopy(pucPN, &(_prHwMacRxStsGroup1)->aucPN[0], 16); \
}

/* DW 12~13 */
#define HAL_RX_STATUS_GET_TIMESTAMP(_prHwMacRxStsGroup2, _ucIdx) \
	((_prHwMacRxStsGroup2)->u4Timestamp)
#define HAL_RX_STATUS_GET_FCS32(_prHwMacRxStsGroup2) \
	((_prHwMacRxStsGroup2)->u4CRC)

/* DW 14~19 */
#define HAL_RX_STATUS_GET_RX_VECTOR(_prHwMacRxStsGroup3, _ucIdx) \
	((_prHwMacRxStsGroup3)->u4RxVector[_ucIdx])

#define HAL_RX_STATUS_GET_RX_NUM(_prHwMacRxStsGroup3)	\
	(((_prHwMacRxStsGroup3)->u4RxVector[0] & RX_VT_NUM_RX_MASK) >> \
	RX_VT_NUM_RX_OFFSET)

#if (CFG_SUPPORT_CONNAC2X == 1)
#define HAL_RX_STATUS_GET_RCPI0(_prHwMacRxStsGroup3)	\
	(((_prHwMacRxStsGroup3)->u4RxVector[1] & RX_VT_RCPI0_MASK) >> \
	RX_VT_RCPI0_OFFSET)
#define HAL_RX_STATUS_GET_RCPI1(_prHwMacRxStsGroup3)	\
	(((_prHwMacRxStsGroup3)->u4RxVector[1] & RX_VT_RCPI1_MASK) >> \
	RX_VT_RCPI1_OFFSET)
#define HAL_RX_STATUS_GET_RCPI2(_prHwMacRxStsGroup3)	\
	(((_prHwMacRxStsGroup3)->u4RxVector[1] & RX_VT_RCPI2_MASK) >> \
	RX_VT_RCPI0_OFFSET)
#define HAL_RX_STATUS_GET_RCPI3(_prHwMacRxStsGroup3)	\
	(((_prHwMacRxStsGroup3)->u4RxVector[1] & RX_VT_RCPI3_MASK) >> \
	RX_VT_RCPI1_OFFSET)
#else
#define HAL_RX_STATUS_GET_RCPI0(_prHwMacRxStsGroup3)	\
	(((_prHwMacRxStsGroup3)->u4RxVector[3] & RX_VT_RCPI0_MASK) >> \
	RX_VT_RCPI0_OFFSET)
#define HAL_RX_STATUS_GET_RCPI1(_prHwMacRxStsGroup3)	\
	(((_prHwMacRxStsGroup3)->u4RxVector[3] & RX_VT_RCPI1_MASK) >> \
	RX_VT_RCPI1_OFFSET)
#endif

/* TBD */
#define HAL_RX_STATUS_GET_RX_PACKET_LEN(_prHwMacRxDesc)
#define HAL_RX_STATUS_IS_MATCH_PACKET(_prHwMacRxDesc)

#define HAL_RX_STATUS_GET_CHNL_NUM(_prHwMacRxDesc) \
	((((_prHwMacRxDesc)->ucChanFreq) > HW_CHNL_NUM_MAX_4G_5G) ? \
	(((_prHwMacRxDesc)->ucChanFreq) - HW_CHNL_NUM_MAX_4G_5G) : \
	((_prHwMacRxDesc)->ucChanFreq))

/* To do: support more bands other than 2.4G and 5G */
#define HAL_RX_STATUS_GET_RF_BAND(_prHwMacRxDesc) \
	((((_prHwMacRxDesc)->ucChanFreq) <= HW_CHNL_NUM_MAX_2G4) ? \
	BAND_2G4 : BAND_5G)

/*------------------------------------------------------------------------------
 * MACRO for HW_RX_VECTOR_DESC_T
 *------------------------------------------------------------------------------
 */
#define HAL_RX_VECTOR_GET_TA(_prHwRxVector, pucTA)   \
{\
	kalMemCopy(pucTA, &(_prHwRxVector)->aucTA[0], 6); \
}

#define HAL_RX_VECTOR_GET_SEQ_NO(_prHwRxVector) \
	((_prHwRxVector)->ucRxVtSeqNo & RX_STATUS_RXV_SEQ_NO_MASK)
#define HAL_RX_VECTOR_IS_FOR_BA_ACK(_prHwRxVector) \
	(((_prHwRxVector)->ucRxVtSeqNo & RX_VECTOR_FOR_BA_ACK)?TRUE:FALSE)
#define HAL_RX_VECTOR_GET_RX_VECTOR(_prHwRxVector, _ucIdx) \
	((_prHwRxVector)->u4RxVector[_ucIdx])

#define RXM_IS_QOS_DATA_FRAME(_u2FrameCtrl) \
	(((_u2FrameCtrl & MASK_FRAME_TYPE) == MAC_FRAME_QOS_DATA) \
	? TRUE : FALSE)

#define RXM_IS_DATA_FRAME(_u2FrameCtrl) \
	(((_u2FrameCtrl & MASK_FC_TYPE) == MAC_FRAME_TYPE_DATA) ? TRUE : FALSE)

#define RXM_IS_CTRL_FRAME(_u2FrameCtrl) \
	(((_u2FrameCtrl & MASK_FC_TYPE) == MAC_FRAME_TYPE_CTRL) ? TRUE : FALSE)

#define RXM_IS_TRIGGER_FRAME(_u2FrameCtrl) \
	(((_u2FrameCtrl & MASK_FRAME_TYPE) == MAC_FRAME_HE_TRIGGER) ?\
		TRUE : FALSE)

#define RXM_IS_BLOCK_ACK_FRAME(_u2FrameCtrl) \
	(((_u2FrameCtrl & MASK_FRAME_TYPE) == MAC_FRAME_BLOCK_ACK) ?\
		TRUE : FALSE)

#define RXM_IS_MGMT_FRAME(_u2FrameCtrl) \
	(((_u2FrameCtrl & MASK_FC_TYPE) == MAC_FRAME_TYPE_MGT) ? TRUE : FALSE)

#define RXM_IS_PROTECTED_FRAME(_u2FrameCtrl) \
	((_u2FrameCtrl & MASK_FC_PROTECTED_FRAME) ? TRUE : FALSE)

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

void nicRxInitialize(IN struct ADAPTER *prAdapter);

void nicRxUninitialize(IN struct ADAPTER *prAdapter);

void nicRxProcessRFBs(IN struct ADAPTER *prAdapter);

void nicRxProcessMsduReport(IN struct ADAPTER *prAdapter,
	IN OUT struct SW_RFB *prSwRfb);

uint32_t nicRxSetupRFB(IN struct ADAPTER *prAdapter, IN struct SW_RFB *prRfb);

void nicRxReturnRFB(IN struct ADAPTER *prAdapter, IN struct SW_RFB *prRfb);

void nicProcessRxInterrupt(IN struct ADAPTER *prAdapter);

void nicRxProcessPktWithoutReorder(IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSwRfb);

u_int8_t nicRxCheckForwardPktResource(
	IN struct ADAPTER *prAdapter, uint32_t ucTid);

void nicRxProcessForwardPkt(IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSwRfb);

void nicRxProcessGOBroadcastPkt(IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSwRfb);

void nicRxFillRFB(IN struct ADAPTER *prAdapter,
	IN OUT struct SW_RFB *prSwRfb);

struct SW_RFB *nicRxDefragMPDU(IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSWRfb, OUT struct QUE *prReturnedQue);

u_int8_t nicRxIsDuplicateFrame(IN OUT struct SW_RFB *prSwRfb);

void nicRxProcessMonitorPacket(IN struct ADAPTER *prAdapter,
	IN OUT struct SW_RFB *prSwRfb);
#if CFG_SUPPORT_PERF_IND
void nicRxPerfIndProcessRXV(IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSwRfb,
	IN uint8_t ucBssIndex);
#endif

void nicRxProcessDataPacket(IN struct ADAPTER *prAdapter,
	IN OUT struct SW_RFB *prSwRfb);

void nicRxProcessEventPacket(IN struct ADAPTER *prAdapter,
	IN OUT struct SW_RFB *prSwRfb);

void nicRxProcessMgmtPacket(IN struct ADAPTER *prAdapter,
	IN OUT struct SW_RFB *prSwRfb);

#if CFG_TCP_IP_CHKSUM_OFFLOAD
void nicRxFillChksumStatus(IN struct ADAPTER *prAdapter,
	IN OUT struct SW_RFB *prSwRfb);

void nicRxUpdateCSUMStatistics(IN struct ADAPTER *prAdapter,
	IN const enum ENUM_CSUM_RESULT aeCSUM[]);
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

void nicRxQueryStatus(IN struct ADAPTER *prAdapter,
	IN uint8_t *pucBuffer, OUT uint32_t *pu4Count);

void nicRxClearStatistics(IN struct ADAPTER *prAdapter);

void nicRxQueryStatistics(IN struct ADAPTER *prAdapter,
	IN uint8_t *pucBuffer, OUT uint32_t *pu4Count);

uint32_t nicRxWaitResponse(IN struct ADAPTER *prAdapter,
	IN uint8_t ucPortIdx, OUT uint8_t *pucRspBuffer,
	IN uint32_t u4MaxRespBufferLen, OUT uint32_t *pu4Length);

void nicRxEnablePromiscuousMode(IN struct ADAPTER *prAdapter);

void nicRxDisablePromiscuousMode(IN struct ADAPTER *prAdapter);

uint32_t nicRxFlush(IN struct ADAPTER *prAdapter);

uint32_t nicRxProcessActionFrame(IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSwRfb);

uint8_t nicRxGetRcpiValueFromRxv(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucRcpiMode,
	IN struct SW_RFB *prSwRfb);

int32_t nicRxGetLastRxRssi(struct ADAPTER *prAdapter, IN char *pcCommand,
			IN int i4TotalLen, IN uint8_t ucWlanIdx);

#endif /* _NIC_RX_H */
