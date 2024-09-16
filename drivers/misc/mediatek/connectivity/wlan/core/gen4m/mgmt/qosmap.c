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

	qosParseQosMapSet(prAdapter, prStaRec, prRxFrame->qosMapSet);

	return 0;
}

void qosParseQosMapSet(IN struct ADAPTER *prAdapter,
	IN struct STA_RECORD *prStaRec,
	IN uint8_t *qosMapSet)
{
	uint8_t dscpExcNum = 0;
	int i = 0;
	uint8_t *tempq = qosMapSet + 2;
	uint8_t *qosmapping = prStaRec->qosMapSet;
	uint8_t excTable[64];

	if (IE_ID(qosMapSet) != ELEM_ID_QOS_MAP_SET) {
		DBGLOG(INIT, WARN,
			"Wrong QosMapSet IE ID: %d\n", IE_ID(qosMapSet));
		return;
	}
	if ((IE_LEN(qosMapSet) < 16) || (IE_LEN(qosMapSet) > 58)) {
		DBGLOG(INIT, WARN,
			"Error in QosMapSet IE len: %d\n", IE_LEN(qosMapSet));
		return;
	}

	qosMapSetInit(prStaRec);
	kalMemSet(excTable, 0, 64);

	dscpExcNum = (IE_LEN(qosMapSet) - WMM_UP_INDEX_NUM * 2) / 2;
	for (i = 0; i < dscpExcNum; i++) {
		uint8_t dscp = *tempq++;
		uint8_t up = *tempq++;

		if (dscp < 64 && up < WMM_UP_INDEX_NUM) {
			qosmapping[dscp] = up;
			excTable[dscp] = TRUE;
		}
	}

	for (i = 0; i < WMM_UP_INDEX_NUM; i++) {
		uint8_t lDscp = *tempq++;
		uint8_t hDscp = *tempq++;
		uint8_t dscp;

		if (lDscp == 255 && hDscp == 255) {
			log_dbg(INIT, WARN, "UP %d is not used\n", i);
			continue;
		}

		if (hDscp < lDscp) {
			log_dbg(INIT, WARN, "CHECK: UP %d, h %d, l %d\n",
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

void qosMapSetInit(IN struct STA_RECORD *prStaRec)
{
	/* DSCP to UP maaping based on RFC8325 in the range 0 to 63 */
	static uint8_t dscp2up[64] = {
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

uint8_t getUpFromDscp(IN struct GLUE_INFO *prGlueInfo, IN int type, IN int dscp)
{
	struct BSS_INFO *prAisBssInfo;
	struct STA_RECORD *prStaRec;

	prAisBssInfo = aisGetAisBssInfo(
		prGlueInfo->prAdapter, type);
	if (prAisBssInfo)
		prStaRec = prAisBssInfo->prStaRecOfAP;
	else
		return 0xFF;

	if (prStaRec && dscp >= 0 && dscp < 64)
		return prStaRec->qosMapSet[dscp];

	return 0xFF;
}
#endif
