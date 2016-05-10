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

/*
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file contains the source code for composing and sending messages
 * to host.
 *
 * Author:      Kevin Nguyen
 * Date:        04/09/02
 * History:-
 * 04/09/02     Created.
 * --------------------------------------------------------------------
 */
#include "palTypes.h"
#include "cfgPriv.h"
#include "limTrace.h"
#include "cfgDebug.h"

extern void SysProcessMmhMsg(tpAniSirGlobal pMac, tSirMsgQ* pMsg);

/*--------------------------------------------------------------------*/
/* ATTENTION:  The functions contained in this module are to be used  */
/*             by CFG module ONLY.                                    */
/*--------------------------------------------------------------------*/


/**---------------------------------------------------------------------
 * cfgSendHostMsg()
 *
 * FUNCTION:
 * Send CNF/RSP to host.
 *
 * LOGIC:
 * Please see Configuration & Statistic Collection Micro-Architecture
 * specification for details.
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param msgType:     message type
 * @param msgLen:      message length
 * @param paramNum:    number of parameters
 * @param pParamList:  pointer to parameter list
 * @param dataLen:     data length
 * @param pData:       pointer to additional data
 *
 * @return None.
 *
 */
void
cfgSendHostMsg(tpAniSirGlobal pMac, tANI_U16 msgType, tANI_U32 msgLen, tANI_U32 paramNum, tANI_U32 *pParamList,
              tANI_U32 dataLen, tANI_U32 *pData)
{
    tANI_U32        *pMsg, *pEnd;
    tSirMsgQ    mmhMsg;

    // sanity
    if ((paramNum > 0) && (NULL == pParamList))
    {
        PELOGE(cfgLog(pMac, LOGE,
                      FL("pParamList NULL when paramNum greater than 0!"));)
        return;
    }
    if ((dataLen > 0) && (NULL == pData))
    {
        PELOGE(cfgLog(pMac, LOGE,
                      FL("pData NULL when dataLen greater than 0!"));)
        return;
    }

    // Allocate message buffer
    pMsg = vos_mem_malloc(msgLen);
    if ( NULL == pMsg )
    {
        PELOGE(cfgLog(pMac, LOGE,
                      FL("Memory allocation failure!"));)
        return;
    }

    // Fill in message details
    mmhMsg.type = msgType;
    mmhMsg.bodyptr = pMsg;
    mmhMsg.bodyval = 0;
    ((tSirMbMsg*)pMsg)->type   = msgType;
    ((tSirMbMsg*)pMsg)->msgLen = (tANI_U16)msgLen;

    switch (msgType)
    {
        case WNI_CFG_GET_RSP:
        case WNI_CFG_PARAM_UPDATE_IND:
        case WNI_CFG_DNLD_REQ:
        case WNI_CFG_DNLD_CNF:
        case WNI_CFG_SET_CNF:
            // Fill in parameters
            pMsg++;
            if (NULL != pParamList)
            {
                pEnd  = pMsg + paramNum;
                while (pMsg < pEnd)
                {
                    *pMsg++ = *pParamList++;
                }
            }
            // Copy data if there is any
            if (NULL != pData)
            {
                pEnd = pMsg + (dataLen >> 2);
                while (pMsg < pEnd)
                {
                    *pMsg++ = *pData++;
                }
            }
            break;

        default:
           PELOGE(cfgLog(pMac, LOGE,
                         FL("Unknown msg %d!"), (int) msgType);)
            vos_mem_free( pMsg);
            return;
    }

    // Ship it
    MTRACE(macTraceMsgTx(pMac, NO_SESSION, mmhMsg.type));
    SysProcessMmhMsg(pMac, &mmhMsg);

} /*** end cfgSendHostMsg() ***/




