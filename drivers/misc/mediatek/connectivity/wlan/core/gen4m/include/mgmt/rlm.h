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
 * Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/
 *							include/mgmt/rlm.h#2
 */

/*! \file   "rlm.h"
 *    \brief
 */


#ifndef _RLM_H
#define _RLM_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
extern u_int8_t g_bIcapEnable;
extern u_int8_t g_bCaptureDone;
extern uint16_t g_u2DumpIndex;
#if CFG_SUPPORT_QA_TOOL
extern uint32_t g_au4Offset[2][2];
extern uint32_t g_au4IQData[256];
#endif

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define ELEM_EXT_CAP_DEFAULT_VAL \
	(ELEM_EXT_CAP_20_40_COEXIST_SUPPORT /*| ELEM_EXT_CAP_PSMP_CAP*/)

#if CFG_SUPPORT_RX_STBC
#define FIELD_HT_CAP_INFO_RX_STBC   HT_CAP_INFO_RX_STBC_1_SS
#else
#define FIELD_HT_CAP_INFO_RX_STBC   HT_CAP_INFO_RX_STBC_NO_SUPPORTED
#endif

#if CFG_SUPPORT_RX_SGI
#define FIELD_HT_CAP_INFO_SGI_20M   HT_CAP_INFO_SHORT_GI_20M
#define FIELD_HT_CAP_INFO_SGI_40M   HT_CAP_INFO_SHORT_GI_40M
#else
#define FIELD_HT_CAP_INFO_SGI_20M   0
#define FIELD_HT_CAP_INFO_SGI_40M   0
#endif

#if CFG_SUPPORT_RX_HT_GF
#define FIELD_HT_CAP_INFO_HT_GF     HT_CAP_INFO_HT_GF
#else
#define FIELD_HT_CAP_INFO_HT_GF     0
#endif

#define HT_CAP_INFO_DEFAULT_VAL \
	(HT_CAP_INFO_SUP_CHNL_WIDTH | HT_CAP_INFO_DSSS_CCK_IN_40M \
		| HT_CAP_INFO_SM_POWER_SAVE)

#define AMPDU_PARAM_DEFAULT_VAL \
	(AMPDU_PARAM_MAX_AMPDU_LEN_64K | AMPDU_PARAM_MSS_NO_RESTRICIT)

#define SUP_MCS_TX_DEFAULT_VAL \
	SUP_MCS_TX_SET_DEFINED	/* TX defined and TX/RX equal (TBD) */

#if CFG_SUPPORT_MFB
#define FIELD_HT_EXT_CAP_MFB    HT_EXT_CAP_MCS_FEEDBACK_BOTH
#else
#define FIELD_HT_EXT_CAP_MFB    HT_EXT_CAP_MCS_FEEDBACK_NO_FB
#endif

#if CFG_SUPPORT_RX_RDG
#define FIELD_HT_EXT_CAP_RDR    HT_EXT_CAP_RD_RESPONDER
#else
#define FIELD_HT_EXT_CAP_RDR    0
#endif

#if CFG_SUPPORT_MFB || CFG_SUPPORT_RX_RDG
#define FIELD_HT_EXT_CAP_HTC    HT_EXT_CAP_HTC_SUPPORT
#else
#define FIELD_HT_EXT_CAP_HTC    0
#endif

#define HT_EXT_CAP_DEFAULT_VAL \
	(HT_EXT_CAP_PCO | HT_EXT_CAP_PCO_TRANS_TIME_NONE | \
	 FIELD_HT_EXT_CAP_MFB | FIELD_HT_EXT_CAP_HTC | \
	 FIELD_HT_EXT_CAP_RDR)

#define TX_BEAMFORMING_CAP_DEFAULT_VAL        0

#if CFG_SUPPORT_BFEE
#define TX_BEAMFORMING_CAP_BFEE \
	(TXBF_RX_NDP_CAPABLE | \
	 TXBF_EXPLICIT_COMPRESSED_FEEDBACK_IMMEDIATE_CAPABLE | \
	 TXBF_MINIMAL_GROUPING_1_2_3_CAPABLE | \
	 TXBF_COMPRESSED_TX_ANTENNANUM_4_SUPPORTED | \
	 TXBF_CHANNEL_ESTIMATION_4STS_CAPABILITY)
#else
#define TX_BEAMFORMING_CAP_BFEE        0
#endif

#if CFG_SUPPORT_BFER
#define TX_BEAMFORMING_CAP_BFER \
	(TXBF_TX_NDP_CAPABLE | \
	 TXBF_EXPLICIT_COMPRESSED_TX_CAPAB)
#else
#define TX_BEAMFORMING_CAP_BFER        0
#endif

#define ASEL_CAP_DEFAULT_VAL                        0

/* Define bandwidth from user setting */
#define CONFIG_BW_20_40M            0
#define CONFIG_BW_20M               1	/* 20MHz only */

#define RLM_INVALID_POWER_LIMIT                     -127 /* dbm */

#define RLM_MAX_TX_PWR		20	/* dbm */
#define RLM_MIN_TX_PWR		8	/* dbm */

#if CFG_SUPPORT_802_11AC
#if CFG_SUPPORT_BFEE
#define FIELD_VHT_CAP_INFO_BFEE \
		(VHT_CAP_INFO_SU_BEAMFORMEE_CAPABLE)
#define VHT_CAP_INFO_BEAMFORMEE_STS_CAP_MAX	3
#else
#define FIELD_VHT_CAP_INFO_BFEE     0
#endif

#if CFG_SUPPORT_BFER
#define FIELD_VHT_CAP_INFO_BFER \
		(VHT_CAP_INFO_SU_BEAMFORMER_CAPABLE| \
		VHT_CAP_INFO_NUMBER_OF_SOUNDING_DIMENSIONS_2_SUPPORTED)
#else
#define FIELD_VHT_CAP_INFO_BFER     0
#endif

#define VHT_CAP_INFO_DEFAULT_VAL \
	(VHT_CAP_INFO_MAX_MPDU_LEN_3K | \
	 (AMPDU_PARAM_MAX_AMPDU_LEN_1024K \
		 << VHT_CAP_INFO_MAX_AMPDU_LENGTH_OFFSET))

#define VHT_CAP_INFO_DEFAULT_HIGHEST_DATA_RATE			0
#endif
/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
struct RLM_CAL_RESULT_ALL_V2 {
	/* Used for checking the Cal Data is damaged */
	uint32_t u4MagicNum1;

	/* Thermal Value when do these Calibration */
	uint32_t u4ThermalInfo;

	/* Total Rom Data Length Backup in Host Side */
	uint32_t u4ValidRomCalDataLength;

	/* Total Ram Data Length Backup in Host Side */
	uint32_t u4ValidRamCalDataLength;

	/* All Rom Cal Data Dumpped by FW */
	uint32_t au4RomCalData[10000];

	/* All Ram Cal Data Dumpped by FW */
	uint32_t au4RamCalData[10000];

	/* Used for checking the Cal Data is damaged */
	uint32_t u4MagicNum2;
};
extern struct RLM_CAL_RESULT_ALL_V2 g_rBackupCalDataAllV2;
#endif

typedef void (*PFN_OPMODE_NOTIFY_DONE_FUNC)(
	struct ADAPTER *, uint8_t, bool);

enum ENUM_OP_NOTIFY_TYPE_T {
	OP_NOTIFY_TYPE_VHT_NSS_BW = 0,
	OP_NOTIFY_TYPE_HT_NSS,
	OP_NOTIFY_TYPE_HT_BW,
	OP_NOTIFY_TYPE_NUM
};

enum ENUM_OP_CHANGE_STATUS_T {
	OP_CHANGE_STATUS_INVALID = 0, /* input invalid */
	/* input valid, but no need to change */
	OP_CHANGE_STATUS_VALID_NO_CHANGE,
	/* process callback done before function return */
	OP_CHANGE_STATUS_VALID_CHANGE_CALLBACK_DONE,
	/* wait next INT to call callback */
	OP_CHANGE_STATUS_VALID_CHANGE_CALLBACK_WAIT,
	OP_CHANGE_STATUS_NUM
};

struct SUB_ELEMENT_LIST {
	struct SUB_ELEMENT_LIST *prNext;
	struct SUB_ELEMENT rSubIE;
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

/* It is used for RLM module to judge if specific network is valid
 * Note: Ad-hoc mode of AIS is not included now. (TBD)
 */
#define RLM_NET_PARAM_VALID(_prBssInfo) \
	(IS_BSS_ACTIVE(_prBssInfo) && \
	 ((_prBssInfo)->eConnectionState == MEDIA_STATE_CONNECTED || \
	  (_prBssInfo)->eCurrentOPMode == OP_MODE_ACCESS_POINT || \
	  (_prBssInfo)->eCurrentOPMode == OP_MODE_IBSS || \
	  IS_BSS_BOW(_prBssInfo)) \
	)

#define RLM_NET_IS_11N(_prBssInfo) \
	((_prBssInfo)->ucPhyTypeSet & PHY_TYPE_SET_802_11N)
#define RLM_NET_IS_11GN(_prBssInfo) \
	((_prBssInfo)->ucPhyTypeSet & PHY_TYPE_SET_802_11GN)

#if CFG_SUPPORT_802_11AC
#define RLM_NET_IS_11AC(_prBssInfo) \
	((_prBssInfo)->ucPhyTypeSet & PHY_TYPE_SET_802_11AC)
#endif
#if (CFG_SUPPORT_802_11AX == 1)
#define RLM_NET_IS_11AX(_prBssInfo) \
	((_prBssInfo)->ucPhyTypeSet & PHY_TYPE_SET_802_11AX)
#endif

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
void rlmFsmEventInit(struct ADAPTER *prAdapter);

void rlmFsmEventUninit(struct ADAPTER *prAdapter);

void rlmReqGenerateHtCapIE(struct ADAPTER *prAdapter,
			   struct MSDU_INFO *prMsduInfo);

void rlmReqGeneratePowerCapIE(struct ADAPTER *prAdapter,
			   struct MSDU_INFO *prMsduInfo);

void rlmReqGenerateSupportedChIE(struct ADAPTER *prAdapter,
			   struct MSDU_INFO *prMsduInfo);

void rlmReqGenerateExtCapIE(struct ADAPTER *prAdapter,
			    struct MSDU_INFO *prMsduInfo);

void rlmRspGenerateHtCapIE(struct ADAPTER *prAdapter,
			   struct MSDU_INFO *prMsduInfo);

void rlmRspGenerateExtCapIE(struct ADAPTER *prAdapter,
			    struct MSDU_INFO *prMsduInfo);

void rlmRspGenerateHtOpIE(struct ADAPTER *prAdapter,
			  struct MSDU_INFO *prMsduInfo);

void rlmRspGenerateErpIE(struct ADAPTER *prAdapter,
			 struct MSDU_INFO *prMsduInfo);

void rlmGenerateMTKOuiIE(struct ADAPTER *prAdapter,
			 struct MSDU_INFO *prMsduInfo);

u_int8_t rlmParseCheckMTKOuiIE(IN struct ADAPTER *prAdapter,
			       IN uint8_t *pucBuf, IN uint32_t *pu4Cap);

void rlmGenerateCsaIE(struct ADAPTER *prAdapter,
		      struct MSDU_INFO *prMsduInfo);

void rlmProcessBcn(struct ADAPTER *prAdapter,
		   struct SW_RFB *prSwRfb, uint8_t *pucIE,
		   uint16_t u2IELength);

void rlmProcessAssocRsp(struct ADAPTER *prAdapter,
			struct SW_RFB *prSwRfb, uint8_t *pucIE,
			uint16_t u2IELength);

void rlmProcessHtAction(struct ADAPTER *prAdapter,
			struct SW_RFB *prSwRfb);

#if CFG_SUPPORT_802_11AC
void rlmProcessVhtAction(struct ADAPTER *prAdapter,
			 struct SW_RFB *prSwRfb);
#endif

void rlmFillSyncCmdParam(struct CMD_SET_BSS_RLM_PARAM
			 *prCmdBody, struct BSS_INFO *prBssInfo);

void rlmSyncOperationParams(struct ADAPTER *prAdapter,
			    struct BSS_INFO *prBssInfo);

void rlmBssInitForAPandIbss(struct ADAPTER *prAdapter,
			    struct BSS_INFO *prBssInfo);

void rlmProcessAssocReq(struct ADAPTER *prAdapter,
			struct SW_RFB *prSwRfb, uint8_t *pucIE,
			uint16_t u2IELength);

void rlmBssAborted(struct ADAPTER *prAdapter,
		   struct BSS_INFO *prBssInfo);

#if CFG_SUPPORT_TDLS
uint32_t
rlmFillHtCapIEByParams(u_int8_t fg40mAllowed,
		       u_int8_t fgShortGIDisabled,
		       uint8_t u8SupportRxSgi20,
		       uint8_t u8SupportRxSgi40, uint8_t u8SupportRxGf,
		       enum ENUM_OP_MODE eCurrentOPMode, uint8_t *pOutBuf);

uint32_t rlmFillHtCapIEByAdapter(struct ADAPTER *prAdapter,
				 struct BSS_INFO *prBssInfo, uint8_t *pOutBuf);

uint32_t rlmFillVhtCapIEByAdapter(struct ADAPTER *prAdapter,
				  struct BSS_INFO *prBssInfo, uint8_t *pOutBuf);

#endif

#if CFG_SUPPORT_802_11AC
void rlmReqGenerateVhtCapIE(struct ADAPTER *prAdapter,
			    struct MSDU_INFO *prMsduInfo);

void rlmRspGenerateVhtCapIE(struct ADAPTER *prAdapter,
			    struct MSDU_INFO *prMsduInfo);

void rlmRspGenerateVhtOpIE(struct ADAPTER *prAdapter,
			   struct MSDU_INFO *prMsduInfo);

void rlmFillVhtOpIE(struct ADAPTER *prAdapter,
		    struct BSS_INFO *prBssInfo, struct MSDU_INFO *prMsduInfo);

void rlmRspGenerateVhtOpNotificationIE(struct ADAPTER
			       *prAdapter, struct MSDU_INFO *prMsduInfo);
void rlmReqGenerateVhtOpNotificationIE(struct ADAPTER
			       *prAdapter, struct MSDU_INFO *prMsduInfo);




#endif

void rlmGenerateCountryIE(struct ADAPTER *prAdapter,
			  struct MSDU_INFO *prMsduInfo);

#if CFG_SUPPORT_DFS
void rlmProcessSpecMgtAction(struct ADAPTER *prAdapter,
			     struct SW_RFB *prSwRfb);
#endif

uint32_t
rlmSendOpModeNotificationFrame(struct ADAPTER *prAdapter,
			       struct STA_RECORD *prStaRec,
			       uint8_t ucChannelWidth, uint8_t ucNss);

uint32_t
rlmSendSmPowerSaveFrame(struct ADAPTER *prAdapter,
			struct STA_RECORD *prStaRec, uint8_t ucNss);

void rlmSendChannelSwitchFrame(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

uint32_t
rlmNotifyVhtOpModeTxDone(struct ADAPTER *prAdapter,
			 struct MSDU_INFO *prMsduInfo,
			 enum ENUM_TX_RESULT_CODE rTxDoneStatus);

uint32_t
rlmSmPowerSaveTxDone(struct ADAPTER *prAdapter,
		     struct MSDU_INFO *prMsduInfo,
		     enum ENUM_TX_RESULT_CODE rTxDoneStatus);

uint32_t
rlmNotifyChannelWidthtTxDone(struct ADAPTER *prAdapter,
			     struct MSDU_INFO *prMsduInfo,
			     enum ENUM_TX_RESULT_CODE rTxDoneStatus);

uint8_t
rlmGetBssOpBwByVhtAndHtOpInfo(struct BSS_INFO *prBssInfo);

uint8_t
rlmGetVhtOpBwByBssOpBw(uint8_t ucBssOpBw);

void
rlmFillVhtOpInfoByBssOpBw(struct BSS_INFO *prBssInfo,
			  uint8_t ucChannelWidth);

enum ENUM_OP_CHANGE_STATUS_T
rlmChangeOperationMode(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t ucChannelWidth,
	uint8_t ucOpRxNss,
	uint8_t ucOpTxNss,
	#if CFG_SUPPORT_SMART_GEAR
	uint8_t eNewReq,
	#endif
	PFN_OPMODE_NOTIFY_DONE_FUNC pfOpChangeHandler
);

void
rlmDummyChangeOpHandler(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex, bool fgIsChangeSuccess);


#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
uint32_t rlmCalBackup(
	struct ADAPTER *prAdapter,
	uint8_t		ucReason,
	uint8_t		ucAction,
	uint8_t		ucRomRam
);

uint32_t rlmTriggerCalBackup(
	struct ADAPTER *prAdapter,
	u_int8_t		fgIsCalDataBackuped
);
#endif

void rlmModifyVhtBwPara(uint8_t *pucVhtChannelFrequencyS1,
			uint8_t *pucVhtChannelFrequencyS2,
			uint8_t *pucVhtChannelWidth);

void rlmReviseMaxBw(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	enum ENUM_CHNL_EXT *peExtend,
	enum ENUM_CHANNEL_WIDTH *peChannelWidth,
	uint8_t *pucS1,
	uint8_t *pucPrimaryCh);

void rlmSetMaxTxPwrLimit(IN struct ADAPTER *prAdapter, int8_t cLimit,
			 uint8_t ucEnable);

void rlmSyncExtCapIEwithSupplicant(uint8_t *aucCapabilities,
	const uint8_t *supExtCapIEs, size_t IElen);

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

#ifndef _lint
static __KAL_INLINE__ void rlmDataTypeCheck(void)
{
}
#endif /* _lint */

#endif /* _RLM_H */
