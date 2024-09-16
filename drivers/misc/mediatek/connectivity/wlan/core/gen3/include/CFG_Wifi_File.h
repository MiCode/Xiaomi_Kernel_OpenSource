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
 * ! \file   CFG_Wifi_File.h
 * \brief  Collection of NVRAM structure used for YuSu project
 *
 *  In this file we collect all compiler flags and detail the driver behavior if
 *  enable/disable such switch or adjust numeric parameters.
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

#if CFG_SUPPORT_PCC
struct TXRX_STRONG_COMPENSATION_T {
	UINT_8	fgRxRxStrongComensation;
	INT_8	acTxPwrOffset_24G[14];		/* 2.4G TSSI per channel offset */
	INT_8	acTxPwrLoss_24G[14];		/* 2.4G TX path loss per channel compensation */
	INT_8	acRxCompensation_24G[14];	/* 2.4G RSSI per channel compensation */
	INT_8	acTxPwrOffset_5G_GRP[8];	/* 5G TSSI per channel offset */
	INT_8	acTxPwrLoss_5G_GRP[8];		/* 5G TX path loss per channel compensation */
	INT_8	acRxCompensation_5G[8];		/* 5G RSSI per channel compensation */
} __KAL_ATTRIB_PACKED__;

struct DPD_INFO {
	INT_8	acDPDEnable;		/* 2G DPD G0 Enable */
	INT_8	acDPDChannel1;		/* 2G Ch1 DPD Value */
	INT_8	acDPDChannel2;		/* 2G Ch2 DPD Value */
	INT_8	acDPDChannel3;		/* 2G Ch3 DPD Value */
	INT_8	acDPDChannel4;		/* 2G Ch4 DPD Value */
	INT_8	acDPDChannel5;		/* 2G Ch5 DPD Value */
	INT_8	acDPDChannel6;		/* 2G Ch6 DPD Value */
	INT_8	acDPDChannel7;		/* 2G Ch7 DPD Value */
	INT_8	acDPDChannel8;		/* 2G Ch8 DPD Value */
	INT_8	acDPDChannel9;		/* 2G Ch9 DPD Value */
	INT_8	acDPDChannel10;		/* 2G Ch10 DPD Value */
	INT_8	acDPDChannel11;		/* 2G Ch11 DPD Value */
	INT_8	acDPDChannel12;		/* 2G Ch12 DPD Value */
	INT_8	acDPDChannel13;		/* 2G Ch13 DPD Value */
	INT_8	acDPDChannel14;		/* 2G Ch14 DPD Value */
} __KAL_ATTRIB_PACKED__;
#endif

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
#if CFG_SUPPORT_PCC
	/* support PCC feature */
	struct TXRX_STRONG_COMPENSATION_T rTxRxStrComp;	/* Start addr 0x200, offset is 0x43 */
	/* support PCC feature */
	struct DPD_INFO rDPDInfo;			/* Start addr 0x243, offset is 0xF */

	/* support ANTSWAP feature */
	struct TXRX_STRONG_COMPENSATION_T rTxRxStrCompAnt1; /* Start addr 0x252, offset is 0x43 */
	/* support ANTSWAP feature */
	struct DPD_INFO rDPDInfoAnt1;

	UINT_8 aucTailPCCReserved[512-67-15-67-15];		/* Start addr 0x252, offset is 0x1AE */
#endif
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
#if CFG_SUPPORT_PCC
#define EXTEND_NVRAM_SIZE 1024
#else
#define EXTEND_NVRAM_SIZE 512
#endif

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

#if CFG_SUPPORT_PCC
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(WIFI_CFG_PARAM_STRUCT) == EXTEND_NVRAM_SIZE);
#else
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(WIFI_CFG_PARAM_STRUCT) == 512);
#endif
#if CFG_SUPPORT_NVRAM_5G
	DATA_STRUCT_INSPECTING_ASSERT((OFFSET_OF(WIFI_CFG_PARAM_STRUCT, EfuseMapping) & 0x0001) == 0);
#else
	DATA_STRUCT_INSPECTING_ASSERT((OFFSET_OF(WIFI_CFG_PARAM_STRUCT, aucEFUSE) & 0x0001) == 0);
#endif
	DATA_STRUCT_INSPECTING_ASSERT((OFFSET_OF(WIFI_CFG_PARAM_STRUCT, aucRegSubbandInfo) & 0x0001)
				     == 0);
#if CFG_SUPPORT_PCC
	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(WIFI_CFG_PARAM_STRUCT, ucRxDiversity) == 263);

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(WIFI_CFG_PARAM_STRUCT, fgRssiCompensationVaildbit) == 266);
	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(WIFI_CFG_PARAM_STRUCT, u2FeatureReserved) == 268);
	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(WIFI_CFG_PARAM_STRUCT, aucTailReserved) == 271);
	/*DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(WIFI_CFG_PARAM_STRUCT, aucTailPCCReserved) == 0x252);*/
#endif
}
#endif

#endif /* _CFG_WIFI_FILE_H */
