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

                       W L A N _ Q C T _ W D I _ D P. C

  OVERVIEW:

  This software unit holds the implementation of the WLAN Device Abstraction     
  Layer Internal Utility routines to be used by the Data Path.

  The functions externalized by this module are to be only by the WDI data
  path.
 
  The module leveraged as much as functionality as was possible from the HAL
  in Libra/Volans.
 
  DEPENDENCIES:

  Are listed for each API below.


  Copyright (c) 2010 QUALCOMM Incorporated.
  All Rights Reserved.
  Qualcomm Confidential and Proprietary
===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


   $Header$$DateTime$$Author$


  when        who     what, where, why
----------    ---    --------------------------------------------------------
2010-08-19    lti     Created module

===========================================================================*/

#include "wlan_qct_pal_api.h"
#include "wlan_qct_pal_type.h"
#include "wlan_qct_wdi.h"
#include "wlan_qct_wdi_i.h"
#include "wlan_qct_wdi_sta.h"
#include "wlan_qct_wdi_dp.h"
#include "wlan_qct_wdi_bd.h"
#include "wlan_qct_pal_trace.h"

#include "wlan_qct_dev_defs.h"
#define MAC_ADDR_ARRAY(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MAC_ADDRESS_STR "%02x:%02x:%02x:%02x:%02x:%02x"

extern uint8 WDA_IsWcnssWlanCompiledVersionGreaterThanOrEqual(uint8 major, uint8 minor, uint8 version, uint8 revision);
extern uint8 WDA_IsWcnssWlanReportedVersionGreaterThanOrEqual(uint8 major, uint8 minor, uint8 version, uint8 revision);


/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------- 
   TID->QueueID mapping
 --------------------------------------------------------------------------*/
static wpt_uint8 btqmQosTid2QidMapping[] = 
{ 
    BTQM_QID0, 
    BTQM_QID1, 
    BTQM_QID2, 
    BTQM_QID3, 
    BTQM_QID4, 
    BTQM_QID5, 
    BTQM_QID6, 
    BTQM_QID7 
};
/*===========================================================================
                       Helper Internal API 
 ===========================================================================*/

/**
 @brief WDI_DP_UtilsInit - Intializes the parameters required to 
        interact with the data path
  
 @param       pWDICtx:    pointer to the main WDI Ctrl Block
  
 @return   success always
*/
WDI_Status 
WDI_DP_UtilsInit
(
  WDI_ControlBlockType*  pWDICtx
)
{
  WDI_RxBdType*  pAmsduRxBdFixMask; 

    // WQ to be used for filling the TxBD
  pWDICtx->ucDpuRF = BMUWQ_BTQM_TX_MGMT; 

#ifdef WLAN_PERF
  pWDICtx->uBdSigSerialNum = 0;
#endif

  pAmsduRxBdFixMask = &pWDICtx->wdiRxAmsduBdFixMask;

  wpalMemoryFill(pAmsduRxBdFixMask,sizeof(WDI_RxBdType), 0xff);

  pAmsduRxBdFixMask->penultimatePduIdx = 0;
  pAmsduRxBdFixMask->headPduIdx        = 0;
  pAmsduRxBdFixMask->tailPduIdx        = 0;
  pAmsduRxBdFixMask->mpduHeaderLength  = 0;
  pAmsduRxBdFixMask->mpduHeaderOffset  = 0;
  pAmsduRxBdFixMask->mpduDataOffset    = 0;
  pAmsduRxBdFixMask->pduCount          = 0;
  pAmsduRxBdFixMask->mpduLength        = 0;
  pAmsduRxBdFixMask->asf               = 0;
  pAmsduRxBdFixMask->esf               = 0;
  pAmsduRxBdFixMask->lsf               = 0;
  pAmsduRxBdFixMask->processOrder      = 0;
  pAmsduRxBdFixMask->sybFrameIdx       = 0;
  pAmsduRxBdFixMask->totalMsduSize     = 0;
  pAmsduRxBdFixMask->aduFeedback       = 0;

  return WDI_STATUS_SUCCESS;
}/*WDI_DP_UtilsInit*/


/**
 @brief WDI_DP_UtilsExit - Clears the parameters required to
        interact with the data path
  
 @param       pWDICtx:    pointer to the main WDI Ctrl Block
  
 @return   success always
*/
WDI_Status
WDI_DP_UtilsExit
( 
    WDI_ControlBlockType*  pWDICtx
)
{
   return WDI_STATUS_SUCCESS;
}/*WDI_DP_UtilsExit*/

/**
 @brief WDI_SwapBytes - Swap Bytes of a given buffer
  
 @param  pBd:    buffer to be swapped 
         nbSwap: number of bytes to swap
  
 @return   none
*/
WPT_STATIC WPT_INLINE void 
WDI_SwapBytes
(
    wpt_uint8 *pBd, 
    wpt_uint32 nbSwap
)
{
  wpt_uint32 *pU32;
  wpt_uint32 nU32;
  wpt_uint32 wc;

  nU32 = (((nbSwap) + 3)>>2);

  pU32 = (wpt_uint32 *)pBd;
  for ( wc = 0; wc < nU32; wc++ )
  {
    pU32[ wc ] = WPAL_BE32_TO_CPU( pU32[ wc ] );
  }
}/*WDI_SwapBytes*/

/**
 @brief WDI_BmuGetQidForQOSTid - returns the BMU QID for a given 
        TID
 
 @param  ucTid:  TID
         pQid:   out QID
  
 @see
 @return Result of the function call
*/
WDI_Status 
WDI_BmuGetQidForQOSTid
(
    wpt_uint8   ucTid, 
    wpt_uint8*  pQid
)
{
    if (ucTid > BTQM_QUEUE_TX_TID_7 )
        return WDI_STATUS_E_FAILURE;
        
    *pQid = btqmQosTid2QidMapping[ucTid];
    return WDI_STATUS_SUCCESS;
}/*WDI_BmuGetQidForQOSTid*/

#ifdef WLAN_PERF

/**
 @brief WDI_ComputeTxBdSignature - computes the BD signature
  
 @param   pWDICtx:       pointer to the global WDI context;
 
    pDestMacAddr:   destination MAC address
    
    ucTid:            TID of the frame

    ucDisableFrmXtl:  Unicast destination
  
 @return   the signature
*/
static wpt_uint32 
WDI_ComputeTxBdSignature
(  
    WDI_ControlBlockType*  pWDICtx,
    wpt_uint8*             pDestMac, 
    wpt_uint8              ucTid, 
    wpt_uint8              ucUnicastDst
)
{
    wpt_uint16 *pMacU16 = (wpt_uint16 *) pDestMac;

    return ((pMacU16[0] ^ pMacU16[1] ^ pMacU16[2])<< WDI_TXBD_SIG_MACADDR_HASH_OFFSET |
        pWDICtx->uBdSigSerialNum << WDI_TXBD_SIG_SERIAL_OFFSET | 
        ucTid << WDI_TXBD_SIG_TID_OFFSET |
        ucUnicastDst << WDI_TXBD_SIG_UCAST_DATA_OFFSET);
}/*WDI_ComputeTxBdSignature*/


/**
 @brief WDI_TxBdFastFwd - evaluates if a frame can be fast 
        forwarded 
  
 @param   pWDICtx: Context to the WDI 
          pDestMac: Destination MAC
          ucTid: packet TID pBDHeader
          ucUnicastDst: is packet unicast
          pTxBd:       pointer to the BD header
          usMpduLength: len 
  
 @return 1 - if the frame can be fast fwd-ed ; 0 if not 
*/
wpt_uint32 
WDI_TxBdFastFwd
(
    WDI_ControlBlockType*  pWDICtx,
    wpt_uint8*             pDestMac, 
    wpt_uint8              ucTid, 
    wpt_uint8              ucUnicastDst, 
    void*                  pTxBd, 
    wpt_uint16             usMpduLength 
 )
{
    WDI_TxBdType*     pBd      = (WDI_TxBdType*) pTxBd;
    wpt_uint32        uRetval  = 0;
#ifdef WPT_LITTLE_BYTE_ENDIAN
    wpt_uint16        usSwapped; 
    wpt_uint16*       pU16     = (wpt_uint16 *) pTxBd;
#endif

    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

    if( pBd->txBdSignature ==  
        WDI_ComputeTxBdSignature(pWDICtx, pDestMac, ucTid, ucUnicastDst))
    {

#ifdef WPT_LITTLE_BYTE_ENDIAN
       /* When swap to BE format, mpduLength field is at 8th WORD location(16th byte) */
       usSwapped = wpt_cpu_to_be16(usMpduLength); 
       pU16[8]   = usSwapped;
#else
        /* Remove the #error when ported to a real BIG ENDIAN machine */
       // #error "Is host byte order really BIG endian?"
       /* When host is already in BE format, no swapping needed.*/
       pBd->mpduLength = usMpduLength;
#endif
       uRetval = 1;
    }
    return uRetval ;
}/*WDI_TxBdFastFwd*/

#endif /*WLAN_PERF*/

/*===========================================================================
                             External API 
 ===========================================================================*/

/**
 @brief   WLANHAL_FillTxBd - Called by TL to fill in TxBD. 

    Following are the highlights of the function

    1. All unicast data packets are sent by data rate decided by TPE.
    (i.e BD rates are disabled).
 
    2. All u/mcast management packets would go in Broadcast
    Management Rates
 
    3. dpuNE would be disabled for all data packets
 
    4. dpuNE would be enabled for all management packets
    excluding packets when RMF is enabled
 
    5. QID8 at self STA is for broadcast data which uses no ACK
    policy.
 
    6. QID9 at self STA, we use it for unicast mgmt and set ACK
    policy to normal ACK.
 
    7. QID10 at self STA, we use it for b/mcast mgmt and set ACK
    policy to NO ACK.
 
    WDI DP Utilities modules must be initiatilized before this
    API can be called.
 
   @param

    IN
    pWDICtx:       pointer to the global WDI context;
 
    ucTypeSubtype:    802.11 [5:4] ucType [3:0] subtype

    pDestMacAddr:   destination MAC address
    
    pTid:           ptr to TID of the frame

    ucDisableFrmXtl:  When set, disables UMA HW frame
                    translation and WDI needs to fill in all BD
                    fields. When not set, UMA performs BD
                    filling and frame translation

    pTxBd:          ptr to the TxBD

    ucTxFlag:    different option setting for TX.

    ucProtMgmtFrame: for management frames, whether the frame is
                     protected (protect bit is set in FC)

    uTimeStamp:      Timestamp when the frame was received from HDD. (usec)
   
   @return
    The result code associated with performing the operation  
 
*/

WDI_Status
WDI_FillTxBd
(
    WDI_ControlBlockType*  pWDICtx, 
    wpt_uint8              ucTypeSubtype, 
    void*                  pDestMacAddr,
    void*                  pAddr2,
    wpt_uint8*             pTid, 
    wpt_uint8              ucDisableFrmXtl, 
    void*                  pTxBd, 
    wpt_uint32             ucTxFlag,
    wpt_uint8              ucProtMgmtFrame,
    wpt_uint32             uTimeStamp,
    wpt_uint8              isEapol,
    wpt_uint8*             staIndex
)
{
    wpt_uint8              ucTid        = *pTid; 
    WDI_TxBdType*          pBd          = (WDI_TxBdType*) pTxBd;
    WDI_Status             wdiStatus    = WDI_STATUS_SUCCESS;
    wpt_uint8              ucUnicastDst = 0;
    wpt_uint8              ucType       = 0;
    wpt_uint8              ucSubType    = 0;
    wpt_uint8              ucIsRMF      = 0;
    WDI_BSSSessionType*    pBSSSes;
    wpt_uint8              ucSTAType   = 0;
#ifdef WLAN_PERF
    wpt_uint32      uTxBdSignature = pBd->txBdSignature;
#endif
    tANI_U8                 useStaRateForBcastFrames = 0;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    /*------------------------------------------------------------------------
       Get type and subtype of the frame first 
    ------------------------------------------------------------------------*/
    ucType = (ucTypeSubtype & WDI_FRAME_TYPE_MASK) >> WDI_FRAME_TYPE_OFFSET;
    ucSubType = (ucTypeSubtype & WDI_FRAME_SUBTYPE_MASK);

    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN, 
               "Type: %d/%d, MAC S: %08x. MAC D: %08x., Tid=%d, frmXlat=%d, pTxBD=%p ucTxFlag 0x%X",
                ucType, ucSubType, 
                *((wpt_uint32 *) pAddr2), 
               *((wpt_uint32 *) pDestMacAddr), 
                ucTid, 
               !ucDisableFrmXtl, pTxBd, ucTxFlag );


    //logic to determine the version match between host and riva to find out when to enable using STA rate for bcast frames.
    //determine if Riva vsersion and host version both are greater than or equal to 0.0.2 (major, minor, version). if yes then use STA rate 
    // instead of BD rate for BC/MC frames. Otherwise use old code to use BD rate instead.
    {    
        if (WDA_IsWcnssWlanCompiledVersionGreaterThanOrEqual(0, 0, 2, 0) &&
            WDA_IsWcnssWlanReportedVersionGreaterThanOrEqual(0, 0, 2, 0))
            useStaRateForBcastFrames = 1;
    }


    /*-----------------------------------------------------------------------
    * Set common fields in TxBD
     *     bdt: always HWBD_TYPE_GENERIC
           dpuRF: This is not used in Gen6 since all WQs are explicitly
           programmed to each HW module
     *     ucTid: from caller, ignored if frame is MGMT frame
     *     fwTxComplete0: always set to 0
     *     txComplete1: If TxComp inrs  is requested, enable TxComplete interrupt
     *     dpuFeedback/aduFeedback/reserved2: Always set to 0
           ap: ACK policy to be placed in Qos ctrl field. Ignored by HW if non
           Qos ucType frames.
           u/b: If Addr1 of this frame in its 802.11 form is unicast, set to 0.
           Otherwise set to 1.
           dpuNE: always set to 0. DPU also uses the privacy bit in 802.11 hdr
           for encryption decision
     -----------------------------------------------------------------------*/
    pBd->bdt   = HWBD_TYPE_GENERIC; 

    // Route all trigger enabled frames to FW WQ, for FW to suspend trigger frame generation 
    // when no traffic is exists on trigger enabled ACs
    if(ucTxFlag & WDI_TRIGGER_ENABLED_AC_MASK) {
        pBd->dpuRF = pWDICtx->ucDpuRF; 
    } else 
    {
        pBd->dpuRF = BMUWQ_BTQM_TX_MGMT; 
    }

    if (ucTxFlag & WDI_USE_FW_IN_TX_PATH)
    {
        WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
          "iType: %d SubType %d, MAC S: %08x. MAC D: %08x., Tid=%d",
                    ucType, ucSubType,
                    *((wpt_uint32 *) pAddr2),
                   *((wpt_uint32 *) pDestMacAddr),
                    ucTid);

        pBd->dpuRF = BMUWQ_FW_DPU_TX;
    }

    pBd->tid           = ucTid; 
    // Clear the reserved field as this field is used for defining special 
    // flow control BD.
    pBd->reserved4 = 0;
    pBd->fwTxComplete0 = 0;

    /* This bit is for host to register TxComplete Interrupt */
    pBd->txComplete1   = (ucTxFlag & WDI_TXCOMP_REQUESTED_MASK) ? 1 : 0; 

    pBd->ap    = WDI_ACKPOLICY_ACK_REQUIRED; 
    pBd->dpuNE = WDI_NO_ENCRYPTION_DISABLED;  

    ucUnicastDst = !(((wpt_uint8 *)pDestMacAddr)[0] & 0x01);    
    *((wpt_uint32 *)pBd + WDI_DPU_FEEDBACK_OFFSET) = 0;

    if(!ucUnicastDst)
    {
      pBd->ap    = WDI_ACKPOLICY_ACK_NOTREQUIRED; 
    }

    if (ucType == WDI_MAC_DATA_FRAME)
    {

        /* Set common fields for data frames (regardless FT enable/disable)
         *     bd_ssn: Let DPU auto generate seq # if QosData frame. All other
               frames DPU generates seq using nonQos counter.
               For QosNull, don't occupy one Qos seq # to avoid a potential 
               hole seen in reorder buffer when BA is enabled.
 
         *     bd_rate:HW default or broadcast data rate
         *     rmf:    RMF doesn't apply for data frames. Always set to 0
         *     u/b: If Addr1 of this frame in its 802.11 form is unicast,
               set to 0. Otherwise set to 1.
         * Sanity: Force disable HW frame translation if incoming frame is
           NULL data frame
         */

        if ((ucSubType & WDI_MAC_DATA_QOS_DATA)&&
            (ucSubType != WDI_MAC_DATA_QOS_NULL))
        {
            pBd->bd_ssn = WDI_TXBD_BD_SSN_FILL_DPU_QOS;
        }
        else
        {
            pBd->bd_ssn = WDI_TXBD_BD_SSN_FILL_DPU_NON_QOS;
        }

        /* Unicast/Mcast decision:
         *  In Infra STA role, all frames to AP are unicast frames.
         *  For IBSS, then check the actual DA MAC address 
            This implementation doesn't support multi BSS and AP case.
            if(eSYSTEM_STA_IN_IBSS_ROLE == systemRole) 
            ucUnicastDst = !(((wpt_uint8 *)pDestMacAddr)[0] & 0x01);
               else
            ucUnicastDst = WDI_DEFAULT_UNICAST_ENABLED;
 
            The above is  original HAL code - however to make implementation
            more elastic and supportive of concurrency scenarios we shall just
            assume that bcast bit of MAC adddress cannot be set if addr is not
            bcast: (!! may want to revisit this during testing) 
         */

        //Broadcast frames buffering don't work well if BD rate is used in AP mode.
        //always use STA rate for data frames.
        //never use BD rate for BC/MC frames in AP mode.


        if (useStaRateForBcastFrames)
        {
            pBd->bdRate = WDI_TXBD_BDRATE_DEFAULT;
        }
        else
        {
            pBd->bdRate = (ucUnicastDst)? WDI_TXBD_BDRATE_DEFAULT : WDI_BDRATE_BCDATA_FRAME;
        }
#ifdef FEATURE_WLAN_TDLS
        if ( ucTxFlag & WDI_USE_BD_RATE2_FOR_MANAGEMENT_FRAME)
        {
           pBd->bdRate = WDI_BDRATE_CTRL_FRAME;
        }
#endif

        if(ucTxFlag & WDI_USE_BD_RATE_MASK)
        {
            pBd->bdRate = WDI_BDRATE_BCDATA_FRAME;
        }

        pBd->rmf    = WDI_RMF_DISABLED;     

        /* sanity: Might already be set by caller, but enforce it here again */
        if( WDI_MAC_DATA_NULL == (ucSubType & ~WDI_MAC_DATA_QOS_DATA))
        {
            ucDisableFrmXtl = 1;
            if (ucTxFlag & WDI_TXCOMP_REQUESTED_MASK) 
            {
                /*Send to FW to transmit NULL frames.*/
                pBd->dpuRF = BMUWQ_FW_TRANSMIT; 
            }
            else
            {
#ifdef LIBRA_WAPI_SUPPORT
                if (ucTxFlag & WDI_WAPI_STA_MASK)
                {
                    pBd->dpuRF = BMUWQ_WAPI_DPU_TX;
                    /*set NE bit to 1 for the null/qos null frames*/
                    pBd->dpuNE = WDI_NO_ENCRYPTION_ENABLED;
                }
#endif
            }
         }
#if defined(WLAN_PERF) || defined(FEATURE_WLAN_WAPI) || defined(LIBRA_WAPI_SUPPORT)
        //For not-NULL data frames
        else
        {
#if defined(FEATURE_WLAN_WAPI)
            //If caller doesn't want this frame to be encrypted, for example, WAI packets
            if( (ucTxFlag & WDI_TX_NO_ENCRYPTION_MASK) )
            {
                pBd->dpuNE = WDI_NO_ENCRYPTION_ENABLED;
            }
#endif //defined(FEATURE_WLAN_WAPI)
#ifdef LIBRA_WAPI_SUPPORT
            if (ucTxFlag & WDI_WAPI_STA_MASK)
            {
                pBd->dpuRF = BMUWQ_WAPI_DPU_TX;
            }
#endif //LIBRA_WAPI_SUPPORT
#if defined(WLAN_PERF)
    uTxBdSignature = WDI_ComputeTxBdSignature(pWDICtx, pDestMacAddr, ucTid, ucUnicastDst);
#endif //defined(WLAN_PERF)
        }
#endif        
    }
    else if (ucType == WDI_MAC_MGMT_FRAME)
    {

        /*--------------------------------------------------------------------
         *  Set common fields for mgmt frames
         *     bd_ssn: Always let DPU auto generate seq # from the nonQos
               sequence number counter.
         *     bd_rate:unicast mgmt frames will go at lower rate (multicast rate).
         *                  multicast mgmt frames will go at the STA rate as in AP mode
         *                  buffering has an issue at HW if BD rate is used.
         *     rmf:    NOT SET here. would be set later after STA id lookup is done.
         * Sanity: Force HW frame translation OFF for mgmt frames.
         --------------------------------------------------------------------*/
         /* apply to both ucast/mcast mgmt frames */
         /* Probe requests are sent using BD rate */
         if( ucSubType ==  WDI_MAC_MGMT_PROBE_REQ )
         {
             pBd->bdRate = WDI_BDRATE_BCMGMT_FRAME;
         }
         else
         {
             if (useStaRateForBcastFrames)
             {
                 pBd->bdRate = (ucUnicastDst)? WDI_BDRATE_BCMGMT_FRAME : WDI_TXBD_BDRATE_DEFAULT;
             }
             else
             {
                 pBd->bdRate = WDI_BDRATE_BCMGMT_FRAME;
             }
         }
         if ( ucTxFlag & WDI_USE_BD_RATE2_FOR_MANAGEMENT_FRAME)
         {
           pBd->bdRate = WDI_BDRATE_CTRL_FRAME;
         }

         pBd->bd_ssn = WDI_TXBD_BD_SSN_FILL_DPU_NON_QOS;
         if((ucSubType == WDI_MAC_MGMT_ACTION) || (ucSubType == WDI_MAC_MGMT_DEAUTH) || 
            (ucSubType == WDI_MAC_MGMT_DISASSOC))
            ucIsRMF = 1;
         ucDisableFrmXtl = 1;
    } 
    else 
    {   // Control Packet
        /* We should never get a control packet, asserting here since something
        is wrong */
        WDI_ASSERT(0);
    }

    pBd->ub = !ucUnicastDst;

    /* Fast path: Leverage UMA for BD filling/frame translation.
     * Must be a data frame to request for FT.
     * When HW frame translation is enabled, UMA fills in the following fields:
     *   DPU Sig 
     *   DPU descriptor index
     *   Updates MPDU header offset, data offset, MPDU length after translation
     *   STA id
     *   BTQM Queue ID
     */

    pBd->ft = pWDICtx->bFrameTransEnabled & !ucDisableFrmXtl;

    if( !pBd->ft)
    {
        /* - Slow path: Frame translation is disabled. Need to set the
        following fields:
         *    STA id
         *    DPU Sig 
         *    DPU descriptor index
         *    BTQM Queue ID
         * - For mgmt frames, also update rmf bits
         */
    
        WDI_StaStruct*  pSta = (WDI_StaStruct*) pWDICtx->staTable;
        wpt_uint8       ucStaId;

        /* Disable frame translation*/
        pBd->ft = 0;
#ifdef WLAN_PERF
        /* Mark the BD could not be reused */
        uTxBdSignature = WDI_TXBD_SIG_MGMT_MAGIC; 
#endif
        if((ucTxFlag & WDI_USE_SELF_STA_REQUESTED_MASK) &&
            !(ucIsRMF && ucProtMgmtFrame))
        {
#ifdef HAL_SELF_STA_PER_BSS
            // Get the (self) station index from ADDR2, which should be the self MAC addr
           wdiStatus = WDI_STATableFindStaidByAddr( pWDICtx, 
                                              *(wpt_macAddr*)pAddr2, &ucStaId );
           if (WDI_STATUS_SUCCESS != wdiStatus) 
           {
                WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR, "WDI_STATableFindStaidByAddr failed");
                WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_ERROR, "STA ID = %d " MAC_ADDRESS_STR,
                                        ucStaId, MAC_ADDR_ARRAY(*(wpt_macAddr*)pAddr2));
                return WDI_STATUS_E_NOT_ALLOWED;
           }
#else
           ucStaId = pWDICtx->ucSelfStaId;
#endif
        }
        else
        {
            /*
               _____________________________________________________________________________________________
               |    |       |                 Data                  ||                Mgmt                   |
               |____|_______|_______________________________________||_______________________________________|
               |    | Mode  | DestAddr          | Addr2 (selfMac)   || DestAddr          | Addr2 (selfMac)   |
               |____|_______|___________________|___________________||___________________|___________________|
               |    |       |                   |                   ||                   |                   |
               |    | STA   | DestAddr->staIdx  | When DestAddr     || DestAddr->staIdx  | -                 |
               |    |       |                   | lookup fails,     ||                   |                   |
               |    |       |                   | Addr2->staIdx     ||                   |                   |
               |U/C | IBSS  | DestAddr->staIdx  |        -          || DestAddr->staIdx  | -                 |
               |    | SoftAP| DestAddr->staIdx  |        -          || DestAddr->staIdx  | When DestAddr     |
               |    |       |                   |                   ||                   | lookup fails,     |
               |    |       |                   |                   ||                   | Addr2->StaIdx     |
               |    | Idle  |     N/A           |        N/A        ||         -         | Addr2->StaIdx     |
               |____|_______|___________________|___________________||___________________|___________________|
               |    |       |                   |                   ||                   |                   |
               |    | STA   |     N/A           |        N/A        ||         -         | Addr2->staIdx->   |
               |    |       |                   |                   ||                   | bssIdx->bcasStaIdx|
               |B/C | IBSS  |     -             | Addr2->staIdx->   ||         -         | Addr2->staIdx->   |
               |    |       |                   | bssIdx->bcasStaIdx||                   | bssIdx->bcasStaIdx|
               |    | SoftAP|     -             | Addr2->staIdx->   ||         -         | Addr2->staIdx->   |
               |    |       |                   | bssIdx->bcasStaIdx||                   | bssIdx->bcasStaIdx|
               |    | Idle  |     N/A           |        N/A        ||         -         | Addr2->staIdx->   |
               |    |       |                   |                   ||                   | bssIdx->bcasStaIdx|
               |____|_______|___________________|___________________||___________________|___________________|*/
            // Get the station index based on the above table
           if( ucUnicastDst ) 
           {
             wdiStatus = WDI_STATableFindStaidByAddr( pWDICtx, 
                 *(wpt_macAddr*)pDestMacAddr, &ucStaId ); 
             // In STA mode the unicast data frame could be 
             // transmitted to a DestAddr for which there might not be an entry in 
             // HAL STA table and the lookup would fail. In such cases use the Addr2 
             // (self MAC address) to get the selfStaIdx.
             // From SelfStaIdx, get BSSIdx and use BSS MacAddr to get the staIdx 
             // corresponding to peerSta(AP).
             // Drop frames only it is a data frame. Management frames can still
             // go out using selfStaIdx.


             if (WDI_STATUS_SUCCESS != wdiStatus) 
             {
               if(ucType == WDI_MAC_MGMT_FRAME)
               {
                 //For management frames, use self staIdx if peer sta 
                 //entry is not found.
                 wdiStatus = WDI_STATableFindStaidByAddr( pWDICtx, 
                     *(wpt_macAddr*)pAddr2, &ucStaId ); 
               }
               else
               {
                 if( !ucDisableFrmXtl )
                 {
                   // FrameTranslation in HW is enanled. This means, 
                   // pDestMacaddress may be unknown. Get the station index 
                   // for ADDR2, which should be the self MAC addr
                   wdiStatus = WDI_STATableFindStaidByAddr( pWDICtx, 
                       *(wpt_macAddr*)pAddr2, &ucStaId ); 
                   if (WDI_STATUS_SUCCESS == wdiStatus)
                   {
                     //Found self Sta index.
                     WDI_StaStruct* pSTATable = (WDI_StaStruct*) pWDICtx->staTable;
                     wpt_uint8                 bssIdx  = 0;

                     pBSSSes = NULL;
                     //Initialize WDI status to error.
                     wdiStatus = WDI_STATUS_E_NOT_ALLOWED;

                     //Check if its BSSIdx is valid.
                     if (pSTATable[ucStaId].bssIdx != WDI_BSS_INVALID_IDX) 
                     {
                       //Use BSSIdx to get the association sequence and use
                       //macBssId to get the peerMac Address(MacBSSID).
                       bssIdx = WDI_FindAssocSessionByBSSIdx( pWDICtx,
                           pSTATable[ucStaId].bssIdx,
                           &pBSSSes);

                       if ( NULL != pBSSSes )
                       {
                         //Get staId from the peerMac. 
                         wdiStatus = WDI_STATableFindStaidByAddr( pWDICtx, 
                             pBSSSes->macBSSID, &ucStaId ); 
                       }
                     }
                   }
                 }
               }
               //wdiStatus will be success if it found valid peerStaIdx
               //Otherwise return failure.
               if(WDI_STATUS_SUCCESS != wdiStatus )
               {
                 return WDI_STATUS_E_NOT_ALLOWED;
               }
             }
           } 
            else
            {
              // For bcast frames use the bcast station index
              wpt_uint8 bssSessIdx;

              // Get the station index for ADDR2, which should be the self MAC addr
              wdiStatus = WDI_STATableFindStaidByAddr( pWDICtx, 
                                              *(wpt_macAddr*)pAddr2, &ucStaId ); 
              if (WDI_STATUS_SUCCESS != wdiStatus)
              {
                return WDI_STATUS_E_NOT_ALLOWED;
              }

              // Get the Bss Index related to the staId
              bssSessIdx = pSta[ucStaId].bssIdx;

              // Get the broadcast station index for this bss
              (void) WDI_FindAssocSessionByBSSIdx( pWDICtx, bssSessIdx, 
                                                   &pBSSSes ); 
              if (NULL == pBSSSes)
              {
                // session not found ?!?
                return WDI_STATUS_E_FAILURE;
              }
              ucStaId = pBSSSes->bcastStaIdx;
           }
         }

        WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO,"StaId:%d and ucTxFlag:%02x", ucStaId, ucTxFlag);

        pBd->staIndex = ucStaId;
        
        *staIndex = ucStaId;

        pSta += ucStaId;  // Go to the curresponding station's station table

        if(ucType == WDI_MAC_MGMT_FRAME)
        {
            if (ucUnicastDst) 
            {
                /* If no ack is requested use the bcast queue */
                if (ucTxFlag & WDI_USE_NO_ACK_REQUESTED_MASK) 
                {
                    pBd->queueId = BTQM_QUEUE_SELF_STA_BCAST_MGMT;
                }
                else
                {
                    /* Assigning Queue Id configured to Ack */ 
                    pBd->queueId = BTQM_QUEUE_SELF_STA_UCAST_MGMT;
                }
            } 
            else 
            {
                /* Assigning to Queue Id configured to No Ack */ 
                pBd->queueId = BTQM_QUEUE_SELF_STA_BCAST_MGMT;
            }

            if(ucIsRMF && pSta->rmfEnabled)
            {
                pBd->dpuNE = !ucProtMgmtFrame;
                pBd->rmf = 1;
                if(!ucUnicastDst)
                    pBd->dpuDescIdx = pSta->bcastMgmtDpuIndex; /* IGTK */
                else
                    pBd->dpuDescIdx = pSta->dpuIndex; /* PTK */
            }
            else
            {
                pBd->dpuNE = WDI_NO_ENCRYPTION_ENABLED;  
                pBd->rmf = 0;
                pBd->dpuDescIdx = pSta->dpuIndex; /* PTK for both u/mcast mgmt frames */
            }
        }
        else
        {
            /* data frames */
            /* TID->QID is one-to-one mapping, the same way as followed in H/W */
            wpt_uint8 queueId = 0;

   
            WDI_STATableGetStaType(pWDICtx, ucStaId, &ucSTAType);
            if(!ucUnicastDst)
                pBd->queueId = BTQM_QID0;
#ifndef HAL_SELF_STA_PER_BSS
            else if( ucUnicastDst && (ucStaId == pWDICtx->ucSelfStaId))
                pBd->queueId = BTQM_QUEUE_SELF_STA_UCAST_DATA;
#else
            else if( ucUnicastDst && (ucSTAType == WDI_STA_ENTRY_SELF))
                pBd->queueId = BTQM_QUEUE_SELF_STA_UCAST_DATA;
#endif
            else if (pSta->qosEnabled) 
            {
                WDI_BmuGetQidForQOSTid( ucTid, &queueId); 
                pBd->queueId = (wpt_uint32) queueId;
            }
            else
                pBd->queueId = BTQM_QUEUE_TX_nQOS;

            if(ucUnicastDst)
            {
                pBd->dpuDescIdx = pSta->dpuIndex; /*unicast data frames: PTK*/
            }
            else
            {
                pBd->dpuDescIdx = pSta->bcastDpuIndex; /* mcast data frames: GTK*/
            }
        }

        pBd->dpuSignature = pSta->dpuSig;

        /* ! Re-analize this assumption
        - original code from HAL is below - however WDI does not have access to a
        DPU index table - so it just stores the signature that it receives from HAL upon
        post assoc 
        if(eHAL_STATUS_SUCCESS == halDpu_GetSignature(pMac, pSta->dpuIndex, &ucDpuSig))
            pBd->dpuSignature = ucDpuSig;
        else{   
            WPAL_TRACE( WPT_WDI_CONTROL_MODULE, WPT_MSG_LEVEL_HIGH, "halDpu_GetSignature() failed for dpuId = %d\n", pBd->dpuDescIdx));
            return VOS_STATUS_E_FAILURE;
        } */
#ifdef WLAN_SOFTAP_VSTA_FEATURE
       // if this is a Virtual Station or statype is TDLS and trig enabled mask
       // set then change the DPU Routing Flag so
       // that the frame will be routed to Firmware for queuing & transmit
       if (IS_VSTA_IDX(ucStaId) ||
                 (
#ifdef FEATURE_WLAN_TDLS
                  (ucSTAType == WDI_STA_ENTRY_TDLS_PEER ) &&
#endif
                  (ucTxFlag & WDI_TRIGGER_ENABLED_AC_MASK)) || isEapol)
       {
          WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_INFO,
          "Sending EAPOL pakcet over WQ5 MAC S: %08x. MAC D: %08x.",
                    *((wpt_uint32 *) pAddr2),
                   *((wpt_uint32 *) pDestMacAddr));
           pBd->dpuRF = BMUWQ_FW_DPU_TX;
       }
#endif

    }

    /*------------------------------------------------------------------------
       Over SDIO bus, SIF won't swap data bytes to/from data FIFO. 
       In order for MAC modules to recognize BD in Riva's default endian
       format (Big endian)
     * All BD fields need to be swaped here
     ------------------------------------------------------------------------*/
    WDI_SwapTxBd((wpt_uint8 *)pBd); 

#ifdef WLAN_PERF
    /* Save the BD signature. This field won't be swapped and remains in host
       byte order */
    pBd->txBdSignature = uTxBdSignature ;
#endif        

    return wdiStatus;
}/*WDI_FillTxBd*/


/**
 @brief WDI_RxBD_GetFrameTypeSubType - Called by the data path 
        to retrieve the type/subtype of the received frame.
  
 @param       pvBDHeader:    Void pointer to the RxBD buffer.
    usFrmCtrl:     the frame ctrl of the 802.11 header 
  
 @return   A byte which contains both type and subtype info. LSB four bytes 
 (b0 to b3)is subtype and b5-b6 is type info. 
*/

wpt_uint8 
WDI_RxBD_GetFrameTypeSubType
(
    void* _pvBDHeader, 
    wpt_uint16 usFrmCtrl
)
{
    WDI_RxBdType*    pRxBd = (WDI_RxBdType*) _pvBDHeader;
    wpt_uint8        typeSubType;
    WDI_MacFrameCtl  wdiFrmCtl; 
    
    if (pRxBd->ft != WDI_RX_BD_FT_DONE)
    {
        if (pRxBd->asf)
        {
            typeSubType = (WDI_MAC_DATA_FRAME << WDI_FRAME_TYPE_OFFSET) |
                                                       WDI_MAC_DATA_QOS_DATA;
        } else {
           wpalMemoryCopy(&wdiFrmCtl, &usFrmCtrl, sizeof(wdiFrmCtl)); 
           typeSubType = (wdiFrmCtl.type << WDI_FRAME_TYPE_OFFSET) |
                                                       wdiFrmCtl.subType;
        }
    }
    else
    {
        wpalMemoryCopy(&wdiFrmCtl, &usFrmCtrl, sizeof(wdiFrmCtl)); 
        typeSubType = (wdiFrmCtl.type << WDI_FRAME_TYPE_OFFSET) |
                        wdiFrmCtl.subType;
    }
    
    return typeSubType;
}/*WDI_RxBD_GetFrameTypeSubType*/

/**
 @brief WDI_SwapRxBd swaps the RX BD.

  
 @param pBd - pointer to the BD (in/out)
  
 @return None
*/
void 
WDI_SwapRxBd(wpt_uint8 *pBd)
{
#ifndef WDI_BIG_BYTE_ENDIAN
    WDI_SwapBytes(pBd , WDI_RX_BD_HEADER_SIZE);
#endif
}/*WDI_SwapRxBd*/


/**
 @brief WDI_SwapTxBd - Swaps the TX BD
  
 @param  pBd - pointer to the BD (in/out)
  
 @return   none
*/
void 
WDI_SwapTxBd(wpt_uint8 *pBd)
{
#ifndef WDI_BIG_BYTE_ENDIAN
    WDI_SwapBytes(pBd , WDI_TX_BD_HEADER_SIZE);
#endif
}/*WDI_SwapTxBd*/

/*! TO DO:  - check if we still need this for RIVA*/
/**
 @brief WDI_RxAmsduBdFix - fix for HW issue for AMSDU 


 @param   pWDICtx:       Context to the WDI
          pBDHeader - pointer to the BD header

 @return None
*/
void 
WDI_RxAmsduBdFix
(
    WDI_ControlBlockType*  pWDICtx,
    void*                 _pvBDHeader
)
{
    WDI_RxBdType*          pRxBd   = (WDI_RxBdType*) _pvBDHeader;
    wpt_uint32 *pModBd, *pMaskBd, *pFirstBd, i;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

    if (pRxBd->asf)
    {
        if (pRxBd->sybFrameIdx == 0)
        {
            //copy the BD of first AMSDU
            pWDICtx->wdiRxAmsduFirstBdCache = *pRxBd;
        }
        else
        {
            pModBd   = (wpt_uint32*)pRxBd;
            pMaskBd  = (wpt_uint32*)&pWDICtx->wdiRxAmsduBdFixMask;
            pFirstBd = (wpt_uint32*)&pWDICtx->wdiRxAmsduFirstBdCache;

            for (i = 0; i < sizeof(WDI_RxBdType)/sizeof(wpt_uint32 *); i++)
            {
                //modified BD = zero out non AMSDU related fields in this BD |
                //              non AMSDU related fields from the first BD.
                pModBd[i] = (pModBd[i] & ~pMaskBd[i]) |
                            (pFirstBd[i] & pMaskBd[i]);
            }
        }
    }
    return;
}/*WDI_RxAmsduBdFix*/

