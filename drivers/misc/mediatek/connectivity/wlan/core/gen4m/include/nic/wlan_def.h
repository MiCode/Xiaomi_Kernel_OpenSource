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
 *      MT6620_WIFI_DRIVER_V2_3/include/nic/wlan_def.h#1
 */

/*! \file   "wlan_def.h"
 *  \brief  This file includes the basic definition of WLAN
 *
 */


#ifndef _WLAN_DEF_H
#define _WLAN_DEF_H

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
/* disconnect reason */
#define DISCONNECT_REASON_CODE_RESERVED           0
#define DISCONNECT_REASON_CODE_RADIO_LOST         1
#define DISCONNECT_REASON_CODE_DEAUTHENTICATED    2
#define DISCONNECT_REASON_CODE_DISASSOCIATED      3
#define DISCONNECT_REASON_CODE_NEW_CONNECTION     4
#define DISCONNECT_REASON_CODE_REASSOCIATION      5
#define DISCONNECT_REASON_CODE_ROAMING            6
#define DISCONNECT_REASON_CODE_CHIPRESET          7
#define DISCONNECT_REASON_CODE_LOCALLY            8
#define DISCONNECT_REASON_CODE_RADIO_LOST_TX_ERR  9

/* The rate definitions */
#define TX_MODE_CCK             0x00
#define TX_MODE_OFDM            0x40
#define TX_MODE_HT_MM           0x80
#define TX_MODE_HT_GF           0xC0
#define TX_MODE_VHT             0x100
#define TX_MODE_HE_SU           0x200
#define TX_MODE_HE_ER_SU        0x240
#define TX_MODE_HE_TB           0X280

#define RATE_CCK_SHORT_PREAMBLE 0x4

#define PHY_RATE_1M             0x0
#define PHY_RATE_2M             0x1
#define PHY_RATE_5_5M           0x2
#define PHY_RATE_11M            0x3
#define PHY_RATE_6M             0xB
#define PHY_RATE_9M             0xF
#define PHY_RATE_12M            0xA
#define PHY_RATE_18M            0xE
#define PHY_RATE_24M            0x9
#define PHY_RATE_36M            0xD
#define PHY_RATE_48M            0x8
#define PHY_RATE_54M            0xC

#define PHY_RATE_MCS0           0x0
#define PHY_RATE_MCS1           0x1
#define PHY_RATE_MCS2           0x2
#define PHY_RATE_MCS3           0x3
#define PHY_RATE_MCS4           0x4
#define PHY_RATE_MCS5           0x5
#define PHY_RATE_MCS6           0x6
#define PHY_RATE_MCS7           0x7
#define PHY_RATE_MCS8           0x8
#define PHY_RATE_MCS9           0x9
#define PHY_RATE_MCS32          0x20

#define PHY_RATE_DCM			0x10
#define PHY_RATE_TONE_106		0x20

#define RATE_CCK_1M_LONG        (TX_MODE_CCK | PHY_RATE_1M)
#define RATE_CCK_2M_LONG        (TX_MODE_CCK | PHY_RATE_2M)
#define RATE_CCK_5_5M_LONG      (TX_MODE_CCK | PHY_RATE_5_5M)
#define RATE_CCK_11M_LONG       (TX_MODE_CCK | PHY_RATE_11M)
#define RATE_CCK_2M_SHORT \
		(TX_MODE_CCK | PHY_RATE_2M | RATE_CCK_SHORT_PREAMBLE)
#define RATE_CCK_5_5M_SHORT \
		(TX_MODE_CCK | PHY_RATE_5_5M | RATE_CCK_SHORT_PREAMBLE)
#define RATE_CCK_11M_SHORT \
		(TX_MODE_CCK | PHY_RATE_11M | RATE_CCK_SHORT_PREAMBLE)
#define RATE_OFDM_6M            (TX_MODE_OFDM | PHY_RATE_6M)
#define RATE_OFDM_9M            (TX_MODE_OFDM | PHY_RATE_9M)
#define RATE_OFDM_12M           (TX_MODE_OFDM | PHY_RATE_12M)
#define RATE_OFDM_18M           (TX_MODE_OFDM | PHY_RATE_18M)
#define RATE_OFDM_24M           (TX_MODE_OFDM | PHY_RATE_24M)
#define RATE_OFDM_36M           (TX_MODE_OFDM | PHY_RATE_36M)
#define RATE_OFDM_48M           (TX_MODE_OFDM | PHY_RATE_48M)
#define RATE_OFDM_54M           (TX_MODE_OFDM | PHY_RATE_54M)

#define RATE_MM_MCS_0           (TX_MODE_HT_MM | PHY_RATE_MCS0)
#define RATE_MM_MCS_1           (TX_MODE_HT_MM | PHY_RATE_MCS1)
#define RATE_MM_MCS_2           (TX_MODE_HT_MM | PHY_RATE_MCS2)
#define RATE_MM_MCS_3           (TX_MODE_HT_MM | PHY_RATE_MCS3)
#define RATE_MM_MCS_4           (TX_MODE_HT_MM | PHY_RATE_MCS4)
#define RATE_MM_MCS_5           (TX_MODE_HT_MM | PHY_RATE_MCS5)
#define RATE_MM_MCS_6           (TX_MODE_HT_MM | PHY_RATE_MCS6)
#define RATE_MM_MCS_7           (TX_MODE_HT_MM | PHY_RATE_MCS7)
#define RATE_MM_MCS_32          (TX_MODE_HT_MM | PHY_RATE_MCS32)

#define RATE_GF_MCS_0           (TX_MODE_HT_GF | PHY_RATE_MCS0)
#define RATE_GF_MCS_1           (TX_MODE_HT_GF | PHY_RATE_MCS1)
#define RATE_GF_MCS_2           (TX_MODE_HT_GF | PHY_RATE_MCS2)
#define RATE_GF_MCS_3           (TX_MODE_HT_GF | PHY_RATE_MCS3)
#define RATE_GF_MCS_4           (TX_MODE_HT_GF | PHY_RATE_MCS4)
#define RATE_GF_MCS_5           (TX_MODE_HT_GF | PHY_RATE_MCS5)
#define RATE_GF_MCS_6           (TX_MODE_HT_GF | PHY_RATE_MCS6)
#define RATE_GF_MCS_7           (TX_MODE_HT_GF | PHY_RATE_MCS7)
#define RATE_GF_MCS_32          (TX_MODE_HT_GF | PHY_RATE_MCS32)

#define RATE_VHT_MCS_0          (TX_MODE_VHT | PHY_RATE_MCS0)
#define RATE_VHT_MCS_1          (TX_MODE_VHT | PHY_RATE_MCS1)
#define RATE_VHT_MCS_2          (TX_MODE_VHT | PHY_RATE_MCS2)
#define RATE_VHT_MCS_3          (TX_MODE_VHT | PHY_RATE_MCS3)
#define RATE_VHT_MCS_4          (TX_MODE_VHT | PHY_RATE_MCS4)
#define RATE_VHT_MCS_5          (TX_MODE_VHT | PHY_RATE_MCS5)
#define RATE_VHT_MCS_6          (TX_MODE_VHT | PHY_RATE_MCS6)
#define RATE_VHT_MCS_7          (TX_MODE_VHT | PHY_RATE_MCS7)
#define RATE_VHT_MCS_8          (TX_MODE_VHT | PHY_RATE_MCS8)
#define RATE_VHT_MCS_9          (TX_MODE_VHT | PHY_RATE_MCS9)

#define RATE_HE_ER_DCM_MCS_0	(TX_MODE_HE_ER_SU | PHY_RATE_DCM)
#define RATE_HE_ER_TONE_106_MCS_0	(TX_MODE_HE_ER_SU | PHY_RATE_TONE_106)

#define RATE_NSTS_MASK					BITS(9, 10)
#define RATE_NSTS_OFFSET				9
#define RATE_TX_MODE_MASK       BITS(6, 8)
#define RATE_TX_MODE_OFFSET     6
#define RATE_CODE_GET_TX_MODE(_ucRateCode) \
		((_ucRateCode & RATE_TX_MODE_MASK) >> RATE_TX_MODE_OFFSET)
#define RATE_PHY_RATE_MASK      BITS(0, 5)
#define RATE_PHY_RATE_OFFSET    0
#define RATE_CODE_GET_PHY_RATE(_ucRateCode) \
		((_ucRateCode & RATE_PHY_RATE_MASK) >> RATE_PHY_RATE_OFFSET)
#define RATE_PHY_RATE_SHORT_PREAMBLE                BIT(2)
#define RATE_CODE_IS_SHORT_PREAMBLE(_ucRateCode) \
		((_ucRateCode & RATE_PHY_RATE_SHORT_PREAMBLE) ? TRUE : FALSE)

#define CHNL_LIST_SZ_2G         14
#define CHNL_LIST_SZ_5G         14

/*! CNM(STA_RECORD_T) related definition */
#define CFG_STA_REC_NUM         27

/* PHY TYPE bit definitions */
/* HR/DSSS PHY (clause 18) */
#define PHY_TYPE_BIT_HR_DSSS    BIT(PHY_TYPE_HR_DSSS_INDEX)
/* ERP PHY (clause 19) */
#define PHY_TYPE_BIT_ERP        BIT(PHY_TYPE_ERP_INDEX)
/* OFDM 5 GHz PHY (clause 17) */
#define PHY_TYPE_BIT_OFDM       BIT(PHY_TYPE_OFDM_INDEX)
/* HT PHY (clause 20) */
#define PHY_TYPE_BIT_HT         BIT(PHY_TYPE_HT_INDEX)
/* HT PHY (clause 22) */
#define PHY_TYPE_BIT_VHT        BIT(PHY_TYPE_VHT_INDEX)

#if (CFG_SUPPORT_802_11AX == 1)
/* HE PHY */
#define PHY_TYPE_BIT_HE         BIT(PHY_TYPE_HE_INDEX)
#endif

/* PHY TYPE set definitions */
#define PHY_TYPE_SET_802_11ABGN (PHY_TYPE_BIT_OFDM | \
				 PHY_TYPE_BIT_HR_DSSS | \
				 PHY_TYPE_BIT_ERP | \
				 PHY_TYPE_BIT_HT)

#define PHY_TYPE_SET_802_11BGN  (PHY_TYPE_BIT_HR_DSSS | \
				 PHY_TYPE_BIT_ERP | \
				 PHY_TYPE_BIT_HT)

#define PHY_TYPE_SET_802_11GN   (PHY_TYPE_BIT_ERP | \
				 PHY_TYPE_BIT_HT)

#define PHY_TYPE_SET_802_11AN   (PHY_TYPE_BIT_OFDM | \
				 PHY_TYPE_BIT_HT)

#define PHY_TYPE_SET_802_11ABG  (PHY_TYPE_BIT_OFDM | \
				 PHY_TYPE_BIT_HR_DSSS | \
				 PHY_TYPE_BIT_ERP)

#define PHY_TYPE_SET_802_11BG   (PHY_TYPE_BIT_HR_DSSS | \
				 PHY_TYPE_BIT_ERP)

#define PHY_TYPE_SET_802_11A    (PHY_TYPE_BIT_OFDM)

#define PHY_TYPE_SET_802_11G    (PHY_TYPE_BIT_ERP)

#define PHY_TYPE_SET_802_11B    (PHY_TYPE_BIT_HR_DSSS)

#define PHY_TYPE_SET_802_11N    (PHY_TYPE_BIT_HT)

#define PHY_TYPE_SET_802_11AC   (PHY_TYPE_BIT_VHT)

#define PHY_TYPE_SET_802_11ANAC (PHY_TYPE_BIT_OFDM | \
				 PHY_TYPE_BIT_HT | \
				 PHY_TYPE_BIT_VHT)

#define PHY_TYPE_SET_802_11ABGNAC (PHY_TYPE_BIT_OFDM | \
				   PHY_TYPE_BIT_HR_DSSS | \
				   PHY_TYPE_BIT_ERP | \
				   PHY_TYPE_BIT_HT | \
				   PHY_TYPE_BIT_VHT)

#if (CFG_SUPPORT_802_11AX == 1)
#define PHY_TYPE_SET_802_11AX   (PHY_TYPE_BIT_HE)

#define PHY_TYPE_SET_802_11ABGNACAX (PHY_TYPE_BIT_OFDM | \
				   PHY_TYPE_BIT_HR_DSSS | \
				   PHY_TYPE_BIT_ERP | \
				   PHY_TYPE_BIT_HT | \
				   PHY_TYPE_BIT_VHT | \
				   PHY_TYPE_BIT_HE)
#endif /* CFG_SUPPORT_802_11AX == 1 */

/* Rate set bit definitions */
#define RATE_SET_BIT_1M         BIT(RATE_1M_SW_INDEX)	/* Bit 0: 1M */
#define RATE_SET_BIT_2M         BIT(RATE_2M_SW_INDEX)	/* Bit 1: 2M */
#define RATE_SET_BIT_5_5M       BIT(RATE_5_5M_SW_INDEX)	/* Bit 2: 5.5M */
#define RATE_SET_BIT_11M        BIT(RATE_11M_SW_INDEX)	/* Bit 3: 11M */
#define RATE_SET_BIT_22M        BIT(RATE_22M_SW_INDEX)	/* Bit 4: 22M */
#define RATE_SET_BIT_33M        BIT(RATE_33M_SW_INDEX)	/* Bit 5: 33M */
#define RATE_SET_BIT_6M         BIT(RATE_6M_SW_INDEX)	/* Bit 6: 6M */
#define RATE_SET_BIT_9M         BIT(RATE_9M_SW_INDEX)	/* Bit 7: 9M */
#define RATE_SET_BIT_12M        BIT(RATE_12M_SW_INDEX)	/* Bit 8: 12M */
#define RATE_SET_BIT_18M        BIT(RATE_18M_SW_INDEX)	/* Bit 9: 18M */
#define RATE_SET_BIT_24M        BIT(RATE_24M_SW_INDEX)	/* Bit 10: 24M */
#define RATE_SET_BIT_36M        BIT(RATE_36M_SW_INDEX)	/* Bit 11: 36M */
#define RATE_SET_BIT_48M        BIT(RATE_48M_SW_INDEX)	/* Bit 12: 48M */
#define RATE_SET_BIT_54M        BIT(RATE_54M_SW_INDEX)	/* Bit 13: 54M */
/* Bit 14: BSS Selector */
#define RATE_SET_BIT_VHT_PHY    BIT(RATE_VHT_PHY_SW_INDEX)
/* Bit 15: BSS Selector */
#define RATE_SET_BIT_HT_PHY     BIT(RATE_HT_PHY_SW_INDEX)

/* Rate set definitions */
#define RATE_SET_HR_DSSS            (RATE_SET_BIT_1M | \
				     RATE_SET_BIT_2M | \
				     RATE_SET_BIT_5_5M | \
				     RATE_SET_BIT_11M)

#define RATE_SET_ERP                (RATE_SET_BIT_1M | \
				     RATE_SET_BIT_2M | \
				     RATE_SET_BIT_5_5M | \
				     RATE_SET_BIT_11M | \
				     RATE_SET_BIT_6M | \
				     RATE_SET_BIT_9M | \
				     RATE_SET_BIT_12M | \
				     RATE_SET_BIT_18M | \
				     RATE_SET_BIT_24M | \
				     RATE_SET_BIT_36M | \
				     RATE_SET_BIT_48M | \
				     RATE_SET_BIT_54M)

#define RATE_SET_ERP_P2P            (RATE_SET_BIT_6M | \
				     RATE_SET_BIT_9M | \
				     RATE_SET_BIT_12M | \
				     RATE_SET_BIT_18M | \
				     RATE_SET_BIT_24M | \
				     RATE_SET_BIT_36M | \
				     RATE_SET_BIT_48M | \
				     RATE_SET_BIT_54M)

#define RATE_SET_OFDM               (RATE_SET_BIT_6M | \
				     RATE_SET_BIT_9M | \
				     RATE_SET_BIT_12M | \
				     RATE_SET_BIT_18M | \
				     RATE_SET_BIT_24M | \
				     RATE_SET_BIT_36M | \
				     RATE_SET_BIT_48M | \
				     RATE_SET_BIT_54M)

#define RATE_SET_HT                 (RATE_SET_ERP)

#define RATE_SET_ALL_ABG             RATE_SET_ERP

#define BASIC_RATE_SET_HR_DSSS      (RATE_SET_BIT_1M | \
				     RATE_SET_BIT_2M)

#define BASIC_RATE_SET_HR_DSSS_ERP  (RATE_SET_BIT_1M | \
				     RATE_SET_BIT_2M | \
				     RATE_SET_BIT_5_5M | \
				     RATE_SET_BIT_11M)

#define BASIC_RATE_SET_ERP          (RATE_SET_BIT_1M | \
				     RATE_SET_BIT_2M | \
				     RATE_SET_BIT_5_5M | \
				     RATE_SET_BIT_11M | \
				     RATE_SET_BIT_6M | \
				     RATE_SET_BIT_12M | \
				     RATE_SET_BIT_24M)

#define BASIC_RATE_SET_OFDM         (RATE_SET_BIT_6M | \
				     RATE_SET_BIT_12M | \
				     RATE_SET_BIT_24M)

#define BASIC_RATE_SET_ERP_P2P      (RATE_SET_BIT_6M | \
				     RATE_SET_BIT_12M | \
				     RATE_SET_BIT_24M)

#define INITIAL_RATE_SET_RCPI_100    RATE_SET_ALL_ABG

#define INITIAL_RATE_SET_RCPI_80    (RATE_SET_BIT_1M | \
				     RATE_SET_BIT_2M | \
				     RATE_SET_BIT_5_5M | \
				     RATE_SET_BIT_11M | \
				     RATE_SET_BIT_6M | \
				     RATE_SET_BIT_9M | \
				     RATE_SET_BIT_12M | \
				     RATE_SET_BIT_24M)

#define INITIAL_RATE_SET_RCPI_60    (RATE_SET_BIT_1M | \
				     RATE_SET_BIT_2M | \
				     RATE_SET_BIT_5_5M | \
				     RATE_SET_BIT_11M | \
				     RATE_SET_BIT_6M)

#define INITIAL_RATE_SET(_rcpi)     (INITIAL_RATE_SET_ ## _rcpi)

#define RCPI_100                    100	/* -60 dBm */
#define RCPI_80                     80	/* -70 dBm */
#define RCPI_60                     60	/* -80 dBm */

/* The number of RCPI records used to calculate their average value */
#define MAX_NUM_RCPI_RECORDS        10

/* The number of RCPI records used to calculate their average value */
#define NO_RCPI_RECORDS             -128
#define MAX_RCPI_DBM                0
#define MIN_RCPI_DBM                -100
/* Available AID: 1 ~ 20(STA_REC_NUM) */
#define MAX_ASSOC_ID                (CFG_STA_REC_NUM)
/* NOTE(Kevin): Used in auth.c */
#define MAX_DEAUTH_INFO_COUNT       4
/* The minimum interval if continuously send Deauth Frame */
#define MIN_DEAUTH_INTERVAL_MSEC    500

/* Authentication Type */
#define AUTH_TYPE_OPEN_SYSTEM \
			BIT(AUTH_ALGORITHM_NUM_OPEN_SYSTEM)
#define AUTH_TYPE_SHARED_KEY \
			BIT(AUTH_ALGORITHM_NUM_SHARED_KEY)
#define AUTH_TYPE_FAST_BSS_TRANSITION \
			BIT(AUTH_ALGORITHM_NUM_FAST_BSS_TRANSITION)
#define AUTH_TYPE_SAE \
			BIT(AUTH_ALGORITHM_NUM_SAE)

/* Authentication Retry Limit */
#define TX_AUTH_ASSOCI_RETRY_LIMIT                  2
#define TX_AUTH_ASSOCI_RETRY_LIMIT_FOR_ROAMING      1

/* WMM-2.2.1 WMM Information Element */
#define ELEM_MAX_LEN_WMM_INFO       7

/* */
#define RA_ER_Disable	0
#define RA_DCM			1
#define RA_ER_106		2

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

enum ENUM_HW_BSSID {
	BSSID_0 = 0,
	BSSID_1,
	BSSID_2,
	BSSID_3,
	BSSID_NUM
};

enum ENUM_HW_MAC_ADDR {
	MAC_ADDR_0 = 0,
	MAC_ADDR_1,
	MAC_ADDR_NUM
};

enum ENUM_HW_OP_MODE {
	HW_OP_MODE_STA = 0,
	HW_OP_MODE_AP,
	HW_OP_MODE_ADHOC,
	HW_OP_MODE_NUM
};

enum ENUM_TSF {
	ENUM_LOCAL_TSF_0,
	ENUM_LOCAL_TSF_1,
	ENUM_LOCAL_TSF_NUM
};

enum HAL_TS_HW_UPDATE_MODE {
	HAL_TSF_HW_UPDATE_BY_TICK_AND_RECEIVED_FRAME,
	HAL_TSF_HW_UPDATE_BY_TICK_ONLY,
	HAL_TSF_HW_UPDATE_BY_RECEIVED_FRAME_ONLY,
	HAL_TSF_HW_UPDATE_BY_TICK_AND_RECEIVED_FRAME_AD_HOC
};

enum ENUM_AC {
	AC0 = 0,
	AC1,
	AC2,
	AC3,
	AC_NUM
};

enum ENUM_NETWORK_TYPE {
	NETWORK_TYPE_AIS,
	NETWORK_TYPE_P2P,
	NETWORK_TYPE_BOW,
	NETWORK_TYPE_MBSS,
	NETWORK_TYPE_NUM
};

/* The Type of STA Type. */
enum ENUM_STA_TYPE_INDEX {
	STA_TYPE_LEGACY_INDEX = 0,
	STA_TYPE_P2P_INDEX,
	STA_TYPE_BOW_INDEX,
	STA_TYPE_INDEX_NUM
};

enum ENUM_PHY_MODE_TYPE {
	PHY_MODE_CCK = 0,
	PHY_MODE_OFDM = 1,
	PHY_MODE_HT20 = 2,
	PHY_MODE_HT40 = 3,
	PHY_MODE_VHT20 = 4,
	PHY_MODE_VHT40 = 5,
	PHY_MODE_VHT80 = 6,
	PHY_MODE_VHT160 = 7,
	PHY_MODE_SU20 = 8,
	PHY_MODE_SU40 = 9,
	PHY_MODE_SU80 = 10,
	PHY_MODE_RU26 = 11,
	PHY_MODE_RU52 = 12,
	PHY_MODE_RU106 = 13,
	PHY_MODE_RU242 = 14,
	PHY_MODE_RU484 = 15,
	PHY_MODE_RU996 = 16,
	PHY_MODE_TYPE_NUM
};


#define STA_ROLE_BASE_INDEX     4

enum ENUM_STA_ROLE_INDEX {
	STA_ROLE_ADHOC_INDEX = STA_ROLE_BASE_INDEX,	/* 4 */
	STA_ROLE_CLIENT_INDEX,
	STA_ROLE_AP_INDEX,
	STA_ROLE_DLS_INDEX,
	STA_ROLE_MAX_INDEX
};

#define STA_ROLE_INDEX_NUM      (STA_ROLE_MAX_INDEX - STA_ROLE_BASE_INDEX)

/* The Power State of a specific Network */
enum ENUM_PWR_STATE {
	PWR_STATE_IDLE = 0,
	PWR_STATE_ACTIVE,
	PWR_STATE_PS,
	PWR_STATE_NUM
};

enum ENUM_PHY_TYPE_INDEX {
	/* PHY_TYPE_DSSS_INDEX, *//* DSSS PHY (clause 15) -- Not used anymore */
	PHY_TYPE_HR_DSSS_INDEX = 0,	/* HR/DSSS PHY (clause 18) */
	PHY_TYPE_ERP_INDEX,	/* ERP PHY (clause 19) */
	PHY_TYPE_ERP_P2P_INDEX,	/* ERP PHY (clause 19) w/o HR/DSSS */
	PHY_TYPE_OFDM_INDEX,	/* OFDM 5 GHz PHY (clause 17) */
	PHY_TYPE_HT_INDEX,	/* HT PHY (clause 20) */
	PHY_TYPE_VHT_INDEX,	/* HT PHY (clause 22) */
#if (CFG_SUPPORT_802_11AX == 1)
	PHY_TYPE_HE_INDEX,	/* HE PHY */
#endif /* CFG_SUPPORT_802_11AX == 1 */

	PHY_TYPE_INDEX_NUM	/* 6 */
};

enum ENUM_SW_RATE_INDEX {
	RATE_1M_SW_INDEX = 0,	/* 1M */
	RATE_2M_SW_INDEX,	/* 2M */
	RATE_5_5M_SW_INDEX,	/* 5.5M */
	RATE_11M_SW_INDEX,	/* 11M */
	RATE_22M_SW_INDEX,	/* 22M */
	RATE_33M_SW_INDEX,	/* 33M */
	RATE_6M_SW_INDEX,	/* 6M */
	RATE_9M_SW_INDEX,	/* 9M */
	RATE_12M_SW_INDEX,	/* 12M */
	RATE_18M_SW_INDEX,	/* 18M */
	RATE_24M_SW_INDEX,	/* 24M */
	RATE_36M_SW_INDEX,	/* 36M */
	RATE_48M_SW_INDEX,	/* 48M */
	RATE_54M_SW_INDEX,	/* 54M */
	RATE_VHT_PHY_SW_INDEX,	/* BSS Selector - VHT PHY */
	RATE_HT_PHY_SW_INDEX,	/* BSS Selector - HT PHY */
	RATE_NUM_SW		/* 16 */
};

enum ENUM_CCK_RATE_INDEX {
	RATE_1M_INDEX = 0,	/* 1M */
	RATE_2M_INDEX,		/* 2M */
	RATE_5_5M_INDEX,	/* 5.5M */
	RATE_11M_INDEX,		/* 11M */
	CCK_RATE_NUM		/* 4 */
};

enum ENUM_OFDM_RATE_INDEX {
	RATE_6M_INDEX = 0,	/* 6M */
	RATE_9M_INDEX,		/* 9M */
	RATE_12M_INDEX,		/* 12M */
	RATE_18M_INDEX,		/* 18M */
	RATE_24M_INDEX,		/* 24M */
	RATE_36M_INDEX,		/* 36M */
	RATE_48M_INDEX,		/* 48M */
	RATE_54M_INDEX,		/* 54M */
	OFDM_RATE_NUM		/* 8 */
};

enum ENUM_HT_RATE_INDEX {
	HT_RATE_MCS32_INDEX = 0,
	HT_RATE_MCS0_INDEX,
	HT_RATE_MCS1_INDEX,
	HT_RATE_MCS2_INDEX,
	HT_RATE_MCS3_INDEX,
	HT_RATE_MCS4_INDEX,
	HT_RATE_MCS5_INDEX,
	HT_RATE_MCS6_INDEX,
	HT_RATE_MCS7_INDEX,
	HT_RATE_MCS8_INDEX,
	HT_RATE_MCS9_INDEX,
	HT_RATE_MCS10_INDEX,
	HT_RATE_MCS11_INDEX,
	HT_RATE_MCS12_INDEX,
	HT_RATE_MCS13_INDEX,
	HT_RATE_MCS14_INDEX,
	HT_RATE_MCS15_INDEX,
	HT_RATE_NUM	/* 16 */
};

enum ENUM_VHT_RATE_INDEX {
	VHT_RATE_MCS0_INDEX = 0,
	VHT_RATE_MCS1_INDEX,
	VHT_RATE_MCS2_INDEX,
	VHT_RATE_MCS3_INDEX,
	VHT_RATE_MCS4_INDEX,
	VHT_RATE_MCS5_INDEX,
	VHT_RATE_MCS6_INDEX,
	VHT_RATE_MCS7_INDEX,
	VHT_RATE_MCS8_INDEX,
	VHT_RATE_MCS9_INDEX,
	VHT_RATE_NUM		/* 10 */
};

enum ENUM_PREMABLE_OPTION {
	/* LONG for PHY_TYPE_HR_DSSS, NONE for PHY_TYPE_OFDM */
	PREAMBLE_DEFAULT_LONG_NONE = 0,
	/* SHORT mandatory for PHY_TYPE_ERP,
	 * SHORT option for PHY_TYPE_HR_DSSS
	 */
	PREAMBLE_OPTION_SHORT,
	PREAMBLE_OFDM_MODE,
	PREAMBLE_HT_MIXED_MODE,
	PREAMBLE_HT_GREEN_FIELD,
	PREAMBLE_VHT_FIELD,
	PREAMBLE_OPTION_NUM
};

enum ENUM_MODULATION_SYSTEM {
	MODULATION_SYSTEM_CCK = 0,
	MODULATION_SYSTEM_OFDM,
	MODULATION_SYSTEM_HT20,
	MODULATION_SYSTEM_HT40,
	MODULATION_SYSTEM_NUM
};

enum ENUM_MODULATION_TYPE {
	MODULATION_TYPE_CCK_BPSK = 0,
	MODULATION_TYPE_QPSK,
	MODULATION_TYPE_16QAM,
	MODULATION_TYPE_64QAM,
	MODULATION_TYPE_NUM
};

enum ENUM_ACPI_STATE {
	ACPI_STATE_D0 = 0,
	ACPI_STATE_D1,
	ACPI_STATE_D2,
	ACPI_STATE_D3
};

/* The operation mode of a specific Network */
enum ENUM_OP_MODE {
	OP_MODE_INFRASTRUCTURE = 0,	/* Infrastructure/GC */
	OP_MODE_IBSS,		/* AdHoc */
	OP_MODE_ACCESS_POINT,	/* For GO */
	OP_MODE_P2P_DEVICE,	/* P2P Device */
	OP_MODE_BOW,
	OP_MODE_NUM
};

/*
 * NL80211 interface type.
 * Refer to the definition of nl80211_iftype in kernel/nl80211.h
 */
enum ENUM_IFTYPE {
	IFTYPE_UNSPECIFIED = 0,
	IFTYPE_ADHOC,
	IFTYPE_STATION,
	IFTYPE_AP,
	IFTYPE_AP_VLAN,
	IFTYPE_WDS,
	IFTYPE_MONITOR,
	IFTYPE_MESH_POINT,
	IFTYPE_P2P_CLIENT,
	IFTYPE_P2P_GO,
	IFTYPE_P2P_DEVICE,
	IFTYPE_OCB,
	IFTYPE_NAN,
	IFTYPE_NUM
};

/* Concurrency mode for p2p preferred freq list*/
enum CONN_MODE_IFACE_TYPE {
	CONN_MODE_IFACE_TYPE_STA = 0,
	CONN_MODE_IFACE_TYPE_SAP,
	CONN_MODE_IFACE_TYPE_P2P_GC,
	CONN_MODE_IFACE_TYPE_P2P_GO,
	CONN_MODE_IFACE_TYPE_IBSS,
	CONN_MODE_IFACE_TYPE_TDLS,
	CONN_MODE_IFACE_TYPE_NUM
};

enum ENUM_CHNL_EXT {
	CHNL_EXT_SCN = 0,
	CHNL_EXT_SCA = 1,
	CHNL_EXT_RES = 2,
	CHNL_EXT_SCB = 3
};

enum ENUM_CHANNEL_WIDTH {
	CW_20_40MHZ = 0,
	CW_80MHZ = 1,
	CW_160MHZ = 2,
	CW_80P80MHZ = 3
};

/* This starting freq of the band is unit of kHz */
enum ENUM_BAND {
	BAND_NULL,
	BAND_2G4,
	BAND_5G,
	BAND_NUM
};

enum ENUM_CH_REQ_TYPE {
	CH_REQ_TYPE_JOIN,
	CH_REQ_TYPE_ROC, /* requested by remain on channel type */
	CH_REQ_TYPE_OFFCHNL_TX,
	CH_REQ_TYPE_GO_START_BSS,
#if (CFG_SUPPORT_DFS_MASTER == 1)
	CH_REQ_TYPE_DFS_CAC,
#endif
	CH_REQ_TYPE_NUM
};

enum ENUM_DBDC_BN {
	ENUM_BAND_0,
	ENUM_BAND_1,
	ENUM_BAND_NUM,
	ENUM_BAND_ALL,
	ENUM_BAND_AUTO	/*Auto select by A/G band, Driver only*/
};

/* Provide supported channel list to other components in array format */
struct RF_CHANNEL_INFO {
	enum ENUM_BAND eBand;
	/* To record Channel Center Frequency Segment 0 (MHz) from CFG80211 */
	uint32_t u4CenterFreq1;
	/* To record Channel Center Frequency Segment 1 (MHz) from CFG80211 */
	uint32_t u4CenterFreq2;
	/* To record primary channel frequency (MHz) from CFG80211 */
	uint16_t u2PriChnlFreq;
	/* To record channel bandwidth from CFG80211 */
	uint8_t ucChnlBw;
	uint8_t ucChannelNum;
	enum nl80211_dfs_state eDFS;
};

enum ENUM_PS_FORWARDING_TYPE {
	PS_FORWARDING_TYPE_NON_PS = 0,
	PS_FORWARDING_TYPE_DELIVERY_ENABLED,
	PS_FORWARDING_TYPE_NON_DELIVERY_ENABLED,
	PS_FORWARDING_MORE_DATA_ENABLED,
	PS_FORWARDING_TYPE_NUM
};

enum ENUM_AR_SS {
	AR_SS_NULL = 0,
	AR_SS_1,
	AR_SS_2,
	AR_SS_3,
	AR_SS_4,
	AR_SS_NUM
};

enum ENUM_MAC_BANDWIDTH {
	MAC_BW_20 = 0,
	MAC_BW_40,
	MAC_BW_80,
	MAC_BW_160
};

struct DEAUTH_INFO {
	uint8_t aucRxAddr[MAC_ADDR_LEN];
	OS_SYSTIME rLastSendTime;
};

enum ENUM_CHNL_SWITCH_POLICY {
	CHNL_SWITCH_POLICY_NONE,
	CHNL_SWITCH_POLICY_DEAUTH,
	CHNL_SWITCH_POLICY_CSA
};

enum ENUM_CHNL_SORT_POLICY {
	CHNL_SORT_POLICY_NONE,
	CHNL_SORT_POLICY_ALL_CN,
	CHNL_SORT_POLICY_BY_CH_DOMAIN
};

/*----------------------------------------------------------------------------*/
/* Information Element (IE) handlers                                          */
/*----------------------------------------------------------------------------*/
typedef void(*PFN_APPEND_IE_FUNC) (struct ADAPTER *,
	struct MSDU_INFO *);
typedef void(*PFN_HANDLE_IE_FUNC) (struct ADAPTER *,
	struct SW_RFB *, struct IE_HDR *);
typedef void(*PFN_VERIFY_IE_FUNC) (struct ADAPTER *,
	struct SW_RFB *, struct IE_HDR *,
	uint16_t *);
typedef uint32_t(*PFN_CALCULATE_VAR_IE_LEN_FUNC) (
	struct ADAPTER *, uint8_t, struct STA_RECORD *);

struct APPEND_IE_ENTRY {
	uint16_t u2EstimatedIELen;
	PFN_APPEND_IE_FUNC pfnAppendIE;
};

struct APPEND_VAR_IE_ENTRY {
	uint16_t u2EstimatedFixedIELen;	/* For Fixed Length */
	PFN_CALCULATE_VAR_IE_LEN_FUNC pfnCalculateVariableIELen;
	PFN_APPEND_IE_FUNC pfnAppendIE;
};

struct HANDLE_IE_ENTRY {
	uint8_t ucElemID;
	PFN_HANDLE_IE_FUNC pfnHandleIE;
};

struct VERIFY_IE_ENTRY {
	uint8_t ucElemID;
	PFN_VERIFY_IE_FUNC pfnVarifyIE;
};

/*----------------------------------------------------------------------------*/
/* Parameters of User Configuration                                           */
/*----------------------------------------------------------------------------*/
enum ENUM_PARAM_CONNECTION_POLICY {
	CONNECT_BY_SSID_BEST_RSSI = 0,
	CONNECT_BY_SSID_GOOD_RSSI_MIN_CH_LOAD,
	CONNECT_BY_SSID_ANY,	/* NOTE(Kevin): Needed by WHQL */
	CONNECT_BY_BSSID,
	CONNECT_BY_BSSID_HINT,
	CONNECT_BY_CUSTOMIZED_RULE	/* NOTE(Kevin): TBD */
};

enum ENUM_PARAM_PREAMBLE_TYPE {
	PREAMBLE_TYPE_LONG = 0,
	PREAMBLE_TYPE_SHORT,
	/*!< Try preamble short first, if fail tray preamble long. */
	PREAMBLE_TYPE_AUTO
};

/* This is enum defined for user to select a phy config listed in combo box */
enum ENUM_PARAM_PHY_CONFIG {
	/* Can associated with 802.11abg AP but without n capability,
	 * Scan dual band.
	 */
	PHY_CONFIG_802_11ABG = 0,
	/* Can associated with 802_11bg AP,
	 * Scan single band and not report 5G BSSs.
	 */
	PHY_CONFIG_802_11BG,
	/* Can associated with 802_11g only AP,
	 * Scan single band and not report 5G BSSs.
	 */
	PHY_CONFIG_802_11G,
	/* Can associated with 802_11a only AP,
	 * Scan single band and not report 2.4G BSSs.
	 */
	PHY_CONFIG_802_11A,
	/* Can associated with 802_11b only AP,
	 * Scan single band and not report 5G BSSs.
	 */
	PHY_CONFIG_802_11B,
	/* Can associated with 802.11abgn AP, Scan dual band. */
	PHY_CONFIG_802_11ABGN,
	/* Can associated with 802_11bgn AP,
	 * Scan single band and not report 5G BSSs.
	 */
	PHY_CONFIG_802_11BGN,
	/* Can associated with 802_11an AP,
	 * Scan single band and not report 2.4G BSSs.
	 */
	PHY_CONFIG_802_11AN,
	/* Can associated with 802_11gn AP,
	 * Scan single band and not report 5G BSSs.
	 */
	PHY_CONFIG_802_11GN,
	PHY_CONFIG_802_11AC,
	PHY_CONFIG_802_11ANAC,
	PHY_CONFIG_802_11ABGNAC,
#if (CFG_SUPPORT_802_11AX == 1)
	PHY_CONFIG_802_11ABGNACAX,
#endif
	PHY_CONFIG_NUM		/* 12 */
};

/* This is enum defined for user to select an AP Mode */
enum ENUM_PARAM_AP_MODE {
	/* Create 11b BSS if we support 802.11abg/802.11bg. */
	AP_MODE_11B = 0,
	/* Create 11bg mixed BSS if we support 802.11abg/802.11bg/802.11g. */
	AP_MODE_MIXED_11BG,
	/* Create 11g only BSS if we support 802.11abg/802.11bg/802.11g. */
	AP_MODE_11G,
	/* Create 11g only BSS for P2P
	 * if we support 802.11abg/802.11bg/802.11g.
	 */
	AP_MODE_11G_P2P,
	/* Create 11a only BSS if we support 802.11abg. */
	AP_MODE_11A,
	AP_MODE_NUM		/* 4 */
};

/* Masks for determining the Network Type
 * or the Station Role, given the ENUM_STA_TYPE_T
 */
#define STA_TYPE_LEGACY_MASK                BIT(STA_TYPE_LEGACY_INDEX)
#define STA_TYPE_P2P_MASK                   BIT(STA_TYPE_P2P_INDEX)
#define STA_TYPE_BOW_MASK                   BIT(STA_TYPE_BOW_INDEX)
#define STA_TYPE_ADHOC_MASK                 BIT(STA_ROLE_ADHOC_INDEX)
#define STA_TYPE_CLIENT_MASK                BIT(STA_ROLE_CLIENT_INDEX)
#define STA_TYPE_AP_MASK                    BIT(STA_ROLE_AP_INDEX)
#define STA_TYPE_DLS_MASK                   BIT(STA_ROLE_DLS_INDEX)

/* Macros for obtaining the Network Type
 * or the Station Role, given the ENUM_STA_TYPE_T
 */
#define IS_STA_IN_AIS(_prStaRec) \
	(prAdapter->aprBssInfo[(_prStaRec)->ucBssIndex]->eNetworkType \
	== NETWORK_TYPE_AIS)
#define IS_STA_IN_P2P(_prStaRec) \
	(prAdapter->aprBssInfo[(_prStaRec)->ucBssIndex]->eNetworkType \
	== NETWORK_TYPE_P2P)
#define IS_STA_LEGACY_TYPE(_prStaRec) \
	((_prStaRec->eStaType) & STA_TYPE_LEGACY_MASK)
#define IS_STA_P2P_TYPE(_prStaRec) \
	((_prStaRec->eStaType) & STA_TYPE_P2P_MASK)
#define IS_STA_BOW_TYPE(_prStaRec) \
	((_prStaRec->eStaType) & STA_TYPE_BOW_MASK)
#define IS_ADHOC_STA(_prStaRec) \
	((_prStaRec->eStaType) & STA_TYPE_ADHOC_MASK)
#define IS_CLIENT_STA(_prStaRec) \
	((_prStaRec->eStaType) & STA_TYPE_CLIENT_MASK)
#define IS_AP_STA(_prStaRec) \
	((_prStaRec->eStaType) & STA_TYPE_AP_MASK)
#define IS_DLS_STA(_prStaRec) \
	((_prStaRec->eStaType) & STA_TYPE_DLS_MASK)

/* The ENUM_STA_TYPE_T accounts for
 * ENUM_NETWORK_TYPE_T and ENUM_STA_ROLE_INDEX_T.
 *   It is a merged version of Network Type and STA Role.
 */
enum ENUM_STA_TYPE {
	STA_TYPE_LEGACY_AP = (STA_TYPE_LEGACY_MASK | STA_TYPE_AP_MASK),
	STA_TYPE_LEGACY_CLIENT = (STA_TYPE_LEGACY_MASK | STA_TYPE_CLIENT_MASK),
	STA_TYPE_ADHOC_PEER = (STA_TYPE_LEGACY_MASK | STA_TYPE_ADHOC_MASK),
#if CFG_ENABLE_WIFI_DIRECT
	STA_TYPE_P2P_GO = (STA_TYPE_P2P_MASK | STA_TYPE_AP_MASK),
	STA_TYPE_P2P_GC = (STA_TYPE_P2P_MASK | STA_TYPE_CLIENT_MASK),
#endif
#if CFG_ENABLE_BT_OVER_WIFI
	STA_TYPE_BOW_AP = (STA_TYPE_BOW_MASK | STA_TYPE_AP_MASK),
	STA_TYPE_BOW_CLIENT = (STA_TYPE_BOW_MASK | STA_TYPE_CLIENT_MASK),
#endif
	STA_TYPE_DLS_PEER = (STA_TYPE_LEGACY_MASK | STA_TYPE_DLS_MASK),
};

/* The type of BSS we discovered */
enum ENUM_BSS_TYPE {
	BSS_TYPE_INFRASTRUCTURE = 1,
	BSS_TYPE_IBSS,
	BSS_TYPE_P2P_DEVICE,
	BSS_TYPE_BOW_DEVICE,
	BSS_TYPE_NUM
};

enum ENUM_ANTENNA_NUM {
	ANTENNA_WF0 = 0,
	ANTENNA_WF1 = 1,
	MAX_ANTENNA_NUM
};

/*----------------------------------------------------------------------------*/
/* RSN structures                                                             */
/*----------------------------------------------------------------------------*/
/* #if defined(WINDOWS_DDK) || defined(WINDOWS_CE) */
/* #pragma pack(1) */
/* #endif */

/* max number of supported cipher suites */
#define MAX_NUM_SUPPORTED_CIPHER_SUITES 10
#if CFG_SUPPORT_802_11W
/* max number of supported AKM suites */
#define MAX_NUM_SUPPORTED_AKM_SUITES    15
#else
/* max number of supported AKM suites */
#define MAX_NUM_SUPPORTED_AKM_SUITES    13
#endif

/* Structure of RSN Information */
struct RSN_INFO {
	uint8_t ucElemId;
	uint16_t u2Version;
	uint32_t u4GroupKeyCipherSuite;
	uint32_t u4PairwiseKeyCipherSuiteCount;
	uint32_t au4PairwiseKeyCipherSuite[MAX_NUM_SUPPORTED_CIPHER_SUITES];
	uint32_t u4AuthKeyMgtSuiteCount;
	uint32_t au4AuthKeyMgtSuite[MAX_NUM_SUPPORTED_AKM_SUITES];
	uint16_t u2RsnCap;
	u_int8_t fgRsnCapPresent;
	uint16_t u2PmkidCount;
	uint8_t aucPmkid[IW_PMKID_LEN];
} __KAL_ATTRIB_PACKED__;

/* max number of supported AKM suites */
#define MAX_NUM_SUPPORTED_WAPI_AKM_SUITES    1
/* max number of supported cipher suites */
#define MAX_NUM_SUPPORTED_WAPI_CIPHER_SUITES 1

/* Structure of WAPI Information */
struct WAPI_INFO {
	uint8_t ucElemId;
	uint8_t ucLength;
	uint16_t u2Version;
	uint32_t u4AuthKeyMgtSuiteCount;
	uint32_t au4AuthKeyMgtSuite[MAX_NUM_SUPPORTED_WAPI_AKM_SUITES];
	uint32_t u4PairwiseKeyCipherSuiteCount;

	uint32_t
		au4PairwiseKeyCipherSuite[MAX_NUM_SUPPORTED_WAPI_CIPHER_SUITES];
	uint32_t u4GroupKeyCipherSuite;
	uint16_t u2WapiCap;
	uint16_t u2Bkid;
	uint8_t aucBkid[1][16];
} __KAL_ATTRIB_PACKED__;

/* #if defined(WINDOWS_DDK) || defined(WINDOWS_CE) */
/* #pragma pack() */
/* #endif */

#if CFG_ENABLE_WIFI_DIRECT

struct P2P_DEVICE_TYPE {
	uint16_t u2CategoryID;
	uint16_t u2SubCategoryID;
};

struct P2P_DEVICE_DESC {
	struct LINK_ENTRY rLinkEntry;
	u_int8_t fgDevInfoValid;
	uint8_t aucDeviceAddr[MAC_ADDR_LEN];	/* Device Address. */
	uint8_t aucInterfaceAddr[MAC_ADDR_LEN];	/* Interface Address. */
	uint8_t ucDeviceCapabilityBitmap;
	uint8_t ucGroupCapabilityBitmap;
	uint16_t u2ConfigMethod;	/* Configure Method support. */
	struct P2P_DEVICE_TYPE rPriDevType;
	uint8_t ucSecDevTypeNum;
	/* Reference to P2P_GC_MAX_CACHED_SEC_DEV_TYPE_COUNT */
	struct P2P_DEVICE_TYPE arSecDevType[8];
	uint16_t u2NameLength;
	uint8_t aucName[32];	/* Reference to WPS_ATTRI_MAX_LEN_DEVICE_NAME */
	/* TODO: Service Information or PasswordID valid? */
};

#endif

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */
/* Macros to get and set the wireless LAN frame fields
 * those are 16/32 bits in length.
 */
#define WLAN_GET_FIELD_16(_memAddr_p, _value_p) \
	{ \
		uint8_t *__cp = (uint8_t *)(_memAddr_p); \
		*(uint16_t *)(_value_p) = ((uint16_t)__cp[0]) | \
			((uint16_t)__cp[1] << 8); \
	}

#define WLAN_GET_FIELD_BE16(_memAddr_p, _value_p) \
	{ \
		uint8_t *__cp = (uint8_t *)(_memAddr_p); \
		*(uint16_t *)(_value_p) = ((uint16_t)__cp[0] << 8) | \
			((uint16_t)__cp[1]); \
	}

#define WLAN_GET_FIELD_24(_memAddr_p, _value_p) \
	{ \
		uint8_t *__cp = (uint8_t *)(_memAddr_p); \
		*(uint32_t *)(_value_p) = 0; \
		*(uint32_t *)(_value_p) = ((uint32_t)__cp[0]) | \
			((uint32_t)__cp[1] << 8) | \
			((uint32_t)__cp[2] << 16); \
	}

#define WLAN_GET_FIELD_BE24(_memAddr_p, _value_p) \
	{ \
		uint8_t *__cp = (uint8_t *)(_memAddr_p); \
		*(uint32_t *)(_value_p) = 0; \
		*(uint32_t *)(_value_p) = ((uint32_t)__cp[0] << 16) | \
		    ((uint32_t)__cp[1] << 8) | (uint32_t)__cp[2]; \
	}

#define WLAN_GET_FIELD_32(_memAddr_p, _value_p) \
	{ \
		uint8_t *__cp = (uint8_t *)(_memAddr_p); \
		*(uint32_t *)(_value_p) = ((uint32_t)__cp[0]) | \
			((uint32_t)__cp[1] << 8) | \
			((uint32_t)__cp[2] << 16) | ((uint32_t)__cp[3] << 24); \
	}

#define WLAN_GET_FIELD_BE32(_memAddr_p, _value_p) \
	{ \
		uint8_t *__cp = (uint8_t *)(_memAddr_p); \
		*(uint32_t *)(_value_p) = ((uint32_t)__cp[0] << 24) | \
		    ((uint32_t)__cp[1] << 16) | ((uint32_t)__cp[2] << 8) | \
		    ((uint32_t)__cp[3]); \
	}

#define WLAN_GET_FIELD_64(_memAddr_p, _value_p) \
	{ \
		uint8_t *__cp = (uint8_t *)(_memAddr_p); \
		*(uint64_t *)(_value_p) = \
			((uint64_t)__cp[0]) | ((uint64_t)__cp[1] << 8) | \
			((uint64_t)__cp[2] << 16) | \
			((uint64_t)__cp[3] << 24) | \
			((uint64_t)__cp[4] << 32) | \
			((uint64_t)__cp[5] << 40) | \
			((uint64_t)__cp[6] << 48) | \
			((uint64_t)__cp[7] << 56); \
	}

#define WLAN_SET_FIELD_16(_memAddr_p, _value) \
	{ \
		uint8_t *__cp = (uint8_t *)(_memAddr_p); \
		__cp[0] = (uint8_t)(_value); \
		__cp[1] = (uint8_t)((_value) >> 8); \
	}

#define WLAN_SET_FIELD_64(_memAddr_p, _value) \
	{ \
		uint8_t *__cp = (uint8_t *)(_memAddr_p); \
		__cp[0] = (uint8_t)((_value) & 0xff); \
		__cp[1] = (uint8_t)((_value) >> 8); \
		__cp[2] = (uint8_t)((_value) >> 16); \
		__cp[3] = (uint8_t)((_value) >> 24); \
		__cp[4] = (uint8_t)((_value) >> 32); \
		__cp[5] = (uint8_t)((_value) >> 40); \
		__cp[6] = (uint8_t)((_value) >> 48); \
		__cp[7] = (uint8_t)((_value) >> 56); \
	}

#define WLAN_SET_FIELD_BE16(_memAddr_p, _value) \
	{ \
		uint8_t *__cp = (uint8_t *)(_memAddr_p); \
		__cp[0] = (uint8_t)((_value) >> 8); \
		__cp[1] = (uint8_t)(_value); \
	}

#define WLAN_SET_FIELD_32(_memAddr_p, _value) \
	{ \
		uint8_t *__cp = (uint8_t *)(_memAddr_p); \
		__cp[0] = (uint8_t)(_value); \
		__cp[1] = (uint8_t)((_value) >> 8); \
		__cp[2] = (uint8_t)((_value) >> 16); \
		__cp[3] = (uint8_t)((_value) >> 24); \
	}

#define WLAN_SET_FIELD_BE24(_memAddr_p, _value) \
	{ \
		uint8_t *__cp = (uint8_t *)(_memAddr_p); \
		__cp[0] = (uint8_t)((_value) >> 16); \
		__cp[1] = (uint8_t)((_value) >> 8); \
		__cp[2] = (uint8_t)(_value); \
	}

#define WLAN_SET_FIELD_BE32(_memAddr_p, _value) \
	{ \
		uint8_t *__cp = (uint8_t *)(_memAddr_p); \
		__cp[0] = (uint8_t)((_value) >> 24); \
		__cp[1] = (uint8_t)((_value) >> 16); \
		__cp[2] = (uint8_t)((_value) >> 8); \
		__cp[3] = (uint8_t)(_value); \
	}

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

#endif /* _WLAN_DEF_H */
