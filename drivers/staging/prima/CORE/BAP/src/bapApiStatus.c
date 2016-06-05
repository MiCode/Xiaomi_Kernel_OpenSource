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

                      b a p A p i S t a t u s . C
                                               
  OVERVIEW:
  
  This software unit holds the implementation of the WLAN BAP modules
  Status functions.
  
  The functions externalized by this module are to be called ONLY by other 
  WLAN modules (HDD) that properly register with the BAP Layer initially.

  DEPENDENCIES: 

  Are listed for each API below. 
  
  
===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


   $Header: /cygdrive/d/Builds/M7201JSDCAAPAD52240B/WM/platform/msm7200/Src/Drivers/SD/ClientDrivers/WLAN/QCT_BTAMP_RSN/CORE/BAP/src/bapApiStatus.c,v 1.7 2009/03/09 08:45:04 jzmuda Exp jzmuda $$DateTime$$Author: jzmuda $


  when        who     what, where, why
----------    ---    --------------------------------------------------------
2008-09-15    jez     Created module

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
//#include "wlan_qct_tl.h"
#include "vos_trace.h"

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

/*
Status Parameters
*/

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadFailedContactCounter()

  DESCRIPTION 
    Implements the actual HCI Read Failed Contact Counter command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIReadFailedContactCounter:  pointer to the "HCI Read Failed Contact Counter" structure.
    pFailedContactCounter:  pointer to return value for the "Failed Contact Counter"
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadFailedContactCounter or
                         pFailedContactCounter is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadFailedContactCounter
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Read_Failed_Contact_Counter_Cmd  *pBapHCIReadFailedContactCounter,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including "Read" Command Complete*/
)
{

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPReadFailedContactCounter */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPResetFailedContactCounter()

  DESCRIPTION 
    Implements the actual HCI Reset Failed Contact Counter command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIResetFailedContactCounter:  pointer to the "HCI Reset Failed Contact Counter" structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIResetFailedContactCounter is NULL
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPResetFailedContactCounter
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Reset_Failed_Contact_Counter_Cmd *pBapHCIResetFailedContactCounter,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPResetFailedContactCounter */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadLinkQuality()

  DESCRIPTION 
    Implements the actual HCI Read Link Quality command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIReadLinkQuality:  pointer to the "HCI Read Link Quality" structure.
    pBapHCILinkQuality:  pointer to return value for the "Link Quality"
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadLinkQuality or 
                         pBapHCILinkQuality is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadLinkQuality
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Read_Link_Quality_Cmd *pBapHCIReadLinkQuality,
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
    if ((NULL == btampHandle) || (NULL == pBapHCIReadLinkQuality) ||
        (NULL == pBapHCIEvent))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Invalid input parameters in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

    /* Validate the physical link handle extracted from
       input parameter. This parameter has 2 bytes for physical handle
       (only lower byte valid) */
    phyLinkHandle = (v_U8_t) pBapHCIReadLinkQuality->log_link_handle;

    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Link_Quality.log_link_handle
        = phyLinkHandle;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Link_Quality.link_quality = 0;

    if (phyLinkHandle != btampContext->phy_link_handle)
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Invalid Physical link handle in %s", __func__);
        pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Link_Quality.status
            = WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
    }
    else
    {
        /* Get the Link quality indication status from control block.
           Link quality value is being updated on the SME callback */
        pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Link_Quality.link_quality
            = btampContext->link_quality;

        pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Link_Quality.status
            = WLANBAP_STATUS_SUCCESS;
    }

    /* Fill in the parameters for command complete event... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT;
    pBapHCIEvent->u.btampCommandCompleteEvent.present = TRUE;
    pBapHCIEvent->u.btampCommandCompleteEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.command_opcode 
        = BTAMP_TLV_HCI_READ_LINK_QUALITY_CMD;

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPReadLinkQuality */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadRSSI()

  DESCRIPTION 
    Implements the actual HCI Read RSSI command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIReadRSSI:  pointer to the "HCI Read RSSI" structure.
    pBapHCIRSSI:  pointer to return value for the "RSSI".
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadRSSI or
                         pBapHCIRSSI is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadRSSI
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Read_RSSI_Cmd *pBapHCIReadRSSI,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{
    VOS_STATUS     vosStatus;
    ptBtampContext btampContext = (ptBtampContext) btampHandle;
    v_U8_t         phyLinkHandle;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: btampHandle value: %p", __func__,  btampHandle);

    /* Validate params */ 
    if ((NULL == btampHandle) || (NULL == pBapHCIReadRSSI) ||
        (NULL == pBapHCIEvent))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Invalid input parameters in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

    /* Validate the physical link handle extracted from
       input parameter. This parameter has 2 bytes for physical handle
       (only lower byte valid) */
    phyLinkHandle = (v_U8_t) pBapHCIReadRSSI->log_link_handle;

    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_RSSI.phy_link_handle
        = phyLinkHandle;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_RSSI.rssi = 0;

    if (phyLinkHandle != btampContext->phy_link_handle)
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Invalid Physical link handle in %s", __func__);
        pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_RSSI.status
            = WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
    }
    else
    {
        /* Get the RSSI value for this station (physical link) */
        vosStatus = WLANTL_GetRssi(btampContext->pvosGCtx, btampContext->ucSTAId,
                        &pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_RSSI.rssi);

        if (VOS_STATUS_SUCCESS == vosStatus)
        {
            /* GetRssi success, indicate the to upper layer */
            pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_RSSI.status
                = WLANBAP_STATUS_SUCCESS;
        }
        else
        {
            /* API failed, indicate unspecified error to upper layer */
            pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_RSSI.status
                = WLANBAP_ERROR_UNSPECIFIED_ERROR;
        }
    }

    /* Fill in the parameters for command complete event... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT;
    pBapHCIEvent->u.btampCommandCompleteEvent.present = TRUE;
    pBapHCIEvent->u.btampCommandCompleteEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.command_opcode 
        = BTAMP_TLV_HCI_READ_RSSI_CMD;

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPReadRSSI */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadLocalAMPInfo()

  DESCRIPTION 
    Implements the actual HCI Read Local AMP Information command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIReadLocalAMPInfo:  pointer to the "HCI Read Local AMP Info" Structure.
   
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadLocalAMPInfo or 
                         pBapHCILocalAMPInfo is NULL
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadLocalAMPInfo
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Read_Local_AMP_Information_Cmd *pBapHCIReadLocalAMPInfo,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{
    /* Validate params */ 
    if (btampHandle == NULL) {
      return VOS_STATUS_E_FAULT;
    }

    /* Validate params */ 
    if (pBapHCIReadLocalAMPInfo == NULL) {
      return VOS_STATUS_E_FAULT;
    }

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: btampHandle value: %p", __func__,  btampHandle);


    /* Format the command complete event to return... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT;
    pBapHCIEvent->u.btampCommandCompleteEvent.present = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.command_opcode 
        = BTAMP_TLV_HCI_READ_LOCAL_AMP_INFORMATION_CMD;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Local_AMP_Info.status 
        = WLANBAP_STATUS_SUCCESS;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Local_AMP_Info.HC_AMP_Status
        = WLANBAP_HCI_AMP_STATUS_NOT_SHARED;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Local_AMP_Info.HC_Total_BW
        = 24000;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Local_AMP_Info.HC_Max_Guaranteed_BW
        = 12000;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Local_AMP_Info.HC_Min_Latency
        = 100;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Local_AMP_Info.HC_Max_PDU_Size
        = WLANBAP_MAX_80211_PAL_PDU_SIZE;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Local_AMP_Info.HC_Controller_Type 
        = 1;
#if 0
AMP Info PAL_Capabilities: Size: 2 Octets

Value    Parameter Description
0xXXXX   Bit 0: "Service Type = Guaranteed" is not supported by PAL = 0
                "Service Type = Guaranteed" is supported by PAL = 1
         Bits 15-1: Reserved (shall be set to 0)
         (See EFS in Generic AMP FIPD [1])
#endif //0
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Local_AMP_Info.HC_PAL_Capabilities
        = 0x00; // was 0x03. Completely wrong.
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Local_AMP_Info.HC_AMP_Assoc_Length
        = 248;
        //= 40;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Local_AMP_Info.HC_Max_Flush_Timeout
        = 10000;  //10;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Local_AMP_Info.HC_BE_Flush_Timeout
        = 10000; //8;

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPReadLocalAMPInfo */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPReadLocalAMPAssoc()

  DESCRIPTION 
    Implements the actual HCI Read Local AMP Assoc command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIReadLocalAMPAssoc:  pointer to the "HCI Read Local AMP Assoc" Structure.
    
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIReadLocalAMPAssoc 
                        (or pBapHCILocalAMPAssoc) is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPReadLocalAMPAssoc
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Read_Local_AMP_Assoc_Cmd   *pBapHCIReadLocalAMPAssoc,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{
    VOS_STATUS  vosStatus;
    ptBtampContext btampContext = (ptBtampContext) btampHandle; /* btampContext value */ 
    tHalHandle hHal;
    tBtampAMP_ASSOC btamp_ASSOC; 
    v_U32_t nConsumed = 0;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/


    /* Validate params */ 
    if ((pBapHCIReadLocalAMPAssoc == NULL) || (NULL == btampHandle))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "param is NULL in %s", __func__);

        return VOS_STATUS_E_FAULT;
    }
    hHal = VOS_GET_HAL_CB(btampContext->pvosGCtx);
    if (NULL == hHal) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "hHal is NULL in %s", __func__);
      return VOS_STATUS_E_FAULT;
    }

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: btampHandle value: %p", __func__,  btampHandle);

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, 
            "In %s, phy_link_handle = %d", __func__, 
            pBapHCIReadLocalAMPAssoc->phy_link_handle); 

    /* Update the MAC address and SSID if in case the Read Local AMP Assoc
     * Request is made before Create Physical Link creation.
     */
    WLANBAP_ReadMacConfig (btampContext);

    /* Fill in the contents of an AMP_Assoc structure in preparation
     * for Packing it into the AMP_assoc_fragment field of the Read 
     * Local AMP Assoc Command Complete Event 
     */ 
    /* Return the local MAC address */ 
    btamp_ASSOC.AMP_Assoc_MAC_Addr.present = 1;
    vos_mem_copy( 
            btamp_ASSOC.AMP_Assoc_MAC_Addr.mac_addr,   
            btampContext->self_mac_addr, 
            sizeof(btampContext->self_mac_addr));

    /*Save the local AMP assoc info*/
    vos_mem_copy(btampContext->btamp_AMP_Assoc.HC_mac_addr,
                 btampContext->self_mac_addr, 
                 sizeof(btampContext->self_mac_addr));


    /* JEZ090303: This logic should return a single channel list with the */ 
    /* selected channel, if we have one. */
    //if (btampContext->channel)  
    if (1)  
    { 
        /* Return the local Preferred Channel List */ 
        /* Return both the Regulatory Info and one channel list */ 
        btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.present = 1; 
        vos_mem_copy (btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.country, "XXX", 3);
        /*Save the local AMP assoc info*/
        vos_mem_copy(btampContext->btamp_AMP_Assoc.HC_pref_country, "XXX", 3);

        btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.num_triplets = 2; 
        btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.triplets[0][0] = 201; 
        btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.triplets[0][1] = 254; 
        btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.triplets[0][2] = 0; 

        if (( BT_INITIATOR == btampContext->BAPDeviceRole ) &&
            ( 0 != btampContext->channel ))
        {
          btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.triplets[1][0] = btampContext->channel;
          btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.triplets[1][1] = 0x01; //we are AP - we start on their 1st preferred channel 
          btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.triplets[1][2] = 0x11;
        }
        else
        {
            if (btampContext->config.ucPreferredChannel)
            {
                btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.triplets[1][0] = btampContext->config.ucPreferredChannel;
                btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.triplets[1][1] = 
                    0x0B - btampContext->config.ucPreferredChannel + 1;  
            }
            else
            {
                btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.triplets[1][0] = 0x01; 
                btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.triplets[1][1] = 0x0B; //all channels for 1 to 11 
            }

            btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.triplets[1][2] = 0x11;
        }
    } else 
    { 
        /* Return the local Preferred Channel List */ 
        /* Return only the Regulatory Info */ 
        btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.present = 1; 
        vos_mem_copy (btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.country, "XXX", 3);
        btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.num_triplets = 1; 
        btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.triplets[0][0] = 201; 
        btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.triplets[0][1] = 254; 
        btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.triplets[0][2] = 0;

    }  

    /*Save the local AMP assoc info*/
    btampContext->btamp_AMP_Assoc.HC_pref_num_triplets   = btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.num_triplets;
    btampContext->btamp_AMP_Assoc.HC_pref_triplets[0][0] = btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.triplets[0][0];
    btampContext->btamp_AMP_Assoc.HC_pref_triplets[0][1] = btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.triplets[0][1]; 
    btampContext->btamp_AMP_Assoc.HC_pref_triplets[0][2] = btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.triplets[0][2];
    btampContext->btamp_AMP_Assoc.HC_pref_triplets[1][0] = btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.triplets[1][0];
    btampContext->btamp_AMP_Assoc.HC_pref_triplets[1][1] = btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.triplets[1][1]; 
    btampContext->btamp_AMP_Assoc.HC_pref_triplets[1][2] = btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.triplets[1][2];

    /* Also, at this point, lie and tell the other side we are connected on */
    /* the one channel we support. I hope this convinces the peer as BT-AMP AP */
    /* We really want him to use our channel.  Since we only support one.*/ 
    /* Return the local Connected Channel */ 
    btamp_ASSOC.AMP_Assoc_Connected_Channel.present = 1; 
    vos_mem_copy (btamp_ASSOC.AMP_Assoc_Connected_Channel.country, "XXX", 3);
    btamp_ASSOC.AMP_Assoc_Connected_Channel.num_triplets = 2; 
    btamp_ASSOC.AMP_Assoc_Connected_Channel.triplets[0][0] = 201; 
    btamp_ASSOC.AMP_Assoc_Connected_Channel.triplets[0][1] = 254; 
    btamp_ASSOC.AMP_Assoc_Connected_Channel.triplets[0][2] = 0; 
    //btamp_ASSOC.AMP_Assoc_Connected_Channel.triplets[1][0] = 0x01; 
    btamp_ASSOC.AMP_Assoc_Connected_Channel.triplets[1][0] = (0 != btampContext->channel)?btampContext->channel:0x01; 
    btamp_ASSOC.AMP_Assoc_Connected_Channel.triplets[1][1] = 0x01; 
    btamp_ASSOC.AMP_Assoc_Connected_Channel.triplets[1][2] = 0x11;


    /* Return the local PAL Capabilities */ 
    btamp_ASSOC.AMP_Assoc_PAL_Capabilities.present = 1;

#if 0
AMP ASSOC Pal Capabilities: Size: 4 Octets

   Value             Description
     4               TypeID for 802.11 PAL Capabilities

     4               Length

   0xXXXXXXXX        Bit 0:
                         0 signifies the PAL is not capable of utilizing
                           received Activity Reports
                         1 signifies the PAL is capable of utilizing
                           received Activity Reports
                     Bit 1:
                         0 signifies the PAL is not capable of utilizing
                           scheduling information sent in an Activity Report
                         1 signifies the PAL is capable of utilizing
                           scheduling information sent in an Activity Report
                     Bits 2..31 Reserved

#endif //0

    btamp_ASSOC.AMP_Assoc_PAL_Capabilities.pal_capabilities 
//        = btampContext->btamp_Remote_AMP_Assoc.HC_pal_capabilities; 
        //= 0x03;
        = 0x00;

    /* Return the local PAL Version */ 
    btamp_ASSOC.AMP_Assoc_PAL_Version.present = 1;

    /* Return the version and company ID data */ 
    btamp_ASSOC.AMP_Assoc_PAL_Version.pal_version = WLANBAP_PAL_VERSION;
    btamp_ASSOC.AMP_Assoc_PAL_Version.pal_CompanyID = WLANBAP_QUALCOMM_COMPANY_ID;  // Qualcomm Company ID
    btamp_ASSOC.AMP_Assoc_PAL_Version.pal_subversion = WLANBAP_PAL_SUBVERSION;

    //Pack the AMP Assoc structure
    vosStatus = btampPackAMP_ASSOC(
            hHal, 
            &btamp_ASSOC, 
            pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Read_Local_AMP_Assoc.AMP_assoc_fragment, 
            248, 
            &nConsumed);

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: nConsumed value: %d", __func__,  nConsumed); 

    /* Format the command complete event to return... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT;
    pBapHCIEvent->u.btampCommandCompleteEvent.present = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.command_opcode 
        = BTAMP_TLV_HCI_READ_LOCAL_AMP_ASSOC_CMD;
    /*Validate the Physical handle*/
    if(pBapHCIReadLocalAMPAssoc->phy_link_handle != 
       btampContext->phy_link_handle) { 
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, 
                "%s: Wrong Physical Link handle in Read Local AMP Assoc cmd: current: %x, new: %x", __func__,  
                btampContext->phy_link_handle, 
                pBapHCIReadLocalAMPAssoc->phy_link_handle);

        pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Read_Local_AMP_Assoc.status
            = WLANBAP_ERROR_NO_CNCT;
    } else
        pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Read_Local_AMP_Assoc.status 
            = WLANBAP_STATUS_SUCCESS;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Read_Local_AMP_Assoc.phy_link_handle 
        = pBapHCIReadLocalAMPAssoc->phy_link_handle;
    /* We will fit in one fragment, so remaining is exactly equal to encoded size*/ 
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Read_Read_Local_AMP_Assoc.remaining_length 
        = nConsumed;

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPReadLocalAMPAssoc */

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPWriteRemoteAMPAssoc()

  DESCRIPTION 
    Implements the actual HCI Write Remote AMP Assoc command.  There 
    is no need for a callback because when this call returns the action 
    has been completed.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: pointer to the BAP handle.  Returned from WLANBAP_GetNewHndl.
    pBapHCIWriteRemoteAMPAssoc:  pointer to the "HCI Write Remote AMP Assoc" Structure.
    
    IN/OUT
    pBapHCIEvent:  Return event value for the command complete event. 
                (The caller of this routine is responsible for sending 
                the Command Complete event up the HCI interface.)
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIWriteRemoteAMPAssoc is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPWriteRemoteAMPAssoc
( 
  ptBtampHandle btampHandle,
  tBtampTLVHCI_Write_Remote_AMP_ASSOC_Cmd   *pBapHCIWriteRemoteAMPAssoc,
  tpBtampHCI_Event pBapHCIEvent /* This now encodes ALL event types */
                                /* Including Command Complete and Command Status*/
)
{
    tWLAN_BAPEvent bapEvent; /* State machine event */
    VOS_STATUS  vosStatus;
    tBtampHCI_Event bapHCIEvent;

    /* I am using btampContext, instead of pBapPhysLinkMachine */ 
    //tWLAN_BAPbapPhysLinkMachine *pBapPhysLinkMachine;
    ptBtampContext btampContext = (ptBtampContext) btampHandle; /* btampContext value */ 
    v_U8_t status;    /* return the BT-AMP status here */

    /* Validate params */ 
    if (pBapHCIWriteRemoteAMPAssoc == NULL) {
      return VOS_STATUS_E_FAULT;
    }

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: btampHandle value: %p", __func__,  btampHandle);

    /* Fill in the event structure */ 
    bapEvent.event = eWLAN_BAP_HCI_WRITE_REMOTE_AMP_ASSOC;
    bapEvent.params = pBapHCIWriteRemoteAMPAssoc;

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: btampContext value: %p", __func__,  btampContext);

    /* Handle event */ 
    vosStatus = btampFsm(btampContext, &bapEvent, &status);
  
    /* Format the command complete event to return... */ 
    pBapHCIEvent->bapHCIEventCode = BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT;
    pBapHCIEvent->u.btampCommandCompleteEvent.present = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.num_hci_command_packets = 1;
    pBapHCIEvent->u.btampCommandCompleteEvent.command_opcode 
        = BTAMP_TLV_HCI_WRITE_REMOTE_AMP_ASSOC_CMD;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Write_Remote_AMP_Assoc.status 
        = status;
    pBapHCIEvent->u.btampCommandCompleteEvent.cc_event.Write_Remote_AMP_Assoc.phy_link_handle 
        = pBapHCIWriteRemoteAMPAssoc->phy_link_handle;

    if(WLANBAP_ERROR_NO_SUITABLE_CHANNEL == status)
    {
        /* Format the Physical Link Complete event to return... */ 
        bapHCIEvent.bapHCIEventCode = BTAMP_TLV_HCI_PHYSICAL_LINK_COMPLETE_EVENT;
        bapHCIEvent.u.btampPhysicalLinkCompleteEvent.present = 1;
        bapHCIEvent.u.btampPhysicalLinkCompleteEvent.status = status;
        bapHCIEvent.u.btampPhysicalLinkCompleteEvent.phy_link_handle 
            = btampContext->phy_link_handle;
        bapHCIEvent.u.btampPhysicalLinkCompleteEvent.ch_number 
            = 0;
    
        vosStatus = (*btampContext->pBapHCIEventCB) 
            (  
             btampContext->pHddHdl,   /* this refers the BSL per application context */
             &bapHCIEvent, /* This now encodes ALL event types */
             VOS_TRUE /* Flag to indicate assoc-specific event */ 
            );
    }

    /* ... */ 

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPWriteRemoteAMPAssoc */






