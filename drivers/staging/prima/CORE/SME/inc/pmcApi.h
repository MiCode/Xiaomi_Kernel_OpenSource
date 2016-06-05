/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
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

* Name:  pmcApi.h

*

* Description: Power Management Control (PMC) API definitions.

* All Rights Reserved.


*

******************************************************************************/


#ifndef __PMC_API_H__

#define __PMC_API_H__

//This timer value determines the default periodicity at which BMPS retries will happen
//This default value is overwritten typicaly by OS specific registry/INI values. 
#define BMPS_TRAFFIC_TIMER_DEFAULT 5000  //unit = ms
#define DHCP_REMAIN_POWER_ACTIVE_THRESHOLD 12 // (12 * 5) sec = 60 seconds = 1 min

//This timer value is used when starting the timer right after association. This value
//should be large enough to allow the auth, DHCP handshake to complete
#define BMPS_TRAFFIC_TIMER_ALLOW_SECURITY_DHCP 8000  //unit = ms

//This timer value is used to start the timer right after key completion
//during roaming. This should be small enough to allow STA to enter PS
//immediately after key completion as no DHCP phase during roaming.
#define TRAFFIC_TIMER_ROAMING 100  //unit = ms

#define PMC_IS_CHIP_ACCESSIBLE(pmcState) ( (IMPS != (pmcState)) && (REQUEST_IMPS != (pmcState)) && \
       (STANDBY != (pmcState)) && (REQUEST_STANDBY != (pmcState)) )

/* Power events that are signaled to PMC. */

typedef enum ePmcPowerEvent

{

    ePMC_SYSTEM_HIBERNATE,  /* host is entering hibernation */

    ePMC_SYSTEM_RESUME,  /* host is resuming after hibernation */

    ePMC_HW_WLAN_SWITCH_OFF,  /* Hardware WLAN Switch has been turned off */

    ePMC_HW_WLAN_SWITCH_ON,  /* Hardware WLAN Switch has been turned on */

    ePMC_SW_WLAN_SWITCH_OFF,  /* Software WLAN Switch has been turned off */

    ePMC_SW_WLAN_SWITCH_ON,  /* Software WLAN Switch has been turned on */

    ePMC_BATTERY_OPERATION,  /* host is now operating on battery power */

    ePMC_AC_OPERATION  /* host is now operating on AC power */

} tPmcPowerEvent;




/* Power saving modes. */

typedef enum ePmcPowerSavingMode

{

    ePMC_IDLE_MODE_POWER_SAVE,  /* Idle Mode Power Save (IMPS) */

    ePMC_BEACON_MODE_POWER_SAVE,  /* Beacon Mode Power Save (BMPS) */

    ePMC_SPATIAL_MULTIPLEX_POWER_SAVE,  /* Spatial Multiplexing Power Save (SMPS) */

    ePMC_UAPSD_MODE_POWER_SAVE,  /* Unscheduled Automatic Power Save Delivery Mode */

    ePMC_STANDBY_MODE_POWER_SAVE,  /* Standby Power Save Mode */

    ePMC_WOWL_MODE_POWER_SAVE  /* Wake-on-Wireless LAN Power Save Mode */

} tPmcPowerSavingMode;




/* Switch states. */

typedef enum ePmcSwitchState

{

    ePMC_SWITCH_OFF,  /* switch off */

    ePMC_SWITCH_ON  /* switch on */

} tPmcSwitchState;




/* Device power states. */

typedef enum ePmcPowerState

{

    ePMC_FULL_POWER,  /* full power */

    ePMC_LOW_POWER,  /* low power */

} tPmcPowerState;

 

/* PMC states. */

typedef enum ePmcState

{

    STOPPED, /* PMC is stopped */

    FULL_POWER, /* full power */

    LOW_POWER, /* low power */

    REQUEST_IMPS,  /* requesting IMPS */

    IMPS,  /* in IMPS */

    REQUEST_BMPS,  /* requesting BMPS */

    BMPS,  /* in BMPS */

    REQUEST_FULL_POWER,  /* requesting full power */

    REQUEST_START_UAPSD,  /* requesting Start UAPSD */

    REQUEST_STOP_UAPSD,  /* requesting Stop UAPSD */

    UAPSD,           /* in UAPSD */

    REQUEST_STANDBY,  /* requesting standby mode */

    STANDBY,  /* in standby mode */

    REQUEST_ENTER_WOWL, /* requesting enter WOWL */

    REQUEST_EXIT_WOWL,  /* requesting exit WOWL */

    WOWL                /* Chip in WOWL mode */

} tPmcState;

/* Which beacons should be forwarded to the host. */

typedef enum ePmcBeaconsToForward

{

    ePMC_NO_BEACONS,  /* none */

    ePMC_BEACONS_WITH_TIM_SET,  /* with TIM set */

    ePMC_BEACONS_WITH_DTIM_SET,  /* with DTIM set */

    ePMC_NTH_BEACON,  /* every Nth beacon */

    ePMC_ALL_BEACONS  /* all beacons */

} tPmcBeaconsToForward;




/* The Spatial Mulitplexing Power Save modes. */

typedef enum ePmcSmpsMode

{

    ePMC_DYNAMIC_SMPS,  /* dynamic SMPS */

    ePMC_STATIC_SMPS  /* static SMPS */

} tPmcSmpsMode;

typedef enum
{
    eWOWL_EXIT_USER,
    eWOWL_EXIT_WAKEIND
}tWowlExitSource;

/* Configuration parameters for Idle Mode Power Save (IMPS). */

typedef struct sPmcImpsConfigParams

{

    tANI_BOOLEAN enterOnAc;  /* FALSE if device should enter IMPS only when host operating

                                on battery power, TRUE if device should enter always */

} tPmcImpsConfigParams, *tpPmcImpsConfigParams;




/* Configuration parameters for Beacon Mode Power Save (BMPS). */

typedef struct sPmcBmpsConfigParams

{

    tANI_BOOLEAN enterOnAc;  /* FALSE if device should enter BMPS only when host operating on

                                battery power, TRUE if device should enter always */

    tANI_U32 txThreshold;  /* transmit rate under which BMPS should be entered (frames / traffic measurement period) */

    tANI_U32 rxThreshold;  /* receive rate under which BMPS should be entered (frames / traffic measurement period) */

    tANI_U32 trafficMeasurePeriod; /* period for BMPS traffic measurement (milliseconds) */

    tANI_U32 bmpsPeriod;  /* amount of time in low power (beacon intervals) */

    tPmcBeaconsToForward forwardBeacons;  /* which beacons should be forwarded to the host */

    tANI_U32 valueOfN;  /* the value of N when forwardBeacons is set to ePMC_NTH_BEACON */

    tANI_BOOLEAN usePsPoll;  /* TRUE if PS-POLL should be used to retrieve frames from AP, FALSE if a

                                null data frame with the PM bit reset should be used */

    tANI_BOOLEAN setPmOnLastFrame; /* TRUE to keep device in BMPS as much as possible, FALSE otherwise, TRUE means:

                                      1) PM bit should be set on last pending transmit data frame

                                      2) null frame with PM bit set should be transmitted after last pending receive

                                         frame has been processed */

    tANI_BOOLEAN enableBeaconEarlyTermination; /* if TRUE, BET feature in RIVA 
                                      will be enabled, FALSE otherwise, TRUE means:
                                      RXP will read the beacon header for the 
                                      TIM bit & discard the rest if set to 0, 
                                      while in BMPS              */
    tANI_U8      bcnEarlyTermWakeInterval; /* This specifies how often in terms 
                                      of LI we will disable BET in order to sync 
                                      up TSF*/

} tPmcBmpsConfigParams, *tpPmcBmpsConfigParams;




/* Configuration parameters for Spatial Mulitplexing Power Save (SMPS). */

typedef struct sPmcSmpsConfigParams

{

    tPmcSmpsMode mode;  /* mode to use */

    tANI_BOOLEAN enterOnAc;  /* FALSE if device should enter SMPS only when host operating on

                                battery power, TRUE if device should enter always */

} tPmcSmpsConfigParams, *tpPmcSmpsConfigParams;


/* Routine definitions. */
extern eHalStatus pmcOpen (tHalHandle hHal);

extern eHalStatus pmcStart (tHalHandle hHal);

extern eHalStatus pmcStop (tHalHandle hHal);

extern eHalStatus pmcClose (tHalHandle hHal );

extern eHalStatus pmcSignalPowerEvent (tHalHandle hHal, tPmcPowerEvent event);

extern eHalStatus pmcSetConfigPowerSave (tHalHandle hHal, tPmcPowerSavingMode psMode, void *pConfigParams);

extern eHalStatus pmcGetConfigPowerSave (tHalHandle hHal, tPmcPowerSavingMode psMode, void *pConfigParams);

extern eHalStatus pmcEnablePowerSave (tHalHandle hHal, tPmcPowerSavingMode psMode);

extern eHalStatus pmcStartAutoBmpsTimer (tHalHandle hHal);

extern eHalStatus pmcStopAutoBmpsTimer (tHalHandle hHal);

extern eHalStatus pmcDisablePowerSave (tHalHandle hHal, tPmcPowerSavingMode psMode);

extern eHalStatus pmcQueryPowerState (tHalHandle hHal, tPmcPowerState *pPowerState, tPmcSwitchState *pHwWlanSwitchState,

                                      tPmcSwitchState *pSwWlanSwitchState);

extern tANI_BOOLEAN pmcIsPowerSaveEnabled (tHalHandle hHal, tPmcPowerSavingMode psMode);

extern eHalStatus pmcRequestFullPower (tHalHandle hHal, void (*callbackRoutine) (void *callbackContext, eHalStatus status),

                                       void *callbackContext, tRequestFullPowerReason fullPowerReason);

extern eHalStatus pmcRequestImps (tHalHandle hHal, tANI_U32 impsPeriod,

                                  void (*callbackRoutine) (void *callbackContext, eHalStatus status),

                                  void *callbackContext);

extern eHalStatus pmcRegisterPowerSaveCheck (tHalHandle hHal, tANI_BOOLEAN (*checkRoutine) (void *checkContext),

                                             void *checkContext);

extern eHalStatus pmcDeregisterPowerSaveCheck (tHalHandle hHal, tANI_BOOLEAN (*checkRoutine) (void *checkContext));

extern void pmcMessageProcessor (tHalHandle hHal, tSirSmeRsp *pMsg);
extern void pmcResetImpsFailStatus (tHalHandle hHal);
extern v_BOOL_t IsPmcImpsReqFailed (tHalHandle hHal);

extern eHalStatus pmcRequestBmps (

   tHalHandle hHal,

   void (*callbackRoutine) (void *callbackContext, eHalStatus status),

   void *callbackContext);


extern eHalStatus pmcStartUapsd (

   tHalHandle hHal,

   void (*callbackRoutine) (void *callbackContext, eHalStatus status),

   void *callbackContext);


extern eHalStatus pmcStopUapsd (tHalHandle hHal);


extern eHalStatus pmcRequestStandby (

   tHalHandle hHal,

   void (*callbackRoutine) (void *callbackContext, eHalStatus status),

   void *callbackContext);


extern eHalStatus pmcRegisterDeviceStateUpdateInd (tHalHandle hHal, 

   void (*callbackRoutine) (void *callbackContext, tPmcState pmcState),

   void *callbackContext);


extern eHalStatus pmcDeregisterDeviceStateUpdateInd (tHalHandle hHal, 

   void (*callbackRoutine) (void *callbackContext, tPmcState pmcState));


extern eHalStatus pmcReady(tHalHandle hHal);


void pmcDumpInit(tHalHandle hHal);


extern eHalStatus pmcWowlAddBcastPattern (
   tHalHandle hHal, 
   tpSirWowlAddBcastPtrn pattern, 
   tANI_U8  sessionId);


extern eHalStatus pmcWowlDelBcastPattern (
   tHalHandle hHal, 
   tpSirWowlDelBcastPtrn pattern,
   tANI_U8 sessionId);


extern eHalStatus pmcEnterWowl ( 

    tHalHandle hHal, 

    void (*enterWowlCallbackRoutine) (void *callbackContext, eHalStatus status),

    void *enterWowlCallbackContext,
#ifdef WLAN_WAKEUP_EVENTS
    void (*wakeReasonIndCB) (void *callbackContext, tpSirWakeReasonInd pWakeReasonInd),

    void *wakeReasonIndCBContext,
#endif // WLAN_WAKEUP_EVENTS
    tpSirSmeWowlEnterParams wowlEnterParams, tANI_U8 sessionId);

extern eHalStatus pmcExitWowl (tHalHandle hHal, tWowlExitSource wowlExitSrc);


extern eHalStatus pmcSetHostOffload (tHalHandle hHal, tpSirHostOffloadReq pRequest,
                                          tANI_U8 sessionId);

/* ---------------------------------------------------------------------------
    \fn pmcSetKeepAlive
    \brief  Set the Keep Alive feature.
    \param  hHal - The handle returned by macOpen.
    \param  pRequest - Pointer to the Keep Alive.
    \return eHalStatus
            eHAL_STATUS_FAILURE  Cannot set the keepalive.
            eHAL_STATUS_SUCCESS  Request accepted. 
  ---------------------------------------------------------------------------*/
extern eHalStatus pmcSetKeepAlive (tHalHandle hHal, tpSirKeepAliveReq pRequest, tANI_U8 sessionId);

extern tANI_BOOLEAN pmcValidateConnectState( tHalHandle hHal );

extern tANI_BOOLEAN pmcAllowImps( tHalHandle hHal );


#ifdef FEATURE_WLAN_SCAN_PNO
/*Pref netw found Cb declaration*/
typedef void(*preferredNetworkFoundIndCallback)(void *callbackContext, tpSirPrefNetworkFoundInd pPrefNetworkFoundInd);

extern eHalStatus pmcSetPreferredNetworkList(tHalHandle hHal, tpSirPNOScanReq pRequest, tANI_U8 sessionId, preferredNetworkFoundIndCallback callbackRoutine, void *callbackContext);
extern eHalStatus pmcSetRssiFilter(tHalHandle hHal, v_U8_t rssiThreshold);
#endif // FEATURE_WLAN_SCAN_PNO

#ifdef WLAN_FEATURE_PACKET_FILTERING
// Packet Coalescing Filter Match Count Callback declaration
typedef void(*FilterMatchCountCallback)(void *callbackContext,
                                        tpSirRcvFltPktMatchRsp pRcvFltPktMatchRsp);
extern eHalStatus pmcGetFilterMatchCount(tHalHandle hHal, FilterMatchCountCallback callbackRoutine, 
                                                void *callbackContext, tANI_U8 sessionId);
#endif // WLAN_FEATURE_PACKET_FILTERING

#ifdef WLAN_FEATURE_GTK_OFFLOAD
// GTK Offload Information Callback declaration
typedef void(*GTKOffloadGetInfoCallback)(void *callbackContext, tpSirGtkOffloadGetInfoRspParams pGtkOffloadGetInfoRsp);

/* ---------------------------------------------------------------------------
    \fn pmcSetGTKOffload
    \brief  Set GTK offload feature.
    \param  hHal - The handle returned by macOpen.
    \param  pGtkOffload - Pointer to the GTK offload request.
    \return eHalStatus
            eHAL_STATUS_FAILURE  Cannot set the offload.
            eHAL_STATUS_SUCCESS  Request accepted. 
  ---------------------------------------------------------------------------*/
extern eHalStatus pmcSetGTKOffload (tHalHandle hHal, tpSirGtkOffloadParams pGtkOffload, tANI_U8 sessionId);

/* ---------------------------------------------------------------------------
    \fn pmcGetGTKOffload
    \brief  Get GTK offload information.
    \param  hHal - The handle returned by macOpen.
    \param  callbackRoutine - Pointer to the GTK Offload Get Info response callback routine.
    \return eHalStatus
            eHAL_STATUS_FAILURE  Cannot set the offload.
            eHAL_STATUS_SUCCESS  Request accepted. 
  ---------------------------------------------------------------------------*/
extern eHalStatus pmcGetGTKOffload(tHalHandle hHal,
                                   GTKOffloadGetInfoCallback callbackRoutine,
                                   void *callbackContext, tANI_U8 sessionId);
#endif // WLAN_FEATURE_GTK_OFFLOAD

#ifdef FEATURE_WLAN_BATCH_SCAN
/*Set batch scan request Cb declaration*/
typedef void(*hddSetBatchScanReqCallback)(void *callbackContext,
     tSirSetBatchScanRsp *pRsp);

/*Trigger batch scan result indication Cb declaration*/
typedef void(*hddTriggerBatchScanResultIndCallback)(void *callbackContext,
     void *pRsp);

/* -----------------------------------------------------------------------------
    \fn pmcSetBatchScanReq
    \brief  Setting batch scan request in FW
    \param  hHal - The handle returned by macOpen.
    \param  sessionId - session id
    \param  callbackRoutine - Pointer to set batch scan request callback routine
    \param  calbackContext - callback context
    \return eHalStatus
             eHAL_STATUS_FAILURE  Cannot set batch scan request
             eHAL_STATUS_SUCCESS  Request accepted.
 -----------------------------------------------------------------------------*/
extern eHalStatus pmcSetBatchScanReq(tHalHandle hHal, tSirSetBatchScanReq
       *pRequest, tANI_U8 sessionId, hddSetBatchScanReqCallback callbackRoutine,
       void *callbackContext);

/* -----------------------------------------------------------------------------
    \fn pmcTriggerBatchScanResultInd
    \brief  API to pull batch scan result from FW
    \param  hHal - The handle returned by macOpen.
    \param  sessionId - session id
    \param  callbackRoutine - Pointer to get batch scan request callback routine
    \param  calbackContext - callback context
    \return eHalStatus
             eHAL_STATUS_FAILURE  Cannot set batch scan request
             eHAL_STATUS_SUCCESS  Request accepted.
 -----------------------------------------------------------------------------*/
extern eHalStatus pmcTriggerBatchScanResultInd
(
    tHalHandle hHal, tSirTriggerBatchScanResultInd *pRequest, tANI_U8 sessionId,
    hddTriggerBatchScanResultIndCallback callbackRoutine, void *callbackContext
);


/* -----------------------------------------------------------------------------
    \fn pmcStopBatchScanInd
    \brief  Stoping batch scan request in FW
    \param  hHal - The handle returned by macOpen.
    \param  pInd - Pointer to stop batch scan indication
    \return eHalStatus
             eHAL_STATUS_FAILURE  Cannot set batch scan request
             eHAL_STATUS_SUCCESS  Request accepted.
 -----------------------------------------------------------------------------*/

extern eHalStatus pmcStopBatchScanInd
(
    tHalHandle hHal,
    tSirStopBatchScanInd *pInd,
    tANI_U8 sessionId
);

#endif // FEATURE_WLAN_BATCH_SCAN


#endif

