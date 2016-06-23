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




/*============================================================================
limLogDump.c

Implements the dump commands specific to the lim module. 

Copyright (c) 2007 QUALCOMM Incorporated.
All Rights Reserved.
Qualcomm Confidential and Proprietary
 ============================================================================*/

#include "vos_types.h"
#include "limApi.h"

#if defined(ANI_LOGDUMP)


#include "limUtils.h"
#include "limSecurityUtils.h"
#include "schApi.h"
#include "limSerDesUtils.h"
#include "limAssocUtils.h"
#include "limSendMessages.h"
#include "logDump.h"
#include "limTrace.h"
#if defined WLAN_FEATURE_VOWIFI
#include "rrmApi.h"
#endif
#if defined WLAN_FEATURE_VOWIFI_11R
#include <limFT.h>
#endif
#include "smeInside.h"
#include "wlan_qct_wda.h"
#include "wlan_qct_wdi_dts.h"

void WDA_TimerTrafficStatsInd(tWDA_CbContext *pWDA);
#ifdef WLANTL_DEBUG
extern void WLANTLPrintPktsRcvdPerRssi(v_PVOID_t pAdapter, v_U8_t staId, v_BOOL_t flush);
extern void WLANTLPrintPktsRcvdPerRateIdx(v_PVOID_t pAdapter, v_U8_t staId, v_BOOL_t flush);
#endif

static char *getRole( tLimSystemRole role )
{
  switch (role)
  {
    case eLIM_UNKNOWN_ROLE:
        return "eLIM_UNKNOWN_ROLE";
    case eLIM_AP_ROLE:
        return "eLIM_AP_ROLE";
    case eLIM_STA_IN_IBSS_ROLE:
        return "eLIM_STA_IN_IBSS_ROLE";
    case eLIM_STA_ROLE:
        return "eLIM_STA_ROLE";
    case eLIM_BT_AMP_STA_ROLE:
        return "eLIM_BT_AMP_STA_ROLE";
    case eLIM_BT_AMP_AP_ROLE:
        return "eLIM_BT_AMP_AP_ROLE";
    default:
        return "UNKNOWN";
  }
}



char *dumpLim( tpAniSirGlobal pMac, char *p, tANI_U32 sessionId)
{
  #ifdef FIXME_GEN6
  //iterate through the sessionTable and dump sta entries for each session.
  //Keep this code under 'WLAN_DEBUG' compile flag.

  tANI_U16 i, j;

  tpPESession psessionEntry = peFindSessionBySessionId(pMac, sessionId);

  if (psessionEntry == NULL)
  {
    p += log_sprintf( pMac, p, "Invalid sessionId: %d \n ", sessionId);
    return p;
  }

  p += log_sprintf( pMac,p, "\n ----- LIM Debug Information ----- \n");
  p += log_sprintf( pMac,p, "LIM Role  = (%d) %s\n",
                  pMac->lim.gLimSystemRole, getRole(pMac->lim.gLimSystemRole));
  p += log_sprintf( pMac,p, "SME State = (%d) %s",
                  pMac->lim.gLimSmeState, limSmeStateStr(pMac->lim.gLimSmeState));
  p += log_sprintf( pMac,p, "MLM State = (%d) %s",
                  pMac->lim.gLimMlmState, limMlmStateStr(pMac->lim.gLimMlmState));
  p += log_sprintf( pMac,p, "802.11n session HT Capability: %s\n",
                  (psessionEntry->htCapability == 1) ? "Enabled" : "Disabled");
  p += log_sprintf( pMac,p, "gLimProcessDefdMsgs: %s\n",
                  (pMac->lim.gLimProcessDefdMsgs == 1) ? "Enabled" : "Disabled");

  if (pMac->lim.gLimSystemRole == eLIM_STA_ROLE)
  {
      p += log_sprintf( pMac,p, "AID = %X\t\t\n", pMac->lim.gLimAID);
      p += log_sprintf( pMac,p, "SSID mismatch in Beacon Count              = %d\n",
                      pMac->lim.gLimBcnSSIDMismatchCnt);
      p += log_sprintf( pMac,p, "Number of link establishments               = %d\n",
                      pMac->lim.gLimNumLinkEsts);
  }
  else if (pMac->lim.gLimSystemRole == eLIM_AP_ROLE)
  {
      p += log_sprintf( pMac,p, "Num of STAs associated                     = %d\n",
                      peGetCurrentSTAsCount(pMac));

      p += log_sprintf( pMac,p, "Num of Pre-auth contexts                   = %d\n",
                      pMac->lim.gLimNumPreAuthContexts);

      p += log_sprintf( pMac,p, "Num of AssocReq dropped in invalid State   = %d\n",
                      pMac->lim.gLimNumAssocReqDropInvldState);

      p += log_sprintf( pMac,p, "Num of ReassocReq dropped in invalid State = %d\n",
                      pMac->lim.gLimNumReassocReqDropInvldState);

      p += log_sprintf( pMac,p, "Num of Hash Miss Event ignored             = %d\n",
                      pMac->lim.gLimNumHashMissIgnored);
  }

  p += log_sprintf( pMac,p, "Num of RxCleanup Count                     = %d\n",
                  pMac->lim.gLimNumRxCleanup);
  p += log_sprintf( pMac,p, "Unexpected Beacon Count                    = %d\n",
                  pMac->lim.gLimUnexpBcnCnt);
  p += log_sprintf( pMac,p, "Number of Re/Assoc rejects of 11b STAs     = %d\n",
                  pMac->lim.gLim11bStaAssocRejectCount);
  p += log_sprintf( pMac,p, "No. of HeartBeat Failures in LinkEst State = %d\n",
                  pMac->lim.gLimHBfailureCntInLinkEstState);
  p += log_sprintf( pMac,p, "No. of Probe Failures after HB failed      = %d\n",
                  pMac->lim.gLimProbeFailureAfterHBfailedCnt);
  p += log_sprintf( pMac,p, "No. of HeartBeat Failures in Other States  = %d\n",
                  pMac->lim.gLimHBfailureCntInOtherStates);
  p += log_sprintf( pMac,p, "No. of Beacons Rxed During HB Interval     = %d\n",
                  pMac->lim.gLimRxedBeaconCntDuringHB);
  p += log_sprintf( pMac,p, "Self Operating Mode                              = %s\n", limDot11ModeStr(pMac, (tANI_U8)pMac->lim.gLimDot11Mode));
  p += log_sprintf( pMac,p, "\n");

  if (pMac->lim.gLimSystemRole == eLIM_AP_ROLE)
      i = 2;
  else
      i = 1;


  for (; i< pMac->lim.maxStation; i++)
  {
      tpDphHashNode pSta = dphGetHashEntry(pMac, (unsigned short)i);
      if (pSta && pSta->added)
      {
          p += log_sprintf( pMac,p, "\nSTA AID: %d  STA ID: %d Valid: %d AuthType: %d MLM State: %s",
                          i, pSta->staIndex, pSta->valid,
                          pSta->mlmStaContext.authType,
                          limMlmStateStr(pSta->mlmStaContext.mlmState));

          p += log_sprintf( pMac,p, "\tAID:%-2d  OpRateMode:%s  ShPrmbl:%d  HT:%d  GF:%d  TxChWidth:%d  MimoPS:%d  LsigProt:%d\n",
                            pSta->assocId, limStaOpRateModeStr(pSta->supportedRates.opRateMode),
                            pSta->shortPreambleEnabled, pSta->mlmStaContext.htCapability,
                            pSta->htGreenfield, pSta->htSupportedChannelWidthSet,
                            pSta->htMIMOPSState, pSta->htLsigTXOPProtection);

          p += log_sprintf( pMac,p, "\tAMPDU [MaxSz(Factor):%d, Dens: %d]  AMSDU-MaxLen: %d\n",
                          pSta->htMaxRxAMpduFactor, pSta->htAMpduDensity,pSta->htMaxAmsduLength);
          p += log_sprintf( pMac,p, "\tDSSCCkMode40Mhz: %d, SGI20: %d, SGI40: %d\n",
                          pSta->htDsssCckRate40MHzSupport, pSta->htShortGI20Mhz,
                          pSta->htShortGI40Mhz);

          p += log_sprintf( pMac,p, "\t11b Rates: ");
          for(j=0; j<SIR_NUM_11B_RATES; j++)
              if(pSta->supportedRates.llbRates[j] > 0)
                  p += log_sprintf( pMac,p, "%d ", pSta->supportedRates.llbRates[j]);

          p += log_sprintf( pMac,p, "\n\t11a Rates: ");
          for(j=0; j<SIR_NUM_11A_RATES; j++)
              if(pSta->supportedRates.llaRates[j] > 0)
                  p += log_sprintf( pMac,p, "%d ", pSta->supportedRates.llaRates[j]);

          p += log_sprintf( pMac,p, "\n\tPolaris Rates: ");
          for(j=0; j<SIR_NUM_POLARIS_RATES; j++)
              if(pSta->supportedRates.aniLegacyRates[j] > 0)
                  p += log_sprintf( pMac,p, "%d ", pSta->supportedRates.aniLegacyRates[j]);

          p += log_sprintf( pMac,p, "\n\tTitan and Taurus Proprietary Rate Bitmap: %08x\n",
                          pSta->supportedRates.aniEnhancedRateBitmap);
          p += log_sprintf( pMac,p, "\tMCS Rate Set Bitmap: ");
          for(j=0; j<SIR_MAC_MAX_SUPPORTED_MCS_SET; j++)
              p += log_sprintf( pMac,p, "%x ", pSta->supportedRates.supportedMCSSet[j]);

      }
  }
  p += log_sprintf( pMac,p, "\nProbe response disable          = %d\n",
                  pMac->lim.gLimProbeRespDisableFlag);

  p += log_sprintf( pMac,p, "Scan mode enable                = %d\n",
                  pMac->sys.gSysEnableScanMode);
  p += log_sprintf( pMac,p, "BackgroundScanDisable           = %d\n",
                  pMac->lim.gLimBackgroundScanDisable);
  p += log_sprintf( pMac,p, "ForceBackgroundScanDisable      = %d\n",
                  pMac->lim.gLimForceBackgroundScanDisable);
  p += log_sprintf( pMac,p, "LinkMonitor mode enable         = %d\n",
                  pMac->sys.gSysEnableLinkMonitorMode);
  p += log_sprintf( pMac,p, "Qos Capable                     = %d\n",
                  SIR_MAC_GET_QOS(pMac->lim.gLimCurrentBssCaps));
  p += log_sprintf( pMac,p, "Wme Capable                     = %d\n",
                  LIM_BSS_CAPS_GET(WME, pMac->lim.gLimCurrentBssQosCaps));
  p += log_sprintf( pMac,p, "Wsm Capable                     = %d\n",
                  LIM_BSS_CAPS_GET(WSM, pMac->lim.gLimCurrentBssQosCaps));
  if (pMac->lim.gLimSystemRole == eLIM_STA_IN_IBSS_ROLE)
  {
      p += log_sprintf( pMac,p, "Number of peers in IBSS         = %d\n",
                      pMac->lim.gLimNumIbssPeers);
      if (pMac->lim.gLimNumIbssPeers)
      {
          tLimIbssPeerNode *pTemp;
          pTemp = pMac->lim.gLimIbssPeerList;
          p += log_sprintf( pMac,p, "MAC-Addr           Ani Edca WmeInfo HT  Caps  #S,#E(Rates)\n");
          while (pTemp != NULL)
          {
              p += log_sprintf( pMac,p, "%02X:%02X:%02X:%02X:%02X:%02X ",
                              pTemp->peerMacAddr[0],
                              pTemp->peerMacAddr[1],
                              pTemp->peerMacAddr[2],
                              pTemp->peerMacAddr[3],
                              pTemp->peerMacAddr[4],
                              pTemp->peerMacAddr[5]);
              p += log_sprintf( pMac,p, " %d   %d,%d        %d  %d  %04X  %d,%d\n",
                              pTemp->aniIndicator,
                              pTemp->edcaPresent, pTemp->wmeEdcaPresent,
                              pTemp->wmeInfoPresent,
                              pTemp->htCapable,
                              pTemp->capabilityInfo,
                              pTemp->supportedRates.numRates,
                              pTemp->extendedRates.numRates);
              pTemp = pTemp->next;
          }
      }
  }
  p += log_sprintf( pMac,p, "System Scan/Learn Mode bit      = %d\n",
                  pMac->lim.gLimSystemInScanLearnMode);
  p += log_sprintf( pMac,p, "Scan override                   = %d\n",
                  pMac->lim.gLimScanOverride);
  p += log_sprintf( pMac,p, "CB State protection             = %d\n",
                  pMac->lim.gLimCBStateProtection);
  p += log_sprintf( pMac,p, "Count of Titan STA's            = %d\n",
                  pMac->lim.gLimTitanStaCount);

  //current BSS capability
  p += log_sprintf( pMac,p, "**********Current BSS Capability********\n");
  p += log_sprintf( pMac,p, "Ess = %d, ", SIR_MAC_GET_ESS(pMac->lim.gLimCurrentBssCaps));
  p += log_sprintf( pMac,p, "Privacy = %d, ", SIR_MAC_GET_PRIVACY(pMac->lim.gLimCurrentBssCaps));
  p += log_sprintf( pMac,p, "Short Preamble = %d, ", SIR_MAC_GET_SHORT_PREAMBLE(pMac->lim.gLimCurrentBssCaps));
  p += log_sprintf( pMac,p, "Short Slot = %d, ", SIR_MAC_GET_SHORT_SLOT_TIME(pMac->lim.gLimCurrentBssCaps));
  p += log_sprintf( pMac,p, "Qos = %d\n", SIR_MAC_GET_QOS(pMac->lim.gLimCurrentBssCaps));

  //Protection related information
  p += log_sprintf( pMac,p, "*****Protection related information******\n");
  p += log_sprintf( pMac,p, "Protection %s\n", pMac->lim.gLimProtectionControl ? "Enabled" : "Disabled");

  p += log_sprintf( pMac,p, "OBSS MODE = %d\n", pMac->lim.gHTObssMode);
    p += log_sprintf( pMac, p, "HT operating Mode = %d, llbCoexist = %d, llgCoexist = %d, ht20Coexist = %d, nonGfPresent = %d, RifsMode = %d, lsigTxop = %d\n",
                      pMac->lim.gHTOperMode, pMac->lim.llbCoexist, pMac->lim.llgCoexist,
                      pMac->lim.ht20MhzCoexist, pMac->lim.gHTNonGFDevicesPresent,
                      pMac->lim.gHTRifsMode, pMac->lim.gHTLSigTXOPFullSupport);
    p += log_sprintf(pMac, p, "2nd Channel offset = %d\n",
                  psessionEntry->hHTSecondaryChannelOffset);
#endif
    return p;
}

/*******************************************
 * FUNCTION: triggerBeaconGen()
 *
 * This logdump sends SIR_SCH_BEACON_GEN_IND to SCH.
 * SCH then proceeds to generate a beacon template
 * and copy it to the Host/SoftMAC shared memory
 *
 * TODO - This routine can safely be deleted once
 * beacon generation is working
 ******************************************/
char *triggerBeaconGen( tpAniSirGlobal pMac, char *p )
{
    tSirMsgQ mesg = { (tANI_U16) SIR_LIM_BEACON_GEN_IND, (tANI_U16) 0, (tANI_U32) 0 };
    
    pMac->lim.gLimSmeState = eLIM_SME_NORMAL_STATE;
    MTRACE(macTrace(pMac, TRACE_CODE_SME_STATE, NO_SESSION, pMac->lim.gLimSmeState));
    pMac->lim.gLimSystemRole = eLIM_AP_ROLE;
    
    p += log_sprintf( pMac, p,
          "Posted SIR_LIM_BEACON_GEN_IND with result = %s\n",
          (eSIR_SUCCESS == limPostMsgApi( pMac, &mesg ))?
            "Success": "Failure" );
    
    return p;
}


/*******************************************
 * FUNCTION: testLimSendProbeRsp()
 *
 * This logdump sends SIR_MAC_MGMT_PROBE_RSP
 *
 * TODO - This routine can safely be deleted once
 * the MGMT frame transmission is working
 ******************************************/
char *testLimSendProbeRsp( tpAniSirGlobal pMac, char *p )
{
    tSirMacAddr peerMacAddr = { 0, 1, 2, 3, 4, 5 };
    tAniSSID ssId;
    tANI_U32 len = SIR_MAC_MAX_SSID_LENGTH;
    tpPESession psessionEntry = &pMac->lim.gpSession[0];  //TBD-RAJESH HOW TO GET sessionEntry?????


    if( eSIR_SUCCESS != wlan_cfgGetStr( pMac,
        WNI_CFG_SSID,
        (tANI_U8 *) &ssId.ssId,
        (tANI_U32 *) &len ))
    {
        // Could not get SSID from CFG. Log error.
        p += log_sprintf( pMac, p, "Unable to retrieve SSID\n" );
        return p;
    }
    else
        ssId.length = (tANI_U8) len;

    p += log_sprintf( pMac, p, "Calling limSendProbeRspMgmtFrame...\n" );
    limSendProbeRspMgmtFrame( pMac, peerMacAddr, &ssId, -1, 1, psessionEntry , 0);

    return p;
}


static char *sendSmeScanReq(tpAniSirGlobal pMac, char *p)
{
    tSirMsgQ         msg;
    tSirSmeScanReq   scanReq, *pScanReq;

    p += log_sprintf( pMac,p, "sendSmeScanReq: Preparing eWNI_SME_SCAN_REQ message\n");

    pScanReq = (tSirSmeScanReq *) &scanReq;

    pScanReq = vos_mem_malloc(sizeof(tSirSmeScanReq));
    if (NULL == pScanReq)
    {
        p += log_sprintf( pMac,p,"sendSmeScanReq: AllocateMemory() failed \n");
        return p;
    }

    pScanReq->messageType = eWNI_SME_SCAN_REQ;
    pScanReq->minChannelTime = 30;
    pScanReq->maxChannelTime = 130;
    pScanReq->bssType = eSIR_INFRASTRUCTURE_MODE;
    limGetMyMacAddr(pMac, pScanReq->bssId);
    pScanReq->numSsid = 1;
    vos_mem_copy((void *) &pScanReq->ssId[0].ssId, (void *)"Ivan", 4);
    pScanReq->ssId[0].length = 4;
    pScanReq->scanType = eSIR_ACTIVE_SCAN;
    pScanReq->returnAfterFirstMatch = 0;
    pScanReq->returnUniqueResults = 0;
    pScanReq->returnFreshResults = SIR_BG_SCAN_PURGE_RESUTLS|SIR_BG_SCAN_RETURN_FRESH_RESULTS;
    pScanReq->channelList.numChannels = 1;
    pScanReq->channelList.channelNumber[0] = 6;
    pScanReq->uIEFieldLen = 0;
    pScanReq->uIEFieldOffset = sizeof(tSirSmeScanReq);
    pScanReq->sessionId = 0;

    msg.type = eWNI_SME_SCAN_REQ;
    msg.bodyptr = pScanReq;
    msg.bodyval = 0;
    p += log_sprintf( pMac,p, "sendSmeScanReq: limPostMsgApi(eWNI_SME_SCAN_REQ) \n");
    limPostMsgApi(pMac, &msg);

    return p;
}

static char *sendSmeDisAssocReq(tpAniSirGlobal pMac, char *p,tANI_U32 arg1 ,tANI_U32 arg2)
{

    tpDphHashNode pStaDs;
    tSirMsgQ  msg;
    tSirSmeDisassocReq *pDisAssocReq;
    tpPESession  psessionEntry;

    //arg1 - assocId
    //arg2 - sessionId
    if( arg1 < 1 )
    {
        p += log_sprintf( pMac,p,"Invalid session OR Assoc ID  \n");
        return p;
    }

    if((psessionEntry = peFindSessionBySessionId(pMac,(tANI_U8)arg2) )== NULL)
    {
        p += log_sprintf( pMac,p,"Session does not exist for given session Id  \n");
        return p;
    }

    pStaDs = dphGetHashEntry(pMac, (tANI_U16)arg1, &psessionEntry->dph.dphHashTable);

    if(NULL == pStaDs)
    {
            p += log_sprintf( pMac,p, "Could not find station with assocId = %d\n", arg1);
            return p;
    }

    pDisAssocReq = vos_mem_malloc(sizeof(tSirSmeDisassocReq));
    if (NULL == pDisAssocReq)
    {
        p += log_sprintf( pMac,p,"sendSmeDisAssocReq: AllocateMemory() failed \n");
        return p;
    }

    if( ( (psessionEntry->limSystemRole == eLIM_STA_ROLE) ||
          (psessionEntry ->limSystemRole == eLIM_BT_AMP_STA_ROLE) ) &&
        (psessionEntry->statypeForBss == STA_ENTRY_PEER))
    {
        sirCopyMacAddr(pDisAssocReq->bssId,psessionEntry->bssId);
        sirCopyMacAddr(pDisAssocReq->peerMacAddr,psessionEntry->bssId);
    }
    if((psessionEntry->limSystemRole == eLIM_BT_AMP_AP_ROLE)
       || (psessionEntry->limSystemRole == eLIM_AP_ROLE)
    )
    {
        sirCopyMacAddr(pDisAssocReq->peerMacAddr,pStaDs->staAddr);
        sirCopyMacAddr(pDisAssocReq->bssId,psessionEntry->bssId);
    }

    pDisAssocReq->messageType = eWNI_SME_DISASSOC_REQ;

    pDisAssocReq->length = sizeof(tSirSmeDisassocReq);

    pDisAssocReq->reasonCode =  eSIR_MAC_UNSPEC_FAILURE_REASON;

    pDisAssocReq->sessionId = 0;

    pDisAssocReq->transactionId = 0; 

    msg.type = eWNI_SME_DISASSOC_REQ;
    msg.bodyptr = pDisAssocReq;
    msg.bodyval = 0;

    p += log_sprintf( pMac,p, "sendSmeDisAssocReq: limPostMsgApi(eWNI_SME_DISASSOC_REQ) \n");
    limPostMsgApi(pMac, &msg);

    return p;
}


static char *sendSmeStartBssReq(tpAniSirGlobal pMac, char *p,tANI_U32 arg1)
{
    tSirMsgQ  msg;
    tSirSmeStartBssReq  *pStartBssReq;
    unsigned char *pBuf;
    ePhyChanBondState  cbMode;
    tSirNwType  nwType;

    p += log_sprintf( pMac,p, "sendSmeStartBssReq: Preparing eWNI_SME_START_BSS_REQ message\n");
   
    if(arg1 > 2)
    {
        p += log_sprintf( pMac,p,"Invalid Argument1 \n");
        return p;
    }

    pStartBssReq = vos_mem_malloc(sizeof(tSirSmeStartBssReq));
    if (NULL == pStartBssReq)
    {
        p += log_sprintf( pMac,p,"sendSmeStartBssReq: AllocateMemory() failed \n");
        return p;
    }

    pStartBssReq->messageType = eWNI_SME_START_BSS_REQ;
    pStartBssReq->length = 29;    // 0x1d
    
    if(arg1 == 0) //BTAMP STATION 
    {
        pStartBssReq->bssType = eSIR_BTAMP_STA_MODE;

        pStartBssReq->ssId.length = 5;
        vos_mem_copy((void *) &pStartBssReq->ssId.ssId, (void *)"BTSTA", 5);
    }
    else if(arg1 == 1) //BTAMP AP 
    {
        pStartBssReq->bssType = eSIR_BTAMP_AP_MODE;
        pStartBssReq->ssId.length = 4;
        vos_mem_copy((void *) &pStartBssReq->ssId.ssId, (void *)"BTAP", 4);
    }
    else  //IBSS
    {
        pStartBssReq->bssType = eSIR_IBSS_MODE;
        pStartBssReq->ssId.length = 4;
        vos_mem_copy((void *) &pStartBssReq->ssId.ssId, (void *)"Ibss", 4);
    }

    // Filling in channel ID 6
    pBuf = &(pStartBssReq->ssId.ssId[pStartBssReq->ssId.length]);
    *pBuf = 6;
    pBuf++;

    // Filling in CB mode
    cbMode = PHY_SINGLE_CHANNEL_CENTERED;
    vos_mem_copy(pBuf, (tANI_U8 *)&cbMode, sizeof(ePhyChanBondState));
    pBuf += sizeof(ePhyChanBondState);

    // Filling in RSN IE Length to zero
    vos_mem_set(pBuf, sizeof(tANI_U16), 0);    //tSirRSNie->length
    pBuf += sizeof(tANI_U16);

    // Filling in NW Type
    nwType = eSIR_11G_NW_TYPE;
    vos_mem_copy(pBuf, (tANI_U8 *)&nwType, sizeof(tSirNwType));
    pBuf += sizeof(tSirNwType);

    /* ---- To be filled by LIM later ---- 
    pStartBssReq->operationalRateSet
    pStartBssReq->extendedRateSet
    pStartBssReq->dot11mode
    pStartBssReq->bssId
    pStartBssReq->selfMacAddr
    pStartBssReq->beaconInterval
    pStartBssReq->sessionId = 0;
    pStartBssReq->transactionId = 0; 
    * ------------------------------------ */

    msg.type = eWNI_SME_START_BSS_REQ;
    msg.bodyptr = pStartBssReq;
    msg.bodyval = 0;
    p += log_sprintf( pMac,p, "sendSmeStartBssReq: limPostMsgApi(eWNI_SME_START_BSS_REQ) \n");
    limPostMsgApi(pMac, &msg);

    return p;
}

static char *sendSmeStopBssReq(tpAniSirGlobal pMac, char *p, tANI_U32 sessionId)
{
    tSirMsgQ  msg;
    tSirSmeStopBssReq  stopBssReq, *pStopBssReq;
    tANI_U16  msgLen = 0;
    tpPESession  psessionEntry;

    psessionEntry = peFindSessionBySessionId(pMac, (tANI_U8)sessionId);
    if ( psessionEntry == NULL )
    {
        limLog(pMac, LOGP, FL("Session entry does not exist for given sessionID \n"));
        return p;
    }

    p += log_sprintf( pMac,p, "sendSmeStopBssReq: Preparing eWNI_SME_STOP_BSS_REQ message\n");
    pStopBssReq = (tSirSmeStopBssReq *) &stopBssReq;

    pStopBssReq = vos_mem_malloc(sizeof(tSirSmeStopBssReq));
    if (NULL == pStopBssReq)
    {
        p += log_sprintf( pMac,p,"sendSmeStopBssReq: AllocateMemory() failed \n");
        return p;
    }

    pStopBssReq->messageType = eWNI_SME_STOP_BSS_REQ;
    msgLen += sizeof(tANI_U32);    // msgType + length
   
    pStopBssReq->reasonCode = eSIR_SME_SUCCESS;
    msgLen += sizeof(tSirResultCodes);

    vos_mem_copy((void *) &pStopBssReq->bssId, (void *)psessionEntry->bssId, 6);
    msgLen += sizeof(tSirMacAddr);

    pStopBssReq->sessionId = (tANI_U8)sessionId;
    msgLen += sizeof(tANI_U8);

    pStopBssReq->transactionId = 0;
    msgLen += sizeof(tANI_U16);

    pStopBssReq->length = msgLen;

    msg.type = eWNI_SME_STOP_BSS_REQ;
    msg.bodyptr = pStopBssReq;
    msg.bodyval = 0;
    p += log_sprintf( pMac,p, "sendSmeStopBssReq: limPostMsgApi(eWNI_SME_STOP_BSS_REQ) \n");
    limPostMsgApi(pMac, &msg);

    return p;
}

static char *sendSmeJoinReq(tpAniSirGlobal pMac, char *p)
{
    tSirMsgQ  msg;
    tSirSmeJoinReq  *pJoinReq;
    unsigned char  *pBuf;
    tANI_U16  msgLen = 307;

    tANI_U8  msgDump[307] = {
        0x06, 0x12, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x01, 0x00, 
        0xDE, 0xAD, 0xBA, 0xEF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x64, 0x00, 0x21, 0x04, 0x02, 0x00, 0x00, 
        0x00, 0x01, 0x1E, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x18, 
        0x00, 0x00, 0x00, 0xA8, 0x85, 0x4F, 0x7A, 0x00, 0x06, 0x41, 
        0x6E, 0x69, 0x4E, 0x65, 0x74, 0x01, 0x04, 0x82, 0x84, 0x8B, 
        0x96, 0x03, 0x01, 0x06, 0x07, 0x06, 0x55, 0x53, 0x49, 0x01, 
        0x0E, 0x1E, 0x2A, 0x01, 0x00, 0x32, 0x08, 0x0C, 0x12, 0x18, 
        0x24, 0x30, 0x48, 0x60, 0x6C, 0x2D, 0x1A, 0xEE, 0x11, 0x03, 
        0xFF, 0xFF, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x3D, 0x16, 0x06, 0x07, 0x11, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xDD, 0x18, 0x00, 
        0x50, 0xF2, 0x02, 0x01, 0x01, 0x01, 0x00, 0x03, 0xA4, 0x00, 
        0x00, 0x27, 0xA4, 0x00, 0x00, 0x42, 0x43, 0x5E, 0x00, 0x62, 
        0x32, 0x2F, 0x00, 0xDD, 0x14, 0x00, 0x0A, 0xF5, 0x00, 0x03, 
        0x01, 0x03, 0x05, 0x0A, 0x02, 0x80, 0xC0, 0x12, 0x06, 0xFF, 
        0xFF, 0xFF, 0xFF, 0xB6, 0x0D, 0xDD, 0x6E, 0x00, 0x50, 0xF2, 
        0x04, 0x10, 0x4A, 0x00, 0x01, 0x10, 0x10, 0x44, 0x00, 0x01, 
        0x01, 0x10, 0x3B, 0x00, 0x01, 0x03, 0x10, 0x47, 0x00, 0x10, 
        0xDB, 0xC6, 0x77, 0x28, 0xB9, 0xF3, 0xD8, 0x58, 0x86, 0xFF, 
        0xFC, 0x6B, 0xB6, 0xB9, 0x27, 0x79, 0x10, 0x21, 0x00, 0x08, 
        0x51, 0x75, 0x61, 0x6C, 0x63, 0x6F, 0x6D, 0x6D, 0x10, 0x23, 
        0x00, 0x07, 0x57, 0x46, 0x52, 0x34, 0x30, 0x33, 0x31, 0x10,
        0x24, 0x00, 0x06, 0x4D, 0x4E, 0x31, 0x32, 0x33, 0x34, 0x10, 
        0x42, 0x00, 0x06, 0x53, 0x4E, 0x31, 0x32, 0x33, 0x34, 0x10, 
        0x54, 0x00, 0x08, 0x00, 0x06, 0x00, 0x50, 0xF2, 0x04, 0x00, 
        0x01, 0x10, 0x11, 0x00, 0x06, 0x31, 0x31, 0x6E, 0x2D, 0x41, 
        0x50, 0x10, 0x08, 0x00, 0x02, 0x01, 0x8E
    };

    pJoinReq = vos_mem_malloc(msgLen);
    if (NULL == pJoinReq)
    {
        p += log_sprintf( pMac,p,"sendSmeJoinReq: AllocateMemory() failed \n");
        return p;
    }

    pBuf = (unsigned char *)pJoinReq;
    vos_mem_copy(pBuf, (tANI_U8 *)msgDump, msgLen);

    msg.type = eWNI_SME_JOIN_REQ;
    msg.bodyptr = pJoinReq;
    msg.bodyval = 0;
    limPostMsgApi(pMac, &msg);

    return p;
}


static char *printSessionInfo(tpAniSirGlobal pMac, char *p)
{
    tpPESession psessionEntry = &pMac->lim.gpSession[0];  
    tANI_U8  i;

    p += log_sprintf( pMac, p, "Dump PE Session \n");

    for(i=0; i < pMac->lim.maxBssId; i++)
    {
        if( pMac->lim.gpSession[i].valid )
        {
            psessionEntry = &pMac->lim.gpSession[i];  
            p += log_sprintf( pMac,p, "*****************************************\n");
            p += log_sprintf( pMac,p, "    PE Session [%d]    \n", i);   
            p += log_sprintf( pMac,p, "available: %d \n", psessionEntry->available);
            p += log_sprintf( pMac,p, "peSessionId: %d,  smeSessionId: %d, transactionId: %d \n", 
                              psessionEntry->peSessionId, psessionEntry->smeSessionId, psessionEntry->smeSessionId);
            p += log_sprintf( pMac,p, "bssId:  %02X:%02X:%02X:%02X:%02X:%02X \n", 
                              psessionEntry->bssId[0], psessionEntry->bssId[1], psessionEntry->bssId[2],
                              psessionEntry->bssId[3], psessionEntry->bssId[4], psessionEntry->bssId[5]);
            p += log_sprintf( pMac,p, "selfMacAddr: %02X:%02X:%02X:%02X:%02X:%02X  \n", 
                              psessionEntry->selfMacAddr[0], psessionEntry->selfMacAddr[1], psessionEntry->selfMacAddr[2],
                              psessionEntry->selfMacAddr[3], psessionEntry->selfMacAddr[4], psessionEntry->selfMacAddr[5]);
            p += log_sprintf( pMac,p, "bssIdx: %d \n", psessionEntry->bssIdx);
            p += log_sprintf( pMac,p, "valid: %d \n", psessionEntry->valid);
            p += log_sprintf( pMac,p, "limMlmState: (%d) %s ", psessionEntry->limMlmState, limMlmStateStr(psessionEntry->limMlmState) );
            p += log_sprintf( pMac,p, "limPrevMlmState: (%d) %s ", psessionEntry->limPrevMlmState, limMlmStateStr(psessionEntry->limMlmState) );
            p += log_sprintf( pMac,p, "limSmeState: (%d) %s ", psessionEntry->limSmeState, limSmeStateStr(psessionEntry->limSmeState) );
            p += log_sprintf( pMac,p, "limPrevSmeState: (%d)  %s ", psessionEntry->limPrevSmeState, limSmeStateStr(psessionEntry->limPrevSmeState) );
            p += log_sprintf( pMac,p, "limSystemRole: (%d) %s \n", psessionEntry->limSystemRole, getRole(psessionEntry->limSystemRole) );
            p += log_sprintf( pMac,p, "bssType: (%d) %s \n", psessionEntry->bssType, limBssTypeStr(psessionEntry->bssType));
            p += log_sprintf( pMac,p, "operMode: %d \n", psessionEntry->operMode);
            p += log_sprintf( pMac,p, "dot11mode: %d \n", psessionEntry->dot11mode);
            p += log_sprintf( pMac,p, "htCapability: %d \n", psessionEntry->htCapability);
            p += log_sprintf( pMac,p, "limRFBand: %d \n", psessionEntry->limRFBand);
            p += log_sprintf( pMac,p, "limIbssActive: %d \n", psessionEntry->limIbssActive);
            p += log_sprintf( pMac,p, "limCurrentAuthType: %d \n", psessionEntry->limCurrentAuthType);
            p += log_sprintf( pMac,p, "limCurrentBssCaps: %d \n", psessionEntry->limCurrentBssCaps);
            p += log_sprintf( pMac,p, "limCurrentBssQosCaps: %d \n", psessionEntry->limCurrentBssQosCaps);
            p += log_sprintf( pMac,p, "limCurrentBssPropCap: %d \n", psessionEntry->limCurrentBssPropCap);
            p += log_sprintf( pMac,p, "limSentCapsChangeNtf: %d \n", psessionEntry->limSentCapsChangeNtf);
            p += log_sprintf( pMac,p, "LimAID: %d \n", psessionEntry->limAID);
            p += log_sprintf( pMac,p, "ReassocbssId: %02X:%02X:%02X:%02X:%02X:%02X  \n", 
                              psessionEntry->limReAssocbssId[0], psessionEntry->limReAssocbssId[1], psessionEntry->limReAssocbssId[2],
                              psessionEntry->limReAssocbssId[3], psessionEntry->limReAssocbssId[4], psessionEntry->limReAssocbssId[5]);
            p += log_sprintf( pMac,p, "limReassocChannelId: %d \n", psessionEntry->limReassocChannelId);
            p += log_sprintf( pMac,p, "limReassocBssCaps: %d \n", psessionEntry->limReassocBssCaps);
            p += log_sprintf( pMac,p, "limReassocBssQosCaps: %d \n", psessionEntry->limReassocBssQosCaps);
            p += log_sprintf( pMac,p, "limReassocBssPropCap: %d \n", psessionEntry->limReassocBssPropCap);
            p += log_sprintf( pMac,p, "********************************************\n");
        }
    }
    return p;
}

void
limSetEdcaBcastACMFlag(tpAniSirGlobal pMac, tANI_U32 ac, tANI_U32 acmFlag)
{
    tpPESession psessionEntry = &pMac->lim.gpSession[0];  //TBD-RAJESH HOW TO GET sessionEntry?????
    psessionEntry->gLimEdcaParamsBC[ac].aci.acm = (tANI_U8)acmFlag;
    psessionEntry->gLimEdcaParamSetCount++;
    schSetFixedBeaconFields(pMac,psessionEntry);
}

static char *
limDumpEdcaParams(tpAniSirGlobal pMac, char *p)
{
    tANI_U8 i = 0;
    tpPESession psessionEntry = &pMac->lim.gpSession[0];  //TBD-RAJESH HOW TO GET sessionEntry?????    
    p += log_sprintf( pMac,p, "EDCA parameter set count = %d\n",  psessionEntry->gLimEdcaParamSetCount);
    p += log_sprintf( pMac,p, "Broadcast parameters\n");
    p += log_sprintf( pMac,p, "AC\tACI\tACM\tAIFSN\tCWMax\tCWMin\tTxopLimit\t\n");
    for(i = 0; i < MAX_NUM_AC; i++)
    {
        //right now I am just interested in ACM bit. this can be extended for all other EDCA paramters.
        p += log_sprintf( pMac,p, "%d\t%d\t%d\t%d\t%d\t%d\t%d\n",  i,
          psessionEntry->gLimEdcaParamsBC[i].aci.aci, psessionEntry->gLimEdcaParamsBC[i].aci.acm,
          psessionEntry->gLimEdcaParamsBC[i].aci.aifsn, psessionEntry->gLimEdcaParamsBC[i].cw.max,
          psessionEntry->gLimEdcaParamsBC[i].cw.min, psessionEntry->gLimEdcaParamsBC[i].txoplimit);
    }

    p += log_sprintf( pMac,p, "\nLocal parameters\n");
    p += log_sprintf( pMac,p, "AC\tACI\tACM\tAIFSN\tCWMax\tCWMin\tTxopLimit\t\n");
    for(i = 0; i < MAX_NUM_AC; i++)
    {
        //right now I am just interested in ACM bit. this can be extended for all other EDCA paramters.
        p += log_sprintf( pMac,p, "%d\t%d\t%d\t%d\t%d\t%d\t%d\n",  i,
              psessionEntry->gLimEdcaParams[i].aci.aci, psessionEntry->gLimEdcaParams[i].aci.acm,
              psessionEntry->gLimEdcaParams[i].aci.aifsn, psessionEntry->gLimEdcaParams[i].cw.max,
              psessionEntry->gLimEdcaParams[i].cw.min, psessionEntry->gLimEdcaParams[i].txoplimit);
    }

    return p;
}


static char* limDumpTspecEntry(tpAniSirGlobal pMac, char *p, tANI_U32 tspecEntryNo)
{
    tpLimTspecInfo pTspecList;
    if(tspecEntryNo >= LIM_NUM_TSPEC_MAX)
    {
        p += log_sprintf( pMac,p, "Tspec Entry no. %d is out of allowed range(0 .. %d)\n",
                        tspecEntryNo,  (LIM_NUM_TSPEC_MAX - 1));
        return p;
    }
    pTspecList = &pMac->lim.tspecInfo[tspecEntryNo];
    if (pTspecList->inuse)
        p += log_sprintf( pMac,p, "Entry %d is VALID\n", tspecEntryNo);
    else
    {
        p += log_sprintf( pMac,p, "Entry %d is UNUSED\n", tspecEntryNo);
        return p;
    }
    p += log_sprintf( pMac,p, "\tSta %0x:%0x:%0x:%0x:%0x:%0x, AID %d, Index %d\n",
                            pTspecList->staAddr[0], pTspecList->staAddr[1],
                            pTspecList->staAddr[2], pTspecList->staAddr[3],
                            pTspecList->staAddr[4], pTspecList->staAddr[5],
                            pTspecList->assocId,  pTspecList->idx);
    p += log_sprintf( pMac,p, "\tType %d, Length %d, ackPolicy %d, userPrio %d, accessPolicy = %d, Dir %d, tsid %d\n",
                            pTspecList->tspec.type, pTspecList->tspec.length,
                            pTspecList->tspec.tsinfo.traffic.ackPolicy, pTspecList->tspec.tsinfo.traffic.userPrio,
                            pTspecList->tspec.tsinfo.traffic.accessPolicy, pTspecList->tspec.tsinfo.traffic.direction,
                            pTspecList->tspec.tsinfo.traffic.tsid);
    p += log_sprintf( pMac,p, "\tPsb %d, Agg %d, TrafficType %d, schedule %d; msduSz: nom %d, max %d\n",
                            pTspecList->tspec.tsinfo.traffic.psb, pTspecList->tspec.tsinfo.traffic.aggregation,
                            pTspecList->tspec.tsinfo.traffic.trafficType, pTspecList->tspec.tsinfo.schedule.schedule,
                            pTspecList->tspec.nomMsduSz,  pTspecList->tspec.maxMsduSz);
    p += log_sprintf( pMac,p, "\tSvcInt: Min %d, Max %d; dataRate: Min %d, mean %d, peak %d\n",
                            pTspecList->tspec.minSvcInterval,  pTspecList->tspec.maxSvcInterval,
                            pTspecList->tspec.minDataRate,  pTspecList->tspec.meanDataRate,
                            pTspecList->tspec.peakDataRate);
    p += log_sprintf( pMac,p, "\tmaxBurstSz %d, delayBound %d, minPhyRate %d, surplusBw %d, mediumTime %d\n",
                            pTspecList->tspec.maxBurstSz, pTspecList->tspec.delayBound,
                            pTspecList->tspec.minPhyRate, pTspecList->tspec.surplusBw,
                            pTspecList->tspec.mediumTime);

    return p;
}

static char* dumpTspecTableSummary(tpAniSirGlobal pMac, tpLimTspecInfo pTspecList, char *p, int ctspec)
{
  p += log_sprintf( pMac, p, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
            ctspec, pTspecList->idx, pTspecList->assocId,
            pTspecList->tspec.tsinfo.traffic.ackPolicy, pTspecList->tspec.tsinfo.traffic.userPrio,
            pTspecList->tspec.tsinfo.traffic.psb, pTspecList->tspec.tsinfo.traffic.aggregation,
            pTspecList->tspec.tsinfo.traffic.accessPolicy, pTspecList->tspec.tsinfo.traffic.direction,
            pTspecList->tspec.tsinfo.traffic.tsid, pTspecList->tspec.tsinfo.traffic.trafficType);

  return p;
}


static char* limDumpDphTableSummary(tpAniSirGlobal pMac,char *p)
{
    tANI_U8  i, j;
    p += log_sprintf( pMac,p, "DPH Table dump\n");

    for(j=0; j < pMac->lim.maxBssId; j++)
    {
        /* Find first free room in session table */
        if(pMac->lim.gpSession[j].valid)
        {
            p += log_sprintf( pMac,p, "aid staId bssid encPol qosMode wme 11e wsm staaddr\n");
            for(i = 0; i < pMac->lim.gpSession[j].dph.dphHashTable.size; i++)
            {
                if (pMac->lim.gpSession[j].dph.dphHashTable.pDphNodeArray[i].added)
                {
                    p += log_sprintf( pMac,p, "%d  %d  %d      %d         %d   %d %d   %d  %x:%x:%x:%x:%x:%x\n",
                                      pMac->lim.gpSession[j].dph.dphHashTable.pDphNodeArray[i].assocId,
                                      pMac->lim.gpSession[j].dph.dphHashTable.pDphNodeArray[i].staIndex,
                                      pMac->lim.gpSession[j].dph.dphHashTable.pDphNodeArray[i].bssId,
                                      pMac->lim.gpSession[j].dph.dphHashTable.pDphNodeArray[i].encPolicy,
                                      pMac->lim.gpSession[j].dph.dphHashTable.pDphNodeArray[i].qosMode,
                                      pMac->lim.gpSession[j].dph.dphHashTable.pDphNodeArray[i].wmeEnabled,
                                      pMac->lim.gpSession[j].dph.dphHashTable.pDphNodeArray[i].lleEnabled,
                                      pMac->lim.gpSession[j].dph.dphHashTable.pDphNodeArray[i].wsmEnabled,
                                      pMac->lim.gpSession[j].dph.dphHashTable.pDphNodeArray[i].staAuthenticated,
                                      pMac->lim.gpSession[j].dph.dphHashTable.pDphNodeArray[i].staAddr[0],
                                      pMac->lim.gpSession[j].dph.dphHashTable.pDphNodeArray[i].staAddr[1],
                                      pMac->lim.gpSession[j].dph.dphHashTable.pDphNodeArray[i].staAddr[2],
                                      pMac->lim.gpSession[j].dph.dphHashTable.pDphNodeArray[i].staAddr[3],
                                      pMac->lim.gpSession[j].dph.dphHashTable.pDphNodeArray[i].staAddr[4],
                                      pMac->lim.gpSession[j].dph.dphHashTable.pDphNodeArray[i].staAddr[5]);
                }
            }   
        }   
    }
    return p;
}     

// add the specified tspec to the tspec list
static char* limDumpTsecTable( tpAniSirGlobal pMac, char* p)
{
    int ctspec;
    tpLimTspecInfo  pTspecList = &pMac->lim.tspecInfo[0];

    p += log_sprintf( pMac,p, "=======LIM TSPEC TABLE DUMP\n");
    p += log_sprintf( pMac,p, "Num\tIdx\tAID\tAckPol\tUP\tPSB\tAgg\tAccessPol\tDir\tTSID\ttraffic\n");

    for (ctspec = 0; ctspec < LIM_NUM_TSPEC_MAX; ctspec++, pTspecList++)
    {
        if (pTspecList->inuse)
            p = dumpTspecTableSummary(pMac, pTspecList, p, ctspec);
    }
    return p;
}

static char *
dump_lim_tspec_table( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;
    p = limDumpTsecTable(pMac, p);
    return p;
}

static char *
dump_lim_tspec_entry( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg2; (void) arg3; (void) arg4;
    p = limDumpTspecEntry(pMac, p, arg1);
    return p;
}

static char *
dump_lim_dph_table_summary( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg2; (void) arg3; (void) arg4;
    p = limDumpDphTableSummary(pMac, p);
    return p;
}


static char *
dump_lim_link_monitor_stats( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    tANI_U32 ind, val;

    (void) arg2; (void) arg3; (void) arg4;
    p += log_sprintf( pMac,p, "\n ----- LIM Heart Beat Stats ----- \n");
    p += log_sprintf( pMac,p, "No. of HeartBeat Failures in LinkEst State = %d\n",
                    pMac->lim.gLimHBfailureCntInLinkEstState);
    p += log_sprintf( pMac,p, "No. of Probe Failures after HB failed      = %d\n",
                    pMac->lim.gLimProbeFailureAfterHBfailedCnt);
    p += log_sprintf( pMac,p, "No. of HeartBeat Failures in Other States = %d\n",
                    pMac->lim.gLimHBfailureCntInOtherStates);

    if (wlan_cfgGetInt(pMac, WNI_CFG_HEART_BEAT_THRESHOLD, &val) == eSIR_SUCCESS)
        p += log_sprintf( pMac,p, "Cfg HeartBeat Threshold = %d\n", val);

    p += log_sprintf( pMac,p, "# Beacons Rcvd in HB interval    # of times\n");

    for (ind = 1; ind < MAX_NO_BEACONS_PER_HEART_BEAT_INTERVAL; ind++)
    {
         p += log_sprintf( pMac,p, "\t\t\t\t\t\t\t\t%2d\t\t\t\t\t\t\t\t\t\t\t%8d\n", ind,
                        pMac->lim.gLimHeartBeatBeaconStats[ind]);
    }
    p += log_sprintf( pMac,p, "\t\t\t\t\t\t\t\t%2d>\t\t\t\t\t\t\t\t\t\t%8d\n",
                    MAX_NO_BEACONS_PER_HEART_BEAT_INTERVAL-1,
                    pMac->lim.gLimHeartBeatBeaconStats[0]);

    if (arg1 != 0)
    {
        for (ind = 0; ind < MAX_NO_BEACONS_PER_HEART_BEAT_INTERVAL; ind++)
           pMac->lim.gLimHeartBeatBeaconStats[ind] = 0;

        pMac->lim.gLimHBfailureCntInLinkEstState   = 0;
        pMac->lim.gLimProbeFailureAfterHBfailedCnt = 0;
        pMac->lim.gLimHBfailureCntInOtherStates    = 0;

        p += log_sprintf( pMac,p, "\nReset HeartBeat Statistics\n");
    }
    return p;
}

static char *
dump_lim_edca_params( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;
    p = limDumpEdcaParams(pMac, p);
    return p;
}

static char *
dump_lim_acm_set( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg3; (void) arg4;
    limSetEdcaBcastACMFlag(pMac, arg1 /*ac(0..3)*/, arg2 /*(acmFlag = 1 to set ACM*/);
    return p;
}

static char *
dump_lim_bgscan_toggle( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg2; (void) arg3; (void) arg4;
    pMac->lim.gLimForceBackgroundScanDisable = (arg1 == 0) ? 1 : 0;
    p += log_sprintf( pMac,p, "Bgnd scan is now %s\n",
        (pMac->lim.gLimForceBackgroundScanDisable) ? "Disabled" : "On");
    return p;
}

static char *
dump_lim_linkmonitor_toggle( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg2; (void) arg3; (void) arg4;
    pMac->sys.gSysEnableLinkMonitorMode = (arg1 == 0) ? 0 : 1;
    p += log_sprintf( pMac,p, "LinkMonitor mode enable = %s\n",
        (pMac->sys.gSysEnableLinkMonitorMode) ? "On" : "Off");
    return p;
}

static char *
dump_lim_proberesp_toggle( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg2; (void) arg3; (void) arg4;
    pMac->lim.gLimProbeRespDisableFlag = (arg1 == 0) ? 0 : 1;
    p += log_sprintf( pMac,p, "ProbeResponse mode disable = %s\n",
        (pMac->lim.gLimProbeRespDisableFlag) ? "On" : "Off");
    return p;
}

static char *
dump_lim_add_sta( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
#ifdef FIXME_GEN6
    tpDphHashNode pStaDs;
    tpPESession psessionEntry = &pMac->lim.gpSession[0];  //TBD-RAJESH HOW TO GET sessionEntry?????
    tSirMacAddr staMac = {0};
    tANI_U16 peerIdx;
    if(arg2 > 5)
      goto addStaFail;
    peerIdx = limAssignPeerIdx(pMac, psessionEntry);
    pStaDs = dphGetHashEntry(pMac, peerIdx);
    if(NULL == pStaDs)
    {
        staMac[5] = (tANI_U8) arg1;
        pStaDs = dphAddHashEntry(pMac, staMac, peerIdx, &psessionEntry->dph.dphHashTable);
        if(NULL == pStaDs)
          goto addStaFail;

        pStaDs->staType = STA_ENTRY_PEER;
        switch(arg2)
        {
            //11b station
            case 0:
                        {
                            pStaDs->mlmStaContext.htCapability = 0;
                            pStaDs->erpEnabled = 0;
                            p += log_sprintf( pMac,p, "11b");
                        }
                        break;
            //11g station
            case 1:
                        {
                            pStaDs->mlmStaContext.htCapability = 0;
                            pStaDs->erpEnabled = 1;
                            p += log_sprintf( pMac,p, "11g");
                        }
                        break;
            //ht20 station non-GF
            case 2:
                        {
                            pStaDs->mlmStaContext.htCapability = 1;
                            pStaDs->erpEnabled = 1;
                            pStaDs->htSupportedChannelWidthSet = 0;
                            pStaDs->htGreenfield = 0;
                            p += log_sprintf( pMac,p, "HT20 non-GF");
                        }
                        break;
            //ht20 station GF
            case 3:
                        {
                            pStaDs->mlmStaContext.htCapability = 1;
                            pStaDs->erpEnabled = 1;
                            pStaDs->htSupportedChannelWidthSet = 0;
                            pStaDs->htGreenfield = 1;
                            p += log_sprintf( pMac,p, "HT20 GF");
                        }
                        break;
            //ht40 station non-GF
            case 4:
                        {
                            pStaDs->mlmStaContext.htCapability = 1;
                            pStaDs->erpEnabled = 1;
                            pStaDs->htSupportedChannelWidthSet = 1;
                            pStaDs->htGreenfield = 0;
                            p += log_sprintf( pMac,p, "HT40 non-GF");
                        }
                        break;
            //ht40 station GF
            case 5:
                        {
                            pStaDs->mlmStaContext.htCapability = 1;
                            pStaDs->erpEnabled = 1;
                            pStaDs->htSupportedChannelWidthSet = 1;
                            pStaDs->htGreenfield = 1;
                            p += log_sprintf( pMac,p, "HT40 GF");
                        }
                        break;
            default:
                        {
                          p += log_sprintf( pMac,p, "arg2 not in range [0..3]. Station not added.\n");
                          goto addStaFail;
                        }
                        break;
        }

        pStaDs->added = 1;
        p += log_sprintf( pMac,p, " station with mac address 00:00:00:00:00:%x added.\n", (tANI_U8)arg1);
        limAddSta(pMac, pStaDs,psessionEntry);
    }
    else
    {
addStaFail:
        p += log_sprintf( pMac,p, "Could not add station\n");
        p += log_sprintf( pMac,p, "arg1: 6th byte of the station MAC address\n");
        p += log_sprintf( pMac,p, "arg2[0..5] : station type as described below\n");
        p += log_sprintf( pMac,p, "\t 0: 11b, 1: 11g, 2: HT20 non-GF, 3: HT20 GF, 4: HT40 non-GF, 5: HT40 GF\n");
    }
#endif
    return p;
}

static char *
dump_lim_del_sta( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{

    tpDphHashNode       pStaDs;
    tLimMlmDisassocInd  mlmDisassocInd;
    tpPESession         psessionEntry;
    tANI_U8 reasonCode = eSIR_MAC_DISASSOC_DUE_TO_INACTIVITY_REASON;

    if((psessionEntry = peFindSessionBySessionId(pMac,(tANI_U8)arg2) )== NULL)
    {
        p += log_sprintf( pMac,p,"Session does not exist for given session Id  \n");
        return p;
    }

    pStaDs = dphGetHashEntry(pMac, (tANI_U16)arg1, &psessionEntry->dph.dphHashTable);

    if(NULL == pStaDs)
    {
            p += log_sprintf( pMac,p, "Could not find station with assocId = %d\n", arg1);
            return p;
    }
    
    if (pStaDs->mlmStaContext.mlmState != eLIM_MLM_LINK_ESTABLISHED_STATE)
    {
        p += log_sprintf( pMac,p, "received Disassoc frame from peer that is in state %X \n", pStaDs->mlmStaContext.mlmState);
        return p;
    }

    pStaDs->mlmStaContext.cleanupTrigger = eLIM_PEER_ENTITY_DISASSOC;
    pStaDs->mlmStaContext.disassocReason = (tSirMacReasonCodes) reasonCode;

    // Issue Disassoc Indication to SME.
    vos_mem_copy((tANI_U8 *) &mlmDisassocInd.peerMacAddr,
                                (tANI_U8 *) pStaDs->staAddr, sizeof(tSirMacAddr));
    mlmDisassocInd.reasonCode = reasonCode;
    mlmDisassocInd.disassocTrigger = eLIM_PEER_ENTITY_DISASSOC;

    mlmDisassocInd.sessionId = psessionEntry->peSessionId;

    limPostSmeMessage(pMac,  LIM_MLM_DISASSOC_IND,  (tANI_U32 *) &mlmDisassocInd);
    // Receive path cleanup
    limCleanupRxPath(pMac, pStaDs,psessionEntry);
    return p;
}




static char *
set_lim_prot_cfg( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{

/**********************************
* Protection Enable
*
*LOWER byte for associated stations
*UPPER byte for overlapping stations.
*11g ==> protection from 11g
*11b ==> protection from 11b
*each byte will have the following info
*bit7     bit6     bit5   bit4 bit3   bit2  bit1 bit0
*reserved reserved RIFS Lsig n-GF ht20 11g 11b
**********************************
WNI_CFG_PROTECTION_ENABLED    I    4    9
V    RW    NP  RESTART
LIM
0    0xff    0xff
V    RW    NP  RESTART
LIM
0    0xffff    0xffff

#ENUM FROM_llB 0
#ENUM FROM_llG 1
#ENUM HT_20 2
#ENUM NON_GF 3
#ENUM LSIG_TXOP 4
#ENUM RIFS 5
#ENUM OLBC_FROM_llB 8
#ENUM OLBC_FROM_llG 9
#ENUM OLBC_HT20 10
#ENUM OLBC_NON_GF 11
#ENUM OLBC_LSIG_TXOP 12
#ENUM OLBC_RIFS 13
******************************************/
    if(1 == arg1)
        dump_cfg_set(pMac, WNI_CFG_PROTECTION_ENABLED, 0xff, arg3, arg4, p);
    else if(2 == arg1)
        dump_cfg_set(pMac, WNI_CFG_PROTECTION_ENABLED, arg2 & 0xff, arg3, arg4, p);
    else
    {
        p += log_sprintf( pMac,p, "To set protection config:\n");
        p += log_sprintf( pMac,p, "arg1: operation type(1 -> set to Default 0xff, 2-> set to a arg2, else print help)\n");
    }
    return p;
}


static char *
dump_lim_set_protection_control( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    dump_cfg_set(pMac, WNI_CFG_FORCE_POLICY_PROTECTION, arg1, arg2, arg3, p);
    limSetCfgProtection(pMac, NULL);
    return p;
}


static char *
dump_lim_send_SM_Power_Mode( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    tSirMsgQ    msg;
    tpSirMbMsg  pMBMsg;
        tSirMacHTMIMOPowerSaveState state;

        p += log_sprintf( pMac,p, "%s: Verifying the Arguments\n", __func__);
    if ((arg1 > 3) || (arg1 == 2))
    {
                p += log_sprintf( pMac,p, "Invalid Argument , enter one of the valid states\n");
                return p;
        }

        state = (tSirMacHTMIMOPowerSaveState) arg1;

    pMBMsg = vos_mem_malloc(WNI_CFG_MB_HDR_LEN + sizeof(tSirMacHTMIMOPowerSaveState));
    if(NULL == pMBMsg)
    {
        p += log_sprintf( pMac,p, "pMBMsg is NULL\n");
        return p;
    }
    pMBMsg->type = eWNI_PMC_SMPS_STATE_IND;
    pMBMsg->msgLen = (tANI_U16)(WNI_CFG_MB_HDR_LEN + sizeof(tSirMacHTMIMOPowerSaveState));
    vos_mem_copy(pMBMsg->data, &state, sizeof(tSirMacHTMIMOPowerSaveState));

    msg.type = eWNI_PMC_SMPS_STATE_IND;
    msg.bodyptr = pMBMsg;
    msg.bodyval = 0;

    if (limPostMsgApi(pMac, &msg) != TX_SUCCESS)
    {
            p += log_sprintf( pMac,p, "Updating the SMPower Request has failed \n");
        vos_mem_free(pMBMsg);
    }
    else
    {
        p += log_sprintf( pMac,p, "Updating the SMPower Request is Done \n");
    }

        return p;
}




static char *
dump_lim_addba_req( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
tSirRetStatus status;
tpDphHashNode pSta;
  tpPESession psessionEntry = &pMac->lim.gpSession[0]; //TBD-RAJESH HOW TO GET sessionEntry?????


  (void) arg4;

  // Get DPH Sta entry for this ASSOC ID
  pSta = dphGetHashEntry( pMac, (tANI_U16) arg1, &psessionEntry->dph.dphHashTable);
  if( NULL == pSta )
  {
    p += log_sprintf( pMac, p,
        "\n%s: Could not find entry in DPH table for assocId = %d\n",
        __func__,
        arg1 );
  }
  else
  {
    status = limPostMlmAddBAReq( pMac, pSta, (tANI_U8) arg2, (tANI_U16) arg3,psessionEntry);
    p += log_sprintf( pMac, p,
        "\n%s: Attempted to send an ADDBA Req to STA Index %d, for TID %d. Send Status = %s\n",
        __func__,
        pSta->staIndex,
        arg2,
        limResultCodeStr( status ));
  }

  return p;
}

static char *
dump_lim_delba_req( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
tSirRetStatus status;
tpDphHashNode pSta;
  tpPESession psessionEntry = &pMac->lim.gpSession[0];  //TBD-RAJESH HOW TO GET sessionEntry?????

  // Get DPH Sta entry for this ASSOC ID
  pSta = dphGetHashEntry( pMac, (tANI_U16) arg1, &psessionEntry->dph.dphHashTable );
  if( NULL == pSta )
  {
    p += log_sprintf( pMac, p,
        "\n%s: Could not find entry in DPH table for assocId = %d\n",
        __func__,
        arg1 );
  }
  else
  {
    status = limPostMlmDelBAReq( pMac, pSta, (tANI_U8) arg2, (tANI_U8) arg3, (tANI_U16) arg4 ,psessionEntry);
    p += log_sprintf( pMac, p,
        "\n%s: Attempted to send a DELBA Ind to STA Index %d, "
        "as the BA \"%s\" for TID %d, with Reason code %d. "
        "Send Status = %s\n",
        __func__,
        pSta->staIndex,
        (arg2 == 1)? "Initiator": "Recipient",
        arg3, // TID
        arg4, // Reason Code
        limResultCodeStr( status ));
  }

  return p;
}

static char *
dump_lim_ba_timeout( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{

/* FIXME: NO HAL IN UMAC for PRIMA */

  p += log_sprintf( pMac, p,
      "\n%s: Attempted to trigger a BA Timeout Ind to STA Index %d, for TID %d, Direction %d\n",
      __func__,
      arg1, // STA index
      arg2, // TID
      arg3 ); // BA Direction

  return p;
}

static char *
dump_lim_list_active_ba( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
tANI_U32 i;
tpDphHashNode pSta;

//TBD-RAJESH HOW TO GET sessionEntry?????

tpPESession psessionEntry = &pMac->lim.gpSession[0];  //TBD-RAJESH

  (void) arg2; (void) arg3; (void) arg4;

  // Get DPH Sta entry for this ASSOC ID
  pSta = dphGetHashEntry( pMac, (tANI_U16) arg1, &psessionEntry->dph.dphHashTable);
  if( NULL == pSta )
  {
    p += log_sprintf( pMac, p,
        "\n%s: Could not find entry in DPH table for assocId = %d\n",
        __func__,
        arg1 );
  }
  else
  {
    p += log_sprintf( pMac, p,
        "\nList of Active BA sessions for STA Index %d with Assoc ID %d\n",
        pSta->staIndex,
        arg1 );

    p += log_sprintf( pMac, p, "TID\tRxBA\tTxBA\tRxBufferSize\tTxBufferSize\tRxBATimeout\tTxBATimeout\n");
    for( i = 0; i < STACFG_MAX_TC; i ++ )
      p += log_sprintf( pMac, p,
          "%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
          i, // TID
          pSta->tcCfg[i].fUseBARx,
          pSta->tcCfg[i].fUseBATx,
          pSta->tcCfg[i].rxBufSize,
          pSta->tcCfg[i].txBufSize,
          pSta->tcCfg[i].tuRxBAWaitTimeout,
          pSta->tcCfg[i].tuTxBAWaitTimeout );
  }

  return p;
}


static char *
dump_lim_AddBA_DeclineStat( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{

    int Tid, Enable=(arg1 & 0x1);
    tANI_U8 val;

    if (arg1 > 1) {
        log_sprintf( pMac,p, "%s:Invalid Value is entered for Enable/Disable \n", __func__ );
        arg1 &= 1;
    }       
    
    val = pMac->lim.gAddBA_Declined;
    
    if (arg2 > 7) {
        log_sprintf( pMac,p, "%s:Invalid Value is entered for Tid \n", __func__ );
        Tid = arg2 & 0x7;
    } else
        Tid = arg2;
    
    
    if ( Enable)
        val  |= Enable << Tid;
    else
        val &=  ~(0x1 << Tid);

    if (cfgSetInt(pMac, (tANI_U16)WNI_CFG_ADDBA_REQ_DECLINE, (tANI_U32) val) != eSIR_SUCCESS)
             log_sprintf( pMac,p, "%s:Config Set for ADDBA REQ Decline has failed \n", __func__ );

     log_sprintf( pMac,p, "%s:Decline value %d is being set for TID %d ,\n \tAddBA_Decline Cfg value is %d \n", __func__ , arg1, Tid, (int) val);

     return p;
}
static char *
dump_lim_set_dot11_mode( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{

    tpPESession psessionEntry =&pMac->lim.gpSession[0];  //TBD-RAJESH HOW TO GET sessionEntry?????
    dump_cfg_set(pMac, WNI_CFG_DOT11_MODE, arg1, arg2, arg3, p);
    if ( (limGetSystemRole(psessionEntry) == eLIM_AP_ROLE) ||
          (limGetSystemRole(psessionEntry) == eLIM_STA_IN_IBSS_ROLE))
        schSetFixedBeaconFields(pMac,psessionEntry);
    p += log_sprintf( pMac,p, "The Dot11 Mode is set to %s", limDot11ModeStr(pMac, (tANI_U8)psessionEntry->dot11mode));
    return p;
}


static char* dump_lim_update_cb_Mode(tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    tANI_U32 localPwrConstraint;
    tpPESession psessionEntry = peFindSessionBySessionId(pMac, arg1);

    if (psessionEntry == NULL)
    {
      p += log_sprintf( pMac, p, "Invalid sessionId: %d \n ", arg1);
      return p;
    }

    if ( !psessionEntry->htCapability )
    {
        p += log_sprintf( pMac,p, "Error: Dot11 mode is non-HT, can not change the CB mode.\n");
        return p;
    }

    psessionEntry->htSupportedChannelWidthSet = arg2?1:0;
    psessionEntry->htRecommendedTxWidthSet = psessionEntry->htSupportedChannelWidthSet;
    psessionEntry->htSecondaryChannelOffset = arg2;

    if(eSIR_SUCCESS != cfgSetInt(pMac, WNI_CFG_CHANNEL_BONDING_MODE,  
                                    arg2 ? WNI_CFG_CHANNEL_BONDING_MODE_ENABLE : WNI_CFG_CHANNEL_BONDING_MODE_DISABLE))
        p += log_sprintf(pMac,p, "cfgSetInt failed for WNI_CFG_CHANNEL_BONDING_MODE\n");

    wlan_cfgGetInt(pMac, WNI_CFG_LOCAL_POWER_CONSTRAINT, &localPwrConstraint);

    limSendSwitchChnlParams(pMac, psessionEntry->currentOperChannel, psessionEntry->htSecondaryChannelOffset,
                                                                  (tPowerdBm) localPwrConstraint, psessionEntry->peSessionId);
    if ( (limGetSystemRole(psessionEntry) == eLIM_AP_ROLE) ||
          (limGetSystemRole(psessionEntry) == eLIM_STA_IN_IBSS_ROLE))
           schSetFixedBeaconFields(pMac,psessionEntry);
    return p;

}

static char* dump_lim_abort_scan(tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
 (void) arg1; (void) arg2; (void) arg3; (void) arg4;
 //csrScanAbortMacScan(pMac);
    return p;
    
}

static char* dump_lim_start_stop_bg_scan(tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
 (void) arg2; (void) arg3; (void) arg4;

 if (TX_TIMER_VALID(pMac->lim.limTimers.gLimBackgroundScanTimer))
 {
     limDeactivateAndChangeTimer(pMac, eLIM_BACKGROUND_SCAN_TIMER);
 }

 if(arg1 == 1)
 {
     if (tx_timer_activate(
                         &pMac->lim.limTimers.gLimBackgroundScanTimer) != TX_SUCCESS)
     {
         pMac->lim.gLimBackgroundScanTerminate = TRUE;
     }
     else
     {
         pMac->lim.gLimBackgroundScanTerminate = FALSE;
         pMac->lim.gLimBackgroundScanDisable = false;
         pMac->lim.gLimForceBackgroundScanDisable = false;
     }
 }
 else
 {
     pMac->lim.gLimBackgroundScanTerminate = TRUE;
     pMac->lim.gLimBackgroundScanChannelId = 0;
     pMac->lim.gLimBackgroundScanDisable = true;
     pMac->lim.gLimForceBackgroundScanDisable = true;
 }
    return p;
    
}

static char* 
dump_lim_get_pe_statistics(tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    tpAniGetPEStatsReq pReq;
    tANI_U32 statsMask;

    (void) arg2; (void) arg3; (void) arg4;

    
    switch(arg1)
    {        
        case 1:
            statsMask = PE_SUMMARY_STATS_INFO;
            break;
        case 2:
            statsMask = PE_GLOBAL_CLASS_A_STATS_INFO;
            break;
        case 3:
            statsMask = PE_GLOBAL_CLASS_B_STATS_INFO;
            break;
        case 4:
            statsMask = PE_GLOBAL_CLASS_C_STATS_INFO;
            break;
        case 5:
            statsMask = PE_PER_STA_STATS_INFO;
            break;
        default:
            return p;
    }
    
    pReq = vos_mem_malloc(sizeof(tAniGetPEStatsReq));
    if (NULL == pReq)
    {
        p += log_sprintf( pMac,p, "Error: Unable to allocate memory.\n");
        return p;
    }

    vos_mem_set(pReq, sizeof(*pReq), 0);
    
    pReq->msgType = eWNI_SME_GET_STATISTICS_REQ;
    pReq->statsMask = statsMask;
    pReq->staId = (tANI_U16)arg2;

    pMac->lim.gLimRspReqd = eANI_BOOLEAN_TRUE;
    limPostSmeMessage(pMac, eWNI_SME_GET_STATISTICS_REQ, (tANI_U32 *) pReq);
    
    return p;
    
}

extern char* setLOGLevel( tpAniSirGlobal pMac, char *p, tANI_U32 module, tANI_U32 level );
static char *
dump_lim_set_log_level( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    p = setLOGLevel(pMac, p, arg1, arg2);
    return p;
}

static char *
dump_lim_update_log_level( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    vos_trace_setLevel( arg1, arg2 );
    return p;
}

static char *
dump_lim_scan_req_send( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;
    p = sendSmeScanReq(pMac, p);
    return p;
}

static char *
dump_lim_send_start_bss_req( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;
    p = sendSmeStartBssReq(pMac, p,arg1);
    return p;
}

static char *
dump_lim_send_join_req( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;
    p = sendSmeJoinReq(pMac, p); 
    return p;
}

static char *
dump_lim_send_disassoc_req( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;

    p = sendSmeDisAssocReq(pMac, p, arg1 ,arg2);
    return p;
}

static char *
dump_lim_stop_bss_req( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg2; (void) arg3; (void) arg4;
    p = sendSmeStopBssReq(pMac, p, arg1);
    return p;
}


static char *
dump_lim_session_print( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;
    p = printSessionInfo(pMac, p);
    return p;
}

static char *
dump_lim_sme_reassoc_req( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    tANI_U32 sessionId = arg1;
    tCsrRoamModifyProfileFields modifyProfileFields;
    tANI_U32 roamId;

    (void) arg2; (void) arg3; (void) arg4;

    if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
    {
      if(HAL_STATUS_SUCCESS(sme_AcquireGlobalLock( &pMac->sme )))
      {
        csrGetModifyProfileFields(pMac, sessionId, &modifyProfileFields);
        csrReassoc( pMac, sessionId, &modifyProfileFields, &roamId, 0);
        sme_ReleaseGlobalLock( &pMac->sme );
      }
    }
    else
    {
      p += log_sprintf( pMac,p, "Invalid session = %d\n", sessionId);
    }

    return p;
}

static char *
dump_lim_dot11h_stats( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    return p;
}

static char *
dump_lim_enable_measurement( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg2; (void) arg3; (void) arg4;

    if (arg1)
    {
        pMac->sys.gSysEnableLearnMode = eANI_BOOLEAN_TRUE;
        p += log_sprintf(pMac, p, "Measurement enabled\n");
    }
    else
    {
        pMac->sys.gSysEnableLearnMode = eANI_BOOLEAN_FALSE;
        p += log_sprintf(pMac, p, "Measurement disabled\n");
    }

    return p;
}

static char *
dump_lim_enable_quietIE( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg2; (void) arg3; (void) arg4;
#if 0
    if (arg1)
    {
        pMac->lim.gLimSpecMgmt.fQuietEnabled = eANI_BOOLEAN_TRUE;
        p += log_sprintf(pMac, p, "QuietIE enabled\n");
    }
    else
    {
        pMac->lim.gLimSpecMgmt.fQuietEnabled = eANI_BOOLEAN_FALSE;
        p += log_sprintf(pMac, p, "QuietIE disabled\n");
    }
#endif

    return p;
}

static char *
dump_lim_disable_enable_scan( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg2; (void) arg3; (void) arg4;

    if (arg1)
    {
        pMac->lim.fScanDisabled = 1;
        p += log_sprintf(pMac, p, "Scan disabled\n");
    }
    else
    {
        pMac->lim.fScanDisabled = 0;
        p += log_sprintf(pMac, p, "scan enabled\n");
    }

    return p;
}

static char *finishScan(tpAniSirGlobal pMac, char *p)
{
    tSirMsgQ         msg;

    p += log_sprintf( pMac,p, "logDump finishScan \n");

    msg.type = SIR_LIM_MIN_CHANNEL_TIMEOUT;
    msg.bodyval = 0;
    msg.bodyptr = NULL;
    
    limPostMsgApi(pMac, &msg);
    return p;
}


static char *
dump_lim_info( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg2; (void) arg3; (void) arg4;
    p = dumpLim( pMac, p, arg1);
    return p;
}

static char *
dump_lim_finishscan_send( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;
    p = finishScan(pMac, p);
    return p;
}

static char *
dump_lim_prb_rsp_send( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;
    p = testLimSendProbeRsp( pMac, p );
    return p;
}

static char *
dump_sch_beacon_trigger( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;
    p = triggerBeaconGen(pMac, p);
    return p;
}

static char* dump_lim_set_scan_in_powersave( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    p += log_sprintf( pMac,p, "logDump set scan in powersave to %d \n", arg1);
    dump_cfg_set(pMac, WNI_CFG_SCAN_IN_POWERSAVE, arg1, arg2, arg3, p);
    return p;
}

#if defined WLAN_FEATURE_VOWIFI
static char *
dump_lim_send_rrm_action( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    tpPESession psessionEntry;
    tSirMacRadioMeasureReport *pRRMReport =
            vos_mem_malloc(4*sizeof(tSirMacRadioMeasureReport));
    tANI_U8 num = (tANI_U8)(arg4 > 4 ? 4 : arg4);
    tANI_U8 i;

    if (!pRRMReport)
    {
        p += log_sprintf(pMac, p,
                         "Unable to allocate memory to process command\n");
        goto done;
    }

    if((psessionEntry = peFindSessionBySessionId(pMac,(tANI_U8)arg2) )== NULL)
    {
        p += log_sprintf( pMac,p,"Session does not exist for given session Id  \n");
        goto done;
    }
    switch (arg3)
    {
         case 0:
              /* send two reports with incapable bit set */
              pRRMReport[0].type = 6;
              pRRMReport[1].type = 7;
              limSendRadioMeasureReportActionFrame( pMac, 1, 2, &pRRMReport[0], psessionEntry->bssId, psessionEntry ); 
              break;     
         case 1:
              for ( i = 0 ; i < num ; i++ ) 
              {
                   pRRMReport[i].type = 5;
                   if ( i == 3 )
                   pRRMReport[i].refused = 1;
                   else
                   pRRMReport[i].refused = 0;

                   pRRMReport[i].report.beaconReport.regClass = 32;
                   pRRMReport[i].report.beaconReport.channel = i;
                   pRRMReport[i].report.beaconReport.measDuration = 23;
                   pRRMReport[i].report.beaconReport.phyType = i << 4; //some value.
                   pRRMReport[i].report.beaconReport.bcnProbeRsp = 1;
                   pRRMReport[i].report.beaconReport.rsni = 10;
                   pRRMReport[i].report.beaconReport.rcpi = 40;

                   pRRMReport[i].report.beaconReport.bssid[0] = 0x00;
                   pRRMReport[i].report.beaconReport.bssid[1] = 0xAA; 
                   pRRMReport[i].report.beaconReport.bssid[2] = 0xBB;
                   pRRMReport[i].report.beaconReport.bssid[3] = 0xCC;
                   pRRMReport[i].report.beaconReport.bssid[4] = 0x00;
                   pRRMReport[i].report.beaconReport.bssid[5] = 0x01 << i;


                   pRRMReport[i].report.beaconReport.antennaId = 10;
                   pRRMReport[i].report.beaconReport.parentTSF = 0x1234;

                   pRRMReport[i].report.beaconReport.numIes = i * 10;
                   {
                        tANI_U8 j;
                   for( j = 0; j < pRRMReport[i].report.beaconReport.numIes ; j++ )
                   {
                      pRRMReport[i].report.beaconReport.Ies[j] = j + i; //Junk values.
                   }
                   }

              }
              limSendRadioMeasureReportActionFrame( pMac, 1, num, &pRRMReport[0], psessionEntry->bssId, psessionEntry ); 
              break;
         case 2:
              //send Neighbor request.
              {
                   tSirMacNeighborReportReq neighbor;
                   neighbor.dialogToken = 2;
                   neighbor.ssid_present = (tANI_U8) arg4;
                   neighbor.ssid.length = 5;
                   vos_mem_copy(neighbor.ssid.ssId, "abcde", 5);

                   limSendNeighborReportRequestFrame( pMac, &neighbor, psessionEntry->bssId, psessionEntry );

              }

              break;
         case 3:
              //send Link measure report.
              {
                  tSirMacLinkReport link;
                  link.dialogToken = 4;
                  link.txPower = 34;
                  link.rxAntenna = 2;
                  link.txAntenna = 1;
                  link.rcpi = 9;
                  link.rsni = 3;
                  limSendLinkReportActionFrame( pMac, &link, psessionEntry->bssId, psessionEntry ); 
              }
              break;
         default:
              break;
    }

done:
    vos_mem_free(pRRMReport);
    return p;    
}

static char *
dump_lim_unpack_rrm_action( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
   tpPESession  psessionEntry;
   tANI_U32     status;

   tANI_U8 size[] = {
      0x2C,
      0x2F,
      0x25,
      0x2C,
      0x1C,
      0x05
   };

   tANI_U8 pBody[][100] = {
      {
         /*Beacon Request 0*/
      0x05, 0x00, 0x01, 0x00, 0x00, 
      //Measurement request IE
      0x26, 0x25, 0x01, 0x00, 
      //Beacon request type
      0x05,
      //Beacon request starts here
      0x0C, 0x01, 0x30, 0x00, 0x14, 0x00, 0x01,
      //BSSID
      0xFF, 0xFF, 0xFF, 0xFF, 0xff, 0xFF, 
      //SSID
      0x00, 0x05, 0x57, 0x69, 0x46, 0x69, 0x31, 
      //Reporting Condition
      0x01, 0x02, 0x00, 0x00,
      //Reporting Detail
      0x02, 0x01, 0x1,
      //Request IE
      0x0A, 0x05, 0x00, 0x30, 0x46, 0x36, 0xDD
      },
      {
         /*Beacon Request 1*/
      0x05, 0x00, 0x01, 0x00, 0x00, 
      //Measurement request IE
      0x26, 0x28, 0x01, 0x00, 
      //Beacon request type
      0x05,
      //Beacon request starts here
      0x0C, 0xFF, 0x30, 0x00, 0x14, 0x00, 0x01,
      //BSSID
      0xFF, 0xFF, 0xFF, 0xFF, 0xff, 0xFF, 
      //SSID
/*      0x00, 0x08, 0x35, 0x36, 0x37, 0x38, 0x39, 0x40, 0x41, 0x42, */
      //Reporting Condition
      0x01, 0x02, 0x00, 0x00,
      //Reporting Detail
      0x02, 0x01, 0x1,
      //Request IE
      0x0A, 0x05, 0x00, 0x30, 0x46, 0x36, 0xDD,
      //AP channel report
      0x33, 0x03, 0x0C, 0x01, 0x06,    
      0x33, 0x03, 0x0C, 0x24, 0x30,    
      },
      {
         /*Beacon Request 2*/
      0x05, 0x00, 0x01, 0x00, 0x00, 
      //Measurement request IE
      0x26, 0x1E, 0x01, 0x00, 
      //Beacon request type
      0x05,
      //Beacon request starts here
      0x0C, 0x00, 0x30, 0x00, 0x14, 0x00, 0x02,
      //BSSID
      0xFF, 0xFF, 0xFF, 0xFF, 0xff, 0xFF, 
      //SSID
      0x00, 0x05, 0x57, 0x69, 0x46, 0x69, 0x31, 
      //0x00, 0x08, 0x41, 0x53, 0x54, 0x2D, 0x57, 0x41, 0x50, 0x49, 
      //Reporting Condition
      0x01, 0x02, 0x00, 0x00,
      //Reporting Detail
      0x02, 0x01, 0x0
      //Request IE
      },
      {
         /*Beacon Request 3*/
      0x05, 0x00, 0x01, 0x00, 0x00, 
      //Measurement request IE
      0x26, 0x25, 0x01, 0x00, 
      //Beacon request type
      0x05,
      //Beacon request starts here
      0x0C, 0x01, 0x30, 0x00, 0x69, 0x00, 0x00,
      //BSSID
      0xFF, 0xFF, 0xFF, 0xFF, 0xff, 0xFF, 
      //SSID
      0x00, 0x05, 0x57, 0x69, 0x46, 0x69, 0x31, 
      //Reporting Condition
      0x01, 0x02, 0x00, 0x00,
      //Reporting Detail
      0x02, 0x01, 0x1,
      //Request IE
      0x0A, 0x05, 0x00, 0x30, 0x46, 0x36, 0xDD
      },
      {
         /*Neighbor report*/
      0x05, 0x05, 0x01,  
      //Measurement request IE
      0x34, 0x17,  
      //BSSID
      0xFF, 0xFF, 0xFF, 0xFF, 0xff, 0xFF, 
      //BSSID INFOo
      0xED, 0x01, 0x00, 0x00,
      //Reg class, channel, Phy type
      0x20, 0x01, 0x02, 
      //TSF Info
      0x01, 0x04, 0x02, 0x00, 0x60, 0x00,
      //Condensed country
      0x02, 0x02, 0x62, 0x63
      },
      {
         /* Link measurement request */
      0x05, 0x02, 0x00,
      //Txpower used
      0x00,
      //Max Tx Power
      0x00   
      }
   };

   if((psessionEntry = peFindSessionBySessionId(pMac,(tANI_U8)arg1) )== NULL)
   {
      p += log_sprintf( pMac,p,"Session does not exist for given session Id  \n");
      return p;
   }
   switch (arg2)
   {
      case 0:
      case 1:
      case 2:
      case 3:
         {
            tDot11fRadioMeasurementRequest *frm =
                    vos_mem_malloc(sizeof(tDot11fRadioMeasurementRequest));
            if (!frm)
            {
                p += log_sprintf(pMac, p,
                            "Unable to allocate memory to process command\n");
                break;
            }
            if( (status = dot11fUnpackRadioMeasurementRequest( pMac, &pBody[arg2][0], size[arg2], frm )) != 0 )
               p += log_sprintf( pMac, p, "failed to unpack.....status = %x\n", status);
            else
               rrmProcessRadioMeasurementRequest( pMac, psessionEntry->bssId, frm, psessionEntry );
            vos_mem_free(frm);
         }
         break;
      case 4:
         {
            tDot11fNeighborReportResponse *frm =
                    vos_mem_malloc(sizeof(tDot11fNeighborReportResponse));
            if (!frm)
            {
                p += log_sprintf(pMac, p,
                            "Unable to allocate memory to process command\n");
                break;
            }
            pBody[arg2][2] = (tANI_U8)arg3; //Dialog Token
            if( (status = dot11fUnpackNeighborReportResponse( pMac, &pBody[arg2][0], size[arg2], frm )) != 0 )
               p += log_sprintf( pMac, p, "failed to unpack.....status = %x\n", status);
            else
               rrmProcessNeighborReportResponse( pMac, frm, psessionEntry );
            vos_mem_free(frm);
         }
         break;
      case 5:
         {
// FIXME.
         }
         break;
      case 6:
         {
            tPowerdBm localConstraint = (tPowerdBm) arg3;
            tPowerdBm maxTxPower = cfgGetRegulatoryMaxTransmitPower( pMac, psessionEntry->currentOperChannel ); 
            maxTxPower = VOS_MIN( maxTxPower, maxTxPower-localConstraint );
            if( maxTxPower != psessionEntry->maxTxPower )
            {
               rrmSendSetMaxTxPowerReq( pMac, maxTxPower, psessionEntry );
               psessionEntry->maxTxPower = maxTxPower;
            }
         }
         break;
      default:
         p += log_sprintf( pMac, p, "Invalid option" );
         break;
   }
   return p;    
}
#endif

#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
#ifdef RSSI_HACK
/* This dump command is needed to set the RSSI values in TL while testing handoff. Handoff code was tested 
 * using this dump command. Whatever the value gives as the first parameter will be considered as the average 
 * RSSI by TL and invokes corresponding callback registered by the clients */
extern int dumpCmdRSSI;
static char *
dump_lim_set_tl_data_pkt_rssi( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    dumpCmdRSSI = arg1;
    limLog(pMac, LOGE, FL("Setting TL data packet RSSI to %d"), dumpCmdRSSI);
    return p;
}
#endif
#endif

#if defined WLAN_FEATURE_VOWIFI_11R
/* This command is used to trigger FT Preauthentication with the AP with BSSID below */
static char *
dump_lim_ft_event( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    static tANI_U8 macAddr[6] =  {0x00, 0xde, 0xad, 0xaf, 0xaf, 0x04};
    tpPESession psessionEntry;
    tSirMsgQ         msg;
    tpSirFTPreAuthReq pftPreAuthReq;
    tANI_U16 auth_req_len = 0;
    tCsrRoamConnectedProfile Profile;

    csrRoamCopyConnectProfile(pMac, arg2, &Profile);

    if((psessionEntry = peFindSessionBySessionId(pMac,(tANI_U8)arg2) )== NULL)
    {
        p += log_sprintf( pMac,
            p,"Session does not exist usage: 363 <0> sessionid channel \n");
        return p;
    }

    switch (arg1)
    {
         case 0:
              // Send Pre-auth event
              {
                   /*----------------*/
                   p += log_sprintf( pMac,p, "Preparing Pre Auth Req message\n");
                   auth_req_len = sizeof(tSirFTPreAuthReq);

                   pftPreAuthReq = vos_mem_malloc(auth_req_len);
                   if (NULL == pftPreAuthReq)
                   {
                       p += log_sprintf( pMac,p,"Pre auth dump: AllocateMemory() failed \n");
                       return p;
                   }
                   pftPreAuthReq->pbssDescription = vos_mem_malloc(sizeof(Profile.pBssDesc->length)+
                                                        Profile.pBssDesc->length);

                   pftPreAuthReq->messageType = eWNI_SME_FT_PRE_AUTH_REQ;
                   pftPreAuthReq->length = auth_req_len + sizeof(Profile.pBssDesc->length) +
                       Profile.pBssDesc->length;
                   pftPreAuthReq->preAuthchannelNum = 6; 

                   vos_mem_copy((void *) &pftPreAuthReq->currbssId,
                       (void *)psessionEntry->bssId, 6);  
                   vos_mem_copy((void *) &pftPreAuthReq->preAuthbssId,
                       (void *)macAddr, 6);  
                   pftPreAuthReq->ft_ies_length = (tANI_U16)pMac->ft.ftSmeContext.auth_ft_ies_length;

                   // Also setup the mac address in sme context.
                   vos_mem_copy(pMac->ft.ftSmeContext.preAuthbssId, macAddr, 6);

                   vos_mem_copy(pftPreAuthReq->ft_ies, pMac->ft.ftSmeContext.auth_ft_ies, 
                       pMac->ft.ftSmeContext.auth_ft_ies_length);

                   vos_mem_copy(Profile.pBssDesc->bssId, macAddr, 6);

                   p += log_sprintf( pMac,p, "\n ----- LIM Debug Information ----- \n");
                   p += log_sprintf( pMac, p, "%s: length = %d\n", __func__, 
                            (int)pMac->ft.ftSmeContext.auth_ft_ies_length);
                   p += log_sprintf( pMac, p, "%s: length = %02x\n", __func__, 
                            (int)pMac->ft.ftSmeContext.auth_ft_ies[0]);
                   p += log_sprintf( pMac, p, "%s: Auth Req %02x %02x %02x\n", 
                            __func__, pftPreAuthReq->ft_ies[0],
                            pftPreAuthReq->ft_ies[1], pftPreAuthReq->ft_ies[2]);

                   p += log_sprintf( pMac, p, "%s: Session %02x %02x %02x\n", __func__, 
                            psessionEntry->bssId[0],
                            psessionEntry->bssId[1], psessionEntry->bssId[2]);
                   p += log_sprintf( pMac, p, "%s: Session %02x %02x %02x %p\n", __func__, 
                            pftPreAuthReq->currbssId[0],
                            pftPreAuthReq->currbssId[1], 
                            pftPreAuthReq->currbssId[2], pftPreAuthReq);

                   Profile.pBssDesc->channelId = (tANI_U8)arg3;
                   vos_mem_copy((void *)pftPreAuthReq->pbssDescription, (void *)Profile.pBssDesc, 
                       Profile.pBssDesc->length);  

                   msg.type = eWNI_SME_FT_PRE_AUTH_REQ;
                   msg.bodyptr = pftPreAuthReq;
                   msg.bodyval = 0;

                   p += log_sprintf( pMac, p, "limPostMsgApi(eWNI_SME_FT_PRE_AUTH_REQ) \n");
                   limPostMsgApi(pMac, &msg);
              }
              break;

         default:
              break;
    }
    return p;    
}
#endif
static char *
dump_lim_channel_switch_announcement( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    tpPESession psessionEntry;
    tANI_U8 nMode = arg2;
    tANI_U8 nNewChannel = arg3;
    tANI_U8 nCount = arg4;
  tANI_U8 peer[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    if((psessionEntry = peFindSessionBySessionId(pMac,(tANI_U8)arg1) )== NULL)
    {
        p += log_sprintf( pMac,
            p,"Session does not exist usage: 363 <0> sessionid channel \n");
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_WARN,"Session Not found!!!!");
        return p;
    }

    limSendChannelSwitchMgmtFrame( pMac, peer, nMode, nNewChannel, nCount, psessionEntry );

    psessionEntry->gLimChannelSwitch.switchCount = nCount;
    psessionEntry->gLimSpecMgmt.dot11hChanSwState = eLIM_11H_CHANSW_RUNNING;
    psessionEntry->gLimChannelSwitch.switchMode = nMode;
    psessionEntry->gLimChannelSwitch.primaryChannel = nNewChannel;

    schSetFixedBeaconFields(pMac, psessionEntry);
    limSendBeaconInd(pMac, psessionEntry); 

  return p;
}

#ifdef WLAN_FEATURE_11AC
static char *
dump_lim_vht_opmode_notification(tpAniSirGlobal pMac, tANI_U32 arg1,tANI_U32 arg2,tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    tANI_U8 peer[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    tANI_U8 nMode = arg2;
    tpPESession psessionEntry;

    if((psessionEntry = peFindSessionBySessionId(pMac,(tANI_U8)arg1) )== NULL)
    {
        p += log_sprintf( pMac,
            p,"Session does not exist usage: 366 <0> sessionid channel \n");
        return p;
    }
    
    limSendVHTOpmodeNotificationFrame(pMac, peer, nMode,psessionEntry);
    
    psessionEntry->gLimOperatingMode.present = 1;
    psessionEntry->gLimOperatingMode.chanWidth = nMode;
    psessionEntry->gLimOperatingMode.rxNSS   = 0;
    psessionEntry->gLimOperatingMode.rxNSSType    = 0;

    schSetFixedBeaconFields(pMac, psessionEntry);
    limSendBeaconInd(pMac, psessionEntry); 

    return p;
}

static char *
dump_lim_vht_channel_switch_notification(tpAniSirGlobal pMac, tANI_U32 arg1,tANI_U32 arg2,tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    tpPESession psessionEntry;
    tANI_U8 nChanWidth = arg2;
    tANI_U8 nNewChannel = arg3;
    tANI_U8 ncbMode = arg4;
    tANI_U8 peer[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    if((psessionEntry = peFindSessionBySessionId(pMac,(tANI_U8)arg1) )== NULL)
    {
        p += log_sprintf( pMac,
            p,"Session does not exist usage: 367 <0> sessionid channel \n");
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_WARN,"Session Not found!!!!");
        return p;
    }

    limSendVHTChannelSwitchMgmtFrame( pMac, peer, nChanWidth, nNewChannel, (ncbMode+1), psessionEntry );

    psessionEntry->gLimChannelSwitch.switchCount = 0;
    psessionEntry->gLimSpecMgmt.dot11hChanSwState = eLIM_11H_CHANSW_RUNNING;
    psessionEntry->gLimChannelSwitch.switchMode = 1;
    psessionEntry->gLimChannelSwitch.primaryChannel = nNewChannel;
    psessionEntry->gLimWiderBWChannelSwitch.newChanWidth = nChanWidth;
    psessionEntry->gLimWiderBWChannelSwitch.newCenterChanFreq0 = limGetCenterChannel(pMac,nNewChannel,(ncbMode+1),nChanWidth);
    psessionEntry->gLimWiderBWChannelSwitch.newCenterChanFreq1 = 0;
    
    schSetFixedBeaconFields(pMac, psessionEntry);
    limSendBeaconInd(pMac, psessionEntry);    

    return p;
}

#endif
static char *
dump_lim_cancel_channel_switch_announcement( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    tpPESession psessionEntry;

    if((psessionEntry = peFindSessionBySessionId(pMac,(tANI_U8)arg1) )== NULL)
    {
        p += log_sprintf( pMac,
            p,"Session does not exist usage: 363 <0> sessionid channel \n");
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_WARN,"Session Not found!!!!");
        return p;
    }
    psessionEntry->gLimChannelSwitch.switchCount = 0;
    psessionEntry->gLimSpecMgmt.dot11hChanSwState = eLIM_11H_CHANSW_INIT;
    psessionEntry->gLimChannelSwitch.switchMode = 0;
    psessionEntry->gLimChannelSwitch.primaryChannel = 0;

    schSetFixedBeaconFields(pMac, psessionEntry);
    limSendBeaconInd(pMac, psessionEntry); 

  return p;
}


static char *
dump_lim_mcc_policy_maker(tpAniSirGlobal pMac, tANI_U32 arg1,tANI_U32 arg2,tANI_U32 arg3, tANI_U32 arg4, char *p)
{
   VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_FATAL, "dump_lim_mcc_policy_maker arg = %d",arg1);
    
   if(arg1 == 0) //Disable feature completely
   {  
      WDA_TrafficStatsTimerActivate(FALSE);
      if (ccmCfgSetInt(pMac, WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED, FALSE,
          NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
      {
         limLog( pMac, LOGE, FL("Could not get WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED"));
      }
   }
   else if(arg1 == 1) //Enable feature
   {   
      if (ccmCfgSetInt(pMac, WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED, TRUE,
         NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
      {
        limLog( pMac, LOGE, FL("Could not set WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED"));
      }    
   }
   else if(arg1 == 2) //Enable feature and activate periodic timer
   {   
      if (ccmCfgSetInt(pMac, WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED, TRUE,
          NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
      {
         limLog( pMac, LOGE, FL("Could not set WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED"));
      }
      WDA_TrafficStatsTimerActivate(TRUE);
   }
   else if(arg1 == 3) //Enable only stats collection - Used for unit testing
   {
      VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_FATAL, "Enabling Traffic Stats in DTS");
      WDI_DS_ActivateTrafficStats();
   }
   else if(arg1 == 4) //Send current stats snapshot to Riva -- Used for unit testing
   {
      v_VOID_t * pVosContext = vos_get_global_context(VOS_MODULE_ID_WDA, NULL);
      tWDA_CbContext *pWDA =  vos_get_context(VOS_MODULE_ID_WDA, pVosContext);
      ccmCfgSetInt(pMac, WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED, TRUE, NULL, eANI_BOOLEAN_FALSE);
      if(pWDA != NULL)
      {
          WDA_TimerTrafficStatsInd(pWDA);
      }
      WDA_TrafficStatsTimerActivate(FALSE);
      ccmCfgSetInt(pMac, WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED, FALSE,NULL, eANI_BOOLEAN_FALSE);
   }
   else if (arg1 == 5) //Change the periodicity of TX stats timer
   {
      v_VOID_t * pVosContext = vos_get_global_context(VOS_MODULE_ID_WDA, NULL);
      tWDA_CbContext *pWDA =  vos_get_context(VOS_MODULE_ID_WDA, pVosContext);
      if (pWDA != NULL && tx_timer_change(&pWDA->wdaTimers.trafficStatsTimer, arg2/10, arg2/10) != TX_SUCCESS)
      {
          limLog(pMac, LOGP, FL("Disable timer before changing timeout value"));
      }
   }
   return p;
}

#ifdef WLANTL_DEBUG
/* API to print number of pkts received based on rate index */
/* arg1 = station Id */
/* arg2 = BOOLEAN value to either or not flush the counters */
static char *
dump_lim_get_pkts_rcvd_per_rate_idx( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    /* if anything other than 1, then we need not flush the counters */
    if( arg2 != 1)
        arg2 = FALSE;

    WLANTLPrintPktsRcvdPerRateIdx(pMac->roam.gVosContext, (tANI_U8)arg1, (v_BOOL_t)arg2);
    return p;
}

/* API to print number of pkts received based on rssi */
/* arg1 = station Id */
/* arg2 = BOOLEAN value to either or not flush the counters */
static char *
dump_lim_get_pkts_rcvd_per_rssi_values( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    /* if anything other than 1, then we need not flush the counters */
    if( arg2 != 1)
        arg2 = FALSE;

    WLANTLPrintPktsRcvdPerRssi(pMac->roam.gVosContext, (tANI_U8)arg1, (v_BOOL_t)arg2);
    return p;
}
#endif


static char *
dump_set_max_probe_req(tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2,
             tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    if ((arg1 <= 0) || (arg1 > 4)){
       limLog(pMac, LOGE,
           FL("invalid number. valid range 1 - 4 \n"));
       return p;
    }
    pMac->lim.maxProbe = arg1;
    return p;
}
/* API to fill Rate Info based on mac efficiency
 * arg 1: mac efficiency to be used to calculate mac thorughput for a given rate index
 * arg 2: starting rateIndex to apply the macEfficiency to
 * arg 3: ending rateIndex to apply the macEfficiency to
 */
static char *
dump_limRateInfoBasedOnMacEff(tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    limLog(pMac, LOGE, FL("arg1 %u, arg2 %u, arg3 %u"), arg1, arg2, arg3);
    WDTS_FillRateInfo((tANI_U8)(arg1), (tANI_U16)(arg2), (tANI_U16)(arg3));
    return p;
}

static tDumpFuncEntry limMenuDumpTable[] = {
    {0,     "PE (300-499)",                                          NULL},
    {300,   "LIM: Dump state(s)/statistics <session id>",            dump_lim_info},
    {301,   "PE.LIM: dump TSPEC Table",                              dump_lim_tspec_table},
    {302,   "PE.LIM: dump specified TSPEC entry (id)",               dump_lim_tspec_entry},
    {303,   "PE.LIM: dump EDCA params",                              dump_lim_edca_params},
    {304,   "PE.LIM: dump DPH table summary",                        dump_lim_dph_table_summary},
    {305,   "PE.LIM: dump link monitor stats",                       dump_lim_link_monitor_stats},
    {306,   "PE.LIM:dump Set the BAR Decline stat(arg1= 1/0 (enable/disable) arg2 =TID",          dump_lim_AddBA_DeclineStat},
    {307,   "PE: LIM: dump CSR Send ReAssocReq",                     dump_lim_sme_reassoc_req},
    {308,   "PE:LIM: dump all 11H related data",                     dump_lim_dot11h_stats},
    {309,   "PE:LIM: dump to enable Measurement on AP",              dump_lim_enable_measurement},
    {310,   "PE:LIM: dump to enable QuietIE on AP",                  dump_lim_enable_quietIE},
    {311,   "PE:LIM: disable/enable scan 1(disable)",                dump_lim_disable_enable_scan},    
    {320,   "PE.LIM: send sme scan request",                         dump_lim_scan_req_send},


    /*FIXME_GEN6*/
    /* This dump command is more of generic dump cmd and hence it should 
     * be moved to logDump.c
     */
    {321,   "PE:LIM: Set Log Level <VOS Module> <VOS Log Level>",    dump_lim_update_log_level},
    {331,   "PE.LIM: Send finish scan to LIM",                       dump_lim_finishscan_send},
    {332,   "PE.LIM: force probe rsp send from LIM",                 dump_lim_prb_rsp_send},
    {333,   "PE.SCH: Trigger to generate a beacon",                  dump_sch_beacon_trigger},
    {335,   "PE.LIM: set ACM flag (0..3)",                           dump_lim_acm_set},
    {336,   "PE.LIM: Send an ADDBA Req to peer MAC arg1=aid,arg2=tid, arg3=ssn",   dump_lim_addba_req},
    {337,   "PE.LIM: Send a DELBA Ind to peer MAC arg1=aid,arg2=recipient(0)/initiator(1),arg3=tid,arg4=reasonCode",    dump_lim_delba_req},
    {338,   "PE.LIM: Trigger a BA timeout for STA index",            dump_lim_ba_timeout},
    {339,   "PE.LIM: List active BA session(s) for AssocID",         dump_lim_list_active_ba},
    {340,   "PE.LIM: Set background scan flag (0-disable, 1-enable)",dump_lim_bgscan_toggle},
    {341,   "PE.LIM: Set link monitoring mode",                      dump_lim_linkmonitor_toggle},
    {342,   "PE.LIM: AddSta <6th byte of station Mac>",              dump_lim_add_sta},
    {343,   "PE.LIM: DelSta <aid>",                                  dump_lim_del_sta},
    {344,   "PE.LIM: Set probe respond flag",                        dump_lim_proberesp_toggle},
    {345,   "PE.LIM: set protection config bitmap",                  set_lim_prot_cfg},
    {346,   "PE:LIM: Set the Dot11 Mode",                            dump_lim_set_dot11_mode},
    {347,   "PE:Enable or Disable Protection",                       dump_lim_set_protection_control},
    {348,   "PE:LIM: Send SM Power Mode Action frame",               dump_lim_send_SM_Power_Mode},
    {349,   "PE: LIM: Change CB Mode <session id> <sec chnl offset>",dump_lim_update_cb_Mode},
    {350,   "PE: LIM: abort scan",                                   dump_lim_abort_scan},
    {351,   "PE: LIM: Start stop BG scan",                           dump_lim_start_stop_bg_scan},
    {352,   "PE: LIM: PE statistics <scanmask>",                     dump_lim_get_pe_statistics},
    {353,   "PE: LIM: Set MAC log level <Mac Module ID> <Log Level>", dump_lim_set_log_level},
    {354,   "PE: LIM: Set Scan in Power Save <0-disable, 1-enable>",  dump_lim_set_scan_in_powersave},
    {355,   "PE.LIM: send sme start BSS request",                    dump_lim_send_start_bss_req},
    {356,   "PE.LIM: dump pesession info ",                          dump_lim_session_print},
    {357,   "PE.LIM: send DisAssocRequest",                          dump_lim_send_disassoc_req},
    {358,   "PE.LIM: send sme stop bss request <session ID>",        dump_lim_stop_bss_req}, 
    {359,   "PE.LIM: send sme join request",                         dump_lim_send_join_req},
#if defined WLAN_FEATURE_VOWIFI
    {360,   "PE.LIM: send an RRM action frame",                      dump_lim_send_rrm_action},
    {361,   "PE.LIM: unpack an RRM action frame",                    dump_lim_unpack_rrm_action},
#endif
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
#ifdef RSSI_HACK
    {362,   "TL Set current RSSI",                                   dump_lim_set_tl_data_pkt_rssi},
#endif
#endif
#ifdef WLAN_FEATURE_VOWIFI_11R
    {363,   "PE.LIM: trigger pre auth/reassoc event",                dump_lim_ft_event},
#endif
    {364,   "PE.LIM: Send a channel switch announcement",            dump_lim_channel_switch_announcement},
    {365,   "PE.LIM: Cancel channel switch announcement",            dump_lim_cancel_channel_switch_announcement},
#ifdef WLAN_FEATURE_11AC
    {366,   "PE.LIM: Send a VHT OPMode Action Frame",                dump_lim_vht_opmode_notification},
    {367,   "PE.LIM: Send a VHT Channel Switch Announcement",        dump_lim_vht_channel_switch_notification},
    {368,   "PE.LIM: MCC Policy Maker",                              dump_lim_mcc_policy_maker},
#endif
#ifdef WLANTL_DEBUG
    {369,   "PE.LIM: pkts/rateIdx: iwpriv wlan0 dump 368 <staId> <boolean to flush counter>",    dump_lim_get_pkts_rcvd_per_rate_idx},
    {370,   "PE.LIM: pkts/rssi: : iwpriv wlan0 dump 369 <staId> <boolean to flush counter>",    dump_lim_get_pkts_rcvd_per_rssi_values},
#endif
    {374,   "PE.LIM: MAS RX stats MAC eff <MAC eff in percentage>",  dump_limRateInfoBasedOnMacEff},
    {376,   "PE.LIM: max number of probe per scan", dump_set_max_probe_req },
};



void limDumpInit(tpAniSirGlobal pMac)
{
    logDumpRegisterTable( pMac, &limMenuDumpTable[0], 
                          sizeof(limMenuDumpTable)/sizeof(limMenuDumpTable[0]) );
}


#endif //#if defined(ANI_LOGDUMP)

