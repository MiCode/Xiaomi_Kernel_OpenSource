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

                      b a p M o d u l e . C

  OVERVIEW:

  This software unit holds the implementation of the WLAN BAP modules
  Module support functions. It is also where the global BAP module
  context, and per-instance (returned in BAP_Open device open) contexts.

  The functions externalized by this module are to be called by the device
  specific BAP Shim Layer (BSL) (in HDD) which implements a stream device on a
  particular platform.

  DEPENDENCIES:

  Are listed for each API below.

===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


   $Header: /home/labuser/ampBlueZ_2/CORE/BAP/src/bapModule.c,v 1.1 2010/07/12 19:05:35 labuser Exp labuser $$DateTime$$Author: labuser $


  when        who     what, where, why
----------    ---    --------------------------------------------------------
2008-09-15    jez     Created module

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
// Pull in some message types used by BTC
#include "sirParams.h"
//#include "halFwApi.h"
 
#include "wlan_qct_tl.h"
#include "vos_trace.h"
// Pick up the sme callback registration API
#include "sme_Api.h"
#include "ccmApi.h"

/* BT-AMP PAL API header file */ 
#include "bapApi.h" 
#include "bapInternal.h" 

// Pick up the BTAMP RSN definitions
#include "bapRsnTxRx.h"
//#include "assert.h" 
#include "bapApiTimer.h"

#if defined(ANI_OS_TYPE_ANDROID)
#include "bap_hdd_main.h"
#endif

//#define BAP_DEBUG
/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
//#define VOS_GET_BAP_CB(ctx) vos_get_context( VOS_MODULE_ID_BAP, ctx) 


/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/
//  include the phy link state machine structure here
static tWLAN_BAPbapPhysLinkMachine bapPhysLinkMachineInitial 
    = BTAMPFSM_INSTANCEDATA_INIT;

/*----------------------------------------------------------------------------
 *  External declarations for global context 
 * -------------------------------------------------------------------------*/
//  No!  Get this from VOS.
//  The main per-Physical Link (per WLAN association) context.
//tBtampContext btampCtx;
ptBtampContext  gpBtampCtx; 

//  Include the Local AMP Info structure.
tBtampHCI_AMP_Info        btampHCI_AMP_Info;
//  Include the Local Data Block Size info structure.
tBtampHCI_Data_Block_Size btampHCI_Data_Block_Size;
//  Include the Local Version info structure.
tBtampHCI_Version_Info   btampHCI_Version_Info;
//  Include the Local Supported Cmds info structure.
tBtampHCI_Supported_Cmds  btampHCI_Supported_Cmds;

static unsigned char pBtStaOwnMacAddr[WNI_CFG_BSSID_LEN];

 /*BT-AMP SSID; per spec should have this format: "AMP-00-0a-f5-04-05-08" */
#define WLAN_BAP_SSID_MAX_LEN 21 
static char pBtStaOwnSsid[WLAN_BAP_SSID_MAX_LEN];

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

  FUNCTION    WLANBAP_Open

  DESCRIPTION 
    Called at driver initialization (vos_open). BAP will initialize 
    all its internal resources and will wait for the call to start to 
    register with the other modules. 
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to BAP's 
                    control block can be extracted from its context 
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to BAP cb is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_Open
( 
  v_PVOID_t  pvosGCtx 
)
{
  ptBtampContext  pBtampCtx = NULL; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Allocate (and sanity check?!) BAP control block 
   ------------------------------------------------------------------------*/
  vos_alloc_context(pvosGCtx, VOS_MODULE_ID_BAP, (v_VOID_t**)&pBtampCtx, sizeof(tBtampContext));

  pBtampCtx = VOS_GET_BAP_CB(pvosGCtx);
  if ( NULL == pBtampCtx ) 
  {
    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                 "Invalid BAP pointer from pvosGCtx on WLANBAP_Open");
                 //"Failed to allocate BAP pointer from pvosGCtx on WLANBAP_Open");
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Clean up BAP control block, initialize all values
   ------------------------------------------------------------------------*/
  VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_Open");

  WLANBAP_CleanCB(pBtampCtx, 0 /*do not empty*/);

  // Setup the "link back" to the VOSS context
  pBtampCtx->pvosGCtx = pvosGCtx;
   
  // Store a pointer to the BAP context provided by VOSS
  gpBtampCtx = pBtampCtx;
  
  /*------------------------------------------------------------------------
    Allocate internal resources
   ------------------------------------------------------------------------*/

  return VOS_STATUS_SUCCESS;
}/* WLANBAP_Open */


/*==========================================================================

  FUNCTION    WLANBAP_Start

  DESCRIPTION 
    Called as part of the overall start procedure (vos_start). BAP will 
    use this call to register with TL as the BAP entity for 
    BT-AMP RSN frames. 
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to BAP's 
                    control block can be extracted from its context 
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to BAP cb is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

    Other codes can be returned as a result of a BAL failure;
    
  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_Start
( 
  v_PVOID_t  pvosGCtx 
)
{
  ptBtampContext  pBtampCtx = NULL; 
  VOS_STATUS      vosStatus;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
    Extract BAP control block 
   ------------------------------------------------------------------------*/
  pBtampCtx = VOS_GET_BAP_CB(pvosGCtx);
  if ( NULL == pBtampCtx ) 
  {
    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                "Invalid BAP pointer from pvosGCtx on WLANBAP_Start");
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Register with TL as an BT-AMP RSN  client 
  ------------------------------------------------------------------------*/
  VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_Start TL register");

  /*------------------------------------------------------------------------
    Register with CSR for Roam (connection status) Events  
  ------------------------------------------------------------------------*/
  VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_Start CSR Register");


  /* Initialize the BAP Tx packet monitor timer */
  WLANBAP_InitConnectionAcceptTimer (pBtampCtx );
  WLANBAP_InitLinkSupervisionTimer(pBtampCtx);

  vosStatus = vos_timer_init( 
          &pBtampCtx->bapTxPktMonitorTimer,
          VOS_TIMER_TYPE_SW, /* use this type */
          WLANBAP_TxPacketMonitorHandler,
          pBtampCtx);

  vosStatus = vos_lock_init(&pBtampCtx->bapLock);
  if(!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE(VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,"Lock Init Fail");
  }

  return vosStatus;
}/* WLANBAP_Start */

/*==========================================================================

  FUNCTION    WLANBAP_Stop

  DESCRIPTION 
    Called by vos_stop to stop operation in BAP, before close. BAP will suspend all 
    BT-AMP Protocol Adaption Layer operation and will wait for the close 
    request to clean up its resources. 
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to BAP's 
                    control block can be extracted from its context 
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to BAP cb is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_Stop
( 
  v_PVOID_t  pvosGCtx 
)
{
  ptBtampContext  pBtampCtx = NULL; 
  VOS_STATUS  vosStatus = VOS_STATUS_SUCCESS;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
    Extract BAP control block 
   ------------------------------------------------------------------------*/
  pBtampCtx = VOS_GET_BAP_CB(pvosGCtx);
  if ( NULL == pBtampCtx ) 
  {
    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                 "Invalid BAP pointer from pvosGCtx on WLANBAP_Stop");
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Stop BAP (de-register RSN handler!?)  
   ------------------------------------------------------------------------*/
  vosStatus = WLANBAP_DeinitConnectionAcceptTimer(pBtampCtx);
  if ( VOS_STATUS_SUCCESS != vosStatus)
  {
    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
               "Couldn't destroy  bapConnectionAcceptTimer");
  }

  vosStatus = WLANBAP_DeinitLinkSupervisionTimer(pBtampCtx);
  if ( VOS_STATUS_SUCCESS != vosStatus)
  {
    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
               "Couldn't destroy  bapLinkSupervisionTimer");
  }

  vosStatus = vos_timer_destroy ( 
    &pBtampCtx->bapTxPktMonitorTimer );
  if ( VOS_STATUS_SUCCESS != vosStatus)
  {
    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
               "Couldn't destroy  bapTxPktMonitorTimer");
  }
  vos_lock_destroy(&pBtampCtx->bapLock);
  return VOS_STATUS_SUCCESS;
}/* WLANBAP_Stop */

/*==========================================================================

  FUNCTION    WLANBAP_Close

  DESCRIPTION 
    Called by vos_close during general driver close procedure. BAP will clean up 
    all the internal resources. 
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to BAP's 
                    control block can be extracted from its context 
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to BAP cb is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_Close
( 
  v_PVOID_t  pvosGCtx 
)
{
  ptBtampContext  pBtampCtx = NULL; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
    Extract BAP control block 
   ------------------------------------------------------------------------*/
  pBtampCtx = VOS_GET_BAP_CB(pvosGCtx);
  if ( NULL == pBtampCtx ) 
  {
    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                 "Invalid BAP pointer from pvosGCtx on WLANBAP_Close");
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Cleanup BAP control block. 
   ------------------------------------------------------------------------*/
  VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_Close");
  WLANBAP_CleanCB(pBtampCtx, 1 /* empty queues/lists/pkts if any*/);
#if  defined(ANI_OS_TYPE_ANDROID) && defined(WLAN_BTAMP_FEATURE)
  BSL_Deinit(pvosGCtx);
#endif
  /*------------------------------------------------------------------------
    Free BAP context from VOSS global 
   ------------------------------------------------------------------------*/
  vos_free_context(pvosGCtx, VOS_MODULE_ID_BAP, pBtampCtx);
  return VOS_STATUS_SUCCESS;
}/* WLANBAP_Close */

/*----------------------------------------------------------------------------
    HDD interfaces - Per instance initialization 
 ---------------------------------------------------------------------------*/

/*==========================================================================

  FUNCTION    WLANBAP_GetNewHndl

  DESCRIPTION 
    Called by HDD at driver open (BSL_Open). BAP will initialize 
    allocate a per-instance "file handle" equivalent for this specific
    open call. 
    
    There should only ever be one call to BSL_Open.  Since 
    the open app user is the BT stack.
    
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    hBtampHandle:   Handle to return btampHandle value in.
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to BAP cb is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/

VOS_STATUS 
WLANBAP_GetNewHndl
( 
   ptBtampHandle *hBtampHandle  /* Handle to return btampHandle value in  */ 
)
{
  ptBtampContext  btampContext = NULL; 
  /*------------------------------------------------------------------------
    Sanity check params
   ------------------------------------------------------------------------*/
  if ( NULL == hBtampHandle) 
  {
    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                 "Invalid BAP handle pointer in WLANBAP_GetNewHndl");
    return VOS_STATUS_E_FAULT;
  }

#ifndef BTAMP_MULTIPLE_PHY_LINKS
  /*------------------------------------------------------------------------
    Sanity check the BAP control block pointer 
   ------------------------------------------------------------------------*/
  if ( NULL == gpBtampCtx ) 
  {
    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                "Invalid BAP pointer in WLANBAP_GetNewHndl");
    return VOS_STATUS_E_FAULT;
  }

  //*hBtampHandle = (ptBtampHandle) &btampCtx; 
  /* return a pointer to the tBtampContext structure - allocated by VOS for us */ 
  *hBtampHandle = (ptBtampHandle) gpBtampCtx; 
  btampContext = gpBtampCtx; 

  /* Update the MAC address and SSID if in case the Read Local AMP Assoc
   * Request is made before Create Physical Link creation.
   */
  WLANBAP_ReadMacConfig (btampContext);
  return VOS_STATUS_SUCCESS;
#else // defined(BTAMP_MULTIPLE_PHY_LINKS)
#endif //BTAMP_MULTIPLE_PHY_LINKS
}/* WLANBAP_GetNewHndl */


/*==========================================================================

  FUNCTION    WLANBAP_ReleaseHndl

  DESCRIPTION 
    Called by HDD at driver open (BSL_Close). BAP will reclaim (invalidate) 
    the "file handle" passed into this call.
    
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    btampHandle:   btampHandle value to invalidate.
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  btampHandle is NULL ; access would cause a 
                         page fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_ReleaseHndl
( 
  ptBtampHandle btampHandle  /* btamp handle value to release  */ 
)
{
  /* obtain btamp Context  */ 
  ptBtampContext  btampContext = (ptBtampContext) btampHandle; 
  tHalHandle halHandle;
  eHalStatus halStatus = eHAL_STATUS_SUCCESS;
  /*------------------------------------------------------------------------
    Sanity check params
   ------------------------------------------------------------------------*/
  if ( NULL == btampHandle) 
  {
    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                 "Invalid BAP handle value in WLANBAP_ReleaseHndl");
    return VOS_STATUS_E_FAULT;
  }

  /* JEZ081001: TODO: Major: */ 
  /* Check to see if any wireless associations are still active */
  /* ...if so, I have to call 
   * sme_RoamDisconnect(VOS_GET_HAL_CB(btampHandle->pvosGCtx), 
   *        btampHandle->sessionId, 
   *       eCSR_DISCONNECT_REASON_UNSPECIFIED); 
   * on all of them  */ 

  halHandle = VOS_GET_HAL_CB(btampContext->pvosGCtx);
  if(NULL == halHandle)
  {
     VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                  "halHandle is NULL in %s", __func__);
     return VOS_STATUS_E_FAULT;
  }

  if( btampContext->isBapSessionOpen == TRUE )
  {
    halStatus = sme_CloseSession(halHandle, 
            btampContext->sessionId, NULL, NULL);
    if(eHAL_STATUS_SUCCESS == halStatus)
    {
      btampContext->isBapSessionOpen = FALSE;
    }
  }

  /* release the btampHandle  */ 

  return VOS_STATUS_SUCCESS;
}/* WLANBAP_ReleaseHndl */

/*----------------------------------------------------------------------------
 * Utility Function implementations 
 * -------------------------------------------------------------------------*/

/*==========================================================================

  FUNCTION    WLANBAP_CleanCB

  DESCRIPTION 
    Clear out all fields in the BAP context.
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pBtampCtx:  pointer to the BAP control block
    freeFlag:   flag indicating whether to free any allocations.
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to BAP cb is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_CleanCB
( 
  ptBtampContext  pBtampCtx,
  v_U32_t freeFlag // 0 /*do not empty*/);
)
{
  v_U16_t         i; /* Logical Link index */
  tpBtampLogLinkCtx  pLogLinkContext = NULL;
 
  /*------------------------------------------------------------------------
    Sanity check BAP control block 
   ------------------------------------------------------------------------*/

  if ( NULL == pBtampCtx ) 
  {
    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                 "Invalid BAP pointer in WLANBAP_CleanCB");
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Clean up BAP control block, initialize all values
   ------------------------------------------------------------------------*/
  VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "WLANBAP_CleanCB");

  // First, clear out EVERYTHING in the BT-AMP context
  vos_mem_set( pBtampCtx, sizeof( *pBtampCtx), 0);

  pBtampCtx->pvosGCtx = NULL;

  // Initialize physical link state machine to DISCONNECTED state
  //pBtampCtx->bapPhysLinkMachine = BTAMPFSM_INSTANCEDATA_INIT;
   
  // Initialize physical link state machine to DISCONNECTED state
  vos_mem_copy( 
          &pBtampCtx->bapPhysLinkMachine,
          &bapPhysLinkMachineInitial,   /* BTAMPFSM_INSTANCEDATA_INIT; */
          sizeof( pBtampCtx->bapPhysLinkMachine));

  VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: Initializing State: %d", __func__, bapPhysLinkMachineInitial.stateVar);   
  VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: Initialized State: %d", __func__,  pBtampCtx->bapPhysLinkMachine.stateVar); 

  //VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: btampContext value: %x", __func__,  pBtampCtx); 
#ifdef BAP_DEBUG
  /* Trace the tBtampCtx being passed in. */
  VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
            "WLAN BAP Context Monitor: pBtampCtx value = %x in %s:%d", pBtampCtx, __func__, __LINE__ );
#endif //BAP_DEBUG


  pBtampCtx->sessionId = 0;
  pBtampCtx->pAppHdl = NULL; // Per-app BSL context
  pBtampCtx->pHddHdl = NULL; // Per-app BSL context
  /* 8 bits of phy_link_handle identifies this association */
  pBtampCtx->phy_link_handle = 0;  
  pBtampCtx->channel = 0; 
  pBtampCtx->BAPDeviceRole = BT_RESPONDER;  
  pBtampCtx->ucSTAId = 0;  

  // gNeedPhysLinkCompEvent
  pBtampCtx->gNeedPhysLinkCompEvent = VOS_FALSE;
  // gPhysLinkStatus 
  pBtampCtx->gPhysLinkStatus = WLANBAP_STATUS_SUCCESS;
  // gDiscRequested
  pBtampCtx->gDiscRequested = VOS_FALSE;
  // gDiscReason 
  pBtampCtx->gDiscReason = WLANBAP_STATUS_SUCCESS;

  /* Connection Accept Timer interval*/
  pBtampCtx->bapConnectionAcceptTimerInterval = WLANBAP_CONNECTION_ACCEPT_TIMEOUT;  
  /* Link Supervision Timer interval*/
  pBtampCtx->bapLinkSupervisionTimerInterval = WLANBAP_LINK_SUPERVISION_TIMEOUT;  
  /* Logical Link Accept Timer interval*/
  pBtampCtx->bapLogicalLinkAcceptTimerInterval = WLANBAP_LOGICAL_LINK_ACCEPT_TIMEOUT;  
  /* Best Effort Flush timer interval*/
  pBtampCtx->bapBEFlushTimerInterval = WLANBAP_BE_FLUSH_TIMEOUT;  

  // Include the associations MAC addresses
  vos_mem_copy( 
          pBtampCtx->self_mac_addr, 
          pBtStaOwnMacAddr,   /* Where do I get the current MAC address? */
          sizeof(pBtampCtx->self_mac_addr)); 

  vos_mem_set( 
          pBtampCtx->peer_mac_addr, 
          sizeof(pBtampCtx->peer_mac_addr),
          0); 

  // The array of logical links
  pBtampCtx->current_log_link_index = 0;  /* assigned mod 16 */  
  pBtampCtx->total_log_link_index = 0;  /* should never be >16 */  

  // Clear up the array of logical links
  for (i = 0; i < WLANBAP_MAX_LOG_LINKS ; i++) 
  {
     pLogLinkContext = &pBtampCtx->btampLogLinkCtx[i];
     pLogLinkContext->present = 0; 
     pLogLinkContext->uTxPktCompleted = 0;
     pLogLinkContext->log_link_handle = 0;
  }


  // Include the HDD BAP Shim Layer callbacks for Fetch, TxComp, and RxPkt
  pBtampCtx->pfnBtampFetchPktCB = NULL;   
  pBtampCtx->pfnBtamp_STARxCB = NULL;   
  pBtampCtx->pfnBtampTxCompCB = NULL;   
  /* Implements the callback for ALL asynchronous events. */ 
  pBtampCtx->pBapHCIEventCB = NULL;   

  /* Set the default for event mask */ 
  vos_mem_set( 
          pBtampCtx->event_mask_page_2, 
          sizeof(pBtampCtx->event_mask_page_2),
          0); 

  /* Set the default for location data. */ 
  pBtampCtx->btamp_Location_Data_Info.loc_options = 0x58;   
  /* Set the default data transfer mode */ 
  pBtampCtx->ucDataTrafficMode = WLANBAP_FLOW_CONTROL_MODE_BLOCK_BASED;

  return VOS_STATUS_SUCCESS;
}/* WLANBAP_CleanCB */

/*==========================================================================

  FUNCTION    WLANBAP_GetCtxFromStaId

  DESCRIPTION 
    Called inside the BT-AMP PAL (BAP) layer whenever we need either the
    BSL context or the BTAMP context from the StaId.
    
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    ucSTAId:   The StaId (used by TL, PE, and HAL) 
   
    OUT
    hBtampHandle: Handle (pointer to a pointer) to return the 
                  btampHandle value in.
    hHddHdl:      Handle to return the BSL context pointer in.
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  NULL pointer; access would cause a page fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_GetCtxFromStaId
( 
  v_U8_t         ucSTAId,  /* The StaId (used by TL, PE, and HAL) */
  ptBtampHandle *hBtampHandle,  /* Handle to return per app btampHandle value in  */ 
  ptBtampContext *hBtampContext, /* Handle to return per assoc btampContext value in  */ 
  v_PVOID_t     *hHddHdl /* Handle to return BSL context in */
)
{
#ifndef BTAMP_MULTIPLE_PHY_LINKS

    /* For now, we know there is only one application context */ 
    /* ...and only one physical link context */ 
    //*hBtampHandle = &((ptBtampContext) btampCtx);  
    //*hBtampHandle = &btampCtx;  
    *hBtampHandle = (v_VOID_t*)gpBtampCtx;  
  
    //*hBtampContext = &btampCtx;
    *hBtampContext = gpBtampCtx;

    /* Handle to return BSL context in */
    //*hHddHdl = btampCtx.pHddHdl;  
    *hHddHdl = gpBtampCtx->pHddHdl;  

    return VOS_STATUS_SUCCESS;
#else // defined(BTAMP_MULTIPLE_PHY_LINKS)

#endif //BTAMP_MULTIPLE_PHY_LINKS
}/* WLANBAP_GetCtxFromStaId */

/*==========================================================================

  FUNCTION    WLANBAP_GetStaIdFromLinkCtx

  DESCRIPTION 
    Called inside the BT-AMP PAL (BAP) layer whenever we need the
    StaId (or hHddHdl) from the BTAMP context and phy_link_handle.
    
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    hBtampHandle: Handle (pointer to a pointer) to return the 
                  btampHandle value in.
    phy_link_handle: physical link handle value. Unique per assoc. 
    
    OUT
    pucSTAId:   The StaId (used by TL, PE, and HAL) 
    hHddHdl:   Handle to return the BSL context pointer in.
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  NULL pointer; access would cause a page fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_GetStaIdFromLinkCtx
( 
  ptBtampHandle  btampHandle,  /* btampHandle value in  */ 
  v_U8_t         phy_link_handle,  /* phy_link_handle value in */
  v_U8_t        *pucSTAId,  /* The StaId (used by TL, PE, and HAL) */
  v_PVOID_t     *hHddHdl /* Handle to return BSL context */
)
{
#ifndef BTAMP_MULTIPLE_PHY_LINKS
    ptBtampContext           pBtampCtx = (ptBtampContext) btampHandle; 
    
    /*------------------------------------------------------------------------
        Sanity check params
      ------------------------------------------------------------------------*/
    if ( NULL == pBtampCtx) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "Invalid BAP handle value in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

    /* Since there is only one physical link...we have stored all
     * the physical link specific context in the application context 
     */ 
    /* The StaId (used by TL, PE, and HAL) */
    *pucSTAId = pBtampCtx->ucSTAId;  

    /* Handle to return BSL context */
    *hHddHdl = pBtampCtx->pHddHdl;  

    return VOS_STATUS_SUCCESS;
#else // defined(BTAMP_MULTIPLE_PHY_LINKS)

#endif //BTAMP_MULTIPLE_PHY_LINKS
}/* WLANBAP_GetStaIdFromLinkCtx */

/*==========================================================================

  FUNCTION    WLANBAP_CreateNewPhyLinkCtx

  DESCRIPTION 
    Called in order to create (or update) a BAP Physical Link "context"
    
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    btampHandle:     BAP app context handle
    phy_link_handle: phy_link_handle from the Command 
    pHddHdl:         BSL passes in its specific context
    
    OUT
    hBtampContext:  Handle (pointer to a pointer) to return the 
                    per "Phy Link" ptBtampContext value in.
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  NULL pointer; access would cause a page fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_CreateNewPhyLinkCtx
( 
  ptBtampHandle  btampHandle,
  v_U8_t         phy_link_handle, /*  I get phy_link_handle from the Command */
  v_PVOID_t      pHddHdl,   /* BSL passes in its specific context */
  ptBtampContext *hBtampContext, /* Handle to return per assoc btampContext value in  */ 
  tWLAN_BAPRole  BAPDeviceRole
)
{
#ifndef BTAMP_MULTIPLE_PHY_LINKS
  ptBtampContext  pBtampCtx = gpBtampCtx;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /* Read and Set MAC address and SSID to BT-AMP context */
  WLANBAP_ReadMacConfig (pBtampCtx);

  /*------------------------------------------------------------------------
    For now, presume security is not enabled.
  ------------------------------------------------------------------------*/
  pBtampCtx->ucSecEnabled = WLANBAP_SECURITY_ENABLED_STATE;

  /*------------------------------------------------------------------------
    Initial Short Range Mode for this physical link is 'disabled'
  ------------------------------------------------------------------------*/
  pBtampCtx->phy_link_srm = 0;

  /*------------------------------------------------------------------------
    Clear out the logical links.
  ------------------------------------------------------------------------*/
  pBtampCtx->current_log_link_index = 0;
  pBtampCtx->total_log_link_index = 0;

  /*------------------------------------------------------------------------
    Now configure the roaming profile links. To SSID and bssid.
  ------------------------------------------------------------------------*/
  // We have room for two SSIDs.  
  pBtampCtx->csrRoamProfile.SSIDs.numOfSSIDs = 1; // This is true for now.  
  pBtampCtx->csrRoamProfile.SSIDs.SSIDList = pBtampCtx->SSIDList;  //Array of two  
  pBtampCtx->csrRoamProfile.SSIDs.SSIDList[0].SSID.length = 0;
  pBtampCtx->csrRoamProfile.SSIDs.SSIDList[0].handoffPermitted = VOS_FALSE;
  pBtampCtx->csrRoamProfile.SSIDs.SSIDList[0].ssidHidden = VOS_FALSE;

  pBtampCtx->csrRoamProfile.BSSIDs.numOfBSSIDs = 1; // This is true for now.  
  pBtampCtx->csrRoamProfile.BSSIDs.bssid = &pBtampCtx->bssid;  

  // Now configure the auth type in the roaming profile. To open.
  //pBtampCtx->csrRoamProfile.AuthType = eCSR_AUTH_TYPE_OPEN_SYSTEM; // open is the default  
  //pBtampCtx->csrRoamProfile.negotiatedAuthType = eCSR_AUTH_TYPE_OPEN_SYSTEM; // open is the default  
  pBtampCtx->csrRoamProfile.negotiatedAuthType = eCSR_AUTH_TYPE_RSN_PSK;   
  pBtampCtx->csrRoamProfile.negotiatedUCEncryptionType = eCSR_ENCRYPT_TYPE_AES;  

  pBtampCtx->phy_link_handle = phy_link_handle;
  /* For now, we know there is only one physical link context */ 
  //*hBtampContext = &btampCtx;

  pBtampCtx->pHddHdl = pHddHdl;

  *hBtampContext = pBtampCtx;
  VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                 "Btamp Ctxt = %p", pBtampCtx);

  return VOS_STATUS_SUCCESS;
#else // defined(BTAMP_MULTIPLE_PHY_LINKS)

#endif //BTAMP_MULTIPLE_PHY_LINKS
}/* WLANBAP_CreateNewPhyLinkCtx */

/*==========================================================================

  FUNCTION    WLANBAP_UpdatePhyLinkCtxStaId

  DESCRIPTION 
    Called to update the STAId value associated with Physical Link "context"
    
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pBtampContext:   ptBtampContext to update.
    ucSTAId:         The StaId (used by TL, PE, and HAL) 
    
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  NULL pointer; access would cause a page fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_UpdatePhyLinkCtxStaId
( 
  ptBtampContext pBtampContext, /* btampContext value in  */ 
  v_U8_t         ucSTAId
)
{
#ifndef BTAMP_MULTIPLE_PHY_LINKS

    /*------------------------------------------------------------------------
        Sanity check params
      ------------------------------------------------------------------------*/
    if ( NULL == pBtampContext) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "Invalid BAP handle value in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

    /* The StaId (used by TL, PE, and HAL) */
    pBtampContext->ucSTAId = ucSTAId;  

    return VOS_STATUS_SUCCESS;
#else // defined(BTAMP_MULTIPLE_PHY_LINKS)

#endif //BTAMP_MULTIPLE_PHY_LINKS
}/* WLANBAP_UpdatePhyLinkCtxStaId */

v_U8_t 
bapAllocNextLogLinkIndex
( 
  ptBtampContext pBtampContext, /* Pointer to the per assoc btampContext value */ 
  v_U8_t         phy_link_handle /*  I get phy_link_handle from the Command */
)
{
  return ++(pBtampContext->current_log_link_index) % WLANBAP_MAX_LOG_LINKS; 
}/* bapAllocNextLogLinkIndex */

/*==========================================================================

  FUNCTION    WLANBAP_CreateNewLogLinkCtx

  DESCRIPTION 
    Called in order to allocate a BAP Logical Link "context" and "index"
    
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    pBtampContext:  Pointer to the ptBtampContext value in.
    phy_link_handle: phy_link_handle involved 
    
    OUT
    pLog_link_handle: return the log_link_handle here 
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  NULL pointer; access would cause a page fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_CreateNewLogLinkCtx
( 
  ptBtampContext pBtampContext, /* Pointer to the per assoc btampContext value */ 
  v_U8_t         phy_link_handle, /*  I get phy_link_handle from the Command */
  v_U8_t         tx_flow_spec[18],
  v_U8_t         rx_flow_spec[18],
  v_U16_t         *pLog_link_handle /*  Return the logical link index here */
)
{
#ifndef BTAMP_MULTIPLE_PHY_LINKS
  v_U16_t         i; /* Logical Link index */
  tpBtampLogLinkCtx        pLogLinkContext;
  v_U32_t         retval;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    For now, allocate the logical links serially.
  ------------------------------------------------------------------------*/
  i = pBtampContext->current_log_link_index 
      = bapAllocNextLogLinkIndex(pBtampContext, phy_link_handle);
  pBtampContext->total_log_link_index++; 

  *pLog_link_handle = (i << 8) + ( v_U16_t ) phy_link_handle ; /*  Return the logical link index here */
  VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO,
                    " %s:*pLog_link_handle=%x", __func__,*pLog_link_handle);

  /*------------------------------------------------------------------------
    Evaluate the Tx and Rx Flow specification for this logical link.
  ------------------------------------------------------------------------*/
  // Currently we only support flow specs with service types of BE (0x01) 

#ifdef BAP_DEBUG
  /* Trace the tBtampCtx being passed in. */
  VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
            "WLAN BAP Context Monitor: pBtampContext value = %p in %s:%d", pBtampContext, __func__, __LINE__ );
#endif //BAP_DEBUG

  /*------------------------------------------------------------------------
    Now configure the Logical Link context.
  ------------------------------------------------------------------------*/
  pLogLinkContext = &(pBtampContext->btampLogLinkCtx[i]);

  /* Extract Tx flow spec into the context structure */
  retval = btampUnpackTlvFlow_Spec((void *)pBtampContext, tx_flow_spec,
                          WLAN_BAP_PAL_FLOW_SPEC_TLV_LEN,
                          &pLogLinkContext->btampFlowSpec);
  if (retval != BTAMP_PARSE_SUCCESS)
  {
    /* Flow spec parsing failed, return failure */
    return VOS_STATUS_E_BADMSG;
  }

  /* Save the Logical link handle in the logical link context
     As of now, only the index is saved as logical link handle since
     same is returned in the event.
     FIXME: Decide whether this index has to be combined with physical
     link handle to generate the Logical link handle.
     */
  pLogLinkContext->log_link_handle = *pLog_link_handle;

  // Mark this entry as OCCUPIED 
  pLogLinkContext->present = VOS_TRUE;
  // Now initialize the Logical Link context
  pLogLinkContext->btampAC = 1;
  // Now initialize the values in the Logical Link context
  pLogLinkContext->ucTID = 0;   // Currently we only support BE TID (0x00)
  pLogLinkContext->ucUP = 0;
  pLogLinkContext->uTxPktCompleted = 0;

  return VOS_STATUS_SUCCESS;
#else // defined(BTAMP_MULTIPLE_PHY_LINKS)

#endif //BTAMP_MULTIPLE_PHY_LINKS
}/* WLANBAP_CreateNewLogLinkCtx */

/*==========================================================================

  FUNCTION    WLANBAP_pmcFullPwrReqCB

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
WLANBAP_pmcFullPwrReqCB
( 
  void *callbackContext, 
  eHalStatus status
)
{

}/* WLANBAP_pmcFullPwrReqCB */


/*==========================================================================

  FUNCTION    WLANBAP_ReadMacConfig

  DESCRIPTION 
    This function sets the MAC config (Address and SSID to BT-AMP context
        
  DEPENDENCIES 
    
  PARAMETERS 

    pvosGCtx:       pointer to the global vos context; a handle to BAP's 
                    control block can be extracted from its context 
   
  RETURN VALUE
    None

  SIDE EFFECTS 
  
============================================================================*/
void
WLANBAP_ReadMacConfig
( 
  ptBtampContext  pBtampCtx 
)
{
  tANI_U32        len = WNI_CFG_BSSID_LEN;
  tHalHandle      pMac = NULL;

  /*------------------------------------------------------------------------
    Temporary method to get the self MAC address
  ------------------------------------------------------------------------*/
  if (NULL == pBtampCtx) 
  {
      VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                   "pBtampCtx is NULL in %s", __func__);

      return;
  }

  pMac = (tHalHandle)vos_get_context( VOS_MODULE_ID_SME, pBtampCtx->pvosGCtx);
  if (NULL == pMac) 
  {
      VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                   "pMac is NULL in %s", __func__);

      return;
  }

  ccmCfgGetStr( pMac, WNI_CFG_STA_ID, pBtStaOwnMacAddr, &len );

  VOS_ASSERT( WNI_CFG_BSSID_LEN == len );
  
  /* Form the SSID from Mac address */
  VOS_SNPRINTF( pBtStaOwnSsid, WLAN_BAP_SSID_MAX_LEN,
            "AMP-%02x-%02x-%02x-%02x-%02x-%02x",
            pBtStaOwnMacAddr[0], pBtStaOwnMacAddr[1], pBtStaOwnMacAddr[2], 
            pBtStaOwnMacAddr[3], pBtStaOwnMacAddr[4], pBtStaOwnMacAddr[5]);

  /*------------------------------------------------------------------------
    Set the MAC address for this instance
  ------------------------------------------------------------------------*/
  vos_mem_copy( 
          pBtampCtx->self_mac_addr, 
          pBtStaOwnMacAddr,
          sizeof(pBtampCtx->self_mac_addr)); 
 
  /*------------------------------------------------------------------------
    Set our SSID value
  ------------------------------------------------------------------------*/
  pBtampCtx->ownSsidLen = 21; 
  vos_mem_copy(
          pBtampCtx->ownSsid, 
          pBtStaOwnSsid,
          pBtampCtx->ownSsidLen); 
}

/*==========================================================================

  FUNCTION    WLANBAP_NeedBTCoexPriority

  DESCRIPTION 
    This function will cause a message to be sent to BTC firmware
    if a change in priority has occurred.  (From AMP's point-of-view.)

  DEPENDENCIES 

  PARAMETERS 

    pvosGCtx:       pointer to the global vos context; a handle to HAL's 
                    control block can be extracted from its context 
   
  RETURN VALUE
    None

  SIDE EFFECTS 
  
============================================================================*/
// Global
static int gBapCoexPriority;

void
WLANBAP_NeedBTCoexPriority
( 
  ptBtampContext  pBtampCtx, 
  v_U32_t         needCoexPriority
)
{
  tHalHandle      pMac = NULL;
  tSmeBtAmpEvent  btAmpEvent;


  /*------------------------------------------------------------------------
    Retrieve the pMac (HAL context)
  ------------------------------------------------------------------------*/
  pMac = (tHalHandle)vos_get_context( VOS_MODULE_ID_SME, pBtampCtx->pvosGCtx);

  // Is re-entrancy protection needed for this?
  if (needCoexPriority != gBapCoexPriority) {
    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, 
            "Calling %s with needCoexPriority=%d.", __func__, needCoexPriority);
 
    gBapCoexPriority = needCoexPriority;
    switch ( needCoexPriority)
    {
      case 0:  /* Idle */
          btAmpEvent.btAmpEventType = BTAMP_EVENT_CONNECTION_TERMINATED;
          pBtampCtx->btamp_session_on = FALSE;
          sme_sendBTAmpEvent(pMac, btAmpEvent);

          break;

      case 1:  /* Associating */
          btAmpEvent.btAmpEventType = BTAMP_EVENT_CONNECTION_START;
          pBtampCtx->btamp_session_on = TRUE;
          sme_sendBTAmpEvent(pMac, btAmpEvent);

          break;

      case 2:  /* Post-assoc */
          btAmpEvent.btAmpEventType = BTAMP_EVENT_CONNECTION_STOP;
          sme_sendBTAmpEvent(pMac, btAmpEvent);

          break;

      default:
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid Coexistence priority request: %d",
                   __func__, needCoexPriority);
    }

  }
}


/*==========================================================================

  FUNCTION    WLANBAP_RxCallback

  DESCRIPTION 
    This function is called by TL call this function for all frames except for Data frames
        
  DEPENDENCIES 
    
  PARAMETERS 

    pvosGCtx:       pointer to the global vos context; a handle to BAP's 
                    control block can be extracted from its context
    pPacket         Vos packet
    frameType       Frame type
   
  RETURN VALUE
    None

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS WLANBAP_RxCallback
(
    v_PVOID_t               pvosGCtx, 
    vos_pkt_t              *pPacket,
    WLANTL_BAPFrameEnumType frameType
)
{
    ptBtampContext  pBtampCtx = NULL; 

    pBtampCtx = VOS_GET_BAP_CB(pvosGCtx);
    if ( NULL == pBtampCtx ) 
    {
      VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                  "Invalid BAP pointer from pvosGCtx on WLANBAP_Start");
      return VOS_STATUS_E_FAULT;
    }

    switch (frameType)
    {
      case WLANTL_BT_AMP_TYPE_LS_REQ:  /* Fall through */
      case WLANTL_BT_AMP_TYPE_LS_REP:
      {
          /* Link supervision frame, process this frame */
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                     "%s: link Supervision packet received over TL: %d, => BAP",
                     __func__, frameType);
          WLANBAP_RxProcLsPkt((ptBtampHandle)pBtampCtx,
                               pBtampCtx->phy_link_handle,
                               frameType,
                               pPacket);
          break;
      }

      case WLANTL_BT_AMP_TYPE_AR: /* Fall through */
      case WLANTL_BT_AMP_TYPE_SEC:
      {
          /* Call the RSN callback handler */
          bapRsnRxCallback (pvosGCtx, pPacket);
          break;
      }

      default:
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid frametype from TL: %d, => BAP",
                   __func__, frameType);
    }

    return ( VOS_STATUS_SUCCESS );
}
