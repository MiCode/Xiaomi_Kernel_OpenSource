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

                      b a p A p i LinkSupervision . C
                                               
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



  when        who     what, where, why
----------    ---    --------------------------------------------------------
2008-03-25    arulv     Created module

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
//#include "wlan_qct_tl.h"
#include "vos_trace.h"
//I need the TL types and API
#include "wlan_qct_tl.h"

/* BT-AMP PAL API header file */ 
#include "bapApi.h" 
#include "bapInternal.h"
#include "bapApiTimer.h"

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

#if 1
//*BT-AMP packet LLC OUI value*/
static const v_U8_t WLANBAP_BT_AMP_OUI[] =  {0x00, 0x19, 0x58 };

/*LLC header value*/
static v_U8_t WLANBAP_LLC_HEADER[] =  {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00 };
#endif

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
VOS_STATUS
WLANBAP_AcquireLSPacket( ptBtampContext pBtampCtx, vos_pkt_t **ppPacket, v_U16_t size, tANI_BOOLEAN isLsReq )
{
    VOS_STATUS vosStatus;
    vos_pkt_t *pPacket;
    WLANBAP_8023HeaderType   w8023Header;
    v_U8_t                   aucLLCHeader[WLANBAP_LLC_HEADER_LEN];
    v_U16_t                  headerLength;  /* The 802.3 frame length*/
    v_U16_t                  protoType;
    v_U8_t                   *pData = NULL;
     

    if(isLsReq)
    {
        protoType = WLANTL_BT_AMP_TYPE_LS_REQ;
    }
    else
    {
        protoType = WLANTL_BT_AMP_TYPE_LS_REP;
    }    

    //If success, vosTxLsPacket is the packet and pData points to the head.
   vosStatus = vos_pkt_get_packet( &pPacket, VOS_PKT_TYPE_TX_802_11_MGMT,size, 1, 
                                    VOS_TRUE, NULL, NULL );
   if( VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
       vosStatus = vos_pkt_reserve_head( pPacket, (v_VOID_t *)&pData, size );
       if( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
       {
                VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                 "%s: failed to reserve size = %d\n",__func__, size );
                 vos_pkt_return_packet( pPacket );
       }
   }

   if( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
       VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                "WLANBAP_LinkSupervisionTimerHandler failed to get vos_pkt\n" );
       return vosStatus;
   }

         // Form the 802.3 header
   vos_mem_copy( w8023Header.vDA, pBtampCtx->peer_mac_addr, VOS_MAC_ADDR_SIZE);
   vos_mem_copy( w8023Header.vSA, pBtampCtx->self_mac_addr, VOS_MAC_ADDR_SIZE);

   headerLength = WLANBAP_LLC_HEADER_LEN;
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
            &protoType,  //WLANBAP_BT_AMP_TYPE_LS_REQ
            WLANBAP_LLC_PROTO_TYPE_SIZE);
 
        /* Push on the LLC header */
   vos_pkt_push_head(pPacket, 
            aucLLCHeader, 
            WLANBAP_LLC_HEADER_LEN);  

        /* Push on the 802.3 header */
   vos_pkt_push_head(pPacket, &w8023Header, sizeof(w8023Header));
   *ppPacket = pPacket;
   return vosStatus;
}



/*===========================================================================

  FUNCTION    WLANBAP_InitLinkSupervision

  DESCRIPTION 

    This API will be called when Link Supervision module is to be initialized when connected at BAP

  PARAMETERS 

    btampHandle: The BT-AMP PAL handle returned in WLANBAP_GetNewHndl.
   
  RETURN VALUE

    The result code associated with performing the operation  

    VOS_STATUS_E_INVAL:  Input parameters are invalid 
    VOS_STATUS_E_FAULT:  BAP handle is NULL  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
#define TX_LS_DATALEN   32

VOS_STATUS
WLANBAP_InitLinkSupervision
( 
  ptBtampHandle     btampHandle
)
{
    VOS_STATUS               vosStatus = VOS_STATUS_SUCCESS;
    ptBtampContext           pBtampCtx = (ptBtampContext) btampHandle;
    vos_pkt_t                *pLSReqPacket; 
    vos_pkt_t                *pLSRepPacket; 
    v_U16_t                   lsPktln; 

    if ( NULL == pBtampCtx) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "Invalid BAP handle value in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

#if 0    
    /* Initialize Link supervision data structure */
    vos_mem_set(pLsInfo, sizeof(tBtampLS),0);

    /* Allocate memory for Static Tx Data */
    pLsInfo->pTxPktData = vos_mem_malloc(sizeof(tBtampLsPktData)+TX_LS_DATALEN);

    /* Initialize Static data for LS pkt Tx */
    pLsInfo->pTxPktData->BufLen = TX_LS_DATALEN;
    vos_mem_copy (&pLsInfo->pTxPktData->pBuf, LsTxData, pLsInfo->pTxPktData->BufLen);
#endif
    pBtampCtx->lsReqPktPending = VOS_FALSE;
    pBtampCtx->retries = 0;

    vosStatus = WLANBAP_AcquireLSPacket( pBtampCtx, &pLSReqPacket,32, TRUE );
    if( VOS_IS_STATUS_SUCCESS( vosStatus ) )
    {
        pBtampCtx->lsReqPacket = pLSReqPacket;
    }
    else
    {
         VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO,
                       "%s:AcquireLSPacket failed\n",__func__);
         pBtampCtx->lsReqPacket = NULL;
         return vosStatus;   
    }

    vosStatus = WLANBAP_AcquireLSPacket( pBtampCtx, &pLSRepPacket,32,FALSE );
    if( VOS_IS_STATUS_SUCCESS( vosStatus ) )
    {
        pBtampCtx->lsRepPacket = pLSRepPacket;
    }
    else
    {
         VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO,
                       "%s:AcquireLSPacket failed\n",__func__);
         pBtampCtx->lsRepPacket = NULL;
         return vosStatus;   
    }        

    vosStatus = vos_pkt_get_packet_length(pBtampCtx->lsRepPacket,&lsPktln); 

    if ( VOS_STATUS_SUCCESS != vosStatus ) 
    {
         VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO,
                       "%s:vos_pkt_get_length error",__func__);
         return VOS_STATUS_E_FAULT;
    }
    pBtampCtx->lsPktln = lsPktln;

    /* Start Link Supervision Timer if not configured for infinite */
    if (pBtampCtx->bapLinkSupervisionTimerInterval)
    {
        vosStatus = WLANBAP_StartLinkSupervisionTimer (pBtampCtx,
                   pBtampCtx->bapLinkSupervisionTimerInterval * WLANBAP_BREDR_BASEBAND_SLOT_TIME);
    }
    else
    {
         VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO,
                       "%s:No LS configured for infinite",__func__);
    }
   
    return vosStatus;
}

/*===========================================================================

  FUNCTION    WLANBAP_DeInitLinkSupervision

  DESCRIPTION 

    This API will be called when Link Supervision module is to be stopped after disconnected at BAP 

  PARAMETERS 

    btampHandle: The BT-AMP PAL handle returned in WLANBAP_GetNewHndl.
   
  RETURN VALUE

    The result code associated with performing the operation  

    VOS_STATUS_E_INVAL:  Input parameters are invalid 
    VOS_STATUS_E_FAULT:  BAP handle is NULL  
    VOS_STATUS_SUCCESS:  Everything is good :) 

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WLANBAP_DeInitLinkSupervision
( 
  ptBtampHandle     btampHandle 
)
{
    VOS_STATUS               vosStatus = VOS_STATUS_SUCCESS;
    ptBtampContext           pBtampCtx = (ptBtampContext) btampHandle;

    if ( NULL == pBtampCtx) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "Invalid BAP handle value in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }
   VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "In: %s", __func__);

   vosStatus = WLANBAP_StopLinkSupervisionTimer(pBtampCtx);

   
    /*Free the vos packet*/
    if ( pBtampCtx->lsRepPacket )
    {
      vosStatus = vos_pkt_return_packet(pBtampCtx->lsRepPacket);
      pBtampCtx->lsRepPacket = NULL;
    }

    if ( pBtampCtx->lsReqPacket )
    {
      vosStatus = vos_pkt_return_packet(pBtampCtx->lsReqPacket);
      pBtampCtx->lsReqPacket = NULL; 
    }
    

    return vosStatus;
}

/*===========================================================================

  FUNCTION    WLANBAP_RxProcLsPkt

  DESCRIPTION 

    This API will be called when Link Supervision frames are received at BAP

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
WLANBAP_RxProcLsPkt
( 
  ptBtampHandle     btampHandle, 
  v_U8_t            phy_link_handle,  /* Used by BAP to indentify the WLAN assoc. (StaId) */
  v_U16_t            RxProtoType,     /* Protocol Type from the frame received */
  vos_pkt_t         *vosRxLsBuff
)
{
    VOS_STATUS               vosStatus;
    ptBtampContext           pBtampCtx = (ptBtampContext) btampHandle;
    WLANBAP_8023HeaderType   w8023Header;
    v_SIZE_t                 HeaderLen = sizeof(w8023Header);


    /*------------------------------------------------------------------------
        Sanity check params
      ------------------------------------------------------------------------*/
    if ( NULL == pBtampCtx) 
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "Invalid BAP handle value in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
               "In %s Received RxProtoType=%x", __func__,RxProtoType);
    
    vos_pkt_extract_data(vosRxLsBuff,0,(v_VOID_t*)&w8023Header,&HeaderLen);
    if ( !(vos_mem_compare( w8023Header.vDA, pBtampCtx->self_mac_addr, VOS_MAC_ADDR_SIZE)
    && vos_mem_compare( w8023Header.vSA, pBtampCtx->peer_mac_addr, VOS_MAC_ADDR_SIZE)))
    {

        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "MAC address mismatch in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

    /*Free the vos packet*/
    vosStatus = vos_pkt_return_packet( vosRxLsBuff );
    if ( VOS_STATUS_SUCCESS != vosStatus)
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                     "Failed to free VOS packet in %s", __func__);
        return VOS_STATUS_E_FAULT;
    }

   
    /* Reset Link Supervision timer */
    if (RxProtoType ==  WLANTL_BT_AMP_TYPE_LS_REP)
    { 
        pBtampCtx->lsReqPktPending = FALSE;
        pBtampCtx->retries = 0;
        if (pBtampCtx->bapLinkSupervisionTimerInterval)
        {
            /* Restart the LS timer */
            WLANBAP_StopLinkSupervisionTimer(pBtampCtx);
            vosStatus = WLANBAP_StartLinkSupervisionTimer (pBtampCtx,
                   pBtampCtx->bapLinkSupervisionTimerInterval * WLANBAP_BREDR_BASEBAND_SLOT_TIME);
        }
    }
    else if(RxProtoType == WLANTL_BT_AMP_TYPE_LS_REQ)
    {
        if (pBtampCtx->bapLinkSupervisionTimerInterval)
        {
            /* Restart the LS timer */
            WLANBAP_StopLinkSupervisionTimer(pBtampCtx);
            vosStatus = WLANBAP_StartLinkSupervisionTimer (pBtampCtx,
                   pBtampCtx->bapLinkSupervisionTimerInterval * WLANBAP_BREDR_BASEBAND_SLOT_TIME);
        }
        pBtampCtx->pPacket = pBtampCtx->lsRepPacket;
        // Handle LS rep frame
        vosStatus = WLANBAP_TxLinkSupervision( btampHandle, phy_link_handle, pBtampCtx->pPacket, WLANTL_BT_AMP_TYPE_LS_REP);
    }
   
    return vosStatus; 

}

/* Tx callback function for LS packet */
static VOS_STATUS WLANBAP_TxLinkSupervisionCB
(
    v_PVOID_t   pvosGCtx,
    vos_pkt_t   *pPacket,
    VOS_STATUS  retStatus
)
{
    VOS_STATUS     vosStatus;
    ptBtampContext bapContext; /* Holds the btampContext value returned */ 
    vos_pkt_t                *pLSPacket; 

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO,
             "TxCompCB reached for LS Pkt");

    /* Get the BT AMP context from the global */
    bapContext = gpBtampCtx;

    if (!VOS_IS_STATUS_SUCCESS (retStatus))
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
              "TxCompCB:Transmit status Failure");
    }

    if ( pPacket == NULL )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                   "WLANBAP_TxCompCB bad input\n" );
        return VOS_STATUS_E_FAILURE;
    }


    /* Return the packet & reallocate */
    
    if( pPacket == bapContext->lsReqPacket )
    {
        vosStatus = WLANBAP_AcquireLSPacket( bapContext, &pLSPacket,32, TRUE );
    if( VOS_IS_STATUS_SUCCESS( vosStatus ) )
    {
            bapContext->lsReqPacket = pLSPacket;
    }
    else
    {
         VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO,
                       "%s:AcquireLSPacket failed\n",__func__);
         bapContext->lsReqPacket = NULL;
         return vosStatus;   
    }
    }
    else
    {
        vosStatus = WLANBAP_AcquireLSPacket( bapContext, &pLSPacket,32, FALSE );
        if( VOS_IS_STATUS_SUCCESS( vosStatus ) )
        {
            bapContext->lsRepPacket = pLSPacket;
        }
        else
        {
             VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO,
                           "%s:AcquireLSPacket failed\n",__func__);
             bapContext->lsRepPacket = NULL;
             return vosStatus;   
        }
    }
    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO,
               "%s:Returned Vos Packet:%p\n", __func__, pPacket );

    vos_pkt_return_packet( pPacket );

    return (VOS_STATUS_SUCCESS );
}

/*===========================================================================

  FUNCTION    WLANBAP_TxLinkSupervision

  DESCRIPTION 

    This API will be called to process Link Supervision Request received

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
WLANBAP_TxLinkSupervision
( 
  ptBtampHandle     btampHandle, 
  v_U8_t            phy_link_handle,  /* Used by BAP to indentify the WLAN assoc. (StaId) */
  vos_pkt_t         *pPacket,
  v_U16_t           protoType
)
{
    ptBtampContext             pBtampCtx = (ptBtampContext)btampHandle;
    VOS_STATUS                 vosStatus = VOS_STATUS_E_FAILURE;
    v_PVOID_t                  pvosGCtx;
    v_U8_t                     ucSTAId;  /* The StaId (used by TL, PE, and HAL) */
    v_PVOID_t                  pHddHdl; /* Handle to return BSL context in */
    WLANTL_MetaInfoType        metaInfo;


    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                         "In  : %s protoType=%x", __func__,protoType);

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
      VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                   "Unable to retrieve STA Id from BAP context and phy_link_handle in WLANBAP_TxLinKSupervisionReq");
      return VOS_STATUS_E_FAULT;
    }

    vos_mem_zero( &metaInfo, sizeof( WLANTL_MetaInfoType ) );
    
    metaInfo.ucTID = 0x00 ;
    metaInfo.ucUP = 0x00;
    metaInfo.ucIsEapol =  VOS_FALSE;//Notify TL that this is NOT an EAPOL frame
    metaInfo.ucDisableFrmXtl = VOS_FALSE;
    metaInfo.ucType = 0x00;
    pBtampCtx->metaInfo = metaInfo;
    
    vosStatus = WLANTL_TxBAPFrm( pvosGCtx, pPacket, &metaInfo, WLANBAP_TxLinkSupervisionCB );
    if( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                        "Tx: Packet rejected by TL in WLANBAP_TxLinkSupervisionReq");
        return vosStatus;
    }
    
    if(protoType ==  WLANTL_BT_AMP_TYPE_LS_REQ)
    {
        pBtampCtx->lsReqPktPending = TRUE;
        pBtampCtx->retries++;
    }
   
    if (pBtampCtx->bapLinkSupervisionTimerInterval)
    {
        /* Restart the LS timer */
        WLANBAP_StopLinkSupervisionTimer(pBtampCtx);
        vosStatus = WLANBAP_StartLinkSupervisionTimer (pBtampCtx,
               pBtampCtx->bapLinkSupervisionTimerInterval * WLANBAP_BREDR_BASEBAND_SLOT_TIME);
    }

   if( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                "WLANBAP_TxLinkSupervisionReq failed to Start LinkSupervision Timer\n" );
        return vosStatus;
   }
   
   return vosStatus;
} /* WLANBAP_RxLinkSupervisionReq */



