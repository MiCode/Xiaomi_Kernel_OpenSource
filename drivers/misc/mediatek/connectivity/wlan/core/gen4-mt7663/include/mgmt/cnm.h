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
 * Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3
 *     /include/mgmt/cnm.h#1
 */

/*! \file   "cnm.h"
 *    \brief
 */


#ifndef _CNM_H
#define _CNM_H

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
#if CFG_SUPPORT_DBDC
#define DBDC_5G_WMM_INDEX	0
#define DBDC_2G_WMM_INDEX	1
#endif
#define HW_WMM_NUM		(prAdapter->ucWmmSetNum)
#define MAX_HW_WMM_INDEX	(HW_WMM_NUM - 1)
#define DEFAULT_HW_WMM_INDEX	MAX_HW_WMM_INDEX
#if CFG_SUPPORT_RX_DYNAMIC_MCC_PRIORITY
#define MCC_CMD_MAX_LEN    100
#define MCC_RX_CNT_LOW_BOUND   100
#define MCC_RX_CNT_HIGH_BOUND  200
#define MCC_CHECK_TIME	5
#endif
#if CFG_SUPPORT_ADJUST_MCC_MODE_SET
#define CFG_MCC_AIS_QUOTA_TYPE	0
#define CFG_MCC_P2PGC_QUOTA_TYPE	1
#define CFG_MCC_P2PGO_QUOTA_TYPE	2
#endif
/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

enum ENUM_CH_REQ_TYPE {
	CH_REQ_TYPE_JOIN,
	CH_REQ_TYPE_P2P_LISTEN,
	CH_REQ_TYPE_OFFCHNL_TX,
	CH_REQ_TYPE_GO_START_BSS,
#if (CFG_SUPPORT_DFS_MASTER == 1)
	CH_REQ_TYPE_DFS_CAC,
#endif
	CH_REQ_TYPE_NUM
};
#if (CFG_SUPPORT_IDC_CH_SWITCH == 1)
enum ENUM_CH_SWITCH_TYPE {
	CH_SWITCH_2G, /* Default */
	CH_SWITCH_5G,
	CH_SWITCH_NUM
};
#endif

struct MSG_CH_REQ {
	struct MSG_HDR rMsgHdr;	/* Must be the first member */
	uint8_t ucBssIndex;
	uint8_t ucTokenID;
	uint8_t ucPrimaryChannel;
	enum ENUM_CHNL_EXT eRfSco;
	enum ENUM_BAND eRfBand;

	/* To support 80/160MHz bandwidth */
	enum ENUM_CHANNEL_WIDTH eRfChannelWidth;

	uint8_t ucRfCenterFreqSeg1;	/* To support 80/160MHz bandwidth */
	uint8_t ucRfCenterFreqSeg2;	/* To support 80/160MHz bandwidth */
	enum ENUM_CH_REQ_TYPE eReqType;
	uint32_t u4MaxInterval;	/* In unit of ms */
	enum ENUM_DBDC_BN eDBDCBand;
};

struct MSG_CH_ABORT {
	struct MSG_HDR rMsgHdr;	/* Must be the first member */
	uint8_t ucBssIndex;
	uint8_t ucTokenID;
	enum ENUM_DBDC_BN eDBDCBand;
};

struct MSG_CH_GRANT {
	struct MSG_HDR rMsgHdr;	/* Must be the first member */
	uint8_t ucBssIndex;
	uint8_t ucTokenID;
	uint8_t ucPrimaryChannel;
	enum ENUM_CHNL_EXT eRfSco;
	enum ENUM_BAND eRfBand;

	/* To support 80/160MHz bandwidth */
	enum ENUM_CHANNEL_WIDTH eRfChannelWidth;

	uint8_t ucRfCenterFreqSeg1;	/* To support 80/160MHz bandwidth */
	uint8_t ucRfCenterFreqSeg2;	/* To support 80/160MHz bandwidth */
	enum ENUM_CH_REQ_TYPE eReqType;
	uint32_t u4GrantInterval;	/* In unit of ms */
	enum ENUM_DBDC_BN eDBDCBand;
};

struct MSG_CH_REOCVER {
	struct MSG_HDR rMsgHdr;	/* Must be the first member */
	uint8_t ucBssIndex;
	uint8_t ucTokenID;
	uint8_t ucPrimaryChannel;
	enum ENUM_CHNL_EXT eRfSco;
	enum ENUM_BAND eRfBand;

	/*  To support 80/160MHz bandwidth */
	enum ENUM_CHANNEL_WIDTH eRfChannelWidth;

	uint8_t ucRfCenterFreqSeg1;	/* To support 80/160MHz bandwidth */
	uint8_t ucRfCenterFreqSeg2;	/* To support 80/160MHz bandwidth */
	enum ENUM_CH_REQ_TYPE eReqType;
};

struct CNM_INFO {
	u_int8_t fgChGranted;
	uint8_t ucBssIndex;
	uint8_t ucTokenID;
};

#if CFG_ENABLE_WIFI_DIRECT
/* Moved from p2p_fsm.h */
struct DEVICE_TYPE {
	uint16_t u2CategoryId;		/* Category ID */
	uint8_t aucOui[4];		/* OUI */
	uint16_t u2SubCategoryId;	/* Sub Category ID */
} __KAL_ATTRIB_PACKED__;
#endif

#if CFG_SUPPORT_DBDC
struct CNM_DBDC_CAP {
	uint8_t ucBssIndex;
	uint8_t ucNss;
	uint8_t ucWmmSetIndex;
};

enum ENUM_CNM_DBDC_MODE {
	/* A/G traffic separate by WMM, but both
	 * TRX on band 0, CANNOT enable DBDC
	 */
	ENUM_DBDC_MODE_DISABLED,

	/* A/G traffic separate by WMM, WMM0/1
	 * TRX on band 0/1, CANNOT disable DBDC
	 */
	ENUM_DBDC_MODE_STATIC,

	/* Automatically enable/disable DBDC,
	 * setting just like static/disable mode
	 */
	ENUM_DBDC_MODE_DYNAMIC,

	ENUM_DBDC_MODE_NUM
};

enum ENUM_CNM_DBDC_SWITCH_MECHANISM { /* When DBDC available in dynamic DBDC */
	/* Switch to DBDC when available (less latency) */
	ENUM_DBDC_SWITCH_MECHANISM_LATENCY_MODE,

	/* Switch to DBDC when DBDC T-put > MCC T-put */
	ENUM_DBDC_SWITCH_MECHANISM_THROUGHPUT_MODE,

	ENUM_DBDC_SWITCH_MECHANISM_NUM
};
#endif	/* CFG_SUPPORT_DBDC */

enum ENUM_CNM_NETWORK_TYPE_T {
	ENUM_CNM_NETWORK_TYPE_OTHER,
	ENUM_CNM_NETWORK_TYPE_AIS,
	ENUM_CNM_NETWORK_TYPE_P2P_GC,
	ENUM_CNM_NETWORK_TYPE_P2P_GO,
	ENUM_CNM_NETWORK_TYPE_NUM
};

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
#define CNM_CH_GRANTED_FOR_BSS(_prAdapter, _ucBssIndex) \
	((_prAdapter)->rCnmInfo.fgChGranted && \
	 (_prAdapter)->rCnmInfo.ucBssIndex == (_ucBssIndex))

#define IS_CONNECTION_NSS2(prBssInfo, prStaRec) \
	((((prBssInfo)->ucNss > 1) && ((prStaRec)->aucRxMcsBitmask[1] != 0x00) \
	&& (((prStaRec)->u2HtCapInfo & HT_CAP_INFO_SM_POWER_SAVE) != 0)) || \
	(((prBssInfo)->ucNss > 1) && ((((prStaRec)->u2VhtRxMcsMap \
	& BITS(2, 3)) >> 2) != BITS(0, 1)) && ((((prStaRec)->ucVhtOpMode \
	& VHT_OP_MODE_RX_NSS) >> VHT_OP_MODE_RX_NSS_OFFSET) > 0)))

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
void cnmInit(struct ADAPTER *prAdapter);

void cnmUninit(struct ADAPTER *prAdapter);

void cnmChMngrRequestPrivilege(struct ADAPTER *prAdapter,
	struct MSG_HDR *prMsgHdr);

void cnmChMngrAbortPrivilege(struct ADAPTER *prAdapter,
	struct MSG_HDR *prMsgHdr);

void cnmChMngrHandleChEvent(struct ADAPTER *prAdapter,
	struct WIFI_EVENT *prEvent);

#if (CFG_SUPPORT_DFS_MASTER == 1)
void cnmRadarDetectEvent(struct ADAPTER *prAdapter,
	struct WIFI_EVENT *prEvent);

void cnmCsaDoneEvent(struct ADAPTER *prAdapter,
	struct WIFI_EVENT *prEvent);

#if (CFG_SUPPORT_IDC_CH_SWITCH == 1)
uint8_t cnmIdcCsaReq(IN struct ADAPTER *prAdapter,
	IN uint8_t ch_num, IN uint8_t ucRoleIdx);

void cnmIdcDetectHandler(IN struct ADAPTER *prAdapter,
	IN struct WIFI_EVENT *prEvent);
#endif
#endif

u_int8_t cnmPreferredChannel(struct ADAPTER *prAdapter, enum ENUM_BAND *prBand,
	uint8_t *pucPrimaryChannel, enum ENUM_CHNL_EXT *prBssSCO);

u_int8_t cnmAisInfraChannelFixed(struct ADAPTER *prAdapter,
	enum ENUM_BAND *prBand, uint8_t *pucPrimaryChannel);

void cnmAisInfraConnectNotify(struct ADAPTER *prAdapter);

u_int8_t cnmAisIbssIsPermitted(struct ADAPTER *prAdapter);

u_int8_t cnmP2PIsPermitted(struct ADAPTER *prAdapter);

u_int8_t cnmBowIsPermitted(struct ADAPTER *prAdapter);

u_int8_t cnmBss40mBwPermitted(struct ADAPTER *prAdapter, uint8_t ucBssIndex);

u_int8_t cnmBss80mBwPermitted(struct ADAPTER *prAdapter, uint8_t ucBssIndex);

uint8_t cnmGetBssMaxBw(struct ADAPTER *prAdapter, uint8_t ucBssIndex);

uint8_t cnmGetBssMaxBwToChnlBW(struct ADAPTER *prAdapter, uint8_t ucBssIndex);

struct BSS_INFO *cnmGetBssInfoAndInit(struct ADAPTER *prAdapter,
	enum ENUM_NETWORK_TYPE eNetworkType, u_int8_t fgIsP2pDevice);

void cnmFreeBssInfo(struct ADAPTER *prAdapter, struct BSS_INFO *prBssInfo);
#if CFG_SUPPORT_CHNL_CONFLICT_REVISE
u_int8_t cnmAisDetectP2PChannel(struct ADAPTER *prAdapter,
	enum ENUM_BAND *prBand, uint8_t *pucPrimaryChannel);
#endif

u_int8_t cnmWmmIndexDecision(IN struct ADAPTER *prAdapter,
	IN struct BSS_INFO *prBssInfo);
void cnmFreeWmmIndex(IN struct ADAPTER *prAdapter,
	IN struct BSS_INFO *prBssInfo);

#if CFG_SUPPORT_DBDC
void cnmInitDbdcSetting(IN struct ADAPTER *prAdapter);

void cnmDbdcOpModeChangeDoneCallback(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex,
	IN u_int8_t fgSuccess);

void cnmUpdateDbdcSetting(IN struct ADAPTER *prAdapter, IN u_int8_t fgDbdcEn);

void cnmGetDbdcCapability(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex,
	IN enum ENUM_BAND eRfBand,
	IN uint8_t ucPrimaryChannel,
	IN uint8_t ucNss,
	OUT struct CNM_DBDC_CAP *prDbdcCap
);

uint8_t cnmGetDbdcBwCapability(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex
);

void cnmDbdcEnableDecision(
	IN struct ADAPTER *prAdapter,
	IN uint8_t ucChangedBssIndex,
	IN enum ENUM_BAND eRfBand,
	IN uint8_t ucPrimaryChannel,
	IN uint8_t ucWmmQueIdx
);

void cnmDbdcDisableDecision(IN struct ADAPTER *prAdapter,
	IN uint8_t ucChangedBssIndex);
void cnmDbdcGuardTimerCallback(IN struct ADAPTER *prAdapter,
	IN unsigned long plParamPtr);
void cnmDbdcEventHwSwitchDone(IN struct ADAPTER *prAdapter,
	IN struct WIFI_EVENT *prEvent);
#endif /*CFG_SUPPORT_DBDC*/
void cnmCheckPendingTimer(IN struct ADAPTER *prAdapter);

enum ENUM_CNM_NETWORK_TYPE_T cnmGetBssNetworkType(struct BSS_INFO *prBssInfo);

#if CFG_SUPPORT_RX_DYNAMIC_MCC_PRIORITY
void cnmRxCntMonitor(IN struct ADAPTER *prAdapter,
	IN unsigned long plParamPtr);
void cnmSetMccTime(IN struct ADAPTER *prAdapter,
	IN uint8_t ucRole, IN uint32_t u4StayTime);
#endif /*CFG_SUPPORT_RX_DYNAMIC_MCC_PRIORITY*/

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
static __KAL_INLINE__ void cnmMsgDataTypeCheck(void)
{
	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSG_CH_GRANT, rMsgHdr)
			== 0);

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSG_CH_GRANT, rMsgHdr)
			== OFFSET_OF(struct MSG_CH_REOCVER, rMsgHdr));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSG_CH_GRANT, ucBssIndex)
			== OFFSET_OF(struct MSG_CH_REOCVER, ucBssIndex));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSG_CH_GRANT, ucTokenID)
			== OFFSET_OF(struct MSG_CH_REOCVER, ucTokenID));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSG_CH_GRANT, ucPrimaryChannel)
			== OFFSET_OF(struct MSG_CH_REOCVER, ucPrimaryChannel));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSG_CH_GRANT, eRfSco)
			== OFFSET_OF(struct MSG_CH_REOCVER, eRfSco));

	DATA_STRUCT_INSPECTING_ASSERT(OFFSET_OF(
		struct MSG_CH_GRANT, eRfBand)
			== OFFSET_OF(struct MSG_CH_REOCVER, eRfBand));

	DATA_STRUCT_INSPECTING_ASSERT(
		OFFSET_OF(struct MSG_CH_GRANT, eReqType)
			== OFFSET_OF(struct MSG_CH_REOCVER, eReqType));
}
#endif /* _lint */

#endif /* _CNM_H */
