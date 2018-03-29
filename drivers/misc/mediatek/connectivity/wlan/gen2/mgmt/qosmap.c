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
static struct _QOS_MAP_SET *QosMapSetMalloc(IN UINT_8 dscpExcNum)
{
	if (dscpExcNum)
		return (struct _QOS_MAP_SET *)kalMemAlloc((sizeof(struct _QOS_MAP_SET) +
				((dscpExcNum - 1) * sizeof(struct _DSCP_EXCEPTION))), VIR_MEM_TYPE);
	else
		return (struct _QOS_MAP_SET *)kalMemAlloc(sizeof(struct _QOS_MAP_SET), VIR_MEM_TYPE);
}

static void QosMapSetFree(IN P_STA_RECORD_T prStaRec)
{
	if (prStaRec && prStaRec->qosMapSet) {
		if (prStaRec->qosMapSet->dscpExceptionNum) {
			kalMemFree(prStaRec->qosMapSet, VIR_MEM_TYPE,
				(sizeof(struct _QOS_MAP_SET) +
				((prStaRec->qosMapSet->dscpExceptionNum - 1) * sizeof(struct _DSCP_EXCEPTION))));
		} else
			kalMemFree(prStaRec->qosMapSet, VIR_MEM_TYPE, sizeof(struct _QOS_MAP_SET));
	}
}

void QosMapSetRelease(IN P_STA_RECORD_T prStaRec)
{
	QosMapSetFree(prStaRec);
}

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

	if (prStaRec->qosMapSet)
		QosMapSetFree(prStaRec);
	prStaRec->qosMapSet = qosParseQosMapSet(prAdapter, prRxFrame->qosMapSet);

	return 0;
}

struct _QOS_MAP_SET *qosParseQosMapSet(IN P_ADAPTER_T prAdapter, IN PUINT_8 qosMapSet)
{
	UINT_8 dscpExcNum = 0;
	struct _QOS_MAP_SET *prQos = NULL;
	int i, j = 0;
	PUINT_8 tempq = qosMapSet + 2;

	if (IE_ID(qosMapSet) != ELEM_ID_QOS_MAP_SET) {
		DBGLOG(INIT, WARN, "Wrong QosMapSet IE ID: %d\n", IE_ID(qosMapSet));
		return NULL;
	}
	if ((IE_LEN(qosMapSet) < 16) || (IE_LEN(qosMapSet) > 58)) {
		DBGLOG(INIT, WARN, "Error in QosMapSet IE len: %d\n", IE_LEN(qosMapSet));
		return NULL;
	}
	dscpExcNum = (IE_LEN(qosMapSet) - 16) / 2;

	prQos = QosMapSetMalloc(dscpExcNum);
	if (!prQos) {
		DBGLOG(INIT, WARN, "can't alloc qosmap\n");
		return NULL;
	}

	prQos->dscpExceptionNum = dscpExcNum;
	for (i = 0; i < dscpExcNum; i++) {
		prQos->dscpException[i].dscp = *tempq;
		tempq++;
		prQos->dscpException[i].userPriority = *tempq;
		tempq++;
	}
	for (j = 0; j < 8; j++) {
		prQos->dscpRange[j].lDscp = *tempq;
		tempq++;
		prQos->dscpRange[j].hDscp = *tempq;
		tempq++;
		if (prQos->dscpRange[j].hDscp < prQos->dscpRange[j].lDscp)
			DBGLOG(INIT, WARN, "CHECK: dscp h val should larger than dscp l val, i: %d\n", j);
		/* TODO: Here skip the overlap check */
	}
	/*
	 *	kalMemCopy(prQos->dscpException, qosMapSet + 2, dscpExcNum * 2);
	 *	kalMemCopy(prQos->dscpRange, qosMapSet + 2 * dscpExcNum + 2, 16);
	 */

	DBGLOG(INIT, INFO, "QosMapSet DSCP Exception number: %d\n", dscpExcNum);

	return prQos;
}

UINT_8 getUpFromDscp(IN P_GLUE_INFO_T prGlueInfo, IN int type, IN int dscp)
{
	P_BSS_INFO_T prAisBssInfo;
	P_STA_RECORD_T prStaRec;

	int i, j = 0;

	prAisBssInfo = &(prGlueInfo->prAdapter->rWifiVar.arBssInfo[type]);
	if (prAisBssInfo)
		prStaRec = prAisBssInfo->prStaRecOfAP;
	else {
		DBGLOG(INIT, WARN, "qosmap type: %d\n", type);
		return 0xFF;
	}

	if (prStaRec && prStaRec->qosMapSet) {
		for (i = 0; i < prStaRec->qosMapSet->dscpExceptionNum; i++) {
			if (dscp == prStaRec->qosMapSet->dscpException[i].dscp)
				return prStaRec->qosMapSet->dscpException[i].userPriority;
		}
		for (j = 0; j < 8; j++) {
			if (prStaRec->qosMapSet->dscpRange[j].lDscp == 255 &&
				prStaRec->qosMapSet->dscpRange[j].hDscp == 255)
				continue;
			if (dscp >= prStaRec->qosMapSet->dscpRange[j].lDscp &&
				dscp <= prStaRec->qosMapSet->dscpRange[j].hDscp)
				return j;
		}
		return 0xFF;
	} else
		return 0xFF;
}
#endif
