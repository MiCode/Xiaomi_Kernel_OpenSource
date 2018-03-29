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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/mgmt/hem_mbox.c#7
*/

/*! \file   "hem_mbox.c"
    \brief

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
#if DBG
/*lint -save -e64 Type mismatch */
static PUINT_8 apucDebugMsg[] = {
	(PUINT_8) DISP_STRING("MID_MNY_CNM_CH_REQ"),
	(PUINT_8) DISP_STRING("MID_MNY_CNM_CH_ABORT"),
	(PUINT_8) DISP_STRING("MID_CNM_AIS_CH_GRANT"),
	(PUINT_8) DISP_STRING("MID_CNM_P2P_CH_GRANT"),
	(PUINT_8) DISP_STRING("MID_CNM_BOW_CH_GRANT"),

	(PUINT_8) DISP_STRING("MID_AIS_SCN_SCAN_REQ"),
	(PUINT_8) DISP_STRING("MID_AIS_SCN_SCAN_REQ_V2"),
	(PUINT_8) DISP_STRING("MID_AIS_SCN_SCAN_CANCEL"),
	(PUINT_8) DISP_STRING("MID_P2P_SCN_SCAN_REQ"),
	(PUINT_8) DISP_STRING("MID_P2P_SCN_SCAN_REQ_V2"),
	(PUINT_8) DISP_STRING("MID_P2P_SCN_SCAN_CANCEL"),
	(PUINT_8) DISP_STRING("MID_BOW_SCN_SCAN_REQ"),
	(PUINT_8) DISP_STRING("MID_BOW_SCN_SCAN_REQ_V2"),
	(PUINT_8) DISP_STRING("MID_BOW_SCN_SCAN_CANCEL"),
	(PUINT_8) DISP_STRING("MID_RLM_SCN_SCAN_REQ"),
	(PUINT_8) DISP_STRING("MID_RLM_SCN_SCAN_REQ_V2"),
	(PUINT_8) DISP_STRING("MID_RLM_SCN_SCAN_CANCEL"),
	(PUINT_8) DISP_STRING("MID_SCN_AIS_SCAN_DONE"),
	(PUINT_8) DISP_STRING("MID_SCN_P2P_SCAN_DONE"),
	(PUINT_8) DISP_STRING("MID_SCN_BOW_SCAN_DONE"),
	(PUINT_8) DISP_STRING("MID_SCN_RLM_SCAN_DONE"),

	(PUINT_8) DISP_STRING("MID_OID_AIS_FSM_JOIN_REQ"),
	(PUINT_8) DISP_STRING("MID_OID_AIS_FSM_ABORT"),
	(PUINT_8) DISP_STRING("MID_AIS_SAA_FSM_START"),
	(PUINT_8) DISP_STRING("MID_AIS_SAA_FSM_ABORT"),
	(PUINT_8) DISP_STRING("MID_SAA_AIS_JOIN_COMPLETE"),

#if CFG_ENABLE_BT_OVER_WIFI
	(PUINT_8) DISP_STRING("MID_BOW_SAA_FSM_START"),
	(PUINT_8) DISP_STRING("MID_BOW_SAA_FSM_ABORT"),
	(PUINT_8) DISP_STRING("MID_SAA_BOW_JOIN_COMPLETE"),
#endif

#if CFG_ENABLE_WIFI_DIRECT
	(PUINT_8) DISP_STRING("MID_P2P_SAA_FSM_START"),
	(PUINT_8) DISP_STRING("MID_P2P_SAA_FSM_ABORT"),
	(PUINT_8) DISP_STRING("MID_SAA_P2P_JOIN_COMPLETE"),

	(PUINT_8) DISP_STRING("MID_MNY_P2P_FUN_SWITCH"),
	(PUINT_8) DISP_STRING("MID_MNY_P2P_DEVICE_DISCOVERY"),
	(PUINT_8) DISP_STRING("MID_MNY_P2P_CONNECTION_REQ"),
	(PUINT_8) DISP_STRING("MID_MNY_P2P_CONNECTION_ABORT"),
	(PUINT_8) DISP_STRING("MID_MNY_P2P_BEACON_UPDATE"),
	(PUINT_8) DISP_STRING("MID_MNY_P2P_STOP_AP"),
	(PUINT_8) DISP_STRING("MID_MNY_P2P_CHNL_REQ"),
	(PUINT_8) DISP_STRING("MID_MNY_P2P_CHNL_ABORT"),
	(PUINT_8) DISP_STRING("MID_MNY_P2P_MGMT_TX"),
	(PUINT_8) DISP_STRING("MID_MNY_P2P_GROUP_DISSOLVE"),
	(PUINT_8) DISP_STRING("MID_MNY_P2P_MGMT_FRAME_REGISTER"),
	(PUINT_8) DISP_STRING("MID_MNY_P2P_NET_DEV_REGISTER"),
	(PUINT_8) DISP_STRING("MID_MNY_P2P_START_AP"),
	(PUINT_8) DISP_STRING("MID_MNY_P2P_UPDATE_IE_BUF"),
#endif

#if CFG_SUPPORT_ADHOC
	/* (PUINT_8)DISP_STRING("MID_AIS_CNM_CREATE_IBSS_REQ"), */
	/* (PUINT_8)DISP_STRING("MID_CNM_AIS_CREATE_IBSS_GRANT"), */
	/* (PUINT_8)DISP_STRING("MID_AIS_CNM_MERGE_IBSS_REQ"), */
	/* (PUINT_8)DISP_STRING("MID_CNM_AIS_MERGE_IBSS_GRANT"), */
	(PUINT_8) DISP_STRING("MID_SCN_AIS_FOUND_IBSS"),
#endif /* CFG_SUPPORT_ADHOC */

	(PUINT_8) DISP_STRING("MID_SAA_AIS_FSM_ABORT"),
	(PUINT_8) DISP_STRING("MID_MNY_AIS_REMAIN_ON_CHANNEL"),
	(PUINT_8) DISP_STRING("MID_MNY_AIS_CANCEL_REMAIN_ON_CHANNEL"),
	(PUINT_8) DISP_STRING("MID_MNY_AIS_MGMT_TX")

};

/*lint -restore */
#endif /* DBG */

/* This message entry will be re-ordered based on the message ID order
 * by invoking mboxInitMsgMap()
 */
static MSG_HNDL_ENTRY_T arMsgMapTable[] = {
	{MID_MNY_CNM_CH_REQ, cnmChMngrRequestPrivilege},
	{MID_MNY_CNM_CH_ABORT, cnmChMngrAbortPrivilege},
	{MID_CNM_AIS_CH_GRANT, aisFsmRunEventChGrant},
#if CFG_ENABLE_WIFI_DIRECT
	{MID_CNM_P2P_CH_GRANT, p2pFsmRunEventChGrant},	/*set in gl_p2p_init.c */
#else
	{MID_CNM_P2P_CH_GRANT, mboxDummy},
#endif

#if CFG_ENABLE_BT_OVER_WIFI
	{MID_CNM_BOW_CH_GRANT, bowRunEventChGrant},
#else
	{MID_CNM_BOW_CH_GRANT, mboxDummy},
#endif

    /*--------------------------------------------------*/
	/* SCN Module Mailbox Messages                      */
    /*--------------------------------------------------*/
	{MID_AIS_SCN_SCAN_REQ, scnFsmMsgStart},
	{MID_AIS_SCN_SCAN_REQ_V2, scnFsmMsgStart},
	{MID_AIS_SCN_SCAN_CANCEL, scnFsmMsgAbort},
	{MID_P2P_SCN_SCAN_REQ, scnFsmMsgStart},
	{MID_P2P_SCN_SCAN_REQ_V2, scnFsmMsgStart},
	{MID_P2P_SCN_SCAN_CANCEL, scnFsmMsgAbort},
	{MID_BOW_SCN_SCAN_REQ, scnFsmMsgStart},
	{MID_BOW_SCN_SCAN_REQ_V2, scnFsmMsgStart},
	{MID_BOW_SCN_SCAN_CANCEL, scnFsmMsgAbort},
	{MID_RLM_SCN_SCAN_REQ, scnFsmMsgStart},
	{MID_RLM_SCN_SCAN_REQ_V2, scnFsmMsgStart},
	{MID_RLM_SCN_SCAN_CANCEL, scnFsmMsgAbort},
	{MID_SCN_AIS_SCAN_DONE, aisFsmRunEventScanDone},
#if CFG_ENABLE_WIFI_DIRECT
	{MID_SCN_P2P_SCAN_DONE, p2pFsmRunEventScanDone},	/*set in gl_p2p_init.c */
#else
	{MID_SCN_P2P_SCAN_DONE, mboxDummy},
#endif

#if CFG_ENABLE_BT_OVER_WIFI
	{MID_SCN_BOW_SCAN_DONE, bowResponderScanDone},
#else
	{MID_SCN_BOW_SCAN_DONE, mboxDummy},
#endif
	{MID_SCN_RLM_SCAN_DONE, rlmObssScanDone},

    /*--------------------------------------------------*/
	/* AIS Module Mailbox Messages                      */
    /*--------------------------------------------------*/
	{MID_OID_AIS_FSM_JOIN_REQ, aisFsmRunEventAbort},
	{MID_OID_AIS_FSM_ABORT, aisFsmRunEventAbort},
	{MID_AIS_SAA_FSM_START, saaFsmRunEventStart},
	{MID_AIS_SAA_FSM_ABORT, saaFsmRunEventAbort},
	{MID_SAA_AIS_JOIN_COMPLETE, aisFsmRunEventJoinComplete},

#if CFG_ENABLE_BT_OVER_WIFI
    /*--------------------------------------------------*/
	/* BOW Module Mailbox Messages                      */
    /*--------------------------------------------------*/
	{MID_BOW_SAA_FSM_START, saaFsmRunEventStart},
	{MID_BOW_SAA_FSM_ABORT, saaFsmRunEventAbort},
	{MID_SAA_BOW_JOIN_COMPLETE, bowFsmRunEventJoinComplete},
#endif

#if CFG_ENABLE_WIFI_DIRECT	/*set in gl_p2p_init.c */
	{MID_P2P_SAA_FSM_START, saaFsmRunEventStart},
	{MID_P2P_SAA_FSM_ABORT, saaFsmRunEventAbort},
	{MID_SAA_P2P_JOIN_COMPLETE, p2pRoleFsmRunEventJoinComplete},	/* V */

	{MID_MNY_P2P_FUN_SWITCH, p2pRoleFsmRunEventSwitchOPMode},
	{MID_MNY_P2P_DEVICE_DISCOVERY, p2pFsmRunEventScanRequest},	/* V */
	{MID_MNY_P2P_CONNECTION_REQ, p2pRoleFsmRunEventConnectionRequest},
	{MID_MNY_P2P_CONNECTION_ABORT, p2pRoleFsmRunEventConnectionAbort},
	{MID_MNY_P2P_BEACON_UPDATE, p2pFsmRunEventBeaconUpdate},
	{MID_MNY_P2P_STOP_AP, p2pRoleFsmRunEventStopAP},
	{MID_MNY_P2P_CHNL_REQ, p2pDevFsmRunEventChannelRequest},	/* V */
	{MID_MNY_P2P_CHNL_ABORT, p2pDevFsmRunEventChannelAbort},	/* V */
	{MID_MNY_P2P_MGMT_TX, p2pDevFsmRunEventMgmtTx},	/* V */
	{MID_MNY_P2P_GROUP_DISSOLVE, p2pRoleFsmRunEventDissolve},
	{MID_MNY_P2P_MGMT_FRAME_REGISTER, p2pDevFsmRunEventMgmtFrameRegister},
	{MID_MNY_P2P_NET_DEV_REGISTER, p2pFsmRunEventNetDeviceRegister},
	{MID_MNY_P2P_START_AP, p2pRoleFsmRunEventStartAP},
	{MID_MNY_P2P_MGMT_FRAME_UPDATE, p2pFsmRunEventUpdateMgmtFrame},
	{MID_MNY_P2P_EXTEND_LISTEN_INTERVAL, p2pFsmRunEventExtendListen},
#if CFG_SUPPORT_WFD
	{MID_MNY_P2P_WFD_CFG_UPDATE, p2pFsmRunEventWfdSettingUpdate},
#endif

#endif

#if CFG_SUPPORT_ADHOC
	{MID_SCN_AIS_FOUND_IBSS, aisFsmRunEventFoundIBSSPeer},
#endif /* CFG_SUPPORT_ADHOC */

	{MID_SAA_AIS_FSM_ABORT, aisFsmRunEventAbort},
	{MID_MNY_AIS_REMAIN_ON_CHANNEL, aisFsmRunEventRemainOnChannel},
	{MID_MNY_AIS_CANCEL_REMAIN_ON_CHANNEL, aisFsmRunEventCancelRemainOnChannel},
	{MID_MNY_AIS_MGMT_TX, aisFsmRunEventMgmtFrameTx},
	{MID_MNY_CNM_REQ_CH_UTIL, cnmRequestChannelUtilization},
	{MID_CNM_AIS_RSP_CH_UTIL, aisRunEventChnlUtilRsp},
	{MID_MNY_CNM_SCAN_CONTINUE, scnFsmMsgStart},
};

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

#if DBG
#define MBOX_HNDL_MSG(prAdapter, prMsg) do { \
	ASSERT(arMsgMapTable[prMsg->eMsgId].pfMsgHndl); \
	if (arMsgMapTable[prMsg->eMsgId].pfMsgHndl) { \
		DBGLOG(CNM, LOUD, "DO MSG [%d: %s]\n", prMsg->eMsgId, apucDebugMsg[prMsg->eMsgId]); \
		arMsgMapTable[prMsg->eMsgId].pfMsgHndl(prAdapter, prMsg); \
	} \
	else { \
	    DBGLOG(CNM, ERROR, "NULL fptr for MSG [%d]\n", prMsg->eMsgId); \
	    cnmMemFree(prAdapter, prMsg); \
	} \
} while (0)
#else
#define MBOX_HNDL_MSG(prAdapter, prMsg) do { \
	ASSERT(arMsgMapTable[prMsg->eMsgId].pfMsgHndl); \
	if (arMsgMapTable[prMsg->eMsgId].pfMsgHndl) { \
		DBGLOG(CNM, LOUD, "DO MSG [%d]\n", prMsg->eMsgId); \
		arMsgMapTable[prMsg->eMsgId].pfMsgHndl(prAdapter, prMsg); \
	} \
	else { \
	    DBGLOG(CNM, ERROR, "NULL fptr for MSG [%d]\n", prMsg->eMsgId); \
	    cnmMemFree(prAdapter, prMsg); \
	} \
} while (0)
#endif
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
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID mboxInitMsgMap(VOID)
{
	UINT_32 i, idx;
	MSG_HNDL_ENTRY_T rTempEntry;

	ASSERT((sizeof(arMsgMapTable) / sizeof(MSG_HNDL_ENTRY_T)) == MID_TOTAL_NUM);

	for (i = 0; i < MID_TOTAL_NUM; i++) {
		if (arMsgMapTable[i].eMsgId == (ENUM_MSG_ID_T) i)
			continue;
		for (idx = i + 1; idx < MID_TOTAL_NUM; idx++) {
			if (arMsgMapTable[idx].eMsgId == (ENUM_MSG_ID_T) i)
				break;
		}
		ASSERT(idx < MID_TOTAL_NUM);
		if (idx >= MID_TOTAL_NUM)
			continue;

		/* Swap target entry and current entry */
		rTempEntry.eMsgId = arMsgMapTable[idx].eMsgId;
		rTempEntry.pfMsgHndl = arMsgMapTable[idx].pfMsgHndl;

		arMsgMapTable[idx].eMsgId = arMsgMapTable[i].eMsgId;
		arMsgMapTable[idx].pfMsgHndl = arMsgMapTable[i].pfMsgHndl;

		arMsgMapTable[i].eMsgId = rTempEntry.eMsgId;
		arMsgMapTable[i].pfMsgHndl = rTempEntry.pfMsgHndl;
	}

	/* Verify the correctness of final message map */
	for (i = 0; i < MID_TOTAL_NUM; i++) {
		ASSERT(arMsgMapTable[i].eMsgId == (ENUM_MSG_ID_T) i);
		while (arMsgMapTable[i].eMsgId != (ENUM_MSG_ID_T) i)
			;
	}

}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID mboxSetup(IN P_ADAPTER_T prAdapter, IN ENUM_MBOX_ID_T eMboxId)
{
	P_MBOX_T prMbox;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(eMboxId < MBOX_ID_TOTAL_NUM);
	ASSERT(prAdapter);

	prMbox = &(prAdapter->arMbox[eMboxId]);

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_MAILBOX);
	LINK_INITIALIZE(&prMbox->rLinkHead);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_MAILBOX);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID
mboxSendMsg(IN P_ADAPTER_T prAdapter,
	    IN ENUM_MBOX_ID_T eMboxId, IN P_MSG_HDR_T prMsg, IN EUNM_MSG_SEND_METHOD_T eMethod)
{
	P_MBOX_T prMbox;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(eMboxId < MBOX_ID_TOTAL_NUM);
	ASSERT(prMsg);
	ASSERT(prAdapter);

	prMbox = &(prAdapter->arMbox[eMboxId]);

	switch (eMethod) {
	case MSG_SEND_METHOD_BUF:
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_MAILBOX);
		LINK_INSERT_TAIL(&prMbox->rLinkHead, &prMsg->rLinkEntry);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_MAILBOX);

		/* to wake up main service thread */
		GLUE_SET_EVENT(prAdapter->prGlueInfo);

		break;

	case MSG_SEND_METHOD_UNBUF:
		MBOX_HNDL_MSG(prAdapter, prMsg);
		break;

	default:
		ASSERT(0);
		break;
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID mboxRcvAllMsg(IN P_ADAPTER_T prAdapter, ENUM_MBOX_ID_T eMboxId)
{
	P_MBOX_T prMbox;
	P_MSG_HDR_T prMsg;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(eMboxId < MBOX_ID_TOTAL_NUM);
	ASSERT(prAdapter);

	prMbox = &(prAdapter->arMbox[eMboxId]);

	while (!LINK_IS_EMPTY(&prMbox->rLinkHead)) {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_MAILBOX);
		LINK_REMOVE_HEAD(&prMbox->rLinkHead, prMsg, P_MSG_HDR_T);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_MAILBOX);

		ASSERT(prMsg);
		MBOX_HNDL_MSG(prAdapter, prMsg);
	}

}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID mboxInitialize(IN P_ADAPTER_T prAdapter)
{
	UINT_32 i;

	ASSERT(prAdapter);

	/* Initialize Mailbox */
	mboxInitMsgMap();

	/* Setup/initialize each mailbox */
	for (i = 0; i < MBOX_ID_TOTAL_NUM; i++)
		mboxSetup(prAdapter, i);

}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID mboxDestroy(IN P_ADAPTER_T prAdapter)
{
	P_MBOX_T prMbox;
	P_MSG_HDR_T prMsg;
	UINT_8 i;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);

	for (i = 0; i < MBOX_ID_TOTAL_NUM; i++) {
		prMbox = &(prAdapter->arMbox[i]);

		while (!LINK_IS_EMPTY(&prMbox->rLinkHead)) {
			KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_MAILBOX);
			LINK_REMOVE_HEAD(&prMbox->rLinkHead, prMsg, P_MSG_HDR_T);
			KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_MAILBOX);

			ASSERT(prMsg);
			cnmMemFree(prAdapter, prMsg);
		}
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This is dummy function to prevent empty arMsgMapTable[] for compiling.
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID mboxDummy(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	ASSERT(prAdapter);

	cnmMemFree(prAdapter, prMsgHdr);

}
