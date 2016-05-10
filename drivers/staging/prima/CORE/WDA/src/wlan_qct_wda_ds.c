/*
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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


                       W L A N _ Q C T _ WDA _ DS . C

  OVERVIEW:

  This software unit holds the implementation of WLAN Data Abtraction APIs
  for the WLAN Transport Layer.

  DEPENDENCIES:

  Are listed for each API below.


===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$ $DateTime: $ $Author: $


when        who          what, where, why
--------    ---         ----------------------------------------------
12/08/2010  seokyoun    Created. Move down HAL interfaces from TL to WDA
                        for UMAC convergence btween Volans/Libra and Prima
=========================================================================== */

#include "wlan_qct_wda.h"
#include "wlan_qct_tl.h"
#include "wlan_qct_tli.h"
#include "tlDebug.h"

#define WDA_DS_DXE_RES_COUNT   (WDA_TLI_MIN_RES_DATA + 20)

/* macro's for acessing TL API/data structures */
#define WDA_TL_SET_TX_XMIT_PENDING(a) WLANTL_SetTxXmitPending(a)
#define WDA_TL_IS_TX_XMIT_PENDING(a) WLANTL_IsTxXmitPending(a)
#define WDA_TL_CLEAR_TX_XMIT_PENDING(a) WLANTL_ClearTxXmitPending(a)

#define WDA_HI_FLOW_MASK 0xF0
#define WDA_LO_FLOW_MASK 0x0F

static v_VOID_t 
WDA_DS_TxCompleteCB
(
 v_PVOID_t pvosGCtx, 
 v_PVOID_t pFrameDataBuff
);


#ifdef WLAN_PERF
/*==========================================================================
  FUNCTION    WDA_TLI_FastHwFwdDataFrame

  DESCRIPTION 
    For NON integrated SOC, this function is called by TL.

    Fast path function to quickly forward a data frame if HAL determines BD 
    signature computed here matches the signature inside current VOSS packet. 
    If there is a match, HAL and TL fills in the swapped packet length into 
    BD header and DxE header, respectively. Otherwise, packet goes back to 
    normal (slow) path and a new BD signature would be tagged into BD in this
    VOSS packet later by the WLANHAL_FillTxBd() function.

  TODO  For integrated SOC, this function does nothing yet. Pima SLM/HAL 
        should provide the equivelant functionality.

  DEPENDENCIES 
     
  PARAMETERS 

   IN
        pvosGCtx    VOS context
        vosDataBuff Ptr to VOSS packet
        pMetaInfo   For getting frame's TID
        pStaInfo    For checking STA type
    
   OUT
        pvosStatus  returned status
        puFastFwdOK Flag to indicate whether frame could be fast forwarded
   
  RETURN VALUE
    No return.   

  SIDE EFFECTS 
  
============================================================================*/
void WDA_TLI_FastHwFwdDataFrame
(
  v_PVOID_t     pvosGCtx,
  vos_pkt_t*    vosDataBuff,
  VOS_STATUS*   pvosStatus,
  v_U32_t*       puFastFwdOK,
  WLANTL_MetaInfoType*  pMetaInfo,
  WLAN_STADescType*  pStaInfo
)
{
  /* FIXME WDI/WDA should support this function
     once HAL supports it
   */
}
#endif /*WLAN_PERF*/

/*==========================================================================
  FUNCTION    WDA_DS_Register

  DESCRIPTION 
    Register TL client to WDA. This function registers TL RX/TX functions
    to WDI by calling WDI_DS_Register.


    For NON integrated SOC, this function calls WLANBAL_RegTlCbFunctions
    to register TL's RX/TX functions to BAL

  TODO 
    For Prima, pfnResourceCB gets called in WDTS_OOResourceNotification.
    The uCount parameter is AC mask. It should be redefined to use the
    same resource callback function.

  DEPENDENCIES 
     
  PARAMETERS 

   IN
        pvosGCtx    VOS context
        pfnTxCompleteCallback       TX complete callback upon TX completion
        pfnRxPacketCallback         RX callback
        pfnTxPacketCallback         TX callback
        pfnResourceCB               gets called when updating TX PDU number
        uResTheshold                minimum TX PDU size for a packet
        pCallbackContext            WDI calls callback function with it
                                    VOS global context pointer
   OUT
        uAvailableTxBuf       available TX PDU numbder. 
                              BAL returns it for NON integrated SOC
   
  RETURN VALUE
    VOS_STATUS_E_FAULT:  pointer is NULL and other errors 
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS WDA_DS_Register 
( 
  v_PVOID_t                 pvosGCtx, 
  WDA_DS_TxCompleteCallback pfnTxCompleteCallback,
  WDA_DS_RxPacketCallback   pfnRxPacketCallback, 
  WDA_DS_TxPacketCallback   pfnTxPacketCallback,
  WDA_DS_ResourceCB         pfnResourceCB,
  v_U32_t                   uResTheshold,
  v_PVOID_t                 pCallbackContext,
  v_U32_t                   *uAvailableTxBuf
)
{
  tWDA_CbContext      *wdaContext = NULL;
  WDI_Status          wdiStatus;

  VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "WLAN WDA: WDA_DS_Register" );

  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( ( NULL == pvosGCtx ) ||
       ( NULL == pfnTxPacketCallback ) ||
       ( NULL == pfnTxCompleteCallback ) ||
       ( NULL == pfnRxPacketCallback ) ||
       ( NULL == pfnResourceCB) )
  {
    VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
               "WLAN WDA:Invalid pointers on WDA_DS_Register" );
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Extract WDA context
   ------------------------------------------------------------------------*/
  wdaContext = (tWDA_CbContext *)vos_get_context( VOS_MODULE_ID_WDA, pvosGCtx );
  if ( NULL == wdaContext )
  {
    VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
               "WLAN WDA:Invalid wda context pointer "
               "from pvosGCtx on WDA_DS_Register" );
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Register with WDI as transport layer client
  ------------------------------------------------------------------------*/
  VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH, "WDA: WDA_DS_Register");

  wdaContext->pfnTxResourceCB       = pfnResourceCB;
  wdaContext->pfnTxCompleteCallback = pfnTxCompleteCallback;

  wdiStatus = WDI_DS_Register( wdaContext->pWdiContext,
                               (WDI_DS_TxCompleteCallback)WDA_DS_TxCompleteCB,
                               (WDI_DS_RxPacketCallback)pfnRxPacketCallback,
                               WDA_DS_TxFlowControlCallback,
                               pvosGCtx );

  if ( WDI_STATUS_SUCCESS != wdiStatus )
  {
    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN TL:TL failed to register with DAL, Err: %d", wdiStatus );
    return VOS_STATUS_E_FAILURE;
  }
   
  /* TODO Out-of-resource condition if PDU size is less than WLANTL_MIN_RES_MF 
     Put hardcoded value not to put TL into OOR. Revisit it */
  *uAvailableTxBuf = WDA_DS_DXE_RES_COUNT; 

  return VOS_STATUS_SUCCESS;
}

/*==========================================================================
  FUNCTION    WDA_DS_StartXmit

  DESCRIPTION 
    Serialize TX transmit reques to TX thread. 

  TODO This sends TX transmit request to TL. It should send to WDI for
         abstraction.

    For NON integrated SOC, this function calls WLANBAL_StartXmit

  DEPENDENCIES 
     
  PARAMETERS 

   IN
        pvosGCtx    VOS context
   
  RETURN VALUE
    VOS_STATUS_E_FAULT:  pointer is NULL and other errors 
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WDA_DS_StartXmit
(
  v_PVOID_t pvosGCtx
)
{
  vos_msg_t                    sMessage;
  VOS_STATUS                   status;
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  if ( NULL == pvosGCtx )
  {
    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WLAN WDA:Invalid pointers on WDA_DS_StartXmit" );
    return VOS_STATUS_E_FAULT;
  }

  if(WDA_TL_IS_TX_XMIT_PENDING( pvosGCtx ))
  {  
    /*Already WDA_DS_TX_START_XMIT msg is pending in TL msg queue */
    return VOS_STATUS_SUCCESS;
  }
  /* Serialize our event  */
  VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "Serializing WDA TX Start Xmit event" );

  vos_mem_zero( &sMessage, sizeof(vos_msg_t) );

  sMessage.bodyptr = NULL;
  sMessage.type    = WDA_DS_TX_START_XMIT;

  WDA_TL_SET_TX_XMIT_PENDING(pvosGCtx);

  status = vos_tx_mq_serialize(VOS_MQ_ID_TL, &sMessage);

  if(status != VOS_STATUS_SUCCESS)
  {
    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_FATAL,
             "Serializing WDA TX Start Xmit event FAILED" );
    WDA_TL_CLEAR_TX_XMIT_PENDING(pvosGCtx);
  }
  return status;
}


/*==========================================================================
  FUNCTION    WDA_DS_FinishULA

  DESCRIPTION 
    Serialize Finish Upper Level Authentication reques to TX thread. 

  DEPENDENCIES 
     
  PARAMETERS 

   IN
        callbackRoutine    routine to be called in TX thread
        callbackContext    user data for the above routine 
   
  RETURN VALUE
    please see vos_tx_mq_serialize

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WDA_DS_FinishULA
(
 void (*callbackRoutine) (void *callbackContext),
 void  *callbackContext
)
{
  vos_msg_t                    sMessage;
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* Serialize our event  */
  VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_HIGH,
             "Serializing WDA_DS_FinishULA event" );

  vos_mem_zero( &sMessage, sizeof(vos_msg_t) );

  sMessage.bodyval  = (v_U32_t)callbackContext;
  sMessage.bodyptr  = callbackRoutine;
  sMessage.type     = WDA_DS_FINISH_ULA;

  return vos_tx_mq_serialize(VOS_MQ_ID_TL, &sMessage);
}/*WDA_DS_FinishULA*/

/*==========================================================================
   FUNCTION    WDA_DS_BuildTxPacketInfo

  DESCRIPTION
    Build TX meta info for integrated SOC.
    
    Same function calls HAL for reserve BD header space into VOS packet and
    HAL function to fill it.
    
  DEPENDENCIES

  PARAMETERS

   IN
    pvosGCtx         VOS context
    vosDataBuff      vos data buffer
    pvDestMacAddr   destination MAC address ponter
    ucDisableFrmXtl  Is frame xtl disabled?
    ucQosEnabled     Is QoS enabled?
    ucWDSEnabled     Is WDS enabled?
    extraHeadSpace   Extra head bytes. If it's not 0 due to 4 bytes align
                     of BD header.
    typeSubtype      typeSubtype from MAC header or TX metainfo/BD
    pAddr2           address 2
    uTid             tid
    txFlag
    timeStamp
    ucIsEapol
    ucUP

   OUT
    *pusPktLen       Packet length

  RETURN VALUE
    VOS_STATUS_E_FAULT:  pointer is NULL and other errors 
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WDA_DS_BuildTxPacketInfo
(
  v_PVOID_t       pvosGCtx,
  vos_pkt_t*      vosDataBuff,
  v_MACADDR_t*    pvDestMacAddr,
  v_U8_t          ucDisableFrmXtl,
  v_U16_t*        pusPktLen,
  v_U8_t          ucQosEnabled,
  v_U8_t          ucWDSEnabled,
  v_U8_t          extraHeadSpace,
  v_U8_t          typeSubtype,
  v_PVOID_t       pAddr2,
  v_U8_t          uTid,
  v_U32_t          txFlag,
  v_U32_t         timeStamp,
  v_U8_t          ucIsEapol,
  v_U8_t          ucUP
)
{
  VOS_STATUS             vosStatus;
  WDI_DS_TxMetaInfoType* pTxMetaInfo = NULL;
  v_SIZE_t               usMacAddrSize;
  wpt_FrameCtrl          *pFrameControl;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
    Extract TL control block
   ------------------------------------------------------------------------*/
  if ( ( NULL == pvosGCtx ) || ( NULL == vosDataBuff ) || ( NULL == pvDestMacAddr ) )
  {
    VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
               "WDA:Invalid parameter sent on WDA_DS_BuildTxPacketInfo" );
    return VOS_STATUS_E_FAULT;
  }

  /*------------------------------------------------------------------------
    Extract TX Meta Info pointer from PAL packet
   ------------------------------------------------------------------------*/
  pTxMetaInfo = WDI_DS_ExtractTxMetaData( VOS_TO_WPAL_PKT(vosDataBuff)  );
  if ( NULL == pTxMetaInfo )
  {
    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
        "WLAN TL:Invalid RxMetaInfo pointer from PAL packet on WLANTL_RxFrames");
    VOS_ASSERT( 0 );
    return VOS_STATUS_E_FAULT;
  }

  /* Zero out memeory */
  vos_mem_zero( pTxMetaInfo, sizeof( WDI_DS_TxMetaInfoType ) );

  /* Fill the TX Meta info */
  pTxMetaInfo->txFlags    = txFlag;
  pTxMetaInfo->qosEnabled = ucQosEnabled;
  pTxMetaInfo->fenableWDS = ucWDSEnabled;
  pTxMetaInfo->ac         = ucUP;
  pTxMetaInfo->fUP        = uTid;
  pTxMetaInfo->isEapol    = ucIsEapol;
  pTxMetaInfo->fdisableFrmXlt = ucDisableFrmXtl;
  pTxMetaInfo->frmType     = ( ( typeSubtype & 0x30 ) >> 4 );
  pTxMetaInfo->typeSubtype = typeSubtype;

  /* Length = MAC header + payload */
  vos_pkt_get_packet_length( vosDataBuff, pusPktLen);
  pTxMetaInfo->fPktlen = *pusPktLen;

  /* For management frames, peek into Frame Control
     field to get value of Protected Frame bit */
  pTxMetaInfo->fProtMgmtFrame = 0;
  if ( WDA_TLI_MGMT_FRAME_TYPE == pTxMetaInfo->frmType )
  {
    if ( 1 == ucDisableFrmXtl )  /* should be 802.11, but check */
    {
      vosStatus = vos_pkt_peek_data( vosDataBuff, 0, (v_PVOID_t)&pFrameControl,
                                     sizeof( wpt_FrameCtrl ));
      if ( VOS_STATUS_SUCCESS != vosStatus )
      {
        VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                   "WDA: Failed while attempting to extract Protect Bit in "
                   "Frame Control, status %d", vosStatus );
        VOS_ASSERT( 0 );
        return VOS_STATUS_E_FAULT;
      }
      pTxMetaInfo->fProtMgmtFrame = pFrameControl->wep;
      VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,
                 "WLAN TL: fProtMgmtFrame:%d", pTxMetaInfo->fProtMgmtFrame );
    }
  }

  // Dst address
  usMacAddrSize = VOS_MAC_ADDR_SIZE;
  vosStatus = vos_pkt_extract_data( vosDataBuff,
                    WLANTL_MAC_ADDR_ALIGN( ucDisableFrmXtl ),
                    (v_PVOID_t)pvDestMacAddr,
                    &usMacAddrSize );
  if ((VOS_STATUS_SUCCESS != vosStatus) ||
          (usMacAddrSize != VOS_MAC_ADDR_SIZE))
  {
    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
               "WDA:Failed while attempting to extract MAC Addr %d",
                vosStatus );
    VOS_ASSERT( 0 );
    return VOS_STATUS_E_FAULT;
  }

  vos_copy_macaddr( (v_MACADDR_t*)pTxMetaInfo->fSTAMACAddress, pvDestMacAddr );

  // ADDR2
  vos_copy_macaddr( (v_MACADDR_t*)pTxMetaInfo->addr2MACAddress, pAddr2 );

  /* Dump TX meta infro for debugging */
  VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO_LOW,
             "WLAN TL: Dump TX meta info: "
             "txFlags:%d, qosEnabled:%d, ac:%d, "
             "isEapol:%d, fdisableFrmXlt:%d, frmType:%d",
             pTxMetaInfo->txFlags, ucQosEnabled, pTxMetaInfo->ac,
             pTxMetaInfo->isEapol, pTxMetaInfo->fdisableFrmXlt,
             pTxMetaInfo->frmType );

  return VOS_STATUS_SUCCESS;
}


/*==========================================================================
   FUNCTION    WDS_DS_TrimRxPacketInfo

  DESCRIPTION
    Trim/Remove RX BD header for NON integrated SOC.
    It does nothing for integrated SOC.
    
  DEPENDENCIES

  PARAMETERS

   IN
    vosDataBuff      vos data buffer

  RETURN VALUE
    VOS_STATUS_E_FAULT:  pointer is NULL and other errors 
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WDA_DS_TrimRxPacketInfo
( 
  vos_pkt_t *vosDataBuff
)
{
  /* Nothing to trim
   * Do Nothing */

  return VOS_STATUS_SUCCESS;
}



/*==========================================================================
   FUNCTION    WDA_DS_PeekRxPacketInfo

  DESCRIPTION
    Return RX metainfo pointer for for integrated SOC.
    
    Same function will return BD header pointer.
    
  DEPENDENCIES

  PARAMETERS

   IN
    vosDataBuff      vos data buffer

    pvDestMacAddr   destination MAC address ponter
    bSwap            Want to swap BD header? For backward compatability
                     It does nothing for integrated SOC
   OUT
    *ppRxHeader      RX metainfo pointer

  RETURN VALUE
    VOS_STATUS_E_FAULT:  pointer is NULL and other errors 
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WDA_DS_PeekRxPacketInfo
(
  vos_pkt_t *vosDataBuff,
  v_PVOID_t *ppRxHeader,
  v_BOOL_t  bSwap
)
{
  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if (  NULL == vosDataBuff )
  {
    VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
               "WDA:Invalid parameter sent on WDA_DS_PeekRxPacketInfo" );
    return VOS_STATUS_E_FAULT;
  }

  *ppRxHeader = WDI_DS_ExtractRxMetaData( (wpt_packet*)vosDataBuff );

  if ( NULL == *ppRxHeader )
  {
    VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
               "WDA:Failed to get RX meta data from WDI" );
     return VOS_STATUS_E_FAILURE;
  }
     
  return VOS_STATUS_SUCCESS;
}



/*==========================================================================
   FUNCTION    WDA_DS_GetFrameTypeSubType

  DESCRIPTION
    Get typeSubtype from the packet. The BD header should have this.
    But some reason, Libra/Volans read it from 802.11 header and save it
    back to BD header. So for NON integrated SOC, this function does
    the same.

    For integrated SOC, WDI does the same, not TL. 
    It does return typeSubtype from RX meta info for integrated SOC.

  DEPENDENCIES

  PARAMETERS

   IN
    pvosGCtx         VOS context
    vosDataBuff      vos data buffer
    pRxHeader        RX meta info or BD header pointer

   OUT
    ucTypeSubtype    typeSubtype

  RETURN VALUE
    VOS_STATUS_E_FAULT:  pointer is NULL and other errors 
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WDA_DS_GetFrameTypeSubType
(
  v_PVOID_t pvosGCtx,
  vos_pkt_t *vosDataBuff,
  v_PVOID_t pRxHeader,
  v_U8_t    *ucTypeSubtype
)
{
  /*------------------------------------------------------------------------
    Sanity check
   ------------------------------------------------------------------------*/
  if ( NULL == pRxHeader )
  {
    VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
               "WDA:Invalid parameter sent on WDA_DS_GetFrameTypeSubType" );
    return VOS_STATUS_E_FAULT;
  }

  *ucTypeSubtype = ( WDA_GET_RX_TYPE( pRxHeader ) << 4 ) | WDA_GET_RX_SUBTYPE( pRxHeader );

  return VOS_STATUS_SUCCESS;
}


/*==========================================================================
   FUNCTION    WDA_DS_RxAmsduBdFix

  DESCRIPTION
    For backward compatability with Libra/Volans. Need to call HAL function
    for HW BD bug fix

    It does nothing for integrated SOC.

  DEPENDENCIES

  PARAMETERS

   IN
    pvosGCtx         VOS context
    pvBDHeader       BD header pointer

   OUT

  RETURN VALUE
    VOS_STATUS_E_FAULT:  pointer is NULL and other errors 
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WDA_DS_RxAmsduBdFix
(
  v_PVOID_t pvosGCtx,
  v_PVOID_t pvBDHeader
)
{
  /* Do nothing for Prima */
  return VOS_STATUS_SUCCESS;
}

/*==========================================================================
   FUNCTION    WDA_DS_GetRssi

  DESCRIPTION
    Get RSSI 

  TODO It returns hardcoded value in the meantime since WDA/WDI does nothing
       support it yet for Prima.

  DEPENDENCIES

  PARAMETERS

   IN
    vosDataBuff      vos data buffer

   OUT
    puRssi           RSSI

  RETURN VALUE
    VOS_STATUS_E_FAULT:  pointer is NULL and other errors 
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WDA_DS_GetRssi
(
  v_PVOID_t pvosGCtx,
  v_S7_t*   puRssi
)
{
  VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
         "WDA:halPS_GetRssi no longer supported. Need replacement");

  *puRssi = -30;

  return VOS_STATUS_SUCCESS;
}

/*==========================================================================
   FUNCTION    WDA_DS_GetTxResources

  DESCRIPTION
    It does return hardcoded value. It should bigger number than 0.
    Returning 0 will put TL in out-of-resource condition for TX.

    Return current PDU resources from BAL for NON integrated SOC.
    
  DEPENDENCIES

  PARAMETERS

   IN
    vosDataBuff      vos data buffer
   
   OUT
    puResCount        available PDU number for TX

  RETURN VALUE
    VOS_STATUS_E_FAULT:  pointer is NULL and other errors 
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WDA_DS_GetTxResources
( 
  v_PVOID_t pvosGCtx,
  v_U32_t*  puResCount
)
{
  /* Return minimum necessary number of packet(DXE descriptor) for data */
  /* TODO Need to get this from DXE??? */
  *puResCount = WDA_TLI_BD_PDU_RESERVE_THRESHOLD + 50;
  return VOS_STATUS_SUCCESS;
}



/*==========================================================================
   FUNCTION    WDA_DS_GetReplayCounter

  DESCRIPTION
    Return replay counter from BD header or RX meta info

  DEPENDENCIES

  PARAMETERS

   IN
    pRxHeader        RX meta info or BD header pointer

   OUT

  RETURN VALUE
    Replay Counter

  SIDE EFFECTS

============================================================================*/
v_U64_t
WDA_DS_GetReplayCounter
(
  v_PVOID_t pRxHeader
)
{
  return WDA_GET_RX_REPLAY_COUNT( pRxHeader );
}

/*==========================================================================
   FUNCTION    WDA_DS_TxFrames

  DESCRIPTION
    Pull packets from TL and push them to WDI. It gets invoked upon
    WDA_DS_TX_START_XMIT.

    This function is equivelant of WLANSSC_Transmit in Libra/Volans.

  TODO
    This function should be implemented and moved in WDI.

  DEPENDENCIES

  PARAMETERS

   IN
    pvosGCtx         VOS context

   OUT

  RETURN VALUE
    VOS_STATUS_E_FAULT:  pointer is NULL and other errors 
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/

VOS_STATUS
WDA_DS_TxFrames
( 
  v_PVOID_t pvosGCtx 
)
{
  VOS_STATUS vosStatus;
  vos_pkt_t  *pTxMgmtChain = NULL;
  vos_pkt_t  *pTxDataChain = NULL;
  vos_pkt_t  *pTxPacket = NULL;
  v_BOOL_t   bUrgent;
  wpt_uint32  ucTxResReq;
  WDI_Status wdiStatus;
  tWDA_CbContext *wdaContext = NULL;
  v_U32_t     uMgmtAvailRes;
  v_U32_t     uDataAvailRes;
  WLANTL_TxCompCBType  pfnTxComp = NULL;
  v_U32_t     uTxFailCount = 0;

  wdaContext = (tWDA_CbContext *)vos_get_context(VOS_MODULE_ID_WDA, pvosGCtx);
  if ( NULL == wdaContext )
  {
    VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
               "WDA:Invalid wda context pointer from pvosGCtx on WDA_DS_TxFrames" );
    return VOS_STATUS_E_FAULT;
  }

  /*-------------------------------------------------------------------------
     Need to fetch separatelly for Mgmt and Data frames because TL is not
     aware of separate resource management at the lower levels 
  -------------------------------------------------------------------------*/
  /*Mgmt tx*/
  uMgmtAvailRes = WDI_GetAvailableResCount(wdaContext->pWdiContext, 
                                           WDI_MGMT_POOL_ID);
  
  ucTxResReq = WLANTL_GetFrames( pvosGCtx, 
                              &pTxMgmtChain, 
                               uMgmtAvailRes, 
                              (wdaContext->uTxFlowMask & WDA_HI_FLOW_MASK),
                              &bUrgent );

  // We need to initialize vsoStatus in case we don't enter the "while"
  // loop.  If we don't enter the loop, it means that there are no packets,
  // available, and that is considered success.  If we enter the loop,
  // vosStatus will be set appropriately inside the loop
  vosStatus = VOS_STATUS_SUCCESS;
      
  while ( NULL != pTxMgmtChain )
  {
    /* Walk the chain and unchain the packet */
    pTxPacket = pTxMgmtChain;
    vosStatus = vos_pkt_walk_packet_chain( pTxMgmtChain, &pTxMgmtChain, VOS_TRUE );

    if( (VOS_STATUS_SUCCESS != vosStatus) &&
        (VOS_STATUS_E_EMPTY != vosStatus) )
    {
      VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
                 "WDA Walking packet chain returned status : %d", vosStatus );
      VOS_ASSERT( 0 );
      vosStatus = VOS_STATUS_E_FAILURE;
      break;
    }

    if ( VOS_STATUS_E_EMPTY == vosStatus )
    {
       vosStatus = VOS_STATUS_SUCCESS;
    }

    wdiStatus = WDI_DS_TxPacket( wdaContext->pWdiContext, 
                                 (wpt_packet*)pTxPacket, 
                                 0 /* more */ );
    if ( WDI_STATUS_SUCCESS != wdiStatus )
    {
      VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
                  "WDA : Pushing a packet to WDI failed." );
      if ( WDI_STATUS_E_NOT_ALLOWED != wdiStatus )
      {
        uTxFailCount++;
      }
      VOS_ASSERT( wdiStatus == WDI_STATUS_E_NOT_ALLOWED );
      //We need to free the packet here
      vos_pkt_get_user_data_ptr(pTxPacket, VOS_PKT_USER_DATA_ID_TL, (void **)&pfnTxComp);
      if(pfnTxComp)
      {
         pfnTxComp(pvosGCtx, pTxPacket, VOS_STATUS_E_FAILURE);
      }
    }

  };

  if ( uTxFailCount )
  {
    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                 "WDA : Tx fail count for mgmt pkts: %d.", uTxFailCount);
    uTxFailCount = 0;
  }

  /*Data tx*/
  uDataAvailRes = WDI_GetAvailableResCount(wdaContext->pWdiContext, 
                                           WDI_DATA_POOL_ID);

  ucTxResReq = WLANTL_GetFrames( pvosGCtx, 
                              &pTxDataChain, 
                              /*WDA_DS_DXE_RES_COUNT*/ uDataAvailRes, 
                              (wdaContext->uTxFlowMask & WDA_LO_FLOW_MASK),
                              &bUrgent );

  // We need to initialize vsoStatus in case we don't enter the "while"
  // loop.  If we don't enter the loop, it means that there are no packets,
  // available, and that is considered success.  If we enter the loop,
  // vosStatus will be set appropriately inside the loop
  vosStatus = VOS_STATUS_SUCCESS;

  while ( NULL != pTxDataChain )
  {
    /* Walk the chain and unchain the packet */
    pTxPacket = pTxDataChain;
    vosStatus = vos_pkt_walk_packet_chain( pTxDataChain, &pTxDataChain, VOS_TRUE );

    if( (VOS_STATUS_SUCCESS != vosStatus) &&
        (VOS_STATUS_E_EMPTY != vosStatus) )
    {
      VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
                 "WDA Walking packet chain returned status : %d", vosStatus );
      VOS_ASSERT( 0 );
      vosStatus = VOS_STATUS_E_FAILURE;
      break;
    }

    if ( VOS_STATUS_E_EMPTY == vosStatus )
    {
       vosStatus = VOS_STATUS_SUCCESS;
    }

    wdiStatus = WDI_DS_TxPacket( wdaContext->pWdiContext, 
                                 (wpt_packet*)pTxPacket, 
                                 0 /* more */ );
    if ( WDI_STATUS_SUCCESS != wdiStatus )
    {
      VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,
                  "WDA : Pushing a packet to WDI failed." );
      if ( WDI_STATUS_E_NOT_ALLOWED != wdiStatus )
      {
        uTxFailCount++;
      }
      VOS_ASSERT( wdiStatus == WDI_STATUS_E_NOT_ALLOWED );
      //We need to free the packet here
      vos_pkt_get_user_data_ptr(pTxPacket, VOS_PKT_USER_DATA_ID_TL, (void **)&pfnTxComp);
      if(pfnTxComp)
      {
         pfnTxComp(pvosGCtx, pTxPacket, VOS_STATUS_E_FAILURE);
      }
    }

  };

  if ( uTxFailCount )
  {
    VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                 "WDA : Tx fail count for data pkts: %d.", uTxFailCount);
  }


  WDI_DS_TxComplete(wdaContext->pWdiContext, ucTxResReq);

  return vosStatus;
}

/*==========================================================================
   FUNCTION    WDA_DS_TxFlowControlCallback

  DESCRIPTION
    Invoked by WDI to control TX flow.

  DEPENDENCIES

  PARAMETERS

   IN
    pvosGCtx         VOS context
    uFlowMask        TX channel mask for flow control
                     Defined in WDA_TXFlowEnumType

   OUT

  RETURN VALUE

  SIDE EFFECTS

============================================================================*/
v_VOID_t
WDA_DS_TxFlowControlCallback
(
   v_PVOID_t pvosGCtx,
   v_U8_t    ucFlowMask
)
{
   tWDA_CbContext* wdaContext = NULL;
    v_U8_t          ucOldFlowMask;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

   /*------------------------------------------------------------------------
     Sanity check
    ------------------------------------------------------------------------*/
   if ( NULL == pvosGCtx )
   {
      VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
                 "WDA: Invalid parameter sent on WDA_DS_TxFlowControlCallback" );
      return;
   }

   wdaContext = (tWDA_CbContext *)vos_get_context( VOS_MODULE_ID_WDA, pvosGCtx );
   if ( NULL == wdaContext )
   {
      VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
                 "WDA: Invalid context on WDA_DS_TxFlowControlCallback" );
      return;
   }

   /* two physical DXE channels
      1) data packets(all four AC) and BAP for the low priority channel(lower 4 bits) 
      2) management packets for high priority channel(5th bit)
   */


   /*Save and reset */
   ucOldFlowMask           = wdaContext->uTxFlowMask; 
   wdaContext->uTxFlowMask = ucFlowMask;

   /*If the AC is being enabled - resume data xfer 
    
    Assume previous value of wdaContext->uTxFlowMask: 
    
    DATA\MGM |  ON  | OFF
    ----------------------
        ON   | 1F   | 0F *
    ----------------------
        OFF  |  10 *| 00 *
    
        * - states in which a channel can be enabled
    
      ucFlowMask will tell which channel must be enabled
      to enable a channel a new bit must be turned on =>
      ucFlowMask > wdaContext->uTxFlowMask when enable happens
   */

   if ( ucFlowMask > ucOldFlowMask  )
   {
     WDA_DS_StartXmit(pvosGCtx);
   }

}

/*==========================================================================
   FUNCTION    WDA_DS_GetTxFlowMask

  DESCRIPTION
    return TX flow mask control value

  DEPENDENCIES

  PARAMETERS

   IN
    pvosGCtx         VOS context

   OUT
    uFlowMask        TX channel mask for flow control
                     Defined in WDA_TXFlowEnumType

  RETURN VALUE
    VOS_STATUS_E_INVAL:  pointer is NULL and other errors 
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WDA_DS_GetTxFlowMask
(
 v_PVOID_t pvosGCtx,
 v_U8_t*   puFlowMask
)
{
   tWDA_CbContext* wdaContext = NULL;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

   /*------------------------------------------------------------------------
     Sanity check
    ------------------------------------------------------------------------*/
   if ( ( NULL == pvosGCtx ) || ( NULL == puFlowMask ) )
   {
      VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
                 "WDA: Invalid parameter sent on WDA_DS_GetTxFlowMask" );
      return VOS_STATUS_E_INVAL;
   }

   wdaContext = (tWDA_CbContext *)vos_get_context( VOS_MODULE_ID_WDA, pvosGCtx );
   if ( NULL == wdaContext )
   {
      VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
                 "WDA: Invalid context on WDA_DS_GetTxFlowMask" );
      return VOS_STATUS_E_INVAL;
   }

   *puFlowMask = wdaContext->uTxFlowMask;

   return VOS_STATUS_SUCCESS;
}

v_VOID_t 
WDA_DS_TxCompleteCB
(
 v_PVOID_t pvosGCtx, 
 v_PVOID_t pFrameDataBuff
)
{
  tWDA_CbContext*        wdaContext = NULL;
  WDI_DS_TxMetaInfoType* pTxMetadata;
  VOS_STATUS             vosStatus;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check
    ------------------------------------------------------------------------*/

  if ( ( NULL == pvosGCtx ) || ( NULL == pFrameDataBuff ) )
  {
    VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
               "WDA: Invalid parameter sent on WDA_DS_TxCompleteCB" );
    return;
  }

  wdaContext = (tWDA_CbContext *)vos_get_context( VOS_MODULE_ID_WDA, pvosGCtx );
  if ( NULL == wdaContext )
  {
    VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
               "WDA: Invalid context on WDA_DS_TxCompleteCB" );
    return;
  }

  // extract metadata from PAL packet
  pTxMetadata = WDI_DS_ExtractTxMetaData( (wpt_packet*)pFrameDataBuff );
  
  if ( eWLAN_PAL_STATUS_SUCCESS == pTxMetadata->txCompleteStatus )
    vosStatus = VOS_STATUS_SUCCESS;
  else 
    vosStatus = VOS_STATUS_E_FAILURE;

  wdaContext->pfnTxCompleteCallback( pvosGCtx, pFrameDataBuff, vosStatus );
}
