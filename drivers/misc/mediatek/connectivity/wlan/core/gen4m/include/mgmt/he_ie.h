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
#ifndef _HE_IE_H
#define _HE_IE_H

#if (CFG_SUPPORT_802_11AX == 1)

/* HE Capabilities */
#define ELEM_EXT_ID_HE_CAP               35
/* HE Operation */
#define ELEM_EXT_ID_HE_OP                36
/* UL OFDMA-based Random Access (UORA) Parameter Set element */
#define ELEM_EXT_ID_UORA_PARAM           37
/* MU EDCA Parameter Set element */
#define ELEM_EXT_ID_MU_EDCA_PARAM        38
/* Spatial Reuse Parameter Set element */
#define ELEM_EXT_ID_SR_PARAM             39

#define ELEM_EXT_CAP_TWT_REQUESTER_SUPP_BIT       77
#define ELEM_EXT_CAP_TWT_RESPONDER_SUPP_BIT       78

#define HE_PHY_CAP_INFO_LEN              9

/* HE CAP - HE MAC Capabilities Information field */
#define HE_MAC_CAP_BYTE_NUM                            6

/* HE MAC Capablilites byte0 */
#define HE_MAC_CAP0_HTC_HE                             BIT(0)
#define HE_MAC_CAP0_HTC_HE_SHFT                        0
#define HE_MAC_CAP0_TWT_REQ                            BIT(1)
#define HE_MAC_CAP0_TWT_REQ_SHFT                       1
#define HE_MAC_CAP0_TWT_RSP                            BIT(2)
#define HE_MAC_CAP0_TWT_RSP_SHFT                       2
#define HE_MAC_CAP0_FRAGMENTATION_SHFT                 3
#define HE_MAC_CAP0_MAX_NUM_OF_FRAGMENTATION_SHFT      5

/* HE MAC Capablilites byte1 */
#define HE_MAC_CAP1_MIN_FRAGMENT_SIZE_SHFT             0
#define HE_MAC_CAP1_TRIGGER_PAD_DURATION_MASK          BITS(2, 3)
#define HE_MAC_CAP1_TRIGGER_PAD_DURATION_SHFT          2
#define HE_MAC_CAP1_MULTI_TID_AGG_RX_SHFT              4
#define HE_MAC_CAP1_LINK_ADP_SHFT                      7

/* HE MAC Capablilites byte2 */
#define HE_MAC_CAP2_LINK_ADP_SHFT                      0
#define HE_MAC_CAP2_ALL_ACK_SHFT                       1
#define HE_MAC_CAP2_TRS_SHFT                           2
#define HE_MAC_CAP2_BSR_SHFT                           3
#define HE_MAC_CAP2_BTWT_SHFT                          4
#define HE_MAC_CAP2_32BIT_BA_BITMAP_SHFT               5
#define HE_MAC_CAP2_MU_CASCAD_SHFT                     6
#define HE_MAC_CAP2_ACK_ENABLED_AGG_SHFT               7

/* HE MAC Capablilites byte3 */
#define HE_MAC_CAP3_RESERVED                           0
#define HE_MAC_CAP3_OM_CTRL                            BIT(1)
#define HE_MAC_CAP3_OM_CTRL_SHFT                       1
#define HE_MAC_CAP3_OFDMA_RA_SHFT                      2
#define HE_MAC_CAP3_MAX_AMPDU_LEN_EXP_SHFT             3
#define HE_MAC_CAP3_AMSDU_FRAGMENT_SHFT                5
#define HE_MAC_CAP3_FLEXIBLE_TWT_SHDL                  BIT(6)
#define HE_MAC_CAP3_FLEXIBLE_TWT_SHDL_SHFT             6
#define HE_MAC_CAP3_RX_CTRL_TO_MUTI_BSS_SHFT           7

/* HE MAC Capablilites byte4 */
#define HE_MAC_CAP4_BSRP_BQRP_AMPDU_AGG_SHFT           0
#define HE_MAC_CAP4_QTP_SHFT                           1
#define HE_MAC_CAP4_BQR_SHFT                           2
#define HE_MAC_CAP4_SRP_RSP_SHFT                       3
#define HE_MAC_CAP4_NDP_FEEDBACK_REPORT_SHFT           4
#define HE_MAC_CAP4_OPS_SHFT                           5
#define HE_MAC_CAP4_AMSDU_IN_AMDU                      BIT(6)
#define HE_MAC_CAP4_AMSDU_IN_AMDU_SHFT                 6
#define HE_MAC_CAP4_MULTI_TID_AGG_TX                   7

/* HE MAC Capablilites byte5 */
#define HE_MAC_CAP5_MULTI_TID_AGG_TX_MASK              BITS(0, 1)
#define HE_MAC_CAP5_MULTI_TID_AGG_TX_SHFT              0
#define HE_MAC_CAP5_SUBCHANNEL_SEL_TX_SHFT             2
#define HE_MAC_CAP5_UL_2X996TONE_RU_SHFT               3
#define HE_MAC_CAP5_OM_CNTL_ULMUDATE_DISABLE_RX_SHFT   4

/* HE CAP - HE PHY Capabilities Information field */
#define HE_PHY_CAP_BYTE_NUM                            11

/* HE PHY Capablilites byte0 */
#define HE_PHY_CAP0_RESERVED                           0
#define HE_PHY_CAP0_CHAN_WIDTH_SET_BW40_2G             BIT(1)
#define HE_PHY_CAP0_CHAN_WIDTH_SET_BW40_BW80_5G        BIT(2)
#define HE_PHY_CAP0_CHAN_WIDTH_SET_BW160_5G            BIT(3)
#define HE_PHY_CAP0_CHAN_WIDTH_SET_BW80P80_5G          BIT(4)
#define HE_PHY_CAP0_CHAN_WIDTH_SET_242TONE_2G          BIT(5)
#define HE_PHY_CAP0_CHAN_WIDTH_SET_242TONE_5G          BIT(6)
#define HE_PHY_CAP0_CHAN_WIDTH_SET_MASK                BITS(1, 7)
#define HE_PHY_CAP0_CHAN_WIDTH_SET_SHFT                1

/* HE PHY Capablilites byte1 */
#define HE_PHY_CAP1_PUNCTURED_PREAMBLE_RX_SHFT         0
#define HE_PHY_CAP1_DEVICE_CALSS_SHFT                  4
#define HE_PHY_CAP1_LDPC_CODING_IN_PAYLOAD             BIT(5)
#define HE_PHY_CAP1_LDPC_CODING_IN_PAYLOAD_SHFT        5
#define HE_PHY_CAP1_1X_HE_LTF_SHFT                     6
#define HE_PHY_CAP1_MIDAMBLE_TXRX_MAX_NSTS_SHFT        7


/* HE PHY Capablilites byte2 */
#define HE_PHY_CAP2_MIDAMBLE_TXRX_MAX_NSTS_SHFT        0
#define HE_PHY_CAP2_NDP_4X_HE_LTF_SHFT                 1
#define HE_PHY_CAP2_STBC_TX_LT_OR_EQ_80M               BIT(2)
#define HE_PHY_CAP2_STBC_TX_LT_OR_EQ_80M_SHFT          2
#define HE_PHY_CAP2_STBC_RX_LT_OR_EQ_80M               BIT(3)
#define HE_PHY_CAP2_STBC_RX_LT_OR_EQ_80M_SHFT          3
#define HE_PHY_CAP2_DOPPLER_TX_SHFT                    4
#define HE_PHY_CAP2_DOPPLER_RX_SHFT                    5
#define HE_PHY_CAP2_FULL_BW_UL_MU_MIMO                 BIT(6)
#define HE_PHY_CAP2_FULL_BW_UL_MU_MIMO_SHFT            6
#define HE_PHY_CAP2_PARTIAL_BW_UL_MU_MIMO              BIT(7)
#define HE_PHY_CAP2_PARTIAL_BW_UL_MU_MIMO_SHFT         7

/* HE PHY Capablilites byte3 */
#define HE_PHY_CAP3_DCM_MAX_CONSTELLATION_TX_SHFT      0
#define HE_PHY_CAP3_DCM_MAX_CONSTELLATION_TX_MASK	   BITS(0, 1)
#define HE_PHY_CAP3_DCM_MAX_NSS_TX_SHFT                2
#define HE_PHY_CAP3_DCM_MAX_CONSTELLATION_RX_SHFT      3
#define HE_PHY_CAP3_DCM_MAX_CONSTELLATION_RX_MASK	   BITS(3, 4)
#define HE_PHY_CAP3_DCM_MAX_NSS_RX_SHFT                5
#define HE_PHY_CAP3_UL_HE_MU_PPDU_SHFT                 6
#define HE_PHY_CAP3_SU_BFMER                           BIT(7)
#define HE_PHY_CAP3_SU_BFMER_SHFT                      7

/* HE PHY Capablilites byte4 */
#define HE_PHY_CAP4_SU_BFMEE                           BIT(0)
#define HE_PHY_CAP4_SU_BFMEE_SHFT                      0
#define HE_PHY_CAP4_MU_BFMER                           BIT(1)
#define HE_PHY_CAP4_MU_BFMER_SHFT                      1
#define HE_PHY_CAP4_BFMEE_STS_LT_OR_EQ_80M_MASK        BITS(2, 4)
#define HE_PHY_CAP4_BFMEE_STS_LT_OR_EQ_80M_SHFT        2
#define HE_PHY_CAP4_BFMEE_STS_GT_80M_SHFT              5

/* HE PHY Capablilites byte5 */
#define HE_PHY_CAP5_NUM_OF_SND_DIM_LT_OR_EQ_80M_SHFT   0
#define HE_PHY_CAP5_NUM_OF_SND_DIM_GT_80M_SHFT         3
#define HE_PHY_CAP5_NG_16_SU_FB                        BIT(6)
#define HE_PHY_CAP5_NG_16_SU_FB_SHFT                   6
#define HE_PHY_CAP5_NG_16_MU_FB                        BIT(7)
#define HE_PHY_CAP5_NG_16_MU_FB_SHFT                   7

/* HE PHY Capablilites byte6 */
#define HE_PHY_CAP6_CODE_BOOK_4_2_SU_FB                BIT(0)
#define HE_PHY_CAP6_CODE_BOOK_4_2_SU_FB_SHFT           0
#define HE_PHY_CAP6_CODE_BOOK_7_5_MU_FB                BIT(1)
#define HE_PHY_CAP6_CODE_BOOK_7_5_MU_FB_SHFT           1
#define HE_PHY_CAP6_TRIG_SU_BF_FB                      BIT(2)
#define HE_PHY_CAP6_TRIG_SU_BF_FB_SHFT                 2
#define HE_PHY_CAP6_TRIG_MU_BF_PARTIAL_BW_FB           BIT(3)
#define HE_PHY_CAP6_TRIG_MU_BF_PARTIAL_BW_FB_SHFT      3
#define HE_PHY_CAP6_TRIG_CQI_FB_SHFT                   4
#define HE_PHY_CAP6_PARTIAL_BW_EXTENDED_RANGE          BIT(5)
#define HE_PHY_CAP6_PARTIAL_BW_EXTENDED_RANGE_SHFT     5
#define HE_PHY_CAP6_PARTIAL_BW_DL_MU_MIMO_SHFT         6
#define HE_PHY_CAP6_PPE_THRESHOLD                      BIT(7)
#define HE_PHY_CAP6_PPE_THRESHOLD_SHFT                 7

/* HE PHY Capablilites byte7 */
#define HE_PHY_CAP7_SRP_BASED_SR_SHFT                  0
#define HE_PHY_CAP7_POWER_BOOST_FACTOR_SHFT            1
#define HE_PHY_CAP7_SU_MU_4X_HE_LTF_SHFT               2
#define HE_PHY_CAP7_MAX_NC_MASK                        BITS(3, 5)
#define HE_PHY_CAP7_MAX_NC_SHFT                        3
#define HE_PHY_CAP7_STBC_TX_GT_80M                     BIT(6)
#define HE_PHY_CAP7_STBC_TX_GT_80M_SHFT                6
#define HE_PHY_CAP7_STBC_RX_GT_80M                     BIT(7)
#define HE_PHY_CAP7_STBC_RX_GT_80M_SHFT                7

/* HE PHY Capablilites byte8 */
#define HE_PHY_CAP8_ER_SU_4X_HE_LTF                    BIT(0)
#define HE_PHY_CAP8_ER_SU_4X_HE_LTF_SHFT               0
#define HE_PHY_CAP8_20M_IN_40M_HE_PPDU_2G_SHFT         1
#define HE_PHY_CAP8_20M_IN_160M_HE_PPDU_SHFT           2
#define HE_PHY_CAP8_80M_IN_160M_HE_PPDU_SHFT           3
#define HE_PHY_CAP8_ER_SU_PPDU_1X_HE_LTF               BIT(4)
#define HE_PHY_CAP8_ER_SU_PPDU_1X_HE_LTF_SHFT          4
#define HE_PHY_CAP8_MIDAMBLE_RX_2X_1X_HE_LTF_SHFT      5
#define HE_PHY_CAP8_DCM_MAX_BW_SHFT                    6

/* HE PHY Capablilites byte9 */
#define HE_PHY_CAP9_LT_16_SIGB_OFDM_SYMBOL_SHFT        0
#define HE_PHY_CAP9_NON_TRIGGER_CQI_FB_SHFT            1
#define HE_PHY_CAP9_TX_1024_QAM_LESS_242_RU_SHFT       2
#define HE_PHY_CAP9_RX_1024_QAM_LESS_242_RU_SHFT       3
#define HE_PHY_CAP9_RX_FULL_BW_COMPRESS_SIGB_SHFT      4
#define HE_PHY_CAP9_RX_FULL_BW_NONCOMPRESS_SIGB_SHFT   5

/* HE PHY Capablilites byte10 */
/* ALL reserved */

/* Trigger Frame MAC Padding Duration */
#define HE_CAP_TRIGGER_PAD_DURATION_0                  0
#define HE_CAP_TRIGGER_PAD_DURATION_8                  1
#define HE_CAP_TRIGGER_PAD_DURATION_16                 2

#define HE_CAP_INFO_MCS_MAP_MCS7                       0
#define HE_CAP_INFO_MCS_MAP_MCS9                       1
#define HE_CAP_INFO_MCS_MAP_MCS11                      2
#define HE_CAP_INFO_MCS_NOT_SUPPORTED                  3

/* PPE Threshold Field */
#define HE_CAP_PPE_NSS                                 BITS(0, 2)
#define HE_CAP_PPE_NSS_SHFT                            0
#define HE_CAP_PPE_RU_IDX_BMP                          BITS(3, 6)
#define HE_CAP_PPE_RU_IDX_BMP_SHFT                     3
#define HE_CAP_PPE_PPET16_NSS1_RU0                     BITS(7, 9)
#define HE_CAP_PPE_PPET16_NSS1_RU0_SHFT                7
#define HE_CAP_PPE_PPET8_NSS1_RU0                      BITS(10, 12)
#define HE_CAP_PPE_PPET8_NSS1_RU0_SHFT                 10

#define HE_CAP_PPE_242_RU_IDX                          BIT(3)
#define HE_CAP_PPE_484_RU_IDX                          BITS(3, 4)
#define HE_CAP_PPE_996_RU_IDX                          BITS(3, 5)
#define HE_CAP_PPE_996X2_RU_IDX                        BITS(3, 6)

#define CONSTELL_IDX_BPSK                              0
#define CONSTELL_IDX_QPSK                              1
#define CONSTELL_IDX_16QAM                             2
#define CONSTELL_IDX_64QAM                             3
#define CONSTELL_IDX_256QAM                            4
#define CONSTELL_IDX_1024QAM                           5
#define CONSTELL_IDX_RESERVED                          6
#define CONSTELL_IDX_NONE                              7

/* HE Operation element - HE Operation Parameters field */
#define HE_OP_BYTE_NUM                                 3

/* HE Operation Parameters - byte0 */
#define HE_OP_PARAM0_DEFAULT_PE_DUR_MASK                BITS(0, 2)
#define HE_OP_PARAM0_DEFAULT_PE_DUR_SHFT                0
#define HE_OP_PARAM0_TWT_REQUIRED_SHFT                  3
#define HE_OP_PARAM0_TXOP_DUR_RTS_THRESHOLD_MASK        BITS(4, 7)
#define HE_OP_PARAM0_TXOP_DUR_RTS_THRESHOLD_SHFT        4

/* HE Operation Parameters - byte1 */
#define HE_OP_PARAM1_TXOP_DUR_RTS_THRESHOLD_MASK        BITS(0, 5)
#define HE_OP_PARAM1_TXOP_DUR_RTS_THRESHOLD_SHFT        0
#define HE_OP_PARAM1_VHT_OP_INFO_PRESENT                BIT(6)
#define HE_OP_PARAM1_VHT_OP_INFO_PRESENT_SHFT           6
#define HE_OP_PARAM1_CO_LOCATED_BSS_SHFT                7

/* HE Operation Parameters - byte2 */
#define HE_OP_PARAM2_ER_SU_DISABLE_MASK					BIT(0)
#define HE_OP_PARAM2_ER_SU_DISABLE_SHFT                 0

/* HE Operation element - BSS Color Information */
#define HE_OP_BSSCOLOR_BSS_COLOR_SHFT                   0
#define HE_OP_BSSCOLOR_PARTIAL_BSS_COLOR_SHFT           6
#define HE_OP_BSSCOLOR_BSS_COLOR_DISABLE_SHFT           7

/* Spatial Reuse Parameter Set element - SR Control field */
#define SR_PARAM_SRP_DISALLOWED                        BIT(0)
#define SR_PARAM_SRP_DISALLOWED_SHFT                   0
#define SR_PARAM_NON_SRG_OBSS_PD_SR_DISALLOWED         BIT(1)
#define SR_PARAM_NON_SRG_OBSS_PD_SR_DISALLOWED_SHFT    1
#define SR_PARAM_NON_SRG_OFFSET_PRESENT                BIT(2)
#define SR_PARAM_NON_SRG_OFFSET_PRESENT_SHFT           2
#define SR_PARAM_SRG_INFO_PRESENT                      BIT(3)
#define SR_PARAM_SRG_INFO_PRESENT_SHFT                 3
#define SR_PARAM_HESIGA_SR_VALUE15_ALLOWED             BIT(4)
#define SR_PARAM_HESIGA_SR_VALUE15_SHFT                4

/* 11ax_D3.0 9.2.4.6 HT Control field */
#define HTC_HE_VARIANT                                 BITS(0, 1)
/* 11ax_D3.0 9.2.4.6.4 A-Control */
#define HTC_HE_A_CTRL_TRS                              0
#define HTC_HE_A_CTRL_OM                               1
#define HTC_HE_A_CTRL_HLA                              2
#define HTC_HE_A_CTRL_BSR                              3
#define HTC_HE_A_CTRL_UPH                              4
#define HTC_HE_A_CTRL_BQR                              5
#define HTC_HE_A_CTRL_CAS                              6

/* 1st - OM_Ctrl, 2nd - UPH_Ctrl */
#define HTC_HE_1ST_A_CTRL_ID                           BITS(2, 5)
#define HTC_HE_1ST_A_CTRL_ID_SHIFT                      2

/* 11ax_D3.0 9.2.4.6a.2 OM Control */
#define HTC_HE_OM_RX_NSS                               BITS(6, 8)
#define HTC_HE_OM_RX_NSS_SHFT                          6
#define HTC_HE_OM_CH_WIDTH                             BITS(9, 10)
#define HTC_HE_OM_CH_WIDTH_SHFT                        9
#define HTC_HE_OM_UL_MU_DISABLE                        BIT(11)
#define HTC_HE_OM_UL_MU_DISABLE_SHFT                   11
#define HTC_HE_OM_TX_NSTS                              BITS(12, 14)
#define HTC_HE_OM_TX_NSTS_SHFT                         12
#define HTC_HE_OM_ER_SU_DISABLE                        BIT(15)
#define HTC_HE_OM_ER_SU_DISABLE_SHFT                   15
#define HTC_HE_OM_DL_MUMIMO_RESND_RECMD                BIT(16)
#define HTC_HE_OM_DL_MUMIMO_RESND_RECMD_SHFT           16
#define HTC_HE_OM_UL_MU_DATA_DISABLE                   BIT(17)
#define HTC_HE_OM_UL_MU_DATA_DISABLE_SHFT              17

/* 1st - OM_CTRL, 2nd - UPH_CTRL */
#define HTC_HE_2ND_A_CTRL_ID                           BITS(18, 21)
#define HTC_HE_2ND_A_CTRL_ID_SHIFT                     18

/* 11ax_D3.0 9.2.4.6a.5 UPH Control */
#define HTC_HE_UPH_UL_PWR_HEADROOM                     BITS(22, 27)
#define HTC_HE_UPH_UL_PWR_HEADROOM_SHIFT               22
#define HTC_HE_UPH_MIN_TX_PWR_FLAG                     BIT(28)
#define HTC_HE_UPH_MIN_TX_PWR_FLAG_SHIFT               28

enum ENUM_HTC_HE_OM_CH_WIDTH  {
	CH_BW_20 = 0,
	CH_BW_40 = 1,
	CH_BW_80 = 2,
	CH_BW_160 = 3,
};

/* 11ax_D3.0 9.3.1.9 BlockAck frame format */
#define HE_BA_TYPE                                     BITS(1, 4)
#define HE_BA_TYPE_SHFT                                1
enum ENUM_HEBA_TYPE {
	HE_BA_TYPE_BASIC = 0,          /* 0 Basic */
	HE_BA_TYPE_EXT_COMPRESSED = 1, /* 1 Extended Compressed */
	HE_BA_TYPE_COMPRESSED = 2,     /* 2 Compressed */
	HE_BA_TYPE_MULTI_TID = 3,      /* 3 Multi-TID */
	/* 4-5 Reserved */
	HE_BA_TYPE_GCR = 6,            /* 6 GCR */
	/* 7-9 Reserved */
	HE_BA_TYPE_GLK_GCR = 10,       /* 10 GLK-GCR */
	HE_BA_TYPE_MULTI_STA = 11,     /* 11 Multi-STA */
	/* 12-15 Reserved */
};

/* should use macro to access field of HE MAC CAP*/
#define HE_SET_MAC_CAP_HTC_HE(_aucHeMacCapInfo) \
	(_aucHeMacCapInfo[0] |= HE_MAC_CAP0_HTC_HE)

#define HE_SET_MAC_CAP_TWT_REQ(_aucHeMacCapInfo) \
	(_aucHeMacCapInfo[0] |=  HE_MAC_CAP0_TWT_REQ)

#define HE_IS_MAC_CAP_TWT_RSP(_aucHeMacCapInfo) \
	(_aucHeMacCapInfo[0] & HE_MAC_CAP0_TWT_RSP)

#define HE_SET_MAC_CAP_TRIGGER_PAD_DURATION(_aucHeMacCapInfo, _ucDur) \
{ \
	_aucHeMacCapInfo[1] &= ~(HE_MAC_CAP1_TRIGGER_PAD_DURATION_MASK); \
	_aucHeMacCapInfo[1] |= \
		((_ucDur << HE_MAC_CAP1_TRIGGER_PAD_DURATION_SHFT) \
			& HE_MAC_CAP1_TRIGGER_PAD_DURATION_MASK); \
}

#define HE_IS_MAC_CAP_FLEXIBLE_TWT_SHDL(_aucHeMacCapInfo) \
	(_aucHeMacCapInfo[3] & HE_MAC_CAP3_FLEXIBLE_TWT_SHDL)


#define HE_SET_MAC_CAP_OM_CTRL(_aucHeMacCapInfo) \
	(_aucHeMacCapInfo[3] |=  HE_MAC_CAP3_OM_CTRL)

/* should use macro to access field of HE PHY CAP*/
#define HE_SET_PHY_CAP_CHAN_WIDTH_SET_BW40_2G(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[0] |=  HE_PHY_CAP0_CHAN_WIDTH_SET_BW40_2G)

#define HE_SET_PHY_CAP_CHAN_WIDTH_SET_BW40_BW80_5G(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[0] |=  HE_PHY_CAP0_CHAN_WIDTH_SET_BW40_BW80_5G)

#define HE_IS_PHY_CAP_CHAN_WIDTH_SET_BW160_5G(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[0] & HE_PHY_CAP0_CHAN_WIDTH_SET_BW160_5G)

#define HE_SET_PHY_CAP_CHAN_WIDTH_SET_BW160_5G(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[0] |= HE_PHY_CAP0_CHAN_WIDTH_SET_BW160_5G)

#define HE_IS_PHY_CAP_CHAN_WIDTH_SET_BW80P80_5G(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[0] & HE_PHY_CAP0_CHAN_WIDTH_SET_BW80P80_5G)

#define HE_SET_PHY_CAP_CHAN_WIDTH_SET_BW80P80_5G(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[0] |= HE_PHY_CAP0_CHAN_WIDTH_SET_BW80P80_5G)

#define HE_SET_PHY_CAP_LDPC_CODING_IN_PAYLOAD(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[1] |= HE_PHY_CAP1_LDPC_CODING_IN_PAYLOAD)

#define HE_SET_PHY_CAP_STBC_TX_LT_OR_EQ_80M(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[2] |= HE_PHY_CAP2_STBC_TX_LT_OR_EQ_80M)

#define HE_SET_PHY_CAP_STBC_RX_LT_OR_EQ_80M(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[2] |= HE_PHY_CAP2_STBC_RX_LT_OR_EQ_80M)

#define HE_UNSET_PHY_CAP_SU_BFMER(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[3] &= ~HE_PHY_CAP3_SU_BFMER)

/* set to 0 if DCM is not supported */
/* set to 1 for BPSK */
/* set to 2 for QPSK */
/* set to 3 for 16-QAM */
#define HE_SET_PHY_CAP_DCM_MAX_CONSTELLATION_TX(_aucHePhyCapInfo, _ucVal) \
{ \
	_aucHePhyCapInfo[3] &= ~(HE_PHY_CAP3_DCM_MAX_CONSTELLATION_TX_MASK); \
	_aucHePhyCapInfo[3] |= \
		((_ucVal << HE_PHY_CAP3_DCM_MAX_CONSTELLATION_TX_SHFT) \
			& HE_PHY_CAP3_DCM_MAX_CONSTELLATION_TX_MASK); \
}

/* set to 0 if DCM is not supported */
/* set to 1 for BPSK */
/* set to 2 for QPSK */
/* set to 3 for 16-QAM */
#define HE_SET_PHY_CAP_DCM_MAX_CONSTELLATION_RX(_aucHePhyCapInfo, _ucVal) \
{ \
	_aucHePhyCapInfo[3] &= ~(HE_PHY_CAP3_DCM_MAX_CONSTELLATION_RX_MASK); \
	_aucHePhyCapInfo[3] |= \
		((_ucVal << HE_PHY_CAP3_DCM_MAX_CONSTELLATION_RX_SHFT) \
			& HE_PHY_CAP3_DCM_MAX_CONSTELLATION_RX_MASK); \
}

#define HE_GET_PHY_CAP_DCM_MAX_CONSTELLATION_TX(_aucHePhyCapInfo) \
	((_aucHePhyCapInfo[3] & HE_PHY_CAP3_DCM_MAX_CONSTELLATION_TX_MASK) \
	>> HE_PHY_CAP3_DCM_MAX_CONSTELLATION_TX_SHFT)

#define HE_GET_PHY_CAP_DCM_MAX_CONSTELLATION_RX(_aucHePhyCapInfo) \
	((_aucHePhyCapInfo[3] & HE_PHY_CAP3_DCM_MAX_CONSTELLATION_RX_MASK) \
	>> HE_PHY_CAP3_DCM_MAX_CONSTELLATION_RX_SHFT)

#define HE_SET_PHY_CAP_SU_BFMEE(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[4] |= HE_PHY_CAP4_SU_BFMEE)

#define HE_UNSET_PHY_CAP_MU_BFMER(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[4] &= ~HE_PHY_CAP4_MU_BFMER)

#define HE_SET_PHY_CAP_BFMEE_STS_LT_OR_EQ_80M(_aucHePhyCapInfo, _ucSts) \
{ \
	_aucHePhyCapInfo[4] &= ~(HE_PHY_CAP4_BFMEE_STS_LT_OR_EQ_80M_MASK); \
	_aucHePhyCapInfo[4] |= \
		((_ucSts << HE_PHY_CAP4_BFMEE_STS_LT_OR_EQ_80M_SHFT) \
			& HE_PHY_CAP4_BFMEE_STS_LT_OR_EQ_80M_MASK); \
}

#define HE_SET_PHY_CAP_NG_16_SU_FB(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[5] |= HE_PHY_CAP5_NG_16_SU_FB)

#define HE_SET_PHY_CAP_NG_16_MU_FB(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[5] |= HE_PHY_CAP5_NG_16_MU_FB)

#define HE_SET_PHY_CAP_CODE_BOOK_4_2_SU_FB(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[6] |= HE_PHY_CAP6_CODE_BOOK_4_2_SU_FB)

#define HE_SET_PHY_CAP_CODE_BOOK_7_5_MU_FB(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[6] |= HE_PHY_CAP6_CODE_BOOK_7_5_MU_FB)

#define HE_SET_PHY_CAP_TRIG_SU_BF_FB(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[6] |= HE_PHY_CAP6_TRIG_SU_BF_FB)

#define HE_SET_PHY_CAP_TRIG_MU_BF_PARTIAL_BW_FB(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[6] |= HE_PHY_CAP6_TRIG_MU_BF_PARTIAL_BW_FB)

#define HE_SET_PHY_CAP_HE_PHY_CAP6_TRIG_MU_BF_PARTIAL_BW_FB(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[6] |= HE_PHY_CAP6_TRIG_MU_BF_PARTIAL_BW_FB)

#define HE_SET_PHY_CAP_PARTIAL_BW_EXTENDED_RANGE(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[6] |= HE_PHY_CAP6_PARTIAL_BW_EXTENDED_RANGE)

#define HE_GET_PHY_CAP_PARTIAL_BW_EXTENDED_RANGE(_aucHePhyCapInfo) \
	((_aucHePhyCapInfo[6] & HE_PHY_CAP6_PARTIAL_BW_EXTENDED_RANGE) \
		>> HE_PHY_CAP6_PARTIAL_BW_EXTENDED_RANGE_SHFT)

#define HE_IS_PHY_CAP_PPE_THRESHOLD(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[6] & HE_PHY_CAP6_PPE_THRESHOLD)

#define HE_SET_PHY_CAP_MAX_NC(_aucHePhyCapInfo, _ucMaxNc) \
{ \
	_aucHePhyCapInfo[7] &= ~(HE_PHY_CAP7_MAX_NC_MASK); \
	_aucHePhyCapInfo[7] |= ((_ucMaxNc << HE_PHY_CAP7_MAX_NC_SHFT) \
		& HE_PHY_CAP7_MAX_NC_MASK); \
}

#define HE_SET_PHY_CAP_STBC_TX_GT_80M(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[7] |= HE_PHY_CAP7_STBC_TX_GT_80M)

#define HE_SET_PHY_CAP_STBC_RX_GT_80M(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[7] |= HE_PHY_CAP7_STBC_RX_GT_80M)

#define HE_SET_PHY_CAP_ER_SU_4X_HE_LTF(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[8] |= HE_PHY_CAP8_ER_SU_4X_HE_LTF)

#define HE_GET_PHY_CAP_ER_SU_4X_HE_LTF(_aucHePhyCapInfo) \
	((_aucHePhyCapInfo[8] & HE_PHY_CAP8_ER_SU_4X_HE_LTF) \
		>> HE_PHY_CAP8_ER_SU_4X_HE_LTF_SHFT)

#define HE_SET_PHY_CAP_ER_SU_PPDU_1X_HE_LTF(_aucHePhyCapInfo) \
	(_aucHePhyCapInfo[8] |= HE_PHY_CAP8_ER_SU_PPDU_1X_HE_LTF)

#define HE_GET_PHY_CAP_ER_SU_PPDU_1X_HE_LTF(_aucHePhyCapInfo) \
	((_aucHePhyCapInfo[8] & HE_PHY_CAP8_ER_SU_PPDU_1X_HE_LTF) \
		>> HE_PHY_CAP8_ER_SU_PPDU_1X_HE_LTF_SHFT)

/* should use macro to access field of HE OP*/
#define HE_IS_VHT_OP_INFO_PRESENT(_aucHeOpParams) \
	(_aucHeOpParams[1] |=  HE_OP_PARAM1_VHT_OP_INFO_PRESENT)


#define HE_IS_HTC_HE_VARIANT(_u4HTC) \
	(_u4HTC & HTC_HE_VARIANT == HTC_HE_VARIANT)
#define HE_SET_HTC_HE_VARIANT(_u4HTC) \
	(_u4HTC |= HTC_HE_VARIANT)

/* Control ID */
#define HE_SET_HTC_1ST_A_CTRL_ID(_u4HTC, _ctrl_id) \
{\
(_u4HTC) &= ~(HTC_HE_1ST_A_CTRL_ID); \
(_u4HTC) |= (((_ctrl_id) << (HTC_HE_1ST_A_CTRL_ID_SHIFT)) \
	& (HTC_HE_1ST_A_CTRL_ID)); \
}
#define HE_GET_HTC_1ST_A_CTRL_ID(_u4HTC) \
	((_u4HTC & HTC_HE_1ST_A_CTRL_ID) >> HTC_HE_1ST_A_CTRL_ID_SHIFT)

#define HE_SET_HTC_2ND_A_CTRL_ID(_u4HTC, _ctrl_id) \
{\
	(_u4HTC) &= ~(HTC_HE_2ND_A_CTRL_ID); \
	(_u4HTC) |= (((_ctrl_id) << (HTC_HE_2ND_A_CTRL_ID_SHIFT)) \
	& (HTC_HE_2ND_A_CTRL_ID)); \
}
#define HE_GET_HTC_2ND_A_CTRL_ID(_u4HTC) \
		((_u4HTC & HTC_HE_2ND_A_CTRL_ID) >> HTC_HE_2ND_A_CTRL_ID_SHIFT)

/* OM - RX NSS */
#define HE_SET_HTC_HE_OM_RX_NSS(_u4HTC, _rx_nss) \
{\
(_u4HTC) &= ~(HTC_HE_OM_RX_NSS); \
(_u4HTC) |= (((_rx_nss) << (HTC_HE_OM_RX_NSS_SHFT)) & (HTC_HE_OM_RX_NSS)); \
}
#define HE_GET_HTC_HE_OM_RX_NSS(_u4HTC) \
	((_u4HTC & HTC_HE_OM_RX_NSS) >> HTC_HE_OM_RX_NSS_SHFT)

/* OM - TX NSTS */
#define HE_SET_HTC_HE_OM_TX_NSTS(_u4HTC, _tx_nsts) \
{\
(_u4HTC) &= ~(HTC_HE_OM_TX_NSTS); \
(_u4HTC) |= (((_tx_nsts) << (HTC_HE_OM_TX_NSTS_SHFT)) & (HTC_HE_OM_TX_NSTS)); \
}
#define HE_GET_HTC_HE_OM_TX_NSTS(_u4HTC) \
	((_u4HTC & HTC_HE_OM_TX_NSTS) >> HTC_HE_OM_TX_NSTS_SHFT)

/* OM - Channel Width */
#define HE_SET_HTC_HE_OM_CH_WIDTH(_u4HTC, _bw) \
{\
(_u4HTC) &= ~(HTC_HE_OM_CH_WIDTH); \
(_u4HTC) |= (((_bw) << (HTC_HE_OM_CH_WIDTH_SHFT)) & (HTC_HE_OM_CH_WIDTH)); \
}
#define HE_GET_HTC_HE_OM_CH_WIDTH(_u4HTC) \
	((_u4HTC & HTC_HE_OM_CH_WIDTH) >> HTC_HE_OM_CH_WIDTH_SHFT)

/* OM - ER SU Disable */
#define HE_SET_HTC_HE_OM_ER_SU_DISABLE(_u4HTC, _er_dis) \
{\
(_u4HTC) &= ~(HTC_HE_OM_ER_SU_DISABLE); \
(_u4HTC) |= (((_er_dis) << (HTC_HE_OM_ER_SU_DISABLE_SHFT)) \
	& (HTC_HE_OM_ER_SU_DISABLE)); \
}
#define HE_GET_HTC_HE_OM_ER_SU_DISABLE(_u4HTC) \
	((_u4HTC & HTC_HE_OM_ER_SU_DISABLE) >> HTC_HE_OM_ER_SU_DISABLE_SHFT)

/* OM - UL MU Disable */
#define HE_SET_HTC_HE_OM_UL_MU_DISABLE(_u4HTC, _ul_dis) \
{\
(_u4HTC) &= ~(HTC_HE_OM_UL_MU_DISABLE); \
(_u4HTC) |= (((_ul_dis) << (HTC_HE_OM_UL_MU_DISABLE_SHFT)) \
	& (HTC_HE_OM_UL_MU_DISABLE)); \
}
#define HE_GET_HTC_HE_OM_UL_MU_DISABLE(_u4HTC) \
	((_u4HTC & HTC_HE_OM_UL_MU_DISABLE) >> HTC_HE_OM_UL_MU_DISABLE_SHFT)

/* OM - UL MU Data Disable */
#define HE_SET_HTC_HE_OM_UL_MU_DATA_DISABLE(_u4HTC, _ul_data_dis) \
{\
(_u4HTC) &= ~(HTC_HE_OM_UL_MU_DATA_DISABLE); \
(_u4HTC) |= (((_ul_data_dis) << (HTC_HE_OM_UL_MU_DATA_DISABLE_SHFT)) \
	& (HTC_HE_OM_UL_MU_DATA_DISABLE)); \
}
#define HE_GET_HTC_HE_OM_UL_MU_DATA_DISABLE(_u4HTC) \
	((_u4HTC & HTC_HE_OM_UL_MU_DATA_DISABLE) \
		>> HTC_HE_OM_UL_MU_DATA_DISABLE_SHFT)

/* UPH - UL PWR HEADROOM */
#define HE_SET_HTC_HE_UPH(_u4HTC, _uph) \
{\
(_u4HTC) &= ~(HTC_HE_UPH_UL_PWR_HEADROOM); \
(_u4HTC) |= (((_uph) << (HTC_HE_UPH_UL_PWR_HEADROOM_SHIFT)) \
	& (HTC_HE_UPH_UL_PWR_HEADROOM)); \
}
#define HE_GET_HTC_HE_UPH(_u4HTC) \
	((_u4HTC & HTC_HE_UPH_UL_PWR_HEADROOM) \
		>> HTC_HE_UPH_UL_PWR_HEADROOM_SHIFT)

/* UPH - MIN TX PWR FLAG  */
#define HE_SET_HTC_HE_UPH_MIN_TX_PWR_FLAG(_u4HTC, _flag) \
{\
(_u4HTC) &= ~(HTC_HE_UPH_MIN_TX_PWR_FLAG); \
(_u4HTC) |= (((_flag) << (HTC_HE_UPH_MIN_TX_PWR_FLAG_SHIFT)) \
	& (HTC_HE_UPH_MIN_TX_PWR_FLAG)); \
}
#define HE_GET_HTC_HE_UPH_MIN_TX_PWR_FLAG(_u4HTC) \
	((_u4HTC & HTC_HE_UPH_MIN_TX_PWR_FLAG) \
		>> HTC_HE_UPH_MIN_TX_PWR_FLAG_SHIFT)

/* should use macro to access field of HE OP*/
#define HE_RESET_HE_OP(_aucHeOpInfo) \
	memset(_aucHeOpInfo, 0, HE_OP_BYTE_NUM)

#define HE_SET_OP_PARAM_ER_SU_DISABLE(_aucHeOpParams) \
	(_aucHeOpParams[2] |= HE_OP_PARAM2_ER_SU_DISABLE_MASK)

#define HE_IS_ER_SU_DISABLE(_aucHeOpParams) \
	(_aucHeOpParams[2] & HE_OP_PARAM2_ER_SU_DISABLE_MASK)

struct _IE_HE_CAP_T {
	u_int8_t  ucId;
	u_int8_t  ucLength;
	u_int8_t  ucExtId;
	u_int8_t  ucHeMacCap[HE_MAC_CAP_BYTE_NUM]; /* BIT0 ~ BIT47 */
	u_int8_t  ucHePhyCap[HE_PHY_CAP_BYTE_NUM]; /* BIT0 ~ BIT87 */
	u_int8_t  aucVarInfo[0];
} __KAL_ATTRIB_PACKED__;

struct _IE_HE_OP_T {
	u_int8_t  ucId;
	u_int8_t  ucLength;
	u_int8_t  ucExtId;
	u_int8_t  ucHeOpParams[HE_OP_BYTE_NUM];
	u_int8_t  ucBssColorInfo;
	u_int16_t u2HeBasicMcsSet;
	u_int8_t  aucVarInfo[0];
} __KAL_ATTRIB_PACKED__;

struct _HE_SUPPORTED_MCS_FIELD {
	u_int16_t u2RxMcsMap;
	u_int16_t u2TxMcsMap;
} __KAL_ATTRIB_PACKED__;

struct _PPE_THRESHOLD_FIELD {
	/* 128-bit space can support 4 NSS */
	u_int64_t u8Space0;
	u_int64_t u8Space1;
} __KAL_ATTRIB_PACKED__;

struct _VHT_OP_INFO_T {
	u_int8_t ucVhtOperation[3];
} __KAL_ATTRIB_PACKED__;

struct _HE_MAX_BSSID_IND_T {
	u_int8_t ucMaxBSSIDIndicator;
} __KAL_ATTRIB_PACKED__;

struct _MU_AC_PARAM_RECORD_T {
	u_int8_t ucAciAifsn;
	u_int8_t ucEcw;
	u_int8_t ucMUEdcaTimer;
} __KAL_ATTRIB_PACKED__;

struct _IE_MU_EDCA_PARAM_T {
	u_int8_t ucId;
	u_int8_t ucLength;
	u_int8_t ucExtId;
	u_int8_t ucMUQosInfo;
	struct _MU_AC_PARAM_RECORD_T arMUAcParam[4];
} __KAL_ATTRIB_PACKED__;

struct _SRG_SR_INFO_T {
	u_int8_t ucObssPdMinOffset;
	u_int8_t ucObssPdMaxOffset;
	u_int64_t u8BSSColorBitmap;
	u_int64_t u8PartialBSSIDBitmap;
} __KAL_ATTRIB_PACKED__;

struct _NON_SRG_SR_INFO_T {
	u_int8_t ucObssPdMaxOffset;
} __KAL_ATTRIB_PACKED__;

struct _IE_SR_PARAM_T {
	u_int8_t  ucId;
	u_int8_t  ucLength;
	u_int8_t  ucExtId;
	u_int8_t  ucSRControl;
	u_int8_t  aucVarInfo[1];
} __KAL_ATTRIB_PACKED__;

#endif /* CFG_SUPPORT_802_11AX == 1 */
#endif /* !_HE_IE_H */
