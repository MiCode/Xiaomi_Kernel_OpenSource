/*
 * Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
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

                      b t a m p F s m . C

  OVERVIEW:

  This software unit holds the implementation of the Finite State Machine that
  controls the operation of each individual AMP Physical link.
  (Currently, this is limited to ONE link.)

  The btampFsm() routine provided by this module is called by the rest of
  the BT-AMP PAL module whenever a control plane operation occurs that requires a
  major state transition.

  DEPENDENCIES:

  Are listed for each API below.


===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


   $Header: /prj/qct/asw/engbuilds/scl/users02/jzmuda/gb-bluez/vendor/qcom/proprietary/wlan/libra/CORE/BAP/src/btampFsm.c,v 1.11 2011/03/30 21:52:10 jzmuda Exp jzmuda $


  when        who     what, where, why
----------    ---    --------------------------------------------------------
2008-10-16    jez     Created module

===========================================================================*/

/* This file is generated from btampFsm.cdd - do not edit manually*/
/* Generated on: Thu Oct 16 15:40:39 PDT 2008 / version 1.2 Beta 1 */

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/


#include "fsmDefs.h"
//#include "btampFsm.h"
#include "bapInternal.h"
#include "btampFsm_ext.h"

// Pick up the BTAMP Timer API definitions
#include "bapApiTimer.h"

// Pick up the BTAMP RSN definitions
#include "bapRsn8021xFsm.h"

#include "bapRsn8021xAuthFsm.h"
// Pick up the SME API definitions
#include "sme_Api.h"

// Pick up the PMC API definitions
#include "pmcApi.h"

// Pick up the BTAMP API defintions for interfacing to External subsystems
#include "bapApiExt.h"

#include "wlan_nlink_common.h"
#include "wlan_btc_svc.h"

// Pick up the DOT11 Frames compiler
// I just need these one "opaque" type definition in order to use the "frames" code
typedef struct sAniSirGlobal *tpAniSirGlobal;
#include "dot11f.h"

#if 0
/*
 * Event-related Defines. 
 *  -  Ultimately, these events will be values
 *  -  from an enumeration.  That are set by some
 *  - of the following events.
 */
#define eWLAN_BAP_MAC_START_BSS_SUCCESS /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_WDS_STARTED */
#define eWLAN_BAP_MAC_START_FAILS /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_FAILURE or eCSR_ROAM_RESULT_NOT_ASSOCIATED */
#define eWLAN_BAP_MAC_SCAN_COMPLETE /* bapScanCompleteCallback */
#define eWLAN_BAP_CHANNEL_NOT_SELECTED /* No existing Infra assoc  - e.g., use HAL to access the STA LIST and find nothing */
#define eWLAN_BAP_MAC_CONNECT_COMPLETED /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_WDS_ASSOCIATED */
#define eWLAN_BAP_MAC_CONNECT_FAILED /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_FAILURE or eCSR_ROAM_RESULT_NOT_ASSOCIATED */
#define eWLAN_BAP_MAC_CONNECT_INDICATION /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_WDS_ASSOCIATION_IND */
#define eWLAN_BAP_RSN_SUCCESS /* setKey IOCTL from the Auth/Supp App */
#define eWLAN_BAP_RSN_FAILURE /* deAuth IOCTL from the Auth/Supp App */
#define eWLAN_BAP_MAC_KEY_SET_SUCCESS /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_KEY_SET */
#define eWLAN_BAP_MAC_INDICATES_MEDIA_DISCONNECTION /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_WDS_DISASSOC_IND */
#define eWLAN_BAP_MAC_READY_FOR_CONNECTIONS /* bapRoamCompleteCallback with eCSR_ROAM_RESULT_WDS_STOPPED */
#define eWLAN_BAP_CHANNEL_SELECTION_FAILED /* ??? */

#endif /* 0 */

/*Min and max channel values in 2.4GHz band for operational channel validation 
  on connect*/
#define WLAN_BAP_MIN_24G_CH  1
#define WLAN_BAP_MAX_24G_CH  14


/* The HCI Disconnect Logical Link Complete Event signalling routine*/
VOS_STATUS
signalHCIDiscLogLinkCompEvent
( 
  ptBtampContext btampContext, /* btampContext value */    
  v_U8_t status,    /* the BT-AMP status */
  v_U16_t log_link_handle,  /* The Logical Link that disconnected*/
  v_U8_t reason    /* the BT-AMP reason code */
);


/* Stubs - TODO : Remove once the functions are available */
int
bapSuppDisconnect(tBtampContext *ctx)
{
    // Disconnect function is called internally
    // TODO : Need to find, if it disconnect will be issued from bap for supplicant
    return ANI_OK;
}

int
bapAuthDisconnect(tBtampContext *ctx)
{
    // Disconnect function is called internally
    // TODO : Need to find, if it disconnect will be issued from bap for supplicant
    return ANI_OK;
}

VOS_STATUS
bapSetKey( v_PVOID_t pvosGCtx, tCsrRoamSetKey *pSetKeyInfo )
{
    tWLAN_BAPEvent bapEvent; /* State machine event */
    VOS_STATUS  vosStatus = VOS_STATUS_SUCCESS;
    ptBtampContext btampContext; /* use btampContext value */ 
    v_U8_t status;    /* return the BT-AMP status here */
    eHalStatus  halStatus;
    v_U32_t roamId = 0xFF;
    tHalHandle     hHal = NULL;
    v_U8_t groupMac[ANI_MAC_ADDR_SIZE] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  
 
    /* Validate params */ 
    if ((pvosGCtx == NULL) || (pSetKeyInfo == NULL))
    {
      return VOS_STATUS_E_FAULT;
    }

    btampContext = VOS_GET_BAP_CB(pvosGCtx); 
    /* Validate params */ 
    if ( btampContext == NULL)
    {
      return VOS_STATUS_E_FAULT;
    }
    hHal = VOS_GET_HAL_CB(btampContext->pvosGCtx);
    if (NULL == hHal) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "hHal is NULL in %s", __func__);

        return VOS_STATUS_E_FAULT;
    }

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: btampContext value: %p", __func__,  btampContext);

    /* Fill in the event structure */ 
    bapEvent.event = eWLAN_BAP_RSN_SUCCESS;
    bapEvent.params = NULL;

    /* Signal the successful RSN auth and key exchange event */ 
    /* (You have to signal BEFORE calling sme_RoamSetKey) */ 
    vosStatus = btampFsm(btampContext, &bapEvent, &status);

    /* Set the Pairwise Key */ 
    halStatus = sme_RoamSetKey( 
            hHal, 
            btampContext->sessionId, 
            pSetKeyInfo, 
            &roamId );
    if ( halStatus != eHAL_STATUS_SUCCESS )
    {
      VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, 
              "[%4d] sme_RoamSetKey returned ERROR status= %d", __LINE__, halStatus );
        return VOS_STATUS_E_FAULT;
    }
                         
    /* Set the Group Key */ 
    vos_mem_copy( pSetKeyInfo->peerMac, groupMac, sizeof( tAniMacAddr ) );
    halStatus = sme_RoamSetKey( 
            hHal, 
            btampContext->sessionId, 
            pSetKeyInfo, 
            &roamId );
    if ( halStatus != eHAL_STATUS_SUCCESS )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, 
                "[%4d] sme_RoamSetKey returned ERROR status= %d", __LINE__, halStatus );
      return VOS_STATUS_E_FAULT;
    }
  
    return vosStatus;
}

/*
 * Debug-related Defines. 
 *  -  Ultimately, these events will be values
 *  -  from an enumeration.  That are set by some
 *  - of the following events.
 */
#define DUMPLOG_ON
#if defined DUMPLOG_ON
#define DUMPLOG(n, name1, name2, aStr, size) do {                       \
        int i;                                                          \
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%d. %s: %s = \n", n, name1, name2); \
        for (i = 0; i < size; i++)                                      \
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%2.2x%s", ((unsigned char *)aStr)[i], i % 16 == 15 ? "\n" : " "); \
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "\n"); \
    } while (0)
#else
#define DUMPLOG(n, name1, name2, aStr, size)
#endif

/*
 * State transition procedures 
 */

VOS_STATUS
gotoS1
( 
    ptBtampContext btampContext, /* btampContext value */    
    ptWLAN_BAPEvent bapEvent, /* State machine event */
    tWLAN_BAPRole BAPDeviceRole,
    v_U8_t *status    /* return the BT-AMP status here */
) 
{
  tBtampTLVHCI_Create_Physical_Link_Cmd *pBapHCIPhysLinkCreate 
      = (tBtampTLVHCI_Create_Physical_Link_Cmd *) bapEvent->params;
  tBtampTLVHCI_Accept_Physical_Link_Cmd *pBapHCIPhysLinkAccept 
      = (tBtampTLVHCI_Accept_Physical_Link_Cmd *) bapEvent->params;
  VOS_STATUS  vosStatus = VOS_STATUS_SUCCESS;
  v_U32_t     conAcceptTOInterval;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /* Remember role */
  btampContext->BAPDeviceRole = BAPDeviceRole;

    switch(BAPDeviceRole)
    {
    case BT_INITIATOR:
      /* Copy down the phy_link_handle value */
      btampContext->phy_link_handle = pBapHCIPhysLinkCreate->phy_link_handle;
      /* Copy out the key material from the HCI command */
      btampContext->key_type = pBapHCIPhysLinkCreate->key_type;
      btampContext->key_length = pBapHCIPhysLinkCreate->key_length;
      vos_mem_copy( 
              btampContext->key_material, 
              pBapHCIPhysLinkCreate->key_material, 
              32);  /* Need a key size define */
      break;
    case BT_RESPONDER:
      /* Copy down the phy_link_handle value */
      btampContext->phy_link_handle = pBapHCIPhysLinkAccept->phy_link_handle;
      /* Copy out the key material from the HCI command */
      btampContext->key_type = pBapHCIPhysLinkAccept->key_type; 
      btampContext->key_length = pBapHCIPhysLinkAccept->key_length; 
      vos_mem_copy( 
              btampContext->key_material, 
              pBapHCIPhysLinkAccept->key_material, 
              32);  /* Need a key size define */
      break;
    default:
      *status = WLANBAP_ERROR_HOST_REJ_RESOURCES;     /* return the BT-AMP status here */
      return VOS_STATUS_E_RESOURCES;
  }

  conAcceptTOInterval = (btampContext->bapConnectionAcceptTimerInterval * 5)/ 8;
  /* Start the Connection Accept Timer */
  vosStatus = WLANBAP_StartConnectionAcceptTimer ( 
          btampContext, 
          conAcceptTOInterval);

  *status = WLANBAP_STATUS_SUCCESS;     /* return the BT-AMP status here */
  
  return VOS_STATUS_SUCCESS;
} //gotoS1

VOS_STATUS
gotoScanning
( 
    ptBtampContext btampContext, /* btampContext value */    
    tWLAN_BAPRole BAPDeviceRole,
    v_U8_t *status    /* return the BT-AMP status here */
)
{
  /* Initiate a SCAN request */
  //csrScanRequest();
  *status = WLANBAP_STATUS_SUCCESS;     /* return the BT-AMP status here */

  return VOS_STATUS_SUCCESS;
}


#if 0
/*==========================================================================
 
  FUNCTION: convertRoleToBssType

  DESCRIPTION:  Return one of the following values:

    eCSR_BSS_TYPE_INFRASTRUCTURE,
    eCSR_BSS_TYPE_IBSS,           // an IBSS network we will NOT start
    eCSR_BSS_TYPE_START_IBSS,     // an IBSS network we will start if no partners detected.
    eCSR_BSS_TYPE_WDS_AP,         // BT-AMP AP
    eCSR_BSS_TYPE_WDS_STA,        // BT-AMP station
    eCSR_BSS_TYPE_ANY, 
============================================================================*/
#endif
eCsrRoamBssType 
convertRoleToBssType
(
    tWLAN_BAPRole bapRole  /* BT-AMP role */
) 
{
    switch (bapRole)
    {
        case BT_RESPONDER: 
            // an WDS network we will join
            return eCSR_BSS_TYPE_WDS_STA;            
            //return eCSR_BSS_TYPE_INFRASTRUCTURE;            
            //return eCSR_BSS_TYPE_IBSS;     // Initial testing with IBSS on both ends makes more sense
        case BT_INITIATOR: 
            // an WDS network we will start if no partners detected.
            return eCSR_BSS_TYPE_WDS_AP;      
            //return eCSR_BSS_TYPE_START_IBSS; // I really should try IBSS on both ends      
        default:
            return eCSR_BSS_TYPE_INFRASTRUCTURE;
    }
} // convertRoleToBssType


char hexValue[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                   '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
                  };

#define BAP_MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX_BYTES 8
// Each byte will be converted to hex digits followed by a
// punctuation (which is specified in the "delimiter" param.) Thus 
// allocate three times the storage.
v_U8_t *
bapBin2Hex(const v_U8_t *bytes, v_U32_t len, char delimiter)
{
  static v_U8_t buf[MAX_BYTES*(2+1)];
  v_U32_t i;
  v_U8_t *ptr;

  len = BAP_MIN(len, MAX_BYTES);
    for (i = 0, ptr = buf; i < len; i++)
    {
    *ptr++ = hexValue[ (bytes[i] >> 4) & 0x0f];
    *ptr++ = hexValue[ bytes[i] & 0x0f];
    *ptr++ = delimiter;
    //sprintf(ptr, "%.2x%c", bytes[i], delimiter);
    //ptr += 3;
  }

  // Delete the extra punctuation and null terminate the string
  if (len > 0)
      ptr--;
  *ptr = '\0';

  return buf;
}// bapBin2Hex

char bapSsidPrefixValue[] = {'A', 'M', 'P', '-'};

v_U8_t * 
convertBSSIDToSSID
(
    v_U8_t *bssid  /* BSSID value */
) 
{
    static v_U8_t ssId[32]; 

    vos_mem_copy( 
            ssId, 
            bapSsidPrefixValue, 
            4); 

    vos_mem_copy( 
            &ssId[4], 
            bapBin2Hex(bssid, 6, '-'),
            17); 

    return ssId;
} // convertBSSIDToSSID

VOS_STATUS
convertToCsrProfile
(
    ptBtampContext btampContext, /* btampContext value */    
    eCsrRoamBssType bssType,
    tCsrRoamProfile *pProfile   /* return the profile info here */
) 
{
    static v_U8_t btampRSNIE[] = {0x30, 0x14, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00, 
                                  0x00, 0x0f, 0xac, 0x04, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x02, 0x00, 0x00
                                 };
    VOS_STATUS  vosStatus = VOS_STATUS_SUCCESS;
    v_S7_t sessionid = -1;
    tHalHandle     hHal = NULL;
    v_U32_t triplet;
    v_U8_t regulatoryClass;
    v_U8_t firstChannel;
    v_U8_t numChannels;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
    if (NULL == btampContext) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "btampContext is NULL in %s", __func__);

        return VOS_STATUS_E_FAULT;
    }

    hHal = VOS_GET_HAL_CB(btampContext->pvosGCtx);
    if (NULL == hHal) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "hHal is NULL in %s", __func__);

        return VOS_STATUS_E_FAULT;
    }

    //Zero out entire roamProfile structure to avoid problems in uninitialized pointers as the structure expands */
    //vos_mem_zero(pProfile,sizeof(tCsrRoamProfile));

    //Set the BSS Type
    //pProfile->BSSType = convertRoleToBssType(btampContext->BAPDeviceRole ); 
    pProfile->BSSType = bssType; 
    //pProfile->BSSType = eCSR_BSS_TYPE_INFRASTRUCTURE;

    //Set the SSID

    if ( bssType == eCSR_BSS_TYPE_WDS_STA) 
    {
        pProfile->SSIDs.numOfSSIDs = 2;

        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: bssType = %s, SSID specified = %s\n", __func__, "eCSR_BSS_TYPE_WDS_STA", convertBSSIDToSSID(btampContext->btamp_Remote_AMP_Assoc.HC_mac_addr));
    
        vos_mem_zero(pProfile->SSIDs.SSIDList[0].SSID.ssId, 
                sizeof(pProfile->SSIDs.SSIDList[0].SSID.ssId));
        vos_mem_copy(pProfile->SSIDs.SSIDList[0].SSID.ssId,
                convertBSSIDToSSID(btampContext->btamp_Remote_AMP_Assoc.HC_mac_addr),
                21);  // Length of BTAMP SSID is 21 bytes
        pProfile->SSIDs.SSIDList[0].SSID.length = 21;

        vos_mem_zero(pProfile->SSIDs.SSIDList[1].SSID.ssId, 
                sizeof(pProfile->SSIDs.SSIDList[1].SSID.ssId));
        vos_mem_copy(pProfile->SSIDs.SSIDList[1].SSID.ssId,
                convertBSSIDToSSID(btampContext->self_mac_addr),
                21);  // Length of BTAMP SSID is 21 bytes
        pProfile->SSIDs.SSIDList[1].SSID.length = 21;
 
        //Set the BSSID to the Remote AP
        pProfile->BSSIDs.numOfBSSIDs = 1;
        vos_mem_copy(pProfile->BSSIDs.bssid, 
                btampContext->btamp_Remote_AMP_Assoc.HC_mac_addr, 
                sizeof( tCsrBssid ) ); 
 
    }
    else if ( bssType == eCSR_BSS_TYPE_WDS_AP)
    {
        pProfile->SSIDs.numOfSSIDs = 1;

        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: bssType = %s, SSID specified = %s\n", __func__, "eCSR_BSS_TYPE_WDS_AP", convertBSSIDToSSID(btampContext->self_mac_addr));
    
        vos_mem_zero(pProfile->SSIDs.SSIDList[0].SSID.ssId, 
                sizeof(pProfile->SSIDs.SSIDList[0].SSID.ssId));
        vos_mem_copy(pProfile->SSIDs.SSIDList[0].SSID.ssId,
                convertBSSIDToSSID(btampContext->self_mac_addr),
                21);  // Length of BTAMP SSID is 21 bytes
        pProfile->SSIDs.SSIDList[0].SSID.length = 21;
 
#if 0
        //In case you are an AP, don't set the BSSID
        pProfile->BSSIDs.numOfBSSIDs = 0;
#endif //0
 
        //Set the BSSID to your "self MAC Addr"
        pProfile->BSSIDs.numOfBSSIDs = 1;
        vos_mem_copy(pProfile->BSSIDs.bssid, 
                btampContext->self_mac_addr,
                sizeof( tCsrBssid ) ); 
 
    }
    else
    // Handle everything else as bssType eCSR_BSS_TYPE_INFRASTRUCTURE
    {
        pProfile->SSIDs.numOfSSIDs = 1;
    
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s: bssType = %s, SSID specified = %s\n", __func__, "eCSR_BSS_TYPE_WDS_STA", convertBSSIDToSSID(btampContext->btamp_Remote_AMP_Assoc.HC_mac_addr));

        vos_mem_zero(pProfile->SSIDs.SSIDList[0].SSID.ssId, 
                sizeof(pProfile->SSIDs.SSIDList[0].SSID.ssId));
        vos_mem_copy(pProfile->SSIDs.SSIDList[0].SSID.ssId,
                convertBSSIDToSSID(btampContext->btamp_Remote_AMP_Assoc.HC_mac_addr),
                21);  // Length of BTAMP SSID is 21 bytes
        pProfile->SSIDs.SSIDList[0].SSID.length = 21;
 
        //Set the BSSID to the Remote AP
        pProfile->BSSIDs.numOfBSSIDs = 1;
        vos_mem_copy(pProfile->BSSIDs.bssid, 
                btampContext->btamp_Remote_AMP_Assoc.HC_mac_addr, 
                sizeof( tCsrBssid ) ); 
 
    }
 
    //Always set the Auth Type
    //pProfile->negotiatedAuthType = eCSR_AUTH_TYPE_RSN_PSK;
    //pProfile->negotiatedAuthType = eCSR_AUTH_TYPE_NONE;
    //pProfile->negotiatedAuthType = eCSR_AUTH_TYPE_OPEN_SYSTEM;
    pProfile->AuthType.numEntries = 1;
    //pProfile->AuthType.authType[0] = eCSR_AUTH_TYPE_OPEN_SYSTEM;
    pProfile->AuthType.authType[0] = eCSR_AUTH_TYPE_RSN_PSK;

    //Always set the Encryption Type
    //pProfile->negotiatedUCEncryptionType = eCSR_ENCRYPT_TYPE_AES;
    //pProfile->negotiatedUCEncryptionType = eCSR_ENCRYPT_TYPE_NONE;
    pProfile->EncryptionType.numEntries = 1;
    //pProfile->EncryptionType.encryptionType[0] = eCSR_ENCRYPT_TYPE_NONE;
    pProfile->EncryptionType.encryptionType[0] = eCSR_ENCRYPT_TYPE_AES;

    pProfile->mcEncryptionType.numEntries = 1;
    //pProfile->mcEncryptionType.encryptionType[0] = eCSR_ENCRYPT_TYPE_NONE;
    pProfile->mcEncryptionType.encryptionType[0] = eCSR_ENCRYPT_TYPE_AES;

    //set the RSN IE
    //This is weird, but it works
    pProfile->pRSNReqIE = &btampRSNIE[0];
    pProfile->nRSNReqIELength = 0x16; //TODO
    //pProfile->pRSNReqIE = NULL;
 
    /** We don't use the WPAIE.But NULL it to avoid being used **/
    pProfile->pWPAReqIE = NULL;
    pProfile->nWPAReqIELength = 0;

    // Identify the operation channel

    /* Choose the operation channel from the preferred channel list */
    pProfile->operationChannel = 0;
    regulatoryClass = 0;
    for (triplet = 0; triplet < btampContext->btamp_Remote_AMP_Assoc.HC_pref_num_triplets; triplet++)
        {
        firstChannel = 0;
        numChannels = 0;

        /* is this a regulatory class triplet? */
        if (btampContext->btamp_Remote_AMP_Assoc.HC_pref_triplets[triplet][0] == 201)
            {
            /* identify supported 2.4GHz regulatory classes */
            switch (btampContext->btamp_Remote_AMP_Assoc.HC_pref_triplets[triplet][1])
            {
                case 254:
                {
                    /* class 254 is special regulatory class defined by BT HS+3.0 spec that
                       is valid only for unknown/'mobile' country */
                    if ((btampContext->btamp_Remote_AMP_Assoc.HC_pref_country[0] == 'X') &&
                        (btampContext->btamp_Remote_AMP_Assoc.HC_pref_country[1] == 'X'))
                    {
                        regulatoryClass = 254;
                        firstChannel = 1;
                        numChannels = 11;
                    }
                    break;
            }
                case 12:
            {
                    /* class 12 in the US regulatory domain is 2.4GHz channels 1-11 */
                    if ((btampContext->btamp_Remote_AMP_Assoc.HC_pref_country[0] == 'U') &&
                        (btampContext->btamp_Remote_AMP_Assoc.HC_pref_country[1] == 'S'))
                    {
                        regulatoryClass = 12;
                        firstChannel = 1;
                        numChannels = 11;
            }
                    break;
                }
                case 4:
            {
                    /* class 4 in the Europe regulatory domain is 2.4GHz channels 1-13 */
                    if ((btampContext->btamp_Remote_AMP_Assoc.HC_pref_country[0] == 'G') &&
                        (btampContext->btamp_Remote_AMP_Assoc.HC_pref_country[1] == 'B'))
                    {
                        regulatoryClass = 4;
                        firstChannel = 1;
                        numChannels = 13;
            }
                    break;
                }
                case 30:
            {
                    /* class 30 in the Japan regulatory domain is 2.4GHz channels 1-13 */
                    if ((btampContext->btamp_Remote_AMP_Assoc.HC_pref_country[0] == 'J') &&
                        (btampContext->btamp_Remote_AMP_Assoc.HC_pref_country[1] == 'P'))
                    {
                        regulatoryClass = 30;
                        firstChannel = 1;
                        numChannels = 13;
            }
                    break;
        }
                default:
        {
                    break;
                }
            }
            /* if the next triplet is not another regulatory class triplet then it must be a sub-band
               triplet. Skip processing the default channels for this regulatory class triplet and let
               the sub-band triplet restrict the available channels */
            if (((triplet+1) < btampContext->btamp_Remote_AMP_Assoc.HC_pref_num_triplets) &&
                (btampContext->btamp_Remote_AMP_Assoc.HC_pref_triplets[triplet+1][0] != 201))
            {
                continue;
        }
    }
    else
    {
            /* if the regulatory class is valid then this is a sub-band triplet */
            if (regulatoryClass)
            {
                firstChannel = btampContext->btamp_Remote_AMP_Assoc.HC_pref_triplets[triplet][0];
                numChannels = btampContext->btamp_Remote_AMP_Assoc.HC_pref_triplets[triplet][1];
            }
        }

        if (firstChannel && numChannels)
        {
            if (!btampContext->btamp_AMP_Assoc.HC_pref_num_triplets)
            {
                pProfile->operationChannel = firstChannel;
                break;
            }
            else if (((btampContext->btamp_AMP_Assoc.HC_pref_triplets[1][0] + btampContext->btamp_AMP_Assoc.HC_pref_triplets[1][1]) <= firstChannel) ||
               ((firstChannel + numChannels ) <= btampContext->btamp_AMP_Assoc.HC_pref_triplets[1][0]))
            {
                continue;
            }
            else if ((btampContext->btamp_AMP_Assoc.HC_pref_triplets[1][0] + btampContext->btamp_AMP_Assoc.HC_pref_triplets[1][1]) > firstChannel)
            {
                pProfile->operationChannel = firstChannel;
                break;
            }
            else if ((firstChannel + numChannels) > btampContext->btamp_AMP_Assoc.HC_pref_triplets[1][0])
            {
                pProfile->operationChannel =  btampContext->btamp_AMP_Assoc.HC_pref_triplets[1][0];
                break;
            }
        }
    }

    if (!pProfile->operationChannel)
    {
        return VOS_STATUS_E_INVAL;
    }

    /*Set the selected channel */
    sessionid = sme_GetInfraSessionId(hHal);
    /*if there is infra session up already, use that channel only for BT AMP
    connection, else we can use the user preferred one*/
    if(-1 != sessionid)
    {
        pProfile->operationChannel = 
            sme_GetInfraOperationChannel(hHal, 
                                         sessionid);
    }
     
    if(sme_IsChannelValid(hHal, pProfile->operationChannel))
    {         
        btampContext->channel = pProfile->operationChannel;
    }
    else
    {
        //no valid channel, not proceeding with connection
        return VOS_STATUS_E_INVAL;
    }
     
    if ( BT_INITIATOR == btampContext->BAPDeviceRole ) 
    {
      pProfile->ChannelInfo.numOfChannels = 1;
      pProfile->ChannelInfo.ChannelList   = &pProfile->operationChannel;
    }
    else 
    {
        pProfile->ChannelInfo.numOfChannels = 1;
        pProfile->ChannelInfo.ChannelList   = &pProfile->operationChannel;
    }


    // Turn off CB mode
    pProfile->CBMode = eCSR_CB_OFF;

    //set the phyMode to accept anything
    //Taurus means everything because it covers all the things we support
    pProfile->phyMode = eCSR_DOT11_MODE_11n; //eCSR_DOT11_MODE_TAURUS; //eCSR_DOT11_MODE_AUTO; /*eCSR_DOT11_MODE_BEST;*/

    //set the mode in CFG as well
    sme_CfgSetInt(hHal, WNI_CFG_DOT11_MODE, WNI_CFG_DOT11_MODE_11N, NULL, eANI_BOOLEAN_FALSE);

    pProfile->bWPSAssociation = eANI_BOOLEAN_FALSE;

    //Make sure we DON'T request UAPSD
    pProfile->uapsd_mask = 0;

    //return the vosStatus
    return vosStatus;
} //convertToCsrProfile

VOS_STATUS
gotoStarting
(
    ptBtampContext btampContext, /* btampContext value */    
    ptWLAN_BAPEvent bapEvent, /* State machine event */
    eCsrRoamBssType bssType,
    v_U8_t *status    /* return the BT-AMP status here */
) 
{
    VOS_STATUS  vosStatus = VOS_STATUS_SUCCESS;
    eHalStatus  halStatus;
    v_U32_t     parseStatus;
    /* tHalHandle */    
    tHalHandle hHal;
    tBtampTLVHCI_Write_Remote_AMP_ASSOC_Cmd *pBapHCIWriteRemoteAMPAssoc 
        = (tBtampTLVHCI_Write_Remote_AMP_ASSOC_Cmd *) bapEvent->params;
    tBtampAMP_ASSOC btamp_ASSOC; 

    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
    if (NULL == btampContext) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "btampContext is NULL in %s", __func__);

        return VOS_STATUS_E_FAULT;
    }

    hHal = VOS_GET_HAL_CB(btampContext->pvosGCtx);
    if (NULL == hHal) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "hHal is NULL in %s", __func__);

        return VOS_STATUS_E_FAULT;
    }

    //If we are a BT-Responder, we are assuming we are a BT "slave" and we HAVE
    //to "squelch" the slaves frequent (every 1.25ms) polls.

    if (eCSR_BSS_TYPE_WDS_STA == bssType)
    {
        /* Sleep for 300(200) milliseconds - to allow BT through */
        vos_sleep( 200 );
        /* Signal BT Coexistence code in firmware to prefer WLAN */
        WLANBAP_NeedBTCoexPriority ( btampContext, 1);
    }


    //Tell PMC to exit BMPS;
    halStatus = pmcRequestFullPower(
            hHal, 
            WLANBAP_pmcFullPwrReqCB, 
            btampContext,
            eSME_REASON_OTHER);
            // JEZ081210: This has to wait until we sync down from 
            // /main/latest as of 12/4.  We are currently at 12/3.
            //eSME_FULL_PWR_NEEDED_BY_BAP);
    //Need to check the result...because Host may have been told by 
    //OS to go to standby (D2) device state. In that case, I have to 
    //fail the HCI Create Physical Link 

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, 
            "In %s, amp_assoc_remaining_length = %d", __func__, 
            pBapHCIWriteRemoteAMPAssoc->amp_assoc_remaining_length); 
#if 0
    DUMPLOG(1, __func__, "amp_assoc_fragment",  
            pBapHCIWriteRemoteAMPAssoc->amp_assoc_fragment, 
            64);
#endif //0

    //What about parsing the AMP Assoc structure?
    parseStatus = btampUnpackAMP_ASSOC(
            hHal, 
            pBapHCIWriteRemoteAMPAssoc->amp_assoc_fragment, 
            pBapHCIWriteRemoteAMPAssoc->amp_assoc_remaining_length, 
            &btamp_ASSOC);

    /* Unknown or Reserved TLVs are allowed in the write AMP assoc fragment */
    if ((BTAMP_PARSE_SUCCESS != parseStatus ) && (BTAMP_UNKNOWN_TLVS != parseStatus))  
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, parseStatus = %d", __func__, parseStatus);
        *status = WLANBAP_ERROR_INVALID_HCI_CMND_PARAM;
        return VOS_STATUS_E_BADMSG;
    }

    //What about writing the peer MAC address, and other info to the BTAMP 
    //context for this physical link?
    if (btamp_ASSOC.AMP_Assoc_MAC_Addr.present == 1)
    {
        /* Save the peer MAC address */ 
        vos_mem_copy( 
                btampContext->btamp_Remote_AMP_Assoc.HC_mac_addr, 
                btamp_ASSOC.AMP_Assoc_MAC_Addr.mac_addr,   
                sizeof(btampContext->btamp_Remote_AMP_Assoc.HC_mac_addr)); 
        /* Save it in the peer MAC address field */ 
        vos_mem_copy( 
                btampContext->peer_mac_addr, 
                btamp_ASSOC.AMP_Assoc_MAC_Addr.mac_addr,   
                sizeof(btampContext->peer_mac_addr)); 
     }

    if (btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.present == 1)
    {
        /* Save the peer Preferred Channel List */ 
        vos_mem_copy( 
                btampContext->btamp_Remote_AMP_Assoc.HC_pref_country, 
                btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.country,   
                sizeof(btampContext->btamp_Remote_AMP_Assoc.HC_pref_country)); 
        /* Save the peer Preferred Channel List */ 
        btampContext->btamp_Remote_AMP_Assoc.HC_pref_num_triplets = 
            btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.num_triplets;   
        if(WLANBAP_MAX_NUM_TRIPLETS < 
           btampContext->btamp_Remote_AMP_Assoc.HC_pref_num_triplets)
        {
            btampContext->btamp_Remote_AMP_Assoc.HC_pref_num_triplets = 
                WLANBAP_MAX_NUM_TRIPLETS;
        }
        vos_mem_copy( 
                btampContext->btamp_Remote_AMP_Assoc.HC_pref_triplets, 
                btamp_ASSOC.AMP_Assoc_Preferred_Channel_List.triplets,   
                sizeof(btampContext->btamp_Remote_AMP_Assoc.HC_pref_triplets[0]) *
                btampContext->btamp_Remote_AMP_Assoc.HC_pref_num_triplets   
                ); 
    }

    if (btamp_ASSOC.AMP_Assoc_Connected_Channel.present == 1)
    {
        /* Save the peer Connected Channel */ 
        vos_mem_copy( 
                btampContext->btamp_Remote_AMP_Assoc.HC_cnct_country, 
                btamp_ASSOC.AMP_Assoc_Connected_Channel.country,   
                sizeof(btampContext->btamp_Remote_AMP_Assoc.HC_cnct_country)); 
        /* Save the peer Connected Channel */ 
        btampContext->btamp_Remote_AMP_Assoc.HC_cnct_num_triplets = 
            btamp_ASSOC.AMP_Assoc_Connected_Channel.num_triplets;
        if(WLANBAP_MAX_NUM_TRIPLETS < 
           btampContext->btamp_Remote_AMP_Assoc.HC_cnct_num_triplets)
        {
            btampContext->btamp_Remote_AMP_Assoc.HC_cnct_num_triplets = 
                WLANBAP_MAX_NUM_TRIPLETS;
        }
        vos_mem_copy( 
                btampContext->btamp_Remote_AMP_Assoc.HC_cnct_triplets, 
                btamp_ASSOC.AMP_Assoc_Connected_Channel.triplets,   
                sizeof(btampContext->btamp_Remote_AMP_Assoc.HC_cnct_triplets[0]) *
                btampContext->btamp_Remote_AMP_Assoc.HC_cnct_num_triplets
                ); 
    }

    if (btamp_ASSOC.AMP_Assoc_PAL_Capabilities.present == 1)
    {
        /* Save the peer PAL Capabilities */ 
        btampContext->btamp_Remote_AMP_Assoc.HC_pal_capabilities 
            = btamp_ASSOC.AMP_Assoc_PAL_Capabilities.pal_capabilities;
    }

    if (btamp_ASSOC.AMP_Assoc_PAL_Version.present == 1)
    {
        /* Save the peer PAL Version */ 
        btampContext->btamp_Remote_AMP_Assoc.HC_pal_version 
            = btamp_ASSOC.AMP_Assoc_PAL_Version.pal_version;

        btampContext->btamp_Remote_AMP_Assoc.HC_pal_CompanyID 
            = btamp_ASSOC.AMP_Assoc_PAL_Version.pal_CompanyID;

        btampContext->btamp_Remote_AMP_Assoc.HC_pal_subversion 
            = btamp_ASSOC.AMP_Assoc_PAL_Version.pal_subversion;
    }

    //Set Connection Accept Timeout;
    /* Already done in gotoS1() */
    //Set gNeedPhysLinkCompEvent;
    //JEZ081114: This needs to happen earlier. In gotoS1. Right at HCI Create Physical Link
    btampContext->gNeedPhysLinkCompEvent = VOS_TRUE;
    //Clear gDiscRequested;
    btampContext->gDiscRequested = VOS_FALSE;
    //Set gPhysLinkStatus to 0 (no error);
    btampContext->gPhysLinkStatus = WLANBAP_STATUS_SUCCESS;
    //Set gDiscReason to 0 (no reason);
    btampContext->gDiscReason = WLANBAP_STATUS_SUCCESS;
    /* Initiate the link as either START or JOIN */
    //halStatus = csrRoamOpenSession(&newSession);
    /*Added by Luiza:*/

    if (btampContext->isBapSessionOpen == FALSE)
    {

        halStatus = sme_OpenSession(hHal, 
                                    WLANBAP_RoamCallback, 
                                    btampContext,
                                    // <=== JEZ081210: FIXME
                                    //(tANI_U8 *) btampContext->self_mac_addr,  
                                    btampContext->self_mac_addr,  
                                    &btampContext->sessionId);
        if(eHAL_STATUS_SUCCESS == halStatus)
        {
            btampContext->isBapSessionOpen = TRUE;
        }
        else
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                         "sme_OpenSession failed in %s", __func__);
            *status = WLANBAP_ERROR_NO_CNCT;
            return VOS_STATUS_E_FAILURE;
        }
    }
    /* Update the SME Session info for this Phys Link (i.e., for this Phys State Machine instance) */
    //bapUpdateSMESessionForThisPhysLink(newSession, PhysLinkHandle);
    // Taken care of, above
    //halStatus = csrRoamConnect(newSession, bssType);
    // Final
    vosStatus = convertToCsrProfile ( 
            btampContext, /* btampContext value */    
            bssType,
            &btampContext->csrRoamProfile);   /* return the profile info here */
    if(VOS_STATUS_E_INVAL == vosStatus)
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "Incorrect channel to create AMP link %s", __func__);
        *status = WLANBAP_ERROR_NO_SUITABLE_CHANNEL;
        return VOS_STATUS_E_INVAL;
    }
#if 0
    halStatus = sme_RoamConnect(VOS_GET_HAL_CB(btampContext->pvosGCtx), 
            &btampContext->csrRoamProfile, 
            NULL,   /* tScanResultHandle hBssListIn, */ 
            &btampContext->csrRoamId);
#endif //0
//#if 0
    halStatus = sme_RoamConnect(hHal, 
            btampContext->sessionId, 
            &btampContext->csrRoamProfile, 
            &btampContext->csrRoamId);
//#endif //0

    //Map the halStatus into a vosStatus
    return vosStatus;
} //gotoStarting
          
VOS_STATUS
gotoConnecting(
    ptBtampContext btampContext /* btampContext value */
)
{
    VOS_STATUS  vosStatus = VOS_STATUS_SUCCESS;

    /* No longer needed.  This call has been made in gotoStarting(). */
    /* Signal BT Coexistence code in firmware to prefer WLAN */
    WLANBAP_NeedBTCoexPriority ( btampContext, 1);

    return vosStatus;
} //gotoConnecting
 
VOS_STATUS
gotoAuthenticating(
    ptBtampContext btampContext /* btampContext value */
)
{
    VOS_STATUS  vosStatus = VOS_STATUS_SUCCESS;

    /* Signal BT Coexistence code in firmware to prefer WLAN */
    WLANBAP_NeedBTCoexPriority ( btampContext, 1);

    return vosStatus;
} //gotoAuthenticating

#if 0
VOID initRsnSupplicant()
{
/* This is a NO-OP.  The Supplicant waits for MSG 1 */
}
#endif /* 0 */
VOS_STATUS
initRsnSupplicant
(
    ptBtampContext btampContext, /* btampContext value */    
    tWLAN_BAPRole BAPDeviceRole
)
{
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;

    /* This is a NO-OP.  The Supplicant waits for MSG 1 */
    /* Init RSN FSM */
    if (!(suppRsnFsmCreate(btampContext)))
    {
        /* Send Start Event */
        /* RSN_FSM_AUTH_START */
    }
    else
    {
        /* RSN Init Failed */
        vosStatus = VOS_STATUS_E_FAILURE;
    }
    /* This is a NO-OP.  The Supplicant waits for MSG 1 */
    return vosStatus;
}

#if 0
VOID initRsnAuthenticator()
{
/* Signal the Authenticator/Supplicant App that we are associated. */
/* Use an IOCTL?  That the app is hanging a read on? Or use a "special" data packet. Again, that the app is waiting on a receive for. */
}
#endif /* 0 */
VOS_STATUS
initRsnAuthenticator
(
    ptBtampContext btampContext, /* btampContext value */    
    tWLAN_BAPRole BAPDeviceRole
)
{
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    /* Init RSN FSM */
    if (!(authRsnFsmCreate(btampContext)))
    {
        /* Send Start Event */
    }
    else
    {
        /* RSN Init Failed */
        vosStatus = VOS_STATUS_E_FAILURE;
    }
    return vosStatus;
/* Signal the Authenticator/Supplicant App that we are associated. */
/* Use an IOCTL?  That the app is hanging a read on? Or use a "special" data packet. Again, that the app is waiting on a receive for. */
}

/* We have to register our STA with TL */
VOS_STATUS
regStaWithTl
(
    ptBtampContext btampContext, /* btampContext value */    
    tWLAN_BAPRole BAPDeviceRole,
    tCsrRoamInfo *pCsrRoamInfo
)
{
    VOS_STATUS vosStatus;
    WLAN_STADescType staDesc;
    tANI_S8          rssi = 0;

    vos_mem_zero(&staDesc, sizeof(WLAN_STADescType));
    /* Fill in everything I know about the STA */
    btampContext->ucSTAId = staDesc.ucSTAId = pCsrRoamInfo->staId;

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BAP register TL ucSTAId=%d\n", 
               staDesc.ucSTAId );

    /* Fill in the peer MAC address */
    vos_mem_copy( 
            staDesc.vSTAMACAddress.bytes, 
            btampContext->peer_mac_addr, 
            sizeof(btampContext->peer_mac_addr)); 

    /* Fill in the self MAC address */
    vos_mem_copy( 
            staDesc.vSelfMACAddress.bytes, 
            btampContext->self_mac_addr, 
            sizeof(btampContext->peer_mac_addr)); 

    /* Set the STA Type */
    staDesc.wSTAType = WLAN_STA_BT_AMP;

    // Set the QoS field appropriately, if the info available
    if( pCsrRoamInfo->u.pConnectedProfile)
    { 
        btampContext->bapQosCfg.bWmmIsEnabled = //1; 
            pCsrRoamInfo->u.pConnectedProfile->qosConnection;
    }
    else
    {
        btampContext->bapQosCfg.bWmmIsEnabled = 0; 
    }

    // set the QoS field appropriately
    if( btampContext->bapQosCfg.bWmmIsEnabled )
    {
       staDesc.ucQosEnabled = 1;
    }
    else
    {
       staDesc.ucQosEnabled = 0;
    }

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "BAP register TL QoS_enabled=%d\n", 
               staDesc.ucQosEnabled );

    // UMA is ready we inform TL not to do frame 
    // translation for WinMob 6.1
    //*** Not to enabled UMA.
    /* Enable UMA for TX translation only when there is no concurrent session active */
    staDesc.ucSwFrameTXXlation = 1;
    staDesc.ucSwFrameRXXlation = 1; 
    staDesc.ucAddRmvLLC = 0;

    if ( btampContext->ucSecEnabled )
    {
       staDesc.ucProtectedFrame = 1;
    }
    else
    {
       staDesc.ucProtectedFrame = 0;
    }

    staDesc.ucUcastSig = pCsrRoamInfo->ucastSig; 
    staDesc.ucBcastSig = pCsrRoamInfo->bcastSig;
    staDesc.ucInitState = ( btampContext->ucSecEnabled)?
        WLANTL_STA_CONNECTED:WLANTL_STA_AUTHENTICATED;
    staDesc.ucIsReplayCheckValid = VOS_FALSE;
    if(NULL != pCsrRoamInfo->pBssDesc)
    {
        rssi = pCsrRoamInfo->pBssDesc->rssi;
    }
    /* register our STA with TL */
    vosStatus = WLANTL_RegisterSTAClient 
        ( 
         btampContext->pvosGCtx,
         WLANBAP_STARxCB,  
         WLANBAP_TxCompCB,  
         (WLANTL_STAFetchPktCBType)WLANBAP_STAFetchPktCB,
         &staDesc ,
         rssi);   
    if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
    {
       VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, 
                  "%s: WLANTL_RegisterSTAClient() failed to register.  Status= %d [0x%08X]",
                  __func__, vosStatus, vosStatus );
    }                                            
     
    if ( !  btampContext->ucSecEnabled )
    {
       VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_MED,
                  "open/shared auth StaId= %d.  Changing TL state to AUTHENTICATED at Join time", btampContext->ucSTAId);
    
       // Connections that do not need Upper layer auth, transition TL directly
       // to 'Authenticated' state.      
       vosStatus = WLANTL_ChangeSTAState( btampContext->pvosGCtx, staDesc.ucSTAId, 
                                            WLANTL_STA_AUTHENTICATED );
    }                                            
    else
    {

       VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_MED,
                  "ULA auth StaId= %d.  Changing TL state to CONNECTED at Join time", btampContext->ucSTAId );
      
       vosStatus = WLANTL_ChangeSTAState( btampContext->pvosGCtx, staDesc.ucSTAId, 
                                          WLANTL_STA_CONNECTED );
    }                                            

    return VOS_STATUS_SUCCESS;
} /* regStaWithTl */

#if 0
/*==========================================================================

  FUNCTION:  determineChan

  DESCRIPTION:  Return the current channel we are to operate on

============================================================================*/
#endif

VOS_STATUS
determineChan
(
    ptBtampContext btampContext, /* btampContext value */    
    tWLAN_BAPRole BAPDeviceRole,
    v_U32_t *channel,  /* Current channel */
    v_U8_t *status    /* return the BT-AMP status here */
)
{
    VOS_STATUS  vosStatus = VOS_STATUS_SUCCESS;
    v_U32_t     activeFlag;  /* Channel active flag */
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    switch(BAPDeviceRole)
    {
    case BT_INITIATOR:
      /* if an Infra assoc already exists, return that channel. */
      /* or use the results from the Scan to determine the least busy channel.  How? */
      /* For now, just do this. */
      vosStatus = WLANBAP_GetCurrentChannel (btampContext, channel, &activeFlag);
    break;
    case BT_RESPONDER:
      /* return the value obtained from the Preferred Channels field of the AMP Assoc structure from the BT-AMP peer (device A) */
      /* No!  I don't have that yet. */
      /* For now, just do this. */
      vosStatus = WLANBAP_GetCurrentChannel (btampContext, channel, &activeFlag);
    break;
    default:
      *status = WLANBAP_ERROR_HOST_REJ_RESOURCES;     /* return the BT-AMP status here */
      return VOS_STATUS_E_RESOURCES;
  }
  *status = WLANBAP_STATUS_SUCCESS;     /* return the BT-AMP status here */

  return vosStatus;
} // determineChan

VOS_STATUS
gotoDisconnected 
(
    ptBtampContext btampContext /* btampContext value */    
)
{
    VOS_STATUS  vosStatus = VOS_STATUS_SUCCESS;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    //Is it legitimate to always make this call?  
    //What if pmcRequestFullPower wasn't called?
    //Tell PMC to resume BMPS;  /* Whatever the previous BMPS "state" was */
    //Comment this out until such time as we have PMC support
    //halStatus = pmcResumePower ( hHal);

    /* Signal BT Coexistence code in firmware to no longer prefer WLAN */
    WLANBAP_NeedBTCoexPriority ( btampContext, 0);

    //Map the halStatus into a vosStatus
    return vosStatus;
} // gotoDisconnected 

VOS_STATUS
gotoDisconnecting 
(
    ptBtampContext   btampContext, /* btampContext value */    
    v_U8_t   needPhysLinkCompEvent,
    v_U8_t   physLinkStatus,   /* BT-AMP disconnecting status */
//    v_U8_t   statusPresent,    /* BT-AMP disconnecting status present */
    v_U8_t   discRequested,
    v_U8_t   discReason        /* BT-AMP disconnecting reason */
)
{

    // gNeedPhysLinkCompEvent
    btampContext->gNeedPhysLinkCompEvent = needPhysLinkCompEvent;
    // gPhysLinkStatus 
    btampContext->gPhysLinkStatus = physLinkStatus;   /* BT-AMP disconnecting status */
    // gDiscRequested
    btampContext->gDiscRequested = discRequested;
    // gDiscReason 
    btampContext->gDiscReason = discReason;       /* BT-AMP disconnecting reason */

    //WLANBAP_DeInitLinkSupervision( btampHandle);
    //WLANBAP_StopLinkSupervisionTimer(btampContext);

    /* Inform user space that no AMP channel is in use, for AFH purposes */
    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_LOW,
               "Calling send_btc_nlink_msg() with AMP channel = 0");
    send_btc_nlink_msg(WLAN_AMP_ASSOC_DONE_IND, 0);

    return VOS_STATUS_SUCCESS;
} //gotoDisconnecting 

VOS_STATUS
gotoConnected
(
    ptBtampContext   btampContext /* btampContext value */    
)
{
    VOS_STATUS  vosStatus = VOS_STATUS_SUCCESS;
    ptBtampHandle     btampHandle = ( ptBtampHandle)btampContext;
//#if 0
    /* Stop the Connection Accept Timer */
    vosStatus = WLANBAP_StopConnectionAcceptTimer (btampContext);
//#endif
    ///*De-initialize the timer */
    //vosStatus = WLANBAP_DeinitConnectionAcceptTimer(btampContext);

    /* Signal BT Coex in firmware to now honor only priority BT requests */
    WLANBAP_NeedBTCoexPriority ( btampContext, 2);

    // If required after successful Upper layer auth, transition TL 
    // to 'Authenticated' state.      
    if ( btampContext->ucSecEnabled )
    { 
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_MED, 
                "open/shared auth StaId= %d.  Changing TL state to AUTHENTICATED at Join time", btampContext->ucSTAId);
    
        vosStatus = WLANTL_ChangeSTAState( 
                btampContext->pvosGCtx, 
                btampContext->ucSTAId, 
                WLANTL_STA_AUTHENTICATED );
    }

    btampContext->dataPktPending = VOS_FALSE;
    vosStatus = WLANBAP_InitLinkSupervision( btampHandle);
 
    /* Inform user space of the AMP channel selected, for AFH purposes */
    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_LOW,
               "Calling send_btc_nlink_msg() with AMP channel %d", btampContext->channel);
    send_btc_nlink_msg(WLAN_AMP_ASSOC_DONE_IND, btampContext->channel);

    return vosStatus;
} //gotoConnected 


/* the HCI Event signalling routine*/
VOS_STATUS
signalHCIPhysLinkCompEvent
( 
  ptBtampContext btampContext, /* btampContext value */    
  v_U8_t status    /* the BT-AMP status */
)
{
    VOS_STATUS  vosStatus = VOS_STATUS_SUCCESS;
    tBtampHCI_Event bapHCIEvent; /* This now encodes ALL event types */
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    /* Format the Physical Link Complete event to return... */ 
    bapHCIEvent.bapHCIEventCode = BTAMP_TLV_HCI_PHYSICAL_LINK_COMPLETE_EVENT;
    bapHCIEvent.u.btampPhysicalLinkCompleteEvent.present = 1;
    bapHCIEvent.u.btampPhysicalLinkCompleteEvent.status = status;
    bapHCIEvent.u.btampPhysicalLinkCompleteEvent.phy_link_handle 
        = btampContext->phy_link_handle;
    bapHCIEvent.u.btampPhysicalLinkCompleteEvent.ch_number 
        = btampContext->channel;

    if(WLANBAP_STATUS_SUCCESS == status)
    {
        /* Start the Tx packet monitoring timer */
        WLANBAP_StartTxPacketMonitorTimer(btampContext);
    }
    else
    {   //reset the PL handle
        btampContext->phy_link_handle = 0;
    }

    vosStatus = (*btampContext->pBapHCIEventCB) 
        (  
         btampContext->pHddHdl,   /* this refers the BSL per application context */
         &bapHCIEvent, /* This now encodes ALL event types */
         VOS_TRUE /* Flag to indicate assoc-specific event */ 
        );
  
  return vosStatus;
} /* signalHCIPhysLinkCompEvent */

/* the HCI Disconnect Complete Event signalling routine*/
VOS_STATUS
signalHCIPhysLinkDiscEvent
( 
  ptBtampContext btampContext, /* btampContext value */    
  v_U8_t status,    /* the BT-AMP status */
  v_U8_t reason    /* the BT-AMP reason code */
)
{
    VOS_STATUS  vosStatus = VOS_STATUS_SUCCESS;
    tBtampHCI_Event bapHCIEvent; /* This now encodes ALL event types */
    v_U8_t          i;
    tpBtampLogLinkCtx  pLogLinkContext = NULL;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

#ifdef BAP_DEBUG
  /* Trace the tBtampCtx being passed in. */
  VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
            "WLAN BAP Context Monitor: btampContext value = %p in %s:%d", btampContext, __func__, __LINE__ );
#endif //BAP_DEBUG

    /* Loop disconnecting all Logical Links on this Physical Link */
    for (i = 0 ; i < WLANBAP_MAX_LOG_LINKS; i++)
    {
        pLogLinkContext = &(btampContext->btampLogLinkCtx[i]);

        if (pLogLinkContext->present == VOS_TRUE) 
        { 
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, 
                    "WLAN BAP: Deleting logical link entry %d in %s", i,
                    __func__); 

            /* Mark this Logical Link index value as free */
            pLogLinkContext->present = VOS_FALSE; 

            // signalHCIDiscLogLink(status = SUCCESS, reason = CONNECTION_TERM_BY_REMOTE_HOST);
            signalHCIDiscLogLinkCompEvent 
                ( btampContext, 
                  WLANBAP_STATUS_SUCCESS, 
                  i,   // logical link 
                  // I don't know how to signal CONNECTION_TERM_BY_REMOTE_HOST
                  WLANBAP_ERROR_TERM_BY_LOCAL_HOST);
        }
    }

    /*Reset current_log_link_index and total_log_link_index values*/
    btampContext->current_log_link_index = 0; 
    btampContext->total_log_link_index = 0; 

    /* Format the Physical Link Disconnect Complete event to return... */ 
    bapHCIEvent.bapHCIEventCode = BTAMP_TLV_HCI_DISCONNECT_PHYSICAL_LINK_COMPLETE_EVENT;
    bapHCIEvent.u.btampDisconnectPhysicalLinkCompleteEvent.present = 1;
    bapHCIEvent.u.btampDisconnectPhysicalLinkCompleteEvent.status = status;
    bapHCIEvent.u.btampDisconnectPhysicalLinkCompleteEvent.reason = reason;//uncommented to debug
    bapHCIEvent.u.btampDisconnectPhysicalLinkCompleteEvent.phy_link_handle 
        = btampContext->phy_link_handle;

    /* Stop the Tx packet monitoring timer */
    WLANBAP_StopTxPacketMonitorTimer(btampContext);

    /*Need to clean up the phy link handle as we are disconnected at this 
      point
      ?? - do we need to do any more cleanup on this*/
    btampContext->phy_link_handle = 0; 
    vosStatus = (*btampContext->pBapHCIEventCB) 
        (  
         btampContext->pHddHdl,   /* this refers the BSL per application context */
         &bapHCIEvent, /* This now encodes ALL event types */
         VOS_TRUE /* Flag to indicate assoc-specific event */ 
        );
  
  return vosStatus;
} /* signalHCIPhysLinkDiscEvent */

/* the HCI Channel Select Event signalling routine*/
VOS_STATUS
signalHCIChanSelEvent
( 
  ptBtampContext btampContext /* btampContext value */    
)
{
    VOS_STATUS  vosStatus = VOS_STATUS_SUCCESS;
    tBtampHCI_Event bapHCIEvent; /* This now encodes ALL event types */
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    /* Format the Physical Link Disconnect Complete event to return... */ 
    bapHCIEvent.bapHCIEventCode = BTAMP_TLV_HCI_CHANNEL_SELECTED_EVENT;
    bapHCIEvent.u.btampChannelSelectedEvent.present = 1;
    bapHCIEvent.u.btampChannelSelectedEvent.phy_link_handle 
        = btampContext->phy_link_handle;
 
    vosStatus = (*btampContext->pBapHCIEventCB) 
        (  
         btampContext->pHddHdl,   /* this refers the BSL per application context */
         &bapHCIEvent, /* This now encodes ALL event types */
         VOS_TRUE /* Flag to indicate assoc-specific event */ 
        );

  return vosStatus;
} /* signalHCIChanSelEvent */


/* the HCI Disconnect Logical Link Complete Event signalling routine*/
VOS_STATUS
signalHCIDiscLogLinkCompEvent
( 
  ptBtampContext btampContext, /* btampContext value */    
  v_U8_t status,    /* the BT-AMP status */
  v_U16_t log_link_handle,  /* The Logical Link that disconnected*/
  v_U8_t reason    /* the BT-AMP reason code */
)
{
    VOS_STATUS  vosStatus = VOS_STATUS_SUCCESS;
    tBtampHCI_Event bapHCIEvent; /* This now encodes ALL event types */
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    /* Format the Logical Link Disconnect Complete event to return... */ 
    bapHCIEvent.bapHCIEventCode = BTAMP_TLV_HCI_DISCONNECT_LOGICAL_LINK_COMPLETE_EVENT;
    bapHCIEvent.u.btampDisconnectLogicalLinkCompleteEvent.present = 1;
    bapHCIEvent.u.btampDisconnectLogicalLinkCompleteEvent.status = status;
    bapHCIEvent.u.btampDisconnectLogicalLinkCompleteEvent.reason = reason;
    bapHCIEvent.u.btampDisconnectLogicalLinkCompleteEvent.log_link_handle 
    = (log_link_handle  << 8) + btampContext->phy_link_handle;

    vosStatus = (*btampContext->pBapHCIEventCB) 
        (  
         btampContext->pHddHdl,   /* this refers the BSL per application context */
         &bapHCIEvent, /* This now encodes ALL event types */
         VOS_TRUE /* Flag to indicate assoc-specific event */ 
        );
  
  return vosStatus;
} /* signalHCIDiscLogLinkCompEvent */

 
// These are needed to recognize RSN suite types
#define WLANBAP_RSN_OUI_SIZE 4
tANI_U8 pRSNOui00[ WLANBAP_RSN_OUI_SIZE ] = { 0x00, 0x0F, 0xAC, 0x00 }; // group cipher
tANI_U8 pRSNOui01[ WLANBAP_RSN_OUI_SIZE ] = { 0x00, 0x0F, 0xAC, 0x01 }; // WEP-40 or RSN
tANI_U8 pRSNOui02[ WLANBAP_RSN_OUI_SIZE ] = { 0x00, 0x0F, 0xAC, 0x02 }; // TKIP or RSN-PSK
tANI_U8 pRSNOui03[ WLANBAP_RSN_OUI_SIZE ] = { 0x00, 0x0F, 0xAC, 0x03 }; // Reserved
tANI_U8 pRSNOui04[ WLANBAP_RSN_OUI_SIZE ] = { 0x00, 0x0F, 0xAC, 0x04 }; // AES-CCMP
tANI_U8 pRSNOui05[ WLANBAP_RSN_OUI_SIZE ] = { 0x00, 0x0F, 0xAC, 0x05 }; // WEP-104

/* Incoming Association indication validation predicate */
v_U32_t 
validAssocInd
( 
    ptBtampContext btampContext, /* btampContext value */    
    tCsrRoamInfo *pRoamInfo
)
{
    /* tHalHandle */    
    tHalHandle hHal = VOS_GET_HAL_CB(btampContext->pvosGCtx);
    v_U32_t ieLen; 
  
    /* For now, always return true */ 
    return VOS_TRUE;

    /* Check for a valid peer MAC address */
    /* For an incoming Assoc Indication, the peer MAC address
     * should match the value that the BlueTooth AMP 
     * configured us with.
     */
    if ( !vos_mem_compare( btampContext->peer_mac_addr, 
                pRoamInfo->peerMac, 
                           sizeof(btampContext->peer_mac_addr) ))
    {
        /* Return not valid */ 
        return VOS_FALSE;
    }

    /* JEZ081115: For now, ignore the RSN IE */ 
    /* Otherwise, it is valid */ 
    return VOS_TRUE;

    /* Check for a trivial case: IEs missing */
    if( pRoamInfo->prsnIE == NULL )
    {
        //btampContext->ieFields = NULL;
        //btampContext->ieLen = 0; 
         /* Return not valid */ 
        return VOS_FALSE;
    }

   //btampContext->ieLen = GET_IE_LEN_IN_BSS( pBssDesc->length );
   //ieLen = GET_IE_LEN_IN_BSS( pBssDesc->length );
   ieLen = pRoamInfo->rsnIELen;
 
    /* Check for a trivial case: IEs zero length */
    //if( btampContext->ieLen == 0 )
    if( ieLen == 0 )
    {
        //btampContext->ieFields = NULL;
        //btampContext->ieLen = 0; 
         /* Return not valid */ 
        return VOS_FALSE;
    }

    {
        // ---  Start of block ---
    tDot11fBeaconIEs dot11BeaconIEs; 
    tDot11fIESSID *pDot11SSID;
    tDot11fIERSN  *pDot11RSN;
 
    // JEZ081215: This really needs to be updated to just validate the RSN IE.
    // Validating the SSID can be done directly from...
 
    // "Unpack" really wants tpAniSirGlobal (pMac) as its first param.
    // But since it isn't used, I just pass in some arbitrary "context" pointer.
    // So hHalHandle will make it happy.
    dot11fUnpackBeaconIEs((tpAniSirGlobal) hHal, 
            (tANI_U8 *) pRoamInfo->prsnIE,
            ieLen, 
            &dot11BeaconIEs);

    //DUMPLOG(9,  __func__, "dot11BeaconIEs", &dot11BeaconIEs, 64);

    pDot11SSID = &dot11BeaconIEs.SSID; 

    // Assume there wasn't an SSID in the Assoc Request
    btampContext->assocSsidLen = 0;

        if (pDot11SSID->present )
        {

        //DUMPLOG(10,  __func__, "pDot11SSID present", pDot11SSID, 64);

        btampContext->assocSsidLen = pDot11SSID->num_ssid;  
        vos_mem_copy(btampContext->assocSsid, 
                pDot11SSID->ssid, 
                btampContext->assocSsidLen );
        }
        else
        return VOS_FALSE;

    // Check the validity of the SSID against our SSID value
    if ( !vos_mem_compare( btampContext->ownSsid, 
                pDot11SSID->ssid, 
                               btampContext->ownSsidLen ))
        {
        /* Return not valid */ 
        return VOS_FALSE;
    }

    pDot11RSN = &dot11BeaconIEs.RSN; 

    // Assume there wasn't an RSN IE in the Assoc Request
    //btampContext->assocRsnIeLen = 0;

        if (pDot11RSN->present )
        {

        //DUMPLOG(10,  __func__, "pDot11RSN present", pDot11RSN, 64);

        //The 802.11 BT-AMP PAL only supports WPA2-PSK  
        if (!vos_mem_compare(pRSNOui02, //  RSN-PSK
                pDot11RSN->akm_suites[0], 
                WLANBAP_RSN_OUI_SIZE)) 
            return VOS_FALSE;

        //The 802.11 BT-AMP PAL only supports AES-CCMP Unicast  
        if (!vos_mem_compare(pRSNOui04, // AES-CCMP
                pDot11RSN->pwise_cipher_suites[0], 
                WLANBAP_RSN_OUI_SIZE)) 
            return VOS_FALSE;
        }
        else
        return VOS_FALSE;


    } // --- End of block ---

    /* Otherwise, it is valid */ 
    return VOS_TRUE;
} /* validAssocInd */

/* the change state function*/
void 
btampfsmChangeToState
(
    BTAMPFSM_INSTANCEDATA_T *instance, 
    BTAMPFSM_STATES_T state
)
{
    instance->stateVar = state;
    //BTAMPFSM_ENTRY_FLAG_T disconnectedEntry;
  
}

/* Physical Link state machine function */
//int 
VOS_STATUS
btampFsm
(
    //BTAMPFSM_INSTANCEDATA_T *instanceVar
    ptBtampContext btampContext, /* btampContext value */    
//    tBtampSessCtx *tpBtampSessCtx,  /* btampContext value */
    ptWLAN_BAPEvent bapEvent, /* State machine event */
    v_U8_t *status    /* return the BT-AMP status here */
)
{
    /* Retrieve the phy link state machine structure 
     * from the btampContext value 
     */    
    BTAMPFSM_INSTANCEDATA_T *instanceVar;
    v_U32_t msg = bapEvent->event;  /* State machine input event message */
    v_U32_t channel;  /* Current channel */
    v_U32_t activeFlag;  /* Channel active flag */
    VOS_STATUS      vosStatus = VOS_STATUS_SUCCESS;
    ptBtampHandle btampHandle = ( ptBtampHandle)btampContext;
    v_U8_t                   ucSTAId;  /* The StaId (used by TL, PE, and HAL) */
    v_PVOID_t                pHddHdl; /* Handle to return BSL context in */
    tHalHandle     hHal = NULL;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
    /* Validate params */ 
    if (btampHandle == NULL) 
    {
      return VOS_STATUS_E_FAULT;
    }
    instanceVar = &(btampContext->bapPhysLinkMachine);

    hHal = VOS_GET_HAL_CB(btampContext->pvosGCtx);
    if (NULL == hHal) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "hHal is NULL in %s", __func__);

        return VOS_STATUS_E_FAULT;
    }


 
#define CHANNEL_NOT_SELECTED (WLANBAP_GetCurrentChannel (btampContext, &channel, &activeFlag) != VOS_STATUS_SUCCESS)

  /*Initialize BTAMP PAL status code being returned to the btampFsm caller */
  *status = WLANBAP_STATUS_SUCCESS;

    switch(instanceVar->stateVar)
    {

      case DISCONNECTED:
        if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_HCI_PHYSICAL_LINK_CREATE))
        {
          /*Transition from DISCONNECTED to S1 (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "DISCONNECTED", "S1");
 
#if 0
         /* This will have issues in multisession. Need not close the session */
         /* TODO : Need to have better handling */ 
          if(btampContext->isBapSessionOpen == TRUE)//We want to close only BT-AMP Session
          {
          sme_CloseSession(VOS_GET_HAL_CB(btampContext->pvosGCtx),
                                         btampContext->sessionId);
          /*Added by Luiza:*/
          btampContext->isBapSessionOpen = FALSE; 
          }   
#endif

          /* Set BAP device role */
          vosStatus = gotoS1( btampContext, bapEvent, BT_INITIATOR, status); 
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, cmd status is %d", __func__, *status);
           /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,S1);
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_HCI_PHYSICAL_LINK_ACCEPT))
        {
          /*Transition from DISCONNECTED to S1 (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "DISCONNECTED", "S1");
          
#if 0
          if(btampContext->isBapSessionOpen == TRUE)
          {
          sme_CloseSession(VOS_GET_HAL_CB(btampContext->pvosGCtx),
                                         btampContext->sessionId);
          /*Added by Luiza:*/
          btampContext->isBapSessionOpen = FALSE; 
          }
          /*Action code for transition */
#endif

          /* Set BAP device role */
          vosStatus = gotoS1(btampContext, bapEvent, BT_RESPONDER, status);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,S1);
        }
        else
        {
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, in state %s, invalid event msg %d", __func__, "DISCONNECTED", msg);
          /* Intentionally left blank */
        }
      break;

      case S1:
        if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_HCI_WRITE_REMOTE_AMP_ASSOC
           ) && (btampContext->BAPDeviceRole == BT_INITIATOR && !(CHANNEL_NOT_SELECTED)))
        {
          /*Transition from S1 to STARTING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "S1", "STARTING");

          /*Action code for transition */
          vosStatus = determineChan(btampContext, BT_INITIATOR, &channel, status);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,STARTING);
          // This has to be commented out until I get the BT-AMP SME/CSR changes 
          vosStatus = gotoStarting( btampContext, bapEvent, eCSR_BSS_TYPE_WDS_AP, status); 
          if (VOS_STATUS_SUCCESS != vosStatus)
          {
              btampfsmChangeToState(instanceVar, S1);
          }
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_TIMER_CONNECT_ACCEPT_TIMEOUT))
        {
          /*Transition from S1 to DISCONNECTED (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "S1", "DISCONNECTED");

          /*Action code for transition */
          /* Set everything back as dis-connected */    
          gotoDisconnected( btampContext); 
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,DISCONNECTED);
          /*Signal the disconnect */
          signalHCIPhysLinkCompEvent( btampContext, WLANBAP_ERROR_HOST_TIMEOUT);
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_HCI_PHYSICAL_LINK_DISCONNECT))
        {
          /*Transition from S1 to DISCONNECTED (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "S1", "DISCONNECTED");

          /*Action code for transition */
          gotoDisconnected(btampContext);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,DISCONNECTED);
          /*Signal the successful physical link disconnect */
          signalHCIPhysLinkDiscEvent 
              ( btampContext, 
                WLANBAP_STATUS_SUCCESS,
                WLANBAP_ERROR_TERM_BY_LOCAL_HOST);
          /*Signal the unsuccessful physical link creation */
          signalHCIPhysLinkCompEvent( btampContext, WLANBAP_ERROR_NO_CNCT );
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_HCI_WRITE_REMOTE_AMP_ASSOC
                ) && (btampContext->BAPDeviceRole == BT_RESPONDER))
        {
          /*Transition from S1 to STARTING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "S1", "STARTING");

          /*Action code for transition */
          //determineChan(BT_RESPONDER);
          vosStatus = determineChan(btampContext, BT_RESPONDER, &channel, status);
          btampfsmChangeToState(instanceVar,STARTING);//Moved to here to debug
          // This has to be commented out until I get the BT-AMP SME/CSR changes 
          /*Advance outer statevar */
         // btampfsmChangeToState(instanceVar,STARTING);
          vosStatus = gotoStarting( btampContext, bapEvent, eCSR_BSS_TYPE_WDS_STA, status); 
          if (VOS_STATUS_SUCCESS != vosStatus)
          {
              btampfsmChangeToState(instanceVar, S1);
          }
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_HCI_WRITE_REMOTE_AMP_ASSOC
                ) && (btampContext->BAPDeviceRole == BT_INITIATOR && CHANNEL_NOT_SELECTED))
        {
          /*Transition from S1 to SCANNING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "S1", "SCANNING");

          /*Action code for transition */
          gotoScanning(btampContext, BT_RESPONDER, status);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,SCANNING);
        }
        else
        {
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, in state %s, invalid event msg %d", __func__, "S1", msg);
          /* Intentionally left blank */
        }
      break;

      case STARTING:
        if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_MAC_START_BSS_SUCCESS
           ) && (btampContext->BAPDeviceRole == BT_INITIATOR))
        {
          /*Transition from STARTING to CONNECTING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "STARTING", "CONNECTING");

          btampfsmChangeToState(instanceVar,CONNECTING);//Moved to debug

          /*Set the selected channel */
  /*should have been already set */
          btampContext->channel = ( 0 == btampContext->channel )?1:btampContext->channel;

          /*Action code for transition */
          signalHCIChanSelEvent(btampContext);
        
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_HCI_PHYSICAL_LINK_DISCONNECT))
        {
          /*Transition from STARTING to DISCONNECTING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "STARTING", "DISCONNECTING");

          /*Action code for transition */
          //csrRoamDisconnect();
          sme_RoamDisconnect(hHal, 
                  //JEZ081115: Fixme 
                  btampContext->sessionId, 
                  // Danlin, where are the richer reason codes?
                  // I want to be able to convey everything 802.11 supports...
                  eCSR_DISCONNECT_REASON_UNSPECIFIED);

          gotoDisconnecting(
                  btampContext,
                  VOS_TRUE, 
                  WLANBAP_ERROR_NO_CNCT, 
                  //VOS_TRUE,   // Should be VOS_FALSE !!! 
                  VOS_FALSE, 
                  WLANBAP_ERROR_TERM_BY_LOCAL_HOST);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,DISCONNECTING);
          // It is NOT clear that we need to send the Phy Link Disconnect
          // Complete Event here.
          signalHCIPhysLinkDiscEvent 
              ( btampContext, 
                WLANBAP_STATUS_SUCCESS,
                WLANBAP_ERROR_TERM_BY_LOCAL_HOST);
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_CHANNEL_SELECTION_FAILED))
        {
          /*Transition from STARTING to DISCONNECTED (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "STARTING", "DISCONNECTED");

          gotoDisconnected(btampContext);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,DISCONNECTED);
          /*Action code for transition */
          signalHCIPhysLinkCompEvent( btampContext, WLANBAP_ERROR_HOST_REJ_RESOURCES );
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_MAC_START_BSS_SUCCESS
                ) && (btampContext->BAPDeviceRole == BT_RESPONDER))
        {
          /*Transition from STARTING to CONNECTING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "STARTING", "CONNECTING");

          /* Set the selected channel */
          /*should have been already set */
          btampContext->channel = ( 0 == btampContext->channel )?1:btampContext->channel;

          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,CONNECTING);
          /*Action code for transition */
            gotoConnecting(btampContext);
          
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_TIMER_CONNECT_ACCEPT_TIMEOUT))
        {
          /*Transition from STARTING to DISCONNECTING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "STARTING", "DISCONNECTING");

          /*Action code for transition */
          //csrRoamDisconnect();
          sme_RoamDisconnect(hHal, 
                  //JEZ081115: Fixme 
                  btampContext->sessionId, 
                  eCSR_DISCONNECT_REASON_UNSPECIFIED);
          gotoDisconnecting(
                  btampContext, 
                  VOS_TRUE,
                  WLANBAP_ERROR_HOST_TIMEOUT, 
                  VOS_FALSE,
                  0);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,DISCONNECTING);
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_MAC_START_FAILS))
        {
          /*Transition from STARTING to DISCONNECTED (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "STARTING", "DISCONNECTED");

          /*Action code for transition */
          gotoDisconnected(btampContext);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,DISCONNECTED);
          signalHCIPhysLinkCompEvent( btampContext, WLANBAP_ERROR_MAX_NUM_CNCTS );
        }
        else
        {
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, in state %s, invalid event msg %d", __func__, "STARTING", msg);
          /* Intentionally left blank */
        }
      break;

      case CONNECTING:
        if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_MAC_CONNECT_COMPLETED
           ) && (btampContext->BAPDeviceRole == BT_RESPONDER))
        {
          /*Transition from CONNECTING to AUTHENTICATING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "CONNECTING", "AUTHENTICATING");
          //VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "CONNECTING", "CONNECTED");

            gotoAuthenticating(btampContext);
          /*Action code for transition */
          initRsnSupplicant(btampContext, BT_RESPONDER);
#if 1
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,AUTHENTICATING);
#else
          /*Action code for transition */
          signalHCIPhysLinkCompEvent(btampContext, WLANBAP_STATUS_SUCCESS);
          gotoConnected(btampContext);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,CONNECTED);
#endif
          /* register our STA with TL */
          regStaWithTl ( 
                  btampContext, /* btampContext value */ 
                  BT_RESPONDER, 
                  (tCsrRoamInfo *)bapEvent->params);

        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_HCI_PHYSICAL_LINK_DISCONNECT))
        {
          /*Transition from CONNECTING to DISCONNECTING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "CONNECTING", "DISCONNECTING");

          /*Action code for transition */
          //csrRoamDisconnect();
          sme_RoamDisconnect(hHal, 
                  //JEZ081115: Fixme 
                  btampContext->sessionId, 
                  eCSR_DISCONNECT_REASON_UNSPECIFIED);
          gotoDisconnecting(
                  btampContext,
                  VOS_TRUE, 
                  WLANBAP_ERROR_NO_CNCT, 
                  //VOS_TRUE,   // Should be VOS_FALSE !!! 
                  VOS_FALSE, 
                  WLANBAP_ERROR_TERM_BY_LOCAL_HOST);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,DISCONNECTING);
          // It is NOT clear that we need to send the Phy Link Disconnect
          // Complete Event here.
          signalHCIPhysLinkDiscEvent 
              ( btampContext, 
                WLANBAP_STATUS_SUCCESS,
                WLANBAP_ERROR_TERM_BY_LOCAL_HOST);
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_MAC_CONNECT_INDICATION
        //) && (bssDesc indicates an invalid peer MAC Addr or SecParam)){
                ) && !validAssocInd(btampContext, (tCsrRoamInfo *)bapEvent->params))
        {
          /*Transition from CONNECTING to DISCONNECTING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "CONNECTING", "DISCONNECTING");
          /*Action code for transition */
          //csrRoamDisconnect(DEAUTH);
          //JEZ081120: Danlin points out that I could just ignore this
          sme_RoamDisconnect(hHal, 
                  //JEZ081115: Fixme 
                  btampContext->sessionId, 
                  eCSR_DISCONNECT_REASON_DEAUTH);
                  //eCSR_DISCONNECT_REASON_UNSPECIFIED);
          gotoDisconnecting(
                  btampContext,
                  VOS_TRUE, 
                  WLANBAP_ERROR_AUTHENT_FAILURE, 
                  VOS_FALSE, 
                  0);

          /*Set the status code being returned to the btampFsm caller */
          *status = WLANBAP_ERROR_AUTHENT_FAILURE;

          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,DISCONNECTING);
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_MAC_CONNECT_INDICATION
        //) && (bssDesc indicates a valid MAC Addr and SecParam)){
                ) && validAssocInd(btampContext, (tCsrRoamInfo *)bapEvent->params))
        {
          /*Transition from CONNECTING to VALIDATED (both without substates)*/
          //VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "CONNECTING", "VALIDATED");
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "CONNECTING", "AUTHENTICATING");
          //VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "CONNECTING", "CONNECTED");

          /*Action code for transition */
          // JEZ081027: This one is a pain.  Since we are responding in the
          // callback itself.  This messes up my state machine.
          //csrRoamAccept();

          // No!  This is fine. 
          /*Set the status code being returned to the btampFsm caller */
          *status = WLANBAP_STATUS_SUCCESS;

          /* JEZ081215: N.B.: Currently, I don't get the 
           * eCSR_ROAM_RESULT_WDS_ASSOCIATED as an AP.
           * So, I have to register with TL, here. This 
           * seems weird.
           */

          /* register our STA with TL */
          regStaWithTl ( 
                  btampContext, /* btampContext value */ 
                  BT_INITIATOR, 
                  (tCsrRoamInfo *)bapEvent->params );
          
            gotoAuthenticating(btampContext);
          /*Action code for transition */
          initRsnAuthenticator(btampContext, BT_INITIATOR);

#if 1
          /*Advance outer statevar */
          //btampfsmChangeToState(instanceVar,VALIDATED);
          btampfsmChangeToState(instanceVar,AUTHENTICATING);
#else
          /*Action code for transition */
          signalHCIPhysLinkCompEvent(btampContext, WLANBAP_STATUS_SUCCESS);
          gotoConnected(btampContext);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,CONNECTED);
#endif
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_MAC_CONNECT_FAILED))
        {
          /*Transition from CONNECTING to DISCONNECTING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "CONNECTING", "DISCONNECTING");

          /*Action code for transition */
            sme_RoamDisconnect(hHal,
                               btampContext->sessionId,
                               eCSR_DISCONNECT_REASON_UNSPECIFIED);
          /* Section 3.1.8 and section 3.1.9 have contradictory semantics for 0x16. 
           * 3.1.8 is "connection terminated by local host". 3.1.9 is "failed connection".  
           */
          //gotoDisconnecting(FAILED_CONNECTION);
          gotoDisconnecting(
                  btampContext,
                  VOS_TRUE, 
                  WLANBAP_ERROR_TERM_BY_LOCAL_HOST, //FAILED_CONNECTION
                  VOS_FALSE, 
                  0);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,DISCONNECTING);
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_TIMER_CONNECT_ACCEPT_TIMEOUT))
        {
          /*Transition from CONNECTING to DISCONNECTING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "CONNECTING", "DISCONNECTING");

          /*Action code for transition */
          //csrRoamDisconnect();
          sme_RoamDisconnect(hHal, 
                  //JEZ081115: Fixme 
                  btampContext->sessionId, 
                  eCSR_DISCONNECT_REASON_UNSPECIFIED);
          gotoDisconnecting(
                  btampContext,
                  VOS_TRUE, 
                  WLANBAP_ERROR_HOST_TIMEOUT,
                  VOS_FALSE, 
                  0);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,DISCONNECTING);
        }
        else
        {
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, in state %s, invalid event msg %d", __func__, "CONNECTING", msg);
          /* Intentionally left blank */
        }
      break;

      case AUTHENTICATING:
        if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_RSN_SUCCESS
           ) && (btampContext->BAPDeviceRole == BT_RESPONDER))
        {
          /*Transition from AUTHENTICATING to KEYING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "AUTHENTICATING", "KEYING");

          /*Action code for transition */
          //sme_RoamSetContext(); 
#if 0
          sme_RoamSetKey(
                  VOS_GET_HAL_CB(btampContext->pvosGCtx), 
                  btampContext->sessionId, 
                  tSirMacAddr peerBssId, 
                  eCsrEncryptionType encryptType,
                  tANI_U16 keyLength, 
                  tANI_U8 *pKey,
                  VOS_TRUE,   // TRUE 
                  tANI_U8 paeRole);
#endif //0
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,KEYING);
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_RSN_SUCCESS
                ) && (btampContext->BAPDeviceRole == BT_INITIATOR))
        {
          /*Transition from AUTHENTICATING to KEYING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "AUTHENTICATING", "KEYING");

          /*Action code for transition */
          //sme_RoamSetContext(); 
#if 0
          sme_RoamSetKey(
                  VOS_GET_HAL_CB(btampContext->pvosGCtx), 
                  btampContext->sessionId, 
                  tSirMacAddr peerBssId, 
                  eCsrEncryptionType encryptType,
                  tANI_U16 keyLength, 
                  tANI_U8 *pKey,
                  VOS_TRUE,   // TRUE 
                  tANI_U8 paeRole);
#endif //0
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,KEYING);
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_TIMER_CONNECT_ACCEPT_TIMEOUT))
        {
          /*Transition from AUTHENTICATING to DISCONNECTING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s ConnectAcceptTimeout", __func__, "AUTHENTICATING", "DISCONNECTING");

          gotoDisconnecting(
                  btampContext,
                  VOS_TRUE, 
                  WLANBAP_ERROR_HOST_TIMEOUT,
                  VOS_FALSE, 
                  0);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,DISCONNECTING);
          /*Action code for transition */
         sme_RoamDisconnect(hHal, 
                  //JEZ081115: Fixme 
                  btampContext->sessionId, 
                  eCSR_DISCONNECT_REASON_UNSPECIFIED);
         
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_HCI_PHYSICAL_LINK_DISCONNECT))
        {
          /*Transition from AUTHENTICATING to DISCONNECTING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s Physicallink Disconnect", __func__, "AUTHENTICATING", "DISCONNECTING");

          /*Action code for transition */
          //csrRoamDisconnect();
          sme_RoamDisconnect(hHal, 
                  //JEZ081115: Fixme 
                  btampContext->sessionId, 
                  eCSR_DISCONNECT_REASON_UNSPECIFIED);
          gotoDisconnecting(
                  btampContext,
                  VOS_TRUE, 
                  WLANBAP_ERROR_NO_CNCT,
                  //VOS_TRUE,   // Should be VOS_FALSE !!! 
                  VOS_FALSE, 
                  WLANBAP_ERROR_TERM_BY_LOCAL_HOST);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,DISCONNECTING);
          // It is NOT clear that we need to send the Phy Link Disconnect
          // Complete Event here.
          signalHCIPhysLinkDiscEvent 
              ( btampContext, 
                WLANBAP_STATUS_SUCCESS,
                WLANBAP_ERROR_TERM_BY_LOCAL_HOST);
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_RSN_FAILURE))
        {
          /*Transition from AUTHENTICATING to DISCONNECTING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s RSN Failure", __func__, "AUTHENTICATING", "DISCONNECTING");

          /*Action code for transition */
          //csrRoamDisconnect(DEAUTH);
          sme_RoamDisconnect(hHal, 
                  //JEZ081115: Fixme 
                  btampContext->sessionId, 
                  eCSR_DISCONNECT_REASON_DEAUTH);
                  //eCSR_DISCONNECT_REASON_UNSPECIFIED);
          gotoDisconnecting(
                  btampContext,
                  VOS_TRUE, 
                  WLANBAP_ERROR_AUTHENT_FAILURE, 
                  VOS_FALSE, 
                  0);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,DISCONNECTING);
        }
        else
        {
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, in state %s, invalid event msg %d", __func__, "AUTHENTICATING", msg);
          /* Intentionally left blank */
        }
      break;

      case CONNECTED:
        if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_HCI_PHYSICAL_LINK_DISCONNECT))
        {
          /*Transition from CONNECTED to DISCONNECTING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "CONNECTED", "DISCONNECTING");

            gotoDisconnecting(
                  btampContext,
                  VOS_FALSE, 
                  0, 
                  VOS_TRUE, 
                  WLANBAP_ERROR_TERM_BY_LOCAL_HOST);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,DISCONNECTING);

          WLANBAP_DeInitLinkSupervision(( ptBtampHandle)btampContext);
          /*Action code for transition */
          //csrRoamDisconnect();
          sme_RoamDisconnect(hHal, 
                  //JEZ081115: Fixme 
                  btampContext->sessionId, 
                  eCSR_DISCONNECT_REASON_UNSPECIFIED);
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_MAC_INDICATES_MEDIA_DISCONNECTION))
        {

          /*Transition from CONNECTED to DISCONNECTING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "CONNECTED", "DISCONNECTING");
          WLANBAP_DeInitLinkSupervision(( ptBtampHandle)btampContext);

          gotoDisconnecting(
                  btampContext,
                  VOS_FALSE, 
                  0, 
                  VOS_TRUE, 
                  WLANBAP_ERROR_TERM_BY_LOCAL_HOST);
            /*Action code for transition */
            sme_RoamDisconnect(hHal,
                               btampContext->sessionId,
                               eCSR_DISCONNECT_REASON_UNSPECIFIED);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,DISCONNECTING);
        }
        else
        {
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, in state %s, invalid event msg %d", __func__, "CONNECTED", msg);
          /* Intentionally left blank */
        }
      break;

/* JEZ081107: This will only work if I have already signalled the disconnect complete
 * event in every case where a physical link complete event is required. And a
 * disconnect was requested.
 *      - - -
 * And only if I check for gNeedPhysLinkCompEvent BEFORE I check gDiscRequested.
 * Naw! Not necessary.
 */
      case DISCONNECTING:
         VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, Entered DISCONNECTING:", __func__);//Debug statement
        if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_MAC_READY_FOR_CONNECTIONS
           ) && (btampContext->gDiscRequested == VOS_TRUE))
        {
          /*Transition from DISCONNECTING to DISCONNECTED (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "DISCONNECTING", "DISCONNECTED");

    //Clear gDiscRequested;
    btampContext->gDiscRequested = VOS_FALSE;
           
    if(btampContext->BAPDeviceRole == BT_INITIATOR) 
          {
                if(!VOS_IS_STATUS_SUCCESS(vos_lock_acquire(&btampContext->bapLock)))
                {
                   VOS_TRACE(VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,"btampFsm, Get LOCK Fail");
                }
              authRsnFsmFree(btampContext);
                if(!VOS_IS_STATUS_SUCCESS(vos_lock_release(&btampContext->bapLock)))
                {
                   VOS_TRACE(VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,"btampFsm, Release LOCK Fail");
                }
          }
          else if(btampContext->BAPDeviceRole == BT_RESPONDER)
          {
              suppRsnFsmFree(btampContext);
          }

          /* Lookup the StaId using the phy_link_handle and the BAP context */ 
          vosStatus = WLANBAP_GetStaIdFromLinkCtx ( 
                    btampHandle,  /* btampHandle value in  */ 
                    btampContext->phy_link_handle,  /* phy_link_handle value in */
                    &ucSTAId,  /* The StaId (used by TL, PE, and HAL) */
                    &pHddHdl); /* Handle to return BSL context */
          if ( VOS_STATUS_SUCCESS != vosStatus ) 
          {
              VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO,
                          "Unable to retrieve STA Id from BAP context and phy_link_handle in %s", __func__);
              return VOS_STATUS_E_FAULT;
          }
          WLANTL_ClearSTAClient(btampContext->pvosGCtx, ucSTAId);

    //      gotoDisconnected(btampContext);

      //    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s:In DISCONNECTING-changing outer state var to DISCONNECTED", __func__);
          /*Advance outer statevar */
        //  btampfsmChangeToState(instanceVar,DISCONNECTED);

          signalHCIPhysLinkDiscEvent 
              ( btampContext, 
                WLANBAP_STATUS_SUCCESS,
                btampContext->gDiscReason);
          /*sme_CloseSession(VOS_GET_HAL_CB(btampContext->pvosGCtx),
                                         btampContext->sessionId);*/
          /*Action code for transition */
          gotoDisconnected(btampContext);

          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "%s:In DISCONNECTING-changing outer state var to DISCONNECTED", __func__);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,DISCONNECTED);
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_MAC_READY_FOR_CONNECTIONS
                ) && (btampContext->gNeedPhysLinkCompEvent == VOS_TRUE))
        {
          /*Transition from DISCONNECTING to DISCONNECTED (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s gNeedPhysLinkComp TRUE", __func__, "DISCONNECTING", "DISCONNECTED");
          if(btampContext->BAPDeviceRole == BT_INITIATOR) 
          {
              if(!VOS_IS_STATUS_SUCCESS(vos_lock_acquire(&btampContext->bapLock)))
              {
                  VOS_TRACE(VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,"btampFsm, Get LOCK Fail");
              }
              authRsnFsmFree(btampContext);
              if(!VOS_IS_STATUS_SUCCESS(vos_lock_release(&btampContext->bapLock)))
              {
                  VOS_TRACE(VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,"btampFsm, Release LOCK Fail");
              }

          }
          else if(btampContext->BAPDeviceRole == BT_RESPONDER)
          {
              suppRsnFsmFree(btampContext);
          }
          /* Lookup the StaId using the phy_link_handle and the BAP context */ 
          vosStatus = WLANBAP_GetStaIdFromLinkCtx ( 
                    btampHandle,  /* btampHandle value in  */ 
                    btampContext->phy_link_handle,  /* phy_link_handle value in */
                    &ucSTAId,  /* The StaId (used by TL, PE, and HAL) */
                    &pHddHdl); /* Handle to return BSL context */
          if ( VOS_STATUS_SUCCESS != vosStatus ) 
          {
              VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO,
                          "Unable to retrieve STA Id from BAP context and phy_link_handle in %s", __func__);
              return VOS_STATUS_E_FAULT;
          }
          WLANTL_ClearSTAClient(btampContext->pvosGCtx, ucSTAId);


          /*Action code for transition */
         // signalHCIPhysLinkCompEvent(btampContext, WLANBAP_ERROR_NO_CNCT/*btampContext->gPhysLinkStatus*/);
          signalHCIPhysLinkCompEvent(btampContext, btampContext->gPhysLinkStatus);
          gotoDisconnected(btampContext);
          /*sme_CloseSession(VOS_GET_HAL_CB(btampContext->pvosGCtx),
                                         btampContext->sessionId);*/
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,DISCONNECTED);
         // signalHCIPhysLinkCompEvent(btampContext, btampContext->gPhysLinkStatus);
        }
        else
        {
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, in state %s, invalid event msg %d", __func__, "DISCONNECTING", msg);
          /* Intentionally left blank */
        }
      break;

      case KEYING:
        if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_TIMER_CONNECT_ACCEPT_TIMEOUT))
        {
          /*Transition from KEYING to DISCONNECTING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "KEYING", "DISCONNECTING");

          /*Action code for transition */
          //csrRoamDisconnect();
          sme_RoamDisconnect(hHal, 
                  //JEZ081115: Fixme 
                  btampContext->sessionId, 
                  eCSR_DISCONNECT_REASON_UNSPECIFIED);
          gotoDisconnecting(
                  btampContext,
                  VOS_TRUE, 
                  WLANBAP_ERROR_HOST_TIMEOUT,
                  VOS_FALSE, 
                  0);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,DISCONNECTING);
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_HCI_PHYSICAL_LINK_DISCONNECT))
        {
          /*Transition from KEYING to DISCONNECTING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "KEYING", "DISCONNECTING");

          /*Action code for transition */
          //csrRoamDisconnect();
          sme_RoamDisconnect(hHal, 
                  //JEZ081115: Fixme 
                  btampContext->sessionId, 
                  eCSR_DISCONNECT_REASON_UNSPECIFIED);

          gotoDisconnecting(
                  btampContext,
                  VOS_TRUE, 
                  WLANBAP_ERROR_NO_CNCT,
                  //VOS_TRUE,   // Should be VOS_FALSE !!! 
                  VOS_FALSE, 
                  WLANBAP_ERROR_TERM_BY_LOCAL_HOST);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,DISCONNECTING);

          // It is NOT clear that we need to send the Phy Link Disconnect
          // Complete Event here.
          signalHCIPhysLinkDiscEvent 
              ( btampContext, 
                WLANBAP_STATUS_SUCCESS,
                WLANBAP_ERROR_TERM_BY_LOCAL_HOST);
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_MAC_KEY_SET_SUCCESS))
        {
          /*Transition from KEYING to CONNECTED (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "KEYING", "CONNECTED");

          /*Action code for transition */
          gotoConnected(btampContext);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,CONNECTED);
          signalHCIPhysLinkCompEvent(btampContext, WLANBAP_STATUS_SUCCESS);
        }
        else
        {
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, in state %s, invalid event msg %d", __func__, "KEYING", msg);
          /* Intentionally left blank */
        }
      break;

      case SCANNING:
        if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_MAC_SCAN_COMPLETE))
        {
          /*Transition from SCANNING to STARTING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "SCANNING", "STARTING");

          /*Action code for transition */
          vosStatus = determineChan(btampContext, BT_INITIATOR, &channel, status);
          // This has to be commented out until I get the BT-AMP SME/CSR changes 
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,STARTING);
          vosStatus = gotoStarting( btampContext, bapEvent, eCSR_BSS_TYPE_WDS_AP, status); 
          if (VOS_STATUS_SUCCESS != vosStatus)
          {
              btampfsmChangeToState(instanceVar, SCANNING);
          }
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_TIMER_CONNECT_ACCEPT_TIMEOUT))
        {
          /*Transition from SCANNING to DISCONNECTED (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "SCANNING", "DISCONNECTED");

          /*Action code for transition */
          gotoDisconnected(btampContext);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,DISCONNECTED);

          signalHCIPhysLinkCompEvent( btampContext, WLANBAP_ERROR_HOST_TIMEOUT);
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_HCI_PHYSICAL_LINK_DISCONNECT))
        {
          /*Transition from SCANNING to DISCONNECTED (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "SCANNING", "DISCONNECTED");

          /*Action code for transition */
          gotoDisconnected(btampContext);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,DISCONNECTED);
          signalHCIPhysLinkDiscEvent 
              ( btampContext, 
                WLANBAP_STATUS_SUCCESS,
                WLANBAP_ERROR_TERM_BY_LOCAL_HOST);
          signalHCIPhysLinkCompEvent( btampContext, WLANBAP_ERROR_NO_CNCT);
        }
        else
        {
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, in state %s, invalid event msg %d", __func__, "SCANNING", msg);
          /* Intentionally left blank */
        }
      break;

      case VALIDATED:
        if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_MAC_CONNECT_COMPLETED
           ) && (btampContext->BAPDeviceRole == BT_INITIATOR))
        {
          /*Transition from VALIDATED to AUTHENTICATING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "VALIDATED", "AUTHENTICATING");

            gotoAuthenticating(btampContext);
          /*Action code for transition */
          initRsnAuthenticator(btampContext, BT_INITIATOR);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,AUTHENTICATING);
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_TIMER_CONNECT_ACCEPT_TIMEOUT))
        {
          /*Transition from VALIDATED to DISCONNECTING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "VALIDATED", "DISCONNECTING");

          /*Action code for transition */
          //csrRoamDisconnect();
          sme_RoamDisconnect(hHal, 
                  //JEZ081115: Fixme 
                  btampContext->sessionId, 
                  eCSR_DISCONNECT_REASON_UNSPECIFIED);
          gotoDisconnecting(
                  btampContext,
                  VOS_TRUE, 
                  WLANBAP_ERROR_HOST_TIMEOUT,
                  VOS_FALSE, 
                  0);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,DISCONNECTING);
        }
        else if((msg==(BTAMPFSM_EVENT_T)eWLAN_BAP_HCI_PHYSICAL_LINK_DISCONNECT))
        {
          /*Transition from VALIDATED to DISCONNECTING (both without substates)*/
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, from state %s => %s", __func__, "VALIDATED", "DISCONNECTING");

          /*Action code for transition */
          //csrRoamDisconnect();
          sme_RoamDisconnect(hHal, 
                  //JEZ081115: Fixme 
                  btampContext->sessionId, 
                  eCSR_DISCONNECT_REASON_UNSPECIFIED);

          gotoDisconnecting(
                  btampContext,
                  VOS_TRUE, 
                  WLANBAP_ERROR_NO_CNCT,
                  //VOS_TRUE,   // Should be VOS_FALSE !!! 
                  VOS_FALSE, 
                  WLANBAP_ERROR_TERM_BY_LOCAL_HOST);
          /*Advance outer statevar */
          btampfsmChangeToState(instanceVar,DISCONNECTING);

          // It is NOT clear that we need to send the Phy Link Disconnect
          // Complete Event here.
          signalHCIPhysLinkDiscEvent 
              ( btampContext, 
                WLANBAP_STATUS_SUCCESS,
                WLANBAP_ERROR_TERM_BY_LOCAL_HOST);
        }
        else
        {
          VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, in state %s, invalid event msg %d", __func__, "VALIDATED", msg);
          /* Intentionally left blank */
        }
      break;

      default:
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, invalid state %d", __func__, instanceVar->stateVar);
        /*Intentionally left blank*/
      break;
  }

  return vosStatus;
}

VOS_STATUS btampEstablishLogLink(ptBtampContext btampContext)
{  
   VOS_STATUS      vosStatus = VOS_STATUS_SUCCESS;
   vos_msg_t       msg;

   tAniBtAmpLogLinkReq *pMsg;

   pMsg = vos_mem_malloc(sizeof(tAniBtAmpLogLinkReq));
   if ( NULL == pMsg ) 
   {
      VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "In %s, failed to allocate mem for req", __func__);
      return VOS_STATUS_E_NOMEM;
   }

   pMsg->msgType = pal_cpu_to_be16((tANI_U16)eWNI_SME_BTAMP_LOG_LINK_IND);
   pMsg->msgLen = (tANI_U16)sizeof(tAniBtAmpLogLinkReq);
   pMsg->sessionId = btampContext->sessionId;
   pMsg->btampHandle = btampContext;

   msg.type = eWNI_SME_BTAMP_LOG_LINK_IND;
   msg.bodyptr = pMsg;
   msg.reserved = 0;

   if(VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MQ_ID_SME, &msg))
   {
       VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "In %s, failed to post msg to self", __func__);
       vos_mem_free(pMsg);
       vosStatus = VOS_STATUS_E_FAILURE;
   }
   return vosStatus;
}

void btampEstablishLogLinkHdlr(void* pMsg)
{
    tAniBtAmpLogLinkReq *pBtAmpLogLinkReq = (tAniBtAmpLogLinkReq*)pMsg;
    ptBtampContext btampContext;

    if(pBtAmpLogLinkReq)
    {
        btampContext = (ptBtampContext)pBtAmpLogLinkReq->btampHandle;
        if(NULL != btampContext)
        {
            vos_sleep( 200 );
            WLAN_BAPEstablishLogicalLink(btampContext);
        }
        else
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "In %s, btampContext is NULL", __func__);                  
            return;
        }
            
    }
    else
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "In %s, pBtAmpLogLinkReq is NULL", __func__);    
    }
    return;
}

