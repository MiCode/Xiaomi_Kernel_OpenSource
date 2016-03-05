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


#if !defined( WLAN_HDD_SOFTAP_TX_RX_H )
#define WLAN_HDD_SOFTAP_TX_RX_H

/**===========================================================================
  
  \file  wlan_hdd_softap_tx_rx.h
  
  \brief Linux HDD SOFTAP Tx/RX APIs
         Copyright 2008 (c) Qualcomm, Incorporated.
         All Rights Reserved.
         Qualcomm Confidential and Proprietary.
  
  ==========================================================================*/
  
/*--------------------------------------------------------------------------- 
  Include files
  -------------------------------------------------------------------------*/ 
#include <wlan_hdd_hostapd.h>

/*--------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  -------------------------------------------------------------------------*/ 
#define HDD_SOFTAP_TX_BK_QUEUE_MAX_LEN (82*2)
#define HDD_SOFTAP_TX_BE_QUEUE_MAX_LEN (78*2)
#define HDD_SOFTAP_TX_VI_QUEUE_MAX_LEN (74*2)
#define HDD_SOFTAP_TX_VO_QUEUE_MAX_LEN (70*2)

/* SoftAP specific AC Weights */
#define HDD_SOFTAP_BK_WEIGHT_DEFAULT                        1
#define HDD_SOFTAP_BE_WEIGHT_DEFAULT                        3
#define HDD_SOFTAP_VI_WEIGHT_DEFAULT                        8
#define HDD_SOFTAP_VO_WEIGHT_DEFAULT                        18

/*--------------------------------------------------------------------------- 
  Type declarations
  -------------------------------------------------------------------------*/ 
 
/*--------------------------------------------------------------------------- 
  Function declarations and documenation
  -------------------------------------------------------------------------*/ 

/**============================================================================
  @brief hdd_softap_hard_start_xmit() - Function registered with the Linux OS for 
  transmitting packets

  @param skb      : [in]  pointer to OS packet (sk_buff)
  @param dev      : [in] pointer to Libra softap network device
  
  @return         : NET_XMIT_DROP if packets are dropped
                  : NET_XMIT_SUCCESS if packet is enqueued succesfully
  ===========================================================================*/
extern int hdd_softap_hard_start_xmit(struct sk_buff *skb, struct net_device *dev);

/**============================================================================
  @brief hdd_softap_tx_timeout() - Function called by OS if there is any
  timeout during transmission. Since HDD simply enqueues packet
  and returns control to OS right away, this would never be invoked

  @param dev : [in] pointer to Libra network device
  @return    : None
  ===========================================================================*/
extern void hdd_softap_tx_timeout(struct net_device *dev);

/**============================================================================
  @brief hdd_softap_stats() - Function registered with the Linux OS for 
  device TX/RX statistics

  @param dev      : [in] pointer to Libra network device
  
  @return         : pointer to net_device_stats structure
  ===========================================================================*/
extern struct net_device_stats* hdd_softap_stats(struct net_device *dev);

/**============================================================================
  @brief hdd_softap_init_tx_rx() - Init function to initialize Tx/RX
  modules in HDD

  @param pAdapter : [in] pointer to adapter context  
  @return         : VOS_STATUS_E_FAILURE if any errors encountered 
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
extern VOS_STATUS hdd_softap_init_tx_rx( hdd_adapter_t *pAdapter );

/**============================================================================
  @brief hdd_softap_deinit_tx_rx() - Deinit function to clean up Tx/RX
  modules in HDD

  @param pAdapter : [in] pointer to adapter context  
  @return         : VOS_STATUS_E_FAILURE if any errors encountered 
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
extern VOS_STATUS hdd_softap_deinit_tx_rx( hdd_adapter_t *pAdapter );

/**============================================================================
  @brief hdd_softap_init_tx_rx_sta() - Init function to initialize a station in Tx/RX
  modules in HDD

  @param pAdapter : [in] pointer to adapter context
  @param STAId    : [in] Station ID to deinit
  @param pmacAddrSTA  : [in] pointer to the MAC address of the station  
  @return         : VOS_STATUS_E_FAILURE if any errors encountered 
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
extern VOS_STATUS hdd_softap_init_tx_rx_sta( hdd_adapter_t *pAdapter, v_U8_t STAId, v_MACADDR_t *pmacAddrSTA);

/**============================================================================
  @brief hdd_softap_deinit_tx_rx_sta() - Deinit function to clean up a statioin in Tx/RX
  modules in HDD

  @param pAdapter : [in] pointer to adapter context
  @param STAId    : [in] Station ID to deinit 
  @return         : VOS_STATUS_E_FAILURE if any errors encountered 
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
extern VOS_STATUS hdd_softap_deinit_tx_rx_sta ( hdd_adapter_t *pAdapter, v_U8_t STAId );

/**============================================================================
  @brief hdd_disconnect_tx_rx() - Disconnect function to clean up Tx/RX
  modules in HDD

  @param pAdapter : [in] pointer to adapter context  
  @return         : VOS_STATUS_E_FAILURE if any errors encountered 
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
extern VOS_STATUS hdd_softap_disconnect_tx_rx( hdd_adapter_t *pAdapter );

/**============================================================================
  @brief hdd_tx_complete_cbk() - Callback function invoked by TL
  to indicate that a packet has been transmitted across the SDIO bus
  succesfully. OS packet resources can be released after this cbk.

  @param vosContext   : [in] pointer to VOS context   
  @param pVosPacket   : [in] pointer to VOS packet (containing skb) 
  @param vosStatusIn  : [in] status of the transmission 

  @return             : VOS_STATUS_E_FAILURE if any errors encountered 
                      : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
extern VOS_STATUS hdd_softap_tx_complete_cbk( v_VOID_t *vosContext, 
                                       vos_pkt_t *pVosPacket, 
                                       VOS_STATUS vosStatusIn );

/**============================================================================
  @brief hdd_softap_tx_fetch_packet_cbk() - Callback function invoked by TL to 
  fetch a packet for transmission.

  @param vosContext   : [in] pointer to VOS context  
  @param staId        : [in] Station for which TL is requesting a pkt
  @param ucAC         : [in] pointer to access category requested by TL
  @param pVosPacket   : [out] pointer to VOS packet packet pointer
  @param pPktMetaInfo : [out] pointer to meta info for the pkt 
  
  @return             : VOS_STATUS_E_EMPTY if no packets to transmit
                      : VOS_STATUS_E_FAILURE if any errors encountered 
                      : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
extern VOS_STATUS hdd_softap_tx_fetch_packet_cbk( v_VOID_t *vosContext,
                                           v_U8_t *pStaId,
                                           WLANTL_ACEnumType    ucAC,
                                           vos_pkt_t **ppVosPacket,
                                           WLANTL_MetaInfoType *pPktMetaInfo );

/**============================================================================
  @brief hdd_softap_tx_low_resource_cbk() - Callback function invoked in the 
  case where VOS packets are not available at the time of the call to get 
  packets. This callback function is invoked by VOS when packets are 
  available.

  @param pVosPacket : [in]  pointer to VOS packet 
  @param userData   : [in]  opaque user data that was passed initially 
  
  @return           : VOS_STATUS_E_FAILURE if any errors encountered, 
                    : VOS_STATUS_SUCCESS otherwise
  =============================================================================*/
extern VOS_STATUS hdd_softap_tx_low_resource_cbk( vos_pkt_t *pVosPacket, 
                                           v_VOID_t *userData );

/**============================================================================
  @brief hdd_softap_rx_packet_cbk() - Receive callback registered with TL.
  TL will call this to notify the HDD when a packet was received 
  for a registered STA.

  @param vosContext   : [in] pointer to VOS context  
  @param pVosPacket   : [in] pointer to VOS packet (conatining sk_buff) 
  @param staId        : [in] Station Id
  @param pRxMetaInfo  : [in] pointer to meta info for the received pkt(s) 

  @return             : VOS_STATUS_E_FAILURE if any errors encountered, 
                      : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
extern VOS_STATUS hdd_softap_rx_packet_cbk( v_VOID_t *vosContext, 
                                     vos_pkt_t *pVosPacket, 
                                     v_U8_t staId,
                                     WLANTL_RxMetaInfoType* pRxMetaInfo );

/**============================================================================
  @brief hdd_softap_DeregisterSTA - Deregister a station from TL block

  @param pAdapter : [in] pointer to adapter context
  @param STAId    : [in] Station ID to deregister
  @return         : VOS_STATUS_E_FAILURE if any errors encountered 
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
extern VOS_STATUS hdd_softap_DeregisterSTA( hdd_adapter_t *pAdapter, tANI_U8 staId );

/**============================================================================
  @brief hdd_softap_RegisterSTA - Register a station into TL block

  @param pAdapter : [in] pointer to adapter context
  @param STAId    : [in] Station ID to deregister
  @param fAuthRequired: [in] Station requires further security negotiation or not
  @param fPrivacyBit  : [in] privacy bit needs to be set or not
  @param ucastSig  : [in] Unicast Signature send to TL
  @param bcastSig  : [in] Broadcast Signature send to TL
  @param pPeerMacAddress  : [in] station MAC address
  @param fWmmEnabled  : [in] Wmm enabled sta or not
  @return         : VOS_STATUS_E_FAILURE if any errors encountered 
                  : VOS_STATUS_SUCCESS otherwise
  =========================================================================== */
extern VOS_STATUS hdd_softap_RegisterSTA( hdd_adapter_t *pAdapter,
                                       v_BOOL_t fAuthRequired,
                                       v_BOOL_t fPrivacyBit,    
                                       v_U8_t staId,
                                       v_U8_t ucastSig,
                                       v_U8_t bcastSig,
                                       v_MACADDR_t *pPeerMacAddress,
                                       v_BOOL_t fWmmEnabled);

/**============================================================================
  @brief hdd_softap_Register_BC_STA - Register a default broadcast station into TL block

  @param pAdapter : [in] pointer to adapter context
  @param fPrivacyBit : [in] privacy bit needs to be set or not
  @return         : VOS_STATUS_E_FAILURE if any errors encountered 
                  : VOS_STATUS_SUCCESS otherwise
  =========================================================================== */
extern VOS_STATUS hdd_softap_Register_BC_STA( hdd_adapter_t *pAdapter, v_BOOL_t fPrivacyBit);

/**============================================================================
  @brief hdd_softap_DeregisterSTA - DeRegister the default broadcast station into TL block

  @param pAdapter : [in] pointer to adapter context
  @return         : VOS_STATUS_E_FAILURE if any errors encountered 
                  : VOS_STATUS_SUCCESS otherwise
  =========================================================================== */
extern VOS_STATUS hdd_softap_Deregister_BC_STA( hdd_adapter_t *pAdapter);

/**============================================================================
  @brief hdd_softap_stop_bss - Helper function to stop bss and do cleanup in HDD and TL

  @param pAdapter : [in] pointer to adapter context
  @return         : VOS_STATUS_E_FAILURE if any errors encountered 
                  : VOS_STATUS_SUCCESS otherwise
  =========================================================================== */
extern VOS_STATUS hdd_softap_stop_bss( hdd_adapter_t *pHostapdAdapter);


/**============================================================================
  @brief hdd_softap_change_STA_state - Helper function to change station state by MAC address

  @param pAdapter : [in] pointer to adapter context
  @param pDestMacAddress : [in] pointer to station MAC address
  @param state    : [in] new station state
  @return         : VOS_STATUS_E_FAILURE if any errors encountered 
                  : VOS_STATUS_SUCCESS otherwise
  =========================================================================== */
extern VOS_STATUS hdd_softap_change_STA_state( hdd_adapter_t *pAdapter, v_MACADDR_t *pDestMacAddress, WLANTL_STAStateType state);

/**============================================================================
  @brief hdd_softap_GetStaId - Helper function to get station Id from MAC address

  @param pAdapter : [in] pointer to adapter context
  @param pDestMacAddress : [in] pointer to station MAC address
  @param staId    : [out] station id
  @return         : VOS_STATUS_E_FAILURE if any errors encountered 
                  : VOS_STATUS_SUCCESS otherwise
  =========================================================================== */
extern VOS_STATUS hdd_softap_GetStaId( hdd_adapter_t *pAdapter, v_MACADDR_t *pMacAddress, v_U8_t *staId);

/**============================================================================
  @brief hdd_softap_GetConnectedStaId - Helper function to get station Id of the connected device

  @param pAdapter : [in] pointer to adapter context
  @param staId    : [out] station id
  @return         : VOS_STATUS_E_FAILURE if any errors encountered
                  : VOS_STATUS_SUCCESS otherwise
  =========================================================================== */
extern VOS_STATUS hdd_softap_GetConnectedStaId( hdd_adapter_t *pAdapter, v_U8_t *staId);

/**==========================================================================

  \brief hdd_start_trafficMonitor() -
   This function dynamically enable traffic monitor functonality
   the command iwpriv wlanX setTrafficMon <value>.

  @param pAdapter : [in] pointer to adapter context
  @return         : VOS_STATUS_E_FAILURE if any errors encountered

  ========================================================================== */
VOS_STATUS hdd_start_trafficMonitor( hdd_adapter_t *pAdapter );

/**==========================================================================

  \brief hdd_stop_trafficMonitor() -
   This function dynamically disable traffic monitor functonality
   the command iwpriv wlanX setTrafficMon <value>.

  @param pAdapter : [in] pointer to adapter context
  @return         : VOS_STATUS_E_FAILURE if any errors encountered

  ========================================================================== */
VOS_STATUS hdd_stop_trafficMonitor( hdd_adapter_t *pAdapter );

#endif    // end #if !defined( WLAN_HDD_SOFTAP_TX_RX_H )
