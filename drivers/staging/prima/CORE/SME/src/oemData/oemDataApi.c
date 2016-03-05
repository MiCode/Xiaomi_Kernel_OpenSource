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

#ifdef FEATURE_OEM_DATA_SUPPORT
/** ------------------------------------------------------------------------- *
    ------------------------------------------------------------------------- *


    \file oemDataApi.c

    Implementation for the OEM DATA REQ/RSP interfaces.
   ========================================================================== */
#include "aniGlobal.h"
#include "oemDataApi.h"
#include "palApi.h"
#include "smeInside.h"
#include "smsDebug.h"

#include "csrSupport.h"
#include "wlan_qct_tl.h"

#include "vos_diag_core_log.h"
#include "vos_diag_core_event.h"

/* ---------------------------------------------------------------------------
    \fn oemData_OemDataReqOpen
    \brief This function must be called before any API call to (OEM DATA REQ/RSP module)
    \return eHalStatus
  -------------------------------------------------------------------------------*/

eHalStatus oemData_OemDataReqOpen(tHalHandle hHal)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    do
    {
        //initialize all the variables to null
        vos_mem_set(&(pMac->oemData), sizeof(tOemDataStruct), 0);
        if(!HAL_STATUS_SUCCESS(status))
        {
            smsLog(pMac, LOGE, "oemData_OemDataReqOpen: Cannot allocate memory for the timer function");
            break;
        }
    } while(0);

    return status;
}

/* ---------------------------------------------------------------------------
    \fn oemData_OemDataReqClose
    \brief This function must be called before closing the csr module
    \return eHalStatus
  -------------------------------------------------------------------------------*/

eHalStatus oemData_OemDataReqClose(tHalHandle hHal)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    do
    {
        if(!HAL_STATUS_SUCCESS(status))
        {
            smsLog(pMac, LOGE, "oemData_OemDataReqClose: Failed in oemData_OemDataReqClose at StopTimers");
            break;
        }

        if(pMac->oemData.pOemDataRsp != NULL)
        {
            vos_mem_free(pMac->oemData.pOemDataRsp);
        }

        //initialize all the variables to null
        vos_mem_set(&(pMac->oemData), sizeof(tOemDataStruct), 0);
    } while(0);

    return eHAL_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------
    \fn oemData_ReleaseOemDataReqCommand
    \brief This function removes the oemDataCommand from the active list and
           and frees up any memory occupied by this
    \return eHalStatus
  -------------------------------------------------------------------------------*/
void oemData_ReleaseOemDataReqCommand(tpAniSirGlobal pMac, tSmeCmd *pOemDataCmd, eOemDataReqStatus oemDataReqStatus)
{
    //Do the callback
    pOemDataCmd->u.oemDataCmd.callback(pMac, pOemDataCmd->u.oemDataCmd.pContext, pOemDataCmd->u.oemDataCmd.oemDataReqID, oemDataReqStatus);

    //First take this command out of the active list
    if(csrLLRemoveEntry(&pMac->sme.smeCmdActiveList, &pOemDataCmd->Link, LL_ACCESS_LOCK))
    {
        vos_mem_set(&(pOemDataCmd->u.oemDataCmd), sizeof(tOemDataCmd), 0);

        //Now put this command back on the avilable command list
        smeReleaseCommand(pMac, pOemDataCmd);
    }
    else
    {
        smsLog(pMac, LOGE, "OEM_DATA: **************** oemData_ReleaseOemDataReqCommand cannot release the command");
    }
}

/* ---------------------------------------------------------------------------
    \fn oemData_OemDataReq
    \brief Request an OEM DATA RSP
    \param sessionId - Id of session to be used
    \param pOemDataReqID - pointer to an object to get back the request ID
    \param callback - a callback function that is called upon finish
    \param pContext - a pointer passed in for the callback
    \return eHalStatus
  -------------------------------------------------------------------------------*/
eHalStatus oemData_OemDataReq(tHalHandle hHal,
                                tANI_U8 sessionId,
                                tOemDataReqConfig *oemDataReqConfig,
                                tANI_U32 *pOemDataReqID,
                                oemData_OemDataReqCompleteCallback callback,
                                void *pContext)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tSmeCmd *pOemDataCmd = NULL;

    do
    {
        if( !CSR_IS_SESSION_VALID( pMac, sessionId ) )
        {
           status = eHAL_STATUS_FAILURE;
           break;
        }

        pMac->oemData.oemDataReqConfig.sessionId = sessionId;
        pMac->oemData.callback = callback;
        pMac->oemData.pContext = pContext;
        pMac->oemData.oemDataReqID = *(pOemDataReqID);

        vos_mem_copy((v_VOID_t*)(pMac->oemData.oemDataReqConfig.oemDataReq), (v_VOID_t*)(oemDataReqConfig->oemDataReq), OEM_DATA_REQ_SIZE);

        pMac->oemData.oemDataReqActive = eANI_BOOLEAN_FALSE;

        pOemDataCmd = smeGetCommandBuffer(pMac);

        //fill up the command before posting it.
        if(pOemDataCmd)
        {
            pOemDataCmd->command = eSmeCommandOemDataReq;
            pOemDataCmd->u.oemDataCmd.callback = callback;
            pOemDataCmd->u.oemDataCmd.pContext = pContext;
            pOemDataCmd->u.oemDataCmd.oemDataReqID = pMac->oemData.oemDataReqID;

            //set the oem data request
            pOemDataCmd->u.oemDataCmd.oemDataReq.sessionId = pMac->oemData.oemDataReqConfig.sessionId;
            vos_mem_copy((v_VOID_t*)(pOemDataCmd->u.oemDataCmd.oemDataReq.oemDataReq),
                                    (v_VOID_t*)(pMac->oemData.oemDataReqConfig.oemDataReq), OEM_DATA_REQ_SIZE);
        }
        else
        {
            status = eHAL_STATUS_FAILURE;
            break;
        }

        //now queue this command in the sme command queue
        //Here since this is not interacting with the csr just push the command
        //into the sme queue. Also push this command with the normal priority
        smePushCommand(pMac, pOemDataCmd, eANI_BOOLEAN_FALSE);

    } while(0);

    if(!HAL_STATUS_SUCCESS(status) && pOemDataCmd)
    {
        oemData_ReleaseOemDataReqCommand(pMac, pOemDataCmd, eOEM_DATA_REQ_FAILURE);
        pMac->oemData.oemDataReqActive = eANI_BOOLEAN_FALSE;
    }

    return status;
}

/* ---------------------------------------------------------------------------
    \fn oemData_SendMBOemDataReq
    \brief Request an OEM DATA REQ to be passed down to PE
    \param pMac:
    \param pOemDataReq: Pointer to the oem data request
    \return eHalStatus
  -------------------------------------------------------------------------------*/
eHalStatus oemData_SendMBOemDataReq(tpAniSirGlobal pMac, tOemDataReq *pOemDataReq)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSirOemDataReq* pMsg;
    tANI_U16 msgLen;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, pOemDataReq->sessionId );

    smsLog(pMac, LOGW, "OEM_DATA: entering Function %s", __func__);

    msgLen = (tANI_U16)(sizeof(tSirOemDataReq));

    status = palAllocateMemory(pMac->hHdd, (void**)&pMsg, msgLen);
    if(HAL_STATUS_SUCCESS(status))
    {
        palZeroMemory(pMac->hHdd, pMsg, msgLen);
        pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_OEM_DATA_REQ);
        palCopyMemory(pMac->hHdd, pMsg->selfMacAddr, pSession->selfMacAddr, sizeof(tSirMacAddr) );
        status = palCopyMemory(pMac->hHdd, pMsg->oemDataReq, pOemDataReq->oemDataReq, OEM_DATA_REQ_SIZE);
        if(HAL_STATUS_SUCCESS(status))
        {
            smsLog(pMac, LOGW, "OEM_DATA: sending message to pe%s", __func__);
            status = palSendMBMessage(pMac->hHdd, pMsg);
        }
        else
        {
            palFreeMemory(pMac->hHdd, pMsg);
        }
    }

    smsLog(pMac, LOGW, "OEM_DATA: exiting Function %s", __func__);

    return status;
}

/* ---------------------------------------------------------------------------
    \fn oemData_ProcessOemDataReqCommand
    \brief This function is called by the smeProcessCommand when the case hits
           eSmeCommandOemDataReq
    \return eHalStatus
  -------------------------------------------------------------------------------*/
eHalStatus oemData_ProcessOemDataReqCommand(tpAniSirGlobal pMac, tSmeCmd *pOemDataReqCmd)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;

    //check if the system is in proper mode of operation for
    //oem data req/rsp to be functional. Currently, concurrency is not
    //supported and the driver must be operational only as
    //STA for oem data req/rsp to be functional. We return an invalid
    //mode flag if it is operational as any one of the following
    //in any of the active sessions
    //1. AP Mode
    //2. IBSS Mode
    //3. BTAMP Mode ...

    if(eHAL_STATUS_SUCCESS == oemData_IsOemDataReqAllowed(pMac))
    {
        smsLog(pMac, LOG1, "%s: OEM_DATA REQ allowed in the current mode", __func__);
        pMac->oemData.oemDataReqActive = eANI_BOOLEAN_TRUE;
        status = oemData_SendMBOemDataReq(pMac, &(pOemDataReqCmd->u.oemDataCmd.oemDataReq));
    }
    else
    {
        smsLog(pMac, LOG1, "%s: OEM_DATA REQ not allowed in the current mode", __func__);
        oemData_ReleaseOemDataReqCommand(pMac, pOemDataReqCmd, eOEM_DATA_REQ_INVALID_MODE);
        pMac->oemData.oemDataReqActive = eANI_BOOLEAN_FALSE;
    }

    return status;
}

/* ---------------------------------------------------------------------------
    \fn sme_HandleOemDataRsp
    \brief This function processes the oem data response obtained from the PE
    \param pMsg - Pointer to the pSirOemDataRsp
    \return eHalStatus
  -------------------------------------------------------------------------------*/
eHalStatus sme_HandleOemDataRsp(tHalHandle hHal, tANI_U8* pMsg)
{
    eHalStatus                         status = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal                     pMac;
    tListElem                          *pEntry = NULL;
    tSmeCmd                            *pCommand = NULL;
    tSirOemDataRsp*                    pOemDataRsp = NULL;
    pMac = PMAC_STRUCT(hHal);

    smsLog(pMac, LOG1, "%s: OEM_DATA Entering", __func__);

    do
    {
        if(pMsg == NULL)
        {
            smsLog(pMac, LOGE, "in %s msg ptr is NULL", __func__);
            status = eHAL_STATUS_FAILURE;
            break;
        }

        pEntry = csrLLPeekHead( &pMac->sme.smeCmdActiveList, LL_ACCESS_LOCK );
        if(pEntry)
        {
            pCommand = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
            if(eSmeCommandOemDataReq == pCommand->command)
            {
                pOemDataRsp = (tSirOemDataRsp*)pMsg;

                //make sure to acquire the lock before modifying the data
                status = sme_AcquireGlobalLock(&pMac->sme);
                if(!HAL_STATUS_SUCCESS(status))
                {
                    break;
                }

                if(pMac->oemData.pOemDataRsp != NULL)
                {
                    vos_mem_free(pMac->oemData.pOemDataRsp);
                }
                pMac->oemData.pOemDataRsp = (tOemDataRsp*)vos_mem_malloc(sizeof(tOemDataRsp));

                if(pMac->oemData.pOemDataRsp == NULL)
                {
                    sme_ReleaseGlobalLock(&pMac->sme);
                    smsLog(pMac, LOGE, "in %s vos_mem_malloc failed for pMac->oemData.pOemDataRsp", __func__);
                    status = eHAL_STATUS_FAILURE;
                    break;
                }

                smsLog(pMac, LOGE, "Before memory copy");
                vos_mem_copy((v_VOID_t*)(pMac->oemData.pOemDataRsp), (v_VOID_t*)(&pOemDataRsp->oemDataRsp), sizeof(tOemDataRsp));
                smsLog(pMac, LOGE, "after memory copy");
                sme_ReleaseGlobalLock(&pMac->sme);
            }
            else
            {
                smsLog(pMac, LOGE, "in %s eWNI_SME_OEM_DATA_RSP Received but NO REQs are ACTIVE ...",
                    __func__);
                status = eHAL_STATUS_FAILURE;
                break;
            }
        }
        else
        {
            smsLog(pMac, LOGE, "in %s eWNI_SME_OEM_DATA_RSP Received but NO commands are ACTIVE ...", __func__);
            status = eHAL_STATUS_FAILURE;
            break;
        }

        oemData_ReleaseOemDataReqCommand(pMac, pCommand, eHAL_STATUS_SUCCESS);
        pMac->oemData.oemDataReqActive = eANI_BOOLEAN_FALSE;

    } while(0);

    return status;
}

/* ---------------------------------------------------------------------------
    \fn oemData_IsOemDataReqAllowed
    \brief This function checks if OEM DATA REQs can be performed in the
           current driver state
    \return eHalStatus
  -------------------------------------------------------------------------------*/
eHalStatus oemData_IsOemDataReqAllowed(tHalHandle hHal)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tANI_U32 sessionId;

    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    for(sessionId = 0; sessionId < CSR_ROAM_SESSION_MAX; sessionId++)
    {
        if(CSR_IS_SESSION_VALID(pMac, sessionId))
        {
            //co-exist with IBSS or BT-AMP mode is not supported
            if(csrIsConnStateIbss(pMac, sessionId) || csrIsBTAMP(pMac, sessionId) )
            {
                //co-exist with IBSS or BT-AMP mode is not supported
                smsLog(pMac, LOGW, "OEM DATA REQ is not allowed due to IBSS|BTAMP exist in session %d", sessionId);
                status = eHAL_STATUS_CSR_WRONG_STATE;
                break;
            }
        }
    }

    smsLog(pMac, LOG1, "Exiting oemData_IsOemDataReqAllowed with status %d", status);

    return (status);
}

#endif /*FEATURE_OEM_DATA_SUPPORT*/
