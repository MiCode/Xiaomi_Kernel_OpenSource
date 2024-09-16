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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/CFG_Wifi_File.h#1
*/

/*! \file   CFG_Wifi_File.h
*    \brief  Collection of NVRAM structure used for YuSu project
*
*    In this file we collect all compiler flags and detail the driver behavior if
*    enable/disable such switch or adjust numeric parameters.
*/

#ifndef _CFG_WIFI_FILE_H
#define _CFG_WIFI_FILE_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "gl_typedef.h"

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
/* duplicated from nic_cmd_event.h to avoid header dependency */
typedef struct _TX_PWR_PARAM_T {
	INT_8 cTxPwr2G4Cck;	/* signed, in unit of 0.5dBm */
	INT_8 cTxPwr2G4Dsss;	/* signed, in unit of 0.5dBm */
	INT_8 acReserved[2];

	INT_8 cTxPwr2G4OFDM_BPSK;
	INT_8 cTxPwr2G4OFDM_QPSK;
	INT_8 cTxPwr2G4OFDM_16QAM;
	INT_8 cTxPwr2G4OFDM_Reserved;
	INT_8 cTxPwr2G4OFDM_48Mbps;
	INT_8 cTxPwr2G4OFDM_54Mbps;

	INT_8 cTxPwr2G4HT20_BPSK;
	INT_8 cTxPwr2G4HT20_QPSK;
	INT_8 cTxPwr2G4HT20_16QAM;
	INT_8 cTxPwr2G4HT20_MCS5;
	INT_8 cTxPwr2G4HT20_MCS6;
	INT_8 cTxPwr2G4HT20_MCS7;

	INT_8 cTxPwr2G4HT40_BPSK;
	INT_8 cTxPwr2G4HT40_QPSK;
	INT_8 cTxPwr2G4HT40_16QAM;
	INT_8 cTxPwr2G4HT40_MCS5;
	INT_8 cTxPwr2G4HT40_MCS6;
	INT_8 cTxPwr2G4HT40_MCS7;

	INT_8 cTxPwr5GOFDM_BPSK;
	INT_8 cTxPwr5GOFDM_QPSK;
	INT_8 cTxPwr5GOFDM_16QAM;
	INT_8 cTxPwr5GOFDM_Reserved;
	INT_8 cTxPwr5GOFDM_48Mbps;
	INT_8 cTxPwr5GOFDM_54Mbps;

	INT_8 cTxPwr5GHT20_BPSK;
	INT_8 cTxPwr5GHT20_QPSK;
	INT_8 cTxPwr5GHT20_16QAM;
	INT_8 cTxPwr5GHT20_MCS5;
	INT_8 cTxPwr5GHT20_MCS6;
	INT_8 cTxPwr5GHT20_MCS7;

	INT_8 cTxPwr5GHT40_BPSK;
	INT_8 cTxPwr5GHT40_QPSK;
	INT_8 cTxPwr5GHT40_16QAM;
	INT_8 cTxPwr5GHT40_MCS5;
	INT_8 cTxPwr5GHT40_MCS6;
	INT_8 cTxPwr5GHT40_MCS7;
} TX_PWR_PARAM_T, *P_TX_PWR_PARAM_T;

typedef struct _TX_AC_PWR_T {
	INT_8 c11AcTxPwr_BPSK;
	INT_8 c11AcTxPwr_QPSK;
	INT_8 c11AcTxPwr_16QAM;
	INT_8 c11AcTxPwr_MCS5_MCS6;
	INT_8 c11AcTxPwr_MCS7;
	INT_8 c11AcTxPwr_MCS8;
	INT_8 c11AcTxPwr_MCS9;
	INT_8 c11AcTxPwrVht40_OFFSET;
	INT_8 c11AcTxPwrVht80_OFFSET;
	INT_8 c11AcTxPwrVht160_OFFSET;
	INT_8 acReverse[2];
} TX_AC_PWR_T, *P_TX_AC_PWR_T;

typedef struct _RSSI_PATH_COMPASATION_T {
	INT_8 c2GRssiCompensation;
	INT_8 c5GRssiCompensation;
} RSSI_PATH_COMPASATION_T, *P_RSSI_PATH_COMPASATION_T;

typedef struct _PWR_5G_OFFSET_T {
	INT_8 cOffsetBand0;	/* 4.915-4.980G */
	INT_8 cOffsetBand1;	/* 5.000-5.080G */
	INT_8 cOffsetBand2;	/* 5.160-5.180G */
	INT_8 cOffsetBand3;	/* 5.200-5.280G */
	INT_8 cOffsetBand4;	/* 5.300-5.340G */
	INT_8 cOffsetBand5;	/* 5.500-5.580G */
	INT_8 cOffsetBand6;	/* 5.600-5.680G */
	INT_8 cOffsetBand7;	/* 5.700-5.825G */
} PWR_5G_OFFSET_T, *P_PWR_5G_OFFSET_T;

typedef struct _PWR_PARAM_T {
	UINT_32 au4Data[28];
	UINT_32 u4RefValue1;
	UINT_32 u4RefValue2;
} PWR_PARAM_T, *P_PWR_PARAM_T;

#if 0
typedef struct _MT6620_CFG_PARAM_STRUCT {
	/* 256 bytes of MP data */
	UINT_16 u2Part1OwnVersion;
	UINT_16 u2Part1PeerVersion;
	UINT_8 aucMacAddress[6];
	UINT_8 aucCountryCode[2];
	TX_PWR_PARAM_T rTxPwr;
	UINT_8 aucEFUSE[144];
	UINT_8 ucTxPwrValid;
	UINT_8 ucSupport5GBand;
	UINT_8 fg2G4BandEdgePwrUsed;
	INT_8 cBandEdgeMaxPwrCCK;
	INT_8 cBandEdgeMaxPwrOFDM20;
	INT_8 cBandEdgeMaxPwrOFDM40;

	UINT_8 ucRegChannelListMap;
	UINT_8 ucRegChannelListIndex;
	UINT_8 aucRegSubbandInfo[36];

	UINT_8 aucReserved2[256 - 240];

	/* 256 bytes of function data */
	UINT_16 u2Part2OwnVersion;
	UINT_16 u2Part2PeerVersion;
	UINT_8 uc2G4BwFixed20M;
	UINT_8 uc5GBwFixed20M;
	UINT_8 ucEnable5GBand;
	UINT_8 aucPreTailReserved;
	UINT_8 aucTailReserved[256 - 8];
} MT6620_CFG_PARAM_STRUCT, *P_MT6620_CFG_PARAM_STRUCT, WIFI_CFG_PARAM_STRUCT, *P_WIFI_CFG_PARAM_STRUCT;
#else

typedef struct _AC_PWR_SETTING_STRUCT {
	UINT_8 c11AcTxPwr_BPSK;
	UINT_8 c11AcTxPwr_QPSK;
	UINT_8 c11AcTxPwr_16QAM;
	UINT_8 c11AcTxPwr_MCS5_MCS6;
	UINT_8 c11AcTxPwr_MCS7;
	UINT_8 c11AcTxPwr_MCS8;
	UINT_8 c11AcTxPwr_MCS9;
	UINT_8 c11AcTxPwr_Reserved;
	UINT_8 c11AcTxPwrVht40_OFFSET;
	UINT_8 c11AcTxPwrVht80_OFFSET;
	UINT_8 c11AcTxPwrVht160_OFFSET;
} AC_PWR_SETTING_STRUCT, *P_AC_PWR_SETTING_STRUCT;

typedef struct _BANDEDGE_5G_T {
	UINT_8 uc5GBandEdgePwrUsed;
	UINT_8 c5GBandEdgeMaxPwrOFDM20;
	UINT_8 c5GBandEdgeMaxPwrOFDM40;
	UINT_8 c5GBandEdgeMaxPwrOFDM80;

} BANDEDGE_5G_T, *P_BANDEDGE_5G_T;

typedef struct _NEW_EFUSE_MAPPING2NVRAM_T {
	UINT_8 ucReverse1[8];
	UINT_16 u2Signature;
	BANDEDGE_5G_T r5GBandEdgePwr;
	UINT_8 ucReverse2[14];

	/* 0x50 */
	UINT_8 aucChOffset[3];
	UINT_8 ucChannelOffsetVaild;
	UINT_8 acAllChannelOffset;
	UINT_8 aucChOffset3[11];

	/* 0x60 */
	UINT_8 auc5GChOffset[8];
	UINT_8 uc5GChannelOffsetVaild;
	UINT_8 aucChOffset4[7];

	/* 0x70 */
	AC_PWR_SETTING_STRUCT r11AcTxPwr;
	UINT_8 uc11AcTxPwrValid;

	UINT_8 ucReverse4[20];

	/* 0x90 */
	AC_PWR_SETTING_STRUCT r11AcTxPwr2G;
	UINT_8 uc11AcTxPwrValid2G;

	UINT_8 ucReverse5[40];
} NEW_EFUSE_MAPPING2NVRAM_T, *P_NEW_EFUSE_MAPPING2NVRAM_T;

typedef struct _MT6620_CFG_PARAM_STRUCT {
	/* 256 bytes of MP data */
	UINT_16 u2Part1OwnVersion;
	UINT_16 u2Part1PeerVersion;
	UINT_8 aucMacAddress[6];
	UINT_8 aucCountryCode[2];
	TX_PWR_PARAM_T rTxPwr;
#if CFG_SUPPORT_NVRAM_5G
	union {
		NEW_EFUSE_MAPPING2NVRAM_T u;
		UINT_8 aucEFUSE[144];
	} EfuseMapping;
#else
	UINT_8 aucEFUSE[144];
#endif
	UINT_8 ucTxPwrValid;
	UINT_8 ucSupport5GBand;
	UINT_8 fg2G4BandEdgePwrUsed;
	INT_8 cBandEdgeMaxPwrCCK;
	INT_8 cBandEdgeMaxPwrOFDM20;
	INT_8 cBandEdgeMaxPwrOFDM40;

	UINT_8 ucRegChannelListMap;
	UINT_8 ucRegChannelListIndex;
	UINT_8 aucRegSubbandInfo[36];

	UINT_8 aucReserved2[256 - 240];

	/* 256 bytes of function data */
	UINT_16 u2Part2OwnVersion;
	UINT_16 u2Part2PeerVersion;
	UINT_8 uc2G4BwFixed20M;
	UINT_8 uc5GBwFixed20M;
	UINT_8 ucEnable5GBand;
	UINT_8 ucRxDiversity;
	RSSI_PATH_COMPASATION_T rRssiPathCompensation;
	UINT_8 fgRssiCompensationVaildbit;
	UINT_8 ucGpsDesense;
	UINT_16 u2FeatureReserved;
	UINT_8 aucPreTailReserved;
	UINT_8 aucTailReserved[256 - 15];
} MT6620_CFG_PARAM_STRUCT, *P_MT6620_CFG_PARAM_STRUCT, WIFI_CFG_PARAM_STRUCT, *P_WIFI_CFG_PARAM_STRUCT;

#endif
/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#ifndef DATA_STRUCT_INSPECTING_ASSERT
#define DATA_STRUCT_INSPECTING_ASSERT(expr) \
		{switch (0) {case 0: case (expr): default:; } }
#endif

#define CFG_FILE_WIFI_REC_SIZE    sizeof(WIFI_CFG_PARAM_STRUCT)

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
#ifndef _lint
/* We don't have to call following function to inspect the data structure.
 * It will check automatically while at compile time.
 * We'll need this to guarantee the same member order in different structures
 * to simply handling effort in some functions.
 */
static __KAL_INLINE__ VOID nvramOffsetCheck(VOID)
{
	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(WIFI_CFG_PARAM_STRUCT, u2Part2OwnVersion) == 256);

	DATA_STRUCT_INSPECTING_ASSERT(sizeof(WIFI_CFG_PARAM_STRUCT) == 512);
#if CFG_SUPPORT_NVRAM_5G
	DATA_STRUCT_INSPECTING_ASSERT((OFFSET_OF(WIFI_CFG_PARAM_STRUCT, EfuseMapping) & 0x0001) == 0);
#else
	DATA_STRUCT_INSPECTING_ASSERT((OFFSET_OF(WIFI_CFG_PARAM_STRUCT, aucEFUSE) & 0x0001) == 0);
#endif
	DATA_STRUCT_INSPECTING_ASSERT((OFFSET_OF(WIFI_CFG_PARAM_STRUCT, aucRegSubbandInfo) & 0x0001)
				      == 0);
}
#endif

#endif /* _CFG_WIFI_FILE_H */
