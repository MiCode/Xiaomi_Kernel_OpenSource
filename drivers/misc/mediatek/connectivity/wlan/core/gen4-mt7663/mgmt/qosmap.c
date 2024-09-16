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
/*! \file   "qosmap.c"
 *    \brief  This file including the qosmap related function.
 *
 *    This file provided the macros and functions library support for the
 *    protocol layer qosmap related function.
 *
 */

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "precomp.h"

#if CFG_SUPPORT_PPR2

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

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */



/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
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
static struct _QOS_MAP_SET *QosMapSetMalloc(IN uint8_t dscpExcNum)
{
	if (dscpExcNum)
		return (struct _QOS_MAP_SET *)
				kalMemAlloc((sizeof(struct _QOS_MAP_SET) +
					((dscpExcNum - 1) *
					sizeof(struct _DSCP_EXCEPTION))),
					VIR_MEM_TYPE);
	else
		return (struct _QOS_MAP_SET *)
			kalMemAlloc(sizeof(struct _QOS_MAP_SET), VIR_MEM_TYPE);

}

static void QosMapSetFree(IN struct STA_RECORD *prStaRec)
{
	if (prStaRec && prStaRec->qosMapSet) {
		if (prStaRec->qosMapSet->dscpExceptionNum) {
			kalMemFree(prStaRec->qosMapSet, VIR_MEM_TYPE,
				(sizeof(struct _QOS_MAP_SET) +
				((prStaRec->qosMapSet->dscpExceptionNum - 1) *
					sizeof(struct _DSCP_EXCEPTION))));
		} else
			kalMemFree(prStaRec->qosMapSet,
				VIR_MEM_TYPE, sizeof(struct _QOS_MAP_SET));
	}
}

void QosMapSetRelease(IN struct STA_RECORD *prStaRec)
{
	QosMapSetFree(prStaRec);
}

void handleQosMapConf(IN struct ADAPTER *prAdapter, IN struct SW_RFB *prSwRfb)
{
	struct WLAN_ACTION_FRAME *prRxFrame;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prRxFrame = (struct WLAN_ACTION_FRAME *) prSwRfb->pvHeader;

	switch (prRxFrame->ucAction) {
	case ACTION_ADDTS_REQ:
	case ACTION_ADDTS_RSP:
	case ACTION_SCHEDULE:
		log_dbg(INIT, INFO, "qos action frame received, action: %d\n",
			prRxFrame->ucAction);
		break;
	case ACTION_QOS_MAP_CONFIGURE:
		qosHandleQosMapConfigure(prAdapter, prSwRfb);
		log_dbg(INIT, INFO, "qos map configure frame received, action: %d\n",
			prRxFrame->ucAction);
		break;
	default:
		log_dbg(INIT, INFO, "qos action frame: %d, try to send to supplicant\n",
			prRxFrame->ucAction);
		break;
	}
}

int qosHandleQosMapConfigure(IN struct ADAPTER *prAdapter,
	IN struct SW_RFB *prSwRfb)
{
	struct _ACTION_QOS_MAP_CONFIGURE_FRAME *prRxFrame = NULL;
	struct STA_RECORD *prStaRec;

	prRxFrame =
		(struct _ACTION_QOS_MAP_CONFIGURE_FRAME *) prSwRfb->pvHeader;
	if (!prRxFrame)
		return -1;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	if ((!prStaRec) || (!prStaRec->fgIsInUse))
		return -1;

	log_dbg(INIT, INFO,
	"IEEE 802.11: Received Qos Map Configure Frame from " MACSTR "\n",
		MAC2STR(prStaRec->aucMacAddr));

	if (prStaRec->qosMapSet)
		QosMapSetFree(prStaRec);
	prStaRec->qosMapSet =
		qosParseQosMapSet(prAdapter, prRxFrame->qosMapSet);

	return 0;
}

struct _QOS_MAP_SET *qosParseQosMapSet(IN struct ADAPTER *prAdapter,
	IN uint8_t *qosMapSet)
{
	uint8_t dscpExcNum = 0;
	struct _QOS_MAP_SET *prQos = NULL;
	int i, j = 0;
	uint8_t *tempq = qosMapSet + 2;

	if (IE_ID(qosMapSet) != ELEM_ID_QOS_MAP_SET) {
		DBGLOG(INIT, WARN,
			"Wrong QosMapSet IE ID: %d\n", IE_ID(qosMapSet));
		return NULL;
	}
	if ((IE_LEN(qosMapSet) < 16) || (IE_LEN(qosMapSet) > 58)) {
		DBGLOG(INIT, WARN,
			"Error in QosMapSet IE len: %d\n", IE_LEN(qosMapSet));
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
			log_dbg(INIT, WARN, "CHECK: dscp h val should larger than dscp l val, i: %d\n",
				j);
		/* TODO: Here skip the overlap check */
	}
	/*
	 *	kalMemCopy(prQos->dscpException,
	 *    qosMapSet + 2, dscpExcNum * 2);
	 *	kalMemCopy(prQos->dscpRange,
	 *    qosMapSet + 2 * dscpExcNum + 2, 16);
	 */

	DBGLOG(INIT, INFO, "QosMapSet DSCP Exception number: %d\n", dscpExcNum);

	return prQos;
}

uint8_t getUpFromDscp(IN struct GLUE_INFO *prGlueInfo, IN int type, IN int dscp)
{
	struct BSS_INFO *prAisBssInfo;
	struct STA_RECORD *prStaRec;

	int i, j = 0;

	prAisBssInfo = prGlueInfo->prAdapter->prAisBssInfo;
	if (prAisBssInfo)
		prStaRec = prAisBssInfo->prStaRecOfAP;
	else {
		/* DBGLOG(INIT, WARN, "qosmap type: %d\n", type); */
		return 0xFF;
	}

	prStaRec = prAisBssInfo->prStaRecOfAP;

	if (prStaRec && prStaRec->qosMapSet) {
		for (i = 0; i < prStaRec->qosMapSet->dscpExceptionNum; i++) {
			if (dscp == prStaRec->qosMapSet->dscpException[i].dscp)
				return prStaRec->qosMapSet->dscpException[i].
				userPriority;
		}
		for (j = 0; j < 8; j++) {
			if (prStaRec->qosMapSet->dscpRange[j].lDscp == 255 &&
				prStaRec->qosMapSet->dscpRange[j].hDscp == 255)
				continue;
			if (dscp >= prStaRec->qosMapSet->dscpRange[j].lDscp &&
				dscp >= prStaRec->qosMapSet->dscpRange[j].hDscp)
				return j;
		}
		/* qosMapSet info error */
		DBGLOG(INIT, WARN, "WRONG QosMapSet info, cant get UP\n");
		return 0xFF;
	}
	return 0xFF;
}
#endif
