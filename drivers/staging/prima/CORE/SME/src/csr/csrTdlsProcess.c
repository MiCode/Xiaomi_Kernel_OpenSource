/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
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

/** ------------------------------------------------------------------------- * 
    ------------------------------------------------------------------------- *  

  
    \file csrTdlsProcess.c
  
    Implementation for the TDLS interface to PE.
  
    Copyright (C) 2010 Qualcomm, Incorporated
  
 
   ========================================================================== */

#ifdef FEATURE_WLAN_TDLS

#include "aniGlobal.h" //for tpAniSirGlobal
#include "palApi.h"
#include "csrInsideApi.h"
#include "smeInside.h"
#include "smsDebug.h"

#include "csrSupport.h"
#include "wlan_qct_tl.h"

#include "vos_diag_core_log.h"
#include "vos_diag_core_event.h"
#include "csrInternal.h"


/*
 * common routine to remove TDLS cmd from SME command list..
 * commands are removed after getting reponse from PE.
 */
eHalStatus csrTdlsRemoveSmeCmd(tpAniSirGlobal pMac, eSmeCommandType cmdType)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tListElem *pEntry;
    tSmeCmd *pCommand;

    pEntry = csrLLPeekHead(&pMac->sme.smeCmdActiveList, LL_ACCESS_LOCK);
    if( pEntry )
    {
        pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
        if( cmdType == pCommand->command )
        {
            if( csrLLRemoveEntry( &pMac->sme.smeCmdActiveList, 
                                             pEntry, LL_ACCESS_LOCK ) )
            {
                vos_mem_zero( &pCommand->u.tdlsCmd, sizeof( tTdlsCmd ) );
                csrReleaseCommand( pMac, pCommand );
                smeProcessPendingQueue( pMac );
                status = eHAL_STATUS_SUCCESS ;
            }
        }
    }
    return status ;
}
    
/*
 * TDLS request API, called from HDD to send a TDLS frame 
 * in SME/CSR and send message to PE to trigger TDLS discovery procedure.
 */
eHalStatus csrTdlsSendMgmtReq(tHalHandle hHal, tANI_U8 sessionId, tCsrTdlsSendMgmt *tdlsSendMgmt)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tSmeCmd *tdlsSendMgmtCmd ;
    eHalStatus status = eHAL_STATUS_FAILURE ;

    //If connected and in Infra. Only then allow this
    if( CSR_IS_SESSION_VALID( pMac, sessionId ) && 
        csrIsConnStateConnectedInfra( pMac, sessionId ) &&
        (NULL != tdlsSendMgmt) )
    {
        tdlsSendMgmtCmd = csrGetCommandBuffer(pMac) ;

        if(tdlsSendMgmtCmd)
        {
            tTdlsSendMgmtCmdInfo *tdlsSendMgmtCmdInfo = 
                            &tdlsSendMgmtCmd->u.tdlsCmd.u.tdlsSendMgmtCmdInfo ;

            vos_mem_zero(&tdlsSendMgmtCmd->u.tdlsCmd, sizeof(tTdlsCmd));

            tdlsSendMgmtCmd->sessionId = sessionId;

            tdlsSendMgmtCmdInfo->frameType = tdlsSendMgmt->frameType ;   
            tdlsSendMgmtCmdInfo->dialog = tdlsSendMgmt->dialog ;   
            tdlsSendMgmtCmdInfo->statusCode = tdlsSendMgmt->statusCode ;
            tdlsSendMgmtCmdInfo->responder = tdlsSendMgmt->responder;
            tdlsSendMgmtCmdInfo->peerCapability = tdlsSendMgmt->peerCapability;
            vos_mem_copy(tdlsSendMgmtCmdInfo->peerMac,
                                   tdlsSendMgmt->peerMac, sizeof(tSirMacAddr)) ; 

            if( (0 != tdlsSendMgmt->len) && (NULL != tdlsSendMgmt->buf) )
            {
                tdlsSendMgmtCmdInfo->buf = vos_mem_malloc(tdlsSendMgmt->len);
                if ( NULL == tdlsSendMgmtCmdInfo->buf )
                    status = eHAL_STATUS_FAILURE;
                else
                    status = eHAL_STATUS_SUCCESS;
                if(!HAL_STATUS_SUCCESS( status ) )
                {
                    smsLog( pMac, LOGE, FL("Alloc Failed") );
                    VOS_ASSERT(0) ;
                    return status ;
                }
                vos_mem_copy(tdlsSendMgmtCmdInfo->buf,
                        tdlsSendMgmt->buf, tdlsSendMgmt->len );
                tdlsSendMgmtCmdInfo->len = tdlsSendMgmt->len;
            }
            else
            {
                tdlsSendMgmtCmdInfo->buf = NULL;
                tdlsSendMgmtCmdInfo->len = 0;
            }

            tdlsSendMgmtCmd->command = eSmeCommandTdlsSendMgmt ;
            tdlsSendMgmtCmd->u.tdlsCmd.size = sizeof(tTdlsSendMgmtCmdInfo) ;
            smePushCommand(pMac, tdlsSendMgmtCmd, FALSE) ;
            status = eHAL_STATUS_SUCCESS ;
            smsLog( pMac, LOG1,
                        FL("Successfully posted tdlsSendMgmtCmd to SME"));
        }
    }

    return status ;
}

/*
 * TDLS request API, called from HDD to modify an existing TDLS peer
 */
eHalStatus csrTdlsChangePeerSta(tHalHandle hHal, tANI_U8 sessionId,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                                const tSirMacAddr peerMac,
#else
                                tSirMacAddr peerMac,
#endif
                                tCsrStaParams *pstaParams)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tSmeCmd *tdlsAddStaCmd ;
    eHalStatus status = eHAL_STATUS_FAILURE ;

    if (NULL == pstaParams)
        return status;

    //If connected and in Infra. Only then allow this
    if (CSR_IS_SESSION_VALID( pMac, sessionId ) &&
        csrIsConnStateConnectedInfra( pMac, sessionId ) &&
        (NULL != peerMac)){

        tdlsAddStaCmd = csrGetCommandBuffer(pMac) ;

        if (tdlsAddStaCmd)
        {
            tTdlsAddStaCmdInfo *tdlsAddStaCmdInfo =
                         &tdlsAddStaCmd->u.tdlsCmd.u.tdlsAddStaCmdInfo ;

            vos_mem_zero(&tdlsAddStaCmd->u.tdlsCmd, sizeof(tTdlsCmd));

            tdlsAddStaCmdInfo->tdlsAddOper = TDLS_OPER_UPDATE;

            tdlsAddStaCmd->sessionId = sessionId;

            vos_mem_copy(tdlsAddStaCmdInfo->peerMac,
                          peerMac, sizeof(tSirMacAddr)) ;
            tdlsAddStaCmdInfo->capability = pstaParams->capability;
            tdlsAddStaCmdInfo->uapsdQueues = pstaParams->uapsd_queues;
            tdlsAddStaCmdInfo->maxSp = pstaParams->max_sp;
            vos_mem_copy(tdlsAddStaCmdInfo->extnCapability,
                          pstaParams->extn_capability,
                          sizeof(pstaParams->extn_capability));

            tdlsAddStaCmdInfo->htcap_present = pstaParams->htcap_present;
            if(pstaParams->htcap_present)
                vos_mem_copy( &tdlsAddStaCmdInfo->HTCap,
                              &pstaParams->HTCap, sizeof(pstaParams->HTCap));
            else
                vos_mem_set(&tdlsAddStaCmdInfo->HTCap, sizeof(pstaParams->HTCap), 0);

            tdlsAddStaCmdInfo->vhtcap_present = pstaParams->vhtcap_present;
            if(pstaParams->vhtcap_present)
                vos_mem_copy( &tdlsAddStaCmdInfo->VHTCap,
                              &pstaParams->VHTCap, sizeof(pstaParams->VHTCap));
            else
                vos_mem_set(&tdlsAddStaCmdInfo->VHTCap, sizeof(pstaParams->VHTCap), 0);

            tdlsAddStaCmdInfo->supportedRatesLen = pstaParams->supported_rates_len;

            if (0 != pstaParams->supported_rates_len)
                vos_mem_copy( &tdlsAddStaCmdInfo->supportedRates,
                              pstaParams->supported_rates,
                              pstaParams->supported_rates_len);

            tdlsAddStaCmd->command = eSmeCommandTdlsAddPeer;
            tdlsAddStaCmd->u.tdlsCmd.size = sizeof(tTdlsAddStaCmdInfo) ;
            smePushCommand(pMac, tdlsAddStaCmd, FALSE) ;
            smsLog( pMac, LOG1,
                        FL("Successfully posted tdlsAddStaCmd to SME to modify peer "));
            status = eHAL_STATUS_SUCCESS ;
        }
    }

    return status ;
}
/*
 * TDLS request API, called from HDD to Send Link Establishment Parameters
 */
VOS_STATUS csrTdlsSendLinkEstablishParams(tHalHandle hHal,
                                          tANI_U8 sessionId,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                                          const tSirMacAddr peerMac,
#else
                                          tSirMacAddr peerMac,
#endif
                                          tCsrTdlsLinkEstablishParams *tdlsLinkEstablishParams)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tSmeCmd *tdlsLinkEstablishCmd;
    eHalStatus status = eHAL_STATUS_FAILURE ;
    //If connected and in Infra. Only then allow this
    if( CSR_IS_SESSION_VALID( pMac, sessionId ) &&
        csrIsConnStateConnectedInfra( pMac, sessionId ) &&
        (NULL != peerMac) )
    {
        tdlsLinkEstablishCmd = csrGetCommandBuffer(pMac) ;

        if(tdlsLinkEstablishCmd)
        {
            tTdlsLinkEstablishCmdInfo *tdlsLinkEstablishCmdInfo =
            &tdlsLinkEstablishCmd->u.tdlsCmd.u.tdlsLinkEstablishCmdInfo ;

            vos_mem_zero(&tdlsLinkEstablishCmd->u.tdlsCmd, sizeof(tTdlsCmd));

            tdlsLinkEstablishCmd->sessionId = sessionId;

            vos_mem_copy( tdlsLinkEstablishCmdInfo->peerMac,
                          peerMac, sizeof(tSirMacAddr));
            tdlsLinkEstablishCmdInfo->isBufSta = tdlsLinkEstablishParams->isBufSta;
            tdlsLinkEstablishCmdInfo->isResponder = tdlsLinkEstablishParams->isResponder;
            tdlsLinkEstablishCmdInfo->maxSp = tdlsLinkEstablishParams->maxSp;
            tdlsLinkEstablishCmdInfo->uapsdQueues = tdlsLinkEstablishParams->uapsdQueues;
            tdlsLinkEstablishCmdInfo->isOffChannelSupported =
                                               tdlsLinkEstablishParams->isOffChannelSupported;

            vos_mem_copy(tdlsLinkEstablishCmdInfo->supportedChannels,
                          tdlsLinkEstablishParams->supportedChannels,
                          tdlsLinkEstablishParams->supportedChannelsLen);
            tdlsLinkEstablishCmdInfo->supportedChannelsLen =
                                    tdlsLinkEstablishParams->supportedChannelsLen;

            vos_mem_copy(tdlsLinkEstablishCmdInfo->supportedOperClasses,
                          tdlsLinkEstablishParams->supportedOperClasses,
                          tdlsLinkEstablishParams->supportedOperClassesLen);
            tdlsLinkEstablishCmdInfo->supportedOperClassesLen =
                                    tdlsLinkEstablishParams->supportedOperClassesLen;
            tdlsLinkEstablishCmdInfo->isResponder= tdlsLinkEstablishParams->isResponder;
            tdlsLinkEstablishCmdInfo->maxSp= tdlsLinkEstablishParams->maxSp;
            tdlsLinkEstablishCmdInfo->uapsdQueues= tdlsLinkEstablishParams->uapsdQueues;
            tdlsLinkEstablishCmd->command = eSmeCommandTdlsLinkEstablish ;
            tdlsLinkEstablishCmd->u.tdlsCmd.size = sizeof(tTdlsLinkEstablishCmdInfo) ;
            smePushCommand(pMac, tdlsLinkEstablishCmd, FALSE) ;
            status = eHAL_STATUS_SUCCESS ;
            smsLog( pMac, LOG1,
                        FL("Successfully posted tdlsLinkEstablishCmd to SME"));
        }
    }

    return status ;
}

/*
 * TDLS request API, called from HDD to add a TDLS peer
 */
eHalStatus csrTdlsAddPeerSta(tHalHandle hHal, tANI_U8 sessionId,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                             const tSirMacAddr peerMac
#else
                             tSirMacAddr peerMac
#endif
                             )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tSmeCmd *tdlsAddStaCmd ;
    eHalStatus status = eHAL_STATUS_FAILURE ;
 
    //If connected and in Infra. Only then allow this
    if( CSR_IS_SESSION_VALID( pMac, sessionId ) && 
        csrIsConnStateConnectedInfra( pMac, sessionId ) &&
        (NULL != peerMac) )
    {
        tdlsAddStaCmd = csrGetCommandBuffer(pMac) ;

        if(tdlsAddStaCmd)
        {
            tTdlsAddStaCmdInfo *tdlsAddStaCmdInfo = 
                &tdlsAddStaCmd->u.tdlsCmd.u.tdlsAddStaCmdInfo ;

            vos_mem_zero(&tdlsAddStaCmd->u.tdlsCmd, sizeof(tTdlsCmd));

            tdlsAddStaCmd->sessionId = sessionId;
            tdlsAddStaCmdInfo->tdlsAddOper = TDLS_OPER_ADD;

            vos_mem_copy( tdlsAddStaCmdInfo->peerMac,
                    peerMac, sizeof(tSirMacAddr)) ; 

            tdlsAddStaCmd->command = eSmeCommandTdlsAddPeer ;
            tdlsAddStaCmd->u.tdlsCmd.size = sizeof(tTdlsAddStaCmdInfo) ;
            smePushCommand(pMac, tdlsAddStaCmd, FALSE) ;
            status = eHAL_STATUS_SUCCESS ;
            smsLog( pMac, LOG1,
                        FL("Successfully posted tdlsAddStaCmd to SME"));
        }
    }

    return status ;
}

/*
 * TDLS request API, called from HDD to delete a TDLS peer
 */
eHalStatus csrTdlsDelPeerSta(tHalHandle hHal, tANI_U8 sessionId,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
                             const tSirMacAddr peerMac
#else
                             tSirMacAddr peerMac
#endif
)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tSmeCmd *tdlsDelStaCmd ;
    eHalStatus status = eHAL_STATUS_FAILURE ;
 
    //If connected and in Infra. Only then allow this
    if( CSR_IS_SESSION_VALID( pMac, sessionId ) && 
        csrIsConnStateConnectedInfra( pMac, sessionId ) &&
        (NULL != peerMac) )
    {
        tdlsDelStaCmd = csrGetCommandBuffer(pMac) ;

        if(tdlsDelStaCmd)
        {
            tTdlsDelStaCmdInfo *tdlsDelStaCmdInfo = 
                            &tdlsDelStaCmd->u.tdlsCmd.u.tdlsDelStaCmdInfo ;

            vos_mem_zero(&tdlsDelStaCmd->u.tdlsCmd, sizeof(tTdlsCmd));

            tdlsDelStaCmd->sessionId = sessionId;

            vos_mem_copy(tdlsDelStaCmdInfo->peerMac,
                                   peerMac, sizeof(tSirMacAddr)) ; 

            tdlsDelStaCmd->command = eSmeCommandTdlsDelPeer ;
            tdlsDelStaCmd->u.tdlsCmd.size = sizeof(tTdlsDelStaCmdInfo) ;
            smePushCommand(pMac, tdlsDelStaCmd, FALSE) ;
            status = eHAL_STATUS_SUCCESS ;
            smsLog( pMac, LOG1,
                        FL("Successfully posted tdlsDelStaCmd to SME"));
        }
    }

    return status ;
}

//tdlsoffchan
/*
 * TDLS request API, called from HDD to Send Channel Switch Parameters
 */
VOS_STATUS csrTdlsSendChanSwitchReq(tHalHandle hHal,
                                    tANI_U8 sessionId,
                                    tSirMacAddr peerMac,
                                    tANI_S32 tdlsOffCh,
                                    tANI_S32 tdlsOffChBwOffset,
                                    tANI_U8 tdlsSwMode)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tSmeCmd *tdlsChanSwitchCmd;
    eHalStatus status = eHAL_STATUS_FAILURE ;

    //If connected and in Infra. Only then allow this
    if( CSR_IS_SESSION_VALID( pMac, sessionId ) &&
        csrIsConnStateConnectedInfra( pMac, sessionId ) &&
        (NULL != peerMac) )
    {
        tdlsChanSwitchCmd = csrGetCommandBuffer(pMac) ;

        if(tdlsChanSwitchCmd)
        {
            tTdlsChanSwitchCmdInfo *tdlsChanSwitchCmdInfo =
            &tdlsChanSwitchCmd->u.tdlsCmd.u.tdlsChanSwitchCmdInfo;

            vos_mem_zero(&tdlsChanSwitchCmd->u.tdlsCmd, sizeof(tTdlsCmd));

            tdlsChanSwitchCmd->sessionId = sessionId;

            vos_mem_copy(tdlsChanSwitchCmdInfo->peerMac,
                         peerMac, sizeof(tSirMacAddr));
            tdlsChanSwitchCmdInfo->tdlsOffCh = tdlsOffCh;
            tdlsChanSwitchCmdInfo->tdlsOffChBwOffset = tdlsOffChBwOffset;
            tdlsChanSwitchCmdInfo->tdlsSwMode = tdlsSwMode;

            tdlsChanSwitchCmd->command = eSmeCommandTdlsChannelSwitch;
            tdlsChanSwitchCmd->u.tdlsCmd.size = sizeof(tTdlsChanSwitchCmdInfo) ;
            smePushCommand(pMac, tdlsChanSwitchCmd, FALSE) ;
            status = eHAL_STATUS_SUCCESS ;
            smsLog( pMac, LOG1,
                        FL("Successfully posted tdlsChanSwitchCmd to SME"));
        }
    }

    return status ;
}


/*
 * TDLS messages sent to PE .
 */
eHalStatus tdlsSendMessage(tpAniSirGlobal pMac, tANI_U16 msg_type, 
                              void *msg_data, tANI_U32 msg_size)
{

    tSirMbMsg *pMsg = (tSirMbMsg *)msg_data ;
    pMsg->type = msg_type ;
    pMsg->msgLen = (tANI_U16) (msg_size) ;

    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                              FL("sending msg = %d"), pMsg->type) ;
      /* Send message. */
    if (palSendMBMessage(pMac->hHdd, pMsg) != eHAL_STATUS_SUCCESS)
    {
        smsLog(pMac, LOGE, FL("Cannot send message"));
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}

eHalStatus csrTdlsProcessSendMgmt( tpAniSirGlobal pMac, tSmeCmd *cmd )
{
    tTdlsSendMgmtCmdInfo *tdlsSendMgmtCmdInfo = &cmd->u.tdlsCmd.u.tdlsSendMgmtCmdInfo ;
    tSirTdlsSendMgmtReq *tdlsSendMgmtReq = NULL ;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, cmd->sessionId );
    eHalStatus status = eHAL_STATUS_FAILURE;

    if (NULL == pSession)
    {
        smsLog( pMac, LOGE, FL("pSession is NULL"));
        return eHAL_STATUS_FAILURE;
    }
    if (NULL == pSession->pConnectBssDesc)
    {
        smsLog( pMac, LOGE, FL("BSS Description is not present") );
        return eHAL_STATUS_FAILURE;
    }

    tdlsSendMgmtReq = vos_mem_malloc(
                      sizeof(tSirTdlsSendMgmtReq) + tdlsSendMgmtCmdInfo->len);
    if ( NULL == tdlsSendMgmtReq )
       status = eHAL_STATUS_FAILURE;
    else
       status = eHAL_STATUS_SUCCESS;

    if (!HAL_STATUS_SUCCESS( status ) )
    {
        smsLog( pMac, LOGE, FL("alloc failed") );
        VOS_ASSERT(0) ;
        return status ;
    }
    tdlsSendMgmtReq->sessionId = cmd->sessionId;
    //Using dialog as transactionId. This can be used to match response with request
    tdlsSendMgmtReq->transactionId = tdlsSendMgmtCmdInfo->dialog;  
    tdlsSendMgmtReq->reqType =  tdlsSendMgmtCmdInfo->frameType ;
    tdlsSendMgmtReq->dialog =  tdlsSendMgmtCmdInfo->dialog ;
    tdlsSendMgmtReq->statusCode =  tdlsSendMgmtCmdInfo->statusCode ;
    tdlsSendMgmtReq->responder =  tdlsSendMgmtCmdInfo->responder;
    tdlsSendMgmtReq->peerCapability = tdlsSendMgmtCmdInfo->peerCapability;

    vos_mem_copy(tdlsSendMgmtReq->bssid,
                  pSession->pConnectBssDesc->bssId, sizeof (tSirMacAddr));

    vos_mem_copy(tdlsSendMgmtReq->peerMac,
            tdlsSendMgmtCmdInfo->peerMac, sizeof(tSirMacAddr)) ;

    if(tdlsSendMgmtCmdInfo->len && tdlsSendMgmtCmdInfo->buf)
    {
        vos_mem_copy(tdlsSendMgmtReq->addIe, tdlsSendMgmtCmdInfo->buf,
                tdlsSendMgmtCmdInfo->len);

    }
    // Send the request to PE.
    smsLog( pMac, LOG1, FL("sending TDLS Mgmt Frame req to PE " ));
    status = tdlsSendMessage(pMac, eWNI_SME_TDLS_SEND_MGMT_REQ, 
            (void *)tdlsSendMgmtReq , sizeof(tSirTdlsSendMgmtReq)+tdlsSendMgmtCmdInfo->len) ;
    if(!HAL_STATUS_SUCCESS( status ) )
    {
        smsLog( pMac, LOGE, FL("Failed to send request to MAC"));
    }
    if(tdlsSendMgmtCmdInfo->len && tdlsSendMgmtCmdInfo->buf)
    {
        //Done with the buf. Free it.
        vos_mem_free( tdlsSendMgmtCmdInfo->buf );
        tdlsSendMgmtCmdInfo->buf = NULL;
        tdlsSendMgmtCmdInfo->len = 0;
    }

    return status;
}

eHalStatus csrTdlsProcessAddSta( tpAniSirGlobal pMac, tSmeCmd *cmd )
{
    tTdlsAddStaCmdInfo *tdlsAddStaCmdInfo = &cmd->u.tdlsCmd.u.tdlsAddStaCmdInfo ;
    tSirTdlsAddStaReq *tdlsAddStaReq = NULL ;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, cmd->sessionId );
    eHalStatus status = eHAL_STATUS_FAILURE;

    if (NULL == pSession)
    {
        smsLog( pMac, LOGE, FL("pSession is NULL"));
        return eHAL_STATUS_FAILURE;
    }

    if (NULL == pSession->pConnectBssDesc)
    {
        smsLog( pMac, LOGE, FL("BSS description is not present") );
        return eHAL_STATUS_FAILURE;
    }

    tdlsAddStaReq = vos_mem_malloc(sizeof(tSirTdlsAddStaReq));
    if ( NULL == tdlsAddStaReq )
        status = eHAL_STATUS_FAILURE;
    else
        status = eHAL_STATUS_SUCCESS;

    if (!HAL_STATUS_SUCCESS( status ) )
    {
        smsLog( pMac, LOGE, FL("alloc failed") );
        VOS_ASSERT(0) ;
        return status ;
    }
    vos_mem_set(tdlsAddStaReq, sizeof(tSirTdlsAddStaReq), 0);

    tdlsAddStaReq->sessionId = cmd->sessionId;
    tdlsAddStaReq->tdlsAddOper = tdlsAddStaCmdInfo->tdlsAddOper;
    //Using dialog as transactionId. This can be used to match response with request
    tdlsAddStaReq->transactionId = 0;

    vos_mem_copy( tdlsAddStaReq->bssid,
                  pSession->pConnectBssDesc->bssId, sizeof (tSirMacAddr));

    vos_mem_copy( tdlsAddStaReq->peerMac,
            tdlsAddStaCmdInfo->peerMac, sizeof(tSirMacAddr)) ;

    tdlsAddStaReq->capability = tdlsAddStaCmdInfo->capability;
    tdlsAddStaReq->uapsd_queues = tdlsAddStaCmdInfo->uapsdQueues;
    tdlsAddStaReq->max_sp = tdlsAddStaCmdInfo->maxSp;

    vos_mem_copy( tdlsAddStaReq->extn_capability,
                              tdlsAddStaCmdInfo->extnCapability,
                              SIR_MAC_MAX_EXTN_CAP);
    tdlsAddStaReq->htcap_present = tdlsAddStaCmdInfo->htcap_present;
    vos_mem_copy( &tdlsAddStaReq->htCap,
                  &tdlsAddStaCmdInfo->HTCap, sizeof(tdlsAddStaCmdInfo->HTCap));
    tdlsAddStaReq->vhtcap_present = tdlsAddStaCmdInfo->vhtcap_present;
    vos_mem_copy( &tdlsAddStaReq->vhtCap,
                  &tdlsAddStaCmdInfo->VHTCap, sizeof(tdlsAddStaCmdInfo->VHTCap));
    tdlsAddStaReq->supported_rates_length = tdlsAddStaCmdInfo->supportedRatesLen;
    vos_mem_copy( &tdlsAddStaReq->supported_rates,
                  tdlsAddStaCmdInfo->supportedRates, tdlsAddStaCmdInfo->supportedRatesLen);

    // Send the request to PE.
    smsLog( pMac, LOGE, "sending TDLS Add Sta req to PE " );
    status = tdlsSendMessage(pMac, eWNI_SME_TDLS_ADD_STA_REQ, 
            (void *)tdlsAddStaReq , sizeof(tSirTdlsAddStaReq)) ;
    if(!HAL_STATUS_SUCCESS( status ) )
    {
        smsLog( pMac, LOGE, FL("Failed to send request to MAC"));
    }
    return status;
}

eHalStatus csrTdlsProcessDelSta( tpAniSirGlobal pMac, tSmeCmd *cmd )
{
    tTdlsDelStaCmdInfo *tdlsDelStaCmdInfo = &cmd->u.tdlsCmd.u.tdlsDelStaCmdInfo ;
    tSirTdlsDelStaReq *tdlsDelStaReq = NULL ;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, cmd->sessionId );
    eHalStatus status = eHAL_STATUS_FAILURE;

    if (NULL == pSession)
    {
        smsLog( pMac, LOGE, FL("pSession is NULL"));
        return eHAL_STATUS_FAILURE;
    }

    if (NULL == pSession->pConnectBssDesc)
    {
        smsLog( pMac, LOGE, FL("BSS description is not present") );
        return eHAL_STATUS_FAILURE;
    }

    tdlsDelStaReq = vos_mem_malloc(sizeof(tSirTdlsDelStaReq));
    if ( NULL == tdlsDelStaReq )
        status = eHAL_STATUS_FAILURE;
    else
        status = eHAL_STATUS_SUCCESS;


    if (!HAL_STATUS_SUCCESS( status ) )
    {
        smsLog( pMac, LOGE, FL("alloc failed") );
        VOS_ASSERT(0) ;
        return status ;
    }
    tdlsDelStaReq->sessionId = cmd->sessionId;
    //Using dialog as transactionId. This can be used to match response with request
    tdlsDelStaReq->transactionId = 0;

    vos_mem_copy( tdlsDelStaReq->bssid,
                  pSession->pConnectBssDesc->bssId, sizeof (tSirMacAddr));

    vos_mem_copy( tdlsDelStaReq->peerMac,
            tdlsDelStaCmdInfo->peerMac, sizeof(tSirMacAddr)) ;

    // Send the request to PE.
    smsLog( pMac, LOG1,
        "sending TDLS Del Sta "MAC_ADDRESS_STR" req to PE",
         MAC_ADDR_ARRAY(tdlsDelStaCmdInfo->peerMac));
    status = tdlsSendMessage(pMac, eWNI_SME_TDLS_DEL_STA_REQ, 
            (void *)tdlsDelStaReq , sizeof(tSirTdlsDelStaReq)) ;
    if(!HAL_STATUS_SUCCESS( status ) )
    {
        smsLog( pMac, LOGE, FL("Failed to send request to MAC"));
    }
    return status;
}
/*
 * commands received from CSR
 */
eHalStatus csrTdlsProcessCmd(tpAniSirGlobal pMac, tSmeCmd *cmd)
{
    eSmeCommandType  cmdType = cmd->command ;
    tANI_BOOLEAN status = eANI_BOOLEAN_TRUE;
    switch(cmdType)
    {
        case eSmeCommandTdlsSendMgmt:
        {
            status = csrTdlsProcessSendMgmt( pMac, cmd );
            if(HAL_STATUS_SUCCESS( status ) )
            {
               status = eANI_BOOLEAN_FALSE ;
            }
        }
        break ;
        case eSmeCommandTdlsAddPeer:
        {
            status = csrTdlsProcessAddSta( pMac, cmd );
            if(HAL_STATUS_SUCCESS( status ) )
            {
               status = eANI_BOOLEAN_FALSE ;
            }
        }
        break;
        case eSmeCommandTdlsDelPeer: 
        {
            status = csrTdlsProcessDelSta( pMac, cmd );
            if(HAL_STATUS_SUCCESS( status ) )
            {
               status = eANI_BOOLEAN_FALSE ;
            }
        }
        break;
        case eSmeCommandTdlsLinkEstablish:
        {
            status = csrTdlsProcessLinkEstablish( pMac, cmd );
            if(HAL_STATUS_SUCCESS( status ) )
            {
               status = eANI_BOOLEAN_FALSE ;
            }
        }
        break;
// tdlsoffchan
        case eSmeCommandTdlsChannelSwitch:
        {
             status = csrTdlsProcessChanSwitchReq( pMac, cmd );
             if(HAL_STATUS_SUCCESS( status ) )
             {
               status = eANI_BOOLEAN_FALSE ;
             }
        }
        break;
       default:
       {
            /* TODO: Add defualt handling */  
           break ;
       } 
             
    }
    return status ; 
}

eHalStatus csrTdlsProcessLinkEstablish( tpAniSirGlobal pMac, tSmeCmd *cmd )
{
    tTdlsLinkEstablishCmdInfo *tdlsLinkEstablishCmdInfo = &cmd->u.tdlsCmd.u.tdlsLinkEstablishCmdInfo ;
    tSirTdlsLinkEstablishReq *tdlsLinkEstablishReq = NULL ;
    eHalStatus status = eHAL_STATUS_FAILURE;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, cmd->sessionId );

    if (NULL == pSession)
    {
        smsLog( pMac, LOGE, FL("pSession is NULL"));
        return eHAL_STATUS_FAILURE;
    }

    tdlsLinkEstablishReq = vos_mem_malloc(sizeof(tSirTdlsLinkEstablishReq));

    if (tdlsLinkEstablishReq == NULL)
    {
        smsLog( pMac, LOGE, FL("alloc failed \n") );
        VOS_ASSERT(0) ;
        return status ;
    }
    tdlsLinkEstablishReq->sessionId = cmd->sessionId;
    //Using dialog as transactionId. This can be used to match response with request
    tdlsLinkEstablishReq->transactionId = 0;
    vos_mem_copy(tdlsLinkEstablishReq->peerMac,
                  tdlsLinkEstablishCmdInfo->peerMac, sizeof(tSirMacAddr));
    vos_mem_copy(tdlsLinkEstablishReq->bssid, pSession->pConnectBssDesc->bssId,
                  sizeof (tSirMacAddr));
    vos_mem_copy(tdlsLinkEstablishReq->supportedChannels,
                  tdlsLinkEstablishCmdInfo->supportedChannels,
                  tdlsLinkEstablishCmdInfo->supportedChannelsLen);
    tdlsLinkEstablishReq->supportedChannelsLen =
                      tdlsLinkEstablishCmdInfo->supportedChannelsLen;
    vos_mem_copy(tdlsLinkEstablishReq->supportedOperClasses,
                  tdlsLinkEstablishCmdInfo->supportedOperClasses,
                  tdlsLinkEstablishCmdInfo->supportedOperClassesLen);
    tdlsLinkEstablishReq->supportedOperClassesLen =
                      tdlsLinkEstablishCmdInfo->supportedOperClassesLen;
    tdlsLinkEstablishReq->isBufSta = tdlsLinkEstablishCmdInfo->isBufSta;
    tdlsLinkEstablishReq->isResponder= tdlsLinkEstablishCmdInfo->isResponder;
    tdlsLinkEstablishReq->uapsdQueues= tdlsLinkEstablishCmdInfo->uapsdQueues;
    tdlsLinkEstablishReq->maxSp= tdlsLinkEstablishCmdInfo->maxSp;
    tdlsLinkEstablishReq->isOffChannelSupported =
        tdlsLinkEstablishCmdInfo->isOffChannelSupported;

    // Send the request to PE.
    smsLog( pMac, LOGE, "sending TDLS Link Establish Request to PE \n" );
    status = tdlsSendMessage(pMac, eWNI_SME_TDLS_LINK_ESTABLISH_REQ,
                             (void *)tdlsLinkEstablishReq,
                             sizeof(tSirTdlsLinkEstablishReq));
    if (!HAL_STATUS_SUCCESS( status ) )
    {
        smsLog( pMac, LOGE, FL("Failed to send request to MAC\n"));
    }
    return status;
}

// tdlsoffchan
eHalStatus csrTdlsProcessChanSwitchReq( tpAniSirGlobal pMac, tSmeCmd *cmd )
{
    tTdlsChanSwitchCmdInfo *tdlsChanSwitchCmdInfo = &cmd->u.tdlsCmd.u.tdlsChanSwitchCmdInfo ;
    tSirTdlsChanSwitch *tdlsChanSwitch = NULL ;
    eHalStatus status = eHAL_STATUS_FAILURE;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, cmd->sessionId );

    if (NULL == pSession)
    {
        smsLog( pMac, LOGE, FL("pSession is NULL"));
        return eHAL_STATUS_FAILURE;
    }

    tdlsChanSwitch = vos_mem_malloc(sizeof(tSirTdlsChanSwitch));
    if (tdlsChanSwitch == NULL)
    {
        smsLog( pMac, LOGE, FL("alloc failed \n") );
        VOS_ASSERT(0) ;
        return status ;
    }
    tdlsChanSwitch->sessionId = cmd->sessionId;
    //Using dialog as transactionId. This can be used to match response with request
    tdlsChanSwitch->transactionId = 0;
    vos_mem_copy( tdlsChanSwitch->peerMac,
                  tdlsChanSwitchCmdInfo->peerMac, sizeof(tSirMacAddr));
    vos_mem_copy(tdlsChanSwitch->bssid, pSession->pConnectBssDesc->bssId,
                 sizeof (tSirMacAddr));

    tdlsChanSwitch->tdlsOffCh = tdlsChanSwitchCmdInfo->tdlsOffCh;
    tdlsChanSwitch->tdlsOffChBwOffset = tdlsChanSwitchCmdInfo->tdlsOffChBwOffset;
    tdlsChanSwitch->tdlsSwMode = tdlsChanSwitchCmdInfo->tdlsSwMode;

    // Send the request to PE.
    smsLog( pMac, LOGE, "sending TDLS Channel Switch to PE \n" );
    status = tdlsSendMessage(pMac, eWNI_SME_TDLS_CHANNEL_SWITCH_REQ,
                             (void *)tdlsChanSwitch,
                             sizeof(tSirTdlsChanSwitch));
    if (!HAL_STATUS_SUCCESS( status ) )
    {
        smsLog( pMac, LOGE, FL("Failed to send request to MAC\n"));
    }
    return status;
}

/*
 * TDLS Message processor, will be called after TDLS message recieved from
 * PE
 */
eHalStatus tdlsMsgProcessor(tpAniSirGlobal pMac,  v_U16_t msgType,
                                void *pMsgBuf)
{
    switch(msgType)
    {
        case eWNI_SME_TDLS_SEND_MGMT_RSP:
        {
            tSirSmeRsp *pMsg = (tSirSmeRsp*) pMsgBuf;
            tCsrRoamInfo roamInfo = {0} ;

            /* remove pending eSmeCommandTdlsDiscovery command */
            csrTdlsRemoveSmeCmd(pMac, eSmeCommandTdlsSendMgmt) ;

            if (eSIR_SME_SUCCESS != pMsg->statusCode)
            {
                /* Tx failed, so there wont be any ack confirmation*/
                /* Indicate ack failure to upper layer */
                roamInfo.reasonCode = 0;
                csrRoamCallCallback(pMac, pMsg->sessionId, &roamInfo,
                        0, eCSR_ROAM_RESULT_MGMT_TX_COMPLETE_IND, 0);
            }
        }
        break;
        case eWNI_SME_TDLS_ADD_STA_RSP:
        {
            tSirTdlsAddStaRsp *addStaRsp = (tSirTdlsAddStaRsp *) pMsgBuf ;
            eCsrRoamResult roamResult ;
            tCsrRoamInfo roamInfo = {0} ;
            vos_mem_copy( &roamInfo.peerMac, addStaRsp->peerMac,
                                         sizeof(tSirMacAddr)) ;
            roamInfo.staId = addStaRsp->staId ;
            roamInfo.ucastSig = addStaRsp->ucastSig ;
            roamInfo.bcastSig = addStaRsp->bcastSig ;
            roamInfo.statusCode = addStaRsp->statusCode ;
            /*
             * register peer with TL, we have to go through HDD as this is
             * the only way to register any STA with TL.
             */
            if (addStaRsp->tdlsAddOper == TDLS_OPER_ADD)
                roamResult = eCSR_ROAM_RESULT_ADD_TDLS_PEER;
            else /* addStaRsp->tdlsAddOper must be TDLS_OPER_UPDATE */
                roamResult = eCSR_ROAM_RESULT_UPDATE_TDLS_PEER;
            csrRoamCallCallback(pMac, addStaRsp->sessionId, &roamInfo, 0, 
                                eCSR_ROAM_TDLS_STATUS_UPDATE,
                                roamResult);

            /* remove pending eSmeCommandTdlsDiscovery command */
            csrTdlsRemoveSmeCmd(pMac, eSmeCommandTdlsAddPeer) ;
        }
        break;
        case eWNI_SME_TDLS_DEL_STA_RSP:
        {
            tSirTdlsDelStaRsp *delStaRsp = (tSirTdlsDelStaRsp *) pMsgBuf ;
            tCsrRoamInfo roamInfo = {0} ;

            vos_mem_copy( &roamInfo.peerMac, delStaRsp->peerMac,
                                         sizeof(tSirMacAddr)) ;
            roamInfo.staId = delStaRsp->staId ;
            roamInfo.statusCode = delStaRsp->statusCode ;
            /*
             * register peer with TL, we have to go through HDD as this is
             * the only way to register any STA with TL.
             */
            csrRoamCallCallback(pMac, delStaRsp->sessionId, &roamInfo, 0, 
                         eCSR_ROAM_TDLS_STATUS_UPDATE, 
                               eCSR_ROAM_RESULT_DELETE_TDLS_PEER);

            csrTdlsRemoveSmeCmd(pMac, eSmeCommandTdlsDelPeer) ;
        }
        break;
        case eWNI_SME_TDLS_DEL_STA_IND:
        {
            tpSirTdlsDelStaInd pSirTdlsDelStaInd = (tpSirTdlsDelStaInd) pMsgBuf ;
            tCsrRoamInfo roamInfo = {0} ;
            vos_mem_copy( &roamInfo.peerMac, pSirTdlsDelStaInd->peerMac,
                                         sizeof(tSirMacAddr)) ;
            roamInfo.staId = pSirTdlsDelStaInd->staId ;
            roamInfo.reasonCode = pSirTdlsDelStaInd->reasonCode ;

            /* Sending the TEARDOWN indication to HDD. */
            csrRoamCallCallback(pMac, pSirTdlsDelStaInd->sessionId, &roamInfo, 0,
                         eCSR_ROAM_TDLS_STATUS_UPDATE,
                               eCSR_ROAM_RESULT_TEARDOWN_TDLS_PEER_IND);
            break ;
        }
        case eWNI_SME_TDLS_DEL_ALL_PEER_IND:
        {
            tpSirTdlsDelAllPeerInd pSirTdlsDelAllPeerInd = (tpSirTdlsDelAllPeerInd) pMsgBuf ;
            tCsrRoamInfo roamInfo = {0} ;

            /* Sending the TEARDOWN indication to HDD. */
            csrRoamCallCallback(pMac, pSirTdlsDelAllPeerInd->sessionId, &roamInfo, 0,
                                eCSR_ROAM_TDLS_STATUS_UPDATE,
                                eCSR_ROAM_RESULT_DELETE_ALL_TDLS_PEER_IND);
            break ;
        }
        case eWNI_SME_MGMT_FRM_TX_COMPLETION_IND:
        {
            tpSirMgmtTxCompletionInd pSirTdlsDelAllPeerInd = (tpSirMgmtTxCompletionInd) pMsgBuf ;
            tCsrRoamInfo roamInfo = {0} ;
            roamInfo.reasonCode = pSirTdlsDelAllPeerInd->txCompleteStatus;

            csrRoamCallCallback(pMac, pSirTdlsDelAllPeerInd->sessionId, &roamInfo,
                                0, eCSR_ROAM_RESULT_MGMT_TX_COMPLETE_IND, 0);
            break;
        }
        case eWNI_SME_TDLS_LINK_ESTABLISH_RSP:
        {
            tSirTdlsLinkEstablishReqRsp *linkEstablishReqRsp = (tSirTdlsLinkEstablishReqRsp *) pMsgBuf ;
            tCsrRoamInfo roamInfo = {0} ;
#if 0
            vos_mem_copy(&roamInfo.peerMac, delStaRsp->peerMac,
                                         sizeof(tSirMacAddr)) ;
            roamInfo.staId = delStaRsp->staId ;
            roamInfo.statusCode = delStaRsp->statusCode ;
#endif
            csrRoamCallCallback(pMac, linkEstablishReqRsp->sessionId, &roamInfo, 0,
                         eCSR_ROAM_TDLS_STATUS_UPDATE,
                               eCSR_ROAM_RESULT_LINK_ESTABLISH_REQ_RSP);
            /* remove pending eSmeCommandTdlsLinkEstablish command */
            csrTdlsRemoveSmeCmd(pMac, eSmeCommandTdlsLinkEstablish);
            break;
        }

        case eWNI_SME_TDLS_CHANNEL_SWITCH_RSP:
        {
#if 0
            tSirTdlsChanSwitchReqRsp *ChanSwitchReqRsp = (tSirTdlsChanSwitchReqRsp *) pMsgBuf ;
            tCsrRoamInfo roamInfo = {0} ;
            vos_mem_copy(&roamInfo.peerMac, delStaRsp->peerMac,
                                         sizeof(tSirMacAddr)) ;
            roamInfo.staId = delStaRsp->staId ;
            roamInfo.statusCode = delStaRsp->statusCode ;
            csrRoamCallCallback(pMac, ChanSwitchReqRsp->sessionId, &roamInfo, 0,
                                eCSR_ROAM_TDLS_STATUS_UPDATE,
                                eCSR_ROAM_RESULT_LINK_ESTABLISH_REQ_RSP);
#endif
            /* remove pending eSmeCommandTdlsChanSwitch command */
            csrTdlsRemoveSmeCmd(pMac, eSmeCommandTdlsChannelSwitch);
            break;
        }
        default:
        {
            break ;
        }
    }
    
    return eHAL_STATUS_SUCCESS ;
}
#endif
