/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
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

/*
 * This file limAIDmgmt.cc contains the functions related to
 * AID pool management like initialization, assignment etc.
 * Author:        Chandra Modumudi
 * Date:          03/20/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */
 
#include "palTypes.h"
#include "wniCfgSta.h"
#include "aniGlobal.h"
#include "cfgApi.h"
#include "sirParams.h"
#include "limUtils.h"
#include "limTimerUtils.h"
#include "limSession.h"
#include "limSessionUtils.h"

#define LIM_START_PEER_IDX   1

/**
 * limInitPeerIdxpool()
 *
 *FUNCTION:
 * This function is called while starting a BSS at AP
 * to initialize AID pool. This may also be called while
 * starting/joining an IBSS if 'Association' is allowed
 * in IBSS.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void
limInitPeerIdxpool(tpAniSirGlobal pMac,tpPESession pSessionEntry)
{
    tANI_U8 i;
    tANI_U8 maxAssocSta = pMac->lim.gLimAssocStaLimit;

    pSessionEntry->gpLimPeerIdxpool[0]=0;

#ifdef FEATURE_WLAN_TDLS
    //In station role, DPH_STA_HASH_INDEX_PEER (index 1) is reserved for peer
    //station index corresponding to AP. Avoid choosing that index and get index
    //starting from (DPH_STA_HASH_INDEX_PEER + 1) (index 2) for TDLS stations;
    if (pSessionEntry->limSystemRole == eLIM_STA_ROLE )
    {
        pSessionEntry->freePeerIdxHead = DPH_STA_HASH_INDEX_PEER + 1;
    }
    else
#endif
    {
        pSessionEntry->freePeerIdxHead=LIM_START_PEER_IDX;
    }

    for (i=pSessionEntry->freePeerIdxHead;i<maxAssocSta; i++)
    {
        pSessionEntry->gpLimPeerIdxpool[i]         = i+1;
    }
    pSessionEntry->gpLimPeerIdxpool[i]         =  0;

    pSessionEntry->freePeerIdxTail=i;

}


/**
 * limAssignPeerIdx()
 *
 *FUNCTION:
 * This function is called to get a peer station index. This index is
 * used during Association/Reassociation
 * frame handling to assign association ID (aid) to a STA.
 * In case of TDLS, this is used to assign a index into the Dph hash entry.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return peerIdx  - assigned peer Station IDx for STA
 */

tANI_U16
limAssignPeerIdx(tpAniSirGlobal pMac, tpPESession pSessionEntry)
{
    tANI_U16 peerId;

    // make sure we haven't exceeded the configurable limit on associations
    // This count is global to ensure that it doesnt exceed the hardware limits.
    if (peGetCurrentSTAsCount(pMac) >= pMac->lim.gLimAssocStaLimit)
    {
        // too many associations already active
        return 0;
    }

    /* return head of free list */

    if (pSessionEntry->freePeerIdxHead)
    {
        peerId=pSessionEntry->freePeerIdxHead;
        pSessionEntry->freePeerIdxHead = pSessionEntry->gpLimPeerIdxpool[pSessionEntry->freePeerIdxHead];
        if (pSessionEntry->freePeerIdxHead==0)
            pSessionEntry->freePeerIdxTail=0;
        pSessionEntry->gLimNumOfCurrentSTAs++;
        //PELOG2(limLog(pMac, LOG2,FL("Assign aid %d, numSta %d, head %d tail %d "),aid,pSessionEntry->gLimNumOfCurrentSTAs,pSessionEntry->freeAidHead,pSessionEntry->freeAidTail);)
        return peerId;
    }

    return 0; /* no more free peer index */
}


/**
 * limReleasePeerIdx()
 *
 *FUNCTION:
 * This function is called when a STA context is removed
 * at AP (or at a STA in IBSS mode or TDLS) to return peer Index
 * to free pool.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  peerIdx - peer station index that need to return to free pool
 *
 * @return None
 */

void
limReleasePeerIdx(tpAniSirGlobal pMac, tANI_U16 peerIdx, tpPESession pSessionEntry)
{
    pSessionEntry->gLimNumOfCurrentSTAs--;

    /* insert at tail of free list */
    if (pSessionEntry->freePeerIdxTail)
    {
        pSessionEntry->gpLimPeerIdxpool[pSessionEntry->freePeerIdxTail]=(tANI_U8)peerIdx;
        pSessionEntry->freePeerIdxTail=(tANI_U8)peerIdx;
    }
    else
    {
        pSessionEntry->freePeerIdxTail=pSessionEntry->freePeerIdxHead=(tANI_U8)peerIdx;
    }
    pSessionEntry->gpLimPeerIdxpool[(tANI_U8)peerIdx]=0;
    //PELOG2(limLog(pMac, LOG2,FL("Release aid %d, numSta %d, head %d tail %d "),aid,pMac->lim.gLimNumOfCurrentSTAs,pMac->lim.freeAidHead,pMac->lim.freeAidTail);)

}
