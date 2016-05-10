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

/*============================================================================
Copyright (c) 2007 QUALCOMM Incorporated.
All Rights Reserved.
Qualcomm Confidential and Proprietary
csrLogDump.c
Implements the dump commands specific to the csr module. 
============================================================================*/
#include "aniGlobal.h"
#include "csrApi.h"
#include "btcApi.h"
#include "logDump.h"
#include "smsDebug.h"
#include "smeInside.h"
#include "csrInsideApi.h"
#if defined(ANI_LOGDUMP)
static char *
dump_csr( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p )
{
    static tCsrRoamProfile x;
    static tSirMacSSid ssid;   //To be allocated for array of SSIDs
    static tANI_U8 sessionId; // Defined for fixed session ID
    palZeroMemory(pMac->hHdd, (void*)&x, sizeof(x)); 
    x.SSIDs.numOfSSIDs=1 ;
    x.SSIDs.SSIDList[0].SSID = ssid ;
    ssid.length=6 ;
    palCopyMemory(pMac->hHdd, ssid.ssId, "AniNet", 6);
    if(HAL_STATUS_SUCCESS(sme_AcquireGlobalLock( &pMac->sme )))
    {
        (void)csrRoamConnect(pMac, sessionId, &x, NULL, NULL);
        sme_ReleaseGlobalLock( &pMac->sme );
    }
    return p;
}
static char *dump_btcSetEvent( tpAniSirGlobal pMac, tANI_U32 arg1, 
                               tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p )
{
    tSmeBtEvent btEvent;
    if( arg1 < BT_EVENT_TYPE_MAX )
    {
        smsLog(pMac, LOGE, FL(" signal BT event (%d) handle (%d) 3rd param(%d)"), arg1, arg2, arg3);
        vos_mem_zero(&btEvent, sizeof(tSmeBtEvent));
        btEvent.btEventType = arg1;
        switch( arg1 )
        {
        case BT_EVENT_SYNC_CONNECTION_COMPLETE:
        case BT_EVENT_SYNC_CONNECTION_UPDATED:
            btEvent.uEventParam.btSyncConnection.connectionHandle = (v_U16_t)arg2;
            btEvent.uEventParam.btSyncConnection.status = (v_U8_t)arg3;
            break;
        case BT_EVENT_DISCONNECTION_COMPLETE:
            btEvent.uEventParam.btDisconnect.connectionHandle = (v_U16_t)arg2;
            break;
        case BT_EVENT_CREATE_ACL_CONNECTION:
        case BT_EVENT_ACL_CONNECTION_COMPLETE:
            btEvent.uEventParam.btAclConnection.connectionHandle = (v_U16_t)arg2;
            btEvent.uEventParam.btAclConnection.status = (v_U8_t)arg3;
            break;
        case BT_EVENT_MODE_CHANGED:
            btEvent.uEventParam.btAclModeChange.connectionHandle = (v_U16_t)arg2;
            break;
        default:
            break;
        }
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
        if(HAL_STATUS_SUCCESS(sme_AcquireGlobalLock( &pMac->sme )))
        {
            btcSignalBTEvent(pMac, &btEvent);
            sme_ReleaseGlobalLock( &pMac->sme );
        }
#endif
    }
    else
    {
        smsLog(pMac, LOGE, FL(" invalid event (%d)"), arg1);
    }
    return p;
}
static char* dump_csrApConcScanParams( tpAniSirGlobal pMac, tANI_U32 arg1, 
                               tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p )
{
    if( arg1 )
    {
        pMac->roam.configParam.nRestTimeConc = arg1;
    }
    if( arg2 )
    {
        pMac->roam.configParam.nActiveMinChnTimeConc = arg2;
    }
    if( arg3 )
    {
        pMac->roam.configParam.nActiveMaxChnTimeConc = arg3;
    }

    smsLog(pMac, LOGE, FL(" Working %d %d %d"), (int) pMac->roam.configParam.nRestTimeConc,
        (int)pMac->roam.configParam.nActiveMinChnTimeConc, (int) pMac->roam.configParam.nActiveMaxChnTimeConc);
    return p;
}

static tDumpFuncEntry csrMenuDumpTable[] = {
    {0,     "CSR (850-860)",                                    NULL},
    {851,   "CSR: CSR testing connection to AniNet",            dump_csr},
    {852,   "BTC: Fake BT events (event, handle)",              dump_btcSetEvent},
    {853,   "CSR: Split Scan related params",                   dump_csrApConcScanParams},
};

void csrDumpInit(tHalHandle hHal)
{
    logDumpRegisterTable( (tpAniSirGlobal)hHal, &csrMenuDumpTable[0], 
                          sizeof(csrMenuDumpTable)/sizeof(csrMenuDumpTable[0]) );
}

#endif //#if defined(ANI_LOGDUMP)
