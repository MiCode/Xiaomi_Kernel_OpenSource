/*******************************************************************************
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
 ******************************************************************************/
/*
 ** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include
 *      /CFG_Wifi_File.h#1
 */

/*! \file   CFG_Wifi_File.h
 *    \brief  Collection of NVRAM structure used for YuSu project
 *
 *    In this file we collect all compiler flags and detail the driver behavior
 *    if enable/disable such switch or adjust numeric parameters.
 */

#ifndef _CFG_WIFI_FILE_H
#define _CFG_WIFI_FILE_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "gl_typedef.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */
/* duplicated from nic_cmd_event.h to avoid header dependency */
struct TX_PWR_PARAM {
	int8_t cTxPwr2G4Cck;	/* signed, in unit of 0.5dBm */
	int8_t cTxPwr2G4Dsss;	/* signed, in unit of 0.5dBm */
	int8_t acReserved[2];

	int8_t cTxPwr2G4OFDM_BPSK;
	int8_t cTxPwr2G4OFDM_QPSK;
	int8_t cTxPwr2G4OFDM_16QAM;
	int8_t cTxPwr2G4OFDM_Reserved;
	int8_t cTxPwr2G4OFDM_48Mbps;
	int8_t cTxPwr2G4OFDM_54Mbps;

	int8_t cTxPwr2G4HT20_BPSK;
	int8_t cTxPwr2G4HT20_QPSK;
	int8_t cTxPwr2G4HT20_16QAM;
	int8_t cTxPwr2G4HT20_MCS5;
	int8_t cTxPwr2G4HT20_MCS6;
	int8_t cTxPwr2G4HT20_MCS7;

	int8_t cTxPwr2G4HT40_BPSK;
	int8_t cTxPwr2G4HT40_QPSK;
	int8_t cTxPwr2G4HT40_16QAM;
	int8_t cTxPwr2G4HT40_MCS5;
	int8_t cTxPwr2G4HT40_MCS6;
	int8_t cTxPwr2G4HT40_MCS7;

	int8_t cTxPwr5GOFDM_BPSK;
	int8_t cTxPwr5GOFDM_QPSK;
	int8_t cTxPwr5GOFDM_16QAM;
	int8_t cTxPwr5GOFDM_Reserved;
	int8_t cTxPwr5GOFDM_48Mbps;
	int8_t cTxPwr5GOFDM_54Mbps;

	int8_t cTxPwr5GHT20_BPSK;
	int8_t cTxPwr5GHT20_QPSK;
	int8_t cTxPwr5GHT20_16QAM;
	int8_t cTxPwr5GHT20_MCS5;
	int8_t cTxPwr5GHT20_MCS6;
	int8_t cTxPwr5GHT20_MCS7;

	int8_t cTxPwr5GHT40_BPSK;
	int8_t cTxPwr5GHT40_QPSK;
	int8_t cTxPwr5GHT40_16QAM;
	int8_t cTxPwr5GHT40_MCS5;
	int8_t cTxPwr5GHT40_MCS6;
	int8_t cTxPwr5GHT40_MCS7;
};

struct TX_AC_PWR {
	int8_t c11AcTxPwr_BPSK;
	int8_t c11AcTxPwr_QPSK;
	int8_t c11AcTxPwr_16QAM;
	int8_t c11AcTxPwr_MCS5_MCS6;
	int8_t c11AcTxPwr_MCS7;
	int8_t c11AcTxPwr_MCS8;
	int8_t c11AcTxPwr_MCS9;
	int8_t c11AcTxPwrVht40_OFFSET;
	int8_t c11AcTxPwrVht80_OFFSET;
	int8_t c11AcTxPwrVht160_OFFSET;
	int8_t acReverse[2];
};

struct RSSI_PATH_COMPASATION {
	int8_t c2GRssiCompensation;
	int8_t c5GRssiCompensation;
};

struct PWR_5G_OFFSET {
	int8_t cOffsetBand0;	/* 4.915-4.980G */
	int8_t cOffsetBand1;	/* 5.000-5.080G */
	int8_t cOffsetBand2;	/* 5.160-5.180G */
	int8_t cOffsetBand3;	/* 5.200-5.280G */
	int8_t cOffsetBand4;	/* 5.300-5.340G */
	int8_t cOffsetBand5;	/* 5.500-5.580G */
	int8_t cOffsetBand6;	/* 5.600-5.680G */
	int8_t cOffsetBand7;	/* 5.700-5.825G */
};

struct PWR_PARAM {
	uint32_t au4Data[28];
	uint32_t u4RefValue1;
	uint32_t u4RefValue2;
};

#if 0
struct WIFI_CFG_PARAM_STRUCT {
	/* 256 bytes of MP data */
	uint16_t u2Part1OwnVersion;
	uint16_t u2Part1PeerVersion;
	uint8_t aucMacAddress[6];
	uint8_t aucCountryCode[2];
	struct TX_PWR_PARAM rTxPwr;
	uint8_t aucEFUSE[144];
	uint8_t ucTxPwrValid;
	uint8_t ucSupport5GBand;
	uint8_t fg2G4BandEdgePwrUsed;
	int8_t cBandEdgeMaxPwrCCK;
	int8_t cBandEdgeMaxPwrOFDM20;
	int8_t cBandEdgeMaxPwrOFDM40;

	uint8_t ucRegChannelListMap;
	uint8_t ucRegChannelListIndex;
	uint8_t aucRegSubbandInfo[36];

	uint8_t aucReserved2[256 - 240];

	/* 256 bytes of function data */
	uint16_t u2Part2OwnVersion;
	uint16_t u2Part2PeerVersion;
	uint8_t uc2G4BwFixed20M;
	uint8_t uc5GBwFixed20M;
	uint8_t ucEnable5GBand;
	uint8_t aucPreTailReserved;
	uint8_t aucTailReserved[256 - 8];
};
#else

struct AC_PWR_SETTING_STRUCT {
	uint8_t c11AcTxPwr_BPSK;
	uint8_t c11AcTxPwr_QPSK;
	uint8_t c11AcTxPwr_16QAM;
	uint8_t c11AcTxPwr_MCS5_MCS6;
	uint8_t c11AcTxPwr_MCS7;
	uint8_t c11AcTxPwr_MCS8;
	uint8_t c11AcTxPwr_MCS9;
	uint8_t c11AcTxPwr_Reserved;
	uint8_t c11AcTxPwrVht40_OFFSET;
	uint8_t c11AcTxPwrVht80_OFFSET;
	uint8_t c11AcTxPwrVht160_OFFSET;
};

struct BANDEDGE_5G {
	uint8_t uc5GBandEdgePwrUsed;
	uint8_t c5GBandEdgeMaxPwrOFDM20;
	uint8_t c5GBandEdgeMaxPwrOFDM40;
	uint8_t c5GBandEdgeMaxPwrOFDM80;

};

struct NEW_EFUSE_MAPPING2NVRAM {
	uint8_t ucReverse1[8];
	uint16_t u2Signature;
	struct BANDEDGE_5G r5GBandEdgePwr;
	uint8_t ucReverse2[14];

	/* 0x50 */
	uint8_t aucChOffset[3];
	uint8_t ucChannelOffsetVaild;
	uint8_t acAllChannelOffset;
	uint8_t aucChOffset3[11];

	/* 0x60 */
	uint8_t auc5GChOffset[8];
	uint8_t uc5GChannelOffsetVaild;
	uint8_t aucChOffset4[7];

	/* 0x70 */
	struct AC_PWR_SETTING_STRUCT r11AcTxPwr;
	uint8_t uc11AcTxPwrValid;

	uint8_t ucReverse4[20];

	/* 0x90 */
	struct AC_PWR_SETTING_STRUCT r11AcTxPwr2G;
	uint8_t uc11AcTxPwrValid2G;

	uint8_t ucReverse5[40];
};

struct WIFI_CFG_PARAM_STRUCT {
	/* 256 bytes of MP data */
	uint16_t u2Part1OwnVersion;
	uint16_t u2Part1PeerVersion;
	uint8_t aucMacAddress[6];
	uint8_t aucCountryCode[2];
	struct TX_PWR_PARAM rTxPwr;
#if CFG_SUPPORT_NVRAM_5G
	union {
		struct NEW_EFUSE_MAPPING2NVRAM u;
		uint8_t aucEFUSE[144];
	} EfuseMapping;
#else
	uint8_t aucEFUSE[144];
#endif
	uint8_t ucTxPwrValid;
	uint8_t ucSupport5GBand;
	uint8_t fg2G4BandEdgePwrUsed;
	int8_t cBandEdgeMaxPwrCCK;
	int8_t cBandEdgeMaxPwrOFDM20;
	int8_t cBandEdgeMaxPwrOFDM40;

	uint8_t ucRegChannelListMap;
	uint8_t ucRegChannelListIndex;
	uint8_t aucRegSubbandInfo[36];

	uint8_t aucReserved2[256 - 240];

	/* 256 bytes of function data */
	uint16_t u2Part2OwnVersion;
	uint16_t u2Part2PeerVersion;
	uint8_t uc2G4BwFixed20M;
	uint8_t uc5GBwFixed20M;
	uint8_t ucEnable5GBand;
	uint8_t ucRxDiversity;
	struct RSSI_PATH_COMPASATION rRssiPathCompensation;
	uint8_t fgRssiCompensationVaildbit;
	uint8_t ucGpsDesense;
	uint16_t u2FeatureReserved;
	uint8_t aucPreTailReserved;
	uint8_t aucTailReserved[256 - 15];
};

#endif
/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */
#ifndef DATA_STRUCT_INSPECTING_ASSERT
#define DATA_STRUCT_INSPECTING_ASSERT(expr) \
		{switch (0) {case 0: case (expr): default:; } }
#endif

#define CFG_FILE_WIFI_REC_SIZE    sizeof(struct WIFI_CFG_PARAM_STRUCT)

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
#ifndef _lint
/* We don't have to call following function to inspect the data structure.
 * It will check automatically while at compile time.
 * We'll need this to guarantee the same member order in different structures
 * to simply handling effort in some functions.
 */
static __KAL_INLINE__ void nvramOffsetCheck(void)
{
	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct WIFI_CFG_PARAM_STRUCT, u2Part2OwnVersion) == 256);

	DATA_STRUCT_INSPECTING_ASSERT(sizeof(
		struct WIFI_CFG_PARAM_STRUCT) == 512);
#if CFG_SUPPORT_NVRAM_5G
	DATA_STRUCT_INSPECTING_ASSERT((OFFSET_OF(
		struct WIFI_CFG_PARAM_STRUCT, EfuseMapping) & 0x0001)
		== 0);
#else
	DATA_STRUCT_INSPECTING_ASSERT((OFFSET_OF(
		struct WIFI_CFG_PARAM_STRUCT, aucEFUSE) & 0x0001)
		== 0);
#endif
	DATA_STRUCT_INSPECTING_ASSERT((OFFSET_OF(
		struct WIFI_CFG_PARAM_STRUCT, aucRegSubbandInfo) & 0x0001)
		== 0);
}
#endif

#endif /* _CFG_WIFI_FILE_H */
