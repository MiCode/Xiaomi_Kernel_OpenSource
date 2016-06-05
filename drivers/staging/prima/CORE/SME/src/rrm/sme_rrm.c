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

/**=========================================================================
  
  \file  sme_Rrm.c
  
  \brief implementation for SME RRM APIs
  
  
  ========================================================================*/

/* $Header$ */

#if defined WLAN_FEATURE_VOWIFI
/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include "aniGlobal.h"
#include "smeInside.h"
#include "sme_Api.h"
#include "smsDebug.h"
#include "cfgApi.h"

#ifdef FEATURE_WLAN_DIAG_SUPPORT
#include "vos_diag_core_event.h"
#include "vos_diag_core_log.h"
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

#include "csrInsideApi.h"

#include "rrmGlobal.h"

#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
#include "csrEse.h"
#endif

/* Roam score for a neighbor AP will be calculated based on the below definitions.
    The calculated roam score will be used to select the roamable candidate from neighbor AP list */
#define RRM_ROAM_SCORE_NEIGHBOR_REPORT_REACHABILITY             0   /* When we support 11r over the DS, this should have a non-zero value */
#define RRM_ROAM_SCORE_NEIGHBOR_REPORT_SECURITY                 10
#define RRM_ROAM_SCORE_NEIGHBOR_REPORT_KEY_SCOPE                20
#define RRM_ROAM_SCORE_NEIGHBOR_REPORT_CAPABILITY_SPECTRUM_MGMT 0   /* Not used */
#define RRM_ROAM_SCORE_NEIGHBOR_REPORT_CAPABILITY_QOS           5
#define RRM_ROAM_SCORE_NEIGHBOR_REPORT_CAPABILITY_APSD          3
#define RRM_ROAM_SCORE_NEIGHBOR_REPORT_CAPABILITY_RRM           8
#define RRM_ROAM_SCORE_NEIGHBOR_REPORT_CAPABILITY_DELAYED_BA    0   /* We dont support delayed BA */
#define RRM_ROAM_SCORE_NEIGHBOR_REPORT_CAPABILITY_IMMEDIATE_BA  3
#define RRM_ROAM_SCORE_NEIGHBOR_REPORT_MOBILITY_DOMAIN          30

#ifdef FEATURE_WLAN_ESE
#define RRM_ROAM_SCORE_NEIGHBOR_IAPP_LIST                       30
#endif
/**---------------------------------------------------------------------------
  
  \brief rrmLLPurgeNeighborCache() - 
    This function purges all the entries in the neighbor cache and frees up all the internal nodes   

  \param  - pMac  - Pointer to the Hal Handle.
          - pList - Pointer the List that should be purged.
  \return - void
  
  --------------------------------------------------------------------------*/
static void rrmLLPurgeNeighborCache(tpAniSirGlobal pMac, tDblLinkList *pList)
{
    tListElem *pEntry;
    tRrmNeighborReportDesc *pNeighborReportDesc;
    
    csrLLLock(pList);
    
    while((pEntry = csrLLRemoveHead(pList, LL_ACCESS_NOLOCK)) != NULL)
    {
        pNeighborReportDesc = GET_BASE_ADDR( pEntry, tRrmNeighborReportDesc, List );
        vos_mem_free(pNeighborReportDesc->pNeighborBssDescription);
        vos_mem_free(pNeighborReportDesc);
    }
    
    csrLLUnlock(pList);   
     
    return;
}

/**---------------------------------------------------------------------------
  
  \brief rrmIndicateNeighborReportResult() - 
        This function calls the callback register by the caller while requesting for 
        neighbor report. This function gets invoked if a neighbor report is received from an AP
        or neighbor response wait timer expires.

  \param  - pMac - Pointer to the Hal Handle.
          - vosStatus - VOS_STATUS_SUCCESS/VOS_STATUS_FAILURE based on whether a valid report is 
            received or neighbor timer expired
  \return - void
  
  --------------------------------------------------------------------------*/
void rrmIndicateNeighborReportResult(tpAniSirGlobal pMac, VOS_STATUS vosStatus)
{
    NeighborReportRspCallback callback;
    void                      *callbackContext;

    /* Reset the neighbor response pending status */
    pMac->rrm.rrmSmeContext.neighborReqControlInfo.isNeighborRspPending = eANI_BOOLEAN_FALSE;

    /* Stop the timer if it is already running. The timer should be running only in the SUCCESS case. */
    if (VOS_TIMER_STATE_RUNNING == vos_timer_getCurrentState(&pMac->rrm.rrmSmeContext.neighborReqControlInfo.neighborRspWaitTimer))
    {
        smsLog( pMac, LOG1, FL("No entry in neighbor report cache"));
        vos_timer_stop(&pMac->rrm.rrmSmeContext.neighborReqControlInfo.neighborRspWaitTimer);
    }
    callback = pMac->rrm.rrmSmeContext.neighborReqControlInfo.neighborRspCallbackInfo.neighborRspCallback;
    callbackContext = pMac->rrm.rrmSmeContext.neighborReqControlInfo.neighborRspCallbackInfo.neighborRspCallbackContext;
    
    /* Reset the callback and the callback context before calling the callback. It is very likely that there may be a registration in 
            callback itself. */
    pMac->rrm.rrmSmeContext.neighborReqControlInfo.neighborRspCallbackInfo.neighborRspCallback = NULL;
    pMac->rrm.rrmSmeContext.neighborReqControlInfo.neighborRspCallbackInfo.neighborRspCallbackContext = NULL;
    
    /* Call the callback with the status received from caller */
    if (callback)
        callback(callbackContext, vosStatus);
#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
    // We came here with IAPP AP List
    // Make sure we inform CSR of the neighbor list
    // for ESE Associations. First clear the cache.
    else
    if (csrNeighborRoamIsESEAssoc(pMac))
    {
        ProcessIAPPNeighborAPList(pMac);
    }
#endif

    return;

}

/**---------------------------------------------------------------------------
  
  \brief sme_RrmBeaconReportXmitInd() - 

   Create and send the beacon report Xmit ind message to PE.

  \param  - pMac - Pointer to the Hal Handle.
              - pResult - scan result.
              - measurementDone - flag to indicate that the measurement is done.        
  \return - 0 for success, non zero for failure
  
  --------------------------------------------------------------------------*/
static eHalStatus sme_RrmSendBeaconReportXmitInd( tpAniSirGlobal pMac,
                                                  tCsrScanResultInfo **pResultArr,
                                                  tANI_U8 measurementDone,
                                                  tANI_U8 bss_count )
{
   tpSirBssDescription pBssDesc = NULL;
   tpSirBeaconReportXmitInd pBeaconRep;
   tANI_U16 length, ie_len;
   tANI_U8 bssCounter=0, msgCounter=0;
   tCsrScanResultInfo *pCurResult=NULL;
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpRrmSMEContext pSmeRrmContext = &pMac->rrm.rrmSmeContext;


#if defined WLAN_VOWIFI_DEBUG
   smsLog( pMac, LOGE, "Beacon report xmit Ind to PE");
#endif

   if( NULL == pResultArr && !measurementDone )
   {
      smsLog( pMac, LOGE, "Beacon report xmit Ind to PE Failed");
      return eHAL_STATUS_FAILURE;
   }

   if (pResultArr)
       pCurResult=pResultArr[bssCounter];

   do 
   {
       length = sizeof(tSirBeaconReportXmitInd);
       pBeaconRep = vos_mem_malloc ( length );
       if ( NULL == pBeaconRep )
       {
          smsLog( pMac, LOGP, "Unable to allocate memory for beacon report");
          return eHAL_STATUS_FAILED_ALLOC;
       }
       vos_mem_zero( pBeaconRep, length );
#if defined WLAN_VOWIFI_DEBUG
       smsLog( pMac, LOGE, FL("Allocated memory for pBeaconRep"));
#endif
       pBeaconRep->messageType = eWNI_SME_BEACON_REPORT_RESP_XMIT_IND;
       pBeaconRep->length = length;
       pBeaconRep->uDialogToken = pSmeRrmContext->token;
       pBeaconRep->duration = pSmeRrmContext->duration[0];
       pBeaconRep->regClass = pSmeRrmContext->regClass;
       vos_mem_copy( pBeaconRep->bssId, pSmeRrmContext->sessionBssId, sizeof(tSirMacAddr) );

       msgCounter=0;
       while (pCurResult) 
       {
           pBssDesc = &pCurResult->BssDescriptor;
           if(pBssDesc != NULL)
           {
               ie_len = GET_IE_LEN_IN_BSS( pBssDesc->length );
               pBeaconRep->pBssDescription[msgCounter] = vos_mem_malloc (
                                            ie_len+sizeof(tSirBssDescription));
               if (NULL == pBeaconRep->pBssDescription[msgCounter])
                   break;
               vos_mem_zero(pBeaconRep->pBssDescription[msgCounter],
                            ie_len+sizeof(tSirBssDescription));
               vos_mem_copy( pBeaconRep->pBssDescription[msgCounter],
                             pBssDesc,
                             sizeof(tSirBssDescription) );
               vos_mem_copy( &pBeaconRep->pBssDescription[msgCounter]->ieFields[0],
                             pBssDesc->ieFields, ie_len  );
               smsLog( pMac, LOG1,
                   "...RRM Result Bssid = "MAC_ADDRESS_STR" chan= %d, rssi = -%d",
                   MAC_ADDR_ARRAY(pBeaconRep->pBssDescription[msgCounter]->bssId),
                   pBeaconRep->pBssDescription[msgCounter]->channelId,
                   pBeaconRep->pBssDescription[msgCounter]->rssi * (-1));

               pBeaconRep->numBssDesc++;

               if (++msgCounter >= SIR_BCN_REPORT_MAX_BSS_DESC)
                   break;

               pCurResult = pResultArr[bssCounter + msgCounter];
           }
           else
           {
               pCurResult = NULL;
               break;
           }
       }

       bssCounter+=msgCounter; 
       if (!pResultArr || (pCurResult == NULL) || (bssCounter >= bss_count))
       {
           pCurResult = NULL;
           smsLog(pMac, LOG1,
                  "Reached to the max/last BSS in pCurResult list");
       }
       else
       {
           pCurResult = pResultArr[bssCounter];
           smsLog(pMac, LOG1,
                  "Move to the next BSS set in pCurResult list");
       }

       pBeaconRep->fMeasureDone = (pCurResult)?false:measurementDone;

       smsLog(pMac, LOG1,
              "SME Sending BcnRepXmit to PE numBss %d msgCounter %d bssCounter %d",
              pBeaconRep->numBssDesc, msgCounter, bssCounter);

       status = palSendMBMessage(pMac->hHdd, pBeaconRep);

   } while (pCurResult);

   return status;
}

#if defined(FEATURE_WLAN_ESE_UPLOAD)
/**---------------------------------------------------------------------------

  \brief sme_EseSendBeaconReqScanResults()

   This function sends up the scan results received as a part of
   beacon request scanning.
   This function is called after receiving the scan results per channel
   Due to the limitation on the size of the IWEVCUSTOM buffer, we send 3 BSSIDs of
   beacon report information in one custom event;

  \param  - pMac -      Pointer to the Hal Handle.
              - sessionId  - Session id
              - channel     - scan results belongs to this channel
              - pResultArr - scan result.
              - measurementDone - flag to indicate that the measurement is done.
              - bss_count - number of bss found
  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/
static eHalStatus sme_EseSendBeaconReqScanResults(tpAniSirGlobal pMac,
                                                  tANI_U32       sessionId,
                                                  tANI_U8        channel,
                                                  tCsrScanResultInfo **pResultArr,
                                                  tANI_U8        measurementDone,
                                                  tANI_U8        bss_count)
{
   eHalStatus              status         = eHAL_STATUS_FAILURE;
   tSirRetStatus           fillIeStatus;
   tpSirBssDescription     pBssDesc       = NULL;
   tANI_U32                ie_len         = 0;
   tANI_U32                outIeLen       = 0;
   tANI_U8                 bssCounter     = 0;
   tCsrScanResultInfo     *pCurResult     = NULL;
   tANI_U8                 msgCounter     = 0;
   tpRrmSMEContext         pSmeRrmContext = &pMac->rrm.rrmSmeContext;
   tCsrRoamInfo            roamInfo;
   tSirEseBcnReportRsp     bcnReport;
   tpSirEseBcnReportRsp    pBcnReport     = &bcnReport;
   tpCsrEseBeaconReqParams pCurMeasReqIe  = NULL;
   tANI_U8                 i              = 0;

   if (NULL == pSmeRrmContext)
   {
       smsLog( pMac, LOGE, "pSmeRrmContext is NULL");
       return eHAL_STATUS_FAILURE;
   }

   if (NULL == pResultArr && !measurementDone)
   {
      smsLog( pMac, LOGE, "Beacon report xmit Ind to HDD Failed");
      return eHAL_STATUS_FAILURE;
   }

   if (pResultArr)
       pCurResult=pResultArr[bssCounter];

   vos_mem_zero(&bcnReport, sizeof(tSirEseBcnReportRsp));
   do
   {
       pCurMeasReqIe = NULL;
       for (i = 0; i < pSmeRrmContext->eseBcnReqInfo.numBcnReqIe; i++)
       {
           if(pSmeRrmContext->eseBcnReqInfo.bcnReq[i].channel == channel)
           {
               pCurMeasReqIe = &pSmeRrmContext->eseBcnReqInfo.bcnReq[i];
               break;
           }
       }
       if(NULL != pCurMeasReqIe)
           pBcnReport->measurementToken = pCurMeasReqIe->measurementToken;
       smsLog( pMac, LOG1, "Channel(%d) MeasToken(%d)", channel, pBcnReport->measurementToken);

       msgCounter=0;
       while (pCurResult)
       {
           pBssDesc = &pCurResult->BssDescriptor;
           if (NULL != pBssDesc)
           {
               ie_len = GET_IE_LEN_IN_BSS( pBssDesc->length );
               pBcnReport->bcnRepBssInfo[msgCounter].bcnReportFields.ChanNum = pBssDesc->channelId;
               pBcnReport->bcnRepBssInfo[msgCounter].bcnReportFields.Spare = 0;
               if(NULL != pCurMeasReqIe)
                   pBcnReport->bcnRepBssInfo[msgCounter].bcnReportFields.MeasDuration = pCurMeasReqIe->measurementDuration;
               pBcnReport->bcnRepBssInfo[msgCounter].bcnReportFields.PhyType = pBssDesc->nwType;
               pBcnReport->bcnRepBssInfo[msgCounter].bcnReportFields.RecvSigPower = pBssDesc->rssi;
               pBcnReport->bcnRepBssInfo[msgCounter].bcnReportFields.ParentTsf = pBssDesc->parentTSF;
               pBcnReport->bcnRepBssInfo[msgCounter].bcnReportFields.TargetTsf[0] = pBssDesc->timeStamp[0];
               pBcnReport->bcnRepBssInfo[msgCounter].bcnReportFields.TargetTsf[1] = pBssDesc->timeStamp[1];
               pBcnReport->bcnRepBssInfo[msgCounter].bcnReportFields.BcnInterval = pBssDesc->beaconInterval;
               pBcnReport->bcnRepBssInfo[msgCounter].bcnReportFields.CapabilityInfo = pBssDesc->capabilityInfo;
               vos_mem_copy(pBcnReport->bcnRepBssInfo[msgCounter].bcnReportFields.Bssid,
                                      pBssDesc->bssId, sizeof(tSirMacAddr));

               fillIeStatus = sirFillBeaconMandatoryIEforEseBcnReport(pMac,
                                                                      (tANI_U8 *)pBssDesc->ieFields,
                                                                      ie_len,
                                                                      &(pBcnReport->bcnRepBssInfo[msgCounter].pBuf),
                                                                      &outIeLen);
               if (eSIR_FAILURE == fillIeStatus)
               {
                  continue;
               }
               pBcnReport->bcnRepBssInfo[msgCounter].ieLen = outIeLen;

               smsLog( pMac, LOG1,"Bssid("MAC_ADDRESS_STR") Channel=%d Rssi=%d",
                       MAC_ADDR_ARRAY(pBssDesc->bssId),
                       pBssDesc->channelId, (-1) * pBssDesc->rssi);

               pBcnReport->numBss++;

               if (++msgCounter >= SIR_BCN_REPORT_MAX_BSS_DESC)
                   break;

               pCurResult = pResultArr[msgCounter];
           }
           else
           {
               pCurResult = NULL;
               break;
           }
       }

       bssCounter += msgCounter;
       if (!pResultArr || !pCurResult || (bssCounter >= SIR_BCN_REPORT_MAX_BSS_DESC))
       {
           pCurResult = NULL;
           smsLog(pMac, LOGE,
                  "Reached to the max/last BSS in pCurResult list");
       }
       else
       {
           pCurResult = pResultArr[bssCounter];
           smsLog(pMac, LOGE,
                  "Move to the next BSS set in pCurResult list");
       }

       pBcnReport->flag = (measurementDone << 1)|((pCurResult)?true:false);

       smsLog(pMac, LOG1, "SME Sending BcnRep to HDD numBss(%d)"
               " msgCounter(%d) bssCounter(%d) flag(%d)",
                pBcnReport->numBss, msgCounter, bssCounter, pBcnReport->flag);

       roamInfo.pEseBcnReportRsp = pBcnReport;
       status = csrRoamCallCallback(pMac, sessionId, &roamInfo,
                           0, eCSR_ROAM_ESE_BCN_REPORT_IND, 0);

       /* Free the memory allocated to IE */
       for (i = 0; i < msgCounter; i++)
       {
           if (pBcnReport->bcnRepBssInfo[i].pBuf)
               vos_mem_free(pBcnReport->bcnRepBssInfo[i].pBuf);
       }
   } while (pCurResult);
   return status;
}

#endif /* FEATURE_WLAN_ESE_UPLOAD */

/**---------------------------------------------------------------------------
  
  \brief sme_RrmSendScanRequest() - 

   This function is called to get the scan result from CSR and send the beacon report
   xmit ind message to PE.

  \param  - pMac - Pointer to the Hal Handle.
              - num_chan - number of channels.
              - channel list - list of channels to fetch the result from.
              - measurementDone - flag to indicate that the measurement is done.        
  \return - 0 for success, non zero for failure
  
  --------------------------------------------------------------------------*/
static eHalStatus sme_RrmSendScanResult( tpAniSirGlobal pMac,
                                         tANI_U8 num_chan,
                                         tANI_U8* chanList,
                                         tANI_U8 measurementDone )
{
   tCsrScanResultFilter filter;
   tScanResultHandle pResult;
   tCsrScanResultInfo *pScanResult, *pNextResult;
   tCsrScanResultInfo *pScanResultsArr[SIR_BCN_REPORT_MAX_BSS_DESC];
   eHalStatus status;
   tANI_U8 counter=0;
   tpRrmSMEContext pSmeRrmContext = &pMac->rrm.rrmSmeContext;
   tANI_U32 sessionId;

#if defined WLAN_VOWIFI_DEBUG
   smsLog( pMac, LOGE, "Send scan result to PE ");
#endif

   vos_mem_zero( &filter, sizeof(filter) );
   vos_mem_zero( pScanResultsArr, sizeof(pNextResult)*SIR_BCN_REPORT_MAX_BSS_DESC );

   filter.BSSIDs.numOfBSSIDs = 1;
   filter.BSSIDs.bssid = &pSmeRrmContext->bssId;

   if( pSmeRrmContext->ssId.length )
   {
      filter.SSIDs.SSIDList =( tCsrSSIDInfo *)vos_mem_malloc(sizeof(tCsrSSIDInfo));
      if( filter.SSIDs.SSIDList == NULL )
      {
         smsLog( pMac, LOGP, FL("vos_mem_malloc failed:") );
         return eHAL_STATUS_FAILURE;
      }
#if defined WLAN_VOWIFI_DEBUG
      smsLog( pMac, LOGE, FL("Allocated memory for SSIDList"));
#endif
      vos_mem_zero( filter.SSIDs.SSIDList, sizeof(tCsrSSIDInfo) );

      filter.SSIDs.SSIDList->SSID.length = pSmeRrmContext->ssId.length;
      vos_mem_copy(filter.SSIDs.SSIDList->SSID.ssId, pSmeRrmContext->ssId.ssId, pSmeRrmContext->ssId.length);
      filter.SSIDs.numOfSSIDs = 1;
   }
   else
   {
      filter.SSIDs.numOfSSIDs = 0;
   }

   filter.ChannelInfo.numOfChannels = num_chan;
   filter.ChannelInfo.ChannelList = chanList; 

   filter.fMeasurement = TRUE; 

   csrRoamGetSessionIdFromBSSID( pMac, (tCsrBssid*)pSmeRrmContext->sessionBssId, &sessionId );
   status = sme_ScanGetResult(pMac, (tANI_U8)sessionId, &filter, &pResult);

   if( filter.SSIDs.SSIDList )
   {
      //Free the memory allocated for SSIDList.
      vos_mem_free( filter.SSIDs.SSIDList );
#if defined WLAN_VOWIFI_DEBUG
      smsLog( pMac, LOGE, FL("Free memory for SSIDList") );
#endif
   }

   if (NULL == pResult)
   {
      // no scan results
      //
      // Spec. doesnt say anything about such condition. 
      // Since section 7.4.6.2 (IEEE802.11k-2008) says-rrm report frame should contain
      // one or more report IEs. It probably means dont send any respose if no matching
      // BSS found. Moreover, there is no flag or field in measurement report IE(7.3.2.22)
      // OR beacon report IE(7.3.2.22.6) that can be set to indicate no BSS found on a given channel.
      //
      // If we finished measurement on all the channels, we still need to
      // send a xmit indication with moreToFollow set to MEASURMENT_DONE
      // so that PE can clean any context allocated.
      if( measurementDone )
      {
#if defined(FEATURE_WLAN_ESE_UPLOAD)
         if (eRRM_MSG_SOURCE_ESE_UPLOAD == pSmeRrmContext->msgSource)
         {
             status = sme_EseSendBeaconReqScanResults(pMac,
                                                  sessionId,
                                                  chanList[0],
                                                  NULL,
                                                  measurementDone,
                                                  0);
         }
         else
#endif /*FEATURE_WLAN_ESE_UPLOAD*/
             status = sme_RrmSendBeaconReportXmitInd( pMac, NULL, measurementDone, 0);
      }
      return status;
   }

   pScanResult = sme_ScanResultGetFirst(pMac, pResult);

   if( NULL == pScanResult && measurementDone )
   {
#if defined(FEATURE_WLAN_ESE_UPLOAD)
       if (eRRM_MSG_SOURCE_ESE_UPLOAD == pSmeRrmContext->msgSource)
       {
           status = sme_EseSendBeaconReqScanResults(pMac,
                                                 sessionId,
                                                 chanList[0],
                                                 NULL,
                                                 measurementDone,
                                                 0);
       }
       else
#endif /*FEATURE_WLAN_ESE_UPLOAD*/
           status = sme_RrmSendBeaconReportXmitInd( pMac, NULL, measurementDone, 0 );
   }

   counter=0;
   while (pScanResult)
   {
      pNextResult = sme_ScanResultGetNext(pMac, pResult);
      pScanResultsArr[counter++] = pScanResult;
      pScanResult = pNextResult; //sme_ScanResultGetNext(hHal, pResult);
      if (counter >= SIR_BCN_REPORT_MAX_BSS_DESC)
         break;
      }

   if (counter)
   {
          smsLog(pMac, LOG1, " Number of BSS Desc with RRM Scan %d ", counter);
#if defined(FEATURE_WLAN_ESE_UPLOAD)
         if (eRRM_MSG_SOURCE_ESE_UPLOAD == pSmeRrmContext->msgSource)
         {
             status = sme_EseSendBeaconReqScanResults(pMac,
                                                sessionId,
                                                chanList[0],
                                                pScanResultsArr,
                                                measurementDone,
                                                counter);
         }
         else
#endif /*FEATURE_WLAN_ESE_UPLOAD*/
             status = sme_RrmSendBeaconReportXmitInd( pMac,
                                                pScanResultsArr,
                                                measurementDone,
                                                counter);
   }
   sme_ScanResultPurge(pMac, pResult); 

   return status;
}
/**---------------------------------------------------------------------------
  
  \brief sme_RrmScanRequestCallback() - 

   The sme module calls this callback function once it finish the scan request
   and this function send the beacon report xmit to PE and starts a timer of
   random interval to issue next request.

  \param  - halHandle - Pointer to the Hal Handle.
              - pContext - Pointer to the data context.
              - scanId - Scan ID.
              - status - CSR Status.        
  \return - 0 for success, non zero for failure
  
  --------------------------------------------------------------------------*/

static eHalStatus sme_RrmScanRequestCallback(tHalHandle halHandle, void *pContext,
                         tANI_U32 scanId, eCsrScanStatus status)
{

   tANI_U16 interval;
   tpAniSirGlobal pMac = (tpAniSirGlobal) halHandle;
   tpRrmSMEContext pSmeRrmContext = &pMac->rrm.rrmSmeContext;
   tANI_U32 time_tick; 



#if defined WLAN_VOWIFI_DEBUG
   smsLog( pMac, LOGE, "Scan Request callback ");
#endif
   //if any more channels are pending, start a timer of a random value within randomization interval.
   //
   //
   if( (pSmeRrmContext->currentIndex + 1) < pSmeRrmContext->channelList.numOfChannels )
   {
      sme_RrmSendScanResult( pMac, 1, &pSmeRrmContext->channelList.ChannelList[pSmeRrmContext->currentIndex], false );

      pSmeRrmContext->currentIndex++; //Advance the current index.
      //start the timer to issue next request. 
      //From timer tick get a random number within 10ms and max randmization interval.
      time_tick = vos_timer_get_system_ticks();
      interval = time_tick % (pSmeRrmContext->randnIntvl - 10 + 1) + 10;

#if defined WLAN_VOWIFI_DEBUG
      smsLog( pMac, LOGE, "Set timer for interval %d ", interval);
#endif
      vos_timer_start( &pSmeRrmContext->IterMeasTimer, interval );

   }
   else
   {
      //Done with the measurement. Clean up all context and send a message to PE with measurement done flag set.
      sme_RrmSendScanResult( pMac, 1, &pSmeRrmContext->channelList.ChannelList[pSmeRrmContext->currentIndex], true );
      vos_mem_free( pSmeRrmContext->channelList.ChannelList );
#if defined WLAN_VOWIFI_DEBUG
      smsLog( pMac, LOGE, FL("Free memory for ChannelList") );
#endif
   }

   return eHAL_STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
  \brief sme_RrmIssueScanReq() - This is called to send a scan request as part 
         of beacon report request .
  
  \param  pMac  - pMac global pointer
  
  \return eHAL_STATUS_SUCCESS - Validation is successful.
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_RrmIssueScanReq( tpAniSirGlobal pMac )
{
   //Issue scan request.
   tCsrScanRequest scanRequest;
   v_U32_t scanId = 0;
   eHalStatus status = eHAL_STATUS_SUCCESS;
   tpRrmSMEContext pSmeRrmContext = &pMac->rrm.rrmSmeContext;
   tANI_U32 sessionId;
   tSirScanType scanType;

   if ((pSmeRrmContext->currentIndex) >= pSmeRrmContext->channelList.numOfChannels)
       return status;

   if( eRRM_MSG_SOURCE_ESE_UPLOAD == pSmeRrmContext->msgSource ||
       eRRM_MSG_SOURCE_LEGACY_ESE == pSmeRrmContext->msgSource )
       scanType = pSmeRrmContext->measMode[pSmeRrmContext->currentIndex];
   else
       scanType = pSmeRrmContext->measMode[0];

   if ((eSIR_ACTIVE_SCAN == scanType) || (eSIR_PASSIVE_SCAN == scanType))
   {
#if defined WLAN_VOWIFI_DEBUG
   smsLog( pMac, LOGE, "Issue scan request " );
#endif

       vos_mem_zero( &scanRequest, sizeof(scanRequest));

       /* set scanType, active or passive */
       scanRequest.scanType = scanType;

       vos_mem_copy(scanRequest.bssid,
             pSmeRrmContext->bssId, sizeof(scanRequest.bssid) );

       if (pSmeRrmContext->ssId.length)
       {
          scanRequest.SSIDs.numOfSSIDs = 1;
          scanRequest.SSIDs.SSIDList =( tCsrSSIDInfo *)vos_mem_malloc(sizeof(tCsrSSIDInfo));
          if (NULL == scanRequest.SSIDs.SSIDList)
          {
              smsLog( pMac, LOGP, FL("vos_mem_malloc failed:") );
              return eHAL_STATUS_FAILURE;
          }
#if defined WLAN_VOWIFI_DEBUG
          smsLog( pMac, LOGE, FL("Allocated memory for pSSIDList"));
#endif
          vos_mem_zero( scanRequest.SSIDs.SSIDList, sizeof(tCsrSSIDInfo) );
          scanRequest.SSIDs.SSIDList->SSID.length = pSmeRrmContext->ssId.length;
          vos_mem_copy(scanRequest.SSIDs.SSIDList->SSID.ssId, pSmeRrmContext->ssId.ssId, pSmeRrmContext->ssId.length);
       }

       /* set min and max channel time */
       scanRequest.minChnTime = 0; //pSmeRrmContext->duration; Dont use min timeout.
       if( eRRM_MSG_SOURCE_ESE_UPLOAD == pSmeRrmContext->msgSource ||
           eRRM_MSG_SOURCE_LEGACY_ESE == pSmeRrmContext->msgSource )
          scanRequest.maxChnTime = pSmeRrmContext->duration[pSmeRrmContext->currentIndex];
       else
          scanRequest.maxChnTime = pSmeRrmContext->duration[0];

       smsLog( pMac, LOG1, "Scan Type(%s (%d)) Max Dwell Time(%d)",
               lim_ScanTypetoString(scanRequest.scanType),
               scanRequest.scanType,
               scanRequest.maxChnTime );

#if defined WLAN_VOWIFI_DEBUG
       smsLog( pMac, LOGE, "For Duration %d ", scanRequest.maxChnTime );
#endif

       /* set BSSType to default type */
       scanRequest.BSSType = eCSR_BSS_TYPE_ANY;

       /*Scan all the channels */
       scanRequest.ChannelInfo.numOfChannels = 1;

       scanRequest.ChannelInfo.ChannelList = &pSmeRrmContext->channelList.ChannelList[pSmeRrmContext->currentIndex];
#if defined WLAN_VOWIFI_DEBUG
       smsLog( pMac, LOGE, "On channel %d ", pSmeRrmContext->channelList.ChannelList[pSmeRrmContext->currentIndex] );
#endif

       /* set requestType to full scan */
       scanRequest.requestType = eCSR_SCAN_REQUEST_FULL_SCAN;

       csrRoamGetSessionIdFromBSSID( pMac, (tCsrBssid*)pSmeRrmContext->sessionBssId, &sessionId );
       status = sme_ScanRequest( pMac, (tANI_U8)sessionId, &scanRequest, &scanId, &sme_RrmScanRequestCallback, NULL );

       if ( pSmeRrmContext->ssId.length )
       {
           vos_mem_free(scanRequest.SSIDs.SSIDList);
#if defined WLAN_VOWIFI_DEBUG
           smsLog( pMac, LOGE, FL("Free memory for SSIDList"));
#endif
       }
   }
   else if (2 == scanType)  /* beacon table */
   {
       if ((pSmeRrmContext->currentIndex + 1) < pSmeRrmContext->channelList.numOfChannels)
       {
           sme_RrmSendScanResult( pMac, 1, &pSmeRrmContext->channelList.ChannelList[pSmeRrmContext->currentIndex], false );
           pSmeRrmContext->currentIndex++; //Advance the current index.
           sme_RrmIssueScanReq(pMac);
       }
       else
       {
           //Done with the measurement. Clean up all context and send a message to PE with measurement done flag set.
           sme_RrmSendScanResult( pMac, 1, &pSmeRrmContext->channelList.ChannelList[pSmeRrmContext->currentIndex], true );
           vos_mem_free( pSmeRrmContext->channelList.ChannelList );
       }
   }
   else
   {
       smsLog( pMac, LOGE, "Unknown beacon report request mode(%s (%d))",
               lim_ScanTypetoString(scanType), scanType);
                /* Indicate measurement completion to PE */
                /* If this is not done, pCurrentReq pointer will not be freed and
                   PE will not handle subsequent Beacon requests */
        sme_RrmSendBeaconReportXmitInd(pMac, NULL, true, 0);
   }

   return status;
}

/*--------------------------------------------------------------------------
  \brief sme_RrmProcessBeaconReportReqInd() - This is called to process the Beacon 
         report request from peer AP forwarded through PE .
  
  \param pMsgBuf - a pointer to a buffer that maps to various structures base 
                   on the message type.
                   The beginning of the buffer can always map to tSirSmeRsp.
  
  \return eHAL_STATUS_SUCCESS - Validation is successful.
  
  \sa
  
  --------------------------------------------------------------------------*/
void sme_RrmProcessBeaconReportReqInd(tpAniSirGlobal pMac, void *pMsgBuf)
{
   tpSirBeaconReportReqInd pBeaconReq = (tpSirBeaconReportReqInd) pMsgBuf;
   tpRrmSMEContext pSmeRrmContext = &pMac->rrm.rrmSmeContext;
   tANI_U32 len = 0, i = 0;

#if defined WLAN_VOWIFI_DEBUG
   smsLog( pMac, LOGE, "Received Beacon report request ind Channel = %d", pBeaconReq->channelInfo.channelNum );
#endif
   //section 11.10.8.1 (IEEE Std 802.11k-2008) 
   //channel 0 and 255 has special meaning.
   if( (pBeaconReq->channelInfo.channelNum == 0)  || 
       ((pBeaconReq->channelInfo.channelNum == 255) && (pBeaconReq->channelList.numChannels == 0) ) ) 
   {
      //Add all the channel in the regulatory domain.
      wlan_cfgGetStrLen( pMac, WNI_CFG_VALID_CHANNEL_LIST, &len );
      pSmeRrmContext->channelList.ChannelList = vos_mem_malloc( len );
      if( pSmeRrmContext->channelList.ChannelList == NULL )
      {
         smsLog( pMac, LOGP, FL("vos_mem_malloc failed:") );
         return;
      }
#if defined WLAN_VOWIFI_DEBUG
      smsLog( pMac, LOGE, FL("Allocated memory for ChannelList") );
#endif
      csrGetCfgValidChannels( pMac, pSmeRrmContext->channelList.ChannelList, &len );
      pSmeRrmContext->channelList.numOfChannels = (tANI_U8)len;
#if defined WLAN_VOWIFI_DEBUG
      smsLog( pMac, LOGE, "channel == 0 performing on all channels");
#endif
   }
   else
   { 
      len = 0;
      pSmeRrmContext->channelList.numOfChannels = 0;

      //If valid channel is present. We first Measure on the given channel. and
      //if there are additional channels present in APchannelreport, measure on these also.
      if ( pBeaconReq->channelInfo.channelNum != 255 )
         len = 1;
#if defined WLAN_VOWIFI_DEBUG
      else
         smsLog( pMac, LOGE, "channel == 255");
#endif

      len += pBeaconReq->channelList.numChannels;

      pSmeRrmContext->channelList.ChannelList = vos_mem_malloc( len );
      if( pSmeRrmContext->channelList.ChannelList == NULL )
      {
         smsLog( pMac, LOGP, FL("vos_mem_malloc failed") );
         return;
      }
#if defined WLAN_VOWIFI_DEBUG
      smsLog( pMac, LOGE, FL("Allocated memory for ChannelList") );
#endif

      if ( pBeaconReq->channelInfo.channelNum != 255 )
      {
#if defined WLAN_VOWIFI_DEBUG
         smsLog( pMac, LOGE, "channel == %d  ", pBeaconReq->channelInfo.channelNum );
#endif
         if(csrRoamIsChannelValid( pMac, pBeaconReq->channelInfo.channelNum ))
            pSmeRrmContext->channelList.ChannelList[pSmeRrmContext->channelList.numOfChannels++] = pBeaconReq->channelInfo.channelNum;
#if defined WLAN_VOWIFI_DEBUG
         else
            smsLog( pMac, LOGE, "is Invalid channel, Ignoring this channel" );
#endif
      }

      for ( i = 0 ; i < pBeaconReq->channelList.numChannels; i++ )
      {
         if(csrRoamIsChannelValid( pMac, pBeaconReq->channelList.channelNumber[i] ))
         {
            pSmeRrmContext->channelList.ChannelList[pSmeRrmContext->channelList.numOfChannels] = pBeaconReq->channelList.channelNumber[i];
            pSmeRrmContext->channelList.numOfChannels++;
         }
      }
   }

   //Copy session bssid
   vos_mem_copy( pSmeRrmContext->sessionBssId, pBeaconReq->bssId, sizeof(tSirMacAddr) );

   //copy measurement bssid
   vos_mem_copy( pSmeRrmContext->bssId, pBeaconReq->macaddrBssid, sizeof(tSirMacAddr) );

   //Copy ssid
   vos_mem_copy( &pSmeRrmContext->ssId, &pBeaconReq->ssId, sizeof(tAniSSID) ); 

   pSmeRrmContext->token = pBeaconReq->uDialogToken;
   pSmeRrmContext->regClass = pBeaconReq->channelInfo.regulatoryClass;
   pSmeRrmContext->randnIntvl = VOS_MAX( pBeaconReq->randomizationInterval, pSmeRrmContext->rrmConfig.maxRandnInterval );
   pSmeRrmContext->currentIndex = 0;
   pSmeRrmContext->msgSource = pBeaconReq->msgSource;
   vos_mem_copy((tANI_U8*)&pSmeRrmContext->measMode, (tANI_U8*)&pBeaconReq->fMeasurementtype, SIR_ESE_MAX_MEAS_IE_REQS);
   vos_mem_copy((tANI_U8*)&pSmeRrmContext->duration, (tANI_U8*)&pBeaconReq->measurementDuration, SIR_ESE_MAX_MEAS_IE_REQS);

   sme_RrmIssueScanReq( pMac );

   return;
}

/*--------------------------------------------------------------------------
  \brief sme_RrmNeighborReportRequest() - This is API can be used to trigger a 
         Neighbor report from the peer.
  
  \param sessionId - session identifier on which the request should be made.       
  \param pNeighborReq - a pointer to a neighbor report request.
  
  \return eHAL_STATUS_SUCCESS - Validation is successful.
  
  \sa
  
  --------------------------------------------------------------------------*/
VOS_STATUS sme_RrmNeighborReportRequest(tpAniSirGlobal pMac, tANI_U8 sessionId, 
                                    tpRrmNeighborReq pNeighborReq, tpRrmNeighborRspCallbackInfo callbackInfo)
{
   eHalStatus status = eHAL_STATUS_SUCCESS;
   tpSirNeighborReportReqInd pMsg;
   tCsrRoamSession *pSession;

#if defined WLAN_VOWIFI_DEBUG
   smsLog( pMac, LOGE, FL("Request to send Neighbor report request received "));
#endif
   if( !CSR_IS_SESSION_VALID( pMac, sessionId ) )
   {  
      smsLog( pMac, LOGE, FL("Invalid session %d"), sessionId );
      return VOS_STATUS_E_INVAL;
   }
   pSession = CSR_GET_SESSION( pMac, sessionId );

   /* If already a report is pending, return failure */
   if (eANI_BOOLEAN_TRUE == pMac->rrm.rrmSmeContext.neighborReqControlInfo.isNeighborRspPending)
   {
       smsLog( pMac, LOGE, FL("Neighbor request already pending.. Not allowed"));
       return VOS_STATUS_E_AGAIN;
   }
   
   pMsg = vos_mem_malloc ( sizeof(tSirNeighborReportReqInd) );
   if ( NULL == pMsg )
   {
      smsLog( pMac, LOGE, "Unable to allocate memory for Neighbor request");
      return VOS_STATUS_E_NOMEM;
   }

   
   vos_mem_zero( pMsg, sizeof(tSirNeighborReportReqInd) );
#if defined WLAN_VOWIFI_DEBUG
   smsLog( pMac, LOGE, FL(" Allocated memory for Neighbor request") );
#endif

   rrmLLPurgeNeighborCache(pMac, &pMac->rrm.rrmSmeContext.neighborReportCache);

#if defined WLAN_VOWIFI_DEBUG
   smsLog( pMac, LOGE, FL("Purged the neighbor cache before sending Neighbor request: Status = %d"), status );
#endif

   pMsg->messageType = eWNI_SME_NEIGHBOR_REPORT_REQ_IND;
   pMsg->length = sizeof( tSirNeighborReportReqInd );
   vos_mem_copy( &pMsg->bssId, &pSession->connectedProfile.bssid, sizeof(tSirMacAddr) );
   pMsg->noSSID = pNeighborReq->no_ssid;
   vos_mem_copy( &pMsg->ucSSID, &pNeighborReq->ssid, sizeof(tSirMacSSid));

   status = palSendMBMessage(pMac->hHdd, pMsg);
   if( status != eHAL_STATUS_SUCCESS )
      return VOS_STATUS_E_FAILURE;

   /* Neighbor report request message sent successfully to PE. Now register the callbacks */
   pMac->rrm.rrmSmeContext.neighborReqControlInfo.neighborRspCallbackInfo.neighborRspCallback = 
                                                            callbackInfo->neighborRspCallback;
   pMac->rrm.rrmSmeContext.neighborReqControlInfo.neighborRspCallbackInfo.neighborRspCallbackContext = 
                                                            callbackInfo->neighborRspCallbackContext;
   pMac->rrm.rrmSmeContext.neighborReqControlInfo.isNeighborRspPending = eANI_BOOLEAN_TRUE;

   /* Start neighbor response wait timer now */
   vos_timer_start(&pMac->rrm.rrmSmeContext.neighborReqControlInfo.neighborRspWaitTimer, callbackInfo->timeout);
   
   return VOS_STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
  \brief rrmCalculateNeighborAPRoamScore() - This API is called while handling 
                individual neighbor reports from the APs neighbor AP report to 
                calculate the cumulative roam score before storing it in neighbor 
                cache.
  
  \param pNeighborReportDesc - Neighbor BSS Descriptor node for which roam score 
                                should be calculated
  
  \return void.
--------------------------------------------------------------------------*/
static void rrmCalculateNeighborAPRoamScore(tpAniSirGlobal pMac, tpRrmNeighborReportDesc pNeighborReportDesc)
{
    tpSirNeighborBssDescripton  pNeighborBssDesc;
    tANI_U32    roamScore = 0;
    
    if (NULL == pNeighborReportDesc)
    {
        VOS_ASSERT(0);
        return;
    }
    if (NULL == pNeighborReportDesc->pNeighborBssDescription)
    {
        VOS_ASSERT(0);
        return;
    }

    pNeighborBssDesc = pNeighborReportDesc->pNeighborBssDescription;

    if (pNeighborBssDesc->bssidInfo.rrmInfo.fMobilityDomain)
    {
        roamScore += RRM_ROAM_SCORE_NEIGHBOR_REPORT_MOBILITY_DOMAIN;
        if (pNeighborBssDesc->bssidInfo.rrmInfo.fSameSecurityMode)
        {
            roamScore += RRM_ROAM_SCORE_NEIGHBOR_REPORT_SECURITY;
            if (pNeighborBssDesc->bssidInfo.rrmInfo.fSameAuthenticator)
            {
                roamScore += RRM_ROAM_SCORE_NEIGHBOR_REPORT_KEY_SCOPE;
                if (pNeighborBssDesc->bssidInfo.rrmInfo.fCapRadioMeasurement)
                {
                    roamScore += RRM_ROAM_SCORE_NEIGHBOR_REPORT_CAPABILITY_RRM;
                    if (pNeighborBssDesc->bssidInfo.rrmInfo.fCapSpectrumMeasurement)
                        roamScore += RRM_ROAM_SCORE_NEIGHBOR_REPORT_CAPABILITY_SPECTRUM_MGMT;
                    if (pNeighborBssDesc->bssidInfo.rrmInfo.fCapQos)
                        roamScore += RRM_ROAM_SCORE_NEIGHBOR_REPORT_CAPABILITY_QOS;
                    if (pNeighborBssDesc->bssidInfo.rrmInfo.fCapApsd)
                        roamScore += RRM_ROAM_SCORE_NEIGHBOR_REPORT_CAPABILITY_APSD;
                    if (pNeighborBssDesc->bssidInfo.rrmInfo.fCapDelayedBlockAck)
                        roamScore += RRM_ROAM_SCORE_NEIGHBOR_REPORT_CAPABILITY_DELAYED_BA;
                    if (pNeighborBssDesc->bssidInfo.rrmInfo.fCapImmediateBlockAck)
                        roamScore += RRM_ROAM_SCORE_NEIGHBOR_REPORT_CAPABILITY_IMMEDIATE_BA;
                    if (pNeighborBssDesc->bssidInfo.rrmInfo.fApPreauthReachable)
                        roamScore += RRM_ROAM_SCORE_NEIGHBOR_REPORT_REACHABILITY;
                }
            }
        }
    }
#ifdef FEATURE_WLAN_ESE
    // It has come in the report so its the best score
    if (csrNeighborRoamIs11rAssoc(pMac) == FALSE)
    {
        // IAPP Route so lets make use of this info
        // save all AP, as the list does not come all the time
        // Save and reuse till the next AP List comes to us.
        // Even save our own MAC address. Will be useful next time around.
        roamScore += RRM_ROAM_SCORE_NEIGHBOR_IAPP_LIST;
    }
#endif
    pNeighborReportDesc->roamScore = roamScore;

    return;
}

/*--------------------------------------------------------------------------
  \brief rrmStoreNeighborRptByRoamScore() - This API is called to store a given 
                        Neighbor BSS descriptor to the neighbor cache. This function 
                        stores the neighbor BSS descriptors in such a way that descriptors 
                        are sorted by roamScore in descending order

  \param pNeighborReportDesc - Neighbor BSS Descriptor node to be stored in cache
  
  \return void.
--------------------------------------------------------------------------*/
void rrmStoreNeighborRptByRoamScore(tpAniSirGlobal pMac, tpRrmNeighborReportDesc pNeighborReportDesc)
{
   tpRrmSMEContext pSmeRrmContext = &pMac->rrm.rrmSmeContext;
   tListElem       *pEntry;
   tRrmNeighborReportDesc  *pTempNeighborReportDesc;

   if (NULL == pNeighborReportDesc)
   {
       VOS_ASSERT(0);
       return;
   }
   if (NULL == pNeighborReportDesc->pNeighborBssDescription)
   {
       VOS_ASSERT(0);
       return;
   }

   if (csrLLIsListEmpty(&pSmeRrmContext->neighborReportCache, LL_ACCESS_LOCK))
   {
       smsLog(pMac, LOGE, FL("Neighbor report cache is empty.. Adding a entry now"));
        /* Neighbor list cache is empty. Insert this entry in the tail */
       csrLLInsertTail(&pSmeRrmContext->neighborReportCache, &pNeighborReportDesc->List, LL_ACCESS_LOCK);
       return;
   }
   else
   {
       /* Should store the neighbor BSS description in the order sorted by roamScore in descending
              order. APs with highest roamScore should be the 1st entry in the list */
        pEntry = csrLLPeekHead(&pSmeRrmContext->neighborReportCache, LL_ACCESS_LOCK);
        while (pEntry != NULL)
        {
            pTempNeighborReportDesc = GET_BASE_ADDR( pEntry, tRrmNeighborReportDesc, List );
            if (pTempNeighborReportDesc->roamScore < pNeighborReportDesc->roamScore)
                break;
            pEntry = csrLLNext(&pSmeRrmContext->neighborReportCache, pEntry, LL_ACCESS_LOCK);
        } 

        if (pEntry)
            /* This BSS roamscore is better than something in the list. Insert this before that one */
            csrLLInsertEntry(&pSmeRrmContext->neighborReportCache, pEntry, &pNeighborReportDesc->List, LL_ACCESS_LOCK);
        else
            /* All the entries in the list has a better roam Score than this one. Insert this at the last */
            csrLLInsertTail(&pSmeRrmContext->neighborReportCache, &pNeighborReportDesc->List, LL_ACCESS_LOCK);
   }
   return;
}

/*--------------------------------------------------------------------------
  \brief sme_RrmProcessNeighborReport() - This is called to process the Neighbor 
         report received from PE.
  
  \param pMsgBuf - a pointer to a buffer that maps to various structures base 
                   on the message type.
                   The beginning of the buffer can always map to tSirSmeRsp.
  
  \return eHAL_STATUS_SUCCESS - Validation is successful.
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_RrmProcessNeighborReport(tpAniSirGlobal pMac, void *pMsgBuf)
{
   eHalStatus status = eHAL_STATUS_SUCCESS;
   tpSirNeighborReportInd pNeighborRpt = (tpSirNeighborReportInd) pMsgBuf;
   tpRrmNeighborReportDesc  pNeighborReportDesc;
   tANI_U8 i = 0;
   VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;

#ifdef FEATURE_WLAN_ESE
   // Clear the cache for ESE.
   if (csrNeighborRoamIsESEAssoc(pMac))
   {
       rrmLLPurgeNeighborCache(pMac, 
           &pMac->rrm.rrmSmeContext.neighborReportCache);
   }
#endif

   for (i = 0; i < pNeighborRpt->numNeighborReports; i++)
   {
       pNeighborReportDesc = vos_mem_malloc(sizeof(tRrmNeighborReportDesc));
       if (NULL == pNeighborReportDesc)
       {
           smsLog( pMac, LOGE, "Failed to allocate memory for RRM Neighbor report desc");
           status = eHAL_STATUS_FAILED_ALLOC;
           goto end;
            
       }

       vos_mem_zero(pNeighborReportDesc, sizeof(tRrmNeighborReportDesc));
       pNeighborReportDesc->pNeighborBssDescription = vos_mem_malloc(sizeof(tSirNeighborBssDescription));
       if (NULL == pNeighborReportDesc->pNeighborBssDescription)
       {
           smsLog( pMac, LOGE, "Failed to allocate memory for RRM Neighbor report BSS Description");
           vos_mem_free(pNeighborReportDesc);
           status = eHAL_STATUS_FAILED_ALLOC;
           goto end;
       }
       vos_mem_zero(pNeighborReportDesc->pNeighborBssDescription, sizeof(tSirNeighborBssDescription));
       vos_mem_copy(pNeighborReportDesc->pNeighborBssDescription, &pNeighborRpt->sNeighborBssDescription[i], 
                                                sizeof(tSirNeighborBssDescription));

#if defined WLAN_VOWIFI_DEBUG
       smsLog( pMac, LOGE, "Received neighbor report with Neighbor BSSID: "MAC_ADDRESS_STR,
                            MAC_ADDR_ARRAY(pNeighborRpt->sNeighborBssDescription[i].bssId));
#endif

       /* Calculate the roam score based on the BSS Capability in the BSSID Information and store it in Neighbor report Desc */
       rrmCalculateNeighborAPRoamScore(pMac, pNeighborReportDesc);

       /* Store the Neighbor report Desc in the cache based on the roam score */
       if ( pNeighborReportDesc->roamScore > 0)
       {
          rrmStoreNeighborRptByRoamScore(pMac, pNeighborReportDesc);
       }
       else
       {
           smsLog(pMac, LOGE, FL("Roam score of BSSID  "MAC_ADDRESS_STR" is 0, Ignoring.."),
                        MAC_ADDR_ARRAY(pNeighborRpt->sNeighborBssDescription[i].bssId));

           vos_mem_free(pNeighborReportDesc->pNeighborBssDescription);
           vos_mem_free(pNeighborReportDesc);
       }
   }
end:  
   
   if (!csrLLCount(&pMac->rrm.rrmSmeContext.neighborReportCache))
      vosStatus = VOS_STATUS_E_FAILURE;
 
   /* Received a report from AP. Indicate SUCCESS to the caller if there are some valid reports */
   rrmIndicateNeighborReportResult(pMac, vosStatus);

   return status;
}
/*--------------------------------------------------------------------------
  \brief sme_RrmMsgProcessor() - sme_ProcessMsg() calls this function for the 
  messages that are handled by SME RRM module.
  
  \param pMac - Pointer to the global MAC parameter structure.
  \param msg_type - the type of msg passed by PE as defined in wniApi.h
  \param pMsgBuf - a pointer to a buffer that maps to various structures base 
                   on the message type.
                   The beginning of the buffer can always map to tSirSmeRsp.
  
  \return eHAL_STATUS_SUCCESS - Validation is successful.
  
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_RrmMsgProcessor( tpAniSirGlobal pMac,  v_U16_t msg_type, 
                                void *pMsgBuf)
{
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, 
         FL(" Msg = %d for RRM measurement") , msg_type );

   //switch on the msg type & make the state transition accordingly
   switch(msg_type)
   {
      case eWNI_SME_NEIGHBOR_REPORT_IND:
         sme_RrmProcessNeighborReport( pMac, pMsgBuf );
         break;

      case eWNI_SME_BEACON_REPORT_REQ_IND:
         sme_RrmProcessBeaconReportReqInd( pMac, pMsgBuf );
         break;

      default:
         //err msg
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
               FL("sme_RrmMsgProcessor:unknown msg type = %d"), msg_type);

         break;
   }

   return eHAL_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------

    \fn rrmIterMeasTimerHandle

    \brief  Timer handler to handlet the timeout condition when a specific BT

            stop event does not come back, in which case to restore back the

            heartbeat timer.

    \param  pMac - The handle returned by macOpen.

    \return VOID

  ---------------------------------------------------------------------------*/

void rrmIterMeasTimerHandle( v_PVOID_t userData )
{
   tpAniSirGlobal pMac = (tpAniSirGlobal) userData;
#if defined WLAN_VOWIFI_DEBUG
   smsLog( pMac, LOGE, "Randomization timer expired...send on next channel ");
#endif
    //Issue a scan req for next channel.
    sme_RrmIssueScanReq( pMac ); 
}

/* ---------------------------------------------------------------------------
    
    \fn rrmNeighborRspTimeoutHandler
    
    \brief  Timer handler to handle the timeout condition when a neighbor request is sent 
                    and no neighbor response is received from the AP
    
    \param  pMac - The handle returned by macOpen.
    
    \return VOID
    
---------------------------------------------------------------------------*/
    
void rrmNeighborRspTimeoutHandler
( v_PVOID_t userData )
{
   tpAniSirGlobal pMac = (tpAniSirGlobal) userData;
#if defined WLAN_VOWIFI_DEBUG
   smsLog( pMac, LOGE, "Neighbor Response timed out ");
#endif
    rrmIndicateNeighborReportResult(pMac, VOS_STATUS_E_FAILURE);
    return;
}

/* ---------------------------------------------------------------------------

    \fn rrmOpen

    \brief  

    \param  pMac - The handle returned by macOpen.

    \return VOS_STATUS

            VOS_STATUS_E_FAILURE  success

            VOS_STATUS_SUCCESS  failure

  ---------------------------------------------------------------------------*/

VOS_STATUS rrmOpen (tpAniSirGlobal pMac)

{

   VOS_STATUS vosStatus;
   tpRrmSMEContext pSmeRrmContext = &pMac->rrm.rrmSmeContext;
   eHalStatus   halStatus = eHAL_STATUS_SUCCESS;

   pSmeRrmContext->rrmConfig.maxRandnInterval = 50; //ms

   vosStatus = vos_timer_init( &pSmeRrmContext->IterMeasTimer,

                      VOS_TIMER_TYPE_SW,

                      rrmIterMeasTimerHandle,

                      (void*) pMac);

   if (!VOS_IS_STATUS_SUCCESS(vosStatus)) {

       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "rrmOpen: Fail to init timer");

       return VOS_STATUS_E_FAILURE;
   }

   vosStatus = vos_timer_init( &pSmeRrmContext->neighborReqControlInfo.neighborRspWaitTimer,

                      VOS_TIMER_TYPE_SW,

                      rrmNeighborRspTimeoutHandler,

                      (void*) pMac);

   if (!VOS_IS_STATUS_SUCCESS(vosStatus)) {

       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "rrmOpen: Fail to init timer");

       return VOS_STATUS_E_FAILURE;
   }

   pSmeRrmContext->neighborReqControlInfo.isNeighborRspPending = eANI_BOOLEAN_FALSE;

   halStatus = csrLLOpen(pMac->hHdd, &pSmeRrmContext->neighborReportCache);
   if (eHAL_STATUS_SUCCESS != halStatus)
   {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "rrmOpen: Fail to open neighbor cache result");
       return VOS_STATUS_E_FAILURE;
   }

   return VOS_STATUS_SUCCESS;
}


/* ---------------------------------------------------------------------------

    \fn rrmClose

    \brief  

    \param  pMac - The handle returned by macOpen.

    \return VOS_STATUS

            VOS_STATUS_E_FAILURE  success

            VOS_STATUS_SUCCESS  failure

  ---------------------------------------------------------------------------*/

VOS_STATUS rrmClose (tpAniSirGlobal pMac)

{

   VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
   tpRrmSMEContext pSmeRrmContext = &pMac->rrm.rrmSmeContext;

   if( VOS_TIMER_STATE_RUNNING == vos_timer_getCurrentState( &pSmeRrmContext->IterMeasTimer ) )
   {
      vosStatus = vos_timer_stop( &pSmeRrmContext->IterMeasTimer );
      if(!VOS_IS_STATUS_SUCCESS(vosStatus))
      {
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, FL("Timer stop fail") );
      }
   }

   vosStatus = vos_timer_destroy( &pSmeRrmContext->IterMeasTimer );
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {

       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, FL("Fail to destroy timer") );

   }

   if( VOS_TIMER_STATE_RUNNING ==
          vos_timer_getCurrentState( &pSmeRrmContext->neighborReqControlInfo.neighborRspWaitTimer ) )
   {
      vosStatus = vos_timer_stop( &pSmeRrmContext->neighborReqControlInfo.neighborRspWaitTimer );
      if(!VOS_IS_STATUS_SUCCESS(vosStatus))
      {
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL, FL("Timer stop fail") );
      }
   }

   vosStatus = vos_timer_destroy( &pSmeRrmContext->neighborReqControlInfo.neighborRspWaitTimer );
   if (!VOS_IS_STATUS_SUCCESS(vosStatus))
   {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL, FL("Fail to destroy timer") );

   }

   rrmLLPurgeNeighborCache(pMac, &pSmeRrmContext->neighborReportCache);

   csrLLClose(&pSmeRrmContext->neighborReportCache);

   return vosStatus;

}




/* ---------------------------------------------------------------------------

    \fn rrmReady

    \brief  fn

    \param  pMac - The handle returned by macOpen.

    \return VOS_STATUS

  ---------------------------------------------------------------------------*/

VOS_STATUS rrmReady (tpAniSirGlobal pMac)

{

    return VOS_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------

    \fn rrmChangeDefaultConfigParam
    \brief  fn

    \param  pMac - The handle returned by macOpen.
    \param  pRrmConfig - pointer to new rrm configs.

    \return VOS_STATUS

  ---------------------------------------------------------------------------*/
VOS_STATUS rrmChangeDefaultConfigParam(tpAniSirGlobal pMac, tpRrmConfigParam pRrmConfig)
{
   vos_mem_copy( &pMac->rrm.rrmSmeContext.rrmConfig, pRrmConfig, sizeof( tRrmConfigParam ) ); 

   return VOS_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------
    
    \fn smeRrmGetFirstBssEntryFromNeighborCache()
    
    \brief  This function returns the first entry from the neighbor cache to the caller

    \param  pMac - The handle returned by macOpen.
    
    \return VOID
    
---------------------------------------------------------------------------*/
tRrmNeighborReportDesc* smeRrmGetFirstBssEntryFromNeighborCache( tpAniSirGlobal pMac)
{
   tListElem *pEntry;
   tRrmNeighborReportDesc *pTempBssEntry = NULL;
   tpRrmSMEContext pSmeRrmContext = &pMac->rrm.rrmSmeContext;


   pEntry = csrLLPeekHead( &pSmeRrmContext->neighborReportCache, LL_ACCESS_LOCK );

   if(!pEntry || !csrLLCount(&pSmeRrmContext->neighborReportCache))
   {
      //list empty
      smsLog(pMac, LOGW, FL("List empty"));
      return NULL;
   }

   pTempBssEntry = GET_BASE_ADDR( pEntry, tRrmNeighborReportDesc, List );

   return pTempBssEntry;
}

/* ---------------------------------------------------------------------------
    
    \fn smeRrmGetNextBssEntryFromNeighborCache()
    
    \brief  This function returns the entry next to the given entry from the 
                neighbor cache to the caller

    \param  pMac - The handle returned by macOpen.
    
    \return VOID
    
---------------------------------------------------------------------------*/
tRrmNeighborReportDesc* smeRrmGetNextBssEntryFromNeighborCache( tpAniSirGlobal pMac, 
                                                        tpRrmNeighborReportDesc pBssEntry)
{
   tListElem *pEntry;
   tRrmNeighborReportDesc *pTempBssEntry = NULL;

   pEntry = csrLLNext(&pMac->rrm.rrmSmeContext.neighborReportCache, &pBssEntry->List, LL_ACCESS_LOCK);

   if(!pEntry)
   {
      //list empty
      smsLog(pMac, LOGW, FL("List empty"));
      return NULL;
   }

   pTempBssEntry = GET_BASE_ADDR( pEntry, tRrmNeighborReportDesc, List );

   return pTempBssEntry;
}

#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
void csrEseSendAdjacentApRepMsg(tpAniSirGlobal pMac, tCsrRoamSession *pSession)
{
   tpSirAdjacentApRepInd pAdjRep;
   tANI_U16 length;
   tANI_U32 roamTS2;
   
   smsLog( pMac, LOG1, "Adjacent AP Report Msg to PE");

   length = sizeof(tSirAdjacentApRepInd );
   pAdjRep = vos_mem_malloc ( length );

   if ( NULL == pAdjRep )
   {
       smsLog( pMac, LOGP, "Unable to allocate memory for Adjacent AP report");
       return;
   }

   vos_mem_zero( pAdjRep, length );
   pAdjRep->messageType = eWNI_SME_ESE_ADJACENT_AP_REPORT;
   pAdjRep->length = length;
   pAdjRep->channelNum = pSession->prevOpChannel;
   vos_mem_copy( pAdjRep->bssid, &pSession->connectedProfile.bssid, sizeof(tSirMacAddr) );
   vos_mem_copy( pAdjRep->prevApMacAddr, &pSession->prevApBssid, sizeof(tSirMacAddr) );
   vos_mem_copy( &pAdjRep->prevApSSID, &pSession->prevApSSID, sizeof(tSirMacSSid) );
   roamTS2 = vos_timer_get_system_time();
   pAdjRep->tsmRoamdelay = roamTS2 - pSession->roamTS1;
   pAdjRep->roamReason =SIR_ESE_ASSOC_REASON_UNSPECIFIED;
   pAdjRep->clientDissSecs =(pAdjRep->tsmRoamdelay/1000);

   palSendMBMessage(pMac->hHdd, pAdjRep);

   return;
}
#endif   /* FEATURE_WLAN_ESE */
#endif
