/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/mgmt/bow_fsm.h#1 $
*/

/*! \file   bow_fsm.h
    \brief  Declaration of functions and finite state machine for BOW Module.

    Declaration of functions and finite state machine for BOW Module.
*/

/*
** $Log: bow_fsm.h $
**
** 09 17 2012 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Duplicate source from MT6620 v2.3 driver branch
** (Davinci label: MT6620_WIFI_Driver_V2_3_120913_1942_As_MT6630_Base)
 *
 * 05 22 2011 terry.wu
 * [WCXRP00000735] [MT6620 Wi-Fi][BoW][FW/Driver] Protect BoW connection establishment
 * Submit missing BoW header files.
 *
 * 03 27 2011 chinghwa.yu
 * [WCXRP00000065] Update BoW design and settings
 * Support multiple physical link.
 *
 * 02 16 2011 chinghwa.yu
 * [WCXRP00000065] Update BoW design and settings
 * Add bowNotifyAllLinkDisconnected  interface and change channel grant procedure for bow starting..
 *
 * 02 15 2011 chinghwa.yu
 * [WCXRP00000065] Update BoW design and settings
 * Add channel previledge into _BOW_FSM_INFO_T.
 *
 * 09 16 2010 chinghwa.yu
 * NULL
 * update bowChangeMediaState.
 *
 * 08 24 2010 chinghwa.yu
 * NULL
 * Update BOW for the 1st time.
 */

#ifndef _BOW_FSM_H
#define _BOW_FSM_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

#define BOW_BG_SCAN_INTERVAL_MIN_SEC        2	/* 30 // exponential to 960 */
#define BOW_BG_SCAN_INTERVAL_MAX_SEC        2	/* 960 // 16min */

#define BOW_DELAY_TIME_OF_DISCONNECT_SEC    10

#define BOW_BEACON_TIMEOUT_COUNT_STARTING   10
#define BOW_BEACON_TIMEOUT_GUARD_TIME_SEC   1	/* Second */

#define BOW_BEACON_MAX_TIMEOUT_TU           100
#define BOW_BEACON_MIN_TIMEOUT_TU           5
#define BOW_BEACON_MAX_TIMEOUT_VALID        TRUE
#define BOW_BEACON_MIN_TIMEOUT_VALID        TRUE

#define BOW_BMC_MAX_TIMEOUT_TU              100
#define BOW_BMC_MIN_TIMEOUT_TU              5
#define BOW_BMC_MAX_TIMEOUT_VALID           TRUE
#define BOW_BMC_MIN_TIMEOUT_VALID           TRUE

#define BOW_JOIN_CH_GRANT_THRESHOLD         10
#define BOW_JOIN_CH_REQUEST_INTERVAL        2000

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

typedef enum _ENUM_BOW_STATE_T {
	BOW_STATE_IDLE = 0,
	BOW_STATE_SEARCH,
	BOW_STATE_SCAN,
	BOW_STATE_ONLINE_SCAN,
	BOW_STATE_LOOKING_FOR,
	BOW_STATE_WAIT_FOR_NEXT_SCAN,
	BOW_STATE_REQ_CHANNEL_JOIN,
	BOW_STATE_REQ_CHANNEL_ALONE,
	BOW_STATE_REQ_CHANNEL_MERGE,
	BOW_STATE_JOIN,
	BOW_STATE_IBSS_ALONE,
	BOW_STATE_IBSS_MERGE,
	BOW_STATE_NORMAL_TR,
	BOW_STATE_NUM
} ENUM_BOW_STATE_T;

typedef struct _BOW_FSM_INFO_T {
	/* Channel Privilege */
	BOOLEAN fgIsChannelRequested;
	BOOLEAN fgIsChannelGranted;
	UINT_32 u4ChGrantedInterval;

	UINT_8 ucPrimaryChannel;
	ENUM_BAND_T eBand;
	UINT_16 u2BeaconInterval;

	P_STA_RECORD_T prTargetStaRec;
	P_BSS_DESC_T prTargetBssDesc;	/* For destination */

	UINT_8 aucPeerAddress[6];
	UINT_8 ucBssIndex;	/* Assume there is only 1 BSS for BOW */
	UINT_8 ucRole;		/* Initiator or responder */
	UINT_8 ucAvailableAuthTypes;	/* Used for AUTH_MODE_AUTO_SWITCH */

	BOOLEAN fgSupportQoS;

	/* Sequence number of requested message. */
	UINT_8 ucSeqNumOfChReq;
	UINT_8 ucSeqNumOfReqMsg;
	UINT_8 ucSeqNumOfScnMsg;
	UINT_8 ucSeqNumOfScanReq;
	UINT_8 ucSeqNumOfCancelMsg;

	/* Timer */
	TIMER_T rStartingBeaconTimer;	/* For device discovery time of each discovery request from user. */
	TIMER_T rChGrantedTimer;

	/* can be deleted? */
	TIMER_T rIndicationOfDisconnectTimer;



} BOW_FSM_INFO_T, *P_BOW_FSM_INFO_T;


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

#define bowChangeMediaState(_prBssInfo, _eNewMediaState) \
	    (_prBssInfo->eConnectionState = (_eNewMediaState));
	    /* (_prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_BOW_INDEX].eConnectionState = (_eNewMediaState)); */

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/



#endif
