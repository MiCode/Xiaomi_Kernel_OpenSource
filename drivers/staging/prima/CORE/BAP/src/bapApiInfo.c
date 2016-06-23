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

                      b a p A p i I n f o . C
                                               
  OVERVIEW:
  
  This software unit holds the implementation of the WLAN BAP modules
  Information functions.
  
  The functions externalized by this module are to be called ONLY by other 
  WLAN modules (HDD) that properly register with the BAP Layer initially.

  DEPENDENCIES: 

  Are listed for each API below. 
  
  
  Copyright (c) 2008 QUALCOMM Incorporated.
  All Rights Reserved.
  Qualcomm Confidential and Proprietary
===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


   $Header: /cygdrive/c/Dropbox/M7201JSDCAAPAD52240B/WM/platform/msm7200/Src/Drivers/SD/ClientDrivers/WLAN/QCT_BTAMP_PAL/CORE/BAP/src/bapApiInfo.c,v 1.2 2008/11/10 22:55:24 jzmuda Exp jzmuda $$DateTime$$Author: jzmuda $


  when        who     what, where, why
----------    ---    --------------------------------------------------------
2008-09-15    jez     Created module

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
//#include "wlan_qct_tl.h"
#include "vos_trace.h"
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


/* Informational Parameters */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadLocalVersionInfo()

  DESCRIPTION 
    Implements the actual HCI Read Local Version Info command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    // There are really no input parameters in this command.  
    // Just the command opcode itself is sufficient.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadLocalVersionInfo is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadLocalVersionInfo
( 
  ptBtampHandle btampHandle,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including "Read" Command Complete*/
)
{

    /* Validate params */ 
    if (btampHandle == NULL) {
      return VOS_STATUS_E_FAULT;
    }


    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: btampHandle value: %p", __func__,  btampHandle);


    /* Format the command complete event to return... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT;
    pBapHCIEvent->u.btampCommandCompleteEvent.present = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.command_opcode 
        = BTAMP_TLV_HCI_READ_LOCAL_VERSION_INFO_CMD;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Local_Version_Info.status 
        = WLANBAP_STATUS_SUCCESS;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Local_Version_Info.HC_HCI_Version
        = WLANBAP_HCI_VERSION;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Local_Version_Info.HC_HCI_Revision
        = WLANBAP_HCI_REVISION;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Local_Version_Info.HC_PAL_Version
        = WLANBAP_PAL_VERSION;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Local_Version_Info.HC_Manufac_Name
        = WLANBAP_QUALCOMM_COMPANY_ID;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Local_Version_Info.HC_PAL_Sub_Version
        = WLANBAP_PAL_SUBVERSION;

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPReadLocalVersionInfo */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadLocalSupportedCmds()

  DESCRIPTION 
    Implements the actual HCI Read Local Supported Commands.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    // There are really no input parameters in this command.  
    // Just the command opcode itself is sufficient.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadLocalSupportedCmds is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadLocalSupportedCmds
( 
  ptBtampHandle btampHandle,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including "Read" Command Complete*/
)
{
    v_U8_t supportedCmds[] = WLANBAP_PAL_SUPPORTED_HCI_CMDS;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/


    /* Validate params */ 
    if (btampHandle == NULL) {
      return VOS_STATUS_E_FAULT;
    }


    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: btampHandle value: %p", __func__,  btampHandle);


    /* Format the command complete event to return... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT;
    pBapHCIEvent->u.btampCommandCompleteEvent.present = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.command_opcode 
        = BTAMP_TLV_HCI_READ_LOCAL_SUPPORTED_CMDS_CMD;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Local_Supported_Cmds.status 
        = WLANBAP_STATUS_SUCCESS;
    /* Return the supported commands bitmask */ 
    vos_mem_copy( 
            pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Local_Supported_Cmds.HC_Support_Cmds,
            supportedCmds,
            sizeof( supportedCmds));    

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPReadLocalSupportedCmds */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadBufferSize()

  DESCRIPTION 
    Implements the actual HCI Read Buffer Size command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIReadBufferSize:  pointer to the "HCI Read Buffer Size" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadBufferSize is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadBufferSize
( 
  ptBtampHandle btampHandle,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including "Read" Command Complete*/
)
{
    /* Validate params */ 
    if (btampHandle == NULL) {
      return VOS_STATUS_E_FAULT;
    }


    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: btampHandle value: %p", __func__,  btampHandle);


    /* Format the command complete event to return... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT;
    pBapHCIEvent->u.btampCommandCompleteEvent.present = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.command_opcode 
        = BTAMP_TLV_HCI_READ_BUFFER_SIZE_CMD;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Buffer_Size.status 
        = WLANBAP_STATUS_SUCCESS;
    /* Return the supported Buffer sizes */ 
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Buffer_Size.HC_ACL_Data_Packet_Length
        = WLANBAP_MAX_80211_PAL_PDU_SIZE;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Buffer_Size.HC_SCO_Packet_Length
        = 0; /* Invalid assignment to Uint8, makes 0 */
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Buffer_Size.HC_Total_Num_ACL_Packets
        = 16;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Buffer_Size.HC_Total_Num_SCO_Packets
        = 0;

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPReadBufferSize */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadDataBlockSize()

  DESCRIPTION 
    Implements the actual HCI Read Data Block Size command.  There 
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

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadDataBlockSize is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadDataBlockSize
( 
  ptBtampHandle btampHandle,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including "Read" Command Complete*/
)
{
   /* Validate params */ 
   if ((btampHandle == NULL) || (NULL == pBapHCIEvent))
   {
     return VOS_STATUS_E_FAULT;
   }


   VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: btampHandle value: %p", __func__,  btampHandle);


   /* Format the command complete event to return... */ 
   pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT;
   pBapHCIEvent->u.btampCommandCompleteEvent.present = 1;
   pBapHCIEvent->u.btampCommandCompleteEvent.num_hci_command_packets = 1;
   pBapHCIEvent->u.btampCommandCompleteEvent.command_opcode 
       = BTAMP_TLV_HCI_READ_DATA_BLOCK_SIZE_CMD;
   pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Data_Block_Size.status 
       = WLANBAP_STATUS_SUCCESS;
   /* Return the supported Block sizes */ 
   pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Data_Block_Size.HC_Data_Block_Length
       = WLANBAP_MAX_80211_PAL_PDU_SIZE;
   pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Data_Block_Size.HC_Max_ACL_Data_Packet_Length
       = WLANBAP_MAX_80211_PAL_PDU_SIZE;
   pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Data_Block_Size.HC_Total_Num_Data_Blocks
       = 16;

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPReadDataBlockSize */


/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPSetConfig()

  DESCRIPTION 
     The function updates some configuration for BAP module in SME during SMEs
     close -> open sequence.
   
     BAP applies the new configuration at the next transaction.


  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIReadRSSI:  pointer to the "HCI Read RSSI" structure.
   
    IN
    pConfig: a pointer to a caller allocated object of typedef struct WLANBAP_ConfigType.
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pConfig or btampHandle is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPSetConfig
( 
  ptBtampHandle btampHandle,
  WLANBAP_ConfigType *pConfig
)
{
   ptBtampContext btampContext;
   /* Validate params */ 
   if ((NULL == btampHandle)|| (NULL == pConfig)) 
   {
     return VOS_STATUS_E_FAULT;
   }
   btampContext = (ptBtampContext) btampHandle; /* btampContext value */ 

   btampContext->config.ucPreferredChannel = pConfig->ucPreferredChannel;
   return VOS_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPGetMask()

  DESCRIPTION 
     The function gets the updated event mask from BAP core.
   


  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    
   
    IN
    pEvent_mask_page_2: a pointer to a caller allocated object of 8 bytes.
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pEvent_mask_page_2 or btampHandle is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS
WLAN_BAPGetMask( ptBtampHandle btampHandle, 
                 v_U8_t       *pEvent_mask_page_2)
{
   ptBtampContext btampContext;
   /* Validate params */ 
   if ((NULL == btampHandle)|| (NULL == pEvent_mask_page_2)) 
   {
     return VOS_STATUS_E_FAULT;
   }
   btampContext = (ptBtampContext) btampHandle; /* btampContext value */ 

   vos_mem_copy( pEvent_mask_page_2, 
                 btampContext->event_mask_page_2, 
                 8 );
   return VOS_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPDisconnect()

  DESCRIPTION 
     The function to request to BAP core to disconnect currecnt AMP connection.
   


  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  btampHandle is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPDisconnect
( 
  ptBtampHandle btampHandle
)
{
    ptBtampContext btampContext = (ptBtampContext) btampHandle;
    tWLAN_BAPEvent bapEvent; /* State machine event */
    v_U8_t status;    /* return the BT-AMP status here */
    VOS_STATUS  vosStatus;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_FATAL, "%s: btampHandle value: %p", __func__,  btampHandle);

    /* Validate params */ 
    if (btampHandle == NULL) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_FATAL,
                     "btampHandle is NULL in %s", __func__);

      return VOS_STATUS_E_FAULT;
    }

    /* Fill in the event structure */ 
    bapEvent.event = eWLAN_BAP_MAC_INDICATES_MEDIA_DISCONNECTION;
    bapEvent.params = NULL;


    /* Handle event */ 
    vosStatus = btampFsm(btampContext, &bapEvent, &status);


        /* Fill in the event structure */ 
    bapEvent.event =  eWLAN_BAP_MAC_READY_FOR_CONNECTIONS;
    bapEvent.params = NULL;

        /* Handle event */ 
    vosStatus = btampFsm(btampContext, &bapEvent, &status);


    return VOS_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPSessionOn()

  DESCRIPTION 
     The function to check from BAP core if AMP connection is up right now.
   


  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.


  RETURN VALUE
    The result code associated with performing the operation  

    VOS_TRUE:  AMP connection is on 
    VOS_FALSE: AMP connection is not on
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
v_BOOL_t WLAN_BAPSessionOn
( 
  ptBtampHandle btampHandle
)
{
   ptBtampContext btampContext = (ptBtampContext) btampHandle;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

   VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: btampHandle value: %p", __func__,  btampHandle);

   /* Validate params */ 
   if (btampHandle == NULL) 
   {
       VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                    "btampHandle is NULL in %s", __func__);

       //?? shall we say true or false
       return VOS_FALSE;
   }

   return btampContext->btamp_session_on;
}
