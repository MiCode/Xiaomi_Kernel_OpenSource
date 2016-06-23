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

/*===========================================================================

                      limProcessTdls.c 

  OVERVIEW:

  DEPENDENCIES:

  Are listed for each API below.
===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


   $Header$$DateTime$$Author$


  when        who     what, where, why
----------    ---    --------------------------------------------------------
05/05/2010   Ashwani    Initial Creation, added TDLS action frame functionality,
                         TDLS message exchange with SME..etc..

===========================================================================*/


/**
 * \file limProcessTdls.c
 *
 * \brief Code for preparing,processing and sending 802.11z action frames
 *
 */

#ifdef FEATURE_WLAN_TDLS

#include "sirApi.h"
#include "aniGlobal.h"
#include "sirMacProtDef.h"
#include "cfgApi.h"
#include "utilsApi.h"
#include "limTypes.h"
#include "limUtils.h"
#include "limSecurityUtils.h"
#include "dot11f.h"
#include "limStaHashApi.h"
#include "schApi.h"
#include "limSendMessages.h"
#include "utilsParser.h"
#include "limAssocUtils.h"
#include "dphHashTable.h"
#include "wlan_qct_wda.h"

/* define NO_PAD_TDLS_MIN_8023_SIZE to NOT padding: See CR#447630
There was IOT issue with cisco 1252 open mode, where it pads
discovery req/teardown frame with some junk value up to min size.
To avoid this issue, we pad QCOM_VENDOR_IE.
If there is other IOT issue because of this bandage, define NO_PAD...
*/
#ifndef NO_PAD_TDLS_MIN_8023_SIZE
#define MIN_IEEE_8023_SIZE              46
#define MIN_VENDOR_SPECIFIC_IE_SIZE     5
#endif


#ifdef FEATURE_WLAN_TDLS_INTERNAL
/* forword declarations */
static tSirRetStatus limTdlsDisAddSta(tpAniSirGlobal pMac, tSirMacAddr peerMac,
                   tSirTdlsPeerInfo *peerInfo, tpPESession psessionEntry) ;
static eHalStatus limSendSmeTdlsLinkSetupInd(tpAniSirGlobal pMac, 
                                   tSirMacAddr peerMac, tANI_U8 status);
static eHalStatus limSendSmeTdlsDelPeerInd(tpAniSirGlobal pMac, 
                 tANI_U8 sessionId, tDphHashNode   *pStaDs, tANI_U8 status) ;
static tSirTdlsPeerInfo *limTdlsFindDisPeerByState(tpAniSirGlobal pMac, 
                                                            tANI_U8 state);
static tANI_U8 limTdlsFindSetupPeerByState(tpAniSirGlobal pMac, tANI_U8 state, 
                                     tLimTdlsLinkSetupPeer **setupPeer) ;
static tSirRetStatus limTdlsLinkEstablish(tpAniSirGlobal pMac, tSirMacAddr peer_mac);

static tSirRetStatus limTdlsLinkTeardown(tpAniSirGlobal pMac, tSirMacAddr peer_mac);
static tpDphHashNode limTdlsDelSta(tpAniSirGlobal pMac, tSirMacAddr peerMac, 
                                                 tpPESession psessionEntry) ;

#endif
static tSirRetStatus limTdlsSetupAddSta(tpAniSirGlobal pMac,
                                        tSirTdlsAddStaReq *pAddStaReq,
                                        tpPESession psessionEntry) ;
void PopulateDot11fLinkIden(tpAniSirGlobal pMac, tpPESession psessionEntry,
                          tDot11fIELinkIdentifier *linkIden, 
                             tSirMacAddr peerMac, tANI_U8 reqType) ;
void PopulateDot11fTdlsExtCapability(tpAniSirGlobal pMac, 
                                    tDot11fIEExtCap *extCapability) ;

void PopulateDot11fTdlsOffchannelParams(tpAniSirGlobal pMac,
          tpPESession psessionEntry,
          tDot11fIESuppChannels *suppChannels,
          tDot11fIESuppOperatingClasses *suppOperClasses);

void limLogVHTCap(tpAniSirGlobal pMac,
                              tDot11fIEVHTCaps *pDot11f);
tSirRetStatus limPopulateVhtMcsSet(tpAniSirGlobal pMac,
                                  tpSirSupportedRates pRates,
                                  tDot11fIEVHTCaps *pPeerVHTCaps,
                                  tpPESession psessionEntry);
ePhyChanBondState  limGetHTCBState(ePhyChanBondState aniCBMode);
/*only 31 op classes are available, 1 entry for current op class*/
static tDot11fIESuppOperatingClasses op_classes = {0};

op_class_map_t global_op_class[] = {
    {81, 25,  BW20,      {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}},
    {82, 25,  BW20,      {14}},
    {83, 40,  BW40PLUS,  {1, 2, 3, 4, 5, 6, 7, 8, 9}},
    {84, 40,  BW40MINUS, {5, 6, 7, 8, 9, 10, 11, 12, 13}},
    {115, 20, BW20,      {36, 40, 44, 48}},
    {116, 40, BW40PLUS,  {36, 44}},
    {117, 40, BW40MINUS, {40, 48}},
    {118, 20, BW20,      {52, 56, 60, 64}},
    {119, 40, BW40PLUS,  {52, 60}},
    {120, 40, BW40MINUS, {56, 64}},
    {121, 20, BW20,   {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140}},
    {122, 40, BW40PLUS,  {100, 108, 116, 124, 132}},
    {123, 40, BW40MINUS, {104, 112, 120, 128, 136}},
    {125, 20, BW20,      {149, 153, 157, 161, 165, 169}},
    {126, 40, BW40PLUS,  {149, 157}},
    {127, 40, BW40MINUS, {153, 161}},
    {0, 0, 0, {0}},

};/*end global_op_class*/

op_class_map_t us_op_class[] = {
    {1, 20,  BW20,       {36, 40, 44, 48}},
    {2, 20,  BW20,       {52, 56, 60, 64}},
    {4, 20,  BW20,   {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140}},
    {5, 20,  BW20,       {149, 153, 157, 161, 165}},
    {22, 40, BW40PLUS,  {36, 44}},
    {23, 40, BW40PLUS,  {52, 60}},
    {24, 40, BW40PLUS,  {100, 108, 116, 124, 132}},
    {26, 40, BW40PLUS,  {149, 157}},
    {27, 40, BW40MINUS, {40, 48}},
    {28, 40, BW40MINUS, {56, 64}},
    {29, 40, BW40MINUS, {104, 112, 120, 128, 136}},
    {31, 40, BW40MINUS, {153, 161}},
    {32, 40, BW40PLUS,  {1, 2, 3, 4, 5, 6, 7}},
    {33, 40, BW40MINUS, {5, 6, 7, 8, 9, 10, 11}},
    {0, 0, 0, {0}},
};/*end us_op_class*/

op_class_map_t euro_op_class[] = {
    {1, 20,  BW20,      {36, 40, 44, 48}},
    {2, 20,  BW20,      {52, 56, 60, 64}},
    {3, 20,  BW20,   {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140}},
    {4, 25,  BW20,      {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}},
    {5, 40,  BW40PLUS,  {36, 44}},
    {6, 40,  BW40PLUS,  {52, 60}},
    {7, 40,  BW40PLUS,  {100, 108, 116, 124, 132}},
    {8, 40,  BW40MINUS, {40, 48}},
    {9, 40,  BW40MINUS, {56, 64}},
    {10, 40, BW40MINUS, {104, 112, 120, 128, 136}},
    {11, 40, BW40PLUS,  {1, 2, 3, 4, 5, 6, 7, 8, 9}},
    {12, 40, BW40MINUS, {5, 6, 7, 8, 9, 10, 11, 12, 13}},
    {17, 20, BW20,      {149, 153, 157, 161, 165, 169}},
    {0, 0, 0, {0}},
};/*end euro_op_class*/

op_class_map_t japan_op_class[] = {
    {1, 20,  BW20,      {36, 40, 44, 48}},
    {30, 25, BW20,      {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}},
    {31, 25, BW20,      {14}},
    {32, 20, BW20,      {52, 56, 60, 64}},
    {34, 20, BW20,   {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140}},
    {36, 40, BW40PLUS,  {36, 44}},
    {37, 40, BW40PLUS,  {52, 60}},
    {39, 40, BW40PLUS,  {100, 108, 116, 124, 132}},
    {41, 40, BW40MINUS, {40, 48}},
    {42, 40, BW40MINUS, {56, 64}},
    {44, 40, BW40MINUS, {104, 112, 120, 128, 136}},
    {0, 0, 0, {0}},
};/*end japan_op_class*/

/*
 * TDLS data frames will go out/come in as non-qos data.
 * so, eth_890d_header will be aligned access..
 */
static const tANI_U8 eth_890d_header[] = 
{ 
    0xaa, 0xaa, 0x03, 0x00, 
    0x00, 0x00, 0x89, 0x0d,
} ;

/*
 * type of links used in TDLS 
 */
enum tdlsLinks
{
    TDLS_LINK_AP,
    TDLS_LINK_DIRECT
} eTdlsLink ;

/* 
 * node status in node searching
 */
enum tdlsLinkNodeStatus
{
    TDLS_NODE_NOT_FOUND,
    TDLS_NODE_FOUND
} eTdlsLinkNodeStatus ;


enum tdlsReqType
{
    TDLS_INITIATOR,
    TDLS_RESPONDER
} eTdlsReqType ;

typedef enum tdlsLinkSetupStatus
{
    TDLS_SETUP_STATUS_SUCCESS = 0,
    TDLS_SETUP_STATUS_FAILURE = 37
}etdlsLinkSetupStatus ;

/* These maps to Kernel TDLS peer capability
 * flags and should get changed as and when necessary
 */
enum tdls_peer_capability {
        TDLS_PEER_HT_CAP  = 0,
        TDLS_PEER_VHT_CAP = 1,
        TDLS_PEER_WMM_CAP = 2
} eTdlsPeerCapability;

/* some local defines */
#define LINK_IDEN_BSSID_OFFSET      (0)
#define PEER_MAC_OFFSET   (12) 
#define STA_MAC_OFFSET    (6)
#define LINK_IDEN_ELE_ID  (101)
//#define LINK_IDEN_LENGTH   (18) 
#define LINK_IDEN_ADDR_OFFSET(x) (&x.LinkIdentifier)
#define PTI_LINK_IDEN_OFFSET     (5)
#define PTI_BUF_STATUS_OFFSET    (25)

/* TODO, Move this parameters to configuration */
#define PEER_PSM_SUPPORT          (0)
#define PEER_BUFFER_STA_SUPPORT   (0)
#define CH_SWITCH_SUPPORT         (0)
#define TDLS_SUPPORT              (1)
#define TDLS_PROHIBITED           (0)
#define TDLS_CH_SWITCH_PROHIBITED (1)
/** @brief Set bit manipulation macro */
#define SET_BIT(value,mask)       ((value) |= (1 << (mask)))
/** @brief Clear bit manipulation macro */
#define CLEAR_BIT(value,mask)     ((value) &= ~(1 << (mask)))
/** @brief Check bit manipulation macro */
#define CHECK_BIT(value, mask)    ((value) & (1 << (mask)))

#define SET_PEER_AID_BITMAP(peer_bitmap, aid) \
                                if ((aid) < (sizeof(tANI_U32) << 3)) \
                                        SET_BIT(peer_bitmap[0], (aid)); \
                                else if ((aid) < (sizeof(tANI_U32) << 4)) \
                                        SET_BIT(peer_bitmap[1], ((aid) - (sizeof(tANI_U32) << 3)));

#define CLEAR_PEER_AID_BITMAP(peer_bitmap, aid) \
                                if ((aid) < (sizeof(tANI_U32) << 3)) \
                                        CLEAR_BIT(peer_bitmap[0], (aid)); \
                                else if ((aid) < (sizeof(tANI_U32) << 4)) \
                                        CLEAR_BIT(peer_bitmap[1], ((aid) - (sizeof(tANI_U32) << 3)));


#ifdef LIM_DEBUG_TDLS

#ifdef FEATURE_WLAN_TDLS
#define WNI_CFG_TDLS_DISCOVERY_RSP_WAIT             (100)
#define WNI_CFG_TDLS_LINK_SETUP_RSP_TIMEOUT         (800)
#define WNI_CFG_TDLS_LINK_SETUP_CNF_TIMEOUT         (200)
#endif

#define IS_QOS_ENABLED(psessionEntry) ((((psessionEntry)->limQosEnabled) && \
                                                  SIR_MAC_GET_QOS((psessionEntry)->limCurrentBssCaps)) || \
                                       (((psessionEntry)->limWmeEnabled ) && \
                                                  LIM_BSS_CAPS_GET(WME, (psessionEntry)->limCurrentBssQosCaps)))

#define TID_AC_VI                  4
#define TID_AC_BK                  1

const tANI_U8* limTraceTdlsActionString( tANI_U8 tdlsActionCode )
{
   switch( tdlsActionCode )
   {
       CASE_RETURN_STRING(SIR_MAC_TDLS_SETUP_REQ);
       CASE_RETURN_STRING(SIR_MAC_TDLS_SETUP_RSP);
       CASE_RETURN_STRING(SIR_MAC_TDLS_SETUP_CNF);
       CASE_RETURN_STRING(SIR_MAC_TDLS_TEARDOWN);
       CASE_RETURN_STRING(SIR_MAC_TDLS_PEER_TRAFFIC_IND);
       CASE_RETURN_STRING(SIR_MAC_TDLS_CH_SWITCH_REQ);
       CASE_RETURN_STRING(SIR_MAC_TDLS_CH_SWITCH_RSP);
       CASE_RETURN_STRING(SIR_MAC_TDLS_PEER_TRAFFIC_RSP);
       CASE_RETURN_STRING(SIR_MAC_TDLS_DIS_REQ);
       CASE_RETURN_STRING(SIR_MAC_TDLS_DIS_RSP);
   }
   return (const tANI_U8*)"UNKNOWN";
}
#endif
#if 0
static void printMacAddr(tSirMacAddr macAddr)
{
    int i = 0 ;
    VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, (" Mac Addr: "));

    for(i = 0 ; i < 6; i++)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
                                                 (" %02x "), macAddr[i]);
    }
    VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, (""));
    return ;
}
#endif
/*
 * initialize TDLS setup list and related data structures.
 */
void limInitTdlsData(tpAniSirGlobal pMac, tpPESession pSessionEntry)
{
#ifdef FEATURE_WLAN_TDLS_INTERNAL
    pMac->lim.gLimTdlsDisResultList = NULL ;
    pMac->lim.gLimTdlsDisStaCount = 0 ;
    vos_mem_set(&pMac->lim.gLimTdlsDisReq, sizeof(tSirTdlsDisReq), 0);
    vos_mem_set(&pMac->lim.gLimTdlsLinkSetupInfo, sizeof(tLimTdlsLinkSetupInfo), 0);
    pMac->lim.gAddStaDisRspWait = 0 ;

#ifdef FEATURE_WLAN_TDLS_NEGATIVE
    /* when reassociated, negative behavior will not be kept */
    /* you have to explicitly enable negative behavior per (re)association */
    pMac->lim.gLimTdlsNegativeBehavior = 0;
#endif
#endif
    limInitPeerIdxpool(pMac, pSessionEntry) ;

    return ;
}
#ifdef FEATURE_WLAN_TDLS_NEGATIVE
void limTdlsSetNegativeBehavior(tpAniSirGlobal pMac, tANI_U8 value, tANI_BOOLEAN on)
{
    if(on) {
        if(value == 255)
            pMac->lim.gLimTdlsNegativeBehavior = 0XFFFFFFFF;
        else
            pMac->lim.gLimTdlsNegativeBehavior |= (1 << (value-1));
    }
    else {
        if(value == 255)
            pMac->lim.gLimTdlsNegativeBehavior = 0;
        else
            pMac->lim.gLimTdlsNegativeBehavior &= ~(1 << (value-1));
    }
    LIM_LOG_TDLS(VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR,("%d %d -> gLimTdlsNegativeBehavior= 0x%lx"),
        value, on, pMac->lim.gLimTdlsNegativeBehavior));
}
#endif
#if 0
/*
 * This function is used for creating TDLS public Action frame to
 * transmit on Direct link
 */
static void limPreparesActionFrameHdr(tpAniSirGlobal pMac, tANI_U8 *pFrame,
                                         tANI_U8 type, tANI_U8 subType,
                                                   tANI_U8 *link_iden )
{
    tpSirMacMgmtHdr pMacHdr ;
    tANI_U8 *bssid = link_iden ;
#if 0     
    tANI_U8 *staMac = (tANI_U8 *)(bssid + sizeof(tSirMacAddr)) ;
    tANI_U8 *peerMac = (tANI_U8 *) (staMac + sizeof(tSirMacAddr)) ;
#else    
   tANI_U8 *peerMac = (tANI_U8 *) (bssid + sizeof(tSirMacAddr)) ;
   tANI_U8 *staMac = (tANI_U8 *)(peerMac + sizeof(tSirMacAddr)) ;
#endif    
    tANI_U8 toDs =  ANI_TXDIR_IBSS  ;

    pMacHdr = (tpSirMacMgmtHdr) (pFrame);

    /*
     * prepare 802.11 header
     */ 
    pMacHdr->fc.protVer = SIR_MAC_PROTOCOL_VERSION;
    pMacHdr->fc.type    = type ;
    pMacHdr->fc.subType = subType ;
    /*
     * TL is not setting up below fields, so we are doing it here
     */
    pMacHdr->fc.toDS    = toDs ;
    pMacHdr->fc.powerMgmt = 0 ;

     
    vos_mem_copy( (tANI_U8 *) pMacHdr->da, peerMac, sizeof( tSirMacAddr ));
    vos_mem_copy( (tANI_U8 *) pMacHdr->sa,
                   staMac, sizeof( tSirMacAddr ));

    vos_mem_copy( (tANI_U8 *) pMacHdr->bssId,
                                bssid, sizeof( tSirMacAddr ));
   
   LIM_LOG_TDLS(VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_WARN, ("Preparing TDLS action frame\n%02x:%02x:%02x:%02x:%02x:%02x/%02x:%02x:%02x:%02x:%02x:%02x/%02x:%02x:%02x:%02x:%02x:%02x"),
       pMacHdr->da[0], pMacHdr->da[1], pMacHdr->da[2], pMacHdr->da[3], pMacHdr->da[4], pMacHdr->da[5],
       pMacHdr->sa[0], pMacHdr->sa[1], pMacHdr->sa[2], pMacHdr->sa[3], pMacHdr->sa[4], pMacHdr->sa[5],
       pMacHdr->bssId[0], pMacHdr->bssId[1], pMacHdr->bssId[2],
       pMacHdr->bssId[3], pMacHdr->bssId[4], pMacHdr->bssId[5]));

    return ; 
}
#endif
/*
 * prepare TDLS frame header, it includes
 * |             |              |                |
 * |802.11 header|RFC1042 header|TDLS_PYLOAD_TYPE|PAYLOAD
 * |             |              |                |
 */
static tANI_U32 limPrepareTdlsFrameHeader(tpAniSirGlobal pMac, tANI_U8* pFrame, 
           tDot11fIELinkIdentifier *link_iden, tANI_U8 tdlsLinkType, tANI_U8 reqType,
           tANI_U8 tid, tpPESession psessionEntry)
{
    tpSirMacDataHdr3a pMacHdr ;
    tANI_U32 header_offset = 0 ;
    tANI_U8 *addr1 = NULL ;
    tANI_U8 *addr3 = NULL ;
    tANI_U8 toDs = (tdlsLinkType == TDLS_LINK_AP) 
                                       ? ANI_TXDIR_TODS :ANI_TXDIR_IBSS  ;
    tANI_U8 *peerMac = (reqType == TDLS_INITIATOR) 
                                       ? link_iden->RespStaAddr : link_iden->InitStaAddr; 
    tANI_U8 *staMac = (reqType == TDLS_INITIATOR) 
                                       ? link_iden->InitStaAddr : link_iden->RespStaAddr; 
   
    pMacHdr = (tpSirMacDataHdr3a) (pFrame);

    /* 
     * if TDLS frame goes through the AP link, it follows normal address
     * pattern, if TDLS frame goes thorugh the direct link, then
     * A1--> Peer STA addr, A2-->Self STA address, A3--> BSSID
     */
    (tdlsLinkType == TDLS_LINK_AP) ? ((addr1 = (link_iden->bssid)),
                                      (addr3 = (peerMac))) 
                                   : ((addr1 = (peerMac)),
                                     (addr3 = (link_iden->bssid))) ;
    /*
     * prepare 802.11 header
     */ 
    pMacHdr->fc.protVer = SIR_MAC_PROTOCOL_VERSION;
    pMacHdr->fc.type    = SIR_MAC_DATA_FRAME ;
    pMacHdr->fc.subType = IS_QOS_ENABLED(psessionEntry) ? SIR_MAC_DATA_QOS_DATA : SIR_MAC_DATA_DATA;

    /*
     * TL is not setting up below fields, so we are doing it here
     */
    pMacHdr->fc.toDS    = toDs ;
    pMacHdr->fc.powerMgmt = 0 ;
    pMacHdr->fc.wep = (psessionEntry->encryptType == eSIR_ED_NONE)? 0 : 1;

     
    vos_mem_copy( (tANI_U8 *) pMacHdr->addr1,
                  (tANI_U8 *)addr1,
                  sizeof( tSirMacAddr ));
    vos_mem_copy( (tANI_U8 *) pMacHdr->addr2,
                  (tANI_U8 *) staMac,
                  sizeof( tSirMacAddr ));

    vos_mem_copy( (tANI_U8 *) pMacHdr->addr3,
                  (tANI_U8 *) (addr3),
                  sizeof( tSirMacAddr ));

    limLog(pMac, LOG1,
           FL("Preparing TDLS frame header to %s A1:"MAC_ADDRESS_STR", A2:"MAC_ADDRESS_STR", A3:"MAC_ADDRESS_STR),
           (tdlsLinkType == TDLS_LINK_AP) ? "AP" : "DIRECT",
           MAC_ADDR_ARRAY(pMacHdr->addr1),
           MAC_ADDR_ARRAY(pMacHdr->addr2),
           MAC_ADDR_ARRAY(pMacHdr->addr3));

    //printMacAddr(pMacHdr->bssId) ;
    //printMacAddr(pMacHdr->sa) ;
    //printMacAddr(pMacHdr->da) ;

    if (IS_QOS_ENABLED(psessionEntry))
    {
        pMacHdr->qosControl.tid = tid;
        header_offset += sizeof(tSirMacDataHdr3a);
    }
    else
        header_offset += sizeof(tSirMacMgmtHdr);

    /* 
     * Now form RFC1042 header
     */
    vos_mem_copy((tANI_U8 *)(pFrame + header_offset),
                 (tANI_U8 *)eth_890d_header, sizeof(eth_890d_header)) ;

    header_offset += sizeof(eth_890d_header) ; 

    /* add payload type as TDLS */
    *(pFrame + header_offset) = PAYLOAD_TYPE_TDLS ;

    return(header_offset += PAYLOAD_TYPE_TDLS_SIZE) ; 
}

/*
 * TX Complete for Management frames
 */
 eHalStatus limMgmtTXComplete(tpAniSirGlobal pMac,
                                   tANI_U32 txCompleteSuccess)
{
    tpPESession psessionEntry = NULL ;

    if (0xff != pMac->lim.mgmtFrameSessionId)
    {
        psessionEntry = peFindSessionBySessionId(pMac, pMac->lim.mgmtFrameSessionId);
        if (NULL == psessionEntry)
        {
            limLog(pMac, LOGE, FL("sessionID %d is not found"),
                               pMac->lim.mgmtFrameSessionId);
            return eHAL_STATUS_FAILURE;
        }
        limSendSmeMgmtTXCompletion(pMac, psessionEntry, txCompleteSuccess);
        pMac->lim.mgmtFrameSessionId = 0xff;
    }
    return eHAL_STATUS_SUCCESS;
}

/*
 * This function can be used for bacst or unicast discovery request
 * We are not differentiating it here, it will all depnds on peer MAC address,
 */
tSirRetStatus limSendTdlsDisReqFrame(tpAniSirGlobal pMac, tSirMacAddr peer_mac,
                                      tANI_U8 dialog, tpPESession psessionEntry)
{
    tDot11fTDLSDisReq   tdlsDisReq ;
    tANI_U32            status = 0 ;
    tANI_U32            nPayload = 0 ;
    tANI_U32            size = 0 ;
    tANI_U32            nBytes = 0 ;
    tANI_U32            header_offset = 0 ;
    tANI_U8            *pFrame;
    void               *pPacket;
    eHalStatus          halstatus;
#ifndef NO_PAD_TDLS_MIN_8023_SIZE
    tANI_U32            padLen = 0;
#endif

    /* 
     * The scheme here is to fill out a 'tDot11fProbeRequest' structure
     * and then hand it off to 'dot11fPackProbeRequest' (for
     * serialization).  We start by zero-initializing the structure:
     */
    vos_mem_set( (tANI_U8*)&tdlsDisReq,
                  sizeof( tDot11fTDLSDisReq ), 0 );

    /*
     * setup Fixed fields,
     */
    tdlsDisReq.Category.category = SIR_MAC_ACTION_TDLS ;
    tdlsDisReq.Action.action     = SIR_MAC_TDLS_DIS_REQ ;
    tdlsDisReq.DialogToken.token = dialog ;


    size = sizeof(tSirMacAddr) ;
   
    PopulateDot11fLinkIden( pMac, psessionEntry, &tdlsDisReq.LinkIdentifier, 
                                                 peer_mac, TDLS_INITIATOR) ;

    /* 
     * now we pack it.  First, how much space are we going to need?
     */
    status = dot11fGetPackedTDLSDisReqSize( pMac, &tdlsDisReq, &nPayload);
    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to calculate the packed size for a discovery Request (0x%08x)."),
               status);
        /* We'll fall back on the worst case scenario: */
        nPayload = sizeof( tDot11fTDLSDisReq );
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog(pMac, LOGW,
               FL("There were warnings while calculating the packed size for a discovery Request (0x%08x)."),
               status);
    }

    /*
     * This frame is going out from PE as data frames with special ethertype
     * 89-0d.
     * 8 bytes of RFC 1042 header
     */ 


    nBytes = nPayload + ((IS_QOS_ENABLED(psessionEntry))
                              ? sizeof(tSirMacDataHdr3a) : sizeof(tSirMacMgmtHdr))
                      + sizeof( eth_890d_header )
                      + PAYLOAD_TYPE_TDLS_SIZE ;

#ifndef NO_PAD_TDLS_MIN_8023_SIZE
    /* IOT issue with some AP : some AP doesn't like the data packet size < minimum 802.3 frame length (64)
       Hence AP itself padding some bytes, which caused teardown packet is dropped at
       receiver side. To avoid such IOT issue, we added some extra bytes to meet data frame size >= 64
     */
    if (nPayload + PAYLOAD_TYPE_TDLS_SIZE < MIN_IEEE_8023_SIZE)
    {
        padLen = MIN_IEEE_8023_SIZE - (nPayload + PAYLOAD_TYPE_TDLS_SIZE ) ;

        /* if padLen is less than minimum vendorSpecific (5), pad up to 5 */
        if (padLen < MIN_VENDOR_SPECIFIC_IE_SIZE)
            padLen = MIN_VENDOR_SPECIFIC_IE_SIZE;

        nBytes += padLen;
    }
#endif

    /* Ok-- try to allocate memory from MGMT PKT pool */

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to allocate %d bytes for a TDLS Discovery Request."),
               nBytes);
        return eSIR_MEM_ALLOC_FAILED;
    }

    /* zero out the memory */
    vos_mem_set( pFrame, nBytes, 0 );

    /* 
     * IE formation, memory allocation is completed, Now form TDLS discovery
     * request frame
     */

    /* fill out the buffer descriptor */

    header_offset = limPrepareTdlsFrameHeader(pMac, pFrame, 
           LINK_IDEN_ADDR_OFFSET(tdlsDisReq), TDLS_LINK_AP, TDLS_INITIATOR, TID_AC_VI, psessionEntry) ;

#ifdef FEATURE_WLAN_TDLS_NEGATIVE
    if(pMac->lim.gLimTdlsNegativeBehavior & LIM_TDLS_NEGATIVE_WRONG_BSSID_IN_DSCV_REQ)
    {
        tdlsDisReq.LinkIdentifier.bssid[4] = 0xde;
        tdlsDisReq.LinkIdentifier.bssid[5] = 0xad; 
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
        ("TDLS negative running: wrong BSSID " MAC_ADDRESS_STR " in TDLS Discovery Req"),
        MAC_ADDR_ARRAY(tdlsDisReq.LinkIdentifier.bssid));
    }
#endif
    status = dot11fPackTDLSDisReq( pMac, &tdlsDisReq, pFrame 
                               + header_offset, nPayload, &nPayload );

    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE, FL("Failed to pack a TDLS discovery req (0x%08x)."),
               status);
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, 
                                   ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog(pMac, LOGW, FL("There were warnings while packing TDLS Discovery Request (0x%08x)."),
               status);
    }

#ifndef NO_PAD_TDLS_MIN_8023_SIZE
    if (padLen != 0)
    {
        /* QCOM VENDOR OUI = { 0x00, 0xA0, 0xC6, type = 0x0000 }; */
        tANI_U8 *padVendorSpecific = pFrame + header_offset + nPayload;
        /* make QCOM_VENDOR_OUI, and type = 0x0000, and all the payload to be zero */
        padVendorSpecific[0] = 221;
        padVendorSpecific[1] = padLen - 2;
        padVendorSpecific[2] = 0x00;
        padVendorSpecific[3] = 0xA0;
        padVendorSpecific[4] = 0xC6;

        limLog(pMac, LOG1, FL("Padding Vendor Specific Ie Len = %d"), padLen);

        /* padding zero if more than 5 bytes are required */
        if (padLen > MIN_VENDOR_SPECIFIC_IE_SIZE)
            vos_mem_set( pFrame + header_offset + nPayload + MIN_VENDOR_SPECIFIC_IE_SIZE,
                         padLen - MIN_VENDOR_SPECIFIC_IE_SIZE, 0);
    }
#endif

    limLog(pMac, LOG1,
           FL("[TDLS] action %d (%s) -AP-> OTA peer="MAC_ADDRESS_STR),
           SIR_MAC_TDLS_DIS_REQ,
           limTraceTdlsActionString(SIR_MAC_TDLS_DIS_REQ),
           MAC_ADDR_ARRAY(peer_mac));

    halstatus = halTxFrameWithTxComplete( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_DATA,
                            ANI_TXDIR_TODS,
                            TID_AC_VI,
                            limTxComplete, pFrame,
                            limMgmtTXComplete,
                            HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME);
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        pMac->lim.mgmtFrameSessionId = 0xff;
        limLog(pMac, LOGE, FL("could not send TDLS Discovery Request frame"));
        return eSIR_FAILURE;
    }
    pMac->lim.mgmtFrameSessionId = psessionEntry->peSessionId;

    return eSIR_SUCCESS;

}

#ifdef FEATURE_WLAN_TDLS_INTERNAL
/*
 * Once Discovery response is sent successfully (or failure) on air, now send
 * response to PE and send del STA to HAL.
 */
eHalStatus limTdlsDisRspTxComplete(tpAniSirGlobal pMac, 
                                           tANI_U32 txCompleteSuccess)
{
    eHalStatus status = eHAL_STATUS_SUCCESS ;
    tpDphHashNode pStaDs = NULL ;
    tSirTdlsPeerInfo *peerInfo = 0 ;

    /* find peer by looking into the list by expected state */
    peerInfo = limTdlsFindDisPeerByState(pMac, TDLS_DIS_RSP_SENT_WAIT_STATE) ;

    if(NULL == peerInfo)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
                                       ("DisRspTxComplete: No TDLS state machine waits for this event"));
        VOS_ASSERT(0) ;
        return eHAL_STATUS_FAILURE;
    }

    peerInfo->tdlsPeerState = TDLS_DIS_RSP_SENT_DONE_STATE ;

    if(peerInfo->delStaNeeded)
    {
        tpPESession psessionEntry;
        
        peerInfo->delStaNeeded = false ;
        psessionEntry = peFindSessionBySessionId (pMac, peerInfo->sessionId);

        if(NULL == psessionEntry) 
        {
            VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
                                           ("DisRspTxComplete: sessionID %d is not found"), peerInfo->sessionId);
            return eHAL_STATUS_FAILURE;
        }
        /* send del STA to remove context for this TDLS STA */
        pStaDs = limTdlsDelSta(pMac, peerInfo->peerMac, psessionEntry) ;

        /* now send indication to SME-->HDD->TL to remove STA from TL */
        if(pStaDs)
        {
            limSendSmeTdlsDelPeerInd(pMac, psessionEntry->smeSessionId, 
                                                     pStaDs, eSIR_SUCCESS) ;
        }
        else
        {
            VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
                           ("DisRspTxComplete: staDs not found for " MAC_ADDRESS_STR),
                           MAC_ADDR_ARRAY((peerInfo)->peerMac));
            VOS_ASSERT(0) ;
            return eHAL_STATUS_FAILURE;
        }
    }
 
    if(!txCompleteSuccess)
     {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
                                       ("TX complete failure for Dis RSP"));
        limSendSmeTdlsDisRsp(pMac, eSIR_FAILURE, 
                                     eWNI_SME_TDLS_DISCOVERY_START_IND) ;
        status = eHAL_STATUS_FAILURE;
    }
    else
    {
        limSendSmeTdlsDisRsp(pMac, eSIR_SUCCESS, 
                                     eWNI_SME_TDLS_DISCOVERY_START_IND) ;
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
                                       ("TX complete Success for Dis RSP"));
        status = eHAL_STATUS_SUCCESS ;
    }
    //pMac->hal.pCBackFnTxComp = NULL ;
    return status ;

}
#endif

#ifdef FEATURE_WLAN_TDLS_INTERNAL
/*
 * Once setup CNF is sent successfully (or failure) on air, now send
 * response to PE and send del STA to HAL.
 */
eHalStatus limTdlsSetupCnfTxComplete(tpAniSirGlobal pMac,
                                           tANI_U32 txCompleteSuccess)
{
    eHalStatus status = eHAL_STATUS_SUCCESS ;
    tLimTdlsLinkSetupPeer *peerInfo = 0 ;
    /* find peer by looking into the list by expected state */
    limTdlsFindSetupPeerByState(pMac, 
                             TDLS_LINK_SETUP_RSP_WAIT_STATE, &peerInfo) ;
  
    if(NULL == peerInfo)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
                                       ("limTdlsSetupCnfTxComplete: No TDLS state machine waits for this event"));
        VOS_ASSERT(0) ;
        return eHAL_STATUS_FAILURE;
    }
 
    (peerInfo)->tdls_prev_link_state = (peerInfo)->tdls_link_state ;
    (peerInfo)->tdls_link_state = TDLS_LINK_SETUP_DONE_STATE ; 

    if(!txCompleteSuccess)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
                                   ("TX complete Failure for setup CNF"));
        limSendSmeTdlsLinkStartRsp(pMac, eSIR_FAILURE, (peerInfo)->peerMac,
                                               eWNI_SME_TDLS_LINK_START_RSP) ;
        status = eHAL_STATUS_FAILURE;
    }
    else
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
              ("RSP-->SME peer MAC = " MAC_ADDRESS_STR),
               MAC_ADDR_ARRAY((peerInfo)->peerMac));
    
        limSendSmeTdlsLinkStartRsp(pMac, eSIR_SUCCESS, (peerInfo)->peerMac,
                                               eWNI_SME_TDLS_LINK_START_RSP) ;

        /* tdls_hklee: prepare PTI template and send it to HAL */
        limTdlsLinkEstablish(pMac, (peerInfo)->peerMac);

        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
                                 ("TX complete Success for setup CNF"));
        status = eHAL_STATUS_SUCCESS ;
    }
    //pMac->hal.pCBackFnTxComp = NULL ;
    return status ;
}
#endif

#ifdef FEATURE_WLAN_TDLS_INTERNAL
/*
 * Tx Complete for Teardown frame
 */
eHalStatus limTdlsTeardownTxComplete(tpAniSirGlobal pMac,
                                           tANI_U32 txCompleteSuccess)  
{
    eHalStatus status = eHAL_STATUS_SUCCESS ;
    tpDphHashNode pStaDs = NULL ;
    tLimTdlsLinkSetupPeer *peerInfo = 0 ;
    tpPESession psessionEntry = NULL ;
    //tANI_U16 msgType = 0 ;

    //tSirMacAddr peerMac = {0} ;
    /* find peer by looking into the list by expected state */
    limTdlsFindSetupPeerByState(pMac, 
                             TDLS_LINK_TEARDOWN_START_STATE, &peerInfo) ;
  
    if(NULL == peerInfo)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
                                       ("limTdlsTeardownTxComplete: No TDLS state machine waits for this event"));
        VOS_ASSERT(0) ;
        return eHAL_STATUS_FAILURE;
    }

    VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO,
                  ("teardown peer Mac = " MAC_ADDRESS_STR),
                   MAC_ADDR_ARRAY((peerInfo)->peerMac));
             

    //pMac->hal.pCBackFnTxComp = NULL ;

    psessionEntry = peFindSessionBySessionId(pMac, (peerInfo)->tdls_sessionId);

    if(NULL == psessionEntry)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
                                       ("limTdlsTeardownTxComplete: sessionID %d is not found"), (peerInfo)->tdls_sessionId);
        VOS_ASSERT(0) ;
        return eHAL_STATUS_FAILURE;
    }

    if(!txCompleteSuccess)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
                         ("TX complete failure for Teardown  ")) ;

        /* 
         * we should be sending Teradown to AP with status code 
         * eSIR_MAC_TDLS_TEARDOWN_PEER_UNREACHABLE, we are not worried if 
         * that is delivered or not, any way we removing this peer STA from our
         * list
         */
        if(NULL != psessionEntry)
        {
            limSendTdlsTeardownFrame(pMac, (peerInfo)->peerMac, 
                     eSIR_MAC_TDLS_TEARDOWN_PEER_UNREACHABLE, psessionEntry, NULL, 0) ;
        }
    }

    if(TDLS_LINK_SETUP_WAIT_STATE != (peerInfo)->tdls_prev_link_state)
    {
        (peerInfo)->tdls_prev_link_state = (peerInfo)->tdls_link_state ;
        (peerInfo)->tdls_link_state = TDLS_LINK_TEARDOWN_DONE_STATE ; 
        /* send del STA to remove context for this TDLS STA */
        if(NULL != psessionEntry)
        {
            /* tdls_hklee: send message to HAL before it is deleted */
            limTdlsLinkTeardown(pMac, (peerInfo)->peerMac) ;

            pStaDs = limTdlsDelSta(pMac, (peerInfo)->peerMac, psessionEntry) ;
        }

        /* now send indication to SME-->HDD->TL to remove STA from TL */
        if(!pStaDs)
        {
            VOS_ASSERT(0) ;
            return eSIR_FAILURE ;
        }
        limSendSmeTdlsDelPeerInd(pMac, psessionEntry->smeSessionId, 
                                                pStaDs, eSIR_SUCCESS) ;
 
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
                      ("TX complete SUCCESS for Teardown")) ;
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
                      ("Prev State = %d"), (peerInfo)->tdls_prev_link_state) ;
        limSendSmeTdlsTeardownRsp(pMac, eSIR_SUCCESS, (peerInfo)->peerMac,
                                                     eWNI_SME_TDLS_TEARDOWN_RSP) ;
        /* Delete Peer for Link Peer List */
        limTdlsDelLinkPeer(pMac, (peerInfo)->peerMac) ;
    }
    else
    {
        (peerInfo)->tdls_prev_link_state = (peerInfo)->tdls_link_state ;
        (peerInfo)->tdls_link_state = TDLS_LINK_TEARDOWN_DONE_STATE ; 
        limSendSmeTdlsTeardownRsp(pMac, eSIR_SUCCESS, (peerInfo)->peerMac,
                                                eWNI_SME_TDLS_TEARDOWN_IND) ;
    }


#if 0
    /* if previous state is link restart, then restart link setup again */
    if(TDLS_LINK_SETUP_RESTART_STATE == (peerInfo)->tdls_prev_link_state)
    {
        tLimTdlsLinkSetupInfo *setupInfo = &pMac->lim.gLimTdlsLinkSetupInfo ;
        limTdlsPrepareSetupReqFrame(pMac, setupInfo, 37, 
                                                   peerMac, psessionEntry) ;
    }
#endif  
    status = eHAL_STATUS_SUCCESS ;
    return status ;
}
#endif

/*
 * This static function is consistent with any kind of TDLS management
 * frames we are sending. Currently it is being used by limSendTdlsDisRspFrame,
 * limSendTdlsLinkSetupReqFrame and limSendTdlsSetupRspFrame
 */
static void PopulateDot11fTdlsHtVhtCap(tpAniSirGlobal pMac, uint32 selfDot11Mode,
                                        tDot11fIEHTCaps *htCap, tDot11fIEVHTCaps *vhtCap,
                                        tpPESession psessionEntry)
{
    if (IS_DOT11_MODE_HT(selfDot11Mode))
    {
        /* Include HT Capability IE */
        PopulateDot11fHTCaps( pMac, NULL, htCap );
        htCap->present = 1;
        if (psessionEntry->currentOperChannel <= SIR_11B_CHANNEL_END)
        {
            /* hardcode NO channel bonding in 2.4Ghz */
            htCap->supportedChannelWidthSet = 0;
        }
        else
        {
            //Placeholder to support different channel bonding mode of TDLS than AP.
            //wlan_cfgGetInt(pMac,WNI_CFG_TDLS_CHANNEL_BONDING_MODE,&tdlsChannelBondingMode);
            //htCap->supportedChannelWidthSet = tdlsChannelBondingMode ? 1 : 0;
            htCap->supportedChannelWidthSet = 1; // hardcode it to max
        }
    }
    else
    {
        htCap->present = 0;
    }
#ifdef WLAN_FEATURE_11AC
    if (((psessionEntry->currentOperChannel <= SIR_11B_CHANNEL_END) &&
          pMac->roam.configParam.enableVhtFor24GHz) ||
         (psessionEntry->currentOperChannel >= SIR_11B_CHANNEL_END))
    {
        if (IS_DOT11_MODE_VHT(selfDot11Mode) &&
            IS_FEATURE_SUPPORTED_BY_FW(DOT11AC))
        {
            /* Include VHT Capability IE */
            PopulateDot11fVHTCaps( pMac, vhtCap, eSIR_FALSE );
        }
        else
        {
            vhtCap->present = 0;
        }
    }
    else
    {
        /* Vht Disable from ini in 2.4 GHz */
        vhtCap->present = 0;
    }
#endif
}

/*
 * Send TDLS discovery response frame on direct link.
 */

static tSirRetStatus limSendTdlsDisRspFrame(tpAniSirGlobal pMac,
                     tSirMacAddr peerMac, tANI_U8 dialog,
                     tpPESession psessionEntry, tANI_U8 *addIe,
                     tANI_U16 addIeLen)
{
    tDot11fTDLSDisRsp   tdlsDisRsp ;
    tANI_U16            caps = 0 ;            
    tANI_U32            status = 0 ;
    tANI_U32            nPayload = 0 ;
    tANI_U32            nBytes = 0 ;
    tANI_U8            *pFrame;
    void               *pPacket;
    eHalStatus          halstatus;
    uint32              selfDot11Mode;
//  Placeholder to support different channel bonding mode of TDLS than AP.
//  Today, WNI_CFG_CHANNEL_BONDING_MODE will be overwritten when connecting to AP
//  To support this feature, we need to introduce WNI_CFG_TDLS_CHANNEL_BONDING_MODE
//  As of now, we hardcoded to max channel bonding of dot11Mode (i.e HT80 for 11ac/HT40 for 11n)
//  uint32 tdlsChannelBondingMode;

    /* 
     * The scheme here is to fill out a 'tDot11fProbeRequest' structure
     * and then hand it off to 'dot11fPackProbeRequest' (for
     * serialization).  We start by zero-initializing the structure:
     */
    vos_mem_set( ( tANI_U8* )&tdlsDisRsp,
                                      sizeof( tDot11fTDLSDisRsp ), 0 );

    /*
     * setup Fixed fields,
     */
    tdlsDisRsp.Category.category = SIR_MAC_ACTION_PUBLIC_USAGE;
    tdlsDisRsp.Action.action     = SIR_MAC_TDLS_DIS_RSP ;
    tdlsDisRsp.DialogToken.token = dialog ;

    PopulateDot11fLinkIden( pMac, psessionEntry, &tdlsDisRsp.LinkIdentifier, 
                                           peerMac, TDLS_RESPONDER) ;

    if (cfgGetCapabilityInfo(pMac, &caps, psessionEntry) != eSIR_SUCCESS)
    {
        /*
         * Could not get Capabilities value
         * from CFG. Log error.
         */
         limLog(pMac, LOGP, FL("could not retrieve Capabilities value"));
    }
    swapBitField16(caps, ( tANI_U16* )&tdlsDisRsp.Capabilities );

    /* populate supported rate IE */
    PopulateDot11fSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL, 
                                     &tdlsDisRsp.SuppRates, psessionEntry );
   
    /* Populate extended supported rates */
    PopulateDot11fExtSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL,
                                &tdlsDisRsp.ExtSuppRates, psessionEntry );

    /* Populate extended supported rates */
    PopulateDot11fTdlsExtCapability( pMac, &tdlsDisRsp.ExtCap );

    wlan_cfgGetInt(pMac,WNI_CFG_DOT11_MODE,&selfDot11Mode);

    /* Populate HT/VHT Capabilities */
    PopulateDot11fTdlsHtVhtCap( pMac, selfDot11Mode, &tdlsDisRsp.HTCaps,
                               &tdlsDisRsp.VHTCaps, psessionEntry );

    if ( 1 == pMac->lim.gLimTDLSOffChannelEnabled )
        PopulateDot11fTdlsOffchannelParams( pMac, psessionEntry,
                                            &tdlsDisRsp.SuppChannels,
                                            &tdlsDisRsp.SuppOperatingClasses);

    if ( 1 == pMac->lim.gLimTDLSOffChannelEnabled &&
         ( pMac->roam.configParam.bandCapability != eCSR_BAND_24) )
    {
        tdlsDisRsp.HT2040BSSCoexistence.present = 1;
        tdlsDisRsp.HT2040BSSCoexistence.infoRequest = 1;
    }
    /* 
     * now we pack it.  First, how much space are we going to need?
     */
    status = dot11fGetPackedTDLSDisRspSize( pMac, &tdlsDisRsp, &nPayload);
    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to calculate the packed size for a Discovery Response (0x%08x)."),
               status);
        /* We'll fall back on the worst case scenario: */
        nPayload = sizeof( tDot11fProbeRequest );
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog(pMac, LOGW,
               FL("There were warnings while calculating the packed size for a Discovery Response (0x%08x)."),
               status);
    }

    /*
     * This frame is going out from PE as data frames with special ethertype
     * 89-0d.
     * 8 bytes of RFC 1042 header
     */ 


    nBytes = nPayload + sizeof( tSirMacMgmtHdr ) + addIeLen;

    /* Ok-- try to allocate memory from MGMT PKT pool */

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to allocate %d bytes for a TDLS Discovery Response."),
               nBytes);
        return eSIR_MEM_ALLOC_FAILED;
    }

    /* zero out the memory */
    vos_mem_set( pFrame, nBytes, 0 );

    /* 
     * IE formation, memory allocation is completed, Now form TDLS discovery
     * response frame
     */

    /* Make public Action Frame */

#if 0
    limPreparesActionFrameHdr(pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                          SIR_MAC_MGMT_ACTION, 
                                            LINK_IDEN_ADDR_OFFSET(tdlsDisRsp)) ;
#endif
    limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
              SIR_MAC_MGMT_ACTION, peerMac, psessionEntry->selfMacAddr);

    {
        tpSirMacMgmtHdr     pMacHdr;
        pMacHdr = ( tpSirMacMgmtHdr ) pFrame;
        pMacHdr->fc.toDS    = ANI_TXDIR_IBSS;
        pMacHdr->fc.powerMgmt = 0 ;
        sirCopyMacAddr(pMacHdr->bssId,psessionEntry->bssId);
    }

#ifdef FEATURE_WLAN_TDLS_NEGATIVE
    if(pMac->lim.gLimTdlsNegativeBehavior & LIM_TDLS_NEGATIVE_WRONG_BSSID_IN_DSCV_RSP)
    {
        tdlsDisRsp.LinkIdentifier.bssid[4] = 0xde;
        tdlsDisRsp.LinkIdentifier.bssid[5] = 0xad; 
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
        ("TDLS negative running: wrong BSSID " MAC_ADDRESS_STR " in TDLS Discovery Rsp"),
         MAC_ADDR_ARRAY(tdlsDisRsp.LinkIdentifier.bssid));
    }
#endif
    status = dot11fPackTDLSDisRsp( pMac, &tdlsDisRsp, pFrame + 
                                              sizeof( tSirMacMgmtHdr ),
                                                  nPayload, &nPayload );

    if ( DOT11F_FAILED( status ) )
    {
        limLog( pMac, LOGE,
                FL("Failed to pack a TDLS Discovery Response (0x%08x)."),
                status);
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, 
                                   ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog( pMac, LOGW,
                FL("There were warnings while packing TDLS Discovery Request (0x%08x)."),
                status);
    }

#if 0
    if(pMac->hal.pCBackFnTxComp == NULL) 
    {
        pMac->hal.pCBackFnTxComp = (tpCBackFnTxComp)limTdlsDisRspTxComplete;

        if(TX_SUCCESS != tx_timer_activate(&pMac->hal.txCompTimer)) 
        {
            status = eHAL_STATUS_FAILURE;
            return status;
                
        }
    }
#endif
    if (0 != addIeLen)
    {
        limLog(pMac, LOG1, FL("Copy Additional Ie Len = %d"), addIeLen);
        vos_mem_copy(pFrame + sizeof(tSirMacMgmtHdr) + nPayload, addIe,
                                                              addIeLen);
    }
    limLog(pMac, LOG1,
           FL("[TDLS] action %d (%s) -DIRECT-> OTA peer="MAC_ADDRESS_STR),
           SIR_MAC_TDLS_DIS_RSP,
           limTraceTdlsActionString(SIR_MAC_TDLS_DIS_RSP),
           MAC_ADDR_ARRAY(peerMac));

    /*
     * Transmit Discovery response and watch if this is delivered to
     * peer STA.
     */
    halstatus = halTxFrameWithTxComplete( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_DATA,
                            ANI_TXDIR_IBSS,
                            0,
                            limTxComplete, pFrame, 
                            limMgmtTXComplete,
                            HAL_USE_SELF_STA_REQUESTED_MASK );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        pMac->lim.mgmtFrameSessionId = 0xff;
        limLog(pMac, LOGE, FL("could not send TDLS Discovery Response frame!"));
        return eSIR_FAILURE;
    }
    pMac->lim.mgmtFrameSessionId = psessionEntry->peSessionId;

    return eSIR_SUCCESS;

}

/*
 * This static function is currently used by limSendTdlsLinkSetupReqFrame and
 * limSendTdlsSetupRspFrame to populate the AID if device is 11ac capable.
 */
static void PopulateDotfTdlsVhtAID(tpAniSirGlobal pMac, uint32 selfDot11Mode,
                                   tSirMacAddr peerMac, tDot11fIEAID *Aid,
                                   tpPESession psessionEntry)
{
    if (((psessionEntry->currentOperChannel <= SIR_11B_CHANNEL_END) &&
          pMac->roam.configParam.enableVhtFor24GHz) ||
         (psessionEntry->currentOperChannel >= SIR_11B_CHANNEL_END))
    {
        if (IS_DOT11_MODE_VHT(selfDot11Mode) &&
            IS_FEATURE_SUPPORTED_BY_FW(DOT11AC))
        {

            tANI_U16 aid;
            tpDphHashNode       pStaDs;

            pStaDs = dphLookupHashEntry(pMac, peerMac, &aid, &psessionEntry->dph.dphHashTable);
            if (NULL != pStaDs)
            {
                 Aid->present = 1;
                 Aid->assocId = aid | LIM_AID_MASK; // set bit 14 and 15 1's
            }
            else
            {
                Aid->present = 0;
                limLog(pMac, LOGE, FL("pStaDs is NULL for " MAC_ADDRESS_STR),
                                   MAC_ADDR_ARRAY(peerMac));
            }
        }
    }
    else
    {
        Aid->present = 0;
        limLog(pMac, LOGW, FL("Vht not enable from ini for 2.4GHz."));
    }
}

/*
 * TDLS setup Request frame on AP link
 */

tSirRetStatus limSendTdlsLinkSetupReqFrame(tpAniSirGlobal pMac,
            tSirMacAddr peerMac, tANI_U8 dialog, tpPESession psessionEntry,
            tANI_U8 *addIe, tANI_U16 addIeLen)
{
    tDot11fTDLSSetupReq    tdlsSetupReq ;
    tANI_U16            caps = 0 ;
    tANI_U32            status = 0 ;
    tANI_U32            nPayload = 0 ;
    tANI_U32            nBytes = 0 ;
    tANI_U32            header_offset = 0 ;
    tANI_U8            *pFrame;
    void               *pPacket;
    eHalStatus          halstatus;
    uint32              selfDot11Mode;
//  Placeholder to support different channel bonding mode of TDLS than AP.
//  Today, WNI_CFG_CHANNEL_BONDING_MODE will be overwritten when connecting to AP
//  To support this feature, we need to introduce WNI_CFG_TDLS_CHANNEL_BONDING_MODE
//  As of now, we hardcoded to max channel bonding of dot11Mode (i.e HT80 for 11ac/HT40 for 11n)
//  uint32 tdlsChannelBondingMode;

    /*
     * The scheme here is to fill out a 'tDot11fProbeRequest' structure
     * and then hand it off to 'dot11fPackProbeRequest' (for
     * serialization).  We start by zero-initializing the structure:
     */
    vos_mem_set(( tANI_U8* )&tdlsSetupReq, sizeof( tDot11fTDLSSetupReq ), 0);
    tdlsSetupReq.Category.category = SIR_MAC_ACTION_TDLS ;
    tdlsSetupReq.Action.action     = SIR_MAC_TDLS_SETUP_REQ ;
    tdlsSetupReq.DialogToken.token = dialog ;


    PopulateDot11fLinkIden( pMac, psessionEntry, &tdlsSetupReq.LinkIdentifier,
                                                    peerMac, TDLS_INITIATOR) ;

    if (cfgGetCapabilityInfo(pMac, &caps, psessionEntry) != eSIR_SUCCESS)
    {
        /*
         * Could not get Capabilities value
         * from CFG. Log error.
         */
         limLog(pMac, LOGE, FL("could not retrieve Capabilities value"));
    }
    swapBitField16(caps, ( tANI_U16* )&tdlsSetupReq.Capabilities );

    /* populate supported rate IE */
    PopulateDot11fSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL,
                              &tdlsSetupReq.SuppRates, psessionEntry );

    /* Populate extended supported rates */
    PopulateDot11fExtSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL,
                                &tdlsSetupReq.ExtSuppRates, psessionEntry );

    /* Populate extended supported rates */
    PopulateDot11fTdlsExtCapability( pMac, &tdlsSetupReq.ExtCap );

    if ( 1 == pMac->lim.gLimTDLSWmmMode )
    {
        /* include WMM IE */
        PopulateDot11fWMMInfoStation( pMac, &tdlsSetupReq.WMMInfoStation );
    }
    else
    {
        /*
         * TODO: we need to see if we have to support conditions where we have
         * EDCA parameter info element is needed a) if we need different QOS
         * parameters for off channel operations or QOS is not supported on
         * AP link and we wanted to QOS on direct link.
         */
        /* Populate QOS info, needed for Peer U-APSD session */
        /* TODO: Now hardcoded, because PopulateDot11fQOSCapsStation() depends on AP's capability, and
         TDLS doesn't want to depend on AP's capability */
        limLog(pMac, LOG1, FL("populate QOS IE in Setup Request Frame"));
        tdlsSetupReq.QOSCapsStation.present = 1;
        tdlsSetupReq.QOSCapsStation.max_sp_length = 0;
        tdlsSetupReq.QOSCapsStation.qack = 0;
        tdlsSetupReq.QOSCapsStation.acbe_uapsd = ((pMac->lim.gLimTDLSUapsdMask & 0x08) >> 3);
        tdlsSetupReq.QOSCapsStation.acbk_uapsd = ((pMac->lim.gLimTDLSUapsdMask & 0x04) >> 2);
        tdlsSetupReq.QOSCapsStation.acvi_uapsd = ((pMac->lim.gLimTDLSUapsdMask & 0x02) >> 1);
        tdlsSetupReq.QOSCapsStation.acvo_uapsd = (pMac->lim.gLimTDLSUapsdMask & 0x01);
    }

    /*
     * we will always try to init TDLS link with 11n capabilities
     * let TDLS setup response to come, and we will set our caps based
     * of peer caps
     */

    wlan_cfgGetInt(pMac,WNI_CFG_DOT11_MODE,&selfDot11Mode);

    /* Populate HT/VHT Capabilities */
    PopulateDot11fTdlsHtVhtCap( pMac, selfDot11Mode, &tdlsSetupReq.HTCaps,
                               &tdlsSetupReq.VHTCaps, psessionEntry );

    /* Populate AID */
    PopulateDotfTdlsVhtAID( pMac, selfDot11Mode, peerMac,
                            &tdlsSetupReq.AID, psessionEntry );

    if ( 1 == pMac->lim.gLimTDLSOffChannelEnabled )
        PopulateDot11fTdlsOffchannelParams( pMac, psessionEntry,
                                            &tdlsSetupReq.SuppChannels,
                                            &tdlsSetupReq.SuppOperatingClasses);

    if ( 1 == pMac->lim.gLimTDLSOffChannelEnabled &&
         ( pMac->roam.configParam.bandCapability != eCSR_BAND_24))
    {
        tdlsSetupReq.HT2040BSSCoexistence.present = 1;
        tdlsSetupReq.HT2040BSSCoexistence.infoRequest = 1;
    }

    /*
     * now we pack it.  First, how much space are we going to need?
     */
    status = dot11fGetPackedTDLSSetupReqSize( pMac, &tdlsSetupReq,
                                                              &nPayload);
    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to calculate the packed size for a Setup Request (0x%08x)."),
               status);
        /* We'll fall back on the worst case scenario: */
        nPayload = sizeof( tDot11fProbeRequest );
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog(pMac, LOGW,
               FL("There were warnings while calculating the packed size for a Setup Request (0x%08x)."),
               status);
    }


    /*
     * This frame is going out from PE as data frames with special ethertype
     * 89-0d.
     * 8 bytes of RFC 1042 header
     */


    nBytes = nPayload + ((IS_QOS_ENABLED(psessionEntry))
                              ? sizeof(tSirMacDataHdr3a) : sizeof(tSirMacMgmtHdr))
                      + sizeof( eth_890d_header )
                      + PAYLOAD_TYPE_TDLS_SIZE
                      + addIeLen;

    /* Ok-- try to allocate memory from MGMT PKT pool */

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to allocate %d bytes for a TDLS Setup Request."),
               nBytes);
        return eSIR_MEM_ALLOC_FAILED;
    }

    /* zero out the memory */
    vos_mem_set( pFrame, nBytes, 0);

    /*
     * IE formation, memory allocation is completed, Now form TDLS discovery
     * request frame
     */

    /* fill out the buffer descriptor */

    header_offset = limPrepareTdlsFrameHeader(pMac, pFrame,
                     LINK_IDEN_ADDR_OFFSET(tdlsSetupReq), TDLS_LINK_AP, TDLS_INITIATOR, TID_AC_BK, psessionEntry) ;

#ifdef FEATURE_WLAN_TDLS_NEGATIVE
    if(pMac->lim.gLimTdlsNegativeBehavior & LIM_TDLS_NEGATIVE_WRONG_BSSID_IN_SETUP_REQ)
    {
        tdlsSetupReq.LinkIdentifier.bssid[4] = 0xde;
        tdlsSetupReq.LinkIdentifier.bssid[5] = 0xad;
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR,
        ("TDLS negative running: wrong BSSID " MAC_ADDRESS_STR " in TDLS Setup Req"),
         MAC_ADDR_ARRAY(tdlsSetupReq.LinkIdentifier.bssid));
    }
#endif
    limLog( pMac, LOGW, FL("SupportedChnlWidth %x rxMCSMap %x rxMCSMap %x txSupDataRate %x"),
            tdlsSetupReq.VHTCaps.supportedChannelWidthSet,
            tdlsSetupReq.VHTCaps.rxMCSMap,
            tdlsSetupReq.VHTCaps.txMCSMap,
            tdlsSetupReq.VHTCaps.txSupDataRate);

    status = dot11fPackTDLSSetupReq( pMac, &tdlsSetupReq, pFrame
                               + header_offset, nPayload, &nPayload );

    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to pack a TDLS Setup Request (0x%08x)."),
                   status);
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, 
                                   ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog(pMac, LOGW,
               FL("There were warnings while packing TDLS Setup Request (0x%08x)."),
               status);
    }

    //Copy the additional IE.
    //TODO : addIe is added at the end of the frame. This means it doesnt
    //follow the order. This should be ok, but we should consider changing this
    //if there is any IOT issue.
    if( addIeLen != 0 )
    {
        limLog(pMac, LOG1, FL("Copy Additional Ie Len = %d"),
                           addIeLen);
        vos_mem_copy( pFrame + header_offset + nPayload, addIe, addIeLen );
    }

    limLog(pMac, LOG1, FL("[TDLS] action %d (%s) -AP-> OTA peer="MAC_ADDRESS_STR),
                       SIR_MAC_TDLS_SETUP_REQ,
                       limTraceTdlsActionString(SIR_MAC_TDLS_SETUP_REQ),
                       MAC_ADDR_ARRAY(peerMac));

    halstatus = halTxFrameWithTxComplete( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_DATA,
                            ANI_TXDIR_TODS,
                            TID_AC_BK,
                            limTxComplete, pFrame,
                            limMgmtTXComplete,
                            HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME );

    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        pMac->lim.mgmtFrameSessionId = 0xff;
        limLog(pMac, LOGE, FL("could not send TDLS Setup Request frame!"));
        return eSIR_FAILURE;
    }
    pMac->lim.mgmtFrameSessionId = psessionEntry->peSessionId;

    return eSIR_SUCCESS;

}
/*
 * Send TDLS Teardown frame on Direct link or AP link, depends on reason code.
 */

tSirRetStatus limSendTdlsTeardownFrame(tpAniSirGlobal pMac,
            tSirMacAddr peerMac, tANI_U16 reason, tANI_U8 responder, tpPESession psessionEntry,
            tANI_U8 *addIe, tANI_U16 addIeLen)
{
    tDot11fTDLSTeardown teardown ;
    tANI_U32            status = 0 ;
    tANI_U32            nPayload = 0 ;
    tANI_U32            nBytes = 0 ;
    tANI_U32            header_offset = 0 ;
    tANI_U8            *pFrame;
    void               *pPacket;
    eHalStatus          halstatus;
#ifndef NO_PAD_TDLS_MIN_8023_SIZE
    tANI_U32            padLen = 0;
#endif
    /*
     * The scheme here is to fill out a 'tDot11fProbeRequest' structure
     * and then hand it off to 'dot11fPackProbeRequest' (for
     * serialization).  We start by zero-initializing the structure:
     */
    vos_mem_set( ( tANI_U8* )&teardown, sizeof( tDot11fTDLSTeardown ), 0 );
    teardown.Category.category = SIR_MAC_ACTION_TDLS ;
    teardown.Action.action     = SIR_MAC_TDLS_TEARDOWN ;
    teardown.Reason.code       = reason ;

    PopulateDot11fLinkIden( pMac, psessionEntry, &teardown.LinkIdentifier,
                                                peerMac, (responder == TRUE) ? TDLS_RESPONDER : TDLS_INITIATOR) ;
    /*
     * now we pack it.  First, how much space are we going to need?
     */
    status = dot11fGetPackedTDLSTeardownSize( pMac, &teardown, &nPayload);
    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to calculate the packed size for Teardown frame (0x%08x)."),
               status);
        /* We'll fall back on the worst case scenario: */
        nPayload = sizeof( tDot11fProbeRequest );
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog(pMac, LOGW,
               FL("There were warnings while calculating the packed size for Teardown frame (0x%08x)."),
               status);
    }
    /*
     * This frame is going out from PE as data frames with special ethertype
     * 89-0d.
     * 8 bytes of RFC 1042 header
     */
    nBytes = nPayload + ((IS_QOS_ENABLED(psessionEntry))
                              ? sizeof(tSirMacDataHdr3a) : sizeof(tSirMacMgmtHdr))
                      + sizeof( eth_890d_header )
                      + PAYLOAD_TYPE_TDLS_SIZE
                      + addIeLen;

#ifndef NO_PAD_TDLS_MIN_8023_SIZE
    /* IOT issue with some AP : some AP doesn't like the data packet size < minimum 802.3 frame length (64)
       Hence AP itself padding some bytes, which caused teardown packet is dropped at
       receiver side. To avoid such IOT issue, we added some extra bytes to meet data frame size >= 64
     */
    if (nPayload + PAYLOAD_TYPE_TDLS_SIZE < MIN_IEEE_8023_SIZE)
    {
        padLen = MIN_IEEE_8023_SIZE - (nPayload + PAYLOAD_TYPE_TDLS_SIZE ) ;

        /* if padLen is less than minimum vendorSpecific (5), pad up to 5 */
        if (padLen < MIN_VENDOR_SPECIFIC_IE_SIZE)
            padLen = MIN_VENDOR_SPECIFIC_IE_SIZE;

        nBytes += padLen;
    }
#endif

    /* Ok-- try to allocate memory from MGMT PKT pool */

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to allocate %d bytes for a TDLS Teardown Frame."),
               nBytes);
        return eSIR_MEM_ALLOC_FAILED;
    }

    /* zero out the memory */
    vos_mem_set( pFrame, nBytes, 0 );

    /*
     * IE formation, memory allocation is completed, Now form TDLS discovery
     * request frame
     */
    limLog(pMac, LOGE, FL("Reason of TDLS Teardown: %d"), reason);
    /* fill out the buffer descriptor */

    header_offset = limPrepareTdlsFrameHeader(pMac, pFrame,
                     LINK_IDEN_ADDR_OFFSET(teardown),
                          (reason == eSIR_MAC_TDLS_TEARDOWN_PEER_UNREACHABLE)
                              ? TDLS_LINK_AP : TDLS_LINK_DIRECT,
                              (responder == TRUE) ? TDLS_RESPONDER : TDLS_INITIATOR,
                              TID_AC_VI, psessionEntry) ;

    status = dot11fPackTDLSTeardown( pMac, &teardown, pFrame
                               + header_offset, nPayload, &nPayload );

    if ( DOT11F_FAILED( status ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a TDLS Teardown req (0x%08x)."),
                status);
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                                   ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog(pMac, LOGW, FL("There were warnings while packing TDLS Teardown frame (0x%08x)."),
               status);
    }
#if 0
    if(pMac->hal.pCBackFnTxComp == NULL)
    {
        pMac->hal.pCBackFnTxComp = (tpCBackFnTxComp)limTdlsTeardownTxComplete;
        if(TX_SUCCESS != tx_timer_activate(&pMac->hal.txCompTimer))
        {
            status = eHAL_STATUS_FAILURE;
            return status;
                
        }
    }
    else
    {
        VOS_ASSERT(0) ;
        return status ;
    }
#endif

    if( addIeLen != 0 )
    {
        limLog(pMac, LOG1, FL("Copy Additional Ie Len = %d"), addIeLen);
        vos_mem_copy( pFrame + header_offset + nPayload, addIe, addIeLen );
    }

#ifndef NO_PAD_TDLS_MIN_8023_SIZE
    if (padLen != 0)
    {
        /* QCOM VENDOR OUI = { 0x00, 0xA0, 0xC6, type = 0x0000 }; */
        tANI_U8 *padVendorSpecific = pFrame + header_offset + nPayload + addIeLen;
        /* make QCOM_VENDOR_OUI, and type = 0x0000, and all the payload to be zero */
        padVendorSpecific[0] = 221;
        padVendorSpecific[1] = padLen - 2;
        padVendorSpecific[2] = 0x00;
        padVendorSpecific[3] = 0xA0;
        padVendorSpecific[4] = 0xC6;

        limLog(pMac, LOG1, FL("Padding Vendor Specific Ie Len = %d"), padLen);

        /* padding zero if more than 5 bytes are required */
        if (padLen > MIN_VENDOR_SPECIFIC_IE_SIZE)
            vos_mem_set( pFrame + header_offset + nPayload + addIeLen + MIN_VENDOR_SPECIFIC_IE_SIZE,
                         padLen - MIN_VENDOR_SPECIFIC_IE_SIZE, 0);
    }
#endif
    limLog(pMac, LOG1, FL("[TDLS] action %d (%s) -%s-> OTA peer="MAC_ADDRESS_STR),
                       SIR_MAC_TDLS_TEARDOWN,
                       limTraceTdlsActionString(SIR_MAC_TDLS_TEARDOWN),
                       ((reason == eSIR_MAC_TDLS_TEARDOWN_PEER_UNREACHABLE) ?
                       "AP": "DIRECT"),
                       MAC_ADDR_ARRAY(peerMac));

    halstatus = halTxFrameWithTxComplete( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_DATA,
                            ANI_TXDIR_TODS,
                            TID_AC_VI,
                            limTxComplete, pFrame,
                            limMgmtTXComplete,
                            HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        pMac->lim.mgmtFrameSessionId = 0xff;
        limLog(pMac, LOGE, FL("could not send TDLS Teardown frame"));
        return eSIR_FAILURE;

    }
    pMac->lim.mgmtFrameSessionId = psessionEntry->peSessionId;
    return eSIR_SUCCESS;

}

/*
 * Send Setup RSP frame on AP link.
 */
static tSirRetStatus limSendTdlsSetupRspFrame(tpAniSirGlobal pMac, 
                    tSirMacAddr peerMac, tANI_U8 dialog, tpPESession psessionEntry, 
                    etdlsLinkSetupStatus setupStatus, tANI_U8 *addIe, tANI_U16 addIeLen )
{
    tDot11fTDLSSetupRsp  tdlsSetupRsp ;
    tANI_U32            status = 0 ;
    tANI_U16            caps = 0 ;            
    tANI_U32            nPayload = 0 ;
    tANI_U32            header_offset = 0 ;
    tANI_U32            nBytes = 0 ;
    tANI_U8            *pFrame;
    void               *pPacket;
    eHalStatus          halstatus;
    uint32             selfDot11Mode;
//  Placeholder to support different channel bonding mode of TDLS than AP.
//  Today, WNI_CFG_CHANNEL_BONDING_MODE will be overwritten when connecting to AP
//  To support this feature, we need to introduce WNI_CFG_TDLS_CHANNEL_BONDING_MODE
//  As of now, we hardcoded to max channel bonding of dot11Mode (i.e HT80 for 11ac/HT40 for 11n)
//  uint32 tdlsChannelBondingMode;

    /* 
     * The scheme here is to fill out a 'tDot11fProbeRequest' structure
     * and then hand it off to 'dot11fPackProbeRequest' (for
     * serialization).  We start by zero-initializing the structure:
     */
    vos_mem_set( ( tANI_U8* )&tdlsSetupRsp, sizeof( tDot11fTDLSSetupRsp ),0 );

    /*
     * setup Fixed fields,
     */
    tdlsSetupRsp.Category.category = SIR_MAC_ACTION_TDLS;
    tdlsSetupRsp.Action.action     = SIR_MAC_TDLS_SETUP_RSP ;
    tdlsSetupRsp.DialogToken.token = dialog;

    PopulateDot11fLinkIden( pMac, psessionEntry, &tdlsSetupRsp.LinkIdentifier,
                 peerMac, TDLS_RESPONDER) ;

    if (cfgGetCapabilityInfo(pMac, &caps, psessionEntry) != eSIR_SUCCESS)
    {
        /*
         * Could not get Capabilities value
         * from CFG. Log error.
         */
         limLog(pMac, LOGE, FL("could not retrieve Capabilities value"));
    }
    swapBitField16(caps, ( tANI_U16* )&tdlsSetupRsp.Capabilities );

    /* ipopulate supported rate IE */
    PopulateDot11fSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL, 
                                &tdlsSetupRsp.SuppRates, psessionEntry );
   
    /* Populate extended supported rates */
    PopulateDot11fExtSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL,
                                &tdlsSetupRsp.ExtSuppRates, psessionEntry );

    /* Populate extended supported rates */
    PopulateDot11fTdlsExtCapability( pMac, &tdlsSetupRsp.ExtCap );

    if ( 1 == pMac->lim.gLimTDLSWmmMode )
    {
        /* include WMM IE */
        PopulateDot11fWMMInfoStation( pMac, &tdlsSetupRsp.WMMInfoStation );
    }
    else
    {
        /*
         * TODO: we need to see if we have to support conditions where we have
         * EDCA parameter info element is needed a) if we need different QOS
         * parameters for off channel operations or QOS is not supported on
         * AP link and we wanted to QOS on direct link.
         */
        /* Populate QOS info, needed for Peer U-APSD session */
        /* TODO: Now hardcoded, because PopulateDot11fQOSCapsStation() depends on AP's capability, and
         TDLS doesn't want to depend on AP's capability */
        limLog(pMac, LOG1, FL("populate QOS IE in Setup Response frame"));
        tdlsSetupRsp.QOSCapsStation.present = 1;
        tdlsSetupRsp.QOSCapsStation.max_sp_length = 0;
        tdlsSetupRsp.QOSCapsStation.qack = 0;
        tdlsSetupRsp.QOSCapsStation.acbe_uapsd = ((pMac->lim.gLimTDLSUapsdMask & 0x08) >> 3);
        tdlsSetupRsp.QOSCapsStation.acbk_uapsd = ((pMac->lim.gLimTDLSUapsdMask & 0x04) >> 2);
        tdlsSetupRsp.QOSCapsStation.acvi_uapsd = ((pMac->lim.gLimTDLSUapsdMask & 0x02) >> 1);
        tdlsSetupRsp.QOSCapsStation.acvo_uapsd = (pMac->lim.gLimTDLSUapsdMask & 0x01);
    }

    wlan_cfgGetInt(pMac,WNI_CFG_DOT11_MODE,&selfDot11Mode);

    /* Populate HT/VHT Capabilities */
    PopulateDot11fTdlsHtVhtCap( pMac, selfDot11Mode, &tdlsSetupRsp.HTCaps,
                                &tdlsSetupRsp.VHTCaps, psessionEntry );

    /* Populate AID */
    PopulateDotfTdlsVhtAID( pMac, selfDot11Mode, peerMac,
                            &tdlsSetupRsp.AID, psessionEntry );

    if ( 1 == pMac->lim.gLimTDLSOffChannelEnabled )
        PopulateDot11fTdlsOffchannelParams( pMac, psessionEntry,
                                            &tdlsSetupRsp.SuppChannels,
                                            &tdlsSetupRsp.SuppOperatingClasses);

    tdlsSetupRsp.Status.status = setupStatus ;

    if ( 1 == pMac->lim.gLimTDLSOffChannelEnabled &&
         ( pMac->roam.configParam.bandCapability != eCSR_BAND_24))
    {
        tdlsSetupRsp.HT2040BSSCoexistence.present = 1;
        tdlsSetupRsp.HT2040BSSCoexistence.infoRequest = 1;
    }
    /* 
     * now we pack it.  First, how much space are we going to need?
     */
    status = dot11fGetPackedTDLSSetupRspSize( pMac, &tdlsSetupRsp, 
                                                     &nPayload);
    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to calculate the packed size for a Setup Response (0x%08x)."),
               status);
        /* We'll fall back on the worst case scenario: */
        nPayload = sizeof( tDot11fProbeRequest );
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog(pMac, LOGW,
               FL("There were warnings while calculating the packed size for Setup Response (0x%08x)."),
               status);
    }

    /*
     * This frame is going out from PE as data frames with special ethertype
     * 89-0d.
     * 8 bytes of RFC 1042 header
     */ 


    nBytes = nPayload + ((IS_QOS_ENABLED(psessionEntry))
                              ? sizeof(tSirMacDataHdr3a) : sizeof(tSirMacMgmtHdr))
                      + sizeof( eth_890d_header )
                      + PAYLOAD_TYPE_TDLS_SIZE
                      + addIeLen;

    /* Ok-- try to allocate memory from MGMT PKT pool */

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to allocate %d bytes for a TDLS Setup Response."),
               nBytes);
        return eSIR_MEM_ALLOC_FAILED;
    }

    /* zero out the memory */
    vos_mem_set(  pFrame, nBytes, 0 );

    /* 
     * IE formation, memory allocation is completed, Now form TDLS discovery
     * request frame
     */

    /* fill out the buffer descriptor */

    header_offset = limPrepareTdlsFrameHeader(pMac, pFrame, 
                                 LINK_IDEN_ADDR_OFFSET(tdlsSetupRsp), 
                                       TDLS_LINK_AP, TDLS_RESPONDER,
                                       TID_AC_BK, psessionEntry) ;

#ifdef FEATURE_WLAN_TDLS_NEGATIVE
    if(pMac->lim.gLimTdlsNegativeBehavior & LIM_TDLS_NEGATIVE_WRONG_BSSID_IN_SETUP_RSP)
    {
        tdlsSetupRsp.LinkIdentifier.bssid[4] = 0xde;
        tdlsSetupRsp.LinkIdentifier.bssid[5] = 0xad; 
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
        ("TDLS negative running: wrong BSSID " MAC_ADDRESS_STR " in TDLS Setup Rsp"),
         MAC_ADDR_ARRAY(tdlsSetupRsp.LinkIdentifier.bssid));
    }
#endif
    limLog(pMac, LOG1,
           FL("SupportedChnlWidth %x rxMCSMap %x rxMCSMap %x txSupDataRate %x"),
           tdlsSetupRsp.VHTCaps.supportedChannelWidthSet,
           tdlsSetupRsp.VHTCaps.rxMCSMap,
           tdlsSetupRsp.VHTCaps.txMCSMap,
           tdlsSetupRsp.VHTCaps.txSupDataRate);
    status = dot11fPackTDLSSetupRsp( pMac, &tdlsSetupRsp, pFrame 
                               + header_offset, nPayload, &nPayload );

    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE, FL("Failed to pack a TDLS Setup Response (0x%08x)."),
               status);
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, 
                                   ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog(pMac, LOGW,
               FL("There were warnings while packing TDLS Setup Response (0x%08x)."),
               status);
    }

    //Copy the additional IE. 
    //TODO : addIe is added at the end of the frame. This means it doesnt
    //follow the order. This should be ok, but we should consider changing this
    //if there is any IOT issue.
    if( addIeLen != 0 )
    {
       vos_mem_copy( pFrame + header_offset + nPayload, addIe, addIeLen );
    }

    limLog(pMac, LOG1,
           FL("[TDLS] action %d (%s) -AP-> OTA peer="MAC_ADDRESS_STR),
           SIR_MAC_TDLS_SETUP_RSP,
           limTraceTdlsActionString(SIR_MAC_TDLS_SETUP_RSP),
           MAC_ADDR_ARRAY(peerMac));

    halstatus = halTxFrameWithTxComplete( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_DATA,
                            ANI_TXDIR_TODS,
                            //ANI_TXDIR_IBSS,
                            TID_AC_BK,
                            limTxComplete, pFrame,
                            limMgmtTXComplete,
                            HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        pMac->lim.mgmtFrameSessionId = 0xff;
        limLog(pMac, LOGE, FL("could not send TDLS Setup Response"));
        return eSIR_FAILURE;
    }
    pMac->lim.mgmtFrameSessionId = psessionEntry->peSessionId;

    return eSIR_SUCCESS;

}

/*
 * Send TDLS setup CNF frame on AP link
 */

tSirRetStatus limSendTdlsLinkSetupCnfFrame(tpAniSirGlobal pMac, tSirMacAddr peerMac,
                    tANI_U8 dialog, tANI_U32 peerCapability, tpPESession psessionEntry, tANI_U8* addIe, tANI_U16 addIeLen)
{
    tDot11fTDLSSetupCnf  tdlsSetupCnf ;
    tANI_U32            status = 0 ;
    tANI_U32            nPayload = 0 ;
    tANI_U32            nBytes = 0 ;
    tANI_U32            header_offset = 0 ;
    tANI_U8            *pFrame;
    void               *pPacket;
    eHalStatus          halstatus;
#ifndef NO_PAD_TDLS_MIN_8023_SIZE
    tANI_U32            padLen = 0;
#endif

    /* 
     * The scheme here is to fill out a 'tDot11fProbeRequest' structure
     * and then hand it off to 'dot11fPackProbeRequest' (for
     * serialization).  We start by zero-initializing the structure:
     */
    vos_mem_set( ( tANI_U8* )&tdlsSetupCnf, sizeof( tDot11fTDLSSetupCnf ), 0 );

    /*
     * setup Fixed fields,
     */
    tdlsSetupCnf.Category.category = SIR_MAC_ACTION_TDLS;
    tdlsSetupCnf.Action.action     = SIR_MAC_TDLS_SETUP_CNF ;
    tdlsSetupCnf.DialogToken.token = dialog ;

#if 1
    PopulateDot11fLinkIden( pMac, psessionEntry, &tdlsSetupCnf.LinkIdentifier,
                      peerMac, TDLS_INITIATOR) ;
#else
    vos_mem_copy( (tANI_U8 *)&tdlsSetupCnf.LinkIdentifier,
                  (tANI_U8 *)&setupRsp->LinkIdentifier, sizeof(tDot11fIELinkIdentifier)) ;
#endif

    /* 
     * TODO: we need to see if we have to support conditions where we have
     * EDCA parameter info element is needed a) if we need different QOS
     * parameters for off channel operations or QOS is not supported on 
     * AP link and we wanted to QOS on direct link.
     */

    /* Check self and peer WMM capable */
    if ((1 == pMac->lim.gLimTDLSWmmMode) && (CHECK_BIT(peerCapability, TDLS_PEER_WMM_CAP)))
    {
       limLog(pMac, LOG1, FL("populate WMM praram in Setup Confirm"));
       PopulateDot11fWMMParams(pMac, &tdlsSetupCnf.WMMParams, psessionEntry);
    }

     /* Check peer is VHT capable*/
    if (CHECK_BIT(peerCapability, TDLS_PEER_VHT_CAP))
    {
       PopulateDot11fVHTOperation( pMac, &tdlsSetupCnf.VHTOperation);
       PopulateDot11fHTInfo( pMac, &tdlsSetupCnf.HTInfo, psessionEntry );
    }
    else if (CHECK_BIT(peerCapability, TDLS_PEER_HT_CAP)) /* Check peer is HT capable */
    {
       PopulateDot11fHTInfo( pMac, &tdlsSetupCnf.HTInfo, psessionEntry );
    }

    if ( 1 == pMac->lim.gLimTDLSOffChannelEnabled &&
         ( pMac->roam.configParam.bandCapability != eCSR_BAND_24))
    {
        tdlsSetupCnf.HT2040BSSCoexistence.present = 1;
        tdlsSetupCnf.HT2040BSSCoexistence.infoRequest = 1;
    }

    /* 
     * now we pack it.  First, how much space are we going to need?
     */
    status = dot11fGetPackedTDLSSetupCnfSize( pMac, &tdlsSetupCnf, 
                                                     &nPayload);
    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to calculate the packed size for Setup Confirm (0x%08x)."),
               status);
        /* We'll fall back on the worst case scenario: */
        nPayload = sizeof( tDot11fProbeRequest );
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog(pMac, LOGW,
               FL("There were warnings while calculating the packed size for Setup Confirm (0x%08x)."),
               status);
    }

    /*
     * This frame is going out from PE as data frames with special ethertype
     * 89-0d.
     * 8 bytes of RFC 1042 header
     */ 


    nBytes = nPayload + ((IS_QOS_ENABLED(psessionEntry))
                              ? sizeof(tSirMacDataHdr3a) : sizeof(tSirMacMgmtHdr))
                      + sizeof( eth_890d_header )
                      + PAYLOAD_TYPE_TDLS_SIZE
                      + addIeLen;

#ifndef NO_PAD_TDLS_MIN_8023_SIZE
    /* IOT issue with some AP : some AP doesn't like the data packet size < minimum 802.3 frame length (64)
       Hence AP itself padding some bytes, which caused teardown packet is dropped at
       receiver side. To avoid such IOT issue, we added some extra bytes to meet data frame size >= 64
     */
    if (nPayload + PAYLOAD_TYPE_TDLS_SIZE < MIN_IEEE_8023_SIZE)
    {
        padLen = MIN_IEEE_8023_SIZE - (nPayload + PAYLOAD_TYPE_TDLS_SIZE ) ;

        /* if padLen is less than minimum vendorSpecific (5), pad up to 5 */
        if (padLen < MIN_VENDOR_SPECIFIC_IE_SIZE)
            padLen = MIN_VENDOR_SPECIFIC_IE_SIZE;

        nBytes += padLen;
    }
#endif


    /* Ok-- try to allocate memory from MGMT PKT pool */

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to allocate %d bytes for a TDLS Setup Confirm."),
               nBytes);
        return eSIR_MEM_ALLOC_FAILED;
    }

    /* zero out the memory */
    vos_mem_set( pFrame, nBytes, 0 );

    /* 
     * IE formation, memory allocation is completed, Now form TDLS discovery
     * request frame
     */

    /* fill out the buffer descriptor */

    header_offset = limPrepareTdlsFrameHeader(pMac, pFrame, 
                     LINK_IDEN_ADDR_OFFSET(tdlsSetupCnf), TDLS_LINK_AP, TDLS_INITIATOR,
                     TID_AC_VI, psessionEntry) ;

#ifdef FEATURE_WLAN_TDLS_NEGATIVE
    if(pMac->lim.gLimTdlsNegativeBehavior & LIM_TDLS_NEGATIVE_STATUS_37_IN_SETUP_CNF) {
        tdlsSetupCnf.StatusCode.statusCode = 37;
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
        ("TDLS negative running: StatusCode = 37 in TDLS Setup Cnf"));
    }
#endif
    status = dot11fPackTDLSSetupCnf( pMac, &tdlsSetupCnf, pFrame 
                               + header_offset, nPayload, &nPayload );

    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to pack a TDLS Setup Confirm (0x%08x)."),
               status);
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, 
                                   ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog(pMac, LOGW,
               FL("There were warnings while packing TDLS Setup Confirm (0x%08x)."),
               status);
    }
#if 0
    if(pMac->hal.pCBackFnTxComp == NULL) 
    {
        pMac->hal.pCBackFnTxComp = (tpCBackFnTxComp)limTdlsSetupCnfTxComplete;
        if(TX_SUCCESS != tx_timer_activate(&pMac->hal.txCompTimer)) 
        {
            status = eHAL_STATUS_FAILURE;
            return status;
                
        }
    }
    else
    {
        VOS_ASSERT(0) ;
        return status ;
    }
#endif
    //Copy the additional IE. 
    //TODO : addIe is added at the end of the frame. This means it doesnt
    //follow the order. This should be ok, but we should consider changing this
    //if there is any IOT issue.
    if( addIeLen != 0 )
    {
       vos_mem_copy( pFrame + header_offset + nPayload, addIe, addIeLen );
    }

#ifndef NO_PAD_TDLS_MIN_8023_SIZE
    if (padLen != 0)
    {
        /* QCOM VENDOR OUI = { 0x00, 0xA0, 0xC6, type = 0x0000 }; */
        tANI_U8 *padVendorSpecific = pFrame + header_offset + nPayload + addIeLen;
        /* make QCOM_VENDOR_OUI, and type = 0x0000, and all the payload to be zero */
        padVendorSpecific[0] = 221;
        padVendorSpecific[1] = padLen - 2;
        padVendorSpecific[2] = 0x00;
        padVendorSpecific[3] = 0xA0;
        padVendorSpecific[4] = 0xC6;

        limLog(pMac, LOG1, FL("Padding Vendor Specific Ie Len = %d"), padLen);

        /* padding zero if more than 5 bytes are required */
        if (padLen > MIN_VENDOR_SPECIFIC_IE_SIZE)
            vos_mem_set( pFrame + header_offset + nPayload + addIeLen + MIN_VENDOR_SPECIFIC_IE_SIZE,
                         padLen - MIN_VENDOR_SPECIFIC_IE_SIZE, 0);
    }
#endif


    limLog(pMac, LOG1,
           FL("[TDLS] action %d (%s) -AP-> OTA peer="MAC_ADDRESS_STR),
           SIR_MAC_TDLS_SETUP_CNF,
           limTraceTdlsActionString(SIR_MAC_TDLS_SETUP_CNF),
           MAC_ADDR_ARRAY(peerMac));

    halstatus = halTxFrameWithTxComplete( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_DATA,
                            ANI_TXDIR_TODS,
                            TID_AC_VI,
                            limTxComplete, pFrame, 
                            limMgmtTXComplete,
                            HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME );


    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        pMac->lim.mgmtFrameSessionId = 0xff;
        limLog(pMac, LOGE, FL("could not send TDLS Setup Confirm frame"));
        return eSIR_FAILURE;

    }
    pMac->lim.mgmtFrameSessionId = psessionEntry->peSessionId;

    return eSIR_SUCCESS;
}

#ifdef FEATURE_WLAN_TDLS_INTERNAL
/*
 * Convert HT caps to lim based HT caps 
 */
static void limTdlsCovertHTCaps(tpAniSirGlobal pMac,
                         tSirTdlsPeerInfo *peerInfo, tDot11fIEHTCaps *HTCaps)
{

    /* HT Capability Info */
    peerInfo->tdlsPeerHtCaps.advCodingCap = HTCaps->advCodingCap ;
    peerInfo->tdlsPeerHtCaps.supportedChannelWidthSet = 
                                            HTCaps->supportedChannelWidthSet ;
    peerInfo->tdlsPeerHtCaps.mimoPowerSave = HTCaps->mimoPowerSave ;
    peerInfo->tdlsPeerHtCaps.greenField = HTCaps->greenField ;
    peerInfo->tdlsPeerHtCaps.shortGI20MHz = HTCaps->shortGI20MHz ;
    peerInfo->tdlsPeerHtCaps.shortGI40MHz = HTCaps->shortGI40MHz ;
    peerInfo->tdlsPeerHtCaps.txSTBC = HTCaps->txSTBC ;
    peerInfo->tdlsPeerHtCaps.rxSTBC = HTCaps->rxSTBC ;
    peerInfo->tdlsPeerHtCaps.delayedBA = HTCaps->delayedBA;
    peerInfo->tdlsPeerHtCaps.maximalAMSDUsize = HTCaps->maximalAMSDUsize ;
    peerInfo->tdlsPeerHtCaps.dsssCckMode40MHz = HTCaps->dsssCckMode40MHz ;
    peerInfo->tdlsPeerHtCaps.psmp = HTCaps->stbcControlFrame ;
    peerInfo->tdlsPeerHtCaps.stbcControlFrame = HTCaps->stbcControlFrame ;
    peerInfo->tdlsPeerHtCaps.lsigTXOPProtection = 
                                                 HTCaps->lsigTXOPProtection ;

    /* HT Capa parameters */
    peerInfo->tdlsPeerHtParams.maxRxAMPDUFactor = HTCaps->maxRxAMPDUFactor ;
    peerInfo->tdlsPeerHtParams.mpduDensity = HTCaps->mpduDensity ;
    peerInfo->tdlsPeerHtParams.reserved = HTCaps->reserved1 ;
    
    /* Extended HT caps */
    peerInfo->tdlsPeerHtExtCaps.pco = HTCaps->pco ;
    peerInfo->tdlsPeerHtExtCaps.transitionTime = HTCaps->transitionTime ;
    peerInfo->tdlsPeerHtExtCaps.mcsFeedback = HTCaps->mcsFeedback ;
    vos_mem_copy( peerInfo->supportedMCSSet,
                      HTCaps->supportedMCSSet, SIZE_OF_SUPPORTED_MCS_SET) ;

    return ;
}

/*
 * update capability info..
 */
void tdlsUpdateCapInfo(tSirMacCapabilityInfo *capabilityInfo, 
                                tDot11fFfCapabilities *Capabilities)
{

    capabilityInfo->ess            = Capabilities->ess;
    capabilityInfo->ibss           = Capabilities->ibss;
    capabilityInfo->cfPollable     = Capabilities->cfPollable;
    capabilityInfo->cfPollReq      = Capabilities->cfPollReq;
    capabilityInfo->privacy        = Capabilities->privacy;
    capabilityInfo->shortPreamble  = Capabilities->shortPreamble;
    capabilityInfo->pbcc           = Capabilities->pbcc;
    capabilityInfo->channelAgility = Capabilities->channelAgility;
    capabilityInfo->spectrumMgt    = Capabilities->spectrumMgt;
    capabilityInfo->qos            = Capabilities->qos;
    capabilityInfo->shortSlotTime  = Capabilities->shortSlotTime;
    capabilityInfo->apsd           = Capabilities->apsd;
    capabilityInfo->rrm            = Capabilities->rrm;
    capabilityInfo->dsssOfdm       = Capabilities->dsssOfdm;
    capabilityInfo->immediateBA    = Capabilities->immediateBA;

    return ;
}

/*
 * update Peer info from the link request frame recieved from Peer..
 * in list of STA participating in TDLS link setup
 */
void limTdlsUpdateLinkReqPeerInfo(tpAniSirGlobal pMac, 
                                 tLimTdlsLinkSetupPeer *setupPeer, 
                                             tDot11fTDLSSetupReq *setupReq)
{

    /* Populate peer info of tdls discovery result */

    tdlsUpdateCapInfo(&setupPeer->capabilityInfo, &setupReq->Capabilities) ;

    if(setupReq->SuppRates.present)
    {
        ConvertSuppRates( pMac, &setupPeer->supportedRates, 
                                            &setupReq->SuppRates );
    }

    /* update QOS info, needed for Peer U-APSD session */
    if(setupReq->QOSCapsStation.present)
    {
       ConvertQOSCapsStation(pMac->hHdd, &setupPeer->qosCaps, 
                   &setupReq->QOSCapsStation) ;
       LIM_LOG_TDLS(VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR,("setupReq->SPLen=%d (be %d %d %d %d vo) more %d qack %d."),
         setupReq->QOSCapsStation.max_sp_length, setupReq->QOSCapsStation.acbe_uapsd,
         setupReq->QOSCapsStation.acbk_uapsd, setupReq->QOSCapsStation.acvi_uapsd,
         setupReq->QOSCapsStation.acvo_uapsd, setupReq->QOSCapsStation.more_data_ack,
         setupReq->QOSCapsStation.qack));
    }
    
    if (setupReq->ExtSuppRates.present)
    {
        setupPeer->ExtRatesPresent = 1;
        ConvertExtSuppRates( pMac, &setupPeer->extendedRates,
                                                &setupReq->ExtSuppRates );
    }
    /* update HT caps */
    if (setupReq->HTCaps.present)
    {
        vos_mem_copy( &setupPeer->tdlsPeerHTCaps,
                    &setupReq->HTCaps, sizeof(tDot11fIEHTCaps)) ;
    }
    /* Update EXT caps */
    if (setupReq->ExtCap.present)
    {
        vos_mem_copy( &setupPeer->tdlsPeerExtCaps,
                    &setupReq->ExtCap, sizeof(tDot11fIEExtCap)) ;
    }    

    return ;
}

/*
 * update peer Info recieved with TDLS setup RSP 
 */
void limTdlsUpdateLinkRspPeerInfo(tpAniSirGlobal pMac, 
                                   tLimTdlsLinkSetupPeer *setupPeer, 
                                             tDot11fTDLSSetupRsp *setupRsp)
{

    /* Populate peer info of tdls discovery result */
    tdlsUpdateCapInfo(&setupPeer->capabilityInfo, &setupRsp->Capabilities) ;

    if(setupRsp->SuppRates.present)
    {
        tDot11fIESuppRates *suppRates = &setupRsp->SuppRates ;
        ConvertSuppRates( pMac, &setupPeer->supportedRates, suppRates);
    }

    /* update QOS info, needed for Peer U-APSD session */
    if(setupRsp->QOSCapsStation.present)
    {
       ConvertQOSCapsStation(pMac->hHdd, &setupPeer->qosCaps, 
                   &setupRsp->QOSCapsStation) ;
       LIM_LOG_TDLS(VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, ("setupRsp->SPLen=%d (be %d %d %d %d vo) more %d qack %d."),
         setupRsp->QOSCapsStation.max_sp_length, setupRsp->QOSCapsStation.acbe_uapsd,
         setupRsp->QOSCapsStation.acbk_uapsd, setupRsp->QOSCapsStation.acvi_uapsd,
         setupRsp->QOSCapsStation.acvo_uapsd, setupRsp->QOSCapsStation.more_data_ack,
         setupRsp->QOSCapsStation.qack));
    }
    
    if(setupRsp->ExtSuppRates.present)
    {
        setupPeer->ExtRatesPresent = 1;
        ConvertExtSuppRates( pMac, &setupPeer->extendedRates,
                                                &setupRsp->ExtSuppRates );
    }
    /* update HT caps */
    if (setupRsp->HTCaps.present)
    {
        vos_mem_copy(&setupPeer->tdlsPeerHTCaps,
                    &setupRsp->HTCaps, sizeof(tDot11fIEHTCaps)) ;
    }

    /* update EXT caps */
    if (setupRsp->ExtCap.present)
    {
        vos_mem_copy( &setupPeer->tdlsPeerExtCaps,
                    &setupRsp->ExtCap, sizeof(tDot11fIEExtCap)) ;
    }

    return ;
}
#endif

/* This Function is similar to PopulateDot11fHTCaps, except that the HT Capabilities
 * are considered from the AddStaReq rather from the cfg.dat as in PopulateDot11fHTCaps
 */
static tSirRetStatus limTdlsPopulateDot11fHTCaps(tpAniSirGlobal pMac, tpPESession psessionEntry,
            tSirTdlsAddStaReq *pTdlsAddStaReq, tDot11fIEHTCaps *pDot11f)
{
    tANI_U32                         nCfgValue;
    tANI_U8                          nCfgValue8;
    tSirMacHTParametersInfo         *pHTParametersInfo;
    union {
        tANI_U16                        nCfgValue16;
        tSirMacHTCapabilityInfo         htCapInfo;
        tSirMacExtendedHTCapabilityInfo extHtCapInfo;
    } uHTCapabilityInfo;

    tSirMacTxBFCapabilityInfo       *pTxBFCapabilityInfo;
    tSirMacASCapabilityInfo         *pASCapabilityInfo;

    nCfgValue = pTdlsAddStaReq->htCap.capInfo;

    uHTCapabilityInfo.nCfgValue16 = nCfgValue & 0xFFFF;

    pDot11f->advCodingCap             = uHTCapabilityInfo.htCapInfo.advCodingCap;
    pDot11f->mimoPowerSave            = uHTCapabilityInfo.htCapInfo.mimoPowerSave;
    pDot11f->greenField               = uHTCapabilityInfo.htCapInfo.greenField;
    pDot11f->shortGI20MHz             = uHTCapabilityInfo.htCapInfo.shortGI20MHz;
    pDot11f->shortGI40MHz             = uHTCapabilityInfo.htCapInfo.shortGI40MHz;
    pDot11f->txSTBC                   = uHTCapabilityInfo.htCapInfo.txSTBC;
    pDot11f->rxSTBC                   = uHTCapabilityInfo.htCapInfo.rxSTBC;
    pDot11f->delayedBA                = uHTCapabilityInfo.htCapInfo.delayedBA;
    pDot11f->maximalAMSDUsize         = uHTCapabilityInfo.htCapInfo.maximalAMSDUsize;
    pDot11f->dsssCckMode40MHz         = uHTCapabilityInfo.htCapInfo.dsssCckMode40MHz;
    pDot11f->psmp                     = uHTCapabilityInfo.htCapInfo.psmp;
    pDot11f->stbcControlFrame         = uHTCapabilityInfo.htCapInfo.stbcControlFrame;
    pDot11f->lsigTXOPProtection       = uHTCapabilityInfo.htCapInfo.lsigTXOPProtection;

    // All sessionized entries will need the check below
    if (psessionEntry == NULL) // Only in case of NO session
    {
        pDot11f->supportedChannelWidthSet = uHTCapabilityInfo.htCapInfo.supportedChannelWidthSet;
    }
    else
    {
        pDot11f->supportedChannelWidthSet = psessionEntry->htSupportedChannelWidthSet;
    }

    /* Ensure that shortGI40MHz is Disabled if supportedChannelWidthSet is
       eHT_CHANNEL_WIDTH_20MHZ */
    if(pDot11f->supportedChannelWidthSet == eHT_CHANNEL_WIDTH_20MHZ)
    {
       pDot11f->shortGI40MHz = 0;
    }

    limLog(pMac, LOG1,
           FL("SupportedChnlWidth: %d, mimoPS: %d, GF: %d, shortGI20:%d, shortGI40: %d, dsssCck: %d"),
           pDot11f->supportedChannelWidthSet,
           pDot11f->mimoPowerSave,
           pDot11f->greenField,
           pDot11f->shortGI20MHz,
           pDot11f->shortGI40MHz,
           pDot11f->dsssCckMode40MHz);

    nCfgValue = pTdlsAddStaReq->htCap.ampduParamsInfo;

    nCfgValue8 = ( tANI_U8 ) nCfgValue;
    pHTParametersInfo = ( tSirMacHTParametersInfo* ) &nCfgValue8;

    pDot11f->maxRxAMPDUFactor = pHTParametersInfo->maxRxAMPDUFactor;
    pDot11f->mpduDensity      = pHTParametersInfo->mpduDensity;
    pDot11f->reserved1        = pHTParametersInfo->reserved;

    limLog(pMac, LOG1, FL("AMPDU Param: %x"), nCfgValue);

    vos_mem_copy( pDot11f->supportedMCSSet, pTdlsAddStaReq->htCap.suppMcsSet,
                  SIZE_OF_SUPPORTED_MCS_SET);

    nCfgValue = pTdlsAddStaReq->htCap.extendedHtCapInfo;

    uHTCapabilityInfo.nCfgValue16 = nCfgValue & 0xFFFF;

    pDot11f->pco            = uHTCapabilityInfo.extHtCapInfo.pco;
    pDot11f->transitionTime = uHTCapabilityInfo.extHtCapInfo.transitionTime;
    pDot11f->mcsFeedback    = uHTCapabilityInfo.extHtCapInfo.mcsFeedback;

    nCfgValue = pTdlsAddStaReq->htCap.txBFCapInfo;

    pTxBFCapabilityInfo = ( tSirMacTxBFCapabilityInfo* ) &nCfgValue;
    pDot11f->txBF                                       = pTxBFCapabilityInfo->txBF;
    pDot11f->rxStaggeredSounding                        = pTxBFCapabilityInfo->rxStaggeredSounding;
    pDot11f->txStaggeredSounding                        = pTxBFCapabilityInfo->txStaggeredSounding;
    pDot11f->rxZLF                                      = pTxBFCapabilityInfo->rxZLF;
    pDot11f->txZLF                                      = pTxBFCapabilityInfo->txZLF;
    pDot11f->implicitTxBF                               = pTxBFCapabilityInfo->implicitTxBF;
    pDot11f->calibration                                = pTxBFCapabilityInfo->calibration;
    pDot11f->explicitCSITxBF                            = pTxBFCapabilityInfo->explicitCSITxBF;
    pDot11f->explicitUncompressedSteeringMatrix         = pTxBFCapabilityInfo->explicitUncompressedSteeringMatrix;
    pDot11f->explicitBFCSIFeedback                      = pTxBFCapabilityInfo->explicitBFCSIFeedback;
    pDot11f->explicitUncompressedSteeringMatrixFeedback = pTxBFCapabilityInfo->explicitUncompressedSteeringMatrixFeedback;
    pDot11f->explicitCompressedSteeringMatrixFeedback   = pTxBFCapabilityInfo->explicitCompressedSteeringMatrixFeedback;
    pDot11f->csiNumBFAntennae                           = pTxBFCapabilityInfo->csiNumBFAntennae;
    pDot11f->uncompressedSteeringMatrixBFAntennae       = pTxBFCapabilityInfo->uncompressedSteeringMatrixBFAntennae;
    pDot11f->compressedSteeringMatrixBFAntennae         = pTxBFCapabilityInfo->compressedSteeringMatrixBFAntennae;

    nCfgValue = pTdlsAddStaReq->htCap.antennaSelectionInfo;

    nCfgValue8 = ( tANI_U8 ) nCfgValue;

    pASCapabilityInfo = ( tSirMacASCapabilityInfo* ) &nCfgValue8;
    pDot11f->antennaSelection         = pASCapabilityInfo->antennaSelection;
    pDot11f->explicitCSIFeedbackTx    = pASCapabilityInfo->explicitCSIFeedbackTx;
    pDot11f->antennaIndicesFeedbackTx = pASCapabilityInfo->antennaIndicesFeedbackTx;
    pDot11f->explicitCSIFeedback      = pASCapabilityInfo->explicitCSIFeedback;
    pDot11f->antennaIndicesFeedback   = pASCapabilityInfo->antennaIndicesFeedback;
    pDot11f->rxAS                     = pASCapabilityInfo->rxAS;
    pDot11f->txSoundingPPDUs          = pASCapabilityInfo->txSoundingPPDUs;

    pDot11f->present = pTdlsAddStaReq->htcap_present;

    return eSIR_SUCCESS;

}

tSirRetStatus
limTdlsPopulateDot11fVHTCaps(tpAniSirGlobal pMac,
                      tSirTdlsAddStaReq *pTdlsAddStaReq,
                      tDot11fIEVHTCaps  *pDot11f)
{
    tANI_U32             nCfgValue=0;
    union {
        tANI_U32                       nCfgValue32;
        tSirMacVHTCapabilityInfo       vhtCapInfo;
    } uVHTCapabilityInfo;
    union {
        tANI_U16                       nCfgValue16;
        tSirMacVHTTxSupDataRateInfo    vhtTxSupDataRateInfo;
        tSirMacVHTRxSupDataRateInfo    vhtRxsupDataRateInfo;
    } uVHTSupDataRateInfo;

    pDot11f->present = pTdlsAddStaReq->vhtcap_present;

    nCfgValue = pTdlsAddStaReq->vhtCap.vhtCapInfo;
    uVHTCapabilityInfo.nCfgValue32 = nCfgValue;

    pDot11f->maxMPDULen =  uVHTCapabilityInfo.vhtCapInfo.maxMPDULen;
    pDot11f->supportedChannelWidthSet =  uVHTCapabilityInfo.vhtCapInfo.supportedChannelWidthSet;
    pDot11f->ldpcCodingCap =  uVHTCapabilityInfo.vhtCapInfo.ldpcCodingCap;
    pDot11f->shortGI80MHz =  uVHTCapabilityInfo.vhtCapInfo.shortGI80MHz;
    pDot11f->shortGI160and80plus80MHz =  uVHTCapabilityInfo.vhtCapInfo.shortGI160and80plus80MHz;
    pDot11f->txSTBC =  uVHTCapabilityInfo.vhtCapInfo.txSTBC;
    pDot11f->rxSTBC =  uVHTCapabilityInfo.vhtCapInfo.rxSTBC;
    pDot11f->suBeamFormerCap =  uVHTCapabilityInfo.vhtCapInfo.suBeamFormerCap;
    pDot11f->suBeamformeeCap =  uVHTCapabilityInfo.vhtCapInfo.suBeamformeeCap;
    pDot11f->csnofBeamformerAntSup =  uVHTCapabilityInfo.vhtCapInfo.csnofBeamformerAntSup;
    pDot11f->numSoundingDim =  uVHTCapabilityInfo.vhtCapInfo.numSoundingDim;
    pDot11f->muBeamformerCap =  uVHTCapabilityInfo.vhtCapInfo.muBeamformerCap;
    pDot11f->muBeamformeeCap =  uVHTCapabilityInfo.vhtCapInfo.muBeamformeeCap;
    pDot11f->vhtTXOPPS =  uVHTCapabilityInfo.vhtCapInfo.vhtTXOPPS;
    pDot11f->htcVHTCap =  uVHTCapabilityInfo.vhtCapInfo.htcVHTCap;
    pDot11f->maxAMPDULenExp =  uVHTCapabilityInfo.vhtCapInfo.maxAMPDULenExp;
    pDot11f->vhtLinkAdaptCap =  uVHTCapabilityInfo.vhtCapInfo.vhtLinkAdaptCap;
    pDot11f->rxAntPattern =  uVHTCapabilityInfo.vhtCapInfo.rxAntPattern;
    pDot11f->txAntPattern =  uVHTCapabilityInfo.vhtCapInfo.txAntPattern;
    pDot11f->reserved1= uVHTCapabilityInfo.vhtCapInfo.reserved1;

    pDot11f->rxMCSMap = pTdlsAddStaReq->vhtCap.suppMcs.rxMcsMap;

    nCfgValue = pTdlsAddStaReq->vhtCap.suppMcs.rxHighest;
    uVHTSupDataRateInfo.nCfgValue16 = nCfgValue & 0xffff;
    pDot11f->rxHighSupDataRate = uVHTSupDataRateInfo.vhtRxsupDataRateInfo.rxSupDataRate;

    pDot11f->txMCSMap = pTdlsAddStaReq->vhtCap.suppMcs.txMcsMap;

    nCfgValue = pTdlsAddStaReq->vhtCap.suppMcs.txHighest;
    uVHTSupDataRateInfo.nCfgValue16 = nCfgValue & 0xffff;
    pDot11f->txSupDataRate = uVHTSupDataRateInfo.vhtTxSupDataRateInfo.txSupDataRate;

    pDot11f->reserved3= uVHTSupDataRateInfo.vhtTxSupDataRateInfo.reserved;

    limLogVHTCap(pMac, pDot11f);

    return eSIR_SUCCESS;

}

static tSirRetStatus
limTdlsPopulateMatchingRateSet(tpAniSirGlobal pMac,
                           tpDphHashNode pStaDs,
                           tANI_U8 *pSupportedRateSet,
                           tANI_U8 supporteRatesLength,
                           tANI_U8* pSupportedMCSSet,
                           tSirMacPropRateSet *pAniLegRateSet,
                           tpPESession  psessionEntry,
                           tDot11fIEVHTCaps *pVHTCaps)

{
    tSirMacRateSet    tempRateSet;
    tANI_U32          i,j,val,min,isArate;
    tSirMacRateSet    tempRateSet2;
    tANI_U32 phyMode;
    tANI_U8 mcsSet[SIZE_OF_SUPPORTED_MCS_SET];
    isArate=0;
    tempRateSet2.numRates = 0;

    // limGetPhyMode(pMac, &phyMode);
    limGetPhyMode(pMac, &phyMode, NULL);

    // get own rate set
    val = WNI_CFG_OPERATIONAL_RATE_SET_LEN;
    if (wlan_cfgGetStr(pMac, WNI_CFG_OPERATIONAL_RATE_SET,
                                          (tANI_U8 *) &tempRateSet.rate,
                                          &val) != eSIR_SUCCESS)
    {
        /// Could not get rateset from CFG. Log error.
        limLog(pMac, LOGE, FL("could not retrieve rateset"));
        val = 0;
    }
    tempRateSet.numRates = val;

    if (phyMode == WNI_CFG_PHY_MODE_11G)
    {

        // get own extended rate set
        val = WNI_CFG_EXTENDED_OPERATIONAL_RATE_SET_LEN;
        if (wlan_cfgGetStr(pMac, WNI_CFG_EXTENDED_OPERATIONAL_RATE_SET,
                                                  (tANI_U8 *) &tempRateSet2.rate,
                                                  &val) != eSIR_SUCCESS)
        tempRateSet2.numRates = val;
    }

    if ((tempRateSet.numRates + tempRateSet2.numRates) > 12)
    {
        limLog(pMac, LOGE, FL("more than 12 rates in CFG"));
        goto error;
    }

    /**
         * Handling of the rate set IEs is the following:
         * - keep only rates that we support and that the station supports
         * - sort and the rates into the pSta->rate array
         */

    // Copy all rates in tempRateSet, there are 12 rates max
    for (i = 0; i < tempRateSet2.numRates; i++)
        tempRateSet.rate[i + tempRateSet.numRates] = tempRateSet2.rate[i];

    tempRateSet.numRates += tempRateSet2.numRates;

    /**
         * Sort rates in tempRateSet (they are likely to be already sorted)
         * put the result in tempRateSet2
         */
    tempRateSet2.numRates = 0;

    for (i = 0;i < tempRateSet.numRates; i++)
    {
        min = 0;
        val = 0xff;

        for(j = 0;j < tempRateSet.numRates; j++)
            if ((tANI_U32) (tempRateSet.rate[j] & 0x7f) < val)
            {
                val = tempRateSet.rate[j] & 0x7f;
                min = j;
            }

        tempRateSet2.rate[tempRateSet2.numRates++] = tempRateSet.rate[min];
        tempRateSet.rate[min] = 0xff;
    }

    /**
     * Copy received rates in tempRateSet, the parser has ensured
     * unicity of the rates so there cannot be more than 12 . Need to Check this
     * TODO Sunil.
     */
    if (supporteRatesLength > SIR_MAC_RATESET_EID_MAX)
    {
       limLog(pMac, LOGW,
              FL("Supported rates length %d more than the Max limit, reset to Max"),
              supporteRatesLength);
       supporteRatesLength = SIR_MAC_RATESET_EID_MAX;
    }
    for (i = 0; i < supporteRatesLength; i++)
    {
        tempRateSet.rate[i] = pSupportedRateSet[i];
    }
    tempRateSet.numRates = supporteRatesLength;

    {
        tpSirSupportedRates  rates = &pStaDs->supportedRates;
        tANI_U8 aRateIndex = 0;
        tANI_U8 bRateIndex = 0;
        vos_mem_set( (tANI_U8 *) rates, sizeof(tSirSupportedRates), 0);

        for (i = 0;i < tempRateSet2.numRates; i++)
        {
            for (j = 0;j < tempRateSet.numRates; j++)
            {
                if ((tempRateSet2.rate[i] & 0x7F) ==
                    (tempRateSet.rate[j] & 0x7F))
                {
#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
                    if ((bRateIndex > HAL_NUM_11B_RATES) || (aRateIndex > HAL_NUM_11A_RATES))
                    {
                        limLog(pMac, LOGE, FL("Invalid number of rates (11b->%d, 11a->%d)"),
                                           bRateIndex,
                                           aRateIndex);
                        return eSIR_FAILURE;
                    }
#endif
                    if (sirIsArate(tempRateSet2.rate[i] & 0x7f))
                    {
                        isArate=1;
                        rates->llaRates[aRateIndex++] = tempRateSet2.rate[i];
                    }
                    else
                        rates->llbRates[bRateIndex++] = tempRateSet2.rate[i];
                    break;
                }
            }
        }
    }


    //compute the matching MCS rate set, if peer is 11n capable and self mode is 11n
#ifdef FEATURE_WLAN_TDLS
    if (pStaDs->mlmStaContext.htCapability)
#else
    if (IS_DOT11_MODE_HT(psessionEntry->dot11mode) &&
       (pStaDs->mlmStaContext.htCapability))
#endif
    {
        val = SIZE_OF_SUPPORTED_MCS_SET;
        if (wlan_cfgGetStr(pMac, WNI_CFG_SUPPORTED_MCS_SET,
                           mcsSet,
                           &val) != eSIR_SUCCESS)
        {
            /// Could not get rateset from CFG. Log error.
            limLog(pMac, LOGP, FL("could not retrieve supportedMCSSet"));
            goto error;
        }

        for (i=0; i<val; i++)
            pStaDs->supportedRates.supportedMCSSet[i] = mcsSet[i] & pSupportedMCSSet[i];

        limLog(pMac, LOG1, FL("MCS Rate Set Bitmap from CFG and DPH:"));
        for (i=0; i<SIR_MAC_MAX_SUPPORTED_MCS_SET; i++)
        {
            limLog(pMac, LOG1, FL("%x %x"),
                               mcsSet[i],
                               pStaDs->supportedRates.supportedMCSSet[i]);
        }
    }

#ifdef WLAN_FEATURE_11AC
    limPopulateVhtMcsSet(pMac, &pStaDs->supportedRates, pVHTCaps, psessionEntry);
#endif
    /**
         * Set the erpEnabled bit iff the phy is in G mode and at least
         * one A rate is supported
         */
    if ((phyMode == WNI_CFG_PHY_MODE_11G) && isArate)
        pStaDs->erpEnabled = eHAL_SET;



    return eSIR_SUCCESS;

 error:

    return eSIR_FAILURE;
}

static int limTdlsSelectCBMode(tDphHashNode *pStaDs, tpPESession psessionEntry)
{
    tANI_U8 channel = psessionEntry->currentOperChannel;

    if ( pStaDs->mlmStaContext.vhtCapability )
    {
        if ( channel== 36 || channel == 52 || channel == 100 ||
             channel == 116 || channel == 149 )
        {
           return PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW - 1;
        }
        else if ( channel == 40 || channel == 56 || channel == 104 ||
             channel == 120 || channel == 153 )
        {
           return PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW - 1;
        }
        else if ( channel == 44 || channel == 60 || channel == 108 ||
                  channel == 124 || channel == 157 )
        {
           return PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH -1;
        }
        else if ( channel == 48 || channel == 64 || channel == 112 ||
             channel == 128 || channel == 161 )
        {
            return PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH - 1;
        }
        else if ( channel == 165 )
        {
            return 0;
        }
    }
    else if ( pStaDs->mlmStaContext.htCapability )
    {
        if ( channel== 40 || channel == 48 || channel == 56 ||
             channel == 64 || channel == 104 || channel == 112 ||
             channel == 120 || channel == 128 || channel == 136 ||
             channel == 144 || channel == 153 || channel == 161 )
        {
           return 1;
        }
        else if ( channel== 36 || channel == 44 || channel == 52 ||
             channel == 60 || channel == 100 || channel == 108 ||
             channel == 116 || channel == 124 || channel == 132 ||
             channel == 140 || channel == 149 || channel == 157 )
        {
           return 2;
        }
        else if ( channel == 165 )
        {
           return 0;
        }
    }
    return 0;
}

/*
 * update HASH node entry info
 */
static void limTdlsUpdateHashNodeInfo(tpAniSirGlobal pMac, tDphHashNode *pStaDs,
              tSirTdlsAddStaReq *pTdlsAddStaReq, tpPESession psessionEntry)
{
    //tDot11fIEHTCaps *htCaps = &setupPeerInfo->tdlsPeerHTCaps ;
    tDot11fIEHTCaps htCap, *htCaps;
    tDot11fIEVHTCaps *pVhtCaps = NULL;
#ifdef WLAN_FEATURE_11AC
    tDot11fIEVHTCaps vhtCap;
    tANI_U8 cbMode;
#endif
    tpDphHashNode pSessStaDs = NULL;
    tANI_U16 aid;

    if (pTdlsAddStaReq->tdlsAddOper == TDLS_OPER_ADD)
    {
        PopulateDot11fHTCaps(pMac, psessionEntry, &htCap);
    }
    else if (pTdlsAddStaReq->tdlsAddOper == TDLS_OPER_UPDATE)
    {
        limTdlsPopulateDot11fHTCaps(pMac, NULL, pTdlsAddStaReq, &htCap);
    }
    htCaps = &htCap;
    if (htCaps->present)
    {
        pStaDs->mlmStaContext.htCapability = 1 ;
        pStaDs->htGreenfield = htCaps->greenField ;
        pStaDs->htSupportedChannelWidthSet =  htCaps->supportedChannelWidthSet ;
        pStaDs->htMIMOPSState =             htCaps->mimoPowerSave ;
        pStaDs->htMaxAmsduLength =  htCaps->maximalAMSDUsize;
        pStaDs->htAMpduDensity =    htCaps->mpduDensity;
        pStaDs->htDsssCckRate40MHzSupport = htCaps->dsssCckMode40MHz ;
        pStaDs->htShortGI20Mhz = htCaps->shortGI20MHz;
        pStaDs->htShortGI40Mhz = htCaps->shortGI40MHz;
        pStaDs->htMaxRxAMpduFactor = htCaps->maxRxAMPDUFactor;
        limFillRxHighestSupportedRate(pMac, 
                             &pStaDs->supportedRates.rxHighestDataRate, 
                                                 htCaps->supportedMCSSet);
        pStaDs->baPolicyFlag = 0xFF;
        pMac->lim.gLimTdlsLinkMode = TDLS_LINK_MODE_N ;
    }
    else
    {
        pStaDs->mlmStaContext.htCapability = 0 ;
        pMac->lim.gLimTdlsLinkMode = TDLS_LINK_MODE_BG ;
    }
#ifdef WLAN_FEATURE_11AC
    limTdlsPopulateDot11fVHTCaps(pMac, pTdlsAddStaReq, &vhtCap);
    pVhtCaps = &vhtCap;
    if (pVhtCaps->present)
    {
        pStaDs->mlmStaContext.vhtCapability = 1 ;

        if ((psessionEntry->currentOperChannel <= SIR_11B_CHANNEL_END) &&
            pMac->roam.configParam.enableVhtFor24GHz)
        {
            pStaDs->vhtSupportedChannelWidthSet = WNI_CFG_VHT_CHANNEL_WIDTH_20_40MHZ;
            pStaDs->htSupportedChannelWidthSet = eHT_CHANNEL_WIDTH_20MHZ;
        }
        else
        {
            pStaDs->vhtSupportedChannelWidthSet =  WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ;
            pStaDs->htSupportedChannelWidthSet = eHT_CHANNEL_WIDTH_40MHZ ;
        }

        pStaDs->vhtLdpcCapable = pVhtCaps->ldpcCodingCap;
        pStaDs->vhtBeamFormerCapable= pVhtCaps->suBeamFormerCap;
        // TODO , is it necessary , Sunil???
        pMac->lim.gLimTdlsLinkMode = TDLS_LINK_MODE_AC;
    }
    else
    {
        pStaDs->mlmStaContext.vhtCapability = 0 ;
        pStaDs->vhtSupportedChannelWidthSet = WNI_CFG_VHT_CHANNEL_WIDTH_20_40MHZ;
    }
#endif
    /*Calculate the Secondary Coannel Offset */
    cbMode = limTdlsSelectCBMode(pStaDs, psessionEntry);

    pStaDs->htSecondaryChannelOffset = cbMode;

#ifdef WLAN_FEATURE_11AC
    if ( pStaDs->mlmStaContext.vhtCapability )
    {
        pStaDs->htSecondaryChannelOffset = limGetHTCBState(cbMode);
    }
#endif
    
    pSessStaDs = dphLookupHashEntry(pMac, psessionEntry->bssId, &aid, 
                                          &psessionEntry->dph.dphHashTable) ;

    /* Lets enable QOS parameter */
    pStaDs->qosMode    = 1;
    pStaDs->wmeEnabled = 1;
    pStaDs->lleEnabled = 0;
    /*  TDLS Dummy AddSTA does not have qosInfo , is it OK ??
     */
    pStaDs->qos.capability.qosInfo = (*(tSirMacQosInfoStation *) &pTdlsAddStaReq->uapsd_queues);

    /* populate matching rate set */

    /* TDLS Dummy AddSTA does not have HTCap,VHTCap,Rates info , is it OK ??
     */
    limTdlsPopulateMatchingRateSet(pMac, pStaDs, pTdlsAddStaReq->supported_rates,
                                   pTdlsAddStaReq->supported_rates_length,
                                   (tANI_U8 *)pTdlsAddStaReq->htCap.suppMcsSet,
                                   &pStaDs->mlmStaContext.propRateSet,
                                   psessionEntry, pVhtCaps);

    /*  TDLS Dummy AddSTA does not have right capability , is it OK ??
     */
    pStaDs->mlmStaContext.capabilityInfo = ( *(tSirMacCapabilityInfo *) &pTdlsAddStaReq->capability);

    return ; 
}

#ifdef FEATURE_WLAN_TDLS_INTERNAL
/*
 * find Peer in setup link list.
 */
 
tANI_U8 limTdlsFindLinkPeer(tpAniSirGlobal pMac, tSirMacAddr peerMac, 
                                            tLimTdlsLinkSetupPeer  **setupPeer)
{
    tLimTdlsLinkSetupInfo *setupInfo = &pMac->lim.gLimTdlsLinkSetupInfo ;
    tLimTdlsLinkSetupPeer *linkSetupList = setupInfo->tdlsLinkSetupList ;
    tANI_U8 checkNode = TDLS_NODE_NOT_FOUND ; 

    while (linkSetupList != NULL)
    {
        if (vos_mem_compare((tANI_U8 *) peerMac,
                            (tANI_U8 *) linkSetupList->peerMac,
                            sizeof(tSirMacAddr)) )
        {
            checkNode = TDLS_NODE_FOUND ;
            *setupPeer = linkSetupList ;
            break ;
        }
        linkSetupList = linkSetupList->next;
    }

    return ((TDLS_NODE_FOUND ==  checkNode) ? eSIR_SUCCESS : eSIR_FAILURE ) ;
}

/*
 * find peer in Discovery list.
 * Dicovery list get populated in two instances, a) Recieved responses in reply
 * to discovery request b) If discover request is received from TDLS peer STA
 */
tSirTdlsPeerInfo *limTdlsFindDisPeer(tpAniSirGlobal pMac, tSirMacAddr peerMac)
{
    tLimDisResultList *discoveryList = pMac->lim.gLimTdlsDisResultList ;
    tSirTdlsPeerInfo *peerInfo = NULL ;

    while (discoveryList != NULL)
    {
        peerInfo = &discoveryList->tdlsDisPeerInfo ;
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
         ("Peer in discovery list = " MAC_ADDRESS_STR),
          MAC_ADDR_ARRAY(peerInfo->peerMac));

        if (vos_mem_compare((tANI_U8 *) peerMac,
                       (tANI_U8 *) &peerInfo->peerMac, sizeof(tSirMacAddr)) )
        {
            break ;
        }
        discoveryList = discoveryList->next;
    }

    return peerInfo ;
}

/*
 * find peer in Discovery list by looking into peer state.
 * Dicovery list get populated in two instances, a) Recieved responses in reply
 * to discovery request b) If discover request is received from TDLS peer STA
 */
static tSirTdlsPeerInfo *limTdlsFindDisPeerByState(tpAniSirGlobal pMac, 
                                                                tANI_U8 state)
{
    tLimDisResultList *discoveryList = pMac->lim.gLimTdlsDisResultList ;
    tSirTdlsPeerInfo *peerInfo = NULL ;

    while (discoveryList != NULL)
    {
        peerInfo = &discoveryList->tdlsDisPeerInfo ;
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
                     ("peerInfo Mac = " MAC_ADDRESS_STR),
                      MAC_ADDR_ARRAY(peerInfo->peerMac));

        if (peerInfo->tdlsPeerState == state)
        {
            break ;
        }
        discoveryList = discoveryList->next;
    }

    return peerInfo ;
}

/*
 * find peer in Setup list by looking into peer state.
 * setup list get populated in two instances, a) Recieved responses in reply
 * to setup request b) If discover request is received from TDLS peer STA
 */
static tANI_U8 limTdlsFindSetupPeerByState(tpAniSirGlobal pMac, tANI_U8 state, 
                                              tLimTdlsLinkSetupPeer **setupPeer)
{    

    tLimTdlsLinkSetupInfo *setupInfo = &pMac->lim.gLimTdlsLinkSetupInfo ;
    tLimTdlsLinkSetupPeer *linkSetupList = setupInfo->tdlsLinkSetupList ;
    tANI_U8 checkNode = TDLS_NODE_NOT_FOUND ; 

    while (linkSetupList != NULL)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
                 ("peer state = %02x"), (linkSetupList)->tdls_link_state) ;
        if((linkSetupList)->tdls_link_state == state) 
        {
            checkNode = TDLS_NODE_FOUND ;
            *setupPeer = linkSetupList ;
            break ;
        }
        linkSetupList = (linkSetupList)->next;
    }

    return ((TDLS_NODE_FOUND == checkNode) ? eSIR_SUCCESS: eSIR_FAILURE) ;
}


/*
 * delete Peer from Setup Link
 */
void limTdlsDelLinkPeer(tpAniSirGlobal pMac, tSirMacAddr peerMac)
{
    tLimTdlsLinkSetupInfo *setupInfo = &pMac->lim.gLimTdlsLinkSetupInfo ;
    tLimTdlsLinkSetupPeer **linkSetupList = &setupInfo->tdlsLinkSetupList ;
    tLimTdlsLinkSetupPeer *currentNode = NULL ;
    tLimTdlsLinkSetupPeer *prevNode = NULL ;

    for(currentNode = *linkSetupList ; currentNode != NULL ;
                    prevNode = currentNode, currentNode = currentNode->next)
    {
        if (vos_mem_compare( (tANI_U8 *) peerMac,
                        (tANI_U8 *) currentNode->peerMac, 
                                                 sizeof(tSirMacAddr)) )
        {
            VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
                    ("Del Node for Peer = " MAC_ADDRESS_STR),
                     MAC_ADDR_ARRAY(currentNode->peerMac));
            /* if it's first Node */
            if(NULL == prevNode)
            {
                *linkSetupList = currentNode->next ;
            }
            else
            {
                prevNode->next = currentNode->next ;
            }
            vos_mem_free(currentNode) ;
            return ;
        }
    }
        
    return ;
}
   


/*
 * TDLS discovery request frame received from TDLS peer STA..
 */
static tSirRetStatus limProcessTdlsDisReqFrame(tpAniSirGlobal pMac, 
                                    tANI_U8 *pBody, tANI_U32 frmLen )
{
    tDot11fTDLSDisReq tdlsDisReq = {{0}} ;
    tANI_U32 status = 0 ;
    tLimDisResultList *tdlsDisResult = NULL ; 
    tLimDisResultList **disResultList = &pMac->lim.gLimTdlsDisResultList ;
    tSirMacAddr peerMac = {0} ;
    tLimTdlsLinkSetupPeer *setupPeer = NULL ;
    tSirTdlsPeerInfo *peerInfo = NULL ;
    tpPESession psessionEntry = NULL ;
    tANI_U8 sessionId = 0 ;

    status = dot11fUnpackTDLSDisReq(pMac, pBody, frmLen, &tdlsDisReq) ;

    VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_WARN, 
            ("TDLS dis request dialog = %d"), tdlsDisReq.DialogToken.token);

    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE, FL("Failed to parse TDLS discovery Request "
                              "frame (0x%08x, %d bytes):"),status, frmLen);
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frmLen);)
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog(pMac, LOGW, FL("There were warnings while unpacking a TDLS "
                               "discovery Request frame (0x%08x, %d bytes):"),
                   status, frmLen);
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frmLen);)
    }

    /*
     * find session entry using BSSID in link identifier, not using MAC
     * header beacuse, there is cases in TDLS, there may be BSSID will not
     * be present in header
     */
    psessionEntry = peFindSessionByBssid(pMac, 
                         &tdlsDisReq.LinkIdentifier.bssid[0], &sessionId) ;
    if(NULL == psessionEntry)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR,
                 ("no Session entry for TDLS session (bssid "MAC_ADDR_ARRAY")"),
                  MAC_ADDR_ARRAY(tdlsDisReq.LinkIdentifier.bssid));

        //VOS_ASSERT(0) ;
        return eSIR_FAILURE;
    }
 
    /* varify BSSID */
    status = vos_mem_compare( &psessionEntry->bssId[0],
                    &tdlsDisReq.LinkIdentifier.bssid[0], sizeof(tSirMacAddr)) ;
    VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
            ("lim BSSID "MAC_ADDRESS_STR),
             MAC_ADDR_ARRAY( psessionEntry->bssId));

    VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
            ("Dis req from BSSID "MAC_ADDRESS_STR),
             MAC_ADDR_ARRAY(tdlsDisReq.LinkIdentifier.bssid));
    if(!status)
    {
        limLog(pMac, LOGE, FL("TDLS discovery request frame from other BSS -> something wrong. Check RXP filter")) ;

        return eSIR_FAILURE ; 
    }

    /*
     * check if this is echo of our transmitted discovery request
     * drop it here, TODO: better to drop this in TL.
     */    
    status = vos_mem_compare( psessionEntry->selfMacAddr,
                    &tdlsDisReq.LinkIdentifier.InitStaAddr[0],
                                                     sizeof(tSirMacAddr)) ;
    if(status)
    {
        limLog( pMac, LOGE, FL("Echo of our TDLS discovery request frame")) ;
        return eSIR_FAILURE ; 
    }

    /*
     * while processing Discovery request from Peer,
     * STA_MAC--> MAC of TDLS discovery initiator
     * STA_PEER_MAC--> MAC of TDLS discovery responder.
     */
    vos_mem_copy( peerMac,
                        &tdlsDisReq.LinkIdentifier.InitStaAddr[0], 
                                                     sizeof(tSirMacAddr)) ;
    /* TODO, do more validation */
    
    /* see if discovery is already in progress */
    peerInfo = limTdlsFindDisPeer(pMac, peerMac) ;

    if(NULL == peerInfo)
    {    
        /*
         * we are allocating peer info for individual peers found in TDLS
         * discovery, we need to keep adding TDLS peers till we have timed
         * out. We are freeing this memory at the time we are sending this
         * collected peer info to SME.
         */
        tdlsDisResult = vos_mem_malloc(sizeof(tLimDisResultList));
        if ( NULL == tdlsDisResult )
        {
            limLog(pMac, LOGP, FL("alloc fail for TDLS discovery "
                                  "reponse info")) ;
            return eSIR_FAILURE ;
        }

 
        peerInfo = &tdlsDisResult->tdlsDisPeerInfo ;
        peerInfo->tdlsPeerState = TDLS_DIS_REQ_PROCESS_STATE ;
        peerInfo->dialog = tdlsDisReq.DialogToken.token ;

        peerInfo->sessionId = psessionEntry->peSessionId;
        
        /* Populate peer info of tdls discovery result */
        vos_mem_copy( peerInfo->peerMac, peerMac, sizeof(tSirMacAddr)) ;

         /*
         * Now, as per D13, there will not be any Supp rates, ext Supp rates
         * info in Discovery request frames, so we are populating this info
         * locally to pass it to ADD STA.
         */
        do
        {
            tDot11fIESuppRates suppRates = {0} ;
            tDot11fIEExtSuppRates extSuppRates = {0} ;
            tANI_U16 caps = 0 ;
            tDot11fFfCapabilities capsInfo = {0} ;
            tDot11fIEHTCaps HTCaps = {0} ;
            /* populate supported rate IE */
            PopulateDot11fSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL, 
                                                  &suppRates, psessionEntry );
            ConvertSuppRates( pMac, &peerInfo->tdlsPeerSuppRates, 
                                                            &suppRates);
            /* Populate extended supported rates */
            PopulateDot11fExtSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL,
                                &extSuppRates, psessionEntry );

            peerInfo->ExtRatesPresent = 1;
            ConvertExtSuppRates( pMac, &peerInfo->tdlsPeerExtRates, 
                                                          &extSuppRates);
 
            if(cfgGetCapabilityInfo(pMac, &caps, psessionEntry) != eSIR_SUCCESS)
            {
                /*
                 * Could not get Capabilities value
                 * from CFG. Log error.
                 */
                 limLog(pMac, LOGP,
                   FL("could not retrieve Capabilities value"));
            }
            swapBitField16(caps, ( tANI_U16* )&capsInfo );
            /* update Caps Info */
            tdlsUpdateCapInfo(&peerInfo->capabilityInfo, &capsInfo) ;

            PopulateDot11fHTCaps( pMac, psessionEntry, &HTCaps );
            limTdlsCovertHTCaps(pMac, peerInfo, &HTCaps) ;

        } while (0) ;
    
        /* now add this new found discovery node into tdls discovery list */
        tdlsDisResult->next = *disResultList ;
        *disResultList = tdlsDisResult ;
        pMac->lim.gLimTdlsDisStaCount++ ; 

        /* See if for this peer already entry in setup Link */ 
        limTdlsFindLinkPeer(pMac, peerMac, &setupPeer) ;

        /* 
         * if there is no entry for this peer in setup list, we need to 
         * do add sta for this peer to transmit discovery rsp.
         */ 
        if(NULL == setupPeer)
        {
            /* To start with, send add STA request to HAL */
            pMac->lim.gLimAddStaTdls = true ;
            peerInfo->delStaNeeded = true ;

            if(eSIR_FAILURE == limTdlsDisAddSta(pMac, peerMac, 
                                                     peerInfo, psessionEntry))
            {
                VOS_ASSERT(0) ;
                limLog(pMac, LOGE, "Add STA for dis response is failed ") ;
                return eSIR_FAILURE ;
            }
        } /* use setup link sta ID for discovery rsp */
        else
        {
            peerInfo->delStaNeeded = false ;
            limSendTdlsDisRspFrame(pMac, peerInfo->peerMac, peerInfo->dialog,
                                   psessionEntry, NULL, 0);
            peerInfo->tdlsPeerState = TDLS_DIS_RSP_SENT_WAIT_STATE ;
        }

    }
    else
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
                    ("discovery procedure in progress for this peer")) ;
    } 

    return eSIR_SUCCESS ;
}

/* Process TDLS setup Request Frame */

static tSirRetStatus limProcessTdlsSetupReqFrame(tpAniSirGlobal pMac, 
                                         tANI_U8 *pBody, tANI_U32 frmLen)
{

    tDot11fTDLSSetupReq tdlsSetupReq = {{0}} ;
    tANI_U32 status = 0 ;
    tpPESession psessionEntry = NULL ;
    tANI_U8 sessionId = 0 ;
    tANI_U8 currentState = TDLS_LINK_SETUP_WAIT_STATE ;
    tANI_U8 previousState = TDLS_LINK_IDLE_STATE ;
    /* create node for Link setup */
    tLimTdlsLinkSetupInfo *linkSetupInfo = &pMac->lim.gLimTdlsLinkSetupInfo ;
    tLimTdlsLinkSetupPeer *setupPeer = NULL ;
    tLimTdlsLinkSetupPeer *tmpSetupPeer = NULL ;

    status = dot11fUnpackTDLSSetupReq(pMac, pBody, frmLen, &tdlsSetupReq) ;

    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE, FL("Failed to parse TDLS discovery Request "
                              "frame (0x%08x, %d bytes):"),status, frmLen);
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frmLen);)
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while unpacking a TDLS "
                      "setup Request frame (0x%08x, %d bytes):"),
                   status, frmLen );
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frmLen);)
    }
    /*
     * find session entry using BSSID in link identifier, not using MAC
     * header beacuse, there is cases in TDLS, there may be BSSID will not
     * be present in header
     */
    psessionEntry = peFindSessionByBssid(pMac, 
                         &tdlsSetupReq.LinkIdentifier.bssid[0], &sessionId) ;
    if(NULL == psessionEntry)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR,
                 ("no Session entry for TDLS session (bssid "
                  MAC_ADDRESS_STR")"),
                  MAC_ADDR_ARRAY(tdlsSetupReq.LinkIdentifier.bssid));

        //VOS_ASSERT(0) ;
        return eSIR_FAILURE ;
    }
    /* TODO: we don;t need this check now, varify BSSID */
    status = vos_mem_compare( psessionEntry->bssId,
                    &tdlsSetupReq.LinkIdentifier.bssid[0], 
                                                     sizeof(tSirMacAddr)) ;
     
    if(!status)
    {
        limLog( pMac, LOGE, FL("TDLS setup request frame from other BSS -> something wrong. Check RXP filter")) ;

        limSendTdlsSetupRspFrame(pMac, tdlsSetupReq.LinkIdentifier.InitStaAddr,
                                 tdlsSetupReq.DialogToken.token, psessionEntry,
                                 TDLS_SETUP_STATUS_FAILURE, NULL, 0 ) ;
        return eSIR_FAILURE ; 
    }

#ifdef FEATURE_WLAN_TDLS_NEGATIVE
    if(pMac->lim.gLimTdlsNegativeBehavior & LIM_TDLS_NEGATIVE_RSP_TIMEOUT_TO_SETUP_REQ) 
    {
        /* simply ignore this setup request packet */
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
        ("TDLS negative running: ignore TDLS Setup Req packet"));
        return eSIR_SUCCESS ;
    }
    if(pMac->lim.gLimTdlsNegativeBehavior & LIM_TDLS_NEGATIVE_SEND_REQ_TO_SETUP_REQ)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
        ("TDLS negative running: send TDLS Setup Req to peer TDLS Setup Req"));
        /* format TDLS discovery request frame and transmit it */
        limSendTdlsLinkSetupReqFrame(pMac, tdlsSetupReq.LinkIdentifier.InitStaAddr, tdlsSetupReq.DialogToken.token, psessionEntry,
            NULL, 0) ;
    }    
#endif
    /* TODO, do more validation */
    
    if(!limTdlsFindLinkPeer(pMac, 
                  &tdlsSetupReq.LinkIdentifier.InitStaAddr[0],
                                                  &tmpSetupPeer))
    {
        tANI_U32 tdlsStateStatus = TDLS_LINK_SETUP_START_STATE ;

        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
                        ("Link is already setup with this peer" )) ;
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
                        ("state = %d"), tmpSetupPeer->tdls_link_state) ;
        //return eSIR_FAILURE ; 

        if(tmpSetupPeer == NULL)
        {
            VOS_ASSERT(0) ;
            return eSIR_FAILURE ; 
            
        }
        switch(tmpSetupPeer->tdls_link_state)
        {

            case TDLS_LINK_SETUP_START_STATE:
            {
                v_SINT_t macCompare = 0 ;
                macCompare= vos_mem_compare2(tmpSetupPeer->peerMac, 
                           psessionEntry->selfMacAddr, sizeof(tSirMacAddr)) ;
                VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
                        ("MAC comparison Rslt = %d"), macCompare ) ;
                if(0 > macCompare)
                {
                    /* 
                     * Delete our Setup Request/Peer info and honour Peer 
                     * Setup Request, go ahead and respond for this 
                     */
                    /* Deactivate the timer */
                    tx_timer_deactivate(&tmpSetupPeer->gLimTdlsLinkSetupRspTimeoutTimer) ;
#ifdef FEATURE_WLAN_TDLS_NEGATIVE
                    if((pMac->lim.gLimTdlsNegativeBehavior & LIM_TDLS_NEGATIVE_SEND_REQ_TO_SETUP_REQ) 
                        != LIM_TDLS_NEGATIVE_SEND_REQ_TO_SETUP_REQ)
#endif
                    limSendSmeTdlsLinkStartRsp(pMac, eSIR_FAILURE, 
                            tmpSetupPeer->peerMac, eWNI_SME_TDLS_LINK_START_RSP);

                    limTdlsDelLinkPeer(pMac, tmpSetupPeer->peerMac) ;
                    tdlsStateStatus = TDLS_LINK_IDLE_STATE ;
                }
                else if(0 < macCompare)
                {
                    /* 
                     * Go ahead with current setup as peer is going to 
                     * respond for setup request 
                     */
                    tdlsStateStatus = TDLS_LINK_SETUP_START_STATE ;
                }
                else
                {
                    /* same MAC, not possible */
                    VOS_ASSERT(0) ;
                }
            
                break ;
            }
#if 1
            case TDLS_LINK_SETUP_DONE_STATE:
            {
                tpDphHashNode pStaDs = NULL ;

                previousState = TDLS_LINK_SETUP_WAIT_STATE ;
                currentState = TDLS_LINK_TEARDOWN_START_STATE ;
                VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
                        ("link Setup Done state "  )) ;
                tmpSetupPeer->tdls_prev_link_state =  previousState ;
                tmpSetupPeer->tdls_link_state = currentState ;
                setupPeer = tmpSetupPeer ;
#if 0                
                /* Send Teardown to this Peer and Initiate new TDLS Setup */
                limSendTdlsTeardownFrame(pMac, 
                      &tdlsSetupReq.LinkIdentifier.InitStaAddr[0], 
                        eSIR_MAC_TDLS_TEARDOWN_UNSPEC_REASON, psessionEntry) ;
#else
                
                /* tdls_hklee: send message to HAL before it is deleted, cause  */
                limTdlsLinkTeardown(pMac, (setupPeer)->peerMac) ;

                /* send del STA to remove context for this TDLS STA */
                pStaDs = limTdlsDelSta(pMac, (setupPeer)->peerMac, psessionEntry) ;

                /* now send indication to SME-->HDD->TL to remove STA from TL */

                if(pStaDs)
                {
                    limSendSmeTdlsDelPeerInd(pMac, psessionEntry->smeSessionId,
                                                           pStaDs, eSIR_SUCCESS) ;

                    /* send Teardown Ind to SME */
                    limSendSmeTdlsTeardownRsp(pMac, eSIR_SUCCESS, (setupPeer)->peerMac,
                                                  eWNI_SME_TDLS_TEARDOWN_IND) ;
                    /* remove node from setup list */
                    limTdlsDelLinkPeer(pMac, (setupPeer)->peerMac) ;
                }
#endif
                //setupPeer->tdls_prev_link_state = TDLS_LINK_SETUP_RESTART_STATE;
                tdlsStateStatus = TDLS_LINK_IDLE_STATE ;
                break ;

            }
            default:
            {
                VOS_ASSERT(0) ;
                VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
                        ("link Setup is Recieved in unknown state" )) ;
                break ;
            }
#endif
        }
        if(tdlsStateStatus == TDLS_LINK_SETUP_START_STATE) 
            return eSIR_FAILURE ;
    }

    if(currentState != TDLS_LINK_TEARDOWN_START_STATE)
    {  
        /* 
         * Now we are sure to send discovery response frame to TDLS discovery 
         * initiator, we don't care, if this request is unicast ro broadcast,
         * we simply, send discovery response frame on direct link.
         */
        setupPeer = vos_mem_malloc(sizeof( tLimTdlsLinkSetupPeer ));
        if ( NULL == setupPeer )
        {
            VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
                                 ( "Unable to allocate memory during ADD_STA" ));
            return eSIR_MEM_ALLOC_FAILED;
        }

        setupPeer->dialog = tdlsSetupReq.DialogToken.token ;
        //setupPeer->tdls_prev_link_state =  setupPeer->tdls_link_state ;
        //setupPeer->tdls_link_state = TDLS_LINK_SETUP_WAIT_STATE ;
        setupPeer->tdls_prev_link_state =  previousState ;
        setupPeer->tdls_link_state = currentState ;
        /* TDLS_sessionize: remember sessionId for future */
        setupPeer->tdls_sessionId = psessionEntry->peSessionId;
        setupPeer->tdls_bIsResponder = 0;

        vos_mem_copy(setupPeer->peerMac,
                     &tdlsSetupReq.LinkIdentifier.InitStaAddr[0], 
                                                     sizeof(tSirMacAddr)) ;

        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
                   ("Setup REQ MAC = " MAC_ADDRESS_STR),
                    MAC_ADDR_ARRAY(setupPeer->peerMac));
 
        limTdlsUpdateLinkReqPeerInfo(pMac, setupPeer, &tdlsSetupReq) ;
        pMac->lim.gLimAddStaTdls = true ;

        /* To start with, send add STA request to HAL */
        if(eSIR_FAILURE == limTdlsSetupAddSta(pMac, setupPeer->peerMac, 
                                                  setupPeer, psessionEntry))
        {
            VOS_ASSERT(0) ;
            vos_mem_free((void **) &setupPeer) ;
            return eSIR_FAILURE ;
        }

        limSendTdlsSetupRspFrame(pMac, tdlsSetupReq.LinkIdentifier.InitStaAddr,
                                  tdlsSetupReq.DialogToken.token, psessionEntry,
                                  TDLS_SETUP_STATUS_SUCCESS, NULL, 0) ;

        limStartTdlsTimer(pMac, psessionEntry->peSessionId, 
                                  &setupPeer->gLimTdlsLinkSetupCnfTimeoutTimer,
                               (tANI_U32)setupPeer->peerMac,
                                 WNI_CFG_TDLS_LINK_SETUP_CNF_TIMEOUT,
                                   SIR_LIM_TDLS_LINK_SETUP_CNF_TIMEOUT) ;

        /* update setup peer list */
        setupPeer->next = linkSetupInfo->tdlsLinkSetupList ;
        linkSetupInfo->tdlsLinkSetupList = setupPeer ;
    }
    else
    {
        setupPeer->dialog = tdlsSetupReq.DialogToken.token ;
        //setupPeer->tdls_prev_link_state =  setupPeer->tdls_link_state ;
        //setupPeer->tdls_link_state = TDLS_LINK_SETUP_WAIT_STATE ;
        setupPeer->tdls_prev_link_state =  previousState ;
        setupPeer->tdls_link_state = currentState ;
        /* TDLS_sessionize: remember sessionId for future */
        setupPeer->tdls_sessionId = psessionEntry->peSessionId;
        setupPeer->tdls_bIsResponder = 0;

        vos_mem_copy( setupPeer->peerMac,
                     &tdlsSetupReq.LinkIdentifier.InitStaAddr[0], 
                                                     sizeof(tSirMacAddr)) ;

        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
                   ("Setup REQ MAC = "MAC_ADDRESS_STR),
                    MAC_ADDR_ARRAY(setupPeer->peerMac));
 
        limTdlsUpdateLinkReqPeerInfo(pMac, setupPeer, &tdlsSetupReq) ;
        limSendTdlsSetupRspFrame(pMac, tdlsSetupReq.LinkIdentifier.InitStaAddr, 
                                 tdlsSetupReq.DialogToken.token, psessionEntry,
                                 TDLS_SETUP_STATUS_SUCCESS, NULL, 0) ;

        limStartTdlsTimer(pMac, psessionEntry->peSessionId, 
                                  &setupPeer->gLimTdlsLinkSetupCnfTimeoutTimer,
                               (tANI_U32)setupPeer->peerMac,
                                 WNI_CFG_TDLS_LINK_SETUP_CNF_TIMEOUT,
                                   SIR_LIM_TDLS_LINK_SETUP_CNF_TIMEOUT) ;
    }
 
   
    return eSIR_SUCCESS ;

}

/*
 * TDLS discovery request frame received from TDLS peer STA..
 */
static tSirRetStatus limProcessTdlsSetupRspFrame(tpAniSirGlobal pMac, 
                                            tANI_U8 *pBody, tANI_U32 frmLen )
{
    tDot11fTDLSSetupRsp tdlsSetupRsp = {{0}} ;
    tANI_U32 status = 0 ;
    tSirMacAddr peerMac = {0} ;
    tLimTdlsLinkSetupPeer *setupPeer = NULL ;
    tpPESession psessionEntry = NULL ;
    tANI_U8 sessionId = 0 ;

    status = dot11fUnpackTDLSSetupRsp(pMac, pBody, frmLen, &tdlsSetupRsp) ;

    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE, FL("Failed to parse TDLS discovery Request "
                              "frame (0x%08x, %d bytes):"),status, frmLen);
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frmLen);)
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while unpacking a TDLS "
                               "discovery Request frame (0x%08x, %d bytes):"),
                   status, frmLen );
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frmLen);)
    }

    /*
     * find session entry using BSSID in link identifier, not using MAC
     * header beacuse, there is cases in TDLS, there may be BSSID will not
     * be present in header
     */
    psessionEntry = peFindSessionByBssid(pMac, 
                         &tdlsSetupRsp.LinkIdentifier.bssid[0], &sessionId) ;
    if(NULL == psessionEntry)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR,
                  ("no Session entry for TDLS session (bssid "
                  MAC_ADDRESS_STR")"),
                  MAC_ADDR_ARRAY(tdlsSetupRsp.LinkIdentifier.bssid));

        //VOS_ASSERT(0) ;
        return eSIR_FAILURE;
    }
  
    /* varify BSSID */
    status = vos_mem_compare( psessionEntry->bssId,
                    &tdlsSetupRsp.LinkIdentifier.bssid[0], 
                                                  sizeof(tSirMacAddr)) ;
     
    if(!status)
    {
        limLog( pMac, LOGE, FL("TDLS discovery request frame from other BSS -> something wrong. Check RXP filter")) ;

        VOS_ASSERT(0) ;
        return eSIR_FAILURE ; 
    }
    vos_mem_copy( peerMac,
                      &tdlsSetupRsp.LinkIdentifier.RespStaAddr[0], 
                                                     sizeof(tSirMacAddr)) ;

    VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
             ("TDLS setup RSP peer = "MAC_ADDRESS_STR), MAC_ADDR_ARRAY(peerMac));
    limTdlsFindLinkPeer(pMac, peerMac, &setupPeer) ;

    if(NULL == setupPeer)
    {
        limLog( pMac, LOGE, FL("unknown setup Response frame other BSS")) ;
        return eSIR_FAILURE ;
    }
                                                
    VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
                                      ("deactivating Setup RSP timer")) ;

    /* Deactivate the timer */
    tx_timer_deactivate(&(setupPeer)->gLimTdlsLinkSetupRspTimeoutTimer) ;

    /*
     * TDLS Setup RSP is recieved with Failure, Delete this STA entry
     * don't respond with TDLS CNF frame.
     */
    if(TDLS_SETUP_STATUS_SUCCESS != tdlsSetupRsp.Status.status)
    {
        limTdlsDelLinkPeer(pMac, (setupPeer)->peerMac) ;
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
                                    ("setup RSP with Failure Code")) ;
        return eSIR_FAILURE ;
    }    
    
    /* update Link Info */
    limTdlsUpdateLinkRspPeerInfo(pMac, setupPeer, &tdlsSetupRsp) ;
 
    /* TODO, do more validation */
    

    /* 
     * Now we are sure to send link setup CNF  frame to TDLS link setup 
     * reponded, now we will create dph hash entry and send add STA to HAL
     */

    pMac->lim.gLimAddStaTdls = true ;
    if(eSIR_FAILURE == limTdlsSetupAddSta(pMac, peerMac,  
                                                 setupPeer, psessionEntry))
    {
       /* through error */
       VOS_ASSERT(0) ;
       return eSIR_FAILURE ;
    } 
    /* TDLS_HKLEE_FIXME: now we add some delay for AddSta_Rsp comes */
    
         
    /* send TDLS confim frame to TDLS Peer STA */           
    limSendTdlsLinkSetupCnfFrame(pMac, peerMac, tdlsSetupRsp.DialogToken.token, 0, psessionEntry, NULL, 0) ;

    /* 
     * set the tdls_link_state to TDLS_LINK_SETUP_RSP_WAIT_STATE, and
     * wait for Setup CNF transmission on air, once we receive tx complete
     * message, we will change the peer state and send message to SME 
     * callback..
     */
    (setupPeer)->tdls_prev_link_state = (setupPeer)->tdls_link_state ;
    (setupPeer)->tdls_link_state = TDLS_LINK_SETUP_RSP_WAIT_STATE ;

    return eSIR_SUCCESS ;
}
/*
 * TDLS setup CNF  frame processing ..
 */

static tSirRetStatus limProcessTdlsSetupCnfFrame(tpAniSirGlobal pMac, 
                                            tANI_U8 *pBody, tANI_U32 frmLen)
{
    tDot11fTDLSSetupCnf tdlsSetupCnf = {{0}} ;
    tANI_U32 status = 0 ;
    tLimTdlsLinkSetupPeer *setupPeer = NULL ;
    tpPESession psessionEntry = NULL ;
    tANI_U8 sessionId = 0 ;

    status = dot11fUnpackTDLSSetupCnf(pMac, pBody, frmLen, &tdlsSetupCnf) ;

    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE, FL("Failed to parse an TDLS discovery Response "
                              "frame (0x%08x, %d bytes):"),status, frmLen);
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frmLen);)
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while unpacking a TDLS "
                               "discovery Response frame (0x%08x, %d bytes):"),
                   status, frmLen );
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frmLen);)
    }
    /*
     * find session entry using BSSID in link identifier, not using MAC
     * header beacuse, there is cases in TDLS, there may be BSSID will not
     * be present in header
     */
    psessionEntry = peFindSessionByBssid(pMac, 
                         &tdlsSetupCnf.LinkIdentifier.bssid[0], &sessionId) ;
    if(NULL == psessionEntry)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR,
                  ("no Session entry for TDLS session (bssid "
                  MAC_ADDRESS_STR")"),
                  MAC_ADDR_ARRAY(tdlsSetupCnf.LinkIdentifier.bssid));

        //VOS_ASSERT(0) ;
        return eSIR_FAILURE;
    }
 
    /* varify BSSID */
    status = vos_mem_compare( psessionEntry->bssId,
                    &tdlsSetupCnf.LinkIdentifier.bssid[0], 
                                                     sizeof(tSirMacAddr)) ;

    if(!status)
    {
        limLog( pMac, LOGE, FL("TDLS setup CNF frame other BSS -> something wrong. Check RXP filter")) ;

        VOS_ASSERT(0) ;
        return eSIR_FAILURE ; 
    }
    /* TODO, do more validation */
    VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
               ("setup Cnf peer MAc = "MAC_ADDRESS_STR),
                MAC_ADDR_ARRAY(tdlsSetupCnf.LinkIdentifier.InitStaAddr));
    
    limTdlsFindLinkPeer(pMac, 
                   &tdlsSetupCnf.LinkIdentifier.InitStaAddr[0],
                            &setupPeer) ;

    if(NULL == setupPeer)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
                                          (" unknown setup CNF frame")) ;
        VOS_ASSERT(0) ;
        return eSIR_FAILURE ;
    }
    VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
                   ("setup CNF peer MAC = "MAC_ADDRESS_STR),
                    MAC_ADDR_ARRAY((setupPeer)->peerMac));
    /*T match dialog token, before proceeding further */
    if((setupPeer)->dialog != tdlsSetupCnf.DialogToken.token)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
                          ("setup CNF frame not matching with setup RSP")) ;
        VOS_ASSERT(0) ;
        return eSIR_FAILURE ;
    }

    /* 
     * Now we are sure that, this set CNF is for us, now stop 
     * the running timer..
     */
    tx_timer_deactivate(&(setupPeer)->gLimTdlsLinkSetupCnfTimeoutTimer) ;

    /* change TDLS peer State */
    (setupPeer)->tdls_prev_link_state = (setupPeer)->tdls_link_state ;
    (setupPeer)->tdls_link_state = TDLS_LINK_SETUP_DONE_STATE ; 

    /* send indication to SME that, new link is setup */
    limSendSmeTdlsLinkSetupInd(pMac, (setupPeer)->peerMac, eSIR_SUCCESS) ;

    /* tdls_hklee: prepare PTI template and send it to HAL */
    limTdlsLinkEstablish(pMac, (setupPeer)->peerMac);

    return eSIR_SUCCESS ; 

}

/*
 * TDLS discovery response frame processing ..
 */

static tSirRetStatus limProcessTdlsDisRspFrame(tpAniSirGlobal pMac, 
                              tANI_U8 *pBody, tANI_U32 frmLen, 
                                 tANI_S8 rssi, tpPESession psessionEntry)
{
    tDot11fTDLSDisRsp tdlsDisRsp = {{0}} ;
    tANI_U32 status = 0 ;
    tLimDisResultList *tdlsDisResult = NULL ; 
    tLimDisResultList **disResultList = &pMac->lim.gLimTdlsDisResultList ;
    tSirTdlsDisReq *prevDisReq = &pMac->lim.gLimTdlsDisReq ;

    status = dot11fUnpackTDLSDisRsp(pMac, pBody, frmLen, &tdlsDisRsp) ;

    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE, FL("Failed to parse an TDLS discovery Response "
                              "frame (0x%08x, %d bytes):"),status, frmLen);
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frmLen);)
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while unpacking a TDLS "
                               "discovery Response frame (0x%08x, %d bytes):"),
                   status, frmLen );
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pFrame, nFrame);)
    }
    /*TODO:  match dialog token, before proceeding further */

    /* varify BSSID */
    status = vos_mem_compare( psessionEntry->bssId,
                    &tdlsDisRsp.LinkIdentifier.bssid[0], 
                                                     sizeof(tSirMacAddr)) ;

    if(!status)
    {
        limLog( pMac, LOGW, FL(" TDLS discovery Response frame other BSS")) ;
        return eSIR_FAILURE ; 
    }
    /* TODO, do more validation */
  
    if(tdlsDisRsp.DialogToken.token != prevDisReq->dialog)
    {
        limLog( pMac, LOGW, FL(" wrong TDLS discovery Response frame")) ;
        return eSIR_FAILURE ;
    } 

    pMac->lim.gLimTdlsDisStaCount++ ;

    /*
     * we are allocating peer info for individual peers found in TDLS
     * discovery, we need to keep adding TDLS peers till we have timed
     * out. We are freeing this memory at the time we are sending this
     * collected peer info to SME.
     */
    tdlsDisResult = vos_mem_malloc(sizeof(tLimDisResultList));
    if ( NULL == tdlsDisResult )
    {
        limLog(pMac, LOGP, FL("alloc fail for TDLS discovery reponse info")) ;
        return eSIR_FAILURE ;
    }

    do
    {
        tSirTdlsPeerInfo *peerInfo = &tdlsDisResult->tdlsDisPeerInfo ;

        /* Populate peer info of tdls discovery result */
        peerInfo->sessionId = psessionEntry->peSessionId;
        /*
         * When we receive DIS RSP from peer MAC,
         * STA_MAC_OFFSET will carry peer MAC address and PEER MAC OFFSET
         * will carry our MAC.
         */
        vos_mem_copy( peerInfo->peerMac,
                    &tdlsDisRsp.LinkIdentifier.RespStaAddr[0], 
                                                     sizeof(tSirMacAddr)) ;

        /* update RSSI for this TDLS peer STA */
        peerInfo->tdlsPeerRssi = rssi ;

        /* update Caps Info */
        tdlsUpdateCapInfo(&peerInfo->capabilityInfo,
                                          &tdlsDisRsp.Capabilities) ;

        /* update Supp rates */
        if(tdlsDisRsp.SuppRates.present)
        { 
            ConvertSuppRates( pMac, &peerInfo->tdlsPeerSuppRates, 
                                             &tdlsDisRsp.SuppRates );
        }

        /* update EXT supp rates */
        if(tdlsDisRsp.ExtSuppRates.present) 
        {
            peerInfo->ExtRatesPresent = 1;
            ConvertExtSuppRates( pMac, &peerInfo->tdlsPeerExtRates, 
                                                    &tdlsDisRsp.ExtSuppRates );
        }
        /* update HT caps */
        if (tdlsDisRsp.HTCaps.present)
        {
            vos_mem_copy( &peerInfo->tdlsPeerHtCaps, &tdlsDisRsp.HTCaps,
                                               sizeof( tDot11fIEHTCaps ) );
        }
    } while(0) ;

    /* now add this new found discovery node into tdls discovery list */
    tdlsDisResult->next = *disResultList ;
    *disResultList = tdlsDisResult ; 

    return eSIR_SUCCESS ; 
}

/* 
 * Process TDLS Teardown request frame from TDLS peer STA
 */
static tSirRetStatus limProcessTdlsTeardownFrame(tpAniSirGlobal pMac, 
                                      tANI_U8 *pBody, tANI_U32 frmLen )
{
    tDot11fTDLSTeardown tdlsTeardown = {{0}} ;
    tANI_U32 status = 0 ;
    tLimTdlsLinkSetupPeer *setupPeer = NULL ;
    tpPESession psessionEntry = NULL ;
    tANI_U8 sessionId = 0 ;

    status = dot11fUnpackTDLSTeardown(pMac, pBody, frmLen, &tdlsTeardown) ;

    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE, FL("Failed to parse an TDLS discovery Response "
                              "frame (0x%08x, %d bytes):"),status, frmLen);
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frmLen);)
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while unpacking a TDLS "
                               "discovery Response frame (0x%08x, %d bytes):"),
                   status, frmLen );
        PELOG2(sirDumpBuf(pMac, SIR_DBG_MODULE_ID, LOG2, pBody, frmLen);)
    }

    /*
     * find session entry using BSSID in link identifier, not using MAC
     * header beacuse, there is cases in TDLS, there may be BSSID will not
     * be present in header
     */
    psessionEntry = peFindSessionByBssid(pMac, 
                         &tdlsTeardown.LinkIdentifier.bssid[0], &sessionId) ;
    if(NULL == psessionEntry)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR,
                  ("no Session entry for TDLS session (bssid "
                  MAC_ADDRESS_STR")"),
                  MAC_ADDR_ARRAY(tdlsTeardown.LinkIdentifier.bssid));

        //VOS_ASSERT(0) ;
        return eSIR_FAILURE;
    }
 
    /* varify BSSID */
    status = vos_mem_compare( psessionEntry->bssId,
                                  &tdlsTeardown.LinkIdentifier.bssid[0], 
                                                     sizeof(tSirMacAddr)) ;


    if(!status)
    {
        limLog( pMac, LOGE, FL("Teardown from other BSS -> something wrong. Check RXP filter")) ;
        VOS_ASSERT(0) ;
        return eSIR_FAILURE ; 
    }
    
    limTdlsFindLinkPeer(pMac, 
                     &tdlsTeardown.LinkIdentifier.InitStaAddr[0],
                                            &setupPeer) ;

    if(NULL == setupPeer)
    {
        //ignore
        //VOS_ASSERT(0) ;
        limLog( pMac, LOGE, FL("Teardown from unknown peer. --> ignored") );
        
        return eSIR_FAILURE ;
    }
    VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
                     ("teardown for peer "MAC_ADDRESS_STR),
                          MAC_ADDR_ARRAY((setupPeer)->peerMac));

    switch(tdlsTeardown.Reason.code)
    {
        case eSIR_MAC_TDLS_TEARDOWN_UNSPEC_REASON:
        {
            VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
                                 ("teardown with unspecified reason")) ;
            break ;
        }
        case eSIR_MAC_TDLS_TEARDOWN_PEER_UNREACHABLE:
        {
            VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
                       (" Teardown from AP, TDLS peer unreachable")) ;
            break ;
        }
        default:
        {
            VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
                                            (" unknown teardown")) ;
            break ;
        }
    }

    /* change TDLS peer State */
    (setupPeer)->tdls_prev_link_state = (setupPeer)->tdls_link_state ;
    (setupPeer)->tdls_link_state = TDLS_LINK_TEARDOWN_START_STATE ; 

    do
    {
        tpDphHashNode pStaDs = NULL ;

        /* tdls_hklee: send message to HAL before it is deleted, cause  */
        limTdlsLinkTeardown(pMac, (setupPeer)->peerMac) ;

        /* send del STA to remove context for this TDLS STA */
        pStaDs = limTdlsDelSta(pMac, (setupPeer)->peerMac, psessionEntry) ;

        /* now send indication to SME-->HDD->TL to remove STA from TL */

        if(pStaDs)
        {
            limSendSmeTdlsDelPeerInd(pMac, psessionEntry->smeSessionId,
                                                   pStaDs, eSIR_SUCCESS) ;
      
            /* send Teardown Ind to SME */
            limSendSmeTdlsTeardownRsp(pMac, eSIR_SUCCESS, (setupPeer)->peerMac,
                                          eWNI_SME_TDLS_TEARDOWN_IND) ;
            /* remove node from setup list */
            limTdlsDelLinkPeer(pMac, (setupPeer)->peerMac) ;
        }

    }while(0) ;
    
    return status ;
}

/*
 * Common processing of TDLS action frames recieved 
 */
void limProcessTdlsFrame(tpAniSirGlobal pMac, tANI_U32 *pBd)
{
    tANI_U8 *pBody = WDA_GET_RX_MPDU_DATA(pBd);
    tANI_U8 pOffset = ((0 == WDA_GET_RX_FT_DONE(pBd)) 
                         ? (( sizeof( eth_890d_header ))) :(0)) ;

    tANI_U8 category   = (pBody + pOffset + PAYLOAD_TYPE_TDLS_SIZE)[0] ; 
    tANI_U8 action     =   (pBody + pOffset + PAYLOAD_TYPE_TDLS_SIZE)[1] ; 
    tANI_U32 frameLen  = WDA_GET_RX_PAYLOAD_LEN(pBd) ;
    tANI_U8 *tdlsFrameBody = (pBody + pOffset + PAYLOAD_TYPE_TDLS_SIZE) ;
    //tANI_S8 rssi = (tANI_S8)SIR_MAC_BD_TO_RSSI_DB(pBd);

    if(category != SIR_MAC_ACTION_TDLS)
    {
        limLog( pMac, LOGE, FL("Invalid TDLS action frame=(%d). Ignored"), category );
        return ; 
    }

    frameLen -= (pOffset + PAYLOAD_TYPE_TDLS_SIZE) ;
    LIM_LOG_TDLS(VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, ("Received TDLS action %d (%s)"),
        action, limTraceTdlsActionString(action) ));

    switch(action)
    {

        case SIR_MAC_TDLS_SETUP_REQ:
        {
            limProcessTdlsSetupReqFrame(pMac, tdlsFrameBody, frameLen) ;
            break ;
        }
        case SIR_MAC_TDLS_SETUP_RSP:
        {
            limProcessTdlsSetupRspFrame(pMac, tdlsFrameBody, frameLen) ;
            break ;
        }
        case SIR_MAC_TDLS_SETUP_CNF:
        {
            limProcessTdlsSetupCnfFrame(pMac, tdlsFrameBody, frameLen) ; 
            break ;
        }
        case SIR_MAC_TDLS_TEARDOWN: 
        {
            limProcessTdlsTeardownFrame(pMac, tdlsFrameBody, frameLen) ; 
            break ;
        }
        case SIR_MAC_TDLS_DIS_REQ:
        {
            limProcessTdlsDisReqFrame(pMac, tdlsFrameBody, frameLen) ;
            break ;
        }
        case SIR_MAC_TDLS_PEER_TRAFFIC_IND:
        case SIR_MAC_TDLS_CH_SWITCH_REQ:      
        case SIR_MAC_TDLS_CH_SWITCH_RSP:    
        case SIR_MAC_TDLS_PEER_TRAFFIC_RSP:
        default:
        {
            break ;
        }
    }
    
    return ;    
}

/*
 * ADD sta for dis response fame sent on direct link
 */
static tSirRetStatus limTdlsDisAddSta(tpAniSirGlobal pMac, tSirMacAddr peerMac, 
                          tSirTdlsPeerInfo *peerInfo, tpPESession psessionEntry)
{
    tpDphHashNode pStaDs = NULL ;
    tSirRetStatus status = eSIR_SUCCESS ;
    tANI_U16 aid = 0 ;

    if(NULL == peerInfo)
    {
        VOS_ASSERT(0) ;
        return status ;

    } 
    VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
               ("ADD STA peer MAC: "MAC_ADDRESS_STR),
                MAC_ADDR_ARRAY(peerMac));


    if(NULL != dphLookupHashEntry(pMac, peerMac, 
                                  &aid, &psessionEntry->dph.dphHashTable))
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
                    (" there is hash entry for this client")) ;
        status = eSIR_FAILURE ;
        VOS_ASSERT(0) ;
        return status ;
    }

    aid = limAssignPeerIdx(pMac, psessionEntry) ;

    /* Set the aid in peerAIDBitmap as it has been assigned to TDLS peer */
    SET_PEER_AID_BITMAP(psessionEntry->peerAIDBitmap, aid);

    pStaDs = dphGetHashEntry(pMac, aid, &psessionEntry->dph.dphHashTable);

    if (pStaDs)
    {
        (void) limDelSta(pMac, pStaDs, false /*asynchronous*/, psessionEntry);
        limDeleteDphHashEntry(pMac, pStaDs->staAddr, aid, psessionEntry);
    }
    pStaDs = dphAddHashEntry(pMac, peerMac, aid, 
                                         &psessionEntry->dph.dphHashTable) ;

    if(NULL == pStaDs)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
                    (" add hash entry failed")) ;
        status = eSIR_FAILURE ;
        VOS_ASSERT(0) ;
        return status;
    }
    if(eSIR_SUCCESS == status)
    {
#ifdef TDLS_RATE_DEBUG
        tSirMacRateSet *suppRates = &peerInfo->tdlsPeerSuppRates ;
        tSirMacRateSet *extRates = &peerInfo->tdlsPeerExtRates ;
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
                                  ("pSta DS [%p] "), pStaDs) ;
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
                   ("peerInfo->tdlsPeerSuppRates = [%p]"),
                        (tANI_U8 *)&peerInfo->tdlsPeerSuppRates) ;
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
               ("peerInfo->tdlsPeerExtRates = [%p]"),
                        (tANI_U8 *)&peerInfo->tdlsPeerExtRates) ;
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
              ("peerInfo->tdlsPeerPropRates = [%p]"),
                        (tANI_U8 *)&pStaDs->mlmStaContext.propRateSet) ;
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
              ("peerInfo->mcs = [%p]"),
                        (tANI_U8 *)peerInfo->supportedMCSSet) ;
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
                  ("num of supp rates = %02x"), suppRates->numRates) ;
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
                      ("num of ext rates = %01x"), extRates->numRates) ;
#endif

        /* Populate matching rate set */
#ifdef WLAN_FEATURE_11AC
        if(eSIR_FAILURE == limPopulateMatchingRateSet(pMac, pStaDs, 
                                    &peerInfo->tdlsPeerSuppRates,
                                      &peerInfo->tdlsPeerExtRates, 
                                        peerInfo->supportedMCSSet,
                                         &pStaDs->mlmStaContext.propRateSet, 
                                                              psessionEntry, NULL))
#else
        if(eSIR_FAILURE == limPopulateMatchingRateSet(pMac, pStaDs, 
                                    &peerInfo->tdlsPeerSuppRates,
                                      &peerInfo->tdlsPeerExtRates, 
                                        peerInfo->supportedMCSSet,
                                         &pStaDs->mlmStaContext.propRateSet, 
                                                              psessionEntry))
#endif
        {
            VOS_ASSERT(0) ;
        }


        pStaDs->mlmStaContext.capabilityInfo = peerInfo->capabilityInfo;
        vos_mem_copy( pStaDs->staAddr, peerMac, sizeof(tSirMacAddr)) ;
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO,
                ("Add STA for Peer: "MAC_ADDRESS_STR),
                 MAC_ADDR_ARRAY(pStaDs->staAddr));
    

        pStaDs->staType = STA_ENTRY_TDLS_PEER ;

        status = limAddSta(pMac, pStaDs, false, psessionEntry);

        if(eSIR_SUCCESS != status)
        {
            /* should not fail */
            VOS_ASSERT(0) ;
        }
    }
  
    return status ;
}
#endif
/*
 * Add STA for TDLS setup procedure 
 */ 
static tSirRetStatus limTdlsSetupAddSta(tpAniSirGlobal pMac,
                                        tSirTdlsAddStaReq *pAddStaReq,
                                        tpPESession psessionEntry)
{
    tpDphHashNode pStaDs = NULL ;
    tSirRetStatus status = eSIR_SUCCESS ;
    tANI_U16 aid = 0 ;

    pStaDs = dphLookupHashEntry(pMac, pAddStaReq->peerMac, &aid,
                                      &psessionEntry->dph.dphHashTable);
    if(NULL == pStaDs)
    {
        aid = limAssignPeerIdx(pMac, psessionEntry) ;

        if( !aid )
        {
            limLog(pMac, LOGE, FL("No more free AID for peer " MAC_ADDRESS_STR),
                               MAC_ADDR_ARRAY(pAddStaReq->peerMac));
            return eSIR_FAILURE;
        }

        /* Set the aid in peerAIDBitmap as it has been assigned to TDLS peer */
        SET_PEER_AID_BITMAP(psessionEntry->peerAIDBitmap, aid);

        limLog(pMac, LOG1, FL("Aid = %d, for peer =" MAC_ADDRESS_STR),
                           aid, MAC_ADDR_ARRAY(pAddStaReq->peerMac));
        pStaDs = dphGetHashEntry(pMac, aid, &psessionEntry->dph.dphHashTable);

        if (pStaDs)
        {
            (void) limDelSta(pMac, pStaDs, false /*asynchronous*/, psessionEntry);
            limDeleteDphHashEntry(pMac, pStaDs->staAddr, aid, psessionEntry);
        }

        pStaDs = dphAddHashEntry(pMac, pAddStaReq->peerMac, aid,
                                             &psessionEntry->dph.dphHashTable) ;

        if(NULL == pStaDs)
        {
            limLog(pMac, LOGE, FL("add hash entry failed"));
            VOS_ASSERT(0) ;
            return eSIR_FAILURE;
        }
    }

    limTdlsUpdateHashNodeInfo(pMac, pStaDs, pAddStaReq, psessionEntry) ;

    pStaDs->staType = STA_ENTRY_TDLS_PEER ;

    status = limAddSta(pMac, pStaDs, (pAddStaReq->tdlsAddOper == TDLS_OPER_UPDATE) ? true: false, psessionEntry);

    if(eSIR_SUCCESS != status)
    {
        /* should not fail */
        VOS_ASSERT(0) ;
    }
    return status ;
}

/*
 * Del STA, after Link is teardown or discovery response sent on direct link
 */
static tpDphHashNode limTdlsDelSta(tpAniSirGlobal pMac, tSirMacAddr peerMac, 
                                                    tpPESession psessionEntry)
{
    tSirRetStatus status = eSIR_SUCCESS ;
    tANI_U16 peerIdx = 0 ;
    tpDphHashNode pStaDs = NULL ;
 
    pStaDs = dphLookupHashEntry(pMac, peerMac, &peerIdx, 
                                         &psessionEntry->dph.dphHashTable) ;

    if(pStaDs)
    {
    
        limLog(pMac, LOG1, FL("DEL STA peer MAC: "MAC_ADDRESS_STR),
                           MAC_ADDR_ARRAY(pStaDs->staAddr));
        limLog(pMac, LOG1, FL("STA type = %x, sta idx = %x"),
                           pStaDs->staType,
                           pStaDs->staIndex);
 
        status = limDelSta(pMac, pStaDs, false, psessionEntry) ;
#ifdef FEATURE_WLAN_TDLS_INTERNAL
        if(eSIR_SUCCESS == status)
        {
            limDeleteDphHashEntry(pMac, pStaDs->staAddr, peerIdx, psessionEntry) ;
            limReleasePeerIdx(pMac, peerIdx, psessionEntry) ;
        }
        else
        {
            VOS_ASSERT(0) ;
        }
#endif
    }
           
    return pStaDs ;
}
     
#ifdef FEATURE_WLAN_TDLS_INTERNAL
/* 
* Prepare link establish message for HAL, construct PTI template.
*
*/   
static tSirRetStatus limTdlsLinkEstablish(tpAniSirGlobal pMac, tSirMacAddr peerMac)
{
    tANI_U8             pFrame[64] ;
    tDot11fTDLSPeerTrafficInd tdlsPtiTemplate ;
    tANI_U32            status = 0 ;
    tANI_U32            nPayload = 0 ;
    tANI_U32            nBytes = 0 ;
    tANI_U32            header_offset = 0 ;
    tANI_U16            aid = 0 ;
    tDphHashNode        *pStaDs = NULL ;
    tLimTdlsLinkSetupPeer *setupPeer = NULL ;
    tpPESession psessionEntry = NULL ;


    limTdlsFindLinkPeer(pMac, peerMac, &setupPeer) ;
    if(NULL == setupPeer) {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR,
            ("limTdlsLinkEstablish: cannot find peer mac "
             "in tdls linksetup list: "MAC_ADDRESS_STR),
             MAC_ADDR_ARRAY(peerMac));
        return eSIR_FAILURE;
    }

    psessionEntry = peFindSessionBySessionId(pMac, 
                         setupPeer->tdls_sessionId) ;

    if(NULL == psessionEntry) 
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
             ("limTdlsLinkEstablish: sessionID %d is not found"), setupPeer->tdls_sessionId);
        VOS_ASSERT(0) ;
        return eHAL_STATUS_FAILURE;
    }


    pStaDs = dphLookupHashEntry(pMac, peerMac, &aid, &psessionEntry->dph.dphHashTable) ;
    if(pStaDs == NULL) {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR,
                  ("limTdlsLinkEstablish: cannot find peer mac "
                   "in tdls linksetup list: "MAC_ADDRESS_STR),
                   MAC_ADDR_ARRAY(peerMac));
        return eSIR_FAILURE;
    }

    vos_mem_set( ( tANI_U8* )&tdlsPtiTemplate,
               sizeof( tDot11fTDLSPeerTrafficInd ), 0 );

    /*
    * setup Fixed fields,
    */
    tdlsPtiTemplate.Category.category = SIR_MAC_ACTION_TDLS;
    tdlsPtiTemplate.Action.action     = SIR_MAC_TDLS_PEER_TRAFFIC_IND;
    tdlsPtiTemplate.DialogToken.token = 0 ; /* filled by firmware at the time of transmission */
#if 1 
    /* CHECK_PTI_LINK_IDENTIFIER_INITIATOR_ADDRESS: initator address should be TDLS link setup's initiator address, 
    then below code makes such an way */
    PopulateDot11fLinkIden( pMac, psessionEntry, &tdlsPtiTemplate.LinkIdentifier,
        peerMac, !setupPeer->tdls_bIsResponder) ;
#else
   /* below code will make PTI's linkIdentifier's initiator address be selfAddr */
    PopulateDot11fLinkIden( pMac, psessionEntry, &tdlsPtiTemplate.LinkIdentifier,
        peerMac, TDLS_INITIATOR) ;
#endif

    /* PUBufferStatus will be filled by firmware at the time of transmission */
    tdlsPtiTemplate.PUBufferStatus.present = 1;

    /* TODO: get ExtendedCapabilities IE */

    /* 
    * now we pack it.  First, how much space are we going to need?
    */
    status = dot11fGetPackedTDLSPeerTrafficIndSize ( pMac, &tdlsPtiTemplate, &nPayload);
    if ( DOT11F_FAILED( status ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size for a PTI template (0x%08x)."), status );
        /* We'll fall back on the worst case scenario: */
        nPayload = sizeof( tdlsPtiTemplate );
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating the packed size for a PTI template (0x%08x)."), status );
    }

    /*
    * This frame is going out from PE as data frames with special ethertype
    * 89-0d.
    * 8 bytes of RFC 1042 header
    */ 

    nBytes = nPayload + sizeof( tSirMacMgmtHdr ) 
            + sizeof( eth_890d_header ) 
            + PAYLOAD_TYPE_TDLS_SIZE ;

    if(nBytes > 64) {
        limLog( pMac, LOGE, FL("required memory for PTI frame is %ld, but reserved only 64."), nBytes);
        nBytes = 64;
    }
    /* zero out the memory */
    vos_mem_set( pFrame, sizeof(pFrame), 0 );

    /* fill out the buffer descriptor */

    header_offset = limPrepareTdlsFrameHeader(pMac, pFrame, 
        LINK_IDEN_ADDR_OFFSET(tdlsPtiTemplate), TDLS_LINK_AP, !setupPeer->tdls_bIsResponder, psessionEntry) ;

    status = dot11fPackTDLSPeerTrafficInd ( pMac, &tdlsPtiTemplate, pFrame 
        + header_offset, nPayload, &nPayload );

    if ( DOT11F_FAILED( status ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a PTI template (0x%08x)."),
                status );
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing TDLS "
                               "Peer Traffic Indication (0x%08x)."), status );
    }

    LIM_LOG_TDLS(VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, ("bIsResponder=%d, header_offset=%ld, linkIdenOffset=%d, ptiBufStatusOffset=%d "),
        setupPeer->tdls_bIsResponder, header_offset, PTI_LINK_IDEN_OFFSET, PTI_BUF_STATUS_OFFSET));

    limSendTdlsLinkEstablish(pMac, setupPeer->tdls_bIsResponder, 
        header_offset+PTI_LINK_IDEN_OFFSET, header_offset+PTI_BUF_STATUS_OFFSET, 
      nBytes, pFrame, (tANI_U8 *)&setupPeer->tdlsPeerExtCaps);

    return eSIR_SUCCESS;
}

/* 
* Prepare link teardown message for HAL from peer_mac
*
*/   
static tSirRetStatus limTdlsLinkTeardown(tpAniSirGlobal pMac, tSirMacAddr peerMac)
{
    tDphHashNode        *pStaDs = NULL ;
    tANI_U16            aid = 0 ;
    tLimTdlsLinkSetupPeer *setupPeer = NULL ;
    tpPESession psessionEntry = NULL ;


    limTdlsFindLinkPeer(pMac, peerMac, &setupPeer) ;
    if(NULL == setupPeer) {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR,
                  ("limTdlsLinkTeardown: cannot find peer mac "
                   "in tdls linksetup list: "
                   MAC_ADDRESS_STR), MAC_ADDR_ARRAY(peerMac));
        return eSIR_FAILURE;
    }

    psessionEntry = peFindSessionBySessionId(pMac, 
                         setupPeer->tdls_sessionId) ;

    if(NULL == psessionEntry) 
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
             ("limTdlsLinkTeardown: sessionID %d is not found"), setupPeer->tdls_sessionId);
        VOS_ASSERT(0) ;
        return eHAL_STATUS_FAILURE;
    }



    pStaDs = dphLookupHashEntry(pMac, peerMac, &aid, &psessionEntry->dph.dphHashTable);

    if(pStaDs == NULL) {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR,
                  ("limTdlsLinkTeardown: cannot find peer mac "
                   "in hash table: "
                   MAC_ADDRESS_STR), MAC_ADDR_ARRAY(peerMac));
        return eSIR_FAILURE;
    }

    limSendTdlsLinkTeardown(pMac, pStaDs->staIndex);

    return eSIR_SUCCESS;
}

/* 
 * Prepare Discovery RSP message for SME, collect peerINfo for all the 
 * peers discovered and delete/clean discovery lists in PE.
 */   
 
static tSirTdlsDisRsp *tdlsPrepareTdlsDisRsp(tpAniSirGlobal pMac, 
                                 tSirTdlsDisRsp *disRsp, tANI_U8 disStaCount) 
{
    tANI_U32 disMsgRspSize = sizeof(tSirTdlsDisRsp);
    tANI_U8 status = eHAL_STATUS_SUCCESS ;

    /*
     * allocate memory for tdls discovery response, allocated memory should
     * be alloc_mem = tdlsStaCount * sizeof(peerinfo) 
     *                              + siezeof tSirTdlsDisRsp.
     */
    disMsgRspSize += (disStaCount * sizeof(tSirTdlsPeerInfo));
        
    /* now allocate memory */

    disRsp = vos_mem_malloc(disMsgRspSize);
    if ( NULL == disRsp )
    {
        limLog(pMac, LOGP, FL("AllocateMemory failed for DIS RSP"));
        return NULL ;
    }
        
    if(disStaCount)
    { 
        tLimDisResultList *tdlsDisRspList = pMac->lim.gLimTdlsDisResultList ;
        tSirTdlsPeerInfo *peerInfo = &disRsp->tdlsDisPeerInfo[0] ;
            
        tLimDisResultList *currentNode = tdlsDisRspList ;
        while(tdlsDisRspList != NULL)
        {

            vos_mem_copy( (tANI_U8 *)peerInfo,
                          (tANI_U8 *) &tdlsDisRspList->tdlsDisPeerInfo, 
                                                 sizeof(tSirTdlsPeerInfo));
        
            VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO, 
            ("Msg Sent to PE, peer MAC: "MAC_ADDRESS_STR),
                                  MAC_ADDR_ARRAY(peerInfo->peerMac));
            disStaCount-- ;
            peerInfo++ ;
            currentNode = tdlsDisRspList ;
            tdlsDisRspList = tdlsDisRspList->next ;
            vos_mem_free(currentNode) ;
            /* boundary condition check, may be fatal */
            if(((!disStaCount) && (tdlsDisRspList)) 
                            || ((!tdlsDisRspList) && disStaCount))
            {
                limLog(pMac, LOG1, FL("mismatch in dis sta count and "
                                      "number of nodes in list")) ;
                VOS_ASSERT(0) ;
                return NULL ;
            } 
        } /* end  of while */

        /* All discovery STA processed */
        pMac->lim.gLimTdlsDisResultList = NULL ;

    } /* end of if dis STA count */
    
    return (disRsp) ;
}

/* Send Teardown response back to PE */

void limSendSmeTdlsTeardownRsp(tpAniSirGlobal pMac, tSirResultCodes statusCode,
                                        tSirMacAddr peerMac, tANI_U16 msgType)
{
    tSirMsgQ  mmhMsg = {0} ;
    tSirTdlsTeardownRsp *teardownRspMsg = NULL ;
    tANI_U8 status = eHAL_STATUS_SUCCESS ;
    
    mmhMsg.type = msgType ;

    teardownRspMsg = vos_mem_malloc(sizeof(tSirTdlsTeardownRsp));
    if ( NULL == teardownRspMsg )
    {
        VOS_ASSERT(0) ;
    } 
    vos_mem_copy( teardownRspMsg->peerMac, (tANI_U8 *)peerMac,
                                                   sizeof(tSirMacAddr)) ;
    teardownRspMsg->statusCode =  statusCode ;
    mmhMsg.bodyptr = teardownRspMsg ;
    mmhMsg.bodyval = 0;
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);

    return ;

}

/*
 * Send Link start RSP back to SME after link is setup or failed
 */
void limSendSmeTdlsLinkStartRsp(tpAniSirGlobal pMac,
                                         tSirResultCodes statusCode,
                                          tSirMacAddr peerMac, 
                                                 tANI_U16 msgType)
{
    tSirMsgQ  mmhMsg = {0} ;
    tSirTdlsLinksetupRsp *setupRspMsg = NULL ;
    tANI_U8 status = eHAL_STATUS_SUCCESS ;

    mmhMsg.type = msgType ;

    setupRspMsg = vos_mem_malloc(sizeof(tSirTdlsLinksetupRsp));
    if ( NULL == setupRspMsg )
    {
        VOS_ASSERT(0) ;
    } 

    vos_mem_copy( setupRspMsg->peerMac, (tANI_U8 *)peerMac,
                                                   sizeof(tSirMacAddr)) ;
    setupRspMsg->statusCode =  statusCode ;
    mmhMsg.bodyptr = setupRspMsg ;
    mmhMsg.bodyval = 0;
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);

    return ;
}

/*
 * Send TDLS discovery RSP back to SME 
 */
void limSendSmeTdlsDisRsp(tpAniSirGlobal pMac, tSirResultCodes statusCode,
                                                          tANI_U16 msgType)
{
    tSirMsgQ  mmhMsg = {0} ;
    tSirTdlsDisRsp *tdlsDisRsp = NULL ;

    mmhMsg.type = msgType ;

    if(eSIR_SME_SUCCESS == statusCode)
    {
        tANI_U8 tdlsStaCount = pMac->lim.gLimTdlsDisStaCount ;

        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
                    ("no of TDLS STA discovered: %d"), tdlsStaCount) ;
        tdlsDisRsp = tdlsPrepareTdlsDisRsp(pMac, tdlsDisRsp, tdlsStaCount) ;

        if(tdlsDisRsp)
        {
            tdlsDisRsp->numDisSta = tdlsStaCount ;
        }
        else
        {
            limLog(pMac, LOGP, FL("fatal failure for TDLS DIS RSP"));
            VOS_ASSERT(0) ; 
            return ;
        }
        /* all Discovery STA is processed */
        pMac->lim.gLimTdlsDisStaCount = 0 ;
    }
    else
    {
        tdlsDisRsp = tdlsPrepareTdlsDisRsp(pMac, tdlsDisRsp, 0) ;
    }

    tdlsDisRsp->statusCode =  statusCode ;
    mmhMsg.bodyptr = tdlsDisRsp ;
    mmhMsg.bodyval = 0;
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);

     return ;
}

/* 
 * Once Link is setup with PEER, send Add STA ind to SME
 */
static eHalStatus limSendSmeTdlsAddPeerInd(tpAniSirGlobal pMac, 
                   tANI_U8 sessionId, tDphHashNode   *pStaDs, tANI_U8 status)
{
    tSirMsgQ  mmhMsg = {0} ;
    tSirTdlsPeerInd *peerInd = NULL ;
    mmhMsg.type = eWNI_SME_ADD_TDLS_PEER_IND ;

    peerInd = vos_mem_malloc(sizeof(tSirTdlsPeerInd));
    if ( NULL == peerInd )
    {
        PELOGE(limLog(pMac, LOGE, FL("Failed to allocate memory"));)
        return eSIR_FAILURE;
    }

    vos_mem_copy( peerInd->peerMac,
                           (tANI_U8 *) pStaDs->staAddr, sizeof(tSirMacAddr));
    peerInd->sessionId = sessionId;
    peerInd->staId = pStaDs->staIndex ;
    peerInd->ucastSig = pStaDs->ucUcastSig ;
    peerInd->bcastSig = pStaDs->ucBcastSig ;
    peerInd->length = sizeof(tSmeIbssPeerInd) ;

    mmhMsg.bodyptr = peerInd ;
    mmhMsg.bodyval = 0;
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);

    return eSIR_SUCCESS ;

}

/*
 * Once link is teardown, send Del Peer Ind to SME
 */
static eHalStatus limSendSmeTdlsDelPeerInd(tpAniSirGlobal pMac, 
                    tANI_U8 sessionId, tDphHashNode   *pStaDs, tANI_U8 status)
{
    tSirMsgQ  mmhMsg = {0} ;
    tSirTdlsPeerInd *peerInd = NULL ;
    mmhMsg.type = eWNI_SME_DELETE_TDLS_PEER_IND ;

    peerInd = vos_mem_malloc(sizeof(tSirTdlsPeerInd));
    if ( NULL == peerInd )
    {
        PELOGE(limLog(pMac, LOGE, FL("Failed to allocate memory"));)
        return eSIR_FAILURE;
    }

    vos_mem_copy( peerInd->peerMac,
                           (tANI_U8 *) pStaDs->staAddr, sizeof(tSirMacAddr));
    peerInd->sessionId = sessionId;
    peerInd->staId = pStaDs->staIndex ;
    peerInd->ucastSig = pStaDs->ucUcastSig ;
    peerInd->bcastSig = pStaDs->ucBcastSig ;
    peerInd->length = sizeof(tSmeIbssPeerInd) ;

    mmhMsg.bodyptr = peerInd ;

    //peerInd->statusCode =  status ;
    mmhMsg.bodyval = 0;
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
    return eSIR_SUCCESS ;

}

/*
 * Send Link setup Ind to SME, This is the case where, link setup is 
 * initiated by peer STA
 */
static eHalStatus limSendSmeTdlsLinkSetupInd(tpAniSirGlobal pMac, 
                                   tSirMacAddr peerMac, tANI_U8 status)
{
    tSirMsgQ  mmhMsg = {0} ;
    tSirTdlsLinkSetupInd *setupInd = NULL ;

    mmhMsg.type = eWNI_SME_TDLS_LINK_START_IND ;
    setupInd = vos_mem_malloc(sizeof(tSirTdlsLinkSetupInd));
    if ( NULL == setupInd )
    {
        PELOGE(limLog(pMac, LOGE, FL("Failed to allocate memory"));)
        return eSIR_FAILURE;
    }

    vos_mem_copy( setupInd->peerMac,
                           (tANI_U8 *) peerMac, sizeof(tSirMacAddr));
    setupInd->length = sizeof(tSirTdlsLinkSetupInd);
    setupInd->statusCode = status ;
    mmhMsg.bodyptr = setupInd ;
    mmhMsg.bodyval = 0;
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);

    return eSIR_SUCCESS ;

}

/*
 * Setup RSP timer handler 
 */
void limTdlsLinkSetupRspTimerHandler(void *pMacGlobal, tANI_U32 timerId)
{

    tANI_U32         statusCode;
    tSirMsgQ    msg;
    tpAniSirGlobal pMac = (tpAniSirGlobal)pMacGlobal;

    /* Prepare and post message to LIM Message Queue */

    msg.type = SIR_LIM_TDLS_LINK_SETUP_RSP_TIMEOUT;
    msg.bodyptr = NULL ;
    msg.bodyval = timerId ;

    if ((statusCode = limPostMsgApi(pMac, &msg)) != eSIR_SUCCESS)
        limLog(pMac, LOGE,
               FL("posting message %X to LIM failed, reason=%d"),
               msg.type, statusCode);
    return ;
}

/*
 * Link setup CNF timer
 */
void limTdlsLinkSetupCnfTimerHandler(void *pMacGlobal, tANI_U32 timerId)
{

    tANI_U32         statusCode;
    tSirMsgQ    msg;
    tpAniSirGlobal pMac = (tpAniSirGlobal)pMacGlobal;

    // Prepare and post message to LIM Message Queue

    msg.type = SIR_LIM_TDLS_LINK_SETUP_CNF_TIMEOUT;
    msg.bodyptr = NULL ;
    msg.bodyval = timerId ;

    if ((statusCode = limPostMsgApi(pMac, &msg)) != eSIR_SUCCESS)
        limLog(pMac, LOGE,
               FL("posting message %X to LIM failed, reason=%d"),
               msg.type, statusCode);
    return ;
}

/*
 * start TDLS timer
 */
void limStartTdlsTimer(tpAniSirGlobal pMac, tANI_U8 sessionId, TX_TIMER *timer,
                        tANI_U32 timerId, tANI_U16 timerType, tANI_U32 timerMsg)
{
    tANI_U32 cfgValue = (timerMsg == SIR_LIM_TDLS_LINK_SETUP_RSP_TIMEOUT)
                           ? WNI_CFG_TDLS_LINK_SETUP_RSP_TIMEOUT
                            : WNI_CFG_TDLS_LINK_SETUP_CNF_TIMEOUT ;

    void *timerFunc = (timerMsg == SIR_LIM_TDLS_LINK_SETUP_RSP_TIMEOUT)
                                ? (limTdlsLinkSetupRspTimerHandler) 
                                    : limTdlsLinkSetupCnfTimerHandler ;

    /* TODO: Read timer vals from CFG */

    cfgValue = SYS_MS_TO_TICKS(cfgValue);
    /*
     * create TDLS discovery response wait timer and activate it
     */
    if (tx_timer_create(timer, "TDLS link setup timers", timerFunc,
                        timerId, cfgValue, 0, TX_NO_ACTIVATE) != TX_SUCCESS)
    {
        limLog(pMac, LOGP,
           FL("could not create TDLS discovery response wait timer"));
        return;
    }

    //assign appropriate sessionId to the timer object
    timer->sessionId = sessionId; 
    
     MTRACE(macTrace(pMac, TRACE_CODE_TIMER_ACTIVATE, 0,
                                             eLIM_TDLS_DISCOVERY_RSP_WAIT));
    if (tx_timer_activate(timer) != TX_SUCCESS)
    {
        limLog(pMac, LOGP, FL("TDLS link setup timer activation failed!"));
        return ;
    }

    return ;

}
#endif

/* 
 * Once Link is setup with PEER, send Add STA ind to SME
 */
static eHalStatus limSendSmeTdlsAddStaRsp(tpAniSirGlobal pMac, 
                   tANI_U8 sessionId, tSirMacAddr peerMac, tANI_U8 updateSta,
                   tDphHashNode  *pStaDs, tANI_U8 status)
{
    tSirMsgQ  mmhMsg = {0} ;
    tSirTdlsAddStaRsp *addStaRsp = NULL ;
    mmhMsg.type = eWNI_SME_TDLS_ADD_STA_RSP ;

    addStaRsp = vos_mem_malloc(sizeof(tSirTdlsAddStaRsp));
    if ( NULL == addStaRsp )
    {
        limLog(pMac, LOGE, FL("Failed to allocate memory"));
        return eSIR_FAILURE;
    }

    addStaRsp->sessionId = sessionId;
    addStaRsp->statusCode = status;
    if( pStaDs )
    {
        addStaRsp->staId = pStaDs->staIndex ;
        addStaRsp->ucastSig = pStaDs->ucUcastSig ;
        addStaRsp->bcastSig = pStaDs->ucBcastSig ;
    }
    if( peerMac )
    {
        vos_mem_copy( addStaRsp->peerMac,
                (tANI_U8 *) peerMac, sizeof(tSirMacAddr));
    }
    if (updateSta)
        addStaRsp->tdlsAddOper = TDLS_OPER_UPDATE;
    else
        addStaRsp->tdlsAddOper = TDLS_OPER_ADD;

    addStaRsp->length = sizeof(tSirTdlsAddStaRsp) ;
    addStaRsp->messageType = eWNI_SME_TDLS_ADD_STA_RSP ;

    mmhMsg.bodyptr = addStaRsp;
    mmhMsg.bodyval = 0;
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);

    return eSIR_SUCCESS ;

}
/* 
 * STA RSP received from HAL
 */
eHalStatus limProcessTdlsAddStaRsp(tpAniSirGlobal pMac, void *msg, 
                                                   tpPESession psessionEntry)
{
    tAddStaParams  *pAddStaParams = (tAddStaParams *) msg ;
    tANI_U8        status = eSIR_SUCCESS ;
    tDphHashNode   *pStaDs = NULL ;
    tANI_U16        aid = 0 ;

    SET_LIM_PROCESS_DEFD_MESGS(pMac, true);
    limLog(pMac, LOG1, FL("staIdx=%d, staMac="MAC_ADDRESS_STR),
                       pAddStaParams->staIdx,
                       MAC_ADDR_ARRAY(pAddStaParams->staMac));

    if (pAddStaParams->status != eHAL_STATUS_SUCCESS)
    {
        VOS_ASSERT(0) ;
        limLog(pMac, LOGE, FL("Add sta failed "));
        status = eSIR_FAILURE;
        goto add_sta_error;
    }

    pStaDs = dphLookupHashEntry(pMac, pAddStaParams->staMac, &aid, 
                                         &psessionEntry->dph.dphHashTable);
    if(NULL == pStaDs)
    {
        limLog(pMac, LOGE, FL("pStaDs is NULL "));
        status = eSIR_FAILURE;
        goto add_sta_error;
    }

    pStaDs->bssId                  = pAddStaParams->bssIdx;
    pStaDs->staIndex               = pAddStaParams->staIdx;
    pStaDs->ucUcastSig             = pAddStaParams->ucUcastSig;
    pStaDs->ucBcastSig             = pAddStaParams->ucBcastSig;
    pStaDs->mlmStaContext.mlmState = eLIM_MLM_LINK_ESTABLISHED_STATE;
    pStaDs->valid                  = 1 ;
#ifdef FEATURE_WLAN_TDLS_INTERNAL    
    status = limSendSmeTdlsAddPeerInd(pMac, psessionEntry->smeSessionId, 
                                                    pStaDs, eSIR_SUCCESS ) ;
    if(eSIR_FAILURE == status)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR,
                                         ("Peer IND msg to SME failed")) ;
        vos_mem_free( pAddStaParams );
        return eSIR_FAILURE ;
    }

    /* 
     * Now, there is two things a) ADD STA RSP for ADD STA request sent
     * after recieving discovery request from Peer.
     * now we have to send discovery response, if there is any pending
     * discovery equest..
     */
    do
    {
        tSirTdlsPeerInfo *peerInfo = limTdlsFindDisPeer(pMac,
                                            pAddStaParams->staMac) ;

    
        if(peerInfo)
        {
            /* 
             * send TDLS discovery response frame on direct link, state machine
             * is rolling.., once discovery response is get Acked, we will 
             * send response to SME based on TxComplete callback results
             */ 
            limSendTdlsDisRspFrame(pMac, peerInfo->peerMac, peerInfo->dialog,
                                   psessionEntry, NULL, 0);
            peerInfo->tdlsPeerState = TDLS_DIS_RSP_SENT_WAIT_STATE ;
        }
    } while(0) ;
#endif
add_sta_error:
    status = limSendSmeTdlsAddStaRsp(pMac, psessionEntry->smeSessionId, 
                                        pAddStaParams->staMac, pAddStaParams->updateSta, pStaDs, status) ;
    vos_mem_free( pAddStaParams );
    return status ;
}

void PopulateDot11fTdlsOffchannelParams(tpAniSirGlobal pMac,
                             tpPESession psessionEntry,
                             tDot11fIESuppChannels *suppChannels,
                             tDot11fIESuppOperatingClasses *suppOperClasses)
{
    tANI_U32   numChans = WNI_CFG_VALID_CHANNEL_LIST_LEN;
    tANI_U8    validChan[WNI_CFG_VALID_CHANNEL_LIST_LEN];
    tANI_U8    i;
    tANI_U8    op_class;
    if (wlan_cfgGetStr(pMac, WNI_CFG_VALID_CHANNEL_LIST,
                          validChan, &numChans) != eSIR_SUCCESS)
    {
        /**
         * Could not get Valid channel list from CFG.
         * Log error.
         */
         limLog(pMac, LOGE, FL("could not retrieve valid channel list"));
    }
    suppChannels->num_bands = (tANI_U8) numChans;

    for ( i = 0U; i < suppChannels->num_bands; i++)
    {
        suppChannels->bands[i][0] = validChan[i];
        suppChannels->bands[i][1] = 1;
    }
    suppChannels->present = 1 ;
    /*Get present operating class based on current operating channel*/
    op_class = limGetOPClassFromChannel(
                                     pMac->scan.countryCodeCurrent,
                                     psessionEntry->currentOperChannel,
                                     psessionEntry->htSecondaryChannelOffset);
    if (op_class == 0)
    {
        limLog(pMac, LOGE,
               FL("Present Operating class is wrong, countryCodeCurrent: %s, currentOperChannel: %d, htSecondaryChannelOffset: %d"),
               pMac->scan.countryCodeCurrent,
               psessionEntry->currentOperChannel,
               psessionEntry->htSecondaryChannelOffset);
    }
    else
    {
        limLog(pMac, LOG1,
               FL("Present Operating channel=%d offset=%d class=%d"),
               psessionEntry->currentOperChannel,
               psessionEntry->htSecondaryChannelOffset,
               op_class);
    }
    suppOperClasses->present = 1;
    suppOperClasses->classes[0] = op_class;
    /*Fill operating classes from static array*/
    suppOperClasses->num_classes = op_classes.num_classes;
    for ( i = 0U; i < suppOperClasses->num_classes; i++)
    {
        suppOperClasses->classes[i+1] = op_classes.classes[i];

    }
    /*increment for present operating class*/
    suppOperClasses->num_classes++;
    return ;
}


/*
 * FUNCTION: Populate Link Identifier element IE
 *
 */


void PopulateDot11fLinkIden(tpAniSirGlobal pMac, tpPESession psessionEntry, 
                                 tDot11fIELinkIdentifier *linkIden,
                                       tSirMacAddr peerMac, tANI_U8 reqType)
{
    //tANI_U32 size = sizeof(tSirMacAddr) ;
    tANI_U8 *initStaAddr = NULL ;
    tANI_U8 *respStaAddr = NULL ;

    (reqType == TDLS_INITIATOR) ? ((initStaAddr = linkIden->InitStaAddr),
                                   (respStaAddr = linkIden->RespStaAddr))
                                : ((respStaAddr = linkIden->InitStaAddr ),
                                   (initStaAddr = linkIden->RespStaAddr)) ;
    vos_mem_copy( (tANI_U8 *)linkIden->bssid,
                     (tANI_U8 *) psessionEntry->bssId, sizeof(tSirMacAddr)) ; 

    vos_mem_copy( (tANI_U8 *)initStaAddr,
                          psessionEntry->selfMacAddr, sizeof(tSirMacAddr)) ;

    vos_mem_copy( (tANI_U8 *)respStaAddr, (tANI_U8 *) peerMac,
                                                       sizeof( tSirMacAddr ));

    linkIden->present = 1 ;
    return ;

}

void PopulateDot11fTdlsExtCapability(tpAniSirGlobal pMac, 
                                        tDot11fIEExtCap *extCapability)
{
    extCapability->TDLSPeerPSMSupp = PEER_PSM_SUPPORT ;
    extCapability->TDLSPeerUAPSDBufferSTA = pMac->lim.gLimTDLSBufStaEnabled;
    extCapability->TDLSChannelSwitching = pMac->lim.gLimTDLSOffChannelEnabled ;
    extCapability->TDLSSupport = TDLS_SUPPORT ;
    extCapability->TDLSProhibited = TDLS_PROHIBITED ;
    extCapability->TDLSChanSwitProhibited = TDLS_CH_SWITCH_PROHIBITED ;
    extCapability->present = 1 ;
    return ;
}
                                     
#ifdef FEATURE_WLAN_TDLS_INTERNAL
/*
 * Public Action frame common processing
 * This Function will be moved/merged to appropriate place
 * once other public action frames (particularly 802.11k)
 * is in place
 */
void limProcessTdlsPublicActionFrame(tpAniSirGlobal pMac, tANI_U32 *pBd, 
                                                  tpPESession psessionEntry)
{
    tANI_U32 frameLen = WDA_GET_RX_PAYLOAD_LEN(pBd) ;
    tANI_U8 *pBody = WDA_GET_RX_MPDU_DATA(pBd) ;
    tANI_S8 rssi = (tANI_S8)WDA_GET_RX_RSSI_DB(pBd) ;

    limProcessTdlsDisRspFrame(pMac, pBody, frameLen, rssi, psessionEntry) ;
    return ; 
}

eHalStatus limTdlsPrepareSetupReqFrame(tpAniSirGlobal pMac, 
                              tLimTdlsLinkSetupInfo *linkSetupInfo,
                                 tANI_U8 dialog, tSirMacAddr peerMac,
                                                 tpPESession psessionEntry)
{
    tLimTdlsLinkSetupPeer *setupPeer = NULL ;

    /*
    * we allocate the TDLS setup Peer Memory here, we will free'd this
    * memory after teardown, if the link is successfully setup or
    * free this memory if any timeout is happen in link setup procedure
    */

    setupPeer = vos_mem_malloc(sizeof( tLimTdlsLinkSetupPeer ));
    if ( NULL == setupPeer )
    {
        limLog( pMac, LOGP, 
                  FL( "Unable to allocate memory during ADD_STA" ));
         VOS_ASSERT(0) ;
         return eSIR_MEM_ALLOC_FAILED;
    }
    setupPeer->dialog = dialog ;
    setupPeer->tdls_prev_link_state =  setupPeer->tdls_link_state ;
    setupPeer->tdls_link_state = TDLS_LINK_SETUP_START_STATE ;

    /* TDLS_sessionize: remember sessionId for future */
    setupPeer->tdls_sessionId = psessionEntry->peSessionId;
    setupPeer->tdls_bIsResponder = 1;

    /* 
    * we only populate peer MAC, so it can assit us to find the
    * TDLS peer after response/or after response timeout
    */
    vos_mem_copy(setupPeer->peerMac, peerMac,
                                              sizeof(tSirMacAddr)) ;
    /* format TDLS discovery request frame and transmit it */
    limSendTdlsLinkSetupReqFrame(pMac, peerMac, dialog, psessionEntry, NULL, 0) ;

    limStartTdlsTimer(pMac, psessionEntry->peSessionId, 
                        &setupPeer->gLimTdlsLinkSetupRspTimeoutTimer,
                            (tANI_U32)setupPeer->peerMac, 
                               WNI_CFG_TDLS_LINK_SETUP_RSP_TIMEOUT,
                                 SIR_LIM_TDLS_LINK_SETUP_RSP_TIMEOUT) ;
    /* update setup peer list */
    setupPeer->next = linkSetupInfo->tdlsLinkSetupList ;
    linkSetupInfo->tdlsLinkSetupList = setupPeer ;

    /* in case of success, eWNI_SME_TDLS_LINK_START_RSP is sent back to 
     * SME later when TDLS setup cnf TX complete is successful. --> see 
     * limTdlsSetupCnfTxComplete() 
     */
    return eSIR_SUCCESS ; 
}
#endif

/*
 * Process Send Mgmt Request from SME and transmit to AP.
 */
tSirRetStatus limProcessSmeTdlsMgmtSendReq(tpAniSirGlobal pMac, 
                                                           tANI_U32 *pMsgBuf)
{
    /* get all discovery request parameters */
    tSirTdlsSendMgmtReq *pSendMgmtReq = (tSirTdlsSendMgmtReq*) pMsgBuf ;
    tpPESession psessionEntry;
    tANI_U8      sessionId;
    tSirResultCodes resultCode = eSIR_SME_INVALID_PARAMETERS;

    limLog(pMac, LOG1, FL("Send Mgmt Recieved"));

    if((psessionEntry = peFindSessionByBssid(pMac, pSendMgmtReq->bssid, &sessionId)) 
            == NULL)
    {
        limLog(pMac, LOGE,
               FL("PE Session does not exist for given sme sessionId %d"),
               pSendMgmtReq->sessionId);
        goto lim_tdls_send_mgmt_error;
    }

    /* check if we are in proper state to work as TDLS client */ 
    if (psessionEntry->limSystemRole != eLIM_STA_ROLE)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
                "send mgmt received in wrong system Role %d",
                psessionEntry->limSystemRole);
        goto lim_tdls_send_mgmt_error;
    }

    /*
     * if we are still good, go ahead and check if we are in proper state to
     * do TDLS discovery req/rsp/....frames.
     */
    if ((psessionEntry->limSmeState != eLIM_SME_ASSOCIATED_STATE) &&
            (psessionEntry->limSmeState != eLIM_SME_LINK_EST_STATE))
    {

        limLog(pMac, LOGE, FL("send mgmt received in invalid LIMsme state (%d)"),
                           psessionEntry->limSmeState);
        goto lim_tdls_send_mgmt_error;
    }

    switch( pSendMgmtReq->reqType )
    {
        case SIR_MAC_TDLS_DIS_REQ:
            limLog(pMac, LOG1, FL("Transmit Discovery Request Frame"));
            /* format TDLS discovery request frame and transmit it */
            limSendTdlsDisReqFrame(pMac, pSendMgmtReq->peerMac, pSendMgmtReq->dialog, 
                    psessionEntry) ;
            resultCode = eSIR_SME_SUCCESS;
            break;
        case SIR_MAC_TDLS_DIS_RSP:
            {
                limLog(pMac, LOG1, FL("Transmit Discovery Response Frame"));
                //Send a response mgmt action frame
                limSendTdlsDisRspFrame(pMac, pSendMgmtReq->peerMac,
                        pSendMgmtReq->dialog, psessionEntry,
                        &pSendMgmtReq->addIe[0],
                        (pSendMgmtReq->length - sizeof(tSirTdlsSendMgmtReq)));
                resultCode = eSIR_SME_SUCCESS;
            }
            break;
        case SIR_MAC_TDLS_SETUP_REQ:
            {
                limLog(pMac, LOG1, FL("Transmit Setup Request Frame"));
                limSendTdlsLinkSetupReqFrame(pMac,
                        pSendMgmtReq->peerMac, pSendMgmtReq->dialog, psessionEntry,
                        &pSendMgmtReq->addIe[0], (pSendMgmtReq->length - sizeof(tSirTdlsSendMgmtReq))); 
                resultCode = eSIR_SME_SUCCESS;
            }
            break;
        case SIR_MAC_TDLS_SETUP_RSP:
            {
                limLog(pMac, LOG1, FL("Transmit Setup Response Frame"));
                limSendTdlsSetupRspFrame(pMac, 
                        pSendMgmtReq->peerMac, pSendMgmtReq->dialog, psessionEntry, pSendMgmtReq->statusCode,
                        &pSendMgmtReq->addIe[0], (pSendMgmtReq->length - sizeof(tSirTdlsSendMgmtReq)));
                resultCode = eSIR_SME_SUCCESS;
            }
            break;
        case SIR_MAC_TDLS_SETUP_CNF:
            {
                limLog(pMac, LOG1, FL("Transmit Setup Confirm Frame"));
                limSendTdlsLinkSetupCnfFrame(pMac, pSendMgmtReq->peerMac, pSendMgmtReq->dialog, pSendMgmtReq->peerCapability,
                        psessionEntry, &pSendMgmtReq->addIe[0], (pSendMgmtReq->length - sizeof(tSirTdlsSendMgmtReq)));  
                resultCode = eSIR_SME_SUCCESS;
            }
            break;
        case SIR_MAC_TDLS_TEARDOWN:
            {
                limLog(pMac, LOG1, FL("Transmit Teardown Frame"));
                limSendTdlsTeardownFrame(pMac,
                        pSendMgmtReq->peerMac, pSendMgmtReq->statusCode, pSendMgmtReq->responder, psessionEntry,
                        &pSendMgmtReq->addIe[0], (pSendMgmtReq->length - sizeof(tSirTdlsSendMgmtReq))); 
                resultCode = eSIR_SME_SUCCESS;
            }
            break;
        case SIR_MAC_TDLS_PEER_TRAFFIC_IND:
            {
            }
            break;
        case SIR_MAC_TDLS_CH_SWITCH_REQ:
            {
            }
            break;
        case SIR_MAC_TDLS_CH_SWITCH_RSP:
            {
            }
            break;
        case SIR_MAC_TDLS_PEER_TRAFFIC_RSP:
            {
            }
            break;
        default:
            break;
    }

lim_tdls_send_mgmt_error:

    limSendSmeRsp( pMac, eWNI_SME_TDLS_SEND_MGMT_RSP,
            resultCode, pSendMgmtReq->sessionId, pSendMgmtReq->transactionId);

    return eSIR_SUCCESS;
}

/*
 * Send Response to Link Establish Request to SME
 */
void limSendSmeTdlsLinkEstablishReqRsp(tpAniSirGlobal pMac,
                    tANI_U8 sessionId, tSirMacAddr peerMac, tDphHashNode   *pStaDs,
                    tANI_U8 status)
{
    tSirMsgQ  mmhMsg = {0} ;

    tSirTdlsLinkEstablishReqRsp *pTdlsLinkEstablishReqRsp = NULL ;

    pTdlsLinkEstablishReqRsp = vos_mem_malloc(sizeof(tSirTdlsLinkEstablishReqRsp));
    if ( NULL == pTdlsLinkEstablishReqRsp )
    {
        limLog(pMac, LOGE, FL("Failed to allocate memory"));
        return ;
    }
    pTdlsLinkEstablishReqRsp->statusCode = status ;
    if ( peerMac )
    {
        vos_mem_copy(pTdlsLinkEstablishReqRsp->peerMac, peerMac, sizeof(tSirMacAddr));
    }
    pTdlsLinkEstablishReqRsp->sessionId = sessionId;
    mmhMsg.type = eWNI_SME_TDLS_LINK_ESTABLISH_RSP ;
    mmhMsg.bodyptr = pTdlsLinkEstablishReqRsp;
    mmhMsg.bodyval = 0;
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
    return ;


}

/*
 * Send Response to Chan Switch Request to SME
 */
void limSendSmeTdlsChanSwitchReqRsp(tpAniSirGlobal pMac,
                    tANI_U8 sessionId, tSirMacAddr peerMac, tDphHashNode   *pStaDs,
                    tANI_U8 status)
{
    tSirMsgQ  mmhMsg = {0} ;

    tSirTdlsChanSwitchReqRsp *pTdlsChanSwitchReqRsp = NULL ;

    pTdlsChanSwitchReqRsp = vos_mem_malloc(sizeof(tSirTdlsChanSwitchReqRsp));
    if ( NULL == pTdlsChanSwitchReqRsp )
    {
        PELOGE(limLog(pMac, LOGE, FL("Failed to allocate memory"));)
        return ;
    }
    pTdlsChanSwitchReqRsp->statusCode = status ;
    if ( peerMac )
    {
        vos_mem_copy(pTdlsChanSwitchReqRsp->peerMac, peerMac, sizeof(tSirMacAddr));
    }
    pTdlsChanSwitchReqRsp->sessionId = sessionId;
    mmhMsg.type = eWNI_SME_TDLS_CHANNEL_SWITCH_RSP ;
    mmhMsg.bodyptr = pTdlsChanSwitchReqRsp;
    mmhMsg.bodyval = 0;
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
    return ;


}
/*
 * Once link is teardown, send Del Peer Ind to SME
 */
static eHalStatus limSendSmeTdlsDelStaRsp(tpAniSirGlobal pMac, 
                    tANI_U8 sessionId, tSirMacAddr peerMac, tDphHashNode   *pStaDs,
                    tANI_U8 status)
{
    tSirMsgQ  mmhMsg = {0} ;
    tSirTdlsDelStaRsp *pDelSta = NULL ;
    mmhMsg.type = eWNI_SME_TDLS_DEL_STA_RSP ;

    pDelSta = vos_mem_malloc(sizeof(tSirTdlsDelStaRsp));
    if ( NULL == pDelSta )
    {
        limLog(pMac, LOGE, FL("Failed to allocate memory"));
            return eSIR_FAILURE;
    }

    pDelSta->sessionId = sessionId;
    pDelSta->statusCode =  status ;
    if( pStaDs )
    {
        pDelSta->staId = pStaDs->staIndex ;
    }
    else
        pDelSta->staId = HAL_STA_INVALID_IDX;

    if( peerMac )
    {
        vos_mem_copy(pDelSta->peerMac, peerMac, sizeof(tSirMacAddr));
    }

    pDelSta->length = sizeof(tSirTdlsDelStaRsp) ;
    pDelSta->messageType = eWNI_SME_TDLS_DEL_STA_RSP ;

    mmhMsg.bodyptr = pDelSta;

    mmhMsg.bodyval = 0;
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
    return eSIR_SUCCESS ;

}

/*
 * Process Send Mgmt Request from SME and transmit to AP.
 */
tSirRetStatus limProcessSmeTdlsAddStaReq(tpAniSirGlobal pMac, 
                                                           tANI_U32 *pMsgBuf)
{
    /* get all discovery request parameters */
    tSirTdlsAddStaReq *pAddStaReq = (tSirTdlsAddStaReq*) pMsgBuf ;
    tpPESession psessionEntry;
    tANI_U8      sessionId;

    limLog(pMac, LOG1, FL("TDLS Add STA Request Recieved"));

    if((psessionEntry = peFindSessionByBssid(pMac, pAddStaReq->bssid, &sessionId)) 
                                                                        == NULL)
    {
         limLog(pMac, LOGE,
                FL("PE Session does not exist for given sme sessionId %d"),
                pAddStaReq->sessionId);
         goto lim_tdls_add_sta_error;
    }
    
    /* check if we are in proper state to work as TDLS client */ 
    if (psessionEntry->limSystemRole != eLIM_STA_ROLE)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
                         "send mgmt received in wrong system Role %d",
                                             psessionEntry->limSystemRole);
        goto lim_tdls_add_sta_error;
    }

    /*
     * if we are still good, go ahead and check if we are in proper state to
     * do TDLS discovery req/rsp/....frames.
     */
     if ((psessionEntry->limSmeState != eLIM_SME_ASSOCIATED_STATE) &&
                (psessionEntry->limSmeState != eLIM_SME_LINK_EST_STATE))
     {
     
         limLog(pMac, LOGE,
                FL("Add STA received in invalid LIMsme state (%d)"),
                psessionEntry->limSmeState);
         goto lim_tdls_add_sta_error;
     }

     pMac->lim.gLimAddStaTdls = true ;

     /* To start with, send add STA request to HAL */
     if (eSIR_FAILURE == limTdlsSetupAddSta(pMac, pAddStaReq, psessionEntry))
     {
         limLog(pMac, LOGE, FL("Add TDLS Station request failed"));
         goto lim_tdls_add_sta_error;
     }
     return eSIR_SUCCESS;
lim_tdls_add_sta_error:
     limSendSmeTdlsAddStaRsp(pMac, 
                   pAddStaReq->sessionId, pAddStaReq->peerMac,
                   (pAddStaReq->tdlsAddOper == TDLS_OPER_UPDATE), NULL, eSIR_FAILURE );

   return eSIR_SUCCESS;
}
/*
 * Process Del Sta Request from SME .
 */
tSirRetStatus limProcessSmeTdlsDelStaReq(tpAniSirGlobal pMac, 
                                                           tANI_U32 *pMsgBuf)
{
    /* get all discovery request parameters */
    tSirTdlsDelStaReq *pDelStaReq = (tSirTdlsDelStaReq*) pMsgBuf ;
    tpPESession psessionEntry;
    tANI_U8      sessionId;
    tpDphHashNode pStaDs = NULL ;

    limLog(pMac, LOG1, FL("TDLS Delete STA Request Recieved"));

    if((psessionEntry = peFindSessionByBssid(pMac, pDelStaReq->bssid, &sessionId)) 
            == NULL)
    {
        limLog(pMac, LOGE,
               FL("PE Session does not exist for given sme sessionId %d"),
               pDelStaReq->sessionId);
        limSendSmeTdlsDelStaRsp(pMac, pDelStaReq->sessionId, pDelStaReq->peerMac,
             NULL, eSIR_FAILURE) ;
        return eSIR_FAILURE;
    }

    /* check if we are in proper state to work as TDLS client */ 
    if (psessionEntry->limSystemRole != eLIM_STA_ROLE)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR, 
                "Del sta received in wrong system Role %d",
                psessionEntry->limSystemRole);
        goto lim_tdls_del_sta_error;
    }

    /*
     * if we are still good, go ahead and check if we are in proper state to
     * do TDLS discovery req/rsp/....frames.
     */
    if ((psessionEntry->limSmeState != eLIM_SME_ASSOCIATED_STATE) &&
            (psessionEntry->limSmeState != eLIM_SME_LINK_EST_STATE))
    {

        limLog(pMac, LOGE, FL("Del Sta received in invalid LIMsme state (%d)"),
                           psessionEntry->limSmeState);
        goto lim_tdls_del_sta_error;
    }

    pStaDs = limTdlsDelSta(pMac, pDelStaReq->peerMac, psessionEntry) ;

    /* now send indication to SME-->HDD->TL to remove STA from TL */

    if(pStaDs)
    {
        limSendSmeTdlsDelStaRsp(pMac, psessionEntry->smeSessionId, pDelStaReq->peerMac,
                pStaDs, eSIR_SUCCESS) ;
        limReleasePeerIdx(pMac, pStaDs->assocId, psessionEntry) ;

        /* Clear the aid in peerAIDBitmap as this aid is now in freepool */
        CLEAR_PEER_AID_BITMAP(psessionEntry->peerAIDBitmap, pStaDs->assocId);
        limDeleteDphHashEntry(pMac, pStaDs->staAddr, pStaDs->assocId, psessionEntry) ;

        return eSIR_SUCCESS;

    }

lim_tdls_del_sta_error:
     limSendSmeTdlsDelStaRsp(pMac, psessionEntry->smeSessionId, pDelStaReq->peerMac,
             NULL, eSIR_FAILURE) ;

    return eSIR_SUCCESS;
}

/* Intersects the two input arrays and outputs an array */
/* For now the array length of tANI_U8 suffices */
static void limTdlsGetIntersection(tANI_U8 *input_array1,tANI_U8 input1_length,
                            tANI_U8 *input_array2,tANI_U8 input2_length,
                            tANI_U8 *output_array,tANI_U8 *output_length)
{
    tANI_U8 i,j,k=0,flag=0;

    if (input1_length > WNI_CFG_VALID_CHANNEL_LIST_LEN)
    {
       input1_length = WNI_CFG_VALID_CHANNEL_LIST_LEN;
    }

    for(i=0;i<input1_length;i++)
    {
        flag=0;
        for(j=0;j<input2_length;j++)
        {
            if(input_array1[i]==input_array2[j])
            {
                flag=1;
                break;
            }
        }
        if(flag==1)
        {
            output_array[k]=input_array1[i];
            k++;
        }
    }
    *output_length = k;
}
/*
 * Process Link Establishment Request from SME .
 */
tSirRetStatus limProcesSmeTdlsLinkEstablishReq(tpAniSirGlobal pMac,
                                                           tANI_U32 *pMsgBuf)
{
    /* get all discovery request parameters */
    tSirTdlsLinkEstablishReq *pTdlsLinkEstablishReq = (tSirTdlsLinkEstablishReq*) pMsgBuf ;
    tpPESession psessionEntry;
    tANI_U8      sessionId;
    tpTdlsLinkEstablishParams pMsgTdlsLinkEstablishReq;
    tSirMsgQ msg;
    tANI_U16 peerIdx = 0 ;
    tpDphHashNode pStaDs = NULL ;

    limLog(pMac, LOG1, FL("Link Establish Request Recieved")) ;

    if((psessionEntry = peFindSessionByBssid(pMac, pTdlsLinkEstablishReq->bssid, &sessionId))
            == NULL)
    {
        limLog(pMac, LOGE,
               FL("PE Session does not exist for given sme sessionId %d"),
               pTdlsLinkEstablishReq->sessionId);
        limSendSmeTdlsLinkEstablishReqRsp(pMac, pTdlsLinkEstablishReq->sessionId, pTdlsLinkEstablishReq->peerMac,
             NULL, eSIR_FAILURE) ;
        return eSIR_FAILURE;
    }

    /* check if we are in proper state to work as TDLS client */
    if (psessionEntry->limSystemRole != eLIM_STA_ROLE)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR,
                "TDLS Link Establish Request received in wrong system Role %d",
                psessionEntry->limSystemRole);
        goto lim_tdls_link_establish_error;
    }

    /*
     * if we are still good, go ahead and check if we are in proper state to
     * do TDLS discovery req/rsp/....frames.
     */
    if ((psessionEntry->limSmeState != eLIM_SME_ASSOCIATED_STATE) &&
            (psessionEntry->limSmeState != eLIM_SME_LINK_EST_STATE))
    {

        limLog(pMac, LOGE,
               FL("TDLS Link Establish Request received in invalid LIMsme state (%d)"),
               psessionEntry->limSmeState);
        goto lim_tdls_link_establish_error;
    }
    /*TODO Sunil , TDLSPeer Entry has the STA ID , Use it */
    pStaDs = dphLookupHashEntry(pMac, pTdlsLinkEstablishReq->peerMac, &peerIdx,
                                &psessionEntry->dph.dphHashTable) ;
    if ( NULL == pStaDs )
    {
        limLog(pMac, LOGE, FL( "pStaDs is NULL"));
        goto lim_tdls_link_establish_error;

    }
    pMsgTdlsLinkEstablishReq = vos_mem_malloc(sizeof( tTdlsLinkEstablishParams ));
    if ( NULL == pMsgTdlsLinkEstablishReq )
    {
        limLog(pMac, LOGE,
               FL("Unable to allocate memory TDLS Link Establish Request"));
        return eSIR_MEM_ALLOC_FAILED;
    }

    vos_mem_set( (tANI_U8 *)pMsgTdlsLinkEstablishReq, sizeof(tTdlsLinkEstablishParams), 0);

    pMsgTdlsLinkEstablishReq->staIdx = pStaDs->staIndex;
    pMsgTdlsLinkEstablishReq->isResponder = pTdlsLinkEstablishReq->isResponder;
    pMsgTdlsLinkEstablishReq->uapsdQueues = pTdlsLinkEstablishReq->uapsdQueues;
    pMsgTdlsLinkEstablishReq->maxSp = pTdlsLinkEstablishReq->maxSp;
    pMsgTdlsLinkEstablishReq->isBufsta = pTdlsLinkEstablishReq->isBufSta;
    pMsgTdlsLinkEstablishReq->isOffChannelSupported =
                                pTdlsLinkEstablishReq->isOffChannelSupported;
    if (psessionEntry->tdlsChanSwitProhibited)
    {
        pMsgTdlsLinkEstablishReq->isOffChannelSupported = 3;
        limLog(pMac, LOG1, FL("Channel Switch Prohibited by AP"));
    }
    else
    {
        pMsgTdlsLinkEstablishReq->isOffChannelSupported = 1;
    }
    if ((pTdlsLinkEstablishReq->supportedChannelsLen > 0) &&
        (pTdlsLinkEstablishReq->supportedChannelsLen <= SIR_MAC_MAX_SUPP_CHANNELS))
    {
        tANI_U32   selfNumChans = WNI_CFG_VALID_CHANNEL_LIST_LEN;
        tANI_U8    selfSupportedChannels[WNI_CFG_VALID_CHANNEL_LIST_LEN];
        if (wlan_cfgGetStr(pMac, WNI_CFG_VALID_CHANNEL_LIST,
                          selfSupportedChannels, &selfNumChans) != eSIR_SUCCESS)
        {
            /**
             * Could not get Valid channel list from CFG.
             * Log error.
             */
             limLog(pMac, LOGE,
                    FL("could not retrieve Valid channel list"));
        }
        limTdlsGetIntersection(selfSupportedChannels, selfNumChans,
                               pTdlsLinkEstablishReq->supportedChannels,
                               pTdlsLinkEstablishReq->supportedChannelsLen,
                               pMsgTdlsLinkEstablishReq->validChannels,
                               &pMsgTdlsLinkEstablishReq->validChannelsLen);
    }
    vos_mem_copy(pMsgTdlsLinkEstablishReq->validOperClasses,
                        pTdlsLinkEstablishReq->supportedOperClasses, pTdlsLinkEstablishReq->supportedOperClassesLen);
    pMsgTdlsLinkEstablishReq->validOperClassesLen =
                                pTdlsLinkEstablishReq->supportedOperClassesLen;

    msg.type = WDA_SET_TDLS_LINK_ESTABLISH_REQ;
    msg.reserved = 0;
    msg.bodyptr = pMsgTdlsLinkEstablishReq;
    msg.bodyval = 0;
    if(eSIR_SUCCESS != wdaPostCtrlMsg(pMac, &msg))
    {
        limLog(pMac, LOGE, FL("halPostMsgApi failed"));
        goto lim_tdls_link_establish_error;
    }
    return eSIR_SUCCESS;
lim_tdls_link_establish_error:
     limSendSmeTdlsLinkEstablishReqRsp(pMac, psessionEntry->smeSessionId, pTdlsLinkEstablishReq->peerMac,
                                       NULL, eSIR_FAILURE) ;

    return eSIR_SUCCESS;
}


/* Delete all the TDLS peer connected before leaving the BSS */
tSirRetStatus limDeleteTDLSPeers(tpAniSirGlobal pMac, tpPESession psessionEntry)
{
    tpDphHashNode pStaDs = NULL ;
    int i, aid;

    if (NULL == psessionEntry)
    {
        limLog(pMac, LOGE, FL("NULL psessionEntry"));
        return eSIR_FAILURE;
    }

    /* Check all the set bit in peerAIDBitmap and delete the peer (with that aid) entry
       from the hash table and add the aid in free pool */
    for (i = 0; i < sizeof(psessionEntry->peerAIDBitmap)/sizeof(tANI_U32); i++)
    {
        for (aid = 0; aid < (sizeof(tANI_U32) << 3); aid++)
        {
            if (CHECK_BIT(psessionEntry->peerAIDBitmap[i], aid))
            {
                pStaDs = dphGetHashEntry(pMac, (aid + i*(sizeof(tANI_U32) << 3)), &psessionEntry->dph.dphHashTable);

                if (NULL != pStaDs)
                {
                    limLog(pMac, LOGE, FL("Deleting "MAC_ADDRESS_STR),
                                       MAC_ADDR_ARRAY(pStaDs->staAddr));

                    limSendDeauthMgmtFrame(pMac, eSIR_MAC_DEAUTH_LEAVING_BSS_REASON,
                                           pStaDs->staAddr, psessionEntry, FALSE);
                    dphDeleteHashEntry(pMac, pStaDs->staAddr, pStaDs->assocId, &psessionEntry->dph.dphHashTable);
                }
                limReleasePeerIdx(pMac, (aid + i*(sizeof(tANI_U32) << 3)), psessionEntry) ;
                CLEAR_BIT(psessionEntry->peerAIDBitmap[i], aid);
            }
        }
    }
    limSendSmeTDLSDeleteAllPeerInd(pMac, psessionEntry);

    return eSIR_SUCCESS;
}


tANI_U8 limGetOPClassFromChannel(tANI_U8 *country,
                                         tANI_U8 channel,
                                         tANI_U8 offset)
{
    op_class_map_t *class = NULL;
    tANI_U16 i = 0;

    if (VOS_TRUE == vos_mem_compare(country,"US", 2))  {

        class = us_op_class;

    } else if (VOS_TRUE == vos_mem_compare(country,"EU", 2)) {

        class = euro_op_class;

    } else if (VOS_TRUE == vos_mem_compare(country,"JP", 2)) {

        class = japan_op_class;

    } else {

        class = global_op_class;

    }

    while (class->op_class)
    {
        if ((offset == class->offset) || (offset == BWALL))
        {
            for (i=0; (i < 15 && class->channels[i]); i++)
            {
                if (channel == class->channels[i])
                    return class->op_class;
            }
        }
        class++;
    }
    return 0;
}

tANI_BOOLEAN  CheckAndAddOP(tANI_U8 class)
{
    tANI_U8 i;

    for (i=0; i < (SIR_MAC_MAX_SUPP_OPER_CLASSES - 1); i++)
    {
        /*0 is an invalid class. If class is already present ignore*/
        if (class == op_classes.classes[i])
            return FALSE;
        if(op_classes.classes[i] == 0)
        {
            return TRUE;
        }
    }
    //limLog(pMac, LOGE, FL("No space left for class = %d"), class);
    return FALSE;
}

void limInitOperatingClasses( tHalHandle hHal )
{

    tANI_U8 Index = 0;
    tANI_U8 class = 0;
    tANI_U8 i = 0;
    tANI_U8 j = 0;
    tANI_U8 swap = 0;
    tANI_U8 numChannels = 0;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    limLog(pMac, LOG1, FL("Current Country = %c%c"),
                          pMac->scan.countryCodeCurrent[0],
                          pMac->scan.countryCodeCurrent[1]);

    vos_mem_set(op_classes.classes, sizeof(op_classes.classes), 0);
    numChannels = pMac->scan.baseChannels.numChannels;
    limLog(pMac, LOG1, "Num of base ch =%d", numChannels);
    for ( Index = 0;
          Index < numChannels && i < (SIR_MAC_MAX_SUPP_OPER_CLASSES - 1);
          Index++)
    {
        class = limGetOPClassFromChannel(
                            pMac->scan.countryCodeCurrent,
                            pMac->scan.baseChannels.channelList[ Index ],
                            BWALL);
        limLog(pMac, LOG4, "ch=%d <=> %d=class",
               pMac->scan.baseChannels.channelList[ Index ],
               class);
        if (CheckAndAddOP(class))
        {
            op_classes.classes[i]= class;
            i++;
        }
    }

    numChannels = pMac->scan.base20MHzChannels.numChannels;
    limLog(pMac, LOG1, "Num of 20MHz ch =%d", numChannels);
    for ( Index = 0;
          Index < numChannels && i < (SIR_MAC_MAX_SUPP_OPER_CLASSES - 1);
          Index++)
    {
        class = limGetOPClassFromChannel(
                            pMac->scan.countryCodeCurrent,
                            pMac->scan.base20MHzChannels.channelList[ Index ],
                            BWALL);
        limLog(pMac, LOG4, "ch=%d <=> %d=class",
               pMac->scan.base20MHzChannels.channelList[ Index ],
               class);
        if (CheckAndAddOP(class))
        {
            op_classes.classes[i]= class;
            i++;
        }
    }

    numChannels = pMac->scan.base40MHzChannels.numChannels;
    limLog(pMac, LOG1, "Num of 40MHz ch =%d", numChannels);
    for ( Index = 0;
          Index < numChannels && i < (SIR_MAC_MAX_SUPP_OPER_CLASSES - 1);
          Index++)
    {
        class = limGetOPClassFromChannel(
                            pMac->scan.countryCodeCurrent,
                            pMac->scan.base40MHzChannels.channelList[ Index ],
                            BWALL);
        limLog(pMac, LOG4, "ch=%d <=> %d=class",
               pMac->scan.base40MHzChannels.channelList[ Index ],
               class);
        if (CheckAndAddOP(class))
        {
            op_classes.classes[i]= class;
            i++;
        }
    }

    op_classes.num_classes = i;
    limLog(pMac, LOG1, "Total number of Unique supported classes =%d",
           op_classes.num_classes);
    /*as per spec the operating classes should be in ascending order*/
    /*Bubble sort is fine as we don't have many classes*/
    for (i = 0 ; i < ( op_classes.num_classes - 1 ); i++)
    {
        for (j = 0 ; j < op_classes.num_classes - i - 1; j++)
        {
            /* For decreasing order use < */
            if (op_classes.classes[j] > op_classes.classes[j+1])
            {
                swap = op_classes.classes[j];
                op_classes.classes[j] = op_classes.classes[j+1];
                op_classes.classes[j+1] = swap;
            }
        }
    }
    for (i=0; i < op_classes.num_classes; i++)
    {

        limLog(pMac, LOG1, "supported op_class[%d]=%d", i,
               op_classes.classes[i]);

    }
}

#endif
// tdlsoffchan
/*
 * Process Channel Switch from SME.
 */
tSirRetStatus limProcesSmeTdlsChanSwitchReq(tpAniSirGlobal pMac,
                                            tANI_U32 *pMsgBuf)
{
    /* get all discovery request parameters */
    tSirTdlsChanSwitch *pTdlsChanSwitch = (tSirTdlsChanSwitch*) pMsgBuf ;
    tpPESession            psessionEntry;
    tANI_U8                sessionId;
    tpTdlsChanSwitchParams pMsgTdlsChanSwitch;
    tSirMsgQ               msg;
    tANI_U16               peerIdx = 0;
    tpDphHashNode          pStaDs = NULL;

    VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO,
             ("TDLS Channel Switch Recieved on peer:" MAC_ADDRESS_STR),
              MAC_ADDR_ARRAY(pTdlsChanSwitch->peerMac));

    psessionEntry = peFindSessionByBssid(pMac,
                                         pTdlsChanSwitch->bssid,
                                         &sessionId);
    if (psessionEntry == NULL)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR,
                  "PE Session does not exist for given sme sessionId %d",
                  pTdlsChanSwitch->sessionId);
        limSendSmeTdlsChanSwitchReqRsp(pMac, pTdlsChanSwitch->sessionId,
                                       pTdlsChanSwitch->peerMac,
                                       NULL, eSIR_FAILURE) ;
        return eSIR_FAILURE;
    }

    /* check if we are in proper state to work as TDLS client */
    if (psessionEntry->limSystemRole != eLIM_STA_ROLE)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR,
                  "TDLS Channel Switch received in wrong system Role %d",
                  psessionEntry->limSystemRole);
        goto lim_tdls_chan_switch_error;
    }

    /*
     * if we are still good, go ahead and check if we are in proper state to
     * do TDLS discovery req/rsp/....frames.
     */
    if ((psessionEntry->limSmeState != eLIM_SME_ASSOCIATED_STATE) &&
            (psessionEntry->limSmeState != eLIM_SME_LINK_EST_STATE))
    {

        limLog(pMac, LOGE, "TDLS Channel Switch received in invalid LIMsme state (%d)",
               psessionEntry->limSmeState);
        goto lim_tdls_chan_switch_error;
    }

    pStaDs = dphLookupHashEntry(pMac, pTdlsChanSwitch->peerMac, &peerIdx,
                                &psessionEntry->dph.dphHashTable) ;
    if ( NULL == pStaDs )
    {
        limLog( pMac, LOGE, FL( "pStaDs is NULL" ));
        goto lim_tdls_chan_switch_error;

    }
    pMsgTdlsChanSwitch = vos_mem_malloc(sizeof( tTdlsChanSwitchParams ));
    if ( NULL == pMsgTdlsChanSwitch )
    {
        limLog( pMac, LOGE,
                     FL( "Unable to allocate memory TDLS Channel Switch" ));
        return eSIR_MEM_ALLOC_FAILED;
    }

    vos_mem_set( (tANI_U8 *)pMsgTdlsChanSwitch, sizeof(tpTdlsChanSwitchParams), 0);

    pMsgTdlsChanSwitch->staIdx = pStaDs->staIndex;
    pMsgTdlsChanSwitch->tdlsOffCh = pTdlsChanSwitch->tdlsOffCh;
    pMsgTdlsChanSwitch->tdlsOffChBwOffset = pTdlsChanSwitch->tdlsOffChBwOffset;
    pMsgTdlsChanSwitch->tdlsSwMode = pTdlsChanSwitch->tdlsSwMode;
    pMsgTdlsChanSwitch->operClass = limGetOPClassFromChannel(
                                           pMac->scan.countryCodeCurrent,
                                           pTdlsChanSwitch->tdlsOffCh,
                                           pTdlsChanSwitch->tdlsOffChBwOffset);
    if(pMsgTdlsChanSwitch->operClass == 0)
    {

        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR,
                                   "Invalid Operating class 0 !!!");
        vos_mem_free(pMsgTdlsChanSwitch);
        goto lim_tdls_chan_switch_error;
    }
    else
    {

        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO,
              "%s: TDLS Channel Switch params: staIdx %d class %d ch %d bw %d mode %d",
               __func__,
               pMsgTdlsChanSwitch->staIdx,
               pMsgTdlsChanSwitch->operClass,
               pMsgTdlsChanSwitch->tdlsOffCh,
               pMsgTdlsChanSwitch->tdlsOffChBwOffset,
               pMsgTdlsChanSwitch->tdlsSwMode);
    }

    msg.type = WDA_SET_TDLS_CHAN_SWITCH_REQ;
    msg.reserved = 0;
    msg.bodyptr = pMsgTdlsChanSwitch;
    msg.bodyval = 0;
    if(eSIR_SUCCESS != wdaPostCtrlMsg(pMac, &msg))
    {
        limLog(pMac, LOGE, FL("halPostMsgApi failed\n"));
        vos_mem_free(pMsgTdlsChanSwitch);
        goto lim_tdls_chan_switch_error;
    }

    return eSIR_SUCCESS;

lim_tdls_chan_switch_error:
    limSendSmeTdlsChanSwitchReqRsp(pMac, pTdlsChanSwitch->sessionId,
                                   pTdlsChanSwitch->peerMac,
                                   NULL, eSIR_FAILURE);
    return eSIR_FAILURE;
}

