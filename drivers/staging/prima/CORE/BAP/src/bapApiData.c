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

                      b a p A p i D a t a . C
                                               
  OVERVIEW:
  
  This software unit holds the implementation of the WLAN BAP modules
  "platform independent" Data path functions.
  
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


   $Header: /cygdrive/e/Builds/M7201JSDCAAPAD52240B/WM/platform/msm7200/Src/Drivers/SD/ClientDrivers/WLAN/QCT/CORE/BAP/src/bapApiData.c,v 1.4 2008/11/10 22:34:22 jzmuda Exp jzmuda $$DateTime$$Author: jzmuda $


  when        who     what, where, why
----------    ---    --------------------------------------------------------
2008-09-15    jez     Created module

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
//#include "wlan_qct_tl.h"
#include "vos_trace.h"
//I need the TL types and API
#include "wlan_qct_tl.h"

#include "wlan_qct_hal.h"

/* BT-AMP PAL API header file */ 
#include "bapApi.h" 
#include "bapInternal.h" 
#include "bapApiTimer.h"

//#define BAP_DEBUG
/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
/*Endian-ness definitions*/

#undef BAP_LITTLE_BIT_ENDIAN
#define BAP_LITTLE_BIT_ENDIAN

/*LLC header definitions*/

/* Length of the LLC header*/
#define WLANBAP_LLC_HEADER_LEN   8 
#if 0
/*Offset of the OUI field inside the LLC/SNAP header*/
#define WLANBAP_LLC_OUI_OFFSET                 3

/*Size of the OUI type field inside the LLC/SNAP header*/
#define WLANBAP_LLC_OUI_SIZE                   3

/*Offset of the protocol type field inside the LLC/SNAP header*/
#define WLANBAP_LLC_PROTO_TYPE_OFFSET  (WLANBAP_LLC_OUI_OFFSET +  WLANBAP_LLC_OUI_SIZE)

/*Size of the protocol type field inside the LLC/SNAP header*/
#define WLANBAP_LLC_PROTO_TYPE_SIZE            2
#endif

/*BT-AMP protocol type values*/
/*BT-AMP packet of type data*/
#define WLANBAP_BT_AMP_TYPE_DATA       0x0001

/*BT-AMP packet of type activity report*/
#define WLANBAP_BT_AMP_TYPE_AR         0x0002

/*BT-AMP packet of type security frame*/
#define WLANBAP_BT_AMP_TYPE_SEC        0x0003

/*802.3 header definitions*/
#define  WLANBAP_802_3_HEADER_LEN             14

/* Offset of DA field in a 802.3 header*/
#define  WLANBAP_802_3_HEADER_DA_OFFSET        0

//*BT-AMP packet LLC OUI value*/
const v_U8_t WLANBAP_BT_AMP_OUI[] =  {0x00, 0x19, 0x58 };

/*LLC header value*/
static v_U8_t WLANBAP_LLC_HEADER[] =  {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00 };

/* HCI header definitions*/

// Define the length of the ACL data packet HCI header
#define WLANBAP_HCI_ACL_HEADER_LEN    4

// Debug related defines
//#define DBGLOG printf
#define DUMPLOG_ON
#ifdef DUMPLOG_ON
#define DUMPLOG(n, name1, name2, aStr, size) do {                       \
        int i;                                                          \
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,"%d. %s: %s = \n", n, name1, name2); \
        for (i = 0; i < size; i++)                                      \
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,"%2.2x%s", ((unsigned char *)aStr)[i], i % 16 == 15 ? "\n" : " "); \
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,"\n");      \
    } while (0)
#else
#define DUMPLOG(n, name1, name2, aStr, size)
#endif

#if 0
// Debug related defines
#define DBGLOG printf
#define DUMPLOG
#if defined DUMPLOG
#define DUMPLOG(n, name1, name2, aStr, size) do {                       \
        int i;                                                          \
        DBGLOG("%d. %s: %s = \n", n, name1, name2);                     \
        for (i = 0; i < size; i++)                                      \
            DBGLOG("%2.2x%s", ((unsigned char *)aStr)[i], i % 16 == 15 ? "\n" : " "); \
        DBGLOG("\n");                                                   \
    } while (0)
#else
#define DUMPLOG(n, name1, name2, aStr, size)
#endif
#endif

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/
// Don't we have this type defined somewhere?
#if 0
/* 802.3 header */
typedef struct 
{
 /* Destination address field */
 v_U8_t   vDA[VOS_MAC_ADDR_SIZE];

 /* Source address field */
 v_U8_t   vSA[VOS_MAC_ADDR_SIZE];

 /* Length field */
 v_U16_t  usLenType;  /* Num bytes in info field (i.e., exclude 802.3 hdr) */
                      /* Max length 1500 (0x5dc) (What about 0x5ee? That
                       * includes 802.3 Header and FCS.) */
}WLANBAP_8023HeaderType;
#endif

/**
 * \brief HCI ACL Data packet format
 *
 *     0      7 8      15 16    23 24    31
 *    +--------+----+----+--------+--------+
 *    | phy_   |log_| PB/|   Data Total    |
 *    | link_  |lnk_| BC |      Length     |
 *    | handle |hndl|Flag|                 |
 *    +--------+----+----+--------+--------+
 *    |                                    |
 *    |                Data                |
 *    ~                                    ~
 *    +--------+---------+--------+--------+
 *
 *  NB: 
 *  This is in little-endian
 *  1) phy_link_handle is the first 8 bits
 *  2) log_link_handle is the next 4 bits 
 *  3) PB flag is the next 2 bits
 *  4) BC flags is the next 2 bits
 *  5) Total length of the data field is the next 16 bits
 *
 */

typedef struct 
{

#ifndef BAP_LITTLE_BIT_ENDIAN

   v_U8_t phyLinkHandle; /* do I have to reverse the byte? I think so... */

   v_U8_t BCFlag :2;
   v_U8_t PBFlag :2;
   v_U8_t logLinkHandle :4;

   v_U16_t dataLength;  /* do I have to reverse each byte? and then reverse the two bytes?  I think so... */

#else

   v_U8_t phyLinkHandle;

   v_U8_t logLinkHandle :4;
   v_U8_t PBFlag :2;
   v_U8_t BCFlag :2;

   v_U16_t dataLength;  /* Max length WLANBAP_MAX_80211_PAL_PDU_SIZE (1492) */

#endif

} WLANBAP_HCIACLHeaderType;



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

#define WLANBAP_DEBUG_FRAME_BYTE_PER_LINE    16
#define WLANBAP_DEBUG_FRAME_BYTE_PER_BYTE    4

/*===========================================================================

  FUNCTION    WLANBAP_XlateTxDataPkt

  DESCRIPTION 

    HDD will call this API when it has a HCI Data Packet and it wants 
    to translate it into a 802.3 LLC frame - ready to send using TL.


  PARAMETERS 

    btampHandle: The BT-AMP PAL handle returned in WLANBAP_GetNewHndl.
    phy_link_handle: Used by BAP to indentify the WLAN assoc. (StaId) 

    pucAC:       Pointer to return the access category 
    vosDataBuff: The data buffer containing the BT-AMP packet to be 
                 translated to an 802.3 LLC frame
    tlMetaInfo:  return meta info gleaned from the outgoing frame, here.
   
  RETURN VALUE

    The result code associated with performing the operation  

    VOS_STATUS_E_INVAL:  Input parameters are invalid 
    VOS_STATUS_E_FAULT:  BAP handle is NULL  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANBAP_XlateTxDataPkt
( 
    ptBtampHandle     btampHandle,  /* Used by BAP to identify the actual session
                                      and therefore addresses */ 
    v_U8_t            phy_link_handle,  /* Used by BAP to indentify the WLAN assoc. (StaId) */
    WLANTL_ACEnumType    *pucAC,        /* Return the AC here */
    WLANTL_MetaInfoType  *tlMetaInfo, /* Return the MetaInfo here. An assist to WLANBAP_STAFetchPktCBType */
    vos_pkt_t        *vosDataBuff
)
{
    ptBtampContext           pBtampCtx = (ptBtampContext) btampHandle; 
    tpBtampLogLinkCtx        pLogLinkContext;
    WLANBAP_8023HeaderType   w8023Header;
    WLANBAP_HCIACLHeaderType hciACLHeader;
    v_U8_t                   aucLLCHeader[WLANBAP_LLC_HEADER_LEN];
    VOS_STATUS               vosStatus;
    v_U8_t                   ucSTAId;  /* The StaId (used by TL, PE, and HAL) */
    v_PVOID_t                pHddHdl; /* Handle to return BSL context in */
    v_U16_t                  headerLength;  /* The 802.3 frame length*/
    v_U16_t                  protoType = WLANBAP_BT_AMP_TYPE_DATA;  /* The protocol type bytes*/
    v_U32_t                  value = 0;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

 
    /*------------------------------------------------------------------------
        Sanity check params
      ------------------------------------------------------------------------*/
    if ( NULL == pBtampCtx) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "Invalid BAP handle value in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

    // Here, I have to make the assumption that this is an 
    // HCI ACL Data packet that I am being handed. 
    vosStatus = vos_pkt_pop_head( vosDataBuff, &hciACLHeader, WLANBAP_HCI_ACL_HEADER_LEN);

    if ( VOS_STATUS_SUCCESS != vosStatus ) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                  "WLAN BAP: Failed to pop HCI ACL header from packet %d",
                  vosStatus);

        return vosStatus;
    }

    // JEZ081003: Remove this after debugging 
    // Sanity check the phy_link_handle value 

    if ( phy_link_handle != hciACLHeader.phyLinkHandle ) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                "WLAN BAP: phy_link_handle mismatch in %s phy_link_handle=%d hciACLHeader.phyLinkHandle=%d",
                __func__, phy_link_handle, hciACLHeader.phyLinkHandle);
        return VOS_STATUS_E_INVAL;
    }


    /* Lookup the StaId using the phy_link_handle and the BAP context */ 

    vosStatus = WLANBAP_GetStaIdFromLinkCtx ( 
            btampHandle,  /* btampHandle value in  */ 
            phy_link_handle,  /* phy_link_handle value in */
            &ucSTAId,  /* The StaId (used by TL, PE, and HAL) */
            &pHddHdl); /* Handle to return BSL context */
    if ( VOS_STATUS_SUCCESS != vosStatus ) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO,
                     "Unable to retrieve STA Id from BAP context and phy_link_handle in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

    // JEZ081003: Remove this after debugging 
    // Sanity check the log_link_handle value 
    if (!BTAMP_VALID_LOG_LINK( hciACLHeader.logLinkHandle))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, 
                "WLAN BAP: Invalid logical link handle (%d) in %s. Corrected.", 
                hciACLHeader.logLinkHandle,
                __func__);

        // JEZ090123: Insure that the logical link value is good
        hciACLHeader.logLinkHandle = 1;
        //return VOS_STATUS_E_INVAL;
    }

    /* Use the log_link_handle to retrieve the logical link context */ 
    /* JEZ081006: abstract this with a proc.  So you can change the impl later */ 
    pLogLinkContext = &(pBtampCtx->btampLogLinkCtx[ hciACLHeader.logLinkHandle ]);

    // JEZ081003: Remove this after debugging 
    // Sanity check the log_link_handle value 
    // JEZ081113: I changed this to fail on an UNOCCUPIED entry 
    if ( pLogLinkContext->present != VOS_TRUE)
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, 
                "WLAN BAP: Invalid logical link entry in %s",
                __func__);

        return VOS_STATUS_E_INVAL;
    }

    // Return the AC and MetaInfo

    // Now copy the AC values from the Logical Link context
    *pucAC = pLogLinkContext->btampAC;
    // Now copy the values from the Logical Link context to the MetaInfo 
    tlMetaInfo->ucTID = pLogLinkContext->ucTID;
    tlMetaInfo->ucUP = pLogLinkContext->ucUP;
    tlMetaInfo->ucIsEapol = VOS_FALSE;
    tlMetaInfo->ucDisableFrmXtl = VOS_FALSE;
    tlMetaInfo->ucBcast = VOS_FALSE; /* hciACLHeader.BCFlag; */ /* Don't I want to use the BCFlag? */
    tlMetaInfo->ucMcast = VOS_FALSE;
    tlMetaInfo->ucType = 0x00;  /* What is this really ?? */
//    tlMetaInfo->usTimeStamp = 0x00;  /* Ravi, shouldn't you be setting this?  It's in the VOS packet.  */

    // Form the 802.3 header

    vos_mem_copy( w8023Header.vDA, pBtampCtx->peer_mac_addr, VOS_MAC_ADDR_SIZE);
    vos_mem_copy( w8023Header.vSA, pBtampCtx->self_mac_addr, VOS_MAC_ADDR_SIZE);
    
    /* Now this length passed down in HCI...is in little-endian */
    headerLength = vos_le16_to_cpu(hciACLHeader.dataLength);  
    headerLength += WLANBAP_LLC_HEADER_LEN;  
    /* Now the 802.3 length field is big-endian?! */
    w8023Header.usLenType = vos_cpu_to_be16(headerLength);

    /* Now adjust the protocol type bytes*/
    protoType = vos_cpu_to_be16( protoType);

    /* Now form the LLC header */
    vos_mem_copy(aucLLCHeader, 
            WLANBAP_LLC_HEADER,  
            sizeof(WLANBAP_LLC_HEADER));
    vos_mem_copy(&aucLLCHeader[WLANBAP_LLC_OUI_OFFSET], 
            WLANBAP_BT_AMP_OUI,  
            WLANBAP_LLC_OUI_SIZE);
    vos_mem_copy(&aucLLCHeader[WLANBAP_LLC_PROTO_TYPE_OFFSET], 
            &protoType,  //WLANBAP_BT_AMP_TYPE_DATA
            WLANBAP_LLC_PROTO_TYPE_SIZE);
 
    /* Push on the LLC header */
    vos_pkt_push_head(vosDataBuff, 
            aucLLCHeader, 
            WLANBAP_LLC_HEADER_LEN);  

    /* Push on the 802.3 header */
    vos_pkt_push_head(vosDataBuff, &w8023Header, sizeof(w8023Header));


    /*Set the logical link handle as user data so that we can retrieve it on 
      Tx Complete */
    value = (v_U32_t)hciACLHeader.logLinkHandle;
    vos_pkt_set_user_data_ptr( vosDataBuff, VOS_PKT_USER_DATA_ID_BAP,
                               (v_VOID_t *)value);

    return VOS_STATUS_SUCCESS;
}/*WLANBAP_XlateTxDataPkt*/

/*===========================================================================

  FUNCTION    WLANBAP_GetAcFromTxDataPkt

  DESCRIPTION 

    HDD will call this API when it has a HCI Data Packet (SKB) and it wants 
    to find AC type of the data frame from the HCI header on the data pkt
    - to be send using TL.


  PARAMETERS 

    btampHandle: The BT-AMP PAL handle returned in WLANBAP_GetNewHndl.
 
    pHciData: Pointer to the HCI data frame
 
    pucAC:       Pointer to return the access category 
   
  RETURN VALUE

    The result code associated with performing the operation  

    VOS_STATUS_E_INVAL:  Input parameters are invalid 
    VOS_STATUS_E_FAULT:  BAP handle is NULL  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANBAP_GetAcFromTxDataPkt
( 
  ptBtampHandle     btampHandle,  /* Used by BAP to identify the actual session
                                    and therefore addresses */
  void              *pHciData,     /* Pointer to the HCI data frame */
  WLANTL_ACEnumType *pucAC        /* Return the AC here */
)
{
    ptBtampContext           pBtampCtx; 
    tpBtampLogLinkCtx        pLogLinkContext;
    WLANBAP_HCIACLHeaderType hciACLHeader;
    /*------------------------------------------------------------------------
        Sanity check params
      ------------------------------------------------------------------------*/
    if (( NULL == btampHandle) || (NULL == pHciData) || (NULL == pucAC))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "Invalid params in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }
    pBtampCtx = (ptBtampContext) btampHandle;

    vos_mem_copy( &hciACLHeader, pHciData, WLANBAP_HCI_ACL_HEADER_LEN);
    // Sanity check the log_link_handle value 
    if (!BTAMP_VALID_LOG_LINK( hciACLHeader.logLinkHandle))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, 
                "WLAN BAP: Invalid logical link handle (%d) in %s", 
                hciACLHeader.logLinkHandle,
                __func__);

        return VOS_STATUS_E_INVAL;
    }

    /* Use the log_link_handle to retrieve the logical link context */ 
    /* JEZ081006: abstract this with a proc.  So you can change the impl later */ 
    pLogLinkContext = &(pBtampCtx->btampLogLinkCtx[ hciACLHeader.logLinkHandle ]);

    // Sanity check the log_link_handle value 
    // JEZ081113: I changed this to fail on an UNOCCUPIED entry 
    if ( pLogLinkContext->present != VOS_TRUE)
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, 
                "WLAN BAP: Invalid logical link entry in %s",
                __func__);

        return VOS_STATUS_E_INVAL;
    }

    // Return the AC

    // Now copy the AC values from the Logical Link context
    *pucAC = pLogLinkContext->btampAC;

    return VOS_STATUS_SUCCESS;
}

/*===========================================================================

  FUNCTION    WLANBAP_XlateRxDataPkt

  DESCRIPTION 

    HDD will call this API when it has received a 802.3 (TL/UMA has 
    Xlated from 802.11) frame from TL and it wants to form a 
    BT HCI Data Packet - ready to signal up to the BT stack application.


  PARAMETERS 

    btampHandle: The BT-AMP PAL handle returned in WLANBAP_GetNewHndl.
    pucAC:       Pointer to return the access category 
    vosDataBuff: The data buffer containing the 802.3 frame to be 
                 translated to BT HCI Data Packet
   
  RETURN VALUE

    The result code associated with performing the operation  

    VOS_STATUS_E_INVAL:  Input parameters are invalid 
    VOS_STATUS_E_FAULT:  BAP handle is NULL  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANBAP_XlateRxDataPkt
( 
  ptBtampHandle     btampHandle, 
  v_U8_t            phy_link_handle,  /* Used by BAP to indentify the WLAN assoc. (StaId) */
  WLANTL_ACEnumType  *pucAC,        /* Return the AC here. I don't think this is needed */
  vos_pkt_t        *vosDataBuff
)
{
    WLANBAP_8023HeaderType  w8023Header;
    WLANBAP_HCIACLHeaderType hciACLHeader;
    v_U8_t                   aucLLCHeader[WLANBAP_LLC_HEADER_LEN];
    ptBtampContext           pBtampCtx = (ptBtampContext) btampHandle; 
    VOS_STATUS               vosStatus;
    //v_PVOID_t                pHddHdl; /* Handle to return BSL context in */
    v_U16_t                  hciDataLength;  /* The HCI packet data length*/
    v_U16_t                  protoType = WLANBAP_BT_AMP_TYPE_DATA;  /* The protocol type bytes*/
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    /*------------------------------------------------------------------------
        Sanity check params
      ------------------------------------------------------------------------*/
    if ( NULL == pBtampCtx) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "Invalid BAP handle value in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

    // Here, I have to make the assumption that this is an 
    // 802.3 header followed by an LLC/SNAP packet. 
    vos_mem_set( &w8023Header, sizeof(w8023Header), 0 );
    vosStatus = vos_pkt_pop_head( vosDataBuff, &w8023Header, sizeof(w8023Header));

    if ( VOS_STATUS_SUCCESS != vosStatus ) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                  "WLAN BAP: Failed to pop 802.3 header from packet %d",
                  vosStatus);

        return vosStatus;
    }

    // Here, is that LLC/SNAP header. 
    // With the BT SIG OUI that I am being handed. 
    vos_mem_set( aucLLCHeader, WLANBAP_LLC_HEADER_LEN, 0 );
    vosStatus = vos_pkt_pop_head( vosDataBuff, aucLLCHeader, WLANBAP_LLC_HEADER_LEN);

    if ( VOS_STATUS_SUCCESS != vosStatus ) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                  "WLAN BAP: Failed to pop LLC/SNAP header from packet %d",
                  vosStatus);

        return vosStatus;
    }

#ifdef BAP_DEBUG
    // JEZ081003: Remove this after debugging 
    // Should I double check that I am getting the BT SIG OUI ?
    if ( !(vos_mem_compare( aucLLCHeader, 
             WLANBAP_LLC_HEADER,  
             sizeof(WLANBAP_LLC_HEADER)  
             - WLANBAP_LLC_OUI_SIZE)  /* Don't check the last three bytes here */ 
         && vos_mem_compare( &aucLLCHeader[WLANBAP_LLC_OUI_OFFSET], 
             (v_VOID_t*)WLANBAP_BT_AMP_OUI,  
             WLANBAP_LLC_OUI_SIZE)))  /* check them here */ 
    {

        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "Invalid LLC header for BT-AMP packet in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }
#endif //BAP_DEBUG

    /* Now adjust the protocol type bytes*/
    protoType = vos_cpu_to_be16( protoType);
    // check if this is a data frame or other, internal to BAP, type...
    // we are only handling data frames in here...
    // The others (Security and AR) are handled by TLs BAP client API. 
    // (Verify with TL)
    if ( !(vos_mem_compare( &aucLLCHeader[WLANBAP_LLC_PROTO_TYPE_OFFSET], 
            &protoType,  //WLANBAP_BT_AMP_TYPE_DATA
            WLANBAP_LLC_PROTO_TYPE_SIZE))) 
    {

        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "Invalid (non-data) frame type in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

#ifdef BAP_DEBUG
    // JEZ081003: Remove this after debugging 
    /*------------------------------------------------------------------------
        Sanity check the MAC address in the physical link context 
        against the value in the incoming Rx Frame.
      ------------------------------------------------------------------------*/
    if ( !(vos_mem_compare( w8023Header.vDA, pBtampCtx->self_mac_addr, VOS_MAC_ADDR_SIZE)
    && vos_mem_compare( w8023Header.vSA, pBtampCtx->peer_mac_addr, VOS_MAC_ADDR_SIZE))) 
    {

        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "MAC address mismatch in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }
#endif //BAP_DEBUG

    /* No lookup is needed.  Because TL has already told WLANBAP_STARxCB 
     * the StaId.  And I told WLANBAP_STARxCBType the corresponding BSL context 
     * Which he used to lookup the phy_link_handle value. 
     */ 


    // Start filling in the HCI header 
    hciACLHeader.phyLinkHandle = phy_link_handle;

    // Continue filling in the HCI header 
    //JEZ100913: On Rx the Logical Link is ALWAYS 0. See Vol 2, Sec E, 5.4.2 of spec.
    hciACLHeader.logLinkHandle = 0;
    hciACLHeader.PBFlag = WLANBAP_HCI_PKT_AMP;
    hciACLHeader.BCFlag = 0;

    /* Now the length field is big-endian?! */
    hciDataLength =  vos_be16_to_cpu(w8023Header.usLenType);
    /* Max length WLANBAP_MAX_80211_PAL_PDU_SIZE (1492) */
    hciDataLength -= WLANBAP_LLC_HEADER_LEN;
    /* The HCI packet data length is Little-endian */
    hciACLHeader.dataLength = vos_cpu_to_le16(hciDataLength);  

    /* Return the AC here. 
     * (I can't because there is no way to figure out what it is.)
     */
    *pucAC = 0;        

    /* Push on the HCI header */
    vos_pkt_push_head(vosDataBuff, &hciACLHeader, WLANBAP_HCI_ACL_HEADER_LEN);

    return VOS_STATUS_SUCCESS;
} /* WLANBAP_XlateRxDataPkt */

/*----------------------------------------------------------------------------

  FUNCTION    WLANBAP_STAFetchPktCB 

  DESCRIPTION   
    The fetch packet callback registered with TL. 
    
    It is called by the TL when the scheduling algorithms allows for 
    transmission of another packet to the module. 
    It will be called in the context of the BAL fetch transmit packet 
    function, initiated by the bus lower layer. 


  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle 
                    to TL's or HDD's control block can be extracted 
                    from its context 

    IN/OUT
    pucSTAId:       the Id of the station for which TL is requesting a 
                    packet, in case HDD does not maintain per station 
                    queues it can give the next packet in its queue 
                    and put in the right value for the 
    pucAC:          access category requested by TL, if HDD does not have 
                    packets on this AC it can choose to service another AC 
                    queue in the order of priority

    OUT
    vosDataBuff:   pointer to the VOSS data buffer that was transmitted 
    tlMetaInfo:    meta info related to the data frame


  
  RETURN VALUE 
    The result code associated with performing the operation

----------------------------------------------------------------------------*/
VOS_STATUS 
WLANBAP_STAFetchPktCB 
( 
  v_PVOID_t             pvosGCtx,
  v_U8_t*               pucSTAId,
  v_U8_t                ucAC,
  vos_pkt_t**           vosDataBuff,
  WLANTL_MetaInfoType*  tlMetaInfo
)
{
    VOS_STATUS    vosStatus; 
    ptBtampHandle bapHdl;  /* holds ptBtampHandle value returned  */ 
    ptBtampContext bapContext; /* Holds the btampContext value returned */ 
    v_PVOID_t     pHddHdl; /* Handle to return BSL context in */

    /* Lookup the BSL and BAP contexts using the StaId */ 

    vosStatus = WLANBAP_GetCtxFromStaId ( 
            *pucSTAId,  /* The StaId (used by TL, PE, and HAL) */
            &bapHdl,  /* "handle" to return ptBtampHandle value in  */ 
            &bapContext,  /* "handle" to return ptBtampContext value in  */ 
            &pHddHdl); /* "handle" to return BSL context in */
    if ( VOS_STATUS_SUCCESS != vosStatus ) 
    {
      VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO,
                   "Unable to retrieve BSL or BAP context from STA Id in WLANBAP_STAFetchPktCB");
      return VOS_STATUS_E_FAULT;
    }

    /* Invoke the callback that BSL registered with me */ 
    vosStatus = (*bapContext->pfnBtampFetchPktCB)( 
            pHddHdl, 
            (WLANTL_ACEnumType)   ucAC, /* typecast it for now */ 
            vosDataBuff, 
            tlMetaInfo);    
    if ( VOS_STATUS_SUCCESS != vosStatus ) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO,
                "Callback registered by BSL failed to fetch pkt in WLANNBAP_STAFetchPktCB");
        return VOS_STATUS_E_FAULT;
    }

    return vosStatus;
} /* WLANBAP_STAFetchPktCB */ 

/*----------------------------------------------------------------------------

  FUNCTION    WLANBAP_STARxCB

  DESCRIPTION   
    The receive callback registered with TL. 
    
    TL will call this to notify the client when a packet was received 
    for a registered STA.

  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to 
                    TL's or HDD's control block can be extracted from 
                    its context 
    vosDataBuff:   pointer to the VOSS data buffer that was received
                    (it may be a linked list) 
    ucSTAId:        station id
    pRxMetaInfo:   meta info for the received packet(s) 
   
  RETURN VALUE 
    The result code associated with performing the operation

----------------------------------------------------------------------------*/
VOS_STATUS 
WLANBAP_STARxCB
( 
  v_PVOID_t          pvosGCtx,
  vos_pkt_t*         vosDataBuff,
  v_U8_t             ucSTAId,
  WLANTL_RxMetaInfoType* pRxMetaInfo
)
{
    VOS_STATUS    vosStatus; 
    ptBtampHandle bapHdl;  /* holds ptBtampHandle value returned  */ 
    ptBtampContext bapContext; /* Holds the btampContext value returned */ 
    v_PVOID_t     pHddHdl; /* Handle to return BSL context in */
    ptBtampHandle            btampHandle;
    WLANBAP_8023HeaderType   w8023Header;
    v_U8_t                   aucLLCHeader[WLANBAP_LLC_HEADER_LEN];
    v_U16_t                  protoType ;
    v_SIZE_t                 llcHeaderLen = WLANBAP_LLC_HEADER_LEN ;
    
    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO,
                   "In WLANBAP_STARxCB");

    /* Lookup the BSL and BAP contexts using the StaId */ 

    vosStatus = WLANBAP_GetCtxFromStaId ( 
            ucSTAId,  /* The StaId (used by TL, PE, and HAL) */
            &bapHdl,  /* "handle" to return ptBtampHandle value in  */ 
            &bapContext,  /* "handle" to return ptBtampContext value in  */ 
            &pHddHdl); /* "handle" to return BSL context in */
    if ( VOS_STATUS_SUCCESS != vosStatus ) 
    {
      VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO,
                   "Unable to retrieve BSL or BAP context from STA Id in WLANBAP_STARxCB");
      /* Drop packet */
      vos_pkt_return_packet(vosDataBuff);
      return VOS_STATUS_E_FAULT;
    }


    vosStatus = vos_pkt_extract_data( vosDataBuff, sizeof(w8023Header), (v_VOID_t *)aucLLCHeader,
                                   &llcHeaderLen);

    if ( NULL == aucLLCHeader/*LLC Header*/ )
    {
        VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                 "WLANBAP_STARxCB:Cannot extract LLC header");
        /* Drop packet */
        vos_pkt_return_packet(vosDataBuff);
        return VOS_STATUS_E_FAULT;
    }
    
    vos_mem_copy(&protoType,&aucLLCHeader[WLANBAP_LLC_PROTO_TYPE_OFFSET],WLANBAP_LLC_PROTO_TYPE_SIZE);
    protoType = vos_be16_to_cpu(protoType);
    
    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
            "%s: received : %d, => BAP",__func__,
                 protoType);
    
    if(WLANBAP_BT_AMP_TYPE_DATA == protoType)
    {
        if (bapContext->bapLinkSupervisionTimerInterval)
        {
            /* Reset Link Supervision timer */
            //vosStatus = WLANBAP_StopLinkSupervisionTimer(bapContext); 
            //vosStatus = WLANBAP_StartLinkSupervisionTimer(bapContext,7000);
            bapContext->dataPktPending = VOS_TRUE;//Indication for LinkSupervision module that data is pending 
            /* Invoke the callback that BSL registered with me */ 
            vosStatus = (*bapContext->pfnBtamp_STARxCB)( 
                pHddHdl,
                vosDataBuff,
                pRxMetaInfo);
        }
        else
        {
            VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_FATAL,
                     "WLANBAP_STARxCB:bapLinkSupervisionTimerInterval is 0");
            /* Drop packet */
            vos_pkt_return_packet(vosDataBuff);
        }
    }
    else
    {
          VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                "%s: link Supervision packet received over TL: %d, => BAP",
                     __func__,protoType);
          btampHandle = (ptBtampHandle)bapContext; 
          vosStatus = WLANBAP_RxProcLsPkt(
                        btampHandle,
                        bapContext->phy_link_handle,
                        protoType,
                        vosDataBuff
                        );
    }  

    return vosStatus;
} /* WLANBAP_STARxCB */


/*----------------------------------------------------------------------------

  FUNCTION    WLANBAP_TxCompCB

  DESCRIPTION   
    The tx complete callback registered with TL. 
    
    TL will call this to notify the client when a transmission for a 
    packet  has ended. 

  PARAMETERS 

    IN
    pvosGCtx:       pointer to the global vos context; a handle to 
                    TL/HAL/PE/BAP/HDD control block can be extracted from 
                    its context 
    vosDataBuff:   pointer to the VOSS data buffer that was transmitted 
    wTxSTAtus:      status of the transmission 

  
  RETURN VALUE 
    The result code associated with performing the operation

----------------------------------------------------------------------------*/
VOS_STATUS 
WLANBAP_TxCompCB
( 
  v_PVOID_t      pvosGCtx,
  vos_pkt_t*     vosDataBuff,
  VOS_STATUS     wTxSTAtus 
)
{
    VOS_STATUS    vosStatus; 
    ptBtampHandle bapHdl;  /* holds ptBtampHandle value returned  */ 
    ptBtampContext bapContext; /* Holds the btampContext value returned */ 
    v_PVOID_t     pHddHdl; /* Handle to return BSL context in */
    v_PVOID_t      pvlogLinkHandle = NULL;
    v_U32_t       value;

    WLANBAP_HCIACLHeaderType hciACLHeader;

    /* retrieve the BSL and BAP contexts */ 

    /* I don't really know how to do this - in the general case. */
    /* So, for now, I will just use something that works. */
    /* (In general, I will have to keep a list of the outstanding transmit */
    /* buffers, in order to determine which assoc they are with.) */
    //vosStatus = WLANBAP_GetCtxFromStaId ( 
    //        ucSTAId,  /* The StaId (used by TL, PE, and HAL) */
    //        &bapHdl,  /* "handle" to return ptBtampHandle value in  */ 
    //        &bapContext,  /* "handle" to return ptBtampContext value in  */ 
    //        &pHddHdl); /* "handle" to return BSL context in */
    /* Temporarily we do the following*/ 
    //bapHdl = &btampCtx;
    bapHdl = (v_PVOID_t)gpBtampCtx;
    /* Typecast the handle into a context. Works as we have only one link*/ 
    bapContext = ((ptBtampContext) bapHdl);  

    /*------------------------------------------------------------------------
      Sanity check params
    ------------------------------------------------------------------------*/
    if ( NULL == vosDataBuff) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                "Invalid vosDataBuff value in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

    if ( NULL == bapContext) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                "Invalid bapContext value in %s", __func__);
        vos_pkt_return_packet( vosDataBuff ); 
        return VOS_STATUS_E_FAULT;
    }

    pHddHdl = bapContext->pHddHdl;
    vosStatus = VOS_STATUS_SUCCESS;
    if ( VOS_STATUS_SUCCESS != vosStatus ) 
    {
      VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO,
                   "Unable to retrieve BSL or BAP context from STA Id in WLANBAP_TxCompCB");
      vos_pkt_return_packet( vosDataBuff ); 
      return VOS_STATUS_E_FAULT;
    }

    /*Get the logical link handle from the vos user data*/
    vos_pkt_get_user_data_ptr( vosDataBuff, VOS_PKT_USER_DATA_ID_BAP,
                               &pvlogLinkHandle);

    value = (v_U32_t)pvlogLinkHandle;
    hciACLHeader.logLinkHandle = value;

#ifdef BAP_DEBUG
    /* Trace the bapContext referenced. */
    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
              "WLAN BAP Context Monitor: bapContext value = %p in %s:%d. vosDataBuff=%p", bapContext, __func__, __LINE__, vosDataBuff );
#endif //BAP_DEBUG

    // Sanity check the log_link_handle value 
// JEZ100722: Temporary changes.
    if (BTAMP_VALID_LOG_LINK( hciACLHeader.logLinkHandle))
    {
       vos_atomic_increment_U32(
           &bapContext->btampLogLinkCtx[hciACLHeader.logLinkHandle].uTxPktCompleted);
//           &bapContext->btampLogLinkCtx[0].uTxPktCompleted);
//       vos_atomic_increment_U32(
//           &bapContext->btampLogLinkCtx[1].uTxPktCompleted);
    } else 
    {
       VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                   "In %s:%d: Invalid logical link handle: %d", __func__, __LINE__, hciACLHeader.logLinkHandle);
    }

    /* Invoke the callback that BSL registered with me */ 
    vosStatus = (*bapContext->pfnBtampTxCompCB)( 
            pHddHdl,
            vosDataBuff,
            wTxSTAtus);

    return vosStatus;
} /* WLANBAP_TxCompCB */

/*==========================================================================

  FUNCTION    WLANBAP_RegisterDataPlane

  DESCRIPTION 
    The HDD calls this routine to register the "data plane" routines
    for Tx, Rx, and Tx complete with BT-AMP.  For now, with only one
    physical association supported at a time, this COULD be called 
    by HDD at the same time as WLANBAP_GetNewHndl.  But, in general
    it needs to be called upon each new physical link establishment.
    
    This registration is really two part.  The routines themselves are
    registered here.  But, the mapping between the BSL context and the
    actual physical link takes place during WLANBAP_PhysicalLinkCreate. 
    
  DEPENDENCIES 
    
  PARAMETERS 

    IN
    btampHandle: The BT-AMP PAL handle returned in WLANBAP_GetNewHndl.
   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to BAP cb is NULL ; access would cause a page 
                         fault  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WLANBAP_RegisterDataPlane
( 
  ptBtampHandle btampHandle,  /* BTAMP context */ 
  WLANBAP_STAFetchPktCBType pfnBtampFetchPktCB, 
  WLANBAP_STARxCBType pfnBtamp_STARxCB,
  WLANBAP_TxCompCBType pfnBtampTxCompCB,
  // phy_link_handle, of course, doesn't come until much later.  At Physical Link create.
  v_PVOID_t      pHddHdl   /* BSL specific context */
)
{
    ptBtampContext pBtampCtx = (ptBtampContext) btampHandle; 

  
    /*------------------------------------------------------------------------
      Sanity check params
     ------------------------------------------------------------------------*/
    if ( NULL == pBtampCtx) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "Invalid BAP handle value in WLANBAP_RegisterDataPlane");
        return VOS_STATUS_E_FAULT;
    }

    // Include the HDD BAP Shim Layer callbacks for Fetch, TxComp, and RxPkt
    pBtampCtx->pfnBtampFetchPktCB = pfnBtampFetchPktCB; 
    pBtampCtx->pfnBtamp_STARxCB = pfnBtamp_STARxCB;
    pBtampCtx->pfnBtampTxCompCB = pfnBtampTxCompCB;

    // (Right now, there is only one)
    pBtampCtx->pHddHdl = pHddHdl;
    /* Set the default data transfer mode */ 
    pBtampCtx->ucDataTrafficMode = WLANBAP_FLOW_CONTROL_MODE_BLOCK_BASED;

    return VOS_STATUS_SUCCESS;
} /* WLANBAP_RegisterDataPlane */


/*===========================================================================

  FUNCTION    WLANBAP_STAPktPending

  DESCRIPTION 

    HDD will call this API when a packet is pending transmission in its 
    queues. HDD uses this instead of WLANTL_STAPktPending because he is
    not aware of the mapping from session to STA ID.

  DEPENDENCIES 

    HDD must have called WLANBAP_GetNewHndl before calling this API.

  PARAMETERS 

    btampHandle: The BT-AMP PAL handle returned in WLANBAP_GetNewHndl.
                 BSL can obtain this from the physical handle value in the
                 downgoing HCI Data Packet. He, after all, was there
                 when the PhysicalLink was created. He knew the btampHandle 
                 value returned by WLANBAP_GetNewHndl. He knows as well, his
                 own pHddHdl (see next).
    phy_link_handle: Used by BAP to indentify the WLAN assoc. (StaId)
    ucAc:        The access category for the pending frame
   
  RETURN VALUE

    The result code associated with performing the operation  

    VOS_STATUS_E_INVAL:  Input parameters are invalid 
    VOS_STATUS_E_FAULT:  BAP handle is NULL  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANBAP_STAPktPending 
( 
  ptBtampHandle  btampHandle,  /* Used by BAP to identify the app context and VOSS ctx (!?) */ 
  v_U8_t         phy_link_handle,  /* Used by BAP to indentify the WLAN assoc. (StaId) */
  WLANTL_ACEnumType ucAc   /* This is the first instance of a TL type in bapApi.h */
)
{
    VOS_STATUS     vosStatus; 
    ptBtampContext pBtampCtx = (ptBtampContext) btampHandle; 
    v_PVOID_t      pvosGCtx;
    v_U8_t         ucSTAId;  /* The StaId (used by TL, PE, and HAL) */
    v_PVOID_t      pHddHdl; /* Handle to return BSL context in */

  
#ifdef BAP_DEBUG
    /* Trace the tBtampCtx being passed in. */
    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO_HIGH,
              "WLAN BAP Context Monitor: pBtampCtx value = %p in %s:%d", pBtampCtx, __func__, __LINE__ );
#endif //BAP_DEBUG

    /*------------------------------------------------------------------------
      Sanity check params
     ------------------------------------------------------------------------*/
    if ( NULL == pBtampCtx) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                "Invalid BAP handle value in WLANBAP_STAPktPending"); 
        return VOS_STATUS_E_FAULT;
    }

    // Retrieve the VOSS context
    pvosGCtx = pBtampCtx->pvosGCtx;
 
    /* Lookup the StaId using the phy_link_handle and the BAP context */ 

    vosStatus = WLANBAP_GetStaIdFromLinkCtx ( 
            btampHandle,  /* btampHandle value in  */ 
            phy_link_handle,  /* phy_link_handle value in */
            &ucSTAId,  /* The StaId (used by TL, PE, and HAL) */
            &pHddHdl); /* Handle to return BSL context */
    if ( VOS_STATUS_SUCCESS != vosStatus ) 
    {
      VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO,
                   "Unable to retrieve STA Id from BAP context and phy_link_handle in WLANBAP_STAPktPending");
      return VOS_STATUS_E_FAULT;
    }


    // Let TL know we have a packet to send...
    vosStatus = WLANTL_STAPktPending( 
            pvosGCtx,
            ucSTAId,
            ucAc);
    if ( VOS_STATUS_SUCCESS != vosStatus ) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                        "Tx: Packet rejected by TL in WLANBAP_STAPktPending");
        return vosStatus;
    }            
    pBtampCtx->dataPktPending = VOS_TRUE;//Indication for LinkSupervision module that data is pending 
    return VOS_STATUS_SUCCESS;
} /* WLANBAP_STAPktPending */ 

/*----------------------------------------------------------------------------

  FUNCTION    WLAN_BAPRegisterBAPCallbacks() 

  DESCRIPTION 
    Register the BAP "Event" callbacks.
    Return the per instance handle.

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    btampHandle: The BT-AMP PAL handle returned in WLANBAP_GetNewHndl.
    pBapHCIEventCB:  pointer to the Event callback
    pAppHdl:  The context passed in by caller. (I.E., BSL app specific context.)

   
  RETURN VALUE
    The result code associated with performing the operation  

    VOS_STATUS_E_FAULT:  pointer to pBapHCIEventCB is NULL 
    VOS_STATUS_SUCCESS:  Success
  
  SIDE EFFECTS 
  
----------------------------------------------------------------------------*/
VOS_STATUS  
WLAN_BAPRegisterBAPCallbacks 
( 
  ptBtampHandle           btampHandle, /* BSL uses my handle to talk to me */
                            /* Returned from WLANBAP_GetNewHndl() */
                            /* It's like each of us is using the other */
                            /* guys reference when invoking him. */
  tpWLAN_BAPEventCB       pBapHCIEventCB, /*Implements the callback for ALL asynchronous events. */ 
  v_PVOID_t               pAppHdl  // Per-app BSL context
)
{
    ptBtampContext pBtampCtx = (ptBtampContext) btampHandle; 

  
    /*------------------------------------------------------------------------
      Sanity check params
     ------------------------------------------------------------------------*/
    if ( NULL == pBtampCtx) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "Invalid BAP handle value in WLAN_BAPRegisterBAPCallbacks");
        return VOS_STATUS_E_FAULT;
    }

    // Save the Event callback 
    pBtampCtx->pBapHCIEventCB = pBapHCIEventCB; 

    // (Right now, there is only one)
    pBtampCtx->pAppHdl = pAppHdl;

    return VOS_STATUS_SUCCESS;
} /* WLAN_BAPRegisterBAPCallbacks */


