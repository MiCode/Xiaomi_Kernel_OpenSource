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

                      b a p A p i H C B B . C
                                               
  OVERVIEW:
  
  This software unit holds the implementation of the WLAN BAP modules
  Host Controller and Baseband functions.
  
  The functions externalized by this module are to be called ONLY by other 
  WLAN modules (HDD) that properly register with the BAP Layer initially.

  DEPENDENCIES: 

  Are listed for each API below. 
  
  
===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


   $Header: /prj/qct/asw/engbuilds/scl/users02/jzmuda/Android/ampBlueZ_6/CORE/BAP/src/bapApiHCBB.c,v 1.7 2011/05/06 00:59:27 jzmuda Exp jzmuda $$DateTime$$Author: jzmuda $


  when        who     what, where, why
----------    ---    --------------------------------------------------------
2008-09-15    jez     Created module

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "vos_trace.h"

// Pick up the sme callback registration API
#include "sme_Api.h"

/* BT-AMP PAL API header file */ 
#include "bapApi.h" 
#include "bapInternal.h" 

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


/* Host Controller and Baseband Commands */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReset()

  DESCRIPTION 
    Implements the actual HCI Reset command.
    Produces an asynchronous command complete event. Through the 
    command complete callback.  (I.E., (*tpWLAN_BAPEventCB).)

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReset
( 
  ptBtampHandle btampHandle
)
{
    VOS_STATUS  vosStatus;
    tBtampHCI_Event bapHCIEvent; /* This now encodes ALL event types */
    ptBtampContext btampContext = (ptBtampContext) btampHandle;
    tHalHandle     hHal = NULL;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: btampHandle value: %p", __func__,  btampHandle);

    /* Validate params */ 
    if (btampHandle == NULL) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "btampHandle is NULL in %s", __func__);

      return VOS_STATUS_E_FAULT;
    }

    /* Perform a "reset" */ 
    hHal = VOS_GET_HAL_CB(btampContext->pvosGCtx);
    if (NULL == hHal) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "hHal is NULL in %s", __func__);

        return VOS_STATUS_E_FAULT;
    }

    //csrRoamDisconnect();
    /* To avoid sending Disassoc on STA interface */
    if( TRUE == btampContext->isBapSessionOpen )
    {
        sme_RoamDisconnect(hHal,
                       btampContext->sessionId,
                       // Danlin, where are the richer reason codes?
                       // I want to be able to convey everything 802.11 supports...
                       eCSR_DISCONNECT_REASON_UNSPECIFIED);
    }

    /* Need to reset the timers as well*/
    /* Connection Accept Timer interval*/
    btampContext->bapConnectionAcceptTimerInterval = WLANBAP_CONNECTION_ACCEPT_TIMEOUT;  
    /* Link Supervision Timer interval*/
    btampContext->bapLinkSupervisionTimerInterval = WLANBAP_LINK_SUPERVISION_TIMEOUT;  
    /* Logical Link Accept Timer interval*/
    btampContext->bapLogicalLinkAcceptTimerInterval = WLANBAP_LOGICAL_LINK_ACCEPT_TIMEOUT;  
    /* Best Effort Flush timer interval*/
    btampContext->bapBEFlushTimerInterval = WLANBAP_BE_FLUSH_TIMEOUT;  


    /* Form and immediately return the command complete event... */ 
    bapHCIEvent.bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT;
    bapHCIEvent.u.btampCommandCompleteEvent.present = 1;
    bapHCIEvent.u.btampCommandCompleteEvent.num_hci_command_packets = 1;
    bapHCIEvent.u.btampCommandCompleteEvent.command_opcode 
        = BTAMP_TLV_HCI_RESET_CMD;
    bapHCIEvent.u.btampCommandCompleteEvent.cc_event.Reset.status 
        = WLANBAP_STATUS_SUCCESS;

    vosStatus = (*btampContext->pBapHCIEventCB) 
        (  
         //btampContext->pHddHdl,   /* this refers to the BSL per connection context */
         btampContext->pAppHdl,   /* this refers the BSL per application context */
         &bapHCIEvent, /* This now encodes ALL event types */
         VOS_FALSE /* Flag to indicate assoc-specific event */ 
        );

    return vosStatus;
} /* WLAN_BAPReset */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPSetEventMask()

  DESCRIPTION 
    Implements the actual HCI Set Event Mask command.  There is no need for 
    a callback because when this call returns the action has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCISetEventMask:  pointer to the "HCI Set Event Mask" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCISetEventMask is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPSetEventMask
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Set_Event_Mask_Cmd   *pBapHCISetEventMask,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPSetEventMask */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPFlush()

  DESCRIPTION 
    Implements the actual HCI Flush command
    Produces an asynchronous command complete event. Through the 
    event callback. And an asynchronous Flush occurred event. Also through the 
    event callback.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIFlush:  pointer to the "HCI Flush" Structure.
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIFlush is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPFlush
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Flush_Cmd     *pBapHCIFlush
)
{
    VOS_STATUS  vosStatus;
    tBtampHCI_Event bapHCIEvent; /* This now encodes ALL event types */
    ptBtampContext btampContext = (ptBtampContext) btampHandle;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: btampHandle value: %p", __func__,  btampHandle);

    /* Validate params */ 
    if (btampHandle == NULL) {
      return VOS_STATUS_E_FAULT;
    }

    /* Form and immediately return the command complete event... */ 
    bapHCIEvent.bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT;
    bapHCIEvent.u.btampCommandCompleteEvent.present = 1;
    bapHCIEvent.u.btampCommandCompleteEvent.num_hci_command_packets = 1;
    bapHCIEvent.u.btampCommandCompleteEvent.command_opcode 
        = BTAMP_TLV_HCI_FLUSH_CMD;
    bapHCIEvent.u.btampCommandCompleteEvent.cc_event.Flush.status
        = WLANBAP_STATUS_SUCCESS;

    vosStatus = (*btampContext->pBapHCIEventCB) 
        (  
         //btampContext->pHddHdl,   /* this refers to the BSL per connection context */
         btampContext->pAppHdl,   /* this refers the BSL per application context */
         &bapHCIEvent, /* This now encodes ALL event types */
         VOS_FALSE /* Flag to indicate assoc-specific event */ 
        );

    return vosStatus;
} /* WLAN_BAPFlush */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_EnhancedBAPFlush()

  DESCRIPTION 
    Implements the actual HCI Enhanced Flush command
    Produces an asynchronous command complete event. Through the command status 
    event callback. And an asynchronous Enhanced Flush Complete event. 

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIFlush:  pointer to the "HCI Enhanced Flush" Structure.
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIFlush is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_EnhancedBAPFlush
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Enhanced_Flush_Cmd     *pBapHCIFlush,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/

)
{
    VOS_STATUS  vosStatus = VOS_STATUS_SUCCESS;
    tBtampHCI_Event bapHCIEvent; /* This now encodes ALL event types */
    ptBtampContext btampContext;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: btampHandle value: %p", __func__,  btampHandle);

    /* Validate params */ 
    /* Validate params */ 
    if ((NULL == btampHandle) || (NULL == pBapHCIEvent))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Invalid input parameters in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

    btampContext = (ptBtampContext) btampHandle;
    /* Form and return the command status event... */
    bapHCIEvent.bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_STATUS_EVENT;
    bapHCIEvent.u.btampCommandStatusEvent.present = 1;
    bapHCIEvent.u.btampCommandStatusEvent.num_hci_command_packets = 1;
    bapHCIEvent.u.btampCommandStatusEvent.command_opcode
        = BTAMP_TLV_HCI_ENHANCED_FLUSH_CMD;
    bapHCIEvent.u.btampCommandStatusEvent.status = WLANBAP_STATUS_SUCCESS;

    /* Form and immediately return the command complete event... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_ENHANCED_FLUSH_COMPLETE_EVENT;
    pBapHCIEvent->u.btampEnhancedFlushCompleteEvent.present = 1;
    pBapHCIEvent->u.btampEnhancedFlushCompleteEvent.log_link_handle = 
        pBapHCIFlush->log_link_handle;

    vosStatus = (*btampContext->pBapHCIEventCB) 
        (  
         //btampContext->pHddHdl,   /* this refers to the BSL per connection context */
         btampContext->pAppHdl,   /* this refers the BSL per application context */
         &bapHCIEvent, /* This now encodes ALL event types */
         VOS_FALSE /* Flag to indicate assoc-specific event */ 
        );

    return vosStatus;
} /* WLAN_EnhancedBAPFlush */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadConnectionAcceptTimeout()

  DESCRIPTION 
    Implements the actual HCI Read Connection Accept Timeout command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadConnectionAcceptTimeout is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadConnectionAcceptTimeout
( 
  ptBtampHandle btampHandle,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including "Read" Command Complete */
)
{
    ptBtampContext btampContext = (ptBtampContext) btampHandle;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: btampHandle value: %p", __func__,  btampHandle);

    /* Validate params */ 
    if ((NULL == btampHandle) || (NULL == pBapHCIEvent))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Invalid input parameters in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

    /* Fill in the parameters for command complete event... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT;
    pBapHCIEvent->u.btampCommandCompleteEvent.present = TRUE;
    pBapHCIEvent->u.btampCommandCompleteEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.command_opcode 
        = BTAMP_TLV_HCI_READ_CONNECTION_ACCEPT_TIMEOUT_CMD;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Connection_Accept_TO.status
        = WLANBAP_STATUS_SUCCESS;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Connection_Accept_TO.connection_accept_timeout
        = btampContext->bapConnectionAcceptTimerInterval;

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPReadConnectionAcceptTimeout */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPWriteConnectionAcceptTimeout()

  DESCRIPTION 
    Implements the actual HCI Write Connection Accept Timeout command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIWriteConnectionAcceptTimeout:  pointer to the "HCI Connection Accept Timeout" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIWriteConnectionAcceptTimeout is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPWriteConnectionAcceptTimeout
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Write_Connection_Accept_Timeout_Cmd   *pBapHCIWriteConnectionAcceptTimeout,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{
    ptBtampContext btampContext = (ptBtampContext) btampHandle;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: btampHandle value: %p", __func__,  btampHandle);

    /* Validate params */ 
    if ((NULL == btampHandle) || (NULL == pBapHCIWriteConnectionAcceptTimeout)
        || (NULL == pBapHCIEvent))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Invalid input parameters in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

    /* Validate the allowed timeout interval range */
    if ((pBapHCIWriteConnectionAcceptTimeout->connection_accept_timeout >
         WLANBAP_CON_ACCEPT_TIMEOUT_MAX_RANGE) || 
        (pBapHCIWriteConnectionAcceptTimeout->connection_accept_timeout <
         WLANBAP_CON_ACCEPT_TIMEOUT_MIN_RANGE))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Out of range for connection accept timeout parameters in %s",
                   __func__);
        pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Write_Connection_Accept_TO.status
            = WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
    }
    else
    {
        /* Save the Physical link connection accept timeout value */
        btampContext->bapConnectionAcceptTimerInterval = 
            pBapHCIWriteConnectionAcceptTimeout->connection_accept_timeout;

        /* Return status for command complete event */
        pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Write_Connection_Accept_TO.status
            = WLANBAP_STATUS_SUCCESS;
    }

    /* Fill in the parameters for command complete event... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT;
    pBapHCIEvent->u.btampCommandCompleteEvent.present = TRUE;
    pBapHCIEvent->u.btampCommandCompleteEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.command_opcode 
        = BTAMP_TLV_HCI_WRITE_CONNECTION_ACCEPT_TIMEOUT_CMD;

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPWriteConnectionAcceptTimeout */


/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadLinkSupervisionTimeout()

  DESCRIPTION 
    Implements the actual HCI Read Link Supervision Timeout command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadLinkSupervisionTimeout is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadLinkSupervisionTimeout
( 
  ptBtampHandle btampHandle,
  /* Only 8 bits (phy_link_handle) of this log_link_handle are valid. */
  tBtampTLVHCI_Read_Link_Supervision_Timeout_Cmd *pBapHCIReadLinkSupervisionTimeout,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including "Read" Command Complete*/
)
{
    ptBtampContext btampContext = (ptBtampContext) btampHandle;
    v_U8_t         phyLinkHandle;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: btampHandle value: %p", __func__,  btampHandle);

    /* Validate params */ 
    if ((NULL == btampHandle) || (NULL == pBapHCIReadLinkSupervisionTimeout) ||
        (NULL == pBapHCIEvent))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Invalid input parameters in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

    /* Validate the phyiscal link handle extracted from
       logical link handle (lower byte valid) */
    phyLinkHandle = (v_U8_t) pBapHCIReadLinkSupervisionTimeout->log_link_handle;

    if (phyLinkHandle != btampContext->phy_link_handle)
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Invalid Physical link handle in %s", __func__);
        pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Link_Supervision_TO.link_supervision_timeout
            = 0x00; /* Invalid value */
        pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Link_Supervision_TO.log_link_handle
            = pBapHCIReadLinkSupervisionTimeout->log_link_handle;
        pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Link_Supervision_TO.status
            = WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
    }
    else
    {
        pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Link_Supervision_TO.link_supervision_timeout
            = btampContext->bapLinkSupervisionTimerInterval;
        pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Link_Supervision_TO.log_link_handle
            = pBapHCIReadLinkSupervisionTimeout->log_link_handle;
        pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Link_Supervision_TO.status
            = WLANBAP_STATUS_SUCCESS;
    }

    /* Fill in the parameters for command complete event... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT;
    pBapHCIEvent->u.btampCommandCompleteEvent.present = TRUE;
    pBapHCIEvent->u.btampCommandCompleteEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.command_opcode 
        = BTAMP_TLV_HCI_READ_LINK_SUPERVISION_TIMEOUT_CMD;

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPReadLinkSupervisionTimeout */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPWriteLinkSupervisionTimeout()

  DESCRIPTION 
    Implements the actual HCI Write Link Supervision Timeout command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIWriteLinkSupervisionTimeout:  pointer to the "HCI Link Supervision Timeout" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIWriteLinkSupervisionTimeout is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPWriteLinkSupervisionTimeout
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Write_Link_Supervision_Timeout_Cmd   *pBapHCIWriteLinkSupervisionTimeout,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{
    ptBtampContext btampContext = (ptBtampContext) btampHandle;
    v_U8_t         phyLinkHandle;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: btampHandle value: %p", __func__,  btampHandle);

    /* Validate params */ 
    if ((NULL == btampHandle) || (NULL == pBapHCIWriteLinkSupervisionTimeout) ||
        (NULL == pBapHCIEvent))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Invalid input parameters in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

    /* Validate the phyiscal link handle extracted from
       logical link handle (lower byte valid) */
    phyLinkHandle = (v_U8_t) pBapHCIWriteLinkSupervisionTimeout->log_link_handle;

    if (phyLinkHandle != btampContext->phy_link_handle)
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Invalid Physical link handle in %s", __func__);
        pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Write_Link_Supervision_TO.log_link_handle
            = pBapHCIWriteLinkSupervisionTimeout->log_link_handle;
        pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Write_Link_Supervision_TO.status
            = WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
    }
    else
    {
        /* Save the LS timeout interval */
        btampContext->bapLinkSupervisionTimerInterval =
            pBapHCIWriteLinkSupervisionTimeout->link_supervision_timeout;

        pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Write_Link_Supervision_TO.log_link_handle
            = pBapHCIWriteLinkSupervisionTimeout->log_link_handle;
        pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Write_Link_Supervision_TO.status
            = WLANBAP_STATUS_SUCCESS;
    }

    /* Fill in the parameters for command complete event... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT;
    pBapHCIEvent->u.btampCommandCompleteEvent.present = TRUE;
    pBapHCIEvent->u.btampCommandCompleteEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.command_opcode 
        = BTAMP_TLV_HCI_WRITE_LINK_SUPERVISION_TIMEOUT_CMD;

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPWriteLinkSupervisionTimeout */

/* v3.0 Host Controller and Baseband Commands */


/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadLogicalLinkAcceptTimeout()

  DESCRIPTION 
    Implements the actual HCI Read Logical Link Accept Timeout command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadLogicalLinkAcceptTimeout is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadLogicalLinkAcceptTimeout
( 
  ptBtampHandle btampHandle,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including "Read" Command Complete*/
)
{
    ptBtampContext btampContext = (ptBtampContext) btampHandle;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: btampHandle value: %p", __func__,  btampHandle);

    /* Validate params */ 
    if ((NULL == btampHandle) || (NULL == pBapHCIEvent))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Invalid input parameters in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

    /* Fill in the parameters for command complete event... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT;
    pBapHCIEvent->u.btampCommandCompleteEvent.present = TRUE;
    pBapHCIEvent->u.btampCommandCompleteEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.command_opcode 
        = BTAMP_TLV_HCI_READ_LOGICAL_LINK_ACCEPT_TIMEOUT_CMD;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Logical_Link_Accept_TO.status
        = WLANBAP_STATUS_SUCCESS;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Logical_Link_Accept_TO.logical_link_accept_timeout
        = btampContext->bapLogicalLinkAcceptTimerInterval;

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPReadLogicalLinkAcceptTimeout */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPWriteLogicalLinkAcceptTimeout()

  DESCRIPTION 
    Implements the actual HCI Write Logical Link Accept Timeout command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIWriteLogicalLinkAcceptTimeout:  pointer to the "HCI Logical Link Accept Timeout" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIWriteLogicalLinkAcceptTimeout is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPWriteLogicalLinkAcceptTimeout
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Write_Logical_Link_Accept_Timeout_Cmd   *pBapHCIWriteLogicalLinkAcceptTimeout,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{
    ptBtampContext btampContext = (ptBtampContext) btampHandle;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: btampHandle value: %p", __func__,  btampHandle);

    /* Validate params */ 
    if ((NULL == btampHandle) || (NULL == pBapHCIWriteLogicalLinkAcceptTimeout)
        || (NULL == pBapHCIEvent))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Invalid input parameters in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

    /* Validate the allowed timeout interval range */
    if ((pBapHCIWriteLogicalLinkAcceptTimeout->logical_link_accept_timeout >
         WLANBAP_CON_ACCEPT_TIMEOUT_MAX_RANGE) || 
        (pBapHCIWriteLogicalLinkAcceptTimeout->logical_link_accept_timeout <
         WLANBAP_CON_ACCEPT_TIMEOUT_MIN_RANGE))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Out of range for logical connection accept timeout parameters in %s",
                   __func__);
        pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Write_Logical_Link_Accept_TO.status
            = WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
    }
    else
    {
        /* Save the Physical link connection accept timeout value */
        btampContext->bapLogicalLinkAcceptTimerInterval = 
            pBapHCIWriteLogicalLinkAcceptTimeout->logical_link_accept_timeout;

        /* Return status for command complete event */
        pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Write_Logical_Link_Accept_TO.status
            = WLANBAP_STATUS_SUCCESS;
    }

    /* Fill in the parameters for command complete event... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT;
    pBapHCIEvent->u.btampCommandCompleteEvent.present = TRUE;
    pBapHCIEvent->u.btampCommandCompleteEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.command_opcode 
        = BTAMP_TLV_HCI_WRITE_LOGICAL_LINK_ACCEPT_TIMEOUT_CMD;

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPWriteLogicalLinkAcceptTimeout */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPSetEventMaskPage2()

  DESCRIPTION 
    Implements the actual HCI Set Event Mask Page 2 command.  There is no need for 
    a callback because when this call returns the action has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCISetEventMaskPage2:  pointer to the "HCI Set Event Mask Page 2" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCISetEventMaskPage2 is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPSetEventMaskPage2
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Set_Event_Mask_Page_2_Cmd   *pBapHCISetEventMaskPage2,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{
    ptBtampContext btampContext = (ptBtampContext) btampHandle;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: btampHandle value: %p", __func__,  btampHandle);

    /* Validate params */ 
    if ((NULL == btampHandle) || (NULL == pBapHCISetEventMaskPage2)
        || (NULL == pBapHCIEvent))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Invalid input parameters in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }


    /* Save away the event mask */
    vos_mem_copy(  
            btampContext->event_mask_page_2, 
            pBapHCISetEventMaskPage2->event_mask_page_2, 
            8 );

    /* Return status for command complete event */
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Set_Event_Mask_Page_2.status
        = WLANBAP_STATUS_SUCCESS;

    /* Fill in the parameters for command complete event... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT;
    pBapHCIEvent->u.btampCommandCompleteEvent.present = TRUE;
    pBapHCIEvent->u.btampCommandCompleteEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.command_opcode 
        = BTAMP_TLV_HCI_SET_EVENT_MASK_PAGE_2_CMD;

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPSetEventMaskPage2 */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadLocationData()

  DESCRIPTION 
    Implements the actual HCI Read Location Data command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadLocationData is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadLocationData
( 
  ptBtampHandle btampHandle,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including "Read" Command Complete*/
)
{
    ptBtampContext btampContext;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: btampHandle value: %p", __func__,  btampHandle);

    /* Validate params */ 
    if ((NULL == btampHandle) || (NULL == pBapHCIEvent))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Invalid input parameters in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

    btampContext = (ptBtampContext) btampHandle;

    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Location_Data.loc_domain_aware
        = btampContext->btamp_Location_Data_Info.loc_domain_aware;

    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Location_Data.loc_options
        = btampContext->btamp_Location_Data_Info.loc_options;

    vos_mem_copy(  
            pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Location_Data.loc_domain,
            btampContext->btamp_Location_Data_Info.loc_domain, 
            3 );

    /* Return status for command complete event */
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Location_Data.status
        = WLANBAP_STATUS_SUCCESS;

    /* Fill in the parameters for command complete event... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT;
    pBapHCIEvent->u.btampCommandCompleteEvent.present = TRUE;
    pBapHCIEvent->u.btampCommandCompleteEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.command_opcode 
        = BTAMP_TLV_HCI_READ_LOCATION_DATA_CMD;

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPReadLocationData */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPWriteLocationData()

  DESCRIPTION 
    Implements the actual HCI Write Location Data command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIWriteLocationData:  pointer to the "HCI Write Location Data" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIWriteLocationData is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPWriteLocationData
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Write_Location_Data_Cmd   *pBapHCIWriteLocationData,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{
    ptBtampContext btampContext;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: btampHandle value: %p", __func__,  btampHandle);

    /* Validate params */ 
    if ((NULL == btampHandle) || (NULL == pBapHCIWriteLocationData)
        || (NULL == pBapHCIEvent))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Invalid input parameters in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

    btampContext = (ptBtampContext) btampHandle;

    btampContext->btamp_Location_Data_Info.loc_domain_aware = 
        pBapHCIWriteLocationData->loc_domain_aware;
    
    btampContext->btamp_Location_Data_Info.loc_options = 
        pBapHCIWriteLocationData->loc_options;

    vos_mem_copy(  
            btampContext->btamp_Location_Data_Info.loc_domain, 
            pBapHCIWriteLocationData->loc_domain, 
            3 );

    /* Return status for command complete event */
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Write_Location_Data.status
        = WLANBAP_STATUS_SUCCESS;

    /* Fill in the parameters for command complete event... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT;
    pBapHCIEvent->u.btampCommandCompleteEvent.present = TRUE;
    pBapHCIEvent->u.btampCommandCompleteEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.command_opcode 
        = BTAMP_TLV_HCI_WRITE_LOCATION_DATA_CMD;

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPWriteLocationData */


/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadFlowControlMode()

  DESCRIPTION 
    Implements the actual HCI Read Flow Control Mode command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadFlowControlMode is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadFlowControlMode
( 
  ptBtampHandle btampHandle,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including "Read" Command Complete*/
)
{
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: btampHandle value: %p", __func__,  btampHandle);

    /* Validate params */ 
    if ((NULL == btampHandle) || (NULL == pBapHCIEvent))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Invalid input parameters in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

    /* Fill in the parameters for command complete event... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT;
    pBapHCIEvent->u.btampCommandCompleteEvent.present = TRUE;
    pBapHCIEvent->u.btampCommandCompleteEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.command_opcode 
        = BTAMP_TLV_HCI_READ_FLOW_CONTROL_MODE_CMD;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Flow_Control_Mode.status
        = WLANBAP_STATUS_SUCCESS;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Flow_Control_Mode.flow_control_mode
        = WLANBAP_FLOW_CONTROL_MODE_BLOCK_BASED;

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPReadFlowControlMode */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPWriteFlowControlMode()

  DESCRIPTION 
    Implements the actual HCI Write Flow Control Mode command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIWriteFlowControlMode:  pointer to the "HCI Write Flow Control Mode" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIWriteFlowControlMode is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPWriteFlowControlMode
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Write_Flow_Control_Mode_Cmd   *pBapHCIWriteFlowControlMode,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPWriteFlowControlMode */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadBestEffortFlushTimeout()

  DESCRIPTION 
    Implements the actual HCI Read Best Effort Flush Timeout command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadBEFlushTO is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadBestEffortFlushTimeout
( 
  ptBtampHandle btampHandle,
  /* The log_link_hanlde identifies which logical link's BE TO*/
  tBtampTLVHCI_Read_Best_Effort_Flush_Timeout_Cmd   *pBapHCIReadBEFlushTO,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including "Read" Command Complete*/
)
{

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPReadBestEffortFlushTimeout */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPWriteBestEffortFlushTimeout()

  DESCRIPTION 
    Implements the actual HCI Write Best Effort Flush TO command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIWriteBEFlushTO:  pointer to the "HCI Write BE Flush TO" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIWriteBEFlushTO is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPWriteBestEffortFlushTimeout
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Write_Best_Effort_Flush_Timeout_Cmd   *pBapHCIWriteBEFlushTO,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPWriteBestEffortFlushTimeout */


/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPSetShortRangeMode()

  DESCRIPTION 
    Implements the actual HCI Set Short Range Mode command.  There is no need for 
    a callback because when this call returns the action has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIShortRangeMode:  pointer to the "HCI Set Short Range Mode" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIShortRangeMode is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPSetShortRangeMode
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Set_Short_Range_Mode_Cmd   *pBapHCIShortRangeMode,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{
    ptBtampContext btampContext = (ptBtampContext) btampHandle;
    BTAMPFSM_INSTANCEDATA_T *instanceVar = &(btampContext->bapPhysLinkMachine);
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: btampHandle value: %p", __func__,  btampHandle);

    /* Validate params */
    if ((NULL == btampHandle) || (NULL == pBapHCIEvent))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Invalid input parameters in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

    /* Validate the BAP state to accept the Short Range Mode set request;
       SRM set requests are allowed only in CONNECTED state */

    /* Form and return the command status event... */
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_STATUS_EVENT;
    pBapHCIEvent->u.btampCommandStatusEvent.present = 1;
    pBapHCIEvent->u.btampCommandStatusEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandStatusEvent.command_opcode
        = BTAMP_TLV_HCI_SET_SHORT_RANGE_MODE_CMD;

    if (CONNECTED != instanceVar->stateVar)
    {
        /* Short Range Mode request in invalid state */
        pBapHCIEvent->u.btampCommandStatusEvent.status =
            WLANBAP_ERROR_CMND_DISALLOWED;
        return VOS_STATUS_SUCCESS;
    }
    else if (pBapHCIShortRangeMode->phy_link_handle != btampContext->phy_link_handle)
    {
       /* Invalid Physical link handle */
        pBapHCIEvent->u.btampCommandStatusEvent.status =
            WLANBAP_ERROR_NO_CNCT;
        return VOS_STATUS_SUCCESS;
    }
    else if (pBapHCIShortRangeMode->short_range_mode > 0x01)
    {
        /* Invalid mode requested */
        pBapHCIEvent->u.btampCommandStatusEvent.status =
            WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
        return VOS_STATUS_SUCCESS;
    }

    pBapHCIEvent->u.btampCommandStatusEvent.status = WLANBAP_STATUS_SUCCESS;

    /* Send the Command Status event (success) here, since Change Complete is next */
    (*btampContext->pBapHCIEventCB)
        (
         btampContext->pHddHdl,   /* this refers to the BSL per connection context */
         pBapHCIEvent, /* This now encodes ALL event types */
         VOS_FALSE /* Flag to indicate assoc-specific event */
        );

    /* Format the Short Range Mode Complete event to return... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_SHORT_RANGE_MODE_CHANGE_COMPLETE_EVENT;
    pBapHCIEvent->u.btampShortRangeModeChangeCompleteEvent.present = 1;

    pBapHCIEvent->u.btampShortRangeModeChangeCompleteEvent.status =
        WLANBAP_STATUS_SUCCESS; /* Assumption for now */

    /* The input parameters will go out in the CC Event */
    pBapHCIEvent->u.btampShortRangeModeChangeCompleteEvent.phy_link_handle =
        pBapHCIShortRangeMode->phy_link_handle;

    pBapHCIEvent->u.btampShortRangeModeChangeCompleteEvent.short_range_mode =
        pBapHCIShortRangeMode->short_range_mode; /* Assumption for now */

    /* If the requested setting is different from the current setting... */
    if (pBapHCIShortRangeMode->short_range_mode != btampContext->phy_link_srm)
    {
        /* ... then change the SRM according to the requested value.
         * If the attempt fails, the assumptions above need to be corrected.
         */
        #if 0
        // Suggested API, needs to be created
        if (VOS_STATUS_SUCCESS != HALSetShortRangeMode(pBapHCIShortRangeMode->short_range_mode))
        #else
        if (0)
        #endif
        {
            pBapHCIEvent->u.btampShortRangeModeChangeCompleteEvent.status =
                WLANBAP_ERROR_HARDWARE_FAILURE;
            pBapHCIEvent->u.btampShortRangeModeChangeCompleteEvent.short_range_mode =
                btampContext->phy_link_srm; /* Switch back to current value */
        }
        else
        {
            /* Update the SRM setting for this physical link, since it worked */
            btampContext->phy_link_srm = pBapHCIShortRangeMode->short_range_mode;
        }
    }

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPSetShortRangeMode */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPVendorSpecificCmd0()

  DESCRIPTION
    Implements the actual HCI Vendor Specific Command 0 (OGF 0x3f, OCF 0x0000).
    There is no need for a callback because when this call returns the action has
    been completed.

    The command is received when:
    - The A2MP Create Phy Link Response has been rx'd by the Bluetooth stack (initiator)

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.

    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event.
                (The caller of this routine is responsible for sending
                the Command Complete event up the HCI interface.)

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT:  pointer to pBapHCIEvent is NULL
    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS

----------------------------------------------------------------------------*/
VOS_STATUS
WLAN_BAPVendorSpecificCmd0
(
  ptBtampHandle btampHandle,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{
    ptBtampContext btampContext = (ptBtampContext) btampHandle;
    BTAMPFSM_INSTANCEDATA_T *instanceVar = &(btampContext->bapPhysLinkMachine);
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: btampHandle value: %p", __func__,  btampHandle);

    /* Validate params */
    if ((NULL == btampHandle) || (NULL == pBapHCIEvent))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Invalid input parameters in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

    /* Validate the BAP state to accept the Vendor Specific Cmd 0:
       this is only allowed for the BT_INITIATOR in the CONNECTING state */

    /* Form and return the command status event... */
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_STATUS_EVENT;
    pBapHCIEvent->u.btampCommandStatusEvent.present = 1;
    pBapHCIEvent->u.btampCommandStatusEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandStatusEvent.command_opcode
        = BTAMP_TLV_HCI_VENDOR_SPECIFIC_CMD_0;

    if ( (BT_INITIATOR != btampContext->BAPDeviceRole) ||
         (CONNECTING != instanceVar->stateVar) )
    {
        /* Vendor Specific Command 0 happened in invalid state */
        pBapHCIEvent->u.btampCommandStatusEvent.status =
            WLANBAP_ERROR_CMND_DISALLOWED;
        return VOS_STATUS_SUCCESS;
    }

    /* Signal BT Coexistence code in firmware to prefer WLAN */
    WLANBAP_NeedBTCoexPriority(btampContext, 1);

    pBapHCIEvent->u.btampCommandStatusEvent.status = WLANBAP_STATUS_SUCCESS;

    /* Send the Command Status event (success) here, since Command Complete is next */
    (*btampContext->pBapHCIEventCB)
        (
         btampContext->pHddHdl,   /* this refers to the BSL per connection context */
         pBapHCIEvent, /* This now encodes ALL event types */
         VOS_FALSE /* Flag to indicate assoc-specific event */
        );

    /* Format the Vendor Specific Command 0 Complete event to return... */
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT;
    pBapHCIEvent->u.btampCommandCompleteEvent.present = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.command_opcode
        = BTAMP_TLV_HCI_VENDOR_SPECIFIC_CMD_0;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Vendor_Specific_Cmd_0.status
        = WLANBAP_STATUS_SUCCESS;

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPVendorSpecificCmd0 */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPVendorSpecificCmd1()

  DESCRIPTION
    Implements the actual HCI Vendor Specific Command 1 (OGF 0x3f, OCF 0x0001).
    There is no need for a callback because when this call returns the action has
    been completed.

    The command is received when:
    - HCI wants to enable testability

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.

    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event.
                (The caller of this routine is responsible for sending
                the Command Complete event up the HCI interface.)

  RETURN VALUE
    The result code associated with performing the operation

    VOS_STATUS_E_FAULT:  pointer to pBapHCIEvent is NULL
    VOS_STATUS_SUCCESS:  Success

  SIDE EFFECTS

----------------------------------------------------------------------------*/
VOS_STATUS
WLAN_BAPVendorSpecificCmd1
(
  ptBtampHandle btampHandle,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{
    ptBtampContext btampContext = (ptBtampContext) btampHandle;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: btampHandle value: %p", __func__,  btampHandle);

    /* Validate params */
    if ((NULL == btampHandle) || (NULL == pBapHCIEvent))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Invalid input parameters in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }


    btampContext->btamp_async_logical_link_create = TRUE;


    /* Format the Vendor Specific Command 1 Complete event to return... */
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT;
    pBapHCIEvent->u.btampCommandCompleteEvent.present = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.command_opcode
        = BTAMP_TLV_HCI_VENDOR_SPECIFIC_CMD_1;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Vendor_Specific_Cmd_1.status
        = WLANBAP_STATUS_SUCCESS;

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPVendorSpecificCmd1 */

/*----------------------------------------------------------------------------

  DESCRIPTION   
    Callback registered with TL for BAP, this is required in order for
    TL to inform BAP, that the flush operation requested has been completed. 
    
    The registered reception callback is being triggered by TL whenever a 
    frame SIR_TL_HAL_FLUSH_AC_RSP is received by TL from HAL.

  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to TL's 
                    or SME's control block can be extracted from its context 
    ucStaId:        station identifier for the requested value
    ucTid:          identifier of the tspec 
    status:         status of the Flush operation
  
  RETURN VALUE 
    The result code associated with performing the operation

----------------------------------------------------------------------------*/
VOS_STATUS WLANBAP_TLFlushCompCallback
( 
  v_PVOID_t     pvosGCtx,
  v_U8_t        ucStaId, 
  v_U8_t        ucTID, 
  v_U8_t        status
)
{

    return VOS_STATUS_SUCCESS;
} // WLANBAP_TLFlushCompCallback


/* End of v3.0 Host Controller and Baseband Commands */


 


