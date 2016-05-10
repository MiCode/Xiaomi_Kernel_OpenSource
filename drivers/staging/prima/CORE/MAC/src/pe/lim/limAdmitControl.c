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
 * This file contains TSPEC and STA admit control related functions
 * NOTE: applies only to AP builds
 *
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#include "limDebug.h"
#include "sysDef.h"
#include "limApi.h"
#include "cfgApi.h" // wlan_cfgGetInt()
#include "limTrace.h"
#include "limSendSmeRspMessages.h"
#include "limTypes.h"


#define ADMIT_CONTROL_LOGLEVEL        LOG1
#define ADMIT_CONTROL_POLICY_LOGLEVEL LOG1
#define ADMIT_CONTROL_MIN_INTERVAL    1000 // min acceptable service interval 1mSec

/* total available bandwidth in bps in each phy mode
 * these should be defined in hal or dph - replace these later
 */
#define LIM_TOTAL_BW_11A   54000000
#define LIM_MIN_BW_11A     6000000
#define LIM_TOTAL_BW_11B   11000000
#define LIM_MIN_BW_11B     1000000
#define LIM_TOTAL_BW_11G   LIM_TOTAL_BW_11A
#define LIM_MIN_BW_11G     LIM_MIN_BW_11B

// conversion factors
#define LIM_CONVERT_SIZE_BITS(numBytes) ((numBytes) * 8)
#define LIM_CONVERT_RATE_MBPS(rate)     ((rate)/1000000)

/* ANI sta's support enhanced rates, so the effective medium time used is
 * half that of other stations. This is the same as if they were requesting
 * half the badnwidth - so we adjust ANI sta's accordingly for bandwidth
 * calculations. Also enhanced rates apply only in case of non 11B mode.
 */
#define LIM_STA_BW_ADJUST(aniPeer, phyMode, bw) \
            (((aniPeer) && ((phyMode) != WNI_CFG_PHY_MODE_11B)) \
              ?   ((bw)/2) : (bw))


//------------------------------------------------------------------------------
// local protos

static tSirRetStatus
limCalculateSvcInt(tpAniSirGlobal, tSirMacTspecIE *, tANI_U32 *);
#if 0 //only EDCA is supported now
static tSirRetStatus
limValidateTspecHcca(tpAniSirGlobal, tSirMacTspecIE *);
#endif
static tSirRetStatus
limValidateTspecEdca(tpAniSirGlobal, tSirMacTspecIE *, tpPESession);
static tSirRetStatus
limValidateTspec(tpAniSirGlobal, tSirMacTspecIE *, tpPESession);
static void
limComputeMeanBwUsed(tpAniSirGlobal, tANI_U32 *, tANI_U32, tpLimTspecInfo, tpPESession);
static void
limGetAvailableBw(tpAniSirGlobal, tANI_U32 *, tANI_U32 *, tANI_U32, tANI_U32);
static tSirRetStatus
limAdmitPolicyOversubscription(tpAniSirGlobal, tSirMacTspecIE *, tpLimAdmitPolicyInfo, tpLimTspecInfo, tpPESession);
static tSirRetStatus
limTspecFindByStaAddr(tpAniSirGlobal, tANI_U8 *, tSirMacTspecIE*, tpLimTspecInfo, tpLimTspecInfo *);
static tSirRetStatus
limValidateAccessPolicy(tpAniSirGlobal, tANI_U8, tANI_U16, tpPESession);


/** -------------------------------------------------------------
\fn limCalculateSvcInt
\brief TSPEC validation and servcie interval determination
\param     tpAniSirGlobal    pMac
\param         tSirMacTspecIE *pTspec
\param         tANI_U32            *pSvcInt
\return eSirRetStatus - status of the comparison
  -------------------------------------------------------------*/

static tSirRetStatus
limCalculateSvcInt(
    tpAniSirGlobal  pMac,
    tSirMacTspecIE *pTspec,
    tANI_U32            *pSvcInt)
{
    tANI_U32 msduSz, dataRate;
    *pSvcInt = 0;

    // if a service interval is already specified, we are done
    if ((pTspec->minSvcInterval != 0) || (pTspec->maxSvcInterval != 0))
    {
        *pSvcInt = (pTspec->maxSvcInterval != 0)
                    ? pTspec->maxSvcInterval : pTspec->minSvcInterval;
        return eSIR_SUCCESS;
    }
    
    /* Masking off the fixed bits according to definition of MSDU size
     * in IEEE 802.11-2007 spec (section 7.3.2.30). Nominal MSDU size
     * is defined as:  Bit[0:14]=Size, Bit[15]=Fixed
     */
    if (pTspec->nomMsduSz != 0) 
        msduSz = (pTspec->nomMsduSz & 0x7fff);
    else if (pTspec->maxMsduSz != 0) 
        msduSz = pTspec->maxMsduSz;
    else
    {
        PELOGE(limLog(pMac, LOGE, FL("MsduSize not specified"));)
        return eSIR_FAILURE;
    }

    /* need to calculate a reasonable service interval
     * this is simply the msduSz/meanDataRate
     */
    if      (pTspec->meanDataRate != 0) dataRate = pTspec->meanDataRate;
    else if (pTspec->peakDataRate != 0) dataRate = pTspec->peakDataRate;
    else if (pTspec->minDataRate  != 0) dataRate = pTspec->minDataRate;
    else
    {
        PELOGE(limLog(pMac, LOGE, FL("DataRate not specified"));)
        return eSIR_FAILURE;
    }

    *pSvcInt = LIM_CONVERT_SIZE_BITS(msduSz) / LIM_CONVERT_RATE_MBPS(dataRate);
    return eSIR_FAILURE;
}

#if 0 //only EDCA is supported now
/** -------------------------------------------------------------
\fn limValidateTspecHcca
\brief  validate the parameters in the hcca tspec
         mandatory fields are derived from 11e Annex I (Table I.1)
\param   tpAniSirGlobal pMac
\param       tSirMacTspecIE *pTspec
\return eSirRetStatus - status
  -------------------------------------------------------------*/
static tSirRetStatus
limValidateTspecHcca(
    tpAniSirGlobal  pMac,
    tSirMacTspecIE *pTspec)
{
    tANI_U32 maxPhyRate, minPhyRate;
    tANI_U32 phyMode;

    tSirRetStatus retval = eSIR_SUCCESS;
    /* make sure a TSID is being requested */
    if (pTspec->tsinfo.traffic.tsid < SIR_MAC_HCCA_TSID_MIN)
    {
        limLog(pMac, LOGW, FL("tsid %d must be >%d)"),
               pTspec->tsinfo.traffic.tsid, SIR_MAC_HCCA_TSID_MIN);
        retval =  eSIR_FAILURE;
    }
    /*
     * With Polaris, there is a limitation in that the tsid cannot be arbitary
     * but is based on the qid. Thus, we cannot have a tspec which requests
     * a tsid of 13 and userPrio of 7, the bottom three bits of the tsid must
     * correspond to the userPrio
     */
    if (pTspec->tsinfo.traffic.userPrio !=
        (pTspec->tsinfo.traffic.tsid - SIR_MAC_HCCA_TSID_MIN))
    {
        limLog(pMac, LOGE, FL("TSid=0x%x, userPrio=%d: is not allowed"),
               pTspec->tsinfo.traffic.tsid, pTspec->tsinfo.traffic.userPrio);
        retval = eSIR_FAILURE;
    }
    // an inactivity interval is mandatory
    if (pTspec->inactInterval == 0)
    {
        PELOGW(limLog(pMac, LOGW, FL("inactInterval unspecified!"));)
        retval =  eSIR_FAILURE;
    }
    // surplus BW must be specified if a delay Bound is specified
    if ((pTspec->delayBound != 0) && (pTspec->surplusBw == 0))
    {
        limLog(pMac, LOGW, FL("delayBound %d, but surplusBw unspecified!"),
               pTspec->delayBound);
        retval =  eSIR_FAILURE;
    }
    // minPhyRate must always be specified and cannot exceed maximum supported
    limGetPhyMode(pMac, &phyMode);
    //limGetAvailableBw(pMac, &maxPhyRate, &minPhyRate, pMac->dph.gDphPhyMode,
    //                  1 /* bandwidth mult factor */);
    limGetAvailableBw(pMac, &maxPhyRate, &minPhyRate, phyMode,
                      1 /* bandwidth mult factor */);
    if ((pTspec->minPhyRate == 0)
        || (pTspec->minPhyRate > maxPhyRate)
        || (pTspec->minPhyRate < minPhyRate))
    {
        limLog(pMac, LOGW, FL("minPhyRate (%d) invalid"),
               pTspec->minPhyRate);
        retval =  eSIR_FAILURE;
    }
    /* NOTE: we will require all Tspec's to specify a mean data rate (and so
     * also the min and peak data rates)
     */
    if ((pTspec->minDataRate  == 0) ||
        (pTspec->meanDataRate == 0) ||
        (pTspec->peakDataRate == 0))
    {
        limLog(pMac, LOGW, FL("DataRate must be specified (min %d, mean %d, peak %d)"),
               pTspec->minDataRate, pTspec->meanDataRate, pTspec->peakDataRate);
        retval =  eSIR_FAILURE;
    }

    // mean data rate can't be more than the min phy rate
    if (pTspec->meanDataRate > pTspec->minPhyRate)
    {
        limLog(pMac, LOGW, FL("Data rate (%d) is more than Phyrate %d"),
               pTspec->meanDataRate, pTspec->minPhyRate);
        return eSIR_FAILURE;
    }

    /* if the tspec specifies a service interval, we won't accept tspec's
     * with service interval less than our allowed minimum, also either both
     * min and max must be specified or neither should be specified (in which
     * case, HC determines the appropriate service interval
     */
    if ((pTspec->minSvcInterval != 0) || (pTspec->maxSvcInterval != 0))
    {
        // max < min is ridiculous
        if (pTspec->maxSvcInterval < pTspec->minSvcInterval)
        {
            limLog(pMac, LOGW, FL("maxSvcInt %d  > minSvcInterval %d!!"),
                   pTspec->maxSvcInterval, pTspec->minSvcInterval);
            retval =  eSIR_FAILURE;
        }
        if (pTspec->maxSvcInterval < ADMIT_CONTROL_MIN_INTERVAL)
        {
            limLog(pMac, LOGW, FL("maxSvcInt %d must be >%d"),
                   pTspec->maxSvcInterval, ADMIT_CONTROL_MIN_INTERVAL);
            retval =  eSIR_FAILURE;
        }
    }
    else // min and max both unspecified
    {
        /* no service interval is specified, so make sure the parameters
         * needed to determine one are specified in the tspec
         * minPhyRate, meanDataRate and nomMsduSz are needed, only nomMsduSz
         * must be checked here since the other two are already validated
         */
         if (pTspec->nomMsduSz == 0)
         {
             PELOGW(limLog(pMac, LOGW, FL("No svcInt and no MsduSize specified"));)
             retval = eSIR_FAILURE;
         }
    }

    limLog(pMac, ADMIT_CONTROL_LOGLEVEL, FL("return status %d"), retval);
    return retval;
}

#endif //only edca is supported now.

/** -------------------------------------------------------------
\fn limValidateTspecEdca
\brief validate the parameters in the edca tspec
         mandatory fields are derived from 11e Annex I (Table I.1)
\param   tpAniSirGlobal pMac
\param        tSirMacTspecIE *pTspec
\return eSirRetStatus - status
  -------------------------------------------------------------*/
static tSirRetStatus
limValidateTspecEdca(
    tpAniSirGlobal  pMac,
    tSirMacTspecIE *pTspec,
    tpPESession  psessionEntry)
{
    tANI_U32           maxPhyRate, minPhyRate;
    tANI_U32 phyMode;
    tSirRetStatus retval = eSIR_SUCCESS;

    limGetPhyMode(pMac, &phyMode, psessionEntry);

    //limGetAvailableBw(pMac, &maxPhyRate, &minPhyRate, pMac->dph.gDphPhyMode,
    //                  1 /* bandwidth mult factor */);
    limGetAvailableBw(pMac, &maxPhyRate, &minPhyRate, phyMode,
                      1 /* bandwidth mult factor */);
    // mandatory fields are derived from 11e Annex I (Table I.1)
    if ((pTspec->nomMsduSz    == 0) ||
        (pTspec->meanDataRate == 0) ||
        (pTspec->surplusBw    == 0) ||
        (pTspec->minPhyRate   == 0) ||
        (pTspec->minPhyRate   > maxPhyRate))
    {
        limLog(pMac, LOGW, FL("Invalid EDCA Tspec: NomMsdu %d, meanDataRate %d, surplusBw %d, minPhyRate %d"),
               pTspec->nomMsduSz, pTspec->meanDataRate, pTspec->surplusBw, pTspec->minPhyRate);
        retval = eSIR_FAILURE;
    }

    limLog(pMac, ADMIT_CONTROL_LOGLEVEL, FL("return status %d"), retval);
    return retval;
}

/** -------------------------------------------------------------
\fn limValidateTspec
\brief validate the offered tspec
\param   tpAniSirGlobal pMac
\param         tSirMacTspecIE *pTspec
\return eSirRetStatus - status
  -------------------------------------------------------------*/

static tSirRetStatus
limValidateTspec(
    tpAniSirGlobal  pMac,
    tSirMacTspecIE *pTspec,
     tpPESession psessionEntry)
{
    tSirRetStatus retval = eSIR_SUCCESS;
    switch (pTspec->tsinfo.traffic.accessPolicy)
    {
        case SIR_MAC_ACCESSPOLICY_EDCA:
            if ((retval = limValidateTspecEdca(pMac, pTspec, psessionEntry)) != eSIR_SUCCESS)
                PELOGW(limLog(pMac, LOGW, FL("EDCA tspec invalid"));)
            break;

        case SIR_MAC_ACCESSPOLICY_HCCA:
#if 0 //Not supported right now.    
            if ((retval = limValidateTspecHcca(pMac, pTspec)) != eSIR_SUCCESS)
                PELOGW(limLog(pMac, LOGW, FL("HCCA tspec invalid"));)
            break;
#endif
       case SIR_MAC_ACCESSPOLICY_BOTH:
         // TBD: should we support hybrid tspec as well?? for now, just fall through
        default:
            limLog(pMac, LOGW, FL("AccessType %d not supported"),
                   pTspec->tsinfo.traffic.accessPolicy);
            retval = eSIR_FAILURE;
            break;
    }
    return retval;
}

//-----------------------------------------------------------------------------
// Admit Control Policy


/** -------------------------------------------------------------
\fn limComputeMeanBwUsed
\brief determime the used/allocated bandwidth
\param   tpAniSirGlobal pMac
\param       tANI_U32              *pBw
\param       tANI_U32               phyMode
\param       tpLimTspecInfo    pTspecInfo
\return eSirRetStatus - status
  -------------------------------------------------------------*/

static void
limComputeMeanBwUsed(
    tpAniSirGlobal    pMac,
    tANI_U32              *pBw,
    tANI_U32               phyMode,
    tpLimTspecInfo    pTspecInfo,
    tpPESession psessionEntry)
{
    tANI_U32 ctspec;
    *pBw = 0;
    for (ctspec = 0; ctspec < LIM_NUM_TSPEC_MAX; ctspec++, pTspecInfo++)
    {
        if (pTspecInfo->inuse)
        {
            tpDphHashNode pSta = dphGetHashEntry(pMac, pTspecInfo->assocId, &psessionEntry->dph.dphHashTable);
            if (pSta == NULL)
            {
                // maybe we should delete the tspec??
                limLog(pMac, LOGE, FL("Tspec %d (assocId %d): dphNode not found"),
                       ctspec, pTspecInfo->assocId);
                continue;
            }
            //FIXME: need to take care of taurusPeer, titanPeer, 11npeer too.
            *pBw += LIM_STA_BW_ADJUST(pSta->aniPeer, phyMode, pTspecInfo->tspec.meanDataRate);
        }
    }
}

/** -------------------------------------------------------------
\fn limGetAvailableBw
\brief based on the phy mode and the bw_factor, determine the total bandwidth that
       can be supported
\param   tpAniSirGlobal pMac
\param       tANI_U32              *pMaxBw
\param       tANI_U32              *pMinBw
\param       tANI_U32               phyMode
\param       tANI_U32               bw_factor
\return eSirRetStatus - status
  -------------------------------------------------------------*/

static void
limGetAvailableBw(
    tpAniSirGlobal    pMac,
    tANI_U32              *pMaxBw,
    tANI_U32              *pMinBw,
    tANI_U32               phyMode,
    tANI_U32               bw_factor)
{
    switch (phyMode)
    {
        case WNI_CFG_PHY_MODE_11B:
            *pMaxBw = LIM_TOTAL_BW_11B;
            *pMinBw = LIM_MIN_BW_11B;
            break;

        case WNI_CFG_PHY_MODE_11A:
            *pMaxBw = LIM_TOTAL_BW_11A;
            *pMinBw = LIM_MIN_BW_11A;
            break;

        case WNI_CFG_PHY_MODE_11G:
        case WNI_CFG_PHY_MODE_NONE:
        default:
            *pMaxBw = LIM_TOTAL_BW_11G;
            *pMinBw = LIM_MIN_BW_11G;
            break;
    }
    *pMaxBw *= bw_factor;
}

/** -------------------------------------------------------------
\fn limAdmitPolicyOversubscription
\brief simple admission control policy based on oversubscription
         if the total bandwidth of all admitted tspec's exceeds (factor * phy-bw) then
         reject the tspec, else admit it. The phy-bw is the peak available bw in the
         current phy mode. The 'factor' is the configured oversubscription factor.
\param   tpAniSirGlobal pMac
\param       tSirMacTspecIE       *pTspec
\param       tpLimAdmitPolicyInfo  pAdmitPolicy
\param       tpLimTspecInfo        pTspecInfo
\return eSirRetStatus - status
  -------------------------------------------------------------*/

/*
 * simple admission control policy based on oversubscription
 * if the total bandwidth of all admitted tspec's exceeds (factor * phy-bw) then
 * reject the tspec, else admit it. The phy-bw is the peak available bw in the
 * current phy mode. The 'factor' is the configured oversubscription factor.
 */
static tSirRetStatus
limAdmitPolicyOversubscription(
    tpAniSirGlobal        pMac,
    tSirMacTspecIE       *pTspec,
    tpLimAdmitPolicyInfo  pAdmitPolicy,
    tpLimTspecInfo        pTspecInfo,
    tpPESession psessionEntry)
{
    tANI_U32 totalbw, minbw, usedbw;
    tANI_U32 phyMode;

    // determine total bandwidth used so far
    limGetPhyMode(pMac, &phyMode, psessionEntry);

    //limComputeMeanBwUsed(pMac, &usedbw, pMac->dph.gDphPhyMode, pTspecInfo);
    limComputeMeanBwUsed(pMac, &usedbw, phyMode, pTspecInfo, psessionEntry);

    // determine how much bandwidth is available based on the current phy mode
    //limGetAvailableBw(pMac, &totalbw, &minbw, pMac->dph.gDphPhyMode, pAdmitPolicy->bw_factor);
    limGetAvailableBw(pMac, &totalbw, &minbw, phyMode, pAdmitPolicy->bw_factor);

    if (usedbw > totalbw) // this can't possibly happen
        return eSIR_FAILURE;

    if ((totalbw - usedbw) < pTspec->meanDataRate)
    {
        limLog(pMac, ADMIT_CONTROL_POLICY_LOGLEVEL,
               FL("Total BW %d, Used %d, Tspec request %d not possible"),
               totalbw, usedbw, pTspec->meanDataRate);
        return eSIR_FAILURE;
    }
    return eSIR_SUCCESS;
}

/** -------------------------------------------------------------
\fn limAdmitPolicy
\brief determine the current admit control policy and apply it for the offered tspec
\param   tpAniSirGlobal pMac
\param         tSirMacTspecIE   *pTspec
\return eSirRetStatus - status
  -------------------------------------------------------------*/

tSirRetStatus limAdmitPolicy(
    tpAniSirGlobal    pMac,
    tSirMacTspecIE   *pTspec,
    tpPESession psessionEntry)
{
    tSirRetStatus retval = eSIR_FAILURE;
    tpLimAdmitPolicyInfo pAdmitPolicy = &pMac->lim.admitPolicyInfo;

    switch (pAdmitPolicy->type)
    {
        case WNI_CFG_ADMIT_POLICY_ADMIT_ALL:
            retval = eSIR_SUCCESS;
            break;

        case WNI_CFG_ADMIT_POLICY_BW_FACTOR:
            retval = limAdmitPolicyOversubscription(pMac, pTspec,
                        &pMac->lim.admitPolicyInfo, &pMac->lim.tspecInfo[0], psessionEntry);
            if (retval != eSIR_SUCCESS)
                PELOGE(limLog(pMac, LOGE, FL("rejected by BWFactor policy"));)
            break;

        case WNI_CFG_ADMIT_POLICY_REJECT_ALL:
            retval = eSIR_FAILURE;
            break;

        default:
            retval = eSIR_SUCCESS;
            limLog(pMac, LOGE, FL("Admit Policy %d unknown, admitting all traffic"),
                   pAdmitPolicy->type);
            break;
    }
    return retval;
}

/** -------------------------------------------------------------
\fn limTspecDelete
\brief delete the specified tspec
\param   tpAniSirGlobal pMac
\param     tpLimTspecInfo pInfo
\return eSirRetStatus - status
  -------------------------------------------------------------*/

//-----------------------------------------------------------------------------
// delete the specified tspec
void limTspecDelete(tpAniSirGlobal pMac, tpLimTspecInfo pInfo)
{
    if (pInfo == NULL)
        return;
        //pierre
    limLog(pMac, ADMIT_CONTROL_LOGLEVEL, FL("tspec entry = %d"), pInfo->idx);
    limLog(pMac, ADMIT_CONTROL_LOGLEVEL, FL("delete tspec %08X"),pInfo);
    pInfo->inuse = 0;

    // clear the hcca/parameterized queue indicator
#if 0
    if ((pInfo->tspec.tsinfo.traffic.direction == SIR_MAC_DIRECTION_UPLINK) ||
        (pInfo->tspec.tsinfo.traffic.direction == SIR_MAC_DIRECTION_BIDIR))
        queue[pInfo->staid][pInfo->tspec.tsinfo.traffic.userPrio][SCH_UL_QUEUE].ts = 0;
#endif

    return;
}

/** -------------------------------------------------------------
\fn limTspecFindByStaAddr
\brief Send halMsg_AddTs to HAL
\param   tpAniSirGlobal pMac
\param   \param       tANI_U8               *pAddr
\param       tSirMacTspecIE    *pTspecIE
\param       tpLimTspecInfo    pTspecList
\param       tpLimTspecInfo   *ppInfo
\return eSirRetStatus - status
  -------------------------------------------------------------*/

// find the specified tspec in the list
static tSirRetStatus
limTspecFindByStaAddr(
    tpAniSirGlobal    pMac,
    tANI_U8               *pAddr,
    tSirMacTspecIE    *pTspecIE,
    tpLimTspecInfo    pTspecList,
    tpLimTspecInfo   *ppInfo)
{
    int ctspec;

    *ppInfo = NULL;

    for (ctspec = 0; ctspec < LIM_NUM_TSPEC_MAX; ctspec++, pTspecList++)
    {
        if ((pTspecList->inuse)
            && (vos_mem_compare(pAddr, pTspecList->staAddr, sizeof(pTspecList->staAddr)))
            && (vos_mem_compare((tANI_U8 *) pTspecIE, (tANI_U8 *) &pTspecList->tspec,
                                            sizeof(tSirMacTspecIE))))
        {
            *ppInfo = pTspecList;
            return eSIR_SUCCESS;
        }
    }
    return eSIR_FAILURE;
}

/** -------------------------------------------------------------
\fn limTspecFindByAssocId
\brief find tspec with matchin staid and Tspec 
\param   tpAniSirGlobal pMac
\param       tANI_U32               staid
\param       tSirMacTspecIE    *pTspecIE
\param       tpLimTspecInfo    pTspecList
\param       tpLimTspecInfo   *ppInfo
\return eSirRetStatus - status
  -------------------------------------------------------------*/

tSirRetStatus
limTspecFindByAssocId(
    tpAniSirGlobal    pMac,
    tANI_U16               assocId,
    tSirMacTspecIE *pTspecIE,
    tpLimTspecInfo    pTspecList,
    tpLimTspecInfo   *ppInfo)
{
    int ctspec;

    *ppInfo = NULL;

    limLog(pMac, ADMIT_CONTROL_LOGLEVEL, FL("Trying to find tspec entry for assocId = %d"), assocId);
    limLog(pMac, ADMIT_CONTROL_LOGLEVEL, FL("pTsInfo->traffic.direction = %d, pTsInfo->traffic.tsid = %d"),
                pTspecIE->tsinfo.traffic.direction, pTspecIE->tsinfo.traffic.tsid);

    for (ctspec = 0; ctspec < LIM_NUM_TSPEC_MAX; ctspec++, pTspecList++)
    {
        if ((pTspecList->inuse)
            && (assocId == pTspecList->assocId)
            && (vos_mem_compare((tANI_U8 *)pTspecIE, (tANI_U8 *)&pTspecList->tspec,
                sizeof(tSirMacTspecIE))))
        {
            *ppInfo = pTspecList;
            return eSIR_SUCCESS;
        }
    }
    return eSIR_FAILURE;
}

/** -------------------------------------------------------------
\fn limFindTspec
\brief finding a TSPEC entry with assocId, tsinfo.direction and tsinfo.tsid
\param    tANI_U16               assocId
\param     tpAniSirGlobal    pMac
\param     tSirMacTSInfo   *pTsInfo
\param         tpLimTspecInfo    pTspecList
\param         tpLimTspecInfo   *ppInfo
\return eSirRetStatus - status of the comparison
  -------------------------------------------------------------*/

tSirRetStatus
limFindTspec(
    tpAniSirGlobal    pMac,
    tANI_U16               assocId,    
    tSirMacTSInfo   *pTsInfo,
    tpLimTspecInfo    pTspecList,
    tpLimTspecInfo   *ppInfo)
{
    int ctspec;

    *ppInfo = NULL;

    limLog(pMac, ADMIT_CONTROL_LOGLEVEL, FL("Trying to find tspec entry for assocId = %d"), assocId);
    limLog(pMac, ADMIT_CONTROL_LOGLEVEL, FL("pTsInfo->traffic.direction = %d, pTsInfo->traffic.tsid = %d"),
                pTsInfo->traffic.direction, pTsInfo->traffic.tsid);

    for (ctspec = 0; ctspec < LIM_NUM_TSPEC_MAX; ctspec++, pTspecList++)
    {
        if ((pTspecList->inuse)
            && (assocId == pTspecList->assocId)
            && (pTsInfo->traffic.direction == pTspecList->tspec.tsinfo.traffic.direction)
            && (pTsInfo->traffic.tsid == pTspecList->tspec.tsinfo.traffic.tsid))
        {
            *ppInfo = pTspecList;
            return eSIR_SUCCESS;
        }
    }
    return eSIR_FAILURE;
}

/** -------------------------------------------------------------
\fn limTspecAdd
\brief add or update the specified tspec to the tspec list
\param tpAniSirGlobal    pMac
\param tANI_U8               *pAddr
\param tANI_U16               assocId
\param tSirMacTspecIE   *pTspec
\param tANI_U32               interval
\param tpLimTspecInfo   *ppInfo

\return eSirRetStatus - status of the comparison
  -------------------------------------------------------------*/

tSirRetStatus limTspecAdd(
    tpAniSirGlobal    pMac,
    tANI_U8           *pAddr,
    tANI_U16          assocId,
    tSirMacTspecIE    *pTspec,
    tANI_U32          interval,
    tpLimTspecInfo    *ppInfo)
{
    tpLimTspecInfo pTspecList = &pMac->lim.tspecInfo[0];
    *ppInfo = NULL;    

    // validate the assocId
    if (assocId >= pMac->lim.maxStation)
    {
        PELOGE(limLog(pMac, LOGE, FL("Invalid assocId 0x%x"), assocId);)
        return eSIR_FAILURE;
    }

    //decide whether to add/update
    {
      *ppInfo = NULL;

      if(eSIR_SUCCESS == limFindTspec(pMac, assocId, &pTspec->tsinfo, pTspecList, ppInfo))
      {
            //update this entry.
            limLog(pMac, ADMIT_CONTROL_LOGLEVEL, FL("updating TSPEC table entry = %d"),
                        (*ppInfo)->idx);
      }
      else
      {
          /* We didn't find one to update. So find a free slot in the 
           * LIM TSPEC list and add this new entry
           */ 
          tANI_U8 ctspec = 0;
          for (ctspec = 0 , pTspecList = &pMac->lim.tspecInfo[0]; ctspec < LIM_NUM_TSPEC_MAX; ctspec++, pTspecList++)
          {
              if (! pTspecList->inuse)
              {
                  limLog(pMac, LOG1, FL("Found free slot in TSPEC list. Add to TSPEC table entry %d"), ctspec);
                  break;
              }
          }

          if (ctspec >= LIM_NUM_TSPEC_MAX)
              return eSIR_FAILURE;

          //Record the new index entry 
          pTspecList->idx = ctspec;
      }
    }

    // update the tspec info
    pTspecList->tspec = *pTspec;
    pTspecList->assocId = assocId;
    vos_mem_copy(pTspecList->staAddr, pAddr, sizeof(pTspecList->staAddr));

    // for edca tspec's, we are all done
    if (pTspec->tsinfo.traffic.accessPolicy == SIR_MAC_ACCESSPOLICY_EDCA)
    {
        pTspecList->inuse = 1;
        *ppInfo = pTspecList;
        limLog(pMac, ADMIT_CONTROL_LOGLEVEL, FL("added entry for EDCA AccessPolicy"));
        return eSIR_SUCCESS;
    }

    /*
     * for hcca tspec's, must set the parameterized bit in the queues
     * the 'ts' bit in the queue data structure indicates that the queue is
     * parameterized (hcca). When the schedule is written this bit is used
     * in the tsid field (bit 3) and the other three bits (0-2) are simply
     * filled in as the user priority (or qid). This applies only to uplink
     * polls where the qos control field must contain the tsid specified in the
     * tspec.
     */
#if 0
    if ((pTspec->tsinfo.traffic.direction == SIR_MAC_DIRECTION_UPLINK) ||
        (pTspec->tsinfo.traffic.direction == SIR_MAC_DIRECTION_BIDIR))
        queue[staid][pTspec->tsinfo.traffic.userPrio][SCH_UL_QUEUE].ts = 1;
#endif
    pTspecList->inuse = 1;
    *ppInfo = pTspecList;
    limLog(pMac, ADMIT_CONTROL_LOGLEVEL, FL("added entry for HCCA AccessPolicy"));
    return eSIR_SUCCESS;
}

/** -------------------------------------------------------------
\fn limValidateAccessPolicy
\brief Validates Access policy
\param   tpAniSirGlobal pMac
\param       tANI_U8              accessPolicy
\param       tANI_U16             assocId
\return eSirRetStatus - status
  -------------------------------------------------------------*/

static tSirRetStatus
limValidateAccessPolicy(
    tpAniSirGlobal  pMac,
    tANI_U8              accessPolicy,
    tANI_U16              assocId,
    tpPESession psessionEntry)
{
    tSirRetStatus retval = eSIR_FAILURE;
    tpDphHashNode pSta = dphGetHashEntry(pMac, assocId, &psessionEntry->dph.dphHashTable);

    if ((pSta == NULL) || (! pSta->valid))
    {
        PELOGE(limLog(pMac, LOGE, FL("invalid station address passed"));)
        return eSIR_FAILURE;
    }

    switch (accessPolicy)
    {
        case SIR_MAC_ACCESSPOLICY_EDCA:
            if (pSta->wmeEnabled || pSta->lleEnabled)
                retval = eSIR_SUCCESS;
            break;

        case SIR_MAC_ACCESSPOLICY_HCCA:
        case SIR_MAC_ACCESSPOLICY_BOTH:
#if 0 //only EDCA supported for now.          
            // TBD: check wsm doesn't support the hybrid access policy
            if (pSta->wsmEnabled || pSta->lleEnabled)
                retval = eSIR_SUCCESS;
            break;
#endif  //only EDCA supported for now.
        default:
            PELOGE(limLog(pMac, LOGE, FL("Invalid accessPolicy %d"), accessPolicy);)
            break;
    }

    if (retval != eSIR_SUCCESS)
        limLog(pMac, LOGW, FL("failed (accPol %d, staId %d, lle %d, wme %d, wsm %d)"),
               accessPolicy, pSta->staIndex, pSta->lleEnabled, pSta->wmeEnabled, pSta->wsmEnabled);

    return retval;
}

/** -------------------------------------------------------------
\fn limAdmitControlAddTS
\brief Determine if STA with the specified TSPEC can be admitted. If it can,
     a schedule element is provided
\param   tpAniSirGlobal pMac
\param       tANI_U8                     *pAddr,
\param       tSirAddtsReqInfo       *pAddts,
\param       tSirMacQosCapabilityIE *pQos,
\param       tANI_U16                     assocId, // assocId, valid only if alloc==true
\param       tANI_U8                    alloc, // true=>allocate bw for this tspec,
                                   // else determine only if space is available
\param       tSirMacScheduleIE      *pSch,
\param       tANI_U8                   *pTspecIdx //index to the lim tspec table.
\return eSirRetStatus - status
  -------------------------------------------------------------*/

tSirRetStatus limAdmitControlAddTS(
    tpAniSirGlobal          pMac,
    tANI_U8                     *pAddr,
    tSirAddtsReqInfo       *pAddts,
    tSirMacQosCapabilityStaIE *pQos,
    tANI_U16                     assocId, // assocId, valid only if alloc==true
    tANI_U8                    alloc, // true=>allocate bw for this tspec,
                                   // else determine only if space is available
    tSirMacScheduleIE      *pSch,
    tANI_U8                   *pTspecIdx, //index to the lim tspec table.
    tpPESession psessionEntry
    )
{
    tpLimTspecInfo pTspecInfo;
    tSirRetStatus  retval;
    tANI_U32            svcInterval;
    (void) pQos;

    // TBD: modify tspec as needed
    // EDCA: need to fill in the medium time and the minimum phy rate
    // to be consistent with the desired traffic parameters.

    limLog(pMac, ADMIT_CONTROL_LOGLEVEL, FL("tsid %d, directn %d, start %d, intvl %d, accPolicy %d, up %d"),
           pAddts->tspec.tsinfo.traffic.tsid, pAddts->tspec.tsinfo.traffic.direction,
           pAddts->tspec.svcStartTime, pAddts->tspec.minSvcInterval,
           pAddts->tspec.tsinfo.traffic.accessPolicy, pAddts->tspec.tsinfo.traffic.userPrio);

    // check for duplicate tspec
    retval = (alloc)
              ? limTspecFindByAssocId(pMac, assocId, &pAddts->tspec, &pMac->lim.tspecInfo[0], &pTspecInfo)
              : limTspecFindByStaAddr(pMac, pAddr, &pAddts->tspec, &pMac->lim.tspecInfo[0], &pTspecInfo);

    if (retval == eSIR_SUCCESS)
    {
        limLog(pMac, ADMIT_CONTROL_LOGLEVEL, FL("duplicate tspec (index %d)!"), pTspecInfo->idx);
        return eSIR_FAILURE;
    }

    // check that the tspec's are well formed and acceptable
    if (limValidateTspec(pMac, &pAddts->tspec, psessionEntry) != eSIR_SUCCESS)
    {
        PELOGW(limLog(pMac, LOGW, FL("tspec validation failed"));)
        return eSIR_FAILURE;
    }

    // determine a service interval for the tspec
    if (limCalculateSvcInt(pMac, &pAddts->tspec, &svcInterval) != eSIR_SUCCESS)
    {
        PELOGW(limLog(pMac, LOGW, FL("SvcInt calculate failed"));)
        return eSIR_FAILURE;
    }

    // determine if the tspec can be admitted or not based on current policy
    if (limAdmitPolicy(pMac, &pAddts->tspec, psessionEntry) != eSIR_SUCCESS)
    {
        PELOGW(limLog(pMac, LOGW, FL("tspec rejected by admit control policy"));)
        return eSIR_FAILURE;
    }

    // fill in a schedule if requested
    if (pSch != NULL)
    {
        vos_mem_set((tANI_U8 *) pSch, sizeof(*pSch), 0);
        pSch->svcStartTime   = pAddts->tspec.svcStartTime;
        pSch->svcInterval    = svcInterval;
        pSch->maxSvcDuration = (tANI_U16) pSch->svcInterval; // use SP = SI
        pSch->specInterval   = 0x1000; // fixed for now: TBD

        pSch->info.direction   = pAddts->tspec.tsinfo.traffic.direction;
        pSch->info.tsid        = pAddts->tspec.tsinfo.traffic.tsid;
        pSch->info.aggregation = 0; // no support for aggregation for now: TBD
    }

    // if no allocation is requested, done
    if (! alloc)
        return eSIR_SUCCESS;

    // check that we are in the proper mode to deal with the tspec type
    if (limValidateAccessPolicy(pMac, (tANI_U8) pAddts->tspec.tsinfo.traffic.accessPolicy, assocId, psessionEntry) != eSIR_SUCCESS)
    {
        limLog(pMac, LOGW, FL("AccessPolicy %d is not valid in current mode"),
               pAddts->tspec.tsinfo.traffic.accessPolicy);
        return eSIR_FAILURE;
    }

    // add tspec to list
    if (limTspecAdd(pMac, pAddr, assocId, &pAddts->tspec, svcInterval, &pTspecInfo)
        != eSIR_SUCCESS)
    {
        PELOGE(limLog(pMac, LOGE, FL("no space in tspec list"));)
        return eSIR_FAILURE;
    }

    //passing lim tspec table index to the caller
    *pTspecIdx = pTspecInfo->idx;

    return eSIR_SUCCESS;
}

/** -------------------------------------------------------------
\fn limAdmitControlDeleteTS
\brief Delete the specified Tspec for the specified STA
\param   tpAniSirGlobal pMac
\param       tANI_U16               assocId
\param       tSirMacTSInfo    *pTsInfo
\param       tANI_U8               *pTsStatus
\param       tANI_U8             *ptspecIdx
\return eSirRetStatus - status
  -------------------------------------------------------------*/

tSirRetStatus
limAdmitControlDeleteTS(
    tpAniSirGlobal    pMac,
    tANI_U16               assocId,
    tSirMacTSInfo    *pTsInfo,
    tANI_U8               *pTsStatus,
    tANI_U8             *ptspecIdx)
{
    tpLimTspecInfo pTspecInfo = NULL;

    if (pTsStatus != NULL)
        *pTsStatus = 0;

    if (limFindTspec(pMac, assocId, pTsInfo, &pMac->lim.tspecInfo[0], &pTspecInfo) == eSIR_SUCCESS)
    {
        if(pTspecInfo != NULL)    
        {
          limLog(pMac, ADMIT_CONTROL_LOGLEVEL, FL("Tspec entry %d found"), pTspecInfo->idx);
        
          *ptspecIdx = pTspecInfo->idx;
          limTspecDelete(pMac, pTspecInfo);
          return eSIR_SUCCESS;
        }
    }
    return eSIR_FAILURE;
}

/** -------------------------------------------------------------
\fn limAdmitControlDeleteSta
\brief Delete all TSPEC for the specified STA
\param   tpAniSirGlobal pMac
\param     tANI_U16 assocId
\return eSirRetStatus - status
  -------------------------------------------------------------*/

tSirRetStatus
limAdmitControlDeleteSta(
    tpAniSirGlobal    pMac,
    tANI_U16 assocId)
{
    tpLimTspecInfo pTspecInfo = &pMac->lim.tspecInfo[0];
    int ctspec;

    for (ctspec = 0; ctspec < LIM_NUM_TSPEC_MAX; ctspec++, pTspecInfo++)
    {
        if (assocId == pTspecInfo->assocId)
        {
            limTspecDelete(pMac, pTspecInfo);
            limLog(pMac, ADMIT_CONTROL_LOGLEVEL, FL("Deleting TSPEC %d for assocId %d"),
                   ctspec, assocId);
        }
    }
    limLog(pMac, ADMIT_CONTROL_LOGLEVEL, FL("assocId %d done"), assocId);

    return eSIR_SUCCESS;
}

/** -------------------------------------------------------------
\fn limAdmitControlInit
\brief init tspec table
\param   tpAniSirGlobal pMac
\return eSirRetStatus - status
  -------------------------------------------------------------*/
tSirRetStatus limAdmitControlInit(tpAniSirGlobal pMac)
{
    vos_mem_set(pMac->lim.tspecInfo, LIM_NUM_TSPEC_MAX * sizeof(tLimTspecInfo), 0);
    return eSIR_SUCCESS;
}

/** -------------------------------------------------------------
\fn limUpdateAdmitPolicy
\brief Set the admit control policy based on CFG parameters
\param   tpAniSirGlobal pMac
\return eSirRetStatus - status
  -------------------------------------------------------------*/

tSirRetStatus limUpdateAdmitPolicy(tpAniSirGlobal    pMac)
{
    tANI_U32 val;
    if (wlan_cfgGetInt(pMac, WNI_CFG_ADMIT_POLICY, &val) != eSIR_SUCCESS)
    {
        limLog(pMac, LOGP, FL("Unable to get CFG_ADMIT_POLICY"));
        return eSIR_FAILURE;
    }
    pMac->lim.admitPolicyInfo.type = (tANI_U8) val;
    if (wlan_cfgGetInt(pMac, WNI_CFG_ADMIT_BWFACTOR, &val) != eSIR_SUCCESS)
    {
        limLog(pMac, LOGP, FL("Unable to get CFG_ADMIT_BWFACTOR"));
        return eSIR_FAILURE;
    }
    pMac->lim.admitPolicyInfo.bw_factor = (tANI_U8) val;

    PELOG1(limLog(pMac, LOG1, FL("LIM: AdmitPolicy %d, bw_factor %d"),
          pMac->lim.admitPolicyInfo.type, pMac->lim.admitPolicyInfo.bw_factor);)

    return eSIR_SUCCESS;
}


/** -------------------------------------------------------------
\fn limSendHalMsgAddTs
\brief Send halMsg_AddTs to HAL
\param   tpAniSirGlobal pMac
\param     tANI_U16        staIdx
\param     tANI_U8         tspecIdx
\param       tSirMacTspecIE tspecIE
\param       tSirTclasInfo   *tclasInfo
\param       tANI_U8           tclasProc
\return eSirRetStatus - status
  -------------------------------------------------------------*/

tSirRetStatus
limSendHalMsgAddTs(
  tpAniSirGlobal pMac,
  tANI_U16       staIdx,
  tANI_U8         tspecIdx,
  tSirMacTspecIE tspecIE,
  tANI_U8        sessionId)
{
    tSirMsgQ msg;
    tpAddTsParams pAddTsParam;

    pAddTsParam = vos_mem_malloc(sizeof(tAddTsParams));
    if (NULL == pAddTsParam)
    {
       PELOGW(limLog(pMac, LOGW, FL("AllocateMemory() failed"));)
       return eSIR_MEM_ALLOC_FAILED;          
    }

    vos_mem_set((tANI_U8 *)pAddTsParam, sizeof(tAddTsParams), 0);
    pAddTsParam->staIdx = staIdx;
    pAddTsParam->tspecIdx = tspecIdx;
    vos_mem_copy(&pAddTsParam->tspec, &tspecIE, sizeof(tSirMacTspecIE));
    pAddTsParam->sessionId = sessionId;
 
    msg.type = WDA_ADD_TS_REQ;
    msg.bodyptr = pAddTsParam;
    msg.bodyval = 0;

    /* We need to defer any incoming messages until we get a
     * WDA_ADD_TS_RSP from HAL.
     */
    SET_LIM_PROCESS_DEFD_MESGS(pMac, false);
    MTRACE(macTraceMsgTx(pMac, sessionId, msg.type));

    if(eSIR_SUCCESS != wdaPostCtrlMsg(pMac, &msg))
    {
       PELOGW(limLog(pMac, LOGW, FL("wdaPostCtrlMsg() failed"));)
       SET_LIM_PROCESS_DEFD_MESGS(pMac, true);
       vos_mem_free(pAddTsParam);
       return eSIR_FAILURE;
    }
  return eSIR_SUCCESS;
}

/** -------------------------------------------------------------
\fn limSendHalMsgDelTs
\brief Send halMsg_AddTs to HAL
\param   tpAniSirGlobal pMac
\param     tANI_U16        staIdx
\param     tANI_U8         tspecIdx
\param     tSirAddtsReqInfo addts
\return eSirRetStatus - status
  -------------------------------------------------------------*/

tSirRetStatus
limSendHalMsgDelTs(
  tpAniSirGlobal pMac,
  tANI_U16       staIdx,
  tANI_U8         tspecIdx,
  tSirDeltsReqInfo delts,
  tANI_U8        sessionId,
  tANI_U8        *bssId)
{
  tSirMsgQ msg;
  tpDelTsParams pDelTsParam;

  pDelTsParam = vos_mem_malloc(sizeof(tDelTsParams));
  if (NULL == pDelTsParam)
  {
     limLog(pMac, LOGP, FL("AllocateMemory() failed"));
     return eSIR_MEM_ALLOC_FAILED;
  }

  msg.type = WDA_DEL_TS_REQ;
  msg.bodyptr = pDelTsParam;
  msg.bodyval = 0;
  vos_mem_set((tANI_U8 *)pDelTsParam, sizeof(tDelTsParams), 0);

  //filling message parameters.
  pDelTsParam->staIdx = staIdx;
  pDelTsParam->tspecIdx = tspecIdx;
  vos_mem_copy(&pDelTsParam->bssId, bssId, sizeof(tSirMacAddr));

  PELOGW(limLog(pMac, LOGW, FL("calling wdaPostCtrlMsg()"));)
  MTRACE(macTraceMsgTx(pMac, sessionId, msg.type));

  if(eSIR_SUCCESS != wdaPostCtrlMsg(pMac, &msg))
  {
     PELOGW(limLog(pMac, LOGW, FL("wdaPostCtrlMsg() failed"));)
     vos_mem_free(pDelTsParam);
     return eSIR_FAILURE;
  }
  return eSIR_SUCCESS;  
}

/** -------------------------------------------------------------
\fn     limProcessHalAddTsRsp
\brief  This function process the WDA_ADD_TS_RSP from HAL. 
\       If response is successful, then send back SME_ADDTS_RSP.
\       Otherwise, send DELTS action frame to peer and then 
\       then send back SME_ADDTS_RSP. 
\
\param  tpAniSirGlobal  pMac
\param  tpSirMsgQ   limMsg
-------------------------------------------------------------*/
void limProcessHalAddTsRsp(tpAniSirGlobal pMac, tpSirMsgQ limMsg)
{
    tpAddTsParams  pAddTsRspMsg = NULL;
    tpDphHashNode  pSta = NULL;
    tANI_U16  assocId =0;
    tSirMacAddr  peerMacAddr;
    tANI_U8   rspReqd = 1;
    tpPESession  psessionEntry = NULL;


    /* Need to process all the deferred messages enqueued 
     * since sending the WDA_ADD_TS_REQ.
     */
    SET_LIM_PROCESS_DEFD_MESGS(pMac, true);

    if (NULL == limMsg->bodyptr)
    {
        limLog(pMac, LOGP, FL("Received WDA_ADD_TS_RSP with NULL "));
        goto end;
    }

    pAddTsRspMsg = (tpAddTsParams) (limMsg->bodyptr);

    // 090803: Use peFindSessionBySessionId() to obtain the PE session context       
    // from the sessionId in the Rsp Msg from HAL
    psessionEntry = peFindSessionBySessionId(pMac, pAddTsRspMsg->sessionId);

    if(psessionEntry == NULL)
    {
        PELOGE(limLog(pMac, LOGE,FL("Session does Not exist with given sessionId :%d "), pAddTsRspMsg->sessionId);)
        limSendSmeAddtsRsp(pMac, rspReqd, eSIR_SME_ADDTS_RSP_FAILED, psessionEntry, pAddTsRspMsg->tspec, 
              pMac->lim.gLimAddtsReq.sessionId, pMac->lim.gLimAddtsReq.transactionId);
        goto end;
    }

    if(pAddTsRspMsg->status == eHAL_STATUS_SUCCESS)
    {
        PELOG1(limLog(pMac, LOG1, FL("Received successful ADDTS response from HAL "));)
        // Use the smesessionId and smetransactionId from the PE session context
        limSendSmeAddtsRsp(pMac, rspReqd, eSIR_SME_SUCCESS, psessionEntry, pAddTsRspMsg->tspec,
                psessionEntry->smeSessionId, psessionEntry->transactionId);
        goto end;
    }
    else
    {
        PELOG1(limLog(pMac, LOG1, FL("Received failure ADDTS response from HAL "));)

        // Send DELTS action frame to AP        
        // 090803: Get peer MAC addr from session        
#if 0  
        cfgLen = sizeof(tSirMacAddr);
        if (wlan_cfgGetStr(pMac, WNI_CFG_BSSID, peerMacAddr, &cfgLen) != eSIR_SUCCESS)
        {
            limLog(pMac, LOGP, FL("Fail to retrieve BSSID "));
            goto end;
        }
#endif //TO SUPPORT BT-AMP
        sirCopyMacAddr(peerMacAddr,psessionEntry->bssId);

        // 090803: Add the SME Session ID        
        limSendDeltsReqActionFrame(pMac, peerMacAddr, rspReqd, &pAddTsRspMsg->tspec.tsinfo, &pAddTsRspMsg->tspec,
                //psessionEntry->smeSessionId);
                psessionEntry);

        // Delete TSPEC
        // 090803: Pull the hash table from the session        
        pSta = dphLookupAssocId(pMac, pAddTsRspMsg->staIdx, &assocId, 
                &psessionEntry->dph.dphHashTable);    
        if (pSta != NULL)
            limAdmitControlDeleteTS(pMac, assocId, &pAddTsRspMsg->tspec.tsinfo, NULL, (tANI_U8 *)&pAddTsRspMsg->tspecIdx);

        // Send SME_ADDTS_RSP
        // 090803: Use the smesessionId and smetransactionId from the PE session context
        limSendSmeAddtsRsp(pMac, rspReqd, eSIR_SME_ADDTS_RSP_FAILED, psessionEntry, pAddTsRspMsg->tspec,
                psessionEntry->smeSessionId, psessionEntry->transactionId);
        goto end;
   }

end:
    if( pAddTsRspMsg != NULL )
        vos_mem_free(pAddTsRspMsg);
    return;
}

