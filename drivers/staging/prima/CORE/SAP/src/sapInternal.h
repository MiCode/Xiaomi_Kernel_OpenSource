/*
 * Copyright (c) 2012-2013, 2015 The Linux Foundation. All rights reserved.
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

#ifndef WLAN_QCT_WLANSAP_INTERNAL_H
#define WLAN_QCT_WLANSAP_INTERNAL_H

/*===========================================================================

               W L A N   S A P  P A L   L A Y E R 
                       I N T E R N A L  A P I


DESCRIPTION
  This file contains the internal API exposed by the wlan SAP PAL layer 
  module.


===========================================================================*/


/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header: /cygdrive/d/Builds/M7201JSDCAAPAD52240B/WM/platform/msm7200/Src/Drivers/SD/ClientDrivers/WLAN/QCT_BTAMP_RSN/CORE/BAP/src/bapInternal.h,v 1.7 2009/03/09 08:50:43 rgidvani Exp rgidvani $ $DateTime: $ $Author: jzmuda $


when           who        what, where, why
--------     ---         ----------------------------------------------------------
09/15/08    SOFTAP     Created module.

===========================================================================*/



/*===========================================================================

                          INCLUDE FILES FOR MODULE

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "vos_api.h" 
#include "vos_packet.h" 

// Pick up the CSR API definitions
#include "csrApi.h"
#include "sapApi.h"
#include "sapFsm_ext.h"
#include "sapChSelect.h"
#include "wlan_hdd_dp_utils.h"
#include "wlan_hdd_main.h"

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
 #ifdef __cplusplus
 extern "C" {
 #endif 
 

/*----------------------------------------------------------------------------
 *  Defines
 * -------------------------------------------------------------------------*/
#define SAP_DEBUG
// Used to enable or disable security on the BT-AMP link 
#define WLANSAP_SECURITY_ENABLED_STATE VOS_TRUE
// How do I get SAP context from voss context? 
#define VOS_GET_SAP_CB(ctx) vos_get_context( VOS_MODULE_ID_SAP, ctx) 

#define VOS_GET_HAL_CB(ctx) vos_get_context( VOS_MODULE_ID_PE, ctx) 
//MAC Address length
#define ANI_EAPOL_KEY_RSN_NONCE_SIZE      32

extern sRegulatoryChannel *regChannels;
extern const tRfChannelProps rfChannels[NUM_RF_CHANNELS];

/*----------------------------------------------------------------------------
 *  Typedefs
 * -------------------------------------------------------------------------*/
typedef struct sSapContext tSapContext;
// tSapContext, *ptSapContext;
/*----------------------------------------------------------------------------
 *  Type Declarations - For internal SAP context information
 * -------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------
 *  Opaque SAP context Type Declaration
 * -------------------------------------------------------------------------*/
// We were only using this syntax, when this was truly opaque. 
// (I.E., it was defined in a different file.)


/* SAP FSM states for Access Point role */
typedef enum {
    eSAP_DISCONNECTED,
    eSAP_CH_SELECT,
    eSAP_STARTING,
    eSAP_STARTED,
    eSAP_DISCONNECTING
} eSapFsmStates_t;

/*----------------------------------------------------------------------------
 *  SAP context Data Type Declaration
 * -------------------------------------------------------------------------*/
 /*----------------------------------------------------------------------------
 *  Type Declarations - QOS related
 * -------------------------------------------------------------------------*/
/* SAP QOS config */
typedef struct sSapQosCfg {
    v_U8_t              WmmIsEnabled;
} tSapQosCfg;

typedef struct sSapAcsChannelInfo {
    v_U32_t             channelNum;
    v_U32_t             weight;
}tSapAcsChannelInfo;

typedef struct {
    /** The station entry is used or not  */
    v_BOOL_t isUsed;

    /** Station ID reported back from HAL (through SAP). Broadcast
     *  uses station ID zero by default in both libra and volans. */
    v_U8_t ucSTAId;

    /** MAC address of the station */
    v_MACADDR_t macAddrSTA;

    /** Current Station state so HDD knows how to deal with packet
     *  queue. Most recent states used to change TL STA state. */
    WLANTL_STAStateType tlSTAState;

   /** Transmit queues for each AC (VO,VI,BE etc). */
   hdd_list_t wmm_tx_queue[NUM_TX_QUEUES];

   /** Might need to differentiate queue depth in contention case */
   v_U16_t aTxQueueDepth[NUM_TX_QUEUES];

   /**Track whether OS TX queue has been disabled.*/
   v_BOOL_t txSuspended[NUM_TX_QUEUES];

   /**Track whether 3/4th of resources are used */
   v_BOOL_t vosLowResource;

   /** Track QoS status of station */
   v_BOOL_t isQosEnabled;

   /** The station entry for which Deauth is in progress  */
   v_BOOL_t isDeauthInProgress;

#ifdef WLAN_FEATURE_AP_HT40_24G
   /** Track HT40 Intolerant station */
   v_BOOL_t isHT40IntolerantSet;
#endif
} hdd_station_info_t;

typedef struct sSapContext {

    vos_lock_t          SapGlobalLock;

    // Include the current channel of AP
    v_U32_t             channel;
 
    // Include the SME(CSR) sessionId here
    v_U8_t              sessionId;

    // Include the key material for this physical link
    v_U8_t              key_type;
    v_U8_t              key_length;
    v_U8_t              key_material[32];

    // Include the associations MAC addresses
    v_U8_t              self_mac_addr[VOS_MAC_ADDRESS_LEN];

    // Own SSID  
    v_U8_t              ownSsid[MAX_SSID_LEN];
    v_U32_t             ownSsidLen;

    // Flag for signaling if security is enabled
    v_U8_t              ucSecEnabled;

    // Include the SME(CSR) context here
    tCsrRoamProfile     csrRoamProfile; 
    v_U32_t             csrRoamId;

    //Sap session
    tANI_BOOLEAN        isSapSessionOpen;

    // SAP event Callback to hdd
    tpWLAN_SAPEventCB   pfnSapEventCallback;

    // Include the enclosing VOSS context here
    v_PVOID_t           pvosGCtx; 

    // Include the state machine structure here, state var that keeps track of state machine
    eSapFsmStates_t     sapsMachine;

    // Actual storage for AP and self (STA) SSID 
    tCsrSSIDInfo        SSIDList[2];

    // Actual storage for AP bssid 
    tCsrBssid           bssid;

    // Mac filtering settings
    eSapMacAddrACL      eSapMacAddrAclMode;
    v_MACADDR_t         acceptMacList[MAX_ACL_MAC_ADDRESS];
    v_U8_t              nAcceptMac;
    v_MACADDR_t         denyMacList[MAX_ACL_MAC_ADDRESS];
    v_U8_t              nDenyMac;

    // QOS config
    tSapQosCfg        SapQosCfg;

    v_PVOID_t         pUsrContext;

    v_U32_t           nStaWPARSnReqIeLength;
    v_U8_t            pStaWpaRsnReqIE[MAX_ASSOC_IND_IE_LEN]; 
    tSirAPWPSIEs      APWPSIEs;
    tSirRSNie         APWPARSNIEs;

    v_U32_t           nStaAddIeLength;
    v_U8_t            pStaAddIE[MAX_ASSOC_IND_IE_LEN]; 
    v_U8_t            *channelList;
    v_U8_t            numofChannel;
    tSapChannelListInfo SapChnlList;

    tANI_BOOLEAN       allBandScanned;
    eCsrBand           currentPreferredBand;
    eCsrBand           scanBandPreference;
    v_U16_t            acsBandSwitchThreshold;
    tSapAcsChannelInfo acsBestChannelInfo;
    spinlock_t staInfo_lock; //To protect access to station Info
    hdd_station_info_t aStaInfo[WLAN_MAX_STA_COUNT];
#ifdef WLAN_FEATURE_AP_HT40_24G
    v_U8_t            affected_start;
    v_U8_t            affected_end;
    v_U8_t            sap_sec_chan;
    v_U8_t            numHT40IntoSta;
    vos_timer_t       sap_HT2040_timer;
    v_U8_t            ObssScanInterval;
    v_U8_t            ObssTransitionDelayFactor;
#endif
} *ptSapContext;


/*----------------------------------------------------------------------------
 *  External declarations for global context 
 * -------------------------------------------------------------------------*/
//  The main per-Physical Link (per WLAN association) context.
extern ptSapContext  gpSapCtx;

/*----------------------------------------------------------------------------
 *  SAP state machine event definition
 * -------------------------------------------------------------------------*/
/* The event structure */ 
typedef struct sWLAN_SAPEvent {
    v_PVOID_t params;   /* A VOID pointer type for all possible inputs */
    v_U32_t   event;    /* State machine input event message */
    v_U32_t   u1;       /* introduced to handle csrRoamCompleteCallback roamStatus */
    v_U32_t   u2;       /* introduced to handle csrRoamCompleteCallback roamResult */
} tWLAN_SAPEvent, *ptWLAN_SAPEvent; 

/*----------------------------------------------------------------------------
 * Function Declarations and Documentation
 * -------------------------------------------------------------------------*/
#ifdef WLAN_FEATURE_AP_HT40_24G
/*==========================================================================

  FUNCTION    sapGet24GOBSSAffectedChannel()

  DESCRIPTION
    Get OBSS Affected Channel no for SAP

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    tHalHandle:  the tHalHandle passed in with the scan request
    ptSapContext: Pointer to SAP context

  RETURN VALUE

  SIDE EFFECTS

============================================================================*/

eHalStatus sapGet24GOBSSAffectedChannel(tHalHandle halHandle,
                                                ptSapContext psapCtx);
#endif

/*==========================================================================

  FUNCTION    WLANSAP_ScanCallback()

  DESCRIPTION 
    Callback for Scan (scan results) Events  

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    tHalHandle:  the tHalHandle passed in with the scan request
    *p2: the second context pass in for the caller, opaque sap Handle here
    scanID:
    status: Status of scan -success, failure or abort 
   
  RETURN VALUE
    The eHalStatus code associated with performing the operation  

    eHAL_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 

============================================================================*/
eHalStatus
WLANSAP_ScanCallback
(
  tHalHandle halHandle, 
  void *pContext,
  v_U32_t scanID, 
  eCsrScanStatus scanStatus
);

/*==========================================================================

  FUNCTION    WLANSAP_RoamCallback()

  DESCRIPTION 
    Callback for Roam (connection status) Events  

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
      pContext:  is the pContext passed in with the roam request
      pCsrRoamInfo: is a pointer to a tCsrRoamInfo, see definition of eRoamCmdStatus and
      eRoamCmdResult: for detail valid members. It may be NULL
      roamId: is to identify the callback related roam request. 0 means unsolicited
      roamStatus: is a flag indicating the status of the callback
      roamResult: is the result
   
  RETURN VALUE
    The eHalStatus code associated with performing the operation  

    eHAL_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
============================================================================*/
eHalStatus
WLANSAP_RoamCallback
(
  void *pContext, 
  tCsrRoamInfo *pCsrRoamInfo,
  v_U32_t roamId, 
  eRoamCmdStatus roamStatus, 
  eCsrRoamResult roamResult
);

/*==========================================================================

  FUNCTION    WLANSAP_CleanCB

  DESCRIPTION 
    Clear out all fields in the SAP context.
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pSapCtx:  pointer to the SAP control block
    freeFlag:   flag indicating whether to free any allocations.

  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to SAP cb is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 

============================================================================*/
VOS_STATUS 
WLANSAP_CleanCB
( 
  ptSapContext  pSapCtx,
  v_U32_t freeFlag /* If 0 do not empty */
);
/*==========================================================================

  FUNCTION    WLANSapFsm

  DESCRIPTION 
    SAP forward state machine to handle the states of the SAP 
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    sapContext:  pointer to the SAP control block
    sapEvent   : SAP event
    status : status of SAP state machine
   
  RETURN VALUE
    Status of the SAP forward machine

    VOS_STATUS_E_FAULT:  pointer to SAP cb is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 

============================================================================*/

VOS_STATUS
SapFsm
(
    ptSapContext sapContext, /* sapContext value */    
    ptWLAN_SAPEvent sapEvent, /* State machine event */
    v_U8_t *status    /* return the SAP status here */
);

/*==========================================================================

  FUNCTION    WLANSAP_pmcFullPwrReqCB

  DESCRIPTION 
    Callback provide to PMC in the pmcRequestFullPower API.     
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    callbackContext:  The user passed in a context to identify 
    status:           The halStatus     
   
  RETURN VALUE
    None

  SIDE EFFECTS 

============================================================================*/
void 
WLANSAP_pmcFullPwrReqCB
( 
  void *callbackContext, 
  eHalStatus status
);

/*==========================================================================

  FUNCTION    sapSelectChannel

  DESCRIPTION 
    Runs a algorithm to select the best channel to operate in for Soft AP in 2.4GHz band

  DEPENDENCIES 

  PARAMETERS 

    IN
       halHandle : Pointer to HAL handle
       pSapCtx : Pointer to SAP context
       pResult : Pointer to tScanResultHandle
   
  RETURN VALUE
    If SUCCESS channel number or zero for FAILURE.

  SIDE EFFECTS 

============================================================================*/

v_U8_t sapSelectChannel(tHalHandle halHandle, ptSapContext pSapCtx, tScanResultHandle pScanResult);

/*==========================================================================

  FUNCTION    sapSignalHDDevent

  DESCRIPTION 
    SAP HDD event callback function

  DEPENDENCIES 

  PARAMETERS 

    IN
       sapContext : Pointer to SAP handle
       pCsrRoamInfo : csrRoamprofile 
       sapHddevent    : SAP HDD callback event
   
  RETURN VALUE
    If SUCCESS or FAILURE.

  SIDE EFFECTS

============================================================================*/

VOS_STATUS
sapSignalHDDevent( ptSapContext sapContext, tCsrRoamInfo * pCsrRoamInfo, eSapHddEvent sapHddevent, void *);

/*==========================================================================

  FUNCTION    sapFsm

  DESCRIPTION 
    SAP Forward state machine

  DEPENDENCIES 
    
  PARAMETERS 

    IN
       sapContext : Pointer to SAP handle
       sapEvent : state machine event

 RETURN VALUE
    If SUCCESS or FAILURE.

  SIDE EFFECTS 

============================================================================*/
VOS_STATUS
sapFsm
(
    ptSapContext sapContext,    /* sapContext value */
    ptWLAN_SAPEvent sapEvent   /* State machine event */
);

/*==========================================================================

  FUNCTION    sapConvertToCsrProfile

  DESCRIPTION 
    sapConvertToCsrProfile

  DEPENDENCIES 

  PARAMETERS 

    IN
       pconfig_params : Pointer to configuration structure
       bssType : SoftAP type
       profile : pointer to a csrProfile that needs to be passed

 RETURN VALUE
    If SUCCESS or FAILURE.

  SIDE EFFECTS

============================================================================*/
eSapStatus
sapconvertToCsrProfile(tsap_Config_t *pconfig_params, eCsrRoamBssType bssType, tCsrRoamProfile *profile);

/*==========================================================================

  FUNCTION    sapFreeRoamProfile

  DESCRIPTION 
    sapConvertToCsrProfile

  DEPENDENCIES 

  PARAMETERS 

    IN
       profile : pointer to a csrProfile that needs to be freed

 RETURN VALUE
    If SUCCESS or FAILURE.

  SIDE EFFECTS

============================================================================*/
void sapFreeRoamProfile(tCsrRoamProfile *profile);


#ifdef WLAN_FEATURE_AP_HT40_24G
/*==========================================================================
  FUNCTION    sapAddHT40IntolerantSta

  DESCRIPTION
    Add HT40 Intolerant STA & Move SAP from HT40 to HT20

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    sapContext   : Sap Context value
    pCsrRoamInfo : Pointer to CSR info

  RETURN VALUE

  SIDE EFFECTS
============================================================================*/

void sapAddHT40IntolerantSta(ptSapContext sapContext, tCsrRoamInfo *pCsrRoamInfo);

/*==========================================================================
  FUNCTION    sapRemoveHT40IntolerantSta

  DESCRIPTION
    Remove HT40 Intolerant STA & Move SAP from HT40 to HT20

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    sapContext   : Sap Context value
    pCsrRoamInfo : Pointer to CSR info

  RETURN VALUE

  SIDE EFFECTS
============================================================================*/

void sapRemoveHT40IntolerantSta(ptSapContext sapContext, tCsrRoamInfo *pCsrRoamInfo);
#endif

/*==========================================================================

  FUNCTION    sapIsPeerMacAllowed

  DESCRIPTION 
    Function to implement MAC filtering for station association in SoftAP

  DEPENDENCIES 

  PARAMETERS 

    IN
       sapContext : Pointer to SAP handle
       peerMac    : Mac address of the peer

 RETURN VALUE
    If SUCCESS or FAILURE.

  SIDE EFFECTS 

============================================================================*/
VOS_STATUS
sapIsPeerMacAllowed(ptSapContext sapContext, v_U8_t *peerMac);

/*==========================================================================

  FUNCTION    sapSortMacList

  DESCRIPTION 
    Function to implement sorting of MAC addresses

  DEPENDENCIES 

  PARAMETERS 

    IN
       macList : Pointer to mac address array
       size    : Number of entries in mac address array

 RETURN VALUE
    None

  SIDE EFFECTS 

============================================================================*/
void
sapSortMacList(v_MACADDR_t *macList, v_U8_t size);

/*==========================================================================

  FUNCTION    sapAddMacToACL

  DESCRIPTION
    Function to ADD a mac address in an ACL.
    The function ensures that the ACL list remains sorted after the addition.
    This API does not take care of buffer overflow i.e. if the list is already
    maxed out while adding a mac address, it will still try to add.
    The caller must take care that the ACL size is less than MAX_ACL_MAC_ADDRESS
    before calling this function.

  DEPENDENCIES

  PARAMETERS

    IN
       macList          : ACL list of mac addresses (black/white list)
       size (I/O)       : size of the ACL. It is an I/O arg. The API takes care
                          of incrementing the size by 1.
       peerMac          : Mac address of the peer to be added

 RETURN VALUE
    None.

  SIDE EFFECTS

============================================================================*/
void
sapAddMacToACL(v_MACADDR_t *macList, v_U8_t *size, v_U8_t *peerMac);

/*==========================================================================

  FUNCTION    sapRemoveMacFromACL

  DESCRIPTION 
    Function to REMOVE a mac address from an ACL.
    The function ensures that the ACL list remains sorted after the DELETION.

  DEPENDENCIES 

  PARAMETERS 

    IN
       macList          : ACL list of mac addresses (black/white list)
       size (I/O)       : size of the ACL. It is an I/O arg. The API takes care of decrementing the size by 1.
       index            : index in the ACL list where the peerMac is present
                          This index can be found by using the "sapSearchMacList" API which returns the index of the MAC
                          addr, if found in an ACL, in one of the arguments passed by the caller.

 RETURN VALUE
    None.

  SIDE EFFECTS 

============================================================================*/
void
sapRemoveMacFromACL(v_MACADDR_t *macList, v_U8_t *size, v_U8_t index);

/*==========================================================================

  FUNCTION    sapPrintACL

  DESCRIPTION 
    Function to print all the mac address of an ACL.
    Useful for debug.
    
  DEPENDENCIES 

  PARAMETERS 

    IN
       macList          : ACL list of mac addresses (black/white list)
       size             : size of the ACL

 RETURN VALUE
    None.

  SIDE EFFECTS 

============================================================================*/
void 
sapPrintACL(v_MACADDR_t *macList, v_U8_t size);

/*==========================================================================

  FUNCTION    sapSearchMacList

  DESCRIPTION 
    Function to search for a mac address in an ACL

  DEPENDENCIES 

  PARAMETERS 

    IN
       macList          : list of mac addresses (black/white list)
       num_mac          : size of the ACL
       peerMac          : Mac address of the peer
    OP       
       index            : the index at which the peer mac is found
                              this value gets filled in this function. If the caller is not interested
                              in the index of the peerMac to be searched, it can pass NULL here.

 RETURN VALUE
    SUCCESS          : if the mac addr being searched for is found
    FAILURE          : if the mac addr being searched for is NOT found

  SIDE EFFECTS 

============================================================================*/
eSapBool
sapSearchMacList(v_MACADDR_t *macList, v_U8_t num_mac, v_U8_t *peerMac, v_U8_t *index);


/*==========================================================================

  FUNCTION    sap_AcquireGlobalLock

  DESCRIPTION 
    Function to implement acquire SAP global lock

  DEPENDENCIES 

  PARAMETERS 

    IN
       sapContext : Pointer to SAP handle
       peerMac    : Mac address of the peer

 RETURN VALUE
    If SUCCESS or FAILURE.

  SIDE EFFECTS 

============================================================================*/
VOS_STATUS
sap_AcquireGlobalLock( ptSapContext  pSapCtx );

/*==========================================================================

  FUNCTION    sapIsPeerMacAllowed

  DESCRIPTION 
    Function to implement release SAP global lock

  DEPENDENCIES 

  PARAMETERS 

    IN
       sapContext : Pointer to SAP handle
       peerMac    : Mac address of the peer

 RETURN VALUE
    If SUCCESS or FAILURE.

  SIDE EFFECTS 

============================================================================*/
VOS_STATUS
sap_ReleaseGlobalLock( ptSapContext  pSapCtx );

/*==========================================================================
FUNCTION  sapConvertSapPhyModeToCsrPhyMode

DESCRIPTION Function to implement selection of CSR PhyMode using SAP PhyMode

DEPENDENCIES PARAMETERS

IN sapPhyMode : SAP Phy Module

RETURN VALUE If SUCCESS or FAILURE

SIDE EFFECTS
============================================================================*/
eCsrPhyMode sapConvertSapPhyModeToCsrPhyMode( eSapPhyMode sapPhyMode );

#ifdef WLAN_FEATURE_AP_HT40_24G
/*==========================================================================
FUNCTION  sap_ht2040_timer_cb

DESCRIPTION Function to implement ht2040 timer callback implementation

SIDE EFFECTS
============================================================================*/
void sap_ht2040_timer_cb(v_PVOID_t usrDataForCallback);

/*==========================================================================
FUNCTION  sapCheckHT40SecondaryIsNotAllowed

DESCRIPTION Function to check HT40 secondary channel is allowed or not

SIDE EFFECTS
============================================================================*/

eHalStatus sapCheckHT40SecondaryIsNotAllowed(ptSapContext psapCtx);
#endif

#ifdef __cplusplus
}
#endif 
#endif /* #ifndef WLAN_QCT_WLANSAP_INTERNAL_H */

