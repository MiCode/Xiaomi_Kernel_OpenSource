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

/**=========================================================================
 *     
 *       \file  wlan_qct_wdi_dts.c
 *          
 *       \brief  Data Transport Service API 
 *                               
 * WLAN Device Abstraction layer External API for Dataservice
 * DESCRIPTION
 *  This file contains the external API implemntation exposed by the 
 *   wlan device abstarction layer module.
 *
 *   Copyright (c) 2008 QUALCOMM Incorporated. All Rights Reserved.
 *   Qualcomm Confidential and Proprietary
 */


#include "wlan_qct_wdi.h"
#include "wlan_qct_dxe.h"
#include "wlan_qct_wdi_ds.h"
#include "wlan_qct_wdi_ds_i.h"
#include "wlan_qct_wdi_dts.h"
#include "wlan_qct_wdi_dp.h"
#include "wlan_qct_wdi_sta.h"

#ifdef DEBUG_ROAM_DELAY
#include "vos_utils.h"
#endif

static WDTS_TransportDriverTrype gTransportDriver = {
  WLANDXE_Open, 
  WLANDXE_Start, 
  WLANDXE_ClientRegistration, 
  WLANDXE_TxFrame,
  WLANDXE_CompleteTX,
  WLANDXE_SetPowerState,
  WLANDXE_ChannelDebug,
  WLANDXE_Stop,
  WLANDXE_Close,
  WLANDXE_GetFreeTxDataResNumber
};

static WDTS_SetPowerStateCbInfoType gSetPowerStateCbInfo;

typedef struct 
{
   uint32 phyRate;   //unit in Mega bits per sec X 10
   uint32 tputRate;  //unit in Mega bits per sec X 10
   uint32 tputBpms;  //unit in Bytes per msec = (tputRateX1024x1024)/(8x10X1000) ~= (tputRate*13)
   uint32 tputBpus;  //unit in Bytes per usec: round off to integral value
}WDTS_RateInfo;

#define WDTS_MAX_RATE_NUM               137
#define WDTS_MAX_11B_RATE_NUM           8
#define MB_PER_SEC_TO_BYTES_PER_MSEC    13

WDTS_RateInfo g11bRateInfo[WDTS_MAX_11B_RATE_NUM]  = {
    //11b rates
    {  10,  9,  117, 8}, //index 0
    {  20,  17, 221, 5}, //index 1
    {  55,  41, 533, 2}, //index 2
    { 110,  68, 884, 1}, //index 3

    //11b short preamble
    {  10,  10,  130, 8}, //index 4
    {  20,  18,  234, 5}, //index 5
    {  55,  44,  572, 2}, //index 6
    { 110,  77, 1001, 1}, //index 7
};

WDTS_RateInfo gRateInfo[WDTS_MAX_RATE_NUM]  = {
    //11b rates
    {  10,  9,  117, 0}, //index 0
    {  20,  17, 221, 0}, //index 1
    {  55,  41, 533, 0}, //index 2
    { 110,  68, 884, 0}, //index 3

    //11b short preamble
    {  10,  10,  130, 0}, //index 4
    {  20,  18,  234, 0}, //index 5
    {  55,  44,  572, 0}, //index 6
    { 110,  77, 1001, 0}, //index 7

    //11ag
    {  60,  50,  650, 1}, //index 8
    {  90,  70,  910, 1}, //index 9
    { 120, 100, 1300, 1}, //index 10
    { 180, 150, 1950, 2}, //index 11
    { 240, 190, 2470, 2}, //index 12
    { 360, 280, 3640, 4}, //index 13
    { 480, 350, 4550, 5}, //index 14
    { 540, 380, 4940, 6}, //index 15

    //11n SIMO
    {  65,  54,  702, 1}, //index 16
    { 130, 108, 1404, 1}, //index 17
    { 195, 161, 2093, 2}, //index 18
    { 260, 217, 2821, 3}, //index 19
    { 390, 326, 4238, 4}, //index 20
    { 520, 435, 5655, 6}, //index 21
    { 585, 492, 6396, 6}, //index 22
    { 650, 548, 7124, 7}, //index 23

    //11n SIMO SGI
    {  72,  59,  767, 1}, //index 24
    { 144, 118, 1534, 2}, //index 25
    { 217, 180, 2340, 2}, //index 26
    { 289, 243, 3159, 3}, //index 27
    { 434, 363, 4719, 5}, //index 28
    { 578, 486, 6318, 6}, //index 29
    { 650, 548, 7124, 7}, //index 30
    { 722, 606, 7878, 8}, //index 31

    //11n GF SIMO
    {  65,  54,  702, 1}, //index 32
    { 130, 108, 1404, 1}, //index 33
    { 195, 161, 2093, 2}, //index 34
    { 260, 217, 2821, 3}, //index 35
    { 390, 326, 4238, 4}, //index 36
    { 520, 435, 5655, 6}, //index 37
    { 585, 492, 6396, 6}, //index 38
    { 650, 548, 7124, 7}, //index 39

    //11n SIMO CB MCS 0 - 7 
    { 135,   110,  1430,  1}, //index 40
    { 270,   223,  2899,  3}, //index 41
    { 405,   337,  4381,  4}, //index 42
    { 540,   454,  5902,  6}, //index 43
    { 810,   679,  8827,  9}, //index 44
    { 1080,  909, 11817, 12}, //index 45
    { 1215, 1022, 13286, 13}, //index 46
    { 1350, 1137, 14781, 15}, //index 47

    //11n SIMO CB SGI MCS 0 - 7
    { 150,   121,  1573,  2}, //index 48
    { 300,   249,  3237,  3}, //index 49
    { 450,   378,  4914,  5}, //index 50
    { 600,   503,  6539,  7}, //index 51
    { 900,   758,  9854,  10}, //index 52
    { 1200, 1010, 13130, 13}, //index 53
    { 1350, 1137, 14781, 15}, //index 54
    { 1500, 1262, 16406, 16}, //index 55

    //11n SIMO GF CB MCS 0 - 7 
    { 135,   110,   1430,  1}, //index 56
    { 270,   223,   2899,  3}, //index 57
    { 405,   337,   4381,  4}, //index 58
    { 540,   454,   5902,  6}, //index 59
    { 810,   679,   8827,  9}, //index 60
    { 1080,  909,  11817, 12}, //index 61
    { 1215, 1022,  13286, 13}, //index 62
    { 1350, 1137,  14781, 15}, //index 63

    //11AC  
    { 1350,  675,  8775,  9}, //reserved 64
    { 1350,  675,  8775,  9}, //reserved 65
    {   65,   45,   585,  1}, //index 66
    {  130,   91,  1183,  1}, //index 67
    {  195,  136,  1768,  2}, //index 68
    {  260,  182,  2366,  2}, //index 69
    {  390,  273,  3549,  4}, //index 70
    {  520,  364,  4732,  5}, //index 71
    {  585,  409,  5317,  5}, //index 72
    {  650,  455,  5915,  6}, //index 73
    {  780,  546,  7098,  7}, //index 74
    { 1350,  675,  8775,  9}, //reserved 75
    { 1350,  675,  8775,  9}, //reserved 76
    { 1350,  675,  8775,  9}, //reserved 77
    { 1350,  675,  8775,  9}, //index 78
    { 1350,  675,  8775,  9}, //index 79
    { 1350,  675,  8775,  9}, //index 80
    { 1350,  675,  8775,  9}, //index 81
    { 1350,  675,  8775,  9}, //index 82
    { 1350,  675,  8775,  9}, //index 83
    {  655,  458,  5954,  6}, //index 84
    {  722,  505,  6565,  7}, //index 85
    {  866,  606,  7878,  8}, //index 86
    { 1350,  675,  8775,  9}, //reserved 87
    { 1350,  675,  8775,  9}, //reserved 88
    { 1350,  675,  8775,  9}, //reserved 89
    {  135,   94,  1222,  1}, //index 90
    {  270,  189,  2457,  2}, //index 91
    {  405,  283,  3679,  4}, //index 92
    {  540,  378,  4914,  5}, //index 93
    {  810,  567,  7371,  7}, //index 94
    { 1080,  756,  9828, 10}, //index 95
    { 1215,  850, 11050, 11}, //index 96
    { 1350,  675,  8775,  9}, //index 97
    { 1350,  675,  8775,  9}, //index 98
    { 1620,  810, 10530, 11}, //index 99
    { 1800,  900, 11700, 12}, //index 100
    { 1350,  675,  8775,  9}, //reserved 101
    { 1350,  675,  8775,  9}, //index 102
    { 1350,  675,  8775,  9}, //index 103
    { 1350,  675,  8775,  9}, //index 104
    { 1350,  675,  8775,  9}, //index 105
    { 1350,  675,  8775,  9}, //index 106
    { 1200,  840, 10920, 11}, //index 107
    { 1350,  675,  8775,  9}, //index 108
    { 1500,  750,  9750, 10}, //index 109
    { 1350,  675,  8775,  9}, //index 110
    { 1800,  900, 11700, 12}, //index 111
    { 2000, 1000, 13000, 13}, //index 112
    { 1350,  675,  8775,  9}, //index 113
    {  292,  204,  2652,  3}, //index 114
    {  585,  409,  5317,  5}, //index 115
    {  877,  613,  7969,  8}, //index 116
    { 1170,  819, 10647, 11}, //index 117
    { 1755,  877, 11401, 11}, //index 118
    { 2340, 1170, 15210, 15}, //index 119
    { 2632, 1316, 17108, 17}, //index 120
    { 2925, 1462, 19006, 19}, //index 121
    { 1350,  675,  8775,  9}, //index 122
    { 3510, 1755, 22815, 23}, //index 123
    { 3900, 1950, 25350, 25}, //index 124
    { 1350,  675,  8775,  9}, //reserved 125
    { 1350,  675,  8775,  9}, //index 126
    { 1350,  675,  8775,  9}, //index 127
    { 1350,  675,  8775,  9}, //index 128
    { 1350,  675,  8775,  9}, //index 129
    { 1350,  675,  8775,  9}, //index 130
    { 1350,  675,  8775,  9}, //index 131
    { 2925, 1462, 19006, 19}, //index 132
    { 3250, 1625, 21125, 21}, //index 133
    { 1350,  675,  8775,  9}, //index 134
    { 3900, 1950, 25350, 25}, //index 135
    { 4333, 2166, 28158, 28}  //index 136
 };

/* TX stats */
typedef struct
{
  wpt_uint32 txBytesPushed;
  wpt_uint32 txPacketsPushed; //Can be removed to optimize memory
}WDI_DTS_TX_TrafficStatsType;

/* RX stats */
typedef struct
{
  wpt_uint32 rxBytesRcvd;
  wpt_uint32 rxPacketsRcvd;  //Can be removed to optimize memory
}WDI_DTS_RX_TrafficStatsType;

typedef struct {
   wpt_uint8 running;
   WDI_DTS_RX_TrafficStatsType rxStats[HAL_NUM_STA][WDTS_MAX_RATE_NUM];
   WDI_DTS_TX_TrafficStatsType txStats[HAL_NUM_STA];
   WDI_TrafficStatsType        netTxRxStats[HAL_NUM_STA];
}WDI_DTS_TrafficStatsType;

static WDI_DTS_TrafficStatsType gDsTrafficStats;

#define DTS_RATE_TPUT(x) gRateInfo[x].tputBpus
#define DTS_11BRATE_TPUT_MULTIPLIER(x) g11bRateInfo[x].tputBpus

/* RX thread frame size threshold to delay frame drain */
#define DTS_RX_DELAY_FRAMESIZE_THRESHOLD  500

/* API to fill Rate Info based on the mac efficiency passed to it
 * macEff si used to caclulate mac throughput based on each rate index/PHY rate.
 * This is eventually used by MAS to calculate RX stats periodically sent to FW
 * The start and end Rate Index are the other arguments to this API - the new mac
 * efficiency passed to this API (Arg1)  is only applied between startRateIndex (arg2) and endRateIndex (arg3).
 */
void WDTS_FillRateInfo(wpt_uint8 macEff, wpt_int16 startRateIndex, wpt_int16 endRateIndex)
{
    int i;

    DTI_TRACE( DTI_TRACE_LEVEL_ERROR, "Change only 11ac rates");

    for (i=startRateIndex; i<=endRateIndex; i++)
    {
        // tputRate --> unit in Mega bits per sec X 10
        gRateInfo[i].tputRate = ((gRateInfo[i].phyRate * macEff)/100);
        // tputBmps --> unit in Bytes per msec = (tputRateX1024x1024)/(8x10X1000) ~= (tputRate*13)
        gRateInfo[i].tputBpms = gRateInfo[i].tputRate * MB_PER_SEC_TO_BYTES_PER_MSEC;
        // tputBpus --> unit in Bytes per usec: (+ 500) to round off to integral value
        gRateInfo[i].tputBpus = ((gRateInfo[i].tputBpms + 500) / 1000);
        if (gRateInfo[i].tputBpus == 0)
            gRateInfo[i].tputBpus = 1;

        DTI_TRACE( DTI_TRACE_LEVEL_ERROR, "%4u, %4u, %5u, %2u",
                            gRateInfo[i].phyRate,
                            gRateInfo[i].tputRate,
                            gRateInfo[i].tputBpms,
                            gRateInfo[i].tputBpus );
    }
}

/* Tx/Rx stats function
 * This function should be invoked to fetch the current stats
  * Parameters:
 *  pStats:Pointer to the collected stats
 *  len: length of buffer pointed to by pStats
 *  Return Status: None
 */
void WDTS_GetTrafficStats(WDI_TrafficStatsType** pStats, wpt_uint32 *len)
{
   if(gDsTrafficStats.running)
   {
      uint8 staIdx, rate;
      WDI_TrafficStatsType *pNetTxRxStats = gDsTrafficStats.netTxRxStats;
      wpalMemoryZero(pNetTxRxStats, sizeof(gDsTrafficStats.netTxRxStats));

      for(staIdx = 0; staIdx < HAL_NUM_STA; staIdx++, pNetTxRxStats++)
      {
          pNetTxRxStats->txBytesPushed += gDsTrafficStats.txStats[staIdx].txBytesPushed;
          pNetTxRxStats->txPacketsPushed+= gDsTrafficStats.txStats[staIdx].txPacketsPushed;
          for(rate = 0; rate < WDTS_MAX_11B_RATE_NUM; rate++)
          {
             pNetTxRxStats->rxBytesRcvd +=
               gDsTrafficStats.rxStats[staIdx][rate].rxBytesRcvd;
             pNetTxRxStats->rxPacketsRcvd +=
               gDsTrafficStats.rxStats[staIdx][rate].rxPacketsRcvd;
             pNetTxRxStats->rxTimeTotal +=
               gDsTrafficStats.rxStats[staIdx][rate].rxBytesRcvd*DTS_11BRATE_TPUT_MULTIPLIER(rate);
          }
          for(rate = WDTS_MAX_11B_RATE_NUM; rate < WDTS_MAX_RATE_NUM; rate++)
          {
             pNetTxRxStats->rxBytesRcvd += 
               gDsTrafficStats.rxStats[staIdx][rate].rxBytesRcvd;
             pNetTxRxStats->rxPacketsRcvd += 
               gDsTrafficStats.rxStats[staIdx][rate].rxPacketsRcvd;
             pNetTxRxStats->rxTimeTotal += 
               gDsTrafficStats.rxStats[staIdx][rate].rxBytesRcvd/DTS_RATE_TPUT(rate);
          }

          pNetTxRxStats->rxTimeTotal = pNetTxRxStats->rxTimeTotal/1000;

      }
      *pStats = gDsTrafficStats.netTxRxStats;
      *len = sizeof(gDsTrafficStats.netTxRxStats);
   }
   else
   {
      *pStats = NULL;
      *len = 0;
   }
}

/* WDTS_DeactivateTrafficStats
 * This function should be invoked to deactivate traffic stats collection
  * Parameters: None
 *  Return Status: None
 */
void WDTS_DeactivateTrafficStats(void)
{
   gDsTrafficStats.running = eWLAN_PAL_FALSE;
}

/* WDTS_ActivateTrafficStats
 * This function should be invoked to activate traffic stats collection
  * Parameters: None
 *  Return Status: None
 */
void WDTS_ActivateTrafficStats(void)
{
   gDsTrafficStats.running = eWLAN_PAL_TRUE;
}

/* WDTS_ClearTrafficStats
 * This function should be invoked to clear traffic stats 
  * Parameters: None
 *  Return Status: None
 */
void WDTS_ClearTrafficStats(void)
{
   wpalMemoryZero(gDsTrafficStats.rxStats, sizeof(gDsTrafficStats.rxStats));
   wpalMemoryZero(gDsTrafficStats.txStats, sizeof(gDsTrafficStats.txStats));
}

/* DTS Tx packet complete function. 
 * This function should be invoked by the transport device to indicate 
 * transmit complete for a frame.
 * Parameters:
 * pContext:Cookie that should be passed back to the caller 
 * pFrame:Refernce to PAL frame.
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
wpt_status WDTS_TxPacketComplete(void *pContext, wpt_packet *pFrame, wpt_status status)
{
  WDI_DS_ClientDataType *pClientData = (WDI_DS_ClientDataType*)(pContext);
  WDI_DS_TxMetaInfoType     *pTxMetadata;
  void *pvBDHeader, *physBDHeader;
  wpt_uint8 staIndex;

  // Do Sanity checks
  if(NULL == pContext || NULL == pFrame){
    return eWLAN_PAL_STATUS_E_FAILURE;
  }


  // extract metadata from PAL packet
  pTxMetadata = WDI_DS_ExtractTxMetaData(pFrame);
  pTxMetadata->txCompleteStatus = status;

  // Free BD header from pool
  WDI_GetBDPointers(pFrame, &pvBDHeader,  &physBDHeader);
  switch(pTxMetadata->frmType) 
  {
    case WDI_MAC_DATA_FRAME:
    /* note that EAPOL frame hasn't incremented ReserveCount. see
       WDI_DS_TxPacket() in wlan_qct_wdi_ds.c
    */
#ifdef FEATURE_WLAN_TDLS
    /* I utilizes TDLS mgmt frame always sent at BD_RATE2. (See limProcessTdls.c)
       Assumption here is data frame sent by WDA_TxPacket() <- HalTxFrame/HalTxFrameWithComplete()
       should take managment path. As of today, only TDLS feature has special data frame
       which needs to be treated as mgmt.
    */
    if((!pTxMetadata->isEapol) &&
       ((pTxMetadata->txFlags & WDI_USE_BD_RATE2_FOR_MANAGEMENT_FRAME) != WDI_USE_BD_RATE2_FOR_MANAGEMENT_FRAME))
#else
    if(!pTxMetadata->isEapol)
#endif
    {
      /* SWAP BD header to get STA index for completed frame */
      WDI_SwapTxBd(pvBDHeader);
      staIndex = (wpt_uint8)WDI_TX_BD_GET_STA_ID(pvBDHeader);
      WDI_DS_MemPoolFree(&(pClientData->dataMemPool), pvBDHeader, physBDHeader);
      WDI_DS_MemPoolDecreaseReserveCount(&(pClientData->dataMemPool), staIndex);
      break;
    }
    // intentional fall-through to handle eapol packet as mgmt
    case WDI_MAC_MGMT_FRAME:
      WDI_DS_MemPoolFree(&(pClientData->mgmtMemPool), pvBDHeader, physBDHeader);
      break;
  }
  WDI_SetBDPointers(pFrame, 0, 0);

  // Invoke Tx complete callback
  pClientData->txCompleteCB(pClientData->pCallbackContext, pFrame);  
  return eWLAN_PAL_STATUS_SUCCESS;

}


/*===============================================================================
  FUNCTION      WLANTL_GetReplayCounterFromRxBD
     
  DESCRIPTION   This function extracts 48-bit replay packet number from RX BD 
 
  DEPENDENCIES  Validity of replay check must be done before the function 
                is called
                          
  PARAMETERS    pucRxHeader pointer to RX BD header
                                       
  RETRUN        v_U64_t    Packet number extarcted from RX BD

  SIDE EFFECTS   none
 ===============================================================================*/
v_U64_t
WDTS_GetReplayCounterFromRxBD
(
   v_U8_t *pucRxBDHeader
)
{
  v_U64_t ullcurrentReplayCounter = 0;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* 48-bit replay counter is created as follows
   from RX BD 6 byte PMI command:
   Addr : AES/TKIP
   0x38 : pn3/tsc3
   0x39 : pn2/tsc2
   0x3a : pn1/tsc1
   0x3b : pn0/tsc0

   0x3c : pn5/tsc5
   0x3d : pn4/tsc4 */
  
#ifdef ANI_BIG_BYTE_ENDIAN
    /* Getting 48-bit replay counter from the RX BD */
    ullcurrentReplayCounter = WDI_RX_BD_GET_PMICMD_20TO23(pucRxBDHeader); 
    ullcurrentReplayCounter <<= 16;
    ullcurrentReplayCounter |= (( WDI_RX_BD_GET_PMICMD_24TO25(pucRxBDHeader) & 0xFFFF0000) >> 16);
    return ullcurrentReplayCounter;
#else
    /* Getting 48-bit replay counter from the RX BD */
    ullcurrentReplayCounter = (WDI_RX_BD_GET_PMICMD_24TO25(pucRxBDHeader) & 0x0000FFFF); 
    ullcurrentReplayCounter <<= 32; 
    ullcurrentReplayCounter |= WDI_RX_BD_GET_PMICMD_20TO23(pucRxBDHeader); 
    return ullcurrentReplayCounter;
#endif
}


/* DTS Rx packet function. 
 * This function should be invoked by the transport device to indicate 
 * reception of a frame.
 * Parameters:
 * pContext:Cookie that should be passed back to the caller 
 * pFrame:Refernce to PAL frame.
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
wpt_status WDTS_RxPacket (void *pContext, wpt_packet *pFrame, WDTS_ChannelType channel)
{
  WDI_DS_ClientDataType *pClientData = 
    (WDI_DS_ClientDataType*)(pContext);
  wpt_boolean       bASF, bFSF, bLSF, bAEF;
  wpt_uint8                   ucMPDUHOffset, ucMPDUHLen, ucTid;
  wpt_uint8                   *pBDHeader;
  wpt_uint16                  usMPDUDOffset, usMPDULen;
  WDI_DS_RxMetaInfoType     *pRxMetadata;
  wpt_uint8                  isFcBd = 0;

  tpSirMacFrameCtl  pMacFrameCtl;
  // Do Sanity checks
  if(NULL == pContext || NULL == pFrame){
    return eWLAN_PAL_STATUS_E_FAILURE;
  }

  /*------------------------------------------------------------------------
    Extract BD header and check if valid
    ------------------------------------------------------------------------*/
  pBDHeader = (wpt_uint8*)wpalPacketGetRawBuf(pFrame);
  if(NULL == pBDHeader)
  {
    DTI_TRACE( DTI_TRACE_LEVEL_ERROR,
       "WLAN TL:BD header received NULL - dropping packet");
    wpalPacketFree(pFrame);
    return eWLAN_PAL_STATUS_E_FAILURE;
  }
  WDI_SwapRxBd(pBDHeader);

  ucMPDUHOffset = (wpt_uint8)WDI_RX_BD_GET_MPDU_H_OFFSET(pBDHeader);
  usMPDUDOffset = (wpt_uint16)WDI_RX_BD_GET_MPDU_D_OFFSET(pBDHeader);
  usMPDULen     = (wpt_uint16)WDI_RX_BD_GET_MPDU_LEN(pBDHeader);
  ucMPDUHLen    = (wpt_uint8)WDI_RX_BD_GET_MPDU_H_LEN(pBDHeader);
  ucTid         = (wpt_uint8)WDI_RX_BD_GET_TID(pBDHeader);

  /* If RX thread drain small size of frame from HW too fast
   * Sometimes HW cannot handle interrupt fast enough
   * And system crash might happen
   * To avoid system crash, input 1usec delay each frame draining
   * within host side, if frame size is smaller that threshold.
   * This is SW work around, to fix HW problem
   * Throughput and SnS test done successfully */
  if (usMPDULen < DTS_RX_DELAY_FRAMESIZE_THRESHOLD)
  {
    wpalBusyWait(1);
  }

  /*------------------------------------------------------------------------
    Gather AMSDU information 
    ------------------------------------------------------------------------*/
  bASF = WDI_RX_BD_GET_ASF(pBDHeader);
  bAEF = WDI_RX_BD_GET_AEF(pBDHeader);
  bFSF = WDI_RX_BD_GET_ESF(pBDHeader);
  bLSF = WDI_RX_BD_GET_LSF(pBDHeader);
  isFcBd = WDI_RX_FC_BD_GET_FC(pBDHeader);

  DTI_TRACE( DTI_TRACE_LEVEL_INFO,
      "WLAN TL:BD header processing data: HO %d DO %d Len %d HLen %d"
      " Tid %d BD %d",
      ucMPDUHOffset, usMPDUDOffset, usMPDULen, ucMPDUHLen, ucTid,
      WDI_RX_BD_HEADER_SIZE);

  if(!isFcBd)
  {
      if(usMPDUDOffset <= ucMPDUHOffset || usMPDULen < ucMPDUHLen) {
        DTI_TRACE( DTI_TRACE_LEVEL_ERROR,
            "WLAN TL:BD header corrupted - dropping packet");
        /* Drop packet ???? */ 
        wpalPacketFree(pFrame);
        return eWLAN_PAL_STATUS_SUCCESS;
      }

      if((ucMPDUHOffset < WDI_RX_BD_HEADER_SIZE) &&  (!(bASF && !bFSF))){
        /* AMSDU case, ucMPDUHOffset = 0  it should be hancdled seperatly */
        /* Drop packet ???? */ 
        wpalPacketFree(pFrame);
        return eWLAN_PAL_STATUS_SUCCESS;
      }

      /* AMSDU frame, but not first sub-frame
       * No MPDU header, MPDU header offset is 0
       * Total frame size is actual frame size + MPDU data offset */
      if((ucMPDUHOffset < WDI_RX_BD_HEADER_SIZE) && (bASF && !bFSF)){
        ucMPDUHOffset = usMPDUDOffset;
      }

      if(VPKT_SIZE_BUFFER_ALIGNED < (usMPDULen+ucMPDUHOffset)){
        WPAL_TRACE(eWLAN_MODULE_DAL_DATA, eWLAN_PAL_TRACE_LEVEL_FATAL,
                   "Invalid Frame size, might memory corrupted(%d+%d/%d)",
                   usMPDULen, ucMPDUHOffset, VPKT_SIZE_BUFFER_ALIGNED);

        /* Size of the packet tranferred by the DMA engine is
         * greater than the the memory allocated for the skb
         */
        WPAL_BUG(0);

        wpalPacketFree(pFrame);
        return eWLAN_PAL_STATUS_SUCCESS;
      }
      if(eWLAN_PAL_STATUS_SUCCESS != wpalPacketSetRxLength(pFrame, usMPDULen+ucMPDUHOffset))
      {
          DTI_TRACE( DTI_TRACE_LEVEL_ERROR, "Invalid Frame Length, Frame dropped..");
          wpalPacketFree(pFrame);
          return eWLAN_PAL_STATUS_SUCCESS;
      }
      if(eWLAN_PAL_STATUS_SUCCESS != wpalPacketRawTrimHead(pFrame, ucMPDUHOffset))
      {
          DTI_TRACE( DTI_TRACE_LEVEL_ERROR, "Failed to trim Raw Packet Head, Frame dropped..");
          wpalPacketFree(pFrame);
          return eWLAN_PAL_STATUS_SUCCESS;
      }
     

      pRxMetadata = WDI_DS_ExtractRxMetaData(pFrame);

      pRxMetadata->fc = isFcBd;
      pRxMetadata->staId = WDI_RX_BD_GET_STA_ID(pBDHeader);
      pRxMetadata->addr3Idx = WDI_RX_BD_GET_ADDR3_IDX(pBDHeader);
      pRxMetadata->rxChannel = WDI_RX_BD_GET_RX_CHANNEL(pBDHeader);
      pRxMetadata->rfBand = WDI_RX_BD_GET_RFBAND(pBDHeader);
      pRxMetadata->rtsf = WDI_RX_BD_GET_RTSF(pBDHeader);
      pRxMetadata->bsf = WDI_RX_BD_GET_BSF(pBDHeader);
      pRxMetadata->scan = WDI_RX_BD_GET_SCAN(pBDHeader);
      pRxMetadata->dpuSig = WDI_RX_BD_GET_DPU_SIG(pBDHeader);
      pRxMetadata->ft = WDI_RX_BD_GET_FT(pBDHeader);
      pRxMetadata->ne = WDI_RX_BD_GET_NE(pBDHeader);
      pRxMetadata->llcr = WDI_RX_BD_GET_LLCR(pBDHeader);
      pRxMetadata->bcast = WDI_RX_BD_GET_UB(pBDHeader);
      pRxMetadata->tid = ucTid;
      pRxMetadata->dpuFeedback = WDI_RX_BD_GET_DPU_FEEDBACK(pBDHeader);
      pRxMetadata->rateIndex = WDI_RX_BD_GET_RATEINDEX(pBDHeader);
      pRxMetadata->rxpFlags = WDI_RX_BD_GET_RXPFLAGS(pBDHeader);
      pRxMetadata->mclkRxTimestamp = WDI_RX_BD_GET_TIMESTAMP(pBDHeader);
#ifdef WLAN_FEATURE_11W
      pRxMetadata->rmf = WDI_RX_BD_GET_RMF(pBDHeader);
#endif
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
      pRxMetadata->offloadScanLearn = WDI_RX_BD_GET_OFFLOADSCANLEARN(pBDHeader);
      pRxMetadata->roamCandidateInd = WDI_RX_BD_GET_ROAMCANDIDATEIND(pBDHeader);
#endif
#ifdef WLAN_FEATURE_EXTSCAN
      pRxMetadata->extscanBuffer = WDI_RX_BD_GET_EXTSCANFULLSCANRESIND(pBDHeader);
#endif
      /* typeSubtype in BD doesn't look like correct. Fill from frame ctrl
         TL does it for Volans but TL does not know BD for Prima. WDI should do it */
      if ( 0 == WDI_RX_BD_GET_FT(pBDHeader) ) {
        if ( bASF ) {
          pRxMetadata->subtype = WDI_MAC_DATA_QOS_DATA;
          pRxMetadata->type    = WDI_MAC_DATA_FRAME;
        } else {
          pMacFrameCtl = (tpSirMacFrameCtl)(((wpt_uint8*)pBDHeader) + ucMPDUHOffset);
          pRxMetadata->subtype = pMacFrameCtl->subType;
          pRxMetadata->type    = pMacFrameCtl->type;
        }
      } else {
        pMacFrameCtl = (tpSirMacFrameCtl)(((wpt_uint8*)pBDHeader) + WDI_RX_BD_HEADER_SIZE);
        pRxMetadata->subtype = pMacFrameCtl->subType;
        pRxMetadata->type    = pMacFrameCtl->type;
      }

      pRxMetadata->mpduHeaderPtr = pBDHeader + ucMPDUHOffset;
      pRxMetadata->mpduDataPtr = pBDHeader + usMPDUDOffset;
      pRxMetadata->mpduLength = usMPDULen;
      pRxMetadata->mpduHeaderLength = ucMPDUHLen;

      /*------------------------------------------------------------------------
        Gather AMPDU information 
        ------------------------------------------------------------------------*/
      pRxMetadata->ampdu_reorderOpcode  = (wpt_uint8)WDI_RX_BD_GET_BA_OPCODE(pBDHeader);
      pRxMetadata->ampdu_reorderSlotIdx = (wpt_uint8)WDI_RX_BD_GET_BA_SI(pBDHeader);
      pRxMetadata->ampdu_reorderFwdIdx  = (wpt_uint8)WDI_RX_BD_GET_BA_FI(pBDHeader);
      pRxMetadata->currentPktSeqNo       = (wpt_uint16)WDI_RX_BD_GET_BA_CSN(pBDHeader);


      /*------------------------------------------------------------------------
        Gather AMSDU information 
        ------------------------------------------------------------------------*/
      pRxMetadata->amsdu_asf  =  bASF;
      pRxMetadata->amsdu_aef  =  bAEF;
      pRxMetadata->amsdu_esf  =  bFSF;
      pRxMetadata->amsdu_lsf  =  bLSF;
      pRxMetadata->amsdu_size =  WDI_RX_BD_GET_AMSDU_SIZE(pBDHeader);

      pRxMetadata->rssi0 = WDI_RX_BD_GET_RSSI0(pBDHeader);
      pRxMetadata->rssi1 = WDI_RX_BD_GET_RSSI1(pBDHeader);


        /* Missing: 
      wpt_uint32 fcSTATxQStatus:8;
      wpt_uint32 fcSTAThreshIndMask:8;
      wpt_uint32 fcSTAPwrSaveStateMask:8;
      wpt_uint32 fcSTAValidMask:8;

      wpt_uint8 fcSTATxQLen[8]; // one byte per STA. 
      wpt_uint8 fcSTACurTxRate[8]; // current Tx rate for each sta.   
      unknownUcastPkt 
      */

      pRxMetadata->replayCount = WDTS_GetReplayCounterFromRxBD(pBDHeader);
      pRxMetadata->snr = WDI_RX_BD_GET_SNR(pBDHeader); 

      /* 
       * PAL BD pointer information needs to be populated 
       */ 
      WPAL_PACKET_SET_BD_POINTER(pFrame, pBDHeader);
      WPAL_PACKET_SET_BD_LENGTH(pFrame, sizeof(WDI_RxBdType));

#ifdef DEBUG_ROAM_DELAY
      //Hack we need to send the frame type, so we are using bufflen as frametype
      vos_record_roam_event(e_DXE_RX_PKT_TIME, (void *)pFrame, pRxMetadata->type);
      //Should we use the below check to avoid funciton calls
      /*
      if(gRoamDelayMetaInfo.dxe_monitor_tx)
      {
      }
      */
#endif
      // Invoke Rx complete callback
      pClientData->receiveFrameCB(pClientData->pCallbackContext, pFrame);  
  }
  else
  {
      wpalPacketSetRxLength(pFrame, usMPDULen+ucMPDUHOffset);
      wpalPacketRawTrimHead(pFrame, ucMPDUHOffset);

      pRxMetadata = WDI_DS_ExtractRxMetaData(pFrame);
      //flow control related
      pRxMetadata->fc = isFcBd;
      pRxMetadata->mclkRxTimestamp = WDI_RX_BD_GET_TIMESTAMP(pBDHeader);
      pRxMetadata->fcStaTxDisabledBitmap = WDI_RX_FC_BD_GET_STA_TX_DISABLED_BITMAP(pBDHeader);
      pRxMetadata->fcSTAValidMask = WDI_RX_FC_BD_GET_STA_VALID_MASK(pBDHeader);
      // Invoke Rx complete callback
      pClientData->receiveFrameCB(pClientData->pCallbackContext, pFrame);  
  }

  //Log the RX Stats
  if(gDsTrafficStats.running && pRxMetadata->staId < HAL_NUM_STA)
  {
     if(pRxMetadata->rateIndex < WDTS_MAX_RATE_NUM)
     {
        if(pRxMetadata->type == WDI_MAC_DATA_FRAME)
        {
           gDsTrafficStats.rxStats[pRxMetadata->staId][pRxMetadata->rateIndex].rxBytesRcvd +=
              pRxMetadata->mpduLength;
           gDsTrafficStats.rxStats[pRxMetadata->staId][pRxMetadata->rateIndex].rxPacketsRcvd++;
        }
     }
  }
  return eWLAN_PAL_STATUS_SUCCESS;
}



/* DTS Out of Resource packet function. 
 * This function should be invoked by the transport device to indicate 
 * the device is out of resources.
 * Parameters:
 * pContext:Cookie that should be passed back to the caller 
 * priority: indicates which channel is out of resource.
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 */
wpt_status WDTS_OOResourceNotification(void *pContext, WDTS_ChannelType channel, wpt_boolean on)
{
  WDI_DS_ClientDataType *pClientData =
    (WDI_DS_ClientDataType *) pContext;
  static wpt_uint8 ac_mask = 0x1f;

  // Do Sanity checks
  if(NULL == pContext){
    return eWLAN_PAL_STATUS_E_FAILURE;
  }
  
  if(on){
    ac_mask |=  channel == WDTS_CHANNEL_TX_LOW_PRI?  0x0f : 0x10;
  } else {
    ac_mask &=  channel == WDTS_CHANNEL_TX_LOW_PRI?  0x10 : 0x0f;
  }


  // Invoke OOR callback
  pClientData->txResourceCB(pClientData->pCallbackContext, ac_mask); 
  return eWLAN_PAL_STATUS_SUCCESS;

}

/* DTS open  function. 
 * On open the transport device should initialize itself.
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along 
 *  with the callback.
 *
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
wpt_status WDTS_openTransport( void *pContext)
{
  void *pDTDriverContext; 
  WDI_DS_ClientDataType *pClientData;
  WDI_Status sWdiStatus = WDI_STATUS_SUCCESS;

  pClientData = (WDI_DS_ClientDataType*) wpalMemoryAllocate(sizeof(WDI_DS_ClientDataType));
  if (!pClientData){
    return eWLAN_PAL_STATUS_E_NOMEM;
  }

  pClientData->suspend = 0;
  WDI_DS_AssignDatapathContext(pContext, (void*)pClientData);

  pDTDriverContext = gTransportDriver.open(); 
  if( NULL == pDTDriverContext )
  {
     DTI_TRACE( DTI_TRACE_LEVEL_ERROR, " %s fail from transport open", __func__);
     return eWLAN_PAL_STATUS_E_FAILURE;
  }
  WDT_AssignTransportDriverContext(pContext, pDTDriverContext);
  gTransportDriver.register_client(pDTDriverContext, WDTS_RxPacket, WDTS_TxPacketComplete, 
    WDTS_OOResourceNotification, (void*)pClientData);

  /* Create a memory pool for Mgmt BDheaders.*/
  sWdiStatus = WDI_DS_MemPoolCreate(&pClientData->mgmtMemPool, WDI_DS_MAX_CHUNK_SIZE, 
                                                     WDI_DS_HI_PRI_RES_NUM);
  if (WDI_STATUS_SUCCESS != sWdiStatus){
    return eWLAN_PAL_STATUS_E_NOMEM;
  }

  /* Create a memory pool for Data BDheaders.*/
  sWdiStatus = WDI_DS_MemPoolCreate(&pClientData->dataMemPool, WDI_DS_MAX_CHUNK_SIZE, 
                                                      WDI_DS_LO_PRI_RES_NUM);
  if (WDI_STATUS_SUCCESS != sWdiStatus){
    return eWLAN_PAL_STATUS_E_NOMEM;
  }

  wpalMemoryZero(&gDsTrafficStats, sizeof(gDsTrafficStats));

  return eWLAN_PAL_STATUS_SUCCESS;

}



/* DTS start  function. 
 * On start the transport device should start running.
 * Parameters:
 * pContext:Cookie that should be passed back to the caller along 
 * with the callback.
 *
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
wpt_status WDTS_startTransport( void *pContext)
{
  void *pDTDriverContext = WDT_GetTransportDriverContext(pContext);
  gTransportDriver.start(pDTDriverContext); 
  return eWLAN_PAL_STATUS_SUCCESS;

}


/* DTS Tx packet function. 
 * This function should be invoked by the DAL Dataservice to schedule transmit frame through DXE/SDIO.
 * Parameters:
 * pContext:Cookie that should be passed back to the caller along with the callback.
 * pFrame:Refernce to PAL frame.
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
wpt_status WDTS_TxPacket(void *pContext, wpt_packet *pFrame)
{
  void *pDTDriverContext = WDT_GetTransportDriverContext(pContext);
  WDI_DS_TxMetaInfoType     *pTxMetadata;
  WDTS_ChannelType channel = WDTS_CHANNEL_TX_LOW_PRI;
  wpt_status status = eWLAN_PAL_STATUS_SUCCESS;

  // extract metadata from PAL packet
  pTxMetadata = WDI_DS_ExtractTxMetaData(pFrame);

  //Log the TX Stats
  if(gDsTrafficStats.running && pTxMetadata->staIdx < HAL_NUM_STA)
  {
     if(pTxMetadata->frmType & WDI_MAC_DATA_FRAME)
     {
        gDsTrafficStats.txStats[pTxMetadata->staIdx].txBytesPushed +=
           pTxMetadata->fPktlen;
        gDsTrafficStats.txStats[pTxMetadata->staIdx].txPacketsPushed += 1;
      }
  }

  // assign MDPU to correct channel??
  channel =  (pTxMetadata->frmType & WDI_MAC_DATA_FRAME)? 
    /* EAPOL frame uses TX_HIGH_PRIORITY DXE channel
       To make sure EAPOL (for second session) is pushed even if TX_LO channel
       already reached to low resource condition
       This can happen especially in MCC, high data traffic TX in first session
     */
#ifdef FEATURE_WLAN_TDLS
     /* I utilizes TDLS mgmt frame always sent at BD_RATE2. (See limProcessTdls.c)
        Assumption here is data frame sent by WDA_TxPacket() <- HalTxFrame/HalTxFrameWithComplete()
        should take managment path. As of today, only TDLS feature has special data frame
        which needs to be treated as mgmt.
      */
      (((pTxMetadata->isEapol) || (pTxMetadata->txFlags & WDI_USE_BD_RATE2_FOR_MANAGEMENT_FRAME))? WDTS_CHANNEL_TX_HIGH_PRI : WDTS_CHANNEL_TX_LOW_PRI) : WDTS_CHANNEL_TX_HIGH_PRI;
#else
      ((pTxMetadata->isEapol) ? WDTS_CHANNEL_TX_HIGH_PRI : WDTS_CHANNEL_TX_LOW_PRI) : WDTS_CHANNEL_TX_HIGH_PRI;
#endif
  // Send packet to  Transport Driver. 
  status =  gTransportDriver.xmit(pDTDriverContext, pFrame, channel);
#ifdef DEBUG_ROAM_DELAY
   //Hack we need to send the frame type, so we are using bufflen as frametype
   vos_record_roam_event(e_DXE_FIRST_XMIT_TIME, (void *)pFrame, pTxMetadata->frmType);
   //Should we use the below check to avoid funciton calls
   /*
   if(gRoamDelayMetaInfo.dxe_monitor_tx)
   {
   }
   */
#endif
  return status;
}

/* DTS Tx Complete function. 
 * This function should be invoked by the DAL Dataservice to notify tx completion to DXE/SDIO.
 * Parameters:
 * pContext:Cookie that should be passed back to the caller along with the callback.
 * ucTxResReq:TX resource number required by TL
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
wpt_status WDTS_CompleteTx(void *pContext, wpt_uint32 ucTxResReq)
{
  void *pDTDriverContext = WDT_GetTransportDriverContext(pContext);
  
  // Notify completion to  Transport Driver. 
  return gTransportDriver.txComplete(pDTDriverContext, ucTxResReq);
}

/* DXE Set power state ACK callback. 
 * This callback function should be invoked by the DXE to notify WDI that set
 * power state request is complete.
 * Parameters:
 * status: status of the set operation
 * Return Value: None.
 *
 */
void  WDTS_SetPowerStateCb(wpt_status   status, unsigned int dxePhyAddr)
{
   //print a msg
   if(NULL != gSetPowerStateCbInfo.cback) 
   {
      gSetPowerStateCbInfo.cback(status, dxePhyAddr, gSetPowerStateCbInfo.pUserData);
   }
}


/* DTS Set power state function. 
 * This function should be invoked by the DAL to notify the WLAN device power state.
 * Parameters:
 * pContext:Cookie that should be passed back to the caller along with the callback.
 * powerState:Power state of the WLAN device.
 * Return Value: SUCCESS  Set successfully in DXE control blk.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
wpt_status WDTS_SetPowerState(void *pContext, WDTS_PowerStateType  powerState,
                              WDTS_SetPowerStateCbType cback)
{
   void *pDTDriverContext = WDT_GetTransportDriverContext(pContext);
   wpt_status status = eWLAN_PAL_STATUS_SUCCESS;

   if( cback )
   {
      //save the cback & cookie
      gSetPowerStateCbInfo.pUserData = pContext;
      gSetPowerStateCbInfo.cback = cback;
      status = gTransportDriver.setPowerState(pDTDriverContext, powerState,
                                            WDTS_SetPowerStateCb);
   }
   else
   {
      status = gTransportDriver.setPowerState(pDTDriverContext, powerState,
                                               NULL);
   }

   return status;
}

/* DTS Transport Channel Debug
 * Display DXE Channel debugging information
 * User may request to display DXE channel snapshot
 * Or if host driver detects any abnormal stcuk may display
 * Parameters:
 *  displaySnapshot : Display DXE snapshot option
 *  enableStallDetect : Enable stall detect feature
                        This feature will take effect to data performance
                        Not integrate till fully verification
 * Return Value: NONE
 *
 */
void WDTS_ChannelDebug(wpt_boolean displaySnapshot, wpt_uint8 debugFlags)
{
   gTransportDriver.channelDebug(displaySnapshot, debugFlags);
   return;
}

/* DTS Stop function. 
 * Stop Transport driver, ie DXE, SDIO
 * Parameters:
 * pContext:Cookie that should be passed back to the caller along with the callback.
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
wpt_status WDTS_Stop(void *pContext)
{
  void *pDTDriverContext = WDT_GetTransportDriverContext(pContext);
  wpt_status status = eWLAN_PAL_STATUS_SUCCESS;

  status =  gTransportDriver.stop(pDTDriverContext);

  wpalMemoryZero(&gDsTrafficStats, sizeof(gDsTrafficStats));

  return status;
}

/* DTS Stop function. 
 * Stop Transport driver, ie DXE, SDIO
 * Parameters:
 * pContext:Cookie that should be passed back to the caller along with the callback.
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
wpt_status WDTS_Close(void *pContext)
{
  void *pDTDriverContext = WDT_GetTransportDriverContext(pContext);
  WDI_DS_ClientDataType *pClientData = WDI_DS_GetDatapathContext(pContext);
  wpt_status status = eWLAN_PAL_STATUS_SUCCESS;

  /*Destroy the mem pool for mgmt BD headers*/
  WDI_DS_MemPoolDestroy(&pClientData->mgmtMemPool);
  
  /*Destroy the mem pool for mgmt BD headers*/
  WDI_DS_MemPoolDestroy(&pClientData->dataMemPool);
  
  status =  gTransportDriver.close(pDTDriverContext);

  wpalMemoryFree(pClientData);

  return status;
}

/* Get free TX data descriptor number from DXE
 * Parameters:
 * pContext: Cookie that should be passed back to the caller along with the callback.
 * Return Value: number of free descriptors for TX data channel
 *
 */
wpt_uint32 WDTS_GetFreeTxDataResNumber(void *pContext)
{
  return 
     gTransportDriver.getFreeTxDataResNumber(WDT_GetTransportDriverContext(pContext));
}
