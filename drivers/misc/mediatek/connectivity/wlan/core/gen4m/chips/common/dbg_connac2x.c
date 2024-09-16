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
 *[File]             dbg_connac2x.c
 *[Version]          v1.0
 *[Revision Date]    2019-04-09
 *[Author]
 *[Description]
 *    The program provides WIFI FALCON MAC Debug APIs
 *[Copyright]
 *    Copyright (C) 2015 MediaTek Incorporation. All Rights Reserved.
 ******************************************************************************/

#if (CFG_SUPPORT_CONNAC2X == 1)
/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "precomp.h"
#include "mt_dmac.h"
#include "wf_ple.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#if defined(MT7915)
#define WTBL_VER	1
#elif defined(SOC3_0)
#define WTBL_VER	2
#else
#define WTBL_VER	3
#endif

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
static char *RATE_V2_HW_TX_MODE_STR[] = {
	"CCK", "OFDM", "MM", "GF", "VHT", "PLR",
	"N/A", "N/A", "HE_SU", "HE_ER", "HE_TRIG", "HE_MU"};

static char *RATE_TBLE[] = {"B", "G", "N", "N_2SS", "AC", "AC_2SS", "BG",
				"HE", "HE_2SS", "N/A"};

static char *RA_STATUS_TBLE[] = {"INVALID", "POWER_SAVING", "SLEEP", "STANDBY",
					"RUNNING", "N/A"};

#if 0
static char *LT_MODE_TBLE[] = {"RSSI", "LAST_RATE", "TRACKING", "N/A"};

static char *SGI_UNSP_STATE_TBLE[] = {"INITIAL", "PROBING", "SUCCESS",
					"FAILURE", "N/A"};

static char *BW_STATE_TBLE[] = {"UNCHANGED", "DOWN", "N/A"};
#endif

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

static void connac2x_dump_tmac_info(
	struct ADAPTER *prAdapter,
	uint8_t *tmac_info)
{
	static const char * const pkt_ft_str[] = {
		"cut_through",
		"store_forward",
		"cmd",
		"PDA_FW_Download"
	};

	static const char * const hdr_fmt_str[] = {
		"Non-80211-Frame",
		"Command-Frame",
		"Normal-80211-Frame",
		"enhanced-80211-Frame",
	};
	struct HW_MAC_CONNAC2X_TX_DESC *txd =
		(struct HW_MAC_CONNAC2X_TX_DESC *)tmac_info;

	DBGLOG(HAL, INFO, "TMAC_TXD Fields:\n");
	DBGLOG(HAL, INFO, "\tTMAC_TXD_0:\n");

	/* DW0 */
	/* TX Byte Count [15:0]  */
	DBGLOG(HAL, INFO, "\t\tTxByteCnt = %d\n",
		((txd->u4DW0 & CONNAC2X_TX_DESC_TX_BYTE_COUNT_MASK) >>
		CONNAC2X_TX_DESC_TX_BYTE_COUNT_OFFSET));

	/* PKT_FT: Packet Format [24:23] */
	DBGLOG(HAL, INFO, "\t\tpkt_ft = %d(%s)\n",
	((txd->u4DW0 & CONNAC2X_TX_DESC_PACKET_FORMAT_MASK) >>
		CONNAC2X_TX_DESC_PACKET_FORMAT_OFFSET),
	pkt_ft_str[((txd->u4DW0 & CONNAC2X_TX_DESC_PACKET_FORMAT_MASK) >>
		CONNAC2X_TX_DESC_PACKET_FORMAT_OFFSET)]);

	/* Q_IDX [31:25]  */
	DBGLOG(HAL, INFO, "\t\tQueID =0x%x\n",
		((txd->u4DW0 & CONNAC2X_TX_DESC_QUEUE_INDEX_MASK) >>
		CONNAC2X_TX_DESC_QUEUE_INDEX_OFFSET));

	DBGLOG(HAL, INFO, "\tTMAC_TXD_1:\n");
	/* DW1 */
	/* WLAN Indec [9:0] */
	DBGLOG(HAL, INFO, "\t\tWlan Index = %d\n",
		((txd->u4DW1 & CONNAC2X_TX_DESC_WLAN_INDEX_MASK) >>
		CONNAC2X_TX_DESC_WLAN_INDEX_OFFSET));

	/* HF: Header Format [17:16] */
	DBGLOG(HAL, INFO, "\t\tHdrFmt = %d(%s)\n",
	((txd->u4DW1 & CONNAC2X_TX_DESC_HEADER_FORMAT_MASK) >>
		CONNAC2X_TX_DESC_HEADER_FORMAT_OFFSET),
	hdr_fmt_str[((txd->u4DW1 & CONNAC2X_TX_DESC_HEADER_FORMAT_MASK) >>
		CONNAC2X_TX_DESC_HEADER_FORMAT_OFFSET)]);

	switch ((txd->u4DW1 & CONNAC2X_TX_DESC_HEADER_FORMAT_MASK) >>
		CONNAC2X_TX_DESC_HEADER_FORMAT_OFFSET) {
	case TMI_HDR_FT_NON_80211:
		/* MRD [11], EOSP [12], RMVL [13], VLAN [14], ETYPE [15] */
		DBGLOG(HAL, INFO,
		"\t\t\tMRD = %d, EOSP = %d, RMVL = %d, VLAN = %d, ETYP = %d\n",
		(txd->u4DW1 & CONNAC2X_TX_DESC_NON_802_11_MORE_DATA) ? 1 : 0,
		(txd->u4DW1 & CONNAC2X_TX_DESC_NON_802_11_EOSP) ? 1 : 0,
		(txd->u4DW1 & CONNAC2X_TX_DESC_NON_802_11_REMOVE_VLAN) ? 1 : 0,
		(txd->u4DW1 & CONNAC2X_TX_DESC_NON_802_11_VLAN_FIELD) ? 1 : 0,
		(txd->u4DW1 & CONNAC2X_TX_DESC_NON_802_11_ETHERNET_II) ? 1 : 0);
		break;

	case TMI_HDR_FT_NOR_80211:
		/* HEADER_LENGTH [15:11] */
		DBGLOG(HAL, INFO, "\t\t\tHeader Len = %d(WORD)\n",
		((txd->u4DW1 & CONNAC2X_TX_DESC_NOR_802_11_HEADER_LENGTH_MASK)
			>> CONNAC2X_TX_DESC_NOR_802_11_HEADER_LENGTH_OFFSET));
		break;

	case TMI_HDR_FT_ENH_80211:
		/* EOSP [12], AMS [13]	*/
		DBGLOG(HAL, INFO, "\t\t\tEOSP = %d, AMS = %d\n",
		(txd->u4DW1 & CONNAC2X_TX_DESC_ENH_802_11_EOSP) ? 1 : 0,
		(txd->u4DW1 & CONNAC2X_TX_DESC_ENH_802_11_AMSDU) ? 1 : 0);
		break;
	}

	/* Header Padding [19:18] */
	DBGLOG(HAL, INFO, "\t\tHdrPad Mode = %d\n",
		(txd->u4DW1 & CONNAC2X_TX_DESC_HEADER_PADDING_MODE) ? 1 : 0);
	DBGLOG(HAL, INFO, "\t\tHdrPad Len = %d\n",
		((txd->u4DW1 & CONNAC2X_TX_DESC_HEADER_PADDING_LENGTH_MASK) >>
		CONNAC2X_TX_DESC_HEADER_PADDING_LENGTH_OFFSET));

	/* TID [22:20] */
	DBGLOG(HAL, INFO, "\t\tTID = %d\n",
		((txd->u4DW1 & CONNAC2X_TX_DESC_TID_MASK) >>
		CONNAC2X_TX_DESC_TID_OFFSET));

	/* UtxB/AMSDU_C/AMSDU [23] */
	DBGLOG(HAL, INFO, "\t\tamsdu = %d\n",
		((txd->u4DW1 & CONNAC2X_TX_DESC_TXD_UTXB_AMSDU_MASK) ? 1 : 0));

	/* OM [29:24] */
	DBGLOG(HAL, INFO, "\t\town_mac = %d\n",
		((txd->u4DW1 & CONNAC2X_TX_DESC_OWN_MAC_MASK) >>
		CONNAC2X_TX_DESC_OWN_MAC_OFFSET));

	/* FT [31] */
	DBGLOG(HAL, INFO, "\t\tTxDFormatType = %d\n",
		(txd->u4DW1 & CONNAC2X_TX_DESC_FORMAT) ? 1 : 0);

	DBGLOG(HAL, INFO, "\tTMAC_TXD_2:\n");
	/* DW2 */
	/* Subtype [3:0] */
	DBGLOG(HAL, INFO, "\t\tsub_type = %d\n",
		((txd->u4DW2 & CONNAC2X_TX_DESC_SUB_TYPE_MASK) >>
		CONNAC2X_TX_DESC_SUB_TYPE_OFFSET));

	/* Type[5:4] */
	DBGLOG(HAL, INFO, "\t\tfrm_type = %d\n",
		((txd->u4DW2 & CONNAC2X_TX_DESC_TYPE_MASK) >>
		CONNAC2X_TX_DESC_TYPE_OFFSET));

	/* NDP [6] */
	DBGLOG(HAL, INFO, "\t\tNDP = %d\n",
		((txd->u4DW2 & CONNAC2X_TX_DESC_NDP) ? 1 : 0));

	/* NDPA [7] */
	DBGLOG(HAL, INFO, "\t\tNDPA = %d\n",
		((txd->u4DW2 & CONNAC2X_TX_DESC_NDPA) ? 1 : 0));

	/* SD [8] */
	DBGLOG(HAL, INFO, "\t\tSounding = %d\n",
		((txd->u4DW2 & CONNAC2X_TX_DESC_SOUNDING) ? 1 : 0));

	/* RTS [9] */
	DBGLOG(HAL, INFO, "\t\tRTS = %d\n",
		((txd->u4DW2 & CONNAC2X_TX_DESC_FORCE_RTS_CTS) ? 1 : 0));

	/* BM [10] */
	DBGLOG(HAL, INFO, "\t\tbc_mc_pkt = %d\n",
		((txd->u4DW2 & CONNAC2X_TX_DESC_BROADCAST_MULTICAST) ? 1 : 0));

	/* B [11]  */
	DBGLOG(HAL, INFO, "\t\tBIP = %d\n",
		((txd->u4DW2 & CONNAC2X_TX_DESC_BIP_PROTECTED) ? 1 : 0));

	/* DU [12] */
	DBGLOG(HAL, INFO, "\t\tDuration = %d\n",
	((txd->u4DW2 & CONNAC2X_TX_DESC_DURATION_FIELD_CONTROL) ? 1 : 0));

	/* HE [13] */
	DBGLOG(HAL, INFO, "\t\tHE(HTC Exist) = %d\n",
		((txd->u4DW2 & CONNAC2X_TX_DESC_HTC_EXISTS) ? 1 : 0));

	/* FRAG [15:14] */
	DBGLOG(HAL, INFO, "\t\tFRAG = %d\n",
		((txd->u4DW2 & CONNAC2X_TX_DESC_FRAGMENT_MASK) >>
		CONNAC2X_TX_DESC_FRAGMENT_OFFSET));

	/* Remaining Life Time [23:16]*/
	DBGLOG(HAL, INFO, "\t\tReamingLife/MaxTx time = %d\n",
		((txd->u4DW2 & CONNAC2X_TX_DESC_REMAINING_MAX_TX_TIME_MASK) >>
		CONNAC2X_TX_DESC_REMAINING_MAX_TX_TIME_OFFSET));

	/* Power Offset [29:24] */
	DBGLOG(HAL, INFO, "\t\tpwr_offset = %d\n",
		((txd->u4DW2 & CONNAC2X_TX_DESC_POWER_OFFSET_MASK) >>
		CONNAC2X_TX_DESC_POWER_OFFSET_OFFSET));

	/* FRM [30] */
	DBGLOG(HAL, INFO, "\t\tfix rate mode = %d\n",
		(txd->u4DW2 & CONNAC2X_TX_DESC_FIXED_RATE_MODE) ? 1 : 0);

	/* FR[31] */
	DBGLOG(HAL, INFO, "\t\tfix rate = %d\n",
		(txd->u4DW2 & CONNAC2X_TX_DESC_FIXED_RATE) ? 1 : 0);

	DBGLOG(HAL, INFO, "\tTMAC_TXD_3:\n");
	/* DW3 */
	/* NA [0] */
	DBGLOG(HAL, INFO, "\t\tNoAck = %d\n",
		(txd->u4DW3 & CONNAC2X_TX_DESC_NO_ACK) ? 1 : 0);

	/* PF [1] */
	DBGLOG(HAL, INFO, "\t\tPF = %d\n",
		(txd->u4DW3 & CONNAC2X_TX_DESC_PROTECTED_FRAME) ? 1 : 0);

	/* EMRD [2] */
	DBGLOG(HAL, INFO, "\t\tEMRD = %d\n",
		(txd->u4DW3 & CONNAC2X_TX_DESC_EXTEND_MORE_DATA) ? 1 : 0);

	/* EEOSP [3] */
	DBGLOG(HAL, INFO, "\t\tEEOSP = %d\n",
		(txd->u4DW3 & CONNAC2X_TX_DESC_EXTEND_EOSP) ? 1 : 0);

	/* DAS [4] */
	DBGLOG(HAL, INFO, "\t\tda_select = %d\n",
		(txd->u4DW3 & CONNAC2X_TX_DESC_DA_SOURCE) ? 1 : 0);

	/* TM [5] */
	DBGLOG(HAL, INFO, "\t\ttm = %d\n",
		(txd->u4DW3 & CONNAC2X_TX_DESC_TIMING_MEASUREMENT) ? 1 : 0);

	/* TX Count [10:6] */
	DBGLOG(HAL, INFO, "\t\ttx_cnt = %d\n",
		((txd->u4DW3 & CONNAC2X_TX_DESC_TX_COUNT_MASK) >>
		CONNAC2X_TX_DESC_TX_COUNT_OFFSET));

	/* Remaining TX Count [15:11] */
	DBGLOG(HAL, INFO, "\t\tremain_tx_cnt = %d\n",
		((txd->u4DW3 & CONNAC2X_TX_DESC_REMAINING_TX_COUNT_MASK) >>
		CONNAC2X_TX_DESC_REMAINING_TX_COUNT_OFFSET));

	/* SN [27:16] */
	DBGLOG(HAL, INFO, "\t\tsn = %d\n",
		((txd->u4DW3 & CONNAC2X_TX_DESC_SEQUENCE_NUMBER_MASK) >>
		CONNAC2X_TX_DESC_SEQUENCE_NUMBER_MASK_OFFSET));

	/* BA_DIS [28] */
	DBGLOG(HAL, INFO, "\t\tba dis = %d\n",
		(txd->u4DW3 & CONNAC2X_TX_DESC_BA_DISABLE) ? 1 : 0);

	/* Power Management [29] */
	DBGLOG(HAL, INFO, "\t\tpwr_mgmt = 0x%x\n",
	(txd->u4DW3 & CONNAC2X_TX_DESC_POWER_MANAGEMENT_CONTROL) ? 1 : 0);

	/* PN_VLD [30] */
	DBGLOG(HAL, INFO, "\t\tpn_vld = %d\n",
		(txd->u4DW3 & CONNAC2X_TX_DESC_PN_IS_VALID) ? 1 : 0);

	/* SN_VLD [31] */
	DBGLOG(HAL, INFO, "\t\tsn_vld = %d\n",
		(txd->u4DW3 & CONNAC2X_TX_DESC_SN_IS_VALID) ? 1 : 0);

	/* DW4 */
	DBGLOG(HAL, INFO, "\tTMAC_TXD_4:\n");

	/* PN_LOW [31:0] */
	DBGLOG(HAL, INFO, "\t\tpn_low = 0x%x\n", txd->u4PN1);

	/* DW5 */
	DBGLOG(HAL, INFO, "\tTMAC_TXD_5:\n");

	/* PN_HIGH [31:16]  */
	DBGLOG(HAL, INFO, "\t\tpn_high = 0x%x\n", txd->u2PN2);

	/* PID [7:0] */
	DBGLOG(HAL, INFO, "\t\tpid = %d\n",
		(txd->u2DW5_0 & CONNAC2X_TX_DESC_PACKET_ID_MASK) >>
			CONNAC2X_TX_DESC_PACKET_ID_OFFSET);

	/* TXSFM [8] */
	DBGLOG(HAL, INFO, "\t\ttx_status_fmt = %d\n",
		(txd->u2DW5_0 & CONNAC2X_TX_DESC_TX_STATUS_FORMAT) ? 1 : 0);

	/* TXS2M [9] */
	DBGLOG(HAL, INFO, "\t\ttx_status_2_mcu = %d\n",
		(txd->u2DW5_0 & CONNAC2X_TX_DESC_TX_STATUS_TO_MCU) ? 1 : 0);

	/* TXS2H [10] */
	DBGLOG(HAL, INFO, "\t\ttx_status_2_host = %d\n",
		(txd->u2DW5_0 & CONNAC2X_TX_DESC_TX_STATUS_TO_HOST) ? 1 : 0);

	/* DW6 */
	DBGLOG(HAL, INFO, "\tTMAC_TXD_6:\n");
	if (txd->u4DW2 & CONNAC2X_TX_DESC_FIXED_RATE) {
		/* Fixed BandWidth mode [2:0] */
		DBGLOG(HAL, INFO, "\t\tbw = %d\n",
			(txd->u4DW6 & CONNAC2X_TX_DESC_BANDWIDTH_MASK) >>
				CONNAC2X_TX_DESC_BANDWIDTH_OFFSET);

		/* DYN_BW [3] */
		DBGLOG(HAL, INFO, "\t\tdyn_bw = %d\n",
			(txd->u4DW6 & CONNAC2X_TX_DESC_DYNAMIC_BANDWIDTH)
				? 1 : 0);

		/* ANT_ID [7:4] */
		DBGLOG(HAL, INFO, "\t\tant_id = %d\n",
			(txd->u4DW6 & CONNAC2X_TX_DESC_ANTENNA_INDEX_MASK) >>
			CONNAC2X_TX_DESC_ANTENNA_INDEX_OFFSET);

		/* SPE_IDX_SEL [10] */
		DBGLOG(HAL, INFO, "\t\tspe idx sel = %d\n",
			(txd->u4DW6 & CONNAC2X_TX_DESC_SPE_IDX_SEL) ? 1 : 0);

		/* LDPC [11] */
		DBGLOG(HAL, INFO, "\t\tldpc = %d\n",
			(txd->u4DW6 & CONNAC2X_TX_DESC_LDPC) ? 1 : 0);

		/* HELTF Type[13:12] */
		DBGLOG(HAL, INFO, "\t\tHELTF Type = %d\n",
			(txd->u4DW6 & CONNAC2X_TX_DESC_HE_LTF_MASK) >>
				CONNAC2X_TX_DESC_HE_LTF_OFFSET);

		/* GI Type [15:14] */
		DBGLOG(HAL, INFO, "\t\tGI = %d\n",
			(txd->u4DW6 & CONNAC2X_TX_DESC_GI_TYPE) >>
				CONNAC2X_TX_DESC_GI_TYPE_OFFSET);

		/* Rate to be Fixed [29:16] */
		DBGLOG(HAL, INFO, "\t\ttx_rate = 0x%x\n",
			(txd->u4DW6 & CONNAC2X_TX_DESC_FIXDE_RATE_MASK) >>
				CONNAC2X_TX_DESC_FIXDE_RATE_OFFSET);
	}

	/* TXEBF [30] */
	DBGLOG(HAL, INFO, "\t\ttxebf = %d\n",
		(txd->u4DW6 & CONNAC2X_TX_DESC_TXE_BF)  ? 1 : 0);

	/* TXIBF [31] */
	DBGLOG(HAL, INFO, "\t\ttxibf = %d\n",
		(txd->u4DW6 & CONNAC2X_TX_DESC_TXI_BF) ? 1 : 0);

	/* DW7 */
	DBGLOG(HAL, INFO, "\tTMAC_TXD_7:\n");

	/* TXD Arrival Time [9:0] */
	DBGLOG(HAL, INFO, "\t\tarrival time = %d\n",
		txd->u4DW7 & CONNAC2X_TX_DESC_TXD_ARRIVAL_TIME_MASK);

	/* HW_AMSDU_CAP [10] */
	DBGLOG(HAL, INFO, "\t\thw amsdu cap = %d\n",
		(txd->u4DW7 & CONNAC2X_TX_DESC_HW_AMSDU) ? 1 : 0);

	/* SPE_IDX [15:11] */
	if (txd->u4DW2 & CONNAC2X_TX_DESC_FIXED_RATE)
		DBGLOG(HAL, INFO, "\t\tspe_idx = 0x%x\n",
			((txd->u4DW7 & CONNAC2X_TX_DESC_SPE_EXT_IDX_MASK) >>
			CONNAC2X_TX_DESC_SPE_EXT_IDX_OFFSET));

	/* PSE_FID [27:16], Indicate frame ID in PSE for this TXD */
	DBGLOG(HAL, INFO, "\t\tpse_fid = 0x%x\n",
		((txd->u4DW7 & CONNAC2X_TX_DESC_PSE_FID_MASK) >>
		CONNAC2X_TX_DESC_PSE_FID_OFFSET));

	/* Subtype [19:16], HW reserved, PP use only */
	DBGLOG(HAL, INFO, "\t\tpp_sub_type=%d\n",
		((txd->u4DW7 & CONNAC2X_TX_DESC7_SUB_TYPE_MASK) >>
		CONNAC2X_TX_DESC7_SUB_TYPE_OFFSET));

	/* Type [21:20], HW reserved, PP use only */
	DBGLOG(HAL, INFO, "\t\tpp_type=%d\n",
		((txd->u4DW7 & CONNAC2X_TX_DESC7_TYPE_MASK) >>
		CONNAC2X_TX_DESC7_TYPE_OFFSET));

	/* CTXD_CNT [25:23], overwritten with PSE_FID by PP */
	DBGLOG(HAL, INFO, "\t\tctxd cnt=0x%x\n",
		((txd->u4DW7 & CONNAC2X_TX_DESC_CTXD_CNT_MASK) >>
		CONNAC2X_TX_DESC_CTXD_CNT_OFFSET));

	/* CTXD [26], overwritten with PSE_FID by PP */
	DBGLOG(HAL, INFO, "\t\tctxd = %d\n",
		(txd->u4DW7 & CONNAC2X_TX_DESC_CTXD) ? 1 : 0);

	/* I [28]  */
	DBGLOG(HAL, INFO, "\t\ti = %d\n",
		(txd->u4DW7 & CONNAC2X_TX_DESC_IP_CHKSUM_OFFLOAD) ? 1 : 0);

	/* UT [29] */
	DBGLOG(HAL, INFO, "\t\tUT = %d\n",
		(txd->u4DW7 & CONNAC2X_TX_DESC_TCP_UDP_CHKSUM_OFFLOAD) ? 1 : 0);

	/* TXDLEN [31:30] */
	DBGLOG(HAL, INFO, "\t\ttxd len= %d\n",
		((txd->u4DW7 & CONNAC2X_TX_DESC_TXD_LENGTH_MASK) >>
		CONNAC2X_TX_DESC_TXD_LENGTH_OFFSET));
}

static void connac2x_event_dump_txd_mem(
	struct ADAPTER *prAdapter,
	struct CMD_INFO *prCmdInfo,
	uint8_t *pucEventBuf)

{
	struct EXT_CMD_EVENT_DUMP_MEM_T *prEventDumpMem;
	uint8_t data[DUMP_MEM_SIZE];
	uint32_t i = 0;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	kalMemZero(data, sizeof(data));
	prEventDumpMem = (struct EXT_CMD_EVENT_DUMP_MEM_T *)(pucEventBuf);
	kalMemCopy(data, prEventDumpMem->ucData, sizeof(data));
	for (i = 0; i < DUMP_MEM_SIZE; i = i + 4)
		DBGLOG(HAL, INFO, "DW%02d: 0x%02x%02x%02x%02x\n",
		i / 4,
		data[i + 3],
		data[i + 2],
		data[i + 1],
		data[i]
		);
	connac2x_dump_tmac_info(prAdapter, &data[0]);
}

void connac2x_show_txd_Info(
	struct ADAPTER *prAdapter,
	u_int32_t fid)
{
	struct EXT_CMD_EVENT_DUMP_MEM_T CmdMemDump;
	u_int32_t Addr = 0;
	u_int32_t rWlanStatus = WLAN_STATUS_SUCCESS;

	DBGLOG(HAL, INFO, "inShowTXDINFO fid=%d 0x%x\n", fid, fid);

	if (fid >= UMAC_FID_FAULT)
		return;

	Addr = 0xa << 28 | fid << 16; /* TXD addr: 0x{a}{fid}{0000}*/

	kalMemSet(&CmdMemDump, 0, sizeof(struct EXT_CMD_EVENT_DUMP_MEM_T));

	CmdMemDump.u4MemAddr = Addr;
	rWlanStatus = wlanSendSetQueryExtCmd(
		prAdapter,
		CMD_ID_LAYER_0_EXT_MAGIC_NUM, EXT_CMD_ID_DUMP_MEM,
		FALSE, /* Query Bit:  True->write  False->read*/
		TRUE,
		FALSE,
		connac2x_event_dump_txd_mem,
		nicOidCmdTimeoutCommon,
		sizeof(struct EXT_CMD_EVENT_DUMP_MEM_T),
		(u_int8_t *)(&CmdMemDump),
		NULL,
		0);
}

static u_int8_t connac2x_wtbl_get_sgi_info(
	struct fwtbl_lmac_struct *pWtbl)
{
	if (!pWtbl)
		return FALSE;

	switch (pWtbl->trx_cap.wtbl_d9.field.fcap) {
	case BW_20:
		return ((pWtbl->trx_cap.wtbl_d2.field.he) ?
			(pWtbl->trx_cap.wtbl_d7.field.g2_he) :
			(pWtbl->trx_cap.wtbl_d7.field.g2));
	case BW_40:
		return ((pWtbl->trx_cap.wtbl_d2.field.he) ?
			(pWtbl->trx_cap.wtbl_d7.field.g4_he) :
			(pWtbl->trx_cap.wtbl_d7.field.g4));
	case BW_80:
		return ((pWtbl->trx_cap.wtbl_d2.field.he) ?
			(pWtbl->trx_cap.wtbl_d7.field.g8_he) :
			(pWtbl->trx_cap.wtbl_d7.field.g8));
	case BW_160:
		return ((pWtbl->trx_cap.wtbl_d2.field.he) ?
			(pWtbl->trx_cap.wtbl_d7.field.g16_he) :
			(pWtbl->trx_cap.wtbl_d7.field.g16));
	default:
		return FALSE;
	}
}

static u_int8_t connac2x_wtbl_get_ldpc_info(
	struct fwtbl_lmac_struct *pWtbl)
{
	if (!pWtbl)
		return FALSE;

	if (pWtbl->trx_cap.wtbl_d2.field.he)
		return pWtbl->trx_cap.wtbl_d4.field.ldpc_he;
	else if (pWtbl->trx_cap.wtbl_d2.field.vht)
		return pWtbl->trx_cap.wtbl_d4.field.ldpc_vht;
	else
		return pWtbl->trx_cap.wtbl_d4.field.ldpc_ht;
}

static int32_t connac2x_wtbl_rate_to_string(
	char *pcCommand,
	int i4TotalLen,
	uint8_t TxRx,
	struct fwtbl_lmac_struct *pWtbl)
{
	uint8_t i, txmode, rate, stbc;
	uint8_t nss, gi;
	int32_t i4BytesWritten = 0;
	uint16_t arTxRate[8];

	arTxRate[0] = pWtbl->auto_rate_tb.wtbl_d10.field.rate1;
	arTxRate[1] = pWtbl->auto_rate_tb.wtbl_d10.field.rate2;
	arTxRate[2] = pWtbl->auto_rate_tb.wtbl_d11.field.rate3;
	arTxRate[3] = pWtbl->auto_rate_tb.wtbl_d11.field.rate4;
	arTxRate[4] = pWtbl->auto_rate_tb.wtbl_d12.field.rate5;
	arTxRate[5] = pWtbl->auto_rate_tb.wtbl_d12.field.rate6;
	arTxRate[6] = pWtbl->auto_rate_tb.wtbl_d13.field.rate7;
	arTxRate[7] = pWtbl->auto_rate_tb.wtbl_d13.field.rate8;
	for (i = 0; i < AUTO_RATE_NUM; i++) {

		txmode = CONNAC2X_HW_TX_RATE_TO_MODE(arTxRate[i]);
		if (txmode >= ENUM_TX_MODE_NUM)
			txmode = ENUM_TX_MODE_NUM - 1;
		rate = HW_TX_RATE_TO_MCS(arTxRate[i]);
		nss = CONNAC2X_HW_TX_RATE_TO_NSS(arTxRate[i]) + 1;
		stbc = CONNAC2X_HW_TX_RATE_TO_STBC(arTxRate[i]);
		gi = connac2x_wtbl_get_sgi_info(pWtbl);

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
					      i4TotalLen - i4BytesWritten,
					      "\tRateIdx[%d] ", i);

		if (pWtbl->trx_cap.wtbl_d9.field.rate_idx == i) {
			if (TxRx == 0)
				i4BytesWritten += kalSnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%s", "[Last RX Rate] ");
			else
				i4BytesWritten += kalSnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%s", "[Last TX Rate] ");
		} else
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s", "               ");

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten, "%s, ",
				rate < 4 ? HW_TX_RATE_CCK_STR[rate] :
					   HW_TX_RATE_CCK_STR[4]);
		else if (txmode == TX_RATE_MODE_OFDM)
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten, "%s, ",
				nicHwRateOfdmStr(rate));
		else if ((txmode == TX_RATE_MODE_HTMIX) ||
			 (txmode == TX_RATE_MODE_HTGF))
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"MCS%d, ", rate);
		else {
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s%d_MCS%d, ", stbc ? "NSTS" : "NSS",
				nss, rate);
		}

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s, ",
			(pWtbl->trx_cap.wtbl_d9.field.fcap < 4) ?
			HW_TX_RATE_BW[pWtbl->trx_cap.wtbl_d9.field.fcap] :
			HW_TX_RATE_BW[4]);

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten, "%s, ",
				rate < 4 ? "LP" : "SP");
		else if (txmode == TX_RATE_MODE_OFDM)
			;
		else if (txmode >= TX_RATE_MODE_HTMIX
			&& txmode <= TX_RATE_MODE_PLR)
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten, "%s, ",
					gi == 0 ? "LGI" : "SGI");
			else
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%s, ", gi == 0 ? "SGI" :
					(gi == 1 ? "MGI" : "LGI"));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "%s%s %s\n",
		RATE_V2_HW_TX_MODE_STR[txmode], stbc ? "STBC" : " ",
		connac2x_wtbl_get_ldpc_info(pWtbl) == 0 ? "BCC" : "LDPC");
	}

	return i4BytesWritten;
}

static int32_t connac2x_dump_helper_wtbl_info(
	struct ADAPTER *prAdapter,
	char *pcCommand,
	int i4TotalLen,
	struct fwtbl_lmac_struct *pWtbl,
	uint32_t u4Index)
{
	int32_t i4BytesWritten = 0;
	uint8_t aucPA[MAC_ADDR_LEN];
	uint8_t u1BeamChgDw5 = 0, u1Dw3NewContent = 1;
	uint8_t u1Dw5SR_R_Old = 0, u1Dw9NewContent = 1;
	uint8_t u1Dw30Rssi = 1;

	if (!pcCommand) {
		DBGLOG(HAL, ERROR, "%s: pcCommand is NULL.\n",
			__func__);
		return i4BytesWritten;
	}

	if ((WTBL_VER == 1) && (wlanGetEcoVersion(prAdapter) < ECO_VER_2)) {
		u1BeamChgDw5 = 1;
		u1Dw3NewContent = 0;
		u1Dw5SR_R_Old = 1;
		u1Dw9NewContent = 0;
		u1Dw30Rssi = 0;
	} else if ((WTBL_VER == 2) &&
		(wlanGetEcoVersion(prAdapter) < ECO_VER_2)) {
		u1Dw3NewContent = 0;
		u1Dw30Rssi = 0;
	}

	aucPA[0] =
		pWtbl->peer_basic_info.wtbl_d1.field.addr_0 & 0xff;
	aucPA[1] =
		((pWtbl->peer_basic_info.wtbl_d1.field.addr_0 &
			0xff00) >> 8);
	aucPA[2] =
		((pWtbl->peer_basic_info.wtbl_d1.field.addr_0 &
			0xff0000) >> 16);
	aucPA[3] =
		((pWtbl->peer_basic_info.wtbl_d1.field.addr_0 &
			0xff000000) >> 24);
	aucPA[4] =
		pWtbl->peer_basic_info.wtbl_d0.field.addr_4 & 0xff;
	aucPA[5] =
		pWtbl->peer_basic_info.wtbl_d0.field.addr_5 & 0xff;

	i4BytesWritten = kalSnprintf(pcCommand, i4TotalLen, "%s",
		"\n\nwtbl:\n");
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"Dump WTBL info of WLAN_IDX	    = %d\n",
		u4Index);
	/* DW0~DW1 */
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "\tADDR="MACSTR"\n",
		MAC2STR(aucPA));
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tKID/RCID/RV/WPI:%d/%d/%d/%d\n",
		pWtbl->peer_basic_info.wtbl_d0.field.muar_idx,
		pWtbl->peer_basic_info.wtbl_d0.field.fd,
		pWtbl->peer_basic_info.wtbl_d0.field.td,
		pWtbl->peer_basic_info.wtbl_d0.field.rc_a1,
		pWtbl->peer_basic_info.wtbl_d0.field.rc_a2);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tKID:%d/RCID:%d/RKV:NA/RV:%d/IKV:NA/WPI_FLAG:%d\n",
		pWtbl->peer_basic_info.wtbl_d0.field.kid,
		pWtbl->peer_basic_info.wtbl_d0.field.rc_id,
		pWtbl->peer_basic_info.wtbl_d0.field.rv,
		pWtbl->peer_basic_info.wtbl_d0.field.wpi_flg);
	/* DW2 */
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tAID12/GID_SU/SPP_EN/WPI_EVEN/AAD_OM:%d/%d/%d/%d/%d\n",
		pWtbl->trx_cap.wtbl_d2.field.aid12,
		pWtbl->trx_cap.wtbl_d2.field.gid_su,
		pWtbl->trx_cap.wtbl_d2.field.spp_en,
		pWtbl->trx_cap.wtbl_d2.field.wpi_even,
		pWtbl->trx_cap.wtbl_d2.field.aad_om);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tCipher/IGTK:%d/%d\n",
		pWtbl->trx_cap.wtbl_d2.field.cipher_suit,
		pWtbl->trx_cap.wtbl_d2.field.cipher_suit_igtk);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tSW/UL/TXPS/QoS/MESH:%d/%d/%d/%d/%d\n",
		pWtbl->trx_cap.wtbl_d2.field.sw,
		pWtbl->trx_cap.wtbl_d2.field.ul,
		pWtbl->trx_cap.wtbl_d2.field.tx_ps,
		pWtbl->trx_cap.wtbl_d2.field.qos,
		pWtbl->trx_cap.wtbl_d2.field.mesh);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tHT/VHT/HE/LDPC[HT/VHT/HE]:%d/%d/%d/%d/%d/%d\n",
		pWtbl->trx_cap.wtbl_d2.field.ht,
		pWtbl->trx_cap.wtbl_d2.field.vht,
		pWtbl->trx_cap.wtbl_d2.field.he,
		pWtbl->trx_cap.wtbl_d4.field.ldpc_ht,
		pWtbl->trx_cap.wtbl_d4.field.ldpc_vht,
			pWtbl->trx_cap.wtbl_d4.field.ldpc_he);
	/* DW3 */
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tWMMQ/RXD_DUP_M/VLAN2ETH/BEAM_CHG:%d/%d/%d/%d\n",
		pWtbl->trx_cap.wtbl_d3.field.wmm_q,
		pWtbl->trx_cap.wtbl_d3.field.rxd_dup_mode,
		pWtbl->trx_cap.wtbl_d3.field.vlan_2e_th,
		(u1BeamChgDw5 > 0) ? (pWtbl->trx_cap.wtbl_d5.field.beam_chg) :
		(pWtbl->trx_cap.wtbl_d3.field_v2.beam_chg));
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tPFMU_IDX/RIBF/TBF[HT/VHT/HE]:%d/%d/%d/%d/%d\n",
		pWtbl->trx_cap.wtbl_d3.field.pfmu_index,
		pWtbl->trx_cap.wtbl_d3.field.ribf,
		pWtbl->trx_cap.wtbl_d3.field.tebf,
		pWtbl->trx_cap.wtbl_d3.field.tebf_vht,
		pWtbl->trx_cap.wtbl_d3.field.tebf_he);

	if (u1Dw3NewContent > 0)
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tBA_M/ULPF_IDX/ULPF/IGN_FBK:%d/%d/%d/%d\n",
			pWtbl->trx_cap.wtbl_d3.field_v2.ba_mode,
			pWtbl->trx_cap.wtbl_d3.field_v2.ulpf_index,
			pWtbl->trx_cap.wtbl_d3.field_v2.ulpf,
			pWtbl->trx_cap.wtbl_d3.field_v2.ign_fbk);
	/* DW4 */
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tANT_ID0/1/2/3/4/5/6/7:%d/%d/%d/%d/%d/%d/%d/%d\n",
		pWtbl->trx_cap.wtbl_d4.field.ant_id_sts0,
		pWtbl->trx_cap.wtbl_d4.field.ant_id_sts1,
		pWtbl->trx_cap.wtbl_d4.field.ant_id_sts2,
		pWtbl->trx_cap.wtbl_d4.field.ant_id_sts3,
		pWtbl->trx_cap.wtbl_d4.field.ant_id_sts4,
		pWtbl->trx_cap.wtbl_d4.field.ant_id_sts5,
		pWtbl->trx_cap.wtbl_d4.field.ant_id_sts6,
		pWtbl->trx_cap.wtbl_d4.field.ant_id_sts7);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tCASCAD/DIS_RHTR/ALL_ACK/DROP/ACK_EN:%d/%d/%d/%d/%d\n",
		pWtbl->trx_cap.wtbl_d4.field.cascad,
		pWtbl->trx_cap.wtbl_d4.field.dis_rhtr,
		pWtbl->trx_cap.wtbl_d4.field.all_ack,
		pWtbl->trx_cap.wtbl_d4.field.drop,
		pWtbl->trx_cap.wtbl_d4.field.ack_en);
	/* DW5 */
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tAF/AFHE/RTS/SMPS/DYNBW/MMSS:%d/%d/%d/%d/%d/%d\n",
		pWtbl->trx_cap.wtbl_d5.field.af,
		pWtbl->trx_cap.wtbl_d5.field.af_he,
		pWtbl->trx_cap.wtbl_d5.field.rts,
		pWtbl->trx_cap.wtbl_d5.field.smps,
		pWtbl->trx_cap.wtbl_d5.field.dyn_bw,
		pWtbl->trx_cap.wtbl_d5.field.mmss);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tUSR/SR_R/SR_A/MPDU_SZ/PE:%d/%d/%d/%d/%d\n",
		pWtbl->trx_cap.wtbl_d5.field.usr,
		(u1Dw5SR_R_Old > 0) ? (pWtbl->trx_cap.wtbl_d5.field.sr_r) :
		(pWtbl->trx_cap.wtbl_d5.field_v2.sr_r),
		pWtbl->trx_cap.wtbl_d5.field.sr_abort,
		pWtbl->trx_cap.wtbl_d5.field.mpdu_size,
		pWtbl->trx_cap.wtbl_d5.field.pe);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tTXPWR_OFST/DOPPL/TXOP_PS_CAP:%d/%d/%d\n",
		pWtbl->trx_cap.wtbl_d5.field.tx_power_offset,
		pWtbl->trx_cap.wtbl_d5.field.doppl,
		pWtbl->trx_cap.wtbl_d5.field.txop_ps_cap);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tDU_I_PSM/I_PSM/PSM/SKIP_TX:%d/%d/%d/%d\n",
		pWtbl->trx_cap.wtbl_d5.field.du_i_psm,
		pWtbl->trx_cap.wtbl_d5.field.i_psm,
		pWtbl->trx_cap.wtbl_d5.field.psm,
		pWtbl->trx_cap.wtbl_d5.field.skip_tx);
	/* DW6 */
	if (pWtbl->trx_cap.wtbl_d2.field.qos)
		i4BytesWritten += kalSnprintf(
			pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tBaWinSize TID0/1/2/3/4/5/6/7:%d/%d/%d/%d/%d/%d/%d/%d\n",
			(uint32_t)
			(pWtbl->trx_cap.wtbl_d6.field.ba_win_size_tid0),
			(uint32_t)
			(pWtbl->trx_cap.wtbl_d6.field.ba_win_size_tid1),
			(uint32_t)
			(pWtbl->trx_cap.wtbl_d6.field.ba_win_size_tid2),
			(uint32_t)
			(pWtbl->trx_cap.wtbl_d6.field.ba_win_size_tid3),
			(uint32_t)
			(pWtbl->trx_cap.wtbl_d6.field.ba_win_size_tid4),
			(uint32_t)
			(pWtbl->trx_cap.wtbl_d6.field.ba_win_size_tid5),
			(uint32_t)
			(pWtbl->trx_cap.wtbl_d6.field.ba_win_size_tid6),
			(uint32_t)
			(pWtbl->trx_cap.wtbl_d6.field.ba_win_size_tid7));
	/* DW7 */
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tCBRN/DBNSS_EN/BAFEN/RDGBA/R:%d/%d/%d/%d/%d\n",
		pWtbl->trx_cap.wtbl_d7.field.cb_rn,
		pWtbl->trx_cap.wtbl_d7.field.dbnss_en,
		pWtbl->trx_cap.wtbl_d7.field.bafen,
		pWtbl->trx_cap.wtbl_d7.field.rdg_ba,
		pWtbl->trx_cap.wtbl_d7.field.r);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tG2/G4/G8/G16/SPE:%d/%d/%d/%d/%d\n",
		pWtbl->trx_cap.wtbl_d7.field.g2,
		pWtbl->trx_cap.wtbl_d7.field.g4,
		pWtbl->trx_cap.wtbl_d7.field.g8,
		pWtbl->trx_cap.wtbl_d7.field.g16,
		pWtbl->trx_cap.wtbl_d7.field.spe_idx);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tHE[G2/G4/G8/G16]:%d/%d/%d/%d\n",
		pWtbl->trx_cap.wtbl_d7.field.g2_he,
		pWtbl->trx_cap.wtbl_d7.field.g4_he,
		pWtbl->trx_cap.wtbl_d7.field.g8_he,
		pWtbl->trx_cap.wtbl_d7.field.g16_he);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tLTF[G2/G4/G8/G16]:%d/%d/%d/%d\n",
		pWtbl->trx_cap.wtbl_d7.field.g2_ltf,
		pWtbl->trx_cap.wtbl_d7.field.g4_ltf,
		pWtbl->trx_cap.wtbl_d7.field.g8_ltf,
		pWtbl->trx_cap.wtbl_d7.field.g16_ltf);
	/* DW8 */
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tCHK_PER/P_AID:%d/%d\n",
		pWtbl->trx_cap.wtbl_d8.field.chk_per,
		pWtbl->trx_cap.wtbl_d8.field.partial_aid);
	/* DW9 */
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tFCAP/PRITX[DCM/ER160/ERSU]:%d/%d/%d/%d\n",
		pWtbl->trx_cap.wtbl_d9.field.fcap,
		pWtbl->trx_cap.wtbl_d9.field.pritx_dcm,
		pWtbl->trx_cap.wtbl_d9.field.pritx_er160,
		pWtbl->trx_cap.wtbl_d9.field.pritx_ersu);

	if (u1Dw9NewContent > 0)
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tPRITX[SW_M/PLR]:%d/%d\n",
			pWtbl->trx_cap.wtbl_d9.field_v2.pritx_sw_mode,
			pWtbl->trx_cap.wtbl_d9.field_v2.pritx_plr);

	if (u1Dw30Rssi > 0)
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tRSSI = %d %d %d %d\n",
		RCPI_TO_dBm(pWtbl->rx_stat.wtbl_d30.field_v2.resp_rcpi_0),
		RCPI_TO_dBm(pWtbl->rx_stat.wtbl_d30.field_v2.resp_rcpi_1),
		RCPI_TO_dBm(pWtbl->rx_stat.wtbl_d30.field_v2.resp_rcpi_2),
		RCPI_TO_dBm(pWtbl->rx_stat.wtbl_d30.field_v2.resp_rcpi_3));
	else
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tRSSI = %d %d %d %d\n",
		RCPI_TO_dBm(pWtbl->rx_stat.wtbl_d29.field.resp_rcpi_0),
		RCPI_TO_dBm(pWtbl->rx_stat.wtbl_d29.field.resp_rcpi_1),
		RCPI_TO_dBm(pWtbl->rx_stat.wtbl_d29.field.resp_rcpi_2),
		RCPI_TO_dBm(pWtbl->rx_stat.wtbl_d29.field.resp_rcpi_3));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "%s", "\tRate Info\n");

	i4BytesWritten += connac2x_wtbl_rate_to_string(
		pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, 1, pWtbl);

	return i4BytesWritten;
}

static void connac2x_print_wtbl_info(
	struct ADAPTER *prAdapter,
	int32_t idx)
{
	struct mt66xx_chip_info *prChipInfo;
	int32_t start_idx, end_idx;
	uint32_t wtbl_offset, addr;
	uint32_t u4Value = 0;
	uint32_t wtbl_lmac_baseaddr;
	unsigned char *wtbl_raw_dw = NULL;
	struct fwtbl_lmac_struct *pwtbl;
	unsigned char myaddr[6];
	uint16_t txrate[8], rate_idx, txmode, mcs, nss, stbc;
	uint8_t u1BeamChgDw5 = 0, u1Dw3NewContent = 1;
	uint8_t u1WtblSize = 0;

	prChipInfo = prAdapter->chip_info;
	if ((idx >= 0) && (idx < prAdapter->ucWtblEntryNum))
		start_idx = end_idx = idx;
	else {
		start_idx = 0;
		end_idx = prAdapter->ucWtblEntryNum - 1;
	}

	if ((WTBL_VER == 1) && (wlanGetEcoVersion(prAdapter) < ECO_VER_2)) {
		u1WtblSize = sizeof(struct fwtbl_lmac_struct);
		u1BeamChgDw5 = 1;
		u1Dw3NewContent = 0;
	} else if ((WTBL_VER == 2) &&
		(wlanGetEcoVersion(prAdapter) < ECO_VER_2)) {
		u1WtblSize = sizeof(struct fwtbl_lmac_struct);
		u1Dw3NewContent = 0;
	} else {
		u1WtblSize =
			sizeof(struct fwtbl_lmac_struct) - sizeof(uint32_t);
	}

	for (idx = start_idx; idx <= end_idx; idx++) {
		/* LMAC */
		CONNAC2X_LWTBL_CONFIG(prAdapter,
			prChipInfo->u4LmacWtblDUAddr, idx);
		wtbl_lmac_baseaddr = CONNAC2X_LWTBL_IDX2BASE(
			prChipInfo->u4LmacWtblDUAddr, idx, 0);
		HAL_MCR_RD(prAdapter, prChipInfo->u4LmacWtblDUAddr,
					&u4Value);

		LOG_FUNC("\n\tLMAC WTBL Addr: group: 0x%x=0x%x addr: 0x%x\n",
			prChipInfo->u4LmacWtblDUAddr,
			u4Value,
			wtbl_lmac_baseaddr);

		wtbl_raw_dw = (unsigned char *)kalMemAlloc(
			sizeof(struct fwtbl_lmac_struct), VIR_MEM_TYPE);
		if (!wtbl_raw_dw) {
			DBGLOG(REQ, ERROR, "WTBL PRN : Memory alloc failed\n");
			return;
		}
		/* Read LWTBL Entries */
		for (wtbl_offset = 0; wtbl_offset <
			sizeof(struct fwtbl_lmac_struct);
			wtbl_offset += 4) {
			addr = wtbl_lmac_baseaddr + wtbl_offset;
			HAL_MCR_RD(prAdapter, addr,
				   &u4Value);
			kalMemCopy(
				(uint32_t *)&wtbl_raw_dw[wtbl_offset],
				&u4Value, sizeof(uint32_t));
		}
		pwtbl = (struct fwtbl_lmac_struct *)wtbl_raw_dw;
		kalMemMove(&myaddr[0],
			   (unsigned char *)&pwtbl->peer_basic_info, 4);
		myaddr[0] =
			pwtbl->peer_basic_info.wtbl_d1.field.addr_0 &
			0xff;
		myaddr[1] =
			((pwtbl->peer_basic_info.wtbl_d1.field.addr_0 &
			  0xff00) >>
			 8);
		myaddr[2] =
			((pwtbl->peer_basic_info.wtbl_d1.field.addr_0 &
			  0xff0000) >>
			 16);
		myaddr[3] =
			((pwtbl->peer_basic_info.wtbl_d1.field.addr_0 &
			  0xff000000) >>
			 24);
		myaddr[4] =
			pwtbl->peer_basic_info.wtbl_d0.field.addr_4 &
			0xff;
		myaddr[5] =
			pwtbl->peer_basic_info.wtbl_d0.field.addr_5 &
			0xff;
		LOG_FUNC(
		"\n\tLWTBL DW 0,1:\n"
		"\tAddr: %x:%x:%x:%x:%x:%x(D0[B0~15], D1[B0~31])\n"
		"\tMUAR_Idx(D0[B16~21]):%d\n"
		"\tRCA1:%d\n"
		"\tKID:%d\n"
		"\tRCID:%d\n"
		"\tFROM_DS:%d\n"
		"\tTO_DS:%d\n"
		"\tRV:%d\n"
		"\tRCA2:%d\n"
		"\tWPI_FLAG:%d\n",
	       myaddr[0], myaddr[1], myaddr[2], myaddr[3],
	       myaddr[4], myaddr[5],
			pwtbl->peer_basic_info.wtbl_d0.field.muar_idx,
			pwtbl->peer_basic_info.wtbl_d0.field.rc_a1,
			pwtbl->peer_basic_info.wtbl_d0.field.kid,
			pwtbl->peer_basic_info.wtbl_d0.field.rc_id,
			pwtbl->peer_basic_info.wtbl_d0.field.fd,
			pwtbl->peer_basic_info.wtbl_d0.field.td,
			pwtbl->peer_basic_info.wtbl_d0.field.rv,
			pwtbl->peer_basic_info.wtbl_d0.field.rc_a2,
			pwtbl->peer_basic_info.wtbl_d0.field.wpi_flg);

		LOG_FUNC(
			"\n\tLWTBL DW 2\n"
			"\tAID12:%d\n"
			"\tGID_SU:%d\n"
			"\tSPP_EN:%d\n"
			"\tWPI_EVEN:%d\n"
			"\tAAD_OM:%d\n"
			"\tCIPHER_SUITE:%d\n"
			"\tCIPHER_SUITE_IGTK:%d\n"
			"\tRSVD:%d\n"
			"\tSW:%d\n"
			"\tUL:%d\n"
			"\tPOWER_SAVE:%d\n"
			"\tQOS:%d\n"
			"\tHT:%d\n"
			"\tVHT:%d\n"
			"\tHE:%d\n"
			"\tMESH:%d\n",
			pwtbl->trx_cap.wtbl_d2.field.aid12,
			pwtbl->trx_cap.wtbl_d2.field.gid_su,
			pwtbl->trx_cap.wtbl_d2.field.spp_en,
			pwtbl->trx_cap.wtbl_d2.field.wpi_even,
			pwtbl->trx_cap.wtbl_d2.field.aad_om,
			pwtbl->trx_cap.wtbl_d2.field.cipher_suit,
			pwtbl->trx_cap.wtbl_d2.field.cipher_suit_igtk,
			pwtbl->trx_cap.wtbl_d2.field.rsvd,
			pwtbl->trx_cap.wtbl_d2.field.sw,
			pwtbl->trx_cap.wtbl_d2.field.ul,
			pwtbl->trx_cap.wtbl_d2.field.tx_ps,
			pwtbl->trx_cap.wtbl_d2.field.qos,
			pwtbl->trx_cap.wtbl_d2.field.ht,
			pwtbl->trx_cap.wtbl_d2.field.vht,
			pwtbl->trx_cap.wtbl_d2.field.he,
			pwtbl->trx_cap.wtbl_d2.field.mesh);

		if (u1Dw3NewContent > 0)
			LOG_FUNC(
				"\n\tLWTBL DW 3\n"
				"\tWMM_Q:%d\n"
				"\tRXD_DUP_MODE:%d\n"
				"\tVLAN2ETH:%d\n"
				"\tBEAM_CHG:%d\n"
				"\tBA_MODE:%d\n"
				"\tPFMU_IDX:%d\n"
				"\tULPF_IDX:%d\n"
				"\tRIBF:%d\n"
				"\tULPF:%d\n"
				"\tIGN_FBK:%d\n"
				"\tTEBF:%d\n"
				"\tTEBF_VHT:%d\n"
				"\tTEBF_HE:%d\n",
				pwtbl->trx_cap.wtbl_d3.field.wmm_q,
				pwtbl->trx_cap.wtbl_d3.field.rxd_dup_mode,
				pwtbl->trx_cap.wtbl_d3.field.vlan_2e_th,
				pwtbl->trx_cap.wtbl_d3.field_v2.beam_chg,
				pwtbl->trx_cap.wtbl_d3.field_v2.ba_mode,
				pwtbl->trx_cap.wtbl_d3.field.pfmu_index,
				pwtbl->trx_cap.wtbl_d3.field_v2.ulpf_index,
				pwtbl->trx_cap.wtbl_d3.field.ribf,
				pwtbl->trx_cap.wtbl_d3.field_v2.ulpf,
				pwtbl->trx_cap.wtbl_d3.field_v2.ign_fbk,
				pwtbl->trx_cap.wtbl_d3.field.tebf,
				pwtbl->trx_cap.wtbl_d3.field.tebf_vht,
				pwtbl->trx_cap.wtbl_d3.field.tebf_he);
		else {
			if (u1BeamChgDw5 > 0)
				LOG_FUNC(
				"\n\tLWTBL DW 3\n"
				"\tWMM_Q:%d\n"
				"\tRXD_DUP_MODE:%d\n"
				"\tVLAN2ETH:%d\n"
				"\tPFMU_IDX:%d\n"
				"\tRIBF:%d\n"
				"\tTEBF:%d\n"
				"\tTEBF_VHT:%d\n"
				"\tTEBF_HE:%d\n",
				pwtbl->trx_cap.wtbl_d3.field.wmm_q,
				pwtbl->trx_cap.wtbl_d3.field.rxd_dup_mode,
				pwtbl->trx_cap.wtbl_d3.field.vlan_2e_th,
				pwtbl->trx_cap.wtbl_d3.field.pfmu_index,
				pwtbl->trx_cap.wtbl_d3.field.ribf,
				pwtbl->trx_cap.wtbl_d3.field.tebf,
				pwtbl->trx_cap.wtbl_d3.field.tebf_vht,
				pwtbl->trx_cap.wtbl_d3.field.tebf_he);
			else
				LOG_FUNC(
				"\n\tLWTBL DW 3\n"
				"\tWMM_Q:%d\n"
				"\tRXD_DUP_MODE:%d\n"
				"\tVLAN2ETH:%d\n"
				"\tBEAM_CHG:%d\n"
				"\tPFMU_IDX:%d\n"
				"\tRIBF:%d\n"
				"\tTEBF:%d\n"
				"\tTEBF_VHT:%d\n"
				"\tTEBF_HE:%d\n",
				pwtbl->trx_cap.wtbl_d3.field.wmm_q,
				pwtbl->trx_cap.wtbl_d3.field.rxd_dup_mode,
				pwtbl->trx_cap.wtbl_d3.field.vlan_2e_th,
				pwtbl->trx_cap.wtbl_d3.field_v2.beam_chg,
				pwtbl->trx_cap.wtbl_d3.field.pfmu_index,
				pwtbl->trx_cap.wtbl_d3.field.ribf,
				pwtbl->trx_cap.wtbl_d3.field.tebf,
				pwtbl->trx_cap.wtbl_d3.field.tebf_vht,
				pwtbl->trx_cap.wtbl_d3.field.tebf_he);
		}

		LOG_FUNC(
		       "\n\tLWTBL DW 4\n"
		       "\tANT_ID_STS0:%d\n"
		       "\tANT_ID_STS1:%d\n"
		       "\tANT_ID_STS2:%d\n"
		       "\tANT_ID_STS3:%d\n"
		       "\tANT_ID_STS4:%d\n"
		       "\tANT_ID_STS5:%d\n"
		       "\tANT_ID_STS6:%d\n"
		       "\tANT_ID_STS7:%d\n"
		       "\tCASCAD:%d\n"
		       "\tLDPC_HT:%d\n"
		       "\tLDPC_VHT:%d\n"
		       "\tLDPC_HE:%d\n"
		       "\tDIS_RTHR:%d\n"
		       "\tALL_ACK:%d\n"
		       "\tDROP:%d\n"
		       "\tACK_EN:%d\n",
		       pwtbl->trx_cap.wtbl_d4.field.ant_id_sts0,
		       pwtbl->trx_cap.wtbl_d4.field.ant_id_sts1,
		       pwtbl->trx_cap.wtbl_d4.field.ant_id_sts2,
		       pwtbl->trx_cap.wtbl_d4.field.ant_id_sts3,
		       pwtbl->trx_cap.wtbl_d4.field.ant_id_sts4,
		       pwtbl->trx_cap.wtbl_d4.field.ant_id_sts5,
		       pwtbl->trx_cap.wtbl_d4.field.ant_id_sts6,
		       pwtbl->trx_cap.wtbl_d4.field.ant_id_sts7,
		       pwtbl->trx_cap.wtbl_d4.field.cascad,
		       pwtbl->trx_cap.wtbl_d4.field.ldpc_ht,
		       pwtbl->trx_cap.wtbl_d4.field.ldpc_vht,
		       pwtbl->trx_cap.wtbl_d4.field.ldpc_he,
		       pwtbl->trx_cap.wtbl_d4.field.dis_rhtr,
		       pwtbl->trx_cap.wtbl_d4.field.all_ack,
		       pwtbl->trx_cap.wtbl_d4.field.drop,
		       pwtbl->trx_cap.wtbl_d4.field.ack_en);

		if (u1BeamChgDw5 > 0)
			LOG_FUNC(
			"\n\tLWTBL DW 5\n"
			"\tAF:%d\n"
			"\tAF_HE:%d\n"
			"\tRTS:%d\n"
			"\tSMPS:%d\n"
			"\tDYN_BW:%d\n"
			"\tMMSS:%d\n"
			"\tUSR:%d\n"
			"\tSR_R:%d\n"
			"\tBEAM_CHG:%d\n"
			"\tSR_ABORT:%d\n"
			"\tTX_POWER_OFFSET:%d\n"
			"\tMPDU_SIZE:%d\n"
			"\tPE:%d\n"
			"\tDOPPL:%d\n"
			"\tTXOP_PS_CAP:%d\n"
			"\tDONOT_UPDATE_I_PSM:%d\n"
			"\tI_PSM:%d\n"
			"\tPSM:%d\n"
			"\tSKIP_TX:%d\n",
			pwtbl->trx_cap.wtbl_d5.field.af,
			pwtbl->trx_cap.wtbl_d5.field.af_he,
			pwtbl->trx_cap.wtbl_d5.field.rts,
			pwtbl->trx_cap.wtbl_d5.field.smps,
			pwtbl->trx_cap.wtbl_d5.field.dyn_bw,
			pwtbl->trx_cap.wtbl_d5.field.mmss,
			pwtbl->trx_cap.wtbl_d5.field.usr,
			pwtbl->trx_cap.wtbl_d5.field.sr_r,
			pwtbl->trx_cap.wtbl_d5.field.beam_chg,
			pwtbl->trx_cap.wtbl_d5.field.sr_abort,
			pwtbl->trx_cap.wtbl_d5.field.tx_power_offset,
			pwtbl->trx_cap.wtbl_d5.field.mpdu_size,
			pwtbl->trx_cap.wtbl_d5.field.pe,
			pwtbl->trx_cap.wtbl_d5.field.doppl,
			pwtbl->trx_cap.wtbl_d5.field.txop_ps_cap,
			pwtbl->trx_cap.wtbl_d5.field.du_i_psm,
			pwtbl->trx_cap.wtbl_d5.field.i_psm,
			pwtbl->trx_cap.wtbl_d5.field.psm,
			pwtbl->trx_cap.wtbl_d5.field.skip_tx);
		else
			LOG_FUNC(
			"\n\tLWTBL DW 5\n"
			"\tAF:%d\n"
			"\tAF_HE:%d\n"
			"\tRTS:%d\n"
			"\tSMPS:%d\n"
			"\tDYN_BW:%d\n"
			"\tMMSS:%d\n"
			"\tUSR:%d\n"
			"\tSR_R:%d\n"
			"\tSR_ABORT:%d\n"
			"\tTX_POWER_OFFSET:%d\n"
			"\tMPDU_SIZE:%d\n"
			"\tPE:%d\n"
			"\tDOPPL:%d\n"
			"\tTXOP_PS_CAP:%d\n"
			"\tDONOT_UPDATE_I_PSM:%d\n"
			"\tI_PSM:%d\n"
			"\tPSM:%d\n"
			"\tSKIP_TX:%d\n",
			pwtbl->trx_cap.wtbl_d5.field.af,
			pwtbl->trx_cap.wtbl_d5.field.af_he,
			pwtbl->trx_cap.wtbl_d5.field.rts,
			pwtbl->trx_cap.wtbl_d5.field.smps,
			pwtbl->trx_cap.wtbl_d5.field.dyn_bw,
			pwtbl->trx_cap.wtbl_d5.field.mmss,
			pwtbl->trx_cap.wtbl_d5.field.usr,
			pwtbl->trx_cap.wtbl_d5.field_v2.sr_r,
			pwtbl->trx_cap.wtbl_d5.field.sr_abort,
			pwtbl->trx_cap.wtbl_d5.field.tx_power_offset,
			pwtbl->trx_cap.wtbl_d5.field.mpdu_size,
			pwtbl->trx_cap.wtbl_d5.field.pe,
			pwtbl->trx_cap.wtbl_d5.field.doppl,
			pwtbl->trx_cap.wtbl_d5.field.txop_ps_cap,
			pwtbl->trx_cap.wtbl_d5.field.du_i_psm,
			pwtbl->trx_cap.wtbl_d5.field.i_psm,
			pwtbl->trx_cap.wtbl_d5.field.psm,
			pwtbl->trx_cap.wtbl_d5.field.skip_tx);

		LOG_FUNC(
		       "\n\tLWTBL DW 6\n"
			"\tTID0 BA_WIN_SIZE:%d\n"
			"\tTID1 BA_WIN_SIZE:%d\n"
			"\tTID2 BA_WIN_SIZE:%d\n"
			"\tTID3 BA_WIN_SIZE:%d\n"
			"\tTID4 BA_WIN_SIZE:%d\n"
			"\tTID5 BA_WIN_SIZE:%d\n"
			"\tTID6 BA_WIN_SIZE:%d\n"
			"\tTID7 BA_WIN_SIZE:%d\n",
			pwtbl->trx_cap.wtbl_d6.field.ba_win_size_tid0,
			pwtbl->trx_cap.wtbl_d6.field.ba_win_size_tid1,
			pwtbl->trx_cap.wtbl_d6.field.ba_win_size_tid2,
			pwtbl->trx_cap.wtbl_d6.field.ba_win_size_tid3,
			pwtbl->trx_cap.wtbl_d6.field.ba_win_size_tid4,
			pwtbl->trx_cap.wtbl_d6.field.ba_win_size_tid5,
			pwtbl->trx_cap.wtbl_d6.field.ba_win_size_tid6,
			pwtbl->trx_cap.wtbl_d6.field.ba_win_size_tid7);


		LOG_FUNC(
			"\n\tLWTBL DW 7\n"
			"\tCBRN:%d\n"
			"\tDBNSS_EN:%d\n"
			"\tBAF_EN:%d\n"
			"\tRDGBA:%d\n"
			"\tR:%d\n"
			"\tSPE_IDX:%d\n"
			"\tG2:%d\n"
			"\tG4:%d\n"
			"\tG8:%d\n"
			"\tG16:%d\n"
			"\tG2_LTF:%d\n"
			"\tG4_LTF:%d\n"
			"\tG8_LTF:%d\n"
			"\tG16_LTF:%d\n"
			"\tG2_HE:%d\n"
			"\tG4_HE:%d\n"
			"\tG8_HE:%d\n"
			"\tG16_HE:%d\n",
			pwtbl->trx_cap.wtbl_d7.field.cb_rn,
			pwtbl->trx_cap.wtbl_d7.field.dbnss_en,
			pwtbl->trx_cap.wtbl_d7.field.bafen,
			pwtbl->trx_cap.wtbl_d7.field.rdg_ba,
			pwtbl->trx_cap.wtbl_d7.field.r,
			pwtbl->trx_cap.wtbl_d7.field.spe_idx,
			pwtbl->trx_cap.wtbl_d7.field.g2,
			pwtbl->trx_cap.wtbl_d7.field.g4,
			pwtbl->trx_cap.wtbl_d7.field.g8,
			pwtbl->trx_cap.wtbl_d7.field.g16,
			pwtbl->trx_cap.wtbl_d7.field.g2_ltf,
			pwtbl->trx_cap.wtbl_d7.field.g4_ltf,
			pwtbl->trx_cap.wtbl_d7.field.g8_ltf,
			pwtbl->trx_cap.wtbl_d7.field.g16_ltf,
			pwtbl->trx_cap.wtbl_d7.field.g2_he,
			pwtbl->trx_cap.wtbl_d7.field.g4_he,
			pwtbl->trx_cap.wtbl_d7.field.g8_he,
			pwtbl->trx_cap.wtbl_d7.field.g16_he);

		LOG_FUNC(
			"\n\tLWTBL DW 9\n"
			"\tFCAP:0x%x\n"
			"\tFCAP_20_40_MHZ:%d\n"
			"\tFCAP_20_TO_160_MHZ:%d\n"
			"\tFCAP_20_TO_80_MHZ:%d\n",
			pwtbl->trx_cap.wtbl_d9.field.fcap,
			((pwtbl->trx_cap.wtbl_d9.field.fcap) &
			(BIT(0))),
			pwtbl->trx_cap.wtbl_d9.field.fcap,
			(((pwtbl->trx_cap.wtbl_d9.field.fcap) &
			(BIT(1))) >> 1));

		/* Rate Info (DW10~13) */
		LOG_FUNC("Rate Info (DW10~13):");
		txrate[0] = pwtbl->auto_rate_tb.wtbl_d10.field.rate1;
		txrate[1] = pwtbl->auto_rate_tb.wtbl_d10.field.rate2;
		txrate[2] = pwtbl->auto_rate_tb.wtbl_d11.field.rate3;
		txrate[3] = pwtbl->auto_rate_tb.wtbl_d11.field.rate4;
		txrate[4] = pwtbl->auto_rate_tb.wtbl_d12.field.rate5;
		txrate[5] = pwtbl->auto_rate_tb.wtbl_d12.field.rate6;
		txrate[6] = pwtbl->auto_rate_tb.wtbl_d13.field.rate7;
		txrate[7] = pwtbl->auto_rate_tb.wtbl_d13.field.rate8;

		for (rate_idx = 0; rate_idx < 8; rate_idx++) {
			txmode = (((txrate[rate_idx]) &
				(0xf << 6)) >> 6);
			mcs = ((txrate[rate_idx]) &
				(0x3f));
			nss = (((txrate[rate_idx]) &
				(0x7 << 10)) >> 10) + 1;
			stbc = (((txrate[rate_idx]) &
				(0x1 << 13)) >> 13);

			if (!(rate_idx%2))
				LOG_FUNC("LWTBL DW %d\n",
					(rate_idx/2)+10);

				if (txmode == TX_RATE_MODE_CCK)
					LOG_FUNC(
	"\tRate%d(0x%x):TxMode=%d(%s) TxRate=%d(%s) Nsts=%d STBC=%d\n",
					rate_idx + 1,
					txrate[rate_idx],
					txmode,
					(txmode < ENUM_TX_MODE_NUM ?
					RATE_V2_HW_TX_MODE_STR[txmode] : "N/A"),
					mcs,
					mcs < 4 ? HW_TX_RATE_CCK_STR[mcs] :
					   HW_TX_RATE_CCK_STR[4],
					nss, stbc);
				else if (txmode == TX_RATE_MODE_OFDM)
					LOG_FUNC(
	"\tRate%d(0x%x):TxMode=%d(%s) TxRate=%d(%s) Nsts=%d STBC=%d\n",
					rate_idx + 1,
					txrate[rate_idx],
					txmode,
					(txmode < ENUM_TX_MODE_NUM ?
					RATE_V2_HW_TX_MODE_STR[txmode] : "N/A"),
					mcs,
					nicHwRateOfdmStr(mcs),
					nss, stbc);
				else
					LOG_FUNC(
	"\tRate%d(0x%x):TxMode=%d(%s) TxRate=%d(MCS%d) Nsts=%d STBC=%d\n",
					rate_idx + 1,
					txrate[rate_idx],
					txmode,
					(txmode < ENUM_TX_MODE_NUM ?
					RATE_V2_HW_TX_MODE_STR[txmode] : "N/A"),
					mcs,
					mcs,
					nss, stbc);
		}
		LOG_FUNC("\n");

		/* Show LWTBL RAW Data */
		for (wtbl_offset = 0; wtbl_offset < u1WtblSize;
			wtbl_offset += 4) {
			kalMemCopy(&u4Value,
				(uint32_t *)&wtbl_raw_dw[wtbl_offset],
				sizeof(uint32_t));
			LOG_FUNC(
				"\tDW%02d: %02x %02x %02x %02x\n",
				wtbl_offset / 4,
				(u4Value & 0xff000000) >> 24,
				(u4Value & 0xff0000) >> 16,
				(u4Value & 0xff00) >> 8,
				u4Value & 0xff);
		}
		LOG_FUNC("\n");
		kalMemFree(wtbl_raw_dw, VIR_MEM_TYPE,
			sizeof(struct fwtbl_lmac_struct));
	}
}

int32_t connac2x_show_wtbl_info(
	struct ADAPTER *prAdapter,
	uint32_t u4Index,
	char *pcCommand,
	int i4TotalLen)
{
	struct mt66xx_chip_info *prChipInfo;
	uint32_t u4Value = 0;
	uint32_t wtbl_lmac_baseaddr;
	uint32_t wtbl_offset, addr;
	unsigned char *wtbl_raw_dw = NULL;
	struct fwtbl_lmac_struct *pwtbl;
	int32_t i4BytesWritten = 0;

	prChipInfo = prAdapter->chip_info;
	DBGLOG(REQ, INFO, "WTBL : index = %d\n", u4Index);

	wtbl_raw_dw = (unsigned char *)kalMemAlloc(
		sizeof(struct fwtbl_lmac_struct), VIR_MEM_TYPE);
	if (!wtbl_raw_dw) {
		DBGLOG(REQ, ERROR, "WTBL : Memory alloc failed\n");
		return 0;
	}

	/* LMAC */
	CONNAC2X_LWTBL_CONFIG(prAdapter, prChipInfo->u4LmacWtblDUAddr, u4Index);
	wtbl_lmac_baseaddr = CONNAC2X_LWTBL_IDX2BASE(
		prChipInfo->u4LmacWtblDUAddr, u4Index, 0);
	HAL_MCR_RD(prAdapter, prChipInfo->u4LmacWtblDUAddr,
				&u4Value);

	DBGLOG(REQ, INFO, "LMAC WTBL Addr: group: 0x%x=0x%x addr: 0x%x\n",
		prChipInfo->u4LmacWtblDUAddr,
		u4Value,
		wtbl_lmac_baseaddr);

	/* Read LWTBL Entries */
	for (wtbl_offset = 0; wtbl_offset <
		sizeof(struct fwtbl_lmac_struct);
		wtbl_offset += 4) {
		addr = wtbl_lmac_baseaddr + wtbl_offset;
		HAL_MCR_RD(prAdapter, addr,
			   &u4Value);
		kalMemCopy(
			(uint32_t *)&wtbl_raw_dw[wtbl_offset],
			&u4Value, sizeof(uint32_t));
	}

	pwtbl = (struct fwtbl_lmac_struct *)wtbl_raw_dw;
	i4BytesWritten = connac2x_dump_helper_wtbl_info(
		prAdapter,
		pcCommand,
		i4TotalLen,
		pwtbl,
		u4Index);

	kalMemFree(wtbl_raw_dw, VIR_MEM_TYPE,
			sizeof(struct fwtbl_lmac_struct));

	connac2x_print_wtbl_info(prAdapter, u4Index);
	return i4BytesWritten;
}

int32_t connac2x_show_umac_wtbl_info(
	struct ADAPTER *prAdapter,
	uint32_t u4Index,
	char *pcCommand,
	int i4TotalLen)
{
	struct mt66xx_chip_info *prChipInfo;
	int32_t i4BytesWritten = 0;
	uint8_t keytbl[32] = {0};
	uint8_t keytbl2[32] = {0};
	uint16_t x;
	uint32_t *dest_cpy = (uint32_t *)keytbl;
	uint32_t sizeInDW = 8;
	uint32_t u4SrcAddr = 0;
	uint32_t u4Value = 0;
	uint32_t wtbl_offset, addr;
	uint32_t wtbl_umac_baseaddr;
	unsigned char *wtbl_raw_dw = NULL;
	struct fwtbl_umac_struct *puwtbl;
	unsigned long long pn = 0;

	prChipInfo = prAdapter->chip_info;
	/* UMAC */
	CONNAC2X_UWTBL_CONFIG(prAdapter, prChipInfo->u4UmacWtblDUAddr, u4Index);
	wtbl_umac_baseaddr = CONNAC2X_UWTBL_IDX2BASE(
		prChipInfo->u4UmacWtblDUAddr, u4Index, 0);
	HAL_MCR_RD(prAdapter, prChipInfo->u4UmacWtblDUAddr, &u4Value);
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
		"UMAC WTBL Addr: group: 0x%x=0x%x addr: 0x%x\n",
		prChipInfo->u4UmacWtblDUAddr,
		u4Value,
		wtbl_umac_baseaddr);

	wtbl_raw_dw = (unsigned char *)kalMemAlloc(
		sizeof(struct fwtbl_umac_struct), VIR_MEM_TYPE);
	if (!wtbl_raw_dw) {
		DBGLOG(REQ, ERROR, "WTBL : Memory alloc failed\n");
		return 0;
	}
	/* Read UWTBL Entries */
	for (wtbl_offset = 0; wtbl_offset <
		sizeof(struct fwtbl_umac_struct);
		wtbl_offset += 4) {
		addr = wtbl_umac_baseaddr + wtbl_offset;
		HAL_MCR_RD(prAdapter, addr,
			   &u4Value);
		kalMemCopy(
			(uint32_t *)&wtbl_raw_dw[wtbl_offset],
			&u4Value, sizeof(uint32_t));
	}
	puwtbl = (struct fwtbl_umac_struct *)wtbl_raw_dw;
	pn = ((pn || (puwtbl->serial_no.wtbl_d1.field.pn1) << 5)
		|| puwtbl->serial_no.wtbl_d0.field.pn0);
	/* UMAC WTBL DW 0,1 */
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
		"UWTBL DW 0,1\n\tpn:%d\n\tcom_sn:%d\n",
		pn,
		puwtbl->serial_no.wtbl_d1.field.com_sn);

	/* UMAC WTBL DW 5 */
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
		"UWTBL DW 5\n"
		"\tKey_loc0:%d\n"
		"\tKey_loc1:%d\n"
		"\tQoS:%d\n"
		"\tHT:%d\n"
		"\tHW_AMSDU_CFG:%d\n",
		puwtbl->klink_amsdu.wtbl_d5.field.key_loc0,
		puwtbl->klink_amsdu.wtbl_d5.field.key_loc1,
		puwtbl->klink_amsdu.wtbl_d5.field.qos,
		puwtbl->klink_amsdu.wtbl_d5.field.ht,
		puwtbl->klink_amsdu.wtbl_d6.field.hw_amsdu_cfg);

	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
		"\n\nkeyloc0:%d\n",
		puwtbl->klink_amsdu.wtbl_d5.field.key_loc0);

	if (puwtbl->klink_amsdu.wtbl_d5.field.key_loc0 !=
		(CONNAC2X_WTBL_KEY_LINK_DW_KEY_LOC0_MASK >>
		CONNAC2X_WTBL_KEY_LINK_DW_KEY_LOC0_OFFSET)) {
		/* will write new value WTBL_UMAC_TOP_BASE */
		CONNAC2X_KEYTBL_CONFIG(prAdapter, prChipInfo->u4UmacWtblDUAddr,
		puwtbl->klink_amsdu.wtbl_d5.field.key_loc0);
		u4SrcAddr = CONNAC2X_KEYTBL_IDX2BASE(
			prChipInfo->u4UmacWtblDUAddr,
			puwtbl->klink_amsdu.wtbl_d5.field.key_loc0, 0);

		HAL_MCR_RD(prAdapter, prChipInfo->u4UmacWtblDUAddr,
			&u4Value);
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
			"KEY WTBL Addr: group:0x%x=0x%x addr: 0x%x\n",
			prChipInfo->u4UmacWtblDUAddr,
			u4Value,
			u4SrcAddr);

		/* Read Entries */
		while (sizeInDW--) {
			HAL_MCR_RD(prAdapter, u4SrcAddr, &u4Value);
			kalMemCopy(dest_cpy, &u4Value, sizeof(uint32_t));
			dest_cpy++;
			u4SrcAddr += 4;
		}

		for (x = 0; x < 8; x++) {
			LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
				"DW%02d: %02x %02x %02x %02x\n",
				x,
				keytbl[x * 4 + 3],
				keytbl[x * 4 + 2],
				keytbl[x * 4 + 1],
				keytbl[x * 4]);
		}
	}
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
				"\nkeyloc1:%d\n",
				puwtbl->klink_amsdu.wtbl_d5.field.key_loc1);

	if (puwtbl->klink_amsdu.wtbl_d5.field.key_loc1 !=
		(CONNAC2X_WTBL_KEY_LINK_DW_KEY_LOC1_MASK >>
		CONNAC2X_WTBL_KEY_LINK_DW_KEY_LOC1_OFFSET)) {
		dest_cpy = (uint32_t *)keytbl2;
		sizeInDW = 8;

		/* will write new value WF_WTBLON_TOP_WDUCR_ADDR */
		CONNAC2X_KEYTBL_CONFIG(prAdapter, prChipInfo->u4UmacWtblDUAddr,
		puwtbl->klink_amsdu.wtbl_d5.field.key_loc1);
		u4SrcAddr = CONNAC2X_KEYTBL_IDX2BASE(
			prChipInfo->u4UmacWtblDUAddr,
			puwtbl->klink_amsdu.wtbl_d5.field.key_loc1, 0);

		HAL_MCR_RD(prAdapter, prChipInfo->u4UmacWtblDUAddr,
			&u4Value);
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
			"KEY WTBL Addr: group:0x%x=0x%x addr: 0x%x\n",
			prChipInfo->u4UmacWtblDUAddr,
			u4Value,
			u4SrcAddr);

		/* Read Entries */
		while (sizeInDW--) {
			HAL_MCR_RD(prAdapter, u4SrcAddr, &u4Value);
			kalMemCopy(dest_cpy, &u4Value, sizeof(uint32_t));
			dest_cpy++;
			u4SrcAddr += 4;
		}

		for (x = 0; x < 8; x++) {
			LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
				"DW%02d: %02x %02x %02x %02x\n",
				x,
				keytbl2[x * 4 + 3],
				keytbl2[x * 4 + 2],
				keytbl2[x * 4 + 1],
				keytbl2[x * 4]);
		}
	}
	kalMemFree(wtbl_raw_dw, VIR_MEM_TYPE,
		sizeof(struct fwtbl_umac_struct));

	return i4BytesWritten;
}

int32_t connac2x_show_rx_rate_info(
		struct ADAPTER *prAdapter,
		char *pcCommand,
		int32_t i4TotalLen,
		uint8_t ucStaIdx)
{
	int32_t i4BytesWritten = 0;
	uint32_t txmode, rate, frmode, sgi, nsts, ldpc, stbc, groupid, mu;
	uint32_t u4RxVector0 = 0, u4RxVector1 = 0, u4RxVector2 = 0;

	/* Group3 PRXV1[0:31] */
	u4RxVector0 = prAdapter->arStaRec[ucStaIdx].u4RxVector0;
	/* Group5 C-B-0[0:31] */
	u4RxVector1 = prAdapter->arStaRec[ucStaIdx].u4RxVector1;
	/* Group5 C-B-1[0:31] */
	u4RxVector2 = prAdapter->arStaRec[ucStaIdx].u4RxVector2;

	DBGLOG(REQ, LOUD, "****** P-RXVector1 = 0x%08x ******\n",
		   u4RxVector0);
	DBGLOG(REQ, LOUD, "****** C-RXVector1 = 0x%08x ******\n",
		   u4RxVector1);
	DBGLOG(REQ, LOUD, "****** C-RXVector2 = 0x%08x ******\n",
		   u4RxVector2);

	/* P-RXV1 */
	rate = (u4RxVector0 & CONNAC2X_RX_VT_RX_RATE_MASK)
				>> CONNAC2X_RX_VT_RX_RATE_OFFSET;
	nsts = ((u4RxVector0 & CONNAC2X_RX_VT_NSTS_MASK)
				>> CONNAC2X_RX_VT_NSTS_OFFSET);
	ldpc = u4RxVector0 & CONNAC2X_RX_VT_LDPC;

	/* C-B-0 */
	stbc = (u4RxVector1 & CONNAC2X_RX_VT_STBC_MASK)
				>> CONNAC2X_RX_VT_STBC_OFFSET;
	txmode = (u4RxVector1 & CONNAC2X_RX_VT_RX_MODE_MASK)
				>> CONNAC2X_RX_VT_RX_MODE_OFFSET;
	frmode = (u4RxVector1 & CONNAC2X_RX_VT_FR_MODE_MASK)
				>> CONNAC2X_RX_VT_FR_MODE_OFFSET;
	sgi = (u4RxVector1 & CONNAC2X_RX_VT_SHORT_GI_MASK)
				>> CONNAC2X_RX_VT_SHORT_GI_OFFSET;
	/* C-B-1 */
	groupid = (u4RxVector2 & CONNAC2X_RX_VT_GROUP_ID_MASK)
				>> CONNAC2X_RX_VT_GROUP_ID_OFFSET;

	if (groupid && groupid != 63) {
		mu = 1;
	} else {
		mu = 0;
		nsts += 1;
	}

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "%-20s%s", "Last RX Rate", " = ");

	if (txmode == TX_RATE_MODE_CCK)
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s, ",
			rate < 4 ? HW_TX_RATE_CCK_STR[rate] :
			(rate < 8 ? HW_TX_RATE_CCK_STR[rate - 4] :
				HW_TX_RATE_CCK_STR[4]));
	else if (txmode == TX_RATE_MODE_OFDM)
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s, ",
			nicHwRateOfdmStr(rate));
	else if ((txmode == TX_RATE_MODE_HTMIX) ||
		 (txmode == TX_RATE_MODE_HTGF))
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "MCS%d, ", rate);
	else
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s%d_MCS%d, ",
			stbc == 1 ? "NSTS" : "NSS", nsts, rate);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "%s, ",
		frmode < 4 ? HW_TX_RATE_BW[frmode] : HW_TX_RATE_BW[4]);

	if (txmode == TX_RATE_MODE_CCK)
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s, ",
			rate < 4 ? "LP" : "SP");
	else if (txmode == TX_RATE_MODE_OFDM)
		;
	else if (txmode == TX_RATE_MODE_HTMIX ||
		 txmode == TX_RATE_MODE_HTGF ||
		 txmode == TX_RATE_MODE_VHT)
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s, ",
			sgi == 0 ? "LGI" : "SGI");
	else
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s, ",
			sgi == 0 ? "SGI" : (sgi == 1 ? "MGI" : "LGI"));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "%s", stbc == 0 ? "" : "STBC, ");

	if (mu) {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s, %s, %s (%d)\n",
			txmode < ENUM_TX_MODE_NUM ?
			HW_TX_MODE_STR[txmode] : "N/A",
			ldpc == 0 ? "BCC" : "LDPC", "MU", groupid);
	} else {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s, %s\n",
			txmode < ENUM_TX_MODE_NUM ?
			HW_TX_MODE_STR[txmode] : "N/A",
			ldpc == 0 ? "BCC" : "LDPC");
	}

	return i4BytesWritten;
}

int32_t connac2x_show_rx_rssi_info(
		struct ADAPTER *prAdapter,
		char *pcCommand,
		int32_t i4TotalLen,
		uint8_t ucStaIdx)
{
	int32_t i4RSSI0 = 0, i4RSSI1 = 0, i4RSSI2 = 0, i4RSSI3 = 0;
	int32_t i4BytesWritten = 0;
	uint32_t u4CRxv4th = 0;

	/* Group5 C-B-3[0:31] */
	u4CRxv4th = prAdapter->arStaRec[ucStaIdx].u4RxVector4;

	DBGLOG(REQ, LOUD, "****** C-RXVector4th cycle = 0x%08x ******\n",
		   u4CRxv4th);

	i4RSSI0 = RCPI_TO_dBm((u4CRxv4th & CONNAC2X_RX_VT_RCPI0_MASK) >>
			      CONNAC2X_RX_VT_RCPI0_OFFSET);
	i4RSSI1 = RCPI_TO_dBm((u4CRxv4th & CONNAC2X_RX_VT_RCPI1_MASK) >>
			      CONNAC2X_RX_VT_RCPI1_OFFSET);

	if (prAdapter->rWifiVar.ucNSS > 2) {
		i4RSSI2 = RCPI_TO_dBm((u4CRxv4th & CONNAC2X_RX_VT_RCPI2_MASK) >>
				      CONNAC2X_RX_VT_RCPI2_OFFSET);
		i4RSSI3 = RCPI_TO_dBm((u4CRxv4th & CONNAC2X_RX_VT_RCPI3_MASK) >>
				      CONNAC2X_RX_VT_RCPI3_OFFSET);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%-20s%s%d %d %d %d\n",
			"Last RX Data RSSI", " = ",
			i4RSSI0, i4RSSI1, i4RSSI2, i4RSSI3);
	} else
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%-20s%s%d %d\n",
			"Last RX Data RSSI", " = ", i4RSSI0, i4RSSI1);

	return i4BytesWritten;
}

int32_t connac2x_show_stat_info(
		struct ADAPTER *prAdapter,
		char *pcCommand,
		int32_t i4TotalLen,
		struct PARAM_HW_WLAN_INFO *prHwWlanInfo,
		struct PARAM_GET_STA_STATISTICS *prQueryStaStatistics,
		uint8_t fgResetCnt,
		uint32_t u4StatGroup)
{
	int32_t i4BytesWritten = 0;
	int32_t rRssi;
	uint16_t u2LinkSpeed, u2Idx = 0;
	uint32_t u4Per, u4RxPer[ENUM_BAND_NUM], u4TxMpduPer[ENUM_BAND_NUM],
		 u4InstantPer;
	uint8_t ucDbdcIdx, ucSkipAr, ucStaIdx, ucNss;
	static uint32_t u4TotalTxCnt[CFG_STAT_DBG_PEER_NUM] = {0};
	static uint32_t u4TotalFailCnt[CFG_STAT_DBG_PEER_NUM] = {0};
	static uint32_t u4Rate1TxCnt[CFG_STAT_DBG_PEER_NUM] = {0};
	static uint32_t u4Rate1FailCnt[CFG_STAT_DBG_PEER_NUM] = {0};
	static uint32_t au4RxMpduCnt[ENUM_BAND_NUM] = {0};
	static uint32_t au4FcsError[ENUM_BAND_NUM] = {0};
	static uint32_t au4RxFifoCnt[ENUM_BAND_NUM] = {0};
	static uint32_t au4AmpduTxSfCnt[ENUM_BAND_NUM] = {0};
	static uint32_t au4AmpduTxAckSfCnt[ENUM_BAND_NUM] = {0};
	struct RX_CTRL *prRxCtrl;
	uint32_t u4InstantRxPer[ENUM_BAND_NUM];
	uint32_t u4InstantTxMpduPer[ENUM_BAND_NUM];
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	int16_t i2Wf0AvgPwr = 0, i2Wf1AvgPwr = 0;
	uint32_t u4BufLen = 0;
	uint8_t ucRaTableNum = sizeof(RATE_TBLE) / sizeof(char *);
	uint8_t ucRaStatusNum = sizeof(RA_STATUS_TBLE) / sizeof(char *);
	uint8_t ucBssIndex = AIS_DEFAULT_INDEX;
	struct PARAM_LINK_SPEED_EX rLinkSpeed;

#if 0
	uint8_t ucRaLtModeNum = sizeof(LT_MODE_TBLE) / sizeof(char *);
	uint8_t ucRaSgiUnSpStateNum = sizeof(SGI_UNSP_STATE_TBLE) /
								sizeof(char *);
	uint8_t ucRaBwStateNum = sizeof(BW_STATE_TBLE) / sizeof(char *);
#endif
	uint8_t aucAggRange[AGG_RANGE_SEL_NUM];
	uint32_t au4RangeCtrl[AGG_RANGE_SEL_4BYTE_NUM];
	enum AGG_RANGE_TYPE_T eRangeType = ENUM_AGG_RANGE_TYPE_TX;

	ucSkipAr = prQueryStaStatistics->ucSkipAr;
	prRxCtrl = &prAdapter->rRxCtrl;
	ucNss = prAdapter->rWifiVar.ucNSS;

	if (ucSkipAr) {
		u2Idx = nicGetStatIdxInfo(prAdapter,
			(uint8_t)(prHwWlanInfo->u4Index));

		if (u2Idx == 0xFFFF)
			return i4BytesWritten;
	}

	if (ucSkipAr) {
		u4TotalTxCnt[u2Idx] += prQueryStaStatistics->u4TransmitCount;
		u4TotalFailCnt[u2Idx] += prQueryStaStatistics->
							u4TransmitFailCount;
		u4Rate1TxCnt[u2Idx] += prQueryStaStatistics->u4Rate1TxCnt;
		u4Rate1FailCnt[u2Idx] += prQueryStaStatistics->u4Rate1FailCnt;
	}

	if (ucSkipAr) {
		u4Per = (u4Rate1TxCnt[u2Idx] == 0) ?
			(0) : (1000 * (u4Rate1FailCnt[u2Idx]) /
			(u4Rate1TxCnt[u2Idx]));

		u4InstantPer = (prQueryStaStatistics->u4Rate1TxCnt == 0) ?
			(0) : (1000 * (prQueryStaStatistics->u4Rate1FailCnt) /
				(prQueryStaStatistics->u4Rate1TxCnt));
	} else {
		u4Per = (prQueryStaStatistics->u4Rate1TxCnt == 0) ?
			(0) : (1000 * (prQueryStaStatistics->u4Rate1FailCnt) /
				(prQueryStaStatistics->u4Rate1TxCnt));

		u4InstantPer = (prQueryStaStatistics->ucPer == 0) ?
			(0) : (prQueryStaStatistics->ucPer);
	}

	for (ucDbdcIdx = 0; ucDbdcIdx < ENUM_BAND_NUM; ucDbdcIdx++) {
		au4RxMpduCnt[ucDbdcIdx] += g_arMibInfo[ucDbdcIdx].u4RxMpduCnt;
		au4FcsError[ucDbdcIdx] += g_arMibInfo[ucDbdcIdx].u4FcsError;
		au4RxFifoCnt[ucDbdcIdx] += g_arMibInfo[ucDbdcIdx].u4RxFifoFull;
		au4AmpduTxSfCnt[ucDbdcIdx] +=
			g_arMibInfo[ucDbdcIdx].u4AmpduTxSfCnt;
		au4AmpduTxAckSfCnt[ucDbdcIdx] +=
			g_arMibInfo[ucDbdcIdx].u4AmpduTxAckSfCnt;

		u4RxPer[ucDbdcIdx] =
		    ((au4RxMpduCnt[ucDbdcIdx] + au4FcsError[ucDbdcIdx]) == 0) ?
			(0) : (1000 * au4FcsError[ucDbdcIdx] /
				(au4RxMpduCnt[ucDbdcIdx] +
				au4FcsError[ucDbdcIdx]));

		u4TxMpduPer[ucDbdcIdx] =
		    (au4AmpduTxSfCnt[ucDbdcIdx] == 0) ?
			(0) : (1000 * (au4AmpduTxSfCnt[ucDbdcIdx] -
				au4AmpduTxAckSfCnt[ucDbdcIdx]) /
				au4AmpduTxSfCnt[ucDbdcIdx]);

		u4InstantRxPer[ucDbdcIdx] =
			((prQueryStaStatistics->rMibInfo[ucDbdcIdx].u4RxMpduCnt
			+ prQueryStaStatistics->rMibInfo[ucDbdcIdx].u4FcsError)
			== 0) ?
				(0) : (1000 * prQueryStaStatistics->
				rMibInfo[ucDbdcIdx].u4FcsError /
				(prQueryStaStatistics->rMibInfo[ucDbdcIdx].
				u4RxMpduCnt +
				prQueryStaStatistics->rMibInfo[ucDbdcIdx].
				u4FcsError));
		u4InstantTxMpduPer[ucDbdcIdx] =
			(prQueryStaStatistics->rMibInfo[ucDbdcIdx].
			u4AmpduTxSfCnt == 0) ?
				(0) : (1000 *
				(prQueryStaStatistics->rMibInfo[ucDbdcIdx].
				u4AmpduTxSfCnt -
				prQueryStaStatistics->rMibInfo[ucDbdcIdx].
				u4AmpduTxAckSfCnt) /
				prQueryStaStatistics->rMibInfo[ucDbdcIdx].
				u4AmpduTxSfCnt);
	}

	rRssi = RCPI_TO_dBm(prQueryStaStatistics->ucRcpi);
	u2LinkSpeed = (prQueryStaStatistics->u2LinkSpeed == 0) ? 0 :
					prQueryStaStatistics->u2LinkSpeed / 2;

	if (ucSkipAr) {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d(%d)\n", "\nWlanIdx(BackupIdx)", "  = ",
			prHwWlanInfo->u4Index, u2Idx);
	} else {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "\nWlanIdx", "  = ",
			prHwWlanInfo->u4Index);
	}

	/* =========== Group 0x0001 =========== */
	if (u4StatGroup & 0x0001) {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "----- STA Stat (Group 0x01) -----\n");

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "CurrTemperature", " = ",
			prQueryStaStatistics->ucTemperature);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "Tx Total cnt", " = ",
			ucSkipAr ? (u4TotalTxCnt[u2Idx]) :
				(prQueryStaStatistics->u4TransmitCount));

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "Tx Fail Cnt", " = ",
			ucSkipAr ? (u4TotalFailCnt[u2Idx]) :
				(prQueryStaStatistics->u4TransmitFailCount));

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "Rate1 Tx Cnt", " = ",
			ucSkipAr ? (u4Rate1TxCnt[u2Idx]) :
				(prQueryStaStatistics->u4Rate1TxCnt));

		if (ucSkipAr)
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d, PER = %d.%1d%%, instant PER = %d.%1d%%\n",
				"Rate1 Fail Cnt", " = ",
				u4Rate1FailCnt[u2Idx], u4Per/10, u4Per%10,
				u4InstantPer/10, u4InstantPer%10);
		else
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d, PER = %d.%1d%%, instant PER = %d%%\n",
				"Rate1 Fail Cnt", " = ",
				prQueryStaStatistics->u4Rate1FailCnt,
				u4Per/10, u4Per%10, u4InstantPer);

		if ((ucSkipAr) && (fgResetCnt)) {
			u4TotalTxCnt[u2Idx] = 0;
			u4TotalFailCnt[u2Idx] = 0;
			u4Rate1TxCnt[u2Idx] = 0;
			u4Rate1FailCnt[u2Idx] = 0;
		}
	}

	/* =========== Group 0x0002 =========== */
	if (u4StatGroup & 0x0002) {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					       i4TotalLen - i4BytesWritten,
			"%s", "----- MIB Info (Group 0x02) -----\n");

		for (ucDbdcIdx = 0; ucDbdcIdx < ENUM_BAND_NUM; ucDbdcIdx++) {
			if (prAdapter->rWifiVar.fgDbDcModeEn)
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"[DBDC_%d] :\n", ucDbdcIdx);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "RX Success", " = ",
				au4RxMpduCnt[ucDbdcIdx]);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d, PER = %d.%1d%%, instant PER = %d.%1d%%\n",
				"RX with CRC", " = ", au4FcsError[ucDbdcIdx],
				u4RxPer[ucDbdcIdx]/10, u4RxPer[ucDbdcIdx]%10,
				u4InstantRxPer[ucDbdcIdx]/10,
				u4InstantRxPer[ucDbdcIdx]%10);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "RX drop FIFO full", " = ",
				au4RxFifoCnt[ucDbdcIdx]);
#if 0
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "TX MPDU Success", " = ",
				au4AmpduTxAckSfCnt[ucDbdcIdx]);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d, PER = %d.%1d%%, instant PER = %d.%1d%%\n",
				"TX MPDU Fail", " = ",
				au4AmpduTxSfCnt[ucDbdcIdx] -
				au4AmpduTxAckSfCnt[ucDbdcIdx],
				u4TxMpduPer[ucDbdcIdx]/10,
				u4TxMpduPer[ucDbdcIdx]%10,
				u4InstantTxMpduPer[ucDbdcIdx]/10,
				u4InstantTxMpduPer[ucDbdcIdx]%10);
#endif
			if (!prAdapter->rWifiVar.fgDbDcModeEn)
				break;
		}

		if (fgResetCnt) {
			kalMemZero(au4RxMpduCnt, sizeof(au4RxMpduCnt));
			kalMemZero(au4FcsError, sizeof(au4RxMpduCnt));
			kalMemZero(au4RxFifoCnt, sizeof(au4RxMpduCnt));
			kalMemZero(au4AmpduTxSfCnt, sizeof(au4RxMpduCnt));
			kalMemZero(au4AmpduTxAckSfCnt, sizeof(au4RxMpduCnt));
		}
	}

	/* =========== Group 0x0004 =========== */
	if (u4StatGroup & 0x0004) {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					       i4TotalLen - i4BytesWritten,
			"%s", "----- Last Rx Info (Group 0x04) -----\n");

		/* get Beacon RSSI */
		ucBssIndex = secGetBssIdxByWlanIdx
			(prAdapter, (uint8_t)(prHwWlanInfo->u4Index));

		rStatus = kalIoctlByBssIdx(prAdapter->prGlueInfo,
				   wlanoidQueryRssi,
				   &rLinkSpeed, sizeof(rLinkSpeed),
				   TRUE, TRUE, TRUE,
				   &u4BufLen, ucBssIndex);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(REQ, WARN, "unable to retrieve rssi\n");

		if (IS_BSS_INDEX_VALID(ucBssIndex))
			rRssi = rLinkSpeed.rLq[ucBssIndex].cRssi;

		rSwCtrlInfo.u4Data = 0;
		rSwCtrlInfo.u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID + 1;
#if 0
		rStatus = kalIoctl(prAdapter->prGlueInfo,
				   wlanoidQuerySwCtrlRead, &rSwCtrlInfo,
				   sizeof(rSwCtrlInfo), TRUE, TRUE, TRUE,
				   &u4BufLen);
#endif
		DBGLOG(REQ, LOUD, "rStatus %u, rSwCtrlInfo.u4Data 0x%x\n",
		       rStatus, rSwCtrlInfo.u4Data);
		if (rStatus == WLAN_STATUS_SUCCESS) {
			i2Wf0AvgPwr = rSwCtrlInfo.u4Data & 0xFFFF;
			i2Wf1AvgPwr = (rSwCtrlInfo.u4Data >> 16) & 0xFFFF;

			i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%d %d\n", "NOISE", " = ",
					i2Wf0AvgPwr, i2Wf1AvgPwr);
		}

#ifndef SOC3_0
		/* Last RX Rate */
		i4BytesWritten += nicGetRxRateInfo(prAdapter,
			pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			(uint8_t)(prHwWlanInfo->u4Index));
#endif
		/* Last RX RSSI */
		i4BytesWritten += nicRxGetLastRxRssi(prAdapter,
			pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			(uint8_t)(prHwWlanInfo->u4Index));

		/* Last TX Resp RSSI */
		if (ucNss > 2)
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d %d %d %d\n",
				"Tx Response RSSI", " = ",
				RCPI_TO_dBm(
				    prHwWlanInfo->rWtblRxCounter.ucRxRcpi0),
				RCPI_TO_dBm(
				    prHwWlanInfo->rWtblRxCounter.ucRxRcpi1),
				RCPI_TO_dBm(
				    prHwWlanInfo->rWtblRxCounter.ucRxRcpi2),
				RCPI_TO_dBm(
				    prHwWlanInfo->rWtblRxCounter.ucRxRcpi3));
		else
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d %d\n", "Tx Response RSSI", " = ",
				RCPI_TO_dBm(
				    prHwWlanInfo->rWtblRxCounter.ucRxRcpi0),
				RCPI_TO_dBm(
				    prHwWlanInfo->rWtblRxCounter.ucRxRcpi1));

		/* Last Beacon RSSI */
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "Beacon RSSI", " = ", rRssi);
	}

	/* =========== Group 0x0008 =========== */
	if (u4StatGroup & 0x0008) {
		/* TxV */
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "----- Last TX Info (Group 0x08) -----\n");

		for (ucDbdcIdx = 0; ucDbdcIdx < ENUM_BAND_NUM; ucDbdcIdx++) {
			int8_t txpwr, pos_txpwr;

			txpwr = TX_VECTOR_GET_TX_PWR(
				&prQueryStaStatistics->rTxVector[ucDbdcIdx]);

			if (prAdapter->rWifiVar.fgDbDcModeEn)
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"[DBDC_%d] :\n", ucDbdcIdx);

			i4BytesWritten += nicTxGetVectorInfo(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				&prQueryStaStatistics->rTxVector[ucDbdcIdx]);

			if (prQueryStaStatistics->rTxVector[ucDbdcIdx]
			    .u4TxV[0] == 0xFFFFFFFF)
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "Chip Out TX Power",
					" = ", "N/A");
			else {
				pos_txpwr = (txpwr < 0) ?
					(~txpwr + 1) : (txpwr);
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%c%d.%1d dBm\n",
					"Chip Out TX Power", " = ",
					(txpwr < 0) ? '-' : '+',
					(pos_txpwr / 2),
					5 * (pos_txpwr % 2));
			}

			if (!prAdapter->rWifiVar.fgDbDcModeEn)
				break;
		}
	}

	/* =========== Group 0x0010 =========== */
	if (u4StatGroup & 0x0010) {
		/* RX Reorder */
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "----- RX Reorder (Group 0x10) -----\n");
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%lu\n", "Rx reorder miss", " = ",
			RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_MISS_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%lu\n", "Rx reorder within", " = ",
			RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_WITHIN_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%lu\n", "Rx reorder ahead", " = ",
			RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_AHEAD_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%lu\n", "Rx reorder behind", " = ",
			RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_BEHIND_COUNT));
	}

	/* =========== Group 0x0020 =========== */
	if (u4StatGroup & 0x0020) {
		/* RA info */
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "----- RA Info (Group 0x20) -----\n");
#if 0
		/* Last TX Rate */
		i4BytesWritten += nicGetTxRateInfo(
			pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			FALSE, prHwWlanInfo, prQueryStaStatistics);
#endif
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%-20s%s%d\n", "LinkSpeed",
			" = ", u2LinkSpeed);

		if (!prQueryStaStatistics->ucSkipAr) {
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%s\n", "RateTable", " = ",
				prQueryStaStatistics->ucArTableIdx <
				    (ucRaTableNum - 1) ?
				    RATE_TBLE[
					prQueryStaStatistics->ucArTableIdx] :
					RATE_TBLE[ucRaTableNum - 1]);

			if (wlanGetStaIdxByWlanIdx(prAdapter,
			    (uint8_t)(prHwWlanInfo->u4Index), &ucStaIdx) ==
			    WLAN_STATUS_SUCCESS){
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%d\n", "2G Support 256QAM TX",
					" = ",
					((prAdapter->arStaRec[ucStaIdx].u4Flags
					 & MTK_SYNERGY_CAP_SUPPORT_24G_MCS89) ||
					(prQueryStaStatistics->
					 ucDynamicGband256QAMState == 2)) ?
					1 : 0);
			}
#if 0
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d%%\n", "Rate1 instantPer", " = ",
				u4InstantPer);
#endif
			if (prQueryStaStatistics->ucAvePer == 0xFF) {
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "Train Down", " = ",
					"N/A");

				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "Train Up", " = ",
					"N/A");
			} else {
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%d -> %d\n", "Train Down",
					" = ",
					(uint16_t)
					(prQueryStaStatistics->u2TrainDown
						& BITS(0, 7)),
					(uint16_t)
					((prQueryStaStatistics->u2TrainDown >>
						8) & BITS(0, 7)));

				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%d -> %d\n", "Train Up", " = ",
					(uint16_t)
					(prQueryStaStatistics->u2TrainUp
						& BITS(0, 7)),
					(uint16_t)
					((prQueryStaStatistics->u2TrainUp >> 8)
						& BITS(0, 7)));
			}

			if (prQueryStaStatistics->fgIsForceTxStream == 0)
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "Force Tx Stream",
					" = ", "N/A");
			else
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%d\n", "Force Tx Stream", " = ",
					prQueryStaStatistics->
						fgIsForceTxStream);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "Force SE off", " = ",
				prQueryStaStatistics->fgIsForceSeOff);
#if 0
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%s%d%s%d%s%d%s%d\n", "TxQuality", " = ",
				"KEEP_", prQueryStaStatistics->aucTxQuality[0],
				", UP_", prQueryStaStatistics->aucTxQuality[1],
				", DOWN_", prQueryStaStatistics->
					aucTxQuality[2],
				", BWUP_", prQueryStaStatistics->
					aucTxQuality[3]);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "UpPenalty", " = ",
				prQueryStaStatistics->ucTxRateUpPenalty);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%s\n", "LtMode", " = ",
				prQueryStaStatistics->ucLowTrafficMode <
				(ucRaLtModeNum - 1) ?
				LT_MODE_TBLE[prQueryStaStatistics->
				ucLowTrafficMode] :
				LT_MODE_TBLE[ucRaLtModeNum - 1]);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "LtCnt", " = ",
				prQueryStaStatistics->ucLowTrafficCount);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "LtDashBoard", " = ",
				prQueryStaStatistics->ucLowTrafficDashBoard);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%s\n", "SgiState", " = ",
				prQueryStaStatistics->ucDynamicSGIState <
				(ucRaSgiUnSpStateNum - 1) ?
				SGI_UNSP_STATE_TBLE[prQueryStaStatistics->
				ucDynamicSGIState] :
				SGI_UNSP_STATE_TBLE[ucRaSgiUnSpStateNum - 1]);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "SgiScore", " = ",
				prQueryStaStatistics->ucDynamicSGIScore);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%s\n", "BwState", " = ",
				prQueryStaStatistics->ucDynamicBWState <
				(ucRaBwStateNum - 1) ?
				BW_STATE_TBLE[prQueryStaStatistics->
				ucDynamicBWState] :
				BW_STATE_TBLE[ucRaBwStateNum - 1]);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%s\n", "NonSpState", " = ",
				prQueryStaStatistics->ucDynamicSGIState <
				(ucRaSgiUnSpStateNum - 1) ?
				SGI_UNSP_STATE_TBLE[prQueryStaStatistics->
				ucVhtNonSpRateState] :
				SGI_UNSP_STATE_TBLE[ucRaSgiUnSpStateNum - 1]);
#endif
		}

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "RunningCnt", " = ",
			prQueryStaStatistics->u2RaRunningCnt);

		prQueryStaStatistics->ucRaStatus &= ~0x80;
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%s\n", "Status", " = ",
			prQueryStaStatistics->ucRaStatus < (ucRaStatusNum - 1) ?
			RA_STATUS_TBLE[prQueryStaStatistics->ucRaStatus] :
			RA_STATUS_TBLE[ucRaStatusNum - 1]);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "MaxAF", " = ",
			prHwWlanInfo->rWtblPeerCap.ucAmpduFactor);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s0x%x\n", "SpeIdx", " = ",
			prHwWlanInfo->rWtblPeerCap.ucSpatialExtensionIndex);
#if 0
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "CBRN", " = ",
			prHwWlanInfo->rWtblPeerCap.ucChangeBWAfterRateN);
#endif
		/* Rate1~Rate8 */
		i4BytesWritten += nicGetTxRateInfo(
			pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			TRUE, prHwWlanInfo, prQueryStaStatistics);
	}

	/* =========== Group 0x0040 =========== */
	if (u4StatGroup & 0x0040) {
		uint8_t ucIdx, ucInt;

		au4RangeCtrl[0] = prQueryStaStatistics->u4AggRangeCtrl_0;
		au4RangeCtrl[1] = prQueryStaStatistics->u4AggRangeCtrl_1;
		au4RangeCtrl[2] = prQueryStaStatistics->u4AggRangeCtrl_2;
		au4RangeCtrl[3] = prQueryStaStatistics->u4AggRangeCtrl_3;

		eRangeType = (enum AGG_RANGE_TYPE_T)
					prQueryStaStatistics->ucRangeType;

		for (ucIdx = 0; ucIdx < AGG_RANGE_SEL_NUM; ucIdx++) {
			ucInt = ucIdx >> 2;
			if (ucIdx % 4 == 0)
				aucAggRange[ucIdx] =
					((au4RangeCtrl[ucInt] &
					AGG_RANGE_SEL_0_MASK) >>
					AGG_RANGE_SEL_0_OFFSET);
			else if (ucIdx % 4 == 1)
				aucAggRange[ucIdx] =
					((au4RangeCtrl[ucInt] &
					AGG_RANGE_SEL_1_MASK) >>
					AGG_RANGE_SEL_1_OFFSET);
			else if (ucIdx % 4 == 2)
				aucAggRange[ucIdx] =
					((au4RangeCtrl[ucInt] &
					AGG_RANGE_SEL_2_MASK) >>
					AGG_RANGE_SEL_2_OFFSET);
			else if (ucIdx % 4 == 3)
				aucAggRange[ucIdx] =
					((au4RangeCtrl[ucInt] &
					AGG_RANGE_SEL_3_MASK) >>
					AGG_RANGE_SEL_3_OFFSET);
		}

		/* Tx Agg */
		i4BytesWritten += kalScnprintf(
			pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s%s%s", "------ ",
			(eRangeType > ENUM_AGG_RANGE_TYPE_TX) ? (
				(eRangeType == ENUM_AGG_RANGE_TYPE_TRX) ?
				("TRX") : ("RX")) : ("TX"),
				" AGG (Group 0x40) -----\n");

		if (eRangeType == ENUM_AGG_RANGE_TYPE_TRX) {
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-6s%8d%5d%1s%2d%5d%1s%2d%5d%1s%2d%5d%1s%2d%5d%1s%2d%5d%1s%2d%5d%3s",
				" TX  :", aucAggRange[0] + 1,
				aucAggRange[0] + 2, "~", aucAggRange[1] + 1,
				aucAggRange[1] + 2, "~", aucAggRange[2] + 1,
				aucAggRange[2] + 2, "~", aucAggRange[3] + 1,
				aucAggRange[3] + 2, "~", aucAggRange[4] + 1,
				aucAggRange[4] + 2, "~", aucAggRange[5] + 1,
				aucAggRange[5] + 2, "~", aucAggRange[6] + 1,
				aucAggRange[6] + 2, "~256\n");

			for (ucDbdcIdx = 0; ucDbdcIdx < ENUM_BAND_NUM;
			     ucDbdcIdx++) {
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"DBDC%d:", ucDbdcIdx);
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%8d%8d%8d%8d%8d%8d%8d%8d\n",
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[0],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[1],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[2],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[3],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[4],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[5],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[6],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[7]);

				if (!prAdapter->rWifiVar.fgDbDcModeEn)
					break;
			}

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-6s%8d%5d%1s%2d%5d%1s%2d%5d%1s%2d%5d%1s%2d%5d%1s%2d%5d%1s%2d%5d%3s",
				" RX  :", aucAggRange[7] + 1,
				aucAggRange[7] + 2, "~", aucAggRange[8] + 1,
				aucAggRange[8] + 2, "~", aucAggRange[9] + 1,
				aucAggRange[9] + 2, "~", aucAggRange[10] + 1,
				aucAggRange[10] + 2, "~", aucAggRange[11] + 1,
				aucAggRange[11] + 2, "~", aucAggRange[12] + 1,
				aucAggRange[12] + 2, "~", aucAggRange[13] + 1,
				aucAggRange[13] + 2, "~256\n");

			for (ucDbdcIdx = 0; ucDbdcIdx < ENUM_BAND_NUM;
			     ucDbdcIdx++) {
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"DBDC%d:", ucDbdcIdx);
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%8d%8d%8d%8d%8d%8d%8d%8d\n",
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[8],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[9],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[10],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[11],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[12],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[13],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[14],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[15]);

				if (!prAdapter->rWifiVar.fgDbDcModeEn)
					break;
			}
		} else {
			for (ucDbdcIdx = 0; ucDbdcIdx < ENUM_BAND_NUM;
			     ucDbdcIdx++) {
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"DBDC%d:\n", ucDbdcIdx);
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-6s%8d%6d%1s%2d%6d%1s%2d%6d%1s%2d%6d%1s%2d%6d%1s%2d%6d%1s%2d%6d%1s%2d\n",
					"Range:", aucAggRange[0] + 1,
					aucAggRange[0] + 2, "~",
					aucAggRange[1] + 1,
					aucAggRange[1] + 2, "~",
					aucAggRange[2] + 1,
					aucAggRange[2] + 2, "~",
					aucAggRange[3] + 1,
					aucAggRange[3] + 2, "~",
					aucAggRange[4] + 1,
					aucAggRange[4] + 2, "~",
					aucAggRange[5] + 1,
					aucAggRange[5] + 2, "~",
					aucAggRange[6] + 1,
					aucAggRange[6] + 2, "~",
					aucAggRange[7] + 1);

				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%14d%9d%9d%9d%9d%9d%9d%9d\n",
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[0],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[1],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[2],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[3],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[4],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[5],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[6],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[7]);

				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-6s%4d%1s%2d%5d%1s%2d%5d%1s%2d%5d%1s%2d%5d%1s%2d%5d%1s%2d%5d%1s%2d%5d%3s",
					"Range:", aucAggRange[7] + 2, "~",
					aucAggRange[8] + 1,
					aucAggRange[8] + 2, "~",
					aucAggRange[9] + 1,
					aucAggRange[9] + 2, "~",
					aucAggRange[10] + 1,
					aucAggRange[10] + 2, "~",
					aucAggRange[11] + 1,
					aucAggRange[11] + 2, "~",
					aucAggRange[12] + 1,
					aucAggRange[12] + 2, "~",
					aucAggRange[13] + 1,
					aucAggRange[13] + 2, "~",
					aucAggRange[14] + 1,
					aucAggRange[14] + 2, "~256\n");

				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%14d%9d%9d%9d%9d%9d%9d%9d\n",
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[8],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[9],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[10],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[11],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[12],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[13],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[14],
					g_arMibInfo[ucDbdcIdx].
					au2TxRangeAmpduCnt[15]);

				if (!prAdapter->rWifiVar.fgDbDcModeEn)
					break;
			}
		}
	}

	kalMemZero(g_arMibInfo, sizeof(g_arMibInfo));

	return i4BytesWritten;
}

#endif /* CFG_SUPPORT_CONNAC2X */
