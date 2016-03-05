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

#if !defined( WLAN_HDD_TX_RX_H )
#define WLAN_HDD_TX_RX_H

/**===========================================================================

  \file  wlan_hdd_tx_rx.h

  \brief Linux HDD Tx/RX APIs
  ==========================================================================*/

/*---------------------------------------------------------------------------
  Include files
  -------------------------------------------------------------------------*/
#include <wlan_hdd_includes.h>
#include <vos_api.h>
#include <linux/skbuff.h>
#include <wlan_qct_tl.h>

/*---------------------------------------------------------------------------
  Preprocessor definitions and constants
  -------------------------------------------------------------------------*/
#define HDD_ETHERTYPE_802_1_X              ( 0x888E )
#define HDD_ETHERTYPE_802_1_X_FRAME_OFFSET ( 12 )
#define HDD_ETHERTYPE_802_1_X_SIZE         ( 2 )
#ifdef FEATURE_WLAN_WAPI
#define HDD_ETHERTYPE_WAI                  ( 0x88b4 )
#endif
#define HDD_ETHERTYPE_ARP                  ( 0x0806 )
#define HDD_ETHERTYPE_ARP_SIZE               42

#define HDD_80211_HEADER_LEN      24
#define HDD_80211_HEADER_QOS_CTL  2
#define HDD_LLC_HDR_LEN           6
#define HDD_FRAME_TYPE_MASK       0x0c 
#define HDD_FRAME_SUBTYPE_MASK    0xf0 
#define HDD_FRAME_TYPE_DATA       0x08
#define HDD_FRAME_TYPE_MGMT       0x00
#define HDD_FRAME_SUBTYPE_QOSDATA 0x80
#define HDD_FRAME_SUBTYPE_DEAUTH  0xC0
#define HDD_FRAME_SUBTYPE_DISASSOC 0xA0
#define HDD_DEST_ADDR_OFFSET      6

#define HDD_MAC_HDR_SIZE          6

#define HDD_PSB_CFG_INVALID                   0xFF
#define HDD_PSB_CHANGED                       0xFF
#define SME_QOS_UAPSD_CFG_BK_CHANGED_MASK     0xF1
#define SME_QOS_UAPSD_CFG_BE_CHANGED_MASK     0xF2
#define SME_QOS_UAPSD_CFG_VI_CHANGED_MASK     0xF4
#define SME_QOS_UAPSD_CFG_VO_CHANGED_MASK     0xF8

/*--------------------------------------------------------------------------- 
  Type declarations
  -------------------------------------------------------------------------*/ 
 
/*--------------------------------------------------------------------------- 
  Function declarations and documenation
  -------------------------------------------------------------------------*/ 

/**============================================================================
  @brief hdd_hard_start_xmit() - Function registered with the Linux OS for 
  transmitting packets

  @param skb      : [in]  pointer to OS packet (sk_buff)
  @param dev      : [in] pointer to Libra network device
  
  @return         : NET_XMIT_DROP if packets are dropped
                  : NET_XMIT_SUCCESS if packet is enqueued succesfully
  ===========================================================================*/
extern int hdd_hard_start_xmit(struct sk_buff *skb, struct net_device *dev);

extern int hdd_mon_hard_start_xmit(struct sk_buff *skb, struct net_device *dev);
/**============================================================================
  @brief hdd_tx_timeout() - Function called by OS if there is any
  timeout during transmission. Since HDD simply enqueues packet
  and returns control to OS right away, this would never be invoked

  @param dev : [in] pointer to Libra network device
  @return    : None
  ===========================================================================*/
extern void hdd_tx_timeout(struct net_device *dev);

/**============================================================================
  @brief hdd_stats() - Function registered with the Linux OS for 
  device TX/RX statistics

  @param dev      : [in] pointer to Libra network device
  
  @return         : pointer to net_device_stats structure
  ===========================================================================*/
extern struct net_device_stats* hdd_stats(struct net_device *dev);

/**============================================================================
  @brief hdd_init_tx_rx() - Init function to initialize Tx/RX
  modules in HDD

  @param pAdapter : [in] pointer to adapter context  
  @return         : VOS_STATUS_E_FAILURE if any errors encountered 
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
extern VOS_STATUS hdd_init_tx_rx( hdd_adapter_t *pAdapter );

/**============================================================================
  @brief hdd_deinit_tx_rx() - Deinit function to clean up Tx/RX
  modules in HDD

  @param pAdapter : [in] pointer to adapter context  
  @return         : VOS_STATUS_E_FAILURE if any errors encountered 
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
extern VOS_STATUS hdd_deinit_tx_rx( hdd_adapter_t *pAdapter );

/**============================================================================
  @brief hdd_disconnect_tx_rx() - Disconnect function to clean up Tx/RX
  modules in HDD

  @param pAdapter : [in] pointer to adapter context  
  @return         : VOS_STATUS_E_FAILURE if any errors encountered 
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
extern VOS_STATUS hdd_disconnect_tx_rx( hdd_adapter_t *pAdapter );

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
extern VOS_STATUS hdd_tx_complete_cbk( v_VOID_t *vosContext, 
                                       vos_pkt_t *pVosPacket, 
                                       VOS_STATUS vosStatusIn );

/**============================================================================
  @brief hdd_tx_fetch_packet_cbk() - Callback function invoked by TL to 
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
extern VOS_STATUS hdd_tx_fetch_packet_cbk( v_VOID_t *vosContext,
                                           v_U8_t *pStaId,
                                           WLANTL_ACEnumType    ucAC,
                                           vos_pkt_t **ppVosPacket,
                                           WLANTL_MetaInfoType *pPktMetaInfo );

/**============================================================================
  @brief hdd_tx_low_resource_cbk() - Callback function invoked in the 
  case where VOS packets are not available at the time of the call to get 
  packets. This callback function is invoked by VOS when packets are 
  available.

  @param pVosPacket : [in]  pointer to VOS packet 
  @param userData   : [in]  opaque user data that was passed initially 
  
  @return           : VOS_STATUS_E_FAILURE if any errors encountered, 
                    : VOS_STATUS_SUCCESS otherwise
  =============================================================================*/
extern VOS_STATUS hdd_tx_low_resource_cbk( vos_pkt_t *pVosPacket, 
                                           v_VOID_t *userData );

/**============================================================================
  @brief hdd_rx_packet_cbk() - Receive callback registered with TL.
  TL will call this to notify the HDD when a packet was received 
  for a registered STA.

  @param vosContext   : [in] pointer to VOS context  
  @param pVosPacket   : [in] pointer to VOS packet (conatining sk_buff) 
  @param staId        : [in] Station Id
  @param pRxMetaInfo  : [in] pointer to meta info for the received pkt(s) 

  @return             : VOS_STATUS_E_FAILURE if any errors encountered, 
                      : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
extern VOS_STATUS hdd_rx_packet_cbk( v_VOID_t *vosContext, 
                                     vos_pkt_t *pVosPacket, 
                                     v_U8_t staId,
                                     WLANTL_RxMetaInfoType* pRxMetaInfo );


/**============================================================================
  @brief hdd_IsEAPOLPacket() - Checks the packet is EAPOL or not.

  @param pVosPacket : [in] pointer to vos packet  
  @return         : VOS_TRUE if the packet is EAPOL 
                  : VOS_FALSE otherwise
  ===========================================================================*/
extern v_BOOL_t hdd_IsEAPOLPacket( vos_pkt_t *pVosPacket );

/**============================================================================
  @brief hdd_mon_tx_mgmt_pkt() - Transmit MGMT packet received on monitor 
                                 interface.

  @param pAdapter: [in] SAP/P2P GO adapter.
  ===========================================================================*/
void hdd_mon_tx_mgmt_pkt(hdd_adapter_t* pAdapter);

/**============================================================================
  @brief hdd_mon_tx_work_queue() - workqueue handler for transmitting mgmt packets..

  @param work: [in] workqueue structure.
  ===========================================================================*/
void hdd_mon_tx_work_queue(struct work_struct *work);

/**============================================================================
  @brief hdd_Ibss_GetStaId() - Get the StationID using the Peer Mac address
  @param pHddStaCtx : [in] pointer to HDD Station Context
  pMacAddress [in]  pointer to Peer Mac address
  staID [out]  pointer to Station Index
  @return    : VOS_STATUS_SUCCESS/VOS_STATUS_E_FAILURE
  ===========================================================================*/
VOS_STATUS hdd_Ibss_GetStaId(hdd_station_ctx_t *pHddStaCtx,
                                  v_MACADDR_t *pMacAddress, v_U8_t *staId);

/**============================================================================
  @brief hdd_tx_rx_pkt_cnt_stat_timer_handler() -
                    Timer handler to check enable/disable split scan
  @param pHddStaCtx : Hdd adapter
  @return    : VOS_STATUS_SUCCESS/VOS_STATUS_E_FAILURE
  ===========================================================================*/
void hdd_tx_rx_pkt_cnt_stat_timer_handler( void *pAdapter);

/**=========================================================================
  @brief hdd_wmm_acquire_access_required()-
                   Determine whether wmm ac acquire access is required
  @param pAdapter  : pointer to Adapter context
  @param acType    : AC
  @return          : void
   ========================================================================*/
void hdd_wmm_acquire_access_required(hdd_adapter_t *pAdapter,
                                     WLANTL_ACEnumType acType);
#endif    // end #if !defined( WLAN_HDD_TX_RX_H )
