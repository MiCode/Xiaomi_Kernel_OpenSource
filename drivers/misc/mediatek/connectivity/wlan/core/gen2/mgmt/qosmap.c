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

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "precomp.h"

#if CFG_SUPPORT_PPR2

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

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/



/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
*
* \brief This routine is called to process the qos category action frame.
*
*
* \note
*      Called by: Handle Rx mgmt request
*/
/*----------------------------------------------------------------------------*/
VOID handleQosMapConf(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_WLAN_ACTION_FRAME prRxFrame;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prRxFrame = (P_WLAN_ACTION_FRAME) prSwRfb->pvHeader;

	switch (prRxFrame->ucAction) {
	case ACTION_ADDTS_REQ:
	case ACTION_ADDTS_RSP:
	case ACTION_SCHEDULE:
		DBGLOG(INIT, INFO, "qos action frame received, action: %d\n", prRxFrame->ucAction);
		break;
	case ACTION_QOS_MAP_CONFIGURE:
		qosHandleQosMapConfigure(prAdapter, prSwRfb);
		DBGLOG(INIT, INFO, "qos map configure frame received, action: %d\n", prRxFrame->ucAction);
		break;
	default:
		DBGLOG(INIT, INFO, "qos action frame: %d, try to send to supplicant\n", prRxFrame->ucAction);
		break;
	}
}

int qosHandleQosMapConfigure(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	struct _ACTION_QOS_MAP_CONFIGURE_FRAME *prRxFrame = NULL;
	P_STA_RECORD_T prStaRec;

	prRxFrame = (struct _ACTION_QOS_MAP_CONFIGURE_FRAME *) prSwRfb->pvHeader;
	if (!prRxFrame)
		return -1;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	if ((!prStaRec) || (!prStaRec->fgIsInUse))
		return -1;

	DBGLOG(INIT, INFO, "IEEE 802.11: Received Qos Map Configure Frame from " MACSTR "\n",
		MAC2STR(prStaRec->aucMacAddr));

	qosParseQosMapSet(prAdapter, prStaRec, prRxFrame->qosMapSet);

	return 0;
}

VOID qosParseQosMapSet(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN PUINT_8 qosMapSet)
{
	UINT_8 dscpExcNum = 0;
	int i = 0;
	PUINT_8 tempq = qosMapSet + 2;
	PUINT_8 qosmapping = prStaRec->qosMapSet;
	UINT_8 excTable[64];

	if (IE_ID(qosMapSet) != ELEM_ID_QOS_MAP_SET) {
		DBGLOG(INIT, WARN, "Wrong QosMapSet IE ID: %d\n", IE_ID(qosMapSet));
		return;
	}
	if ((IE_LEN(qosMapSet) < 16) || (IE_LEN(qosMapSet) > 58)) {
		DBGLOG(INIT, WARN, "Error in QosMapSet IE len: %d\n", IE_LEN(qosMapSet));
		return;
	}

	qosMapSetInit(prStaRec);
	kalMemSet(excTable, 0, 64);
	dscpExcNum = (IE_LEN(qosMapSet) - WMM_UP_INDEX_NUM * 2) / 2;

	for (i = 0; i < dscpExcNum; i++) {
		UINT_8 dscp = *tempq++;
		UINT_8 up = *tempq++;

		if (dscp < 64 && up < WMM_UP_INDEX_NUM) {
			qosmapping[dscp] = up;
			excTable[dscp] = TRUE;
		}
	}

	for (i = 0; i < WMM_UP_INDEX_NUM; i++) {
		UINT_8 lDscp = *tempq++;
		UINT_8 hDscp = *tempq++;
		UINT_8 dscp;

		if (lDscp == 255 && hDscp == 255) {
			DBGLOG(INIT, WARN, "UP %d is not used\n", i);
			continue;
		}

		if (hDscp < lDscp) {
			DBGLOG(INIT, WARN, "CHECK: UP %d, h %d, l %d\n",
				i, hDscp, lDscp);
			continue;
		}

		for (dscp = lDscp; dscp < 64 && dscp <= hDscp; dscp++) {
			if (!excTable[dscp])
				qosmapping[dscp] = i;
		}
	}

	DBGLOG(INIT, INFO, "QosMapSet DSCP Exception number: %d\n", dscpExcNum);
}

VOID qosMapSetInit(IN P_STA_RECORD_T prStaRec)
{
	/* DSCP to UP maaping based on RFC8325 in the range 0 to 63 */
	static UINT_8 dscp2up[64] = {
		[0 ... 63] = 0xFF,
		[0] = WMM_UP_BE_INDEX,
		[8] = WMM_UP_BK_INDEX,
		[10] = WMM_UP_BE_INDEX,
		[12] = WMM_UP_BE_INDEX,
		[14] = WMM_UP_BE_INDEX,
		[16] = WMM_UP_BE_INDEX,
		[18] = WMM_UP_EE_INDEX,
		[20] = WMM_UP_EE_INDEX,
		[22] = WMM_UP_EE_INDEX,
		[24] = WMM_UP_CL_INDEX,
		[26] = WMM_UP_CL_INDEX,
		[28] = WMM_UP_CL_INDEX,
		[30] = WMM_UP_CL_INDEX,
		[32] = WMM_UP_CL_INDEX,
		[34] = WMM_UP_CL_INDEX,
		[36] = WMM_UP_CL_INDEX,
		[38] = WMM_UP_CL_INDEX,
		[40] = WMM_UP_VI_INDEX,
		[44] = WMM_UP_VO_INDEX,
		[46] = WMM_UP_VO_INDEX,
		[48] = WMM_UP_VO_INDEX,
		[56] = WMM_UP_NC_INDEX,
	};

	kalMemCopy(prStaRec->qosMapSet, dscp2up, 64);
}

UINT_8 getUpFromDscp(IN P_GLUE_INFO_T prGlueInfo, IN int type, IN int dscp)
{
	P_BSS_INFO_T prAisBssInfo;
	P_STA_RECORD_T prStaRec;

	prAisBssInfo = &(prGlueInfo->prAdapter->rWifiVar.arBssInfo[type]);
	if (prAisBssInfo)
		prStaRec = prAisBssInfo->prStaRecOfAP;
	else
		return 0xFF;

	if (prStaRec && dscp < 64)
		return prStaRec->qosMapSet[dscp];

	return 0xFF;
}
#endif
