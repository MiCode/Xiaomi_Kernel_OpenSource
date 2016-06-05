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

/*===========================================================================

                      b a p A p i L i n k C n t l . C
                                               
  OVERVIEW:
  
  This software unit holds the implementation of the WLAN BAP modules
  Link Control functions.
  
  The functions externalized by this module are to be called ONLY by other 
  WLAN modules (HDD) that properly register with the BAP Layer initially.

  DEPENDENCIES: 

  Are listed for each API below. 
  
  
===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


   $Header: /home/labuser/ampBlueZ_2/CORE/BAP/src/bapApiLinkCntl.c,v 1.1 2010/10/23 23:40:28 labuser Exp labuser $$DateTime$$Author: labuser $


  when        who     what, where, why
----------    ---    --------------------------------------------------------
2008-09-15    jez     Created module

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
//#include "wlan_qct_tl.h"
#include "vos_trace.h"
// Pick up the CSR callback definition
#include "csrApi.h"

/* BT-AMP PAL API header file */ 
#include "bapApi.h" 
#include "bapInternal.h" 
#include "btampFsm.h"

//#define BAP_DEBUG
/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Static Variable Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Static Function Declarations and Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Externalized Function Definitions
* -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Function Declarations and Documentation
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------

  FUNCTION    WLANBAP_RoamCallback()

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
  
----------------------------------------------------------------------------*/
#if 0
eCSR_ROAM_RESULT_WDS_STARTED 
#define eWLAN_BAP_MAC_START_BSS_SUCCESS /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_WDS_STARTED */

eCSR_ROAM_RESULT_FAILURE 
eCSR_ROAM_RESULT_NOT_ASSOCIATED
#define eWLAN_BAP_MAC_START_FAILS /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_FAILURE or eCSR_ROAM_RESULT_NOT_ASSOCIATED */

eCSR_ROAM_RESULT_WDS_ASSOCIATED
#define eWLAN_BAP_MAC_CONNECT_COMPLETED /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_WDS_ASSOCIATED */


eCSR_ROAM_RESULT_FAILURE 
eCSR_ROAM_RESULT_NOT_ASSOCIATED 
#define eWLAN_BAP_MAC_CONNECT_FAILED /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_FAILURE or eCSR_ROAM_RESULT_NOT_ASSOCIATED */


eCSR_ROAM_RESULT_WDS_ASSOCIATION_IND
#define eWLAN_BAP_MAC_CONNECT_INDICATION /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_WDS_ASSOCIATION_IND */


eCSR_ROAM_RESULT_KEY_SET 
#define eWLAN_BAP_MAC_KEY_SET_SUCCESS /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_KEY_SET */


eCSR_ROAM_RESULT_WDS_DISASSOC_IND
#define eWLAN_BAP_MAC_INDICATES_MEDIA_DISCONNECTION /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_WDS_DISASSOC_IND */


eCSR_ROAM_RESULT_WDS_STOPPED
#define eWLAN_BAP_MAC_READY_FOR_CONNECTIONS /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_WDS_STOPPED */

#endif //0


eHalStatus
WLANBAP_RoamCallback
(
  void *pContext, 
  tCsrRoamInfo *pCsrRoamInfo,
  tANI_U32 roamId, 
  eRoamCmdStatus roamStatus, 
  eCsrRoamResult roamResult
)
{
    eHalStatus  halStatus = eHAL_STATUS_SUCCESS; 
    /* btampContext value */    
    ptBtampContext btampContext = (ptBtampContext) pContext; 
    tWLAN_BAPEvent bapEvent; /* State machine event */
    VOS_STATUS  vosStatus = VOS_STATUS_SUCCESS; 
    v_U8_t status;    /* return the BT-AMP status here */
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, before switch on roamStatus = %d", __func__, roamStatus);

    switch (roamStatus) {
        //JEZ081110: For testing purposes, with Infra STA as BT STA, this 
        //actually takes care of the "eCSR_ROAM_RESULT_WDS_STARTED" case, 
        //below, better than "eCSR_ROAM_RESULT_IBSS_STARTED".  
        //case eCSR_ROAM_ROAMING_START: 
        case eCSR_ROAM_ASSOCIATION_START: 
            /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_WDS_STARTED */
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)", __func__, "eCSR_ROAM_ROAMING_START", roamResult);   
            // This only gets called when CSR decides to roam on its own - due to lostlink. 
#if 0
            if ((pCsrRoamInfo) && (pCsrRoamInfo->pConnectedProfile) && (pCsrRoamInfo->pConnectedProfile->pBssDesc))
            {
                memcpy(bssid.ether_addr_octet, pCsrRoamInfo->pConnectedProfile->pBssDesc->bssId,
                       sizeof(tSirMacAddr)); 
                apple80211Interface->willRoam(&bssid);  // Return result isn't significant 
                VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: willRoam returns\n", __func__);
            }
#endif //0
            /* Fill in the event structure */ 
            bapEvent.event = eWLAN_BAP_MAC_START_BSS_SUCCESS; 
            bapEvent.params = pCsrRoamInfo;
            bapEvent.u1 = roamStatus;
            bapEvent.u2 = roamResult;

            /* Handle event */ 
            vosStatus = btampFsm(btampContext, &bapEvent, &status);

            break;

        case eCSR_ROAM_SET_KEY_COMPLETE:
            /* bapRoamCompleteCallback with eCSR_ROAM_SET_KEY_COMPLETE */
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamStatus = %s (%d)", __func__, "eCSR_ROAM_SET_KEY_COMPLETE", roamStatus);   

            /* Fill in the event structure */ 
            bapEvent.event = eWLAN_BAP_MAC_KEY_SET_SUCCESS; 
            bapEvent.params = pCsrRoamInfo;
            bapEvent.u1 = roamStatus;
            bapEvent.u2 = roamResult;

            /* Handle event */ 
            vosStatus = btampFsm(btampContext, &bapEvent, &status);
  
            break;

        case eCSR_ROAM_DISASSOCIATED: 
            /* bapRoamCompleteCallback with eCSR_ROAM_DISASSOCIATED */
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamStatus = %s (%d)", __func__, "eCSR_ROAM_DISASSOCIATED", roamStatus);   
        case eCSR_ROAM_LOSTLINK:
            /* bapRoamCompleteCallback with eCSR_ROAM_LOSTLINK */
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamStatus = %s (%d)", __func__, "eCSR_ROAM_LOSTLINK", roamStatus);   

            if (roamResult != eCSR_ROAM_RESULT_NONE) {
                /* Fill in the event structure */ 
                bapEvent.event = eWLAN_BAP_MAC_READY_FOR_CONNECTIONS; 
                bapEvent.params = pCsrRoamInfo;
                bapEvent.u1 = roamStatus;
                bapEvent.u2 = roamResult; 
                
                /* Handle event */ 
                vosStatus = btampFsm(btampContext, &bapEvent, &status);
            }
  
            break;

        default:
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, unsupported CSR roamStatus = %d", __func__, roamStatus);

            break;
    }

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, before switch on roamResult = %d", __func__, roamResult);

    switch (roamResult) {
        //JEZ081110: Commented out for testing. Test relies upon IBSS. 
        case eCSR_ROAM_RESULT_IBSS_STARTED:  
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)", __func__, "eCSR_ROAM_RESULT_IBSS_STARTED", roamResult);   
        case eCSR_ROAM_RESULT_WDS_STARTED: 
            /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_WDS_STARTED */
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)", __func__, "eCSR_ROAM_RESULT_WDS_STARTED", roamResult);   

            /* Fill in the event structure */ 
            bapEvent.event = eWLAN_BAP_MAC_START_BSS_SUCCESS; 
            bapEvent.params = pCsrRoamInfo;
            bapEvent.u1 = roamStatus;
            bapEvent.u2 = roamResult;

            /* Handle event */ 
            vosStatus = btampFsm(btampContext, &bapEvent, &status);
  
            break;

        //JEZ081110: Commented out for testing. Test relies upon IBSS. 
        //JEZ081110: But I cannot rely upon IBSS for the initial testing. 
        case eCSR_ROAM_RESULT_FAILURE: 
        //case eCSR_ROAM_RESULT_NOT_ASSOCIATED:
        //case eCSR_ROAM_RESULT_IBSS_START_FAILED:
            /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_FAILURE or eCSR_ROAM_RESULT_NOT_ASSOCIATED */
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)", __func__, "eCSR_ROAM_RESULT_FAILURE", roamResult);   
#ifdef FEATURE_WLAN_BTAMP_UT_RF
            break;
#endif
        case eCSR_ROAM_RESULT_WDS_START_FAILED:
            /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_WDS_START_FAILED */
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)", __func__, "eCSR_ROAM_RESULT_WDS_START_FAILED", roamResult);   

            /* Fill in the event structure */ 
            /* I don't think I should signal a eCSR_ROAM_RESULT_FAILURE 
             * as a eWLAN_BAP_MAC_START_FAILS
             */ 
            bapEvent.event = eWLAN_BAP_MAC_START_FAILS; 
            bapEvent.params = pCsrRoamInfo;
            bapEvent.u1 = roamStatus;
            bapEvent.u2 = roamResult;

            /* Handle event */ 
            vosStatus = btampFsm(btampContext, &bapEvent, &status);

            break;

        //JEZ081110: Commented out for testing. This handles both Infra STA and IBSS STA.
        case eCSR_ROAM_RESULT_IBSS_CONNECT:
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)", __func__, "eCSR_ROAM_RESULT_IBSS_CONNECT", roamResult);   
        case eCSR_ROAM_RESULT_ASSOCIATED:
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)", __func__, "eCSR_ROAM_RESULT_ASSOCIATED", roamResult);   
        case eCSR_ROAM_RESULT_WDS_ASSOCIATED:
            /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_WDS_ASSOCIATED */
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)", __func__, "eCSR_ROAM_RESULT_WDS_ASSOCIATED", roamResult);   

            /* Fill in the event structure */ 
            bapEvent.event = eWLAN_BAP_MAC_CONNECT_COMPLETED;
            bapEvent.params = pCsrRoamInfo;
            bapEvent.u1 = roamStatus;
            bapEvent.u2 = roamResult;

            /* Handle event */ 
            vosStatus = btampFsm(btampContext, &bapEvent, &status);
  
            break;

        //JEZ081110: Commented out for testing. Test relies upon IBSS. 
        //JEZ081110: But I cannot rely upon IBSS for the initial testing. 
        //case eCSR_ROAM_RESULT_FAILURE: 
        case eCSR_ROAM_RESULT_IBSS_START_FAILED:
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)", __func__, "eCSR_ROAM_RESULT_IBSS_START_FAILED", roamResult);   
        case eCSR_ROAM_RESULT_NOT_ASSOCIATED:
            /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_FAILURE or eCSR_ROAM_RESULT_NOT_ASSOCIATED */
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)", __func__, "eCSR_ROAM_RESULT_NOT_ASSOCIATED", roamResult);   
#ifdef FEATURE_WLAN_BTAMP_UT_RF
            break;
#endif
        case eCSR_ROAM_RESULT_WDS_NOT_ASSOCIATED:
            /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_WDS_NOT_ASSOCIATED */
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)", __func__, "eCSR_ROAM_RESULT_WDS_NOT_ASSOCIATED", roamResult);   

            /* Fill in the event structure */ 
            bapEvent.event = eWLAN_BAP_MAC_CONNECT_FAILED; 
            bapEvent.params = pCsrRoamInfo;
            bapEvent.u1 = roamStatus;
            bapEvent.u2 = roamResult;

            /* Handle event */ 
            vosStatus = btampFsm(btampContext, &bapEvent, &status);
  
            break;

        //JEZ081110: I think I have to check for the bssType to
        //differentiate between IBSS Start and IBSS Join success.  
        //case eCSR_ROAM_RESULT_IBSS_CONNECT:
            //VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)", __func__, "eCSR_ROAM_RESULT_IBSS_CONNECT", roamResult);   

        //JEZ081110: Commented out for testing. Test relies upon IBSS. 
        // No longer commented out. 
        case eCSR_ROAM_RESULT_WDS_ASSOCIATION_IND:
            /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_WDS_ASSOCIATION_IND */
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)", __func__, "eCSR_ROAM_RESULT_WDS_ASSOCIATION_IND", roamResult);   

            /* Fill in the event structure */ 
            bapEvent.event = eWLAN_BAP_MAC_CONNECT_INDICATION;
            bapEvent.params = pCsrRoamInfo;
            bapEvent.u1 = roamStatus;
            bapEvent.u2 = roamResult;

            /* Handle event */ 
            vosStatus = btampFsm(btampContext, &bapEvent, &status);
  
            /* If BAP doesn't like the incoming association, signal SME/CSR */ 
            if ( status != WLANBAP_STATUS_SUCCESS) 
                halStatus = eHAL_STATUS_FAILURE;
  
            break;

        //JEZ081110: Not supported in SME and CSR, yet. 
#if 0
        case eCSR_ROAM_RESULT_KEY_SET: 
            /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_KEY_SET */
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)", __func__, "eCSR_ROAM_RESULT_KEY_SET", roamResult);   

            /* Fill in the event structure */ 
            bapEvent.event = eWLAN_BAP_MAC_KEY_SET_SUCCESS; 
            bapEvent.params = pCsrRoamInfo;
            bapEvent.u1 = roamStatus;
            bapEvent.u2 = roamResult;

            /* Handle event */ 
            vosStatus = btampFsm(btampContext, &bapEvent, &status);
  
            break;
#endif //0

        case eCSR_ROAM_RESULT_DISASSOC_IND:
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)", __func__, "eCSR_ROAM_RESULT_DISASSOC_IND", roamResult);   
        case eCSR_ROAM_RESULT_WDS_DISASSOCIATED:
            /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_WDS_DISASSOCIATED */
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)", __func__, "eCSR_ROAM_RESULT_WDS_DISASSOCIATED", roamResult);   

            /* Fill in the event structure */ 
            bapEvent.event =  eWLAN_BAP_MAC_INDICATES_MEDIA_DISCONNECTION; 
            bapEvent.params = pCsrRoamInfo;
            bapEvent.u1 = roamStatus;
            bapEvent.u2 = roamResult;

            /* Handle event */ 
            vosStatus = btampFsm(btampContext, &bapEvent, &status);
  
            /* Fill in the event structure */ 
            bapEvent.event =  eWLAN_BAP_MAC_READY_FOR_CONNECTIONS;
            bapEvent.params = pCsrRoamInfo;
            bapEvent.u1 = roamStatus;
            bapEvent.u2 = roamResult;

            /* Handle event */ 
            vosStatus = btampFsm(btampContext, &bapEvent, &status);

            break;

        //JEZ081110: Commented out for testing. Test relies upon IBSS. 
        case eCSR_ROAM_RESULT_IBSS_INACTIVE:
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)", __func__, "eCSR_ROAM_RESULT_IBSS_INACTIVE", roamResult);   
        case eCSR_ROAM_RESULT_WDS_STOPPED:
            /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_WDS_STOPPED */
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)", __func__, "eCSR_ROAM_RESULT_WDS_STOPPED", roamResult);   

            /* Fill in the event structure */ 
            bapEvent.event = eWLAN_BAP_MAC_READY_FOR_CONNECTIONS; 
            bapEvent.params = pCsrRoamInfo;
            bapEvent.u1 = roamStatus;
            bapEvent.u2 = roamResult;

            /* Handle event */ 
            vosStatus = btampFsm(btampContext, &bapEvent, &status);
  
            break;

        default:
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, unsupported CSR roamResult = %d", __func__, roamResult);

            break;
    }

#if 0
    switch (roamResult) {
        case eCSR_ROAM_RESULT_IBSS_CONNECT:
            // we have an IBSS connection...

            // update our state
            btampContext->mAssociatedStatus = WLANBAP_STATUS_SUCCESS;
            btampContext->mAssociated = VOS_TRUE;
            // update "assocBssid" with the BSSID of the IBSS
            if (pCsrRoamInfo) 
                memcpy(btampContext->assocBssid, pCsrRoamInfo->peerMacOrBssidForIBSS, 6);

            // We must update the system role to match that of the
            // lower layers in case the upper layers decided to try
            // joining the network in infrastructure mode if the
            // initial join in IBSS mode fails.  Andreas Wolf
            // (awolf@apple.com) explains the behavior as follows:
            // "If the client attempts to join an open network and it fails 
            // on the first attempt, it reverts back to b-only mode. This
            // workaround was specifically put in place to allow the client
            // to associate to some third party b-only infrastructure APs.
            // It did not take IBSS into account, it seems that the fallback
            // always forces infrastructure." 

            btampContext->systemRole = eSYSTEM_STA_IN_IBSS_ROLE;

            if (mLinkStatus == 0)
            {
                // enable the flow of data
                DBGLOG("%s: marking link as up in %s\n", __func__, "eCSR_ROAM_RESULT_IBSS_CONNECT");
                mLinkStatus = 1;
                ((IO80211Interface*) mNetworkIF)->setLinkState(kIO80211NetworkLinkUp);
                outputQueue->setCapacity(TRANSMIT_QUEUE_SIZE);
                outputQueue->start();
                // Let them know we are ready
                ((IO80211Interface*) mNetworkIF)->postMessage(APPLE80211_M_ASSOC_DONE);
            }
            else
            {
                DBGLOG("%s: link is already up in %s\n", __func__, "eCSR_ROAM_RESULT_IBSS_CONNECT");
            }
            break;

        case eCSR_ROAM_RESULT_IBSS_INACTIVE:
            // we have no more IBSS peers, so disable the flow of data
            if (mLinkStatus != 0)
            {
                DBGLOG("%s: marking link as down in %s\n", __func__, "eCSR_ROAM_RESULT_IBSS_INACTIVE");
                mLinkStatus = (tANI_U8) 0;
                // JEZ070627: Revisit ?
                ((IO80211Interface*) mNetworkIF)->setLinkState(kIO80211NetworkLinkDown);
                outputQueue->stop();
                outputQueue->setCapacity(0);

                // update our state
                btampContext->mAssociated = false;
            }
            else
            {
                DBGLOG("%s: link already down in %s\n", __func__, "eCSR_ROAM_RESULT_IBSS_INACTIVE");
            }

            break;

        case eCSR_ROAM_RESULT_ASSOCIATED:
            btampContext->mAssociatedStatus = APPLE80211_STATUS_SUCCESS;
            btampContext->mAssociated = true;

            if ((pCsrRoamInfo) && (pCsrRoamInfo->pBssDesc)) {
                ccpCsrToAppleScanResult(mPMacObject, pCsrRoamInfo->pBssDesc, &scanResult); 
                
                /* Save away the IEs used by the AP */
                ccpCsrToAssocApiedata( mPMacObject, pCsrRoamInfo->pBssDesc, &(btampContext->apiedata));
                
                if (BssidChanged((tCsrBssid*) btampContext->assocBssid, (ether_addr*) scanResult.asr_bssid)) { 
                    memcpy(btampContext->assocBssid, scanResult.asr_bssid, 6);
                    ((IO80211Interface*) mNetworkIF)->postMessage(APPLE80211_M_BSSID_CHANGED );
                }
            }

            ((IO80211Interface*) mNetworkIF)->postMessage(APPLE80211_M_ASSOC_DONE);

            if (mLinkStatus == 0)
            {
                mLinkStatus = (tANI_U8) 1;
                ((IO80211Interface*) mNetworkIF)->setLinkState(kIO80211NetworkLinkUp);
                DBGLOG("%s: marking link as up in %s\n", __func__, "eCSR_ROAM_RESULT_ASSOCIATED");
                outputQueue->setCapacity(TRANSMIT_QUEUE_SIZE);
                outputQueue->start();
            }
            else
            {
                DBGLOG("%s: link is already up in %s\n", __func__, "eCSR_ROAM_RESULT_ASSOCIATED");
            }
            break;
        case eCSR_ROAM_RESULT_NOT_ASSOCIATED:
            btampContext->mAssociatedStatus = APPLE80211_STATUS_UNAVAILABLE;
            btampContext->mAssociated = false;

            if (mLinkStatus != 0)
            {
                DBGLOG("%s: marking link as down in %s\n", __func__, "eCSR_ROAM_RESULT_NOT_ASSOCIATED");
                mLinkStatus = (tANI_U8) 0;
                ((IO80211Interface*) mNetworkIF)->setLinkState(kIO80211NetworkLinkDown);
            }
            else
            {
                DBGLOG("%s: link already down in %s\n", __func__, "eCSR_ROAM_RESULT_NOT_ASSOCIATED");
            }
            break;
           
        case eCSR_ROAM_RESULT_FAILURE:
            btampContext->mAssociatedStatus = APPLE80211_STATUS_UNSPECIFIED_FAILURE;
            btampContext->mAssociated = false;

            if (mLinkStatus != 0)
            {
                DBGLOG("%s: marking link as down in %s\n", __func__, "eCSR_ROAM_RESULT_FAILURE");
                mLinkStatus = (tANI_U8) 0;
                ((IO80211Interface*) mNetworkIF)->setLinkState(kIO80211NetworkLinkDown);
            }
            else
            {
                DBGLOG("%s: link already down in %s\n", __func__, "eCSR_ROAM_RESULT_FAILURE");
            }
            break;
        
        case eCSR_ROAM_RESULT_DISASSOC_IND:
            {
                btampContext->mAssociated = false;

                if (mLinkStatus != 0)
                {
                    DBGLOG("%s: marking link as down in %s\n", __func__, "eCSR_ROAM_RESULT_DISASSOC_IND");
                    mLinkStatus = (tANI_U8) 0;
                    ((IO80211Interface*) mNetworkIF)->setLinkState(kIO80211NetworkLinkDown);
                }
                else
                {
                    DBGLOG("%s: link already down in %s\n", __func__, "eCSR_ROAM_RESULT_DISASSOC_IND");
                }

                //if (pCsrRoamInfo)  // For now, leave this commented out. Until CSR changes integrated.
                {
                    // Now set the reason and status codes.  
                    // Actually, the "result code" field in the tSirSmeDisassocInd should be named reasonCode and NOT statusCode.  
                    // "Reason Codes" are found in DisAssoc or DeAuth Ind. "Status Code" fields are found in Rsp Mgmt Frame.  
                    // For now, we are going to have to (painfully) map the only "result code" type information we have 
                    // available at ALL from LIM/CSR.  And that is the statusCode field of type tSirResultCodes         
                    // BTW, tSirResultCodes is the COMPLETELY WRONG TYPE for this "result code" field. It SHOULD be 
                    // of type tSirMacReasonCodes.         
                    // Right now, we don't even have that.  So, I have to just make up some "reason code" that I will
                    // pretend I found in the incoming DisAssoc Indication.        
                    //btampContext->statusCode = ((tpSirSmeDisassocInd) pCallbackInfo)->statusCode; // tSirResultCodes         
                    //btampContext->reasonCode = ((tpSirSmeDisassocInd) pCallbackInfo)->statusCode; // tSirResultCodes         
                    btampContext->reasonCode = (tANI_U16) eSIR_MAC_UNSPEC_FAILURE_REASON; //tANI_U16 // tSirMacReasonCodes         
                    btampContext->deAuthReasonCode = 0; // tANI_U16   // eSIR_SME_DEAUTH_FROM_PEER 
                    // Shouldn't the next line really use a tANI_U16? //0; // tANI_U16   // eSIR_SME_DISASSOC_FROM_PEER
                    btampContext->disassocReasonCode = btampContext->reasonCode; // tSirMacReasonCodes 
                    // Let's remember the peer who just disassoc'd us
                    //memcpy(btampContext->peerMacAddr, pCsrRoamInfo->peerMacOrBssidForIBSS, 6);
                } 
            }
            break;

        case eCSR_ROAM_RESULT_DEAUTH_IND:
            {
                btampContext->mAssociated = false;

                if (mLinkStatus != 0)
                {
                    DBGLOG("%s: marking link as down in %s\n", __func__, "eCSR_ROAM_RESULT_DEAUTH_IND");
                    mLinkStatus = (tANI_U8) 0;
                    ((IO80211Interface*) mNetworkIF)->setLinkState(kIO80211NetworkLinkDown);
                }
                else
                {
                    DBGLOG("%s: link already down in %s\n", __func__, "eCSR_ROAM_RESULT_DEAUTH_IND");
                }

                //if (pCsrRoamInfo)  // For now, leave this commented out. Until CSR changes integrated.
                {
                    // Now set the reason and status codes.  
                    // Actually, the "result code" field in the tSirSmeDeauthInd should be named reasonCode and NOT statusCode.  
                    // "Reason Codes" are found in DisAssoc or DeAuth Ind. "Status Code" fields are found in Rsp Mgmt Frame.  
                    // For now, we are going to have to (painfully) map the only "result code" type information we have 
                    // available at ALL from LIM/CSR.  And that is the statusCode field of type tSirResultCodes         
                    // BTW, tSirResultCodes is the COMPLETELY WRONG TYPE for this "result code" field. It SHOULD be 
                    // of type tSirMacReasonCodes.         
                    // Right now, we don't even have that.  So, I have to just make up some "reason code" that I will
                    // pretend I found in the incoming DeAuth Indication.        
                    //btampContext->statusCode = ((tpSirSmeDeauthInd) pCallbackInfo)->statusCode; // tSirResultCodes         
                    //btampContext->reasonCode = ((tpSirSmeDeauthInd) pCallbackInfo)->statusCode; // tSirResultCodes         
                    btampContext->reasonCode = (tANI_U16) eSIR_MAC_UNSPEC_FAILURE_REASON; //tANI_U16 // tSirMacReasonCodes         
                    btampContext->disassocReasonCode = 0; // tANI_U16   // eSIR_SME_DISASSOC_FROM_PEER 
                    // Shouldn't the next line really use a tANI_U16? //0; // tANI_U16   // eSIR_SME_DEAUTH_FROM_PEER
                    btampContext->deAuthReasonCode = btampContext->reasonCode; // tSirMacReasonCodes 
                    // Let's remember the peer who just de-auth'd us
                    //memcpy(btampContext->peerMacAddr, ((tpSirSmeDeauthInd) pCallbackInfo)->peerMacAddr, 6);                
                } 
            }
            break;

        case eCSR_ROAM_RESULT_MIC_ERROR_UNICAST: 

            //if (eCSR_ROAM_MIC_ERROR_IND == roamStatus)  // Make sure
            { 
                if (btampContext->mTKIPCounterMeasures)
                {
                    ((IO80211Interface*) mNetworkIF)->postMessage(APPLE80211_M_MIC_ERROR_UCAST); 
                    DBGLOG("%s: TKIP Countermeasures in effect in %s\n", __func__, "eCSR_ROAM_RESULT_MIC_ERROR_UNICAST"); 
                } 
                else 
                { 
                    DBGLOG("%s: TKIP Countermeasures disabled in %s\n", __func__, "eCSR_ROAM_RESULT_MIC_ERROR_UNICAST"); 
                }
            }
            break;

        case eCSR_ROAM_RESULT_MIC_ERROR_GROUP: 

            //if (eCSR_ROAM_MIC_ERROR_IND == roamStatus)  // Make sure
            { 
                if (btampContext->mTKIPCounterMeasures)
                { 
                    ((IO80211Interface*) mNetworkIF)->postMessage(APPLE80211_M_MIC_ERROR_MCAST); 
                    DBGLOG("%s: TKIP Countermeasures in effect in %s\n", __func__, "eCSR_ROAM_RESULT_MIC_ERROR_GROUP"); 
                } 
                else 
                { 
                    DBGLOG("%s: TKIP Countermeasures disabled in %s\n", __func__, "eCSR_ROAM_RESULT_MIC_ERROR_GROUP"); 
                }
            }
            break;

        default:
            break;
    }
    switch (roamStatus) {
        case eCSR_ROAM_ROAMING_START: 
            DBGLOG("%s: In %s\n", __func__, "eCSR_ROAM_ROAMING_START");
            // This only gets called when CSR decides to roam on its own - due to lostlink. 
            // Apple still needs to be told.
            if ((pCsrRoamInfo) && (pCsrRoamInfo->pConnectedProfile) && (pCsrRoamInfo->pConnectedProfile->pBssDesc))
            {
                memcpy(bssid.ether_addr_octet, pCsrRoamInfo->pConnectedProfile->pBssDesc->bssId,
                       sizeof(tSirMacAddr)); 
                apple80211Interface->willRoam(&bssid);  // Return result isn't significant 
                DBGLOG("%s: willRoam returns\n", __func__);
            }
            break;

        case eCSR_ROAM_SHOULD_ROAM:
            if ((pCsrRoamInfo) && (pCsrRoamInfo->pBssDesc)) {
                // pCallbackInfo points to the BSS desc. Convert to Apple Scan Result.
                halStatus = ccpCsrToAppleScanResult( 
                        mPMacObject, 
                        pCsrRoamInfo->pBssDesc, 
                        &scanResult); 
                if ( halStatus != 0 ) 
                    return eHAL_STATUS_FAILURE;
                roamAccepted = apple80211Interface->shouldRoam(&scanResult);  // Return result is crucial
                if (roamAccepted == true) { 
                    // If the roam is acceptable, return SUCCESS 
                    DBGLOG("%s: shouldRoam returns \"acceptable\"\n", __func__);
//#if 0
                    // Actually, before returning, immediately signal willRoam
                    // This is a workaround for a CSR bug.  Eventually, when 
                    // eCSR_ROAM_ASSOCIATION_START gets called WITH callback param p1
                    // pointing to a tBssDescription, this work-around can be removed.
                    memcpy(bssid.ether_addr_octet, pCsrRoamInfo->pBssDesc->bssId, sizeof(tSirMacAddr)); 
                    apple80211Interface->willRoam(&bssid);  // Return result isn't significant 
                    DBGLOG("%s: willRoam (called out of order) returns\n", __func__);
                    DBGLOG("    with BSSID = " MAC_ADDR_STRING(bssid.ether_addr_octet));
//#endif
                    return eHAL_STATUS_SUCCESS;
                } else { 
                    // If the roam is NOT acceptable, return FAILURE
                    DBGLOG("%s: shouldRoam returns \"NOT acceptable\"\n", __func__);
                    return eHAL_STATUS_FAILURE;
                }
            }
            break;

        case eCSR_ROAM_DISASSOCIATED:
            //if (eCSR_ROAM_RESULT_FORCED == roamResult || eCSR_ROAM_RESULT_MIC_ERROR == roamResult)
            {
                btampContext->mAssociated = false;

                if (mLinkStatus != 0)
                {
                    DBGLOG("%s: marking link as down in %s\n", __func__, "eCSR_ROAM_DISASSOCIATED");
                    mLinkStatus = (tANI_U8) 0;
                    ((IO80211Interface*) mNetworkIF)->setLinkState(kIO80211NetworkLinkDown);
                }
                else
                {
                    DBGLOG("%s: link already down in %s\n", __func__, "eCSR_ROAM_DISASSOCIATED");
                }
            }
            break;

        case eCSR_ROAM_LOSTLINK:
            btampContext->mAssociatedStatus = APPLE80211_STATUS_UNSPECIFIED_FAILURE;
            btampContext->mAssociated = false;

            if (mLinkStatus != 0)
            {
                DBGLOG("%s: marking link as down in %s\n", __func__, "eCSR_ROAM_LOSTLINK");
                mLinkStatus = (tANI_U8) 0;
                ((IO80211Interface*) mNetworkIF)->setLinkState(kIO80211NetworkLinkDown);
            }
            else
            {
                DBGLOG("%s: link already down in %s\n", __func__, "eCSR_ROAM_LOSTLINK");
            }
            break;

        case eCSR_ROAM_ASSOCIATION_START:
            DBGLOG("%s: In %s\n", __func__, "eCSR_ROAM_ASSOCIATION_START");
#if 0
            // This is the right place to call willRoam - for an "initial" association.
            // But, unfortunately, when eCSR_ROAM_ASSOCIATION_START gets called, 
            // it doesn't have a pointer to the tBssDescription in the roaming callback 
            // routines parameter p1 (pCallbackInfo in SetWextState).   So, don't use this code, yet.
            if ((pCsrRoamInfo) && (pCsrRoamInfo->pBssDesc) {
                memcpy(bssid.ether_addr_octet, pCsrRoamInfo->pBssDesc->bssId, 6); 
                apple80211Interface->willRoam(&bssid);  // Return result isn't significant 
                DBGLOG("%s: willRoam returns\n", __func__);
                DBGLOG("    with BSSID = " MAC_ADDR_STRING(bssid.ether_addr_octet));
            }
#endif //0
            break;

        case eCSR_ROAM_ASSOCIATION_COMPLETION:
            DBGLOG("%s: In %s\n", __func__, "eCSR_ROAM_ASSOCIATION_COMPLETION");
            break;

        case eCSR_ROAM_MIC_ERROR_IND:   // Handled in eCSR_ROAM_RESULT_MIC_ERROR_UNICAST and GROUP, above
        case eCSR_ROAM_CANCELLED:
        case eCSR_ROAM_ROAMING_COMPLETION:
        case eCSR_ROAM_SCAN_FOUND_NEW_BSS:
        default:
            break;
    }
#endif //0

    return halStatus;
}

/*----------------------------------------------------------------------------
    Host Controller Interface Procedural API
 ---------------------------------------------------------------------------*/

/** BT v3.0 Link Control commands */

/*----------------------------------------------------------------------------
    Each of the next eight command result in asynchronous events (e.g.,  
    HCI_PHYSICAL_LINK_COMPLETE_EVENT, HCI_LOGICAL_LINK_COMPLETE_EVENT, etc...)
    These are signalled thru the event callback. (I.E., (*tpWLAN_BAPEventCB).)
 ---------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPPhysicalLinkCreate()

  DESCRIPTION 
    Implements the actual HCI Create Physical Link command

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
                 WLANBAP_GetNewHndl has to be called before every call to 
                 WLAN_BAPPhysicalLinkCreate. Since the context is per 
                 physical link.
    pBapHCIPhysLinkCreate:  pointer to the "HCI Create Physical Link" Structure.
    pHddHdl:  The context passed in by the caller. (e.g., BSL specific context)

    IN/OUT
    pBapHCIEvent:  Return event value for the command status event. 
                (The caller of this routine is responsible for sending 
                the Command Status event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIPhysLinkCreate is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPPhysicalLinkCreate 
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Create_Physical_Link_Cmd   *pBapHCIPhysLinkCreate,
  v_PVOID_t      pHddHdl,   /* BSL passes in its specific context */
                            /* And I get phy_link_handle from the Command */
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{
    tWLAN_BAPEvent bapEvent; /* State machine event */
    VOS_STATUS  vosStatus;
    /* I am using btampContext, instead of pBapPhysLinkMachine */ 
    //tWLAN_BAPbapPhysLinkMachine *pBapPhysLinkMachine;
    ptBtampContext btampContext = (ptBtampContext) btampHandle; /* btampContext value */ 
    v_U8_t status;    /* return the BT-AMP status here */
    BTAMPFSM_INSTANCEDATA_T *instanceVar = &(btampContext->bapPhysLinkMachine);

    /* Validate params */ 
    if ((pBapHCIPhysLinkCreate == NULL) || (NULL == btampContext))
    {
      VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "%s: btampHandle value: %p, pBapHCIPhysLinkCreate is %p",
                 __func__,  btampHandle, pBapHCIPhysLinkCreate); 
      return VOS_STATUS_E_FAULT;
    }

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: btampHandle value: %p", __func__,  btampHandle);

    if(DISCONNECTED != instanceVar->stateVar)
    {
       /* Create/Accept Phy link request in invalid state */
        status = WLANBAP_ERROR_MAX_NUM_CNCTS;

    }
    else
    {
        /* Fill in the event structure */ 
        bapEvent.event = eWLAN_BAP_HCI_PHYSICAL_LINK_CREATE;
        bapEvent.params = pBapHCIPhysLinkCreate;
        //bapEvent.callback = pBapHCIPhysLinkCreateCB;

        /* Allocate a new state machine instance */ 
        /* There will only ever be one of these (NB: Don't assume this.) */
        /* So for now this returns a pointer to a static structure */ 
        /* (With all state set to initial values) */
        vosStatus = WLANBAP_CreateNewPhyLinkCtx ( 
                btampHandle, 
                pBapHCIPhysLinkCreate->phy_link_handle, /*  I get phy_link_handle from the Command */
                pHddHdl,   /* BSL passes in its specific context */
                &btampContext, /* Handle to return per assoc btampContext value in  */ 
                BT_INITIATOR); /* BT_INITIATOR */ 

        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: btampContext value: %p", __func__,  btampContext);

        /* Handle event */ 
        vosStatus = btampFsm(btampContext, &bapEvent, &status);
    }
  
        /* Format the command status event to return... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_STATUS_EVENT;
    pBapHCIEvent->u.btampCommandStatusEvent.present = 1;
    pBapHCIEvent->u.btampCommandStatusEvent.status = status;
    pBapHCIEvent->u.btampCommandStatusEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandStatusEvent.command_opcode 
        = BTAMP_TLV_HCI_CREATE_PHYSICAL_LINK_CMD;

    /* ... */ 

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPPhysicalLinkCreate */ 

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPPhysicalLinkAccept()

  DESCRIPTION 
    Implements the actual HCI Accept Physical Link command

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIPhysLinkAccept:  pointer to the "HCI Accept Physical Link" Structure.
    pHddHdl:  The context passed in by the caller. (e.g., BSL specific context)
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command status event. 
                (The caller of this routine is responsible for sending 
                the Command Status event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIPhysLinkAccept is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPPhysicalLinkAccept 
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Accept_Physical_Link_Cmd   *pBapHCIPhysLinkAccept,
  v_PVOID_t      pHddHdl,   /* BSL passes in its specific context */
                            /* And I get phy_link_handle from the Command */
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{
    tWLAN_BAPEvent bapEvent; /* State machine event */
    VOS_STATUS  vosStatus;
    /* I am using btampContext, instead of pBapPhysLinkMachine */ 
    //tWLAN_BAPbapPhysLinkMachine *pBapPhysLinkMachine;
    ptBtampContext btampContext = (ptBtampContext) btampHandle; /* btampContext value */ 
    v_U8_t status;    /* return the BT-AMP status here */
    BTAMPFSM_INSTANCEDATA_T *instanceVar;

    /* Validate params */ 
    if ((pBapHCIPhysLinkAccept == NULL) || (NULL == btampContext))
    {
      VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "%s: btampHandle value: %p, pBapHCIPhysLinkAccept is %p",
                 __func__,  btampHandle, pBapHCIPhysLinkAccept); 
      return VOS_STATUS_E_FAULT;
    }

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: btampHandle value: %p", __func__,  btampHandle);

    instanceVar = &(btampContext->bapPhysLinkMachine);
    if(DISCONNECTED != instanceVar->stateVar)
    {
       /* Create/Accept Phy link request in invalid state */
        status = WLANBAP_ERROR_MAX_NUM_CNCTS;

    }
    else
    {
        /* Fill in the event structure */ 
        bapEvent.event = eWLAN_BAP_HCI_PHYSICAL_LINK_ACCEPT;
        bapEvent.params = pBapHCIPhysLinkAccept;
        //bapEvent.callback = pBapHCIPhysLinkAcceptCB;

        /* Allocate a new state machine instance */ 
        /* There will only ever be one of these (NB: Don't assume this.) */
        /* So for now this returns a pointer to a static structure */ 
        /* (With all state set to initial values) */
        vosStatus = WLANBAP_CreateNewPhyLinkCtx ( 
                btampHandle, 
                pBapHCIPhysLinkAccept->phy_link_handle, /*  I get phy_link_handle from the Command */
                pHddHdl,   /* BSL passes in its specific context */
                &btampContext, /* Handle to return per assoc btampContext value in  */ 
                BT_RESPONDER); /* BT_RESPONDER */ 

        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: btampContext value: %p", __func__,  btampContext);

        /* Handle event */ 
        vosStatus = btampFsm(btampContext, &bapEvent, &status);
  
    }
    /* Format the command status event to return... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_STATUS_EVENT;
    pBapHCIEvent->u.btampCommandStatusEvent.present = 1;
    pBapHCIEvent->u.btampCommandStatusEvent.status = status;
    pBapHCIEvent->u.btampCommandStatusEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandStatusEvent.command_opcode 
        = BTAMP_TLV_HCI_ACCEPT_PHYSICAL_LINK_CMD;

    /* ... */ 

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPPhysicalLinkAccept */ 

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPPhysicalLinkDisconnect()

  DESCRIPTION 
    Implements the actual HCI Disconnect Physical Link command

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIPhysLinkDisconnect:  pointer to the "HCI Disconnect Physical Link" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command status event. 
                (The caller of this routine is responsible for sending 
                the Command Status event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIPhysLinkDisconnect is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPPhysicalLinkDisconnect 
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Disconnect_Physical_Link_Cmd   *pBapHCIPhysLinkDisconnect,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{
    tWLAN_BAPEvent bapEvent; /* State machine event */
    VOS_STATUS  vosStatus;
    /* I am using btampContext, instead of pBapPhysLinkMachine */ 
    //tWLAN_BAPbapPhysLinkMachine *pBapPhysLinkMachine;
    ptBtampContext btampContext = (ptBtampContext) btampHandle; /* btampContext value */ 
    v_U8_t status;    /* return the BT-AMP status here */

    /* Validate params */ 
    if (pBapHCIPhysLinkDisconnect == NULL) {
      return VOS_STATUS_E_FAULT;
    }

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: btampHandle value: %p", __func__,  btampHandle);

    /* Validate the Physical link handle */
    if (pBapHCIPhysLinkDisconnect->phy_link_handle != btampContext->phy_link_handle) 
    {
        /* Format the command status event to return... */ 
        pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_STATUS_EVENT;
        pBapHCIEvent->u.btampCommandStatusEvent.present = 1;
        pBapHCIEvent->u.btampCommandStatusEvent.status = WLANBAP_ERROR_NO_CNCT;
        pBapHCIEvent->u.btampCommandStatusEvent.num_hci_command_packets = 1;
        pBapHCIEvent->u.btampCommandStatusEvent.command_opcode
            = BTAMP_TLV_HCI_DISCONNECT_PHYSICAL_LINK_CMD;
        return VOS_STATUS_SUCCESS;
    }

    /* Fill in the event structure */ 
    bapEvent.event = eWLAN_BAP_HCI_PHYSICAL_LINK_DISCONNECT;
    bapEvent.params = pBapHCIPhysLinkDisconnect;

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: btampContext value: %p", __func__,  btampContext);

    /* Handle event */ 
    vosStatus = btampFsm(btampContext, &bapEvent, &status);
  
        /* Fill in the event structure */ 
    bapEvent.event =  eWLAN_BAP_MAC_READY_FOR_CONNECTIONS;
    bapEvent.params = pBapHCIPhysLinkDisconnect;

        /* Handle event */ 
    vosStatus = btampFsm(btampContext, &bapEvent, &status);


    /* Format the command status event to return... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_STATUS_EVENT;
    pBapHCIEvent->u.btampCommandStatusEvent.present = 1;
    pBapHCIEvent->u.btampCommandStatusEvent.status = status;
    pBapHCIEvent->u.btampCommandStatusEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandStatusEvent.command_opcode
        = BTAMP_TLV_HCI_DISCONNECT_PHYSICAL_LINK_CMD;

    /* ... */ 

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPPhysicalLinkDisconnect */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPLogicalLinkCreate()

  DESCRIPTION 
    Implements the actual HCI Create Logical Link command

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCILogLinkCreate:  pointer to the "HCI Create Logical Link" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command status event. 
                (The caller of this routine is responsible for sending 
                the Command Status event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCILogLinkCreate is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPLogicalLinkCreate
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Create_Logical_Link_Cmd   *pBapHCILogLinkCreate,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{
    tBtampHCI_Event bapHCIEvent; /* This now encodes ALL event types */
    VOS_STATUS  vosStatus;
    ptBtampContext btampContext = (ptBtampContext) btampHandle;
    v_U16_t log_link_index = 0;
    BTAMPFSM_INSTANCEDATA_T *instanceVar = &(btampContext->bapPhysLinkMachine);
    VOS_STATUS  retval;
    v_U16_t index_for_logLinkCtx = 0;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/


    /* Validate params */ 
    if (btampHandle == NULL) {
      return VOS_STATUS_E_FAULT;
    }

    /* Validate params */ 
    if (pBapHCILogLinkCreate == NULL) {
      return VOS_STATUS_E_FAULT;
    }


    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: btampHandle value: %p", __func__,  btampHandle);

    /* Validate the BAP state to accept the logical link request
       Logical Link create/accept requests are allowed only in
       CONNECTED state */
    /* Form and immediately return the command status event... */ 
    bapHCIEvent.bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_STATUS_EVENT;
    bapHCIEvent.u.btampCommandStatusEvent.present = 1;
    bapHCIEvent.u.btampCommandStatusEvent.num_hci_command_packets = 1;
    bapHCIEvent.u.btampCommandStatusEvent.command_opcode 
        = BTAMP_TLV_HCI_CREATE_LOGICAL_LINK_CMD;

    retval = VOS_STATUS_E_FAILURE;
    if(DISCONNECTED == instanceVar->stateVar)
    {
       /* Create Logical link request in invalid state */
        pBapHCIEvent->u.btampLogicalLinkCompleteEvent.status =
            WLANBAP_ERROR_CMND_DISALLOWED;
        bapHCIEvent.u.btampCommandStatusEvent.status = WLANBAP_ERROR_NO_CNCT;

    }
    else if (CONNECTED != instanceVar->stateVar)
    {
        /* Create Logical link request in invalid state */
        pBapHCIEvent->u.btampLogicalLinkCompleteEvent.status =
            WLANBAP_ERROR_CMND_DISALLOWED;
        bapHCIEvent.u.btampCommandStatusEvent.status = WLANBAP_ERROR_CMND_DISALLOWED;
    }
    else if (pBapHCILogLinkCreate->phy_link_handle != btampContext->phy_link_handle)
    {
       /* Invalid Physical link handle */
        pBapHCIEvent->u.btampLogicalLinkCompleteEvent.status =
            WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
        bapHCIEvent.u.btampCommandStatusEvent.status = WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
    }
    else
    {
        btampContext->btamp_logical_link_state = WLAN_BAPLogLinkInProgress;

        if( TRUE == btampContext->btamp_logical_link_cancel_pending )
        {
            pBapHCIEvent->u.btampLogicalLinkCompleteEvent.status =
                WLANBAP_ERROR_NO_CNCT;
            bapHCIEvent.u.btampCommandStatusEvent.status = WLANBAP_ERROR_NO_CNCT;
            btampContext->btamp_logical_link_state = WLAN_BAPLogLinkClosed;
            btampContext->btamp_logical_link_cancel_pending = FALSE;
        }
        else
        {
            /* If btamp_async_logical_link_create is set, we will seralize the req
               on MC thread & handle it there after; If the above flag is not set
               respond to HCI the sync way as before */
            if(FALSE == btampContext->btamp_async_logical_link_create)
            {
                 /* Allocate a logical link index for these flow specs */
                 vosStatus = WLANBAP_CreateNewLogLinkCtx( 
                   btampContext, /* per assoc btampContext value */ 
                   pBapHCILogLinkCreate->phy_link_handle, /*  I get phy_link_handle from the Command */
                   pBapHCILogLinkCreate->tx_flow_spec, /*  I get tx_flow_spec from the Command */
                   pBapHCILogLinkCreate->rx_flow_spec, /*  I get rx_flow_spec from the Command */
                   &log_link_index /*  Return the logical link index here */
                   );
                 if (VOS_STATUS_SUCCESS != vosStatus)
                 {
                     /* Invalid flow spec format */
                     pBapHCIEvent->u.btampLogicalLinkCompleteEvent.status =
                        WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
                     bapHCIEvent.u.btampCommandStatusEvent.status = WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
                     btampContext->btamp_logical_link_state = WLAN_BAPLogLinkClosed;
                 }
                 else
                 {
                      retval = VOS_STATUS_SUCCESS;
                      bapHCIEvent.u.btampCommandStatusEvent.status = WLANBAP_STATUS_SUCCESS;

                      pBapHCIEvent->u.btampLogicalLinkCompleteEvent.status = WLANBAP_STATUS_SUCCESS;
                      btampContext->btamp_logical_link_state = WLAN_BAPLogLinkOpen;
                 }
            }
            else
            {
                btampContext->btamp_logical_link_req_info.phyLinkHandle = 
                    pBapHCILogLinkCreate->phy_link_handle;
                vos_mem_copy(btampContext->btamp_logical_link_req_info.txFlowSpec,
                             pBapHCILogLinkCreate->tx_flow_spec, 18);
                vos_mem_copy(btampContext->btamp_logical_link_req_info.rxFlowSpec,
                             pBapHCILogLinkCreate->rx_flow_spec, 18);
                btampContext->btamp_async_logical_link_create = FALSE;
                vosStatus = btampEstablishLogLink(btampContext);
                if(VOS_STATUS_SUCCESS == vosStatus)
                {
                    retval = VOS_STATUS_E_BUSY;//this will make sure event complete is not sent to HCI
                }
                else
                {
                    pBapHCIEvent->u.btampLogicalLinkCompleteEvent.status =
                        WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
                    bapHCIEvent.u.btampCommandStatusEvent.status = WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
                    btampContext->btamp_logical_link_state = WLAN_BAPLogLinkClosed;
                }
    
            }
        }
    }

    vosStatus = (*btampContext->pBapHCIEventCB) 
                (  
                 btampContext->pHddHdl,   /* this refers to the BSL per connection context */
                 &bapHCIEvent, /* This now encodes ALL event types */
                 VOS_TRUE /* Flag to indicate assoc-specific event */ 
                );

    index_for_logLinkCtx = log_link_index >> 8;
    /* Format the Logical Link Complete event to return... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_LOGICAL_LINK_COMPLETE_EVENT;
    pBapHCIEvent->u.btampLogicalLinkCompleteEvent.present = 1;

    /*  Return the logical link index here */
    pBapHCIEvent->u.btampLogicalLinkCompleteEvent.log_link_handle 
        = log_link_index;
    pBapHCIEvent->u.btampLogicalLinkCompleteEvent.phy_link_handle 
        = pBapHCILogLinkCreate->phy_link_handle;
    pBapHCIEvent->u.btampLogicalLinkCompleteEvent.flow_spec_id
        = btampContext->btampLogLinkCtx[index_for_logLinkCtx].btampFlowSpec.flow_spec_id;

    /* ... */ 

    return retval;
} /* WLAN_BAPLogicalLinkCreate */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPLogicalLinkAccept()

  DESCRIPTION 
    Implements the actual HCI Accept Logical Link command

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCILogLinkAccept:  pointer to the "HCI Accept Logical Link" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command status event. 
                (The caller of this routine is responsible for sending 
                the Command Status event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCILogLinkAccept is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPLogicalLinkAccept
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Accept_Logical_Link_Cmd   *pBapHCILogLinkAccept,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{
    tBtampHCI_Event bapHCIEvent; /* This now encodes ALL event types */
    VOS_STATUS  vosStatus;
    ptBtampContext btampContext = (ptBtampContext) btampHandle;
    v_U16_t log_link_index = 0;
    BTAMPFSM_INSTANCEDATA_T *instanceVar = &(btampContext->bapPhysLinkMachine);
    VOS_STATUS  retval;
    v_U16_t index_for_logLinkCtx;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/


    /* Validate params */ 
    if (btampHandle == NULL) {
      return VOS_STATUS_E_FAULT;
    }

    /* Validate params */ 
    if (pBapHCILogLinkAccept == NULL) {
      return VOS_STATUS_E_FAULT;
    }


    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: btampHandle value: %p", __func__,  btampHandle);

    /* Validate the BAP state to accept the logical link request
       Logical Link create/accept requests are allowed only in
       CONNECTED state */
    /* Form and immediately return the command status event... */ 
    bapHCIEvent.bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_STATUS_EVENT;
    bapHCIEvent.u.btampCommandStatusEvent.present = 1;
    bapHCIEvent.u.btampCommandStatusEvent.num_hci_command_packets = 1;
    bapHCIEvent.u.btampCommandStatusEvent.command_opcode 
        = BTAMP_TLV_HCI_ACCEPT_LOGICAL_LINK_CMD;

    retval = VOS_STATUS_E_FAILURE;
    if(DISCONNECTED == instanceVar->stateVar)
    {
       /* Create Logical link request in invalid state */
        pBapHCIEvent->u.btampLogicalLinkCompleteEvent.status =
            WLANBAP_ERROR_CMND_DISALLOWED;
        bapHCIEvent.u.btampCommandStatusEvent.status = WLANBAP_ERROR_NO_CNCT;

    }
    else if (CONNECTED != instanceVar->stateVar)
    {
        /* Create Logical link request in invalid state */
        pBapHCIEvent->u.btampLogicalLinkCompleteEvent.status =
            WLANBAP_ERROR_CMND_DISALLOWED;
        bapHCIEvent.u.btampCommandStatusEvent.status = WLANBAP_ERROR_CMND_DISALLOWED;
    }
    else if (pBapHCILogLinkAccept->phy_link_handle != btampContext->phy_link_handle)
    {
       /* Invalid Physical link handle */
        pBapHCIEvent->u.btampLogicalLinkCompleteEvent.status =
            WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
        bapHCIEvent.u.btampCommandStatusEvent.status = WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
    }
    else
    {
        btampContext->btamp_logical_link_state = WLAN_BAPLogLinkInProgress;
        if( TRUE == btampContext->btamp_logical_link_cancel_pending )
        {
            pBapHCIEvent->u.btampLogicalLinkCompleteEvent.status =
                WLANBAP_ERROR_NO_CNCT;
            bapHCIEvent.u.btampCommandStatusEvent.status = WLANBAP_ERROR_NO_CNCT;
            btampContext->btamp_logical_link_state = WLAN_BAPLogLinkClosed;
            btampContext->btamp_logical_link_cancel_pending = FALSE;
        }
        else
        {
            /* If btamp_async_logical_link_create is set, we will seralize the req
               on MC thread & handle it there after; If the above flag is not set
               respond to HCI the sync way as before */
            if(FALSE == btampContext->btamp_async_logical_link_create)
            {
                /* Allocate a logical link index for these flow specs */
                vosStatus = WLANBAP_CreateNewLogLinkCtx( 
                        btampContext, /* per assoc btampContext value */ 
                        pBapHCILogLinkAccept->phy_link_handle, /*  I get phy_link_handle from the Command */
                        pBapHCILogLinkAccept->tx_flow_spec, /*  I get tx_flow_spec from the Command */
                        pBapHCILogLinkAccept->rx_flow_spec, /*  I get rx_flow_spec from the Command */
                        &log_link_index /*  Return the logical link index here */
                        );
                if (VOS_STATUS_SUCCESS != vosStatus)
                {
                    /* Invalid flow spec format */
                    pBapHCIEvent->u.btampLogicalLinkCompleteEvent.status =
                        WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
                    bapHCIEvent.u.btampCommandStatusEvent.status = WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
                    btampContext->btamp_logical_link_state = WLAN_BAPLogLinkClosed;
                }
                else
                {
                    retval = VOS_STATUS_SUCCESS;
                    bapHCIEvent.u.btampCommandStatusEvent.status = WLANBAP_STATUS_SUCCESS;

                    pBapHCIEvent->u.btampLogicalLinkCompleteEvent.status = WLANBAP_STATUS_SUCCESS;
                    btampContext->btamp_logical_link_state = WLAN_BAPLogLinkOpen;
                }
            }
            else
            {
                btampContext->btamp_logical_link_req_info.phyLinkHandle = 
                    pBapHCILogLinkAccept->phy_link_handle;
                vos_mem_copy(btampContext->btamp_logical_link_req_info.txFlowSpec,
                             pBapHCILogLinkAccept->tx_flow_spec, 18);
                vos_mem_copy(btampContext->btamp_logical_link_req_info.rxFlowSpec,
                             pBapHCILogLinkAccept->rx_flow_spec, 18);
                btampContext->btamp_async_logical_link_create = FALSE;
                vosStatus = btampEstablishLogLink(btampContext);
                if(VOS_STATUS_SUCCESS == vosStatus)
                {
                    retval = VOS_STATUS_E_BUSY;//this will make sure event complete is not sent to HCI
                }
                else
                {
                    pBapHCIEvent->u.btampLogicalLinkCompleteEvent.status =
                        WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
                    bapHCIEvent.u.btampCommandStatusEvent.status = WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
                    btampContext->btamp_logical_link_state = WLAN_BAPLogLinkClosed;
                }
    
            }
        }
    }
    vosStatus = (*btampContext->pBapHCIEventCB) 
            (  
             btampContext->pHddHdl,   /* this refers to the BSL per connection context */
             &bapHCIEvent, /* This now encodes ALL event types */
             VOS_TRUE /* Flag to indicate assoc-specific event */ 
             );

    index_for_logLinkCtx = log_link_index >> 8;

    /* Format the Logical Link Complete event to return... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_LOGICAL_LINK_COMPLETE_EVENT;
    pBapHCIEvent->u.btampLogicalLinkCompleteEvent.present = 1;
    /*  Return the logical link index here */
    pBapHCIEvent->u.btampLogicalLinkCompleteEvent.log_link_handle 
        = log_link_index;
    pBapHCIEvent->u.btampLogicalLinkCompleteEvent.phy_link_handle 
        = pBapHCILogLinkAccept->phy_link_handle;
    pBapHCIEvent->u.btampLogicalLinkCompleteEvent.flow_spec_id
        = btampContext->btampLogLinkCtx[index_for_logLinkCtx].btampFlowSpec.flow_spec_id;

    /* ... */ 

    return retval;
} /* WLAN_BAPLogicalLinkAccept */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPLogicalLinkDisconnect()

  DESCRIPTION 
    Implements the actual HCI Disconnect Logical Link command

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCILogLinkDisconnect:  pointer to the "HCI Disconnect Logical Link" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command status event. 
                (The caller of this routine is responsible for sending 
                the Command Status event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCILogLinkDisconnect is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPLogicalLinkDisconnect
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Disconnect_Logical_Link_Cmd   *pBapHCILogLinkDisconnect,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{
    tBtampHCI_Event     bapHCIEvent; /* This now encodes ALL event types */
    ptBtampContext      btampContext = (ptBtampContext) btampHandle;
    tpBtampLogLinkCtx   pLogLinkContext;
    VOS_STATUS          retval = VOS_STATUS_SUCCESS;
    v_U8_t              log_link_index;

   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*------------------------------------------------------------------------
    Sanity check
    ------------------------------------------------------------------------*/
    if (( NULL == pBapHCILogLinkDisconnect ) ||
        ( NULL == btampContext))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, 
                   "Critical error: Invalid input parameter on %s", 
                   __func__); 
        return VOS_STATUS_E_FAULT; 
    }

    /* Derive logical link index from handle */
    log_link_index = ((pBapHCILogLinkDisconnect->log_link_handle) >> 8);

    if( log_link_index >= WLANBAP_MAX_LOG_LINKS )
    {
       VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, 
                  "Critical error: Invalid input parameter on %s", 
                  __func__); 
        /* Fill in the event code to propagate the event notification to BRM
           BRM generates the Command status Event based on this.*/
        pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_DISCONNECT_LOGICAL_LINK_COMPLETE_EVENT;
        pBapHCIEvent->u.btampDisconnectLogicalLinkCompleteEvent.present = 1;
        pBapHCIEvent->u.btampDisconnectLogicalLinkCompleteEvent.status =
            WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
        return VOS_STATUS_E_INVAL; 

    }

#ifdef BAP_DEBUG
  /* Trace the tBtampCtx being passed in. */
  VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
            "WLAN BAP Context Monitor: btampContext value = %p in %s:%d", btampContext, __func__, __LINE__ );
#endif //BAP_DEBUG

    bapHCIEvent.bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_STATUS_EVENT;
    bapHCIEvent.u.btampCommandStatusEvent.present = 1;
    bapHCIEvent.u.btampCommandStatusEvent.num_hci_command_packets = 1;
    bapHCIEvent.u.btampCommandStatusEvent.command_opcode 
        = BTAMP_TLV_HCI_DISCONNECT_LOGICAL_LINK_CMD;

   /*------------------------------------------------------------------------
    FIXME: Validate the Logical Link handle, Generation and freeing...
    Here the Logical link is not validated and assumed that it is correct to. 
    get the Logical link context.                                                                        . 
    ------------------------------------------------------------------------*/
    pLogLinkContext = 
    &(btampContext->btampLogLinkCtx[log_link_index]);
    
    // Validate whether the context is active.
    if ((VOS_FALSE == pLogLinkContext->present) ||
        (pBapHCILogLinkDisconnect->log_link_handle != pLogLinkContext->log_link_handle))
    {
        /* If status is failed, the platform specific layer generates the
           command status event with proper status */
        pBapHCIEvent->u.btampDisconnectLogicalLinkCompleteEvent.status =
            WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
        bapHCIEvent.u.btampCommandStatusEvent.status = WLANBAP_ERROR_NO_CNCT;
        retval = VOS_STATUS_E_FAILURE;
#ifdef BAP_DEBUG
        /* Log the error. */
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                "%s:%d Invalid Logical Link handle(should be) = %d(%d)", __func__, __LINE__,  
                pBapHCILogLinkDisconnect->log_link_handle, pLogLinkContext->log_link_handle);
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                " Logical Link index = %d", log_link_index);
#endif //BAP_DEBUG
    }
    else
    {
        /* Form and return the command status event... */ 
        bapHCIEvent.u.btampCommandStatusEvent.status = WLANBAP_STATUS_SUCCESS;
        pBapHCIEvent->u.btampDisconnectLogicalLinkCompleteEvent.status 
            = WLANBAP_STATUS_SUCCESS;
    

        pLogLinkContext->present         = VOS_FALSE; 
        pLogLinkContext->uTxPktCompleted = 0;
        pLogLinkContext->log_link_handle = 0;
        /* Decrement the total logical link count */
        btampContext->total_log_link_index--;
        btampContext->btamp_logical_link_state = WLAN_BAPLogLinkClosed;
    }
    
    /* Notify the Command status Event */
    (*btampContext->pBapHCIEventCB) 
        (  
            btampContext->pHddHdl,   /* this refers to the BSL per connection context */
            &bapHCIEvent, /* This now encodes ALL event types */
            VOS_TRUE /* Flag to indicate assoc-specific event */ 
        );

    /* Format the Logical Link Complete event to return... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_DISCONNECT_LOGICAL_LINK_COMPLETE_EVENT;
    pBapHCIEvent->u.btampDisconnectLogicalLinkCompleteEvent.present = 1;
    /*  Return the logical link index here */
    pBapHCIEvent->u.btampDisconnectLogicalLinkCompleteEvent.log_link_handle 
        = pBapHCILogLinkDisconnect->log_link_handle;
    pBapHCIEvent->u.btampDisconnectLogicalLinkCompleteEvent.reason 
        = WLANBAP_ERROR_TERM_BY_LOCAL_HOST;

    return retval;
} /* WLAN_BAPLogicalLinkDisconnect */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPLogicalLinkCancel()

  DESCRIPTION 
    Implements the actual HCI Cancel Logical Link command

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCILogLinkCancel:  pointer to the "HCI Cancel Logical Link" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
                (BTW, the required "HCI Logical Link Complete Event" 
                will be generated by the BAP state machine and sent up 
                via the (*tpWLAN_BAPEventCB).)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCILogLinkCancel is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPLogicalLinkCancel
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Logical_Link_Cancel_Cmd   *pBapHCILogLinkCancel,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{
   ptBtampContext btampContext;
   BTAMPFSM_INSTANCEDATA_T *instanceVar;
    /* Validate params */ 
    if ((btampHandle == NULL) || (pBapHCILogLinkCancel == NULL) || 
        (pBapHCIEvent == NULL))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
            "%s: Null Parameters Not allowed", __func__); 
        return VOS_STATUS_E_FAULT;
    }

    btampContext = (ptBtampContext) btampHandle;
    instanceVar = &(btampContext->bapPhysLinkMachine);

    /* Form and immediately return the command status event... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT;
    pBapHCIEvent->u.btampCommandCompleteEvent.present = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.command_opcode =
        BTAMP_TLV_HCI_LOGICAL_LINK_CANCEL_CMD;
    pBapHCIEvent->u.btampCommandCompleteEvent.num_hci_command_packets = 1;

    if (pBapHCILogLinkCancel->phy_link_handle != btampContext->phy_link_handle) 
    {
        pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Logical_Link_Cancel.status =
            WLANBAP_ERROR_NO_CNCT;
    }
    else
    {
        /* As the logical link create is returned immediately, the logical link is
           created and so cancel can not return success.
           And it returns WLANBAP_ERROR_NO_CNCT if not connected or
           WLANBAP_ERROR_MAX_NUM_ACL_CNCTS if connected */
        if(WLAN_BAPLogLinkClosed == btampContext->btamp_logical_link_state )
        {
           /* Cancel Logical link request in invalid state */
           pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Logical_Link_Cancel.status =
               WLANBAP_ERROR_NO_CNCT;
        }
        else if(WLAN_BAPLogLinkOpen == btampContext->btamp_logical_link_state )
        {
           /* Cancel Logical link request in conected state */
           pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Logical_Link_Cancel.status =
               WLANBAP_ERROR_MAX_NUM_ACL_CNCTS;       
        }
        else if(WLAN_BAPLogLinkInProgress == btampContext->btamp_logical_link_state )
        {
           /* Cancel Logical link request in progress state, need to fail logical link
            creation as well */
            btampContext->btamp_logical_link_cancel_pending = TRUE;
            pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Logical_Link_Cancel.status =
                WLANBAP_STATUS_SUCCESS;       
        }
        else
        {
            pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Logical_Link_Cancel.status =
                WLANBAP_ERROR_NO_CNCT;
        }
    }
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Logical_Link_Cancel.phy_link_handle =
        pBapHCILogLinkCancel->phy_link_handle;
    /* Since the status is not success, the Tx flow spec Id is not meaningful and
       filling with 0 */
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Logical_Link_Cancel.tx_flow_spec_id = 
        pBapHCILogLinkCancel->tx_flow_spec_id;

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPLogicalLinkCancel */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPFlowSpecModify()

  DESCRIPTION 
    Implements the actual HCI Modify Logical Link command
    Produces an asynchronous flow spec modify complete event. Through the 
    event callback.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIFlowSpecModify:  pointer to the "HCI Flow Spec Modify" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command status event. 
                (The caller of this routine is responsible for sending 
                the Command Status event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIFlowSpecModify is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPFlowSpecModify
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Flow_Spec_Modify_Cmd   *pBapHCIFlowSpecModify,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{
   v_U16_t                  index_for_logLinkHandle = 0;
   ptBtampContext           btampContext;
   tpBtampLogLinkCtx        pLogLinkContext;
   v_U32_t                  retval;
   VOS_STATUS               vosStatus = VOS_STATUS_SUCCESS;
   tBtampHCI_Event          bapHCIEvent; /* This now encodes ALL event types */
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
   /* Validate params */ 
   if ((btampHandle == NULL) || (pBapHCIFlowSpecModify == NULL) || 
       (pBapHCIEvent == NULL))
   {
       VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
           "%s: Null Parameters Not allowed", __func__); 
       return VOS_STATUS_E_FAULT;
   }

   btampContext = (ptBtampContext) btampHandle;

   index_for_logLinkHandle = pBapHCIFlowSpecModify->log_link_handle >> 8; /*  Return the logical link index here */
   VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO,
              " %s:index_for_logLinkHandle=%d", __func__,index_for_logLinkHandle);

   bapHCIEvent.bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_STATUS_EVENT;
   bapHCIEvent.u.btampCommandStatusEvent.present = 1;
   bapHCIEvent.u.btampCommandStatusEvent.num_hci_command_packets = 1;
   bapHCIEvent.u.btampCommandStatusEvent.command_opcode 
       = BTAMP_TLV_HCI_FLOW_SPEC_MODIFY_CMD;

   /*------------------------------------------------------------------------
     Evaluate the Tx and Rx Flow specification for this logical link.
   ------------------------------------------------------------------------*/
   // Currently we only support flow specs with service types of BE (0x01) 

   /*------------------------------------------------------------------------
     Now configure the Logical Link context.
   ------------------------------------------------------------------------*/
   pLogLinkContext = &(btampContext->btampLogLinkCtx[index_for_logLinkHandle]);

   /* Extract Tx flow spec into the context structure */
   retval = btampUnpackTlvFlow_Spec((void *)btampContext, pBapHCIFlowSpecModify->tx_flow_spec,
                                    WLAN_BAP_PAL_FLOW_SPEC_TLV_LEN,
                                    &pLogLinkContext->btampFlowSpec);
   if (retval != BTAMP_PARSE_SUCCESS)
   {
     /* Flow spec parsing failed, return failure */
     vosStatus = VOS_STATUS_E_FAILURE;
     pBapHCIEvent->u.btampFlowSpecModifyCompleteEvent.status =
         WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
     bapHCIEvent.u.btampCommandStatusEvent.status = WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
   }
   else
   {
      bapHCIEvent.u.btampCommandStatusEvent.status = WLANBAP_STATUS_SUCCESS;
      pBapHCIEvent->u.btampFlowSpecModifyCompleteEvent.status 
          = WLANBAP_STATUS_SUCCESS;

   }
           /* Notify the Command status Event */
   vosStatus =   
      (*btampContext->pBapHCIEventCB) 
          (  
           btampContext->pHddHdl,   /* this refers to the BSL per connection context */
           &bapHCIEvent, /* This now encodes ALL event types */
           VOS_TRUE /* Flag to indicate assoc-specific event */ 
          );

   /* Form and immediately return the command status event... */ 
   pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_FLOW_SPEC_MODIFY_COMPLETE_EVENT;
   pBapHCIEvent->u.btampFlowSpecModifyCompleteEvent.present = 1;
   pBapHCIEvent->u.btampFlowSpecModifyCompleteEvent.log_link_handle = 
      pBapHCIFlowSpecModify->log_link_handle;

   return vosStatus;
} /* WLAN_BAPFlowSpecModify */


void WLAN_BAPEstablishLogicalLink(ptBtampContext btampContext)
{
    tBtampHCI_Event bapHCIEvent; /* This now encodes ALL event types */
    v_U16_t         log_link_index = 0;
    v_U16_t         index_for_logLinkCtx = 0;
    VOS_STATUS      vosStatus = VOS_STATUS_SUCCESS;

    if (btampContext == NULL) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
            "%s: Null Parameters Not allowed", __func__); 
        return;
    }

    if( TRUE == btampContext->btamp_logical_link_cancel_pending )
    {
        bapHCIEvent.u.btampCommandStatusEvent.status = WLANBAP_ERROR_NO_CNCT;
        btampContext->btamp_logical_link_state = WLAN_BAPLogLinkClosed;
        btampContext->btamp_logical_link_cancel_pending = FALSE;
    }
    else
    {
        /* Allocate a logical link index for these flow specs */
        vosStatus = WLANBAP_CreateNewLogLinkCtx( 
            btampContext, /* per assoc btampContext value */ 
            btampContext->btamp_logical_link_req_info.phyLinkHandle, /*  I get phy_link_handle from the Command */
            btampContext->btamp_logical_link_req_info.txFlowSpec, /*  I get tx_flow_spec from the Command */
            btampContext->btamp_logical_link_req_info.rxFlowSpec, /*  I get rx_flow_spec from the Command */
            &log_link_index /*  Return the logical link index here */
            );
        if (VOS_STATUS_SUCCESS != vosStatus)
        {
            /* Invalid flow spec format */
            bapHCIEvent.u.btampLogicalLinkCompleteEvent.status = WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
            btampContext->btamp_logical_link_state = WLAN_BAPLogLinkClosed;
        }
        else
        {
            bapHCIEvent.u.btampLogicalLinkCompleteEvent.status = WLANBAP_STATUS_SUCCESS;
            btampContext->btamp_logical_link_state = WLAN_BAPLogLinkOpen;
        }
    }
    
    index_for_logLinkCtx = log_link_index >> 8;
    /* Format the Logical Link Complete event to return... */ 
    bapHCIEvent.bapHCIEventCode = BTAMP_TLV_HCI_LOGICAL_LINK_COMPLETE_EVENT;
    bapHCIEvent.u.btampLogicalLinkCompleteEvent.present = 1;

    /*  Return the logical link index here */
    bapHCIEvent.u.btampLogicalLinkCompleteEvent.log_link_handle 
        = log_link_index;
    bapHCIEvent.u.btampLogicalLinkCompleteEvent.phy_link_handle 
        = btampContext->btamp_logical_link_req_info.phyLinkHandle;
    bapHCIEvent.u.btampLogicalLinkCompleteEvent.flow_spec_id
        = btampContext->btampLogLinkCtx[index_for_logLinkCtx].btampFlowSpec.flow_spec_id;

    vosStatus = (*btampContext->pBapHCIEventCB) 
    (  
     btampContext->pHddHdl,   /* this refers to the BSL per connection context */
     &bapHCIEvent, /* This now encodes ALL event types */
     VOS_TRUE /* Flag to indicate assoc-specific event */ 
     );
    return;
}



