/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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
	INT_8 acReserved[3];	/* form MT6628 acReserved[0]=cTxPwr2G4Dsss */
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

typedef struct _MITIGATED_PWR_BY_CH_BY_MODE {
	UINT_8 channel;
	INT_8 mitigatedCckDsss;
	INT_8 mitigatedOfdm;
	INT_8 mitigatedHt20;
	INT_8 mitigatedHt40;
} MITIGATED_PWR_BY_CH_BY_MODE, *P_MITIGATED_PWR_BY_CH_BY_MODE;

typedef struct _FCC_TX_PWR_ADJUST_T {
	UINT_8 fgFccTxPwrAdjust;
	UINT_8 uOffsetCCK;
	UINT_8 uOffsetHT20;
	UINT_8 uOffsetHT40;
	UINT_8 aucChannelCCK[2];
	UINT_8 aucChannelHT20[2];
	UINT_8 aucChannelHT40[2];
} FCC_TX_PWR_ADJUST, *P_FCC_TX_PWR_ADJUST;

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
	UINT_8 aucReserved2[256-248];
	UINT_8 aucChannelBandEdge[2];
	UINT_16 u2SizeOfNvram;
	INT_8 bTxPowerLimitEnable2G;
	INT_8 cTxBackOffMaxPower2G;
	INT_8 bTxPowerLimitEnable5G;
	INT_8 cTxBackOffMaxPower5G;

	/* 256 bytes of function data */
	UINT_16 u2Part2OwnVersion;
	UINT_16 u2Part2PeerVersion;
	UINT_8 uc2G4BwFixed20M;
	UINT_8 uc5GBwFixed20M;
	UINT_8 ucEnable5GBand;
	UINT_8 aucPreTailReserved;
	UINT_8 uc2GRssiCompensation;
	UINT_8 uc5GRssiCompensation;
	UINT_8 fgRssiCompensationValidbit;
	UINT_8 ucRxAntennanumber;
	/*support tx power back off [start]*/
	MITIGATED_PWR_BY_CH_BY_MODE arRlmMitigatedPwrByChByMode[40];
	UINT_8 fgRlmMitigatedPwrByChByMode;
	FCC_TX_PWR_ADJUST rFccTxPwrAdjust;
	/*support tx power back off [end]*/
	UINT_8 aucTailReserved[768 - 12 - 211];
} MT6620_CFG_PARAM_STRUCT, *P_MT6620_CFG_PARAM_STRUCT, WIFI_CFG_PARAM_STRUCT, *P_WIFI_CFG_PARAM_STRUCT;

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
{ \
	switch (0) {case 0: case (expr): default:; } \
}
#endif

#define CFG_FILE_WIFI_REC_SIZE    sizeof(WIFI_CFG_PARAM_STRUCT)
#define EXTEND_NVRAM_SIZE 1024

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
static inline VOID nvramOffsetCheck(VOID)
{
	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(WIFI_CFG_PARAM_STRUCT, u2Part2OwnVersion) == 256);

	DATA_STRUCT_INSPECTING_ASSERT(sizeof(WIFI_CFG_PARAM_STRUCT) == EXTEND_NVRAM_SIZE);

	DATA_STRUCT_INSPECTING_ASSERT((OFFSET_OF(WIFI_CFG_PARAM_STRUCT, aucEFUSE) & 0x0001) == 0);

	DATA_STRUCT_INSPECTING_ASSERT((OFFSET_OF(WIFI_CFG_PARAM_STRUCT, aucRegSubbandInfo) & 0x0001) == 0);
}
#endif

#endif /* _CFG_WIFI_FILE_H */
