/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/******************************************************************************
*
* Name:  btcApi.h
*
* Description: BTC Events Layer API definitions.
*
*
******************************************************************************/

#ifndef __BTC_API_H__
#define __BTC_API_H__

#include "vos_types.h"
#include "vos_timer.h"
#include "vos_nvitem.h"

#define BT_INVALID_CONN_HANDLE (0xFFFF)  /**< Invalid connection handle */

/* ACL and Sync connection attempt results */
#define BT_CONN_STATUS_FAIL      (0)         /**< Connection failed */
#define BT_CONN_STATUS_SUCCESS   (1)         /**< Connection successful */
#define BT_CONN_STATUS_MAX       (2)         /**< This and beyond are invalid values */

/** ACL and Sync link types
  These must match the Bluetooth Spec!
*/
#define BT_SCO                  (0)   /**< SCO Link */
#define BT_ACL                  (1)   /**< ACL Link */
#define BT_eSCO                 (2)   /**< eSCO Link */
#define BT_LINK_TYPE_MAX        (3)   /**< This value and higher are invalid */

/** ACL link modes
    These must match the Bluetooth Spec!
*/
#define BT_ACL_ACTIVE           (0)   /**< Active mode */
#define BT_ACL_HOLD             (1)   /**< Hold mode */
#define BT_ACL_SNIFF            (2)   /**< Sniff mode */
#define BT_ACL_PARK             (3)   /**< Park mode */
#define BT_ACL_MODE_MAX         (4)   /**< This value and higher are invalid */

/**
 * A2DP BTC max no of BT sub intervals
 *
 * **/
#define BTC_MAX_NUM_ACL_BT_SUB_INTS (7)

/** BTC Executions Modes allowed to be set by user
*/
#define BTC_SMART_COEXISTENCE   (0) /** BTC Mapping Layer decides whats best */
#define BTC_WLAN_ONLY           (1) /** WLAN takes all mode */
#define BTC_PTA_ONLY            (2) /** Allow only 3 wire protocol in H/W */
#define BTC_SMART_MAX_WLAN      (3) /** BTC Mapping Layer decides whats best, WLAN weighted */
#define BTC_SMART_MAX_BT        (4) /** BTC Mapping Layer decides whats best, BT weighted */
#define BTC_SMART_BT_A2DP       (5) /** BTC Mapping Layer decides whats best, balanced + BT A2DP weight */
#define BT_EXEC_MODE_MAX        (6) /** This and beyond are invalid values */

/** Enumeration of different kinds actions that BTC Mapping Layer
    can do if PM indication (to AP) fails.
*/
#define BTC_RESTART_CURRENT     (0) /** Restart the interval we just failed to leave */
#define BTC_START_NEXT          (1) /** Start the next interval even though the PM transition at the AP was unsuccessful */
#define BTC_ACTION_TYPE_MAX     (2) /** This and beyond are invalid values */

#define BTC_BT_INTERVAL_MODE1_DEFAULT       (120) /** BT Interval in Mode 1 */
#define BTC_WLAN_INTERVAL_MODE1_DEFAULT     (30)  /** WLAN Interval in Mode 1 */

/** Bitmaps used for maintaining various BT events that requires
    enough time to complete such that it might require disbling of
    heartbeat monitoring to avoid WLAN link loss with the AP
*/
#define BT_INQUIRY_STARTED                  (1<<0)
#define BT_PAGE_STARTED                     (1<<1)
#define BT_CREATE_ACL_CONNECTION_STARTED    (1<<2)
#define BT_CREATE_SYNC_CONNECTION_STARTED   (1<<3)

/** Maximum time duration in milliseconds between a specific BT start event and its
    respective stop event, before it can be declared timed out on receiving the stop event.
*/
#define BT_MAX_EVENT_DONE_TIMEOUT   45000

/*
    To suppurt multiple SCO connections for BT+UAPSD work
*/
#define BT_MAX_SCO_SUPPORT  3
#define BT_MAX_ACL_SUPPORT  3
#define BT_MAX_DISCONN_SUPPORT (BT_MAX_SCO_SUPPORT+BT_MAX_ACL_SUPPORT)
#define BT_MAX_NUM_EVENT_ACL_DEFERRED  4  //We may need to defer these many BT events for ACL
#define BT_MAX_NUM_EVENT_SCO_DEFERRED  4  //We may need to defer these many BT events for SYNC

/** Default values for the BTC tunables parameters
*/
#define BTC_STATIC_BT_LEN_INQ_DEF     (120000)  // 120 msec
#define BTC_STATIC_BT_LEN_PAGE_DEF     (10000)  // 10 msec (don't care)
#define BTC_STATIC_BT_LEN_CONN_DEF     (10000)  // 10 msec (don't care)
#define BTC_STATIC_BT_LEN_LE_DEF       (10000)  // 10 msec (don't care)
#define BTC_STATIC_WLAN_LEN_INQ_DEF    (30000)  // 30 msec
#define BTC_STATIC_WLAN_LEN_PAGE_DEF       (0)  // 0 msec (BT takes all)
#define BTC_STATIC_WLAN_LEN_CONN_DEF       (0)  // 0 msec (BT takes all)
#define BTC_STATIC_WLAN_LEN_LE_DEF         (0)  // 0 msec (BT takes all)
#define BTC_DYNAMIC_BT_LEN_MAX_DEF    (250000)  // 250 msec
#define BTC_DYNAMIC_WLAN_LEN_MAX_DEF   (45000)  // 45 msec
#define BTC_SCO_BLOCK_PERC_DEF             (1)  // 1 percent
#define BTC_DHCP_ON_A2DP_DEF               (1)  // ON
#define BTC_DHCP_ON_SCO_DEF                (0)  // OFF

/*
 * Number of victim tables and mws coex configurations
 */
#define MWS_COEX_MAX_VICTIM_TABLE             10
#define MWS_COEX_MAX_CONFIG                   6

/** Enumeration of all the different kinds of BT events
*/
typedef enum eSmeBtEventType
{
  BT_EVENT_DEVICE_SWITCHED_ON,
  BT_EVENT_DEVICE_SWITCHED_OFF,
  BT_EVENT_INQUIRY_STARTED,
  BT_EVENT_INQUIRY_STOPPED,
  BT_EVENT_INQUIRY_SCAN_STARTED,
  BT_EVENT_INQUIRY_SCAN_STOPPED,
  BT_EVENT_PAGE_STARTED,
  BT_EVENT_PAGE_STOPPED,
  BT_EVENT_PAGE_SCAN_STARTED,
  BT_EVENT_PAGE_SCAN_STOPPED,
  BT_EVENT_CREATE_ACL_CONNECTION,
  BT_EVENT_ACL_CONNECTION_COMPLETE,
  BT_EVENT_CREATE_SYNC_CONNECTION,
  BT_EVENT_SYNC_CONNECTION_COMPLETE,
  BT_EVENT_SYNC_CONNECTION_UPDATED,
  BT_EVENT_DISCONNECTION_COMPLETE,
  BT_EVENT_MODE_CHANGED,
  BT_EVENT_A2DP_STREAM_START,
  BT_EVENT_A2DP_STREAM_STOP,
  BT_EVENT_TYPE_MAX,    //This and beyond are invalid values
} tSmeBtEventType;

/** BT-AMP events type
*/
typedef enum eSmeBtAmpEventType
{
  BTAMP_EVENT_CONNECTION_START,
  BTAMP_EVENT_CONNECTION_STOP,
  BTAMP_EVENT_CONNECTION_TERMINATED,
  BTAMP_EVENT_TYPE_MAX, //This and beyond are invalid values
} tSmeBtAmpEventType;


/**Data structure that specifies the needed event parameters for
    BT_EVENT_CREATE_ACL_CONNECTION and BT_EVENT_ACL_CONNECTION_COMPLETE
*/
typedef struct sSmeBtAclConnectionParam
{
   v_U8_t       bdAddr[6];
   v_U16_t      connectionHandle;
   v_U8_t       status;
} tSmeBtAclConnectionParam, *tpSmeBtAclConnectionParam;

/** Data structure that specifies the needed event parameters for
    BT_EVENT_CREATE_SYNC_CONNECTION, BT_EVENT_SYNC_CONNECTION_COMPLETE
    and BT_EVENT_SYNC_CONNECTION_UPDATED
*/
typedef struct sSmeBtSyncConnectionParam
{
   v_U8_t       bdAddr[6];
   v_U16_t      connectionHandle;
   v_U8_t       status;
   v_U8_t       linkType;
   v_U8_t       scoInterval; //units in number of 625us slots
   v_U8_t       scoWindow;   //units in number of 625us slots
   v_U8_t       retransmisisonWindow; //units in number of 625us slots
} tSmeBtSyncConnectionParam, *tpSmeBtSyncConnectionParam;

typedef struct sSmeBtSyncUpdateHist
{
    tSmeBtSyncConnectionParam btSyncConnection;
    v_BOOL_t fValid;
} tSmeBtSyncUpdateHist, *tpSmeBtSyncUpdateHist;

/**Data structure that specifies the needed event parameters for
    BT_EVENT_MODE_CHANGED
*/
typedef struct sSmeBtAclModeChangeParam
{
    v_U16_t     connectionHandle;
    v_U8_t      mode;
} tSmeBtAclModeChangeParam, *tpSmeBtAclModeChangeParam;

/*Data structure that specifies the needed event parameters for
    BT_EVENT_DISCONNECTION_COMPLETE
*/
typedef struct sSmeBtDisconnectParam
{
   v_U16_t connectionHandle;
} tSmeBtDisconnectParam, *tpSmeBtDisconnectParam;

/*Data structure that specifies the needed event parameters for
    BT_EVENT_A2DP_STREAM_START
    BT_EVENT_A2DP_STREAM_STOP
*/
typedef struct sSmeBtA2DPParam
{
   v_U8_t       bdAddr[6];
} tSmeBtA2DPParam, *tpSmeBtA2DPParam;


/** Generic Bluetooth Event structure for BTC
*/
typedef struct sSmeBtcBtEvent
{
   tSmeBtEventType btEventType;
   union
   {
      v_U8_t                    bdAddr[6];    /**< For events with only a BT Addr in event_data */
      tSmeBtAclConnectionParam  btAclConnection;
      tSmeBtSyncConnectionParam btSyncConnection;
      tSmeBtDisconnectParam     btDisconnect;
      tSmeBtAclModeChangeParam  btAclModeChange;
   }uEventParam;
} tSmeBtEvent, *tpSmeBtEvent;


/**
    BT-AMP Event Structure
*/
typedef struct sSmeBtAmpEvent
{
  tSmeBtAmpEventType btAmpEventType;

} tSmeBtAmpEvent, *tpSmeBtAmpEvent;


/** Data structure that specifies the BTC Configuration parameters
*/
typedef struct sSmeBtcConfig
{
   v_U8_t       btcExecutionMode;
   v_U8_t       btcConsBtSlotsToBlockDuringDhcp;
   v_U8_t       btcA2DPBtSubIntervalsDuringDhcp;
   v_U8_t       btcActionOnPmFail;
   v_U8_t       btcBtIntervalMode1;
   v_U8_t       btcWlanIntervalMode1;

   v_U32_t      btcStaticLenInqBt;
   v_U32_t      btcStaticLenPageBt;
   v_U32_t      btcStaticLenConnBt;
   v_U32_t      btcStaticLenLeBt;
   v_U32_t      btcStaticLenInqWlan;
   v_U32_t      btcStaticLenPageWlan;
   v_U32_t      btcStaticLenConnWlan;
   v_U32_t      btcStaticLenLeWlan;
   v_U32_t      btcDynMaxLenBt;
   v_U32_t      btcDynMaxLenWlan;
   v_U32_t      btcMaxScoBlockPerc;
   v_U32_t      btcDhcpProtOnA2dp;
   v_U32_t      btcDhcpProtOnSco;

   v_U32_t      mwsCoexVictimWANFreq[MWS_COEX_MAX_VICTIM_TABLE];
   v_U32_t      mwsCoexVictimWLANFreq[MWS_COEX_MAX_VICTIM_TABLE];
   v_U32_t      mwsCoexVictimConfig[MWS_COEX_MAX_VICTIM_TABLE];
   v_U32_t      mwsCoexVictimConfig2[MWS_COEX_MAX_VICTIM_TABLE];
   v_U32_t      mwsCoexModemBackoff;
   v_U32_t      mwsCoexConfig[MWS_COEX_MAX_CONFIG];
   v_U32_t      SARPowerBackoff;
} tSmeBtcConfig, *tpSmeBtcConfig;


typedef struct sSmeBtAclModeChangeEventHist
{
    tSmeBtAclModeChangeParam  btAclModeChange;
    v_BOOL_t fValid;
} tSmeBtAclModeChangeEventHist, *tpSmeBtAclModeChangeEventHist;

typedef struct sSmeBtAclEventHist
{
    //At most, cached events are COMPLETION, DISCONNECT, CREATION, COMPLETION
    tSmeBtEventType btEventType[BT_MAX_NUM_EVENT_ACL_DEFERRED];
    tSmeBtAclConnectionParam  btAclConnection[BT_MAX_NUM_EVENT_ACL_DEFERRED];
    //bNextEventIdx == 0 meaning no event cached here
    tANI_U8 bNextEventIdx;
} tSmeBtAclEventHist, *tpSmeBtAclEventHist;

typedef struct sSmeBtSyncEventHist
{
    //At most, cached events are COMPLETION, DISCONNECT, CREATION, COMPLETION
    tSmeBtEventType btEventType[BT_MAX_NUM_EVENT_SCO_DEFERRED];
    tSmeBtSyncConnectionParam  btSyncConnection[BT_MAX_NUM_EVENT_SCO_DEFERRED];
    //bNextEventIdx == 0 meaning no event cached here
    tANI_U8 bNextEventIdx;
} tSmeBtSyncEventHist, *tpSmeBtSyncEventHist;

typedef struct sSmeBtDisconnectEventHist
{
    tSmeBtDisconnectParam btDisconnect;
    v_BOOL_t fValid;
} tSmeBtDisconnectEventHist, *tpSmeBtDisconnectEventHist;


/*
  Data structure for the history of BT events
*/
typedef struct sSmeBtcEventHist
{
   tSmeBtSyncEventHist btSyncConnectionEvent[BT_MAX_SCO_SUPPORT];
   tSmeBtAclEventHist btAclConnectionEvent[BT_MAX_ACL_SUPPORT];
   tSmeBtAclModeChangeEventHist btAclModeChangeEvent[BT_MAX_ACL_SUPPORT];
   tSmeBtDisconnectEventHist btDisconnectEvent[BT_MAX_DISCONN_SUPPORT];
   tSmeBtSyncUpdateHist btSyncUpdateEvent[BT_MAX_SCO_SUPPORT];
   int nInquiryEvent;    //>0 for # of outstanding inquiriy starts
                         //<0 for # of outstanding inquiry stops
                         //0 == no inquiry event
   int nPageEvent;  //>0 for # of outstanding page starts
                    //<0 for # of outstanding page stops
                    //0 == no page event
   v_BOOL_t fA2DPStarted;
   v_BOOL_t fA2DPStopped;
} tSmeBtcEventHist, *tpSmeBtcEventHist;

typedef struct sSmeBtcEventReplay
{
   tSmeBtcEventHist btcEventHist;
   v_BOOL_t fBTSwitchOn;
   v_BOOL_t fBTSwitchOff;
   //This is not directly tied to BT event so leave it alone when processing BT events
   v_BOOL_t fRestoreHBMonitor;
} tSmeBtcEventReplay, *tpSmeBtcEventReplay;

typedef struct sSmeBtcInfo
{
   tSmeBtcConfig btcConfig;
   v_BOOL_t      btcReady;
   v_U8_t        btcEventState;
   v_U8_t        btcHBActive;    /* Is HB currently active */
   v_U8_t        btcHBCount;     /* default HB count */
   vos_timer_t   restoreHBTimer; /* Timer to restore heart beat */
   tSmeBtcEventReplay btcEventReplay;
   v_BOOL_t      fReplayBTEvents;
   v_BOOL_t      btcUapsdOk;  /* Indicate whether BTC is ok with UAPSD */
   v_BOOL_t      fA2DPTrafStop;/*flag to check A2DP_STOP event has come before MODE_CHANGED*/
   v_U16_t       btcScoHandles[BT_MAX_SCO_SUPPORT];  /* Handles for SCO, if any*/
   v_BOOL_t      fA2DPUp;        /*remember whether A2DP is in session*/
   v_BOOL_t      btcScanCompromise;
   v_U8_t        btcBssfordisableaggr[VOS_MAC_ADDRESS_LEN];
   vos_timer_t   enableUapsdTimer;
} tSmeBtcInfo, *tpSmeBtcInfo;


/** Routine definitions
*/

#ifndef WLAN_MDM_CODE_REDUCTION_OPT
VOS_STATUS btcOpen (tHalHandle hHal);
VOS_STATUS btcClose (tHalHandle hHal);
VOS_STATUS btcReady (tHalHandle hHal);
VOS_STATUS btcSendCfgMsg(tHalHandle hHal, tpSmeBtcConfig pSmeBtcConfig);
VOS_STATUS btcSignalBTEvent (tHalHandle hHal, tpSmeBtEvent pBtEvent);
VOS_STATUS btcSetConfig (tHalHandle hHal, tpSmeBtcConfig pSmeBtcConfig);
VOS_STATUS btcGetConfig (tHalHandle hHal, tpSmeBtcConfig pSmeBtcConfig);
/*
   Caller can check whether BTC's current event allows UAPSD. This doesn't affect
   BMPS.
   return:  VOS_TRUE -- BTC is ready for UAPSD
            VOS_FALSE -- certain BT event is active, cannot enter UAPSD
*/
v_BOOL_t btcIsReadyForUapsd( tHalHandle hHal );
eHalStatus btcHandleCoexInd(tHalHandle hHal, void* pMsg);
#endif /* End of WLAN_MDM_CODE_REDUCTION_OPT */

#endif
