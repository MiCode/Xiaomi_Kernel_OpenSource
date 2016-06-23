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

  \file  smeApi.c

  \brief Definitions for SME APIs

   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.

   Qualcomm Confidential and Proprietary.

  ========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.



  when           who                 what, where, why
----------       ---                --------------------------------------------------------
06/03/10     js                     Added support to hostapd driven
 *                                  deauth/disassoc/mic failure

===========================================================================*/

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/


#include "smsDebug.h"
#include "sme_Api.h"
#include "csrInsideApi.h"
#include "smeInside.h"
#include "csrInternal.h"
#include "wlan_qct_wda.h"
#include "halMsgApi.h"
#include "vos_trace.h"
#include "sme_Trace.h"
#include "vos_types.h"
#include "vos_trace.h"
#include "sapApi.h"
#include "macTrace.h"


#ifdef DEBUG_ROAM_DELAY
#include "vos_utils.h"
#endif

extern tSirRetStatus uMacPostCtrlMsg(void* pSirGlobal, tSirMbMsg* pMb);

#include <wlan_qct_pal_api.h>
#define LOG_SIZE 256
#define READ_MEMORY_DUMP_CMD     9
#define TL_INIT_STATE            0


#define CSR_ACTIVE_LIST_CMD_TIMEOUT_VALUE 1000*30  //30s

// TxMB Functions
extern eHalStatus pmcPrepareCommand( tpAniSirGlobal pMac, eSmeCommandType cmdType, void *pvParam,
                            tANI_U32 size, tSmeCmd **ppCmd );
extern void pmcReleaseCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand );
extern void qosReleaseCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand );
extern void csrReleaseRocReqCommand( tpAniSirGlobal pMac);
extern eHalStatus p2pProcessRemainOnChannelCmd(tpAniSirGlobal pMac, tSmeCmd *p2pRemainonChn);
extern eHalStatus sme_remainOnChnRsp( tpAniSirGlobal pMac, tANI_U8 *pMsg);
extern eHalStatus sme_mgmtFrmInd( tHalHandle hHal, tpSirSmeMgmtFrameInd pSmeMgmtFrm);
extern eHalStatus sme_remainOnChnReady( tHalHandle hHal, tANI_U8* pMsg);
extern eHalStatus sme_sendActionCnf( tHalHandle hHal, tANI_U8* pMsg);
extern eHalStatus p2pProcessNoAReq(tpAniSirGlobal pMac, tSmeCmd *pNoACmd);

static eHalStatus initSmeCmdList(tpAniSirGlobal pMac);
static void smeAbortCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand, tANI_BOOLEAN fStopping );

eCsrPhyMode sme_GetPhyMode(tHalHandle hHal);

eHalStatus sme_HandleChangeCountryCode(tpAniSirGlobal pMac,  void *pMsgBuf);

void sme_DisconnectConnectedSessions(tpAniSirGlobal pMac);

eHalStatus sme_HandleGenericChangeCountryCode(tpAniSirGlobal pMac,  void *pMsgBuf);

eHalStatus sme_HandlePreChannelSwitchInd(tHalHandle hHal);

eHalStatus sme_HandlePostChannelSwitchInd(tHalHandle hHal);

#ifdef FEATURE_WLAN_LFR
tANI_BOOLEAN csrIsScanAllowed(tpAniSirGlobal pMac);
#endif

#ifdef WLAN_FEATURE_11W
eHalStatus sme_UnprotectedMgmtFrmInd( tHalHandle hHal,
                                      tpSirSmeUnprotMgmtFrameInd pSmeMgmtFrm );
#endif

//Internal SME APIs
eHalStatus sme_AcquireGlobalLock( tSmeStruct *psSme)
{
    eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;

    if(psSme)
    {
        if( VOS_IS_STATUS_SUCCESS( vos_lock_acquire( &psSme->lkSmeGlobalLock) ) )
        {
            status = eHAL_STATUS_SUCCESS;
        }
    }

    return (status);
}


eHalStatus sme_ReleaseGlobalLock( tSmeStruct *psSme)
{
    eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;

    if(psSme)
    {
        if( VOS_IS_STATUS_SUCCESS( vos_lock_release( &psSme->lkSmeGlobalLock) ) )
        {
            status = eHAL_STATUS_SUCCESS;
        }
    }

    return (status);
}



static eHalStatus initSmeCmdList(tpAniSirGlobal pMac)
{
    eHalStatus status;
    tSmeCmd *pCmd;
    tANI_U32 cmd_idx;
    VOS_STATUS vosStatus;
    vos_timer_t* cmdTimeoutTimer = NULL;

    pMac->sme.totalSmeCmd = SME_TOTAL_COMMAND;
    if (!HAL_STATUS_SUCCESS(status = csrLLOpen(pMac->hHdd,
                           &pMac->sme.smeCmdActiveList)))
       goto end;

    if (!HAL_STATUS_SUCCESS(status = csrLLOpen(pMac->hHdd,
                           &pMac->sme.smeCmdPendingList)))
       goto end;

    if (!HAL_STATUS_SUCCESS(status = csrLLOpen(pMac->hHdd,
                           &pMac->sme.smeScanCmdActiveList)))
       goto end;

    if (!HAL_STATUS_SUCCESS(status = csrLLOpen(pMac->hHdd,
                                            &pMac->sme.smeScanCmdPendingList)))
       goto end;

    if (!HAL_STATUS_SUCCESS(status = csrLLOpen(pMac->hHdd,
                                             &pMac->sme.smeCmdFreeList)))
       goto end;

    pCmd = vos_mem_malloc(sizeof(tSmeCmd) * pMac->sme.totalSmeCmd);
    if ( NULL == pCmd )
       status = eHAL_STATUS_FAILURE;
    else
    {
       status = eHAL_STATUS_SUCCESS;

       vos_mem_set(pCmd, sizeof(tSmeCmd) * pMac->sme.totalSmeCmd, 0);
       pMac->sme.pSmeCmdBufAddr = pCmd;

       for (cmd_idx = 0; cmd_idx < pMac->sme.totalSmeCmd; cmd_idx++)
       {
           csrLLInsertTail(&pMac->sme.smeCmdFreeList,
                        &pCmd[cmd_idx].Link, LL_ACCESS_LOCK);
       }
    }

    /* This timer is only to debug the active list command timeout */

    cmdTimeoutTimer = (vos_timer_t*)vos_mem_malloc(sizeof(vos_timer_t));
    if (cmdTimeoutTimer)
    {
        pMac->sme.smeCmdActiveList.cmdTimeoutTimer = cmdTimeoutTimer;
        vosStatus =
            vos_timer_init( pMac->sme.smeCmdActiveList.cmdTimeoutTimer,
                              VOS_TIMER_TYPE_SW,
                              activeListCmdTimeoutHandle,
                              (void*) pMac);

        if (!VOS_IS_STATUS_SUCCESS(vosStatus))
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                "Init Timer fail for active list command process time out");
            vos_mem_free(pMac->sme.smeCmdActiveList.cmdTimeoutTimer);
        }
        else
        {
            pMac->sme.smeCmdActiveList.cmdTimeoutDuration =
                CSR_ACTIVE_LIST_CMD_TIMEOUT_VALUE;
        }
    }
end:
    if (!HAL_STATUS_SUCCESS(status))
       smsLog(pMac, LOGE, "failed to initialize sme command list:%d\n",
              status);

    return (status);
}


void smeReleaseCommand(tpAniSirGlobal pMac, tSmeCmd *pCmd)
{
    pCmd->command = eSmeNoCommand;
    csrLLInsertTail(&pMac->sme.smeCmdFreeList, &pCmd->Link, LL_ACCESS_LOCK);
}



static void smeReleaseCmdList(tpAniSirGlobal pMac, tDblLinkList *pList)
{
    tListElem *pEntry;
    tSmeCmd *pCommand;

    while((pEntry = csrLLRemoveHead(pList, LL_ACCESS_LOCK)) != NULL)
    {
        //TODO: base on command type to call release functions
        //reinitialize different command types so they can be reused
        pCommand = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
        smeAbortCommand(pMac, pCommand, eANI_BOOLEAN_TRUE);
    }
}

static void purgeSmeCmdList(tpAniSirGlobal pMac)
{
    //release any out standing commands back to free command list
    smeReleaseCmdList(pMac, &pMac->sme.smeCmdPendingList);
    smeReleaseCmdList(pMac, &pMac->sme.smeCmdActiveList);
    smeReleaseCmdList(pMac, &pMac->sme.smeScanCmdPendingList);
    smeReleaseCmdList(pMac, &pMac->sme.smeScanCmdActiveList);
}

void purgeSmeSessionCmdList(tpAniSirGlobal pMac, tANI_U32 sessionId,
        tDblLinkList *pList)
{
    //release any out standing commands back to free command list
    tListElem *pEntry, *pNext;
    tSmeCmd *pCommand;
    tDblLinkList localList;

    vos_mem_zero(&localList, sizeof(tDblLinkList));
    if(!HAL_STATUS_SUCCESS(csrLLOpen(pMac->hHdd, &localList)))
    {
        smsLog(pMac, LOGE, FL(" failed to open list"));
        return;
    }

    csrLLLock(pList);
    pEntry = csrLLPeekHead(pList, LL_ACCESS_NOLOCK);
    while(pEntry != NULL)
    {
        pNext = csrLLNext(pList, pEntry, LL_ACCESS_NOLOCK);
        pCommand = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
        if(pCommand->sessionId == sessionId)
        {
            if(csrLLRemoveEntry(pList, pEntry, LL_ACCESS_NOLOCK))
            {
                csrLLInsertTail(&localList, pEntry, LL_ACCESS_NOLOCK);
            }
        }
        pEntry = pNext;
    }
    csrLLUnlock(pList);

    while( (pEntry = csrLLRemoveHead(&localList, LL_ACCESS_NOLOCK)) )
    {
        pCommand = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
        smeAbortCommand(pMac, pCommand, eANI_BOOLEAN_TRUE);
    }
    csrLLClose(&localList);
}


static eHalStatus freeSmeCmdList(tpAniSirGlobal pMac)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;

    purgeSmeCmdList(pMac);
    csrLLClose(&pMac->sme.smeCmdPendingList);
    csrLLClose(&pMac->sme.smeCmdActiveList);
    csrLLClose(&pMac->sme.smeScanCmdPendingList);
    csrLLClose(&pMac->sme.smeScanCmdActiveList);
    csrLLClose(&pMac->sme.smeCmdFreeList);

    /*destroy active list command time out timer */
    vos_timer_destroy(pMac->sme.smeCmdActiveList.cmdTimeoutTimer);
    vos_mem_free(pMac->sme.smeCmdActiveList.cmdTimeoutTimer);
    pMac->sme.smeCmdActiveList.cmdTimeoutTimer = NULL;

    status = vos_lock_acquire(&pMac->sme.lkSmeGlobalLock);
    if(status != eHAL_STATUS_SUCCESS)
    {
        smsLog(pMac, LOGE,
            FL("Failed to acquire the lock status = %d"), status);
        goto done;
    }

    if(NULL != pMac->sme.pSmeCmdBufAddr)
    {
        vos_mem_free(pMac->sme.pSmeCmdBufAddr);
        pMac->sme.pSmeCmdBufAddr = NULL;
    }

    status = vos_lock_release(&pMac->sme.lkSmeGlobalLock);
    if(status != eHAL_STATUS_SUCCESS)
    {
        smsLog(pMac, LOGE,
            FL("Failed to release the lock status = %d"), status);
    }
done:
    return (status);
}


void dumpCsrCommandInfo(tpAniSirGlobal pMac, tSmeCmd *pCmd)
{
    switch( pCmd->command )
    {
    case eSmeCommandScan:
        smsLog( pMac, LOGE, " scan command reason is %d", pCmd->u.scanCmd.reason );
        break;

    case eSmeCommandRoam:
        smsLog( pMac, LOGE, " roam command reason is %d", pCmd->u.roamCmd.roamReason );
        break;

    case eSmeCommandWmStatusChange:
        smsLog( pMac, LOGE, " WMStatusChange command type is %d", pCmd->u.wmStatusChangeCmd.Type );
        break;

    case eSmeCommandSetKey:
        smsLog( pMac, LOGE, " setKey command auth(%d) enc(%d)",
                        pCmd->u.setKeyCmd.authType, pCmd->u.setKeyCmd.encType );
        break;

    case eSmeCommandRemoveKey:
        smsLog( pMac, LOGE, " removeKey command auth(%d) enc(%d)",
                        pCmd->u.removeKeyCmd.authType, pCmd->u.removeKeyCmd.encType );
        break;

    default:
        smsLog( pMac, LOGE, " default: Unhandled command %d",
                pCmd->command);
        break;
    }
}

tSmeCmd *smeGetCommandBuffer( tpAniSirGlobal pMac )
{
    tSmeCmd *pRetCmd = NULL, *pTempCmd = NULL;
    tListElem *pEntry;
    static int smeCommandQueueFull = 0;
    pEntry = csrLLRemoveHead( &pMac->sme.smeCmdFreeList, LL_ACCESS_LOCK );

    // If we can get another MS Msg buffer, then we are ok.  Just link
    // the entry onto the linked list.  (We are using the linked list
    // to keep track of tfhe message buffers).
    if ( pEntry )
    {
        pRetCmd = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
        /* reset when free list is available */
            smeCommandQueueFull = 0;
    }
    else {
        int idx = 1;

        //Cannot change pRetCmd here since it needs to return later.
        pEntry = csrLLPeekHead( &pMac->sme.smeCmdActiveList, LL_ACCESS_LOCK );
        if( pEntry )
        {
           pTempCmd = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
        }
        smsLog( pMac, LOGE, "Out of command buffer.... command (0x%X) stuck",
           (pTempCmd) ? pTempCmd->command : eSmeNoCommand );
        if(pTempCmd)
        {
            if( eSmeCsrCommandMask & pTempCmd->command )
            {
                //CSR command is stuck. See what the reason code is for that command
                dumpCsrCommandInfo(pMac, pTempCmd);
            }
        } //if(pTempCmd)

        //dump what is in the pending queue
        csrLLLock(&pMac->sme.smeCmdPendingList);
        pEntry = csrLLPeekHead( &pMac->sme.smeCmdPendingList, LL_ACCESS_NOLOCK );
        while(pEntry && !smeCommandQueueFull)
        {
            pTempCmd = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
            /* Print only 1st five commands from pending queue. */
            if (idx <= 5)
            smsLog( pMac, LOGE, "Out of command buffer.... SME pending command #%d (0x%X)",
                    idx, pTempCmd->command );
            idx++;
            if( eSmeCsrCommandMask & pTempCmd->command )
            {
                //CSR command is stuck. See what the reason code is for that command
                dumpCsrCommandInfo(pMac, pTempCmd);
            }
            pEntry = csrLLNext( &pMac->sme.smeCmdPendingList, pEntry, LL_ACCESS_NOLOCK );
        }
        /* Increament static variable so that it prints pending command only once*/
        smeCommandQueueFull++;
        csrLLUnlock(&pMac->sme.smeCmdPendingList);

        //There may be some more command in CSR's own pending queue
        csrLLLock(&pMac->roam.roamCmdPendingList);
        pEntry = csrLLPeekHead( &pMac->roam.roamCmdPendingList, LL_ACCESS_NOLOCK );
        while(pEntry)
        {
            pTempCmd = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
            smsLog( pMac, LOGE, "Out of command buffer.... CSR pending command #%d (0x%X)",
                    idx++, pTempCmd->command );
            dumpCsrCommandInfo(pMac, pTempCmd);
            pEntry = csrLLNext( &pMac->roam.roamCmdPendingList, pEntry, LL_ACCESS_NOLOCK );
        }
        csrLLUnlock(&pMac->roam.roamCmdPendingList);
    }

    return( pRetCmd );
}


void smePushCommand( tpAniSirGlobal pMac, tSmeCmd *pCmd, tANI_BOOLEAN fHighPriority )
{
    if ( fHighPriority )
    {
        csrLLInsertHead( &pMac->sme.smeCmdPendingList, &pCmd->Link, LL_ACCESS_LOCK );
    }
    else
    {
        csrLLInsertTail( &pMac->sme.smeCmdPendingList, &pCmd->Link, LL_ACCESS_LOCK );
    }

    // process the command queue...
    smeProcessPendingQueue( pMac );

    return;
}


static eSmeCommandType smeIsFullPowerNeeded( tpAniSirGlobal pMac, tSmeCmd *pCommand )
{
    eSmeCommandType pmcCommand = eSmeNoCommand;
    tANI_BOOLEAN fFullPowerNeeded = eANI_BOOLEAN_FALSE;
    tPmcState pmcState;
    eHalStatus status;

    do
    {
        pmcState = pmcGetPmcState(pMac);

        status = csrIsFullPowerNeeded( pMac, pCommand, NULL, &fFullPowerNeeded );
        if( !HAL_STATUS_SUCCESS(status) )
        {
            //PMC state is not right for the command, drop it
            return ( eSmeDropCommand );
        }
        if( fFullPowerNeeded  ) break;
        fFullPowerNeeded = ( ( eSmeCommandAddTs == pCommand->command ) ||
                    ( eSmeCommandDelTs ==  pCommand->command ) );
        if( fFullPowerNeeded ) break;
#ifdef FEATURE_OEM_DATA_SUPPORT
        fFullPowerNeeded = (pmcState == IMPS &&
                                       eSmeCommandOemDataReq == pCommand->command);
        if(fFullPowerNeeded) break;
#endif
        fFullPowerNeeded = (pmcState == IMPS &&
                            eSmeCommandRemainOnChannel == pCommand->command);
        if(fFullPowerNeeded) break;
    } while(0);

    if( fFullPowerNeeded )
    {
        switch( pmcState )
        {
        case IMPS:
        case STANDBY:
            pmcCommand = eSmeCommandExitImps;
            break;

        case BMPS:
            pmcCommand = eSmeCommandExitBmps;
            break;

        case UAPSD:
            pmcCommand = eSmeCommandExitUapsd;
            break;

        case WOWL:
            pmcCommand = eSmeCommandExitWowl;
            break;

        default:
            break;
        }
    }

    return ( pmcCommand );
}


//For commands that need to do extra cleanup.
static void smeAbortCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand, tANI_BOOLEAN fStopping )
{
    if( eSmePmcCommandMask & pCommand->command )
    {
        pmcAbortCommand( pMac, pCommand, fStopping );
    }
    else if ( eSmeCsrCommandMask & pCommand->command )
    {
        csrAbortCommand( pMac, pCommand, fStopping );
    }
    else
    {
        switch( pCommand->command )
        {
            case eSmeCommandRemainOnChannel:
                if (NULL != pCommand->u.remainChlCmd.callback)
                {
                    remainOnChanCallback callback =
                                            pCommand->u.remainChlCmd.callback;
                    /* process the msg */
                    if( callback )
                    {
                        callback(pMac, pCommand->u.remainChlCmd.callbackCtx,
                                            eCSR_SCAN_ABORT );
                    }
                }
                smeReleaseCommand( pMac, pCommand );
                break;
            default:
                smeReleaseCommand( pMac, pCommand );
                break;
        }
    }
}

tListElem *csrGetCmdToProcess(tpAniSirGlobal pMac, tDblLinkList *pList,
                              tANI_U8 sessionId, tANI_BOOLEAN fInterlocked)
{
    tListElem *pCurEntry = NULL;
    tSmeCmd *pCommand;

    /* Go through the list and return the command whose session id is not
     * matching with the current ongoing scan cmd sessionId */
    pCurEntry = csrLLPeekHead( pList, LL_ACCESS_LOCK );
    while (pCurEntry)
    {
        pCommand = GET_BASE_ADDR(pCurEntry, tSmeCmd, Link);
        if (pCommand->sessionId != sessionId)
        {
            smsLog(pMac, LOG1, "selected the command with different sessionId");
            return pCurEntry;
        }

        pCurEntry = csrLLNext(pList, pCurEntry, fInterlocked);
    }

    smsLog(pMac, LOG1, "No command pending with different sessionId");
    return NULL;
}

tANI_BOOLEAN smeProcessScanQueue(tpAniSirGlobal pMac)
{
    tListElem *pEntry;
    tSmeCmd *pCommand;
    tListElem *pSmeEntry;
    tSmeCmd *pSmeCommand;
    tANI_BOOLEAN status = eANI_BOOLEAN_TRUE;

    csrLLLock( &pMac->sme.smeScanCmdActiveList );
    if (csrLLIsListEmpty( &pMac->sme.smeScanCmdActiveList,
                LL_ACCESS_NOLOCK ))
    {
        if (!csrLLIsListEmpty(&pMac->sme.smeScanCmdPendingList,
                    LL_ACCESS_LOCK))
        {
            pEntry = csrLLPeekHead( &pMac->sme.smeScanCmdPendingList,
                    LL_ACCESS_LOCK );
            if (pEntry)
            {
                pCommand = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
                //We cannot execute any command in wait-for-key state until setKey is through.
                if (CSR_IS_WAIT_FOR_KEY( pMac, pCommand->sessionId))
                {
                    if (!CSR_IS_SET_KEY_COMMAND(pCommand))
                    {
                        smsLog(pMac, LOGE,
                                "  Cannot process command(%d) while waiting for key",
                                pCommand->command);
                        status = eANI_BOOLEAN_FALSE;
                        goto end;
                    }
                }

                if ((!csrLLIsListEmpty(&pMac->sme.smeCmdActiveList,
                                       LL_ACCESS_LOCK )))
                {
                    pSmeEntry = csrLLPeekHead(&pMac->sme.smeCmdActiveList,
                                              LL_ACCESS_LOCK);
                    if (pEntry)
                    {
                       pSmeCommand = GET_BASE_ADDR(pEntry, tSmeCmd,
                                                   Link) ;

                       /* if scan is running on one interface and SME recei
                          ves the next command on the same interface then
                          dont the allow the command to be queued to
                          smeCmdPendingList. If next scan is allowed on
                          the same interface the CSR state machine will
                          get screwed up. */
                          if (pSmeCommand->sessionId == pCommand->sessionId)
                          {
                              status = eANI_BOOLEAN_FALSE;
                              goto end;
                          }
                    }
                }
                if ( csrLLRemoveEntry( &pMac->sme.smeScanCmdPendingList,
                            pEntry, LL_ACCESS_LOCK ) )
                {
                    csrLLInsertHead( &pMac->sme.smeScanCmdActiveList,
                            &pCommand->Link, LL_ACCESS_NOLOCK );

                    switch (pCommand->command)
                    {
                        case eSmeCommandScan:
                            smsLog(pMac, LOG1,
                                    " Processing scan offload command ");
                            csrProcessScanCommand( pMac, pCommand );
                            break;
                        default:
                            smsLog(pMac, LOGE,
                                    " Something wrong, wrong command enqueued"
                                    " to smeScanCmdPendingList");
                            pEntry = csrLLRemoveHead(
                                    &pMac->sme.smeScanCmdActiveList,
                                    LL_ACCESS_NOLOCK );
                            pCommand = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
                            smeReleaseCommand( pMac, pCommand );
                            break;
                    }
                }
            }
        }
    }
end:
    csrLLUnlock(&pMac->sme.smeScanCmdActiveList);
    return status;
}

tANI_BOOLEAN smeProcessCommand( tpAniSirGlobal pMac )
{
    tANI_BOOLEAN fContinue = eANI_BOOLEAN_FALSE;
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tListElem *pEntry;
    tSmeCmd *pCommand;
    tListElem *pSmeEntry;
    tSmeCmd *pSmeCommand;
    eSmeCommandType pmcCommand = eSmeNoCommand;

    // if the ActiveList is empty, then nothing is active so we can process a
    // pending command...
    //alwasy lock active list before locking pending list
    csrLLLock( &pMac->sme.smeCmdActiveList );
    if ( csrLLIsListEmpty( &pMac->sme.smeCmdActiveList, LL_ACCESS_NOLOCK ) )
    {
        if(!csrLLIsListEmpty(&pMac->sme.smeCmdPendingList, LL_ACCESS_LOCK))
        {
            /* If scan command is pending in the smeScanCmdActive list
             * then pick the command from smeCmdPendingList which is
             * not matching with the scan command session id.
             * At any point of time only one command will be allowed
             * on a single session. */
            if ((pMac->fScanOffload) &&
                    (!csrLLIsListEmpty(&pMac->sme.smeScanCmdActiveList,
                                       LL_ACCESS_LOCK)))
            {
                pSmeEntry = csrLLPeekHead(&pMac->sme.smeScanCmdActiveList,
                        LL_ACCESS_LOCK);
                if (pSmeEntry)
                {
                    pSmeCommand = GET_BASE_ADDR(pSmeEntry, tSmeCmd, Link);

                    pEntry = csrGetCmdToProcess(pMac,
                            &pMac->sme.smeCmdPendingList,
                            pSmeCommand->sessionId,
                            LL_ACCESS_LOCK);
                    goto sme_process_cmd;
                }
            }

            //Peek the command
            pEntry = csrLLPeekHead( &pMac->sme.smeCmdPendingList, LL_ACCESS_LOCK );
sme_process_cmd:
            if( pEntry )
            {
                pCommand = GET_BASE_ADDR( pEntry, tSmeCmd, Link );

                /* Allow only disconnect command
                 * in wait-for-key state until setKey is through.
                 */
                if( CSR_IS_WAIT_FOR_KEY( pMac, pCommand->sessionId ) &&
                    !CSR_IS_DISCONNECT_COMMAND( pCommand ) )
                {
                    if( !CSR_IS_SET_KEY_COMMAND( pCommand ) )
                    {
                        csrLLUnlock( &pMac->sme.smeCmdActiveList );
                        smsLog(pMac, LOGE, FL("SessionId %d:  Cannot process "
                               "command(%d) while waiting for key"),
                               pCommand->sessionId, pCommand->command);
                        fContinue = eANI_BOOLEAN_FALSE;
                        goto sme_process_scan_queue;
                    }
                }
                pmcCommand = smeIsFullPowerNeeded( pMac, pCommand );
                if( eSmeDropCommand == pmcCommand )
                {
                    //This command is not ok for current PMC state
                    if( csrLLRemoveEntry( &pMac->sme.smeCmdPendingList, pEntry, LL_ACCESS_LOCK ) )
                    {
                        smeAbortCommand( pMac, pCommand, eANI_BOOLEAN_FALSE );
                    }
                    csrLLUnlock( &pMac->sme.smeCmdActiveList );
                    //tell caller to continue
                    fContinue = eANI_BOOLEAN_TRUE;
                    goto sme_process_scan_queue;
                }
                else if( eSmeNoCommand != pmcCommand )
                {
                    tExitBmpsInfo exitBmpsInfo;
                    void *pv = NULL;
                    tANI_U32 size = 0;
                    tSmeCmd *pPmcCmd = NULL;

                    if( eSmeCommandExitBmps == pmcCommand )
                    {
                        exitBmpsInfo.exitBmpsReason = eSME_REASON_OTHER;
                        pv = (void *)&exitBmpsInfo;
                        size = sizeof(tExitBmpsInfo);
                    }
                    //pmcCommand has to be one of the exit power save command
                    status = pmcPrepareCommand( pMac, pmcCommand, pv, size, &pPmcCmd );
                    if( HAL_STATUS_SUCCESS( status ) && pPmcCmd )
                    {
                        //Force this command to wake up the chip
                        csrLLInsertHead( &pMac->sme.smeCmdActiveList, &pPmcCmd->Link, LL_ACCESS_NOLOCK );
                        csrLLUnlock( &pMac->sme.smeCmdActiveList );
                        fContinue = pmcProcessCommand( pMac, pPmcCmd );
                        if( fContinue )
                        {
                            //The command failed, remove it
                            if( csrLLRemoveEntry( &pMac->sme.smeCmdActiveList, &pPmcCmd->Link, LL_ACCESS_NOLOCK ) )
                            {
                                pmcReleaseCommand( pMac, pPmcCmd );
                            }
                        }
                    }
                    else
                    {
                        csrLLUnlock( &pMac->sme.smeCmdActiveList );
                        smsLog( pMac, LOGE, FL(  "Cannot issue command(0x%X) to wake up the chip. Status = %d"), pmcCommand, status );
                        //Let it retry
                        fContinue = eANI_BOOLEAN_TRUE;
                    }
                    goto sme_process_scan_queue;
                }
                if ( csrLLRemoveEntry( &pMac->sme.smeCmdPendingList, pEntry, LL_ACCESS_LOCK ) )
                {
                    // we can reuse the pCommand

                    // Insert the command onto the ActiveList...
                    csrLLInsertHead( &pMac->sme.smeCmdActiveList, &pCommand->Link, LL_ACCESS_NOLOCK );

                    if( pMac->deferImps )
                    {
                        /* IMPS timer is already running so stop it and
                         * it will get restarted when no command is pending
                         */
                        csrScanStopIdleScanTimer( pMac );
                        pMac->scan.fRestartIdleScan = eANI_BOOLEAN_TRUE;
                        pMac->deferImps = eANI_BOOLEAN_FALSE;
                    }

                    // .... and process the command.

                    MTRACE(vos_trace(VOS_MODULE_ID_SME,
                       TRACE_CODE_SME_COMMAND, pCommand->sessionId, pCommand->command));
                    switch ( pCommand->command )
                    {

                        case eSmeCommandScan:
                            csrLLUnlock( &pMac->sme.smeCmdActiveList );
                            status = csrProcessScanCommand( pMac, pCommand );
                            break;

                        case eSmeCommandRoam:
                            csrLLUnlock( &pMac->sme.smeCmdActiveList );
                            status  = csrRoamProcessCommand( pMac, pCommand );
                            if(!HAL_STATUS_SUCCESS(status))
                            {
                                if( csrLLRemoveEntry( &pMac->sme.smeCmdActiveList,
                                            &pCommand->Link, LL_ACCESS_LOCK ) )
                                {
                                    csrReleaseCommandRoam( pMac, pCommand );
                                }
                            }
                            break;

                        case eSmeCommandWmStatusChange:
                            csrLLUnlock( &pMac->sme.smeCmdActiveList );
                            csrRoamProcessWmStatusChangeCommand(pMac, pCommand);
                            break;

                        case eSmeCommandSetKey:
                            csrLLUnlock( &pMac->sme.smeCmdActiveList );
                            status = csrRoamProcessSetKeyCommand( pMac, pCommand );
                            if(!HAL_STATUS_SUCCESS(status))
                            {
                                if( csrLLRemoveEntry( &pMac->sme.smeCmdActiveList,
                                            &pCommand->Link, LL_ACCESS_LOCK ) )
                                {
                                    csrReleaseCommandSetKey( pMac, pCommand );
                                }
                            }
                            break;

                        case eSmeCommandRemoveKey:
                            csrLLUnlock( &pMac->sme.smeCmdActiveList );
                            status = csrRoamProcessRemoveKeyCommand( pMac, pCommand );
                            if(!HAL_STATUS_SUCCESS(status))
                            {
                                if( csrLLRemoveEntry( &pMac->sme.smeCmdActiveList,
                                            &pCommand->Link, LL_ACCESS_LOCK ) )
                                {
                                    csrReleaseCommandRemoveKey( pMac, pCommand );
                                }
                            }
                            break;

                        case eSmeCommandAddStaSession:
                            csrLLUnlock( &pMac->sme.smeCmdActiveList );
                            csrProcessAddStaSessionCommand( pMac, pCommand );
                            break;
                        case eSmeCommandDelStaSession:
                            csrLLUnlock( &pMac->sme.smeCmdActiveList );
                            csrProcessDelStaSessionCommand( pMac, pCommand );
                            break;

#ifdef FEATURE_OEM_DATA_SUPPORT
                        case eSmeCommandOemDataReq:
                            csrLLUnlock(&pMac->sme.smeCmdActiveList);
                            oemData_ProcessOemDataReqCommand(pMac, pCommand);
                            break;
#endif
                        case eSmeCommandRemainOnChannel:
                            csrLLUnlock(&pMac->sme.smeCmdActiveList);
                            p2pProcessRemainOnChannelCmd(pMac, pCommand);
                            break;
                        case eSmeCommandNoAUpdate:
                            csrLLUnlock( &pMac->sme.smeCmdActiveList );
                            p2pProcessNoAReq(pMac,pCommand);
                        case eSmeCommandEnterImps:
                        case eSmeCommandExitImps:
                        case eSmeCommandEnterBmps:
                        case eSmeCommandExitBmps:
                        case eSmeCommandEnterUapsd:
                        case eSmeCommandExitUapsd:
                        case eSmeCommandEnterWowl:
                        case eSmeCommandExitWowl:
                            csrLLUnlock( &pMac->sme.smeCmdActiveList );
                            fContinue = pmcProcessCommand( pMac, pCommand );
                            if( fContinue )
                            {
                                //The command failed, remove it
                                if( csrLLRemoveEntry( &pMac->sme.smeCmdActiveList,
                                            &pCommand->Link, LL_ACCESS_LOCK ) )
                                {
                                    pmcReleaseCommand( pMac, pCommand );
                                }
                            }
                            break;

                        //Treat standby differently here because caller may not be able to handle
                        //the failure so we do our best here
                        case eSmeCommandEnterStandby:
                            if( csrIsConnStateDisconnected( pMac, pCommand->sessionId ) )
                            {
                                //It can continue
                                csrLLUnlock( &pMac->sme.smeCmdActiveList );
                                fContinue = pmcProcessCommand( pMac, pCommand );
                                if( fContinue )
                                {
                                    //The command failed, remove it
                                    if( csrLLRemoveEntry( &pMac->sme.smeCmdActiveList,
                                                &pCommand->Link, LL_ACCESS_LOCK ) )
                                    {
                                        pmcReleaseCommand( pMac, pCommand );
                                    }
                                }
                            }
                            else
                            {
                                //Need to issue a disconnect first before processing this command
                                tSmeCmd *pNewCmd;

                                //We need to re-run the command
                                fContinue = eANI_BOOLEAN_TRUE;
                                //Pull off the standby command first
                                if( csrLLRemoveEntry( &pMac->sme.smeCmdActiveList,
                                                &pCommand->Link, LL_ACCESS_NOLOCK ) )
                                {
                                    csrLLUnlock( &pMac->sme.smeCmdActiveList );
                                    //Need to call CSR function here because the disconnect command
                                    //is handled by CSR
                                    pNewCmd = csrGetCommandBuffer( pMac );
                                    if( NULL != pNewCmd )
                                    {
                                        //Put the standby command to the head of the pending list first
                                        csrLLInsertHead( &pMac->sme.smeCmdPendingList, &pCommand->Link,
                                                        LL_ACCESS_LOCK );
                                        pNewCmd->command = eSmeCommandRoam;
                                        pNewCmd->u.roamCmd.roamReason = eCsrForcedDisassoc;
                                        //Put the disassoc command before the standby command
                                        csrLLInsertHead( &pMac->sme.smeCmdPendingList, &pNewCmd->Link,
                                                        LL_ACCESS_LOCK );
                                    }
                                    else
                                    {
                                        //Continue the command here
                                        fContinue = pmcProcessCommand( pMac, pCommand );
                                        if( fContinue )
                                        {
                                            //The command failed, remove it
                                            if( csrLLRemoveEntry( &pMac->sme.smeCmdActiveList,
                                                        &pCommand->Link, LL_ACCESS_LOCK ) )
                                            {
                                                pmcReleaseCommand( pMac, pCommand );
                                            }
                                        }
                                    }
                                }
                                else
                                {
                                    csrLLUnlock( &pMac->sme.smeCmdActiveList );
                                    smsLog( pMac, LOGE, FL(" failed to remove standby command") );
                                    VOS_ASSERT(0);
                                }
                            }
                            break;

                        case eSmeCommandAddTs:
                        case eSmeCommandDelTs:
                            csrLLUnlock( &pMac->sme.smeCmdActiveList );
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
                            fContinue = qosProcessCommand( pMac, pCommand );
                            if( fContinue )
                            {
                                //The command failed, remove it
                                if( csrLLRemoveEntry( &pMac->sme.smeCmdActiveList,
                                            &pCommand->Link, LL_ACCESS_NOLOCK ) )
                                {
//#ifndef WLAN_MDM_CODE_REDUCTION_OPT
                                    qosReleaseCommand( pMac, pCommand );
//#endif /* WLAN_MDM_CODE_REDUCTION_OPT*/
                                }
                            }
#endif
                            break;
#ifdef FEATURE_WLAN_TDLS
                        case eSmeCommandTdlsSendMgmt:
                        case eSmeCommandTdlsAddPeer:
                        case eSmeCommandTdlsDelPeer:
                        case eSmeCommandTdlsLinkEstablish:
                        case eSmeCommandTdlsChannelSwitch: // tdlsoffchan
#ifdef FEATURE_WLAN_TDLS_INTERNAL
                        case eSmeCommandTdlsDiscovery:
                        case eSmeCommandTdlsLinkSetup:
                        case eSmeCommandTdlsLinkTear:
                        case eSmeCommandTdlsEnterUapsd:
                        case eSmeCommandTdlsExitUapsd:
#endif
                            {
                                VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                                        "sending TDLS Command 0x%x to PE", pCommand->command);

                                csrLLUnlock( &pMac->sme.smeCmdActiveList );
                                status = csrTdlsProcessCmd( pMac, pCommand );
                            }
                            break ;
#endif

                        default:
                            //something is wrong
                            //remove it from the active list
                            smsLog(pMac, LOGE, " csrProcessCommand processes an unknown command %d", pCommand->command);
                            pEntry = csrLLRemoveHead( &pMac->sme.smeCmdActiveList, LL_ACCESS_NOLOCK );
                            csrLLUnlock( &pMac->sme.smeCmdActiveList );
                            pCommand = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
                            smeReleaseCommand( pMac, pCommand );
                            status = eHAL_STATUS_FAILURE;
                            break;
                    }
                    if(!HAL_STATUS_SUCCESS(status))
                    {
                        fContinue = eANI_BOOLEAN_TRUE;
                    }
                }//if(pEntry)
                else
                {
                    //This is odd. Some one else pull off the command.
                    csrLLUnlock( &pMac->sme.smeCmdActiveList );
                }
            }
            else
            {
                csrLLUnlock( &pMac->sme.smeCmdActiveList );
            }
        }
        else
        {
            //No command waiting
            csrLLUnlock( &pMac->sme.smeCmdActiveList );
            //This is only used to restart an idle mode scan, it means at least one other idle scan has finished.
            if(pMac->scan.fRestartIdleScan && eANI_BOOLEAN_FALSE == pMac->scan.fCancelIdleScan)
            {
                tANI_U32 nTime = 0;

                pMac->scan.fRestartIdleScan = eANI_BOOLEAN_FALSE;
                if(!HAL_STATUS_SUCCESS(csrScanTriggerIdleScan(pMac, &nTime)))
                {
                    csrScanStartIdleScanTimer(pMac, nTime);
                }
            }
        }
    }
    else {
        csrLLUnlock( &pMac->sme.smeCmdActiveList );
    }

sme_process_scan_queue:
    if (pMac->fScanOffload && !(smeProcessScanQueue(pMac)))
        fContinue = eANI_BOOLEAN_FALSE;

    return ( fContinue );
}

void smeProcessPendingQueue( tpAniSirGlobal pMac )
{
    while( smeProcessCommand( pMac ) );
}


tANI_BOOLEAN smeCommandPending(tpAniSirGlobal pMac)
{
    return ( !csrLLIsListEmpty( &pMac->sme.smeCmdActiveList, LL_ACCESS_NOLOCK ) ||
        !csrLLIsListEmpty(&pMac->sme.smeCmdPendingList, LL_ACCESS_NOLOCK) );
}



//Global APIs

/*--------------------------------------------------------------------------

  \brief sme_Open() - Initialze all SME modules and put them at idle state

  The function initializes each module inside SME, PMC, CCM, CSR, etc. . Upon
  successfully return, all modules are at idle state ready to start.

  smeOpen must be called before any other SME APIs can be involved.
  smeOpen must be called after macOpen.
  This is a synchronous call
  \param hHal - The handle returned by macOpen.

  \return eHAL_STATUS_SUCCESS - SME is successfully initialized.

          Other status means SME is failed to be initialized
  \sa

  --------------------------------------------------------------------------*/
eHalStatus sme_Open(tHalHandle hHal)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   do {
      pMac->sme.state = SME_STATE_STOP;
      pMac->sme.currDeviceMode = VOS_STA_MODE;
      if( !VOS_IS_STATUS_SUCCESS( vos_lock_init( &pMac->sme.lkSmeGlobalLock ) ) )
      {
          smsLog( pMac, LOGE, "sme_Open failed init lock" );
          status = eHAL_STATUS_FAILURE;
          break;
      }

      status = ccmOpen(hHal);
      if ( ! HAL_STATUS_SUCCESS( status ) ) {
         smsLog( pMac, LOGE,
                 "ccmOpen failed during initialization with status=%d", status );
         break;
      }

      status = csrOpen(pMac);
      if ( ! HAL_STATUS_SUCCESS( status ) ) {
         smsLog( pMac, LOGE,
                 "csrOpen failed during initialization with status=%d", status );
         break;
      }

      status = pmcOpen(hHal);
      if ( ! HAL_STATUS_SUCCESS( status ) ) {
         smsLog( pMac, LOGE,
                 "pmcOpen failed during initialization with status=%d", status );
         break;
      }

#ifdef FEATURE_WLAN_TDLS
      pMac->isTdlsPowerSaveProhibited = 0;
#endif

#ifndef WLAN_MDM_CODE_REDUCTION_OPT
      status = sme_QosOpen(pMac);
      if ( ! HAL_STATUS_SUCCESS( status ) ) {
         smsLog( pMac, LOGE,
                 "Qos open failed during initialization with status=%d", status );
         break;
      }

      status = btcOpen(pMac);
      if ( ! HAL_STATUS_SUCCESS( status ) ) {
         smsLog( pMac, LOGE,
                 "btcOpen open failed during initialization with status=%d", status );
         break;
      }
#endif
#ifdef FEATURE_OEM_DATA_SUPPORT
      status = oemData_OemDataReqOpen(pMac);
      if ( ! HAL_STATUS_SUCCESS( status ) ) {
         smsLog(pMac, LOGE,
                "oemData_OemDataReqOpen failed during initialization with status=%d", status );
         break;
      }
#endif

      if(!HAL_STATUS_SUCCESS((status = initSmeCmdList(pMac))))
          break;

      {
         v_PVOID_t pvosGCtx = vos_get_global_context(VOS_MODULE_ID_SAP, NULL);
         if ( NULL == pvosGCtx ){
            smsLog( pMac, LOGE, "WLANSAP_Open open failed during initialization");
            status = eHAL_STATUS_FAILURE;
            break;
         }

         status = WLANSAP_Open( pvosGCtx );
         if ( ! HAL_STATUS_SUCCESS( status ) ) {
             smsLog( pMac, LOGE,
                     "WLANSAP_Open open failed during initialization with status=%d", status );
             break;
         }
      }
#if defined WLAN_FEATURE_VOWIFI
      status = rrmOpen(pMac);
      if ( ! HAL_STATUS_SUCCESS( status ) ) {
         smsLog( pMac, LOGE,
                 "rrmOpen open failed during initialization with status=%d", status );
         break;
      }
#endif

#if defined WLAN_FEATURE_VOWIFI_11R
      sme_FTOpen(pMac);
#endif
      sme_p2pOpen(pMac);
      smeTraceInit(pMac);

   }while (0);

   return status;
}

/*--------------------------------------------------------------------------

  \brief sme_set11dinfo() - Set the 11d information about valid channels
   and there power using information from nvRAM
   This function is called only for AP.

  This is a synchronous call

  \param hHal - The handle returned by macOpen.
  \Param pSmeConfigParams - a pointer to a caller allocated object of
  typedef struct _smeConfigParams.

  \return eHAL_STATUS_SUCCESS - SME update the config parameters successfully.

          Other status means SME is failed to update the config parameters.
  \sa
--------------------------------------------------------------------------*/

eHalStatus sme_set11dinfo(tHalHandle hHal,  tpSmeConfigParams pSmeConfigParams)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
                      TRACE_CODE_SME_RX_HDD_MSG_SET_11DINFO, NO_SESSION, 0));
   if (NULL == pSmeConfigParams ) {
      smsLog( pMac, LOGE,
              "Empty config param structure for SME, nothing to update");
      return status;
   }

   status = csrSetChannels(hHal, &pSmeConfigParams->csrConfig );
   if ( ! HAL_STATUS_SUCCESS( status ) ) {
      smsLog( pMac, LOGE, "csrChangeDefaultConfigParam failed with status=%d",
              status );
   }
    return status;
}

/*--------------------------------------------------------------------------

  \brief sme_getSoftApDomain() - Get the current regulatory domain of softAp.

  This is a synchronous call

  \param hHal - The handle returned by HostapdAdapter.
  \Param v_REGDOMAIN_t - The current Regulatory Domain requested for SoftAp.

  \return eHAL_STATUS_SUCCESS - SME successfully completed the request.

          Other status means, failed to get the current regulatory domain.
  \sa
--------------------------------------------------------------------------*/

eHalStatus sme_getSoftApDomain(tHalHandle hHal,  v_REGDOMAIN_t *domainIdSoftAp)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
             TRACE_CODE_SME_RX_HDD_MSG_GET_SOFTAP_DOMAIN, NO_SESSION, 0));
   if (NULL == domainIdSoftAp ) {
      smsLog( pMac, LOGE, "Uninitialized domain Id");
      return status;
   }

   *domainIdSoftAp = pMac->scan.domainIdCurrent;
   status = eHAL_STATUS_SUCCESS;

   return status;
}


eHalStatus sme_setRegInfo(tHalHandle hHal,  tANI_U8 *apCntryCode)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
             TRACE_CODE_SME_RX_HDD_MSG_SET_REGINFO, NO_SESSION, 0));
   if (NULL == apCntryCode ) {
      smsLog( pMac, LOGE, "Empty Country Code, nothing to update");
      return status;
   }

   status = csrSetRegInfo(hHal, apCntryCode );
   if ( ! HAL_STATUS_SUCCESS( status ) ) {
      smsLog( pMac, LOGE, "csrSetRegInfo failed with status=%d",
              status );
   }
    return status;
}

#ifdef FEATURE_WLAN_SCAN_PNO
/*--------------------------------------------------------------------------

  \brief sme_UpdateChannelConfig() - Update channel configuration in RIVA.

  It is used at driver start up to inform RIVA of the default channel
  configuration.

  This is a synchronous call

  \param hHal - The handle returned by macOpen.

  \return eHAL_STATUS_SUCCESS - SME update the channel config successfully.

          Other status means SME is failed to update the channel config.
  \sa

  --------------------------------------------------------------------------*/
eHalStatus sme_UpdateChannelConfig(tHalHandle hHal)
{
  tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

  MTRACE(vos_trace(VOS_MODULE_ID_SME,
           TRACE_CODE_SME_RX_HDD_MSG_UPDATE_CHANNEL_CONFIG, NO_SESSION, 0));
  pmcUpdateScanParams( pMac, &(pMac->roam.configParam),
                      &pMac->scan.base20MHzChannels, FALSE);
  return eHAL_STATUS_SUCCESS;
}
#endif // FEATURE_WLAN_SCAN_PNLO

eHalStatus sme_UpdateChannelList(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status = eHAL_STATUS_SUCCESS;

    status = csrUpdateChannelList(pMac);

    if (eHAL_STATUS_SUCCESS != status)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                "failed to update the supported channel list");
    }
    return status;
}

/*--------------------------------------------------------------------------

  \brief sme_UpdateConfig() - Change configurations for all SME moduels

  The function updates some configuration for modules in SME, CCM, CSR, etc
  during SMEs close open sequence.

  Modules inside SME apply the new configuration at the next transaction.

  This is a synchronous call

  \param hHal - The handle returned by macOpen.
  \Param pSmeConfigParams - a pointer to a caller allocated object of
  typedef struct _smeConfigParams.

  \return eHAL_STATUS_SUCCESS - SME update the config parameters successfully.

          Other status means SME is failed to update the config parameters.
  \sa

  --------------------------------------------------------------------------*/
eHalStatus sme_UpdateConfig(tHalHandle hHal, tpSmeConfigParams pSmeConfigParams)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
                   TRACE_CODE_SME_RX_HDD_MSG_UPDATE_CONFIG, NO_SESSION, 0));
   if (NULL == pSmeConfigParams ) {
      smsLog( pMac, LOGE,
              "Empty config param structure for SME, nothing to update");
      return status;
   }

   status = csrChangeDefaultConfigParam(pMac, &pSmeConfigParams->csrConfig);

   if ( ! HAL_STATUS_SUCCESS( status ) ) {
      smsLog( pMac, LOGE, "csrChangeDefaultConfigParam failed with status=%d",
              status );
   }
#if defined WLAN_FEATURE_P2P_INTERNAL
   status = p2pChangeDefaultConfigParam(pMac, &pSmeConfigParams->p2pConfig);

   if ( ! HAL_STATUS_SUCCESS( status ) ) {
      smsLog( pMac, LOGE, "p2pChangeDefaultConfigParam failed with status=%d",
              status );
   }
#endif
#if defined WLAN_FEATURE_VOWIFI
   status = rrmChangeDefaultConfigParam(hHal, &pSmeConfigParams->rrmConfig);

   if ( ! HAL_STATUS_SUCCESS( status ) ) {
      smsLog( pMac, LOGE, "rrmChangeDefaultConfigParam failed with status=%d",
              status );
   }
#endif
   //For SOC, CFG is set before start
   //We don't want to apply global CFG in connect state because that may cause some side affect
   if(
      csrIsAllSessionDisconnected( pMac) )
   {
       csrSetGlobalCfgs(pMac);
   }

   /* update the directed scan offload setting */
   pMac->fScanOffload = pSmeConfigParams->fScanOffload;

   /* Enable channel bonding mode in 2.4GHz */
   if ((pSmeConfigParams->csrConfig.channelBondingMode24GHz == TRUE) &&
       (IS_HT40_OBSS_SCAN_FEATURE_ENABLE))
   {
      ccmCfgSetInt(hHal,WNI_CFG_CHANNEL_BONDING_24G,
                 eANI_BOOLEAN_TRUE, NULL,eANI_BOOLEAN_FALSE);
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Setting channelBondingMode24GHz:%d " ,
                pSmeConfigParams->csrConfig.channelBondingMode24GHz);
   }
   if (pMac->fScanOffload)
   {
       /* If scan offload is enabled then lim has allow the sending of
       scan request to firmware even in powersave mode. The firmware has
       to take care of exiting from power save mode */
       status = ccmCfgSetInt(hHal, WNI_CFG_SCAN_IN_POWERSAVE,
                   eANI_BOOLEAN_TRUE, NULL, eANI_BOOLEAN_FALSE);

       if (eHAL_STATUS_SUCCESS != status)
       {
           VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                   "Could not pass on WNI_CFG_SCAN_IN_POWERSAVE to CCM");
       }
   }
   pMac->isCoalesingInIBSSAllowed =
         pSmeConfigParams->csrConfig.isCoalesingInIBSSAllowed;
   pMac->fEnableDebugLog = pSmeConfigParams->fEnableDebugLog;
   pMac->fDeferIMPSTime = pSmeConfigParams->fDeferIMPSTime;

   return status;
}

#ifdef WLAN_FEATURE_GTK_OFFLOAD
void sme_ProcessGetGtkInfoRsp( tHalHandle hHal,
                            tpSirGtkOffloadGetInfoRspParams pGtkOffloadGetInfoRsp)
{
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   if (NULL == pMac)
   {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL,
           "%s: pMac is null", __func__);
       return ;
   }
   if (pMac->pmc.GtkOffloadGetInfoCB == NULL)
   {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
           "%s: HDD callback is null", __func__);
       return ;
   }
   pMac->pmc.GtkOffloadGetInfoCB(pMac->pmc.GtkOffloadGetInfoCBContext,
                                 pGtkOffloadGetInfoRsp);
}
#endif

/* ---------------------------------------------------------------------------
    \fn sme_ChangeConfigParams
    \brief The SME API exposed for HDD to provide config params to SME during
    SMEs stop -> start sequence.

    If HDD changed the domain that will cause a reset. This function will
    provide the new set of 11d information for the new domain. Currrently this
    API provides info regarding 11d only at reset but we can extend this for
    other params (PMC, QoS) which needs to be initialized again at reset.

    This is a synchronous call

    \param hHal - The handle returned by macOpen.

    \Param
    pUpdateConfigParam - a pointer to a structure (tCsrUpdateConfigParam) that
                currently provides 11d related information like Country code,
                Regulatory domain, valid channel list, Tx power per channel, a
                list with active/passive scan allowed per valid channel.

    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_ChangeConfigParams(tHalHandle hHal,
                                 tCsrUpdateConfigParam *pUpdateConfigParam)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   if (NULL == pUpdateConfigParam ) {
      smsLog( pMac, LOGE,
              "Empty config param structure for SME, nothing to reset");
      return status;
   }

   status = csrChangeConfigParams(pMac, pUpdateConfigParam);

   if ( ! HAL_STATUS_SUCCESS( status ) ) {
      smsLog( pMac, LOGE, "csrUpdateConfigParam failed with status=%d",
              status );
   }

   return status;

}

/*--------------------------------------------------------------------------

  \brief sme_HDDReadyInd() - SME sends eWNI_SME_SYS_READY_IND to PE to inform
  that the NIC is ready tio run.

  The function is called by HDD at the end of initialization stage so PE/HAL can
  enable the NIC to running state.

  This is a synchronous call
  \param hHal - The handle returned by macOpen.

  \return eHAL_STATUS_SUCCESS - eWNI_SME_SYS_READY_IND is sent to PE
                                successfully.

          Other status means SME failed to send the message to PE.
  \sa

  --------------------------------------------------------------------------*/
eHalStatus sme_HDDReadyInd(tHalHandle hHal)
{
   tSirSmeReadyReq Msg;
   eHalStatus status = eHAL_STATUS_FAILURE;
   tPmcPowerState powerState;
   tPmcSwitchState hwWlanSwitchState;
   tPmcSwitchState swWlanSwitchState;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
                  TRACE_CODE_SME_RX_HDD_MSG_HDDREADYIND, NO_SESSION, 0));
   do
   {

      Msg.messageType = eWNI_SME_SYS_READY_IND;
      Msg.length      = sizeof( tSirSmeReadyReq );

      if (eSIR_FAILURE != uMacPostCtrlMsg( hHal, (tSirMbMsg*)&Msg ))
      {
         status = eHAL_STATUS_SUCCESS;
      }
      else
      {
         smsLog( pMac, LOGE,
                 "uMacPostCtrlMsg failed to send eWNI_SME_SYS_READY_IND");
         break;
      }

      status = pmcQueryPowerState( hHal, &powerState,
                                &hwWlanSwitchState, &swWlanSwitchState );
      if ( ! HAL_STATUS_SUCCESS( status ) )
      {
         smsLog( pMac, LOGE, "pmcQueryPowerState failed with status=%d",
                 status );
         break;
      }

      if ( (ePMC_SWITCH_OFF != hwWlanSwitchState) &&
           (ePMC_SWITCH_OFF != swWlanSwitchState) )
      {
         status = csrReady(pMac);
         if ( ! HAL_STATUS_SUCCESS( status ) )
         {
            smsLog( pMac, LOGE, "csrReady failed with status=%d", status );
            break;
         }
         status = pmcReady(hHal);
         if ( ! HAL_STATUS_SUCCESS( status ) )
         {
             smsLog( pMac, LOGE, "pmcReady failed with status=%d", status );
             break;
         }
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
         if(VOS_STATUS_SUCCESS != btcReady(hHal))
         {
             status = eHAL_STATUS_FAILURE;
             smsLog( pMac, LOGE, "btcReady failed");
             break;
         }
#endif

#if defined WLAN_FEATURE_VOWIFI
         if(VOS_STATUS_SUCCESS != rrmReady(hHal))
         {
             status = eHAL_STATUS_FAILURE;
             smsLog( pMac, LOGE, "rrmReady failed");
             break;
         }
#endif
      }
      pMac->sme.state = SME_STATE_READY;
   } while( 0 );

   return status;
}

/*--------------------------------------------------------------------------

  \brief sme_Start() - Put all SME modules at ready state.

  The function starts each module in SME, PMC, CCM, CSR, etc. . Upon
  successfully return, all modules are ready to run.
  This is a synchronous call
  \param hHal - The handle returned by macOpen.

  \return eHAL_STATUS_SUCCESS - SME is ready.

          Other status means SME is failed to start
  \sa

  --------------------------------------------------------------------------*/
eHalStatus sme_Start(tHalHandle hHal)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   do
   {
      status = csrStart(pMac);
      if ( ! HAL_STATUS_SUCCESS( status ) ) {
         smsLog( pMac, LOGE, "csrStart failed during smeStart with status=%d",
                 status );
         break;
      }

      status = pmcStart(hHal);
      if ( ! HAL_STATUS_SUCCESS( status ) ) {
         smsLog( pMac, LOGE, "pmcStart failed during smeStart with status=%d",
                 status );
         break;
      }

      status = WLANSAP_Start(vos_get_global_context(VOS_MODULE_ID_SAP, NULL));
      if ( ! HAL_STATUS_SUCCESS( status ) ) {
         smsLog( pMac, LOGE, "WLANSAP_Start failed during smeStart with status=%d",
                 status );
         break;
      }
      pMac->sme.state = SME_STATE_START;
   }while (0);

   return status;
}


#ifdef WLAN_FEATURE_PACKET_FILTERING
/******************************************************************************
*
* Name: sme_PCFilterMatchCountResponseHandler
*
* Description:
*    Invoke Packet Coalescing Filter Match Count callback routine
*
* Parameters:
*    hHal - HAL handle for device
*    pMsg - Pointer to tRcvFltPktMatchRsp structure
*
* Returns: eHalStatus
*
******************************************************************************/
eHalStatus sme_PCFilterMatchCountResponseHandler(tHalHandle hHal, void* pMsg)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpSirRcvFltPktMatchRsp pRcvFltPktMatchRsp = (tpSirRcvFltPktMatchRsp)pMsg;

    if (NULL == pMsg)
    {
        smsLog(pMac, LOGE, "in %s msg ptr is NULL", __func__);
        status = eHAL_STATUS_FAILURE;
    }
    else
    {
        smsLog(pMac, LOG2, "SME: entering "
            "sme_FilterMatchCountResponseHandler");

        /* Call Packet Coalescing Filter Match Count callback routine. */
        if (pMac->pmc.FilterMatchCountCB != NULL)
           pMac->pmc.FilterMatchCountCB(pMac->pmc.FilterMatchCountCBContext,
                                          pRcvFltPktMatchRsp);

        smsLog(pMac, LOG1, "%s: status=0x%x", __func__,
               pRcvFltPktMatchRsp->status);

        pMac->pmc.FilterMatchCountCB = NULL;
        pMac->pmc.FilterMatchCountCBContext = NULL;
    }

    return(status);
}
#endif // WLAN_FEATURE_PACKET_FILTERING


#ifdef WLAN_FEATURE_11W
/*------------------------------------------------------------------
 *
 * Handle the unprotected management frame indication from LIM and
 * forward it to HDD.
 *
 *------------------------------------------------------------------*/

eHalStatus sme_UnprotectedMgmtFrmInd( tHalHandle hHal,
                                      tpSirSmeUnprotMgmtFrameInd pSmeMgmtFrm)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus  status = eHAL_STATUS_SUCCESS;
    tCsrRoamInfo pRoamInfo = {0};
    tANI_U32 SessionId = pSmeMgmtFrm->sessionId;

    pRoamInfo.nFrameLength = pSmeMgmtFrm->frameLen;
    pRoamInfo.pbFrames = pSmeMgmtFrm->frameBuf;
    pRoamInfo.frameType = pSmeMgmtFrm->frameType;

    /* forward the mgmt frame to HDD */
    csrRoamCallCallback(pMac, SessionId, &pRoamInfo, 0, eCSR_ROAM_UNPROT_MGMT_FRAME_IND, 0);

    return status;
}
#endif

#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
/*------------------------------------------------------------------
 *
 * Handle the tsm ie indication from  LIM and forward it to HDD.
 *
 *------------------------------------------------------------------*/

eHalStatus sme_TsmIeInd(tHalHandle hHal, tSirSmeTsmIEInd *pSmeTsmIeInd)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus     status = eHAL_STATUS_SUCCESS;
    tCsrRoamInfo   pRoamInfo = {0};
    tANI_U32       SessionId = pSmeTsmIeInd->sessionId;

    pRoamInfo.tsmIe.tsid= pSmeTsmIeInd->tsmIe.tsid;
    pRoamInfo.tsmIe.state= pSmeTsmIeInd->tsmIe.state;
    pRoamInfo.tsmIe.msmt_interval= pSmeTsmIeInd->tsmIe.msmt_interval;

    /* forward the tsm ie information to HDD */
    csrRoamCallCallback(pMac, SessionId, &pRoamInfo, 0, eCSR_ROAM_TSM_IE_IND, 0);

    return status;
}

/* ---------------------------------------------------------------------------
    \fn sme_SetCCKMIe
    \brief  function to store the CCKM IE passed from supplicant and use it while packing
    reassociation request
    \param  hHal - HAL handle for device
    \param  pCckmIe - pointer to CCKM IE data
    \param  pCckmIeLen - length of the CCKM IE
    \- return Success or failure
    -------------------------------------------------------------------------*/
eHalStatus sme_SetCCKMIe(tHalHandle hHal, tANI_U8 sessionId,
                              tANI_U8 *pCckmIe, tANI_U8 cckmIeLen)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus     status  = eHAL_STATUS_SUCCESS;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        csrSetCCKMIe(pMac, sessionId, pCckmIe, cckmIeLen);
        sme_ReleaseGlobalLock( &pMac->sme );
    }
    return status;
}

/* ---------------------------------------------------------------------------
    \fn sme_SetEseBeaconRequest
    \brief  function to set Ese beacon request parameters
    \param  hHal       - HAL handle for device
    \param  sessionId  - Session id
    \param  pEseBcnReq - pointer to Ese beacon request
    \- return Success or failure
    -------------------------------------------------------------------------*/
eHalStatus sme_SetEseBeaconRequest(tHalHandle hHal, const tANI_U8 sessionId,
                                   const tCsrEseBeaconReq* pEseBcnReq)
{
   eHalStatus               status           = eSIR_SUCCESS;
   tpAniSirGlobal           pMac             = PMAC_STRUCT( hHal );
   tpSirBeaconReportReqInd  pSmeBcnReportReq = NULL;
   tCsrEseBeaconReqParams  *pBeaconReq       = NULL;
   tANI_U8                  counter          = 0;
   tCsrRoamSession         *pSession         = CSR_GET_SESSION(pMac, sessionId);
   tpRrmSMEContext          pSmeRrmContext   = &pMac->rrm.rrmSmeContext;

   /* Store the info in RRM context */
   vos_mem_copy(&pSmeRrmContext->eseBcnReqInfo, pEseBcnReq, sizeof(tCsrEseBeaconReq));

   //Prepare the request to send to SME.
   pSmeBcnReportReq = vos_mem_malloc(sizeof( tSirBeaconReportReqInd ));
   if(NULL == pSmeBcnReportReq)
   {
      smsLog(pMac, LOGP, "Memory Allocation Failure!!! Ese  BcnReq Ind to SME");
      return eSIR_FAILURE;
   }

   smsLog(pMac, LOGE, "Sending Beacon Report Req to SME");
   vos_mem_zero( pSmeBcnReportReq, sizeof( tSirBeaconReportReqInd ));

   pSmeBcnReportReq->messageType = eWNI_SME_BEACON_REPORT_REQ_IND;
   pSmeBcnReportReq->length = sizeof( tSirBeaconReportReqInd );
   vos_mem_copy( pSmeBcnReportReq->bssId, pSession->connectedProfile.bssid, sizeof(tSirMacAddr) );
   pSmeBcnReportReq->channelInfo.channelNum = 255;
   pSmeBcnReportReq->channelList.numChannels = pEseBcnReq->numBcnReqIe;
   pSmeBcnReportReq->msgSource = eRRM_MSG_SOURCE_ESE_UPLOAD;

   for (counter = 0; counter < pEseBcnReq->numBcnReqIe; counter++)
   {
        pBeaconReq = (tCsrEseBeaconReqParams *)&pEseBcnReq->bcnReq[counter];
        pSmeBcnReportReq->fMeasurementtype[counter] = pBeaconReq->scanMode;
        pSmeBcnReportReq->measurementDuration[counter] = SYS_TU_TO_MS(pBeaconReq->measurementDuration);
        pSmeBcnReportReq->channelList.channelNumber[counter] = pBeaconReq->channel;
   }

   sme_RrmProcessBeaconReportReqInd(pMac, pSmeBcnReportReq);
   return status;
}

#endif /* FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */


/* ---------------------------------------------------------------------------
    \fn sme_getBcnMissRate
    \brief function sends 'WDA_GET_BCN_MISS_RATE_REQ' to WDA layer,
    \param  hHal - HAL handle for device.
    \param  sessionId - session ID.
    \- return Success or Failure.
    -------------------------------------------------------------------------*/

eHalStatus sme_getBcnMissRate(tHalHandle hHal, tANI_U8 sessionId, void *callback, void *data)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    VOS_STATUS vosStatus = VOS_STATUS_E_FAILURE;
    vos_msg_t vosMessage;
    tSirBcnMissRateReq *pMsg;
    tCsrRoamSession *pSession;

    if ( eHAL_STATUS_SUCCESS ==  sme_AcquireGlobalLock( &pMac->sme  ) )
    {
        pSession = CSR_GET_SESSION( pMac, sessionId );

        if (!pSession)
        {
            smsLog(pMac, LOGE, FL("session %d not found"), sessionId);
            sme_ReleaseGlobalLock( &pMac->sme );
            return eHAL_STATUS_FAILURE;
        }

        pMsg = (tSirBcnMissRateReq *) vos_mem_malloc(sizeof(tSirBcnMissRateReq));
        if (NULL == pMsg)
        {
            smsLog(pMac, LOGE, FL("failed to allocated memory"));
            sme_ReleaseGlobalLock( &pMac->sme );
            return eHAL_STATUS_FAILURE;
        }

        vos_mem_copy(pMsg->bssid, pSession->connectedProfile.bssid,
                 sizeof(tSirMacAddr));

        pMsg->msgLen   = sizeof(tSirBcnMissRateReq);
        pMsg->callback = callback;
        pMsg->data     = data;

        vosMessage.type = WDA_GET_BCN_MISS_RATE_REQ;
        vosMessage.bodyptr = pMsg;
        vosMessage.reserved = 0;
        vosStatus = vos_mq_post_message( VOS_MQ_ID_WDA, &vosMessage );
        if ( !VOS_IS_STATUS_SUCCESS(vosStatus) )
        {
           VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                     "%s: Post Set TM Level MSG fail", __func__);
           vos_mem_free(pMsg);
           sme_ReleaseGlobalLock( &pMac->sme );
           return eHAL_STATUS_FAILURE;
        }
        sme_ReleaseGlobalLock( &pMac->sme);
        return eHAL_STATUS_SUCCESS;
    }
    return eHAL_STATUS_FAILURE;
}

eHalStatus sme_EncryptMsgResponseHandler(tHalHandle hHal,
                                      tpSirEncryptedDataRspParams pEncRspParams)
{
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   if (NULL == pMac)
   {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL,
           "%s: pMac is null", __func__);
       return eHAL_STATUS_FAILURE;
   }
   if (pMac->sme.pEncMsgInfoParams.pEncMsgCbk == NULL)
   {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
           "%s: HDD callback is null", __func__);
       return eHAL_STATUS_FAILURE;
   }
   pMac->sme.pEncMsgInfoParams.pEncMsgCbk(pMac->sme.pEncMsgInfoParams.pUserData,
                                        &pEncRspParams->encryptedDataRsp);
   return eHAL_STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------

  \brief sme_ProcessMsg() - The main message processor for SME.

  The function is called by a message dispatcher when to process a message
  targeted for SME.

  This is a synchronous call
  \param hHal - The handle returned by macOpen.
  \param pMsg - A pointer to a caller allocated object of tSirMsgQ.

  \return eHAL_STATUS_SUCCESS - SME successfully process the message.

          Other status means SME failed to process the message to HAL.
  \sa

  --------------------------------------------------------------------------*/
eHalStatus sme_ProcessMsg(tHalHandle hHal, vos_msg_t* pMsg)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   if (pMsg == NULL) {
      smsLog( pMac, LOGE, "Empty message for SME, nothing to process");
      return status;
   }

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
      if( SME_IS_START(pMac) )
      {
          switch (pMsg->type) { // TODO: Will be modified to do a range check for msgs instead of having cases for each msgs
          case eWNI_PMC_ENTER_BMPS_RSP:
          case eWNI_PMC_EXIT_BMPS_RSP:
          case eWNI_PMC_EXIT_BMPS_IND:
          case eWNI_PMC_ENTER_IMPS_RSP:
          case eWNI_PMC_EXIT_IMPS_RSP:
          case eWNI_PMC_SMPS_STATE_IND:
          case eWNI_PMC_ENTER_UAPSD_RSP:
          case eWNI_PMC_EXIT_UAPSD_RSP:
          case eWNI_PMC_ENTER_WOWL_RSP:
          case eWNI_PMC_EXIT_WOWL_RSP:
             //PMC
             if (pMsg->bodyptr)
             {
                pmcMessageProcessor(hHal, pMsg->bodyptr);
                status = eHAL_STATUS_SUCCESS;
                vos_mem_free(pMsg->bodyptr);
             } else {
                smsLog( pMac, LOGE, "Empty rsp message for PMC, nothing to process");
             }
             break;

          case WNI_CFG_SET_CNF:
          case WNI_CFG_DNLD_CNF:
          case WNI_CFG_GET_RSP:
          case WNI_CFG_ADD_GRP_ADDR_CNF:
          case WNI_CFG_DEL_GRP_ADDR_CNF:
             //CCM
             if (pMsg->bodyptr)
             {
                ccmCfgCnfMsgHandler(hHal, pMsg->bodyptr);
                status = eHAL_STATUS_SUCCESS;
                vos_mem_free(pMsg->bodyptr);
             } else {
                smsLog( pMac, LOGE, "Empty rsp message for CCM, nothing to process");
             }
             break;

          case eWNI_SME_ADDTS_RSP:
          case eWNI_SME_DELTS_RSP:
          case eWNI_SME_DELTS_IND:
#ifdef WLAN_FEATURE_VOWIFI_11R
          case eWNI_SME_FT_AGGR_QOS_RSP:
#endif
             //QoS
             if (pMsg->bodyptr)
             {
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
                status = sme_QosMsgProcessor(pMac, pMsg->type, pMsg->bodyptr);
                vos_mem_free(pMsg->bodyptr);
#endif
             } else {
                smsLog( pMac, LOGE, "Empty rsp message for QoS, nothing to process");
             }
             break;
#if defined WLAN_FEATURE_VOWIFI
          case eWNI_SME_NEIGHBOR_REPORT_IND:
          case eWNI_SME_BEACON_REPORT_REQ_IND:
#if defined WLAN_VOWIFI_DEBUG
             smsLog( pMac, LOGE, "Received RRM message. Message Id = %d", pMsg->type );
#endif
             if ( pMsg->bodyptr )
             {
                status = sme_RrmMsgProcessor( pMac, pMsg->type, pMsg->bodyptr );
                vos_mem_free(pMsg->bodyptr);
             }
             else
             {
                smsLog( pMac, LOGE, "Empty message for RRM, nothing to process");
             }
             break;
#endif

#ifdef FEATURE_OEM_DATA_SUPPORT
          //Handle the eWNI_SME_OEM_DATA_RSP:
          case eWNI_SME_OEM_DATA_RSP:
                if(pMsg->bodyptr)
                {
                        status = sme_HandleOemDataRsp(pMac, pMsg->bodyptr);
                        vos_mem_free(pMsg->bodyptr);
                }
                else
                {
                        smsLog( pMac, LOGE, "Empty rsp message for oemData_ (eWNI_SME_OEM_DATA_RSP), nothing to process");
                }
                smeProcessPendingQueue( pMac );
                break;
#endif

          case eWNI_SME_ADD_STA_SELF_RSP:
                if(pMsg->bodyptr)
                {
                   status = csrProcessAddStaSessionRsp(pMac, pMsg->bodyptr);
                   vos_mem_free(pMsg->bodyptr);
                }
                else
                {
                   smsLog( pMac, LOGE, "Empty rsp message for meas (eWNI_SME_ADD_STA_SELF_RSP), nothing to process");
                }
                break;
          case eWNI_SME_DEL_STA_SELF_RSP:
                if(pMsg->bodyptr)
                {
                   status = csrProcessDelStaSessionRsp(pMac, pMsg->bodyptr);
                   vos_mem_free(pMsg->bodyptr);
                }
                else
                {
                   smsLog( pMac, LOGE, "Empty rsp message for meas (eWNI_SME_DEL_STA_SELF_RSP), nothing to process");
                }
                break;
          case eWNI_SME_REMAIN_ON_CHN_RSP:
                if(pMsg->bodyptr)
                {
                    status = sme_remainOnChnRsp(pMac, pMsg->bodyptr);
                    vos_mem_free(pMsg->bodyptr);
                }
                else
                {
                    smsLog( pMac, LOGE, "Empty rsp message for meas (eWNI_SME_REMAIN_ON_CHN_RSP), nothing to process");
                }
                break;
          case eWNI_SME_REMAIN_ON_CHN_RDY_IND:
                if(pMsg->bodyptr)
                {
                    status = sme_remainOnChnReady(pMac, pMsg->bodyptr);
                    vos_mem_free(pMsg->bodyptr);
                }
                else
                {
                    smsLog( pMac, LOGE, "Empty rsp message for meas (eWNI_SME_REMAIN_ON_CHN_RDY_IND), nothing to process");
                }
                break;
           case eWNI_SME_MGMT_FRM_IND:
                if(pMsg->bodyptr)
                {
                    sme_mgmtFrmInd(pMac, pMsg->bodyptr);
                    vos_mem_free(pMsg->bodyptr);
                }
                else
                {
                    smsLog( pMac, LOGE, "Empty rsp message for meas (eWNI_SME_MGMT_FRM_IND), nothing to process");
                }
                break;
           case eWNI_SME_ACTION_FRAME_SEND_CNF:
                if(pMsg->bodyptr)
                {
                    status = sme_sendActionCnf(pMac, pMsg->bodyptr);
                    vos_mem_free(pMsg->bodyptr);
                }
                else
                {
                    smsLog( pMac, LOGE, "Empty rsp message for meas (eWNI_SME_ACTION_FRAME_SEND_CNF), nothing to process");
                }
                break;
          case eWNI_SME_COEX_IND:
                if(pMsg->bodyptr)
                {
                   tSirSmeCoexInd *pSmeCoexInd = (tSirSmeCoexInd *)pMsg->bodyptr;

                   if (pSmeCoexInd->coexIndType == SIR_COEX_IND_TYPE_DISABLE_AGGREGATION_IN_2p4)
                   {
                       smsLog( pMac, LOG1, FL("SIR_COEX_IND_TYPE_DISABLE_AGGREGATION_IN_2p4"));
                       sme_RequestFullPower(hHal, NULL, NULL, eSME_REASON_OTHER);
                       pMac->isCoexScoIndSet = 1;
                   }
                   else if (pSmeCoexInd->coexIndType == SIR_COEX_IND_TYPE_ENABLE_AGGREGATION_IN_2p4)
                   {
                       smsLog( pMac, LOG1, FL("SIR_COEX_IND_TYPE_ENABLE_AGGREGATION_IN_2p4"));
                       pMac->isCoexScoIndSet = 0;
                       sme_RequestBmps(hHal, NULL, NULL);
                   }

                   status = btcHandleCoexInd((void *)pMac, pMsg->bodyptr);
                   vos_mem_free(pMsg->bodyptr);
                }
                else
                {
                   smsLog(pMac, LOGE, "Empty rsp message for meas (eWNI_SME_COEX_IND), nothing to process");
                }
                break;

#ifdef FEATURE_WLAN_SCAN_PNO
          case eWNI_SME_PREF_NETWORK_FOUND_IND:
                if(pMsg->bodyptr)
                {
                   status = sme_PreferredNetworkFoundInd((void *)pMac, pMsg->bodyptr);
                   vos_mem_free(pMsg->bodyptr);
                }
                else
                {
                   smsLog(pMac, LOGE, "Empty rsp message for meas (eWNI_SME_PREF_NETWORK_FOUND_IND), nothing to process");
                }
                break;
#endif // FEATURE_WLAN_SCAN_PNO

          case eWNI_SME_TX_PER_HIT_IND:
                if (pMac->sme.pTxPerHitCallback)
                {
                   pMac->sme.pTxPerHitCallback(pMac->sme.pTxPerHitCbContext);
                }
                break;

          case eWNI_SME_CHANGE_COUNTRY_CODE:
              if(pMsg->bodyptr)
                {
                   status = sme_HandleChangeCountryCode((void *)pMac, pMsg->bodyptr);
                   vos_mem_free(pMsg->bodyptr);
                }
                else
                {
                    smsLog(pMac, LOGE, "Empty rsp message for message (eWNI_SME_CHANGE_COUNTRY_CODE), nothing to process");
                }
                break;

          case eWNI_SME_GENERIC_CHANGE_COUNTRY_CODE:
              if (pMsg->bodyptr)
                {
                    status = sme_HandleGenericChangeCountryCode((void *)pMac, pMsg->bodyptr);
                    vos_mem_free(pMsg->bodyptr);
                }
                else
                {
                   smsLog(pMac, LOGE, "Empty rsp message for message (eWNI_SME_GENERIC_CHANGE_COUNTRY_CODE), nothing to process");
                }
                break;

#ifdef WLAN_FEATURE_PACKET_FILTERING
          case eWNI_PMC_PACKET_COALESCING_FILTER_MATCH_COUNT_RSP:
                if(pMsg->bodyptr)
                {
                   status = sme_PCFilterMatchCountResponseHandler((void *)pMac, pMsg->bodyptr);
                   vos_mem_free(pMsg->bodyptr);
                }
                else
                {
                   smsLog(pMac, LOGE, "Empty rsp message for meas "
                          "(PACKET_COALESCING_FILTER_MATCH_COUNT_RSP), nothing to process");
                }
                break;
#endif // WLAN_FEATURE_PACKET_FILTERING
          case eWNI_SME_PRE_SWITCH_CHL_IND:
             {
                status = sme_HandlePreChannelSwitchInd(pMac);
                break;
             }

          case eWNI_SME_POST_SWITCH_CHL_IND:
             {
                status = sme_HandlePostChannelSwitchInd(pMac);
                break;
             }

#ifdef WLAN_WAKEUP_EVENTS
          case eWNI_SME_WAKE_REASON_IND:
                if(pMsg->bodyptr)
                {
                   status = sme_WakeReasonIndCallback((void *)pMac, pMsg->bodyptr);
                   vos_mem_free(pMsg->bodyptr);
                }
                else
                {
                   smsLog(pMac, LOGE, "Empty rsp message for meas (eWNI_SME_WAKE_REASON_IND), nothing to process");
                }
                break;
#endif // WLAN_WAKEUP_EVENTS

#ifdef FEATURE_WLAN_TDLS
          /*
           * command rescived from PE, SME tdls msg processor shall be called
           * to process commands recieved from PE
           */
          case eWNI_SME_TDLS_SEND_MGMT_RSP:
          case eWNI_SME_TDLS_ADD_STA_RSP:
          case eWNI_SME_TDLS_DEL_STA_RSP:
          case eWNI_SME_TDLS_DEL_STA_IND:
          case eWNI_SME_TDLS_DEL_ALL_PEER_IND:
          case eWNI_SME_MGMT_FRM_TX_COMPLETION_IND:
          case eWNI_SME_TDLS_LINK_ESTABLISH_RSP:
          case eWNI_SME_TDLS_CHANNEL_SWITCH_RSP:
#ifdef FEATURE_WLAN_TDLS_INTERNAL
          case eWNI_SME_TDLS_DISCOVERY_START_RSP:
          case eWNI_SME_TDLS_DISCOVERY_START_IND:
          case eWNI_SME_TDLS_LINK_START_RSP:
          case eWNI_SME_TDLS_LINK_START_IND:
          case eWNI_SME_TDLS_TEARDOWN_RSP:
          case eWNI_SME_TDLS_TEARDOWN_IND:
          case eWNI_SME_ADD_TDLS_PEER_IND:
          case eWNI_SME_DELETE_TDLS_PEER_IND:
#endif
                {
                    if (pMsg->bodyptr)
                    {
                        status = tdlsMsgProcessor(pMac, pMsg->type, pMsg->bodyptr);
                        vos_mem_free(pMsg->bodyptr);
                    }
                    else
                    {
                        smsLog( pMac, LOGE, "Empty rsp message for TDLS, \
                                nothing to process");
                    }
                    break;
                }
#endif

#ifdef WLAN_FEATURE_11W
           case eWNI_SME_UNPROT_MGMT_FRM_IND:
                if (pMsg->bodyptr)
                {
                    sme_UnprotectedMgmtFrmInd(pMac, pMsg->bodyptr);
                    vos_mem_free(pMsg->bodyptr);
                }
                else
                {
                    smsLog(pMac, LOGE, "Empty rsp message for meas (eWNI_SME_UNPROT_MGMT_FRM_IND), nothing to process");
                }
                break;
#endif
#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
           case eWNI_SME_TSM_IE_IND:
              {
                if (pMsg->bodyptr)
                {
                    sme_TsmIeInd(pMac, pMsg->bodyptr);
                    vos_mem_free(pMsg->bodyptr);
                }
                else
                {
                    smsLog(pMac, LOGE, "Empty rsp message for (eWNI_SME_TSM_IE_IND), nothing to process");
                }
                break;
              }
#endif /* FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
          case eWNI_SME_ROAM_SCAN_OFFLOAD_RSP:
                status = csrRoamOffloadScanRspHdlr((void *)pMac, pMsg->bodyval);
                break;
#endif // WLAN_FEATURE_ROAM_SCAN_OFFLOAD

#ifdef WLAN_FEATURE_GTK_OFFLOAD
           case eWNI_PMC_GTK_OFFLOAD_GETINFO_RSP:
                if (pMsg->bodyptr)
                {
                    sme_ProcessGetGtkInfoRsp(pMac, pMsg->bodyptr);
                    vos_mem_zero(pMsg->bodyptr,
                                  sizeof(tSirGtkOffloadGetInfoRspParams));
                    vos_mem_free(pMsg->bodyptr);
                }
                else
                {
                    smsLog(pMac, LOGE, "Empty rsp message for (eWNI_PMC_GTK_OFFLOAD_GETINFO_RSP), nothing to process");
                }
                break ;
#endif

#ifdef FEATURE_WLAN_LPHB
          /* LPHB timeout indication arrived, send IND to client */
          case eWNI_SME_LPHB_IND:
                if (pMac->sme.pLphbIndCb)
                {
                   pMac->sme.pLphbIndCb(pMac->pAdapter, pMsg->bodyptr);
                }
                vos_mem_free(pMsg->bodyptr);

                break;
#endif /* FEATURE_WLAN_LPHB */

#ifdef FEATURE_WLAN_CH_AVOID
          /* LPHB timeout indication arrived, send IND to client */
          case eWNI_SME_CH_AVOID_IND:
                if (pMac->sme.pChAvoidNotificationCb)
                {
                   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                             "%s: CH avoid notification", __func__);
                   pMac->sme.pChAvoidNotificationCb(pMac->pAdapter, pMsg->bodyptr);
                }
                vos_mem_free(pMsg->bodyptr);

                break;
#endif /* FEATURE_WLAN_CH_AVOID */

          case eWNI_SME_ENCRYPT_MSG_RSP:
              if (pMsg->bodyptr)
              {
                  sme_EncryptMsgResponseHandler(pMac, pMsg->bodyptr);
                  vos_mem_free(pMsg->bodyptr);
              }
              else
              {
                  smsLog(pMac, LOGE,
                         "Empty rsp message for (eWNI_SME_ENCRYPT_MSG_RSP),"
                         " nothing to process");
              }
              break ;


          default:

             if ( ( pMsg->type >= eWNI_SME_MSG_TYPES_BEGIN )
                  &&  ( pMsg->type <= eWNI_SME_MSG_TYPES_END ) )
             {
                //CSR
                if (pMsg->bodyptr)
                {
                   status = csrMsgProcessor(hHal, pMsg->bodyptr);
                   vos_mem_free(pMsg->bodyptr);
                }
                else
                {
                   smsLog( pMac, LOGE, "Empty rsp message for CSR, nothing to process");
                }
             }
             else
             {
                smsLog( pMac, LOGW, "Unknown message type %d, nothing to process",
                        pMsg->type);
                if (pMsg->bodyptr)
                {
                   vos_mem_free(pMsg->bodyptr);
                }
             }
          }//switch
      } //SME_IS_START
      else
      {
         smsLog( pMac, LOGW, "message type %d in stop state ignored", pMsg->type);
         if (pMsg->bodyptr)
         {
            vos_mem_free(pMsg->bodyptr);
         }
      }
      sme_ReleaseGlobalLock( &pMac->sme );
   }
   else
   {
      smsLog( pMac, LOGW, "Locking failed, bailing out");
      if (pMsg->bodyptr)
      {
          vos_mem_free(pMsg->bodyptr);
      }
   }

   return status;
}


//No need to hold the global lock here because this function can only be called
//after sme_Stop.
v_VOID_t sme_FreeMsg( tHalHandle hHal, vos_msg_t* pMsg )
{
   if( pMsg )
   {
      if (pMsg->bodyptr)
      {
         vos_mem_free(pMsg->bodyptr);
      }
   }

}


/*--------------------------------------------------------------------------

  \brief sme_Stop() - Stop all SME modules and put them at idle state

  The function stops each module in SME, PMC, CCM, CSR, etc. . Upon
  return, all modules are at idle state ready to start.

  This is a synchronous call
  \param hHal - The handle returned by macOpen
  \param tHalStopType - reason for stopping

  \return eHAL_STATUS_SUCCESS - SME is stopped.

          Other status means SME is failed to stop but caller should still
          consider SME is stopped.
  \sa

  --------------------------------------------------------------------------*/
eHalStatus sme_Stop(tHalHandle hHal, tHalStopType stopType)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   eHalStatus fail_status = eHAL_STATUS_SUCCESS;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = WLANSAP_Stop(vos_get_global_context(VOS_MODULE_ID_SAP, NULL));
   if ( ! HAL_STATUS_SUCCESS( status ) ) {
      smsLog( pMac, LOGE, "WLANSAP_Stop failed during smeStop with status=%d",
                          status );
      fail_status = status;
   }

   p2pStop(hHal);

   status = pmcStop(hHal);
   if ( ! HAL_STATUS_SUCCESS( status ) ) {
      smsLog( pMac, LOGE, "pmcStop failed during smeStop with status=%d",
              status );
      fail_status = status;
   }

   status = csrStop(pMac, stopType);
   if ( ! HAL_STATUS_SUCCESS( status ) ) {
      smsLog( pMac, LOGE, "csrStop failed during smeStop with status=%d",
              status );
      fail_status = status;
   }

   ccmStop(hHal);

   purgeSmeCmdList(pMac);

   if (!HAL_STATUS_SUCCESS( fail_status )) {
      status = fail_status;
   }

   pMac->sme.state = SME_STATE_STOP;

   return status;
}

/*--------------------------------------------------------------------------

  \brief sme_Close() - Release all SME modules and their resources.

  The function release each module in SME, PMC, CCM, CSR, etc. . Upon
  return, all modules are at closed state.

  No SME APIs can be involved after smeClose except smeOpen.
  smeClose must be called before macClose.
  This is a synchronous call
  \param hHal - The handle returned by macOpen

  \return eHAL_STATUS_SUCCESS - SME is successfully close.

          Other status means SME is failed to be closed but caller still cannot
          call any other SME functions except smeOpen.
  \sa

  --------------------------------------------------------------------------*/
eHalStatus sme_Close(tHalHandle hHal)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   eHalStatus fail_status = eHAL_STATUS_SUCCESS;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = csrClose(pMac);
   if ( ! HAL_STATUS_SUCCESS( status ) ) {
      smsLog( pMac, LOGE, "csrClose failed during sme close with status=%d",
              status );
      fail_status = status;
   }

   status = WLANSAP_Close(vos_get_global_context(VOS_MODULE_ID_SAP, NULL));
   if ( ! HAL_STATUS_SUCCESS( status ) ) {
      smsLog( pMac, LOGE, "WLANSAP_close failed during sme close with status=%d",
              status );
      fail_status = status;
   }

#ifndef WLAN_MDM_CODE_REDUCTION_OPT
   status = btcClose(hHal);
   if ( ! HAL_STATUS_SUCCESS( status ) ) {
      smsLog( pMac, LOGE, "BTC close failed during sme close with status=%d",
              status );
      fail_status = status;
   }

   status = sme_QosClose(pMac);
   if ( ! HAL_STATUS_SUCCESS( status ) ) {
      smsLog( pMac, LOGE, "Qos close failed during sme close with status=%d",
              status );
      fail_status = status;
   }
#endif
#ifdef FEATURE_OEM_DATA_SUPPORT
   status = oemData_OemDataReqClose(hHal);
   if ( ! HAL_STATUS_SUCCESS( status ) ) {
       smsLog( pMac, LOGE, "OEM DATA REQ close failed during sme close with status=%d",
              status );
      fail_status = status;
   }
#endif

   status = ccmClose(hHal);
         if ( ! HAL_STATUS_SUCCESS( status ) ) {
      smsLog( pMac, LOGE, "ccmClose failed during sme close with status=%d",
                 status );
             fail_status = status;
         }

   status = pmcClose(hHal);
   if ( ! HAL_STATUS_SUCCESS( status ) ) {
      smsLog( pMac, LOGE, "pmcClose failed during sme close with status=%d",
              status );
      fail_status = status;
   }
#if defined WLAN_FEATURE_VOWIFI
   status = rrmClose(hHal);
   if ( ! HAL_STATUS_SUCCESS( status ) ) {
      smsLog( pMac, LOGE, "RRM close failed during sme close with status=%d",
              status );
      fail_status = status;
   }
#endif

#if defined WLAN_FEATURE_VOWIFI_11R
   sme_FTClose(hHal);
#endif
   sme_p2pClose(hHal);

   freeSmeCmdList(pMac);

   if( !VOS_IS_STATUS_SUCCESS( vos_lock_destroy( &pMac->sme.lkSmeGlobalLock ) ) )
   {
       fail_status = eHAL_STATUS_FAILURE;
   }

   if (!HAL_STATUS_SUCCESS( fail_status )) {
      status = fail_status;
   }

   pMac->sme.state = SME_STATE_STOP;

   return status;
}
#ifdef FEATURE_WLAN_LFR
tANI_BOOLEAN csrIsScanAllowed(tpAniSirGlobal pMac)
{
#if 0
        switch(pMac->roam.neighborRoamInfo.neighborRoamState) {
                case eCSR_NEIGHBOR_ROAM_STATE_REPORT_SCAN:
                case eCSR_NEIGHBOR_ROAM_STATE_PREAUTHENTICATING:
                case eCSR_NEIGHBOR_ROAM_STATE_PREAUTH_DONE:
                case eCSR_NEIGHBOR_ROAM_STATE_REASSOCIATING:
                        return eANI_BOOLEAN_FALSE;
                default:
                        return eANI_BOOLEAN_TRUE;
        }
#else
        /*
         * TODO: always return TRUE for now until
         * we figure out why we could be stuck in
         * one of the roaming states forever.
         */
        return eANI_BOOLEAN_TRUE;
#endif
}
#endif

/* ---------------------------------------------------------------------------
    \fn sco_isScanAllowed
    \brief check for scan interface connection status
    \param pMac     - Pointer to the global MAC parameter structure
    \param pScanReq - scan request structure.

    \return tANI_BOOLEAN TRUE to allow scan otherwise FALSE
  ---------------------------------------------------------------------------*/
tANI_BOOLEAN sco_isScanAllowed(tpAniSirGlobal pMac, tCsrScanRequest *pscanReq)
{
    tANI_BOOLEAN ret;

    if (pscanReq->p2pSearch)
        ret = csrIsP2pSessionConnected(pMac);
    else
        ret = csrIsStaSessionConnected(pMac);

    return !ret;
}

/* ---------------------------------------------------------------------------
    \fn sme_ScanRequest
    \brief a wrapper function to Request a 11d or full scan from CSR.
    This is an asynchronous call
    \param pScanRequestID - pointer to an object to get back the request ID
    \param callback - a callback function that scan calls upon finish, will not
                      be called if csrScanRequest returns error
    \param pContext - a pointer passed in for the callback
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_ScanRequest(tHalHandle hHal, tANI_U8 sessionId, tCsrScanRequest *pscanReq,
                           tANI_U32 *pScanRequestID,
                           csrScanCompleteCallback callback, void *pContext)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    MTRACE(vos_trace(VOS_MODULE_ID_SME,
           TRACE_CODE_SME_RX_HDD_MSG_SCAN_REQ, sessionId, pscanReq->scanType));
    smsLog(pMac, LOG2, FL("enter"));
    do
    {
        if(pMac->scan.fScanEnable &&
           (pMac->isCoexScoIndSet ? sco_isScanAllowed(pMac, pscanReq) : TRUE))
        {
            status = sme_AcquireGlobalLock( &pMac->sme );
            if ( HAL_STATUS_SUCCESS( status ) )
            {
                {
#ifdef FEATURE_WLAN_LFR
                    if(csrIsScanAllowed(pMac))
                    {
#endif
                        status = csrScanRequest( hHal, sessionId, pscanReq,
                                                 pScanRequestID, callback, pContext );
                        if ( !HAL_STATUS_SUCCESS( status ) )
                        {
                            smsLog(pMac, LOGE, FL("csrScanRequest failed"
                                    " SId=%d"), sessionId);
                        }
#ifdef FEATURE_WLAN_LFR
                    }
                    else
                    {
                        smsLog(pMac, LOGE, FL("Scan denied in state %s"
                               "(sub-state %s)"),
                               macTraceGetNeighbourRoamState(
                               pMac->roam.neighborRoamInfo.neighborRoamState),
                               macTraceGetcsrRoamSubState(
                               pMac->roam.curSubState[sessionId]));
                        /*HandOff is in progress. So schedule this scan later*/
                        status = eHAL_STATUS_RESOURCES;
                    }
#endif
                }

                sme_ReleaseGlobalLock( &pMac->sme );
            } //sme_AcquireGlobalLock success
            else
            {
                smsLog(pMac, LOGE, FL("sme_AcquireGlobalLock failed"));
            }
        } //if(pMac->scan.fScanEnable)
        else
        {
            smsLog(pMac, LOGE, FL("fScanEnable %d isCoexScoIndSet: %d "),
                     pMac->scan.fScanEnable, pMac->isCoexScoIndSet);
        }
    } while( 0 );

    return (status);


}

/* ---------------------------------------------------------------------------
    \fn sme_ScanGetResult
    \brief a wrapper function to request scan results from CSR.
    This is a synchronous call
    \param pFilter - If pFilter is NULL, all cached results are returned
    \param phResult - an object for the result.
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_ScanGetResult(tHalHandle hHal, tANI_U8 sessionId, tCsrScanResultFilter *pFilter,
                            tScanResultHandle *phResult)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
              TRACE_CODE_SME_RX_HDD_MSG_SCAN_GET_RESULTS, sessionId,0 ));
   smsLog(pMac, LOG2, FL("enter"));
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status = csrScanGetResult( hHal, pFilter, phResult );
       sme_ReleaseGlobalLock( &pMac->sme );
   }
   smsLog(pMac, LOG2, FL("exit status %d"), status);

   return (status);
}


/* ---------------------------------------------------------------------------
    \fn sme_ScanFlushResult
    \brief a wrapper function to request CSR to clear scan results.
    This is a synchronous call
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_ScanFlushResult(tHalHandle hHal, tANI_U8 sessionId)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
          TRACE_CODE_SME_RX_HDD_MSG_SCAN_FLUSH_RESULTS, sessionId,0 ));
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status = csrScanFlushResult( hHal );
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_FilterScanResults
    \brief a wrapper function to request CSR to clear scan results.
    This is a synchronous call
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_FilterScanResults(tHalHandle hHal, tANI_U8 sessionId)
{
   eHalStatus status = eHAL_STATUS_SUCCESS;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(macTraceNew(pMac, VOS_MODULE_ID_SME,
          TRACE_CODE_SME_RX_HDD_MSG_SCAN_FLUSH_RESULTS, sessionId,0 ));
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       csrScanFilterResults(pMac);
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

eHalStatus sme_ScanFlushP2PResult(tHalHandle hHal, tANI_U8 sessionId)
{
        eHalStatus status = eHAL_STATUS_FAILURE;
        tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

        MTRACE(vos_trace(VOS_MODULE_ID_SME,
           TRACE_CODE_SME_RX_HDD_MSG_SCAN_FLUSH_P2PRESULTS, sessionId,0 ));
        status = sme_AcquireGlobalLock( &pMac->sme );
        if ( HAL_STATUS_SUCCESS( status ) )
        {
                status = csrScanFlushSelectiveResult( hHal, VOS_TRUE );
                sme_ReleaseGlobalLock( &pMac->sme );
        }

        return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_ScanResultGetFirst
    \brief a wrapper function to request CSR to returns the first element of
           scan result.
    This is a synchronous call
    \param hScanResult - returned from csrScanGetResult
    \return tCsrScanResultInfo * - NULL if no result
  ---------------------------------------------------------------------------*/
tCsrScanResultInfo *sme_ScanResultGetFirst(tHalHandle hHal,
                                          tScanResultHandle hScanResult)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   tCsrScanResultInfo *pRet = NULL;

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
          TRACE_CODE_SME_RX_HDD_MSG_SCAN_RESULT_GETFIRST, NO_SESSION,0 ));
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       pRet = csrScanResultGetFirst( pMac, hScanResult );
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (pRet);
}


/* ---------------------------------------------------------------------------
    \fn sme_ScanResultGetNext
    \brief a wrapper function to request CSR to returns the next element of
           scan result. It can be called without calling csrScanResultGetFirst
           first
    This is a synchronous call
    \param hScanResult - returned from csrScanGetResult
    \return Null if no result or reach the end
  ---------------------------------------------------------------------------*/
tCsrScanResultInfo *sme_ScanResultGetNext(tHalHandle hHal,
                                          tScanResultHandle hScanResult)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tCsrScanResultInfo *pRet = NULL;

    MTRACE(vos_trace(VOS_MODULE_ID_SME ,
        TRACE_CODE_SME_RX_HDD_MSG_SCAN_RESULT_GETNEXT, NO_SESSION,0 ));
    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        pRet = csrScanResultGetNext( pMac, hScanResult );
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return (pRet);
}


/* ---------------------------------------------------------------------------
    \fn sme_ScanSetBGScanparams
    \brief a wrapper function to request CSR to set BG scan params in PE
    This is a synchronous call
    \param pScanReq - BG scan request structure
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_ScanSetBGScanparams(tHalHandle hHal, tANI_U8 sessionId, tCsrBGScanRequest *pScanReq)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    if( NULL != pScanReq )
    {
        status = sme_AcquireGlobalLock( &pMac->sme );
        if ( HAL_STATUS_SUCCESS( status ) )
        {
            status = csrScanSetBGScanparams( hHal, pScanReq );
            sme_ReleaseGlobalLock( &pMac->sme );
        }
    }

    return (status);
}


/* ---------------------------------------------------------------------------
    \fn sme_ScanResultPurge
    \brief a wrapper function to request CSR to remove all items(tCsrScanResult)
           in the list and free memory for each item
    This is a synchronous call
    \param hScanResult - returned from csrScanGetResult. hScanResult is
                         considered gone by
    calling this function and even before this function reutrns.
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_ScanResultPurge(tHalHandle hHal, tScanResultHandle hScanResult)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
             TRACE_CODE_SME_RX_HDD_MSG_SCAN_RESULT_PURGE, NO_SESSION,0 ));
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status = csrScanResultPurge( hHal, hScanResult );
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_ScanGetPMKIDCandidateList
    \brief a wrapper function to return the PMKID candidate list
    This is a synchronous call
    \param pPmkidList - caller allocated buffer point to an array of
                        tPmkidCandidateInfo
    \param pNumItems - pointer to a variable that has the number of
                       tPmkidCandidateInfo allocated when retruning, this is
                       either the number needed or number of items put into
                       pPmkidList
    \return eHalStatus - when fail, it usually means the buffer allocated is not
                         big enough and pNumItems
    has the number of tPmkidCandidateInfo.
    \Note: pNumItems is a number of tPmkidCandidateInfo,
           not sizeof(tPmkidCandidateInfo) * something
  ---------------------------------------------------------------------------*/
eHalStatus sme_ScanGetPMKIDCandidateList(tHalHandle hHal, tANI_U8 sessionId,
                                        tPmkidCandidateInfo *pPmkidList,
                                        tANI_U32 *pNumItems )
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status = csrScanGetPMKIDCandidateList( pMac, sessionId, pPmkidList, pNumItems );
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/*----------------------------------------------------------------------------
  \fn sme_RoamRegisterLinkQualityIndCallback

  \brief
  a wrapper function to allow HDD to register a callback handler with CSR for
  link quality indications.

  Only one callback may be registered at any time.
  In order to deregister the callback, a NULL cback may be provided.

  Registration happens in the task context of the caller.

  \param callback - Call back being registered
  \param pContext - user data

  DEPENDENCIES: After CSR open

  \return eHalStatus
-----------------------------------------------------------------------------*/
eHalStatus sme_RoamRegisterLinkQualityIndCallback(tHalHandle hHal, tANI_U8 sessionId,
                                                  csrRoamLinkQualityIndCallback   callback,
                                                  void                           *pContext)
{
   return(csrRoamRegisterLinkQualityIndCallback((tpAniSirGlobal)hHal, callback, pContext));
}

/* ---------------------------------------------------------------------------
    \fn sme_RoamRegisterCallback
    \brief a wrapper function to allow HDD to register a callback with CSR.
           Unlike scan, roam has one callback for all the roam requests
    \param callback - a callback function that roam calls upon when state changes
    \param pContext - a pointer passed in for the callback
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamRegisterCallback(tHalHandle hHal,
                                    csrRoamCompleteCallback callback,
                                    void *pContext)
{
   return(csrRoamRegisterCallback((tpAniSirGlobal)hHal, callback, pContext));
}

eCsrPhyMode sme_GetPhyMode(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    return pMac->roam.configParam.phyMode;
}

/* ---------------------------------------------------------------------------
    \fn sme_GetChannelBondingMode5G
    \brief get the channel bonding mode for 5G band
    \param hHal - HAL handle
    \return channel bonding mode for 5G
  ---------------------------------------------------------------------------*/
tANI_U32 sme_GetChannelBondingMode5G(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tSmeConfigParams  smeConfig;

    sme_GetConfigParam(pMac, &smeConfig);

    return smeConfig.csrConfig.channelBondingMode5GHz;
}

/* ---------------------------------------------------------------------------
    \fn sme_GetChannelBondingMode24G
    \brief get the channel bonding mode for 2.4G band
    \param hHal - HAL handle
    \return channel bonding mode for 2.4G
  ---------------------------------------------------------------------------*/
tANI_U32 sme_GetChannelBondingMode24G(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tSmeConfigParams  smeConfig;

    sme_GetConfigParam(pMac, &smeConfig);

    return smeConfig.csrConfig.channelBondingMode24GHz;
}


/* ---------------------------------------------------------------------------
    \fn sme_RoamConnect
    \brief a wrapper function to request CSR to inititiate an association
    This is an asynchronous call.
    \param sessionId - the sessionId returned by sme_OpenSession.
    \param pProfile - description of the network to which to connect
    \param hBssListIn - a list of BSS descriptor to roam to. It is returned
                        from csrScanGetResult
    \param pRoamId - to get back the request ID
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamConnect(tHalHandle hHal, tANI_U8 sessionId, tCsrRoamProfile *pProfile,
                           tANI_U32 *pRoamId)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    if (!pMac)
    {
        return eHAL_STATUS_FAILURE;
    }

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
                   TRACE_CODE_SME_RX_HDD_MSG_CONNECT, sessionId, 0));
    smsLog(pMac, LOG2, FL("enter"));
    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
        {
            status = csrRoamConnect( pMac, sessionId, pProfile, NULL, pRoamId );
        }
        else
        {
            smsLog(pMac, LOGE, FL("invalid sessionID %d"), sessionId);
            status = eHAL_STATUS_INVALID_PARAMETER;
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }
    else
    {
        smsLog(pMac, LOGE, FL("sme_AcquireGlobalLock failed"));
    }

    return (status);
}

/* ---------------------------------------------------------------------------

    \fn sme_SetPhyMode

    \brief Changes the PhyMode.

    \param hHal - The handle returned by macOpen.

    \param phyMode new phyMode which is to set

    \return eHalStatus  SUCCESS.

  -------------------------------------------------------------------------------*/
eHalStatus sme_SetPhyMode(tHalHandle hHal, eCsrPhyMode phyMode)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    if (NULL == pMac)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                  "%s: invalid context", __func__);
        return eHAL_STATUS_FAILURE;
    }

    pMac->roam.configParam.phyMode = phyMode;
    pMac->roam.configParam.uCfgDot11Mode = csrGetCfgDot11ModeFromCsrPhyMode(NULL,
                                                 pMac->roam.configParam.phyMode,
                                    pMac->roam.configParam.ProprietaryRatesEnabled);

    return eHAL_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------
    \fn sme_RoamReassoc
    \brief a wrapper function to request CSR to inititiate a re-association
    \param pProfile - can be NULL to join the currently connected AP. In that
    case modProfileFields should carry the modified field(s) which could trigger
    reassoc
    \param modProfileFields - fields which are part of tCsrRoamConnectedProfile
    that might need modification dynamically once STA is up & running and this
    could trigger a reassoc
    \param pRoamId - to get back the request ID
    \return eHalStatus
  -------------------------------------------------------------------------------*/
eHalStatus sme_RoamReassoc(tHalHandle hHal, tANI_U8 sessionId, tCsrRoamProfile *pProfile,
                          tCsrRoamModifyProfileFields modProfileFields,
                          tANI_U32 *pRoamId, v_BOOL_t fForce)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
                      TRACE_CODE_SME_RX_HDD_ROAM_REASSOC, sessionId, 0));
    smsLog(pMac, LOG2, FL("enter"));
    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
        {
            if((NULL == pProfile) && (fForce == 1))
            {
                tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
                /* to force the AP initiate fresh 802.1x authentication need to clear
                 * the PMKID cache for that set the following boolean. this is needed
                 * by the HS 2.0 passpoint certification 5.2.a and b testcases */
                pSession->fIgnorePMKIDCache = TRUE;
                status = csrReassoc( pMac, sessionId, &modProfileFields, pRoamId , fForce);
            }
            else
            {
                status = csrRoamReassoc( pMac, sessionId, pProfile, modProfileFields, pRoamId );
            }
        }
        else
        {
            status = eHAL_STATUS_INVALID_PARAMETER;
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_RoamConnectToLastProfile
    \brief a wrapper function to request CSR to disconnect and reconnect with
           the same profile
    This is an asynchronous call.
    \return eHalStatus. It returns fail if currently connected
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamConnectToLastProfile(tHalHandle hHal, tANI_U8 sessionId)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
      if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
      {
         status = csrRoamConnectToLastProfile( pMac, sessionId );
      }
      else
      {
          status = eHAL_STATUS_INVALID_PARAMETER;
      }
      sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_RoamDisconnect
    \brief a wrapper function to request CSR to disconnect from a network
    This is an asynchronous call.
    \param reason -- To indicate the reason for disconnecting. Currently, only
                     eCSR_DISCONNECT_REASON_MIC_ERROR is meanful.
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamDisconnect(tHalHandle hHal, tANI_U8 sessionId, eCsrRoamDisconnectReason reason)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
                        TRACE_CODE_SME_RX_HDD_ROAM_DISCONNECT, sessionId, reason));
   smsLog(pMac, LOG2, FL("enter"));
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
      if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
      {
         status = csrRoamDisconnect( pMac, sessionId, reason );
      }
      else
      {
          status = eHAL_STATUS_INVALID_PARAMETER;
      }
      sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_RoamStopBss
    \brief To stop BSS for Soft AP. This is an asynchronous API.
    \param hHal - Global structure
    \param sessionId - sessionId of SoftAP
    \return eHalStatus  SUCCESS  Roam callback will be called to indicate actual results
  -------------------------------------------------------------------------------*/
eHalStatus sme_RoamStopBss(tHalHandle hHal, tANI_U8 sessionId)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   smsLog(pMac, LOG2, FL("enter"));
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
      if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
      {
         status = csrRoamIssueStopBssCmd( pMac, sessionId, eANI_BOOLEAN_FALSE );
      }
      else
      {
          status = eHAL_STATUS_INVALID_PARAMETER;
      }
      sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_RoamDisconnectSta
    \brief To disassociate a station. This is an asynchronous API.
    \param hHal - Global structure
    \param sessionId - sessionId of SoftAP
    \param pPeerMacAddr - Caller allocated memory filled with peer MAC address (6 bytes)
    \return eHalStatus  SUCCESS  Roam callback will be called to indicate actual results
  -------------------------------------------------------------------------------*/
eHalStatus sme_RoamDisconnectSta(tHalHandle hHal, tANI_U8 sessionId,
                                tANI_U8 *pPeerMacAddr)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   if ( NULL == pMac )
   {
     VOS_ASSERT(0);
     return status;
   }

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
      if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
      {
         status = csrRoamIssueDisassociateStaCmd( pMac, sessionId, pPeerMacAddr,
                                                  eSIR_MAC_DEAUTH_LEAVING_BSS_REASON);
      }
      else
      {
         status = eHAL_STATUS_INVALID_PARAMETER;
      }
      sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_RoamDeauthSta
    \brief To disassociate a station. This is an asynchronous API.
    \param hHal - Global structure
    \param sessionId - sessionId of SoftAP
    \param pDelStaParams -Pointer to parameters of the station to deauthenticate
    \return eHalStatus  SUCCESS  Roam callback will be called to indicate actual results
  -------------------------------------------------------------------------------*/
eHalStatus sme_RoamDeauthSta(tHalHandle hHal, tANI_U8 sessionId,
                             struct tagCsrDelStaParams *pDelStaParams)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   if ( NULL == pMac )
   {
     VOS_ASSERT(0);
     return status;
   }

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
      if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
      {
         status = csrRoamIssueDeauthStaCmd( pMac, sessionId, pDelStaParams);
      }
      else
      {
         status = eHAL_STATUS_INVALID_PARAMETER;
      }
      sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_RoamTKIPCounterMeasures
    \brief To start or stop TKIP counter measures. This is an asynchronous API.
    \param sessionId - sessionId of SoftAP
    \param pPeerMacAddr - Caller allocated memory filled with peer MAC address (6 bytes)
    \return eHalStatus
  -------------------------------------------------------------------------------*/
eHalStatus sme_RoamTKIPCounterMeasures(tHalHandle hHal, tANI_U8 sessionId,
                                        tANI_BOOLEAN bEnable)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   if ( NULL == pMac )
   {
     VOS_ASSERT(0);
     return status;
   }

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
        if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
        {
            status = csrRoamIssueTkipCounterMeasures( pMac, sessionId, bEnable);
        }
        else
        {
            status = eHAL_STATUS_INVALID_PARAMETER;
        }
        sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_RoamGetAssociatedStas
    \brief To probe the list of associated stations from various modules of CORE stack.
    \This is an asynchronous API.
    \param sessionId    - sessionId of SoftAP
    \param modId        - Module from whom list of associtated stations is to be probed.
                          If an invalid module is passed then by default VOS_MODULE_ID_PE will be probed
    \param pUsrContext  - Opaque HDD context
    \param pfnSapEventCallback  - Sap event callback in HDD
    \param pAssocBuf    - Caller allocated memory to be filled with associatd stations info
    \return eHalStatus
  -------------------------------------------------------------------------------*/
eHalStatus sme_RoamGetAssociatedStas(tHalHandle hHal, tANI_U8 sessionId,
                                        VOS_MODULE_ID modId, void *pUsrContext,
                                        void *pfnSapEventCallback, tANI_U8 *pAssocStasBuf)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   if ( NULL == pMac )
   {
     VOS_ASSERT(0);
     return status;
   }

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
        if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
        {
            status = csrRoamGetAssociatedStas( pMac, sessionId, modId, pUsrContext, pfnSapEventCallback, pAssocStasBuf );
        }
        else
        {
            status = eHAL_STATUS_INVALID_PARAMETER;
        }
        sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_RoamGetWpsSessionOverlap
    \brief To get the WPS PBC session overlap information.
    \This is an asynchronous API.
    \param sessionId    - sessionId of SoftAP
    \param pUsrContext  - Opaque HDD context
    \param pfnSapEventCallback  - Sap event callback in HDD
    \pRemoveMac - pointer to Mac address which needs to be removed from session
    \return eHalStatus
  -------------------------------------------------------------------------------*/
eHalStatus sme_RoamGetWpsSessionOverlap(tHalHandle hHal, tANI_U8 sessionId,
                                        void *pUsrContext, void
                                        *pfnSapEventCallback, v_MACADDR_t pRemoveMac)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   if ( NULL == pMac )
   {
     VOS_ASSERT(0);
     return status;
   }

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
        if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
        {
            status = csrRoamGetWpsSessionOverlap( pMac, sessionId, pUsrContext, pfnSapEventCallback, pRemoveMac);
        }
        else
        {
            status = eHAL_STATUS_INVALID_PARAMETER;
        }
        sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}


/* ---------------------------------------------------------------------------
    \fn sme_RoamGetConnectState
    \brief a wrapper function to request CSR to return the current connect state
           of Roaming
    This is a synchronous call.
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamGetConnectState(tHalHandle hHal, tANI_U8 sessionId, eCsrConnectState *pState)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
       if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
       {
          status = csrRoamGetConnectState( pMac, sessionId, pState );
       }
       else
       {
           status = eHAL_STATUS_INVALID_PARAMETER;
       }
       sme_ReleaseGlobalLock( &pMac->sme );
    }

    return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_RoamGetConnectProfile
    \brief a wrapper function to request CSR to return the current connect
           profile. Caller must call csrRoamFreeConnectProfile after it is done
           and before reuse for another csrRoamGetConnectProfile call.
    This is a synchronous call.
    \param pProfile - pointer to a caller allocated structure
                      tCsrRoamConnectedProfile
    \return eHalStatus. Failure if not connected
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamGetConnectProfile(tHalHandle hHal, tANI_U8 sessionId,
                                     tCsrRoamConnectedProfile *pProfile)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
                TRACE_CODE_SME_RX_HDD_ROAM_GET_CONNECTPROFILE, sessionId, 0));
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
      if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
      {
         status = csrRoamGetConnectProfile( pMac, sessionId, pProfile );
      }
      else
      {
          status = eHAL_STATUS_INVALID_PARAMETER;
      }
      sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_RoamFreeConnectProfile
    \brief a wrapper function to request CSR to free and reinitialize the
           profile returned previously by csrRoamGetConnectProfile.
    This is a synchronous call.
    \param pProfile - pointer to a caller allocated structure
                      tCsrRoamConnectedProfile
    \return eHalStatus.
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamFreeConnectProfile(tHalHandle hHal,
                                      tCsrRoamConnectedProfile *pProfile)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
            TRACE_CODE_SME_RX_HDD_ROAM_FREE_CONNECTPROFILE, NO_SESSION, 0));
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status = csrRoamFreeConnectProfile( pMac, pProfile );
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_RoamSetPMKIDCache
    \brief a wrapper function to request CSR to return the PMKID candidate list
    This is a synchronous call.
    \param pPMKIDCache - caller allocated buffer point to an array of
                         tPmkidCacheInfo
    \param numItems - a variable that has the number of tPmkidCacheInfo
                      allocated when retruning, this is either the number needed
                      or number of items put into pPMKIDCache
    \param update_entire_cache - this bool value specifies if the entire pmkid
                                 cache should be overwritten or should it be
                                 updated entry by entry.
    \return eHalStatus - when fail, it usually means the buffer allocated is not
                         big enough and pNumItems has the number of
                         tPmkidCacheInfo.
    \Note: pNumItems is a number of tPmkidCacheInfo,
           not sizeof(tPmkidCacheInfo) * something
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamSetPMKIDCache( tHalHandle hHal, tANI_U8 sessionId,
                                  tPmkidCacheInfo *pPMKIDCache,
                                  tANI_U32 numItems,
                                  tANI_BOOLEAN update_entire_cache )
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
          TRACE_CODE_SME_RX_HDD_ROAM_SET_PMKIDCACHE, sessionId, numItems));
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
      if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
      {
         status = csrRoamSetPMKIDCache( pMac, sessionId, pPMKIDCache,
                                        numItems, update_entire_cache );
      }
      else
      {
          status = eHAL_STATUS_INVALID_PARAMETER;
      }
      sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

eHalStatus sme_RoamDelPMKIDfromCache( tHalHandle hHal, tANI_U8 sessionId,
                                      tANI_U8 *pBSSId,
                                      tANI_BOOLEAN flush_cache )
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
      if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
      {
         status = csrRoamDelPMKIDfromCache( pMac, sessionId,
                                            pBSSId, flush_cache );
      }
      else
      {
          status = eHAL_STATUS_INVALID_PARAMETER;
      }
      sme_ReleaseGlobalLock( &pMac->sme );
   }
   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_RoamGetSecurityReqIE
    \brief a wrapper function to request CSR to return the WPA or RSN or WAPI IE CSR
           passes to PE to JOIN request or START_BSS request
    This is a synchronous call.
    \param pLen - caller allocated memory that has the length of pBuf as input.
                  Upon returned, *pLen has the needed or IE length in pBuf.
    \param pBuf - Caller allocated memory that contain the IE field, if any,
                  upon return
    \param secType - Specifies whether looking for WPA/WPA2/WAPI IE
    \return eHalStatus - when fail, it usually means the buffer allocated is not
                         big enough
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamGetSecurityReqIE(tHalHandle hHal, tANI_U8 sessionId, tANI_U32 *pLen,
                                  tANI_U8 *pBuf, eCsrSecurityType secType)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
        {
           status = csrRoamGetWpaRsnReqIE( hHal, sessionId, pLen, pBuf );
        }
        else
        {
           status = eHAL_STATUS_INVALID_PARAMETER;
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_RoamGetSecurityRspIE
    \brief a wrapper function to request CSR to return the WPA or RSN or WAPI IE from
           the beacon or probe rsp if connected
    This is a synchronous call.
    \param pLen - caller allocated memory that has the length of pBuf as input.
                  Upon returned, *pLen has the needed or IE length in pBuf.
    \param pBuf - Caller allocated memory that contain the IE field, if any,
                  upon return
    \param secType - Specifies whether looking for WPA/WPA2/WAPI IE
    \return eHalStatus - when fail, it usually means the buffer allocated is not
                         big enough
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamGetSecurityRspIE(tHalHandle hHal, tANI_U8 sessionId, tANI_U32 *pLen,
                                  tANI_U8 *pBuf, eCsrSecurityType secType)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
        {
           status = csrRoamGetWpaRsnRspIE( pMac, sessionId, pLen, pBuf );
        }
        else
        {
           status = eHAL_STATUS_INVALID_PARAMETER;
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return (status);

}


/* ---------------------------------------------------------------------------
    \fn sme_RoamGetNumPMKIDCache
    \brief a wrapper function to request CSR to return number of PMKID cache
           entries
    This is a synchronous call.
    \return tANI_U32 - the number of PMKID cache entries
  ---------------------------------------------------------------------------*/
tANI_U32 sme_RoamGetNumPMKIDCache(tHalHandle hHal, tANI_U8 sessionId)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tANI_U32 numPmkidCache = 0;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
        {
           numPmkidCache = csrRoamGetNumPMKIDCache( pMac, sessionId );
           status = eHAL_STATUS_SUCCESS;
        }
        else
        {
           status = eHAL_STATUS_INVALID_PARAMETER;
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return (numPmkidCache);
}

/* ---------------------------------------------------------------------------
    \fn sme_RoamGetPMKIDCache
    \brief a wrapper function to request CSR to return PMKID cache from CSR
    This is a synchronous call.
    \param pNum - caller allocated memory that has the space of the number of
                  pBuf tPmkidCacheInfo as input. Upon returned, *pNum has the
                  needed or actually number in tPmkidCacheInfo.
    \param pPmkidCache - Caller allocated memory that contains PMKID cache, if
                         any, upon return
    \return eHalStatus - when fail, it usually means the buffer allocated is not
                         big enough
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamGetPMKIDCache(tHalHandle hHal, tANI_U8 sessionId, tANI_U32 *pNum,
                                 tPmkidCacheInfo *pPmkidCache)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
       {
          status = csrRoamGetPMKIDCache( pMac, sessionId, pNum, pPmkidCache );
       }
       else
       {
          status = eHAL_STATUS_INVALID_PARAMETER;
       }
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}


/* ---------------------------------------------------------------------------
    \fn sme_GetConfigParam
    \brief a wrapper function that HDD calls to get the global settings
           currently maintained by CSR.
    This is a synchronous call.
    \param pParam - caller allocated memory
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_GetConfigParam(tHalHandle hHal, tSmeConfigParams *pParam)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
            TRACE_CODE_SME_RX_HDD_GET_CONFIGPARAM, NO_SESSION, 0));
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
      status = csrGetConfigParam(pMac, &pParam->csrConfig);
      if (status != eHAL_STATUS_SUCCESS)
      {
         smsLog( pMac, LOGE, "%s csrGetConfigParam failed", __func__);
         sme_ReleaseGlobalLock( &pMac->sme );
         return status;
      }
#if defined WLAN_FEATURE_P2P_INTERNAL
      status = p2pGetConfigParam(pMac, &pParam->p2pConfig);
      if (status != eHAL_STATUS_SUCCESS)
      {
         smsLog( pMac, LOGE, "%s p2pGetConfigParam failed", __func__);
         sme_ReleaseGlobalLock( &pMac->sme );
         return status;
      }
#endif
      sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_CfgSetInt
    \brief a wrapper function that HDD calls to set parameters in CFG.
    This is a synchronous call.
    \param cfgId - Configuration Parameter ID (type) for STA.
    \param ccmValue - The information related to Configuration Parameter ID
                      which needs to be saved in CFG
    \param callback - To be registered by CSR with CCM. Once the CFG done with
                      saving the information in the database, it notifies CCM &
                      then the callback will be invoked to notify.
    \param toBeSaved - To save the request for future reference
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_CfgSetInt(tHalHandle hHal, tANI_U32 cfgId, tANI_U32 ccmValue,
                         tCcmCfgSetCallback callback, eAniBoolean toBeSaved)
{
   return(ccmCfgSetInt(hHal, cfgId, ccmValue, callback, toBeSaved));
}

/* ---------------------------------------------------------------------------
    \fn sme_CfgSetStr
    \brief a wrapper function that HDD calls to set parameters in CFG.
    This is a synchronous call.
    \param cfgId - Configuration Parameter ID (type) for STA.
    \param pStr - Pointer to the byte array which carries the information needs
                  to be saved in CFG
    \param length - Length of the data to be saved
    \param callback - To be registered by CSR with CCM. Once the CFG done with
                      saving the information in the database, it notifies CCM &
                      then the callback will be invoked to notify.
    \param toBeSaved - To save the request for future reference
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_CfgSetStr(tHalHandle hHal, tANI_U32 cfgId, tANI_U8 *pStr,
                         tANI_U32 length, tCcmCfgSetCallback callback,
                         eAniBoolean toBeSaved)
{
   return(ccmCfgSetStr(hHal, cfgId, pStr, length, callback, toBeSaved));
}

/* ---------------------------------------------------------------------------
    \fn sme_GetModifyProfileFields
    \brief HDD or SME - QOS calls this function to get the current values of
    connected profile fields, changing which can cause reassoc.
    This function must be called after CFG is downloaded and STA is in connected
    state. Also, make sure to call this function to get the current profile
    fields before calling the reassoc. So that pModifyProfileFields will have
    all the latest values plus the one(s) has been updated as part of reassoc
    request.
    \param pModifyProfileFields - pointer to the connected profile fields
    changing which can cause reassoc

    \return eHalStatus
  -------------------------------------------------------------------------------*/
eHalStatus sme_GetModifyProfileFields(tHalHandle hHal, tANI_U8 sessionId,
                                     tCsrRoamModifyProfileFields * pModifyProfileFields)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
              TRACE_CODE_SME_RX_HDD_GET_MODPROFFIELDS, sessionId, 0));
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
       {
          status = csrGetModifyProfileFields(pMac, sessionId, pModifyProfileFields);
       }
       else
       {
          status = eHAL_STATUS_INVALID_PARAMETER;
       }
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_HT40StopOBSSScan
    \brief HDD or SME - Command to stop the OBSS scan
     THis is implemented only for debugging purpose.
     As per spec while operating in 2.4GHz OBSS scan shouldnt be stopped.
    \param sessionId - sessionId
    changing which can cause reassoc

    \return eHalStatus
  -------------------------------------------------------------------------------*/
eHalStatus sme_HT40StopOBSSScan(tHalHandle hHal, tANI_U8 sessionId)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    smsLog(pMac, LOG2, FL("enter"));
    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
        {
           csrHT40StopOBSSScan( pMac, sessionId );
        }
        else
        {
           VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
             "%s: Invalid session sessionId %d", __func__,sessionId);
           status = eHAL_STATUS_INVALID_PARAMETER;
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }
    return (status);
}

/*--------------------------------------------------------------------------
    \fn sme_SetConfigPowerSave
    \brief  Wrapper fn to change power save configuration in SME (PMC) module.
            For BMPS related configuration, this function also updates the CFG
            and sends a message to FW to pick up the new values. Note: Calling
            this function only updates the configuration and does not enable
            the specified power save mode.
    \param  hHal - The handle returned by macOpen.
    \param  psMode - Power Saving mode being modified
    \param  pConfigParams - a pointer to a caller allocated object of type
            tPmcSmpsConfigParams or tPmcBmpsConfigParams or tPmcImpsConfigParams
    \return eHalStatus
  --------------------------------------------------------------------------*/
eHalStatus sme_SetConfigPowerSave(tHalHandle hHal, tPmcPowerSavingMode psMode,
                                  void *pConfigParams)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
                 TRACE_CODE_SME_RX_HDD_SET_CONFIG_PWRSAVE, NO_SESSION, 0));
   if (NULL == pConfigParams ) {
      smsLog( pMac, LOGE, "Empty config param structure for PMC, "
              "nothing to update");
      return eHAL_STATUS_FAILURE;
   }

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status = pmcSetConfigPowerSave(hHal, psMode, pConfigParams);
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/*--------------------------------------------------------------------------
    \fn sme_GetConfigPowerSave
    \brief  Wrapper fn to retrieve power save configuration in SME (PMC) module
    \param  hHal - The handle returned by macOpen.
    \param  psMode - Power Saving mode
    \param  pConfigParams - a pointer to a caller allocated object of type
            tPmcSmpsConfigParams or tPmcBmpsConfigParams or tPmcImpsConfigParams
    \return eHalStatus
  --------------------------------------------------------------------------*/
eHalStatus sme_GetConfigPowerSave(tHalHandle hHal, tPmcPowerSavingMode psMode,
                                  void *pConfigParams)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
            TRACE_CODE_SME_RX_HDD_GET_CONFIG_PWRSAVE, NO_SESSION, 0));
   if (NULL == pConfigParams ) {
      smsLog( pMac, LOGE, "Empty config param structure for PMC, "
              "nothing to update");
      return eHAL_STATUS_FAILURE;
   }

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status = pmcGetConfigPowerSave(hHal, psMode, pConfigParams);
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_SignalPowerEvent
    \brief  Signals to PMC that a power event has occurred. Used for putting
            the chip into deep sleep mode.
    \param  hHal - The handle returned by macOpen.
    \param  event - the event that has occurred
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_SignalPowerEvent (tHalHandle hHal, tPmcPowerEvent event)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status = pmcSignalPowerEvent(hHal, event);
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_EnablePowerSave
    \brief  Enables one of the power saving modes.
    \param  hHal - The handle returned by macOpen.
    \param  psMode - The power saving mode to enable. If BMPS mode is enabled
                     while the chip is operating in Full Power, PMC will start
                     a timer that will try to put the chip in BMPS mode after
                     expiry.
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_EnablePowerSave (tHalHandle hHal, tPmcPowerSavingMode psMode)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
          TRACE_CODE_SME_RX_HDD_ENABLE_PWRSAVE, NO_SESSION, psMode));
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status =  pmcEnablePowerSave(hHal, psMode);
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_DisablePowerSave
    \brief   Disables one of the power saving modes.
    \param  hHal - The handle returned by macOpen.
    \param  psMode - The power saving mode to disable. Disabling does not imply
                     that device will be brought out of the current PS mode. This
                     is purely a configuration API.
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_DisablePowerSave (tHalHandle hHal, tPmcPowerSavingMode psMode)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
              TRACE_CODE_SME_RX_HDD_DISABLE_PWRSAVE, NO_SESSION, psMode));
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status = pmcDisablePowerSave(hHal, psMode);
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
 }

/* ---------------------------------------------------------------------------
+    \fn sme_SetHostPowerSave
+    \brief   Enables BMPS logic to be controlled by User level apps
+    \param  hHal - The handle returned by macOpen.
+    \param  psMode - The power saving mode to disable. Disabling does not imply
+                     that device will be brought out of the current PS mode. This
+                     is purely a configuration API.
+    \return eHalStatus
+  ---------------------------------------------------------------------------*/
eHalStatus sme_SetHostPowerSave (tHalHandle hHal, v_BOOL_t psMode)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   pMac->pmc.isHostPsEn = psMode;

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_StartAutoBmpsTimer
    \brief  Starts a timer that periodically polls all the registered
            module for entry into Bmps mode. This timer is started only if BMPS is
            enabled and whenever the device is in full power.
    \param  hHal - The handle returned by macOpen.
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_StartAutoBmpsTimer ( tHalHandle hHal)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
             TRACE_CODE_SME_RX_HDD_START_AUTO_BMPSTIMER, NO_SESSION, 0));
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status = pmcStartAutoBmpsTimer(hHal);
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}
/* ---------------------------------------------------------------------------
    \fn sme_StopAutoBmpsTimer
    \brief  Stops the Auto BMPS Timer that was started using sme_startAutoBmpsTimer
            Stopping the timer does not cause a device state change. Only the timer
            is stopped. If "Full Power" is desired, use the sme_RequestFullPower API
    \param  hHal - The handle returned by macOpen.
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_StopAutoBmpsTimer ( tHalHandle hHal)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
          TRACE_CODE_SME_RX_HDD_STOP_AUTO_BMPSTIMER, NO_SESSION, 0));
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status = pmcStopAutoBmpsTimer(hHal);
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}
/* ---------------------------------------------------------------------------
    \fn sme_QueryPowerState
    \brief  Returns the current power state of the device.
    \param  hHal - The handle returned by macOpen.
    \param pPowerState - pointer to location to return power state (LOW or HIGH)
    \param pSwWlanSwitchState - ptr to location to return SW WLAN Switch state
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_QueryPowerState (
   tHalHandle hHal,
   tPmcPowerState *pPowerState,
   tPmcSwitchState *pSwWlanSwitchState)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status = pmcQueryPowerState (hHal, pPowerState, NULL, pSwWlanSwitchState);
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_IsPowerSaveEnabled
    \brief  Checks if the device is able to enter a particular power save mode
            This does not imply that the device is in a particular PS mode
    \param  hHal - The handle returned by macOpen.
    \param psMode - the power saving mode
    \return eHalStatus
  ---------------------------------------------------------------------------*/
tANI_BOOLEAN sme_IsPowerSaveEnabled (tHalHandle hHal, tPmcPowerSavingMode psMode)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   tANI_BOOLEAN result = false;

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
               TRACE_CODE_SME_RX_HDD_IS_PWRSAVE_ENABLED, NO_SESSION, psMode));
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       result = pmcIsPowerSaveEnabled(hHal, psMode);
       sme_ReleaseGlobalLock( &pMac->sme );
       return result;
   }

   return false;
}

/* ---------------------------------------------------------------------------
    \fn sme_RequestFullPower
    \brief  Request that the device be brought to full power state. When the
            device enters Full Power PMC will start a BMPS timer if BMPS PS mode
            is enabled. On timer expiry PMC will attempt to put the device in
            BMPS mode if following holds true:
            - BMPS mode is enabled
            - Polling of all modules through the Power Save Check routine passes
            - STA is associated to an access point
    \param  hHal - The handle returned by macOpen.
    \param  - callbackRoutine Callback routine invoked in case of success/failure
    \return eHalStatus - status
     eHAL_STATUS_SUCCESS - device brought to full power state
     eHAL_STATUS_FAILURE - device cannot be brought to full power state
     eHAL_STATUS_PMC_PENDING - device is being brought to full power state,
  ---------------------------------------------------------------------------*/
eHalStatus sme_RequestFullPower (
   tHalHandle hHal,
   void (*callbackRoutine) (void *callbackContext, eHalStatus status),
   void *callbackContext,
   tRequestFullPowerReason fullPowerReason)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
         TRACE_CODE_SME_RX_HDD_REQUEST_FULLPOWER, NO_SESSION, fullPowerReason));
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status = pmcRequestFullPower(hHal, callbackRoutine, callbackContext, fullPowerReason);
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_RequestBmps
    \brief  Request that the device be put in BMPS state. Request will be
            accepted only if BMPS mode is enabled and power save check routine
            passes.
    \param  hHal - The handle returned by macOpen.
    \param  - callbackRoutine Callback routine invoked in case of success/failure
    \return eHalStatus
      eHAL_STATUS_SUCCESS - device is in BMPS state
      eHAL_STATUS_FAILURE - device cannot be brought to BMPS state
      eHAL_STATUS_PMC_PENDING - device is being brought to BMPS state
  ---------------------------------------------------------------------------*/
eHalStatus sme_RequestBmps (
   tHalHandle hHal,
   void (*callbackRoutine) (void *callbackContext, eHalStatus status),
   void *callbackContext)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
           TRACE_CODE_SME_RX_HDD_REQUEST_BMPS, NO_SESSION, 0));
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status = pmcRequestBmps(hHal, callbackRoutine, callbackContext);
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}


/* ---------------------------------------------------------------------------
    \fn  sme_SetDHCPTillPowerActiveFlag
    \brief  Sets/Clears DHCP related flag in PMC to disable/enable auto BMPS
            entry by PMC
    \param  hHal - The handle returned by macOpen.
  ---------------------------------------------------------------------------*/
void  sme_SetDHCPTillPowerActiveFlag(tHalHandle hHal, tANI_U8 flag)
{
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
          TRACE_CODE_SME_RX_HDD_SET_DHCP_FLAG, NO_SESSION, flag));
   // Set/Clear the DHCP flag which will disable/enable auto BMPS entery by PMC
   pMac->pmc.remainInPowerActiveTillDHCP = flag;
}


/* ---------------------------------------------------------------------------
    \fn sme_StartUapsd
    \brief  Request that the device be put in UAPSD state. If the device is in
            Full Power it will be put in BMPS mode first and then into UAPSD
            mode.
    \param  hHal - The handle returned by macOpen.
    \param  - callbackRoutine Callback routine invoked in case of success/failure
      eHAL_STATUS_SUCCESS - device is in UAPSD state
      eHAL_STATUS_FAILURE - device cannot be brought to UAPSD state
      eHAL_STATUS_PMC_PENDING - device is being brought to UAPSD state
      eHAL_STATUS_PMC_DISABLED - UAPSD is disabled or BMPS mode is disabled
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_StartUapsd (
   tHalHandle hHal,
   void (*callbackRoutine) (void *callbackContext, eHalStatus status),
   void *callbackContext)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status = pmcStartUapsd(hHal, callbackRoutine, callbackContext);
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
 }

/* ---------------------------------------------------------------------------
    \fn sme_StopUapsd
    \brief  Request that the device be put out of UAPSD state. Device will be
            put in in BMPS state after stop UAPSD completes.
    \param  hHal - The handle returned by macOpen.
    \return eHalStatus
      eHAL_STATUS_SUCCESS - device is put out of UAPSD and back in BMPS state
      eHAL_STATUS_FAILURE - device cannot be brought out of UAPSD state
  ---------------------------------------------------------------------------*/
eHalStatus sme_StopUapsd (tHalHandle hHal)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status = pmcStopUapsd(hHal);
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_RequestStandby
    \brief  Request that the device be put in standby. It is HDD's responsibility
            to bring the chip to full power and do a disassoc before calling
            this API.
    \param  hHal - The handle returned by macOpen.
    \param  - callbackRoutine Callback routine invoked in case of success/failure
    \return eHalStatus
      eHAL_STATUS_SUCCESS - device is in Standby mode
      eHAL_STATUS_FAILURE - device cannot be put in standby mode
      eHAL_STATUS_PMC_PENDING - device is being put in standby mode
  ---------------------------------------------------------------------------*/
eHalStatus sme_RequestStandby (
   tHalHandle hHal,
   void (*callbackRoutine) (void *callbackContext, eHalStatus status),
   void *callbackContext)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
             TRACE_CODE_SME_RX_HDD_REQUEST_STANDBY, NO_SESSION, 0));
   smsLog( pMac, LOG1, FL(" called") );
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status = pmcRequestStandby(hHal, callbackRoutine, callbackContext);
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_RegisterPowerSaveCheck
    \brief  Register a power save check routine that is called whenever
            the device is about to enter one of the power save modes.
    \param  hHal - The handle returned by macOpen.
    \param  checkRoutine -  Power save check routine to be registered
    \return eHalStatus
            eHAL_STATUS_SUCCESS - successfully registered
            eHAL_STATUS_FAILURE - not successfully registered
  ---------------------------------------------------------------------------*/
eHalStatus sme_RegisterPowerSaveCheck (
   tHalHandle hHal,
   tANI_BOOLEAN (*checkRoutine) (void *checkContext), void *checkContext)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status = pmcRegisterPowerSaveCheck (hHal, checkRoutine, checkContext);
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_Register11dScanDoneCallback
    \brief  Register a routine of type csrScanCompleteCallback which is
            called whenever an 11d scan is done
    \param  hHal - The handle returned by macOpen.
    \param  callback -  11d scan complete routine to be registered
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_Register11dScanDoneCallback (
   tHalHandle hHal,
   csrScanCompleteCallback callback)
{
   eHalStatus status = eHAL_STATUS_SUCCESS;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   pMac->scan.callback11dScanDone = callback;

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_DeregisterPowerSaveCheck
    \brief  Deregister a power save check routine
    \param  hHal - The handle returned by macOpen.
    \param  checkRoutine -  Power save check routine to be deregistered
    \return eHalStatus
            eHAL_STATUS_SUCCESS - successfully deregistered
            eHAL_STATUS_FAILURE - not successfully deregistered
  ---------------------------------------------------------------------------*/
eHalStatus sme_DeregisterPowerSaveCheck (
   tHalHandle hHal,
   tANI_BOOLEAN (*checkRoutine) (void *checkContext))
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status = pmcDeregisterPowerSaveCheck (hHal, checkRoutine);
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_RegisterDeviceStateUpdateInd
    \brief  Register a callback routine that is called whenever
            the device enters a new device state (Full Power, BMPS, UAPSD)
    \param  hHal - The handle returned by macOpen.
    \param  callbackRoutine -  Callback routine to be registered
    \param  callbackContext -  Cookie to be passed back during callback
    \return eHalStatus
            eHAL_STATUS_SUCCESS - successfully registered
            eHAL_STATUS_FAILURE - not successfully registered
  ---------------------------------------------------------------------------*/
eHalStatus sme_RegisterDeviceStateUpdateInd (
   tHalHandle hHal,
   void (*callbackRoutine) (void *callbackContext, tPmcState pmcState),
   void *callbackContext)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status = pmcRegisterDeviceStateUpdateInd (hHal, callbackRoutine, callbackContext);
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_DeregisterDeviceStateUpdateInd
    \brief  Deregister a routine that was registered for device state changes
    \param  hHal - The handle returned by macOpen.
    \param  callbackRoutine -  Callback routine to be deregistered
    \return eHalStatus
            eHAL_STATUS_SUCCESS - successfully deregistered
            eHAL_STATUS_FAILURE - not successfully deregistered
  ---------------------------------------------------------------------------*/
eHalStatus sme_DeregisterDeviceStateUpdateInd (
   tHalHandle hHal,
   void (*callbackRoutine) (void *callbackContext, tPmcState pmcState))
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status = pmcDeregisterDeviceStateUpdateInd (hHal, callbackRoutine);
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_WowlAddBcastPattern
    \brief  Add a pattern for Pattern Byte Matching in Wowl mode. Firmware will
            do a pattern match on these patterns when Wowl is enabled during BMPS
            mode. Note that Firmware performs the pattern matching only on
            broadcast frames and while Libra is in BMPS mode.
    \param  hHal - The handle returned by macOpen.
    \param  pattern -  Pattern to be added
    \return eHalStatus
            eHAL_STATUS_FAILURE  Cannot add pattern
            eHAL_STATUS_SUCCESS  Request accepted.
  ---------------------------------------------------------------------------*/
eHalStatus sme_WowlAddBcastPattern (
   tHalHandle hHal,
   tpSirWowlAddBcastPtrn pattern,
   tANI_U8    sessionId)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    MTRACE(vos_trace(VOS_MODULE_ID_SME,
           TRACE_CODE_SME_RX_HDD_WOWL_ADDBCAST_PATTERN, sessionId, 0));
    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
       status = pmcWowlAddBcastPattern (hHal, pattern, sessionId);
       sme_ReleaseGlobalLock( &pMac->sme );
    }

    return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_WowlDelBcastPattern
    \brief  Delete a pattern that was added for Pattern Byte Matching.
    \param  hHal - The handle returned by macOpen.
    \param  pattern -  Pattern to be deleted
    \return eHalStatus
            eHAL_STATUS_FAILURE  Cannot delete pattern
            eHAL_STATUS_SUCCESS  Request accepted.
  ---------------------------------------------------------------------------*/
eHalStatus sme_WowlDelBcastPattern (
   tHalHandle hHal,
   tpSirWowlDelBcastPtrn pattern,
   tANI_U8  sessionId)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    MTRACE(vos_trace(VOS_MODULE_ID_SME,
           TRACE_CODE_SME_RX_HDD_WOWL_DELBCAST_PATTERN, sessionId, 0));
    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
       status = pmcWowlDelBcastPattern (hHal, pattern, sessionId);
       sme_ReleaseGlobalLock( &pMac->sme );
    }

    return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_EnterWowl
    \brief  This is the SME API exposed to HDD to request enabling of WOWL mode.
            WoWLAN works on top of BMPS mode. If the device is not in BMPS mode,
            SME will will cache the information that WOWL has been enabled and
            attempt to put the device in BMPS. On entry into BMPS, SME will
            enable the WOWL mode.
            Note 1: If we exit BMPS mode (someone requests full power), we
            will NOT resume WOWL when we go back to BMPS again. Request for full
            power (while in WOWL mode) means disable WOWL and go to full power.
            Note 2: Both UAPSD and WOWL work on top of BMPS. On entry into BMPS, SME
            will give priority to UAPSD and enable only UAPSD if both UAPSD and WOWL
            are required. Currently there is no requirement or use case to support
            UAPSD and WOWL at the same time.

    \param  hHal - The handle returned by macOpen.
    \param  enterWowlCallbackRoutine -  Callback routine provided by HDD.
                               Used for success/failure notification by SME
    \param  enterWowlCallbackContext - A cookie passed by HDD, that is passed back to HDD
                              at the time of callback.
    \param  wakeReasonIndCB -  Callback routine provided by HDD.
                               Used for Wake Reason Indication by SME
    \param  wakeReasonIndCBContext - A cookie passed by HDD, that is passed back to HDD
                              at the time of callback.
    \return eHalStatus
            eHAL_STATUS_SUCCESS  Device is already in WoWLAN mode
            eHAL_STATUS_FAILURE  Device cannot enter WoWLAN mode.
            eHAL_STATUS_PMC_PENDING  Request accepted. SME will enable WOWL after
                                      BMPS mode is entered.
  ---------------------------------------------------------------------------*/
eHalStatus sme_EnterWowl (
    tHalHandle hHal,
    void (*enterWowlCallbackRoutine) (void *callbackContext, eHalStatus status),
    void *enterWowlCallbackContext,
#ifdef WLAN_WAKEUP_EVENTS
    void (*wakeIndicationCB) (void *callbackContext, tpSirWakeReasonInd pWakeReasonInd),
    void *wakeIndicationCBContext,
#endif // WLAN_WAKEUP_EVENTS
    tpSirSmeWowlEnterParams wowlEnterParams, tANI_U8 sessionId)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    MTRACE(vos_trace(VOS_MODULE_ID_SME,
           TRACE_CODE_SME_RX_HDD_ENTER_WOWL, sessionId, 0));
    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
       status = pmcEnterWowl (hHal, enterWowlCallbackRoutine, enterWowlCallbackContext,
#ifdef WLAN_WAKEUP_EVENTS
                              wakeIndicationCB, wakeIndicationCBContext,
#endif // WLAN_WAKEUP_EVENTS
                              wowlEnterParams, sessionId);
       sme_ReleaseGlobalLock( &pMac->sme );
    }
    return (status);
}
/* ---------------------------------------------------------------------------
    \fn sme_ExitWowl
    \brief  This is the SME API exposed to HDD to request exit from WoWLAN mode.
            SME will initiate exit from WoWLAN mode and device will be put in BMPS
            mode.
    \param  hHal - The handle returned by macOpen.
    \return eHalStatus
            eHAL_STATUS_FAILURE  Device cannot exit WoWLAN mode.
            eHAL_STATUS_SUCCESS  Request accepted to exit WoWLAN mode.
  ---------------------------------------------------------------------------*/
eHalStatus sme_ExitWowl (tHalHandle hHal, tWowlExitSource wowlExitSrc)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    MTRACE(vos_trace(VOS_MODULE_ID_SME,
           TRACE_CODE_SME_RX_HDD_EXIT_WOWL, NO_SESSION, 0));
    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
       status = pmcExitWowl (hHal, wowlExitSrc);
       sme_ReleaseGlobalLock( &pMac->sme );
    }

    return (status);
}

/* ---------------------------------------------------------------------------

    \fn sme_RoamSetKey

    \brief To set encryption key. This function should be called only when connected
    This is an asynchronous API.

    \param pSetKeyInfo - pointer to a caller allocated object of tCsrSetContextInfo

    \param pRoamId  Upon success return, this is the id caller can use to identify the request in roamcallback

    \return eHalStatus  SUCCESS  Roam callback will be called indicate actually results

                         FAILURE or RESOURCES  The API finished and failed.

  -------------------------------------------------------------------------------*/
eHalStatus sme_RoamSetKey(tHalHandle hHal, tANI_U8 sessionId, tCsrRoamSetKey *pSetKey, tANI_U32 *pRoamId)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   tANI_U32 roamId;
   tANI_U32 i;
   tCsrRoamSession *pSession = NULL;

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
          TRACE_CODE_SME_RX_HDD_SET_KEY, sessionId, 0));
   if (pSetKey->keyLength > CSR_MAX_KEY_LEN)
   {
      smsLog(pMac, LOGE, FL("Invalid key length %d"), pSetKey->keyLength);
      return eHAL_STATUS_FAILURE;
   }
   /*Once Setkey is done, we can go in BMPS*/
   if(pSetKey->keyLength)
   {
     pMac->pmc.remainInPowerActiveTillDHCP = FALSE;
     smsLog(pMac, LOG1, FL("Reset remainInPowerActiveTillDHCP"
                           " to allow BMPS"));
   }

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
      roamId = GET_NEXT_ROAM_ID(&pMac->roam);
      if(pRoamId)
      {
         *pRoamId = roamId;
      }

      smsLog(pMac, LOG2, FL("keyLength %d"), pSetKey->keyLength);

      for(i=0; i<pSetKey->keyLength; i++)
          smsLog(pMac, LOG2, FL("%02x"), pSetKey->Key[i]);

      smsLog(pMac, LOG2, "\n sessionId=%d roamId=%d", sessionId, roamId);

      pSession = CSR_GET_SESSION(pMac, sessionId);

      if(!pSession)
      {
         smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
         sme_ReleaseGlobalLock( &pMac->sme );
         return eHAL_STATUS_FAILURE;
      }

      if(CSR_IS_INFRA_AP(&pSession->connectedProfile))
      {
         if(pSetKey->keyDirection == eSIR_TX_DEFAULT)
         {
            if ( ( eCSR_ENCRYPT_TYPE_WEP40 == pSetKey->encType ) ||
                 ( eCSR_ENCRYPT_TYPE_WEP40_STATICKEY == pSetKey->encType ))
            {
               pSession->pCurRoamProfile->negotiatedUCEncryptionType = eCSR_ENCRYPT_TYPE_WEP40_STATICKEY;
            }
            if ( ( eCSR_ENCRYPT_TYPE_WEP104 == pSetKey->encType ) ||
                 ( eCSR_ENCRYPT_TYPE_WEP104_STATICKEY == pSetKey->encType ))
            {
               pSession->pCurRoamProfile->negotiatedUCEncryptionType = eCSR_ENCRYPT_TYPE_WEP104_STATICKEY;
            }
         }
      }

      status = csrRoamSetKey ( pMac, sessionId, pSetKey, roamId );
      sme_ReleaseGlobalLock( &pMac->sme );
   }
#ifdef DEBUG_ROAM_DELAY
   //Store sent PTK key time
   if(pSetKey->keyDirection == eSIR_TX_RX)
   {
      vos_record_roam_event(e_HDD_SET_PTK_REQ, NULL, 0);
   }
   else if(pSetKey->keyDirection == eSIR_RX_ONLY)
   {
      vos_record_roam_event(e_HDD_SET_GTK_REQ, NULL, 0);
   }
   else
   {
      return (status);
   }
#endif

   return (status);
}


/* ---------------------------------------------------------------------------

    \fn sme_RoamRemoveKey

    \brief To set encryption key. This is an asynchronous API.

    \param pRemoveKey - pointer to a caller allocated object of tCsrRoamRemoveKey

    \param pRoamId  Upon success return, this is the id caller can use to identify the request in roamcallback

    \return eHalStatus  SUCCESS  Roam callback will be called indicate actually results

                         FAILURE or RESOURCES  The API finished and failed.

  -------------------------------------------------------------------------------*/
eHalStatus sme_RoamRemoveKey(tHalHandle hHal, tANI_U8 sessionId,
                             tCsrRoamRemoveKey *pRemoveKey, tANI_U32 *pRoamId)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   tANI_U32 roamId;

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
          TRACE_CODE_SME_RX_HDD_REMOVE_KEY, sessionId, 0));
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
      roamId = GET_NEXT_ROAM_ID(&pMac->roam);
      if(pRoamId)
      {
         *pRoamId = roamId;
      }
      status = csrRoamIssueRemoveKeyCommand( pMac, sessionId, pRemoveKey, roamId );
      sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_GetRssi
    \brief a wrapper function that client calls to register a callback to get RSSI

    \param callback - SME sends back the requested stats using the callback
    \param staId - The station ID for which the stats is requested for
    \param pContext - user context to be passed back along with the callback
    \param pVosContext - vos context
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_GetRssi(tHalHandle hHal,
                             tCsrRssiCallback callback,
                             tANI_U8 staId, tCsrBssid bssId,
                             void *pContext, void* pVosContext)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
      status = csrGetRssi( pMac, callback,
                                 staId, bssId, pContext, pVosContext);
      sme_ReleaseGlobalLock( &pMac->sme );
   }
   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_GetSnr
    \brief a wrapper function that client calls to register a callback to
           get SNR

    \param callback - SME sends back the requested stats using the callback
    \param staId - The station ID for which the stats is requested for
    \param pContext - user context to be passed back along with the callback
    \param pVosContext - vos context
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_GetSnr(tHalHandle hHal,
                      tCsrSnrCallback callback,
                      tANI_U8 staId, tCsrBssid bssId,
                      void *pContext)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
      status = csrGetSnr(pMac, callback,
                          staId, bssId, pContext);
      sme_ReleaseGlobalLock( &pMac->sme );
   }
   return status;
}
#if defined WLAN_FEATURE_VOWIFI_11R || defined FEATURE_WLAN_ESE || defined(FEATURE_WLAN_LFR)
/* ---------------------------------------------------------------------------
    \fn sme_GetRoamRssi
    \brief a wrapper function that client calls to register a callback to get Roam RSSI

    \param callback - SME sends back the requested stats using the callback
    \param staId - The station ID for which the stats is requested for
    \param pContext - user context to be passed back along with the callback
    \param pVosContext - vos context
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_GetRoamRssi(tHalHandle hHal,
                             tCsrRssiCallback callback,
                             tANI_U8 staId, tCsrBssid bssId,
                             void *pContext, void* pVosContext)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
      status = csrGetRoamRssi( pMac, callback,
                                 staId, bssId, pContext, pVosContext);
      sme_ReleaseGlobalLock( &pMac->sme );
   }
   return (status);
}
#endif


#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
/* ---------------------------------------------------------------------------
    \fn sme_GetTsmStats
    \brief a wrapper function that client calls to register a callback to get TSM Stats

    \param callback - SME sends back the requested stats using the callback
    \param staId - The station ID for which the stats is requested for
    \param pContext - user context to be passed back along with the callback
    \param pVosContext - vos context
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_GetTsmStats(tHalHandle hHal,
                             tCsrTsmStatsCallback callback,
                             tANI_U8 staId, tCsrBssid bssId,
                             void *pContext, void* pVosContext, tANI_U8 tid)
{
   eHalStatus     status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
      status = csrGetTsmStats( pMac, callback,
                                 staId, bssId, pContext, pVosContext, tid);
      sme_ReleaseGlobalLock( &pMac->sme );
   }
   return (status);
}
#endif


/* ---------------------------------------------------------------------------
    \fn sme_GetStatistics
    \brief a wrapper function that client calls to register a callback to get
    different PHY level statistics from CSR.

    \param requesterId - different client requesting for statistics, HDD, UMA/GAN etc
    \param statsMask - The different category/categories of stats requester is looking for
    \param callback - SME sends back the requested stats using the callback
    \param periodicity - If requester needs periodic update in millisec, 0 means
                         it's an one time request
    \param cache - If requester is happy with cached stats
    \param staId - The station ID for which the stats is requested for
    \param pContext - user context to be passed back along with the callback
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_GetStatistics(tHalHandle hHal, eCsrStatsRequesterType requesterId,
                             tANI_U32 statsMask,
                             tCsrStatsCallback callback,
                             tANI_U32 periodicity, tANI_BOOLEAN cache,
                             tANI_U8 staId, void *pContext)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
                TRACE_CODE_SME_RX_HDD_GET_STATS, NO_SESSION,  periodicity));
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
      status = csrGetStatistics( pMac, requesterId , statsMask, callback,
                                 periodicity, cache, staId, pContext);
      sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);

}

eHalStatus sme_GetFwStats(tHalHandle hHal, tANI_U32 stats, void *data,
                                                        void *callback)
{
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   vos_msg_t msg;
   tSirFWStatsGetReq *pGetFWStatsReq;

   smsLog(pMac, LOG1, FL(" ENTER stats = %d "),stats);

   if ( eHAL_STATUS_SUCCESS ==  sme_AcquireGlobalLock( &pMac->sme ))
   {
       pGetFWStatsReq = (tSirFWStatsGetReq *)vos_mem_malloc(sizeof(tSirFWStatsGetReq));
       if ( NULL == pGetFWStatsReq)
       {
          smsLog(pMac, LOGE, FL("Not able to allocate memory for "
               "WDA_FW_STATS_GET_REQ"));
          sme_ReleaseGlobalLock( &pMac->sme );
          return eHAL_STATUS_FAILURE;
       }
       pGetFWStatsReq->stats = stats;
       pGetFWStatsReq->callback = (tSirFWStatsCallback)callback;
       pGetFWStatsReq->data = data;

       msg.type = WDA_FW_STATS_GET_REQ;
       msg.reserved = 0;
       msg.bodyptr = pGetFWStatsReq;
       if(VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MQ_ID_WDA, &msg))
       {
            smsLog(pMac, LOGE,
              FL("Not able to post WDA_FW_STATS_GET_REQ message to HAL"));
            vos_mem_free(pGetFWStatsReq);
            sme_ReleaseGlobalLock( &pMac->sme );
            return eHAL_STATUS_FAILURE;
       }
       sme_ReleaseGlobalLock( &pMac->sme );
       return eHAL_STATUS_SUCCESS;
   }
   return eHAL_STATUS_FAILURE;
}

/* ---------------------------------------------------------------------------
    \fn smeGetTLSTAState
    \helper function to get the TL STA State whenever the function is called.

    \param staId - The staID to be passed to the TL
            to get the relevant TL STA State
    \return the state as tANI_U16
  ---------------------------------------------------------------------------*/
tANI_U16 smeGetTLSTAState(tHalHandle hHal, tANI_U8 staId)
{
   tANI_U16 tlSTAState = TL_INIT_STATE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   eHalStatus status = eHAL_STATUS_FAILURE;

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
      tlSTAState = csrGetTLSTAState( pMac, staId);
      sme_ReleaseGlobalLock( &pMac->sme );
   }

   return tlSTAState;
}

/* ---------------------------------------------------------------------------

    \fn sme_GetCountryCode

    \brief To return the current country code. If no country code is applied, default country code is
    used to fill the buffer.
    If 11d supported is turned off, an error is return and the last applied/default country code is used.
    This is a synchronous API.

    \param pBuf - pointer to a caller allocated buffer for returned country code.

    \param pbLen  For input, this parameter indicates how big is the buffer.
                   Upon return, this parameter has the number of bytes for country. If pBuf
                   doesn't have enough space, this function returns
                   fail status and this parameter contains the number that is needed.

    \return eHalStatus  SUCCESS.

                         FAILURE or RESOURCES  The API finished and failed.

  -------------------------------------------------------------------------------*/
eHalStatus sme_GetCountryCode(tHalHandle hHal, tANI_U8 *pBuf, tANI_U8 *pbLen)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
                 TRACE_CODE_SME_RX_HDD_GET_CNTRYCODE, NO_SESSION, 0));

    return ( csrGetCountryCode( pMac, pBuf, pbLen ) );
}


/* ---------------------------------------------------------------------------

    \fn sme_SetCountryCode

    \brief To change the current/default country code.
    If 11d supported is turned off, an error is return.
    This is a synchronous API.

    \param pCountry - pointer to a caller allocated buffer for the country code.

    \param pfRestartNeeded  A pointer to caller allocated memory, upon successful return, it indicates
    whether a reset is required.

    \return eHalStatus  SUCCESS.

                         FAILURE or RESOURCES  The API finished and failed.

  -------------------------------------------------------------------------------*/
eHalStatus sme_SetCountryCode(tHalHandle hHal, tANI_U8 *pCountry, tANI_BOOLEAN *pfRestartNeeded)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
              TRACE_CODE_SME_RX_HDD_SET_CNTRYCODE, NO_SESSION, 0));
    return ( csrSetCountryCode( pMac, pCountry, pfRestartNeeded ) );
}


/* ---------------------------------------------------------------------------
    \fn sme_ResetCountryCodeInformation
    \brief this function is to reset the country code current being used back to EEPROM default
    this includes channel list and power setting. This is a synchronous API.
    \param pfRestartNeeded - pointer to a caller allocated space. Upon successful return, it indicates whether
    a restart is needed to apply the change
    \return eHalStatus
  -------------------------------------------------------------------------------*/
eHalStatus sme_ResetCountryCodeInformation(tHalHandle hHal, tANI_BOOLEAN *pfRestartNeeded)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    return ( csrResetCountryCodeInformation( pMac, pfRestartNeeded ) );
}


/* ---------------------------------------------------------------------------
    \fn sme_GetSupportedCountryCode
    \brief this function is to get a list of the country code current being supported
    \param pBuf - Caller allocated buffer with at least 3 bytes, upon success return,
    this has the country code list. 3 bytes for each country code. This may be NULL if
    caller wants to know the needed byte count.
    \param pbLen - Caller allocated, as input, it indicates the length of pBuf. Upon success return,
    this contains the length of the data in pBuf. If pbuf is NULL, as input, *pbLen should be 0.
    \return eHalStatus
  -------------------------------------------------------------------------------*/
eHalStatus sme_GetSupportedCountryCode(tHalHandle hHal, tANI_U8 *pBuf, tANI_U32 *pbLen)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    return ( csrGetSupportedCountryCode( pMac, pBuf, pbLen ) );
}


/* ---------------------------------------------------------------------------
    \fn sme_GetCurrentRegulatoryDomain
    \brief this function is to get the current regulatory domain. This is a synchronous API.
    This function must be called after CFG is downloaded and all the band/mode setting already passed into
    SME. The function fails if 11d support is turned off.
    \param pDomain - Caller allocated buffer to return the current domain.
    \return eHalStatus  SUCCESS.

                         FAILURE or RESOURCES  The API finished and failed.
  -------------------------------------------------------------------------------*/
eHalStatus sme_GetCurrentRegulatoryDomain(tHalHandle hHal, v_REGDOMAIN_t *pDomain)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;

    if( pDomain )
    {
        if( csrIs11dSupported( pMac ) )
        {
            *pDomain = csrGetCurrentRegulatoryDomain( pMac );
            status = eHAL_STATUS_SUCCESS;
        }
        else
        {
            status = eHAL_STATUS_FAILURE;
        }
    }

    return ( status );
}


/* ---------------------------------------------------------------------------
    \fn sme_SetRegulatoryDomain
    \brief this function is to set the current regulatory domain.
    This function must be called after CFG is downloaded and all the band/mode setting already passed into
    SME. This is a synchronous API.
    \param domainId - indicate the domain (defined in the driver) needs to set to.
    See v_REGDOMAIN_t for definition
    \param pfRestartNeeded - pointer to a caller allocated space. Upon successful return, it indicates whether
    a restart is needed to apply the change
    \return eHalStatus
  -------------------------------------------------------------------------------*/
eHalStatus sme_SetRegulatoryDomain(tHalHandle hHal, v_REGDOMAIN_t domainId, tANI_BOOLEAN *pfRestartNeeded)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    return ( csrSetRegulatoryDomain( pMac, domainId, pfRestartNeeded ) );
}


/* ---------------------------------------------------------------------------

    \fn sme_GetRegulatoryDomainForCountry

    \brief To return a regulatory domain base on a country code. This is a synchronous API.

    \param pCountry - pointer to a caller allocated buffer for input country code.

    \param pDomainId  Upon successful return, it is the domain that country belongs to.
    If it is NULL, returning success means that the country code is known.

    \return eHalStatus  SUCCESS.

                         FAILURE or RESOURCES  The API finished and failed.

  -------------------------------------------------------------------------------*/
eHalStatus sme_GetRegulatoryDomainForCountry(tHalHandle hHal, tANI_U8 *pCountry, v_REGDOMAIN_t *pDomainId)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    return csrGetRegulatoryDomainForCountry(pMac, pCountry, pDomainId,
                                            COUNTRY_QUERY);
}




/* ---------------------------------------------------------------------------

    \fn sme_GetSupportedRegulatoryDomains

    \brief To return a list of supported regulatory domains. This is a synchronous API.

    \param pDomains - pointer to a caller allocated buffer for returned regulatory domains.

    \param pNumDomains  For input, this parameter indicates howm many domains pDomains can hold.
                         Upon return, this parameter has the number for supported domains. If pDomains
                         doesn't have enough space for all the supported domains, this function returns
                         fail status and this parameter contains the number that is needed.

    \return eHalStatus  SUCCESS.

                         FAILURE or RESOURCES  The API finished and failed.

  -------------------------------------------------------------------------------*/
eHalStatus sme_GetSupportedRegulatoryDomains(tHalHandle hHal, v_REGDOMAIN_t *pDomains, tANI_U32 *pNumDomains)
{
    eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;

    //We support all domains for now
    if( pNumDomains )
    {
        if( NUM_REG_DOMAINS <= *pNumDomains )
        {
            status = eHAL_STATUS_SUCCESS;
        }
        *pNumDomains = NUM_REG_DOMAINS;
    }
    if( HAL_STATUS_SUCCESS( status ) )
    {
        if( pDomains )
        {
            pDomains[0] = REGDOMAIN_FCC;
            pDomains[1] = REGDOMAIN_ETSI;
            pDomains[2] = REGDOMAIN_JAPAN;
            pDomains[3] = REGDOMAIN_WORLD;
            pDomains[4] = REGDOMAIN_N_AMER_EXC_FCC;
            pDomains[5] = REGDOMAIN_APAC;
            pDomains[6] = REGDOMAIN_KOREA;
            pDomains[7] = REGDOMAIN_HI_5GHZ;
            pDomains[8] = REGDOMAIN_NO_5GHZ;
        }
        else
        {
            status = eHAL_STATUS_INVALID_PARAMETER;
        }
    }

    return ( status );
}


//some support functions
tANI_BOOLEAN sme_Is11dSupported(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    return ( csrIs11dSupported( pMac ) );
}


tANI_BOOLEAN sme_Is11hSupported(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    return ( csrIs11hSupported( pMac ) );
}


tANI_BOOLEAN sme_IsWmmSupported(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    return ( csrIsWmmSupported( pMac ) );
}

//Upper layer to get the list of the base channels to scan for passively 11d info from csr
eHalStatus sme_ScanGetBaseChannels( tHalHandle hHal, tCsrChannelInfo * pChannelInfo )
{
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   return(csrScanGetBaseChannels(pMac,pChannelInfo) );
}

/* ---------------------------------------------------------------------------

    \fn sme_ChangeCountryCode

    \brief Change Country code from upperlayer during WLAN driver operation.
           This is a synchronous API.

    \param hHal - The handle returned by macOpen.

    \param pCountry New Country Code String

    \param sendRegHint If we want to send reg hint to nl80211

    \return eHalStatus  SUCCESS.

                         FAILURE or RESOURCES  The API finished and failed.

  -------------------------------------------------------------------------------*/
eHalStatus sme_ChangeCountryCode( tHalHandle hHal,
                                          tSmeChangeCountryCallback callback,
                                          tANI_U8 *pCountry,
                                          void *pContext,
                                          void* pVosContext,
                                          tAniBool countryFromUserSpace,
                                          tAniBool sendRegHint )
{
   eHalStatus                status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal            pMac = PMAC_STRUCT( hHal );
   vos_msg_t                 msg;
   tAniChangeCountryCodeReq *pMsg;

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
            TRACE_CODE_SME_RX_HDD_CHANGE_CNTRYCODE, NO_SESSION, 0));
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
      smsLog(pMac, LOG1, FL(" called"));

      if ((pMac->roam.configParam.Is11dSupportEnabledOriginal == true) &&
          (!pMac->roam.configParam.fSupplicantCountryCodeHasPriority))
      {

          smsLog(pMac, LOGW, "Set Country Code Fail since the STA is associated and userspace does not have priority ");

	  sme_ReleaseGlobalLock( &pMac->sme );
          status = eHAL_STATUS_FAILURE;
          return status;
      }

      pMsg = vos_mem_malloc(sizeof(tAniChangeCountryCodeReq));
      if ( NULL == pMsg )
      {
         smsLog(pMac, LOGE, " csrChangeCountryCode: failed to allocate mem for req");
         sme_ReleaseGlobalLock( &pMac->sme );
         return eHAL_STATUS_FAILURE;
      }

      pMsg->msgType = pal_cpu_to_be16((tANI_U16)eWNI_SME_CHANGE_COUNTRY_CODE);
      pMsg->msgLen = (tANI_U16)sizeof(tAniChangeCountryCodeReq);
      vos_mem_copy(pMsg->countryCode, pCountry, 3);
      pMsg->countryFromUserSpace = countryFromUserSpace;
      pMsg->sendRegHint = sendRegHint;
      pMsg->changeCCCallback = callback;
      pMsg->pDevContext = pContext;
      pMsg->pVosContext = pVosContext;

      msg.type = eWNI_SME_CHANGE_COUNTRY_CODE;
      msg.bodyptr = pMsg;
      msg.reserved = 0;

      if(VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MQ_ID_SME, &msg))
      {
          smsLog(pMac, LOGE, " sme_ChangeCountryCode failed to post msg to self ");
          vos_mem_free((void *)pMsg);
          status = eHAL_STATUS_FAILURE;
      }
      smsLog(pMac, LOG1, FL(" returned"));
      sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/*--------------------------------------------------------------------------

    \fn sme_GenericChangeCountryCode

    \brief Change Country code from upperlayer during WLAN driver operation.
           This is a synchronous API.

    \param hHal - The handle returned by macOpen.

    \param pCountry New Country Code String

    \param reg_domain regulatory domain

    \return eHalStatus  SUCCESS.

                         FAILURE or RESOURCES  The API finished and failed.

-----------------------------------------------------------------------------*/
eHalStatus sme_GenericChangeCountryCode( tHalHandle hHal,
                                         tANI_U8 *pCountry,
                                         v_REGDOMAIN_t reg_domain)
{
    eHalStatus                status = eHAL_STATUS_FAILURE;
    tpAniSirGlobal            pMac = PMAC_STRUCT( hHal );
    vos_msg_t                 msg;
    tAniGenericChangeCountryCodeReq *pMsg;

    if (NULL == pMac)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL,
            "%s: pMac is null", __func__);
        return status;
    }

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        smsLog(pMac, LOG1, FL(" called"));
        pMsg = vos_mem_malloc(sizeof(tAniGenericChangeCountryCodeReq));
        if (NULL == pMsg)
        {
            smsLog(pMac, LOGE, " sme_GenericChangeCountryCode: failed to allocate mem for req");
            sme_ReleaseGlobalLock( &pMac->sme );
            return eHAL_STATUS_FAILURE;
        }

        pMsg->msgType = pal_cpu_to_be16((tANI_U16)eWNI_SME_GENERIC_CHANGE_COUNTRY_CODE);
        pMsg->msgLen = (tANI_U16)sizeof(tAniGenericChangeCountryCodeReq);
        vos_mem_copy(pMsg->countryCode, pCountry, 2);

        pMsg->countryCode[2] = ' '; /* For ASCII space */

        pMsg->domain_index = reg_domain;

        msg.type = eWNI_SME_GENERIC_CHANGE_COUNTRY_CODE;
        msg.bodyptr = pMsg;
        msg.reserved = 0;

        if (VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MQ_ID_SME, &msg))
        {
            smsLog(pMac, LOGE, "sme_GenericChangeCountryCode failed to post msg to self");
            vos_mem_free(pMsg);
            status = eHAL_STATUS_FAILURE;
        }
        smsLog(pMac, LOG1, FL(" returned"));
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return (status);
}

/* ---------------------------------------------------------------------------

    \fn sme_InitChannels

    \brief Used to initialize CSR channel lists while driver loading

    \param hHal - global pMac structure

    \return eHalStatus  SUCCESS.

                         FAILURE or RESOURCES  The API finished and failed.

 -------------------------------------------------------------------------------*/
eHalStatus sme_InitChannels(tHalHandle hHal)
{
    eHalStatus          status = eHAL_STATUS_FAILURE;
    tpAniSirGlobal      pMac = PMAC_STRUCT(hHal);

    if (NULL == pMac)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL,
            "%s: pMac is null", __func__);
        return status;
    }

    status = csrInitChannels(pMac);

    return status;
}

#ifdef CONFIG_ENABLE_LINUX_REG
/*-------------------------------------------------------------------------
    \fn sme_InitChannelsForCC

    \brief Used to issue regulatory hint to user

    \param hHal - global pMac structure

    \return eHalStatus  SUCCESS.

                         FAILURE or RESOURCES  The API finished and failed.
--------------------------------------------------------------------------*/

eHalStatus sme_InitChannelsForCC(tHalHandle hHal, driver_load_type init)
{
    eHalStatus          status = eHAL_STATUS_FAILURE;
    tpAniSirGlobal      pMac = PMAC_STRUCT(hHal);

    if (NULL == pMac)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL,
            "%s: pMac is null", __func__);
        return status;
    }
    status = csrInitChannelsForCC(pMac, init);

    return status;
}
#endif

/* ---------------------------------------------------------------------------

    \fn sme_DHCPStartInd

    \brief API to signal the FW about the DHCP Start event.

    \param hHal - HAL handle for device.

    \param device_mode - mode(AP,SAP etc) of the device.

    \param macAddr  - MAC address of the device.

    \return eHalStatus  SUCCESS.

                         FAILURE or RESOURCES  The API finished and failed.
  --------------------------------------------------------------------------*/
eHalStatus sme_DHCPStartInd( tHalHandle hHal,
                                   tANI_U8 device_mode,
                                   tANI_U8 sessionId )
{
    eHalStatus          status;
    VOS_STATUS          vosStatus;
    tpAniSirGlobal      pMac = PMAC_STRUCT( hHal );
    vos_msg_t           vosMessage;
    tAniDHCPInd         *pMsg;
    tCsrRoamSession     *pSession;

    status = sme_AcquireGlobalLock(&pMac->sme);
    if ( eHAL_STATUS_SUCCESS == status)
    {
        pSession = CSR_GET_SESSION( pMac, sessionId );

        if (!pSession)
        {
            smsLog(pMac, LOGE, FL("session %d not found "), sessionId);
            sme_ReleaseGlobalLock( &pMac->sme );
            return eHAL_STATUS_FAILURE;
        }

        pMsg = (tAniDHCPInd*)vos_mem_malloc(sizeof(tAniDHCPInd));
        if (NULL == pMsg)
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                   "%s: Not able to allocate memory for dhcp start", __func__);
            sme_ReleaseGlobalLock( &pMac->sme );
            return eHAL_STATUS_FAILURE;
        }
        pMsg->msgType = WDA_DHCP_START_IND;
        pMsg->msgLen = (tANI_U16)sizeof(tAniDHCPInd);
        pMsg->device_mode = device_mode;
        vos_mem_copy(pMsg->macAddr, pSession->connectedProfile.bssid,
                                         sizeof(tSirMacAddr));
        vosMessage.type = WDA_DHCP_START_IND;
        vosMessage.bodyptr = pMsg;
        vosMessage.reserved = 0;

        vosStatus = vos_mq_post_message( VOS_MQ_ID_WDA, &vosMessage );
        if ( !VOS_IS_STATUS_SUCCESS(vosStatus) )
        {
           VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                         "%s: Post DHCP Start MSG fail", __func__);
           vos_mem_free(pMsg);
           status = eHAL_STATUS_FAILURE;
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }
    return (status);
}
/* ---------------------------------------------------------------------------
    \fn sme_DHCPStopInd

    \brief API to signal the FW about the DHCP complete event.

    \param hHal - HAL handle for device.

    \param device_mode - mode(AP, SAP etc) of the device.

    \param macAddr  - MAC address of the device.

    \return eHalStatus  SUCCESS.
                         FAILURE or RESOURCES  The API finished and failed.
  --------------------------------------------------------------------------*/
eHalStatus sme_DHCPStopInd( tHalHandle hHal,
                              tANI_U8 device_mode,
                              tANI_U8 sessionId )
{
    eHalStatus          status;
    VOS_STATUS          vosStatus;
    tpAniSirGlobal      pMac = PMAC_STRUCT( hHal );
    vos_msg_t           vosMessage;
    tAniDHCPInd         *pMsg;
    tCsrRoamSession     *pSession;

    status = sme_AcquireGlobalLock(&pMac->sme);
    if ( eHAL_STATUS_SUCCESS == status)
    {
        pSession = CSR_GET_SESSION( pMac, sessionId );

        if (!pSession)
        {
            smsLog(pMac, LOGE, FL("session %d not found "), sessionId);
            sme_ReleaseGlobalLock( &pMac->sme );
            return eHAL_STATUS_FAILURE;
        }

        pMsg = (tAniDHCPInd*)vos_mem_malloc(sizeof(tAniDHCPInd));
        if (NULL == pMsg)
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                     "%s: Not able to allocate memory for dhcp stop", __func__);
            sme_ReleaseGlobalLock( &pMac->sme );
            return eHAL_STATUS_FAILURE;
       }

       pMsg->msgType = WDA_DHCP_STOP_IND;
       pMsg->msgLen = (tANI_U16)sizeof(tAniDHCPInd);
       pMsg->device_mode = device_mode;
       vos_mem_copy(pMsg->macAddr, pSession->connectedProfile.bssid,
                                         sizeof(tSirMacAddr));

       vosMessage.type = WDA_DHCP_STOP_IND;
       vosMessage.bodyptr = pMsg;
       vosMessage.reserved = 0;

       vosStatus = vos_mq_post_message( VOS_MQ_ID_WDA, &vosMessage );
       if ( !VOS_IS_STATUS_SUCCESS(vosStatus) )
       {
           VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                        "%s: Post DHCP Stop MSG fail", __func__);
           vos_mem_free(pMsg);
           status = eHAL_STATUS_FAILURE;
       }

       sme_ReleaseGlobalLock( &pMac->sme );
    }
    return (status);
}


/* ---------------------------------------------------------------------------
    \fn sme_BtcSignalBtEvent
    \brief  API to signal Bluetooth (BT) event to the WLAN driver. Based on the
            BT event type and the current operating mode of Libra (full power,
            BMPS, UAPSD etc), appropriate Bluetooth Coexistence (BTC) strategy
            would be employed.
    \param  hHal - The handle returned by macOpen.
    \param  pBtEvent -  Pointer to a caller allocated object of type tSmeBtEvent
                        Caller owns the memory and is responsible for freeing it.
    \return VOS_STATUS
            VOS_STATUS_E_FAILURE  BT Event not passed to HAL. This can happen
                                   if BTC execution mode is set to BTC_WLAN_ONLY
                                   or BTC_PTA_ONLY.
            VOS_STATUS_SUCCESS    BT Event passed to HAL
  ---------------------------------------------------------------------------*/
VOS_STATUS sme_BtcSignalBtEvent (tHalHandle hHal, tpSmeBtEvent pBtEvent)
{
    VOS_STATUS status = VOS_STATUS_E_FAILURE;

#ifndef WLAN_MDM_CODE_REDUCTION_OPT
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
           TRACE_CODE_SME_RX_HDD_BTC_SIGNALEVENT, NO_SESSION, 0));
    if ( eHAL_STATUS_SUCCESS == sme_AcquireGlobalLock( &pMac->sme ) )
    {
        status = btcSignalBTEvent (hHal, pBtEvent);
        sme_ReleaseGlobalLock( &pMac->sme );
    }
#endif
    return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_BtcSetConfig
    \brief  API to change the current Bluetooth Coexistence (BTC) configuration
            This function should be invoked only after CFG download has completed.
            Calling it after sme_HDDReadyInd is recommended.
    \param  hHal - The handle returned by macOpen.
    \param  pSmeBtcConfig - Pointer to a caller allocated object of type tSmeBtcConfig.
                            Caller owns the memory and is responsible for freeing it.
    \return VOS_STATUS
            VOS_STATUS_E_FAILURE  Config not passed to HAL.
            VOS_STATUS_SUCCESS  Config passed to HAL
  ---------------------------------------------------------------------------*/
VOS_STATUS sme_BtcSetConfig (tHalHandle hHal, tpSmeBtcConfig pSmeBtcConfig)
{
    VOS_STATUS status = VOS_STATUS_E_FAILURE;
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    MTRACE(vos_trace(VOS_MODULE_ID_SME,
                 TRACE_CODE_SME_RX_HDD_BTC_SETCONFIG, NO_SESSION, 0));
    if ( eHAL_STATUS_SUCCESS == sme_AcquireGlobalLock( &pMac->sme ) )
    {
        status = btcSetConfig (hHal, pSmeBtcConfig);
        sme_ReleaseGlobalLock( &pMac->sme );
    }
#endif
    return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_BtcGetConfig
    \brief  API to retrieve the current Bluetooth Coexistence (BTC) configuration
    \param  hHal - The handle returned by macOpen.
    \param  pSmeBtcConfig - Pointer to a caller allocated object of type
                            tSmeBtcConfig. Caller owns the memory and is responsible
                            for freeing it.
    \return VOS_STATUS
            VOS_STATUS_E_FAILURE - failure
            VOS_STATUS_SUCCESS  success
  ---------------------------------------------------------------------------*/
VOS_STATUS sme_BtcGetConfig (tHalHandle hHal, tpSmeBtcConfig pSmeBtcConfig)
{
    VOS_STATUS status = VOS_STATUS_E_FAILURE;
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
             TRACE_CODE_SME_RX_HDD_BTC_GETCONFIG, NO_SESSION, 0));
    if ( eHAL_STATUS_SUCCESS == sme_AcquireGlobalLock( &pMac->sme ) )
    {
        status = btcGetConfig (hHal, pSmeBtcConfig);
        sme_ReleaseGlobalLock( &pMac->sme );
    }
#endif
    return (status);
}
/* ---------------------------------------------------------------------------
    \fn sme_SetCfgPrivacy
    \brief  API to set configure privacy parameters
    \param  hHal - The handle returned by macOpen.
    \param  pProfile - Pointer CSR Roam profile.
    \param  fPrivacy - This parameter indicates status of privacy

    \return void
  ---------------------------------------------------------------------------*/
void sme_SetCfgPrivacy( tHalHandle hHal,
                        tCsrRoamProfile *pProfile,
                        tANI_BOOLEAN fPrivacy
                        )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    MTRACE(vos_trace(VOS_MODULE_ID_SME,
               TRACE_CODE_SME_RX_HDD_SET_CFGPRIVACY, NO_SESSION, 0));
    if ( eHAL_STATUS_SUCCESS == sme_AcquireGlobalLock( &pMac->sme ) )
    {
        csrSetCfgPrivacy(pMac, pProfile, fPrivacy);
        sme_ReleaseGlobalLock( &pMac->sme );
    }
}

#if defined WLAN_FEATURE_VOWIFI
/* ---------------------------------------------------------------------------
    \fn sme_NeighborReportRequest
    \brief  API to request neighbor report.
    \param  hHal - The handle returned by macOpen.
    \param  pRrmNeighborReq - Pointer to a caller allocated object of type
                            tRrmNeighborReq. Caller owns the memory and is responsible
                            for freeing it.
    \return VOS_STATUS
            VOS_STATUS_E_FAILURE - failure
            VOS_STATUS_SUCCESS  success
  ---------------------------------------------------------------------------*/
VOS_STATUS sme_NeighborReportRequest (tHalHandle hHal, tANI_U8 sessionId,
                                    tpRrmNeighborReq pRrmNeighborReq, tpRrmNeighborRspCallbackInfo callbackInfo)
{
    VOS_STATUS status = VOS_STATUS_E_FAILURE;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    MTRACE(vos_trace(VOS_MODULE_ID_SME,
                 TRACE_CODE_SME_RX_HDD_NEIGHBOR_REPORTREQ, NO_SESSION, 0));

    if ( eHAL_STATUS_SUCCESS == sme_AcquireGlobalLock( &pMac->sme ) )
    {
        status = sme_RrmNeighborReportRequest (hHal, sessionId, pRrmNeighborReq, callbackInfo);
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return (status);
}
#endif

//The following are debug APIs to support direct read/write register/memory
//They are placed in SME because HW cannot be access when in LOW_POWER state
//AND not connected. The knowledge and synchronization is done in SME

//sme_DbgReadRegister
//Caller needs to validate the input values
VOS_STATUS sme_DbgReadRegister(tHalHandle hHal, v_U32_t regAddr, v_U32_t *pRegValue)
{
   VOS_STATUS   status = VOS_STATUS_E_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
   tPmcPowerState PowerState;
   tANI_U32  sessionId = 0;
   MTRACE(vos_trace(VOS_MODULE_ID_SME,
               TRACE_CODE_SME_RX_HDD_DBG_READREG, NO_SESSION, 0));

   /* 1) To make Quarky work in FTM mode **************************************/

   if(eDRIVER_TYPE_MFG == pMac->gDriverType)
   {
      if (eWLAN_PAL_STATUS_SUCCESS == wpalDbgReadRegister(regAddr, pRegValue))
      {
         return VOS_STATUS_SUCCESS;
      }
      return VOS_STATUS_E_FAILURE;
   }

   /* 2) NON FTM mode driver *************************************************/

   /* Acquire SME global lock */
   if (eHAL_STATUS_SUCCESS != sme_AcquireGlobalLock(&pMac->sme))
   {
      return VOS_STATUS_E_FAILURE;
   }

   if(HAL_STATUS_SUCCESS(pmcQueryPowerState(pMac, &PowerState, NULL, NULL)))
   {
      /* Are we not in IMPS mode? Or are we in connected? Then we're safe*/
      if(!csrIsConnStateDisconnected(pMac, sessionId) || (ePMC_LOW_POWER != PowerState))
      {
         if (eWLAN_PAL_STATUS_SUCCESS == wpalDbgReadRegister(regAddr, pRegValue))
         {
            status = VOS_STATUS_SUCCESS;
         }
         else
         {
            status = VOS_STATUS_E_FAILURE;
         }
      }
      else
      {
         status = VOS_STATUS_E_FAILURE;
      }
   }

   /* This is a hack for Qualky/pttWniSocket
      Current implementation doesn't allow pttWniSocket to inform Qualky an error */
   if ( VOS_STATUS_SUCCESS != status )
   {
      *pRegValue = 0xDEADBEEF;
       status = VOS_STATUS_SUCCESS;
   }

   /* Release SME global lock */
   sme_ReleaseGlobalLock(&pMac->sme);

   return (status);
}


//sme_DbgWriteRegister
//Caller needs to validate the input values
VOS_STATUS sme_DbgWriteRegister(tHalHandle hHal, v_U32_t regAddr, v_U32_t regValue)
{
   VOS_STATUS    status = VOS_STATUS_E_FAILURE;
   tpAniSirGlobal  pMac = PMAC_STRUCT(hHal);
   tPmcPowerState PowerState;
   tANI_U32   sessionId = 0;

   /* 1) To make Quarky work in FTM mode **************************************/

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
                    TRACE_CODE_SME_RX_HDD_DBG_WRITEREG, NO_SESSION, 0));
   if(eDRIVER_TYPE_MFG == pMac->gDriverType)
   {
      if (eWLAN_PAL_STATUS_SUCCESS == wpalDbgWriteRegister(regAddr, regValue))
      {
         return VOS_STATUS_SUCCESS;
      }
      return VOS_STATUS_E_FAILURE;
   }

   /* 2) NON FTM mode driver *************************************************/

   /* Acquire SME global lock */
   if (eHAL_STATUS_SUCCESS != sme_AcquireGlobalLock(&pMac->sme))
   {
      return VOS_STATUS_E_FAILURE;
   }

   if(HAL_STATUS_SUCCESS(pmcQueryPowerState(pMac, &PowerState, NULL, NULL)))
   {
      /* Are we not in IMPS mode? Or are we in connected? Then we're safe*/
      if(!csrIsConnStateDisconnected(pMac, sessionId) || (ePMC_LOW_POWER != PowerState))
      {
         if (eWLAN_PAL_STATUS_SUCCESS == wpalDbgWriteRegister(regAddr, regValue))
         {
            status = VOS_STATUS_SUCCESS;
         }
         else
         {
            status = VOS_STATUS_E_FAILURE;
         }
      }
      else
      {
         status = VOS_STATUS_E_FAILURE;
      }
   }

   /* Release SME global lock */
   sme_ReleaseGlobalLock(&pMac->sme);

   return (status);
}



//sme_DbgReadMemory
//Caller needs to validate the input values
//pBuf caller allocated buffer has the length of nLen
VOS_STATUS sme_DbgReadMemory(tHalHandle hHal, v_U32_t memAddr, v_U8_t *pBuf, v_U32_t nLen)
{
   VOS_STATUS  status  = VOS_STATUS_E_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   tPmcPowerState PowerState;
   tANI_U32 sessionId  = 0;
   tANI_U32 cmd = READ_MEMORY_DUMP_CMD;
   tANI_U32 arg1 = memAddr;
   tANI_U32 arg2 = nLen/4;
   tANI_U32 arg3 = 4;
   tANI_U32 arg4 = 0;
   /* 1) To make Quarky work in FTM mode **************************************/

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
                       TRACE_CODE_SME_RX_HDD_DBG_READMEM, NO_SESSION, 0));
   if(eDRIVER_TYPE_MFG == pMac->gDriverType)
   {
      if (VOS_STATUS_SUCCESS == WDA_HALDumpCmdReq(pMac, cmd, arg1, arg2, arg3, arg4, (tANI_U8*)pBuf))
      {
         return VOS_STATUS_SUCCESS;
      }
      return VOS_STATUS_E_FAILURE;
   }

   /* 2) NON FTM mode driver *************************************************/

   /* Acquire SME global lock */
   if (eHAL_STATUS_SUCCESS != sme_AcquireGlobalLock(&pMac->sme))
   {
      return VOS_STATUS_E_FAILURE;
   }

   if(HAL_STATUS_SUCCESS(pmcQueryPowerState(pMac, &PowerState, NULL, NULL)))
   {
      /* Are we not in IMPS mode? Or are we in connected? Then we're safe*/
      if(!csrIsConnStateDisconnected(pMac, sessionId) || (ePMC_LOW_POWER != PowerState))
      {
         if (VOS_STATUS_SUCCESS == WDA_HALDumpCmdReq(pMac, cmd, arg1, arg2, arg3, arg4, (tANI_U8 *)pBuf))
         {
            status = VOS_STATUS_SUCCESS;
         }
         else
         {
            status = VOS_STATUS_E_FAILURE;
         }
      }
      else
      {
         status = VOS_STATUS_E_FAILURE;
      }
   }

   /* This is a hack for Qualky/pttWniSocket
      Current implementation doesn't allow pttWniSocket to inform Qualky an error */
   if (VOS_STATUS_SUCCESS != status)
   {
      vos_mem_set(pBuf, nLen, 0xCD);
      status = VOS_STATUS_SUCCESS;
      smsLog(pMac, LOGE, FL(" filled with 0xCD because it cannot access the hardware"));
   }

   /* Release SME lock */
   sme_ReleaseGlobalLock(&pMac->sme);

   return (status);
}


//sme_DbgWriteMemory
//Caller needs to validate the input values
VOS_STATUS sme_DbgWriteMemory(tHalHandle hHal, v_U32_t memAddr, v_U8_t *pBuf, v_U32_t nLen)
{
   VOS_STATUS    status = VOS_STATUS_E_FAILURE;
   tpAniSirGlobal  pMac = PMAC_STRUCT(hHal);
   tPmcPowerState PowerState;
   tANI_U32   sessionId = 0;

   /* 1) To make Quarky work in FTM mode **************************************/

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
                      TRACE_CODE_SME_RX_HDD_DBG_WRITEMEM, NO_SESSION, 0));
   if(eDRIVER_TYPE_MFG == pMac->gDriverType)
   {
      {
         return VOS_STATUS_SUCCESS;
      }
      return VOS_STATUS_E_FAILURE;
   }

   /* 2) NON FTM mode driver *************************************************/

   /* Acquire SME global lock */
   if (eHAL_STATUS_SUCCESS != sme_AcquireGlobalLock(&pMac->sme))
   {
      return VOS_STATUS_E_FAILURE;
   }

   if(HAL_STATUS_SUCCESS(pmcQueryPowerState(pMac, &PowerState, NULL, NULL)))
   {
      /* Are we not in IMPS mode? Or are we in connected? Then we're safe*/
      if(!csrIsConnStateDisconnected(pMac, sessionId) || (ePMC_LOW_POWER != PowerState))
      {
         if (eWLAN_PAL_STATUS_SUCCESS == wpalDbgWriteMemory(memAddr, (void *)pBuf, nLen))
         {
            status = VOS_STATUS_SUCCESS;
         }
         else
         {
            status = VOS_STATUS_E_FAILURE;
         }
      }
      else
      {
         status = VOS_STATUS_E_FAILURE;
      }
   }

   /* Release Global lock */
   sme_ReleaseGlobalLock(&pMac->sme);

   return (status);
}


void pmcLog(tpAniSirGlobal pMac, tANI_U32 loglevel, const char *pString, ...)
{
    VOS_TRACE_LEVEL  vosDebugLevel;
    char    logBuffer[LOG_SIZE];
    va_list marker;

    /* getting proper Debug level */
    vosDebugLevel = getVosDebugLevel(loglevel);

    /* extracting arguments from pstring */
    va_start( marker, pString );
    vsnprintf(logBuffer, LOG_SIZE, pString, marker);

    VOS_TRACE(VOS_MODULE_ID_PMC, vosDebugLevel, "%s", logBuffer);
    va_end( marker );
}


void smsLog(tpAniSirGlobal pMac, tANI_U32 loglevel, const char *pString,...)
{
#ifdef WLAN_DEBUG
    // Verify against current log level
    if ( loglevel > pMac->utils.gLogDbgLevel[LOG_INDEX_FOR_MODULE( SIR_SMS_MODULE_ID )] )
        return;
    else
    {
        va_list marker;

        va_start( marker, pString );     /* Initialize variable arguments. */

        logDebug(pMac, SIR_SMS_MODULE_ID, loglevel, pString, marker);

        va_end( marker );              /* Reset variable arguments.      */
    }
#endif
}

/* ---------------------------------------------------------------------------
    \fn sme_GetWcnssWlanCompiledVersion
    \brief  This API returns the version of the WCNSS WLAN API with
            which the HOST driver was built
    \param  hHal - The handle returned by macOpen.
    \param  pVersion - Points to the Version structure to be filled
    \return VOS_STATUS
            VOS_STATUS_E_INVAL - failure
            VOS_STATUS_SUCCESS  success
  ---------------------------------------------------------------------------*/
VOS_STATUS sme_GetWcnssWlanCompiledVersion(tHalHandle hHal,
                                           tSirVersionType *pVersion)
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    v_CONTEXT_t vosContext = vos_get_global_context(VOS_MODULE_ID_SME, NULL);

    if ( eHAL_STATUS_SUCCESS == sme_AcquireGlobalLock( &pMac->sme ) )
    {
        if( pVersion != NULL )
        {
            status = WDA_GetWcnssWlanCompiledVersion(vosContext, pVersion);
        }
        else
        {
            status = VOS_STATUS_E_INVAL;
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return (status);
}


/* ---------------------------------------------------------------------------
    \fn sme_GetWcnssWlanReportedVersion
    \brief  This API returns the version of the WCNSS WLAN API with
            which the WCNSS driver reports it was built
    \param  hHal - The handle returned by macOpen.
    \param  pVersion - Points to the Version structure to be filled
    \return VOS_STATUS
            VOS_STATUS_E_INVAL - failure
            VOS_STATUS_SUCCESS  success
  ---------------------------------------------------------------------------*/
VOS_STATUS sme_GetWcnssWlanReportedVersion(tHalHandle hHal,
                                           tSirVersionType *pVersion)
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    v_CONTEXT_t vosContext = vos_get_global_context(VOS_MODULE_ID_SME, NULL);

    if ( eHAL_STATUS_SUCCESS == sme_AcquireGlobalLock( &pMac->sme ) )
    {
        if( pVersion != NULL )
        {
            status = WDA_GetWcnssWlanReportedVersion(vosContext, pVersion);
        }
        else
        {
            status = VOS_STATUS_E_INVAL;
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return (status);
}


/* ---------------------------------------------------------------------------
    \fn sme_GetWcnssSoftwareVersion
    \brief  This API returns the version string of the WCNSS driver
    \param  hHal - The handle returned by macOpen.
    \param  pVersion - Points to the Version string buffer to be filled
    \param  versionBufferSize - THe size of the Version string buffer
    \return VOS_STATUS
            VOS_STATUS_E_INVAL - failure
            VOS_STATUS_SUCCESS  success
  ---------------------------------------------------------------------------*/
VOS_STATUS sme_GetWcnssSoftwareVersion(tHalHandle hHal,
                                       tANI_U8 *pVersion,
                                       tANI_U32 versionBufferSize)
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    v_CONTEXT_t vosContext = vos_get_global_context(VOS_MODULE_ID_SME, NULL);

    if ( eHAL_STATUS_SUCCESS == sme_AcquireGlobalLock( &pMac->sme ) )
    {
        if( pVersion != NULL )
        {
            status = WDA_GetWcnssSoftwareVersion(vosContext, pVersion,
                                                 versionBufferSize);
        }
        else
        {
            status = VOS_STATUS_E_INVAL;
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return (status);
}


/* ---------------------------------------------------------------------------
    \fn sme_GetWcnssHardwareVersion
    \brief  This API returns the version string of the WCNSS hardware
    \param  hHal - The handle returned by macOpen.
    \param  pVersion - Points to the Version string buffer to be filled
    \param  versionBufferSize - THe size of the Version string buffer
    \return VOS_STATUS
            VOS_STATUS_E_INVAL - failure
            VOS_STATUS_SUCCESS  success
  ---------------------------------------------------------------------------*/
VOS_STATUS sme_GetWcnssHardwareVersion(tHalHandle hHal,
                                       tANI_U8 *pVersion,
                                       tANI_U32 versionBufferSize)
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    v_CONTEXT_t vosContext = vos_get_global_context(VOS_MODULE_ID_SME, NULL);

    if ( eHAL_STATUS_SUCCESS == sme_AcquireGlobalLock( &pMac->sme ) )
    {
        if( pVersion != NULL )
        {
            status = WDA_GetWcnssHardwareVersion(vosContext, pVersion,
                                                 versionBufferSize);
        }
        else
        {
            status = VOS_STATUS_E_INVAL;
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return (status);
}


#ifdef FEATURE_WLAN_WAPI
/* ---------------------------------------------------------------------------
    \fn sme_RoamSetBKIDCache
    \brief The SME API exposed to HDD to allow HDD to provde SME the BKID
    candidate list.
    \param hHal - Handle to the HAL. The HAL handle is returned by the HAL after
    it is opened (by calling halOpen).
    \param pBKIDCache - caller allocated buffer point to an array of tBkidCacheInfo
    \param numItems - a variable that has the number of tBkidCacheInfo allocated
    when retruning, this is the number of items put into pBKIDCache
    \return eHalStatus - when fail, it usually means the buffer allocated is not
    big enough and pNumItems has the number of tBkidCacheInfo.
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamSetBKIDCache( tHalHandle hHal, tANI_U32 sessionId, tBkidCacheInfo *pBKIDCache,
                                 tANI_U32 numItems )
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status = csrRoamSetBKIDCache( pMac, sessionId, pBKIDCache, numItems );
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_RoamGetBKIDCache
    \brief The SME API exposed to HDD to allow HDD to request SME to return its
    BKID cache.
    \param hHal - Handle to the HAL. The HAL handle is returned by the HAL after
    it is opened (by calling halOpen).
    \param pNum - caller allocated memory that has the space of the number of
    tBkidCacheInfo as input. Upon returned, *pNum has the needed number of entries
    in SME cache.
    \param pBkidCache - Caller allocated memory that contains BKID cache, if any,
    upon return
    \return eHalStatus - when fail, it usually means the buffer allocated is not
    big enough.
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamGetBKIDCache(tHalHandle hHal, tANI_U32 *pNum,
                                tBkidCacheInfo *pBkidCache)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       smsLog(pMac, LOGE, FL(" !!!!!!!!!!!!!!!!!!SessionId is hardcoded"));
       status = csrRoamGetBKIDCache( pMac, 0, pNum, pBkidCache );
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_RoamGetNumBKIDCache
    \brief The SME API exposed to HDD to allow HDD to request SME to return the
    number of BKID cache entries.
    \param hHal - Handle to the HAL. The HAL handle is returned by the HAL after
    it is opened (by calling halOpen).
    \return tANI_U32 - the number of BKID cache entries.
  ---------------------------------------------------------------------------*/
tANI_U32 sme_RoamGetNumBKIDCache(tHalHandle hHal, tANI_U32 sessionId)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   tANI_U32 numBkidCache = 0;

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       numBkidCache = csrRoamGetNumBKIDCache( pMac, sessionId );
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (numBkidCache);
}

/* ---------------------------------------------------------------------------
    \fn sme_ScanGetBKIDCandidateList
    \brief a wrapper function to return the BKID candidate list
    \param pBkidList - caller allocated buffer point to an array of
                        tBkidCandidateInfo
    \param pNumItems - pointer to a variable that has the number of
                       tBkidCandidateInfo allocated when retruning, this is
                       either the number needed or number of items put into
                       pPmkidList
    \return eHalStatus - when fail, it usually means the buffer allocated is not
                         big enough and pNumItems
    has the number of tBkidCandidateInfo.
    \Note: pNumItems is a number of tBkidCandidateInfo,
           not sizeof(tBkidCandidateInfo) * something
  ---------------------------------------------------------------------------*/
eHalStatus sme_ScanGetBKIDCandidateList(tHalHandle hHal, tANI_U32 sessionId,
                                        tBkidCandidateInfo *pBkidList,
                                        tANI_U32 *pNumItems )
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        status = csrScanGetBKIDCandidateList( pMac, sessionId, pBkidList, pNumItems );
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return (status);
}
#endif /* FEATURE_WLAN_WAPI */

#ifdef FEATURE_OEM_DATA_SUPPORT

/*****************************************************************************
 OEM DATA related modifications and function additions
 *****************************************************************************/

/* ---------------------------------------------------------------------------
    \fn sme_getOemDataRsp
    \brief a wrapper function to obtain the OEM DATA RSP
    \param pOemDataRsp - A pointer to the response object
    \param pContext - a pointer passed in for the callback
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_getOemDataRsp(tHalHandle hHal,
        tOemDataRsp **pOemDataRsp)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    do
    {
        //acquire the lock for the sme object
        status = sme_AcquireGlobalLock(&pMac->sme);

        if(!HAL_STATUS_SUCCESS(status))
        {
            break;
        }

        if(pMac->oemData.pOemDataRsp != NULL)
        {
            *pOemDataRsp = pMac->oemData.pOemDataRsp;
        }
        else
        {
            status = eHAL_STATUS_FAILURE;
        }

        //release the lock for the sme object
        sme_ReleaseGlobalLock( &pMac->sme );

    } while(0);

    return status;
}

/* ---------------------------------------------------------------------------
    \fn sme_OemDataReq
    \brief a wrapper function for OEM DATA REQ
    \param sessionId - session id to be used.
    \param pOemDataReqId - pointer to an object to get back the request ID
    \param callback - a callback function that is called upon finish
    \param pContext - a pointer passed in for the callback
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_OemDataReq(tHalHandle hHal,
        tANI_U8 sessionId,
        tOemDataReqConfig *pOemDataReqConfig,
        tANI_U32 *pOemDataReqID,
        oemData_OemDataReqCompleteCallback callback,
        void *pContext)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    do
    {
        //acquire the lock for the sme object
        status = sme_AcquireGlobalLock(&pMac->sme);
        if(HAL_STATUS_SUCCESS(status))
        {
            tANI_U32 lOemDataReqId = pMac->oemData.oemDataReqID++; //let it wrap around

            if(pOemDataReqID)
            {
               *pOemDataReqID = lOemDataReqId;
            }
            else
            {
                sme_ReleaseGlobalLock( &pMac->sme );
                return eHAL_STATUS_FAILURE;
            }

            status = oemData_OemDataReq(hHal, sessionId, pOemDataReqConfig, pOemDataReqID, callback, pContext);

            //release the lock for the sme object
            sme_ReleaseGlobalLock( &pMac->sme );
        }
    } while(0);

    smsLog(pMac, LOGW, "exiting function %s", __func__);

    return(status);
}

#endif /*FEATURE_OEM_DATA_SUPPORT*/

/*--------------------------------------------------------------------------

  \brief sme_OpenSession() - Open a session for scan/roam operation.

  This is a synchronous API.


  \param hHal - The handle returned by macOpen.
  \param callback - A pointer to the function caller specifies for roam/connect status indication
  \param pContext - The context passed with callback
  \param pSelfMacAddr - Caller allocated memory filled with self MAC address (6 bytes)
  \param pbSessionId - pointer to a caller allocated buffer for returned session ID

  \return eHAL_STATUS_SUCCESS - session is opened. sessionId returned.

          Other status means SME is failed to open the session.
          eHAL_STATUS_RESOURCES - no more session available.
  \sa

  --------------------------------------------------------------------------*/
eHalStatus sme_OpenSession(tHalHandle hHal, csrRoamCompleteCallback callback,
                           void *pContext, tANI_U8 *pSelfMacAddr,
                           tANI_U8 *pbSessionId)
{
   eHalStatus status;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   if( NULL == pbSessionId )
   {
      status = eHAL_STATUS_INVALID_PARAMETER;
   }
   else
   {
      status = sme_AcquireGlobalLock( &pMac->sme );
      if ( HAL_STATUS_SUCCESS( status ) )
      {
         status = csrRoamOpenSession(pMac, callback, pContext,
                                     pSelfMacAddr, pbSessionId);

         sme_ReleaseGlobalLock( &pMac->sme );
      }
   }
   if( NULL != pbSessionId )
   MTRACE(vos_trace(VOS_MODULE_ID_SME,
                 TRACE_CODE_SME_RX_HDD_OPEN_SESSION,*pbSessionId, 0));

   return ( status );
}


/*--------------------------------------------------------------------------

  \brief sme_CloseSession() - Open a session for scan/roam operation.

  This is a synchronous API.


  \param hHal - The handle returned by macOpen.

  \param sessionId - A previous opened session's ID.

  \return eHAL_STATUS_SUCCESS - session is closed.

          Other status means SME is failed to open the session.
          eHAL_STATUS_INVALID_PARAMETER - session is not opened.
  \sa

  --------------------------------------------------------------------------*/
eHalStatus sme_CloseSession(tHalHandle hHal, tANI_U8 sessionId,
                          csrRoamSessionCloseCallback callback, void *pContext)
{
   eHalStatus status;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
                 TRACE_CODE_SME_RX_HDD_CLOSE_SESSION, sessionId, 0));
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
      status = csrRoamCloseSession( pMac, sessionId, FALSE,
                                    callback, pContext );

      sme_ReleaseGlobalLock( &pMac->sme );
   }

   return ( status );
}

/* ---------------------------------------------------------------------------

    \fn sme_RoamUpdateAPWPSIE

    \brief To update AP's WPS IE. This function should be called after SME AP session is created
    This is an asynchronous API.

    \param pAPWPSIES - pointer to a caller allocated object of tSirAPWPSIEs

    \return eHalStatus  SUCCESS 

                         FAILURE or RESOURCES  The API finished and failed.

  -------------------------------------------------------------------------------*/
eHalStatus sme_RoamUpdateAPWPSIE(tHalHandle hHal, tANI_U8 sessionId, tSirAPWPSIEs *pAPWPSIES)
{

   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {

      status = csrRoamUpdateAPWPSIE( pMac, sessionId, pAPWPSIES );

      sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}
/* ---------------------------------------------------------------------------

    \fn sme_RoamUpdateAPWPARSNIEs

    \brief To update AP's WPA/RSN IEs. This function should be called after SME AP session is created
    This is an asynchronous API.

    \param pAPSirRSNie - pointer to a caller allocated object of tSirRSNie with WPS/RSN IEs

    \return eHalStatus  SUCCESS 

                         FAILURE or RESOURCES  The API finished and failed.

  -------------------------------------------------------------------------------*/
eHalStatus sme_RoamUpdateAPWPARSNIEs(tHalHandle hHal, tANI_U8 sessionId, tSirRSNie * pAPSirRSNie)
{

   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {

      status = csrRoamUpdateWPARSNIEs( pMac, sessionId, pAPSirRSNie);

      sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}
/* ---------------------------------------------------------------------------

    \fn sme_ChangeMCCBeaconInterval

    \brief To update P2P-GO beaconInterval. This function should be called after
    disassociating all the station is done
    This is an asynchronous API.

    \param

    \return eHalStatus  SUCCESS
                        FAILURE or RESOURCES
                        The API finished and failed.

  -------------------------------------------------------------------------------*/
eHalStatus sme_ChangeMCCBeaconInterval(tHalHandle hHal, tANI_U8 sessionId)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   smsLog(pMac, LOG1, FL("Update Beacon PARAMS "));
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
      status = csrSendChngMCCBeaconInterval( pMac, sessionId);
      sme_ReleaseGlobalLock( &pMac->sme );
   }
   return (status);
}

/*-------------------------------------------------------------------------------*

  \fn sme_sendBTAmpEvent

  \brief to receive the coex priorty request from BT-AMP PAL
  and send the BT_AMP link state to HAL

  \param btAmpEvent - btAmpEvent

  \return eHalStatus: SUCCESS : BTAmp event successfully sent to HAL

                      FAILURE: API failed

-------------------------------------------------------------------------------*/

eHalStatus sme_sendBTAmpEvent(tHalHandle hHal, tSmeBtAmpEvent btAmpEvent)
{
  vos_msg_t msg;
  tpSmeBtAmpEvent ptrSmeBtAmpEvent = NULL;
  eHalStatus status = eHAL_STATUS_FAILURE;

  ptrSmeBtAmpEvent = vos_mem_malloc(sizeof(tSmeBtAmpEvent));
  if (NULL == ptrSmeBtAmpEvent)
     {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: "
           "Not able to allocate memory for BTAmp event", __func__);
        return status;
   }

  vos_mem_copy(ptrSmeBtAmpEvent, (void*)&btAmpEvent, sizeof(tSmeBtAmpEvent));
  msg.type = WDA_SIGNAL_BTAMP_EVENT;
  msg.reserved = 0;
  msg.bodyptr = ptrSmeBtAmpEvent;

  //status = halFW_SendBTAmpEventMesg(pMac, event);

  if(VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MODULE_ID_WDA, &msg))
  {
    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: "
           "Not able to post SIR_HAL_SIGNAL_BTAMP_EVENT message to HAL", __func__);
    vos_mem_free(ptrSmeBtAmpEvent);
    return status;
  }

  return eHAL_STATUS_SUCCESS;

}

/* ---------------------------------------------------------------------------
    \fn     smeIssueFastRoamNeighborAPEvent
    \brief  API to trigger fast BSS roam independent of RSSI triggers
    \param  hHal - The handle returned by macOpen.
    \param  bssid -  Pointer to the BSSID to roam to.
    \param  fastRoamTrig - Trigger to Scan or roam
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus smeIssueFastRoamNeighborAPEvent (tHalHandle hHal,
                                            tANI_U8 *bssid,
                                            tSmeFastRoamTrigger fastRoamTrig)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tpCsrNeighborRoamControlInfo  pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    VOS_STATUS  vosStatus = VOS_STATUS_SUCCESS;
    eHalStatus  status    = eHAL_STATUS_SUCCESS;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                  "%s: invoked", __func__);

        if (eSME_ROAM_TRIGGER_SCAN == fastRoamTrig)
        {
            smsLog(pMac, LOG1, FL("CFG Channel list scan... "));
            pNeighborRoamInfo->cfgRoamEn = eSME_ROAM_TRIGGER_SCAN;
            vos_mem_copy((void *)(&pNeighborRoamInfo->cfgRoambssId),
                       (void *)bssid, sizeof(tSirMacAddr));
            smsLog(pMac, LOG1, "Calling Roam Look Up down Event BSSID "
                   MAC_ADDRESS_STR, MAC_ADDR_ARRAY(pNeighborRoamInfo->cfgRoambssId));

            vosStatus = csrNeighborRoamTransitToCFGChanScan(pMac);
            if (VOS_STATUS_SUCCESS != vosStatus)
            {
                smsLog(pMac, LOGE,
                       FL("CFG Channel list scan state failed with status %d "),
                       vosStatus);
            }
        }
        else if (eSME_ROAM_TRIGGER_FAST_ROAM == fastRoamTrig)
        {
             vos_mem_copy((void *)(&pNeighborRoamInfo->cfgRoambssId),
                       (void *)bssid, sizeof(tSirMacAddr));
             pNeighborRoamInfo->cfgRoamEn = eSME_ROAM_TRIGGER_FAST_ROAM;
             smsLog(pMac, LOG1, "Roam to BSSID "MAC_ADDRESS_STR,
                    MAC_ADDR_ARRAY(pNeighborRoamInfo->cfgRoambssId));

             vosStatus = csrNeighborRoamReassocIndCallback(pMac->roam.gVosContext,
                                                           0,
                                                           pMac,
                                                           0);

             if (!VOS_IS_STATUS_SUCCESS(vosStatus))
             {
                 smsLog(pMac,
                        LOGE,
                        FL(" Call to csrNeighborRoamReassocIndCallback failed, status = %d"),
                        vosStatus);
             }
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }
    return vosStatus;
}
/* ---------------------------------------------------------------------------
    \fn sme_SetHostOffload
    \brief  API to set the host offload feature.
    \param  hHal - The handle returned by macOpen.
    \param  pRequest -  Pointer to the offload request.
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_SetHostOffload (tHalHandle hHal, tANI_U8 sessionId,
                                    tpSirHostOffloadReq pRequest)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status = eHAL_STATUS_FAILURE;

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
               TRACE_CODE_SME_RX_HDD_SET_HOSTOFFLOAD, sessionId, 0));
    if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme ) ) )
    {
#ifdef WLAN_NS_OFFLOAD
        if(SIR_IPV6_NS_OFFLOAD == pRequest->offloadType)
        {
            status = pmcSetNSOffload( hHal, pRequest, sessionId);
        }
        else
#endif //WLAN_NS_OFFLOAD
        {
            status = pmcSetHostOffload (hHal, pRequest, sessionId);
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return (status);
}

#ifdef WLAN_FEATURE_GTK_OFFLOAD
/* ---------------------------------------------------------------------------
    \fn sme_SetGTKOffload
    \brief  API to set GTK offload information.
    \param  hHal - The handle returned by macOpen.
    \param  pRequest -  Pointer to the GTK offload request.
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_SetGTKOffload (tHalHandle hHal, tpSirGtkOffloadParams pRequest,
                                    tANI_U8 sessionId)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status;

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
                    TRACE_CODE_SME_RX_HDD_SET_GTKOFFLOAD, sessionId, 0));
    if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme ) ) )
    {
        status = pmcSetGTKOffload( hHal, pRequest, sessionId );
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_GetGTKOffload
    \brief  API to get GTK offload information.
    \param  hHal - The handle returned by macOpen.
    \param  pRequest -  Pointer to the GTK offload response.
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_GetGTKOffload (tHalHandle hHal, GTKOffloadGetInfoCallback callbackRoutine,
                                    void *callbackContext, tANI_U8 sessionId )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status;

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
                TRACE_CODE_SME_RX_HDD_GET_GTKOFFLOAD, sessionId, 0));
    if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme ) ) )
    {
        pmcGetGTKOffload(hHal, callbackRoutine, callbackContext, sessionId);
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return (status);
}
#endif // WLAN_FEATURE_GTK_OFFLOAD

/* ---------------------------------------------------------------------------
    \fn sme_SetKeepAlive
    \brief  API to set the Keep Alive feature.
    \param  hHal - The handle returned by macOpen.
    \param  pRequest -  Pointer to the Keep Alive request.
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_SetKeepAlive (tHalHandle hHal, tANI_U8 sessionId,
                                 tpSirKeepAliveReq pRequest)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status;
    if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme ) ) )
    {
        status = pmcSetKeepAlive (hHal, pRequest, sessionId);
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return (status);
}

#ifdef FEATURE_WLAN_SCAN_PNO
/* ---------------------------------------------------------------------------
    \fn sme_SetPreferredNetworkList
    \brief  API to set the Preferred Network List Offload feature.
    \param  hHal - The handle returned by macOpen.
    \param  pRequest -  Pointer to the offload request.
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_SetPreferredNetworkList (tHalHandle hHal, tpSirPNOScanReq pRequest, tANI_U8 sessionId, void (*callbackRoutine) (void *callbackContext, tSirPrefNetworkFoundInd *pPrefNetworkFoundInd), void *callbackContext )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status;

    if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme ) ) )
    {
        pmcSetPreferredNetworkList(hHal, pRequest, sessionId, callbackRoutine, callbackContext);
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return (status);
}

eHalStatus sme_SetRSSIFilter(tHalHandle hHal, v_U8_t rssiThreshold)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status;

    if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme ) ) )
    {
        pmcSetRssiFilter(hHal, rssiThreshold);
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return (status);
}

#endif // FEATURE_WLAN_SCAN_PNO

eHalStatus sme_SetPowerParams(tHalHandle hHal, tSirSetPowerParamsReq* pwParams, tANI_BOOLEAN forced)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status;

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
                       TRACE_CODE_SME_RX_HDD_SET_POWERPARAMS, NO_SESSION, 0));
    if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme ) ) )
    {
        pmcSetPowerParams(hHal, pwParams, forced);
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_AbortMacScan
    \brief  API to cancel MAC scan.
    \param  hHal - The handle returned by macOpen.
    \param  sessionId - sessionId on which we need to abort scan.
    \param  reason - Reason to abort the scan.
    \return VOS_STATUS
            VOS_STATUS_E_FAILURE - failure
            VOS_STATUS_SUCCESS  success
  ---------------------------------------------------------------------------*/
eHalStatus sme_AbortMacScan(tHalHandle hHal, tANI_U8 sessionId,
                            eCsrAbortReason reason)
{
    eHalStatus status;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
               TRACE_CODE_SME_RX_HDD_ABORT_MACSCAN, NO_SESSION, 0));
    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
       status = csrScanAbortMacScan(pMac, sessionId, reason);

       sme_ReleaseGlobalLock( &pMac->sme );
    }

    return ( status );
}

/* ----------------------------------------------------------------------------
        \fn sme_GetOperationChannel
        \brief API to get current channel on which STA is parked
        this function gives channel information only of infra station or IBSS station
        \param hHal, pointer to memory location and sessionId
        \returns eHAL_STATUS_SUCCESS
                eHAL_STATUS_FAILURE
-------------------------------------------------------------------------------*/
eHalStatus sme_GetOperationChannel(tHalHandle hHal, tANI_U32 *pChannel, tANI_U8 sessionId)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tCsrRoamSession *pSession;

    if (CSR_IS_SESSION_VALID( pMac, sessionId ))
    {
       pSession = CSR_GET_SESSION( pMac, sessionId );

       if(( pSession->connectedProfile.BSSType == eCSR_BSS_TYPE_INFRASTRUCTURE ) ||
          ( pSession->connectedProfile.BSSType == eCSR_BSS_TYPE_IBSS ) ||
          ( pSession->connectedProfile.BSSType == eCSR_BSS_TYPE_INFRA_AP ) ||
          ( pSession->connectedProfile.BSSType == eCSR_BSS_TYPE_START_IBSS ))
       {
           *pChannel =pSession->connectedProfile.operationChannel;
           return eHAL_STATUS_SUCCESS;
       }
    }
    return eHAL_STATUS_FAILURE;
}// sme_GetOperationChannel ends here

/* ---------------------------------------------------------------------------

    \fn sme_RegisterMgtFrame

    \brief To register managment frame of specified type and subtype.
    \param frameType - type of the frame that needs to be passed to HDD.
    \param matchData - data which needs to be matched before passing frame
                       to HDD.
    \param matchDataLen - Length of matched data.
    \return eHalStatus
  -------------------------------------------------------------------------------*/
eHalStatus sme_RegisterMgmtFrame(tHalHandle hHal, tANI_U8 sessionId,
                     tANI_U16 frameType, tANI_U8* matchData, tANI_U16 matchLen)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
                          TRACE_CODE_SME_RX_HDD_REGISTER_MGMTFR, sessionId, 0));
    if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme ) ) )
    {
        tSirRegisterMgmtFrame *pMsg;
        tANI_U16 len;
        tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

        if(!pSession)
        {
            smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
            sme_ReleaseGlobalLock( &pMac->sme );
            return eHAL_STATUS_FAILURE;
        }

        if( !pSession->sessionActive )
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                        "%s Invalid Sessionid", __func__);
            sme_ReleaseGlobalLock( &pMac->sme );
            return eHAL_STATUS_FAILURE;
        }

        len = sizeof(tSirRegisterMgmtFrame) + matchLen;
        
        pMsg = vos_mem_malloc(len);
        if ( NULL == pMsg )
           status = eHAL_STATUS_FAILURE;
        else
        {
            vos_mem_set(pMsg, len, 0);
            pMsg->messageType     = eWNI_SME_REGISTER_MGMT_FRAME_REQ;
            pMsg->length          = len;
            pMsg->sessionId       = sessionId;
            pMsg->registerFrame   = VOS_TRUE;
            pMsg->frameType       = frameType;
            pMsg->matchLen        = matchLen;
            vos_mem_copy(pMsg->matchData, matchData, matchLen);
            status = palSendMBMessage(pMac->hHdd, pMsg);
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }
    return status;
}

/* ---------------------------------------------------------------------------

    \fn sme_DeregisterMgtFrame

    \brief To De-register managment frame of specified type and subtype.
    \param frameType - type of the frame that needs to be passed to HDD.
    \param matchData - data which needs to be matched before passing frame
                       to HDD.
    \param matchDataLen - Length of matched data.
    \return eHalStatus
  -------------------------------------------------------------------------------*/
eHalStatus sme_DeregisterMgmtFrame(tHalHandle hHal, tANI_U8 sessionId,
                     tANI_U16 frameType, tANI_U8* matchData, tANI_U16 matchLen)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
              TRACE_CODE_SME_RX_HDD_DEREGISTER_MGMTFR, sessionId, 0));
    if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme ) ) )
    {
        tSirRegisterMgmtFrame *pMsg;
        tANI_U16 len;
        tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

        if(!pSession)
        {
            smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
            sme_ReleaseGlobalLock( &pMac->sme );
            return eHAL_STATUS_FAILURE;
        }

        if( !pSession->sessionActive )
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                        "%s Invalid Sessionid", __func__);
            sme_ReleaseGlobalLock( &pMac->sme );
            return eHAL_STATUS_FAILURE;
        }

        len = sizeof(tSirRegisterMgmtFrame) + matchLen;
        
        pMsg = vos_mem_malloc(len);
        if ( NULL == pMsg )
           status = eHAL_STATUS_FAILURE;
        else
        {
            vos_mem_set(pMsg, len, 0);
            pMsg->messageType     = eWNI_SME_REGISTER_MGMT_FRAME_REQ;
            pMsg->length          = len;
            pMsg->registerFrame   = VOS_FALSE;
            pMsg->frameType       = frameType;
            pMsg->matchLen        = matchLen;
            vos_mem_copy(pMsg->matchData, matchData, matchLen);
            status = palSendMBMessage(pMac->hHdd, pMsg);
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }
    return status;
}

/* ---------------------------------------------------------------------------
    \fn sme_RemainOnChannel
    \brief  API to request remain on channel for 'x' duration. used in p2p in listen state
    \param  hHal - The handle returned by macOpen.
    \param  pRequest -  channel
    \param  duration - duration in ms
    \param callback - HDD registered callback to process reaminOnChannelRsp
    \param context - HDD Callback param
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_RemainOnChannel(tHalHandle hHal, tANI_U8 sessionId,
                               tANI_U8 channel, tANI_U32 duration,
                               remainOnChanCallback callback,
                               void *pContext,
                               tANI_U8 isP2PProbeReqAllowed)
{
  eHalStatus status = eHAL_STATUS_SUCCESS;
  tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

  MTRACE(vos_trace(VOS_MODULE_ID_SME,
            TRACE_CODE_SME_RX_HDD_REMAIN_ONCHAN, sessionId, 0));
  if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme ) ) )
  {
    status = p2pRemainOnChannel (hHal, sessionId, channel, duration, callback, pContext,
                                 isP2PProbeReqAllowed
#ifdef WLAN_FEATURE_P2P_INTERNAL
                                , eP2PRemainOnChnReasonUnknown
#endif
                                );
    sme_ReleaseGlobalLock( &pMac->sme );
  }
  return(status);
}

/* ---------------------------------------------------------------------------
    \fn sme_ReportProbeReq
    \brief  API to enable/disable forwarding of probeReq to apps in p2p.
    \param  hHal - The handle returned by macOpen.
    \param  falg: to set the Probe request forarding to wpa_supplicant in listen state in p2p
    \return eHalStatus
  ---------------------------------------------------------------------------*/

#ifndef WLAN_FEATURE_CONCURRENT_P2P
eHalStatus sme_ReportProbeReq(tHalHandle hHal, tANI_U8 flag)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    do
    {
        //acquire the lock for the sme object
        status = sme_AcquireGlobalLock(&pMac->sme);
        if(HAL_STATUS_SUCCESS(status))
        {
            /* call set in context */
            pMac->p2pContext.probeReqForwarding = flag;
            //release the lock for the sme object
            sme_ReleaseGlobalLock( &pMac->sme );
        }
    } while(0);

    smsLog(pMac, LOGW, "exiting function %s", __func__);

    return(status);
}

/* ---------------------------------------------------------------------------
    \fn sme_updateP2pIe
    \brief  API to set the P2p Ie in p2p context
    \param  hHal - The handle returned by macOpen.
    \param  p2pIe -  Ptr to p2pIe from HDD.
    \param p2pIeLength: length of p2pIe
    \return eHalStatus
  ---------------------------------------------------------------------------*/

eHalStatus sme_updateP2pIe(tHalHandle hHal, void *p2pIe, tANI_U32 p2pIeLength)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    //acquire the lock for the sme object
    status = sme_AcquireGlobalLock(&pMac->sme);
    if(HAL_STATUS_SUCCESS(status))
    {
        if(NULL != pMac->p2pContext.probeRspIe){
            vos_mem_free(pMac->p2pContext.probeRspIe);
            pMac->p2pContext.probeRspIeLength = 0;
        }

        pMac->p2pContext.probeRspIe = vos_mem_malloc(p2pIeLength);
        if (NULL == pMac->p2pContext.probeRspIe)
        {
            smsLog(pMac, LOGE, "%s: Unable to allocate P2P IE", __func__);
            pMac->p2pContext.probeRspIeLength = 0;
            status = eHAL_STATUS_FAILURE;
        }
        else
        {
            pMac->p2pContext.probeRspIeLength = p2pIeLength;

            sirDumpBuf( pMac, SIR_LIM_MODULE_ID, LOG2,
                        pMac->p2pContext.probeRspIe,
                        pMac->p2pContext.probeRspIeLength );
            vos_mem_copy((tANI_U8 *)pMac->p2pContext.probeRspIe, p2pIe,
                         p2pIeLength);
        }

        //release the lock for the sme object
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    smsLog(pMac, LOG2, "exiting function %s", __func__);

    return(status);
}
#endif

/* ---------------------------------------------------------------------------
    \fn sme_sendAction
    \brief  API to send action frame from supplicant.
    \param  hHal - The handle returned by macOpen.
    \return eHalStatus
  ---------------------------------------------------------------------------*/

eHalStatus sme_sendAction(tHalHandle hHal, tANI_U8 sessionId,
                          const tANI_U8 *pBuf, tANI_U32 len,
                          tANI_U16 wait, tANI_BOOLEAN noack)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
               TRACE_CODE_SME_RX_HDD_SEND_ACTION, sessionId, 0));
    //acquire the lock for the sme object
    status = sme_AcquireGlobalLock(&pMac->sme);
    if(HAL_STATUS_SUCCESS(status))
    {
        p2pSendAction(hHal, sessionId, pBuf, len, wait, noack);
        //release the lock for the sme object
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    smsLog(pMac, LOGW, "exiting function %s", __func__);

    return(status);
}

eHalStatus sme_CancelRemainOnChannel(tHalHandle hHal, tANI_U8 sessionId )
{
  eHalStatus status = eHAL_STATUS_SUCCESS;
  tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

  MTRACE(vos_trace(VOS_MODULE_ID_SME,
             TRACE_CODE_SME_RX_HDD_CANCEL_REMAIN_ONCHAN, sessionId, 0));
  if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme ) ) )
  {
    status = p2pCancelRemainOnChannel (hHal, sessionId);
    sme_ReleaseGlobalLock( &pMac->sme );
  }
  return(status);
}

//Power Save Related
eHalStatus sme_p2pSetPs(tHalHandle hHal, tP2pPsConfig * data)
{
  eHalStatus status = eHAL_STATUS_SUCCESS;
  tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

  if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme ) ) )
  {
    status = p2pSetPs (hHal, data);
    sme_ReleaseGlobalLock( &pMac->sme );
  }
  return(status);
}


/* ---------------------------------------------------------------------------

  \fn    sme_ConfigureRxpFilter

  \brief
    SME will pass this request to lower mac to set/reset the filter on RXP for
    multicast & broadcast traffic.

  \param

    hHal - The handle returned by macOpen.

    filterMask- Currently the API takes a 1 or 0 (set or reset) as filter.
    Basically to enable/disable the filter (to filter "all" mcbc traffic) based
    on this param. In future we can use this as a mask to set various types of
    filters as suggested below:
    FILTER_ALL_MULTICAST:
    FILTER_ALL_BROADCAST:
    FILTER_ALL_MULTICAST_BROADCAST:


  \return eHalStatus


--------------------------------------------------------------------------- */
eHalStatus sme_ConfigureRxpFilter( tHalHandle hHal,
                            tpSirWlanSetRxpFilters  wlanRxpFilterParam)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    vos_msg_t       vosMessage;

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
                    TRACE_CODE_SME_RX_HDD_CONFIG_RXPFIL, NO_SESSION, 0));
    if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme ) ) )
    {
        /* serialize the req through MC thread */
        vosMessage.bodyptr = wlanRxpFilterParam;
        vosMessage.type         = WDA_CFG_RXP_FILTER_REQ;
        vosStatus = vos_mq_post_message( VOS_MQ_ID_WDA, &vosMessage );
        if ( !VOS_IS_STATUS_SUCCESS(vosStatus) )
        {
           status = eHAL_STATUS_FAILURE;
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }
    return(status);
}

/* ---------------------------------------------------------------------------

  \fn    sme_ConfigureSuspendInd

  \brief
    SME will pass this request to lower mac to Indicate that the wlan needs to
    be suspended

  \param

    hHal - The handle returned by macOpen.

    wlanSuspendParam- Depicts the wlan suspend params


  \return eHalStatus


--------------------------------------------------------------------------- */
eHalStatus sme_ConfigureSuspendInd( tHalHandle hHal,
                          tpSirWlanSuspendParam  wlanSuspendParam)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    vos_msg_t       vosMessage;

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
                  TRACE_CODE_SME_RX_HDD_CONFIG_SUSPENDIND, NO_SESSION, 0));
    if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme ) ) )
    {
        /* serialize the req through MC thread */
        vosMessage.bodyptr = wlanSuspendParam;
        vosMessage.type    = WDA_WLAN_SUSPEND_IND;
        vosStatus = vos_mq_post_message( VOS_MQ_ID_WDA, &vosMessage );
        if ( !VOS_IS_STATUS_SUCCESS(vosStatus) )
        {
           status = eHAL_STATUS_FAILURE;
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }
    return(status);
}

/* ---------------------------------------------------------------------------

  \fn    sme_ConfigureResumeReq

  \brief
    SME will pass this request to lower mac to Indicate that the wlan needs to
    be Resumed

  \param

    hHal - The handle returned by macOpen.

    wlanResumeParam- Depicts the wlan resume params


  \return eHalStatus


--------------------------------------------------------------------------- */
eHalStatus sme_ConfigureResumeReq( tHalHandle hHal,
                             tpSirWlanResumeParam  wlanResumeParam)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    vos_msg_t       vosMessage;

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
                     TRACE_CODE_SME_RX_HDD_CONFIG_RESUMEREQ, NO_SESSION, 0));
    if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme ) ) )
    {
        /* serialize the req through MC thread */
        vosMessage.bodyptr = wlanResumeParam;
        vosMessage.type    = WDA_WLAN_RESUME_REQ;
        vosStatus = vos_mq_post_message( VOS_MQ_ID_WDA, &vosMessage );
        if ( !VOS_IS_STATUS_SUCCESS(vosStatus) )
        {
           status = eHAL_STATUS_FAILURE;
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }
    return(status);
}

/* ---------------------------------------------------------------------------

    \fn sme_GetInfraSessionId

    \brief To get the session ID for infra session, if connected
    This is a synchronous API.

    \param hHal - The handle returned by macOpen.

    \return sessionid, -1 if infra session is not connected

  -------------------------------------------------------------------------------*/
tANI_S8 sme_GetInfraSessionId(tHalHandle hHal)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tANI_S8 sessionid = -1;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {

      sessionid = csrGetInfraSessionId( pMac);

      sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (sessionid);
}

/* ---------------------------------------------------------------------------

    \fn sme_GetInfraOperationChannel

    \brief To get the operating channel for infra session, if connected
    This is a synchronous API.

    \param hHal - The handle returned by macOpen.
    \param sessionId - the sessionId returned by sme_OpenSession.

    \return operating channel, 0 if infra session is not connected

  -------------------------------------------------------------------------------*/
tANI_U8 sme_GetInfraOperationChannel( tHalHandle hHal, tANI_U8 sessionId)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   tANI_U8 channel = 0;
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {

      channel = csrGetInfraOperationChannel( pMac, sessionId);

      sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (channel);
}

//This routine will return poerating channel on which other BSS is operating to be used for concurrency mode.
//If other BSS is not up or not connected it will return 0
tANI_U8 sme_GetConcurrentOperationChannel( tHalHandle hHal )
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   tANI_U8 channel = 0;
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {

      channel = csrGetConcurrentOperationChannel( pMac );
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, "%s: "
           " Other Concurrent Channel = %d", __func__,channel);
      sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (channel);
}

#ifdef FEATURE_WLAN_SCAN_PNO
/******************************************************************************
*
* Name: sme_PreferredNetworkFoundInd
*
* Description:
*    Invoke Preferred Network Found Indication
*
* Parameters:
*    hHal - HAL handle for device
*    pMsg - found network description
*
* Returns: eHalStatus
*
******************************************************************************/
eHalStatus sme_PreferredNetworkFoundInd (tHalHandle hHal, void* pMsg)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
   eHalStatus status = eHAL_STATUS_SUCCESS;
   tSirPrefNetworkFoundInd *pPrefNetworkFoundInd = (tSirPrefNetworkFoundInd *)pMsg;
   v_U8_t dumpSsId[SIR_MAC_MAX_SSID_LENGTH + 1];
   tANI_U8 ssIdLength = 0;

   if (NULL == pMsg)
   {
      smsLog(pMac, LOGE, "in %s msg ptr is NULL", __func__);
      status = eHAL_STATUS_FAILURE;
   }
   else
   {
      if (pPrefNetworkFoundInd->ssId.length > 0)
      {
         ssIdLength = CSR_MIN(SIR_MAC_MAX_SSID_LENGTH,
                              pPrefNetworkFoundInd->ssId.length);
         vos_mem_copy(dumpSsId, pPrefNetworkFoundInd->ssId.ssId, ssIdLength);
         dumpSsId[ssIdLength] = 0;
         smsLog(pMac, LOG1, FL(" SSID=%s frame length %d"),
             dumpSsId, pPrefNetworkFoundInd->frameLength);

         /* Flush scan results, So as to avoid indication/updation of
          * stale entries, which may not have aged out during APPS collapse
          */
         sme_ScanFlushResult(hHal,0);

         //Save the frame to scan result
         if (pPrefNetworkFoundInd->mesgLen > sizeof(tSirPrefNetworkFoundInd))
         {
            //we may have a frame
            status = csrScanSavePreferredNetworkFound(pMac,
                        pPrefNetworkFoundInd);
            if (!HAL_STATUS_SUCCESS(status))
            {
               smsLog(pMac, LOGE, FL(" fail to save preferred network"));
            }
         }
         else
         {
            smsLog(pMac, LOGE, FL(" not enough data length %u needed %zu"),
               pPrefNetworkFoundInd->mesgLen, sizeof(tSirPrefNetworkFoundInd));
         }

         /* Call Preferred Netowrk Found Indication callback routine. */
         if (HAL_STATUS_SUCCESS(status) && (pMac->pmc.prefNetwFoundCB != NULL))
         {
            pMac->pmc.prefNetwFoundCB(
                pMac->pmc.preferredNetworkFoundIndCallbackContext,
                pPrefNetworkFoundInd);
         }
      }
      else
      {
         smsLog(pMac, LOGE, "%s: callback failed - SSID is NULL", __func__);
         status = eHAL_STATUS_FAILURE;
      }
   }


   return(status);
}

#endif // FEATURE_WLAN_SCAN_PNO


eHalStatus sme_GetCfgValidChannels(tHalHandle hHal, tANI_U8 *aValidChannels, tANI_U32 *len)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        status = csrGetCfgValidChannels(pMac, aValidChannels, len);
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return (status);
}


/* ---------------------------------------------------------------------------

    \fn sme_SetTxPerTracking

    \brief Set Tx PER tracking configuration parameters

    \param hHal - The handle returned by macOpen.
    \param pTxPerTrackingConf - Tx PER configuration parameters

    \return eHalStatus

  -------------------------------------------------------------------------------*/
eHalStatus sme_SetTxPerTracking(tHalHandle hHal,
                                void (*pCallbackfn) (void *pCallbackContext),
                                void *pCallbackContext,
                                tpSirTxPerTrackingParam pTxPerTrackingParam)
{
    vos_msg_t msg;
    tpSirTxPerTrackingParam pTxPerTrackingParamReq = NULL;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    if ( eHAL_STATUS_SUCCESS == sme_AcquireGlobalLock( &pMac->sme ) )
    {
        pMac->sme.pTxPerHitCallback = pCallbackfn;
        pMac->sme.pTxPerHitCbContext = pCallbackContext;
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    // free this memory in failure case or WDA request callback function
    pTxPerTrackingParamReq = vos_mem_malloc(sizeof(tSirTxPerTrackingParam));
    if (NULL == pTxPerTrackingParamReq)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to allocate memory for tSirTxPerTrackingParam", __func__);
        return eHAL_STATUS_FAILURE;
    }

    vos_mem_copy(pTxPerTrackingParamReq, (void*)pTxPerTrackingParam,
                 sizeof(tSirTxPerTrackingParam));
    msg.type = WDA_SET_TX_PER_TRACKING_REQ;
    msg.reserved = 0;
    msg.bodyptr = pTxPerTrackingParamReq;

    if(VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MODULE_ID_WDA, &msg))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to post WDA_SET_TX_PER_TRACKING_REQ message to WDA", __func__);
        vos_mem_free(pTxPerTrackingParamReq);
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------

    \fn sme_HandleChangeCountryCode

    \brief Change Country code, Reg Domain and channel list

    \details Country Code Priority
    0 = 11D > Configured Country > NV
    1 = Configured Country > 11D > NV
    If Supplicant country code is priority than 11d is disabled.
    If 11D is enabled, we update the country code after every scan.
    Hence when Supplicant country code is priority, we don't need 11D info.
    Country code from Supplicant is set as current courtry code.
    User can send reset command XX (instead of country code) to reset the
    country code to default values which is read from NV.
    In case of reset, 11D is enabled and default NV code is Set as current country code
    If 11D is priority,
    Than Supplicant country code code is set to default code. But 11D code is set as current country code

    \param pMac - The handle returned by macOpen.
    \param pMsgBuf - MSG Buffer

    \return eHalStatus

  -------------------------------------------------------------------------------*/
eHalStatus sme_HandleChangeCountryCode(tpAniSirGlobal pMac,  void *pMsgBuf)
{
   eHalStatus  status = eHAL_STATUS_SUCCESS;
   tAniChangeCountryCodeReq *pMsg;
   v_REGDOMAIN_t domainIdIoctl;
   VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
   static uNvTables nvTables;
   pMsg = (tAniChangeCountryCodeReq *)pMsgBuf;


   /* if the reset Supplicant country code command is triggered, enable 11D, reset the NV country code and return */
   if( VOS_TRUE == vos_mem_compare(pMsg->countryCode, SME_INVALID_COUNTRY_CODE, 2) )
   {
       pMac->roam.configParam.Is11dSupportEnabled = pMac->roam.configParam.Is11dSupportEnabledOriginal;

       vosStatus = vos_nv_readDefaultCountryTable( &nvTables );

       /* read the country code from NV and use it */
       if ( VOS_IS_STATUS_SUCCESS(vosStatus) )
       {
           vos_mem_copy(pMsg->countryCode,
                        nvTables.defaultCountryTable.countryCode,
                        WNI_CFG_COUNTRY_CODE_LEN);
       }
       else
       {
           status = eHAL_STATUS_FAILURE;
           return status;
       }
       /* Update the 11d country to default country from NV bin so that when
        * callback is received for this default country, driver will not
        * disable the 11d taking it as valid country by user.
        */
       smsLog(pMac, LOG1,
         FL("Set default country code (%c%c) from NV as invalid country received"),
         pMsg->countryCode[0],pMsg->countryCode[1]);
       vos_mem_copy(pMac->scan.countryCode11d, pMsg->countryCode,
                                 WNI_CFG_COUNTRY_CODE_LEN);
   }
   else
   {
       /* if Supplicant country code has priority, disable 11d */
       if(pMac->roam.configParam.fSupplicantCountryCodeHasPriority &&
         pMsg->countryFromUserSpace)
       {
           pMac->roam.configParam.Is11dSupportEnabled = eANI_BOOLEAN_FALSE;
       }
   }

   /* WEXT set country code means
    * 11D should be supported?
    * 11D Channel should be enforced?
    * 11D Country code should be matched?
    * 11D Reg Domian should be matched?
    * Country string changed */
   if(pMac->roam.configParam.Is11dSupportEnabled &&
      pMac->roam.configParam.fEnforce11dChannels &&
      pMac->roam.configParam.fEnforceCountryCodeMatch &&
      pMac->roam.configParam.fEnforceDefaultDomain &&
      !csrSave11dCountryString(pMac, pMsg->countryCode, eANI_BOOLEAN_TRUE))
   {
      /* All 11D related options are already enabled
       * Country string is not changed
       * Do not need do anything for country code change request */
      return eHAL_STATUS_SUCCESS;
   }

   /* Set Current Country code and Current Regulatory domain */
   status = csrSetCountryCode(pMac, pMsg->countryCode, NULL);
   if(eHAL_STATUS_SUCCESS != status)
   {
       /* Supplicant country code failed. So give 11D priority */
       pMac->roam.configParam.Is11dSupportEnabled = pMac->roam.configParam.Is11dSupportEnabledOriginal;
       smsLog(pMac, LOGE, "Set Country Code Fail %d", status);
       return status;
   }

   /* overwrite the defualt country code */
   vos_mem_copy(pMac->scan.countryCodeDefault,
                pMac->scan.countryCodeCurrent,
                WNI_CFG_COUNTRY_CODE_LEN);

   /* Get Domain ID from country code */
   status = csrGetRegulatoryDomainForCountry(pMac,
                   pMac->scan.countryCodeCurrent,
                   (v_REGDOMAIN_t *) &domainIdIoctl,
                   COUNTRY_QUERY);
   if ( status != eHAL_STATUS_SUCCESS )
   {
       smsLog( pMac, LOGE, FL("  fail to get regId %d"), domainIdIoctl );
       return status;
   }
   else if (REGDOMAIN_WORLD == domainIdIoctl)
   {
       /* Supplicant country code is invalid, so we are on world mode now. So
          give 11D chance to update */
       pMac->roam.configParam.Is11dSupportEnabled = pMac->roam.configParam.Is11dSupportEnabledOriginal;
       smsLog(pMac, LOG1, FL("Country Code unrecognized by driver"));
   }


   status = WDA_SetRegDomain(pMac, domainIdIoctl, pMsg->sendRegHint);

   if ( status != eHAL_STATUS_SUCCESS )
   {
       smsLog( pMac, LOGE, FL("  fail to set regId %d"), domainIdIoctl );
       return status;
   }
   else
   {
       //if 11d has priority, clear currentCountryBssid & countryCode11d to get
       //set again if we find AP with 11d info during scan
       if (!pMac->roam.configParam.fSupplicantCountryCodeHasPriority)
       {
           smsLog( pMac, LOGW, FL("Clearing currentCountryBssid, countryCode11d"));
           vos_mem_zero(&pMac->scan.currentCountryBssid, sizeof(tCsrBssid));
           vos_mem_zero( pMac->scan.countryCode11d, sizeof( pMac->scan.countryCode11d ) );
       }
   }
#ifndef CONFIG_ENABLE_LINUX_REG
   /* set to default domain ID */
   pMac->scan.domainIdDefault = pMac->scan.domainIdCurrent;

   /* get the channels based on new cc */
   status = csrInitGetChannels( pMac );

   if ( status != eHAL_STATUS_SUCCESS )
   {
       smsLog( pMac, LOGE, FL("  fail to get Channels "));
       return status;
   }

   /* reset info based on new cc, and we are done */
   csrResetCountryInformation(pMac, eANI_BOOLEAN_TRUE, eANI_BOOLEAN_TRUE);
   /* Country code  Changed, Purge Only scan result
    * which does not have channel number belong to 11d
    * channel list
    */
   csrScanFilterResults(pMac);

#endif
   if( pMsg->changeCCCallback )
   {
      ((tSmeChangeCountryCallback)(pMsg->changeCCCallback))((void *)pMsg->pDevContext);
   }

   return eHAL_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------

    \fn sme_HandleChangeCountryCodeByUser

    \brief Change Country code, Reg Domain and channel list

    If Supplicant country code is priority than 11d is disabled.
    If 11D is enabled, we update the country code after every scan.
    Hence when Supplicant country code is priority, we don't need 11D info.
    Country code from Supplicant is set as current country code.

    \param pMac - The handle returned by macOpen.
    \param pMsg - Carrying new CC & domain set in kernel by user

    \return eHalStatus

  -------------------------------------------------------------------------------*/
eHalStatus sme_HandleChangeCountryCodeByUser(tpAniSirGlobal pMac,
                                             tAniGenericChangeCountryCodeReq *pMsg)
{
    eHalStatus  status = eHAL_STATUS_SUCCESS;
    v_REGDOMAIN_t reg_domain_id;
    v_BOOL_t is11dCountry = VOS_FALSE;

    smsLog(pMac, LOG1, FL(" called"));
    reg_domain_id =  (v_REGDOMAIN_t)pMsg->domain_index;

    if (memcmp(pMsg->countryCode, pMac->scan.countryCode11d,
               VOS_COUNTRY_CODE_LEN) == 0)
    {
        is11dCountry = VOS_TRUE;
    }

    /* Set the country code given by userspace when 11dOriginal is FALSE
     * when 11doriginal is True,is11dCountry =0 and
     * fSupplicantCountryCodeHasPriority = 0, then revert the country code,
     * and return failure
     */
    if (pMac->roam.configParam.Is11dSupportEnabledOriginal == true)
    {
        if ((!is11dCountry) && (!pMac->roam.configParam.fSupplicantCountryCodeHasPriority)&&
                           (!pMac->roam.configParam.fEnforceCountryCode) )
        {

            smsLog( pMac, LOGW, FL(" incorrect country being set, nullify this request"));

            status = csrGetRegulatoryDomainForCountry(pMac,
                                                  pMac->scan.countryCode11d,
                                                  (v_REGDOMAIN_t *) &reg_domain_id,
                                                  COUNTRY_IE);

            return eHAL_STATUS_FAILURE;
        }
    }
    pMac->roam.configParam.fEnforceCountryCode = eANI_BOOLEAN_FALSE;
    /* if Supplicant country code has priority, disable 11d */
    if (!is11dCountry && pMac->roam.configParam.fSupplicantCountryCodeHasPriority)
    {
        pMac->roam.configParam.Is11dSupportEnabled = eANI_BOOLEAN_FALSE;
        smsLog( pMac, LOG1, FL(" 11d is being  disabled"));
    }

    vos_mem_copy(pMac->scan.countryCodeCurrent, pMsg->countryCode,
                  WNI_CFG_COUNTRY_CODE_LEN);

    status = WDA_SetRegDomain(pMac, reg_domain_id, eSIR_TRUE);

    if (VOS_FALSE == is11dCountry )
    {
        /* overwrite the defualt country code */
        vos_mem_copy(pMac->scan.countryCodeDefault,
                      pMac->scan.countryCodeCurrent, WNI_CFG_COUNTRY_CODE_LEN);
        /* set to default domain ID */
        pMac->scan.domainIdDefault = pMac->scan.domainIdCurrent;
    }

    if ( status != eHAL_STATUS_SUCCESS )
    {
        smsLog( pMac, LOGE, FL("  fail to set regId %d"), reg_domain_id );
        return status;
    }
    else
    {
        //if 11d has priority, clear currentCountryBssid & countryCode11d to get
        //set again if we find AP with 11d info during scan
        if((!pMac->roam.configParam.fSupplicantCountryCodeHasPriority) &&
           (VOS_FALSE == is11dCountry ))
        {
            smsLog( pMac, LOGW, FL("Clearing currentCountryBssid, countryCode11d"));
            vos_mem_zero(&pMac->scan.currentCountryBssid, sizeof(tCsrBssid));
            vos_mem_zero( pMac->scan.countryCode11d, sizeof( pMac->scan.countryCode11d ) );
        }
    }

    /* get the channels based on new cc */
    status = csrInitGetChannels(pMac);

    if ( status != eHAL_STATUS_SUCCESS )
    {
        smsLog( pMac, LOGE, FL("  fail to get Channels "));
        return status;
    }

    /* reset info based on new cc, and we are done */
    csrResetCountryInformation(pMac, eANI_BOOLEAN_TRUE, eANI_BOOLEAN_TRUE);
    if (VOS_TRUE == is11dCountry)
    {
        pMac->scan.f11dInfoApplied = eANI_BOOLEAN_TRUE;
        pMac->scan.f11dInfoReset = eANI_BOOLEAN_FALSE;
    }
    /* Country code  Changed, Purge Only scan result
     * which does not have channel number belong to 11d
     * channel list
     */
    csrScanFilterResults(pMac);
    // Do active scans after the country is set by User hints or Country IE
    pMac->scan.curScanType = eSIR_ACTIVE_SCAN;
    sme_DisconnectConnectedSessions(pMac);
    smsLog(pMac, LOG1, FL(" returned"));
    return eHAL_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------

    \fn sme_HandleChangeCountryCodeByCore

    \brief Update Country code in the driver if set by kernel as world

    If 11D is enabled, we update the country code after every scan & notify kernel.
    This is to make sure kernel & driver are in sync in case of CC found in
    driver but not in kernel database

    \param pMac - The handle returned by macOpen.
    \param pMsg - Carrying new CC set in kernel

    \return eHalStatus

  -------------------------------------------------------------------------------*/
eHalStatus sme_HandleChangeCountryCodeByCore(tpAniSirGlobal pMac, tAniGenericChangeCountryCodeReq *pMsg)
{
    eHalStatus  status;

    smsLog(pMac, LOG1, FL(" called"));

    //this is to make sure kernel & driver are in sync in case of CC found in
    //driver but not in kernel database
    if (('0' == pMsg->countryCode[0]) && ('0' == pMsg->countryCode[1]))
    {
        smsLog( pMac, LOGW, FL("Setting countryCode11d & countryCodeCurrent to world CC"));
        vos_mem_copy(pMac->scan.countryCode11d, pMsg->countryCode,
                      WNI_CFG_COUNTRY_CODE_LEN);
        vos_mem_copy(pMac->scan.countryCodeCurrent, pMsg->countryCode,
                      WNI_CFG_COUNTRY_CODE_LEN);
    }

    status = WDA_SetRegDomain(pMac, REGDOMAIN_WORLD, eSIR_TRUE);

    if ( status != eHAL_STATUS_SUCCESS )
    {
        smsLog( pMac, LOGE, FL("  fail to set regId") );
        return status;
    }
    else
    {
        status = csrInitGetChannels(pMac);
        if ( status != eHAL_STATUS_SUCCESS )
        {
            smsLog( pMac, LOGE, FL("  fail to get Channels "));
        }
        else
        {
            csrInitChannelList(pMac);
        }
    }
    /* Country code  Changed, Purge Only scan result
     * which does not have channel number belong to 11d
     * channel list
     */
    csrScanFilterResults(pMac);
    smsLog(pMac, LOG1, FL(" returned"));
    return eHAL_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------

    \fn sme_DisconnectConnectedSessions

    \brief Disconnect STA and P2P client session if channel is not supported

    If new country code does not support the channel on which STA/P2P client
    is connetced, it sends the disconnect to the AP/P2P GO

    \param pMac - The handle returned by macOpen

    \return eHalStatus

   -------------------------------------------------------------------------------*/

void sme_DisconnectConnectedSessions(tpAniSirGlobal pMac)
{
    v_U8_t i, sessionId, isChanFound = false;
    tANI_U8 currChannel;

    for (sessionId=0; sessionId < CSR_ROAM_SESSION_MAX; sessionId++)
    {
        if (csrIsSessionClientAndConnected(pMac, sessionId))
        {
            isChanFound = false;
            //Session is connected.Check the channel
            currChannel = csrGetInfraOperationChannel(pMac, sessionId);
            smsLog(pMac, LOGW, "Current Operating channel : %d, session :%d",
                                                     currChannel, sessionId);
            for (i=0; i < pMac->scan.base20MHzChannels.numChannels; i++)
            {
                if (pMac->scan.base20MHzChannels.channelList[i] == currChannel)
                {
                    isChanFound = true;
                    break;
                }
            }

            if (!isChanFound)
            {
                for (i=0; i < pMac->scan.base40MHzChannels.numChannels; i++)
                {
                    if (pMac->scan.base40MHzChannels.channelList[i] == currChannel)
                    {
                        isChanFound = true;
                        break;
                    }
                }
            }
            if (!isChanFound)
            {
                smsLog(pMac, LOGW, "%s : Disconnect Session :%d", __func__, sessionId);
                csrRoamDisconnect(pMac, sessionId, eCSR_DISCONNECT_REASON_UNSPECIFIED);
            }
        }
    }
}
/* ---------------------------------------------------------------------------

    \fn sme_HandleGenericChangeCountryCode

    \brief Change Country code, Reg Domain and channel list

    If Supplicant country code is priority than 11d is disabled.
    If 11D is enabled, we update the country code after every scan.
    Hence when Supplicant country code is priority, we don't need 11D info.
    Country code from kernel is set as current country code.

    \param pMac - The handle returned by macOpen.
    \param pMsgBuf - message buffer

    \return eHalStatus

  -------------------------------------------------------------------------------*/
eHalStatus sme_HandleGenericChangeCountryCode(tpAniSirGlobal pMac,  void *pMsgBuf)
{
    tAniGenericChangeCountryCodeReq *pMsg;
    v_REGDOMAIN_t reg_domain_id;

    smsLog(pMac, LOG1, FL(" called"));
    pMsg = (tAniGenericChangeCountryCodeReq *)pMsgBuf;
    reg_domain_id =  (v_REGDOMAIN_t)pMsg->domain_index;

    if (REGDOMAIN_COUNT == reg_domain_id)
    {
        sme_HandleChangeCountryCodeByCore(pMac, pMsg);
    }
    else
    {
        sme_HandleChangeCountryCodeByUser(pMac, pMsg);
    }
    smsLog(pMac, LOG1, FL(" returned"));
    return eHAL_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_PACKET_FILTERING
eHalStatus sme_8023MulticastList (tHalHandle hHal, tANI_U8 sessionId, tpSirRcvFltMcAddrList pMulticastAddrs)
{
    tpSirRcvFltMcAddrList   pRequestBuf;
    vos_msg_t               msg;
    tpAniSirGlobal          pMac = PMAC_STRUCT(hHal);
    tCsrRoamSession         *pSession = NULL;

    VOS_TRACE( VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, "%s: "
               "ulMulticastAddrCnt=%d, multicastAddr[0]=%p", __func__,
               pMulticastAddrs->ulMulticastAddrCnt,
               pMulticastAddrs->multicastAddr[0]);

    /*
     *Find the connected Infra / P2P_client connected session
    */
    if (CSR_IS_SESSION_VALID(pMac, sessionId) &&
        csrIsConnStateInfra(pMac, sessionId))
    {
        pSession = CSR_GET_SESSION( pMac, sessionId );
    }

    if(pSession == NULL )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Unable to find "
            "the right session", __func__);
        return eHAL_STATUS_FAILURE;
    }

    pRequestBuf = vos_mem_malloc(sizeof(tSirRcvFltMcAddrList));
    if (NULL == pRequestBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to "
            "allocate memory for 8023 Multicast List request", __func__);
        return eHAL_STATUS_FAILED_ALLOC;
    }

    if( !csrIsConnStateConnectedInfra (pMac, sessionId ))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Ignoring the "
                       "indication as we are not connected", __func__);
        vos_mem_free(pRequestBuf);
        return eHAL_STATUS_FAILURE;
    }

    vos_mem_copy(pRequestBuf, pMulticastAddrs, sizeof(tSirRcvFltMcAddrList));

    vos_mem_copy(pRequestBuf->selfMacAddr, pSession->selfMacAddr,
                 sizeof(tSirMacAddr));
    vos_mem_copy(pRequestBuf->bssId, pSession->connectedProfile.bssid, 
                 sizeof(tSirMacAddr));

    msg.type = WDA_8023_MULTICAST_LIST_REQ;
    msg.reserved = 0;
    msg.bodyptr = pRequestBuf;
    if(VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MODULE_ID_WDA, &msg))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to "
            "post WDA_8023_MULTICAST_LIST message to WDA", __func__);
        vos_mem_free(pRequestBuf);
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}

eHalStatus sme_ReceiveFilterSetFilter(tHalHandle hHal, tpSirRcvPktFilterCfgType pRcvPktFilterCfg,
                                           tANI_U8 sessionId)
{
    tpSirRcvPktFilterCfgType    pRequestBuf;
    v_SINT_t                allocSize;
    vos_msg_t               msg;
    tpAniSirGlobal          pMac = PMAC_STRUCT(hHal);
    tCsrRoamSession         *pSession = CSR_GET_SESSION( pMac, sessionId );
    v_U8_t   idx=0;

    VOS_TRACE( VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, "%s: filterType=%d, "
               "filterId = %d", __func__,
               pRcvPktFilterCfg->filterType, pRcvPktFilterCfg->filterId);

    allocSize = sizeof(tSirRcvPktFilterCfgType);

    pRequestBuf = vos_mem_malloc(allocSize);

    if (NULL == pRequestBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to "
            "allocate memory for Receive Filter Set Filter request", __func__);
        return eHAL_STATUS_FAILED_ALLOC;
    }

    if( NULL == pSession )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Session Not found ", __func__);
        vos_mem_free(pRequestBuf);
        return eHAL_STATUS_FAILURE;
    }
    
    vos_mem_copy(pRcvPktFilterCfg->selfMacAddr, pSession->selfMacAddr,
                 sizeof(tSirMacAddr));
    vos_mem_copy(pRcvPktFilterCfg->bssId, pSession->connectedProfile.bssid,
                 sizeof(tSirMacAddr));
    vos_mem_copy(pRequestBuf, pRcvPktFilterCfg, allocSize);

    msg.type = WDA_RECEIVE_FILTER_SET_FILTER_REQ;
    msg.reserved = 0;
    msg.bodyptr = pRequestBuf;

    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, "Pkt Flt Req : "
           "FT %d FID %d ",
           pRequestBuf->filterType, pRequestBuf->filterId);

    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, "Pkt Flt Req : "
           "params %d CT %d",
           pRequestBuf->numFieldParams, pRequestBuf->coalesceTime);

    for (idx=0; idx<pRequestBuf->numFieldParams; idx++)
    {

      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
           "Proto %d Comp Flag %d ",
           pRequestBuf->paramsData[idx].protocolLayer,
           pRequestBuf->paramsData[idx].cmpFlag);

      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
           "Data Offset %d Data Len %d",
           pRequestBuf->paramsData[idx].dataOffset,
           pRequestBuf->paramsData[idx].dataLength);

      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
          "CData: %d:%d:%d:%d:%d:%d",
           pRequestBuf->paramsData[idx].compareData[0],
           pRequestBuf->paramsData[idx].compareData[1],
           pRequestBuf->paramsData[idx].compareData[2],
           pRequestBuf->paramsData[idx].compareData[3],
           pRequestBuf->paramsData[idx].compareData[4],
           pRequestBuf->paramsData[idx].compareData[5]);

      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
           "MData: %d:%d:%d:%d:%d:%d",
           pRequestBuf->paramsData[idx].dataMask[0],
           pRequestBuf->paramsData[idx].dataMask[1],
           pRequestBuf->paramsData[idx].dataMask[2],
           pRequestBuf->paramsData[idx].dataMask[3],
           pRequestBuf->paramsData[idx].dataMask[4],
           pRequestBuf->paramsData[idx].dataMask[5]);

    }

    if(VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MODULE_ID_WDA, &msg))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to post "
            "WDA_RECEIVE_FILTER_SET_FILTER message to WDA", __func__);
        vos_mem_free(pRequestBuf);
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}

eHalStatus sme_GetFilterMatchCount(tHalHandle hHal,
                                   FilterMatchCountCallback callbackRoutine,
                                   void *callbackContext,
                                   tANI_U8 sessionId)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status;

    VOS_TRACE( VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, "+%s", __func__);

    if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme)))
    {
        pmcGetFilterMatchCount(hHal, callbackRoutine, callbackContext, sessionId);
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    VOS_TRACE( VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, "-%s", __func__);

    return (status);
}

eHalStatus sme_ReceiveFilterClearFilter(tHalHandle hHal, tpSirRcvFltPktClearParam pRcvFltPktClearParam,
                                             tANI_U8 sessionId)
{
    tpSirRcvFltPktClearParam pRequestBuf;
    vos_msg_t               msg;
    tpAniSirGlobal          pMac = PMAC_STRUCT(hHal);
    tCsrRoamSession         *pSession = CSR_GET_SESSION( pMac, sessionId );

    VOS_TRACE( VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, "%s: filterId = %d", __func__,
               pRcvFltPktClearParam->filterId);

    pRequestBuf = vos_mem_malloc(sizeof(tSirRcvFltPktClearParam));
    if (NULL == pRequestBuf)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
            "%s: Not able to allocate memory for Receive Filter "
            "Clear Filter request", __func__);
        return eHAL_STATUS_FAILED_ALLOC;
    }
    if( NULL == pSession )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
            "%s: Session Not find ", __func__);
        vos_mem_free(pRequestBuf);
        return eHAL_STATUS_FAILURE;
    }

    vos_mem_copy(pRcvFltPktClearParam->selfMacAddr, pSession->selfMacAddr,
                 sizeof(tSirMacAddr));
    vos_mem_copy(pRcvFltPktClearParam->bssId, pSession->connectedProfile.bssid,
                 sizeof(tSirMacAddr));

    vos_mem_copy(pRequestBuf, pRcvFltPktClearParam, sizeof(tSirRcvFltPktClearParam));

    msg.type = WDA_RECEIVE_FILTER_CLEAR_FILTER_REQ;
    msg.reserved = 0;
    msg.bodyptr = pRequestBuf;
    if(VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MODULE_ID_WDA, &msg))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to post "
            "WDA_RECEIVE_FILTER_CLEAR_FILTER message to WDA", __func__);
        vos_mem_free(pRequestBuf);
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}
#endif // WLAN_FEATURE_PACKET_FILTERING

/* ---------------------------------------------------------------------------
    \fn sme_PreChannelSwitchIndFullPowerCB
    \brief  call back function for the PMC full power request because of pre
             channel switch.
    \param callbackContext
    \param status
  ---------------------------------------------------------------------------*/
void sme_PreChannelSwitchIndFullPowerCB(void *callbackContext,
                eHalStatus status)
{
    tpAniSirGlobal pMac = (tpAniSirGlobal)callbackContext;
    tSirMbMsg *pMsg;
    tANI_U16 msgLen;

    msgLen = (tANI_U16)(sizeof( tSirMbMsg ));
    pMsg = vos_mem_malloc(msgLen);
    if ( NULL != pMsg )
    {
        vos_mem_set(pMsg, msgLen, 0);
        pMsg->type = pal_cpu_to_be16((tANI_U16)eWNI_SME_PRE_CHANNEL_SWITCH_FULL_POWER);
        pMsg->msgLen = pal_cpu_to_be16(msgLen);
        status = palSendMBMessage(pMac->hHdd, pMsg);
    }

    return;
}

/* ---------------------------------------------------------------------------
    \fn sme_HandlePreChannelSwitchInd
    \brief  Processes the indcation from PE for pre-channel switch.
    \param hHal
    \- The handle returned by macOpen. return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_HandlePreChannelSwitchInd(tHalHandle hHal)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status = pmcRequestFullPower(hHal, sme_PreChannelSwitchIndFullPowerCB,
                            pMac, eSME_FULL_PWR_NEEDED_BY_CHANNEL_SWITCH);
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------
    \fn sme_HandlePostChannelSwitchInd
    \brief  Processes the indcation from PE for post-channel switch.
    \param hHal
    \- The handle returned by macOpen. return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_HandlePostChannelSwitchInd(tHalHandle hHal)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
       status = pmcRequestBmps(hHal, NULL, NULL);
       sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (status);
}

/* ---------------------------------------------------------------------------

    \fn sme_IsChannelValid

    \brief To check if the channel is valid for currently established domain
    This is a synchronous API.

    \param hHal - The handle returned by macOpen.
    \param channel - channel to verify

    \return TRUE/FALSE, TRUE if channel is valid

  -------------------------------------------------------------------------------*/
tANI_BOOLEAN sme_IsChannelValid(tHalHandle hHal, tANI_U8 channel)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tANI_BOOLEAN valid = FALSE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {

      valid = csrRoamIsChannelValid( pMac, channel);

      sme_ReleaseGlobalLock( &pMac->sme );
   }

   return (valid);
}

/* ---------------------------------------------------------------------------
    \fn sme_SetFreqBand
    \brief  Used to set frequency band.
    \param  hHal
    \eBand  band value to be configured
    \- return eHalStatus
    -------------------------------------------------------------------------*/
eHalStatus sme_SetFreqBand(tHalHandle hHal, eCsrBand eBand)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
      status = csrSetBand(hHal, eBand);
      sme_ReleaseGlobalLock( &pMac->sme );
   }
   return status;
}

/* ---------------------------------------------------------------------------
    \fn sme_GetFreqBand
    \brief  Used to get the current band settings.
    \param  hHal
    \pBand  pointer to hold band value
    \- return eHalStatus
    -------------------------------------------------------------------------*/
eHalStatus sme_GetFreqBand(tHalHandle hHal, eCsrBand *pBand)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   status = sme_AcquireGlobalLock( &pMac->sme );
   if ( HAL_STATUS_SUCCESS( status ) )
   {
      *pBand = csrGetCurrentBand( hHal );
      sme_ReleaseGlobalLock( &pMac->sme );
   }
   return status;
}

#ifdef WLAN_WAKEUP_EVENTS
/******************************************************************************
  \fn sme_WakeReasonIndCallback

  \brief
  a callback function called when SME received eWNI_SME_WAKE_REASON_IND event from WDA

  \param hHal - HAL handle for device
  \param pMsg - Message body passed from WDA; includes Wake Reason Indication parameter

  \return eHalStatus
******************************************************************************/
eHalStatus sme_WakeReasonIndCallback (tHalHandle hHal, void* pMsg)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
   eHalStatus status = eHAL_STATUS_SUCCESS;
   tSirWakeReasonInd *pWakeReasonInd = (tSirWakeReasonInd *)pMsg;

   if (NULL == pMsg)
   {
      smsLog(pMac, LOGE, "in %s msg ptr is NULL", __func__);
      status = eHAL_STATUS_FAILURE;
   }
   else
   {
      smsLog(pMac, LOG2, "SME: entering sme_WakeReasonIndCallback");

      /* Call Wake Reason Indication callback routine. */
      if (pMac->pmc.wakeReasonIndCB != NULL)
          pMac->pmc.wakeReasonIndCB(pMac->pmc.wakeReasonIndCBContext, pWakeReasonInd);

      smsLog(pMac, LOG1, "Wake Reason Indication in %s(), reason=%d", __func__, pWakeReasonInd->ulReason);
   }

   return(status);
}
#endif // WLAN_WAKEUP_EVENTS


/* ---------------------------------------------------------------------------

    \fn sme_SetMaxTxPower

    \brief Set the Maximum Transmit Power dynamically. Note: this setting will
    not persist over reboots.

    \param  hHal
    \param pBssid  BSSID to set the power cap for
    \param pBssid  pSelfMacAddress self MAC Address
    \param pBssid  power to set in dB
    \- return eHalStatus

  -------------------------------------------------------------------------------*/
eHalStatus sme_SetMaxTxPower(tHalHandle hHal, tSirMacAddr pBssid,
                             tSirMacAddr pSelfMacAddress, v_S7_t dB)
{
    vos_msg_t msg;
    tpMaxTxPowerParams pMaxTxParams = NULL;
    MTRACE(vos_trace(VOS_MODULE_ID_SME,
                     TRACE_CODE_SME_RX_HDD_SET_MAXTXPOW, NO_SESSION, 0));
    pMaxTxParams = vos_mem_malloc(sizeof(tMaxTxPowerParams));
    if (NULL == pMaxTxParams)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to allocate memory for pMaxTxParams", __func__);
        return eHAL_STATUS_FAILURE;
    }

    vos_mem_copy(pMaxTxParams->bssId, pBssid, SIR_MAC_ADDR_LENGTH);
    vos_mem_copy(pMaxTxParams->selfStaMacAddr, pSelfMacAddress,
                 SIR_MAC_ADDR_LENGTH);
    pMaxTxParams->power = dB;

    msg.type = WDA_SET_MAX_TX_POWER_REQ;
    msg.reserved = 0;
    msg.bodyptr = pMaxTxParams;

    if(VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MODULE_ID_WDA, &msg))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to post WDA_SET_MAX_TX_POWER_REQ message to WDA", __func__);
        vos_mem_free(pMaxTxParams);
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------

    \fn sme_SetMaxTxPowerPerBand

    \brief Set the Maximum Transmit Power specific to band dynamically.
    Note: this setting will not persist over reboots.

    \param band
    \param power to set in dB
    \- return eHalStatus

  ----------------------------------------------------------------------------*/
eHalStatus sme_SetMaxTxPowerPerBand(eCsrBand band, v_S7_t dB)
{
    vos_msg_t msg;
    tpMaxTxPowerPerBandParams pMaxTxPowerPerBandParams = NULL;

    pMaxTxPowerPerBandParams = vos_mem_malloc(sizeof(tMaxTxPowerPerBandParams));
    if (NULL == pMaxTxPowerPerBandParams)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                  "%s:Not able to allocate memory for pMaxTxPowerPerBandParams",
                  __func__);
        return eHAL_STATUS_FAILURE;
    }

    pMaxTxPowerPerBandParams->power = dB;
    pMaxTxPowerPerBandParams->bandInfo = band;

    msg.type = WDA_SET_MAX_TX_POWER_PER_BAND_REQ;
    msg.reserved = 0;
    msg.bodyptr = pMaxTxPowerPerBandParams;

    if (VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MODULE_ID_WDA, &msg))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                  "%s:Not able to post WDA_SET_MAX_TX_POWER_PER_BAND_REQ",
                  __func__);
        vos_mem_free(pMaxTxPowerPerBandParams);
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------

    \fn sme_SetTxPower

    \brief Set Transmit Power dynamically. Note: this setting will
    not persist over reboots.

    \param  hHal
    \param sessionId  Target Session ID
    \param mW  power to set in mW
    \- return eHalStatus

  -------------------------------------------------------------------------------*/
eHalStatus sme_SetTxPower(tHalHandle hHal, v_U8_t sessionId, v_U8_t mW)
{

   eHalStatus status = eHAL_STATUS_FAILURE;
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

   MTRACE(vos_trace(VOS_MODULE_ID_SME,
                 TRACE_CODE_SME_RX_HDD_SET_TXPOW, NO_SESSION, 0));
   smsLog(pMac, LOG1, FL("set tx power %dmW"), mW);
   status = sme_AcquireGlobalLock(&pMac->sme);
   if (HAL_STATUS_SUCCESS(status))
   {
      status = csrSetTxPower(pMac, sessionId, mW);
      sme_ReleaseGlobalLock(&pMac->sme);
   }
   return status;
}

/* ---------------------------------------------------------------------------

    \fn sme_HideSSID

    \brief hide/show SSID dynamically. Note: this setting will
    not persist over reboots.

    \param hHal
    \param sessionId
    \param ssidHidden 0 - Broadcast SSID, 1 - Disable broadcast SSID
    \- return eHalStatus

  -------------------------------------------------------------------------------*/
eHalStatus sme_HideSSID(tHalHandle hHal, v_U8_t sessionId, v_U8_t ssidHidden)
{
    eHalStatus status   = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tANI_U16 len;

    if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme ) ) )
    {
        tpSirUpdateParams pMsg;
        tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

        if(!pSession)
        {
            smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
            sme_ReleaseGlobalLock( &pMac->sme );
            return eHAL_STATUS_FAILURE;
        }

        if( !pSession->sessionActive )
            VOS_ASSERT(0);

        /* Create the message and send to lim */
        len = sizeof(tSirUpdateParams); 
        pMsg = vos_mem_malloc(len);
        if ( NULL == pMsg )
           status = eHAL_STATUS_FAILURE;
        else
        {
            vos_mem_set(pMsg, sizeof(tSirUpdateParams), 0);
            pMsg->messageType     = eWNI_SME_HIDE_SSID_REQ;
            pMsg->length          = len;
            /* Data starts from here */
            pMsg->sessionId       = sessionId;
            pMsg->ssidHidden      = ssidHidden;
            status = palSendMBMessage(pMac->hHdd, pMsg);
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }
   return status;
}

/* ---------------------------------------------------------------------------

    \fn sme_SetTmLevel
    \brief  Set Thermal Mitigation Level to RIVA
    \param  hHal - The handle returned by macOpen.
    \param  newTMLevel - new Thermal Mitigation Level
    \param  tmMode - Thermal Mitigation handle mode, default 0
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_SetTmLevel(tHalHandle hHal, v_U16_t newTMLevel, v_U16_t tmMode)
{
    eHalStatus          status    = eHAL_STATUS_SUCCESS;
    VOS_STATUS          vosStatus = VOS_STATUS_SUCCESS;
    tpAniSirGlobal      pMac      = PMAC_STRUCT(hHal);
    vos_msg_t           vosMessage;
    tAniSetTmLevelReq  *setTmLevelReq = NULL;

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
                         TRACE_CODE_SME_RX_HDD_SET_TMLEVEL, NO_SESSION, 0));
    if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme ) ) )
    {
        setTmLevelReq = (tAniSetTmLevelReq *)vos_mem_malloc(sizeof(tAniSetTmLevelReq));
        if (NULL == setTmLevelReq)
        {
           VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                     "%s: Not able to allocate memory for sme_SetTmLevel", __func__);
           sme_ReleaseGlobalLock( &pMac->sme );
           return eHAL_STATUS_FAILURE;
        }

        setTmLevelReq->tmMode     = tmMode;
        setTmLevelReq->newTmLevel = newTMLevel;

        /* serialize the req through MC thread */
        vosMessage.bodyptr = setTmLevelReq;
        vosMessage.type    = WDA_SET_TM_LEVEL_REQ;
        vosStatus = vos_mq_post_message( VOS_MQ_ID_WDA, &vosMessage );
        if ( !VOS_IS_STATUS_SUCCESS(vosStatus) )
        {
           VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                     "%s: Post Set TM Level MSG fail", __func__);
           vos_mem_free(setTmLevelReq);
           status = eHAL_STATUS_FAILURE;
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }
    return(status);
}

/*---------------------------------------------------------------------------

  \brief sme_featureCapsExchange() - SME interface to exchange capabilities between
  Host and FW.

  \param  hHal - HAL handle for device

  \return NONE

---------------------------------------------------------------------------*/
void sme_featureCapsExchange( tHalHandle hHal)
{
    v_CONTEXT_t vosContext = vos_get_global_context(VOS_MODULE_ID_SME, NULL);
    MTRACE(vos_trace(VOS_MODULE_ID_SME,
                     TRACE_CODE_SME_RX_HDD_CAPS_EXCH, NO_SESSION, 0));
    WDA_featureCapsExchange(vosContext);
}

/*---------------------------------------------------------------------------

  \brief sme_disableFeatureCapablity() - SME interface to disable Active mode offload capablity
  in Host.

  \param  hHal - HAL handle for device

  \return NONE

---------------------------------------------------------------------------*/
void sme_disableFeatureCapablity(tANI_U8 feature_index)
{
    WDA_disableCapablityFeature(feature_index);
}

/* ---------------------------------------------------------------------------

    \fn sme_GetDefaultCountryCode

    \brief Get the default country code from NV

    \param  hHal
    \param  pCountry
    \- return eHalStatus

  -------------------------------------------------------------------------------*/
eHalStatus sme_GetDefaultCountryCodeFrmNv(tHalHandle hHal, tANI_U8 *pCountry)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    MTRACE(vos_trace(VOS_MODULE_ID_SME,
             TRACE_CODE_SME_RX_HDD_GET_DEFCCNV, NO_SESSION, 0));
    return csrGetDefaultCountryCodeFrmNv(pMac, pCountry);
}

/* ---------------------------------------------------------------------------

    \fn sme_GetCurrentCountryCode

    \brief Get the current country code

    \param  hHal
    \param  pCountry
    \- return eHalStatus

  -------------------------------------------------------------------------------*/
eHalStatus sme_GetCurrentCountryCode(tHalHandle hHal, tANI_U8 *pCountry)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    MTRACE(vos_trace(VOS_MODULE_ID_SME,
              TRACE_CODE_SME_RX_HDD_GET_CURCC, NO_SESSION, 0));
    return csrGetCurrentCountryCode(pMac, pCountry);
}

/* ---------------------------------------------------------------------------
    \fn sme_transportDebug
    \brief  Dynamically monitoring Transport channels
            Private IOCTL will querry transport channel status if driver loaded
    \param  hHal Upper MAC context
    \param  displaySnapshot Display transport channel snapshot option
    \param  toggleStallDetect Enable stall detect feature
                              This feature will take effect to data performance
                              Not integrate till fully verification
    \- return NONE
    -------------------------------------------------------------------------*/
void sme_transportDebug(tHalHandle hHal, v_BOOL_t displaySnapshot, v_BOOL_t toggleStallDetect)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

   if (NULL == pMac)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                "%s: invalid context", __func__);
      return;
   }
   WDA_TransportChannelDebug(pMac, displaySnapshot, toggleStallDetect);
}

/* ---------------------------------------------------------------------------
    \fn     sme_ResetPowerValuesFor5G
    \brief  Reset the power values for 5G band with NV power values.
    \param  hHal - HAL handle for device
    \- return NONE
    -------------------------------------------------------------------------*/
void sme_ResetPowerValuesFor5G (tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT (hHal);
    MTRACE(vos_trace(VOS_MODULE_ID_SME,
             TRACE_CODE_SME_RX_HDD_RESET_PW5G, NO_SESSION, 0));
    csrSaveChannelPowerForBand(pMac, eANI_BOOLEAN_TRUE);
    csrApplyPower2Current(pMac);     // Store the channel+power info in the global place: Cfg
}

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
/* ---------------------------------------------------------------------------
    \fn sme_UpdateRoamPrefer5GHz
    \brief  enable/disable Roam prefer 5G runtime option
            This function is called through dynamic setConfig callback function
            to configure the Roam prefer 5G runtime option
    \param  hHal - HAL handle for device
    \param  nRoamPrefer5GHz Enable/Disable Roam prefer 5G runtime option
    \- return Success or failure
    -------------------------------------------------------------------------*/

eHalStatus sme_UpdateRoamPrefer5GHz(tHalHandle hHal, v_BOOL_t nRoamPrefer5GHz)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus          status    = eHAL_STATUS_SUCCESS;

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
                       TRACE_CODE_SME_RX_HDD_UPDATE_RP5G, NO_SESSION, 0));
    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                     "%s: gRoamPrefer5GHz is changed from %d to %d", __func__,
                      pMac->roam.configParam.nRoamPrefer5GHz,
                      nRoamPrefer5GHz);
        pMac->roam.configParam.nRoamPrefer5GHz = nRoamPrefer5GHz;
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return status ;
}

/* ---------------------------------------------------------------------------
    \fn sme_setRoamIntraBand
    \brief  enable/disable Intra band roaming
            This function is called through dynamic setConfig callback function
            to configure the intra band roaming
    \param  hHal - HAL handle for device
    \param  nRoamIntraBand Enable/Disable Intra band roaming
    \- return Success or failure
    -------------------------------------------------------------------------*/
eHalStatus sme_setRoamIntraBand(tHalHandle hHal, const v_BOOL_t nRoamIntraBand)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus          status    = eHAL_STATUS_SUCCESS;

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
              TRACE_CODE_SME_RX_HDD_SET_ROAMIBAND, NO_SESSION, 0));
    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                     "%s: gRoamIntraBand is changed from %d to %d", __func__,
                      pMac->roam.configParam.nRoamIntraBand,
                      nRoamIntraBand);
        pMac->roam.configParam.nRoamIntraBand = nRoamIntraBand;
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return status ;
}

/* ---------------------------------------------------------------------------
    \fn sme_UpdateRoamScanNProbes
    \brief  function to update roam scan N probes
            This function is called through dynamic setConfig callback function
            to update roam scan N probes
    \param  hHal - HAL handle for device
    \param  nProbes number of probe requests to be sent out
    \- return Success or failure
    -------------------------------------------------------------------------*/
eHalStatus sme_UpdateRoamScanNProbes(tHalHandle hHal, const v_U8_t nProbes)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus          status    = eHAL_STATUS_SUCCESS;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                     "%s: gRoamScanNProbes is changed from %d to %d", __func__,
                      pMac->roam.configParam.nProbes,
                      nProbes);
        pMac->roam.configParam.nProbes = nProbes;
        sme_ReleaseGlobalLock( &pMac->sme );
    }
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
    if (pMac->roam.configParam.isRoamOffloadScanEnabled)
    {
        csrRoamOffloadScan(pMac, ROAM_SCAN_OFFLOAD_UPDATE_CFG,
                           REASON_NPROBES_CHANGED);
    }
#endif
    return status ;
}

/* ---------------------------------------------------------------------------
    \fn sme_UpdateRoamScanHomeAwayTime
    \brief  function to update roam scan Home away time
            This function is called through dynamic setConfig callback function
            to update roam scan home away time
    \param  hHal - HAL handle for device
    \param  nRoamScanAwayTime Scan home away time
    \param  bSendOffloadCmd If TRUE then send offload command to firmware
                            If FALSE then command is not sent to firmware
    \- return Success or failure
    -------------------------------------------------------------------------*/
eHalStatus sme_UpdateRoamScanHomeAwayTime(tHalHandle hHal, const v_U16_t nRoamScanHomeAwayTime,
                                          const eAniBoolean bSendOffloadCmd)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus          status    = eHAL_STATUS_SUCCESS;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                     "%s: gRoamScanHomeAwayTime is changed from %d to %d", __func__,
                      pMac->roam.configParam.nRoamScanHomeAwayTime,
                      nRoamScanHomeAwayTime);
        pMac->roam.configParam.nRoamScanHomeAwayTime = nRoamScanHomeAwayTime;
        sme_ReleaseGlobalLock( &pMac->sme );
    }

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
    if (pMac->roam.configParam.isRoamOffloadScanEnabled && bSendOffloadCmd)
    {
        csrRoamOffloadScan(pMac, ROAM_SCAN_OFFLOAD_UPDATE_CFG,
                           REASON_HOME_AWAY_TIME_CHANGED);
    }
#endif
    return status;
}


/* ---------------------------------------------------------------------------
    \fn sme_getRoamIntraBand
    \brief  get Intra band roaming
    \param  hHal - HAL handle for device
    \- return Success or failure
    -------------------------------------------------------------------------*/
v_BOOL_t sme_getRoamIntraBand(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    MTRACE(vos_trace(VOS_MODULE_ID_SME,
              TRACE_CODE_SME_RX_HDD_GET_ROAMIBAND, NO_SESSION, 0));
    return pMac->roam.configParam.nRoamIntraBand;
}

/* ---------------------------------------------------------------------------
    \fn sme_getRoamScanNProbes
    \brief  get N Probes
    \param  hHal - HAL handle for device
    \- return Success or failure
    -------------------------------------------------------------------------*/
v_U8_t sme_getRoamScanNProbes(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    return pMac->roam.configParam.nProbes;
}

/* ---------------------------------------------------------------------------
    \fn sme_getRoamScanHomeAwayTime
    \brief  get Roam scan home away time
    \param  hHal - HAL handle for device
    \- return Success or failure
    -------------------------------------------------------------------------*/
v_U16_t sme_getRoamScanHomeAwayTime(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    return pMac->roam.configParam.nRoamScanHomeAwayTime;
}


/* ---------------------------------------------------------------------------
    \fn sme_UpdateImmediateRoamRssiDiff
    \brief  Update nImmediateRoamRssiDiff
            This function is called through dynamic setConfig callback function
            to configure nImmediateRoamRssiDiff
            Usage: adb shell iwpriv wlan0 setConfig gImmediateRoamRssiDiff=[0 .. 125]
    \param  hHal - HAL handle for device
    \param  nImmediateRoamRssiDiff - minimum rssi difference between potential
            candidate and current AP.
    \- return Success or failure
    -------------------------------------------------------------------------*/

eHalStatus sme_UpdateImmediateRoamRssiDiff(tHalHandle hHal, v_U8_t nImmediateRoamRssiDiff)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus          status    = eHAL_STATUS_SUCCESS;

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
             TRACE_CODE_SME_RX_HDD_UPDATE_IMMRSSIDIFF, NO_SESSION, 0));
    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                     "LFR runtime successfully set immediate roam rssi diff to"
                     "%d - old value is %d - roam state is %s",
                     nImmediateRoamRssiDiff,
                     pMac->roam.configParam.nImmediateRoamRssiDiff,
                     macTraceGetNeighbourRoamState(
                     pMac->roam.neighborRoamInfo.neighborRoamState));
        pMac->roam.configParam.nImmediateRoamRssiDiff = nImmediateRoamRssiDiff;
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return status ;
}

/* ---------------------------------------------------------------------------
    \fn sme_UpdateRoamRssiDiff
    \brief  Update RoamRssiDiff
            This function is called through dynamic setConfig callback function
            to configure RoamRssiDiff
            Usage: adb shell iwpriv wlan0 setConfig RoamRssiDiff=[0 .. 125]
    \param  hHal - HAL handle for device
    \param  RoamRssiDiff - minimum rssi difference between potential
            candidate and current AP.
    \- return Success or failure
    -------------------------------------------------------------------------*/

eHalStatus sme_UpdateRoamRssiDiff(tHalHandle hHal, v_U8_t RoamRssiDiff)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus          status    = eHAL_STATUS_SUCCESS;

    status = sme_AcquireGlobalLock( &pMac->sme );
    MTRACE(vos_trace(VOS_MODULE_ID_SME,
               TRACE_CODE_SME_RX_HDD_UPDATE_RSSIDIFF, NO_SESSION, 0));
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                     "LFR runtime successfully set roam rssi diff to %d"
                     " - old value is %d - roam state is %s",
                     RoamRssiDiff,
                     pMac->roam.configParam.RoamRssiDiff,
                     macTraceGetNeighbourRoamState(
                     pMac->roam.neighborRoamInfo.neighborRoamState));
        pMac->roam.configParam.RoamRssiDiff = RoamRssiDiff;
        sme_ReleaseGlobalLock( &pMac->sme );
    }
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
    if (pMac->roam.configParam.isRoamOffloadScanEnabled)
    {
       csrRoamOffloadScan(pMac, ROAM_SCAN_OFFLOAD_UPDATE_CFG, REASON_RSSI_DIFF_CHANGED);
    }
#endif
    return status ;
}

/*--------------------------------------------------------------------------
  \brief sme_UpdateFastTransitionEnabled() - enable/disable Fast Transition support at runtime
  It is used at in the REG_DYNAMIC_VARIABLE macro definition of
  isFastTransitionEnabled.
  This is a synchronous call
  \param hHal - The handle returned by macOpen.
  \return eHAL_STATUS_SUCCESS - SME update isFastTransitionEnabled config successfully.
          Other status means SME is failed to update isFastTransitionEnabled.
  \sa
  --------------------------------------------------------------------------*/
eHalStatus sme_UpdateFastTransitionEnabled(tHalHandle hHal,
        v_BOOL_t isFastTransitionEnabled)
{
  tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus          status    = eHAL_STATUS_SUCCESS;

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
            TRACE_CODE_SME_RX_HDD_UPDATE_FTENABLED, NO_SESSION, 0));
    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                     "%s: FastTransitionEnabled is changed from %d to %d", __func__,
                      pMac->roam.configParam.isFastTransitionEnabled,
                      isFastTransitionEnabled);
        pMac->roam.configParam.isFastTransitionEnabled = isFastTransitionEnabled;
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return status ;
}

/* ---------------------------------------------------------------------------
    \fn sme_UpdateWESMode
    \brief  Update WES Mode
            This function is called through dynamic setConfig callback function
            to configure isWESModeEnabled
    \param  hHal - HAL handle for device
    \return eHAL_STATUS_SUCCESS - SME update isWESModeEnabled config successfully.
          Other status means SME is failed to update isWESModeEnabled.
    -------------------------------------------------------------------------*/

eHalStatus sme_UpdateWESMode(tHalHandle hHal, v_BOOL_t isWESModeEnabled)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus          status    = eHAL_STATUS_SUCCESS;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                     "LFR runtime successfully set WES Mode to %d"
                     "- old value is %d - roam state is %s",
                     isWESModeEnabled,
                     pMac->roam.configParam.isWESModeEnabled,
                     macTraceGetNeighbourRoamState(
                     pMac->roam.neighborRoamInfo.neighborRoamState));
        pMac->roam.configParam.isWESModeEnabled = isWESModeEnabled;
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return status ;
}

/* ---------------------------------------------------------------------------
    \fn sme_SetRoamScanControl
    \brief  Set roam scan control
            This function is called to set roam scan control
            if roam scan control is set to 0, roaming scan cache is cleared
            any value other than 0 is treated as invalid value
    \param  hHal - HAL handle for device
    \return eHAL_STATUS_SUCCESS - SME update config successfully.
          Other status means SME failure to update
    -------------------------------------------------------------------------*/
eHalStatus sme_SetRoamScanControl(tHalHandle hHal, v_BOOL_t roamScanControl)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus          status    = eHAL_STATUS_SUCCESS;

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
             TRACE_CODE_SME_RX_HDD_SET_SCANCTRL, NO_SESSION, 0));
    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                     "LFR runtime successfully set roam scan control to %d"
                     " - old value is %d - roam state is %s",
                     roamScanControl,
                     pMac->roam.configParam.nRoamScanControl,
                     macTraceGetNeighbourRoamState(
                     pMac->roam.neighborRoamInfo.neighborRoamState));
        pMac->roam.configParam.nRoamScanControl = roamScanControl;
        if ( 0 == roamScanControl)
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                     "LFR runtime successfully cleared roam scan cache");
            csrFlushCfgBgScanRoamChannelList(pMac);
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
           if (pMac->roam.configParam.isRoamOffloadScanEnabled)
           {
               csrRoamOffloadScan(pMac, ROAM_SCAN_OFFLOAD_UPDATE_CFG, REASON_FLUSH_CHANNEL_LIST);
           }
#endif
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }
    return status ;
}
#endif /* (WLAN_FEATURE_VOWIFI_11R) || (FEATURE_WLAN_ESE) || (FEATURE_WLAN_LFR) */

#ifdef FEATURE_WLAN_LFR
/*--------------------------------------------------------------------------
  \brief sme_UpdateIsFastRoamIniFeatureEnabled() - enable/disable LFR support at runtime
  It is used at in the REG_DYNAMIC_VARIABLE macro definition of
  isFastRoamIniFeatureEnabled.
  This is a synchronous call
  \param hHal - The handle returned by macOpen.
  \return eHAL_STATUS_SUCCESS - SME update isFastRoamIniFeatureEnabled config successfully.
          Other status means SME is failed to update isFastRoamIniFeatureEnabled.
  \sa
  --------------------------------------------------------------------------*/
eHalStatus sme_UpdateIsFastRoamIniFeatureEnabled(tHalHandle hHal,
        const v_BOOL_t isFastRoamIniFeatureEnabled)
{
  tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

  if (pMac->roam.configParam.isFastRoamIniFeatureEnabled == isFastRoamIniFeatureEnabled)
  {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                     "%s: FastRoam is already enabled or disabled, nothing to do (returning) old(%d) new(%d)", __func__,
                      pMac->roam.configParam.isFastRoamIniFeatureEnabled,
                      isFastRoamIniFeatureEnabled);
      return eHAL_STATUS_SUCCESS;
  }

  VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                     "%s: FastRoamEnabled is changed from %d to %d", __func__,
                      pMac->roam.configParam.isFastRoamIniFeatureEnabled,
                      isFastRoamIniFeatureEnabled);
  pMac->roam.configParam.isFastRoamIniFeatureEnabled = isFastRoamIniFeatureEnabled;
  csrNeighborRoamUpdateFastRoamingEnabled(pMac, isFastRoamIniFeatureEnabled);

  return eHAL_STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
  \brief sme_ConfigFwrRoaming() - enable/disable LFR support at runtime
  When Supplicant issue enabled / disable fwr based roaming on the basis
  of the Bssid modification in network block ( e.g. AutoJoin mody N/W block)

  This is a synchronous call
  \param hHal - The handle returned by macOpen.
  \return eHAL_STATUS_SUCCESS - SME (enabled/disabled) offload scan successfully.
          Other status means SME is failed to (enabled/disabled) offload scan.
  \sa
  --------------------------------------------------------------------------*/

eHalStatus sme_ConfigFwrRoaming(tHalHandle hHal,
        const v_BOOL_t isFastRoamEnabled)
{
  tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
  if (!pMac->roam.configParam.isFastRoamIniFeatureEnabled)
  {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                 "%s: FastRoam is disabled through ini", __func__);
      return  eHAL_STATUS_FAILURE;
  }
  csrNeighborRoamUpdateFastRoamingEnabled(pMac, isFastRoamEnabled);
  return eHAL_STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------
  \brief sme_UpdateIsMAWCIniFeatureEnabled() -
  Enable/disable LFR MAWC support at runtime
  It is used at in the REG_DYNAMIC_VARIABLE macro definition of
  isMAWCIniFeatureEnabled.
  This is a synchronous call
  \param hHal - The handle returned by macOpen.
  \return eHAL_STATUS_SUCCESS - SME update MAWCEnabled config successfully.
          Other status means SME is failed to update MAWCEnabled.
  \sa
  --------------------------------------------------------------------------*/
eHalStatus sme_UpdateIsMAWCIniFeatureEnabled(tHalHandle hHal,
        const v_BOOL_t MAWCEnabled)
{
  tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
  eHalStatus status = eHAL_STATUS_SUCCESS;

  status = sme_AcquireGlobalLock( &pMac->sme );
  if ( HAL_STATUS_SUCCESS( status ) )
  {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                "%s: MAWCEnabled is changed from %d to %d", __func__,
                pMac->roam.configParam.MAWCEnabled,
                MAWCEnabled);
      pMac->roam.configParam.MAWCEnabled = MAWCEnabled;
      sme_ReleaseGlobalLock( &pMac->sme );
  }

  return status ;

}

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
/*--------------------------------------------------------------------------
  \brief sme_UpdateEnableFastRoamInConcurrency() - enable/disable LFR if Concurrent session exists
  This is a synchronuous call
  \param hHal - The handle returned by macOpen.
  \return eHAL_STATUS_SUCCESS
          Other status means SME is failed
  \sa
  --------------------------------------------------------------------------*/

eHalStatus sme_UpdateEnableFastRoamInConcurrency(tHalHandle hHal,
                          v_BOOL_t bFastRoamInConIniFeatureEnabled)
{

    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus  status  = eHAL_STATUS_SUCCESS;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        pMac->roam.configParam.bFastRoamInConIniFeatureEnabled = bFastRoamInConIniFeatureEnabled;
        if (0 == pMac->roam.configParam.isRoamOffloadScanEnabled)
        {
            pMac->roam.configParam.bFastRoamInConIniFeatureEnabled = 0;
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return status;
}
#endif
#endif /* FEATURE_WLAN_LFR */

#ifdef FEATURE_WLAN_ESE
/*--------------------------------------------------------------------------
  \brief sme_UpdateIsEseFeatureEnabled() - enable/disable Ese support at runtime
  It is used at in the REG_DYNAMIC_VARIABLE macro definition of
  isEseIniFeatureEnabled.
  This is a synchronous call
  \param hHal - The handle returned by macOpen.
  \return eHAL_STATUS_SUCCESS - SME update isEseIniFeatureEnabled config successfully.
          Other status means SME is failed to update isEseIniFeatureEnabled.
  \sa
  --------------------------------------------------------------------------*/

eHalStatus sme_UpdateIsEseFeatureEnabled(tHalHandle hHal,
                const v_BOOL_t isEseIniFeatureEnabled)
{
  tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

  if (pMac->roam.configParam.isEseIniFeatureEnabled == isEseIniFeatureEnabled)
  {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                     "%s: Ese Mode is already enabled or disabled, nothing to do (returning) old(%d) new(%d)", __func__,
                      pMac->roam.configParam.isEseIniFeatureEnabled,
                      isEseIniFeatureEnabled);
      return eHAL_STATUS_SUCCESS;
  }

  VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                     "%s: EseEnabled is changed from %d to %d", __func__,
                      pMac->roam.configParam.isEseIniFeatureEnabled,
                      isEseIniFeatureEnabled);
  pMac->roam.configParam.isEseIniFeatureEnabled = isEseIniFeatureEnabled;
  csrNeighborRoamUpdateEseModeEnabled(pMac, isEseIniFeatureEnabled);

  if(TRUE == isEseIniFeatureEnabled)
  {
      sme_UpdateFastTransitionEnabled(hHal, TRUE);
  }

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
    if (pMac->roam.configParam.isRoamOffloadScanEnabled)
    {
       csrRoamOffloadScan(pMac, ROAM_SCAN_OFFLOAD_UPDATE_CFG, REASON_ESE_INI_CFG_CHANGED);
    }
#endif
  return eHAL_STATUS_SUCCESS;
}
#endif /* FEATURE_WLAN_ESE */

/*--------------------------------------------------------------------------
  \brief sme_UpdateConfigFwRssiMonitoring() - enable/disable firmware RSSI Monitoring at runtime
  It is used at in the REG_DYNAMIC_VARIABLE macro definition of
  fEnableFwRssiMonitoring.
  This is a synchronous call
  \param hHal - The handle returned by macOpen.
  \return eHAL_STATUS_SUCCESS - SME update fEnableFwRssiMonitoring. config successfully.
          Other status means SME is failed to update fEnableFwRssiMonitoring.
  \sa
  --------------------------------------------------------------------------*/

eHalStatus sme_UpdateConfigFwRssiMonitoring(tHalHandle hHal,
        v_BOOL_t fEnableFwRssiMonitoring)
{
    eHalStatus halStatus = eHAL_STATUS_SUCCESS;

    if (ccmCfgSetInt(hHal, WNI_CFG_PS_ENABLE_RSSI_MONITOR, fEnableFwRssiMonitoring,
                    NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {
        halStatus = eHAL_STATUS_FAILURE;
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                     "Failure: Could not pass on WNI_CFG_PS_RSSI_MONITOR configuration info to CCM");
    }

    return (halStatus);
}

#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
/*--------------------------------------------------------------------------
  \brief sme_setNeighborLookupRssiThreshold() - update neighbor lookup rssi threshold
  This is a synchronous call
  \param hHal - The handle returned by macOpen.
  \return eHAL_STATUS_SUCCESS - SME update config successful.
          Other status means SME is failed to update
  \sa
  --------------------------------------------------------------------------*/
eHalStatus sme_setNeighborLookupRssiThreshold(tHalHandle hHal,
               v_U8_t neighborLookupRssiThreshold)
{
    tpAniSirGlobal pMac    = PMAC_STRUCT( hHal );
    eHalStatus     status  = eHAL_STATUS_SUCCESS;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        status = csrNeighborRoamSetLookupRssiThreshold(pMac, neighborLookupRssiThreshold);
        if (HAL_STATUS_SUCCESS(status))
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                         "LFR runtime successfully set Lookup threshold to %d"
                         " - old value is %d - roam state is %s",
                         neighborLookupRssiThreshold,
                         pMac->roam.configParam.neighborRoamConfig.nNeighborLookupRssiThreshold,
                         macTraceGetNeighbourRoamState(
                         pMac->roam.neighborRoamInfo.neighborRoamState));
            pMac->roam.configParam.neighborRoamConfig.nNeighborLookupRssiThreshold =
                                            neighborLookupRssiThreshold;
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }
    return status;
}

/*--------------------------------------------------------------------------
  \brief sme_setNeighborReassocRssiThreshold() - update neighbor reassoc rssi threshold
  This is a synchronous call
  \param hHal - The handle returned by macOpen.
  \return eHAL_STATUS_SUCCESS - SME update config successful.
          Other status means SME is failed to update
  \sa
  --------------------------------------------------------------------------*/
eHalStatus sme_setNeighborReassocRssiThreshold(tHalHandle hHal,
        v_U8_t neighborReassocRssiThreshold)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus          status    = eHAL_STATUS_SUCCESS;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                     "LFR runtime successfully set Reassoc threshold to %d"
                     "- old value is %d - roam state is %s",
                     neighborReassocRssiThreshold,
                     pMac->roam.configParam.neighborRoamConfig.nNeighborReassocRssiThreshold,
                     macTraceGetNeighbourRoamState(
                     pMac->roam.neighborRoamInfo.neighborRoamState));
        pMac->roam.configParam.neighborRoamConfig.nNeighborReassocRssiThreshold =
                                      neighborReassocRssiThreshold;
        pMac->roam.neighborRoamInfo.cfgParams.neighborReassocThreshold =
                                      neighborReassocRssiThreshold;
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return status ;
}


/*--------------------------------------------------------------------------
  \brief sme_getNeighborLookupRssiThreshold() - get neighbor lookup rssi threshold
  This is a synchronous call
  \param hHal - The handle returned by macOpen.
  \return eHAL_STATUS_SUCCESS - SME update config successful.
          Other status means SME is failed to update
  \sa
  --------------------------------------------------------------------------*/
v_U8_t sme_getNeighborLookupRssiThreshold(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    return pMac->roam.configParam.neighborRoamConfig.nNeighborLookupRssiThreshold;
}

/*--------------------------------------------------------------------------
  \brief sme_setNeighborScanRefreshPeriod() - set neighbor scan results refresh period
  This is a synchronous call
  \param hHal - The handle returned by macOpen.
  \return eHAL_STATUS_SUCCESS - SME update config successful.
          Other status means SME is failed to update
  \sa
  --------------------------------------------------------------------------*/
eHalStatus sme_setNeighborScanRefreshPeriod(tHalHandle hHal,
                 v_U16_t neighborScanResultsRefreshPeriod)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus          status    = eHAL_STATUS_SUCCESS;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                     "LFR runtime successfully set roam scan refresh period to %d"
                     " - old value is %d - roam state is %s",
                     neighborScanResultsRefreshPeriod,
                     pMac->roam.configParam.neighborRoamConfig.nNeighborResultsRefreshPeriod,
                     macTraceGetNeighbourRoamState(
                     pMac->roam.neighborRoamInfo.neighborRoamState));
        pMac->roam.configParam.neighborRoamConfig.nNeighborResultsRefreshPeriod =
                                  neighborScanResultsRefreshPeriod;
        pMac->roam.neighborRoamInfo.cfgParams.neighborResultsRefreshPeriod =
                                  neighborScanResultsRefreshPeriod;

        sme_ReleaseGlobalLock( &pMac->sme );
    }

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
    if (pMac->roam.configParam.isRoamOffloadScanEnabled)
    {
       csrRoamOffloadScan(pMac, ROAM_SCAN_OFFLOAD_UPDATE_CFG,
                          REASON_NEIGHBOR_SCAN_REFRESH_PERIOD_CHANGED);
    }
#endif
    return status ;
}

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
/*--------------------------------------------------------------------------
  \brief sme_UpdateRoamScanOffloadEnabled() - enable/disable roam scan offload feaure
  It is used at in the REG_DYNAMIC_VARIABLE macro definition of
  gRoamScanOffloadEnabled.
  This is a synchronous call
  \param hHal - The handle returned by macOpen.
  \return eHAL_STATUS_SUCCESS - SME update config successfully.
          Other status means SME is failed to update.
  \sa
  --------------------------------------------------------------------------*/

eHalStatus sme_UpdateRoamScanOffloadEnabled(tHalHandle hHal,
        v_BOOL_t nRoamScanOffloadEnabled)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus          status    = eHAL_STATUS_SUCCESS;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                     "%s: gRoamScanOffloadEnabled is changed from %d to %d", __func__,
                      pMac->roam.configParam.isRoamOffloadScanEnabled,
                      nRoamScanOffloadEnabled);
        pMac->roam.configParam.isRoamOffloadScanEnabled = nRoamScanOffloadEnabled;
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return status ;
}
#endif

/*--------------------------------------------------------------------------
  \brief sme_getNeighborScanRefreshPeriod() - get neighbor scan results refresh period
  This is a synchronous call
  \param hHal - The handle returned by macOpen.
  \return v_U16_t - Neighbor scan results refresh period value
  \sa
  --------------------------------------------------------------------------*/
v_U16_t sme_getNeighborScanRefreshPeriod(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    return pMac->roam.configParam.neighborRoamConfig.nNeighborResultsRefreshPeriod;
}

/*--------------------------------------------------------------------------
  \brief sme_getEmptyScanRefreshPeriod() - get empty scan refresh period
  This is a synchronuous call
  \param hHal - The handle returned by macOpen.
  \return eHAL_STATUS_SUCCESS - SME update config successful.
          Other status means SME is failed to update
  \sa
  --------------------------------------------------------------------------*/
v_U16_t sme_getEmptyScanRefreshPeriod(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    return pMac->roam.configParam.neighborRoamConfig.nEmptyScanRefreshPeriod;
}

/* ---------------------------------------------------------------------------
    \fn sme_UpdateEmptyScanRefreshPeriod
    \brief  Update nEmptyScanRefreshPeriod
            This function is called through dynamic setConfig callback function
            to configure nEmptyScanRefreshPeriod
            Usage: adb shell iwpriv wlan0 setConfig nEmptyScanRefreshPeriod=[0 .. 60]
    \param  hHal - HAL handle for device
    \param  nEmptyScanRefreshPeriod - scan period following empty scan results.
    \- return Success or failure
    -------------------------------------------------------------------------*/

eHalStatus sme_UpdateEmptyScanRefreshPeriod(tHalHandle hHal, v_U16_t nEmptyScanRefreshPeriod)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus          status    = eHAL_STATUS_SUCCESS;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                     "LFR runtime successfully set roam scan period to %d -"
                     "old value is %d - roam state is %s",
                     nEmptyScanRefreshPeriod,
                     pMac->roam.configParam.neighborRoamConfig.nEmptyScanRefreshPeriod,
                     macTraceGetNeighbourRoamState(
                     pMac->roam.neighborRoamInfo.neighborRoamState));
        pMac->roam.configParam.neighborRoamConfig.nEmptyScanRefreshPeriod = nEmptyScanRefreshPeriod;
        pMac->roam.neighborRoamInfo.cfgParams.emptyScanRefreshPeriod = nEmptyScanRefreshPeriod;
        sme_ReleaseGlobalLock( &pMac->sme );
    }

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
    if (pMac->roam.configParam.isRoamOffloadScanEnabled)
    {
       csrRoamOffloadScan(pMac, ROAM_SCAN_OFFLOAD_UPDATE_CFG,
                          REASON_EMPTY_SCAN_REF_PERIOD_CHANGED);
    }
#endif
    return status ;
}

/* ---------------------------------------------------------------------------
    \fn sme_setNeighborScanMinChanTime
    \brief  Update nNeighborScanMinChanTime
            This function is called through dynamic setConfig callback function
            to configure gNeighborScanChannelMinTime
            Usage: adb shell iwpriv wlan0 setConfig gNeighborScanChannelMinTime=[0 .. 60]
    \param  hHal - HAL handle for device
    \param  nNeighborScanMinChanTime - Channel minimum dwell time
    \- return Success or failure
    -------------------------------------------------------------------------*/
eHalStatus sme_setNeighborScanMinChanTime(tHalHandle hHal, const v_U16_t nNeighborScanMinChanTime)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus          status    = eHAL_STATUS_SUCCESS;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                     "LFR runtime successfully set channel min dwell time to %d"
                     " - old value is %d - roam state is %s",
                     nNeighborScanMinChanTime,
                     pMac->roam.configParam.neighborRoamConfig.nNeighborScanMinChanTime,
                     macTraceGetNeighbourRoamState(
                     pMac->roam.neighborRoamInfo.neighborRoamState));
        pMac->roam.configParam.neighborRoamConfig.nNeighborScanMinChanTime = nNeighborScanMinChanTime;
        pMac->roam.neighborRoamInfo.cfgParams.minChannelScanTime = nNeighborScanMinChanTime;
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return status ;
}

/* ---------------------------------------------------------------------------
    \fn sme_setNeighborScanMaxChanTime
    \brief  Update nNeighborScanMaxChanTime
            This function is called through dynamic setConfig callback function
            to configure gNeighborScanChannelMaxTime
            Usage: adb shell iwpriv wlan0 setConfig gNeighborScanChannelMaxTime=[0 .. 60]
    \param  hHal - HAL handle for device
    \param  nNeighborScanMinChanTime - Channel maximum dwell time
    \- return Success or failure
    -------------------------------------------------------------------------*/
eHalStatus sme_setNeighborScanMaxChanTime(tHalHandle hHal, const v_U16_t nNeighborScanMaxChanTime)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus          status    = eHAL_STATUS_SUCCESS;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                     "LFR runtime successfully set channel max dwell time to %d"
                     " - old value is %d - roam state is %s",
                     nNeighborScanMaxChanTime,
                     pMac->roam.configParam.neighborRoamConfig.nNeighborScanMaxChanTime,
                     macTraceGetNeighbourRoamState(
                     pMac->roam.neighborRoamInfo.neighborRoamState));
        pMac->roam.configParam.neighborRoamConfig.nNeighborScanMaxChanTime = nNeighborScanMaxChanTime;
        pMac->roam.neighborRoamInfo.cfgParams.maxChannelScanTime = nNeighborScanMaxChanTime;
        sme_ReleaseGlobalLock( &pMac->sme );
    }
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
    if (pMac->roam.configParam.isRoamOffloadScanEnabled)
    {
       csrRoamOffloadScan(pMac, ROAM_SCAN_OFFLOAD_UPDATE_CFG,
                          REASON_SCAN_CH_TIME_CHANGED);
    }
#endif

    return status ;
}

/* ---------------------------------------------------------------------------
    \fn sme_getNeighborScanMinChanTime
    \brief  get neighbor scan min channel time
    \param hHal - The handle returned by macOpen.
    \return v_U16_t - channel min time value
    -------------------------------------------------------------------------*/
v_U16_t sme_getNeighborScanMinChanTime(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    return pMac->roam.neighborRoamInfo.cfgParams.minChannelScanTime;
}

/* ---------------------------------------------------------------------------
    \fn sme_getNeighborScanMaxChanTime
    \brief  get neighbor scan max channel time
    \param hHal - The handle returned by macOpen.
    \return v_U16_t - channel max time value
    -------------------------------------------------------------------------*/
v_U16_t sme_getNeighborScanMaxChanTime(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    return pMac->roam.neighborRoamInfo.cfgParams.maxChannelScanTime;
}

/* ---------------------------------------------------------------------------
    \fn sme_setNeighborScanPeriod
    \brief  Update nNeighborScanPeriod
            This function is called through dynamic setConfig callback function
            to configure nNeighborScanPeriod
            Usage: adb shell iwpriv wlan0 setConfig nNeighborScanPeriod=[0 .. 1000]
    \param  hHal - HAL handle for device
    \param  nNeighborScanPeriod - neighbor scan period
    \- return Success or failure
    -------------------------------------------------------------------------*/
eHalStatus sme_setNeighborScanPeriod(tHalHandle hHal, const v_U16_t nNeighborScanPeriod)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus          status    = eHAL_STATUS_SUCCESS;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                     "LFR runtime successfully set neighbor scan period to %d"
                     " - old value is %d - roam state is %s",
                     nNeighborScanPeriod,
                     pMac->roam.configParam.neighborRoamConfig.nNeighborScanTimerPeriod,
                     macTraceGetNeighbourRoamState(
                     pMac->roam.neighborRoamInfo.neighborRoamState));
        pMac->roam.configParam.neighborRoamConfig.nNeighborScanTimerPeriod = nNeighborScanPeriod;
        pMac->roam.neighborRoamInfo.cfgParams.neighborScanPeriod = nNeighborScanPeriod;
        sme_ReleaseGlobalLock( &pMac->sme );
    }
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
    if (pMac->roam.configParam.isRoamOffloadScanEnabled)
    {
       csrRoamOffloadScan(pMac, ROAM_SCAN_OFFLOAD_UPDATE_CFG,
                          REASON_SCAN_HOME_TIME_CHANGED);
    }
#endif

    return status ;
}

/* ---------------------------------------------------------------------------
    \fn sme_getNeighborScanPeriod
    \brief  get neighbor scan period
    \param hHal - The handle returned by macOpen.
    \return v_U16_t - neighbor scan period
    -------------------------------------------------------------------------*/
v_U16_t sme_getNeighborScanPeriod(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    return pMac->roam.neighborRoamInfo.cfgParams.neighborScanPeriod;
}

#endif

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)

/*--------------------------------------------------------------------------
  \brief sme_getRoamRssiDiff() - get Roam rssi diff
  This is a synchronous call
  \param hHal - The handle returned by macOpen.
  \return v_U16_t - Rssi diff value
  \sa
  --------------------------------------------------------------------------*/
v_U8_t sme_getRoamRssiDiff(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    return pMac->roam.configParam.RoamRssiDiff;
}

/*--------------------------------------------------------------------------
  \brief sme_ChangeRoamScanChannelList() - Change roam scan channel list
  This is a synchronous call
  \param hHal - The handle returned by macOpen.
  \return eHAL_STATUS_SUCCESS - SME update config successful.
          Other status means SME is failed to update
  \sa
  --------------------------------------------------------------------------*/
eHalStatus sme_ChangeRoamScanChannelList(tHalHandle hHal, tANI_U8 *pChannelList,
                                         tANI_U8 numChannels)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus          status    = eHAL_STATUS_SUCCESS;
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    tANI_U8 oldChannelList[WNI_CFG_VALID_CHANNEL_LIST_LEN*2] = {0};
    tANI_U8 newChannelList[WNI_CFG_VALID_CHANNEL_LIST_LEN*2] = {0};
    tANI_U8 i = 0, j = 0;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        if (NULL != pNeighborRoamInfo->cfgParams.channelInfo.ChannelList)
        {
            for (i = 0; i < pNeighborRoamInfo->cfgParams.channelInfo.numOfChannels; i++)
            {
              if (j < sizeof(oldChannelList))
              {
                 j += snprintf(oldChannelList + j, sizeof(oldChannelList) - j," %d",
                 pNeighborRoamInfo->cfgParams.channelInfo.ChannelList[i]);
              }
              else
              {
                 break;
              }
            }
        }
        csrFlushCfgBgScanRoamChannelList(pMac);
        csrCreateBgScanRoamChannelList(pMac, pChannelList, numChannels);
        sme_SetRoamScanControl(hHal, 1);
        if (NULL != pNeighborRoamInfo->cfgParams.channelInfo.ChannelList)
        {
            j = 0;
            for (i = 0; i < pNeighborRoamInfo->cfgParams.channelInfo.numOfChannels; i++)
            {
              if (j < sizeof(oldChannelList))
              {
                 j += snprintf(newChannelList + j, sizeof(newChannelList) - j," %d",
                 pNeighborRoamInfo->cfgParams.channelInfo.ChannelList[i]);
              }
              else
              {
                 break;
              }
            }
        }

        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                     "LFR runtime successfully set roam scan channels to %s"
                     "- old value is %s - roam state is %s",
                     newChannelList, oldChannelList,
                     macTraceGetNeighbourRoamState(
                     pMac->roam.neighborRoamInfo.neighborRoamState));
        sme_ReleaseGlobalLock( &pMac->sme );
    }
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
    if (pMac->roam.configParam.isRoamOffloadScanEnabled)
    {
       csrRoamOffloadScan(pMac, ROAM_SCAN_OFFLOAD_UPDATE_CFG, REASON_CHANNEL_LIST_CHANGED);
    }
#endif

    return status ;
}


#ifdef FEATURE_WLAN_ESE_UPLOAD
/*--------------------------------------------------------------------------
  \brief sme_SetEseRoamScanChannelList() - set ese roam scan channel list
  This is a synchronuous call
  \param hHal - The handle returned by macOpen.
  \return eHAL_STATUS_SUCCESS - SME update config successful.
          Other status means SME is failed to update
  \sa
  --------------------------------------------------------------------------*/
eHalStatus sme_SetEseRoamScanChannelList(tHalHandle hHal,
                                         tANI_U8 *pChannelList,
                                         tANI_U8 numChannels)
{
    tpAniSirGlobal      pMac = PMAC_STRUCT( hHal );
    eHalStatus          status    = eHAL_STATUS_SUCCESS;
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    tpCsrChannelInfo    currChannelListInfo = &pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo;
    tANI_U8             oldChannelList[WNI_CFG_VALID_CHANNEL_LIST_LEN*2] = {0};
    tANI_U8             newChannelList[128] = {0};
    tANI_U8             i = 0, j = 0;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        if (NULL != currChannelListInfo->ChannelList)
        {
            for (i = 0; i < currChannelListInfo->numOfChannels; i++)
            {
                j += snprintf(oldChannelList + j, sizeof(oldChannelList) - j," %d",
                      currChannelListInfo->ChannelList[i]);
            }
        }
        status = csrCreateRoamScanChannelList(pMac, pChannelList, numChannels, csrGetCurrentBand(hHal));

        if ( HAL_STATUS_SUCCESS( status ))
        {
            if (NULL != currChannelListInfo->ChannelList)
            {
                j = 0;
                for (i = 0; i < currChannelListInfo->numOfChannels; i++)
                {
                    j += snprintf(newChannelList + j, sizeof(newChannelList) - j," %d",
                    currChannelListInfo->ChannelList[i]);
                }
            }

            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                        "ESE roam scan channel list successfully set to %s - old value is %s - roam state is %s",
                        newChannelList, oldChannelList,
                        macTraceGetNeighbourRoamState(pMac->roam.neighborRoamInfo.neighborRoamState));
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
        if (pMac->roam.configParam.isRoamOffloadScanEnabled)
        {
           csrRoamOffloadScan(pMac, ROAM_SCAN_OFFLOAD_UPDATE_CFG, REASON_CHANNEL_LIST_CHANGED);
        }
#endif

    return status ;
}
#endif

/*--------------------------------------------------------------------------
  \brief sme_getRoamScanChannelList() - get roam scan channel list
  This is a synchronous call
  \param hHal - The handle returned by macOpen.
  \return eHAL_STATUS_SUCCESS - SME update config successful.
          Other status means SME is failed to update
  \sa
  --------------------------------------------------------------------------*/
eHalStatus sme_getRoamScanChannelList(tHalHandle hHal, tANI_U8 *pChannelList,
                                         tANI_U8 *pNumChannels)
{
    int i  = 0;
    tANI_U8 *pOutPtr = pChannelList;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    eHalStatus          status    = eHAL_STATUS_SUCCESS;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        if (NULL == pNeighborRoamInfo->cfgParams.channelInfo.ChannelList)
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_WARN,
                     "Roam Scan channel list is NOT yet initialized");
            *pNumChannels = 0;
            sme_ReleaseGlobalLock( &pMac->sme );
            return status;
        }

        *pNumChannels = pNeighborRoamInfo->cfgParams.channelInfo.numOfChannels;
        for (i = 0; i < (*pNumChannels); i++)
        {
            pOutPtr[i] = pNeighborRoamInfo->cfgParams.channelInfo.ChannelList[i];
        }
        pOutPtr[i] = '\0';
        sme_ReleaseGlobalLock( &pMac->sme );
    }
    return status ;
}

/*--------------------------------------------------------------------------
  \brief sme_getIsEseFeatureEnabled() - get Ese feature enabled or not
  This is a synchronuous call
  \param hHal - The handle returned by macOpen.
  \return TRUE (1) - if the Ese feature is enabled
          FALSE (0) - if feature is disabled (compile or runtime)
  \sa
  --------------------------------------------------------------------------*/
tANI_BOOLEAN sme_getIsEseFeatureEnabled(tHalHandle hHal)
{
#ifdef FEATURE_WLAN_ESE
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    return csrRoamIsEseIniFeatureEnabled(pMac);
#else
    return eANI_BOOLEAN_FALSE;
#endif
}

/*--------------------------------------------------------------------------
  \brief sme_GetWESMode() - get WES Mode
  This is a synchronous call
  \param hHal - The handle returned by macOpen
  \return v_U8_t - WES Mode Enabled(1)/Disabled(0)
  \sa
  --------------------------------------------------------------------------*/
v_BOOL_t sme_GetWESMode(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    return pMac->roam.configParam.isWESModeEnabled;
}

/*--------------------------------------------------------------------------
  \brief sme_GetRoamScanControl() - get scan control
  This is a synchronous call
  \param hHal - The handle returned by macOpen.
  \return v_BOOL_t - Enabled(1)/Disabled(0)
  \sa
  --------------------------------------------------------------------------*/
v_BOOL_t sme_GetRoamScanControl(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    return pMac->roam.configParam.nRoamScanControl;
}
#endif

/*--------------------------------------------------------------------------
  \brief sme_getIsLfrFeatureEnabled() - get LFR feature enabled or not
  This is a synchronuous call
  \param hHal - The handle returned by macOpen.
  \return TRUE (1) - if the feature is enabled
          FALSE (0) - if feature is disabled (compile or runtime)
  \sa
  --------------------------------------------------------------------------*/
tANI_BOOLEAN sme_getIsLfrFeatureEnabled(tHalHandle hHal)
{
#ifdef FEATURE_WLAN_LFR
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    return pMac->roam.configParam.isFastRoamIniFeatureEnabled;
#else
    return eANI_BOOLEAN_FALSE;
#endif
}

/*--------------------------------------------------------------------------
  \brief sme_getIsFtFeatureEnabled() - get FT feature enabled or not
  This is a synchronuous call
  \param hHal - The handle returned by macOpen.
  \return TRUE (1) - if the feature is enabled
          FALSE (0) - if feature is disabled (compile or runtime)
  \sa
  --------------------------------------------------------------------------*/
tANI_BOOLEAN sme_getIsFtFeatureEnabled(tHalHandle hHal)
{
#ifdef WLAN_FEATURE_VOWIFI_11R
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    return pMac->roam.configParam.isFastTransitionEnabled;
#else
    return eANI_BOOLEAN_FALSE;
#endif
}

/* ---------------------------------------------------------------------------
    \fn sme_IsFeatureSupportedByFW

    \brief  Check if a feature is enabled by FW

    \param  featEnumValue - Enumeration value from placeHolderInCapBitmap

    \- return 1/0 (TRUE/FALSE)
    -------------------------------------------------------------------------*/
tANI_U8 sme_IsFeatureSupportedByFW(tANI_U8 featEnumValue)
{
   return IS_FEATURE_SUPPORTED_BY_FW(featEnumValue);
}

/* ---------------------------------------------------------------------------
    \fn sme_IsFeatureSupportedByDriver
    \brief  Check if a feature is enabled by Driver

    \param  featEnumValue - Enumeration value from placeHolderInCapBitmap
    \- return 1/0 (TRUE/FALSE)
    -------------------------------------------------------------------------*/

tANI_U8 sme_IsFeatureSupportedByDriver(tANI_U8 featEnumValue)
{
   return IS_FEATURE_SUPPORTED_BY_DRIVER(featEnumValue);
}

#ifdef FEATURE_WLAN_TDLS

/* ---------------------------------------------------------------------------
    \fn sme_SendTdlsMgmtFrame
    \brief  API to send TDLS management frames.

    \param  peerMac - peer's Mac Adress.
    \param  tdlsLinkEstablishParams - TDLS Peer Link Establishment Parameters
    \- return VOS_STATUS_SUCCES
    -------------------------------------------------------------------------*/
VOS_STATUS sme_SendTdlsLinkEstablishParams(tHalHandle hHal,
                                                   tANI_U8 sessionId,
                                                   tSirMacAddr peerMac,
                                                   tCsrTdlsLinkEstablishParams *tdlsLinkEstablishParams)
{
    eHalStatus          status    = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal      pMac      = PMAC_STRUCT(hHal);

    status = sme_AcquireGlobalLock( &pMac->sme );

    if ( HAL_STATUS_SUCCESS( status ) )
    {
        status = csrTdlsSendLinkEstablishParams(hHal, sessionId, peerMac, tdlsLinkEstablishParams) ;
        sme_ReleaseGlobalLock( &pMac->sme );
    }
   return status ;
}

// tdlsoffchan

/* ---------------------------------------------------------------------------
    \fn sme_SendTdlsChanSwitchReq
    \brief  API to send TDLS management frames.

    \param  peerMac - peer's Mac Adress.
    \param  tdlsLinkEstablishParams - TDLS Peer Link Establishment Parameters
    \- return VOS_STATUS_SUCCES
    -------------------------------------------------------------------------*/
VOS_STATUS sme_SendTdlsChanSwitchReq(tHalHandle hHal,
                                     tANI_U8 sessionId,
                                     tSirMacAddr peerMac,
                                     tANI_S32 tdlsOffCh,
                                     tANI_S32 tdlsOffChBwOffset,
                                     tANI_U8 tdlsSwMode)
{
    eHalStatus          status    = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal      pMac      = PMAC_STRUCT(hHal);

    status = sme_AcquireGlobalLock( &pMac->sme );

    if ( HAL_STATUS_SUCCESS( status ) )
    {
        status = csrTdlsSendChanSwitchReq(hHal, sessionId, peerMac,
                                          tdlsOffCh, tdlsOffChBwOffset,
                                          tdlsSwMode);
    }
    sme_ReleaseGlobalLock( &pMac->sme );
    return status ;
}

/* ---------------------------------------------------------------------------
    \fn sme_SendTdlsMgmtFrame
    \brief  API to send TDLS management frames.

    \param  peerMac - peer's Mac Adress.
    \param frame_type - Type of TDLS mgmt frame to be sent.
    \param dialog - dialog token used in the frame.
    \param status - status to be incuded in the frame.
    \param peerCapability - peer cpabilities
    \param buf - additional IEs to be included
    \param len - lenght of additional Ies
    \param responder - Tdls request type
    \- return VOS_STATUS_SUCCES
    -------------------------------------------------------------------------*/
VOS_STATUS sme_SendTdlsMgmtFrame(tHalHandle hHal, tANI_U8 sessionId, tSirMacAddr peerMac,
      tANI_U8 frame_type, tANI_U8 dialog, tANI_U16 statusCode, tANI_U32 peerCapability, tANI_U8 *buf, tANI_U8 len, tANI_U8 responder)
{
    eHalStatus          status    = eHAL_STATUS_SUCCESS;
    tCsrTdlsSendMgmt sendTdlsReq = {{0}} ;
    tpAniSirGlobal      pMac      = PMAC_STRUCT(hHal);

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        vos_mem_copy(sendTdlsReq.peerMac, peerMac, sizeof(tSirMacAddr)) ;
        sendTdlsReq.frameType = frame_type;
        sendTdlsReq.buf = buf;
        sendTdlsReq.len = len;
        sendTdlsReq.dialog = dialog;
        sendTdlsReq.statusCode = statusCode;
        sendTdlsReq.responder = responder;
        sendTdlsReq.peerCapability = peerCapability;

        status = csrTdlsSendMgmtReq(hHal, sessionId, &sendTdlsReq) ;

        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return status ;

}
/* ---------------------------------------------------------------------------
    \fn sme_ChangeTdlsPeerSta
    \brief  API to Update TDLS peer sta parameters.

    \param  peerMac - peer's Mac Adress.
    \param  staParams - Peer Station Parameters
    \- return VOS_STATUS_SUCCES
    -------------------------------------------------------------------------*/
VOS_STATUS sme_ChangeTdlsPeerSta(tHalHandle hHal, tANI_U8 sessionId, tSirMacAddr peerMac,
                                 tCsrStaParams *pstaParams)
{
    eHalStatus          status    = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal      pMac      = PMAC_STRUCT(hHal);

   if (NULL == pstaParams)
   {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                "%s :pstaParams is NULL",__func__);
        return eHAL_STATUS_FAILURE;
    }
    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        status = csrTdlsChangePeerSta(hHal, sessionId, peerMac,pstaParams);

        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return status ;

}

/* ---------------------------------------------------------------------------
    \fn sme_AddTdlsPeerSta
    \brief  API to Add TDLS peer sta entry.

    \param  peerMac - peer's Mac Adress.
    \- return VOS_STATUS_SUCCES
    -------------------------------------------------------------------------*/
VOS_STATUS sme_AddTdlsPeerSta(tHalHandle hHal, tANI_U8 sessionId, tSirMacAddr peerMac)
{
    eHalStatus          status    = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal      pMac      = PMAC_STRUCT(hHal);

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        status = csrTdlsAddPeerSta(hHal, sessionId, peerMac);

        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return status ;

}
/* ---------------------------------------------------------------------------
    \fn sme_DeleteTdlsPeerSta
    \brief  API to Delete TDLS peer sta entry.

    \param  peerMac - peer's Mac Adress.
    \- return VOS_STATUS_SUCCES
    -------------------------------------------------------------------------*/
VOS_STATUS sme_DeleteTdlsPeerSta(tHalHandle hHal, tANI_U8 sessionId, tSirMacAddr peerMac)
{
    eHalStatus          status    = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal      pMac      = PMAC_STRUCT(hHal);

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        status = csrTdlsDelPeerSta(hHal, sessionId, peerMac) ;

        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return status ;

}
/* ---------------------------------------------------------------------------
    \fn sme_SetTdlsPowerSaveProhibited
    \API to set/reset the isTdlsPowerSaveProhibited.

    \- return void
    -------------------------------------------------------------------------*/
void sme_SetTdlsPowerSaveProhibited(tHalHandle hHal, v_BOOL_t val)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    pMac->isTdlsPowerSaveProhibited = val;
    return;
}
#endif
/* ---------------------------------------------------------------------------
    \fn sme_IsPmcBmps
    \API to Check if PMC state is BMPS.

    \- return v_BOOL_t
    -------------------------------------------------------------------------*/
v_BOOL_t sme_IsPmcBmps(tHalHandle hHal)
{
    return (BMPS == pmcGetPmcState(hHal));
}

#ifdef FEATURE_WLAN_TDLS_INTERNAL
/*
 * SME API to start TDLS discovery Procedure
 */
VOS_STATUS sme_StartTdlsDiscoveryReq(tHalHandle hHal, tANI_U8 sessionId, tSirMacAddr peerMac)
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    tCsrTdlsDisRequest disReq = {{0}} ;
    vos_mem_copy(disReq.peerMac, peerMac, sizeof(tSirMacAddr)) ;
    status = csrTdlsDiscoveryReq(hHal, sessionId, &disReq) ;

    return status ;

}

/*
 * Process TDLS discovery results
 */
v_U8_t sme_GetTdlsDiscoveryResult(tHalHandle hHal,
                                 tSmeTdlsDisResult *disResult, v_U8_t listType)
{
    tCsrTdlsPeerLinkinfo *peerLinkInfo = NULL ;
    tSirTdlsPeerInfo *peerInfo = NULL ;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tCsrTdlsCtxStruct *disInfo = &pMac->tdlsCtx ;
    tDblLinkList *peerList =  &disInfo->tdlsPotentialPeerList ;
    tListElem *pEntry = NULL ;
    v_U8_t peerCnt = 0 ;

    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
            ("TDLS peer count = %d"),disInfo->tdlsPeerCount ) ;
    pEntry = csrLLPeekHead( peerList, LL_ACCESS_LOCK );
    while(pEntry)
    {
        peerLinkInfo = GET_BASE_ADDR( pEntry, tCsrTdlsPeerLinkinfo,
                tdlsPeerStaLink) ;
        peerInfo = &peerLinkInfo->tdlsDisPeerInfo ;

        switch(listType)
        {
            case TDLS_SETUP_LIST:
                {
                    if(TDLS_LINK_SETUP_STATE == peerInfo->tdlsPeerState)
                    {
                        vos_mem_copy(disResult[peerCnt].tdlsPeerMac,
                                     peerInfo->peerMac, sizeof(tSirMacAddr));
                        disResult[peerCnt].tdlsPeerRssi = peerInfo->tdlsPeerRssi ;
                        peerCnt++ ;
                    }
                    break ;
                }
            case TDLS_DIS_LIST:
                {
                    vos_mem_copy(disResult[peerCnt].tdlsPeerMac,
                                 peerInfo->peerMac, sizeof(tSirMacAddr));
                    disResult[peerCnt].tdlsPeerRssi = peerInfo->tdlsPeerRssi ;
                    peerCnt++ ;
                    break ;
                }
            default:
                {
                    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                            ("unknown list type ")) ;
                    break ;
                }
        }

        pEntry = csrLLNext( peerList, pEntry, LL_ACCESS_LOCK) ;
    }

    return peerCnt ;

}

/*
 * SME API to start TDLS link setup Procedure.
 */
VOS_STATUS sme_StartTdlsLinkSetupReq(tHalHandle hHal, tANI_U8 sessionId, tSirMacAddr peerMac)
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    tCsrTdlsSetupRequest setupReq = {{0}} ;
    vos_mem_copy(setupReq.peerMac, peerMac, sizeof(tSirMacAddr)) ;
    status = csrTdlsSetupReq(hHal, sessionId, &setupReq) ;
    return status ;

}

/*
 * SME API to start TDLS link Teardown Procedure.
 */
VOS_STATUS sme_StartTdlsLinkTeardownReq(tHalHandle hHal, tANI_U8 sessionId, tSirMacAddr peerMac)
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    tCsrTdlsTeardownRequest teardownReq = {{0}} ;
    vos_mem_copy(teardownReq.peerMac, peerMac, sizeof(tSirMacAddr)) ;
    status = csrTdlsTeardownReq(hHal, sessionId, &teardownReq) ;
    return status ;

}

#endif /* FEATURE_WLAN_TDLS */

eHalStatus sme_UpdateDfsSetting(tHalHandle hHal, tANI_U8 fUpdateEnableDFSChnlScan)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    smsLog(pMac, LOG2, FL("enter"));
    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        pMac->scan.fEnableDFSChnlScan = fUpdateEnableDFSChnlScan;
        sme_ReleaseGlobalLock( &pMac->sme );
    }
    smsLog(pMac, LOG2, FL("exit status %d"), status);

    return (status);
}

/*
 * SME API to enable/disable WLAN driver initiated SSR
 */
void sme_UpdateEnableSSR(tHalHandle hHal, tANI_BOOLEAN enableSSR)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    eHalStatus     status = eHAL_STATUS_SUCCESS;

    status = sme_AcquireGlobalLock(&pMac->sme);
    if (HAL_STATUS_SUCCESS(status))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                     "SSR level is changed %d", enableSSR);
       /* not serializing this messsage, as this is only going
        * to set a variable in WDA/WDI
        */
        WDA_SetEnableSSR(enableSSR);
        sme_ReleaseGlobalLock(&pMac->sme);
    }
    return;
}

/*
 * SME API to determine the channel bonding mode
 */
VOS_STATUS sme_SelectCBMode(tHalHandle hHal, eCsrPhyMode eCsrPhyMode, tANI_U8 channel)
{
   tSmeConfigParams  smeConfig;
   tpAniSirGlobal    pMac = PMAC_STRUCT(hHal);

   if (
#ifdef WLAN_FEATURE_11AC
         eCSR_DOT11_MODE_11ac != eCsrPhyMode &&
         eCSR_DOT11_MODE_11ac_ONLY != eCsrPhyMode &&
#endif
         eCSR_DOT11_MODE_11n != eCsrPhyMode &&
         eCSR_DOT11_MODE_11n_ONLY != eCsrPhyMode &&

         eCSR_DOT11_MODE_11a != eCsrPhyMode &&
         eCSR_DOT11_MODE_11a_ONLY != eCsrPhyMode &&

         eCSR_DOT11_MODE_abg != eCsrPhyMode
      )
   {
      return VOS_STATUS_SUCCESS;
   }

   /* If channel bonding mode is not required */
   if ( !pMac->roam.configParam.channelBondingMode5GHz ) {
      return VOS_STATUS_SUCCESS;
   }

   vos_mem_zero(&smeConfig, sizeof (tSmeConfigParams));
   sme_GetConfigParam(pMac, &smeConfig);

#ifdef WLAN_FEATURE_11AC
   if ( eCSR_DOT11_MODE_11ac == eCsrPhyMode ||
         eCSR_DOT11_MODE_11ac_ONLY == eCsrPhyMode )
   {
      if ( channel== 36 || channel == 52 || channel == 100 ||
            channel == 116 || channel == 149 )
      {
         smeConfig.csrConfig.channelBondingMode5GHz =
            PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW - 1;
      }
      else if ( channel == 40 || channel == 56 || channel == 104 ||
            channel == 120 || channel == 153 )
      {
         smeConfig.csrConfig.channelBondingMode5GHz =
            PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW - 1;
      }
      else if ( channel == 44 || channel == 60 || channel == 108 ||
            channel == 124 || channel == 157 )
      {
         smeConfig.csrConfig.channelBondingMode5GHz =
            PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH -1;
      }
      else if ( channel == 48 || channel == 64 || channel == 112 ||
            channel == 128 || channel == 144 || channel == 161 )
      {
         smeConfig.csrConfig.channelBondingMode5GHz =
            PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH - 1;
      }
      else if ( channel == 165 )
      {
         smeConfig.csrConfig.channelBondingMode5GHz = 0;
      }
   }
#endif

   if ( eCSR_DOT11_MODE_11n == eCsrPhyMode ||
         eCSR_DOT11_MODE_11n_ONLY == eCsrPhyMode )
   {
      if ( channel== 40 || channel == 48 || channel == 56 ||
            channel == 64 || channel == 104 || channel == 112 ||
            channel == 120 || channel == 128 || channel == 136 ||
            channel == 144 || channel == 153 || channel == 161 )
      {
         smeConfig.csrConfig.channelBondingMode5GHz = 1;
      }
      else if ( channel== 36 || channel == 44 || channel == 52 ||
            channel == 60 || channel == 100 || channel == 108 ||
            channel == 116 || channel == 124 || channel == 132 ||
            channel == 140 || channel == 149 || channel == 157 )
      {
         smeConfig.csrConfig.channelBondingMode5GHz = 2;
      }
      else if ( channel == 165 )
      {
         smeConfig.csrConfig.channelBondingMode5GHz = 0;
      }
   }

   /*
      for 802.11a phy mode, channel bonding should be zero.
      From default config, it is set as PHY_DOUBLE_CHANNEL_HIGH_PRIMARY = 3
      through csrChangeDefaultConfigParam function. We will override this
      value here.
   */
   if (  eCSR_DOT11_MODE_11a == eCsrPhyMode ||
         eCSR_DOT11_MODE_11a_ONLY == eCsrPhyMode ||
         eCSR_DOT11_MODE_abg == eCsrPhyMode)
   {
      smeConfig.csrConfig.channelBondingMode5GHz = 0;
   }

   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_WARN,
         "cbmode selected=%d", smeConfig.csrConfig.channelBondingMode5GHz);

   sme_UpdateConfig (pMac, &smeConfig);
   return VOS_STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------

  \brief sme_SetCurrDeviceMode() - Sets the current operating device mode.
  \param hHal - The handle returned by macOpen.
  \param currDeviceMode - Current operating device mode.
  --------------------------------------------------------------------------*/

void sme_SetCurrDeviceMode (tHalHandle hHal, tVOS_CON_MODE currDeviceMode)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    pMac->sme.currDeviceMode = currDeviceMode;
    return;
}

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
/*--------------------------------------------------------------------------
  \brief sme_HandoffRequest() - a wrapper function to Request a handoff
  from CSR.
  This is a synchronous call
  \param hHal - The handle returned by macOpen
  \param pHandoffInfo - info provided by HDD with the handoff request (namely:
  BSSID, channel etc.)
  \return eHAL_STATUS_SUCCESS - SME passed the request to CSR successfully.
          Other status means SME is failed to send the request.
  \sa
  --------------------------------------------------------------------------*/

eHalStatus sme_HandoffRequest(tHalHandle hHal,
                              tCsrHandoffRequest *pHandoffInfo)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus          status    = eHAL_STATUS_SUCCESS;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                     "%s: invoked", __func__);
        status = csrHandoffRequest(pMac, pHandoffInfo);
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return status ;
}
#endif

/*
 * SME API to check if there is any infra station or
 * P2P client is connected
 */
VOS_STATUS sme_isSta_p2p_clientConnected(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    if(csrIsInfraConnected(pMac))
    {
        return VOS_STATUS_SUCCESS;
    }
    return VOS_STATUS_E_FAILURE;
}

/*
 * SME API to check if any sessoion connected.
 */
VOS_STATUS sme_is_any_session_connected(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    if(csrIsAnySessionConnected(pMac))
    {

        return VOS_STATUS_SUCCESS;
    }
    return VOS_STATUS_E_FAILURE;
}


#ifdef FEATURE_WLAN_LPHB
/* ---------------------------------------------------------------------------
    \fn sme_LPHBConfigReq
    \API to make configuration LPHB within FW.
    \param hHal - The handle returned by macOpen
    \param lphdReq - LPHB request argument by client
    \param pCallbackfn - LPHB timeout notification callback function pointer
    \- return Configuration message posting status, SUCCESS or Fail
    -------------------------------------------------------------------------*/
eHalStatus sme_LPHBConfigReq
(
   tHalHandle hHal,
   tSirLPHBReq *lphdReq,
   void (*pCallbackfn)(void *pAdapter, void *indParam)
)
{
    eHalStatus          status    = eHAL_STATUS_SUCCESS;
    VOS_STATUS          vosStatus = VOS_STATUS_SUCCESS;
    tpAniSirGlobal      pMac      = PMAC_STRUCT(hHal);
    vos_msg_t           vosMessage;

    status = sme_AcquireGlobalLock(&pMac->sme);
    if (eHAL_STATUS_SUCCESS == status)
    {
        if ((LPHB_SET_EN_PARAMS_INDID == lphdReq->cmd) &&
            (NULL == pCallbackfn) &&
            (NULL == pMac->sme.pLphbIndCb))
        {
           VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                     "%s: Indication Call back did not registered", __func__);
           sme_ReleaseGlobalLock(&pMac->sme);
           return eHAL_STATUS_FAILURE;
        }
        else if (NULL != pCallbackfn)
        {
           pMac->sme.pLphbIndCb = pCallbackfn;
        }

        /* serialize the req through MC thread */
        vosMessage.bodyptr = lphdReq;
        vosMessage.type    = WDA_LPHB_CONF_REQ;
        vosStatus = vos_mq_post_message(VOS_MQ_ID_WDA, &vosMessage);
        if (!VOS_IS_STATUS_SUCCESS(vosStatus))
        {
           VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                     "%s: Post Config LPHB MSG fail", __func__);
           status = eHAL_STATUS_FAILURE;
        }
        sme_ReleaseGlobalLock(&pMac->sme);
    }

    return(status);
}
#endif /* FEATURE_WLAN_LPHB */
/*--------------------------------------------------------------------------
  \brief sme_enable_disable_split_scan() - a wrapper function to set the split
                                          scan parameter.
  This is a synchronous call
  \param hHal - The handle returned by macOpen
  \return NONE.
  \sa
  --------------------------------------------------------------------------*/
void sme_enable_disable_split_scan (tHalHandle hHal, tANI_U8 nNumStaChan,
                                          tANI_U8 nNumP2PChan)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    pMac->roam.configParam.nNumStaChanCombinedConc = nNumStaChan;
    pMac->roam.configParam.nNumP2PChanCombinedConc = nNumP2PChan;

    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                 "%s: SCAN nNumStaChanCombinedConc : %d,"
                           "nNumP2PChanCombinedConc : %d ",
                 __func__, nNumStaChan, nNumP2PChan);

    return;

}

/* ---------------------------------------------------------------------------
    \fn sme_AddPeriodicTxPtrn
    \brief  API to Periodic TX Pattern Offload feature
    \param  hHal - The handle returned by macOpen
    \param  addPeriodicTxPtrnParams - Pointer to the add pattern structure
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_AddPeriodicTxPtrn(tHalHandle hHal, tSirAddPeriodicTxPtrn
                                 *addPeriodicTxPtrnParams)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    eHalStatus status;
    vos_msg_t msg;

    if (eHAL_STATUS_SUCCESS == (status = sme_AcquireGlobalLock(&pMac->sme)))
    {
        msg.type     = WDA_ADD_PERIODIC_TX_PTRN_IND;
        msg.bodyptr  = addPeriodicTxPtrnParams;

        if (!VOS_IS_STATUS_SUCCESS(vos_mq_post_message(VOS_MODULE_ID_WDA, &msg)))
        {
            VOS_TRACE( VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,"%s: Not able "
                       "to post WDA_ADD_PERIODIC_TX_PTRN_IND to WDA!",
                       __func__);

            sme_ReleaseGlobalLock(&pMac->sme);
            return eHAL_STATUS_FAILURE;
        }

        sme_ReleaseGlobalLock(&pMac->sme);
        return eHAL_STATUS_SUCCESS;
    }

    return status;
}

/* ---------------------------------------------------------------------------
    \fn sme_DelPeriodicTxPtrn
    \brief  API to Periodic TX Pattern Offload feature
    \param  hHal - The handle returned by macOpen
    \param  delPeriodicTxPtrnParams - Pointer to the delete pattern structure
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_DelPeriodicTxPtrn(tHalHandle hHal, tSirDelPeriodicTxPtrn
                                 *delPeriodicTxPtrnParams)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    eHalStatus status;
    vos_msg_t msg;

    if (eHAL_STATUS_SUCCESS == (status = sme_AcquireGlobalLock(&pMac->sme)))
    {
        msg.type     = WDA_DEL_PERIODIC_TX_PTRN_IND;
        msg.bodyptr  = delPeriodicTxPtrnParams;

        if (!VOS_IS_STATUS_SUCCESS(vos_mq_post_message(VOS_MODULE_ID_WDA, &msg)))
        {
            VOS_TRACE( VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,"%s: Not able "
                       "to post WDA_DEL_PERIODIC_TX_PTRN_IND to WDA!",
                       __func__);

            sme_ReleaseGlobalLock(&pMac->sme);
            return eHAL_STATUS_FAILURE;
        }

        sme_ReleaseGlobalLock(&pMac->sme);
        return eHAL_STATUS_SUCCESS;
    }

    return status;
}

void smeGetCommandQStatus( tHalHandle hHal )
{
    tSmeCmd *pTempCmd = NULL;
    tListElem *pEntry;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    if (NULL == pMac)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL,
            "%s: pMac is null", __func__);
        return;
    }

    pEntry = csrLLPeekHead( &pMac->sme.smeCmdActiveList, LL_ACCESS_LOCK );
    if( pEntry )
    {
        pTempCmd = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
    }
    smsLog( pMac, LOGE, "Currently smeCmdActiveList has command (0x%X)",
            (pTempCmd) ? pTempCmd->command : eSmeNoCommand );
    if(pTempCmd)
    {
        if( eSmeCsrCommandMask & pTempCmd->command )
        {
            //CSR command is stuck. See what the reason code is for that command
            dumpCsrCommandInfo(pMac, pTempCmd);
        }
    } //if(pTempCmd)

    smsLog( pMac, LOGE, "Currently smeCmdPendingList has %d commands",
            csrLLCount(&pMac->sme.smeCmdPendingList));

    smsLog( pMac, LOGE, "Currently roamCmdPendingList has %d commands",
            csrLLCount(&pMac->roam.roamCmdPendingList));

    return;
}

#ifdef FEATURE_WLAN_BATCH_SCAN
/* ---------------------------------------------------------------------------
    \fn sme_SetBatchScanReq
    \brief  API to set batch scan request in FW
    \param  hHal - The handle returned by macOpen.
    \param  pRequest -  Pointer to the batch request.
    \param  sessionId - session ID
    \param  callbackRoutine - HDD callback which needs to be invoked after
            getting set batch scan response from FW
    \param  callbackContext - pAdapter context
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_SetBatchScanReq
(
    tHalHandle hHal, tSirSetBatchScanReq *pRequest, tANI_U8 sessionId,
    void (*callbackRoutine) (void *callbackCtx, tSirSetBatchScanRsp *pRsp),
    void *callbackContext
)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status;

    if (!pMac)
    {
        return eHAL_STATUS_FAILURE;
    }

    if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme )))
    {
       status = pmcSetBatchScanReq(hHal, pRequest, sessionId, callbackRoutine,
                  callbackContext);
       sme_ReleaseGlobalLock( &pMac->sme );
    }

    return status;
}

/* ---------------------------------------------------------------------------
    \fn sme_SendRateUpdateInd
    \brief  API to Update rate
    \param  hHal - The handle returned by macOpen
    \param  rateUpdateParams - Pointer to rate update params
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_SendRateUpdateInd(tHalHandle hHal, tSirRateUpdateInd *rateUpdateParams)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    eHalStatus status;
    vos_msg_t msg;

    if (eHAL_STATUS_SUCCESS == (status = sme_AcquireGlobalLock(&pMac->sme)))
    {
        msg.type     = WDA_RATE_UPDATE_IND;
        msg.bodyptr  = rateUpdateParams;

        if (!VOS_IS_STATUS_SUCCESS(vos_mq_post_message(VOS_MODULE_ID_WDA, &msg)))
        {
            VOS_TRACE( VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,"%s: Not able "
                       "to post WDA_SET_RMC_RATE_IND to WDA!",
                       __func__);

            sme_ReleaseGlobalLock(&pMac->sme);
            return eHAL_STATUS_FAILURE;
        }

        sme_ReleaseGlobalLock(&pMac->sme);
        return eHAL_STATUS_SUCCESS;
    }

    return status;
}

/* ---------------------------------------------------------------------------
    \fn sme_TriggerBatchScanResultInd
    \brief  API to trigger batch scan result indications from FW
    \param  hHal - The handle returned by macOpen.
    \param  pRequest -  Pointer to get batch request.
    \param  sessionId - session ID
    \param  callbackRoutine - HDD callback which needs to be invoked after
            getting batch scan result indication from FW
    \param  callbackContext - pAdapter context
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_TriggerBatchScanResultInd
(
    tHalHandle hHal, tSirTriggerBatchScanResultInd *pRequest, tANI_U8 sessionId,
    void (*callbackRoutine) (void *callbackCtx, void *pRsp),
    void *callbackContext
)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status;

    if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme )))
    {
       status = pmcTriggerBatchScanResultInd(hHal, pRequest, sessionId,
                   callbackRoutine, callbackContext);
       sme_ReleaseGlobalLock( &pMac->sme );
    }

    return status;
}


/* ---------------------------------------------------------------------------
    \fn sme_StopBatchScanInd
    \brief  API to stop batch scan request in FW
    \param  hHal - The handle returned by macOpen.
    \param  pRequest -  Pointer to the batch request.
    \param  sessionId - session ID
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_StopBatchScanInd
(
    tHalHandle hHal, tSirStopBatchScanInd *pRequest, tANI_U8 sessionId
)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status;

    if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme )))
    {
       status = pmcStopBatchScanInd(hHal, pRequest, sessionId);
       sme_ReleaseGlobalLock( &pMac->sme );
    }

    return status;
}

#endif


#ifdef FEATURE_WLAN_CH_AVOID
/* ---------------------------------------------------------------------------
    \fn sme_AddChAvoidCallback
    \brief  Used to plug in callback function
            Which notify channel may not be used with SAP or P2PGO mode.
            Notification come from FW.
    \param  hHal
    \param  pCallbackfn : callback function pointer should be plugged in
    \- return eHalStatus
    -------------------------------------------------------------------------*/
eHalStatus sme_AddChAvoidCallback
(
   tHalHandle hHal,
   void (*pCallbackfn)(void *pAdapter, void *indParam)
)
{
    eHalStatus          status    = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal      pMac      = PMAC_STRUCT(hHal);

    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
              "%s: Plug in CH AVOID CB", __func__);

    status = sme_AcquireGlobalLock(&pMac->sme);
    if (eHAL_STATUS_SUCCESS == status)
    {
        if (NULL != pCallbackfn)
        {
           pMac->sme.pChAvoidNotificationCb = pCallbackfn;
        }
        sme_ReleaseGlobalLock(&pMac->sme);
    }

    return(status);
}
#endif /* FEATURE_WLAN_CH_AVOID */

void activeListCmdTimeoutHandle(void *userData)
{
    if (NULL == userData)
        return;
    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
        "%s: Active List command timeout Cmd List Count %d", __func__,
    csrLLCount(&((tpAniSirGlobal) userData)->sme.smeCmdActiveList) );
    smeGetCommandQStatus((tHalHandle) userData);
}

#ifdef WLAN_FEATURE_LINK_LAYER_STATS

/* ---------------------------------------------------------------------------
    \fn sme_LLStatsSetReq
    \brief  API to set link layer stats request to FW
    \param  hHal - The handle returned by macOpen.

    \Param  pStatsReq - a pointer to a caller allocated object of
     typedef struct tSirLLStatsSetReq, signifying the parameters to link layer
     stats set.

    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_LLStatsSetReq(tHalHandle hHal,
                                tSirLLStatsSetReq *pLinkLayerStatsSetReq)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    vos_msg_t msg;
    eHalStatus status = eHAL_STATUS_FAILURE;
    tSirLLStatsSetReq *plinkLayerSetReq;

    plinkLayerSetReq = vos_mem_malloc(sizeof(*plinkLayerSetReq));
    if ( !plinkLayerSetReq)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                "%s: Not able to allocate memory for "
                "WDA_LINK_LAYER_STATS_SET_REQ",
                __func__);
        return eHAL_STATUS_FAILURE;
    }

    *plinkLayerSetReq = *pLinkLayerStatsSetReq;


    if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme )))
    {
        msg.type = WDA_LINK_LAYER_STATS_SET_REQ;
        msg.reserved = 0;
        msg.bodyptr = plinkLayerSetReq;

        if(VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MODULE_ID_WDA, &msg))
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: "
                    "Not able to post SIR_HAL_LL_STATS_SET message to HAL", __func__);
            status = eHAL_STATUS_FAILURE;
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }
    else
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: "
                "sme_AcquireGlobalLock error", __func__);
    }
    return status;
}

/* ---------------------------------------------------------------------------
    \fn sme_LLStatsGetReq
    \brief  API to get link layer stats request to FW
    \param  hHal - The handle returned by macOpen.

    \Param  pStatsReq - a pointer to a caller allocated object of
     typedef struct tSirLLStatsGetReq, signifying the parameters to link layer
     stats get.

    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_LLStatsGetReq(tHalHandle hHal,
                                tSirLLStatsGetReq *pLinkLayerStatsGetReq)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    vos_msg_t msg;
    eHalStatus status = eHAL_STATUS_FAILURE;
    tSirLLStatsGetReq *pGetStatsReq;

    pGetStatsReq = vos_mem_malloc(sizeof(*pGetStatsReq));
    if ( !pGetStatsReq)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                "%s: Not able to allocate memory for "
                "WDA_LINK_LAYER_STATS_GET_REQ",
                __func__);
        return eHAL_STATUS_FAILURE;
    }
    *pGetStatsReq = *pLinkLayerStatsGetReq;

    if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme )))
    {
        msg.type = WDA_LINK_LAYER_STATS_GET_REQ;
        msg.reserved = 0;
        msg.bodyptr = pGetStatsReq;
        if(VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MODULE_ID_WDA, &msg))
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: "
                    "Not able to post SIR_HAL_LL_STATS_GET message to HAL", __func__);
            status = eHAL_STATUS_FAILURE;
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }
    else
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: "
                "sme_AcquireGlobalLock error", __func__);
    }
    return status;
}

/* ---------------------------------------------------------------------------
    \fn sme_LLStatsClearReq
    \brief  API to clear link layer stats request to FW
    \param  hHal - The handle returned by macOpen.

    \Param  pStatsReq - a pointer to a caller allocated object of
     typedef struct tSirLLStatsClearReq, signifying the parameters to link layer
     stats clear.

    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_LLStatsClearReq(tHalHandle hHal,
                                tSirLLStatsClearReq *pLinkLayerStatsClear)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    vos_msg_t msg;
    eHalStatus status = eHAL_STATUS_FAILURE;
    tSirLLStatsClearReq *pClearStatsReq;



    pClearStatsReq = vos_mem_malloc(sizeof(*pClearStatsReq));
    if ( !pClearStatsReq)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                "%s: Not able to allocate memory for "
                "WDA_LINK_LAYER_STATS_CLEAR_REQ",
                __func__);
        return eHAL_STATUS_FAILURE;
    }

    *pClearStatsReq = *pLinkLayerStatsClear;

    if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme )))
    {
        msg.type = WDA_LINK_LAYER_STATS_CLEAR_REQ;
        msg.reserved = 0;
        msg.bodyptr = pClearStatsReq;

        if(VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MODULE_ID_WDA, &msg))
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: "
                    "Not able to post SIR_HAL_LL_STATS_CLEAR message to HAL", __func__);
            status = eHAL_STATUS_FAILURE;
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }
    else
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: "
                "sme_AcquireGlobalLock error", __func__);
    }

    return status;
}

/* ---------------------------------------------------------------------------
    \fn sme_SetLinkLayerStatsIndCB
    \brief  API to trigger Link Layer Statistic indications from FW
    \param  hHal - The handle returned by macOpen.
    \param  sessionId - session ID
    \param  callbackRoutine - HDD callback which needs to be invoked after
            getting Link Layer Statistics from FW
    \param  callbackContext - pAdapter context
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_SetLinkLayerStatsIndCB
(
    tHalHandle hHal,
    void (*callbackRoutine) (void *callbackCtx, int indType, void *pRsp,
     tANI_U8  *macAddr)
)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status;

    if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme )))
    {
        if (NULL != callbackRoutine)
        {
           pMac->sme.pLinkLayerStatsIndCallback = callbackRoutine;
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }

    return status;
}
#endif /* WLAN_FEATURE_LINK_LAYER_STATS */

eHalStatus sme_UpdateConnectDebug(tHalHandle hHal, tANI_U32 set_value)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    pMac->fEnableDebugLog = set_value;
    return (status);
}

VOS_STATUS sme_UpdateDSCPtoUPMapping( tHalHandle hHal,
                                      sme_QosWmmUpType  *dscpmapping,
                                      v_U8_t sessionId )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status = eHAL_STATUS_SUCCESS;
    v_U8_t i, j, peSessionId;
    tCsrRoamSession *pCsrSession = NULL;
    tpPESession pSession = NULL;

    status = sme_AcquireGlobalLock( &pMac->sme );
    if ( HAL_STATUS_SUCCESS( status ) )
    {
        if (!CSR_IS_SESSION_VALID( pMac, sessionId ))
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                     "%s: Invalid session Id %u", __func__, sessionId);
            sme_ReleaseGlobalLock( &pMac->sme);
            return eHAL_STATUS_FAILURE;
        }
        pCsrSession = CSR_GET_SESSION( pMac, sessionId );

        if (pCsrSession == NULL)
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                     "%s: CSR Session lookup fails %u", __func__, sessionId);
            sme_ReleaseGlobalLock( &pMac->sme);
            return eHAL_STATUS_FAILURE;
        }

        pSession = peFindSessionByBssid( pMac,
            pCsrSession->connectedProfile.bssid, &peSessionId );

        if (pSession == NULL)
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                     "%s: Session lookup fails for BSSID", __func__);
            sme_ReleaseGlobalLock( &pMac->sme);
            return eHAL_STATUS_FAILURE;
        }

        if ( !pSession->QosMapSet.present )
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                     "%s: QOS Mapping IE not present", __func__);
            sme_ReleaseGlobalLock( &pMac->sme);
            return eHAL_STATUS_FAILURE;
        }
        else
        {
            for (i = 0; i < SME_QOS_WMM_UP_MAX; i++)
            {
                for (j = pSession->QosMapSet.dscp_range[i][0];
                               j <= pSession->QosMapSet.dscp_range[i][1]; j++)
                {
                   if ((pSession->QosMapSet.dscp_range[i][0] == 255) &&
                                (pSession->QosMapSet.dscp_range[i][1] == 255))
                   {
                       dscpmapping[j]= 0;
                       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                       "%s: User Priority %d is not used in mapping",
                                                             __func__, i);
                       break;
                   }
                   else
                   {
                       dscpmapping[j]= i;
                   }
                }
            }
            for (i = 0; i< pSession->QosMapSet.num_dscp_exceptions; i++)
            {
                if (pSession->QosMapSet.dscp_exceptions[i][0] != 255)
                {
                    dscpmapping[pSession->QosMapSet.dscp_exceptions[i][0] ] =
                                         pSession->QosMapSet.dscp_exceptions[i][1];
                }
            }
        }
    }
    sme_ReleaseGlobalLock( &pMac->sme);
    return status;
}

tANI_BOOLEAN  sme_Is11dCountrycode(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    if (VOS_TRUE == vos_mem_compare(pMac->scan.countryCodeCurrent,
                  pMac->scan.countryCode11d, 2))
    {
        return eANI_BOOLEAN_TRUE;
    }
    else
    {
        return eANI_BOOLEAN_FALSE;
    }
}

tANI_BOOLEAN  sme_SpoofMacAddrReq(tHalHandle hHal, v_MACADDR_t *macaddr)
{
    eHalStatus status   = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tANI_U16 len;

    if ( eHAL_STATUS_SUCCESS == ( status = sme_AcquireGlobalLock( &pMac->sme ) ) )
    {
        tpSirSpoofMacAddrReq pMsg;

        /* Create the message and send to lim */
        len = sizeof(tSirSpoofMacAddrReq);
        pMsg = vos_mem_malloc(len);
        if ( NULL == pMsg )
           status = eHAL_STATUS_FAILURE;
        else
        {
            vos_mem_set(pMsg, sizeof(tSirSpoofMacAddrReq), 0);
            pMsg->messageType     = eWNI_SME_MAC_SPOOF_ADDR_IND;
            pMsg->length          = len;
            /* Data starts from here */
            vos_mem_copy(pMsg->macAddr, macaddr->bytes, VOS_MAC_ADDRESS_LEN);

            status = palSendMBMessage(pMac->hHdd, pMsg);
        }
        sme_ReleaseGlobalLock( &pMac->sme );
    }
    return status;
}

#ifdef WLAN_FEATURE_EXTSCAN
/* ---------------------------------------------------------------------------
    \fn sme_GetValidChannelsByBand
    \brief  SME API to fetch all valid channel filtered by band
    \param  hHal
    \param  wifiBand: RF band information
    \param  aValidChannels: Array to store channel info
    \param  len: number of channels
    \- return eHalStatus
    -------------------------------------------------------------------------*/
eHalStatus sme_GetValidChannelsByBand (tHalHandle hHal, tANI_U8 wifiBand,
                               tANI_U32 *aValidChannels, tANI_U8 *pNumChannels)
{
    eHalStatus status                                   = eHAL_STATUS_SUCCESS;
    tANI_U8 chanList[WNI_CFG_VALID_CHANNEL_LIST_LEN]    = {0};
    tpAniSirGlobal pMac                                 = PMAC_STRUCT(hHal);
    tANI_U8 numChannels                                 = 0;
    tANI_U8 i                                           = 0;
    tANI_U32 totValidChannels                 = WNI_CFG_VALID_CHANNEL_LIST_LEN;

    if (!aValidChannels || !pNumChannels) {
        smsLog(pMac, VOS_TRACE_LEVEL_ERROR,
               FL("Output channel list/NumChannels is NULL"));
        return eHAL_STATUS_INVALID_PARAMETER;
    }

    if ((wifiBand < WIFI_BAND_UNSPECIFIED) || (wifiBand >= WIFI_BAND_MAX)) {
        smsLog(pMac, VOS_TRACE_LEVEL_ERROR,
                     FL("Invalid wifiBand (%d)"), wifiBand);
        return eHAL_STATUS_INVALID_PARAMETER;
    }

    status = sme_GetCfgValidChannels(hHal, &chanList[0],
                                     &totValidChannels);
    if (!HAL_STATUS_SUCCESS(status)) {
        smsLog(pMac, VOS_TRACE_LEVEL_ERROR,
               FL("Failed to get valid channel list (err=%d)"), status);
        return status;
    }

    switch (wifiBand) {
    case WIFI_BAND_UNSPECIFIED:
            smsLog(pMac, VOS_TRACE_LEVEL_INFO, FL("Unspecified wifiBand, "
                         "return all (%d) valid channels"), totValidChannels);
            numChannels = totValidChannels;
            for (i = 0; i < numChannels; i++)
                aValidChannels[i] = vos_chan_to_freq(chanList[i]);
            break;

    case WIFI_BAND_BG:
            smsLog(pMac, VOS_TRACE_LEVEL_INFO, FL("WIFI_BAND_BG (2.4 GHz)"));
            for (i = 0; i < totValidChannels; i++)
                if (CSR_IS_CHANNEL_24GHZ(chanList[i]))
                    aValidChannels[numChannels++] =
                                                 vos_chan_to_freq(chanList[i]);
            break;

    case WIFI_BAND_A:
            smsLog(pMac, VOS_TRACE_LEVEL_INFO,
                          FL("WIFI_BAND_A (5 GHz without DFS)"));
            for (i = 0; i < totValidChannels; i++)
                if (CSR_IS_CHANNEL_5GHZ(chanList[i]) &&
                             !CSR_IS_CHANNEL_DFS(chanList[i]))
                    aValidChannels[numChannels++] =
                                                 vos_chan_to_freq(chanList[i]);
            break;

    case WIFI_BAND_ABG:
            smsLog(pMac, VOS_TRACE_LEVEL_INFO,
                          FL("WIFI_BAND_ABG (2.4 GHz + 5 GHz; no DFS)"));
            for (i = 0; i < totValidChannels; i++)
                if ((CSR_IS_CHANNEL_24GHZ(chanList[i]) ||
                             CSR_IS_CHANNEL_5GHZ(chanList[i])) &&
                             !CSR_IS_CHANNEL_DFS(chanList[i]))
                    aValidChannels[numChannels++] =
                                                 vos_chan_to_freq(chanList[i]);
            break;

    case WIFI_BAND_A_DFS_ONLY:
            smsLog(pMac, VOS_TRACE_LEVEL_INFO,
                          FL("WIFI_BAND_A_DFS (5 GHz DFS only)"));
            for (i = 0; i < totValidChannels; i++)
                if (CSR_IS_CHANNEL_5GHZ(chanList[i]) &&
                             CSR_IS_CHANNEL_DFS(chanList[i]))
                    aValidChannels[numChannels++] =
                                                 vos_chan_to_freq(chanList[i]);
            break;

    case WIFI_BAND_A_WITH_DFS:
            smsLog(pMac, VOS_TRACE_LEVEL_INFO,
                          FL("WIFI_BAND_A_WITH_DFS (5 GHz with DFS)"));
            for (i = 0; i < totValidChannels; i++)
                if (CSR_IS_CHANNEL_5GHZ(chanList[i]))
                    aValidChannels[numChannels++] =
                                                 vos_chan_to_freq(chanList[i]);
            break;

    case WIFI_BAND_ABG_WITH_DFS:
            smsLog(pMac, VOS_TRACE_LEVEL_INFO,
                     FL("WIFI_BAND_ABG_WITH_DFS (2.4 GHz + 5 GHz with DFS)"));
            for (i = 0; i < totValidChannels; i++)
                if (CSR_IS_CHANNEL_24GHZ(chanList[i]) ||
                             CSR_IS_CHANNEL_5GHZ(chanList[i]))
                    aValidChannels[numChannels++] =
                                                 vos_chan_to_freq(chanList[i]);
            break;

    default:
            smsLog(pMac, VOS_TRACE_LEVEL_ERROR,
                    FL("Unknown wifiBand (%d))"), wifiBand);
            return eHAL_STATUS_INVALID_PARAMETER;
            break;
    }
    *pNumChannels = numChannels;

    return status;
}
/* ---------------------------------------------------------------------------
    \fn sme_EXTScanGetCapabilities
    \brief  SME API to fetch Extended Scan capabilities
    \param  hHal
    \param  pReq: Extended Scan capabilities structure
    \- return eHalStatus
    -------------------------------------------------------------------------*/
eHalStatus sme_EXTScanGetCapabilities (tHalHandle hHal,
                                     tSirGetEXTScanCapabilitiesReqParams *pReq)
{
    eHalStatus status    = eHAL_STATUS_SUCCESS;
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    tpAniSirGlobal pMac  = PMAC_STRUCT(hHal);
    vos_msg_t vosMessage;
    tSirGetEXTScanCapabilitiesReqParams *pGetEXTScanCapabilitiesReq;

    pGetEXTScanCapabilitiesReq =
        vos_mem_malloc(sizeof(*pGetEXTScanCapabilitiesReq));
    if ( !pGetEXTScanCapabilitiesReq)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                "%s: Not able to allocate memory for "
                "WDA_EXTSCAN_GET_CAPABILITIES_REQ",
                __func__);
        return eHAL_STATUS_FAILURE;
    }

    *pGetEXTScanCapabilitiesReq = *pReq;

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
               TRACE_CODE_SME_RX_HDD_EXTSCAN_GET_CAPABILITIES, NO_SESSION, 0));
    if (eHAL_STATUS_SUCCESS == (status = sme_AcquireGlobalLock(&pMac->sme))) {
        /* Serialize the req through MC thread */
        vosMessage.bodyptr = pGetEXTScanCapabilitiesReq;
        vosMessage.type    = WDA_EXTSCAN_GET_CAPABILITIES_REQ;
        vosStatus = vos_mq_post_message(VOS_MQ_ID_WDA, &vosMessage);
        if (!VOS_IS_STATUS_SUCCESS(vosStatus))
           status = eHAL_STATUS_FAILURE;

        sme_ReleaseGlobalLock(&pMac->sme);
    }
    return(status);
}

/* ---------------------------------------------------------------------------
    \fn sme_EXTScanStart
    \brief  SME API to issue Extended Scan start
    \param  hHal
    \param  pStartCmd: Extended Scan start structure
    \- return eHalStatus
    -------------------------------------------------------------------------*/
eHalStatus sme_EXTScanStart (tHalHandle hHal,
                           tSirEXTScanStartReqParams *pStartCmd)
{
    eHalStatus status    = eHAL_STATUS_SUCCESS;
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    tpAniSirGlobal pMac  = PMAC_STRUCT(hHal);
    vos_msg_t vosMessage;
    tSirEXTScanStartReqParams *pextScanStartReq;

    pextScanStartReq = vos_mem_malloc(sizeof(*pextScanStartReq));
    if ( !pextScanStartReq)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                "%s: Not able to allocate memory for "
                "WDA_EXTSCAN_START_REQ",
                __func__);
        return eHAL_STATUS_FAILURE;
    }

    *pextScanStartReq = *pStartCmd;


    MTRACE(vos_trace(VOS_MODULE_ID_SME,
                 TRACE_CODE_SME_RX_HDD_EXTSCAN_START, NO_SESSION, 0));
    if (eHAL_STATUS_SUCCESS == (status = sme_AcquireGlobalLock(&pMac->sme))) {
        /* Serialize the req through MC thread */
        vosMessage.bodyptr = pextScanStartReq;
        vosMessage.type    = WDA_EXTSCAN_START_REQ;
        vosStatus = vos_mq_post_message(VOS_MQ_ID_WDA, &vosMessage);
        if (!VOS_IS_STATUS_SUCCESS(vosStatus))
           status = eHAL_STATUS_FAILURE;

        sme_ReleaseGlobalLock(&pMac->sme);
    }
    return(status);
}

/* ---------------------------------------------------------------------------
    \fn sme_EXTScanStop
    \brief  SME API to issue Extended Scan stop
    \param  hHal
    \param  pStopReq: Extended Scan stop structure
    \- return eHalStatus
    -------------------------------------------------------------------------*/
eHalStatus sme_EXTScanStop(tHalHandle hHal, tSirEXTScanStopReqParams *pStopReq)
{
    eHalStatus status    = eHAL_STATUS_SUCCESS;
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    tpAniSirGlobal pMac  = PMAC_STRUCT(hHal);
    vos_msg_t vosMessage;
    tSirEXTScanStopReqParams *pEXTScanStopReq;

    pEXTScanStopReq = vos_mem_malloc(sizeof(*pEXTScanStopReq));
    if ( !pEXTScanStopReq)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                "%s: Not able to allocate memory for "
                "WDA_EXTSCAN_STOP_REQ",
                __func__);
        return eHAL_STATUS_FAILURE;
    }

    *pEXTScanStopReq = *pStopReq;

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
                 TRACE_CODE_SME_RX_HDD_EXTSCAN_STOP, NO_SESSION, 0));
    if (eHAL_STATUS_SUCCESS == (status = sme_AcquireGlobalLock(&pMac->sme)))
    {
        /* Serialize the req through MC thread */
        vosMessage.bodyptr = pEXTScanStopReq;
        vosMessage.type    = WDA_EXTSCAN_STOP_REQ;
        vosStatus = vos_mq_post_message(VOS_MQ_ID_WDA, &vosMessage);
        if (!VOS_IS_STATUS_SUCCESS(vosStatus))
        {
           status = eHAL_STATUS_FAILURE;
        }
        sme_ReleaseGlobalLock(&pMac->sme);
    }
    return(status);
}

/* ---------------------------------------------------------------------------
    \fn sme_SetBssHotlist
    \brief  SME API to set BSSID hotlist
    \param  hHal
    \param  pSetHotListReq: Extended Scan set hotlist structure
    \- return eHalStatus
    -------------------------------------------------------------------------*/
eHalStatus sme_SetBssHotlist (tHalHandle hHal,
                            tSirEXTScanSetBssidHotListReqParams *pSetHotListReq)
{
    eHalStatus status    = eHAL_STATUS_SUCCESS;
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    tpAniSirGlobal pMac  = PMAC_STRUCT(hHal);
    vos_msg_t vosMessage;
    tSirEXTScanSetBssidHotListReqParams *pEXTScanSetBssidHotlistReq;

    pEXTScanSetBssidHotlistReq =
        vos_mem_malloc(sizeof(*pEXTScanSetBssidHotlistReq));
    if ( !pEXTScanSetBssidHotlistReq)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                "%s: Not able to allocate memory for "
                "WDA_EXTSCAN_SET_BSSID_HOTLIST_REQ",
                __func__);
        return eHAL_STATUS_FAILURE;
    }

    *pEXTScanSetBssidHotlistReq = *pSetHotListReq;

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
               TRACE_CODE_SME_RX_HDD_EXTSCAN_SET_BSS_HOTLIST, NO_SESSION, 0));
    if (eHAL_STATUS_SUCCESS == (status = sme_AcquireGlobalLock(&pMac->sme))) {
        /* Serialize the req through MC thread */
        vosMessage.bodyptr = pEXTScanSetBssidHotlistReq;
        vosMessage.type    = WDA_EXTSCAN_SET_BSSID_HOTLIST_REQ;
        vosStatus = vos_mq_post_message(VOS_MQ_ID_WDA, &vosMessage);
        if (!VOS_IS_STATUS_SUCCESS(vosStatus))
           status = eHAL_STATUS_FAILURE;

        sme_ReleaseGlobalLock(&pMac->sme);
    }

    return(status);
}

/* ---------------------------------------------------------------------------
    \fn sme_ResetBssHotlist
    \brief  SME API to reset BSSID hotlist
    \param  hHal
    \param  pSetHotListReq: Extended Scan set hotlist structure
    \- return eHalStatus
    -------------------------------------------------------------------------*/
eHalStatus sme_ResetBssHotlist (tHalHandle hHal,
                               tSirEXTScanResetBssidHotlistReqParams *pResetReq)
{
    eHalStatus status    = eHAL_STATUS_SUCCESS;
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    tpAniSirGlobal pMac  = PMAC_STRUCT(hHal);
    vos_msg_t vosMessage;
    tSirEXTScanResetBssidHotlistReqParams *pEXTScanHotlistResetReq;

    pEXTScanHotlistResetReq = vos_mem_malloc(sizeof(*pEXTScanHotlistResetReq));
    if ( !pEXTScanHotlistResetReq)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                "%s: Not able to allocate memory for "
                "WDA_EXTSCAN_RESET_BSSID_HOTLIST_REQ",
                __func__);
        return eHAL_STATUS_FAILURE;
    }

    *pEXTScanHotlistResetReq = *pResetReq;


    MTRACE(vos_trace(VOS_MODULE_ID_SME,
              TRACE_CODE_SME_RX_HDD_EXTSCAN_RESET_BSS_HOTLIST, NO_SESSION, 0));
    if (eHAL_STATUS_SUCCESS == (status = sme_AcquireGlobalLock(&pMac->sme))) {
        /* Serialize the req through MC thread */
        vosMessage.bodyptr = pEXTScanHotlistResetReq;
        vosMessage.type    = WDA_EXTSCAN_RESET_BSSID_HOTLIST_REQ;
        vosStatus = vos_mq_post_message(VOS_MQ_ID_WDA, &vosMessage);
        if (!VOS_IS_STATUS_SUCCESS(vosStatus))
           status = eHAL_STATUS_FAILURE;

        sme_ReleaseGlobalLock(&pMac->sme);
    }
    return(status);
}

/* ---------------------------------------------------------------------------
    \fn sme_SetSignificantChange
    \brief  SME API to set significant change
    \param  hHal
    \param  pSetSignificantChangeReq: Extended Scan set significant change structure
    \- return eHalStatus
    -------------------------------------------------------------------------*/
eHalStatus sme_SetSignificantChange (tHalHandle hHal,
         tSirEXTScanSetSignificantChangeReqParams *pSetSignificantChangeReq)
{
    eHalStatus status    = eHAL_STATUS_SUCCESS;
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    tpAniSirGlobal pMac  = PMAC_STRUCT(hHal);
    vos_msg_t vosMessage;
    tSirEXTScanSetSignificantChangeReqParams *pEXTScanSetSignificantReq;

    pEXTScanSetSignificantReq = vos_mem_malloc(sizeof(*pEXTScanSetSignificantReq));
    if ( !pEXTScanSetSignificantReq)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                "%s: Not able to allocate memory for "
                "WDA_EXTSCAN_SET_SIGNF_RSSI_CHANGE_REQ",
                __func__);
        return eHAL_STATUS_FAILURE;
    }

    *pEXTScanSetSignificantReq = *pSetSignificantChangeReq;



    MTRACE(vos_trace(VOS_MODULE_ID_SME,
           TRACE_CODE_SME_RX_HDD_EXTSCAN_SET_SIGNF_CHANGE, NO_SESSION, 0));
    if (eHAL_STATUS_SUCCESS == (status = sme_AcquireGlobalLock(&pMac->sme))) {
        /* Serialize the req through MC thread */
        vosMessage.bodyptr = pEXTScanSetSignificantReq;
        vosMessage.type    = WDA_EXTSCAN_SET_SIGNF_RSSI_CHANGE_REQ;
        vosStatus = vos_mq_post_message(VOS_MQ_ID_WDA, &vosMessage);
        if (!VOS_IS_STATUS_SUCCESS(vosStatus))
           status = eHAL_STATUS_FAILURE;

        sme_ReleaseGlobalLock(&pMac->sme);
    }
    return(status);
}

/* ---------------------------------------------------------------------------
    \fn sme_ResetSignificantChange
    \brief  SME API to reset significant change
    \param  hHal
    \param  pResetReq: Extended Scan reset significant change structure
    \- return eHalStatus
    -------------------------------------------------------------------------*/
eHalStatus sme_ResetSignificantChange (tHalHandle hHal,
                          tSirEXTScanResetSignificantChangeReqParams *pResetReq)
{
    eHalStatus status    = eHAL_STATUS_SUCCESS;
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    tpAniSirGlobal pMac  = PMAC_STRUCT(hHal);
    vos_msg_t vosMessage;
    tSirEXTScanResetSignificantChangeReqParams *pEXTScanResetSignificantReq;

    pEXTScanResetSignificantReq =
        vos_mem_malloc(sizeof(*pEXTScanResetSignificantReq));
    if ( !pEXTScanResetSignificantReq)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                "%s: Not able to allocate memory for "
                "WDA_EXTSCAN_RESET_SIGNF_RSSI_CHANGE_REQ",
                __func__);
        return eHAL_STATUS_FAILURE;
    }

    *pEXTScanResetSignificantReq = *pResetReq;

    MTRACE(vos_trace(VOS_MODULE_ID_SME,
           TRACE_CODE_SME_RX_HDD_EXTSCAN_RESET_SIGNF_CHANGE, NO_SESSION, 0));
    if (eHAL_STATUS_SUCCESS == (status = sme_AcquireGlobalLock(&pMac->sme))) {
        /* Serialize the req through MC thread */
        vosMessage.bodyptr = pEXTScanResetSignificantReq;
        vosMessage.type    = WDA_EXTSCAN_RESET_SIGNF_RSSI_CHANGE_REQ;
        vosStatus = vos_mq_post_message(VOS_MQ_ID_WDA, &vosMessage);
        if (!VOS_IS_STATUS_SUCCESS(vosStatus))
           status = eHAL_STATUS_FAILURE;

        sme_ReleaseGlobalLock(&pMac->sme);
    }
    return(status);
}

/* ---------------------------------------------------------------------------
    \fn sme_getCachedResults
    \brief  SME API to get cached results
    \param  hHal
    \param  pCachedResultsReq: Extended Scan get cached results structure
    \- return eHalStatus
    -------------------------------------------------------------------------*/
eHalStatus sme_getCachedResults (tHalHandle hHal,
                        tSirEXTScanGetCachedResultsReqParams *pCachedResultsReq)
{
    eHalStatus status    = eHAL_STATUS_SUCCESS;
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    tpAniSirGlobal pMac  = PMAC_STRUCT(hHal);
    vos_msg_t vosMessage;
    tSirEXTScanGetCachedResultsReqParams *pEXTScanCachedResultsReq;

    pEXTScanCachedResultsReq =
            vos_mem_malloc(sizeof(*pEXTScanCachedResultsReq));
    if ( !pEXTScanCachedResultsReq)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                "%s: Not able to allocate memory for "
                "WDA_EXTSCAN_GET_CACHED_RESULTS_REQ",
                __func__);
        return eHAL_STATUS_FAILURE;
    }

    *pEXTScanCachedResultsReq = *pCachedResultsReq;


    MTRACE(vos_trace(VOS_MODULE_ID_SME,
           TRACE_CODE_SME_RX_HDD_EXTSCAN_GET_CACHED_RESULTS, NO_SESSION, 0));
    if (eHAL_STATUS_SUCCESS == (status = sme_AcquireGlobalLock(&pMac->sme))) {
        /* Serialize the req through MC thread */
        vosMessage.bodyptr = pEXTScanCachedResultsReq;
        vosMessage.type    = WDA_EXTSCAN_GET_CACHED_RESULTS_REQ;
        vosStatus = vos_mq_post_message(VOS_MQ_ID_WDA, &vosMessage);
        if (!VOS_IS_STATUS_SUCCESS(vosStatus))
           status = eHAL_STATUS_FAILURE;

        sme_ReleaseGlobalLock(&pMac->sme);
    }
    return(status);
}

eHalStatus sme_EXTScanRegisterCallback (tHalHandle hHal,
                          void (*pEXTScanIndCb)(void *, const tANI_U16, void *),
                           void *callbackContext)
{
    eHalStatus status    = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal pMac  = PMAC_STRUCT(hHal);

    if (eHAL_STATUS_SUCCESS == (status = sme_AcquireGlobalLock(&pMac->sme))) {
        pMac->sme.pEXTScanIndCb = pEXTScanIndCb;
        pMac->sme.pEXTScanCallbackContext = callbackContext;
        sme_ReleaseGlobalLock(&pMac->sme);
    }
    return(status);
}

void sme_SetMiracastMode (tHalHandle hHal,tANI_U8 mode)
{
    tpAniSirGlobal pMac  = PMAC_STRUCT(hHal);

    pMac->miracast_mode = mode;
}
#endif /* WLAN_FEATURE_EXTSCAN */

void sme_resetCoexEevent(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    if (pMac == NULL)
    {
        printk("btc: %s pMac is NULL \n",__func__);
        return;
    }

    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
              FL("isCoexScoIndSet: %d"), pMac->isCoexScoIndSet);

    if (pMac->isCoexScoIndSet)
    {
        pMac->isCoexScoIndSet = 0;
        ccmCfgSetInt(pMac, WNI_CFG_DEL_ALL_RX_TX_BA_SESSIONS_2_4_G_BTC, 0,
                             NULL, eANI_BOOLEAN_FALSE);
    }

    return;
}

void sme_disable_dfs_channel(tHalHandle hHal, bool disbale_dfs)
{
    tpAniSirGlobal pMac  = PMAC_STRUCT(hHal);
    pMac->scan.fEnableDFSChnlScan = !disbale_dfs;
    csrDisableDfsChannel(pMac);

}

/* ---------------------------------------------------------------------------
    \fn sme_Encryptmsgsend
    \brief  SME API to issue encrypt message request
    \param  hHal
    \param  pCmd: Data to be encrypted
    \- return eHalStatus
    -------------------------------------------------------------------------*/
eHalStatus sme_Encryptmsgsend (tHalHandle hHal,
                               u8 *pCmd,
                               int length,
                               pEncryptMsgRSPCb encMsgCbk)
{
    eHalStatus status    = eHAL_STATUS_SUCCESS;
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    tpAniSirGlobal pMac  = PMAC_STRUCT(hHal);
    vos_msg_t vosMessage;
    u8 *pEncryptMsg;

    pEncryptMsg = vos_mem_malloc(length);
    if ( !pEncryptMsg)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                "%s: Not able to allocate memory for "
                "SIR_HAL_ENCRYPT_MSG_REQ",
                __func__);
        return eHAL_STATUS_FAILURE;
    }

    vos_mem_copy(pEncryptMsg, pCmd, length);

    if (eHAL_STATUS_SUCCESS == (status = sme_AcquireGlobalLock(&pMac->sme))) {

        pMac->sme.pEncMsgInfoParams.pEncMsgCbk = encMsgCbk;
        pMac->sme.pEncMsgInfoParams.pUserData = hHal;
        /* Serialize the req through MC thread */
        vosMessage.bodyptr = pEncryptMsg;
        vosMessage.type    = SIR_HAL_ENCRYPT_MSG_REQ;
        vosStatus = vos_mq_post_message(VOS_MQ_ID_WDA, &vosMessage);
        if (!VOS_IS_STATUS_SUCCESS(vosStatus))
           status = eHAL_STATUS_FAILURE;

        sme_ReleaseGlobalLock(&pMac->sme);
    }
    return(status);
}
