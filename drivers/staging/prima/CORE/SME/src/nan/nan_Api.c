/*
 * Copyright (c) 2014-2015 The Linux Foundation. All rights reserved.
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

#include "sme_Api.h"
#include "smsDebug.h"
#include "csrInsideApi.h"
#include "smeInside.h"
#include "limApi.h"
#include "cfgApi.h"

/******************************************************************************
 * Function: sme_NanRegisterCallback
 *
 * Description:
 * This function gets called when HDD wants register nan rsp callback with
 * sme layer.
 *
 * Args:
 * hHal and callback which needs to be registered.
 *
 * Returns:
 * void
 *****************************************************************************/
void sme_NanRegisterCallback(tHalHandle hHal, NanCallback callback)
{
    tpAniSirGlobal pMac = NULL;

    if (NULL == hHal)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                FL("hHal is not valid"));
        return;
    }
    pMac = PMAC_STRUCT(hHal);
    pMac->sme.nanCallback = callback;
}

/******************************************************************************
 * Function: sme_NanRequest
 *
 * Description:
 * This function gets called when HDD receives NAN vendor command
 * from userspace
 *
 * Args:
 * hHal, Nan Request structure ptr and sessionId
 *
 * Returns:
 * VOS_STATUS
 *****************************************************************************/
VOS_STATUS sme_NanRequest(tHalHandle hHalHandle, tpNanRequestReq input,
        tANI_U32 sessionId)
{
    tNanRequest *pNanReq = NULL;
    size_t data_len;
    tSmeCmd *pCommand;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHalHandle);

    pCommand = csrGetCommandBuffer(pMac);
    if (NULL == pCommand)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                FL("Failed to get command buffer for nan req"));
        return eHAL_STATUS_RESOURCES;
    }

    data_len = sizeof(tNanRequest) - sizeof(pNanReq->request_data)
                 + input->request_data_len;
    pNanReq = vos_mem_malloc(data_len);

    if (pNanReq == NULL)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                FL("Memory allocation failure, size : %zu"), data_len);
        csrReleaseCommand(pMac, pCommand);
        return eHAL_STATUS_RESOURCES;
    }

    smsLog(pMac, LOG1, "Posting NAN command to csr queue");
    vos_mem_zero(pNanReq, data_len);
    pNanReq->request_data_len = input->request_data_len;
    vos_mem_copy(pNanReq->request_data,
                 input->request_data,
                 input->request_data_len);

    pCommand->command = eSmeCommandNanReq;
    pCommand->sessionId = sessionId;
    pCommand->u.pNanReq = pNanReq;

    if (!HAL_STATUS_SUCCESS(csrQueueSmeCommand(pMac, pCommand, TRUE)))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                 FL("failed to post eSmeCommandNanReq command"));
        csrReleaseCommand(pMac, pCommand);
        vos_mem_free(pNanReq);
        return VOS_STATUS_E_FAILURE;
    }

    return VOS_STATUS_SUCCESS;
}

/******************************************************************************
 * Function: sme_NanEvent
 *
 * Description:
 * This callback function will be called when SME received eWNI_SME_NAN_EVENT
 * event from WMA
 *
 * Args:
 * hHal - HAL handle for device
 * pMsg - Message body passed from WDA; includes NAN header
 *
 * Returns:
 * VOS_STATUS
******************************************************************************/
VOS_STATUS sme_NanEvent(tHalHandle hHal, void* pMsg)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    VOS_STATUS status = VOS_STATUS_SUCCESS;

    if (NULL == pMsg)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                FL("msg ptr is NULL"));
        status = VOS_STATUS_E_FAILURE;
    }
    else
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_MED,
                FL("SME: Received sme_NanEvent"));
        if (pMac->sme.nanCallback)
        {
            pMac->sme.nanCallback(pMac->hHdd, (tSirNanEvent *)pMsg);
        }
        else
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                    FL("nanCallback is NULL"));
        }
    }

    return status;
}
