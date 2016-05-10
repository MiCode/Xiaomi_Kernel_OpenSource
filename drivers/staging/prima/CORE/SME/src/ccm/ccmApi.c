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

#include "palTypes.h"
#include "wniApi.h"     /* WNI_CFG_SET_REQ */
#include "sirParams.h"  /* tSirMbMsg */
#include "smsDebug.h"   /* smsLog */
#include "cfgApi.h"
#include "ccmApi.h"
#include "logDump.h"

//#define CCM_DEBUG
#undef CCM_DEBUG

#define CCM_DEBUG2
//#undef CCM_DEBUG2

#define CFGOBJ_ALIGNTO          4
#define CFGOBJ_ALIGN(len)       ( ((len)+CFGOBJ_ALIGNTO-1) & ~(CFGOBJ_ALIGNTO-1) )

#define CFGOBJ_ID_SIZE                  4       /* 4 bytes for cfgId */
#define CFGOBJ_LEN_SIZE                 4       /* 4 bytes for length */
#define CFGOBJ_INTEGER_VALUE_SIZE       4       /* 4 bytes for integer value */

#define CFG_UPDATE_MAGIC_DWORD     0xabab

#define halHandle2HddHandle(hHal)  ( (NULL == (hHal)) ? 0 : ((tpAniSirGlobal)(hHal))->hHdd )

static void ccmComplete(tHddHandle hHdd, void *done)
{
    if (done)
    {
        (void)palSemaphoreGive(hHdd, done);
    }
}

static void ccmWaitForCompletion(tHddHandle hHdd, void *done)
{
    if (done)
    {
        (void)palSemaphoreTake(hHdd, done);
    }
}

static tANI_U32 * encodeCfgReq(tHddHandle hHdd, tANI_U32 *pl, tANI_U32 cfgId, tANI_S32 length, void *pBuf, tANI_U32 value, tANI_U32 type)
{
    *pl++ = pal_cpu_to_be32(cfgId) ;
    *pl++ = pal_cpu_to_be32(length) ;
    if (type == CCM_INTEGER_TYPE)
    {
        *pl++ = pal_cpu_to_be32(value) ;
    }
    else
    {
        palCopyMemory(hHdd, (void *)pl, (void *)pBuf, length);
        pl += (CFGOBJ_ALIGN(length) / CFGOBJ_ALIGNTO);
    }
    return pl ;
}

/*
 * CCM_STRING_TYPE                       CCM_INTEGER_TYPE
 * |<--------  4   ----->|               |<--------  4   ----->|                          
 * +----------+            <-- msg  -->  +----------+                                 
 * |type      |                          |type      |           
 * +----------+                          +----------+           
 * |msgLen=24 |                          |msgLen=16 |           
 * +----------+----------+               +----------+----------+
 * | cfgId               |               | cfgId               |  
 * +---------------------+               +---------------------+
 * | length=11           |               | length=4            |  
 * +---------------------+               +---------------------+
 * |                     |               | value               |  
 * |                     |               +---------------------+  
 * |                     |
 * |                +----+
 * |                |////| <- padding to 4-byte boundary
 * +----------------+----+
 */
static eHalStatus sendCfg(tpAniSirGlobal pMac, tHddHandle hHdd, tCfgReq *req, tANI_BOOLEAN fRsp)
{
    tSirMbMsg *msg;
    eHalStatus status;
    tANI_S16 msgLen = (tANI_U16)(4 +    /* 4 bytes for msg header */
                                 CFGOBJ_ID_SIZE +
                                 CFGOBJ_LEN_SIZE +
                                 CFGOBJ_ALIGN(req->length)) ;

    status = palAllocateMemory(hHdd, (void **)&msg, msgLen);
    if (status == eHAL_STATUS_SUCCESS)
    {
        if( fRsp )
        {
            msg->type = pal_cpu_to_be16(WNI_CFG_SET_REQ);
        }
        else
        {
            msg->type = pal_cpu_to_be16(WNI_CFG_SET_REQ_NO_RSP);
        }
        msg->msgLen = pal_cpu_to_be16(msgLen);
        (void)encodeCfgReq(hHdd, msg->data, req->cfgId, req->length, req->ccmPtr, req->ccmValue, req->type) ;

        status = palSendMBMessage(hHdd, msg) ;
        if (status != eHAL_STATUS_SUCCESS)
        {
            smsLog( pMac, LOGE, FL("palSendMBMessage() failed"));
            //No need to free msg. palSendMBMessage frees it.
            status = eHAL_STATUS_FAILURE ;
        }
    }
    else
    {
        smsLog( pMac, LOGW, FL("palAllocateMemory(len=%d)"), msgLen );
    }

    return status ;
}

static tCfgReq * allocateCfgReq(tHddHandle hHdd, tANI_U32 type, tANI_S32 length)
{
    tCfgReq *req ;
    tANI_S32 alloc_len = sizeof(tCfgReq) ;

    if (type == CCM_STRING_TYPE)
    {
        alloc_len += length ;
    }

    if (palAllocateMemory(hHdd, (void **)&req, alloc_len) != eHAL_STATUS_SUCCESS)
    {
        return NULL ;
    }

    req->ccmPtr = (req+1);

    return req ;
}

static void freeCfgReq(tHddHandle hHdd, tCfgReq *req)
{
    palFreeMemory(hHdd, (void*)req) ;
}

static void add_req_tail(tCfgReq *req, struct ccmlink *q)
{
    if (q->tail)
    {
        q->tail->next = req;
        q->tail = req ;
    }
    else
    {
        q->head = q->tail = req ;
    }
}

static void del_req(tCfgReq *req, struct ccmlink *q)
{
    q->head = req->next ;
    req->next = NULL ;
    if (q->head == NULL)
    {
        q->tail = NULL ;
    }
}

static void purgeReqQ(tHalHandle hHal)
{
    tHddHandle hHdd = halHandle2HddHandle(hHal);
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tCfgReq *req, *tmp ;

    for (req = pMac->ccm.reqQ.head; req; req = tmp)
    {
        /* loop thru reqQ and invoke callback to return failure */
        smsLog(pMac, LOGW, FL("deleting cfgReq, cfgid=%d"), (int)req->cfgId);

        tmp = req->next ;

        if (req->callback)
        {
            req->callback(hHal, eHAL_STATUS_FAILURE);
        }
        palSpinLockTake(hHdd, pMac->ccm.lock);
        del_req(req, &pMac->ccm.reqQ);
        palSpinLockGive(hHdd, pMac->ccm.lock);
        freeCfgReq(hHdd, req);
    }
    return ;
}

static void sendQueuedReqToMacSw(tpAniSirGlobal pMac, tHddHandle hHdd)
{
    tCfgReq *req ;

    /* Send the head req */
    req = pMac->ccm.reqQ.head ;
    if (req)
    {
        if (req->state == eCCM_REQ_QUEUED)
        {
            /* Send WNI_CFG_SET_REQ */
            req->state = eCCM_REQ_SENT;
            if (sendCfg(pMac, hHdd, req, eANI_BOOLEAN_TRUE) != eHAL_STATUS_SUCCESS)
            {
                smsLog( pMac, LOGE, FL("sendCfg() failed"));
                palSpinLockTake(hHdd, pMac->ccm.lock);
                del_req(req, &pMac->ccm.reqQ) ;
                palSpinLockGive(hHdd, pMac->ccm.lock);
                if (req->callback)
                {
                    req->callback((tHalHandle)pMac, WNI_CFG_OTHER_ERROR) ;
                }

#ifdef CCM_DEBUG
                smsLog(pMac, LOGW, FL("ccmComplete(%p)"), req->done);
#endif
                ccmComplete(hHdd, req->done);

                freeCfgReq(hHdd, req);
            }
        }
        else
        {
            smsLog( pMac, LOGW, FL("reqState is not eCCM_REQ_QUEUED, is %d"), req->state );
        }
    }

    return ;
}

static eHalStatus cfgSetSub(tpAniSirGlobal pMac, tHddHandle hHdd, tANI_U32 cfgId, tANI_U32 type, 
                            tANI_S32 length, void *ccmPtr, tANI_U32 ccmValue, 
                            tCcmCfgSetCallback callback, eAniBoolean toBeSaved, void *sem, tCfgReq **r)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tCfgReq *req ;

    do
    {
        *r = NULL ;

        if (pMac->ccm.state == eCCM_STOPPED)
        {
            status = eHAL_STATUS_FAILURE ;
            break ;
        }

        req = allocateCfgReq(hHdd, type, length);
        if (req == NULL)
        {
            status = eHAL_STATUS_FAILED_ALLOC ;
            break ;
        }

        req->next       = NULL ;
        req->cfgId      = (tANI_U16)cfgId ;
        req->type       = (tANI_U8)type ;
        req->state      = eCCM_REQ_QUEUED ;
        req->toBeSaved  = !!toBeSaved ;
        req->length     = length ;
        req->done       = sem ;
        req->callback   = callback ;
        if (type == CCM_INTEGER_TYPE)
        {
            req->ccmValue = ccmValue ;
        }
        else
        {
            palCopyMemory(hHdd, (void*)req->ccmPtr, (void*)ccmPtr, length); 
        }

        palSpinLockTake(hHdd, pMac->ccm.lock);

        add_req_tail(req, &pMac->ccm.reqQ);
        /* If this is the first req on the queue, send it to MAC SW */
        if ((pMac->ccm.replay.started == 0) && (pMac->ccm.reqQ.head == req))
        {
            /* Send WNI_CFG_SET_REQ */
            req->state = eCCM_REQ_SENT;
            palSpinLockGive(hHdd, pMac->ccm.lock);
            status = sendCfg(pMac, hHdd, req, eANI_BOOLEAN_TRUE) ;
            if (status != eHAL_STATUS_SUCCESS)
            {
                smsLog( pMac, LOGE, FL("sendCfg() failed"));
                palSpinLockTake(hHdd, pMac->ccm.lock);
                del_req(req, &pMac->ccm.reqQ);
                palSpinLockGive(hHdd, pMac->ccm.lock);
                freeCfgReq(hHdd, req);
                break ;
            }
            else
            {
                palSpinLockTake(hHdd, pMac->ccm.lock);
                if(req != pMac->ccm.reqQ.head)
                {
                    //We send the request and it must be done already
                    req = NULL;
                }
                palSpinLockGive(hHdd, pMac->ccm.lock);
            }
        }
        else
        {
            palSpinLockGive(hHdd, pMac->ccm.lock);
        }
        *r = req ;

    } while(0) ;

    return status;
}

static eHalStatus cfgSet(tHalHandle hHal, tANI_U32 cfgId, tANI_U32 type, tANI_S32 length, void * ccmPtr, tANI_U32 ccmValue, tCcmCfgSetCallback callback, eAniBoolean toBeSaved)
{
    tHddHandle hHdd = halHandle2HddHandle(hHal);
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status;
    tCfgReq *req ;

    if (pal_in_interrupt())
    {
#ifdef CCM_DEBUG2
        smsLog(pMac, LOG1, FL("WNI_CFG_%s (%d 0x%x), in_interrupt()=TRUE"), gCfgParamName[cfgId], (int)cfgId, (int)cfgId);
#endif
        status = cfgSetSub(pMac, hHdd, cfgId, type, length, ccmPtr, ccmValue, callback, toBeSaved, NULL, &req);
    }
    else
    {
        void *sem ;

#ifdef CCM_DEBUG2
        smsLog(pMac, LOG1, FL("WNI_CFG_%s (%d 0x%x), in_interrupt()=FALSE"), gCfgParamName[cfgId], (int)cfgId, (int)cfgId);
#endif
        pal_local_bh_disable() ;

        status = palMutexAllocLocked( hHdd, &sem ) ;
        if (status != eHAL_STATUS_SUCCESS)
        {
            smsLog(pMac, LOGE, FL("mutex alloc failed"));
            sem = NULL;
        }
        else
        {
            status = cfgSetSub(pMac, hHdd, cfgId, type, length, ccmPtr, ccmValue, callback, toBeSaved, sem, &req);
            if ((status != eHAL_STATUS_SUCCESS) || (req == NULL))
            {
                //Either it fails to send or the req is finished already
                palSemaphoreFree( hHdd, sem );
                sem = NULL;
            }
        }

        pal_local_bh_enable() ;

        if ((status == eHAL_STATUS_SUCCESS) && (sem != NULL))
        {
#ifdef CCM_DEBUG
            smsLog(pMac, LOG1, FL("ccmWaitForCompletion(%p)"), req->done);
#endif
            ccmWaitForCompletion(hHdd, sem);

#ifdef CCM_DEBUG
            smsLog(pMac, LOG1, FL("free(%p)"), req->done);
#endif
            palSemaphoreFree( hHdd, sem ) ;
        }
    }

    return status ;
}

eHalStatus ccmOpen(tHalHandle hHal)
{
    tHddHandle hHdd = halHandle2HddHandle(hHal);
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    (void)palZeroMemory(hHdd, &pMac->ccm, sizeof(tCcm)) ;
    return palSpinLockAlloc(hHdd, &pMac->ccm.lock);
}

eHalStatus ccmClose(tHalHandle hHal)
{
    tHddHandle hHdd = halHandle2HddHandle(hHal);
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tANI_U32 i ; 
    tCfgReq *req ;

    ccmStop(hHal);

    /* Go thru comp[] to free all saved requests */
    for (i = 0 ; i < CFG_PARAM_MAX_NUM ; ++i)
    {
        if ((req = pMac->ccm.comp[i]) != NULL)
        {
            freeCfgReq(hHdd, req);
        }
    }

    return palSpinLockFree(hHdd, pMac->ccm.lock);
}

/* This function executes in (Linux) softirq context */
void ccmCfgCnfMsgHandler(tHalHandle hHal, void *m)
{
    tHddHandle hHdd = halHandle2HddHandle(hHal);
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tSirMbMsg *msg = (tSirMbMsg *)m ;
    tANI_U32 result, cfgId ;
    tCfgReq *req, *old ;

#if 0
    if (pMac->ccm.state != eCCM_STARTED)
    {
        return ;
    }
#endif

    result  = pal_be32_to_cpu(msg->data[0]);
    cfgId   = pal_be32_to_cpu(msg->data[1]);

    if (pMac->ccm.replay.started && cfgId == CFG_UPDATE_MAGIC_DWORD)
    {
        pMac->ccm.replay.in_progress = 1 ;
        return ;
    }

    if (pMac->ccm.replay.in_progress)
    {
        /* save error code */
        if (!CCM_IS_RESULT_SUCCESS(result))
        {
            pMac->ccm.replay.result = result ;
        }

        if (--pMac->ccm.replay.nr_param == 0)
        {
            pMac->ccm.replay.in_progress = 0 ;

            if (pMac->ccm.replay.callback)
            {
                pMac->ccm.replay.callback(hHal, pMac->ccm.replay.result);
            }

            pMac->ccm.replay.started = 0 ;

            /* Wake up the sleeping process */
#ifdef CCM_DEBUG
            smsLog(pMac, LOGW, FL("ccmComplete(%p)"), pMac->ccm.replay.done);
#endif
            ccmComplete(hHdd, pMac->ccm.replay.done);
            //Let go with the rest of the set CFGs waiting.
            sendQueuedReqToMacSw(pMac, hHdd);
        }
    }
    else
    {
        /*
         * Try to match this response with the request.
         * What if i could not find the req entry ???
         */
        req = pMac->ccm.reqQ.head ;
        if (req)
        {

            if (req->cfgId == cfgId && req->state == eCCM_REQ_SENT)
            {
                palSpinLockTake(hHdd, pMac->ccm.lock);
                del_req(req, &pMac->ccm.reqQ);
                palSpinLockGive(hHdd, pMac->ccm.lock);
                req->state = eCCM_REQ_DONE ;

                if (result == WNI_CFG_NEED_RESTART ||
                    result == WNI_CFG_NEED_RELOAD)
                {
#ifdef CCM_DEBUG
                    smsLog(pMac, LOGW, FL("need restart/reload, cfgId=%d"), req->cfgId) ;
#endif
                    //purgeReqQ(hHal);
                }

                /* invoke callback */
                if (req->callback)
                {
#ifdef CCM_DEBUG
                    req->callback(hHal, cfgId) ;
#else
                    req->callback(hHal, result) ;
#endif
                }

                /* Wake up the sleeping process */
#ifdef CCM_DEBUG
                smsLog(pMac, LOGW, FL("cfgId=%ld, calling ccmComplete(%p)"), cfgId, req->done);
#endif
                ccmComplete(hHdd, req->done);

                /* move the completed req from reqQ to comp[] */
                if (req->toBeSaved && (CCM_IS_RESULT_SUCCESS(result)))
                {
                    if (cfgId < CFG_PARAM_MAX_NUM)
                    {
                        if ((old = pMac->ccm.comp[cfgId]) != NULL)
                        {
                            freeCfgReq(hHdd, old) ;
                        }
                        pMac->ccm.comp[cfgId] = req ;
                    }
                }
                else
                {
                    freeCfgReq(hHdd, req) ;
                }
                sendQueuedReqToMacSw(pMac, hHdd);
            }
            else
            {
                smsLog( pMac, LOGW, FL("can not match RSP with REQ, rspcfgid=%d result=%d reqcfgid=%d reqstate=%d"),
                        (int)cfgId, (int)result, req->cfgId, req->state);

#ifdef CCM_DEBUG
                smsLog(pMac, LOGW, FL("ccmComplete(%p)"), req->done);
#endif
            }

        }
    }

    return ;
}

void ccmStart(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    pMac->ccm.state = eCCM_STARTED ;

#if defined(ANI_LOGDUMP)
    ccmDumpInit(hHal);
#endif //#if defined(ANI_LOGDUMP)

    return ;
}

void ccmStop(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    pMac->ccm.state = eCCM_STOPPED ;

    pal_local_bh_disable() ;
    purgeReqQ(hHal);
    pal_local_bh_enable() ;

    return ;
}

eHalStatus ccmCfgSetInt(tHalHandle hHal, tANI_U32 cfgId, tANI_U32 ccmValue, tCcmCfgSetCallback callback, eAniBoolean toBeSaved)
{
    if( callback || toBeSaved)
    {
        //we need to sychronous this one
        return cfgSet(hHal, cfgId, CCM_INTEGER_TYPE, sizeof(tANI_U32), NULL, ccmValue, callback, toBeSaved);
    }
    else
    {
        //Simply push to CFG and not waiting for the response
        tCfgReq req;
        tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

        req.callback = NULL;
        req.next = NULL;
        req.cfgId = ( tANI_U16 )cfgId;
        req.length = sizeof( tANI_U32 );
        req.type = CCM_INTEGER_TYPE;
        req.ccmPtr = NULL;
        req.ccmValue = ccmValue;
        req.toBeSaved = toBeSaved;
        req.state = eCCM_REQ_SENT;

        return ( sendCfg( pMac, pMac->hHdd, &req, eANI_BOOLEAN_FALSE ) );
    }
}

eHalStatus ccmCfgSetStr(tHalHandle hHal, tANI_U32 cfgId, tANI_U8 *pStr, tANI_U32 length, tCcmCfgSetCallback callback, eAniBoolean toBeSaved)
{
    if( callback || toBeSaved )
    {
        //we need to sychronous this one
        return cfgSet(hHal, cfgId, CCM_STRING_TYPE, length, pStr, 0, callback, toBeSaved);
    }
    else
    {
        //Simply push to CFG and not waiting for the response
        tCfgReq req;
        tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

        req.callback = NULL;
        req.next = NULL;
        req.cfgId = ( tANI_U16 )cfgId;
        req.length = length;
        req.type = CCM_STRING_TYPE;
        req.ccmPtr = pStr;
        req.ccmValue = 0;
        req.toBeSaved = toBeSaved;
        req.state = eCCM_REQ_SENT;

        return ( sendCfg( pMac, pMac->hHdd, &req, eANI_BOOLEAN_FALSE ) );
    }
}

eHalStatus ccmCfgGetInt(tHalHandle hHal, tANI_U32 cfgId, tANI_U32 *pValue)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status = eHAL_STATUS_SUCCESS ;
    tCfgReq *req = pMac->ccm.comp[cfgId] ;

    if (req && req->state == eCCM_REQ_DONE)
    {
        *pValue = req->ccmValue ; 
    }
    else
    {
        if (wlan_cfgGetInt(pMac, (tANI_U16)cfgId, pValue) != eSIR_SUCCESS)
            status = eHAL_STATUS_FAILURE;
    }

    return status ;
}

eHalStatus ccmCfgGetStr(tHalHandle hHal, tANI_U32 cfgId, tANI_U8 *pBuf, tANI_U32 *pLength)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tHddHandle hHdd;
    eHalStatus status = eHAL_STATUS_SUCCESS ;
    tCfgReq *req;

    if (!pMac)
        return eHAL_STATUS_FAILURE;

    hHdd = halHandle2HddHandle(hHal);
    req = pMac->ccm.comp[cfgId] ;

    if (req && req->state == eCCM_REQ_DONE && (tANI_U32)req->length <= *pLength)
    {
        *pLength = req->length ; 
        palCopyMemory(hHdd, (void*)pBuf, (void*)req->ccmPtr, req->length); 
    }
    else
    {
        if (wlan_cfgGetStr(pMac, (tANI_U16)cfgId, pBuf, pLength) != eSIR_SUCCESS)
            status = eHAL_STATUS_FAILURE;
    }

    return status ;
}

/*
 * Loop thru comp[] and form an ANI message which contains all completed cfgIds.
 * The message begins with an INTEGER parameter (cfgId=CFG_UPDATE_MAGIC_DWORD)     
 * to mark the start of the message.
 */ 
static eHalStatus cfgUpdate(tpAniSirGlobal pMac, tHddHandle hHdd, tCcmCfgSetCallback callback)
{
    tANI_U32 i, *pl ;
    tCfgReq *req ;
    tSirMbMsg *msg ;
    eHalStatus status ;
    tANI_S16 msgLen = 4 +       /* 4 bytes for msg header */ 
                                /* for CFG_UPDATE_MAGIC_DWORD */ 
        CFGOBJ_ID_SIZE +
        CFGOBJ_LEN_SIZE +
        CFGOBJ_INTEGER_VALUE_SIZE ;

    if (pMac->ccm.state == eCCM_STOPPED || pMac->ccm.replay.started)
    {
        status = eHAL_STATUS_FAILURE ;
        goto end ;
    }

    palSpinLockTake(hHdd, pMac->ccm.lock);

    pMac->ccm.replay.started    = 1 ;
    pMac->ccm.replay.nr_param   = 0 ;

    palSpinLockGive(hHdd, pMac->ccm.lock);

    /* Calculate message length */
    for (i = 0 ; i < CFG_PARAM_MAX_NUM ; ++i)
    {
        if ((req = pMac->ccm.comp[i]) != NULL)
        {
            msgLen += (tANI_S16)(CFGOBJ_ID_SIZE + CFGOBJ_LEN_SIZE + CFGOBJ_ALIGN(req->length)) ;
            pMac->ccm.replay.nr_param += 1 ;
#ifdef CCM_DEBUG
            smsLog(pMac, LOGW, FL("cfgId=%d"), req->cfgId);
#endif
        }
    }

    if (pMac->ccm.replay.nr_param == 0)
    {
        if (callback)
        {
            callback((tHalHandle)pMac, WNI_CFG_SUCCESS) ;
        }
        status = eHAL_STATUS_SUCCESS ;
        goto end ;
    }

    pMac->ccm.replay.in_progress = 0 ;
    pMac->ccm.replay.result      = WNI_CFG_SUCCESS ;
    pMac->ccm.replay.callback    = callback ;
    pMac->ccm.replay.done        = NULL ;

    status = palAllocateMemory(hHdd, (void **)&msg, msgLen) ;
    if (status != eHAL_STATUS_SUCCESS)
    {
        pMac->ccm.replay.started = 0 ;
        goto end ; 
    }

    msg->type   = pal_cpu_to_be16(WNI_CFG_SET_REQ);
    msg->msgLen = pal_cpu_to_be16(msgLen);

    /* Encode the starting cfgId */
    pl = encodeCfgReq(hHdd, msg->data, CFG_UPDATE_MAGIC_DWORD, 4, NULL, 0, CCM_INTEGER_TYPE) ;

    /* Encode the saved cfg requests */
    for (i = 0 ; i < CFG_PARAM_MAX_NUM ; ++i)
    {
        if ((req = pMac->ccm.comp[i]) != NULL)
        {
            pl = encodeCfgReq(hHdd, pl, req->cfgId, req->length, req->ccmPtr, req->ccmValue, req->type) ;
        }
    }

    status = palSendMBMessage(hHdd, msg) ;
    if (status != eHAL_STATUS_SUCCESS)
    {
        smsLog(pMac, LOGW, FL("palSendMBMessage() failed. status=%d"), status);
        pMac->ccm.replay.started = 0 ;
        //No need to free msg. palSendMBMessage frees it.
        goto end ;
    }

 end:
    return status ;
}

eHalStatus ccmCfgUpdate(tHalHandle hHal, tCcmCfgSetCallback callback)
{
    tHddHandle hHdd = halHandle2HddHandle(hHal);
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status ;

    pal_local_bh_disable() ;

    status = cfgUpdate(pMac, hHdd, callback) ;
    if (status == eHAL_STATUS_SUCCESS)
    {
        if (pMac->ccm.replay.nr_param == 0)
        {
            /* there is nothing saved at comp[], so we are done! */
            pMac->ccm.replay.started = 0 ;
        }
        else
        {
            /* we have sent update message to MAC SW */
            void *sem ;

            status = palMutexAllocLocked( hHdd, &sem ) ;
            if (status != eHAL_STATUS_SUCCESS)
            {
                smsLog(pMac, LOGE, FL("mutex alloc failed"));
                pMac->ccm.replay.started = 0 ;
            }
            else
            {
                pMac->ccm.replay.done = sem ;
            }
        }
    }

    pal_local_bh_enable() ;

    /* Waiting here ... */
    if (status == eHAL_STATUS_SUCCESS && pMac->ccm.replay.done)
    {
#ifdef CCM_DEBUG
        smsLog(pMac, LOGW, FL("ccmWaitForCompletion(%p)"), pMac->ccm.replay.done);
#endif
        ccmWaitForCompletion(hHdd, pMac->ccm.replay.done);

#ifdef CCM_DEBUG
        smsLog(pMac, LOGW, FL("free(%p)"), pMac->ccm.replay.done);
#endif
        palSemaphoreFree( hHdd, pMac->ccm.replay.done) ;
    }

    return status ;
}

