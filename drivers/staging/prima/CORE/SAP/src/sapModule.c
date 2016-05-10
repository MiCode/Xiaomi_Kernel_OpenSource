/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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


/*===========================================================================

                      s a p M o d u l e . C

  OVERVIEW:

  This software unit holds the implementation of the WLAN SAP modules
  functions providing EXTERNAL APIs. It is also where the global SAP module
  context gets initialised

  DEPENDENCIES:

  Are listed for each API below.

  Copyright (c) 2010 Qualcomm Technologies, Inc.
  All Rights Reserved.
  Qualcomm Technologies Confidential and Proprietary
===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.



  when               who                 what, where, why
----------       ---                --------------------------------------------------------
03/15/10     SOFTAP team            Created module
06/03/10     js                     Added support to hostapd driven
 *                                  deauth/disassoc/mic failure

===========================================================================*/

/* $Header$ */

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "wlan_qct_tl.h"
#include "vos_trace.h"

// Pick up the sme callback registration API
#include "sme_Api.h"

// SAP API header file

#include "sapInternal.h"
#include "smeInside.h"

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
#define SAP_DEBUG

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 *  External declarations for global context
 * -------------------------------------------------------------------------*/
//  No!  Get this from VOS.
//  The main per-Physical Link (per WLAN association) context.
ptSapContext  gpSapCtx;

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

/*==========================================================================
  FUNCTION    WLANSAP_Open

  DESCRIPTION
    Called at driver initialization (vos_open). SAP will initialize
    all its internal resources and will wait for the call to start to
    register with the other modules.

  DEPENDENCIES

  PARAMETERS

    IN
    pvosGCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT: Pointer to SAP cb is NULL ; access would cause a page
                         fault
    VOS_STATUS_SUCCESS: Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_Open
(
    v_PVOID_t pvosGCtx
)
{

    ptSapContext  pSapCtx = NULL;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
    VOS_ASSERT(pvosGCtx);
    /*------------------------------------------------------------------------
    Allocate (and sanity check?!) SAP control block
    ------------------------------------------------------------------------*/
    vos_alloc_context(pvosGCtx, VOS_MODULE_ID_SAP, (v_VOID_t **)&pSapCtx, sizeof(tSapContext));

    if (NULL == pSapCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pvosGCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }

    vos_mem_zero(pSapCtx, sizeof(tSapContext));

    /*------------------------------------------------------------------------
        Clean up SAP control block, initialize all values
    ------------------------------------------------------------------------*/
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANSAP_Open");

    WLANSAP_CleanCB(pSapCtx, 0 /*do not empty*/);

    // Setup the "link back" to the VOSS context
    pSapCtx->pvosGCtx = pvosGCtx;

    // Store a pointer to the SAP context provided by VOSS
    gpSapCtx = pSapCtx;

    /*------------------------------------------------------------------------
        Allocate internal resources
       ------------------------------------------------------------------------*/

    return VOS_STATUS_SUCCESS;
}// WLANSAP_Open

/*==========================================================================
  FUNCTION    WLANSAP_Start

  DESCRIPTION
    Called as part of the overall start procedure (vos_start). SAP will
    use this call to register with TL as the SAP entity for
    SAP RSN frames.

  DEPENDENCIES

  PARAMETERS

    IN
    pvosGCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT: Pointer to SAP cb is NULL ; access would cause a page
                         fault
    VOS_STATUS_SUCCESS: Success

  SIDE EFFECTS
============================================================================*/

VOS_STATUS
WLANSAP_Start
(
    v_PVOID_t  pvosGCtx
)
{
    ptSapContext  pSapCtx = NULL;

    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                 "WLANSAP_Start invoked successfully\n");
    /*------------------------------------------------------------------------
        Sanity check
        Extract SAP control block
    ------------------------------------------------------------------------*/
    pSapCtx = VOS_GET_SAP_CB(pvosGCtx);
    if ( NULL == pSapCtx )
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pvosGCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }

    /*------------------------------------------------------------------------
        For now, presume security is not enabled.
    -----------------------------------------------------------------------*/
    pSapCtx->ucSecEnabled = WLANSAP_SECURITY_ENABLED_STATE;


    /*------------------------------------------------------------------------
        Now configure the roaming profile links. To SSID and bssid.
    ------------------------------------------------------------------------*/
    // We have room for two SSIDs.
    pSapCtx->csrRoamProfile.SSIDs.numOfSSIDs = 1; // This is true for now.
    pSapCtx->csrRoamProfile.SSIDs.SSIDList = pSapCtx->SSIDList;  //Array of two
    pSapCtx->csrRoamProfile.SSIDs.SSIDList[0].SSID.length = 0;
    pSapCtx->csrRoamProfile.SSIDs.SSIDList[0].handoffPermitted = VOS_FALSE;
    pSapCtx->csrRoamProfile.SSIDs.SSIDList[0].ssidHidden = pSapCtx->SSIDList[0].ssidHidden;

    pSapCtx->csrRoamProfile.BSSIDs.numOfBSSIDs = 1; // This is true for now.
    pSapCtx->csrRoamProfile.BSSIDs.bssid = &pSapCtx->bssid;

    // Now configure the auth type in the roaming profile. To open.
    pSapCtx->csrRoamProfile.negotiatedAuthType = eCSR_AUTH_TYPE_OPEN_SYSTEM; // open is the default

    if( !VOS_IS_STATUS_SUCCESS( vos_lock_init( &pSapCtx->SapGlobalLock)))
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                 "WLANSAP_Start failed init lock\n");
        return VOS_STATUS_E_FAULT;
    }



    return VOS_STATUS_SUCCESS;
}/* WLANSAP_Start */

/*==========================================================================

  FUNCTION    WLANSAP_Stop

  DESCRIPTION
    Called by vos_stop to stop operation in SAP, before close. SAP will suspend all
    BT-AMP Protocol Adaption Layer operation and will wait for the close
    request to clean up its resources.

  DEPENDENCIES

  PARAMETERS

    IN
    pvosGCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT: Pointer to SAP cb is NULL ; access would cause a page
                         fault
    VOS_STATUS_SUCCESS: Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_Stop
(
    v_PVOID_t  pvosGCtx
)
{

    ptSapContext  pSapCtx = NULL;

    /*------------------------------------------------------------------------
        Sanity check
        Extract SAP control block
    ------------------------------------------------------------------------*/
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                "WLANSAP_Stop invoked successfully ");

    pSapCtx = VOS_GET_SAP_CB(pvosGCtx);
    if (NULL == pSapCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pvosGCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }

    sapFreeRoamProfile(&pSapCtx->csrRoamProfile);

    if( !VOS_IS_STATUS_SUCCESS( vos_lock_destroy( &pSapCtx->SapGlobalLock ) ) )
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                 "WLANSAP_Stop failed destroy lock\n");
        return VOS_STATUS_E_FAULT;
    }
    /*------------------------------------------------------------------------
        Stop SAP (de-register RSN handler!?)
    ------------------------------------------------------------------------*/

    return VOS_STATUS_SUCCESS;
}/* WLANSAP_Stop */

/*==========================================================================
  FUNCTION    WLANSAP_Close

  DESCRIPTION
    Called by vos_close during general driver close procedure. SAP will clean up
    all the internal resources.

  DEPENDENCIES

  PARAMETERS

    IN
    pvosGCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT: Pointer to SAP cb is NULL ; access would cause a page
                         fault
    VOS_STATUS_SUCCESS: Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_Close
(
    v_PVOID_t  pvosGCtx
)
{
    ptSapContext  pSapCtx = NULL;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    /*------------------------------------------------------------------------
        Sanity check
        Extract SAP control block
    ------------------------------------------------------------------------*/
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                 "WLANSAP_Close invoked");

    pSapCtx = VOS_GET_SAP_CB(pvosGCtx);
    if (NULL == pSapCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pvosGCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }

    /*------------------------------------------------------------------------
        Cleanup SAP control block.
    ------------------------------------------------------------------------*/
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANSAP_Close");
    WLANSAP_CleanCB(pSapCtx, VOS_TRUE /* empty queues/lists/pkts if any*/);

    /*------------------------------------------------------------------------
        Free SAP context from VOSS global
    ------------------------------------------------------------------------*/
    vos_free_context(pvosGCtx, VOS_MODULE_ID_SAP, pSapCtx);

    return VOS_STATUS_SUCCESS;
}/* WLANSAP_Close */

/*----------------------------------------------------------------------------
 * Utility Function implementations
 * -------------------------------------------------------------------------*/

/*==========================================================================

  FUNCTION    WLANSAP_CleanCB

  DESCRIPTION
    Clear out all fields in the SAP context.

  DEPENDENCIES

  PARAMETERS

    IN
    pvosGCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its context

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT: Pointer to SAP cb is NULL ; access would cause a page
                         fault
    VOS_STATUS_SUCCESS: Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_CleanCB
(
    ptSapContext  pSapCtx,
    v_U32_t freeFlag // 0 /*do not empty*/);
)
{
    /*------------------------------------------------------------------------
        Sanity check SAP control block
    ------------------------------------------------------------------------*/

    if (NULL == pSapCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer", __func__);
        return VOS_STATUS_E_FAULT;
    }

    /*------------------------------------------------------------------------
        Clean up SAP control block, initialize all values
    ------------------------------------------------------------------------*/
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANSAP_CleanCB");

    vos_mem_zero( pSapCtx, sizeof(tSapContext));

    pSapCtx->pvosGCtx = NULL;

    pSapCtx->sapsMachine= eSAP_DISCONNECTED;

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: Initializing State: %d, sapContext value = %p",
            __func__, pSapCtx->sapsMachine, pSapCtx);
    pSapCtx->sessionId = 0;
    pSapCtx->channel = 0;

    return VOS_STATUS_SUCCESS;
}// WLANSAP_CleanCB

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
)
{
    if(HAL_STATUS_SUCCESS(status))
    {
         //If success what else to be handled???
    }
    else
    {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_FATAL,
               "WLANSAP_pmcFullPwrReqCB: PMC failed to put the chip in Full power\n");

    }

}// WLANSAP_pmcFullPwrReqCB
/*==========================================================================
  FUNCTION    WLANSAP_getState

  DESCRIPTION
    This api returns the current SAP state to the caller.

  DEPENDENCIES

  PARAMETERS

    IN
    pContext            : Pointer to Sap Context structure

  RETURN VALUE
    Returns the SAP FSM state.
============================================================================*/

v_U8_t WLANSAP_getState
(
    v_PVOID_t  pvosGCtx
)
{
    ptSapContext  pSapCtx = NULL;

    pSapCtx = VOS_GET_SAP_CB(pvosGCtx);

    if ( NULL == pSapCtx )
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "%s: Invalid SAP pointer from pvosGCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }
    return pSapCtx->sapsMachine;
}

/*==========================================================================
  FUNCTION    WLANSAP_StartBss

  DESCRIPTION
    This api function provides SAP FSM event eWLAN_SAP_PHYSICAL_LINK_CREATE for
    starting AP BSS

  DEPENDENCIES

  PARAMETERS

    IN
    pContext            : Pointer to Sap Context structure
    pQctCommitConfig    : Pointer to configuration structure passed down from HDD(HostApd for Android)
    hdd_SapEventCallback: Callback function in HDD called by SAP to inform HDD about SAP results
    pUsrContext         : Parameter that will be passed back in all the SAP callback events.

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT: Pointer to SAP cb is NULL ; access would cause a page
                         fault
    VOS_STATUS_SUCCESS: Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_StartBss
(
    v_PVOID_t  pvosGCtx,//pwextCtx
    tpWLAN_SAPEventCB pSapEventCallback,
    tsap_Config_t *pConfig,
    v_PVOID_t  pUsrContext
)
{
    tWLAN_SAPEvent sapEvent;    /* State machine event*/
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    ptSapContext  pSapCtx = NULL;
    tANI_BOOLEAN restartNeeded;
    tHalHandle hHal;

    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    /*------------------------------------------------------------------------
        Sanity check
        Extract SAP control block
    ------------------------------------------------------------------------*/
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                 "WLANSAP_StartBss");

    if (VOS_STA_SAP_MODE == vos_get_conparam ())
    {
        pSapCtx = VOS_GET_SAP_CB(pvosGCtx);
        if ( NULL == pSapCtx )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                       "%s: Invalid SAP pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }
        pSapCtx->sapsMachine = eSAP_DISCONNECTED;

        /* Channel selection is auto or configured */
        pSapCtx->channel = pConfig->channel;
        pSapCtx->pUsrContext = pUsrContext;

        //Set the BSSID to your "self MAC Addr" read the mac address from Configuation ITEM received from HDD
        pSapCtx->csrRoamProfile.BSSIDs.numOfBSSIDs = 1;
        vos_mem_copy(pSapCtx->csrRoamProfile.BSSIDs.bssid,
                     pSapCtx->self_mac_addr,
                     sizeof( tCsrBssid ) );

        //Save a copy to SAP context
        vos_mem_copy(pSapCtx->csrRoamProfile.BSSIDs.bssid,
                    pConfig->self_macaddr.bytes, sizeof(v_MACADDR_t));
        vos_mem_copy(pSapCtx->self_mac_addr,
                    pConfig->self_macaddr.bytes, sizeof(v_MACADDR_t));

        //copy the configuration items to csrProfile
        sapconvertToCsrProfile( pConfig, eCSR_BSS_TYPE_INFRA_AP, &pSapCtx->csrRoamProfile);
        hHal = (tHalHandle)VOS_GET_HAL_CB(pvosGCtx);
        if (NULL == hHal)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                       "%s: Invalid MAC context from pvosGCtx", __func__);
        }
        else
        {
            //If concurrent session is running that is already associated
            //then we just follow that sessions country info (whether
            //present or not doesn't maater as we have to follow whatever
            //STA session does)
            if ((0 == sme_GetConcurrentOperationChannel(hHal)) &&
                pConfig->ieee80211d)
            {
                /* Setting the region/country  information */
                sme_setRegInfo(hHal, pConfig->countryCode);
                sme_ResetCountryCodeInformation(hHal, &restartNeeded);
            }
        }

        // Copy MAC filtering settings to sap context
        pSapCtx->eSapMacAddrAclMode = pConfig->SapMacaddr_acl;
        vos_mem_copy(pSapCtx->acceptMacList, pConfig->accept_mac, sizeof(pConfig->accept_mac));
        pSapCtx->nAcceptMac = pConfig->num_accept_mac;
        sapSortMacList(pSapCtx->acceptMacList, pSapCtx->nAcceptMac);
        vos_mem_copy(pSapCtx->denyMacList, pConfig->deny_mac, sizeof(pConfig->deny_mac));
        pSapCtx->nDenyMac = pConfig->num_deny_mac;
        sapSortMacList(pSapCtx->denyMacList, pSapCtx->nDenyMac);

        /* Fill in the event structure for FSM */
        sapEvent.event = eSAP_HDD_START_INFRA_BSS;
        sapEvent.params = 0;//pSapPhysLinkCreate

        /* Store the HDD callback in SAP context */
        pSapCtx->pfnSapEventCallback = pSapEventCallback;

        /* Handle event*/
        vosStatus = sapFsm(pSapCtx, &sapEvent);
     }
     else
     {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                "SoftAp role has not been enabled");
     }

    return vosStatus;
}// WLANSAP_StartBss

/*==========================================================================
  FUNCTION    WLANSAP_SetMacACL

  DESCRIPTION
    This api function provides SAP to set mac list entry in accept list as well
    as deny list

  DEPENDENCIES

  PARAMETERS

    IN
    pContext            : Pointer to Sap Context structure
    pQctCommitConfig    : Pointer to configuration structure passed down from
                          HDD(HostApd for Android)

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT: Pointer to SAP cb is NULL ; access would cause a page
                         fault
    VOS_STATUS_SUCCESS: Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_SetMacACL
(
    v_PVOID_t  pvosGCtx,   //pwextCtx
    tsap_Config_t *pConfig
)
{
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    ptSapContext  pSapCtx = NULL;

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                 "WLANSAP_SetMacACL");

    if (VOS_STA_SAP_MODE == vos_get_conparam ())
    {
        pSapCtx = VOS_GET_SAP_CB(pvosGCtx);
        if ( NULL == pSapCtx )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                       "%s: Invalid SAP pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }

        // Copy MAC filtering settings to sap context
        pSapCtx->eSapMacAddrAclMode = pConfig->SapMacaddr_acl;

        if (eSAP_DENY_UNLESS_ACCEPTED == pSapCtx->eSapMacAddrAclMode)
        {
            vos_mem_copy(pSapCtx->acceptMacList, pConfig->accept_mac,
                                                 sizeof(pConfig->accept_mac));
            pSapCtx->nAcceptMac = pConfig->num_accept_mac;
            sapSortMacList(pSapCtx->acceptMacList, pSapCtx->nAcceptMac);
        }
        else if (eSAP_ACCEPT_UNLESS_DENIED == pSapCtx->eSapMacAddrAclMode)
        {
            vos_mem_copy(pSapCtx->denyMacList, pConfig->deny_mac,
                                               sizeof(pConfig->deny_mac));
            pSapCtx->nDenyMac = pConfig->num_deny_mac;
            sapSortMacList(pSapCtx->denyMacList, pSapCtx->nDenyMac);
        }
    }
    else
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s : SoftAp role has not been enabled", __func__);
        return VOS_STATUS_E_FAULT;
    }

    return vosStatus;
}//WLANSAP_SetMacACL

/*==========================================================================
  FUNCTION    WLANSAP_StopBss

  DESCRIPTION
    This api function provides SAP FSM event eSAP_HDD_STOP_INFRA_BSS for
    stopping AP BSS

  DEPENDENCIES

  PARAMETERS

    IN
    pvosGCtx    : Pointer to the global vos context; a handle to SAP's
                  control block can be extracted from its contexe

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT: Pointer to VOSS GC is NULL ; access would cause a page
                         fault
    VOS_STATUS_SUCCESS: Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_StopBss
(
 v_PVOID_t  pvosGCtx
)
{
    tWLAN_SAPEvent sapEvent;    /* State machine event*/
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    ptSapContext  pSapCtx = NULL;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    /*------------------------------------------------------------------------
        Sanity check
        Extract SAP control block
    ------------------------------------------------------------------------*/
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                 "WLANSAP_StopBss");

    if ( NULL == pvosGCtx )
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid Global VOSS handle", __func__);
        return VOS_STATUS_E_FAULT;
    }

    pSapCtx = VOS_GET_SAP_CB(pvosGCtx);

    if (NULL == pSapCtx )
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pvosGCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }

    /* Fill in the event structure for FSM */
    sapEvent.event = eSAP_HDD_STOP_INFRA_BSS;
    sapEvent.params = 0;

    /* Handle event*/
    vosStatus = sapFsm(pSapCtx, &sapEvent);

    return vosStatus;
}

/*==========================================================================
  FUNCTION    WLANSAP_GetAssocStations

  DESCRIPTION
    This api function is used to probe the list of associated stations from various modules of CORE stack

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    pvosGCtx        : Pointer to vos global context structure
    modId           : Module from whom list of associtated stations  is supposed to be probed. If an invalid module is passed
                        then by default VOS_MODULE_ID_PE will be probed
    IN/OUT
    pAssocStas      : Pointer to list of associated stations that are known to the module specified in mod parameter

  NOTE: The memory for this list will be allocated by the caller of this API

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_GetAssocStations
(
    v_PVOID_t pvosGCtx,
    VOS_MODULE_ID modId,
    tpSap_AssocMacAddr pAssocStas
)
{
    ptSapContext  pSapCtx = VOS_GET_SAP_CB(pvosGCtx);

    /*------------------------------------------------------------------------
      Sanity check
      Extract SAP control block
      ------------------------------------------------------------------------*/
    if (NULL == pSapCtx)
    {
      VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                 "%s: Invalid SAP pointer from pvosGCtx", __func__);
      return VOS_STATUS_E_FAULT;
    }

    sme_RoamGetAssociatedStas( VOS_GET_HAL_CB(pSapCtx->pvosGCtx), pSapCtx->sessionId,
                                modId,
                                pSapCtx->pUsrContext,
                                (v_PVOID_t *)pSapCtx->pfnSapEventCallback,
                                (v_U8_t *)pAssocStas );

    return VOS_STATUS_SUCCESS;
}


/*==========================================================================
  FUNCTION    WLANSAP_RemoveWpsSessionOverlap

  DESCRIPTION
    This api function provides for Ap App/HDD to remove an entry from session session overlap info.

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    pvosGCtx: Pointer to vos global context structure
    pRemoveMac: pointer to v_MACADDR_t for session MAC address

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success
    VOS_STATUS_E_FAULT:  Session is not dectected. The parameter is function not valid.

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_RemoveWpsSessionOverlap

(
    v_PVOID_t pvosGCtx,
    v_MACADDR_t pRemoveMac
)
{
  ptSapContext  pSapCtx = VOS_GET_SAP_CB(pvosGCtx);

  /*------------------------------------------------------------------------
    Sanity check
    Extract SAP control block
  ------------------------------------------------------------------------*/
  if (NULL == pSapCtx)
  {
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
               "%s: Invalid SAP pointer from pvosGCtx", __func__);
    return VOS_STATUS_E_FAULT;
  }

  sme_RoamGetWpsSessionOverlap( VOS_GET_HAL_CB(pSapCtx->pvosGCtx), pSapCtx->sessionId,
                                pSapCtx->pUsrContext,
                                (v_PVOID_t *)pSapCtx->pfnSapEventCallback,
                                pRemoveMac);

  return VOS_STATUS_SUCCESS;
}

/*==========================================================================
  FUNCTION    WLANSAP_getWpsSessionOverlap

  DESCRIPTION
    This api function provides for Ap App/HDD to get WPS session overlap info.

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    pvosGCtx: Pointer to vos global context structure

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_getWpsSessionOverlap
(
 v_PVOID_t pvosGCtx
)
{
    v_MACADDR_t pRemoveMac = VOS_MAC_ADDR_ZERO_INITIALIZER;

    ptSapContext  pSapCtx = VOS_GET_SAP_CB(pvosGCtx);

    /*------------------------------------------------------------------------
      Sanity check
      Extract SAP control block
      ------------------------------------------------------------------------*/
    if (NULL == pSapCtx)
    {
      VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                 "%s: Invalid SAP pointer from pvosGCtx", __func__);
      return VOS_STATUS_E_FAULT;
    }

    sme_RoamGetWpsSessionOverlap( VOS_GET_HAL_CB(pSapCtx->pvosGCtx), pSapCtx->sessionId,
                                pSapCtx->pUsrContext,
                                (v_PVOID_t *)pSapCtx->pfnSapEventCallback,
                                pRemoveMac);

    return VOS_STATUS_SUCCESS;
}


/* This routine will set the mode of operation for ACL dynamically*/
VOS_STATUS
WLANSAP_SetMode ( v_PVOID_t  pvosGCtx, v_U32_t mode)
{
    ptSapContext  pSapCtx = VOS_GET_SAP_CB(pvosGCtx);

    if (NULL == pSapCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pvosGCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }

    pSapCtx->eSapMacAddrAclMode = (eSapMacAddrACL)mode;
    return VOS_STATUS_SUCCESS;
}

/* This routine will clear all the entries in accept list as well as deny list  */

VOS_STATUS
WLANSAP_ClearACL( v_PVOID_t  pvosGCtx)
{
    ptSapContext  pSapCtx = VOS_GET_SAP_CB(pvosGCtx);
    v_U8_t i;

    if (NULL == pSapCtx)
    {
        return VOS_STATUS_E_RESOURCES;
    }

    if (pSapCtx->denyMacList != NULL)
    {
        for (i = 0; i < (pSapCtx->nDenyMac-1); i++)
        {
            vos_mem_zero((pSapCtx->denyMacList+i)->bytes, sizeof(v_MACADDR_t));

        }
    }
    sapPrintACL(pSapCtx->denyMacList, pSapCtx->nDenyMac);
    pSapCtx->nDenyMac  = 0;

    if (pSapCtx->acceptMacList!=NULL)
    {
        for (i = 0; i < (pSapCtx->nAcceptMac-1); i++)
        {
            vos_mem_zero((pSapCtx->acceptMacList+i)->bytes, sizeof(v_MACADDR_t));

        }
    }
    sapPrintACL(pSapCtx->acceptMacList, pSapCtx->nAcceptMac);
    pSapCtx->nAcceptMac = 0;

    return VOS_STATUS_SUCCESS;
}

VOS_STATUS
WLANSAP_ModifyACL
(
    v_PVOID_t  pvosGCtx,
    v_U8_t *pPeerStaMac,
    eSapACLType listType,
    eSapACLCmdType cmd
)
{
    eSapBool staInWhiteList=eSAP_FALSE, staInBlackList=eSAP_FALSE;
    v_U8_t staWLIndex, staBLIndex;
    ptSapContext  pSapCtx = VOS_GET_SAP_CB(pvosGCtx);

    if (NULL == pSapCtx)
    {
       VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                  "%s: Invalid SAP Context", __func__);
       return VOS_STATUS_E_FAULT;
    }

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_LOW,"Modify ACL entered\n"
            "Before modification of ACL\n"
            "size of accept and deny lists %d %d",
            pSapCtx->nAcceptMac, pSapCtx->nDenyMac);
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,"*** WHITE LIST ***");
    sapPrintACL(pSapCtx->acceptMacList, pSapCtx->nAcceptMac);
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,"*** BLACK LIST ***");
    sapPrintACL(pSapCtx->denyMacList, pSapCtx->nDenyMac);

    /* the expectation is a mac addr will not be in both the lists at the same time.
       It is the responsiblity of userspace to ensure this */
    staInWhiteList = sapSearchMacList(pSapCtx->acceptMacList, pSapCtx->nAcceptMac, pPeerStaMac, &staWLIndex);
    staInBlackList = sapSearchMacList(pSapCtx->denyMacList, pSapCtx->nDenyMac, pPeerStaMac, &staBLIndex);

    if (staInWhiteList && staInBlackList)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                "Peer mac "MAC_ADDRESS_STR" found in white and black lists."
                "Initial lists passed incorrect. Cannot execute this command.",
                MAC_ADDR_ARRAY(pPeerStaMac));
        return VOS_STATUS_E_FAILURE;

    }

    switch(listType)
    {
        case eSAP_WHITE_LIST:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_LOW, "cmd %d", cmd);
            if (cmd == ADD_STA_TO_ACL)
            {
                //error check
                // if list is already at max, return failure
                if (pSapCtx->nAcceptMac == MAX_ACL_MAC_ADDRESS)
                {
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                            "White list is already maxed out. Cannot accept "MAC_ADDRESS_STR,
                            MAC_ADDR_ARRAY(pPeerStaMac));
                    return VOS_STATUS_E_FAILURE;
                }
                if (staInWhiteList)
                {
                    //Do nothing if already present in white list. Just print a warning
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_WARN,
                            "MAC address already present in white list "MAC_ADDRESS_STR,
                            MAC_ADDR_ARRAY(pPeerStaMac));
                } else
                {
                    if (staInBlackList)
                    {
                        //remove it from black list before adding to the white list
                        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_WARN,
                                "STA present in black list so first remove from it");
                        sapRemoveMacFromACL(pSapCtx->denyMacList, &pSapCtx->nDenyMac, staBLIndex);
                    }
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                            "... Now add to the white list");
                    sapAddMacToACL(pSapCtx->acceptMacList, &pSapCtx->nAcceptMac, pPeerStaMac);
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_LOW, "size of accept and deny lists %d %d",
                            pSapCtx->nAcceptMac, pSapCtx->nDenyMac);
                }
            }
            else if (cmd == DELETE_STA_FROM_ACL)
            {
                if (staInWhiteList)
                {
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO, "Delete from white list");
                    sapRemoveMacFromACL(pSapCtx->acceptMacList, &pSapCtx->nAcceptMac, staWLIndex);
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_LOW, "size of accept and deny lists %d %d",
                            pSapCtx->nAcceptMac, pSapCtx->nDenyMac);
                }
                else
                {
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_WARN,
                            "MAC address to be deleted is not present in the white list "MAC_ADDRESS_STR,
                            MAC_ADDR_ARRAY(pPeerStaMac));
                    return VOS_STATUS_E_FAILURE;
                }
            }
            else
            {
                VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR, "Invalid cmd type passed");
                return VOS_STATUS_E_FAILURE;
            }
            break;

        case eSAP_BLACK_LIST:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_LOW,
                    "cmd %d", cmd);
            if (cmd == ADD_STA_TO_ACL)
            {
                //error check
                // if list is already at max, return failure
                if (pSapCtx->nDenyMac == MAX_ACL_MAC_ADDRESS)
                {
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                            "Black list is already maxed out. Cannot accept "MAC_ADDRESS_STR,
                            MAC_ADDR_ARRAY(pPeerStaMac));
                    return VOS_STATUS_E_FAILURE;
                }
                if (staInBlackList)
                {
                    //Do nothing if already present in white list
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_WARN,
                            "MAC address already present in black list "MAC_ADDRESS_STR,
                            MAC_ADDR_ARRAY(pPeerStaMac));
                } else
                {
                    if (staInWhiteList)
                    {
                        //remove it from white list before adding to the white list
                        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_WARN,
                                "Present in white list so first remove from it");
                        sapRemoveMacFromACL(pSapCtx->acceptMacList, &pSapCtx->nAcceptMac, staWLIndex);
                    }
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                            "... Now add to black list");
                    sapAddMacToACL(pSapCtx->denyMacList, &pSapCtx->nDenyMac, pPeerStaMac);
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_LOW,"size of accept and deny lists %d %d",
                            pSapCtx->nAcceptMac, pSapCtx->nDenyMac);
                }
            }
            else if (cmd == DELETE_STA_FROM_ACL)
            {
                if (staInBlackList)
                {
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO, "Delete from black list");
                    sapRemoveMacFromACL(pSapCtx->denyMacList, &pSapCtx->nDenyMac, staBLIndex);
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_LOW,"no accept and deny mac %d %d",
                            pSapCtx->nAcceptMac, pSapCtx->nDenyMac);
                }
                else
                {
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_WARN,
                            "MAC address to be deleted is not present in the black list "MAC_ADDRESS_STR,
                            MAC_ADDR_ARRAY(pPeerStaMac));
                    return VOS_STATUS_E_FAILURE;
                }
            }
            else
            {
                VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR, "Invalid cmd type passed");
                return VOS_STATUS_E_FAILURE;
            }
            break;

        default:
            {
                VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                        "Invalid list type passed %d",listType);
                return VOS_STATUS_E_FAILURE;
            }
    }
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_LOW,"After modification of ACL");
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,"*** WHITE LIST ***");
    sapPrintACL(pSapCtx->acceptMacList, pSapCtx->nAcceptMac);
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,"*** BLACK LIST ***");
    sapPrintACL(pSapCtx->denyMacList, pSapCtx->nDenyMac);
    return VOS_STATUS_SUCCESS;
}

/*==========================================================================
  FUNCTION    WLANSAP_DisassocSta

  DESCRIPTION
    This api function provides for Ap App/HDD initiated disassociation of station

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    pvosGCtx            : Pointer to vos global context structure
    pPeerStaMac         : Mac address of the station to disassociate

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_DisassocSta
(
    v_PVOID_t  pvosGCtx,
    v_U8_t *pPeerStaMac
)
{
    ptSapContext  pSapCtx = VOS_GET_SAP_CB(pvosGCtx);

    /*------------------------------------------------------------------------
      Sanity check
      Extract SAP control block
      ------------------------------------------------------------------------*/
    if (NULL == pSapCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pvosGCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }

    sme_RoamDisconnectSta(VOS_GET_HAL_CB(pSapCtx->pvosGCtx), pSapCtx->sessionId,
                            pPeerStaMac);

    return VOS_STATUS_SUCCESS;
}

/*==========================================================================
  FUNCTION    WLANSAP_DeauthSta

  DESCRIPTION
    This api function provides for Ap App/HDD initiated deauthentication of station

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    pvosGCtx            : Pointer to vos global context structure
    pPeerStaMac         : Mac address of the station to deauthenticate

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_DeauthSta
(
    v_PVOID_t  pvosGCtx,
    v_U8_t *pPeerStaMac
)
{
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    VOS_STATUS vosStatus = VOS_STATUS_E_FAULT;
    ptSapContext  pSapCtx = VOS_GET_SAP_CB(pvosGCtx);

    /*------------------------------------------------------------------------
      Sanity check
      Extract SAP control block
      ------------------------------------------------------------------------*/
    if (NULL == pSapCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pvosGCtx", __func__);
        return vosStatus;
    }

    halStatus = sme_RoamDeauthSta(VOS_GET_HAL_CB(pSapCtx->pvosGCtx), pSapCtx->sessionId,
                            pPeerStaMac);

    if (halStatus == eHAL_STATUS_SUCCESS)
    {
        vosStatus = VOS_STATUS_SUCCESS;
    }
    return vosStatus;
}
/*==========================================================================
  FUNCTION    WLANSAP_SetChannelRange

  DESCRIPTION
    This api function sets the range of channels for AP.

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    startChannel         : start channel
    endChannel           : End channel
    operatingBand        : Operating band (2.4GHz/5GHz)

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_SetChannelRange(tHalHandle hHal,v_U8_t startChannel, v_U8_t endChannel,
                              v_U8_t operatingBand)
{

    v_U8_t    validChannelFlag =0;
    v_U8_t    loopStartCount =0;
    v_U8_t    loopEndCount =0;
    v_U8_t    bandStartChannel =0;
    v_U8_t    bandEndChannel =0;

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
         "WLANSAP_SetChannelRange:startChannel %d,EndChannel %d,Operatingband:%d",
         startChannel,endChannel,operatingBand);

    /*------------------------------------------------------------------------
      Sanity check
      ------------------------------------------------------------------------*/
    if (( WNI_CFG_SAP_CHANNEL_SELECT_OPERATING_BAND_STAMIN > operatingBand) ||
          (WNI_CFG_SAP_CHANNEL_SELECT_OPERATING_BAND_STAMAX < operatingBand))
    {
         VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                     "Invalid operatingBand on WLANSAP_SetChannelRange");
        return VOS_STATUS_E_FAULT;
    }
    if (( WNI_CFG_SAP_CHANNEL_SELECT_START_CHANNEL_STAMIN > startChannel) ||
         (WNI_CFG_SAP_CHANNEL_SELECT_START_CHANNEL_STAMAX < startChannel))
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                    "Invalid startChannel value on WLANSAP_SetChannelRange");
        return VOS_STATUS_E_FAULT;
    }
    if (( WNI_CFG_SAP_CHANNEL_SELECT_END_CHANNEL_STAMIN > endChannel) ||
         (WNI_CFG_SAP_CHANNEL_SELECT_END_CHANNEL_STAMAX < endChannel))
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                      "Invalid endChannel value on WLANSAP_SetChannelRange");
        return VOS_STATUS_E_FAULT;
    }
    switch(operatingBand)
    {
       case RF_SUBBAND_2_4_GHZ:
          bandStartChannel = RF_CHAN_1;
          bandEndChannel = RF_CHAN_14;
          break;

       case RF_SUBBAND_5_LOW_GHZ:
          bandStartChannel = RF_CHAN_36;
          bandEndChannel = RF_CHAN_64;
          break;

       case RF_SUBBAND_5_MID_GHZ:
          bandStartChannel = RF_CHAN_100;
#ifndef FEATURE_WLAN_CH144
          bandEndChannel = RF_CHAN_140;
#else
          bandEndChannel = RF_CHAN_144;
#endif /* FEATURE_WLAN_CH144 */
          break;

       case RF_SUBBAND_5_HIGH_GHZ:
          bandStartChannel = RF_CHAN_149;
          bandEndChannel = RF_CHAN_165;
          break;

       default:
          VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "Invalid operatingBand value on WLANSAP_SetChannelRange");
          break;
    }

    /* Validating the start channel is in range or not*/
    for(loopStartCount = bandStartChannel ; loopStartCount <= bandEndChannel ;
    loopStartCount++)
    {
       if(rfChannels[loopStartCount].channelNum == startChannel )
       {
          /* start channel is in the range */
          break;
       }
    }
    /* Validating the End channel is in range or not*/
    for(loopEndCount = bandStartChannel ; loopEndCount <= bandEndChannel ;
    loopEndCount++)
    {
        if(rfChannels[loopEndCount].channelNum == endChannel )
        {
          /* End channel is in the range */
            break;
        }
    }
    if((loopStartCount > bandEndChannel)||(loopEndCount > bandEndChannel))
    {
       VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                  "%s: Invalid startChannel-%d or EndChannel-%d for band -%d",
                   __func__,startChannel,endChannel,operatingBand);
       /* Supplied channels are nt in the operating band so set the default
            channels for the given operating band */
       startChannel = rfChannels[bandStartChannel].channelNum;
       endChannel = rfChannels[bandEndChannel].channelNum;
    }

    /*Search for the Active channels in the given range */
    for( loopStartCount = bandStartChannel; loopStartCount <= bandEndChannel; loopStartCount++ )
    {
       if((startChannel <= rfChannels[loopStartCount].channelNum)&&
             (endChannel >= rfChannels[loopStartCount].channelNum ))
       {
          if( regChannels[loopStartCount].enabled )
          {
             validChannelFlag = 1;
             break;
          }
       }
    }
    if(0 == validChannelFlag)
    {
       VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
         "%s-No active channels present in the given range for the current region",
         __func__);
       /* There is no active channel in the supplied range.Updating the config
       with the default channels in the given band so that we can select the best channel in the sub-band*/
       startChannel = rfChannels[bandStartChannel].channelNum;
       endChannel = rfChannels[bandEndChannel].channelNum;
    }

    if (ccmCfgSetInt(hHal, WNI_CFG_SAP_CHANNEL_SELECT_OPERATING_BAND,
       operatingBand, NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {
         VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
          "Could not pass on WNI_CFG_SAP_CHANNEL_SELECT_OPERATING_BAND to CCn");
         return VOS_STATUS_E_FAULT;
    }
    if (ccmCfgSetInt(hHal, WNI_CFG_SAP_CHANNEL_SELECT_START_CHANNEL,
        startChannel, NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {

       VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
          "Could not pass on WNI_CFG_SAP_CHANNEL_SELECT_START_CHANNEL to CCM");
       return VOS_STATUS_E_FAULT;

    }
    if (ccmCfgSetInt(hHal, WNI_CFG_SAP_CHANNEL_SELECT_END_CHANNEL,
       endChannel, NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {

       VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
          "Could not pass on WNI_CFG_SAP_CHANNEL_SELECT_START_CHANNEL to CCM");
       return VOS_STATUS_E_FAULT;
    }
    return VOS_STATUS_SUCCESS;
}

/*==========================================================================
  FUNCTION    WLANSAP_SetCounterMeasure

  DESCRIPTION
    This api function is used to disassociate all the stations and prevent
    association for any other station.Whenever Authenticator receives 2 mic failures
    within 60 seconds, Authenticator will enable counter measure at SAP Layer.
    Authenticator will start the 60 seconds timer. Core stack will not allow any
    STA to associate till HDD disables counter meassure. Core stack shall kick out all the
    STA which are currently associated and DIASSOC Event will be propogated to HDD for
    each STA to clean up the HDD STA table.Once the 60 seconds timer expires, Authenticator
    will disable the counter meassure at core stack. Now core stack can allow STAs to associate.

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
pvosGCtx: Pointer to vos global context structure
bEnable: If TRUE than all stations will be disassociated and no more will be allowed to associate. If FALSE than CORE
will come out of this state.

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_SetCounterMeasure
(
    v_PVOID_t pvosGCtx,
    v_BOOL_t bEnable
)
{
    ptSapContext  pSapCtx = VOS_GET_SAP_CB(pvosGCtx);

    /*------------------------------------------------------------------------
      Sanity check
      Extract SAP control block
      ------------------------------------------------------------------------*/
    if (NULL == pSapCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pvosGCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }

    sme_RoamTKIPCounterMeasures(VOS_GET_HAL_CB(pSapCtx->pvosGCtx), pSapCtx->sessionId, bEnable);

    return VOS_STATUS_SUCCESS;
}

/*==========================================================================

  FUNCTION    WLANSAP_SetKeysSta

  DESCRIPTION
    This api function provides for Ap App/HDD to set key for a station.

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
pvosGCtx: Pointer to vos global context structure
pSetKeyInfo: tCsrRoamSetKey structure for the station

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_SetKeySta
(
    v_PVOID_t pvosGCtx, tCsrRoamSetKey *pSetKeyInfo
)
{
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    ptSapContext  pSapCtx = NULL;
    v_PVOID_t hHal = NULL;
        eHalStatus halStatus = eHAL_STATUS_FAILURE;
        v_U32_t roamId=0xFF;

    if (VOS_STA_SAP_MODE == vos_get_conparam ( ))
    {
        pSapCtx = VOS_GET_SAP_CB(pvosGCtx);
        if (NULL == pSapCtx)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }
        hHal = VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if (NULL == hHal)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid HAL pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }
        halStatus = sme_RoamSetKey(hHal, pSapCtx->sessionId, pSetKeyInfo, &roamId);

        if (halStatus == eHAL_STATUS_SUCCESS)
        {
            vosStatus = VOS_STATUS_SUCCESS;
        } else
        {
            vosStatus = VOS_STATUS_E_FAULT;
        }
    }
    else
        vosStatus = VOS_STATUS_E_FAULT;

    return vosStatus;
}

/*==========================================================================
  FUNCTION    WLANSAP_DelKeySta

  DESCRIPTION
    This api function provides for Ap App/HDD to delete key for a station.

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
pvosGCtx: Pointer to vos global context structure
pSetKeyInfo: tCsrRoamRemoveKey structure for the station

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_DelKeySta
(
     v_PVOID_t pvosGCtx,
    tCsrRoamRemoveKey *pRemoveKeyInfo
)
{
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    ptSapContext  pSapCtx = NULL;
    v_PVOID_t hHal = NULL;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    v_U32_t roamId=0xFF;
    tCsrRoamRemoveKey RemoveKeyInfo;

    if (VOS_STA_SAP_MODE == vos_get_conparam ( ))
    {
        pSapCtx = VOS_GET_SAP_CB(pvosGCtx);
        if (NULL == pSapCtx)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }

        hHal = VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if (NULL == hHal)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid HAL pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }

        vos_mem_zero(&RemoveKeyInfo, sizeof(RemoveKeyInfo));
        RemoveKeyInfo.encType = pRemoveKeyInfo->encType;
        vos_mem_copy(RemoveKeyInfo.peerMac, pRemoveKeyInfo->peerMac, WNI_CFG_BSSID_LEN);
        RemoveKeyInfo.keyId = pRemoveKeyInfo->keyId;

        halStatus = sme_RoamRemoveKey(hHal, pSapCtx->sessionId, &RemoveKeyInfo, &roamId);

        if (HAL_STATUS_SUCCESS(halStatus))
        {
            vosStatus = VOS_STATUS_SUCCESS;
        }
        else
        {
            vosStatus = VOS_STATUS_E_FAULT;
        }
    }
    else
        vosStatus = VOS_STATUS_E_FAULT;

    return vosStatus;
}

VOS_STATUS
WLANSap_getstationIE_information(v_PVOID_t pvosGCtx,
                                 v_U32_t   *pLen,
                                 v_U8_t    *pBuf)
{
    VOS_STATUS vosStatus = VOS_STATUS_E_FAILURE;
    ptSapContext  pSapCtx = NULL;
    v_U32_t len = 0;

    if (VOS_STA_SAP_MODE == vos_get_conparam ( )){
        pSapCtx = VOS_GET_SAP_CB(pvosGCtx);
        if (NULL == pSapCtx)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }
        if (pLen)
        {
            len = *pLen;
            *pLen = pSapCtx->nStaWPARSnReqIeLength;
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                       "%s: WPAIE len : %x", __func__, *pLen);
            if(pBuf)
            {
                if(len >= pSapCtx->nStaWPARSnReqIeLength)
                {
                    vos_mem_copy( pBuf, pSapCtx->pStaWpaRsnReqIE, pSapCtx->nStaWPARSnReqIeLength);
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                               "%s: WPAIE: %02x:%02x:%02x:%02x:%02x:%02x",
                               __func__,
                               pBuf[0], pBuf[1], pBuf[2],
                               pBuf[3], pBuf[4], pBuf[5]);
                    vosStatus = VOS_STATUS_SUCCESS;
                }
            }
        }
    }

    if( VOS_STATUS_E_FAILURE == vosStatus)
    {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                  "%s: Error unable to populate the RSNWPAIE",
                  __func__);
    }

    return vosStatus;

}

/*==========================================================================
  FUNCTION    WLANSAP_Set_WpsIe

  DESCRIPTION
    This api function provides for Ap App/HDD to set WPS IE.

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
pvosGCtx: Pointer to vos global context structure
pWPSIE:  tSap_WPSIE structure that include WPS IEs

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_Set_WpsIe
(
 v_PVOID_t pvosGCtx, tSap_WPSIE *pSap_WPSIe
)
{
    ptSapContext  pSapCtx = NULL;
    v_PVOID_t hHal = NULL;

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
            "%s, %d", __func__, __LINE__);

    if(VOS_STA_SAP_MODE == vos_get_conparam ( )) {
        pSapCtx = VOS_GET_SAP_CB(pvosGCtx);
        if ( NULL == pSapCtx )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }

        hHal = VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if ( NULL == hHal ){
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid HAL pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }

        if ( sap_AcquireGlobalLock( pSapCtx ) == VOS_STATUS_SUCCESS )
        {
            if (pSap_WPSIe->sapWPSIECode == eSAP_WPS_BEACON_IE)
            {
                vos_mem_copy(&pSapCtx->APWPSIEs.SirWPSBeaconIE, &pSap_WPSIe->sapwpsie.sapWPSBeaconIE, sizeof(tSap_WPSBeaconIE));
            }
            else if (pSap_WPSIe->sapWPSIECode == eSAP_WPS_PROBE_RSP_IE)
            {
                vos_mem_copy(&pSapCtx->APWPSIEs.SirWPSProbeRspIE, &pSap_WPSIe->sapwpsie.sapWPSProbeRspIE, sizeof(tSap_WPSProbeRspIE));
            }
            else
            {
                sap_ReleaseGlobalLock( pSapCtx );
                return VOS_STATUS_E_FAULT;
            }
            sap_ReleaseGlobalLock( pSapCtx );
            return VOS_STATUS_SUCCESS;
        }
        else
            return VOS_STATUS_E_FAULT;
    }
    else
        return VOS_STATUS_E_FAULT;
}

/*==========================================================================
  FUNCTION   WLANSAP_Update_WpsIe

  DESCRIPTION
    This api function provides for Ap App/HDD to update WPS IEs.

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
pvosGCtx: Pointer to vos global context structure

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_Update_WpsIe
(
 v_PVOID_t pvosGCtx
)
{
    VOS_STATUS vosStatus = VOS_STATUS_E_FAULT;
    ptSapContext  pSapCtx = NULL;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    v_PVOID_t hHal = NULL;

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
            "%s, %d", __func__, __LINE__);

    if(VOS_STA_SAP_MODE == vos_get_conparam ( )){
        pSapCtx = VOS_GET_SAP_CB(pvosGCtx);
        if ( NULL == pSapCtx )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }

        hHal = VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if ( NULL == hHal ){
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid HAL pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }

        halStatus = sme_RoamUpdateAPWPSIE( hHal, pSapCtx->sessionId, &pSapCtx->APWPSIEs);

        if(halStatus == eHAL_STATUS_SUCCESS) {
            vosStatus = VOS_STATUS_SUCCESS;
        } else
        {
            vosStatus = VOS_STATUS_E_FAULT;
        }

    }

    return vosStatus;
}

/*==========================================================================
  FUNCTION    WLANSAP_Get_WPS_State

  DESCRIPTION
    This api function provides for Ap App/HDD to check if WPS session in process.

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
pvosGCtx: Pointer to vos global context structure

    OUT
pbWPSState: Pointer to variable to indicate if it is in WPS Registration state

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS
WLANSAP_Get_WPS_State
(
 v_PVOID_t pvosGCtx, v_BOOL_t *bWPSState
)
{
    ptSapContext  pSapCtx = NULL;
    v_PVOID_t hHal = NULL;

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
        "%s, %d", __func__, __LINE__);

    if(VOS_STA_SAP_MODE == vos_get_conparam ( )){

        pSapCtx = VOS_GET_SAP_CB(pvosGCtx);
        if ( NULL == pSapCtx )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pvosGCtx", __func__);
             return VOS_STATUS_E_FAULT;
        }

        hHal = VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if ( NULL == hHal ){
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid HAL pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }

        if ( sap_AcquireGlobalLock(pSapCtx ) == VOS_STATUS_SUCCESS )
        {
            if(pSapCtx->APWPSIEs.SirWPSProbeRspIE.FieldPresent & SIR_WPS_PROBRSP_SELECTEDREGISTRA_PRESENT)
                *bWPSState = eANI_BOOLEAN_TRUE;
            else
                *bWPSState = eANI_BOOLEAN_FALSE;

            sap_ReleaseGlobalLock( pSapCtx  );

            return VOS_STATUS_SUCCESS;
        }
        else
            return VOS_STATUS_E_FAULT;
    }
    else
        return VOS_STATUS_E_FAULT;

}

VOS_STATUS
sap_AcquireGlobalLock
(
    ptSapContext  pSapCtx
)
{
    VOS_STATUS vosStatus = VOS_STATUS_E_FAULT;

    if( VOS_IS_STATUS_SUCCESS( vos_lock_acquire( &pSapCtx->SapGlobalLock) ) )
    {
            vosStatus = VOS_STATUS_SUCCESS;
    }

    return (vosStatus);
}

VOS_STATUS
sap_ReleaseGlobalLock
(
    ptSapContext  pSapCtx
)
{
    VOS_STATUS vosStatus = VOS_STATUS_E_FAULT;

    if( VOS_IS_STATUS_SUCCESS( vos_lock_release( &pSapCtx->SapGlobalLock) ) )
    {
        vosStatus = VOS_STATUS_SUCCESS;
    }

    return (vosStatus);
}

/*==========================================================================
  FUNCTION    WLANSAP_Set_WPARSNIes

  DESCRIPTION
    This api function provides for Ap App/HDD to set AP WPA and RSN IE in its beacon and probe response.

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
        pvosGCtx: Pointer to vos global context structure
        pWPARSNIEs: buffer to the WPA/RSN IEs
        WPARSNIEsLen: length of WPA/RSN IEs

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS WLANSAP_Set_WPARSNIes(v_PVOID_t pvosGCtx, v_U8_t *pWPARSNIEs, v_U32_t WPARSNIEsLen)
{

    ptSapContext  pSapCtx = NULL;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    v_PVOID_t hHal = NULL;

    if(VOS_STA_SAP_MODE == vos_get_conparam ( )){
        pSapCtx = VOS_GET_SAP_CB(pvosGCtx);
        if ( NULL == pSapCtx )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }

        hHal = VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if ( NULL == hHal ){
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid HAL pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }

        pSapCtx->APWPARSNIEs.length = (tANI_U16)WPARSNIEsLen;
        vos_mem_copy(pSapCtx->APWPARSNIEs.rsnIEdata, pWPARSNIEs, WPARSNIEsLen);

        halStatus = sme_RoamUpdateAPWPARSNIEs( hHal, pSapCtx->sessionId, &pSapCtx->APWPARSNIEs);

        if(halStatus == eHAL_STATUS_SUCCESS) {
            return VOS_STATUS_SUCCESS;
        } else
        {
            return VOS_STATUS_E_FAULT;
        }
    }

    return VOS_STATUS_E_FAULT;
}

VOS_STATUS WLANSAP_GetStatistics(v_PVOID_t pvosGCtx, tSap_SoftapStats *statBuf, v_BOOL_t bReset)
{
    if (NULL == pvosGCtx)
    {
        return VOS_STATUS_E_FAULT;
    }

    return (WLANTL_GetSoftAPStatistics(pvosGCtx, statBuf, bReset));
}

/*==========================================================================

  FUNCTION    WLANSAP_SendAction

  DESCRIPTION
    This api function provides to send action frame sent by upper layer.

  DEPENDENCIES
    NA.

  PARAMETERS

  IN
    pvosGCtx: Pointer to vos global context structure
    pBuf: Pointer of the action frame to be transmitted
    len: Length of the action frame

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS WLANSAP_SendAction( v_PVOID_t pvosGCtx, const tANI_U8 *pBuf,
                               tANI_U32 len, tANI_U16 wait )
{
    ptSapContext  pSapCtx = NULL;
    v_PVOID_t hHal = NULL;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;

    if( VOS_STA_SAP_MODE == vos_get_conparam ( ) )
    {
        pSapCtx = VOS_GET_SAP_CB( pvosGCtx );
        if (NULL == pSapCtx)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }
        hHal = VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if( ( NULL == hHal ) || ( eSAP_TRUE != pSapCtx->isSapSessionOpen ) )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: HAL pointer (%p) NULL OR SME session is not open (%d)",
                       __func__, hHal, pSapCtx->isSapSessionOpen );
            return VOS_STATUS_E_FAULT;
        }

        halStatus = sme_sendAction( hHal, pSapCtx->sessionId, pBuf, len, 0 , 0);

        if ( eHAL_STATUS_SUCCESS == halStatus )
        {
            return VOS_STATUS_SUCCESS;
        }
    }

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
               "Failed to Send Action Frame");

    return VOS_STATUS_E_FAULT;
}

/*==========================================================================

  FUNCTION    WLANSAP_RemainOnChannel

  DESCRIPTION
    This api function provides to set Remain On channel on specified channel
    for specified duration.

  DEPENDENCIES
    NA.

  PARAMETERS

  IN
    pvosGCtx: Pointer to vos global context structure
    channel: Channel on which driver has to listen
    duration: Duration for which driver has to listen on specified channel
    callback: Callback function to be called once Listen is done.
    pContext: Context needs to be called in callback function.

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS WLANSAP_RemainOnChannel( v_PVOID_t pvosGCtx,
                                    tANI_U8 channel, tANI_U32 duration,
                                    remainOnChanCallback callback,
                                    void *pContext )
{
    ptSapContext  pSapCtx = NULL;
    v_PVOID_t hHal = NULL;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;

    if( VOS_STA_SAP_MODE == vos_get_conparam ( ) )
    {
        pSapCtx = VOS_GET_SAP_CB( pvosGCtx );
        if (NULL == pSapCtx)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }
        hHal = VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if( ( NULL == hHal ) || ( eSAP_TRUE != pSapCtx->isSapSessionOpen ) )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: HAL pointer (%p) NULL OR SME session is not open (%d)",
                       __func__, hHal, pSapCtx->isSapSessionOpen );
            return VOS_STATUS_E_FAULT;
        }

        halStatus = sme_RemainOnChannel( hHal, pSapCtx->sessionId,
                          channel, duration, callback, pContext, TRUE );

        if( eHAL_STATUS_SUCCESS == halStatus )
        {
            return VOS_STATUS_SUCCESS;
        }
    }

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
               "Failed to Set Remain on Channel");

    return VOS_STATUS_E_FAULT;
}

/*==========================================================================

  FUNCTION    WLANSAP_CancelRemainOnChannel

  DESCRIPTION
    This api cancel previous remain on channel request.

  DEPENDENCIES
    NA.

  PARAMETERS

  IN
    pvosGCtx: Pointer to vos global context structure

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS WLANSAP_CancelRemainOnChannel( v_PVOID_t pvosGCtx )
{
    ptSapContext  pSapCtx = NULL;
    v_PVOID_t hHal = NULL;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;

    if( VOS_STA_SAP_MODE == vos_get_conparam ( ) )
    {
        pSapCtx = VOS_GET_SAP_CB( pvosGCtx );
        if (NULL == pSapCtx)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }
        hHal = VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if( ( NULL == hHal ) || ( eSAP_TRUE != pSapCtx->isSapSessionOpen ) )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: HAL pointer (%p) NULL OR SME session is not open (%d)",
                       __func__, hHal, pSapCtx->isSapSessionOpen );
            return VOS_STATUS_E_FAULT;
        }

        halStatus = sme_CancelRemainOnChannel( hHal, pSapCtx->sessionId );

        if( eHAL_STATUS_SUCCESS == halStatus )
        {
            return VOS_STATUS_SUCCESS;
        }
    }

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                    "Failed to Cancel Remain on Channel");

    return VOS_STATUS_E_FAULT;
}

/*==========================================================================

  FUNCTION    WLANSAP_RegisterMgmtFrame

  DESCRIPTION
    HDD use this API to register specified type of frame with CORE stack.
    On receiving such kind of frame CORE stack should pass this frame to HDD

  DEPENDENCIES
    NA.

  PARAMETERS

  IN
    pvosGCtx: Pointer to vos global context structure
    frameType: frameType that needs to be registered with PE.
    matchData: Data pointer which should be matched after frame type is matched.
    matchLen: Length of the matchData

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS WLANSAP_RegisterMgmtFrame( v_PVOID_t pvosGCtx, tANI_U16 frameType,
                                      tANI_U8* matchData, tANI_U16 matchLen )
{
    ptSapContext  pSapCtx = NULL;
    v_PVOID_t hHal = NULL;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;

    if( VOS_STA_SAP_MODE == vos_get_conparam ( ) )
    {
        pSapCtx = VOS_GET_SAP_CB( pvosGCtx );
        if (NULL == pSapCtx)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }
        hHal = VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if( ( NULL == hHal ) || ( eSAP_TRUE != pSapCtx->isSapSessionOpen ) )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: HAL pointer (%p) NULL OR SME session is not open (%d)",
                       __func__, hHal, pSapCtx->isSapSessionOpen );
            return VOS_STATUS_E_FAULT;
        }

        halStatus = sme_RegisterMgmtFrame(hHal, pSapCtx->sessionId,
                          frameType, matchData, matchLen);

        if( eHAL_STATUS_SUCCESS == halStatus )
        {
            return VOS_STATUS_SUCCESS;
        }
    }

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                    "Failed to Register MGMT frame");

    return VOS_STATUS_E_FAULT;
}

/*==========================================================================

  FUNCTION    WLANSAP_DeRegisterMgmtFrame

  DESCRIPTION
   This API is used to deregister previously registered frame.

  DEPENDENCIES
    NA.

  PARAMETERS

  IN
    pvosGCtx: Pointer to vos global context structure
    frameType: frameType that needs to be De-registered with PE.
    matchData: Data pointer which should be matched after frame type is matched.
    matchLen: Length of the matchData

  RETURN VALUE
    The VOS_STATUS code associated with performing the operation

    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS
============================================================================*/
VOS_STATUS WLANSAP_DeRegisterMgmtFrame( v_PVOID_t pvosGCtx, tANI_U16 frameType,
                                      tANI_U8* matchData, tANI_U16 matchLen )
{
    ptSapContext  pSapCtx = NULL;
    v_PVOID_t hHal = NULL;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;

    if( VOS_STA_SAP_MODE == vos_get_conparam ( ) )
    {
        pSapCtx = VOS_GET_SAP_CB( pvosGCtx );
        if (NULL == pSapCtx)
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }
        hHal = VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if( ( NULL == hHal ) || ( eSAP_TRUE != pSapCtx->isSapSessionOpen ) )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: HAL pointer (%p) NULL OR SME session is not open (%d)",
                       __func__, hHal, pSapCtx->isSapSessionOpen );
            return VOS_STATUS_E_FAULT;
        }

        halStatus = sme_DeregisterMgmtFrame( hHal, pSapCtx->sessionId,
                          frameType, matchData, matchLen );

        if( eHAL_STATUS_SUCCESS == halStatus )
        {
            return VOS_STATUS_SUCCESS;
        }
    }

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                    "Failed to Deregister MGMT frame");

    return VOS_STATUS_E_FAULT;
}
