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
* Name:  pmc.h
*
* Description: Power Management Control (PMC) internal definitions.
*
* Copyright 2008 (c) Qualcomm, Incorporated.  
  All Rights Reserved.
* Qualcomm Confidential and Proprietary.
*
******************************************************************************/

#ifndef __PMC_H__
#define __PMC_H__


#include "palTimer.h"
#include "csrLinkList.h"
#include "pmcApi.h"
#include "smeInternal.h"


//Change PMC_ABORT to no-op for now. We need to define it as VOS_ASSERT(0) once we 
//cleanup the usage.
#define PMC_ABORT  

/* Host power sources. */
typedef enum ePowerSource
{
    AC_POWER,  /* host is operating from AC power */
    BATTERY_POWER  /* host is operating from battery power */
} tPowerSource;


/* Power save check routine list entry. */
typedef struct sPowerSaveCheckEntry
{
    tListElem link;  /* list links */
    tANI_BOOLEAN (*checkRoutine) (void *checkContext);  /* power save check routine */
    void *checkContext;  /* value to be passed as parameter to routine specified above */
} tPowerSaveCheckEntry, *tpPowerSaveCheckEntry;


/* Device Power State update indication list entry. */
typedef struct sDeviceStateUpdateIndEntry
{
    tListElem link;  /* list links */
    void (*callbackRoutine) (void *callbackContext, tPmcState pmcState); /* Callback routine to be invoked when pmc changes device state */
    void *callbackContext;  /* value to be passed as parameter to routine specified above */
} tDeviceStateUpdateIndEntry, *tpDeviceStateUpdateIndEntry;

/* Request full power callback routine list entry. */
typedef struct sRequestFullPowerEntry
{
    tListElem link;  /* list links */
    void (*callbackRoutine) (void *callbackContext, eHalStatus status);  /* routine to call when full power is restored */
    void *callbackContext;  /* value to be passed as parameter to routine specified above */
} tRequestFullPowerEntry, *tpRequestFullPowerEntry;


/* Request BMPS callback routine list entry. */
typedef struct sRequestBmpsEntry
{
   tListElem link;  /* list links */

   /* routine to call when BMPS request succeeded/failed */
   void (*callbackRoutine) (void *callbackContext, eHalStatus status);

   /* value to be passed as parameter to routine specified above */
   void *callbackContext;  

} tRequestBmpsEntry, *tpRequestBmpsEntry;


/* Start U-APSD callback routine list entry. */
typedef struct sStartUapsdEntry
{
   tListElem link;  /* list links */

   /* routine to call when Uapsd Start succeeded/failed*/
   void (*callbackRoutine) (void *callbackContext, eHalStatus status);

   /* value to be passed as parameter to routine specified above */
   void *callbackContext;  

} tStartUapsdEntry, *tpStartUapsdEntry;

typedef struct sPmcDeferredMsg
{
    tListElem link;
    tpAniSirGlobal pMac;
    tANI_U16 messageType;
    tANI_U16 size;  //number of bytes in u.data
    union
    {
        tSirPowerSaveCfg powerSaveConfig;
        tSirWowlAddBcastPtrn wowlAddPattern;
        tSirWowlDelBcastPtrn wowlDelPattern;
        tANI_U8 data[1];    //a place holder
    }u;
} tPmcDeferredMsg;



/* Current PMC information for a particular device. */
typedef struct sPmcInfo
{
    tPowerSource powerSource;  /* host power source */
    tPmcSwitchState hwWlanSwitchState;  /* Hardware WLAN Switch state */
    tPmcSwitchState swWlanSwitchState;  /* Software WLAN Switch state */
    tPmcState pmcState;  /* PMC state */
    tANI_BOOLEAN requestFullPowerPending;  /* TRUE if a request for full power is pending */
    tRequestFullPowerReason requestFullPowerReason; /* reason for requesting full power */
    tPmcImpsConfigParams impsConfig;  /* IMPS configuration */
    tPmcBmpsConfigParams bmpsConfig;  /* BMPS configuration */
    tPmcSmpsConfigParams smpsConfig;  /* SMPS configuration */
    tANI_BOOLEAN impsEnabled;  /* TRUE if IMPS is enabled */
    tANI_BOOLEAN bmpsEnabled;  /* TRUE if BMPS is enabled */
    tANI_BOOLEAN autoBmpsEntryEnabled;  /* TRUE if auto BMPS entry is enabled. If set to TRUE, PMC will
                                           attempt to put the device into BMPS on entry into full Power */
    tANI_BOOLEAN bmpsRequestedByHdd; /*TRUE if BMPS mode has been requested by HDD */
    tANI_BOOLEAN bmpsRequestQueued; /*If a enter BMPS request is queued*/
    tANI_BOOLEAN smpsEnabled;  /* TRUE if SMPS is enabled */
    tANI_BOOLEAN remainInPowerActiveTillDHCP;  /* Remain in Power active till DHCP completes */
    tANI_U32 remainInPowerActiveThreshold;  /*Remain in Power active till DHCP threshold*/
    tANI_U32 impsPeriod;  /* amount of time to remain in IMPS */
    void (*impsCallbackRoutine) (void *callbackContext, eHalStatus status);  /* routine to call when IMPS period
                                                                                has finished */ 
    void *impsCallbackContext;  /* value to be passed as parameter to routine specified above */
    vos_timer_t hImpsTimer;  /* timer to use with IMPS */
    vos_timer_t hTrafficTimer;  /* timer to measure traffic for BMPS */
#ifdef FEATURE_WLAN_DIAG_SUPPORT    
    vos_timer_t hDiagEvtTimer;  /* timer to report PMC state through DIAG event */
#endif
    vos_timer_t hExitPowerSaveTimer;  /* timer for deferred exiting of power save mode */
    tDblLinkList powerSaveCheckList; /* power save check routine list */
    tDblLinkList requestFullPowerList; /* request full power callback routine list */
    tANI_U32 cLastTxUnicastFrames;  /* transmit unicast frame count at last BMPS traffic timer expiration */
    tANI_U32 cLastRxUnicastFrames;  /* receive unicast frame count at last BMPS traffic timer expiration */


    tANI_BOOLEAN uapsdEnabled;  /* TRUE if UAPSD is enabled */
    tANI_BOOLEAN uapsdSessionRequired; /* TRUE if device should go to UAPSD on entering BMPS*/
    tDblLinkList requestBmpsList; /* request Bmps callback routine list */
    tDblLinkList requestStartUapsdList; /* request start Uapsd callback routine list */
    tANI_BOOLEAN standbyEnabled;  /* TRUE if Standby is enabled */
    void (*standbyCallbackRoutine) (void *callbackContext, eHalStatus status); /* routine to call for standby request */ 
    void *standbyCallbackContext;/* value to be passed as parameter to routine specified above */
    tDblLinkList deviceStateUpdateIndList; /*update device state indication list */
    tANI_BOOLEAN pmcReady; /*whether eWNI_SME_SYS_READY_IND has been sent to PE or not */
    tANI_BOOLEAN wowlEnabled;  /* TRUE if WoWL is enabled */
    tANI_BOOLEAN wowlModeRequired; /* TRUE if device should go to WOWL on entering BMPS */
    tWowlExitSource wowlExitSrc; /*WoWl exiting because of wakeup pkt or user explicitly disabling WoWL*/
    void (*enterWowlCallbackRoutine) (void *callbackContext, eHalStatus status); /* routine to call for wowl request */ 
    void *enterWowlCallbackContext;/* value to be passed as parameter to routine specified above */
    tSirSmeWowlEnterParams wowlEnterParams; /* WOWL mode configuration */
    tDblLinkList deferredMsgList;   //The message in here are deferred and DONOT expect response from PE
    tANI_BOOLEAN rfSuppliesVotedOff;  //Whether RF supplies are voted off or not.
#ifdef FEATURE_WLAN_SCAN_PNO
    preferredNetworkFoundIndCallback  prefNetwFoundCB; /* routine to call for Preferred Network Found Indication */ 
    void *preferredNetworkFoundIndCallbackContext;/* value to be passed as parameter to routine specified above */
#endif // FEATURE_WLAN_SCAN_PNO
#ifdef WLAN_FEATURE_PACKET_FILTERING
    FilterMatchCountCallback  FilterMatchCountCB; /* routine to call for Packet Coalescing Filter Match Count */ 
    void *FilterMatchCountCBContext;/* value to be passed as parameter to routine specified above */
#endif // WLAN_FEATURE_PACKET_FILTERING
#ifdef WLAN_FEATURE_GTK_OFFLOAD
    GTKOffloadGetInfoCallback  GtkOffloadGetInfoCB; /* routine to call for GTK Offload Information */ 
    void *GtkOffloadGetInfoCBContext;        /* value to be passed as parameter to routine specified above */
#endif // WLAN_FEATURE_GTK_OFFLOAD

#ifdef WLAN_WAKEUP_EVENTS
    void (*wakeReasonIndCB) (void *callbackContext, tpSirWakeReasonInd pWakeReasonInd);  /* routine to call for Wake Reason Indication */ 
    void *wakeReasonIndCBContext;  /* value to be passed as parameter to routine specified above */
#endif // WLAN_WAKEUP_EVENTS

/* If TRUE driver will go to BMPS only if host operatiing system asks to enter BMPS.
* For android wlan_hdd_cfg80211_set_power_mgmt API will be used to set host powersave*/
    v_BOOL_t    isHostPsEn;
    v_BOOL_t    ImpsReqFailed;
    v_BOOL_t    ImpsReqTimerFailed;
    tANI_U8     ImpsReqFailCnt;
    tANI_U8     ImpsReqTimerfailCnt;

#ifdef FEATURE_WLAN_BATCH_SCAN
   /*HDD callback to be called after receiving SET BATCH SCAN RSP from FW*/
   hddSetBatchScanReqCallback setBatchScanReqCallback;
   void * setBatchScanReqCallbackContext;
   /*HDD callback to be called after receiving BATCH SCAN iRESULT IND from FW*/
   hddTriggerBatchScanResultIndCallback batchScanResultCallback;
   void * batchScanResultCallbackContext;
#endif


} tPmcInfo, *tpPmcInfo;


//MACRO
#define PMC_IS_READY(pMac)  ( ((pMac)->pmc.pmcReady) && (STOPPED != (pMac)->pmc.pmcState) )


/* Routine definitions. */
extern eHalStatus pmcEnterLowPowerState (tHalHandle hHal);
extern eHalStatus pmcExitLowPowerState (tHalHandle hHal);
extern eHalStatus pmcEnterFullPowerState (tHalHandle hHal);
extern eHalStatus pmcEnterRequestFullPowerState (tHalHandle hHal, tRequestFullPowerReason fullPowerReason);
extern eHalStatus pmcEnterRequestImpsState (tHalHandle hHal);
extern eHalStatus pmcEnterImpsState (tHalHandle hHal);
extern eHalStatus pmcEnterRequestBmpsState (tHalHandle hHal);
extern eHalStatus pmcEnterBmpsState (tHalHandle hHal);
extern eHalStatus pmcEnterRequestStartUapsdState (tHalHandle hHal);
extern eHalStatus pmcEnterUapsdState (tHalHandle hHal);
extern eHalStatus pmcEnterRequestStopUapsdState (tHalHandle hHal);
extern eHalStatus pmcEnterRequestStandbyState (tHalHandle hHal);
extern eHalStatus pmcEnterStandbyState (tHalHandle hHal);
extern tANI_BOOLEAN pmcPowerSaveCheck (tHalHandle hHal);
extern eHalStatus pmcSendPowerSaveConfigMessage (tHalHandle hHal);
extern eHalStatus pmcSendMessage (tpAniSirGlobal pMac, tANI_U16 messageType, void *pMessageData, tANI_U32 messageSize);
extern void pmcDoCallbacks (tHalHandle hHal, eHalStatus callbackStatus);
extern void pmcDoBmpsCallbacks (tHalHandle hHal, eHalStatus callbackStatus);
extern void pmcDoStartUapsdCallbacks (tHalHandle hHal, eHalStatus callbackStatus);
extern void pmcDoStandbyCallbacks (tHalHandle hHal, eHalStatus callbackStatus);
extern eHalStatus pmcStartTrafficTimer (tHalHandle hHal, tANI_U32 expirationTime);
extern void pmcStopTrafficTimer (tHalHandle hHal);
extern void pmcImpsTimerExpired (tHalHandle hHal);
extern void pmcTrafficTimerExpired (tHalHandle hHal);

#ifdef FEATURE_WLAN_DIAG_SUPPORT    
extern eHalStatus pmcStartDiagEvtTimer (tHalHandle hHal);
extern void pmcStopDiagEvtTimer (tHalHandle hHal);
extern void pmcDiagEvtTimerExpired (tHalHandle hHal);
#endif

extern void pmcExitPowerSaveTimerExpired (tHalHandle hHal);
extern tPmcState pmcGetPmcState (tHalHandle hHal);
extern const char* pmcGetPmcStateStr(tPmcState state);
extern void pmcDoDeviceStateUpdateCallbacks (tHalHandle hHal, tPmcState state);
extern eHalStatus pmcRequestEnterWowlState(tHalHandle hHal, tpSirSmeWowlEnterParams wowlEnterParams);
extern eHalStatus pmcEnterWowlState (tHalHandle hHal);
extern eHalStatus pmcRequestExitWowlState(tHalHandle hHal);
extern void pmcDoEnterWowlCallbacks (tHalHandle hHal, eHalStatus callbackStatus);
//The function will request for full power as well in addition to defer the message
extern eHalStatus pmcDeferMsg( tpAniSirGlobal pMac, tANI_U16 messageType, 
                                               void *pData, tANI_U32 size);
extern eHalStatus pmcIssueCommand( tpAniSirGlobal pMac, eSmeCommandType cmdType, void *pvParam, 
                                   tANI_U32 size, tANI_BOOLEAN fPutToListHead );
extern eHalStatus pmcEnterImpsCheck( tpAniSirGlobal pMac );
extern eHalStatus pmcEnterBmpsCheck( tpAniSirGlobal pMac );
extern tANI_BOOLEAN pmcShouldBmpsTimerRun( tpAniSirGlobal pMac );
#endif
