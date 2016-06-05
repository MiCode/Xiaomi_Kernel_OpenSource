/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
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

#ifndef __WLAN_QCT_DTS_H
#define __WLAN_QCT_DTS_H

#include "wlan_qct_wdi.h"

/**=========================================================================
 *     
 *       \file  wlan_qct_wdi_dts.h
 *          
 *       \brief define Datas Trnasport Service API 
 *                               
 * WLAN Device Abstraction layer interface for Transport drivers (SDIO/DXE)
 * DESCRIPTION
 * This file contains the API exposed by the 
 * wlan device abstarction layer module for abstracting DXE/SDIO.
 *
 *
 * Example usage for DXE.
 * ----------------------
 * On Platform init
 *         DAL  will then invoke WDTS_open
 *
 * On DAL init
 *     DAL will invike WDTS_start
 *
 * On transmit:
 *         DAL will invoke WDTS_TxPacket API
 * 
 * On transmit complete:
 *         DXE will serialize into TX thread
 *         In TX thread it will invoke
 *
 * On receive:
 *        DXE will serialize into RX thread
 *        In TX thread it will invoke WDTS_RXPacket API 
 *
 * On DXE ring full:
 *        DXE will serialize into TX thread
 *        In TX thread it will invoke WDTS_OOResourceNotification API 
 */

typedef enum
{
   WDTS_CHANNEL_TX_LOW_PRI,
   WDTS_CHANNEL_TX_HIGH_PRI,
   WDTS_CHANNEL_RX_LOW_PRI,
   WDTS_CHANNEL_RX_HIGH_PRI,
   WDTS_CHANNEL_RX_LOG,
   WDTS_CHANNEL_RX_FW_LOG,
   WDTS_CHANNEL_MAX
}  WDTS_ChannelType;

#define WDTS_TRANSPORT_CHANNELS_MASK \
        (1<<WDTS_CHANNEL_TX_LOW_PRI | 1<<WDTS_CHANNEL_TX_HIGH_PRI \
         | 1<<WDTS_CHANNEL_RX_LOW_PRI | 1<<WDTS_CHANNEL_RX_HIGH_PRI)

#define WDTS_RX_LOG_CHANNEL_MASK (1<<WDTS_CHANNEL_RX_LOG)
#define WDTS_RX_FW_LOG_CHANNEL_MASK (1<<WDTS_CHANNEL_RX_FW_LOG)

typedef enum
{
   WDTS_POWER_STATE_FULL,
   WDTS_POWER_STATE_IMPS,
   WDTS_POWER_STATE_BMPS,
   WDTS_POWER_STATE_DOWN,
   WDTS_POWER_STATE_MAX
} WDTS_PowerStateType;


typedef wpt_status (*WDTS_TxCompleteCbType)(void *pContext, wpt_packet *pFrame, wpt_status status);
typedef wpt_status (*WDTS_RxFrameReadyCbType) (void *pContext, wpt_packet *pFrame, WDTS_ChannelType channel);
typedef wpt_status (*WDTS_LowResourceCbType)(void *pContext, WDTS_ChannelType channel, wpt_boolean on);
typedef void  (*WDTS_SetPSCbType)(wpt_status  status, unsigned int dxePhyAddr);
typedef void (*WDTS_MbReceiveMsgType)(void *pContext);
typedef void (*WDTS_RxLogDoneType)(void *pContext);

/* DTS Set power state ACK callback. 
 * This callback function should be invoked by the DTS to notify WDI that set
 * power state request is complete.
 * Parameters:
 * status: status of the set operation
 * pUserData:Cookie that should be passed back to the caller along with the callback.
 * Return Value: None.
 *
 */
typedef void  (*WDTS_SetPowerStateCbType)(wpt_status   status,
                                          unsigned int dxePhyAddr,
                                          void*        pUserData);
typedef struct
{
   WDTS_RxFrameReadyCbType  rxFrameReadyCB;
   WDTS_TxCompleteCbType    txCompleteCB;
   WDTS_LowResourceCbType   lowResourceCB;
   WDTS_MbReceiveMsgType    receiveMbMsgCB;
   WDTS_RxLogDoneType       receiveLogCompleteCB;
}WDTS_ClientCallbacks;

typedef struct {
  void * (*open)(void);
  wpt_status (*start) (void *pContext);
  wpt_status (*register_client)(void *pContext, WDTS_ClientCallbacks, void *clientData);
  wpt_status (*xmit) (void *pContext, wpt_packet *packet, WDTS_ChannelType channel);
  wpt_status (*txComplete) (void *pContext, wpt_uint32 ucTxResReq);
  wpt_status (*setPowerState) (void *pContext, WDTS_PowerStateType   powerState, 
                               WDTS_SetPSCbType cBack);
  void (*channelDebug)(wpt_boolean displaySnapshot,
                       wpt_uint8   debugFlags);
  void (*kickDxe) (void);
  wpt_status (*stop) (void *pContext);
  wpt_status (*close) (void *pContext);
  wpt_uint32 (*getFreeTxDataResNumber) (void *pContext);
  wpt_uint32 (*setupLogTransfer) (wpt_uint64 bufferAddr, wpt_uint32 bufferSize);
  wpt_status  (*startLogTransfer) (void);
} WDTS_TransportDriverTrype;

typedef struct {
   WDTS_SetPowerStateCbType cback;
   void*        pUserData;
} WDTS_SetPowerStateCbInfoType;

/* Tx/Rx stats function
 * This function should be invoked to fetch the current stats
  * Parameters:
 *  pStats:Pointer to the collected stats
 *  len: length of buffer pointed to by pStats
 *  Return Status: None
 */
void WDTS_GetTrafficStats(WDI_TrafficStatsType** pStats, wpt_uint32 *len);

/* WDTS_DeactivateTrafficStats
 * This function should be invoked to suspend traffic stats collection
  * Parameters: None
 *  Return Status: None
 */
void WDTS_DeactivateTrafficStats(void);

/* WDTS_ActivateTrafficStats
 * This function should be invoked to activate traffic stats collection
  * Parameters: None
 *  Return Status: None
 */
void WDTS_ActivateTrafficStats(void);

/* WDTS_ClearTrafficStats
 * This function should be invoked to clear all past stats
  * Parameters: None
 *  Return Status: None
 */
void WDTS_ClearTrafficStats(void);

/* DTS open  function. 
 * On open the transport device should initialize itself.
 * Parameters:
 *  pContext:Cookie that should be passed back to the caller along 
 *  with the callback.
 *
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 
 */
wpt_status WDTS_openTransport( void *pContext);




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
wpt_status WDTS_startTransport( void *pContext);




/* DTS Tx packet function. 
 * This function should be invoked by the DAL Dataservice to schedule transmit frame through DXE/SDIO.
 * Parameters:
 * pContext:Cookie that should be passed back to the caller along with the callback.
 * pFrame:Refernce to PAL frame.
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
wpt_status WDTS_TxPacket(void *pContext, wpt_packet *pFrame);

/* DTS Tx Complete function. 
 * This function should be invoked by the DAL Dataservice to notify tx completion to DXE/SDIO.
 * Parameters:
 * pContext:Cookie that should be passed back to the caller along with the callback.
 * ucTxResReq:TX resource number required by TL
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
wpt_status WDTS_CompleteTx(void *pContext, wpt_uint32 ucTxResReq);

/* DTS Set power state function. 
 * This function should be invoked by the DAL to notify the WLAN device power state.
 * Parameters:
 * pContext:Cookie that should be passed back to the caller along with the callback.
 * powerState:Power state of the WLAN device.
 * Return Value: SUCCESS  Set successfully in DXE control blk.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
wpt_status WDTS_SetPowerState(void *pContext, WDTS_PowerStateType powerState,
                              WDTS_SetPowerStateCbType cback);

/* DTS Transport Channel Debug
 * Display DXE Channel debugging information
 * User may request to display DXE channel snapshot
 * Or if host driver detects any abnormal stcuk may display
 * Parameters:
 *  displaySnapshot : Display DXE snapshot option
 *  debugFlags      : Enable stall detect features
 *                    defined by WPAL_DeviceDebugFlags
 *                    These features may effect
 *                    data performance.
 *
 *                    Not integrate till fully verification
 * Return Value: NONE
 *
 */
void WDTS_ChannelDebug(wpt_boolean displaySnapshot, wpt_uint8 debugFlags);

/* DTS Transport Channel Kick Dxe
 * Request Kick DXE when HDD TX timeout happen
 *
 * Parameters  : NONE
 * Return Value: NONE
 *
 */
void WDTS_ChannelKickDxe(void);

/* DTS Stop function. 
 * Stop Transport driver, ie DXE, SDIO
 * Parameters:
 * pContext:Cookie that should be passed back to the caller along with the callback.
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
wpt_status WDTS_Stop(void *pContext);

/* DTS Close function. 
 * Close Transport driver, ie DXE, SDIO
 * Parameters:
 * pContext:Cookie that should be passed back to the caller along with the callback.
 * Return Value: SUCCESS  Completed successfully.
 *     FAILURE_XXX  Request was rejected due XXX Reason.
 *
 */
wpt_status WDTS_Close(void *pContext);

/* Get free TX data descriptor number from DXE
 * Parameters:
 * pContext: Cookie that should be passed back to the caller along with the callback.
 * Return Value: number of free descriptors for TX data channel
 *
 */
wpt_uint32 WDTS_GetFreeTxDataResNumber(void *pContext);

/* API to fill Rate Info based on the mac efficiency passed to it
 * macEff si used to caclulate mac throughput based on each rate index/PHY rate.
 * This is eventually used by MAS to calculate RX stats periodically sent to FW
 * The start and end Rate Index are the other arguments to this API - the new mac
 * efficiency passed to this API (Arg1)  is only applied between startRateIndex (arg2) and endRateIndex (arg3).
 */
void WDTS_FillRateInfo(wpt_uint8 macEff, wpt_int16 startRateIndex, wpt_int16 endRateIndex);
#endif
